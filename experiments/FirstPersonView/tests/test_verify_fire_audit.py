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
            cases += [(mode, name, False) for name in ['keyboard_single', 'lmb_single', 'cancel', 'empty', 'no_target', 'window'] for _ in range(10)]
            cases += [(mode, name, False) for name in ['blocked_wall', 'vertical_cancel', 'burst']]
        for i, (mode, name, control) in enumerate(cases):
            shot = not control and name in ['keyboard_single', 'lmb_single', 'window', 'burst']
            self.report['trials'].append(dict(renderer=mode, scenario=name, legacy_control=control,
                first_frame=i, last_frame_exclusive=i+1, enabled_before=True, enabled_after=True,
                yaw_before=0, yaw_after=0, pitch_before=0, pitch_after=0, escape_recovery=False,
                attack_serial_before=0, attack_serial_after=int(shot), ammo_before=7, ammo_after=7-int(shot),
                moves_before=100, moves_after=100-int(shot)))
            file = f'first-person-fire-frame-{i}.png'
            self.report['frames'].append(dict(file=file, width=2, height=2, captured=True, presented=True,
                enabled=True, perspective=not control, gpu=mode == 'gpu'))
            (self.root/file).write_bytes(png())

    def run_report(self):
        (self.root/'first-person-fire-audit.json').write_text(json.dumps(self.report))
        return verify(self.root)

    def test_complete_synthetic_fixture(self):
        self.assertEqual(self.run_report()['normal_native_ui_attempts'], 126)

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
