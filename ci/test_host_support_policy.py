#!/usr/bin/env python3
"""Regression checks for active baselines and fail-closed Host retirement."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

from host_support_policy import filter_tags, load_policy, prune_feed, require_active

ROOT = Path(__file__).resolve().parents[1]
RETIRED = "cdda-experimental-2026-09-23-0546"
OLD_SHA = "e262adb299a7613b4aedc5f12c08fe0413c56a84"
BASELINE = "cdda-experimental-2026-10-01-1040"
BASE_SHA = "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83"
SOURCE_ROOT = None


class HostSupportPolicyTests(unittest.TestCase):
    def setUp(self):
        self.policy = load_policy(ROOT / "ci/retired-hosts.json")

    def test_retired_identity_rejected(self):
        with self.assertRaisesRegex(ValueError, "Retired"):
            require_active(RETIRED, self.policy)
        with self.assertRaisesRegex(ValueError, "Retired"):
            require_active(BASELINE, self.policy, OLD_SHA)
        require_active(BASELINE, self.policy, BASE_SHA)

    def test_discovery_filters_deduplicates_and_preserves_order(self):
        newer = "cdda-experimental-2026-10-10-0420"
        self.assertEqual(filter_tags([RETIRED, newer, BASELINE, newer, "", "# comment"], self.policy),
                         [newer, BASELINE])
        self.assertEqual(filter_tags([RETIRED, RETIRED, ""], self.policy), [])
        self.assertEqual(filter_tags(["  " + BASELINE + "\r\n"], self.policy), [BASELINE])

    def test_invalid_identity_rejected(self):
        for tag in ("", "main", BASELINE + "\n", "latest", "../release"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                require_active(tag, self.policy)
        with self.assertRaises(ValueError):
            require_active(BASELINE, self.policy, "not-a-sha")
        with self.assertRaises(ValueError):
            filter_tags([BASELINE, "unexpected"], self.policy)

    def test_prune_both_retired_executables_and_mislabeled_source(self):
        old = {"upstream_tag": RETIRED, "source_commit": OLD_SHA}
        keep = {"upstream_tag": BASELINE, "source_commit": BASE_SHA, "host_sha256": "unchanged"}
        feed = {"schema": 1, "patch_revision": "keep", "hosts": {
            "old-plain": old, "old-sounds": old, "old-alias": {**old, "upstream_tag": BASELINE},
            "current": keep}}
        result = prune_feed(feed, self.policy)
        self.assertEqual(result, {"schema": 1, "patch_revision": "keep", "hosts": {"current": keep}})
        self.assertEqual(len(feed["hosts"]), 4, "Do not mutate caller data")
        self.assertEqual(prune_feed(result, self.policy), result)

    def test_malformed_policy_fails_closed(self):
        row = {"tag": RETIRED, "commit": OLD_SHA}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "policy.json"
            with self.assertRaises(OSError):
                load_policy(path)
            for data in (None, [], {"schema": True, "retired": []}, {"schema": 2},
                         {"schema": 1, "retired": None}, {"schema": 1, "retired": [None]},
                         {"schema": 1, "retired": [{"tag": "latest", "commit": OLD_SHA}]},
                         {"schema": 1, "retired": [{"tag": RETIRED, "commit": "bad"}]},
                         {"schema": 1, "retired": [row, row]}):
                path.write_text(json.dumps(data), encoding="utf-8")
                with self.subTest(data=data), self.assertRaises(ValueError):
                    load_policy(path)
            path.write_text('{broken', encoding="utf-8")
            with self.assertRaises(ValueError):
                load_policy(path)
            path.write_text('{"schema":1,"retired":[]}', encoding="utf-8")
            self.assertEqual(load_policy(path), (set(), set()))

    def test_invalid_feed_rejected(self):
        for feed in (None, {}, {"hosts": []}, {"hosts": {"bad": None}}):
            with self.subTest(feed=feed), self.assertRaises(ValueError):
                prune_feed(feed, self.policy)

    def test_cli_empty_selection_and_noop_feed(self):
        script = ROOT / "ci/host_support_policy.py"
        result = subprocess.run([sys.executable, str(script), "filter-tags"],
                                input=RETIRED + "\n", text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")
        result = subprocess.run([sys.executable, str(script), "check", "--tag", RETIRED],
                                text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "feed.json"
            original = b'{ "schema": 1, "hosts": {} }\n'
            path.write_bytes(original)
            result = subprocess.run([sys.executable, str(script), "prune-feed", str(path)],
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(path.read_bytes(), original)

    def test_active_source_defaults_and_feed(self):
        seeds = (ROOT / "ci/seed-hosts.txt").read_text().splitlines()
        self.assertEqual(seeds[0], BASELINE)
        self.assertEqual(filter_tags(seeds, self.policy), seeds)
        for path in (ROOT / ".github/workflows").glob("*.yml"):
            text = path.read_text(encoding="utf-8-sig")
            self.assertNotIn(RETIRED, text, str(path))
            self.assertNotIn(OLD_SHA, text, str(path))
        payload = (ROOT / "payload/SURVIVOR_0911_0915_v8.7.6.8.ps1").read_text(encoding="utf-8-sig")
        defaults = payload.split("$ErrorActionPreference", 1)[0]
        self.assertIn(BASE_SHA, defaults)
        self.assertNotIn(OLD_SHA, defaults)
        manifest = json.loads((ROOT / "compat/compatibility.manifest.json").read_text())
        self.assertEqual(manifest["exact_seed_adapter"], "cdda-2026-10-01-1040-3f7fb352")
        feed = json.loads((ROOT / "compat/feed/index.json").read_text())
        self.assertNotIn(OLD_SHA, [row["commit"] for row in feed["entries"]])
        self.assertIn(BASE_SHA, [row["commit"] for row in feed["entries"]])
        self.assertFalse((ROOT / "adapters/cdda_2026_09_23_0546.ps1").exists())
        self.assertFalse((ROOT / ".github/workflows/ncmm-ebm-test-installer.yml").exists())
        self.assertFalse((ROOT / "ci/ebm-test").exists())

    def test_discovery_and_publication_use_same_policy(self):
        workflow = (ROOT / ".github/workflows/ncmm-host.yml").read_text()
        for needle in ("ci/host_support_policy.py filter-tags", "ci/host_support_policy.py prune-feed",
                       'ci/host_support_policy.py check --tag "$REQUESTED_TAG"',
                       'ci/host_support_policy.py check --tag "$tag" --commit "$commit"'):
            self.assertIn(needle, workflow)
        for path in ("ci/host_support_policy.py", "ci/retired-hosts.json"):
            self.assertIn(path, (ROOT / "ci/patch-revision-files.txt").read_text())
            self.assertIn(path.replace("/", "\\"), (ROOT / "compat/package-files.txt").read_text())

    def test_exact_1040_reference_blobs(self):
        if SOURCE_ROOT is None:
            self.skipTest("Pass --source-root to verify against an actual pristine 1040 checkout")
        if (SOURCE_ROOT / ".git").exists():
            actual = subprocess.check_output(["git", "-C", str(SOURCE_ROOT), "rev-parse", "HEAD"], text=True).strip()
            self.assertEqual(actual, BASE_SHA)
        adapter = (ROOT / "adapters/cdda_2026_10_01_1040.ps1").read_text(encoding="utf-8-sig")
        refs = re.findall(r"'(src/[^']+)' = '([0-9a-f]{40})'", adapter)
        self.assertEqual(len(refs), 27)
        self.assertEqual(len({path for path, _ in refs}), 27)
        for path, expected in refs:
            data = (SOURCE_ROOT / path).read_bytes().replace(b"\r\n", b"\n")
            actual = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
            self.assertEqual(actual, expected, path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--source-root", type=Path)
    args, rest = parser.parse_known_args()
    SOURCE_ROOT = args.source_root
    unittest.main(argv=[sys.argv[0], *rest])
