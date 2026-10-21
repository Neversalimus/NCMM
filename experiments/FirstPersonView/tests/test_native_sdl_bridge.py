"""Reject stale, unpinned and partially prepared SDK source before any write."""
import importlib.util
import json
import copy
from pathlib import Path
import shutil
import tempfile
import unittest

BRIDGE=Path(__file__).resolve().parents[2]/'graphics_bridge/native_sdl_bridge.py'
spec=importlib.util.spec_from_file_location('native_sdl_bridge',BRIDGE)
sdl=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdl)
spec=importlib.util.spec_from_file_location('sdl_installer_contract',Path(__file__).with_name('verify_installer_artifacts.py'))
contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
FIXTURE=Path(__file__).with_name('fixtures')/'native-vcpkg.json'

class NativeSDLTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)/'native';self.root.mkdir()
        (self.root/'msvc-full-features').mkdir()
        shutil.copyfile(FIXTURE,self.root/'msvc-full-features/vcpkg.json')
        self.vendor=Path(self.temp.name)/'vendor';shutil.copytree(sdl.VENDOR,self.vendor)
        self.pin=sorted(sdl.PINS)[0];self.receipt()
    def receipt(self,**extra):
        value=dict(status='experimental-not-certified',source_commit=self.pin,inputs={'fixture.cpp':'owned'})
        value.update(extra);(self.root/'first-person-source.json').write_text(json.dumps(value),encoding='utf-8')
    def snapshot(self):
        return {p.relative_to(self.root).as_posix():None if p.is_dir() else p.read_bytes() for p in self.root.rglob('*')}
    def rejected(self,identity=None):
        before=self.snapshot()
        with self.assertRaises(ValueError):sdl.apply(self.root,identity or self.pin,self.vendor)
        self.assertEqual(before,self.snapshot())
    def test_both_real_source_pins_and_all_overlay_bytes(self):
        for pin in sorted(sdl.PINS):
            with self.subTest(pin=pin):
                updates,metadata=sdl.plan(self.root,pin,self.vendor)
                self.assertEqual(len(updates),6)
                self.assertEqual(metadata['sdl_version'],'3.4.10')
                self.assertTrue(metadata['windows_build_only'] and metadata['global_vcpkg_unchanged'])
                for path,content in updates.items():self.assertEqual(sdl.digest(content),metadata['files'][path.relative_to(self.root).as_posix()])
    def test_apply_records_dependency_and_rejects_reapplication(self):
        metadata=sdl.apply(self.root,self.pin,self.vendor)
        self.assertEqual(json.loads((self.root/'first-person-source.json').read_text())['windows_sdl'],metadata)
        for name,expected in metadata['files'].items():self.assertEqual(sdl.digest((self.root/name).read_bytes()),expected)
        self.rejected()
    def test_verify_only_does_not_write(self):
        before=self.snapshot();sdl.apply(self.root,self.pin,self.vendor,verify_only=True)
        self.assertEqual(before,self.snapshot())
    def test_unknown_engine_pin(self):self.rejected('0'*40)
    def test_changed_native_manifest(self):
        path=self.root/'msvc-full-features/vcpkg.json';v=json.loads(path.read_text());v['dependencies'].append('unreviewed')
        path.write_text(json.dumps(v));self.rejected()
    def test_existing_project_configuration(self):
        (self.root/'msvc-full-features/vcpkg-configuration.json').write_text('{"owned":true}');self.rejected()
    def test_existing_overlay(self):
        (self.root/'msvc-full-features/ncmm-sdl-overlay').mkdir();self.rejected()
    def test_changed_raw_vendor(self):
        (self.vendor/'source/usage').write_text('tampered');self.rejected()
    def test_changed_patch(self):
        (self.vendor/'overlay/sdl3/ncmm-graphics-descriptors.patch').write_text('tampered');self.rejected()
    def test_changed_vendor_manifest(self):
        path=self.vendor/'manifest.json';path.write_bytes(path.read_bytes()+b' ');self.rejected()
    def test_extra_vendor_file(self):
        (self.vendor/'untracked.cmake').write_text('unreviewed');self.rejected()
    def test_missing_vendor_file(self):
        (self.vendor/'source/usage').unlink();self.rejected()
    def test_wrong_prepared_engine(self):self.receipt(source_commit='0'*40);self.rejected()
    def test_certified_receipt_is_not_modified(self):self.receipt(status='certified');self.rejected()
    def test_existing_dependency_receipt_is_not_overwritten(self):self.receipt(windows_sdl={'owned':True});self.rejected()
    def test_packaged_dependency_matches_trusted_sources(self):
        metadata=sdl.plan(self.root,self.pin,self.vendor)[1]
        prefix='experiments/graphics_bridge/third_party/sdl3-port/'
        read=lambda name:(self.vendor/name.removeprefix(prefix)).read_bytes()
        self.assertEqual(contract.verify_sdl_dependency({'windows_sdl':metadata},read),metadata)
        for key in ['runtime_vendor','patch_sha256','port_manifest_sha256','files']:
            with self.subTest(key=key):
                changed=copy.deepcopy(metadata);changed[key]='unreviewed'
                with self.assertRaises(ValueError):contract.verify_sdl_dependency({'windows_sdl':changed},read)
        with self.assertRaises(ValueError):contract.verify_sdl_dependency({},read)

if __name__=='__main__':unittest.main()
