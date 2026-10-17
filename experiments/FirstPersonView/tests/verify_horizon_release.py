"""Verify the shipped ZIP and distinguish first-frame rendering from native move latency."""
import argparse
import base64
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import zipfile


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def percentile(values, q):
    values = sorted(values)
    at = (len(values) - 1) * q
    low = math.floor(at)
    return values[low] + (values[math.ceil(at)] - values[low]) * (at - low)


def samples(record, name, count):
    values = record[name]
    require(len(values) == count, f"{name}: wrong sample count")
    require(all(type(v) in (float, int) and math.isfinite(v) and v > 0 for v in values),
            f"{name}: non-finite or non-positive sample")
    stem = name.removesuffix("_samples_ms")
    for metric, q in (("median", .5), ("p95", .95)):
        require(abs(record[f"{stem}_{metric}_ms"] - percentile(values, q)) < .05,
                f"{stem}: reported {metric} disagrees with raw samples")
    return {"median_ms": percentile(values, .5), "p95_ms": percentile(values, .95)}


def frame_phases(record):
    require(record.get("exclusive_phases_available") is True, "Movement frame phases missing")
    wall = record["frame_samples_ms"]
    phases = ("native_tiles_samples_ms", "perspective_samples_ms", "present_samples_ms", "unattributed_samples_ms")
    nested = ("extract_samples_ms", "callback_samples_ms", "gpu_geometry_samples_ms", "gpu_resource_samples_ms", "gpu_upload_samples_ms", "gpu_record_samples_ms", "gpu_submit_samples_ms")
    require(len(wall) >= 108, "Movement frame sequence incomplete")
    for name in ("frame_samples_ms",) + phases + nested:
        values = record[name]
        require(len(values) == len(wall) and all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in values), "Invalid movement phase samples: " + name)
    for i, total in enumerate(wall):
        require(abs(total - sum(record[name][i] for name in phases)) <= .003, "Movement phases overlap or omit frame work")
    indices = record["first_frame_indices"]
    require(len(indices) == 12 and all(type(i) is int and 96 <= i < len(wall) for i in indices)
            and all(a < b for a, b in zip(indices, indices[1:])), "Invalid native-step frame indices")
    require(all(abs(record["first_frame_after_step_samples_ms"][n] - wall[i]) <= .0001 for n, i in enumerate(indices)), "Native-step frames differ from measured frame stream")
    return {"first_after_step": {name: {"median_ms": percentile([record[name][i] for i in indices], .5),
                                     "p95_ms": percentile([record[name][i] for i in indices], .95),
                                     "p99_ms": percentile([record[name][i] for i in indices], .99)} for name in phases + nested},
            "warm_rotation": {name: {"median_ms": percentile(record[name][48:96], .5),
                                    "p95_ms": percentile(record[name][48:96], .95),
                                    "p99_ms": percentile(record[name][48:96], .99)} for name in phases + nested}}


def verify(args):
    root = Path(args.installer_root)
    archive = root / f"NCMM_FirstPersonView_{args.expected_version}_TEST_{args.build}_Windows_x64.zip"
    raw = archive.read_bytes()
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        entries = {}
        for info in z.infolist():
            name = info.filename.replace("\\", "/")
            require(not name.startswith("/") and ".." not in PurePosixPath(name).parts,
                    "Unsafe archive path")
            if name.endswith("/"):
                continue
            require(name not in entries, "Duplicate archive path")
            entries[name] = info
        def read(name):
            return z.read(entries[name])
        release = json.loads(read("release-manifest.json").decode("utf-8-sig"))
        bundle = json.loads(read("payload/first-person-bundle.json").decode("utf-8-sig"))
        require(release["installer_revision"] == args.expected_version and
                release["status"] == "experimental-not-certified", "Wrong release identity")
        require(release["source_commit"] == bundle["ncmm_source_commit"] == args.expected_source,
                "Installer source differs from qualified CI checkout")
        require(str(release["workflow_run"]) == args.expected_run, "Wrong workflow run")
        pins = {"1040": "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83",
                "1807": "074aa98bd5be3de4c35f154082db32a0e63bb0f1"}
        require(bundle["source_commit"] == release["engine_source"] == pins[args.build],
                "Wrong engine pin")
        mod = json.loads(read("payload/code_mods/FirstPersonView/mod.json").decode("utf-8-sig"))
        require(mod["version"] == args.expected_version + "-dev", "Wrong module version")
        require(len(bundle["files"]) == 32, "Incomplete payload file matrix")
        for name, expected in bundle["files"].items():
            require(digest(read("payload/" + name)) == expected, "Payload hash mismatch: " + name)
        require(digest(read("NCMM_Setup_FirstPerson_TEST.exe")) == release["installer_sha256"],
                "Setup hash mismatch")
        require(bundle["host_sha256"] == release["host_sha256"] ==
                bundle["files"]["host/cataclysm-tiles.ncmm.exe"], "Host identity mismatch")
        require(release["module_sha256"] == bundle["files"]["code_mods/FirstPersonView/ncmm_mod.dll"],
                "Module identity mismatch")
    stress = load_json(root / "first-person-gpu-focused-stress.json")
    require(len(stress) == 8 and [s["attempt"] for s in stress] == list(range(1, 9)), "Incomplete focused teardown stress matrix")
    for attempt in stress:
        p = attempt["process"]
        require(attempt["clean_exit"] is True and attempt["verified"]["success"] is True and p["exit_signed"] == 0
                and p["crash_dumps"] == 0 and p["debugger_error"] is None and p["debugger_requested"] is True
                and p["host_sha256"] == release["host_sha256"] and p["module_sha256"] == release["module_sha256"]
                and p["ncmm_commit"] == args.expected_source, "Focused stress bytes or clean exit mismatch")
    provenance = load_json(root / "first-person-movement-provenance.json")
    require(provenance["success"] is True and provenance["manual_playthrough"] is False,
            "Missing automated benchmark provenance")
    require(provenance["host_sha256"] == release["host_sha256"] and
            provenance["module_sha256"] == release["module_sha256"] and
            provenance["ncmm_commit"] == args.expected_source,
            "Timed binaries differ from ZIP payload")
    report = load_json(root / "first-person-movement-benchmark.json")
    require(report["success"] is True and report["actual_cdda_sdl"] is True and
            report["simulation_turns_advanced"] == 0, "Invalid native movement run")
    require(len(report["records"]) == 12, "Incomplete movement matrix")
    facade = report["facade_regression"]
    require(facade["normal_vision"] is True and facade["connected_front_wall_cells"] == 18
            and facade["slabs_connected"] is True, "Disconnected ordinary-vision facade")
    horizon = report["horizon_regression"]
    require(horizon["known_forest_patches"] > 20 and horizon["unknown_neutral_patches"] > 20
            and horizon["near_overlap"] is False and horizon["native_perception"] is True
            and horizon["cpu_and_gpu"] is True, "Horizon privacy/depth regression")
    geometry = report["geometry_regressions"]
    require(geometry["success"] is True and geometry["cpu_and_gpu"] is True and geometry["captures"] == 8, "Missing geometry regression matrix")
    for name in ("lattice_cells", "canopy_without_slab", "connected_fence_corners", "windows_without_door_model", "unseen_continuous_walls", "supported_item_cells"):
        require(geometry[name] > 0, "Missing native geometry coverage: " + name)
    timings = []
    matrix = set()
    for record in report["records"]:
        key = (record["scene"], record["renderer"], record["width"], record["height"])
        require(key not in matrix, "Duplicate movement matrix entry")
        matrix.add(key)
        require(record["native_steps"] == 12, "Native steps missing")
        phases = frame_phases(record)
        frame = samples(record, "first_frame_after_step_samples_ms", 12)
        ready = samples(record, "first_ready_samples_ms", 12)
        cold = samples(record, "cold_rotation_samples_ms", 48)
        warm = samples(record, "warm_rotation_samples_ms", 48)
        if record["renderer"] == "gpu":
            require(len(record["frame_renderers_gpu"]) == len(record["frame_samples_ms"])
                    and all(v == 1 for v in record["frame_renderers_gpu"]),
                    "GPU walking or rotation fell back to CPU")
            timings.append({"scene": record["scene"], "width": record["width"],
                            "height": record["height"], "first_render_frame": frame,
                            "native_move_to_first_ready": ready, "cold_rotation": cold,
                            "warm_rotation": warm, "frame_phases": phases, "device": record["device"],
                            "backend": record["backend"],
                            "software_device": record["software_device"]})
    require(len(timings) == 6, "GPU matrix incomplete")
    result = {"success": True, "version": args.expected_version, "build": args.build,
              "source_commit": args.expected_source, "workflow_run": args.expected_run,
              "installer": archive.name, "installer_sha256": digest(raw),
              "host_sha256": release["host_sha256"], "module_sha256": release["module_sha256"],
              "verified_payload_files": 32, "manual_playthrough": False,
              "facade": facade, "horizon": horizon, "geometry": geometry, "gpu_timings": timings,
              "clean_focused_process_exits": len(stress), "exclusive_movement_phases_verified": True,
              "timing_scope": "CI device; first_render_frame excludes native avatar_action/refresh; native_move_to_first_ready includes both"}
    # These are disposable native-game fixture captures, never desktop captures.
    from PIL import Image
    for mode in ("cpu", "gpu"):
        image = Image.open(Path(args.evidence_root) / f"first-person-horizon-{mode}.bmp").convert("RGB")
        require(image.width >= 640 and image.height >= 360, "Horizon capture too small")
        if args.log_preview and mode == "gpu":
            for width in (640, 480, 320):
                preview = image.copy()
                preview.thumbnail((width, width))
                output = io.BytesIO()
                preview.quantize(colors=64).save(output, format="PNG", optimize=True)
                data = output.getvalue()
                if len(data) < 131072:
                    break
            require(len(data) < 131072, "Preview unexpectedly large")
            print(f"NCMM_HORIZON_PNG_BEGIN bytes={len(data)}", flush=True)
            encoded = base64.b64encode(data).decode("ascii")
            for at in range(0, len(encoded), 4096):
                print("NCMM_HORIZON_PNG_DATA " + encoded[at:at+4096], flush=True)
            print("NCMM_HORIZON_PNG_END", flush=True)
    Path(args.output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("NCMM_RELEASE_VERIFIED_BEGIN", flush=True)
    print(json.dumps(result, separators=(",", ":")), flush=True)
    print("NCMM_RELEASE_VERIFIED_END", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--installer-root", required=True)
    parser.add_argument("--evidence-root", required=True)
    parser.add_argument("--build", choices=("1040", "1807"), required=True)
    parser.add_argument("--expected-run", required=True)
    parser.add_argument("--expected-source", required=True)
    parser.add_argument("--expected-version", default="0.12.9")
    parser.add_argument("--output", required=True)
    parser.add_argument("--log-preview", action="store_true")
    verify(parser.parse_args())
