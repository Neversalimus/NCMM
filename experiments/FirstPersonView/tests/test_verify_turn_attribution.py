import copy
import unittest
from verify_turn_attribution import BOUNDARIES, CALLBACK_SCOPE, CPU_SCOPE, DRAW_SCOPE, GROUPS, OTHER_SCOPE, PHASES, verify


def no_cpu_metrics():
    return dict(reports=0, available=False, width=0, height=0, raster_calls=0,
                triangles=0, cache_bytes=0, frame_ms=0, geometry_ms=0, raster_ms=0, effects_ms=0)


def fixture():
    samples = []
    for enemies, count in GROUPS:
        for i in range(count):
            samples.append(dict(initial_enemies=enemies, round=i, requested_renderer='gpu' if enemies else 'cpu',
                claimed=True, returned=True, closed=True, valid=True, errors=0,
                wall_ns=1000, draw_wall_ns=400, other_wall_ns=600, draw_calls=2, outer_draws=1,
                max_depth=2, cpu_draws=1, gpu_draws=1, legacy_draws=0, skipped_draws=0,
                callback_details=True, callbacks=[dict(ordinal=0, returned=True, rendered=True,
                    backend='gpu', viewport_width=1280, viewport_height=720, image_width=640, image_height=360,
                    source_cells=64, source_slices=1, cpu_metrics=no_cpu_metrics(),
                    boundaries=[dict(tag=tag, offset_ns=offset) for tag, offset in zip(BOUNDARIES, (0, 10, 30, 70, 140, 200, 330, 400))])]))
    return dict(actual_cdda_sdl=True, manual_playthrough=False,
                native_turn_diagnostics=dict(schema=3, controlled_fixture_without_interactive_main_loop=True,
                    ordinary_performance_measurement=False, draw_wall_scope=DRAW_SCOPE,
                    remainder_scope=OTHER_SCOPE, callback_scope=CALLBACK_SCOPE, cpu_metric_scope=CPU_SCOPE, samples=samples))


class TurnAttributionTests(unittest.TestCase):
    def test_nested_intervals_are_not_added_as_independent_work(self):
        result = verify(fixture())
        self.assertEqual(result['controlled_native_turns'], 26)
        self.assertEqual([x['draw_wall_ns'] for x in result['groups']], [2400, 2400, 2400, 3200])
        self.assertFalse(result['pure_ai_time_measured'])
        self.assertFalse(result['first_correct_present_measured'])
        self.assertEqual(result['groups'][0]['callbacks']['native_cache_ns'], 120)
        self.assertEqual(result['groups'][0]['callbacks']['world_callbacks'], 6)
        self.assertEqual(sum(result['groups'][0]['callbacks'][key] for key in PHASES), 2400)

    def test_no_render_inside_a_turn_is_valid_and_honest(self):
        report = fixture()
        sample = report['native_turn_diagnostics']['samples'][0]
        for key in ('draw_calls', 'outer_draws', 'max_depth', 'cpu_draws', 'gpu_draws', 'draw_wall_ns'):
            sample[key] = 0
        sample['other_wall_ns'] = sample['wall_ns']
        sample['callbacks'] = []
        self.assertTrue(verify(report)['success'])

    def test_each_partial_scope_flag_is_rejected(self):
        for key in ('claimed', 'returned', 'closed', 'valid'):
            report = fixture(); report['native_turn_diagnostics']['samples'][0][key] = False
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report)

    def test_each_capture_fault_is_rejected(self):
        for fault in (1, 2, 4, 8, 16, 32, 64):
            report = fixture(); report['native_turn_diagnostics']['samples'][0]['errors'] = fault
            with self.subTest(fault=fault), self.assertRaises(ValueError): verify(report)

    def test_mismatched_partition_negative_nonfinite_and_boolean_are_rejected(self):
        for key, value in (('wall_ns', 10), ('other_wall_ns', -1), ('draw_wall_ns', float('nan')),
                           ('draw_wall_ns', float('inf')), ('wall_ns', True), ('wall_ns', 2**63)):
            report = fixture(); report['native_turn_diagnostics']['samples'][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): verify(report)

    def test_backend_ancestry_or_renderer_lies_are_rejected(self):
        for key, value in (('cpu_draws', 2), ('draw_calls', 1), ('outer_draws', 0), ('max_depth', 3),
                           ('max_depth', True), ('requested_renderer', 'auto')):
            report = fixture(); report['native_turn_diagnostics']['samples'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report)

    def test_missing_duplicate_reordered_turns_are_rejected(self):
        for mutation in ('missing', 'duplicate', 'reorder', 'enemy_count', 'round_bool'):
            report = fixture(); samples = report['native_turn_diagnostics']['samples']
            if mutation == 'missing': samples.pop()
            elif mutation == 'duplicate': samples[1] = copy.deepcopy(samples[0])
            elif mutation == 'reorder': samples[0], samples[1] = samples[1], samples[0]
            elif mutation == 'enemy_count': samples[0]['initial_enemies'] = True
            else: samples[0]['round'] = False
            with self.subTest(mutation=mutation), self.assertRaises(ValueError): verify(report)

    def test_scope_model_or_ordinary_claim_cannot_qualify(self):
        for key, value in (('schema', True), ('schema', 0), ('ordinary_performance_measurement', True),
                           ('controlled_fixture_without_interactive_main_loop', False), ('draw_wall_scope', 'all CPU'),
                           ('remainder_scope', 'pure AI'), ('schema', 1), ('schema', 2), ('callback_scope', 'pure GPU'),
                           ('cpu_metric_scope', 'independent CPU phases')):
            report = fixture(); report['native_turn_diagnostics'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report)
        report = fixture(); report['actual_cdda_sdl'] = False
        with self.assertRaises(ValueError): verify(report)

    def test_incomplete_or_false_callback_capture_is_rejected(self):
        for mutation in ('off', 'missing', 'duplicate', 'ordinal', 'returned', 'rendered', 'backend'):
            report = fixture(); sample = report['native_turn_diagnostics']['samples'][0]
            row = sample['callbacks'][0]
            if mutation == 'off': sample['callback_details'] = False
            elif mutation == 'missing': sample['callbacks'] = []
            elif mutation == 'duplicate': sample['callbacks'].append(copy.deepcopy(row))
            elif mutation == 'ordinal': row['ordinal'] = True
            elif mutation == 'returned': row['returned'] = False
            elif mutation == 'rendered': row['rendered'] = 1
            else: row['backend'] = 'vulkan'
            with self.subTest(mutation=mutation), self.assertRaises(ValueError): verify(report)

    def test_every_missing_or_reordered_callback_boundary_is_rejected(self):
        for index in range(len(BOUNDARIES)):
            for mutation in ('missing', 'label'):
                report = fixture(); row = report['native_turn_diagnostics']['samples'][0]['callbacks'][0]
                if mutation == 'missing': row['boundaries'].pop(index)
                else: row['boundaries'][index]['tag'] = 'wrong'
                with self.subTest(index=index, mutation=mutation), self.assertRaises(ValueError): verify(report)

    def test_callback_offsets_cannot_overlap_or_change_union(self):
        for index, value in ((0, 1), (2, 9), (7, 401), (3, -1), (3, True),
                             (3, float('nan')), (3, float('inf')), (3, 2**63)):
            report = fixture(); row = report['native_turn_diagnostics']['samples'][0]['callbacks'][0]
            row['boundaries'][index]['offset_ns'] = value
            with self.subTest(index=index, value=value), self.assertRaises(ValueError): verify(report)

    def test_actual_viewport_and_image_dimensions_must_both_be_valid(self):
        for key in ('viewport_width', 'viewport_height', 'image_width', 'image_height'):
            for value in (0, -1, True, 8193, 2**32, float('nan')):
                report = fixture(); row = report['native_turn_diagnostics']['samples'][0]['callbacks'][0]
                row[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError): verify(report)

    def test_early_skips_and_legacy_do_not_invent_a_world_partition(self):
        for mode, rendered, key in (('legacy', True, 'legacy_draws'), ('cpu', False, 'skipped_draws')):
            report = fixture(); sample = report['native_turn_diagnostics']['samples'][0]
            sample.update(cpu_draws=0, gpu_draws=0, draw_calls=1, max_depth=1)
            sample[key] = 1
            row = sample['callbacks'][0]
            row.update(backend=mode, rendered=rendered, image_width=0, image_height=0,
                       boundaries=[dict(tag='entry', offset_ns=0), dict(tag='returned', offset_ns=400)])
            result = verify(report)
            self.assertEqual(result['groups'][0]['callbacks']['world_callbacks'], 5)
            self.assertEqual(result['groups'][0]['callbacks']['other_callbacks_wall_ns'], 400)

    def test_fresh_cpu_report_is_a_subset_of_one_module_call(self):
        report = fixture(); sample = report['native_turn_diagnostics']['samples'][0]
        sample.update(cpu_draws=1, gpu_draws=0, draw_calls=1, max_depth=1)
        row = sample['callbacks'][0]
        row.update(backend='cpu', cpu_metrics=dict(reports=1, available=True, width=640, height=360,
                   raster_calls=5, triangles=1234, cache_bytes=8192, frame_ms=.0001,
                   geometry_ms=.00008, raster_ms=.00007, effects_ms=.00004))
        self.assertTrue(verify(report)['success'])
        self.assertFalse(verify(report)['cpu_metric_components_adjacent'])

    def test_stale_or_mismatched_cpu_report_cannot_qualify(self):
        for key, value in (('reports', 2), ('reports', True), ('available', True), ('frame_ms', .1),
                           ('geometry_ms', float('nan')), ('raster_ms', -1), ('width', 640),
                           ('raster_calls', 1), ('triangles', 2**64)):
            report = fixture(); cpu = report['native_turn_diagnostics']['samples'][0]['callbacks'][0]['cpu_metrics']
            cpu[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): verify(report)
        report = fixture(); row = report['native_turn_diagnostics']['samples'][0]['callbacks'][0]
        row['cpu_metrics'].update(reports=1, available=True, width=640, height=360)
        with self.assertRaises(ValueError): verify(report)  # GPU callback cannot report CPU image work.

    def test_source_dimensions_cannot_be_missing_or_invented(self):
        for key, value in (('source_cells', True), ('source_cells', -1), ('source_cells', 2**32),
                           ('source_slices', 0), ('source_slices', 64), ('source_slices', True)):
            report = fixture(); row = report['native_turn_diagnostics']['samples'][0]['callbacks'][0]
            row[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): verify(report)




class StairAttributionTests(unittest.TestCase):
    @staticmethod
    def report():
        from verify_turn_attribution import STAIR_SCOPE
        report=fixture();base=copy.deepcopy(report['native_turn_diagnostics']['samples'][0]);rows=[];z=0
        for delta in (1,-1,-1,1):
            frame=copy.deepcopy(base)
            rows.append(dict(from_z=z,to_z=z+delta,delta=delta,requested_renderer='gpu',successful_present=True,
                action=copy.deepcopy(base),first_frame=frame,frame_stages={k:0 for k in ('full_frame_wall_ms','native_cache_cpu_ms','native_tiles_cpu_ms','perspective_callback_wall_ms','actual_present_wall_ms')}));z+=delta
        report['stair_transition_diagnostics']=dict(schema=1,scope=STAIR_SCOPE,ordinary_performance_measurement=False,gpu_execution_measured=False,samples=rows)
        return report

    def test_exact_four_adjacent_transitions(self):
        from verify_turn_attribution import verify_stair_transitions
        self.assertTrue(verify_stair_transitions(self.report())['success'])

    def test_partial_reordered_stale_or_nonfinite_stairs_rejected(self):
        from verify_turn_attribution import verify_stair_transitions
        for kind in ('missing','reordered','delta_bool','wrong_z','no_present','partial','partition','stale_cpu','missing_stage','nan_stage','oversized_stage','invented_gpu'):
            report=self.report();data=report['stair_transition_diagnostics'];rows=data['samples'];row=rows[0]
            if kind=='missing': rows.pop()
            elif kind=='reordered': rows[0],rows[1]=rows[1],rows[0]
            elif kind=='delta_bool': row['delta']=True
            elif kind=='wrong_z': row['to_z']=2
            elif kind=='no_present': row['successful_present']=False
            elif kind=='partial': row['action']['closed']=False
            elif kind=='partition': row['first_frame']['other_wall_ns']+=1
            elif kind=='stale_cpu': row['first_frame']['callbacks'][0]['cpu_metrics']['reports']=1
            elif kind=='missing_stage': row['frame_stages'].pop('actual_present_wall_ms')
            elif kind=='nan_stage': row['frame_stages']['actual_present_wall_ms']=float('nan')
            elif kind=='oversized_stage': row['frame_stages']['actual_present_wall_ms']=99999
            else: data['gpu_execution_measured']=True
            with self.subTest(kind=kind),self.assertRaises(ValueError):verify_stair_transitions(report)


if __name__ == '__main__':
    unittest.main()
