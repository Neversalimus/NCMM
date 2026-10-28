"""Contrived receipts test fail-closed gates; none are Native timing evidence."""
from contextlib import redirect_stdout
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import run_terrain_pair
from verify_gpu_profile import verify as verify_focused
from verify_terrain_pair import digest, verify as verify_pair
from verify_terrain_profile import SCENES, layout_digest, verify as verify_profile


def report(scenes=SCENES):
    records = []
    for scene in scenes:
        for mode in ('cpu', 'gpu'):
            r = dict(scene=scene, renderer=mode, width=640, height=480, internal_width=640,
                     internal_height=480, frames=96, frame_renderers_gpu=[int(mode == 'gpu')] * 96,
                     median_ms=4., p95_ms=4., p99_ms=4., native_tiles_wall_median_ms=1.,
                     perspective_wall_median_ms=1., actual_present_wall_median_ms=1.,
                     extract_median_ms=.2, callback_median_ms=.2, gpu_geometry_prepare_median_ms=.1,
                     cpu_geometry_median_ms=.1, gpu_resource_update_median_ms=.1,
                     cpu_cache_bytes=32, producer_cpu_cache_bytes_peak=16, host_cpu_cache_bytes_peak=16,
                     gpu_3d_draw_calls=int(mode == 'gpu'), mesh_triangles_submitted=10, upload_bytes_peak=8,
                     backend='contrived', device='synthetic-device', software_device=True, fallback_creatures=0,
                     mode_first_ready_ms=4., warmup_frames=1, warmup_frame_max_ms=4., steady_frame_max_ms=4.,
                     creatures=0, terrain_capture_seconds=1000.5, terrain_layout_digest=layout_digest(scene) if scene in SCENES else 0,
                     warmup_frame_wall_samples=[4.], warmup_native_tiles_wall_samples=[1.],
                     warmup_perspective_wall_samples=[1.], warmup_actual_present_wall_samples=[1.],
                     warmup_cpu_geometry_samples=[.2 if mode == 'cpu' else 0.],
                     warmup_gpu_geometry_prepare_samples=[.2 if mode == 'gpu' else 0.],
                     capture=f'first-person-gpu-{scene}-640x480-{mode}.bmp')
            r.update({name + '_samples': [value] * 96 for name, value in (
                ('frame_wall', 4.), ('native_tiles_wall', 1.), ('perspective_wall', 1.),
                ('actual_present_wall', 1.), ('unattributed_wall', 1.))})
            records.append(r)
    return dict(schema=2, terrain_profile_only=True, ordinary_performance_measurement=False,
                terrain_grid_side=97, stable_cpu_binary=False, actual_cdda_sdl=True,
                focused_reproduction=True, phase_samples_available=True, player_fps=False,
                gpu_timestamps_available=False, records=records)


def write(path, value):
    path.write_text(json.dumps(value), encoding='utf-8')


class TerrainProfileTests(unittest.TestCase):
    def check_profile(self, value):
        with redirect_stdout(io.StringIO()):
            return verify_profile(value)

    def test_complete_profile_and_unchanged_default_gate(self):
        self.assertEqual(self.check_profile(report())['cases'], 6)
        with redirect_stdout(io.StringIO()):
            self.assertEqual(verify_focused(report(('smoke-gas', 'creatures-1')))['cases'], 4)
            with self.assertRaises(AssertionError):
                verify_focused(report())

    def test_bad_scenes_layout_clock_and_partial_samples(self):
        for key, value in (('scene', 'smoke-gas'), ('terrain_layout_digest', 0),
                           ('terrain_capture_seconds', 1001.), ('frames', 95),
                           ('creatures', 1), ('warmup_frames', 0), ('warmup_frame_max_ms', 0.),
                           ('mode_first_ready_ms', 1.), ('warmup_perspective_wall_samples', [5.]),
                           ('warmup_cpu_geometry_samples', [float('nan')]),
                           ('warmup_gpu_geometry_prepare_samples', [1.]),
                           ('perspective_wall_median_ms', 99.), ('frame_wall_samples', [4.] * 95)):
            with self.subTest(key=key):
                value_report = report()
                value_report['records'][0][key] = value
                with self.assertRaises(AssertionError):
                    self.check_profile(value_report)

    def test_reference_metadata_or_player_claim_rejected(self):
        for key, value in (('ordinary_performance_measurement', True), ('stable_cpu_binary', True),
                           ('cpu_algorithm_commit', 'old'), ('stable_main_commit', 'old'),
                           ('terrain_grid_side', 96), ('gpu_timestamps_available', True)):
            value_report = report()
            value_report[key] = value
            with self.subTest(key=key), self.assertRaises(AssertionError):
                self.check_profile(value_report)


class TerrainPairTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='terrain-gates-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        source, engine = '1' * 40, '3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'
        fixture = {'config/options.json': '8' * 64}
        write(self.root / 'fixture-files.json', fixture)
        self.manifest = dict(schema=1, source_commit=source, source_tree='2' * 40, engine_commit=engine,
            workflow_run='123', host_sha256='3' * 64, current_module_sha256='4' * 64,
            reference_module_sha256='5' * 64, reference_include_sha256=run_terrain_pair.REFERENCE_SHA,
            reference_include_commit=run_terrain_pair.REFERENCE_COMMIT, current_include_sha256='6' * 64,
            reference_kind='current-source-with-pre71-terrain-include', pairs=3, trials=[],
            fixture_files_sha256=digest(self.root / 'fixture-files.json'),
            fixture_digest=hashlib.sha256(json.dumps(fixture, sort_keys=True).encode()).hexdigest(),
            module_restored=True, fixture_restored=True, ordinary_game_performance=False,
            gpu_execution_timestamps_available=False)
        write(self.root / 'bundle.json', dict(schema=1, ncmm_source_commit=source, source_commit=engine,
            host_sha256='3' * 64, files={'code_mods/FirstPersonView/ncmm_mod.dll': '4' * 64}))
        # One synthetic, nonconstant BMP shared by hard links in this owned temp.
        size = 54 + 640 * 480 * 3
        bmp = bytearray(b'BM' + struct.pack('<IHHI', size, 0, 0, 54) +
                        struct.pack('<IiiHHIIiiII', 40, 640, 480, 1, 24, 0, size - 54, 0, 0, 0, 0))
        bmp.extend(bytes((value % 256 for value in range(size - 54))))
        original = self.root / 'contrived.bmp'
        original.write_bytes(bmp)
        for i in range(6):
            pair, order = divmod(i, 2)
            kind = ('reference', 'current')[(pair + order) % 2]
            directory = self.root / f'pair-{pair + 1:02d}-{kind}'
            directory.mkdir()
            value_report = report()
            write(directory / 'first-person-terrain-benchmark.json', value_report)
            write(directory / 'process.json', dict(exit_code=0, pid=i + 1, timeout=False, started_ns=100,
                ended_ns=1000, source_commit=source, engine_commit=engine, host_sha256='3' * 64,
                module_sha256=self.manifest[kind + '_module_sha256'], fixture_digest=self.manifest['fixture_digest']))
            images = {}
            for r in value_report['records']:
                os.link(original, directory / r['capture'])
                images[r['capture']] = digest(directory / r['capture'])
            self.manifest['trials'].append(dict(pair=pair, order=order, kind=kind, directory=directory.name,
                report_sha256=digest(directory / 'first-person-terrain-benchmark.json'),
                process_sha256=digest(directory / 'process.json'), images=images))
        self.persist()

    def persist(self):
        write(self.root / 'pair.json', self.manifest)

    def verify(self):
        with redirect_stdout(io.StringIO()):
            return verify_pair(self.root)

    def test_valid_ablation_without_ordinary_performance_claim(self):
        result = self.verify()
        self.assertTrue(result['success'])
        self.assertFalse(result['ordinary_game_performance'])
        self.assertEqual((result['pairs'], len(result['comparisons'])), (3, 6))

    def test_wrong_source_restoration_order_and_partial_pairs(self):
        original = copy.deepcopy(self.manifest)
        for mutate in (lambda m: m.update(source_commit='7' * 40),
                       lambda m: m.update(host_sha256='7' * 64),
                       lambda m: m.update(module_restored=False),
                       lambda m: m.update(fixture_restored=False),
                       lambda m: m.update(ordinary_game_performance=True),
                       lambda m: m['trials'].pop(),
                       lambda m: m['trials'][0].update(kind='current'),
                       lambda m: m['trials'][0].update(directory='../escape')):
            self.manifest = copy.deepcopy(original)
            mutate(self.manifest)
            self.persist()
            with self.assertRaises(AssertionError):
                self.verify()

    def test_crash_stale_module_and_report_even_with_refreshed_hashes(self):
        trial = self.manifest['trials'][0]
        path = self.root / trial['directory'] / 'process.json'
        original = json.loads(path.read_text())
        for key, value in (('exit_code', 1), ('timeout', True), ('module_sha256', '9' * 64),
                           ('source_commit', '9' * 40), ('fixture_digest', '9' * 64)):
            process = dict(original)
            process[key] = value
            write(path, process)
            trial['process_sha256'] = digest(path)
            self.persist()
            with self.subTest(key=key), self.assertRaises(AssertionError):
                self.verify()

    def test_tampered_image_rejected(self):
        trial = self.manifest['trials'][0]
        name = next(iter(trial['images']))
        path = self.root / trial['directory'] / name
        old = bytearray(path.read_bytes())
        path.unlink()  # Unlink only this owned temporary hard link.
        old[-1] ^= 1
        path.write_bytes(old)
        trial['images'][name] = digest(path)
        self.persist()
        with self.assertRaises(AssertionError):
            self.verify()


class TerrainRunnerGuardTests(unittest.TestCase):
    def test_no_local_launch_even_before_argument_access(self):
        with patch.dict(os.environ, {}, clear=True), patch('subprocess.Popen') as launch:
            with self.assertRaisesRegex(ValueError, 'isolated Windows'):
                run_terrain_pair.run(None)
            launch.assert_not_called()

    def test_traversal_and_equal_root_rejected(self):
        with tempfile.TemporaryDirectory(prefix='terrain-paths-') as temporary:
            root = Path(temporary)
            self.assertEqual(run_terrain_pair.below(root / 'owned', root), root.resolve() / 'owned')
            for path in (root, root / '..' / 'escape'):
                with self.assertRaises(ValueError):
                    run_terrain_pair.below(path, root)


class TerrainChildReceiptTests(unittest.TestCase):
    """Only mocked children; no Native process, window, or input is launched."""
    def exercise(self, behavior):
        with tempfile.TemporaryDirectory(prefix='terrain-child-') as temporary:
            root = Path(temporary)
            process = unittest.mock.Mock(pid=123)
            process.poll.return_value = None
            def wait(timeout):
                if behavior == 'timeout' and timeout == 600:
                    raise subprocess.TimeoutExpired('contrived-owned-child', timeout)
                process.poll.return_value = -9 if behavior == 'timeout' else 0
                return process.poll.return_value
            process.wait.side_effect = wait
            launch_error = OSError('contrived launch failure') if behavior == 'launch' else None
            with patch('run_terrain_pair.require_ci'), patch('subprocess.Popen', return_value=process, side_effect=launch_error):
                if behavior in ('launch', 'timeout'):
                    with self.assertRaises((OSError, subprocess.TimeoutExpired)):
                        run_terrain_pair.run_child(['never-launched'], root, {}, root, {'source_commit': '1' * 40})
                else:
                    run_terrain_pair.run_child(['never-launched'], root, {}, root, {'source_commit': '1' * 40})
            result = json.loads((root / 'process.json').read_text())
            self.assertGreater(result['ended_ns'], result['started_ns'])
            self.assertEqual(result['source_commit'], '1' * 40)
            self.assertEqual(result['state'], {'launch': 'launch-failed', 'timeout': 'timeout', 'success': 'exited'}[behavior])
            if behavior == 'timeout':
                process.kill.assert_called_once()
                self.assertTrue(result['timeout'])
                self.assertEqual(result['exit_code'], -9)
            elif behavior == 'launch':
                self.assertIsNone(result['pid'])
                self.assertEqual(result['error_type'], 'OSError')
            else:
                self.assertEqual(result['exit_code'], 0)
                self.assertIsNone(result['error_type'])

    def test_success_receipt(self): self.exercise('success')
    def test_timeout_receipt_survives_owned_child_cleanup(self): self.exercise('timeout')
    def test_launch_error_receipt_survives(self): self.exercise('launch')


if __name__ == '__main__':
    unittest.main()
