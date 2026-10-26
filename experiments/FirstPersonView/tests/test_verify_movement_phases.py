import copy
import math
import unittest
from verify_horizon_release import frame_phases, expected_payload_files, verify_payload_file_matrix

class FramePhaseTests(unittest.TestCase):
    def valid_record(self):
        r={'exclusive_phases_available': True, 'frame_samples_ms': [4.0]*108,
           'first_frame_indices': list(range(96,108)), 'first_frame_after_step_samples_ms': [4.0]*12}
        for name in ('native_tiles','perspective','present','unattributed'):
            r[name+'_samples_ms']=[1.0]*108
        for name in ('extract','callback','gpu_geometry','gpu_resource','gpu_upload','gpu_record','gpu_submit'):
            r[name+'_samples_ms']=[0.0]*108
        return r
    def test_complete_exclusive_phases(self):
        result=frame_phases(self.valid_record())
        self.assertEqual(result['first_after_step']['perspective_samples_ms']['median_ms'],1.0)
    def test_extended_warm_slice_and_step_indices(self):
        r=self.valid_record()
        for name,values in r.items():
            if isinstance(values,list):r[name]=[values[0]]*256
        r['first_frame_indices']=list(range(192,256))
        r['first_frame_after_step_samples_ms']=[4.0]*64
        r['present_samples_ms'][96:192]=[2.0]*96
        r['native_tiles_samples_ms'][96:192]=[0.0]*96
        result=frame_phases(r,extended=True)
        self.assertEqual(result['warm_rotation']['present_samples_ms']['median_ms'],2.0)
        self.assertEqual(result['first_after_step']['present_samples_ms']['median_ms'],1.0)
        with self.assertRaises(ValueError):frame_phases(r)
    def test_overlap_and_missing_work_rejected(self):
        for value in (0.0, 2.0):
            r=self.valid_record();r['present_samples_ms'][99]=value
            with self.assertRaisesRegex(ValueError,'overlap or omit'):frame_phases(r)
    def test_step_indices_must_match_frame_stream(self):
        for indices in ([96]*12, list(range(97,109))):
            r=self.valid_record();r['first_frame_indices']=indices
            with self.assertRaises(ValueError):frame_phases(r)
        r=self.valid_record();r['first_frame_after_step_samples_ms'][0]=3.0
        with self.assertRaisesRegex(ValueError,'differ'):frame_phases(r)
    def test_invalid_nested_samples_rejected(self):
        for values in ([0.0]*107,[float('nan')]*108,[-1.0]*108):
            r=self.valid_record();r['gpu_resource_samples_ms']=values
            with self.assertRaisesRegex(ValueError,'Invalid movement phase'):frame_phases(r)
    def test_no_claim_without_exclusive_measurement(self):
        r=self.valid_record();r['exclusive_phases_available']=False
        with self.assertRaisesRegex(ValueError,'missing'):frame_phases(r)

class PayloadMatrixTests(unittest.TestCase):
    held='experiments/FirstPersonView/assets/held_items/selection.json'
    world='experiments/FirstPersonView/assets/world_props/selection.json'
    nvtx='experiments/graphics_bridge/third_party/nvtx3/LICENSE.txt'
    def entries(self,files):
        return {'payload/'+name for name in files}|{'payload/first-person-bundle.json'}
    def test_exact_base_camera_assets_and_combined_contracts(self):
        for paths,count in (([],32),([self.nvtx],33),([self.held],34),([self.held,self.nvtx],35)):
            with self.subTest(paths=paths):
                files=expected_payload_files(paths)
                self.assertEqual(len(files),count)
                verify_payload_file_matrix(files,self.entries(files),paths)
    def test_missing_asset_metadata_and_license_rejected(self):
        paths=[self.held,self.nvtx];expected=expected_payload_files(paths)
        for name in ('held-assets.manifest.json','held-assets-licenses.txt','NVTX-LICENSE.txt'):
            files=expected-{'code_mods/FirstPersonView/'+name}
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'trusted source'):
                verify_payload_file_matrix(files,self.entries(files),paths)
    def test_world_assets_license_and_effect_references_are_required(self):
        paths=[self.held,self.world,self.nvtx];expected=expected_payload_files(paths)
        self.assertEqual(len(expected),38)
        verify_payload_file_matrix(expected,self.entries(expected),paths)
        for name in ('world-assets.manifest.json','world-assets-license.txt','weapon-effects.references.json'):
            files=expected-{'code_mods/FirstPersonView/'+name}
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'trusted source'):
                verify_payload_file_matrix(files,self.entries(files),paths)
    def test_same_count_wrong_file_is_rejected(self):
        paths=[self.held];files=expected_payload_files(paths)
        files.remove('code_mods/FirstPersonView/held-assets-licenses.txt')
        files.add('code_mods/FirstPersonView/unreviewed.bin')
        self.assertEqual(len(files),34)
        with self.assertRaisesRegex(ValueError,'trusted source'):
            verify_payload_file_matrix(files,self.entries(files),paths)
    def test_unlisted_archive_file_and_missing_archive_file_are_rejected(self):
        paths=[self.held,self.nvtx];files=expected_payload_files(paths)
        for entries in (self.entries(files)|{'payload/extra.bin'},self.entries(files)-{'payload/host/first-person-source.json'}):
            with self.assertRaisesRegex(ValueError,'Archive payload'):
                verify_payload_file_matrix(files,entries,paths)
    def test_runtime_module_or_persistent_data_cannot_disappear(self):
        expected=expected_payload_files([])
        for name in ('code_mods/EquipmentBodyMap/ncmm_mod.dll','code_mods/SurvivorProgression/persistent_data/mana_hand_carrier.json'):
            files=expected-{name}
            with self.assertRaisesRegex(ValueError,'trusted source'):
                verify_payload_file_matrix(files,self.entries(files),[])

if __name__=='__main__':unittest.main()
