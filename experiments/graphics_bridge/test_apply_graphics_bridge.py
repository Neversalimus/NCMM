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
        (root / "src/game.h").write_text("        bool save();\n")
        (root / "src/overmapbuffer.h").write_text("        bool has( const point_abs_om &p );\n")
        (root / "src/map.h").write_text("        int supports_zlevels() const {\n            return zlevels;\n        }\n")
        map_cpp='#include "map.h"\n'
        for signature in ("void map::set_transparency_cache_dirty( const int zlev )",
                          "void map::set_transparency_cache_dirty( const tripoint_bub_ms &p, bool field )",
                          "void map::set_outside_cache_dirty( const int zlev )",
                          "void map::set_floor_cache_dirty( const int zlev )"):
            map_cpp+=signature+"\n{\n}\n"
        (root / "src/map.cpp").write_text(map_cpp)
        (root / "src/input_context.cpp").write_text('#include "input_context.h"\nconst std::string &input_context::handle_input( const int timeout )\n{\n}\n')
        (root / "src/sdltiles.cpp").write_text(SDL_ANCHOR+'\nstatic void CheckMessages()\n{\n        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );\n}\n')
        (root / "src/sdl_wrappers.cpp").write_text('#include "sdl_wrappers.h"\nvoid RenderPresent() {\n    printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" );\n}\n')
        with (root / "src/sdltiles.cpp").open('a') as f:
            f.write('\n'.join('void teardown_'+str(i)+'() {\n    renderer.reset();\n}' for i in range(4))+'\n')
        (root / "src/main_menu.cpp").write_text("        std::exit( ncmm::run_gameplay_smoke() );")
        (root / "src/sdltiles.h").write_text("SDL_Window *get_sdl_window();")
        (root / "src/handle_action.cpp").write_text("    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }")
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n    if( act == ACTION_NULL ) {\n        act = look_up_action( action );\n    }')
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {}')
        (root / "src/melee.cpp").write_text('    const bool hits = hit_spread >= 0;')
        (root / "src/ranged.cpp").write_text('        curshot++;\n\n        if( !gun.is_gun() ) {}\n    bool skip_redraw = false;')
        (root / "src/creature.cpp").write_text("int Creature::deal_melee_attack( Creature *source, int hitroll )\n{\n}\n    apply_damage( source, bp, total_damage );")
        (root / "src/ballistics.cpp").write_text('#include "ballistics.h"\n    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );')
        (root / "src/monster.cpp").write_text("    // We were carrying a creature, deposit the rider")
        (root / "src/npc.cpp").write_text("    prevent_death_reminder = false;\n    dialogue d( get_talker_for( this ),")
        (root / "src/cata_tiles.h").write_text("        std::optional<texture_draw_data> get_texture_draw_data( const std::string &id,\n                TILE_CATEGORY category, const tripoint_bub_ms &p );")
        (root / "src/cata_tiles.cpp").write_text("""void cata_tiles::load_tileset( const std::string &tileset_id, const bool precheck,
                               const bool force, const bool pump_events, const bool terrain )
{
}
std::optional<texture_draw_data> cata_tiles::get_texture_draw_data( const std::string &id,
        TILE_CATEGORY category, const tripoint_bub_ms &p )
{
    const std::vector<int> *indices = ttype.fg.pick( seed );
}
std::unordered_set<std::string> cata_tiles::get_all_portrait_tile_ids() {}
bool cata_tiles::draw_sprite_at() {
    const texture *sprite_tex = tileset_ptr->get_tile( sprite_index );
    height_3d += tile.height_3d;
    return true;
}
""")
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
                self.assertIn("tilecontext->ncmm_prepare_perspective(", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn(SDL_ANCHOR, (root / "src/sdltiles.cpp").read_text(encoding="utf-8")) # Full native fallback remains available.
                self.assertIn('if( !printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" ) )', (root / "src/sdl_wrappers.cpp").read_text(encoding="utf-8"))
                self.assertEqual((root / "src/sdl_wrappers.cpp").read_text(encoding="utf-8").count("ncmm::graphics_frame_presented();"), 1)
                self.assertIn("graphics_erase( module_id )", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics.creatures.v1", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_creature( this, 4 )", (root / "src/monster.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_creature( origin, 2", (root / "src/ballistics.cpp").read_text(encoding="utf-8"))
                tiles = (root / "src/cata_tiles.cpp").read_text(encoding="utf-8")
                self.assertEqual(tiles.count("if( stable && ttype.animated ) seed = 0;"), 1)
                self.assertIn("cata_tiles::get_texture_draw_data", tiles)
                self.assertIn("~restore_flag() { flag = previous; }", tiles)
                self.assertEqual(tiles.count("if( ncmm_perspective_prepass )"),1)
                before = self.fingerprint(root)
                with self.assertRaises(ValueError): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_missing_shutdown_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/main_menu.cpp").write_text("changed smoke entry")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_creature_hook_no_partial_patch(self):
        for name in ("creature.cpp", "ballistics.cpp", "monster.cpp", "npc.cpp"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                (root / "src" / name).write_text("changed creature event pathway")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                    with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_changed_atlas_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            path = root / "src/cata_tiles.cpp"
            path.write_text(path.read_text().replace("ttype.fg.pick( seed )", "ttype.fg.pick( different_seed )"))
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_renderer_teardown_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/sdltiles.cpp'
            path.write_text(path.read_text().replace('renderer.reset();','renderer.changed_reset();',1));before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'teardown'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_present_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/sdl_wrappers.cpp'
            path.write_text(path.read_text().replace('SDL_RenderPresent( renderer.get() )','changed_present( renderer.get() )'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_changed_native_sprite_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/cata_tiles.cpp'
            path.write_text(path.read_text().replace('tileset_ptr->get_tile( sprite_index )','changed_native_sprite_lookup()'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

if __name__ == "__main__": unittest.main()
