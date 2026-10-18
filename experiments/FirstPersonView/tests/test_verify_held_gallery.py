"""Reject missing Native coverage, mismatched resources and corrupt captures."""
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from verify_held_gallery import ACCESSORIES, BODY, PHASES, verify


class HeldGalleryVerification(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.ids = {name:100+i for i,name in enumerate([*BODY, *ACCESSORIES.values()])}
        self.manifest = self.root/'manifest.json'
        self.manifest.write_text(json.dumps(dict(models=[dict(id=n,identity=i) for n,i in self.ids.items()])))
        image = bytearray(54+1280*800*3)
        struct.pack_into('<2sI', image, 0, b'BM', len(image))
        struct.pack_into('<IIiiHH', image, 10, 54, 40, 1280, 800, 1, 24)
        template = self.root/'template.bmp'
        template.write_bytes(image)
        self.records = []
        for mode in ('cpu','gpu'):
            for phase in sorted(PHASES):
                model = phase if phase in BODY else 'pump' if phase.startswith('pump-') else 'assault'
                gun = model in ('pistol','revolver','smg','assault','battle','pump','auto_shotgun')
                ammo = int(gun and phase != 'pump-empty')
                attached = [ACCESSORIES[phase]] if phase in ACCESSORIES else []
                name = f'first-person-held-{phase}-{mode}.bmp'
                os.link(template, self.root/name)
                self.records.append(dict(name=phase,renderer=mode,canonical=BODY[model],model=model,
                    identity=self.ids[model],matched=1,fallback_reason=0,gpu=mode=='gpu',
                    attachments=len(attached),draws=2 if attached or phase=='pump-loaded' else 1,
                    pose_matrices=20,native_is_gun=gun,native_ammo_remaining=ammo,ammo_loaded=bool(ammo),
                    accessory_models=attached,accessory_identities=[self.ids[n] for n in attached],
                    static_builds=0,static_reuses=1+len(attached),static_mesh_upload_bytes=0,
                    dynamic_geometry_bytes=0,gpu_draw_calls=10,backend='direct3d12',device='RTX test',
                    software_device=False,gpu_resource_bytes=1000,capture=name,width=1280,height=800))

    def check(self):
        report=dict(schema=1,success=True,actual_cdda_sdl=True,manual_playthrough=False,records=self.records)
        (self.root/'first-person-held-gallery.json').write_text(json.dumps(report))
        return verify(self.root,self.manifest,'RTX test')

    def first(self,phase,mode='gpu'):
        record=next(r for r in self.records if r['name']==phase and r['renderer']==mode)
        self.records.remove(record);self.records.insert(0,record)
        return record

    def test_complete_capture_integrity_and_exact_inventory(self):
        result=self.check()
        self.assertEqual((result['native_cases'],result['compiled_models'],len(result['images'])),(64,30,64))

    def test_duplicate_case_cannot_replace_missing_native_coverage(self):
        self.records[-1]=self.records[0]
        with self.assertRaises(AssertionError):self.check()

    def test_compiled_identity_and_native_canonical_are_verified(self):
        record=self.first('pistol');record['identity']+=1
        with self.assertRaises(AssertionError):self.check()
        record['identity']=self.ids['pistol'];record['canonical']='unrelated_mod_item'
        with self.assertRaises(AssertionError):self.check()

    def test_actual_attachment_identity_is_verified(self):
        self.first('grip')['accessory_identities']=[self.ids['red_dot']]
        with self.assertRaises(AssertionError):self.check()

    def test_cpu_fallback_and_software_device_do_not_pass_hardware_proof(self):
        record=self.first('pistol');record['gpu']=False
        with self.assertRaises(AssertionError):self.check()
        record['gpu']=True;record['software_device']=True
        with self.assertRaises(AssertionError):self.check()

    def test_static_reupload_does_not_pass_pose_only_reuse(self):
        self.first('pistol')['static_mesh_upload_bytes']=128
        with self.assertRaises(AssertionError):self.check()

    def test_empty_native_ammunition_cannot_claim_loaded_round(self):
        self.first('pump-empty')['ammo_loaded']=True
        with self.assertRaises(AssertionError):self.check()

    def test_capture_header_and_complete_pixel_storage_are_required(self):
        record=self.first('pistol')
        (self.root/record['capture']).write_bytes(b'BM'+b'\0'*52)
        with self.assertRaises(AssertionError):self.check()

    def test_capture_cannot_reference_another_file(self):
        self.first('pistol')['capture']='../outside.bmp'
        with self.assertRaises(AssertionError):self.check()

    def test_vulkan_requires_explicit_backend(self):
        for record in self.records:
            if record['renderer']=='gpu':record['backend']='vulkan'
        with self.assertRaises(AssertionError):self.check()
        self.assertEqual(verify(self.root,self.manifest,'RTX test',expected_backend='vulkan')['native_cases'],64)


if __name__=='__main__':unittest.main()
