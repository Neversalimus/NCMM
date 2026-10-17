"""Choose the least expensive FPV tier that covers the changed responsibilities."""
import argparse
import json
import os
from pathlib import Path

TIERS = ("quick", "integration", "full")
FPV = "experiments/FirstPersonView/"
BRIDGE = "experiments/graphics_bridge/"


def tier(path):
    path = path.replace("\\", "/")
    if path.endswith(".md") or path.startswith(FPV + "qualification/"):
        return "quick"
    if path.startswith(("host_patch/", "sdk/", "runtime/", "payload/")):
        return "full"
    if path.startswith((BRIDGE + "ncmm_gpu", BRIDGE + "apply_graphics_bridge.py")):
        return "full"
    if path.startswith(BRIDGE):
        return "quick" if path.startswith(BRIDGE + "test_") else "integration"
    if path.startswith(FPV + "tests/"):
        return "quick"
    if path.startswith(FPV):
        if path.startswith(FPV + "shaders/") or path[len(FPV):] in (
                "CMakeLists.txt", "mod.json", "Build-FirstPersonInstaller.ps1", "compile_gpu_shaders.py"):
            return "full"
        return "integration"
    if path in (".github/workflows/ncmm-first-person-prototype.yml",
                ".github/workflows/ncmm-gpu-device.yml", "ci/fpv_qualification.py"):
        return "full"
    return "quick"


def plan(paths, requested=None):
    required = max((tier(path) for path in paths), key=TIERS.index, default="quick")
    selected = requested or required
    return {"required": required, "selected": selected,
            "native": str(TIERS.index(selected) >= 1).lower(),
            "full": str(selected == "full").lower(),
            "sufficient": str(TIERS.index(selected) >= TIERS.index(required)).lower()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--files", type=Path, required=True)
    args = parser.parse_args()
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
    requested = event.get("inputs", {}).get("qualification")
    pr = event.get("pull_request", {})
    for candidate in reversed(TIERS[1:]):
        if f"<!-- fp-qa:{candidate}@{pr.get('head', {}).get('sha')} -->" in (pr.get("body") or ""):
            required = plan(json.loads(args.files.read_text(encoding="utf-8")))["required"]
            requested = max((required, candidate), key=TIERS.index)
            break
    result = plan(json.loads(args.files.read_text(encoding="utf-8")), requested)
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        for key, value in result.items():
            output.write(f"{key}={value}\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
