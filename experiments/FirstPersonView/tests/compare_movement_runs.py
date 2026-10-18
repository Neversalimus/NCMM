"""Compare exact packaged native movement runs without treating CI wall times as FPS."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
from verify_horizon_release import frame_phases, percentile, require

WEATHER = {
    "clear": "1882a60c3327bc811344cb7f46411e705d6c930e261a7a3b6ea96db38e053833",
    "rain": "dd16a6ce9a64f5b866dd96c605f3f27794c1b77a958bb118779c6f1c54bcb79e",
}
SCENES = ("street", "multi_level_horde", "city_blocks")
EXTENDED_SCENES = ("street", "city_blocks", "dense_city", "forest", "multi_level_horde",
                   "creatures_20", "creatures_100", "smoke_rain", "armed", "empty_attacks")
EXTENDED_VIEWPORTS = ((640, 480), (1280, 800), (1768, 1084), (1920, 1080))
RESOURCE_STREAMS = ("gpu_upload_bytes", "gpu_static_upload_bytes", "gpu_dynamic_geometry_bytes",
                    "gpu_draw_calls", "gpu_bones", "gpu_held_bones", "gpu_resource_bytes",
                    "host_cpu_cache_bytes", "process_rss_bytes")
ENGINE = "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83"
BASELINE = "4ea4e39e0d6288e0ab8ca4cf62f19c36f0ff4386"


def load(file):
    return json.loads(Path(file).read_text(encoding="utf-8-sig"))


def digest(file):
    return hashlib.sha256(Path(file).read_bytes()).hexdigest()


def weather_selection(extended, single):
    require(single is None or extended, "A single weather fixture is allowed only for the explicit extended diagnostic matrix")
    return (single,) if single is not None else tuple(WEATHER)


def payload(bundle, source, *, diagnostic_host_source=None):
    bundle = Path(bundle)
    manifest = load(bundle / "payload/first-person-bundle.json")
    require(manifest["source_commit"] == ENGINE,
            "This paired hardware route requires the exact 1040 engine")
    if diagnostic_host_source is None:
        release = load(bundle / "release-manifest.json")
        require(release["source_commit"] == manifest["ncmm_source_commit"] == source,
                "Unexpected packaged NCMM source")
        require(release["engine_source"] == ENGINE, "Unexpected release engine")
    else:
        components = manifest.get("diagnostic_mixed_components", {})
        require(all(re.fullmatch(r"[0-9a-f]{40}", value) for value in (source, diagnostic_host_source)),
                "Diagnostic components require complete expected Git commits")
        require(components.get("host_ncmm_commit") == diagnostic_host_source and
                components.get("module_ncmm_commit") == manifest["ncmm_source_commit"] == source,
                "Unexpected diagnostic component source")
        release = {"source_commit": source, "host_source_commit": diagnostic_host_source,
                   "host_sha256": manifest["host_sha256"],
                   "module_sha256": manifest["files"]["code_mods/FirstPersonView/ncmm_mod.dll"],
                   "installer_revision": "diagnostic component snapshot; not a qualified installer"}
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
    result = {name: release[name] for name in ("source_commit", "host_sha256", "module_sha256", "installer_revision")}
    if diagnostic_host_source is not None:
        result["host_source_commit"] = diagnostic_host_source
    return result


def read_run(root, identity, weather, phases, *, extended=False):
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
    scenes = EXTENDED_SCENES if extended else SCENES
    viewports = EXTENDED_VIEWPORTS if extended else ((640, 480), (1768, 1084))
    steps, rotations = (64, 96) if extended else (12, 48)
    expected = {(scene, renderer, width, height) for scene in scenes for renderer in ("cpu", "gpu")
                for width, height in viewports}
    actual = {(r["scene"], r["renderer"], r["width"], r["height"]) for r in records}
    require(len(records) == len(expected) and actual == expected, "Incomplete or duplicate movement matrix")
    summary = []
    first_full = []
    for record in records:
        require(record["native_steps"] == steps and record["tileset"] == "UndeadPeople_0J_Hybrid_v3"
                and record["native_art"] is True, "Wrong movement fixture")
        wall = record["frame_samples_ms"]
        first = record["first_frame_after_step_samples_ms"]
        ready = record["first_ready_samples_ms"]
        warm = record["warm_rotation_samples_ms"]
        require(len(first) == len(ready) == steps and len(warm) == rotations and len(wall) >= rotations * 2 + steps,
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
                "first_p99_ms": percentile(first, .99),
                "first_max_ms": max(first), "first_under_15_ms": sum(v <= 15 for v in first),
                "first_samples": len(first), "ready_median_ms": percentile(ready, .5),
                "ready_p95_ms": percentile(ready, .95), "ready_p99_ms": percentile(ready, .99),
                "ready_max_ms": max(ready),
                "warm_median_ms": percentile(warm, .5), "warm_p95_ms": percentile(warm, .95),
                "warm_p99_ms": percentile(warm, .99), "warm_max_ms": max(warm),
                "source_cache_bytes_peak": record.get("source_cache_bytes_peak")}
        if phases or extended:
            item["phases"] = frame_phases(record, extended=extended)
            if record["renderer"] == "cpu":
                # An idle GPU backend may retain the preceding GPU frame metrics.
                for group in item["phases"].values():
                    for name in list(group):
                        if name.startswith("gpu_"):
                            del group[name]
        if extended:
            require(record.get("extended_matrix") is True, "Extended fixture marker missing")
            internal = [record["internal_width"], record["internal_height"]]
            cap = (1920, 1080) if record["renderer"] == "gpu" else (960, 600)
            require(all(type(v) is int and 0 < v <= min(limit, viewport)
                        for v, limit, viewport in zip(internal, cap, (record["width"], record["height"]))),
                    "Invalid recorded internal resolution")
            require(record["native_confirmed_attacks"] == (16 if record["scene"] == "empty_attacks" else 0),
                    "Missing or unexpected confirmed native attacks")
            require(record["native_weather_id"] == ("rain" if record["scene"] == "smoke_rain" else "sunny"),
                    "Unexpected extended scene weather")
            item["native_weather_id"] = record["native_weather_id"]
            cold = record["cold_rotation_samples_ms"]
            require(len(cold) == rotations and all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in cold),
                    "Invalid cold rotation samples")
            item["internal_resolution"] = internal
            item["cold_rotation"] = {"median_ms": percentile(cold, .5), "p95_ms": percentile(cold, .95),
                                     "p99_ms": percentile(cold, .99), "max_ms": max(cold)}
            item["resources"] = {}
            for name in RESOURCE_STREAMS:
                values = record[name]
                require(len(values) == len(wall) and all(type(v) is int and v >= 0 for v in values),
                        "Invalid resource sample stream: " + name)
                selected = [values[i] for i in record["first_frame_indices"]]
                item["resources"][name] = {"first_median": percentile(selected, .5),
                    "first_p95": percentile(selected, .95), "peak": max(values),
                    "begin": values[0], "end": values[-1],
                    "warm_median": percentile(values[rotations:rotations*2], .5)}
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
            parser.add_argument("--" + version + "-" + weather, type=Path)
    parser.add_argument("--expected-candidate-source", required=True)
    parser.add_argument("--expected-baseline-source", default=BASELINE)
    parser.add_argument("--extended", action="store_true", help="Strict 80-record, 64-step diagnostic matrix")
    parser.add_argument("--single-extended-weather", choices=tuple(WEATHER),
                        help="Use one hashed input fixture for extended scenes, which select native sunny/rain weather themselves")
    for version in ("baseline", "candidate"):
        parser.add_argument("--" + version + "-host-source", help="Explicit component snapshot Host commit; not release qualification")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    weathers = weather_selection(args.extended, args.single_extended_weather)
    for version in ("baseline", "candidate"):
        require(all(getattr(args, version + "_" + weather) is not None for weather in weathers),
                "Missing required " + version + " weather run")
    require(bool(args.baseline_host_source) == bool(args.candidate_host_source),
            "Both diagnostic Host sources must be supplied")
    result = {"schema": 1, "success": False, "scope": "CPU wall time of native SDL frames; not GPU timestamps or manual gameplay FPS",
              "simulation_turns_advanced": 0, "quality": "Fresh Balanced; unchanged GPU internal cap 1920x1080 and CPU cap 960x600. Internal dimensions are inferred from source, not recorded by this route.",
              "versions": {}, "comparisons": []}
    result["diagnostic_component_snapshots"] = bool(args.baseline_host_source)
    result["weather_fixture_files"] = list(weathers)
    if args.extended:
        result["quality"] = "Fresh Balanced; actual internal dimensions recorded; identical GPU 1920x1080 and CPU 960x600 caps"
        result["timing_scopes"] = {"first": "native tile preparation + graphics extraction/callback + SDL presentation; excludes avatar_action and pre-render native cache refresh",
            "ready": "native avatar_action + native cache refresh + render + SDL presentation + diagnostic instrumentation",
            "rotation": "native camera action + cache refresh + render + SDL presentation + diagnostic instrumentation",
            "phases": "exclusive frame phases sum per sample; extraction/callback/GPU values are nested and must not be added"}
    udp_reference = None
    for version, source in (("baseline", args.expected_baseline_source), ("candidate", args.expected_candidate_source)):
        identity = payload(getattr(args, version + "_bundle"), source,
                           diagnostic_host_source=getattr(args, version + "_host_source"))
        runs = []
        for weather in weathers:
            run, udp = read_run(getattr(args, version + "_" + weather), identity, weather, version == "candidate", extended=args.extended)
            if udp_reference is None:
                udp_reference = udp
            require(udp == udp_reference, "UDP assets differ between runs")
            runs.append(run)
        result["versions"][version] = {"identity": identity, "runs": runs}
    for n, weather in enumerate(weathers):
        before = result["versions"]["baseline"]["runs"][n]["records"]
        after = result["versions"]["candidate"]["runs"][n]["records"]
        lookup = {(r["scene"], r["renderer"], tuple(r["viewport"])): r for r in before}
        for row in after:
            old = lookup[(row["scene"], row["renderer"], tuple(row["viewport"]))]
            if args.extended:
                require(old["internal_resolution"] == row["internal_resolution"], "Paired internal resolutions differ")
            result["comparisons"].append({"weather": weather, "scene": row["scene"],
                "renderer": row["renderer"], "viewport": row["viewport"],
                "first_median_delta_ms": row["first_median_ms"] - old["first_median_ms"],
                "first_p95_delta_ms": row["first_p95_ms"] - old["first_p95_ms"],
                "first_p99_delta_ms": row["first_p99_ms"] - old["first_p99_ms"],
                "warm_median_delta_ms": row["warm_median_ms"] - old["warm_median_ms"],
                "warm_p95_delta_ms": row["warm_p95_ms"] - old["warm_p95_ms"],
                "warm_p99_delta_ms": row["warm_p99_ms"] - old["warm_p99_ms"]})
    result["udp_manifest_sha256"] = hashlib.sha256(json.dumps(udp_reference, sort_keys=True).encode()).hexdigest()
    candidate_runs = result["versions"]["candidate"]["runs"]
    result["target_15_ms"] = {"scope": "Full-size GPU first rendered frames after native steps, clear plus controlled rain",
        "under_or_equal": sum(r["full_size_gpu_first_under_15_ms"] for r in candidate_runs),
        "total": sum(r["full_size_gpu_first_samples"] for r in candidate_runs)}
    if args.extended:
        result["target_15_ms"]["scope"] = "1768x1084 GPU first rendered frames across all ten scenes; actual native weather recorded per scene; excludes pre-render move/cache refresh"
    result["target_15_ms"]["all_samples_met"] = result["target_15_ms"]["under_or_equal"] == result["target_15_ms"]["total"]
    result["success"] = True
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"success": True, "target_15_ms": result["target_15_ms"], "comparisons": len(result["comparisons"])}))


if __name__ == "__main__":
    main()
