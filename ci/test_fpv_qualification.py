import unittest
from fpv_qualification import plan


class QualificationPolicyTests(unittest.TestCase):
    def test_docs_and_evidence_are_quick(self):
        self.assertEqual(plan(["experiments/FirstPersonView/README.md",
                               "experiments/FirstPersonView/qualification/result.json"])["required"], "quick")

    def test_renderer_cache_and_extraction_need_both_hosts(self):
        for path in ("experiments/FirstPersonView/src/gpu_scene.hpp",
                     "experiments/graphics_bridge/ncmm_graphics_world.inc"):
            with self.subTest(path=path):
                self.assertEqual(plan([path])["native"], "true")

    def test_abi_device_shaders_and_installer_require_full(self):
        for path in ("sdk/ncmm_api.h", "host_patch/Apply-LegacyCharacterPoints.ps1",
                     "experiments/graphics_bridge/ncmm_gpu_resources.hpp",
                     "experiments/FirstPersonView/shaders/world.frag",
                     "experiments/FirstPersonView/Build-FirstPersonInstaller.ps1",
                     "experiments/FirstPersonView/CMakeLists.txt",
                     ".github/workflows/ncmm-fpv-linux-native.yml",
                     "experiments/graphics_bridge/native_library_bridge.py",
                     "experiments/graphics_bridge/ncmm_graphics_library.hpp",
                     "experiments/graphics_bridge/native_options_bridge.py"):
            with self.subTest(path=path):
                self.assertEqual(plan([path])["full"], "true")

    def test_tests_only_do_not_compile_native_hosts(self):
        self.assertEqual(plan(["experiments/FirstPersonView/tests/camera_motion_test.cpp"])["native"], "false")

    def test_explicit_quick_cannot_qualify_runtime_change(self):
        self.assertEqual(plan(["sdk/ncmm_api.h"], "quick")["sufficient"], "false")

    def test_full_always_keeps_native_release_gates(self):
        result = plan([], "full")
        self.assertEqual((result["native"], result["full"], result["sufficient"]), ("true", "true", "true"))

    def test_mixed_changes_take_strongest_tier(self):
        result = plan(["experiments/FirstPersonView/README.md", "host_patch/ncmm_loader.cpp"])
        self.assertEqual(result["required"], "full")

    def test_unknown_fpv_implementation_is_not_quick(self):
        self.assertEqual(plan(["experiments/FirstPersonView/future_internal.cpp"])["required"], "integration")


if __name__ == "__main__":
    unittest.main()
