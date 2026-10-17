"""Reject misleading hardware evidence, especially success before a teardown crash."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import compare_movement_runs as compare


class HardwareEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bundle = self.root / "bundle"
        self.run = self.root / "run"
        files = {"host/cataclysm-tiles.ncmm.exe": b"host",
                 "code_mods/FirstPersonView/ncmm_mod.dll": b"module"}
        files.update({f"extra/{i}.txt": str(i).encode() for i in range(30)})
        hashes = {}
        for name, data in files.items():
            self.write(self.bundle / "payload" / name, data)
            hashes[name] = hashlib.sha256(data).hexdigest()
        self.identity = {"source_commit": "candidate", "installer_revision": "0.12.8",
                         "host_sha256": hashes["host/cataclysm-tiles.ncmm.exe"],
                         "module_sha256": hashes["code_mods/FirstPersonView/ncmm_mod.dll"]}
        self.save(self.bundle / "release-manifest.json", dict(self.identity, engine_source=compare.ENGINE))
        self.save(self.bundle / "payload/first-person-bundle.json",
                  {"ncmm_source_commit": "candidate", "source_commit": compare.ENGINE,
                   "host_sha256": self.identity["host_sha256"], "files": hashes})
        self.write(self.run / "game/cataclysm-tiles.ncmm.exe", b"host")
        self.write(self.run / "game/code_mods/FirstPersonView/ncmm_mod.dll", b"module")
        self.write(self.run / "game/data/json/weather_type.json", b"clear fixture")
        self.weather = patch.dict(compare.WEATHER, {"clear": hashlib.sha256(b"clear fixture").hexdigest()})
        self.weather.start()
        self.addCleanup(self.weather.stop)
        shared = {"ncmm_commit": "candidate", "host_sha256": self.identity["host_sha256"],
                  "module_sha256": self.identity["module_sha256"]}
        self.process = dict(shared, engine_commit=compare.ENGINE, exit_signed=0, crash_dumps=0,
                            debugger_error=None, wrapper_failure=None, last_progress="movement_complete")
        self.provenance = dict(shared, source_commit=compare.ENGINE, success=True,
                               movement_benchmark=True, manual_playthrough=False, udp_requested=True)
        records = []
        for scene in compare.SCENES:
            for renderer in ("cpu", "gpu"):
                for width, height in ((640, 480), (1768, 1084)):
                    record = {"scene": scene, "renderer": renderer, "width": width, "height": height,
                              "native_steps": 12, "tileset": "UndeadPeople_0J_Hybrid_v3", "native_art": True,
                              "frame_samples_ms": [4.0] * 108, "first_frame_after_step_samples_ms": [4.0] * 12,
                              "first_ready_samples_ms": [9.0] * 12, "warm_rotation_samples_ms": [4.0] * 48,
                              "frame_renderers_gpu": [int(renderer == "gpu")] * 108,
                              "device": "NVIDIA GeForce RTX 4090", "backend": "direct3d12", "software_device": False,
                              "exclusive_phases_available": True, "first_frame_indices": list(range(96, 108))}
                    for name in ("native_tiles", "perspective", "present", "unattributed"):
                        record[name + "_samples_ms"] = [1.0] * 108
                    for name in ("extract", "callback", "gpu_geometry", "gpu_resource", "gpu_upload", "gpu_record", "gpu_submit"):
                        record[name + "_samples_ms"] = [0.0] * 108
                    records.append(record)
        self.report = {"success": True, "actual_cdda_sdl": True, "simulation_turns_advanced": 0,
                       "geometry_regressions": {"success": True, "captures": 8},
                       "facade_regression": {"normal_vision": True}, "horizon_regression": {"near_overlap": False},
                       "records": records}
        self.save(self.run / "evidence/udp-asset-sha256.json",
                  [{"file": f"{i}.png", "sha256": hashlib.sha256(str(i).encode()).hexdigest()} for i in range(12)])
        self.persist()

    @staticmethod
    def write(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def save(self, path, data):
        self.write(path, json.dumps(data).encode())

    def persist(self):
        self.save(self.run / "benchmark-provenance.json", self.provenance)
        self.save(self.run / "evidence/first-person-process-result.json", self.process)
        self.save(self.run / "evidence/first-person-movement-benchmark.json", self.report)

    def read(self):
        return compare.read_run(self.run, self.identity, "clear", True)

    def test_complete_exact_package_and_run(self):
        self.assertEqual(compare.payload(self.bundle, "candidate"), self.identity)
        result, udp = self.read()
        self.assertEqual(len(result["records"]), 12)
        self.assertEqual(result["full_size_gpu_first_under_15_ms"], 36)
        self.assertEqual(len(udp), 12)
        for row in result["records"]:
            if row["renderer"] == "cpu":
                self.assertFalse(any(key.startswith("gpu_") for group in row["phases"].values() for key in group))

    def test_successful_report_does_not_mask_bad_process(self):
        valid = copy.deepcopy(self.process)
        for key, value in (("exit_signed", -1073741819), ("crash_dumps", 1),
                           ("debugger_error", "attach failed"), ("wrapper_failure", "timeout"),
                           ("last_progress", "bootstrap_complete")):
            with self.subTest(key=key):
                self.process = dict(valid, **{key: value})
                self.persist()
                with self.assertRaisesRegex(ValueError, "did not finish cleanly"):
                    self.read()

    def test_mixed_package_or_engine_provenance_rejected(self):
        for target, key in ((self.process, "host_sha256"), (self.process, "engine_commit"),
                            (self.provenance, "ncmm_commit"), (self.provenance, "module_sha256")):
            with self.subTest(key=key):
                old = target[key]
                target[key] = "different"
                self.persist()
                with self.assertRaisesRegex(ValueError, "identity|pin"):
                    self.read()
                target[key] = old

    def test_changed_installed_binary_rejected(self):
        self.write(self.run / "game/cataclysm-tiles.ncmm.exe", b"other host")
        with self.assertRaisesRegex(ValueError, "binaries changed"):
            self.read()

    def test_changed_packaged_file_rejected(self):
        self.write(self.bundle / "payload/extra/2.txt", b"changed")
        with self.assertRaisesRegex(ValueError, "Payload changed"):
            compare.payload(self.bundle, "candidate")

    def test_missing_or_duplicate_scene_rejected(self):
        original = copy.deepcopy(self.report["records"])
        for records in (original[:-1], original[:-1] + [original[0]]):
            self.report["records"] = records
            self.persist()
            with self.assertRaisesRegex(ValueError, "movement matrix"):
                self.read()

    def test_cpu_fallback_or_software_gpu_rejected(self):
        gpu = next(r for r in self.report["records"] if r["renderer"] == "gpu")
        gpu["frame_renderers_gpu"][96] = 0
        self.persist()
        with self.assertRaisesRegex(ValueError, "Renderer changed"):
            self.read()
        gpu["frame_renderers_gpu"][96] = 1
        gpu["software_device"] = True
        self.persist()
        with self.assertRaisesRegex(ValueError, "hardware device"):
            self.read()

    def test_wrong_weather_or_duplicate_assets_rejected(self):
        self.write(self.run / "game/data/json/weather_type.json", b"other weather")
        with self.assertRaisesRegex(ValueError, "Weather fixture changed"):
            self.read()
        self.write(self.run / "game/data/json/weather_type.json", b"clear fixture")
        self.save(self.run / "evidence/udp-asset-sha256.json", [{"file": "same.png", "sha256": "same"}] * 12)
        with self.assertRaisesRegex(ValueError, "asset receipt"):
            self.read()

    def test_missing_phase_stream_or_selected_step_mismatch_rejected(self):
        record = self.report["records"][0]
        record["exclusive_phases_available"] = False
        self.persist()
        with self.assertRaisesRegex(ValueError, "phases missing"):
            self.read()
        record["exclusive_phases_available"] = True
        record["first_frame_after_step_samples_ms"][0] = 1.0
        self.persist()
        with self.assertRaisesRegex(ValueError, "differ from measured"):
            self.read()


if __name__ == "__main__":
    unittest.main()
