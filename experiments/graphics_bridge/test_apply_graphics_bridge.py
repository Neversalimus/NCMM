import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import apply_graphics_bridge as bridge

REPO = Path(__file__).resolve().parents[2]
SDL_ANCHOR = """        tilecontext->draw(
            point( win->pos.x * fontwidth, win->pos.y * fontheight ),
            g->ter_view_p,
            TERRAIN_WINDOW_TERM_WIDTH * font->width,
            TERRAIN_WINDOW_TERM_HEIGHT * font->height,
            overlay_strings,
            color_blocks );"""

class ApplyTests(unittest.TestCase):
    def fixture(self, root):
        (root / "src").mkdir()
        for name in ("ncmm_loader.cpp", "ncmm_loader.h"):
            (root / "src" / name).write_bytes((REPO / "host_patch" / name).read_bytes())
        (root / "src/input_context.cpp").write_text('#include "input_context.h"\nconst std::string &input_context::handle_input( const int timeout )\n{\n}\n')
        (root / "src/sdltiles.cpp").write_text(SDL_ANCHOR)
        (root / "src/handle_action.cpp").write_text("    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }")
    def fingerprint(self, root):
        return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob("*") if p.is_file()}
    def test_unknown_identity_no_writes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root); before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value="a" * 40):
                with self.assertRaisesRegex(ValueError, "Unqualified"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_missing_late_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/sdltiles.cpp").write_text("changed renderer")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_success_and_reapply_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
                self.assertTrue((root / "first-person-source.json").is_file())
                self.assertIn("draw_graphics_view", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_requires_terrain_pass", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_erase( module_id )", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                before = self.fingerprint(root)
                with self.assertRaises(ValueError): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))

if __name__ == "__main__": unittest.main()
