"""Synthetic verifier fixtures, never actual native HUD evidence."""
import json
from pathlib import Path
import struct
import tempfile
import unittest
from verify_target_audit import verify, PHASES, HIDDEN


class TargetEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.report = dict(schema=1, success=True, actual_cdda_sdl=True, manual_playthrough=False,base_viewport_width=640,base_viewport_height=480,cases=[])
        for mode in ['cpu', 'gpu']:
            for phase in sorted(PHASES):
                width,height=(1280,720) if phase=='wide' else (640,480)
                pixels=b'\0'*(width*height*4)
                bmp=b'BM'+struct.pack('<IHHI',54+len(pixels),0,0,54)+struct.pack('<IiiHHIIiiII',40,width,height,1,32,0,len(pixels),0,0,0,0)+pixels
                selected = phase not in HIDDEN; hostile = selected and phase != 'friendly-name-only'
                hp = 100 if phase in {'full', 'replaced-same-cell', 'glass'} else 50
                entity = 2 if phase == 'replaced-same-cell' else 3 if phase in {'glass', 'friendly-name-only'} else 1
                file = f'first-person-audit-target-hud-{phase}-{mode}.bmp'
                self.report['cases'].append(dict(renderer=mode, phase=phase, file=file, selected=selected, hostile=hostile,
                    known=hostile, fraction=hp/100 if hostile else 0, hp=hp if hostile else -1, maximum=100 if hostile else -1,
                    entity=entity if selected else 0, world=7 if selected else 0, frame=1 if selected else 0, gpu=mode == 'gpu',
                    viewport_width=width,viewport_height=height,window_display_scale=1,device='Test device',backend='direct3d12',software_device=True))
                (self.root/file).write_bytes(bmp)

    def run_report(self):
        (self.root/'first-person-target-audit.json').write_text(json.dumps(self.report))
        return verify(self.root)

    def case(self, phase):
        return next(c for c in self.report['cases'] if c['phase'] == phase)

    def test_complete_synthetic_fixture(self):
        self.assertEqual(self.run_report()['native_cases'], 42)

    def test_replacement_cannot_reuse_identity(self):
        self.case('replaced-same-cell')['entity'] = self.case('full')['entity']
        with self.assertRaises(AssertionError): self.run_report()

    def test_health_must_match_native_pair(self):
        self.case('damaged')['fraction'] = 1
        with self.assertRaises(AssertionError): self.run_report()

    def test_wall_cannot_reveal_target(self):
        self.case('opaque-wall')['selected'] = True
        with self.assertRaises(AssertionError): self.run_report()

    def test_unknown_health_cannot_appear_full(self):
        self.case('lost-aim')['fraction'] = 1
        with self.assertRaises(AssertionError): self.run_report()

    def test_no_hidden_fallback(self):
        next(c for c in self.report['cases'] if c['renderer'] == 'gpu')['gpu'] = False
        with self.assertRaises(AssertionError): self.run_report()

    def test_truncated_capture(self):
        file = self.root/self.case('full')['file']; file.write_bytes(file.read_bytes()[:-1])
        with self.assertRaises(AssertionError): self.run_report()


if __name__ == '__main__': unittest.main()
