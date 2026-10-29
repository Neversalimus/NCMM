import copy
import hashlib
import json
import os
from pathlib import Path
import runpy
import tempfile
import types
import unittest
from unittest import mock

import run_retirement_pair as runner
import run_terrain_pair as progress_reader
import test_verify_turn_attribution as turn_fixture
from verify_retirement_pair import (CANDIDATE_SHA, CANDIDATE_SOURCE, PAIR_ACTIONS, PAIR_SCENES, PARSER_SHA,
                                    REFERENCE_SHA, REFERENCE_SOURCE, sha, verify)


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def fixture(root):
    inputs = {f'input-{i}.hpp': 'a' * 64 for i in range(101)}
    inputs['ncmm_gpu_backend.hpp'] = CANDIDATE_SHA
    save(root / 'native-inputs.json', {'candidate': inputs, 'reference': dict(inputs, **{'ncmm_gpu_backend.hpp': REFERENCE_SHA})})
    save(root / 'fixture-files.json', {'config/options.json': 'b' * 64})
    report = turn_fixture.StairAttributionTests.report()
    report.update(schema=1, success=True, width=640, height=480,
                  retirement_pair_only=True, full_gameplay_coverage=False,
                  actions=[dict(name=name, success=True) for name in sorted(PAIR_ACTIONS)],
                  soak=dict(automated=True, requested_seconds=60, actual_wall_seconds=60.1, frames=5),
                  tilesets=[dict(id='UltimateCataclysm', actual_loaded_id='UltimateCataclysm', cases=[
                      dict(name=name, renderer='gpu', device='CI software device', backend='direct3d12', software_device=True) for name in sorted(PAIR_SCENES)])],
                  native_action_stages={'native_stair_first_ready_wall_ms':
                  dict(samples=8, median=4., p95=5., p99=6., max=7.)})
    pair = dict(schema=1, pairs=7, phase='complete', diagnostic_only=True, ordinary_game_performance=False,
                fixture_mode='focused_turns_stairs',
                whole_historical_parent_measured=False, source_commit='1' * 40, source_tree='2' * 40,
                reference_backend_source=REFERENCE_SOURCE, candidate_backend_source=CANDIDATE_SOURCE,
                reference_backend_sha256=REFERENCE_SHA, candidate_backend_sha256=CANDIDATE_SHA,
                engine_commit='3f7fb352bf492ba521bd9408a0c9f6ce239e8d83', turn_parser_sha256=PARSER_SHA,
                host_restored=True, source_restored=True, fixture_restored=True,
                host_sha256={'reference': 'c' * 64, 'candidate': 'd' * 64}, module_sha256='e' * 64,
                fixture_digest=sha(root / 'fixture-files.json'), native_inputs_sha256=sha(root / 'native-inputs.json'), trials=[])
    for ordinal in range(14):
        index, order = divmod(ordinal, 2)
        kind = ('reference', 'candidate')[(index + order) % 2]
        directory = f'pair-{index + 1:02d}-{kind}'
        process = dict(state='exited', exit_code=0, timeout=False, error_type=None, pid=100 + ordinal,
                       started_ns=10, ended_ns=20, source_commit=pair['source_commit'], engine_commit=pair['engine_commit'],
                       host_sha256=pair['host_sha256'][kind], module_sha256=pair['module_sha256'], fixture_digest=pair['fixture_digest'])
        save(root / directory / 'process.json', process)
        save(root / directory / 'gameplay.json', report)
        pair['trials'].append(dict(pair=index, order=order, kind=kind, directory=directory,
                                  process_sha256=sha(root / directory / 'process.json'), report_sha256=sha(root / directory / 'gameplay.json')))
    save(root / 'pair.json', pair)
    return pair


class RetirementPairTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.pair = fixture(self.root)

    def update_process(self, index, change):
        row = self.pair['trials'][index]
        path = self.root / row['directory'] / 'process.json'
        value = json.loads(path.read_text()); value.update(change); save(path, value)
        row['process_sha256'] = sha(path); save(self.root / 'pair.json', self.pair)

    def test_complete_diagnostic_is_not_performance_admission(self):
        result = verify(self.root)
        self.assertEqual(result['native_stair_transitions'], 112)
        self.assertEqual(result['gpu_stair_transitions'], 56)
        self.assertFalse(result['performance_accepted'])
        self.assertEqual(result['comparisons'][0]['native_first_ready_all_eight_summary_ms']['reference']['median'], 4.)

    def test_no_local_launch_build_or_file_mutation(self):
        with mock.patch.dict(os.environ, {}, clear=True), mock.patch('subprocess.run') as run, \
                mock.patch('subprocess.Popen') as popen, mock.patch.object(Path, 'write_bytes') as write:
            with self.assertRaises(ValueError): runner.run(None)
            with self.assertRaises(ValueError): runner.run_build([], None)
            run.assert_not_called(); popen.assert_not_called(); write.assert_not_called()

    def test_wrong_backend_denied_before_reconstruction(self):
        with self.assertRaises(ValueError): runner.reference_backend(b'unreviewed header')

    def test_exact_parent_reconstruction(self):
        path = Path(__file__).resolve().parents[1] / 'backend-under-test.hpp'
        if not path.exists():
            path = Path(__file__).resolve().parents[2] / 'graphics_bridge/ncmm_gpu_backend.hpp'
        self.assertEqual(hashlib.sha256(runner.reference_backend(path.read_bytes())).hexdigest(), REFERENCE_SHA)

    def test_stale_source_parser_and_scope_denied(self):
        for key, value in [('reference_backend_source', '0' * 40), ('candidate_backend_sha256', REFERENCE_SHA),
                           ('turn_parser_sha256', '0' * 64), ('whole_historical_parent_measured', True), ('host_restored', False)]:
            changed = copy.deepcopy(self.pair); changed[key] = value; save(self.root / 'pair.json', changed)
            with self.subTest(key=key), self.assertRaises(ValueError): verify(self.root)

    def test_missing_duplicate_and_reordered_children_denied(self):
        for rows in (self.pair['trials'][:-1], list(reversed(self.pair['trials'])), [self.pair['trials'][0]] * 14):
            changed = copy.deepcopy(self.pair); changed['trials'] = rows; save(self.root / 'pair.json', changed)
            with self.assertRaises(ValueError): verify(self.root)

    def test_failed_timeout_or_changed_binary_denied(self):
        for change in ({'exit_code': 1}, {'exit_code': False}, {'timeout': True}, {'state': 'running'}, {'host_sha256': '0' * 64}):
            self.pair = fixture(self.root); self.update_process(0, change)
            with self.subTest(change=change), self.assertRaises(ValueError): verify(self.root)

    def test_corrupt_old_stair_gate_is_still_rejected(self):
        row = self.pair['trials'][0]; path = self.root / row['directory'] / 'gameplay.json'
        report = json.loads(path.read_text()); report['stair_transition_diagnostics']['samples'][4]['resource_preparation']['wall_ns'] += 1
        save(path, report); row['report_sha256'] = sha(path); save(self.root / 'pair.json', self.pair)
        with self.assertRaises(ValueError): verify(self.root)

    def test_focused_scope_and_all_native_actions_are_required(self):
        for change in ({'retirement_pair_only': False}, {'full_gameplay_coverage': True}, {'actions': []}):
            self.pair = fixture(self.root); row = self.pair['trials'][0]
            path = self.root / row['directory'] / 'gameplay.json'; report = json.loads(path.read_text())
            report.update(change); save(path, report); row['report_sha256'] = sha(path); save(self.root / 'pair.json', self.pair)
            with self.subTest(change=change), self.assertRaises(ValueError): verify(self.root)

    def test_all_twelve_gpu_scene_descriptors_still_required(self):
        row = self.pair['trials'][0]; path = self.root / row['directory'] / 'gameplay.json'
        report = json.loads(path.read_text()); report['tilesets'][0]['cases'].pop()
        save(path, report); row['report_sha256'] = sha(path); save(self.root / 'pair.json', self.pair)
        with self.assertRaises(ValueError): verify(self.root)

    def test_original_sixty_second_soak_cannot_be_shortened(self):
        for change in ({'requested_seconds': 0}, {'actual_wall_seconds': 59.9}, {'frames': 0}):
            self.pair = fixture(self.root); row = self.pair['trials'][0]
            path = self.root / row['directory'] / 'gameplay.json'; report = json.loads(path.read_text())
            report['soak'].update(change); save(path, report); row['report_sha256'] = sha(path); save(self.root / 'pair.json', self.pair)
            with self.subTest(change=change), self.assertRaises(ValueError): verify(self.root)

    def test_focused_report_cannot_pass_full_gameplay_qualification(self):
        save(self.root / 'first-person-gameplay-audit.json', {'retirement_pair_only': True, 'full_gameplay_coverage': False})
        args = types.SimpleNamespace(root=self.root, minimum_soak_seconds=60, output=self.root / 'full.json')
        with mock.patch('argparse.ArgumentParser.parse_args', return_value=args), \
                mock.patch('subprocess.Popen', side_effect=AssertionError('No local child')), \
                mock.patch('subprocess.run', side_effect=AssertionError('No local command')):
            with self.assertRaisesRegex(AssertionError, 'Focused pair report cannot qualify as full gameplay'):
                runpy.run_path(str(Path(__file__).with_name('verify_gameplay_audit.py')), run_name='focused_denial_test')
        self.assertFalse((self.root / 'full.json').exists())

    def test_changed_viewport_device_or_ready_summary_denied(self):
        for change in ('viewport', 'device', 'ready'):
            self.pair = fixture(self.root); row = self.pair['trials'][1]
            path = self.root / row['directory'] / 'gameplay.json'; report = json.loads(path.read_text())
            if change == 'viewport': report['width'] = 1280
            elif change == 'device':
                for case in report['tilesets'][0]['cases']: case['device'] = 'Other device'
            else: report['native_action_stages']['native_stair_first_ready_wall_ms']['median'] = float('nan')
            save(path, report); row['report_sha256'] = sha(path); save(self.root / 'pair.json', self.pair)
            with self.subTest(change=change), self.assertRaises(ValueError): verify(self.root)

    def test_moving_cost_out_of_resource_child_is_not_gain(self):
        for row in self.pair['trials']:
            if row['kind'] != 'candidate': continue
            path = self.root / row['directory'] / 'gameplay.json'; report = json.loads(path.read_text())
            for stair in report['stair_transition_diagnostics']['samples'][4:]:
                frame = stair['first_frame']; frame['other_wall_ns'] += frame['wall_ns']; frame['wall_ns'] *= 2
                packet = stair['resource_preparation']; packet['wall_ns'] //= 2
                for part in packet['phases'].values(): part['wall_ns'] //= 2
            save(path, report); row['report_sha256'] = sha(path)
        save(self.root / 'pair.json', self.pair); result = verify(self.root)
        self.assertEqual(result['per_pair_gpu_first_frame_median_ratio'], [2.] * 7)
        self.assertFalse(result['performance_accepted'])


class ProgressReaderTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'ncmm').mkdir()
        self.path = self.root / 'ncmm/first-person-audit-progress.txt'

    def test_actual_native_and_locale_grouped_records(self):
        for value, expected in [
            (b'123 native_pickup_begin frames=4\n', 'native_pickup_begin'),
            (b'1,569,654 native_multilevel_save_load_next frames=2,604\n', 'native_multilevel_save_load_next'),
            (b'3,503,384 native_multilevel_save_load_next frames=14,105\n', 'native_multilevel_save_load_next'),
            (b'1\xa0234 load_tileset_MSX++.1 frames=12\xa0345\n', 'load_tileset_MSX++.1'),
            (b'1\xc2\xa0234 native_pickup PASS frames=12\xc2\xa0345\r\n', 'native_pickup_PASS'),
            (b'1,,234 bad frames=3\n', 'unavailable'),
            (b'1 bad\x1b[2J frames=3\n', 'unavailable'),
            (b'123 ' + b'x' * 129 + b' frames=1\n', 'unavailable'),
        ]:
            self.path.write_bytes(value)
            with self.subTest(value=value):
                self.assertEqual(progress_reader.read_progress_phase(self.root, 0), expected)

    def test_latest_complete_record_from_long_tail(self):
        self.path.write_bytes(b'1 previous frames=0\n' * 1000 + b'2 current frames=40\n3 partial frames=')
        self.assertEqual(progress_reader.read_progress_phase(self.root, 0), 'current')

    def test_missing_stale_or_partial_progress_is_not_labelled_startup(self):
        self.assertEqual(progress_reader.read_progress_phase(self.root, 0), 'unavailable')
        self.path.write_bytes(b'1 old frames=1\n')
        self.assertEqual(progress_reader.read_progress_phase(self.root, self.path.stat().st_mtime_ns + 1_000_000_000), 'unavailable')
        self.path.write_bytes(b'2 unfinished frames=')
        self.assertEqual(progress_reader.read_progress_phase(self.root, 0), 'unavailable')


if __name__ == '__main__':
    unittest.main()
