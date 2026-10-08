#!/usr/bin/env python3
"""Regenerate canonical Survivor and smoke snapshots in the legacy payload.

Only replaces two known embedded base64 literals; no PowerShell transforms,
Host patches, or installer behavior are altered. Use --check in validation
and --write when either authoritative source file intentionally changes.
"""
import argparse
import base64
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = ROOT / "payload/SURVIVOR_0911_0915_v8.7.6.8.ps1"
SNAPSHOTS = [
    (
        re.compile(r"(\[IO\.File\]::WriteAllBytes\(\$spPath,\[Convert\]::FromBase64String\(')([A-Za-z0-9+/=]+)('\)\))"),
        ROOT / "mods/SurvivorProgression/src/survivor_progression.cpp",
    ),
    (
        re.compile(r"(Write-NcmmCanonicalPayloadFile 'tests\\smoke_host\.cpp' ')([A-Za-z0-9+/=]+)(')"),
        ROOT / "tests/smoke_host.cpp",
    ),
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true", help="update embedded snapshots")
    parser.add_argument("--check", action="store_true", help="verify snapshots (default)")
    opts = parser.parse_args()
    if opts.write and opts.check:
        parser.error("choose --write or --check")
    raw = PAYLOAD.read_bytes()
    # Preserve CRLF/LF bytes exactly in unrelated payload regions.
    document = raw.decode("utf-8-sig")
    changed = []
    for pattern, source in SNAPSHOTS:
        matches = list(pattern.finditer(document))
        if len(matches) != 1:
            raise RuntimeError(f"Expected one canonical snapshot for {source.name}; found {len(matches)}")
        match = matches[0]
        actual = base64.b64decode(match.group(2), validate=True)
        expected = source.read_bytes()
        if actual == expected:
            continue
        changed.append(source.name)
        if opts.write:
            encoded = base64.b64encode(expected).decode("ascii")
            document = document[:match.start(2)] + encoded + document[match.end(2):]
    if changed and not opts.write:
        raise RuntimeError("Canonical payload snapshots stale: " + ", ".join(changed) +
                           "; run python ci/sync_survivor_payload.py --write and regenerate package integrity")
    if opts.write and changed:
        bom = b"\xef\xbb\xbf" if raw.startswith(b"\xef\xbb\xbf") else b""
        PAYLOAD.write_bytes(bom + document.encode("utf-8"))
    print(f"Survivor canonical snapshots: PASS ({len(SNAPSHOTS)} checked" +
          (f"; updated {', '.join(changed)}" if changed else "; already synchronized") + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
