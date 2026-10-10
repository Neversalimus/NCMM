#!/usr/bin/env python3
"""Exclude retired engines from discovery/publication; never certify a new one.

The policy is fail-closed when missing or malformed. Historical release assets
and local game installations are deliberately outside this tool's scope.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Iterable

TAG = re.compile(r"cdda-experimental-\d{4}-\d{2}-\d{2}-\d{4}\Z")
SHA = re.compile(r"[0-9a-f]{40}\Z")


def load_policy(path: Path) -> tuple[set[str], set[str]]:
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict) or type(data.get("schema")) is not int or data["schema"] != 1:
        raise ValueError("Unsupported retired-Host policy schema")
    rows = data.get("retired")
    if not isinstance(rows, list):
        raise ValueError("Retired-Host policy must contain a retired array")
    tags: set[str] = set()
    commits: set[str] = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("Invalid retired-Host record")
        tag, commit = row.get("tag"), row.get("commit")
        if not isinstance(tag, str) or not TAG.fullmatch(tag):
            raise ValueError("Invalid retired-Host release tag")
        if not isinstance(commit, str) or not SHA.fullmatch(commit):
            raise ValueError("Invalid retired-Host source commit")
        if tag in tags:
            raise ValueError("Duplicate retired-Host release tag")
        tags.add(tag)
        commits.add(commit)
    return tags, commits


def require_active(tag: str, policy: tuple[set[str], set[str]], commit: str | None = None) -> None:
    if not TAG.fullmatch(tag) or (commit is not None and not SHA.fullmatch(commit)):
        raise ValueError("Invalid exact upstream tag/source identity")
    if tag in policy[0] or (commit is not None and commit in policy[1]):
        raise ValueError(f"Retired CDDA build is no longer qualified or published: {tag}")


def filter_tags(lines: Iterable[str], policy: tuple[set[str], set[str]]) -> list[str]:
    result: list[str] = []
    seen: set[str] = set()
    for line in lines:
        tag = line.strip()
        if not tag or tag.startswith("#"):
            continue
        if not TAG.fullmatch(tag):
            raise ValueError(f"Invalid upstream candidate tag: {tag}")
        if tag not in policy[0] and tag not in seen:
            result.append(tag)
            seen.add(tag)
    return result


def prune_feed(data: dict, policy: tuple[set[str], set[str]]) -> dict:
    if not isinstance(data, dict) or not isinstance(data.get("hosts"), dict):
        raise ValueError("Host feed must contain a hosts object")
    retained = {}
    for key, host in data["hosts"].items():
        if not isinstance(host, dict):
            raise ValueError("Invalid Host feed entry")
        if host.get("upstream_tag") not in policy[0] and host.get("source_commit") not in policy[1]:
            retained[key] = host
    return {**data, "hosts": retained}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=Path(__file__).with_name("retired-hosts.json"))
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("filter-tags")
    check = commands.add_parser("check")
    check.add_argument("--tag", required=True)
    check.add_argument("--commit")
    prune = commands.add_parser("prune-feed")
    prune.add_argument("path", type=Path)
    args = parser.parse_args()
    try:
        policy = load_policy(args.policy)
        if args.command == "filter-tags":
            for tag in filter_tags(sys.stdin, policy):
                print(tag)
        elif args.command == "check":
            require_active(args.tag, policy, args.commit)
        else:
            original = json.loads(args.path.read_text(encoding="utf-8-sig"))
            filtered = prune_feed(original, policy)
            if filtered != original:
                temporary = args.path.with_name(args.path.name + ".retirement.tmp")
                temporary.write_text(json.dumps(filtered, indent=2) + "\n", encoding="utf-8")
                temporary.replace(args.path)
    except (OSError, ValueError) as error:
        print(f"Host support policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
