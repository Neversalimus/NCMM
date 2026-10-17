"""Compare exact packaged native movement runs without treating CI wall times as FPS."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from verify_horizon_release import frame_phases, percentile, require

WEATHER = {
    "clear": "1882a60c3327bc811344cb7f46411e705d6c930e261a7a3b6ea96db38e053833",
    "rain": "dd16a6ce9a64f5b866dd96c605f3f27794c1b77a958bb118779c6f1c54bcb79e",
}
SCENES = ("street", "multi_level_horde", "city_blocks")
ENGINE = "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83"
BASELINE = "4ea4e39e0d6288e0ab8ca4cf62f19c36f0ff4386"


def load(file):
    return json.loads(Path(file).read_text(encoding="utf-8-sig"))


def digest(file):
    return hashlib.sha256(Path(file).read_bytes()).hexdigest()


def payload(bundle, source):
    bundle = Path(bundle)
    release = load(bundle / "release-manifest.json")
    manifest = load(bundle / "payload/first-person-bundle.json")
    require(release["source_commit"] == manifest["ncmm_source_commit"] == source,
            "Unexpected packaged NCMM source")
    require(release["engine_source"] == manifest["source_commit"] == ENGINE,
            "This paired hardware route requires the exact 1040 engine")
    require(len(manifest["files"]) == 32, "Incomplete packaged payload")
    for name, expected in manifest["files"].items():
        relative = Path(name)
        require(not relative.is_absolute() and ".." not in relative.parts and ":" not in name,
                "Unsafe payload path")
        require(digest(bundle / "payload" / relative) == expected, "Payload changed: " + name)
    require(release["host_sha256"] == manifest["host_sha256"] ==
            manifest["files"]["host/cataclysm-tiles.ncmm.exe"], "Host manifest mismatch")
    require(release["module_sha256"] == manifest["files"]["code_mods/FirstPersonView/ncmm_mod.dll"],
            "Module manifest mismatch")
    return {name: release[name] for name in ("source_commit", "host_sha256", "module_sha256", "installer_revision")}


def read_run(root, identity, weather, phases):
    root = Path(root)
    evidence = root / "evidence"
    report = load(evidence / "first-person-movement-benchmark.json")
    provenance = load(root / "benchmark-provenance.json")
    process = load(evidence / "first-person-process-result.json")
    require(provenance["success"] is True and provenance["movement_benchmark"] is True
            and provenance["manual_playthrough"] is False and provenance["udp_requested"] is True,
            "Invalid hardware movement provenance")
    require(process["exit_signed"] == 0 and process["crash_dumps"] == 0
            and process["debugger_error"] is None and process["wrapper_failure"] is None
            and process["last_progress"] == "movement_complete", "Native child did not finish cleanly")
    for data in (process, provenance):
        require(data["ncmm_commit"] == identity["source_commit"] and
                data["host_sha256"] == identity["host_sha256"] and
                data["module_sha256"] == identity["module_sha256"], "Timed package identity mismatch")
    require(provenance["source_commit"] == ENGINE and process["engine_commit"] == ENGINE,
            "Timed engine pin mismatch")
    require(digest(root / "game/cataclysm-tiles.ncmm.exe") == identity["host_sha256"] and
            digest(root / "game/code_mods/FirstPersonView/ncmm_mod.dll") == identity["module_sha256"],
            "Installed timed binaries changed")
    require(digest(root / "game/data/json/weather_type.json") == WEATHER[weather],
            "Weather fixture changed")
    require(report["success"] is True and report["actual_cdda_sdl"] is True and
            report["simulation_turns_advanced"] == 0, "Movement route advanced simulation or failed")
    require(report["geometry_regressions"]["success"] is True and
            report["geometry_regressions"]["captures"] == 8 and
            report["facade_regression"]["normal_vision"] is True and
            report["horizon_regression"]["near_overlap"] is False, "Geometry regression failed")
    records = report["records"]
    expected = {(scene, renderer, width, height) for scene in SCENES for renderer in ("cpu", "gpu")
                for width, height in ((640, 480), (1768, 1084))}
    actual = {(r["scene"], r["renderer"], r["width"], r["height"]) for r in records}
    require(len(records) == 12 and actual == expected, "Incomplete or duplicate movement matrix")
    summary = []
    first_full = []
    for record in records:
        require(record["native_steps"] == 12 and record["tileset"] == "UndeadPeople_0J_Hybrid_v3"
                and record["native_art"] is True, "Wrong movement fixture")
        wall = record["frame_samples_ms"]
        first = record["first_frame_after_step_samples_ms"]
        ready = record["first_ready_samples_ms"]
        warm = record["warm_rotation_samples_ms"]
        require(len(first) == len(ready) == 12 and len(warm) == 48 and len(wall) >= 108,
                "Incomplete timing samples")
        require(all(type(v) in (int, float) and math.isfinite(v) and v >= 0
                    for values in (wall, first, ready, warm) for v in values), "Invalid timing sample")
        require(all(r + .003 >= f for r, f in zip(ready, first)), "Ready time is shorter than its rendered frame")
        require(len(record["frame_renderers_gpu"]) == len(wall) and
                all(v == (1 if record["renderer"] == "gpu" else 0)
                    for v in record["frame_renderers_gpu"]), "Renderer changed during measurement")
        if record["renderer"] == "gpu":
            require(record["device"] == "NVIDIA GeForce RTX 4090" and
                    record["backend"] == "direct3d12" and record["software_device"] is False,
                    "Expected hardware device was not used")
        item = {"scene": record["scene"], "renderer": record["renderer"],
                "viewport": [record["width"], record["height"]],
                "first_median_ms": percentile(first, .5), "first_p95_ms": percentile(first, .95),
                "first_max_ms": max(first), "first_under_15_ms": sum(v <= 15 for v in first),
                "first_samples": len(first), "ready_median_ms": percentile(ready, .5),
                "warm_median_ms": percentile(warm, .5), "warm_p95_ms": percentile(warm, .95)}
        if phases:
            item["phases"] = frame_phases(record)
            if record["renderer"] == "cpu":
                # An idle GPU backend may retain the preceding GPU frame metrics.
                for group in item["phases"].values():
                    for name in list(group):
                        if name.startswith("gpu_"):
                            del group[name]
        summary.append(item)
        if record["renderer"] == "gpu" and record["width"] == 1768:
            first_full.extend(first)
    udp_entries = load(evidence / "udp-asset-sha256.json")
    udp = {entry["file"]: entry["sha256"] for entry in udp_entries}
    require(len(udp) == len(udp_entries) and len(udp) > 10, "Invalid UDP asset receipt")
    return {"weather": weather, "process_exit": process["exit_signed"], "records": summary,
            "full_size_gpu_first_under_15_ms": sum(v <= 15 for v in first_full),
            "full_size_gpu_first_samples": len(first_full)}, udp


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for version in ("baseline", "candidate"):
        parser.add_argument("--" + version + "-bundle", type=Path, required=True)
        for weather in WEATHER:
            parser.add_argument("--" + version + "-" + weather, type=Path, required=True)
    parser.add_argument("--expected-candidate-source", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {"schema": 1, "success": False, "scope": "CPU wall time of native SDL frames; not GPU timestamps or manual gameplay FPS",
              "simulation_turns_advanced": 0, "quality": "Fresh Balanced; unchanged GPU internal cap 1920x1080 and CPU cap 1280x800. Internal dimensions are inferred from source, not recorded by this route.",
              "versions": {}, "comparisons": []}
    udp_reference = None
    for version, source in (("baseline", BASELINE), ("candidate", args.expected_candidate_source)):
        identity = payload(getattr(args, version + "_bundle"), source)
        runs = []
        for weather in WEATHER:
            run, udp = read_run(getattr(args, version + "_" + weather), identity, weather, version == "candidate")
            if udp_reference is None:
                udp_reference = udp
            require(udp == udp_reference, "UDP assets differ between runs")
            runs.append(run)
        result["versions"][version] = {"identity": identity, "runs": runs}
    for n, weather in enumerate(WEATHER):
        before = result["versions"]["baseline"]["runs"][n]["records"]
        after = result["versions"]["candidate"]["runs"][n]["records"]
        lookup = {(r["scene"], r["renderer"], tuple(r["viewport"])): r for r in before}
        for row in after:
            old = lookup[(row["scene"], row["renderer"], tuple(row["viewport"]))]
            result["comparisons"].append({"weather": weather, "scene": row["scene"],
                "renderer": row["renderer"], "viewport": row["viewport"],
                "first_median_delta_ms": row["first_median_ms"] - old["first_median_ms"],
                "first_p95_delta_ms": row["first_p95_ms"] - old["first_p95_ms"],
                "warm_median_delta_ms": row["warm_median_ms"] - old["warm_median_ms"]})
    result["udp_manifest_sha256"] = hashlib.sha256(json.dumps(udp_reference, sort_keys=True).encode()).hexdigest()
    candidate_runs = result["versions"]["candidate"]["runs"]
    result["target_15_ms"] = {"scope": "Full-size GPU first rendered frames after native steps, clear plus controlled rain",
        "under_or_equal": sum(r["full_size_gpu_first_under_15_ms"] for r in candidate_runs),
        "total": sum(r["full_size_gpu_first_samples"] for r in candidate_runs)}
    result["target_15_ms"]["all_samples_met"] = result["target_15_ms"]["under_or_equal"] == result["target_15_ms"]["total"]
    result["success"] = True
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"success": True, "target_15_ms": result["target_15_ms"], "comparisons": len(result["comparisons"])}))


if __name__ == "__main__":
    main()
