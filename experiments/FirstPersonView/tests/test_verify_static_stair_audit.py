"""Contrived verifier inputs and pixels only; never Native diagnostic evidence."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image, ImageDraw
from verify_static_stair_audit import (BODY, CAMERA, NAV, ROI, STRUCTURE, THRESHOLDS,
                                       RETAINED_NAV, RETAINED_THRESHOLDS, verify)


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


def retained_fixture():
    result = fixture()
    result.update(schema=2, retained_thresholds=copy.deepcopy(RETAINED_THRESHOLDS))
    for case in result['cases']:
        case.update(matrix_eligible=False, inconclusive_reason='terrain_semantics_already_published',
                    matrix00_normal_pixels_equal=False, matrix=[])
        for phase in case['phases']:
            if phase['name'] != 'debug':
                phase['target']['flags'] |= RETAINED_NAV
                phase['target']['direction'] = 2
                phase['triangles'] = 124
        last = case['phases'][-1]
        retained = dict(eligible=True, inconclusive_reason='', normal_static_pixels_equal=True, frames=[])
        for keep, name in ((1, 'normal_static'), (0, 'nav_removed')):
            frame = copy.deepcopy(last)
            flags = last['target']['flags'] if keep else last['target']['flags'] & ~RETAINED_NAV
            direction = last['target']['direction'] if keep else 0
            frame.update(name=name, file=f'first-person-static-stair-{case["name"]}-{name}.bmp',
                         body=0, semantics=keep, target_flags=flags, target_direction=direction,
                         world_digest='100' if keep else '400', native_source_digest='100',
                         published_light=last['target']['published_light'].copy(), triangles=124 if keep else 100)
            frame['target'].update(flags=flags, direction=direction)
            retained['frames'].append(frame)
        case['retained_geometry'] = retained
    return result


def paint_image(root, filename, intensity=0, inside=True, count=8):
    image = Image.new('RGB', (640, 480), (20, 20, 20))
    if intensity:
        x, y = (ROI[0], ROI[1]) if inside else (10, 10)
        ImageDraw.Draw(image).line((x, y, x + count - 1, y), fill=(20 + intensity, 20, 20))
    image.save(root / filename)


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
        paint_image(cls.root, filename, intensity, inside, count)

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


class RetainedStairEvidenceTests(unittest.TestCase):
    """Synthetic retained controls are distinct from the old factorial evidence."""
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.baseline = retained_fixture()
        for case in cls.baseline['cases']:
            for frame in case['phases'] + case['retained_geometry']['frames']:
                paint_image(cls.root, frame['file'], 0 if frame['name'] in ('debug', 'nav_removed') else 4)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.report = copy.deepcopy(self.baseline)
        self.case = self.report['cases'][0]
        self.packet = self.case['retained_geometry']
        self.static, self.removed = self.packet['frames']
        self.changed_files = set()

    def tearDown(self):
        for filename in self.changed_files:
            paint_image(self.root, filename, 0 if filename.endswith(('-debug.bmp', '-nav_removed.bmp')) else 4)

    def run_report(self):
        (self.root / 'first-person-static-stair-audit.json').write_text(json.dumps(self.report))
        return verify(self.root)

    def alter_pixels(self, frame, intensity=0, inside=True, count=8):
        self.changed_files.add(frame['file'])
        paint_image(self.root, frame['file'], intensity, inside, count)

    def alter_normal_pixels(self, intensity=4, inside=True, count=8):
        for frame in (self.case['phases'][0], self.case['phases'][2], self.static):
            self.alter_pixels(frame, intensity, inside, count)

    def test_retained_geometry_is_a_separate_status(self):
        result = self.run_report()
        self.assertEqual(result['status'], 'retained_geometry_verified')
        case = result['cases'][0]
        self.assertEqual(case['factorial_status'], 'inconclusive')
        self.assertIsNone(case['interaction_triangles'])
        self.assertEqual(case['retained_geometry']['geometry_triangles'], 24)
        self.assertEqual(case['retained_geometry']['geometry_pixels'], 8)
        self.assertFalse(result['ordinary_performance_measurement'])

    def test_schema2_still_verifies_the_unchanged_missing_nav_factorial(self):
        report = fixture()
        report.update(schema=2, retained_thresholds=copy.deepcopy(RETAINED_THRESHOLDS))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for case in report['cases']:
                case['retained_geometry'] = dict(eligible=False, inconclusive_reason='normal_terrain_navigation_incomplete',
                                                 normal_static_pixels_equal=False, frames=[])
                for frame in case['phases'] + case['matrix']:
                    paint_image(root, frame['file'], 4 if frame['name'] == 'matrix11' else 0)
            (root / 'first-person-static-stair-audit.json').write_text(json.dumps(report))
            self.assertEqual(verify(root)['status'], 'confirmed')

    def test_schema1_cannot_smuggle_new_fields_and_unknown_schema_is_rejected(self):
        self.report['schema'] = 1
        with self.assertRaisesRegex(AssertionError, 'Schema1'): self.run_report()
        del self.report['retained_thresholds']
        with self.assertRaisesRegex(AssertionError, 'Schema1'): self.run_report()
        self.report['schema'] = 3
        with self.assertRaises(AssertionError): self.run_report()

    def test_schema2_requires_exact_protocol_and_fixed_thresholds(self):
        for change in ('missing_thresholds', 'changed_thresholds', 'missing_packet', 'extra_packet_key'):
            with self.subTest(change=change):
                self.report = copy.deepcopy(self.baseline)
                if change == 'missing_thresholds': del self.report['retained_thresholds']
                elif change == 'changed_thresholds': self.report['retained_thresholds']['geometry_pixels'] = 7
                elif change == 'missing_packet': del self.report['cases'][0]['retained_geometry']
                else: self.report['cases'][0]['retained_geometry']['reported_pass'] = True
                with self.assertRaises(AssertionError): self.run_report()

    def test_eligibility_cannot_be_forced_for_body_or_missing_structure_or_nonordinary_stair(self):
        for remove, add in ((0, BODY), (STRUCTURE, 0), (256, 0), (16384, 0), (0, 1024), (0, 2048)):
            with self.subTest(remove=remove, add=add):
                self.report = copy.deepcopy(self.baseline)
                for phase in self.report['cases'][0]['phases']:
                    if phase['name'] != 'debug': phase['target']['flags'] = phase['target']['flags'] & ~remove | add
                    elif remove & RETAINED_NAV: phase['target']['flags'] &= ~remove
                with self.assertRaisesRegex(AssertionError, 'Retained eligibility'): self.run_report()

    def test_real_terrain_up_cannot_be_replaced_by_furniture(self):
        self.case['phases'][1]['target'].update(raw_terrain_up=False, raw_furniture_up=True)
        with self.assertRaises(AssertionError): self.run_report()

    def test_nav_removal_preserves_body_structure_down_and_all_other_bits(self):
        for remove, add in ((0, BODY), (STRUCTURE, 0), (512, 0), (0, 8)):
            with self.subTest(remove=remove, add=add):
                self.removed['target_flags'] = self.baseline['cases'][0]['retained_geometry']['frames'][1]['target_flags'] & ~remove | add
                with self.assertRaisesRegex(AssertionError, 'unrelated terrain flags'): self.run_report()

    def test_directions_and_normal_illumination_are_fixed(self):
        for frame, key, value in ((self.static, 'target_direction', 0), (self.removed, 'target_direction', 1),
                                  (self.static, 'published_light', [.2, .1, .1])):
            before = frame[key]; frame[key] = value
            with self.assertRaises(AssertionError): self.run_report()
            frame[key] = before

    def test_false_normal_static_identity_or_repeat_flag_is_rejected(self):
        self.packet['normal_static_pixels_equal'] = False
        with self.assertRaisesRegex(AssertionError, 'normal_static pixels'): self.run_report()
        self.packet['normal_static_pixels_equal'] = True
        self.alter_pixels(self.static, 0)
        with self.assertRaisesRegex(AssertionError, 'normal_static pixels'): self.run_report()

    def test_retained_snapshot_source_and_fresh_raster_are_required(self):
        for key, value in (('protected_snapshot_digest', '42'), ('native_source_digest', '42'),
                           ('world_digest', '42'), ('raster_calls', 0)):
            before = self.static[key]; self.static[key] = value
            with self.assertRaises(AssertionError): self.run_report()
            self.static[key] = before

    def test_pair_cannot_be_missing_reordered_or_marked_ineligible(self):
        original = self.packet['frames']
        for frames in ([], original[::-1], original[:1]):
            self.packet['frames'] = frames
            with self.assertRaisesRegex(AssertionError, 'retained controls'): self.run_report()
        self.packet.update(eligible=False, frames=[], normal_static_pixels_equal=False, inconclusive_reason='Ignored valid target')
        with self.assertRaisesRegex(AssertionError, 'Retained eligibility'): self.run_report()

    def test_hidden_contents_and_focus_remain_identical_in_both_owned_frames(self):
        for frame in (self.static, self.removed):
            for key, value in (('furniture', 'f_chair'), ('item', 'rock'), ('creature', 'mon_zombie'),
                               ('field', 'fd_smoke'), ('focus_result', 1), ('target_result', 1)):
                before = frame['hidden_control'][key]; frame['hidden_control'][key] = value
                with self.assertRaisesRegex(AssertionError, 'protected contents/focus'): self.run_report()
                frame['hidden_control'][key] = before

    def test_target_appearance_furniture_and_vision_are_protected(self):
        for key, value in (('terrain', 't_stairs_down'), ('furniture', 'f_chair'), ('item', 'rock'), ('native_sees', True)):
            before = self.static['target'][key]; self.static['target'][key] = value
            with self.assertRaisesRegex(AssertionError, 'protected observations'): self.run_report()
            self.static['target'][key] = before
        self.static['debug_nightvision'] = True
        with self.assertRaisesRegex(AssertionError, 'changed vision'): self.run_report()

    def test_physical_raw_light_memory_and_discovery_cannot_change_during_pair(self):
        for group in ('physical', 'memory', 'overmap'):
            record = self.removed['ledger_after'][group]
            if group == 'physical': before = record['light']['hash']; record['light']['hash'] = '42'
            else: before = record['hash']; record['hash'] = '42'
            with self.assertRaisesRegex(AssertionError, 'Native state/discovery'): self.run_report()
            if group == 'physical': record['light']['hash'] = before
            else: record['hash'] = before

    def test_raw_phase_light_drift_is_honestly_inconclusive(self):
        self.case['phases'][1]['ledger_after']['physical']['light']['hash'] = '42'
        with self.assertRaisesRegex(AssertionError, 'Retained eligibility'): self.run_report()
        self.packet.update(eligible=False, frames=[], normal_static_pixels_equal=False, inconclusive_reason='physical_light_mismatch')
        self.assertEqual(self.run_report()['cases'][0]['status'], 'inconclusive')

    def test_subthreshold_pixels_or_contrast_cannot_verify_retention(self):
        for intensity, count, inside in ((3, 8, True), (4, 7, True), (4, 8, False)):
            self.alter_normal_pixels(intensity, inside, count)
            self.assertEqual(self.run_report()['cases'][0]['retained_geometry']['status'], 'inconclusive')

    def test_triangle_removal_is_signed_and_independent_of_pixels(self):
        for triangles in (101, 125):
            self.removed['triangles'] = triangles
            self.assertEqual(self.run_report()['cases'][0]['retained_geometry']['status'], 'inconclusive')
        self.static['triangles'] = 125
        with self.assertRaisesRegex(AssertionError, 'real normal snapshot'): self.run_report()


if __name__ == '__main__':
    unittest.main()
