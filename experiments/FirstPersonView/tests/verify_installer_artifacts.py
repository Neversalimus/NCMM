"""Verify qualified First Person installer artifacts and stage direct downloads."""
import argparse
import functools
import hashlib
import io
import json
import math
import os
import pathlib
import re
import stat
import statistics
import subprocess
import zipfile

PINS = {
    "1040": "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83",
    "1807": "074aa98bd5be3de4c35f154082db32a0e63bb0f1",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


@functools.lru_cache(maxsize=None)
def api(endpoint):
    result = subprocess.run(
        ["gh", "api", endpoint], check=True, capture_output=True, text=True
    )
    return json.loads(result.stdout)


def catalog(archive):
    entries = {}
    folded = set()
    for item in archive.infolist():
        name = item.filename.replace("\\", "/")
        path = pathlib.PurePosixPath(name)
        require(
            name and not path.is_absolute() and ".." not in path.parts
            and ":" not in name and not stat.S_ISLNK(item.external_attr >> 16),
            "Unsafe ZIP member: " + name,
        )
        key = name.rstrip("/").casefold()
        require(name not in entries and key not in folded, "Duplicate ZIP member: " + name)
        entries[name] = item
        folded.add(key)
    return entries


def source_matches(repo, source, runtime_head):
    runtime = api("repos/" + repo + "/git/commits/" + runtime_head)
    commit = api("repos/" + repo + "/git/commits/" + source)
    require(
        source == runtime_head or (
            runtime_head in [parent["sha"] for parent in commit["parents"]]
            and commit["tree"]["sha"] == runtime["tree"]["sha"]
        ),
        "Installer does not contain the qualified runtime tree",
    )


def movement_summary(outer, entries):
    matches = [
        name for name in entries
        if pathlib.PurePosixPath(name).name == "first-person-movement-benchmark.json"
    ]
    require(len(matches) == 1, "Movement evidence is missing or ambiguous")
    report = json.loads(outer.read(entries[matches[0]]).decode("utf-8-sig"))
    require(
        report.get("success") is True and report.get("actual_cdda_sdl") is True
        and report.get("simulation_turns_advanced") == 0
        and len(report.get("records", [])) == 12,
        "Movement evidence is incomplete",
    )
    summary = []
    for record in report["records"]:
        require(record["native_steps"] == 12, "Native movement count differs")
        require(len(record["first_ready_samples_ms"]) == 12, "Step samples differ")
        require(
            len(record["cold_rotation_samples_ms"]) == 48
            and len(record["warm_rotation_samples_ms"]) == 48,
            "Rotation samples differ",
        )
        tags = record["frame_renderers_gpu"]
        require(tags and all(tag == (1 if record["renderer"] == "gpu" else 0) for tag in tags),
                "Requested renderer fell back")
        row = {key: record[key] for key in (
            "scene", "renderer", "width", "height", "device", "software_device",
            "first_ready_median_ms", "first_ready_p95_ms", "easing_median_ms",
            "cold_rotation_median_ms", "cold_rotation_p95_ms",
            "warm_rotation_median_ms", "warm_rotation_p95_ms",
        )}
        for key, value in row.items():
            if key.endswith("_ms"):
                require(math.isfinite(value) and value >= 0, "Non-finite frame timing")
        row["warm_visibility_median_ms"] = statistics.median(
            record["visibility_samples_ms"][48:96]
        )
        row["geometry_revision_count"] = len(set(record["geometry_revisions"]))
        row["source_cache_misses_max"] = max(record["source_cache_misses"])
        row["geometry_cache_misses_max"] = max(record["geometry_cache_misses"])
        row["native_sight_evaluations_max"] = max(record["native_sight_evaluations"])
        row["material_evaluations_max"] = max(record["native_material_evaluations"])
        row["material_cache_hits_max"] = max(record["native_material_cache_hits"])
        summary.append(row)
    return summary


def verify(build, artifact, args, repo):
    require(artifact.get("digest", "").startswith("sha256:"), "Artifact digest missing")
    destination = pathlib.Path("artifact-review")
    destination.mkdir(exist_ok=True)
    archive_path = destination / (build + "-source.zip")
    with archive_path.open("wb") as output:
        subprocess.run(
            ["gh", "api", "repos/" + repo + "/actions/artifacts/" + str(artifact["id"]) + "/zip"],
            stdout=output, check=True,
        )
    require(sha256(archive_path.read_bytes()) == artifact["digest"][7:],
            "GitHub artifact digest mismatch")
    name = "NCMM_FirstPersonView_" + args.version + "_TEST_" + build + "_Windows_x64.zip"
    with zipfile.ZipFile(archive_path) as outer:
        outer_entries = catalog(outer)
        candidates = [
            key for key in outer_entries if pathlib.PurePosixPath(key).name == name
        ]
        require(len(candidates) == 1, "Installer ZIP missing or ambiguous")
        require(outer_entries[candidates[0]].file_size < 256 * 1024 * 1024,
                "Unexpectedly large installer ZIP")
        data = outer.read(outer_entries[candidates[0]])
        movement = movement_summary(outer, outer_entries)
    with zipfile.ZipFile(io.BytesIO(data)) as inner:
        entries = catalog(inner)
        manifests = [key for key in entries if key.endswith("release-manifest.json")]
        require(len(manifests) == 1, "Release manifest missing or ambiguous")
        prefix = manifests[0][:-len("release-manifest.json")]

        def read(relative):
            return inner.read(entries[prefix + relative.replace("\\", "/")])

        release = json.loads(read("release-manifest.json").decode("utf-8-sig"))
        bundle = json.loads(read("payload/first-person-bundle.json").decode("utf-8-sig"))
        require(release["installer_revision"] == args.version, "Wrong installer version")
        require(release["status"] == "experimental-not-certified", "Wrong release status")
        require(str(release["workflow_run"]) == args.source_run, "Wrong source workflow")
        require(bundle["source_commit"] == release["engine_source"] == PINS[build],
                "Wrong engine pin")
        require(bundle["ncmm_source_commit"] == release["source_commit"],
                "Source provenance differs")
        require(not bundle.get("diagnostic_only"), "Diagnostic bundle cannot be delivered")
        source_matches(repo, release["source_commit"], args.runtime_head)
        require(release["recommended_entry"] == "NCMM_Setup_FirstPerson_TEST.exe",
                "Wrong installer entry")
        require(sha256(read(release["recommended_entry"])) == release["installer_sha256"],
                "Setup SHA256 mismatch")
        for relative, digest in bundle["files"].items():
            require(re.fullmatch("[0-9a-f]{64}", digest), "Invalid payload digest")
            require(sha256(read("payload/" + relative)) == digest,
                    "Payload SHA256 mismatch: " + relative)
        host = bundle["files"]["host/cataclysm-tiles.ncmm.exe"]
        module = bundle["files"]["code_mods/FirstPersonView/ncmm_mod.dll"]
        require(host == bundle["host_sha256"] == release["host_sha256"],
                "Host receipts differ")
        require(module == release["module_sha256"], "Module receipts differ")
        mod = json.loads(read("payload/code_mods/FirstPersonView/mod.json").decode("utf-8-sig"))
        require(mod["version"] == args.version + "-dev", "Wrong module version")
        native = json.loads(read("payload/host/first-person-source.json").decode("utf-8-sig"))
        require(native["source_commit"] == PINS[build], "Native source receipt pin differs")
        checked_inputs = 0
        for filename, digest in native["inputs"].items():
            require(filename == pathlib.PurePosixPath(filename).name,
                    "Unexpected native input path")
            relative = pathlib.Path("experiments/graphics_bridge") / filename
            if relative.is_file():
                content = subprocess.run(
                    ["git", "show", args.runtime_head + ":" + relative.as_posix()],
                    check=True, capture_output=True,
                ).stdout.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")
                require(sha256(content) == digest, "Native source input differs: " + filename)
                checked_inputs += 1
        require(checked_inputs >= 20, "Too few native source inputs verified")
        stage = pathlib.Path("delivery") / build
        stage.mkdir(parents=True, exist_ok=False)
        for key, item in entries.items():
            require(key.startswith(prefix), "Unexpected file outside package root")
            relative = key[len(prefix):]
            if not relative:
                continue
            target = stage.joinpath(*pathlib.PurePosixPath(relative).parts)
            require(target.resolve().is_relative_to(stage.resolve()), "Extraction escaped package")
            if item.is_dir() or key.endswith("/"):
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(inner.read(item))
    return {
        "build": build, "version": args.version, "source_run": args.source_run,
        "runtime_head": args.runtime_head, "source_commit": release["source_commit"],
        "original_installer_zip_sha256": sha256(data),
        "source_artifact_sha256": artifact["digest"][7:],
        "host_sha256": host, "module_sha256": module,
        "setup_sha256": release["installer_sha256"],
        "payload_hashes_verified": len(bundle["files"]),
        "native_inputs_verified": checked_inputs,
        "delivery": "Repacked qualified files; payload and Setup bytes unchanged",
        "hardware_playthrough": False,
        "movement": movement,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-run", required=True)
    parser.add_argument("--runtime-head", required=True)
    parser.add_argument("--version", default="0.12.4")
    args = parser.parse_args()
    require(re.fullmatch("[0-9]+", args.source_run), "Invalid source workflow ID")
    require(re.fullmatch("[0-9a-f]{40}", args.runtime_head), "Invalid runtime SHA")
    require(re.fullmatch("[0-9]+[.][0-9]+[.][0-9]+", args.version), "Invalid version")
    repo = os.environ["GITHUB_REPOSITORY"]
    require(repo == "Neversalimus/NCMM", "Unexpected repository")
    run = api("repos/" + repo + "/actions/runs/" + args.source_run)
    require(run["conclusion"] == "success" and run["head_sha"] == args.runtime_head,
            "Source native qualification did not pass at the expected head")
    subprocess.run(
        ["git", "fetch", "--no-tags", "origin", args.runtime_head, "--depth=1"], check=True
    )
    artifacts = api("repos/" + repo + "/actions/runs/" + args.source_run + "/artifacts?per_page=100")
    results = []
    for build in PINS:
        matches = [
            artifact for artifact in artifacts["artifacts"]
            if artifact["name"] == "first-person-installer-" + build + "-Windows-x64"
            and not artifact["expired"]
        ]
        require(len(matches) == 1, "Installer artifact missing or ambiguous: " + build)
        results.append(verify(build, matches[0], args, repo))
    report = {"success": True, "results": results}
    pathlib.Path("delivery/verification.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    for result in results:
        print("FP_INSTALLER_REVIEW " + json.dumps(result, separators=(",", ":")))
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as output:
            output.write("Both installer packages passed source, payload, Setup and movement-evidence checks.\n\n")
            output.write("The delivery artifacts contain the extracted qualified installer files, ready to unzip once.\n\n")
            output.write("CI timings identify their actual adapter; they are not a hardware playthrough.\n")


if __name__ == "__main__":
    main()
