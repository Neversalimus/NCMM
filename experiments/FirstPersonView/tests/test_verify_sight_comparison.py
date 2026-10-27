import copy
import json
from pathlib import Path
import tempfile
import unittest
from verify_sight_comparison import SCENES, verify, verify_directory


def valid(scene='street'):
    report = dict(schema=1, success=True, scene=scene, actual_cdda_sdl=True,
                  manual_playthrough=False, same_process=True, origin_preserved=True,
                  moves_preserved=True, calendar_preserved=True, gpu_execution_measured=False,
                  first_correct_present_measured=False, scope='native_3d_caster_only',
                  timing='steady_clock_wall_batch_ms',
                  world_state='warmed_native_caches_no_simulation_turns',
                  reference='original_sight_formula_generic_forwarding',
                  candidate='retained_exact_one_entry_cache', width=132, height=132,
                  layers=21, max_view_distance=60, offset_distance=0, repetitions=8,
                  protected_native_bytes=132*132*21*13, numerator=1.0,
                  origin=[60,60,0], direction='up', native_before='1456712345671234567')
    report['samples'] = [dict(sample=i, reference_first=i % 2 == 0,
        output_bits_equal=True, protected_native_bytes_equal=True,
        output_floats_compared=132*132*21, reference_digest='123456789', cached_digest='123456789',
        native_after=report['native_before'], reference_wall_ms=32.0, cached_wall_ms=36.0)
        for i in range(9)]
    return report


class SightComparisonTests(unittest.TestCase):
    def test_regression_remains_a_valid_measurement_without_speed_gate(self):
        result = verify(valid(), 'street')
        self.assertEqual(result['reference_median_ms'], 4.0)
        self.assertEqual(result['cached_median_ms'], 4.5)
        self.assertEqual(result['paired_ratio_median'], 1.125)
        self.assertEqual(result['regressions'], 9)
        self.assertFalse(result['first_correct_present_qualified'])
        self.assertFalse(result['ordinary_game_fps_qualified'])

    def test_missing_top_level_receipts_fail_closed(self):
        for key in valid():
            report=valid(); del report[key]
            with self.subTest(key=key), self.assertRaises((ValueError, KeyError)):
                verify(report, 'street')

    def test_scope_and_protected_state_cannot_be_weakened(self):
        for key, value in dict(schema=True, success=False, actual_cdda_sdl=1, same_process=False,
            origin_preserved=False, moves_preserved=False, calendar_preserved=False,
            manual_playthrough=True, gpu_execution_measured=True, first_correct_present_measured=True,
            scene='forest', scope='whole_frame', world_state='cold_load',
            reference='cached', candidate='approximate', width=131, layers=1,
            offset_distance=1, max_view_distance=59, repetitions=7,
            protected_native_bytes=1, numerator=0.9, origin=[-1,0,0], direction='down').items():
            report=valid(); report[key]=value
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report, 'street')

    def test_incomplete_duplicate_and_reordered_pairs_are_rejected(self):
        report=valid()
        for samples in (report['samples'][:-1], report['samples']+report['samples'][:1],
                        report['samples'][::-1], [report['samples'][0]]*9):
            changed=copy.deepcopy(report); changed['samples']=samples
            with self.subTest(samples=len(samples)), self.assertRaises(ValueError): verify(changed, 'street')

    def test_nonfinite_zero_and_boolean_times_are_rejected(self):
        for key in ('reference_wall_ms','cached_wall_ms'):
            for value in (0, -1, True, '12', None, float('nan'), float('inf'), float('-inf')):
                report=valid(); report['samples'][4][key]=value
                with self.subTest(key=key,value=value), self.assertRaises(ValueError): verify(report, 'street')

    def test_each_pair_must_protect_all_native_and_output_bytes(self):
        for key, value in dict(sample=True, reference_first=False, output_bits_equal=1,
            protected_native_bytes_equal=False, output_floats_compared=1,
            reference_digest='18446744073709551616', cached_digest='12', native_after='42').items():
            report=valid(); report['samples'][0][key]=value
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report, 'street')

    def test_each_sample_field_is_required(self):
        for key in valid()['samples'][0]:
            report=valid(); del report['samples'][0][key]
            with self.subTest(key=key), self.assertRaises(ValueError): verify(report, 'street')

    def test_declared_digest_format_is_bounded_and_canonical(self):
        for value in ('', '0x42', '-1', '00', '18446744073709551616', True, 123):
            report=valid(); report['native_before']=value
            with self.subTest(value=value), self.assertRaises(ValueError): verify(report, 'street')

    def test_native_origin_shape_and_axis_bounds(self):
        for origin in ([1,2], [1,2,0,4], [True,0,0], [132,0,0], [0,132,0], [0,0,-11], [0,0,11]):
            report=valid(); report['origin']=origin
            with self.subTest(origin=origin), self.assertRaises(ValueError): verify(report, 'street')

    def test_both_actual_directions_and_xyz_extremes_are_allowed(self):
        for origin in ([0,0,-10], [131,131,10]):
            for direction in ('up','both'):
                report=valid(); report.update(origin=origin,direction=direction)
                self.assertEqual(verify(report, 'street')['pairs'], 9)

    def test_directory_requires_exact_scene_files_and_utf8_bom(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)
            for scene in SCENES:
                (path/('first-person-sight-cache-'+scene+'.json')).write_text(json.dumps(valid(scene)), encoding='utf-8-sig')
            self.assertEqual(len(verify_directory(path)), 3)
            extra=path/'first-person-sight-cache-duplicate.json'
            extra.write_text(json.dumps(valid()))
            with self.assertRaises(ValueError): verify_directory(path)
            extra.unlink()
            (path/'first-person-sight-cache-street.json').unlink()
            with self.assertRaises(ValueError): verify_directory(path)


if __name__ == '__main__':
    unittest.main()
