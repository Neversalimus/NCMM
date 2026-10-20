"""Synthetic pixel fixtures only; never native gameplay evidence."""
from pathlib import Path
import tempfile
import unittest
from PIL import Image, ImageDraw
from verify_fire_pixels import verify_spatial

class FirePixelTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup); self.root = Path(self.temp.name)
        self.trials = [dict(legacy_control=True, first_frame=0, last_frame_exclusive=2, scenario='cancel', renderer='cpu'),
                       dict(legacy_control=False, first_frame=2, last_frame_exclusive=4, scenario='keyboard_single', renderer='cpu')]
        self.frames = [dict(perspective=index!=1) for index in range(4)]
        for index in range(4): self.paint(index, index!=1)
    def paint(self, index, perspective):
        Image.new('RGB', (200, 100), (135, 180, 215) if perspective else (110, 80, 50)).save(self.root/f'first-person-fire-frame-{index}.png')
    def test_paired_fixture(self):
        result = verify_spatial(self.root, self.trials, self.frames)
        self.assertEqual(result['legacy_visible_map_frame'], 1); self.assertEqual(result['frames_checked'], 2)
    def test_true_flag_cannot_hide_map_pixels(self):
        self.paint(3, False)
        with self.assertRaisesRegex(AssertionError, 'frame 3'): verify_spatial(self.root, self.trials, self.frames)
    def test_legacy_false_flag_does_not_prove_visible_regression(self):
        self.paint(1, True)
        with self.assertRaisesRegex(AssertionError, 'actual pixels'): verify_spatial(self.root, self.trials, self.frames)
    def test_blank_window_rejected(self):
        Image.new('RGB', (200, 100)).save(self.root/'first-person-fire-frame-3.png')
        with self.assertRaises(AssertionError): verify_spatial(self.root, self.trials, self.frames)
    def test_map_cannot_be_its_own_perspective_reference(self):
        self.paint(2, False); self.paint(3, False)
        with self.assertRaisesRegex(AssertionError, 'Trial reference'): verify_spatial(self.root, self.trials, self.frames)
    def test_missing_capture_rejected(self):
        (self.root/'first-person-fire-frame-3.png').unlink()
        with self.assertRaises(FileNotFoundError): verify_spatial(self.root, self.trials, self.frames)
    def window_reference(self):
        # Same observed grey foreground as the Native CI window fixture; its
        # colour is close to the control's floor map, while the sky remains a
        # valid discriminator on both sides. These are synthetic unit pixels.
        for index in (2,3):
            with Image.open(self.root/f'first-person-fire-frame-{index}.png') as image:
                ImageDraw.Draw(image).rectangle((33,0,98,99),fill=(95,94,85))
                image.save(self.root/f'first-person-fire-frame-{index}.png')
    def test_foreground_window_uses_its_own_discriminating_world_samples(self):
        self.window_reference()
        result=verify_spatial(self.root,self.trials,self.frames)
        record=result['records'][0]
        self.assertGreaterEqual(len(record['sample_coordinates']),12)
        self.assertEqual(record['minimum_reference_match'],1)
    def test_foreground_window_does_not_hide_a_subsequent_actual_map(self):
        self.window_reference();self.paint(3,False)
        with self.assertRaisesRegex(AssertionError,'frame 3'):verify_spatial(self.root,self.trials,self.frames)

    def target_panel(self):
        with Image.open(self.root/'first-person-fire-frame-3.png') as image:
            ImageDraw.Draw(image).rectangle((64,0,199,99),fill=(0,0,0))
            image.save(self.root/'first-person-fire-frame-3.png')
    def test_native_target_panel_over_foreground_window_preserves_world(self):
        self.window_reference();self.target_panel()
        result=verify_spatial(self.root,self.trials,self.frames)
        self.assertEqual(result['records'][0]['minimum_reference_match'],1)
        self.assertTrue(all(x<=.30 for x,_ in result['records'][0]['sample_coordinates']))
    def test_actual_map_under_native_target_panel_still_fails(self):
        self.window_reference();self.paint(3,False);self.target_panel()
        with self.assertRaisesRegex(AssertionError,'frame 3'):verify_spatial(self.root,self.trials,self.frames)
    def test_blank_world_under_native_target_panel_still_fails(self):
        self.window_reference()
        Image.new('RGB',(200,100)).save(self.root/'first-person-fire-frame-3.png')
        self.target_panel()
        with self.assertRaisesRegex(AssertionError,'frame 3'):verify_spatial(self.root,self.trials,self.frames)

if __name__ == '__main__': unittest.main()
