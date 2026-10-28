import copy
import unittest
from verify_turn_attribution import DRAW_SCOPE, GROUPS, OTHER_SCOPE, verify


def fixture():
    samples = []
    for enemies, count in GROUPS:
        for i in range(count):
            samples.append(dict(initial_enemies=enemies, round=i, requested_renderer='gpu' if enemies else 'cpu',
                claimed=True, returned=True, closed=True, valid=True, errors=0,
                wall_ns=1000, draw_wall_ns=400, other_wall_ns=600, draw_calls=2, outer_draws=1,
                max_depth=2, cpu_draws=1, gpu_draws=1, legacy_draws=0, skipped_draws=0))
    return dict(actual_cdda_sdl=True, manual_playthrough=False,
                native_turn_diagnostics=dict(schema=1, controlled_fixture_without_interactive_main_loop=True,
                    ordinary_performance_measurement=False, draw_wall_scope=DRAW_SCOPE,
                    remainder_scope=OTHER_SCOPE, samples=samples))


class TurnAttributionTests(unittest.TestCase):
    def test_nested_intervals_are_not_added_as_independent_work(self):
        result = verify(fixture())
        self.assertEqual(result['controlled_native_turns'], 26)
        self.assertEqual([x['draw_wall_ns'] for x in result['groups']], [2400, 2400, 2400, 3200])
        self.assertFalse(result['pure_ai_time_measured'])
        self.assertFalse(result['first_correct_present_measured'])

    def test_no_render_inside_a_turn_is_valid_and_honest(self):
        report = fixture()
        sample = report['native_turn_diagnostics']['samples'][0]
        for key in ('draw_calls', 'outer_draws', 'max_depth', 'cpu_draws', 'gpu_draws', 'draw_wall_ns'):
            sample[key] = 0
        sample['other_wall_ns'] = sample['wall_ns']
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
                           ('remainder_scope', 'pure AI')):
            report = fixture(); report['native_turn_diagnostics'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report)
        report = fixture(); report['actual_cdda_sdl'] = False
        with self.assertRaises(ValueError): verify(report)


if __name__ == '__main__':
    unittest.main()
