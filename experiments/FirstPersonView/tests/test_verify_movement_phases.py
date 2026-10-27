import copy
import math
import unittest
from verify_horizon_release import (NATIVE_STEP_LABELS, NATIVE_MAP_SHIFT_LABELS, NATIVE_MAP_CACHE_LABELS, frame_phases, native_step_phases,
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

class NativeSubmapPhaseTests(unittest.TestCase):
    """Fabricated protocol fixtures; none of these numbers measure performance."""
    def valid_record(self):
        record = NativeActionPhaseTests().valid_record()
        previous = [60, 60]
        for row in record['native_step_phases']['samples']:
            input_xy = [p+b-a for p,a,b in zip(previous, row['from'], row['to'])]
            shift_xy = [(v-60)//12 for v in input_xy]
            output_xy = [v-s*12 for v,s in zip(input_xy, shift_xy)]
            shifted = shift_xy != [0, 0]
            labels = ['entry', 'route', 'shift', 'rebase_load_npcs', 'invalidate', 'build', 'spawn', 'overmap', 'exit'] if shifted else [
                'entry', 'route', 'zero_position', 'zero_overmap', 'exit']
            row['native_submap_updates'] = dict(schema=1, clock='std::chrono::steady_clock', units='milliseconds',
                diagnostic_probe_overhead_included=True, claimed=True, reused_storage=False, nested_capture=False,
                counter_overflow=False, invocations=1, recorded=1, dropped=0, max_depth=1,
                exceptional_invocations=0, protocol_errors=0, events=[dict(ordinal=0, depth=1, parent_ordinal=-1,
                    input_xy=input_xy, output_xy=output_xy,
                    shift_xy=shift_xy, z_level_changed=False, shift_known=True,
                    complete=True, exceptional=False, boundary_labels=labels,
                    boundary_offsets_ms=[.01*(i+1) for i in range(len(labels))])])
            previous = output_xy
        return record

    def test_complete_internal_matrix_and_historical_absence(self):
        result = native_step_phases(self.valid_record(), require_submaps=True)['submap_updates']
        self.assertTrue(result['available']); self.assertEqual(len(result['samples']), 12)
        self.assertEqual(result['samples'][1]['shift_xy'], [0, -1])
        old = NativeActionPhaseTests().valid_record()
        self.assertFalse(native_step_phases(old)['submap_updates']['available'])
        with self.assertRaisesRegex(ValueError, 'submap receipt missing'): native_step_phases(old, require_submaps=True)

    def test_missing_and_partial_receipts_rejected(self):
        record = self.valid_record(); del record['native_step_phases']['samples'][0]['native_submap_updates']
        with self.assertRaises(ValueError): native_step_phases(record, require_submaps=True)
        with self.assertRaisesRegex(ValueError, 'Partial'): native_step_phases(record)

    def test_individually_valid_events_from_other_actions_are_rejected(self):
        record = self.valid_record(); rows = record['native_step_phases']['samples']
        rows[0]['native_submap_updates'], rows[2]['native_submap_updates'] = rows[2]['native_submap_updates'], rows[0]['native_submap_updates']
        with self.assertRaisesRegex(ValueError, 'submap action mapping'):
            native_step_phases(record, require_submaps=True)

    def test_nested_overflow_reused_and_exceptional_capture_rejected(self):
        for name, value in (('nested_capture', True), ('counter_overflow', True), ('reused_storage', True),
                            ('dropped', 1), ('invocations', 0), ('invocations', 2), ('recorded', 0),
                            ('max_depth', 2), ('exceptional_invocations', 1), ('protocol_errors', 1), ('claimed', False)):
            record = self.valid_record(); record['native_step_phases']['samples'][0]['native_submap_updates'][name] = value
            with self.subTest(name=name, value=value), self.assertRaises(ValueError): native_step_phases(record, require_submaps=True)

    def test_event_identity_rebase_and_privacy_of_scope_rejected(self):
        for name, value in (('ordinal', 1), ('depth', 2), ('parent_ordinal', 0), ('input_xy', [65, 66]),
                            ('output_xy', [65, 85]), ('shift_xy', [1, 0]), ('complete', False), ('shift_known', False),
                            ('exceptional', True), ('z_level_changed', True)):
            record = self.valid_record(); record['native_step_phases']['samples'][0]['native_submap_updates']['events'][0][name] = value
            with self.subTest(name=name), self.assertRaises(ValueError): native_step_phases(record, require_submaps=True)

    def test_missing_misassigned_overlapping_and_outside_action_boundaries_rejected(self):
        for mode in ('missing', 'labels', 'backwards', 'outside', 'nan'):
            record = self.valid_record(); event = record['native_step_phases']['samples'][1]['native_submap_updates']['events'][0]
            if mode == 'missing': event['boundary_offsets_ms'].pop()
            if mode == 'labels': event['boundary_labels'][3] = 'build'
            if mode == 'backwards': event['boundary_offsets_ms'][3] = 0
            if mode == 'outside': event['boundary_offsets_ms'][-1] = .6
            if mode == 'nan': event['boundary_offsets_ms'][3] = math.nan
            with self.subTest(mode=mode), self.assertRaises(ValueError): native_step_phases(record, require_submaps=True)

class NativeMapShiftPhaseTests(unittest.TestCase):
    """Invented times exercise the format; none constitute a measurement."""
    def add_shifts(self, row):
        parent = row['native_submap_updates']['events'][0]
        remaining = parent['shift_xy'][:]
        count = max(map(abs, remaining))
        events = []
        start = parent['boundary_offsets_ms'][1]
        span = (parent['boundary_offsets_ms'][2]-start)/(2*count+1) if count else 0
        for ordinal in range(count):
            direction = [max(-1, min(1, v)) for v in remaining]
            remaining = [a-b for a,b in zip(remaining, direction)]
            offsets = [start+(2*ordinal+1)*span+span*b/11 for b in range(12)]
            events.append(dict(ordinal=ordinal, depth=1, parent_ordinal=-1, parent_update_ordinal=0,
                shift_xy=direction, route_accepted=True, complete=True, exceptional=False,
                boundary_labels=list(NATIVE_MAP_SHIFT_LABELS), boundary_offsets_ms=offsets))
        row['native_map_shifts'] = dict(schema=1, clock='std::chrono::steady_clock', units='milliseconds',
            diagnostic_probe_overhead_included=True, invocations=count, recorded=count, dropped=0,
            max_depth=int(count != 0), exceptional_invocations=0, events=events)

    def valid_record(self, multiple=False):
        record = NativeSubmapPhaseTests().valid_record()
        if multiple:
            parent = record['native_step_phases']['samples'][0]['native_submap_updates']['events'][0]
            parent['shift_xy'] = [3, -2]
            parent['input_xy'] = [v+s*12 for v,s in zip(parent['output_xy'], parent['shift_xy'])]
            parent['boundary_labels'] = ['entry', 'route', 'shift', 'rebase_load_npcs', 'invalidate', 'build', 'spawn', 'overmap', 'exit']
            parent['boundary_offsets_ms'] = [.01*(i+1) for i in range(9)]
        for row in record['native_step_phases']['samples']: self.add_shifts(row)
        return record

    def verify(self, record):
        return native_step_phases(record, require_submaps=True, require_shifts=True)['map_shifts']

    def test_complete_stationary_and_crossing_routes_conserve_parent_time(self):
        result = self.verify(self.valid_record())
        self.assertTrue(result['available'])
        self.assertEqual([s['calls'] for s in result['samples']], [0, 1]*6)
        child = result['samples'][1]
        self.assertAlmostEqual(child['child_wall_ms']+child['parent_gap_wall_ms'], child['parent_wall_ms'])
        self.assertEqual(len(child['children'][0]['phases']), 11)

    def test_multi_call_parent_uses_native_clamped_sequence(self):
        result = self.verify(self.valid_record(multiple=True))['samples'][0]
        self.assertEqual([e['shift_xy'] for e in result['children']], [[1, -1], [1, -1], [1, 0]])
        self.assertAlmostEqual(result['child_wall_ms']+result['parent_gap_wall_ms'], result['parent_wall_ms'])

    def test_historical_absence_is_unavailable_and_cannot_qualify(self):
        old = NativeSubmapPhaseTests().valid_record()
        self.assertFalse(native_step_phases(old)['map_shifts']['available'])
        with self.assertRaisesRegex(ValueError, 'map-shift receipt missing'): self.verify(old)
        record = self.valid_record(); del record['native_step_phases']['samples'][0]['native_map_shifts']
        with self.assertRaisesRegex(ValueError, 'Partial Native map-shift'): native_step_phases(record)

    def test_parent_is_required_even_when_child_is_present(self):
        record = self.valid_record(); del record['native_step_phases']['samples'][1]['native_submap_updates']
        with self.assertRaises(ValueError): native_step_phases(record)

    def test_invalid_schema_clock_counts_overflow_or_exceptions_rejected(self):
        for key, value in (('schema', True), ('schema', 2), ('clock', 'cpu'), ('units', 'seconds'),
                           ('diagnostic_probe_overhead_included', False), ('invocations', 0), ('invocations', 2),
                           ('recorded', 0), ('recorded', True), ('dropped', 1), ('max_depth', 2), ('exceptional_invocations', 1)):
            record = self.valid_record(); record['native_step_phases']['samples'][1]['native_map_shifts'][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): self.verify(record)

    def test_incomplete_nested_or_wrong_parent_child_rejected(self):
        for key, value in (('ordinal', 1), ('depth', 2), ('parent_ordinal', 0), ('parent_update_ordinal', 1),
                           ('route_accepted', False), ('complete', False), ('exceptional', True),
                           ('shift_xy', [0, 1]), ('shift_xy', [False, -1])):
            record = self.valid_record(); record['native_step_phases']['samples'][1]['native_map_shifts']['events'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): self.verify(record)

    def test_missing_duplicate_or_misordered_children_rejected(self):
        for mode in ('missing', 'duplicate', 'order'):
            record = self.valid_record(multiple=True)
            events = record['native_step_phases']['samples'][0]['native_map_shifts']['events']
            if mode == 'missing': events.pop()
            elif mode == 'duplicate': events.append(copy.deepcopy(events[0]))
            else: events[0], events[2] = events[2], events[0]
            with self.subTest(mode=mode), self.assertRaises(ValueError): self.verify(record)

    def test_children_from_other_individually_valid_actions_rejected(self):
        record = self.valid_record(); rows = record['native_step_phases']['samples']
        rows[1]['native_map_shifts'], rows[3]['native_map_shifts'] = rows[3]['native_map_shifts'], rows[1]['native_map_shifts']
        with self.assertRaisesRegex(ValueError, 'direction/order'): self.verify(record)

    def test_bad_boundaries_or_parent_containment_rejected(self):
        for mode in ('missing', 'labels', 'negative', 'nan', 'bool', 'reverse', 'start', 'end'):
            record = self.valid_record(); event = record['native_step_phases']['samples'][1]['native_map_shifts']['events'][0]
            points = event['boundary_offsets_ms']
            if mode == 'missing': points.pop()
            elif mode == 'labels': event['boundary_labels'][3] = 'build'
            elif mode == 'negative': points[0] = -.1
            elif mode == 'nan': points[3] = math.nan
            elif mode == 'bool': points[3] = True
            elif mode == 'reverse': points[3] = 0
            elif mode == 'start': event['boundary_offsets_ms'] = [v-.02 for v in points]
            else: event['boundary_offsets_ms'] = [v+.02 for v in points]
            with self.subTest(mode=mode), self.assertRaises(ValueError): self.verify(record)

    def test_overlap_smaller_than_outer_tolerance_still_rejected(self):
        record = self.valid_record(multiple=True)
        events = record['native_step_phases']['samples'][0]['native_map_shifts']['events']
        # A 1 ns serialized quantum permits 3 ns, never the outer 3 us budget.
        delta = events[1]['boundary_offsets_ms'][0]-events[0]['boundary_offsets_ms'][-1]+.0001
        events[1]['boundary_offsets_ms'] = [v-delta for v in events[1]['boundary_offsets_ms']]
        with self.assertRaisesRegex(ValueError, 'Overlapping'): self.verify(record)

    def test_unexpected_child_on_stationary_route_rejected(self):
        record = self.valid_record(); rows = record['native_step_phases']['samples']
        rows[0]['native_map_shifts'] = copy.deepcopy(rows[1]['native_map_shifts'])
        with self.assertRaisesRegex(ValueError, 'count'): self.verify(record)

class NativeMapCachePhaseTests(unittest.TestCase):
    """Fabricated child receipts validate protocol only; these are not timings."""
    def event(self, start=.051, end=.0565, owner=0, shift=-1, ordinal=0):
        return dict(ordinal=ordinal, depth=1, parent_ordinal=-1, parent_update_ordinal=owner,
                    parent_shift_ordinal=shift, zlev=0, skip_lightmap=False, zlevels=True,
                    bounds_known=True, min_z=-10, max_z=10, complete=True, exceptional=False,
                    boundary_labels=list(NATIVE_MAP_CACHE_LABELS),
                    boundary_offsets_ms=[start+(end-start)*i/11 for i in range(12)])
    def valid_record(self):
        record=NativeMapShiftPhaseTests().valid_record()
        for row in record['native_step_phases']['samples']:
            shifted=row['native_submap_updates']['events'][0]['shift_xy']!=[0,0]
            row['native_map_cache_builds']=dict(schema=1,clock='std::chrono::steady_clock',units='milliseconds',
                diagnostic_probe_overhead_included=True, invocations=int(shifted),recorded=int(shifted),dropped=0,
                max_depth=int(shifted),exceptional_invocations=0,events=[self.event()] if shifted else [])
        return record
    def verify(self, record):
        return native_step_phases(record,require_submaps=True,require_shifts=True,require_caches=True)['map_cache_builds']
    def test_main_calls_conserve_real_parent_and_missing_hooks_fail(self):
        r=self.valid_record();parsed=self.verify(r)
        self.assertTrue(parsed['available']);self.assertEqual([v['calls'] for v in parsed['samples']],[0,1]*6)
        self.assertEqual(parsed['samples'][1]['children'][0]['location'],'invalidate_to_build')
        r['native_step_phases']['samples'][1]['native_map_cache_builds'].update(invocations=0,recorded=0,max_depth=0,events=[])
        with self.assertRaisesRegex(ValueError,'main update call'):self.verify(r)
    def test_historical_absence_is_explicit_and_partial_data_rejected(self):
        old=NativeMapShiftPhaseTests().valid_record()
        self.assertFalse(native_step_phases(old)['map_cache_builds']['available'])
        with self.assertRaisesRegex(ValueError,'map-cache receipt'):self.verify(old)
        r=self.valid_record();del r['native_step_phases']['samples'][0]['native_map_cache_builds']
        with self.assertRaisesRegex(ValueError,'Partial Native map-cache'):native_step_phases(r)
    def test_counts_schema_nesting_and_exceptions_rejected(self):
        for key,value in (('schema',2),('clock','cpu_clock'),('units','seconds'),('diagnostic_probe_overhead_included',False),
                          ('invocations',True),('invocations',33),('recorded',0),('dropped',1),('max_depth',2),('exceptional_invocations',1)):
            r=self.valid_record();r['native_step_phases']['samples'][1]['native_map_cache_builds'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.verify(r)
    def test_event_identity_flags_bounds_and_owners_rejected(self):
        for key,value in (('ordinal',1),('depth',2),('parent_ordinal',0),('parent_update_ordinal',1),
                          ('parent_update_ordinal',-1),('parent_shift_ordinal',0),('bounds_known',False),
                          ('zlevels',0),('skip_lightmap',1),('min_z',0),('max_z',11),('zlev',True),
                          ('complete',False),('exceptional',True)):
            r=self.valid_record();r['native_step_phases']['samples'][1]['native_map_cache_builds']['events'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.verify(r)
    def test_malformed_backward_outside_and_nonfinite_boundaries_rejected(self):
        for mode in ('missing','labels','backward','outside_update','outside_action','nan','bool'):
            r=self.valid_record();e=r['native_step_phases']['samples'][1]['native_map_cache_builds']['events'][0]
            if mode=='missing':e['boundary_offsets_ms'].pop()
            if mode=='labels':e['boundary_labels'][3]='seen'
            if mode=='backward':e['boundary_offsets_ms'][3]=0
            if mode=='outside_update':e['boundary_offsets_ms'][-1]=.2
            if mode=='outside_action':e['boundary_offsets_ms'][-1]=.6
            if mode=='nan':e['boundary_offsets_ms'][4]=math.nan
            if mode=='bool':e['boundary_offsets_ms'][4]=True
            with self.subTest(mode=mode),self.assertRaises(ValueError):self.verify(r)
    def test_shift_child_and_standalone_builders_have_distinct_owners(self):
        r=self.valid_record();rows=r['native_step_phases']['samples'];v=rows[1]['native_map_cache_builds']
        child=self.event(.024,.025,shift=0)
        v['events'][0]['ordinal']=1;v['events'].insert(0,child);v.update(invocations=2,recorded=2)
        parsed=self.verify(r)
        self.assertEqual([c['location'] for c in parsed['samples'][1]['children']],['map_shift','invalidate_to_build'])
        child['parent_shift_ordinal']=-1
        with self.assertRaisesRegex(ValueError,'lost its active shift'):self.verify(r)
        r=self.valid_record();v=r['native_step_phases']['samples'][0]['native_map_cache_builds']
        v.update(invocations=1,recorded=1,max_depth=1,events=[self.event(.2,.21,owner=-1)])
        self.assertEqual(self.verify(r)['samples'][0]['children'][0]['location'],'action_outside_update')
    def test_overlap_smaller_than_outer_budget_is_rejected(self):
        r=self.valid_record();v=r['native_step_phases']['samples'][1]['native_map_cache_builds']
        v['events'].append(self.event(.2,.21,owner=-1,ordinal=1));v.update(invocations=2,recorded=2)
        self.verify(r)
        v['events'][1]['boundary_offsets_ms']=[.0564+i*.00001 for i in range(12)]
        with self.assertRaisesRegex(ValueError,'Overlapping'):self.verify(r)
    def test_single_level_bounds_and_skip_lightmap_metadata(self):
        r=self.valid_record();e=r['native_step_phases']['samples'][1]['native_map_cache_builds']['events'][0]
        e.update(zlev=-3,zlevels=False,min_z=-3,max_z=-3)
        self.assertEqual(self.verify(r)['samples'][1]['children'][0]['min_z'],-3)
        e['skip_lightmap']=True
        with self.assertRaisesRegex(ValueError,'default lightmap branch'):self.verify(r)
        r=self.valid_record();v=r['native_step_phases']['samples'][0]['native_map_cache_builds']
        e=self.event(.2,.21,owner=-1);e.update(zlev=-3,zlevels=False,min_z=-3,max_z=-3,skip_lightmap=True)
        v.update(invocations=1,recorded=1,max_depth=1,events=[e])
        self.assertTrue(self.verify(r)['samples'][0]['children'][0]['skip_lightmap'])
    def test_signed_main_build_gap_retains_serialization_roundoff(self):
        r=self.valid_record();e=r['native_step_phases']['samples'][1]['native_map_cache_builds']['events'][0]
        e['boundary_offsets_ms']=[.049999+(.060001-.049999)*i/11 for i in range(12)]
        value=self.verify(r)['samples'][1]
        self.assertLess(value['direct_build_gap_wall_ms'],0)
        self.assertAlmostEqual(value['direct_build_gap_wall_ms'],-.000002,places=9)


if __name__=='__main__':unittest.main()
