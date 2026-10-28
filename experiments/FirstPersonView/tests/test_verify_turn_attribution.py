import copy
import unittest
from verify_turn_attribution import BOUNDARIES, CALLBACK_SCOPE, DRAW_SCOPE, GROUPS, OTHER_SCOPE, PHASES, verify


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
                    boundaries=[dict(tag=tag, offset_ns=offset) for tag, offset in zip(BOUNDARIES, (0, 10, 30, 70, 140, 200, 330, 400))])]))
    return dict(actual_cdda_sdl=True, manual_playthrough=False,
                native_turn_diagnostics=dict(schema=2, controlled_fixture_without_interactive_main_loop=True,
                    ordinary_performance_measurement=False, draw_wall_scope=DRAW_SCOPE,
                    remainder_scope=OTHER_SCOPE, callback_scope=CALLBACK_SCOPE, samples=samples))


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
                           ('remainder_scope', 'pure AI'), ('schema', 1), ('callback_scope', 'pure GPU')):
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


if __name__ == '__main__':
    unittest.main()
