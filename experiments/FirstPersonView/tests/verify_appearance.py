"""Validate paired actual-game input and report measured output differences.

Image differences are diagnostics, not an automatic assertion of art quality.
Original SDL captures remain available for visual review.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


def key(frame):
    return tuple(frame[name] for name in ("id", "angle", "distance_band", "lighting", "pose"))


def read_matrix(root, mode):
    report = json.loads((root / f"first-person-appearance-{mode}.json").read_text())
    assert report["actual_cdda_sdl"] and report["fixed_presentation_clock"]
    frames = {key(frame): frame for frame in report["frames"]}
    assert len(frames) == len(report["frames"]) >= 880, "duplicate/incomplete cases"
    ids = {item[0] for item in frames}
    assert len(ids) == 14 and "npc-clothed" in ids and "mon_ncmm_appearance_unknown" in ids
    for name in ids:
        idle = [item for item in frames if item[0] == name and item[-1] == "idle"]
        assert len(idle) == 8 * 3 * 2, f"incomplete directions/distances/lighting: {name}"
    return report, frames


def validate(root):
    before, old = read_matrix(root, "before-cpu")
    cpu, current = read_matrix(root, "after-cpu")
    gpu, accelerated = read_matrix(root, "after-gpu")
    assert old.keys() == current.keys() == accelerated.keys(), "paired cases differ"
    assert before["tileset"] == cpu["tileset"] == gpu["tileset"], "tileset differs"
    records = []
    for case, base in old.items():
        after, hardware = current[case], accelerated[case]
        for field in ("clock", "camera_yaw", "camera_pitch", "camera_x", "camera_y", "distance_cells",
                      "subject_flags", "subject_heading", "subject_height", "subject_width",
                      "subject_liters", "subject_kilograms", "observer_perception"):
            assert abs(base[field] - after[field]) < 1e-5 and abs(after[field] - hardware[field]) < 1e-5, f"input mismatch {case}: {field}"
        for field in ("source_width", "source_height", "source_pixel_hash"):
            assert base[field] == after[field] == hardware[field], f"source mismatch {case}: {field}"
        assert min(base["pixels"], after["pixels"], hardware["pixels"]) > 0
        paths = [root / frame["file"] for frame in (base, after, hardware)]
        images = []
        for path in paths:
            with Image.open(path) as image:
                images.append(np.asarray(image.convert("RGB"), dtype=np.int16))
        assert images[0].shape == images[1].shape == images[2].shape
        delta = np.abs(images[1] - images[2]) / 255
        records.append({"case": list(case), "files": [path.name for path in paths],
                        "before_after_rgb_mae": float(np.abs(images[0] - images[1]).mean() / 255),
                        "cpu_gpu_rgb_mae": float(delta.mean()),
                        "width": images[0].shape[1], "height": images[0].shape[0],
                        "sha256": [hashlib.sha256(path.read_bytes()).hexdigest() for path in paths]})
    return {"schema": 1, "actual_cdda_sdl": True, "player_fps": False,
            "tileset": before["tileset"], "paired_cases": len(records),
            "input_matched": True, "requires_visual_review": True,
            "maximum_cpu_gpu_rgb_mae": max(row["cpu_gpu_rgb_mae"] for row in records),
            "records": records}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = validate(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Paired real-game appearance: {result['paired_cases']} cases; "
          f"inputs matched; maximum CPU/GPU RGB MAE {result['maximum_cpu_gpu_rgb_mae']:.6f}")
