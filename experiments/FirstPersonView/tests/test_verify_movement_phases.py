import copy
import math
import unittest
from verify_horizon_release import (NATIVE_STEP_LABELS, frame_phases, native_step_phases,
                                    expected_payload_files, verify_payload_file_matrix)

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

class NativeActionPhaseTests(unittest.TestCase):
    """Contrived receipts exercise accounting guards, never provide timings."""
    def valid_record(self, steps=12):
        indices = list(range(192 if steps == 64 else 96, (192 if steps == 64 else 96) + steps))
        record = dict(native_steps=steps, first_frame_indices=indices,
                      frame_samples_ms=[4.0] * (indices[-1] + 1), first_ready_samples_ms=[9.2] * steps)
        rows = []
        position, moves = [20, 30, 0], 100000
        for step, index in enumerate(indices):
            offsets = [0, .5, .6, .7, .8, 2.8, 2.9, 4.9, 5, 9.1, 9.2]
            direction = ((1, 0, 0), (0, -1, 0), (-1, 0, 0), (0, 1, 0))[step % 4]
            target = [a + b for a, b in zip(position, direction)]
            rows.append(dict(step=step, frame_index=index, boundary_offsets_ms=offsets,
                             phase_wall_ms=[b-a for a, b in zip(offsets, offsets[1:])],
                             first_ready_wall_ms=9.2, first_frame_wall_ms=4.0,
                             render_bookkeeping_wall_ms=.1, residual_wall_ms=0,
                             moves_before=moves, moves_after=moves-100, **{'from': position, 'to': target}))
            position, moves = target, moves-100
        record['native_step_phases'] = dict(clock='std::chrono::steady_clock', units='milliseconds',
                                           phase_labels=list(NATIVE_STEP_LABELS), samples=rows)
        return record

    def test_conserved_adjacent_phases_for_short_and_extended_routes(self):
        for steps in (12, 64):
            result = native_step_phases(self.valid_record(steps))
            self.assertEqual(result['samples'], steps)
            self.assertAlmostEqual(result['phases']['move']['median_ms'], .5)
            self.assertAlmostEqual(result['phases']['render_call']['median_ms'], 4.1)
            self.assertAlmostEqual(result['render_bookkeeping_median_ms'], .1)

    def test_legacy_absence_is_explicit_and_cannot_pass_new_qualification(self):
        record = self.valid_record(); del record['native_step_phases']
        self.assertFalse(native_step_phases(record, required=False)['available'])
        with self.assertRaisesRegex(ValueError, 'receipt missing'): native_step_phases(record)

    def test_invalid_clock_or_phase_order_is_rejected(self):
        for name, value in (('clock', 'cpu_clock'), ('units', 'seconds'), ('phase_labels', list(reversed(NATIVE_STEP_LABELS)))):
            record = self.valid_record(); record['native_step_phases'][name] = value
            with self.assertRaises(ValueError): native_step_phases(record)

    def test_missing_reordered_and_duplicate_action_samples_are_rejected(self):
        for change in ('missing', 'reordered', 'duplicate'):
            record = self.valid_record(); rows = record['native_step_phases']['samples']
            if change == 'missing': rows.pop()
            elif change == 'duplicate': rows[1] = copy.deepcopy(rows[0])
            else: rows[0], rows[1] = rows[1], rows[0]
            with self.assertRaises(ValueError): native_step_phases(record)

    def test_action_frame_mapping_must_use_the_original_stream(self):
        record = self.valid_record(); record['native_step_phases']['samples'][0]['frame_index'] += 1
        with self.assertRaisesRegex(ValueError, 'action/frame'): native_step_phases(record)

    def test_backward_nonfinite_and_omitted_boundaries_are_rejected(self):
        for value in (-1, float('nan'), float('inf'), True):
            record = self.valid_record(); record['native_step_phases']['samples'][0]['boundary_offsets_ms'][4] = value
            with self.assertRaisesRegex(ValueError, 'boundaries'): native_step_phases(record)
        record = self.valid_record(); record['native_step_phases']['samples'][0]['boundary_offsets_ms'].pop()
        with self.assertRaisesRegex(ValueError, 'boundaries'): native_step_phases(record)

    def test_inclusive_frame_cannot_be_added_to_render_twice(self):
        record = self.valid_record(); record['native_step_phases']['samples'][0]['phase_wall_ms'][8] += 4
        with self.assertRaisesRegex(ValueError, 'overlap or omit'): native_step_phases(record)

    def test_original_totals_and_inner_render_accounting_are_required(self):
        for name in ('first_ready_wall_ms', 'first_frame_wall_ms', 'render_bookkeeping_wall_ms'):
            record = self.valid_record(); record['native_step_phases']['samples'][0][name] += .01
            with self.assertRaises(ValueError): native_step_phases(record)

    def test_signed_residual_cannot_be_clamped_to_zero(self):
        record = self.valid_record(); row = record['native_step_phases']['samples'][0]
        row['first_ready_wall_ms'] -= .002; record['first_ready_samples_ms'][0] -= .002
        row['residual_wall_ms'] = -.002
        self.assertAlmostEqual(native_step_phases(record)['max_abs_accounting_residual_ms'], .002)
        row['residual_wall_ms'] = 0
        with self.assertRaisesRegex(ValueError, 'clamped residual'): native_step_phases(record)

    def test_unaccounted_action_work_is_rejected(self):
        record = self.valid_record(); row = record['native_step_phases']['samples'][0]
        row['first_ready_wall_ms'] += .01; record['first_ready_samples_ms'][0] += .01; row['residual_wall_ms'] = .01
        with self.assertRaisesRegex(ValueError, 'Unconserved'): native_step_phases(record)

    def test_route_teleport_and_missing_move_cost_are_rejected(self):
        for name, value in (('from', [30, 40, 0]), ('to', [20, 30, 1]), ('moves_after', 100000)):
            record = self.valid_record(); record['native_step_phases']['samples'][0][name] = value
            with self.assertRaises(ValueError): native_step_phases(record)


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
