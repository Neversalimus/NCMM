"""Synthetic verifier fixtures only; these are not native gameplay evidence."""
from copy import deepcopy
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
from verify_fire_audit import verify


def png():
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress((b'\0' + b'\x10\x20\x30\xff' * 2) * 2)) + chunk(b'IEND', b'')


class FireEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.report = dict(schema=1, success=True, actual_cdda_sdl=True, manual_playthrough=False,
                           synthetic_attack_notifications=False, normal_game_command_dispatch=True,
                           native_activity_and_target_ui=True, ordinary_ai_turns=False, trials=[], frames=[])
        cases = [('cpu', 'cancel', True)]
        for mode in ['cpu', 'gpu']:
            cases += [(mode, name, False) for name in ['keyboard_single', 'lmb_single', 'cancel', 'empty', 'no_target', 'window']
                      for _ in range(20 if name in {'keyboard_single', 'lmb_single', 'window'} else 10)]
            cases += [(mode, name, False) for name in ['blocked_wall', 'vertical_cancel', 'burst', 'dead_target', 'changed_weapon', 'pointer_only']]
        for i, (mode, name, control) in enumerate(cases):
            shot = not control and name in ['keyboard_single', 'lmb_single', 'window', 'burst', 'changed_weapon', 'pointer_only']
            count = 3 if shot and name == 'burst' else int(shot)
            self.report['trials'].append(dict(renderer=mode, scenario=name, legacy_control=control,
                first_frame=i, last_frame_exclusive=i+1, enabled_before=True, enabled_after=True,
                native_command='SELECT' if name in {'lmb_single', 'pointer_only'} else 'fire', dispatch_calls=1, keyboard_mode='keychar' if i%2 else 'keycode',
                native_keyboard_mode='keychar' if i%2 else 'keycode', default_keychar_inputs=int(bool(i%2)), default_keycode_inputs=int(not i%2),
                yaw_before=0, yaw_after=0, pitch_before=0, pitch_after=0, escape_recovery=False,
                attack_serial_before=0, attack_serial_after=count, ammo_before=7, ammo_after=7-count,
                moves_before=100, moves_after=100-int(shot)))
            file = f'first-person-fire-frame-{i}.png'
            self.report['frames'].append(dict(file=file, width=2, height=2, captured=True, presented=True,
                enabled=True, perspective=not control, gpu=mode == 'gpu'))
            (self.root/file).write_bytes(png())

    def run_report(self):
        (self.root/'first-person-fire-audit.json').write_text(json.dumps(self.report))
        return verify(self.root)

    def test_complete_synthetic_fixture(self):
        self.assertEqual(self.run_report()['normal_native_ui_attempts'], 192)

    def fps_fixture(self):
        self.report['fps_input_router'] = True
        for trial in self.report['trials']:
            trial['native_command'] = 'fire'
            trial['fps_attack_intents'] = int(trial['scenario'] in {'lmb_single', 'pointer_only'})

    def test_complete_fps_synthetic_fixture(self):
        self.fps_fixture()
        self.assertEqual(self.run_report()['normal_native_ui_attempts'], 192)

    def test_fps_click_cannot_bypass_router(self):
        self.fps_fixture()
        trial = next(t for t in self.report['trials'] if t['scenario'] == 'lmb_single')
        trial['fps_attack_intents'] = 0
        with self.assertRaises(AssertionError): self.run_report()

    def test_fps_click_cannot_create_two_intents(self):
        self.fps_fixture()
        trial = next(t for t in self.report['trials'] if t['scenario'] == 'lmb_single')
        trial['fps_attack_intents'] = 2
        with self.assertRaises(AssertionError): self.run_report()

    def test_native_keyboard_cannot_be_reported_as_fps_click(self):
        self.fps_fixture()
        self.report['trials'][0]['fps_attack_intents'] = 1
        with self.assertRaises(AssertionError): self.run_report()

    def test_undispatched_command_rejected(self):
        self.report['trials'][2]['dispatch_calls'] = 0
        with self.assertRaises(AssertionError): self.run_report()

    def test_missing_keyboard_mode_coverage_rejected(self):
        for trial in self.report['trials']:trial['keyboard_mode']='keycode'
        with self.assertRaises(AssertionError):self.run_report()

    def test_wrong_native_command_rejected(self):
        self.report['trials'][2]['native_command'] = 'TIMEOUT'
        with self.assertRaises(AssertionError): self.run_report()

    def test_option_only_mode_coverage_rejected(self):
        trial = next(t for t in self.report['trials'] if t['keyboard_mode'] == 'keycode')
        trial['native_keyboard_mode'] = 'keychar'
        with self.assertRaisesRegex(AssertionError, 'cached keyboard'): self.run_report()

    def test_missing_actual_native_wait_rejected(self):
        self.report['trials'][2]['default_keycode_inputs'] = 0
        with self.assertRaises(AssertionError): self.run_report()

    def test_mixed_native_default_modes_rejected(self):
        self.report['trials'][2]['default_keychar_inputs'] = 1
        with self.assertRaises(AssertionError): self.run_report()

    def test_single_input_cannot_fire_twice(self):
        trial = next(t for t in self.report['trials'] if t['scenario'] == 'keyboard_single')
        trial['attack_serial_after'] = 2; trial['ammo_after'] = 5
        with self.assertRaises(AssertionError): self.run_report()

    def test_confirmation_must_match_native_ammo(self):
        trial = next(t for t in self.report['trials'] if t['scenario'] == 'keyboard_single')
        trial['attack_serial_after'] = 2
        with self.assertRaises(AssertionError): self.run_report()

    def test_intermediate_map_flash_rejected(self):
        self.report['frames'][20]['perspective'] = False
        with self.assertRaises(AssertionError): self.run_report()

    def test_silent_cpu_fallback_rejected(self):
        self.report['frames'][-1]['gpu'] = False
        with self.assertRaises(AssertionError): self.run_report()

    def test_unpresented_readback_rejected(self):
        self.report['frames'][20]['presented'] = False
        with self.assertRaises(AssertionError): self.run_report()

    def test_corrupt_png_rejected(self):
        file = self.root/self.report['frames'][20]['file']
        data = bytearray(file.read_bytes()); data[45] ^= 1; file.write_bytes(data)
        with self.assertRaises(AssertionError): self.run_report()

    def test_missing_attempt_rejected(self):
        self.report['trials'].pop()
        with self.assertRaises(AssertionError): self.run_report()

    def test_uncovered_frame_rejected(self):
        self.report['trials'][20]['last_frame_exclusive'] += 1
        with self.assertRaises(AssertionError): self.run_report()

    def test_control_must_reproduce_regression(self):
        self.report['frames'][0]['perspective'] = True
        with self.assertRaises(AssertionError): self.run_report()


if __name__ == '__main__': unittest.main()
