"""Verify the shipped ZIP and distinguish first-frame rendering from native move latency."""
import argparse
import base64
import hashlib
import io
import json
import math
import re
import subprocess
from pathlib import Path, PurePosixPath
import zipfile


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def expected_payload_files(source_paths):
    """Exact package contract, including resources owned by this source tree."""
    modules = ("AdvancedWorldSettings", "BallisticHitChance", "EquipmentBodyMap",
               "ItemGlyphs", "LegacyCharacterPoints", "SurvivorProgression")
    expected = {f"code_mods/{module}/{name}" for module in modules
                for name in ("about.en.txt", "about.ru.txt", "mod.json", "ncmm_mod.dll")}
    expected.update(("cataclysm-tiles.ncmm-bootstrap.exe", "host/cataclysm-tiles.ncmm.exe",
                     "host/first-person-source.json", "code_mods/FirstPersonView/mod.json",
                     "code_mods/FirstPersonView/ncmm_mod.dll",
                     "code_mods/SurvivorProgression/data/respec.json",
                     "code_mods/SurvivorProgression/persistent_data/dimensional_pouch.json",
                     "code_mods/SurvivorProgression/persistent_data/mana_hand_carrier.json"))
    source_paths = set(source_paths)
    if "experiments/FirstPersonView/assets/held_items/selection.json" in source_paths:
        expected.update(("code_mods/FirstPersonView/held-assets.manifest.json",
                         "code_mods/FirstPersonView/held-assets-licenses.txt"))
    if "experiments/FirstPersonView/assets/world_props/selection.json" in source_paths:
        expected.update(("code_mods/FirstPersonView/world-assets.manifest.json",
                         "code_mods/FirstPersonView/world-assets-license.txt",
                         "code_mods/FirstPersonView/weapon-effects.references.json"))
    if "experiments/graphics_bridge/third_party/nvtx3/LICENSE.txt" in source_paths:
        expected.add("code_mods/FirstPersonView/NVTX-LICENSE.txt")
    return expected


def verify_payload_file_matrix(files, archive_entries, source_paths):
    expected = expected_payload_files(source_paths)
    require(set(files) == expected, "Payload file matrix differs from trusted source: "
            f"missing={sorted(expected-set(files))}; unexpected={sorted(set(files)-expected)}")
    archived = {name.removeprefix("payload/") for name in archive_entries
                if name.startswith("payload/") and name != "payload/first-person-bundle.json"}
    require(archived == expected, "Archive payload matrix differs from declared source contract")


def source_tree_paths(commit):
    require(re.fullmatch(r"[0-9a-f]{40}", commit), "Expected an exact source commit")
    repo = Path(__file__).resolve().parents[3]
    return subprocess.check_output(["git", "-c", "safe.directory="+repo.as_posix(),
                                    "-C", str(repo), "ls-tree", "-r", "--name-only", commit],
                                   text=True, encoding="utf-8").splitlines()


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


def frame_phases(record, *, extended=False):
    steps, rotations = (64, 96) if extended else (12, 48)
    require(record.get("exclusive_phases_available") is True, "Movement frame phases missing")
    wall = record["frame_samples_ms"]
    phases = ("native_tiles_samples_ms", "perspective_samples_ms", "present_samples_ms", "unattributed_samples_ms")
    nested = ("extract_samples_ms", "callback_samples_ms", "gpu_geometry_samples_ms", "gpu_resource_samples_ms", "gpu_upload_samples_ms", "gpu_record_samples_ms", "gpu_submit_samples_ms")
    require(len(wall) >= rotations * 2 + steps, "Movement frame sequence incomplete")
    for name in ("frame_samples_ms",) + phases + nested:
        values = record[name]
        require(len(values) == len(wall) and all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in values), "Invalid movement phase samples: " + name)
    for i, total in enumerate(wall):
        require(abs(total - sum(record[name][i] for name in phases)) <= .003, "Movement phases overlap or omit frame work")
    indices = record["first_frame_indices"]
    require(len(indices) == steps and all(type(i) is int and rotations * 2 <= i < len(wall) for i in indices)
            and all(a < b for a, b in zip(indices, indices[1:])), "Invalid native-step frame indices")
    require(len(record["first_frame_after_step_samples_ms"]) == steps, "Native-step samples missing")
    require(all(abs(record["first_frame_after_step_samples_ms"][n] - wall[i]) <= .0001 for n, i in enumerate(indices)), "Native-step frames differ from measured frame stream")
    return {"first_after_step": {name: {"median_ms": percentile([record[name][i] for i in indices], .5),
                                     "p95_ms": percentile([record[name][i] for i in indices], .95),
                                     "p99_ms": percentile([record[name][i] for i in indices], .99)} for name in phases + nested},
            "warm_rotation": {name: {"median_ms": percentile(record[name][rotations:rotations*2], .5),
                                    "p95_ms": percentile(record[name][rotations:rotations*2], .95),
                                    "p99_ms": percentile(record[name][rotations:rotations*2], .99)} for name in phases + nested}}


NATIVE_STEP_LABELS = ('move', 'post_action_checks', 'view_center', 'reset_light_level',
                      'build_map_cache', 'invalidate_visibility_cache', 'update_visibility_cache',
                      'refresh_return', 'render_call', 'outer_tail')


def native_submap_update(row, *, required=False):
    """One synchronous Native call; nested/incomplete captures never qualify."""
    value = row.get('native_submap_updates')
    if value is None and not required:
        return dict(available=False, reason='Historical report has no internal submap scopes')
    require(type(value) is dict, 'Native submap receipt missing')
    require(type(value.get('schema')) is int and value['schema'] == 1
            and value.get('clock') == 'std::chrono::steady_clock' and value.get('units') == 'milliseconds'
            and value.get('diagnostic_probe_overhead_included') is True, 'Invalid Native submap clock/schema')
    require(value.get('claimed') is True, 'Native submap capture not claimed')
    for name in ('reused_storage', 'nested_capture', 'counter_overflow'):
        require(value.get(name) is False, 'Invalid Native submap capture: ' + name)
    for name, expected in (('invocations', 1), ('recorded', 1), ('dropped', 0), ('max_depth', 1),
                           ('exceptional_invocations', 0), ('protocol_errors', 0)):
        require(type(value.get(name)) is int and value[name] == expected, 'Invalid Native submap count: ' + name)
    events = value.get('events')
    require(type(events) is list and len(events) == 1 and type(events[0]) is dict, 'Invalid Native submap event matrix')
    event = events[0]
    for name, expected in (('ordinal', 0), ('depth', 1), ('parent_ordinal', -1)):
        require(type(event.get(name)) is int and event[name] == expected, 'Nested or misassigned Native submap event')
    require(event.get('shift_known') is True and event.get('complete') is True
            and event.get('exceptional') is False and event.get('z_level_changed') is False,
            'Incomplete or exceptional Native submap event')
    for name in ('input_xy', 'output_xy', 'shift_xy'):
        require(type(event.get(name)) is list and len(event[name]) == 2
                and all(type(v) is int and -(1 << 31) <= v < 1 << 31 for v in event[name]), 'Invalid Native submap coordinates')
    require(all(a-b == s*12 for a,b,s in zip(event['input_xy'], event['output_xy'], event['shift_xy']))
            and all(60 <= v < 72 for v in event['output_xy']), 'Native submap rebase identity failed')
    labels = ['entry', 'route', 'zero_position', 'zero_overmap', 'exit'] if event['shift_xy'] == [0, 0] else [
        'entry', 'route', 'shift', 'rebase_load_npcs', 'invalidate', 'build', 'spawn', 'overmap', 'exit']
    offsets = event.get('boundary_offsets_ms')
    require(event.get('boundary_labels') == labels and type(offsets) is list and len(offsets) == len(labels),
            'Missing or misassigned Native submap boundaries')
    require(all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in offsets)
            and all(a <= b for a,b in zip(offsets, offsets[1:]))
            and offsets[-1] <= row['phase_wall_ms'][0] + .003, 'Overlapping or uncontained Native submap scopes')
    return dict(available=True, input_xy=event['input_xy'], output_xy=event['output_xy'],
                shift_xy=event['shift_xy'], elapsed_ms=offsets[-1]-offsets[0],
                phases={a+'_to_'+b: y-x for a,b,x,y in zip(labels, labels[1:], offsets, offsets[1:])},
                scope='Synchronous wall scopes include diagnostic probes; not independent GPU/CPU execution')


NATIVE_MAP_SHIFT_LABELS = ('entry', 'setup', 'unload', 'origin_traps_remote', 'vehicle_zones',
                           'cache_rebase', 'grid_copy_load', 'vehicle_levels_remote', 'support_rebase',
                           'actualize', 'reconcile', 'exit')


def native_map_shifts(row, parent, *, required=False):
    """Bounded children partition map::shift; never add them to their parent."""
    value = row.get('native_map_shifts')
    if value is None and not required:
        return dict(available=False, reason='Historical report has no map::shift child scopes')
    require(parent['available'], 'Native map-shift parent receipt missing')
    require(type(value) is dict, 'Native map-shift receipt missing')
    require(type(value.get('schema')) is int and value['schema'] == 1
            and value.get('clock') == 'std::chrono::steady_clock' and value.get('units') == 'milliseconds'
            and value.get('diagnostic_probe_overhead_included') is True, 'Invalid Native map-shift clock/schema')
    dx, dy = parent['shift_xy']
    expected = max(abs(dx), abs(dy))
    require(expected <= 32, 'Native map-shift route exceeds bounded capture')
    for name, count in (('invocations', expected), ('recorded', expected), ('dropped', 0),
                        ('max_depth', int(expected != 0)), ('exceptional_invocations', 0)):
        require(type(value.get(name)) is int and value[name] == count, 'Invalid Native map-shift count: ' + name)
    events = value.get('events')
    require(type(events) is list and len(events) == expected, 'Incomplete Native map-shift event matrix')
    update = row['native_submap_updates']['events'][0]
    route_start = update['boundary_offsets_ms'][1]
    route_end = update['boundary_offsets_ms'][2] if expected else route_start
    prior_end, elapsed, children = route_start, 0.0, []
    rounding = .000003  # Three nanoseconds; serialized offsets have 1 ns quantum.
    remaining = [dx, dy]
    for ordinal, event in enumerate(events):
        require(type(event) is dict, 'Invalid Native map-shift event')
        for name, count in (('ordinal', ordinal), ('depth', 1), ('parent_ordinal', -1), ('parent_update_ordinal', 0)):
            require(type(event.get(name)) is int and event[name] == count, 'Nested or misassigned Native map-shift event')
        require(event.get('route_accepted') is True and event.get('complete') is True
                and event.get('exceptional') is False, 'Incomplete or exceptional Native map-shift event')
        direction = [max(-1, min(1, v)) for v in remaining]
        require(type(event.get('shift_xy')) is list and len(event['shift_xy']) == 2
                and all(type(v) is int for v in event['shift_xy']) and event['shift_xy'] == direction,
                'Native map-shift direction/order differs from the accepted parent route')
        remaining = [a-b for a,b in zip(remaining, direction)]
        offsets = event.get('boundary_offsets_ms')
        require(event.get('boundary_labels') == list(NATIVE_MAP_SHIFT_LABELS) and type(offsets) is list
                and len(offsets) == len(NATIVE_MAP_SHIFT_LABELS), 'Missing or misassigned Native map-shift boundaries')
        require(all(type(v) in (int, float) and math.isfinite(v) and v >= 0 for v in offsets)
                and all(a <= b for a,b in zip(offsets, offsets[1:]))
                and offsets[0] >= prior_end - rounding and offsets[-1] <= route_end + rounding,
                'Overlapping or uncontained Native map-shift children')
        wall = offsets[-1] - offsets[0]
        elapsed += wall; prior_end = offsets[-1]
        children.append(dict(shift_xy=direction, elapsed_ms=wall,
                             phases={a+'_to_'+b: y-x for a,b,x,y in zip(NATIVE_MAP_SHIFT_LABELS,
                                     NATIVE_MAP_SHIFT_LABELS[1:], offsets, offsets[1:])}))
    gap = route_end - route_start - elapsed
    require(remaining == [0, 0] and gap >= -rounding, 'Native map-shift child accounting exceeds its parent')
    return dict(available=True, calls=expected, children=children, parent_wall_ms=route_end-route_start,
                child_wall_ms=elapsed, parent_gap_wall_ms=gap,
                scope='Inclusive synchronous child wall scopes; probes and cleanup included; never add to parent time')


def native_step_phases(record, *, required=True, require_submaps=False, require_shifts=False):
    """Adjacent Native-call wall scopes; render is inclusive of its inner frame."""
    receipt = record.get('native_step_phases')
    if receipt is None and not required:
        return dict(available=False, reason='Historical report has no action/refresh split')
    require(type(receipt) is dict, 'Native action phase receipt missing')
    require(receipt.get('clock') == 'std::chrono::steady_clock' and receipt.get('units') == 'milliseconds',
            'Unsupported Native action phase clock')
    require(receipt.get('phase_labels') == list(NATIVE_STEP_LABELS), 'Native action phase order changed')
    count = record['native_steps']
    require(type(count) is int and 0 < count <= 64, 'Invalid Native action count')
    rows = receipt['samples']
    require(type(rows) is list and len(rows) == count, 'Incomplete Native action phase matrix')
    indices, frames, ready = record['first_frame_indices'], record['frame_samples_ms'], record['first_ready_samples_ms']
    require(len(indices) == len(ready) == count and all(type(i) is int for i in indices)
            and all(a < b for a, b in zip(indices, indices[1:])), 'Invalid Native action frame mapping')
    scalar = lambda value: type(value) in (int, float) and math.isfinite(value)
    route = ((1, 0, 0), (0, -1, 0), (-1, 0, 0), (0, 1, 0))
    previous_to = None
    submaps, shifts = [], []
    for step, row in enumerate(rows):
        require(type(row['step']) is int and row['step'] == step, 'Reordered Native action samples')
        index = row['frame_index']
        require(type(index) is int and index == indices[step] and 0 <= index < len(frames), 'Native action/frame mismatch')
        offsets, durations = row['boundary_offsets_ms'], row['phase_wall_ms']
        require(type(offsets) is list and len(offsets) == 11 and all(scalar(v) and v >= 0 for v in offsets)
                and offsets[0] == 0 and all(a <= b for a, b in zip(offsets, offsets[1:])), 'Invalid Native action boundaries')
        require(type(durations) is list and len(durations) == 10 and all(scalar(v) and v >= 0 for v in durations),
                'Invalid Native action durations')
        require(all(abs(b - a - duration) <= .0001 for a, b, duration in zip(offsets, offsets[1:], durations)),
                'Native action scopes overlap or omit work')
        for name in ('first_ready_wall_ms', 'first_frame_wall_ms', 'render_bookkeeping_wall_ms', 'residual_wall_ms'):
            require(scalar(row[name]), 'Nonfinite Native action accounting')
        outer, inner = row['first_ready_wall_ms'], row['first_frame_wall_ms']
        require(scalar(ready[step]) and scalar(frames[index]) and inner > 0 and outer > 0
                and abs(outer - ready[step]) <= .0001 and abs(inner - frames[index]) <= .0001,
                'Native action totals differ from the original samples')
        require(abs(outer - offsets[-1]) <= .003 and abs(row['residual_wall_ms']) <= .003
                and abs(row['residual_wall_ms'] - (outer - sum(durations))) <= .0001,
                'Unconserved Native action total or clamped residual')
        require(durations[8] >= inner - .003 and row['render_bookkeeping_wall_ms'] >= -.003
                and abs(row['render_bookkeeping_wall_ms'] - (durations[8] - inner)) <= .0001,
                'Native render/frame scopes overlap incorrectly')
        for name in ('from', 'to'):
            position = row[name]
            require(type(position) is list and len(position) == 3
                    and all(type(v) is int and -(1 << 31) <= v < 1 << 31 for v in position), 'Invalid Native action position')
        require(tuple(b - a for a, b in zip(row['from'], row['to'])) == route[step % 4]
                and (previous_to is None or row['from'] == previous_to), 'Native movement route changed')
        previous_to = row['to']
        require(all(type(row[name]) is int and -(1 << 31) <= row[name] < 1 << 31 for name in ('moves_before', 'moves_after'))
                and row['moves_after'] < row['moves_before'], 'Native movement did not consume action cost')
        submap = native_submap_update(row, required=require_submaps)
        if submap['available'] and submaps and submaps[-1]['available']:
            require(submap['input_xy'] == [p+d for p,d in zip(submaps[-1]['output_xy'], route[step % 4])],
                    'Native submap action mapping changed')
        submaps.append(submap)
        shifts.append(native_map_shifts(row, submap, required=require_shifts))
    require(all(s['available'] for s in submaps) or not any(s['available'] for s in submaps), 'Partial Native submap matrix')
    require(all(s['available'] for s in shifts) or not any(s['available'] for s in shifts), 'Partial Native map-shift matrix')
    return dict(available=True, clock=receipt['clock'], units='milliseconds', samples=count,
                phases={name: dict(median_ms=percentile([row['phase_wall_ms'][i] for row in rows], .5),
                                   p95_ms=percentile([row['phase_wall_ms'][i] for row in rows], .95),
                                   p99_ms=percentile([row['phase_wall_ms'][i] for row in rows], .99))
                        for i, name in enumerate(NATIVE_STEP_LABELS)},
                render_bookkeeping_median_ms=percentile([row['render_bookkeeping_wall_ms'] for row in rows], .5),
                max_abs_accounting_residual_ms=max(abs(row['residual_wall_ms']) for row in rows),
                submap_updates=dict(available=all(s['available'] for s in submaps), samples=submaps),
                map_shifts=dict(available=all(s['available'] for s in shifts), samples=shifts),
                scope='Synchronous Native-call wall times; render_call includes first_frame, never add both; not on-core CPU or GPU execution')


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
        verify_payload_file_matrix(bundle["files"], entries, source_tree_paths(args.expected_source))
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
        action_phases = native_step_phases(record, require_submaps=True, require_shifts=True)
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
                            "native_move_to_first_ready": ready, "native_action_phases": action_phases, "cold_rotation": cold,
                            "warm_rotation": warm, "frame_phases": phases, "device": record["device"],
                            "backend": record["backend"],
                            "software_device": record["software_device"]})
    require(len(timings) == 6, "GPU matrix incomplete")
    result = {"success": True, "version": args.expected_version, "build": args.build,
              "source_commit": args.expected_source, "workflow_run": args.expected_run,
              "installer": archive.name, "installer_sha256": digest(raw),
              "host_sha256": release["host_sha256"], "module_sha256": release["module_sha256"],
              "verified_payload_files": len(bundle["files"]), "manual_playthrough": False,
              "facade": facade, "horizon": horizon, "geometry": geometry, "gpu_timings": timings,
              "clean_focused_process_exits": len(stress), "exclusive_movement_phases_verified": True,
              "exclusive_native_action_phases_verified": True,
              "timing_scope": "CI device; first_render_frame excludes native avatar_action/refresh; native_move_to_first_ready includes both. Native action phases are adjacent synchronous wall scopes with clock-probe overhead, not on-core CPU or GPU execution."}
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
