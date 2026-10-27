"""Contrived verifier inputs and pixels only; never Native diagnostic evidence."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image, ImageDraw
from verify_static_stair_audit import BODY, CAMERA, NAV, ROI, THRESHOLDS, verify


def observation(furniture=False, hidden=False, debug=False):
    flags = 1 | 512 | 262144
    if debug and not furniture and not hidden:
        flags |= BODY | 256 | 16384
    return dict(present=True, index=5 if hidden else 4 if furniture else 3, flags=flags,
                direction=2 if debug and not furniture and not hidden else 0,
                terrain='t_stairs_up' if not furniture and not hidden else 't_floor',
                furniture='', item='', creature='', field='',
                raw_terrain='t_stairs_up' if not furniture and not hidden else 't_floor',
                raw_furniture='f_ncmm_static_stair_control' if furniture else 'f_chair' if hidden else 'f_null',
                raw_terrain_up=not furniture and not hidden, raw_furniture_up=furniture,
                native_sees=debug and not hidden, ambient_light=1.0, light=[.1, .1, .1], published_light=[.1, .1, .1],
                focus_attempted=True, focus_result=0, target_result=0)


def fixture():
    unit = dict(count=216, hash='9001')
    state = dict(avatar=[0, 0, 0], origin=[-48, -48, 0], avatar_moves=0, avatar_hp=100,
                 avatar_hp_max=100, turn=0, weather='sunny', winddirection=0, windspeed=0,
                 lightning=False, camera=CAMERA.copy(), eye_height=1.55, animation_seconds=1000.5)
    physical = {key: copy.deepcopy(unit) for key in ('terrain', 'furniture', 'light')}
    physical.update(scalars=state, fields=dict(count=0, hash='1'), map_items=dict(count=1, hash='2'),
                    avatar_items=dict(count=0, hash='3'), creatures=dict(count=1, hash='4'), vehicles=dict(count=0, hash='5'))
    ledger = dict(physical=physical, memory=dict(count=216, hash='6', valid=True, show=True),
                  overmap=dict(count=1, hash='7', resident_tiles=1))
    result = dict(schema=1, success=True, diagnostic_only=True, actual_cdda_sdl=True, preflight_frames=2,
                  renderer='cpu', width=640, height=480, fixture_camera=CAMERA.copy(), thresholds=copy.deepcopy(THRESHOLDS), cases=[])
    for scene in ('day', 'dusk', 'night'):
        case = dict(name=scene, roi=ROI.copy(), target=[0, -3, 0], furniture_control=[2, -3, 0],
                    hidden_control=[-2, -6, 0], physical_bounds=[[-4, -7, 0], [4, 4, 1]],
                    memory_bounds=[[-4, -7, 0], [4, 4, 1]], omt_bounds=[[0, 0, 0], [0, 0, 0]],
                    matrix_eligible=True, inconclusive_reason='', normal_repeat_pixels_equal=True,
                    matrix00_normal_pixels_equal=True, phases=[], matrix=[])
        for name in ('normal_before', 'debug', 'normal_after'):
            debug = name == 'debug'
            case['phases'].append(dict(name=name, debug_nightvision=debug,
                ledger_before=copy.deepcopy(ledger), ledger_after=copy.deepcopy(ledger),
                target=observation(debug=debug), furniture_control=observation(furniture=True, debug=debug),
                hidden_control=observation(hidden=True, debug=debug), world_digest='200' if debug else '100',
                protected_snapshot_digest='901' if debug else '900', pixel_fingerprint='1000',
                file=f'first-person-static-stair-{scene}-{name}.bmp', triangles=100,
                raster_calls=1, raster_width=640, raster_height=480))
        for name in ('matrix11', 'matrix10', 'matrix01', 'matrix00'):
            body, semantics = map(int, name[-2:])
            flags = 1 | 512 | 262144 | (BODY if body else 0) | ((256 | 16384) if semantics else 0)
            case['matrix'].append(dict(name=name, body=body, semantics=semantics,
                target_flags=flags, target_direction=2 if semantics else 0,
                world_digest={'matrix11':'111', 'matrix10':'110', 'matrix01':'101', 'matrix00':'100'}[name],
                protected_snapshot_digest='900', pixel_fingerprint='1000', published_light=[.1, .1, .1],
                file=f'first-person-static-stair-{scene}-{name}.bmp', triangles=124 if name == 'matrix11' else 100,
                raster_calls=1, raster_width=640, raster_height=480, ledger_after=copy.deepcopy(ledger)))
        result['cases'].append(case)
    return result


class StaticStairEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.baseline = fixture()
        for case in cls.baseline['cases']:
            for frame in case['phases'] + case['matrix']:
                cls.paint(frame['file'], 4 if frame['name'] == 'matrix11' else 0)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @classmethod
    def paint(cls, filename, intensity=0, inside=True, count=8):
        image = Image.new('RGB', (640, 480), (20, 20, 20))
        if intensity:
            x, y = (ROI[0], ROI[1]) if inside else (10, 10)
            ImageDraw.Draw(image).line((x, y, x + count - 1, y), fill=(20 + intensity, 20, 20))
        image.save(cls.root / filename)

    def setUp(self):
        self.report = copy.deepcopy(self.baseline)
        self.case = self.report['cases'][0]
        self.changed_files = set()

    def tearDown(self):
        for filename in self.changed_files:
            self.paint(filename, 4 if filename.endswith('-matrix11.bmp') else 0)

    def alter_pixels(self, record, intensity, inside=True, count=8):
        self.changed_files.add(record['file'])
        self.paint(record['file'], intensity, inside, count)

    def run_report(self):
        (self.root / 'first-person-static-stair-audit.json').write_text(json.dumps(self.report))
        return verify(self.root)

    def ineligible(self, reason):
        self.case.update(matrix_eligible=False, inconclusive_reason=reason,
                         matrix00_normal_pixels_equal=False, matrix=[])

    def test_complete_factorial_fixture(self):
        result = self.run_report()
        self.assertEqual(result['status'], 'confirmed')
        self.assertFalse(result['ordinary_performance_measurement'])
        self.assertEqual(result['cases'][0]['interaction_pixels'], 8)
        self.assertEqual(result['cases'][0]['interaction_triangles'], 24)

    def test_already_visible_stair_is_inconclusive(self):
        for phase in self.case['phases']:
            if phase['name'] != 'debug': phase['target']['flags'] |= BODY
        self.ineligible('Normal terrain target is already body-visible')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_missing_normal_target_is_inconclusive(self):
        for phase in self.case['phases']:
            if phase['name'] != 'debug': phase['target'].update(present=False, index=-1, flags=0)
        self.ineligible('No normal target snapshot')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_different_debug_light_is_inconclusive(self):
        self.case['phases'][1]['target']['light'][0] = .2
        self.ineligible('Published light differs')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_raw_light_cache_drift_is_inconclusive(self):
        self.case['phases'][1]['ledger_after']['physical']['light']['hash'] = '42'
        self.ineligible('Raw light cache differed')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_unusable_memory_cannot_confirm(self):
        for frame in self.case['phases']:
            for key in ('ledger_before', 'ledger_after'):
                frame[key]['memory']['valid'] = False
        self.ineligible('No usable memory ledger')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_honest_nonrepeatable_observation_is_inconclusive(self):
        self.alter_pixels(self.case['phases'][2], 10)
        self.case['normal_repeat_pixels_equal'] = False
        self.ineligible('Normal pixels were not repeatable')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_debug_color_change_does_not_alter_the_normal_factorial_experiment(self):
        self.case['phases'][1]['target']['published_light'][0] = .2
        result = self.run_report()['cases'][0]
        self.assertEqual(result['status'], 'confirmed')
        self.assertTrue(result['debug_published_light_changed'])

    def test_matrix_must_retain_normal_published_illumination(self):
        self.case['matrix'][0]['published_light'][0] = .2
        with self.assertRaisesRegex(AssertionError, 'normal published illumination'): self.run_report()

    def test_incomplete_debug_navigation_is_inconclusive(self):
        self.case['phases'][1]['target']['flags'] &= ~16384
        self.ineligible('Debug observation has no connected upper exit')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_full_host_window_is_not_silently_truncated(self):
        count = 97 * 97 * 32
        for case in self.report['cases']:
            case['physical_bounds'] = case['memory_bounds'] = [[-48, -48, -16], [48, 48, 15]]
            for frame in case['phases'] + case['matrix']:
                keys = ('ledger_before', 'ledger_after') if 'ledger_before' in frame else ('ledger_after',)
                for key in keys:
                    for component in ('terrain', 'furniture', 'light'):
                        frame[key]['physical'][component]['count'] = count
                    frame[key]['memory']['count'] = count
        self.assertEqual(self.run_report()['status'], 'confirmed')

    def test_pixel_threshold_cannot_be_rounded_down(self):
        self.alter_pixels(self.case['matrix'][0], 3)
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_seven_interacting_pixels_are_insufficient(self):
        self.alter_pixels(self.case['matrix'][0], 4, count=7)
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_geometry_threshold_is_independent(self):
        self.case['matrix'][0]['triangles'] = 123
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_additive_changes_are_not_a_causal_interaction(self):
        for frame, intensity in zip(self.case['matrix'], (8, 4, 4, 0)):
            self.alter_pixels(frame, intensity)
        self.assertEqual(self.run_report()['cases'][0]['interaction_pixels'], 0)

    def test_pixels_outside_fixed_roi_do_not_confirm(self):
        self.alter_pixels(self.case['matrix'][0], 4, inside=False)
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_normal_boolean_cannot_hide_changed_pixels(self):
        self.alter_pixels(self.case['phases'][2], 10)
        with self.assertRaisesRegex(AssertionError, 'normal pixels'): self.run_report()

    def test_matrix00_boolean_cannot_hide_changed_pixels(self):
        self.alter_pixels(self.case['matrix'][3], 10)
        with self.assertRaisesRegex(AssertionError, 'matrix00 pixels'): self.run_report()

    def test_missing_or_reordered_controls_are_rejected(self):
        self.case['matrix'][0], self.case['matrix'][1] = self.case['matrix'][1], self.case['matrix'][0]
        with self.assertRaisesRegex(AssertionError, 'factorial controls'): self.run_report()

    def test_changed_owned_contents_are_rejected(self):
        self.case['matrix'][0]['protected_snapshot_digest'] = '42'
        with self.assertRaisesRegex(AssertionError, 'Owned snapshot'): self.run_report()

    def test_unrelated_flags_cannot_change(self):
        self.case['matrix'][0]['target_flags'] |= 8
        with self.assertRaisesRegex(AssertionError, 'unrelated terrain flags'): self.run_report()

    def test_existing_down_flag_cannot_be_removed(self):
        self.case['matrix'][1]['target_flags'] &= ~512
        with self.assertRaisesRegex(AssertionError, 'unrelated terrain flags'): self.run_report()

    def test_native_state_and_discovery_must_remain_constant(self):
        for key in ('physical', 'memory', 'overmap'):
            with self.subTest(key=key):
                self.report = copy.deepcopy(self.baseline); self.case = self.report['cases'][0]
                record = self.case['matrix'][0]['ledger_after'][key]
                if key == 'physical': record['scalars']['avatar_moves'] += 1
                else: record['hash'] = '42'
                with self.assertRaisesRegex(AssertionError, 'Native state/discovery'): self.run_report()

    def test_hidden_identities_and_targets_are_rejected(self):
        for key, value in (('item', 'rock'), ('furniture', 'f_chair'), ('creature', 'mon_zombie'), ('focus_result', 1), ('target_result', 1)):
            with self.subTest(key=key):
                self.report = copy.deepcopy(self.baseline); self.case = self.report['cases'][0]
                self.case['phases'][1]['hidden_control'][key] = value
                with self.assertRaises(AssertionError): self.run_report()

    def test_furniture_up_is_not_terrain_evidence(self):
        self.case['phases'][1]['target'].update(raw_terrain_up=False, raw_furniture_up=True)
        with self.assertRaises(AssertionError): self.run_report()

    def test_raster_cache_hit_cannot_fake_a_control(self):
        self.case['matrix'][0]['raster_calls'] = 0
        with self.assertRaises(AssertionError): self.run_report()

    def test_avatar_cell_is_not_a_visibility_probe(self):
        self.case['target'] = [0, 0, 0]
        with self.assertRaisesRegex(AssertionError, 'forced avatar BODY'): self.run_report()

    def test_capture_paths_cannot_escape_root(self):
        self.case['matrix'][0]['file'] = '../outside.bmp'
        with self.assertRaises(AssertionError): self.run_report()

    def test_nonfinite_observation_is_rejected(self):
        self.case['phases'][1]['target']['light'][0] = float('nan')
        with self.assertRaises(AssertionError): self.run_report()

    def test_threshold_or_roi_tuning_is_rejected(self):
        self.report['thresholds']['interaction_pixels'] = 7
        with self.assertRaisesRegex(AssertionError, 'thresholds changed'): self.run_report()
        self.report = copy.deepcopy(self.baseline)
        self.report['cases'][0]['roi'][0] += 1
        with self.assertRaisesRegex(AssertionError, 'ROI changed'): self.run_report()

    def test_fixed_camera_cannot_be_redeclared_after_observation(self):
        self.report['fixture_camera'][1] += .01
        with self.assertRaisesRegex(AssertionError, 'camera changed'): self.run_report()

    def test_native_camera_must_match_the_declared_fixture(self):
        for case in self.report['cases']:
            for frame in case['phases'] + case['matrix']:
                keys = ('ledger_before', 'ledger_after') if 'ledger_before' in frame else ('ledger_after',)
                for key in keys:
                    frame[key]['physical']['scalars']['camera'][1] = -.35
        with self.assertRaisesRegex(AssertionError, 'fixed fixture'): self.run_report()

    def test_eligibility_is_derived_from_raw_observations(self):
        self.ineligible('Recorder ignored valid controls')
        with self.assertRaisesRegex(AssertionError, 'eligibility'): self.run_report()

    def test_empty_hidden_fixture_is_rejected(self):
        self.case['phases'][0]['ledger_before']['physical']['creatures']['count'] = 0
        with self.assertRaisesRegex(AssertionError, 'Vacuous'): self.run_report()


if __name__ == '__main__':
    unittest.main()
