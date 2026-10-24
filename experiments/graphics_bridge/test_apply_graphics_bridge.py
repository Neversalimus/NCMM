import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import apply_graphics_bridge as bridge
from native_compiler_bridge import BEFORE as NATIVE_TOGGLE

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
        (root / "src/game.cpp").write_text('#include "game.h"\n'+bridge.REBASE_CACHE_RESET+'\n')
        (root / "src/vehicle_use.cpp").write_text(NATIVE_TOGGLE)
        (root / "src/debug.cpp").write_text('\nstd::string ncmm_debug_report_text( const std::string &text )\n{ return text; }\n')
        options='static void ncmm_apply_deferred_option_value( const std::string &, options_manager::cOpt & );\n// NCMM v8.7.6.6 stale world-option repair:\n'
        for kind in ['bool','int','float','enum']:
            options+='bool options_manager::ncmm_register_world_'+kind+'()\n{\n    if( missing ) {\n        return true;\n    }\n    cOpt &opt = it->second;\n    if( world_options.has_value() ) { }\n    return true;\n}\n'
        options+='\nvoid options_manager::update_global_locale() { }\n'
        (root/'src/options.cpp').write_text(options)
        (root / "src/creature.h").write_text('#include <array>\nclass Creature {\n    public:\n        ~Creature() override;\n};\n')
        (root / "src/overmapbuffer.h").write_text("        bool has( const point_abs_om &p );\n")
        (root / "src/map.h").write_text("        int supports_zlevels() const {\n            return zlevels;\n        }\n")
        map_cpp='#include "map.h"\n'
        for signature in ("void map::set_transparency_cache_dirty( const int zlev )",
                          "void map::set_transparency_cache_dirty( const tripoint_bub_ms &p, bool field )",
                          "void map::set_outside_cache_dirty( const int zlev )",
                          "void map::set_floor_cache_dirty( const int zlev )"):
            map_cpp+=signature+"\n{\n}\n"
        (root / "src/map.cpp").write_text(map_cpp+bridge.SUBMAP_CACHE_RESET+'\n    current_submap->set_ter( l, new_terrain );\n    current_submap->set_furn( l, new_target_furniture );\n'+'void map::on_field_modified( const tripoint_bub_ms &p, const field_type &fd_type )\n{\n}\n')
        (root / "src/input_context.cpp").write_text('#include "input_context.h"\nconst std::string &input_context::handle_input( const int timeout )\n{\n        next_action = inp_mngr.get_input_event( preferred_keyboard_mode );\n        if( next_action.type == input_event_t::timeout ) {\n        }\n    inp_mngr.set_timeout( old_timeout );\n    return *result;\n}\n')
        (root / "src/sdltiles.cpp").write_text(SDL_ANCHOR+'\nstatic void CheckMessages()\n{\n        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );\n    if( needupdate ) {\n        try_sdl_update();\n    }\n}\n')
        (root / "src/sdl_wrappers.cpp").write_text('#include "sdl_wrappers.h"\nvoid RenderPresent() {\n    printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" );\n}\n')
        with (root / "src/sdltiles.cpp").open('a') as f:
            f.write('\n'.join('void teardown_'+str(i)+'() {\n    renderer.reset();\n}' for i in range(4))+'\n')
        (root / "src/main_menu.cpp").write_text("        std::exit( ncmm::run_gameplay_smoke() );")
        (root / "src/sdltiles.h").write_text("SDL_Window *get_sdl_window();")
        (root / "src/handle_action.cpp").write_text("    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }")
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n    if( act == ACTION_NULL ) {\n        act = look_up_action( action );\n    }')
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {}\ninput_context game::get_player_input( std::string &action )\n{\n    return ctxt;\n}\n\nstatic void rcdrive( ) {}')
        with (root / "src/handle_action.cpp").open('a') as f:f.write('\n        if( act == ACTION_INTERACT ) {}\nplayer_character.assign_activity( open_tile_activity_actor( std::nullopt ) );\n')
        (root / "src/melee.cpp").write_text('    const bool hits = hit_spread >= 0;')
        (root / "src/ranged.cpp").write_text('target_handler::trajectory target_ui::run()\n{\n}\n        curshot++;\n\n        if( !gun.is_gun() ) {}\n    bool skip_redraw = false;')
        (root / "src/creature.cpp").write_text("int Creature::deal_melee_attack( Creature *source, int hitroll )\n{\n}\n    apply_damage( source, bp, total_damage );")
        (root / "src/ballistics.cpp").write_text('#include "ballistics.h"\n    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );')
        (root / "src/monster.cpp").write_text("    // We were carrying a creature, deposit the rider")
        (root / "src/npc.cpp").write_text("    prevent_death_reminder = false;\n    dialogue d( get_talker_for( this ),")
        (root / "src/cata_tiles.h").write_text("        std::optional<texture_draw_data> get_texture_draw_data( const std::string &id,\n                TILE_CATEGORY category, const tripoint_bub_ms &p );")
        (root / "src/tileset_loader.h").write_text("                                      const atlas_replay_quarantine::gate &abandon_gate );\n        void create_textures_from_tile_atlas")
        (root / "src/tileset_loader.cpp").write_text('#include "tileset_loader.h"\n'
            '        const std::vector<SDL_Rect> &opaque_bounds,\n        const atlas_replay_quarantine::gate &abandon_gate )\n'
            '    cata_assert( texture_ptr );\n    ++texture_count;\n'
            '            copy_surface_to_texture( scan_surf, offset, *tile_values, opaque_bounds, abandon_gate );\n')
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
    def test_changed_submap_cache_reset_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/map.cpp';target.write_text(target.read_text().replace('set_pathfinding_cache_dirty( z );','changed_native_cache_reset();'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_rebase_cache_reset_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            (root/'src/game.cpp').write_text('#include "game.h"\nchanged_native_cache_reset();')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_missing_field_notification_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/map.cpp';target.write_text(target.read_text().replace('void map::on_field_modified(', 'void changed_field_notification('))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_missing_late_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/sdltiles.cpp").write_text("changed renderer")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_input_return_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            path=root/'src/input_context.cpp'
            path.write_text(path.read_text().replace('return *result;', 'return modified_result;'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_success_and_reapply_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
                self.assertTrue((root / "first-person-source.json").is_file())
                self.assertIn("draw_graphics_view", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_requires_terrain_pass", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("ncmm_native_pixels_ready = true;", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn("&& !ncmm_native_pixels_ready", (root / "src/sdltiles.cpp").read_text(encoding="utf-8"))
                self.assertIn(SDL_ANCHOR, (root / "src/sdltiles.cpp").read_text(encoding="utf-8")) # Full native fallback remains available.
                self.assertIn('if( !printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" ) )', (root / "src/sdl_wrappers.cpp").read_text(encoding="utf-8"))
                self.assertEqual((root / "src/sdl_wrappers.cpp").read_text(encoding="utf-8").count("ncmm::graphics_frame_presented();"), 1)
                self.assertIn("graphics_erase( module_id )", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_context.note( category );", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_targeting_scope ncmm_aiming", (root / "src/ranged.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_input_wait( category, input_manager::actual_keyboard_mode( preferred_keyboard_mode ) == keyboard_mode::keychar )", (root / "src/input_context.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_input_wait( const std::string &, bool text )", (root / "src/ncmm_loader.h").read_text(encoding="utf-8"))
                self.assertTrue((root / "src/ncmm_graphics_context.hpp").is_file())
                self.assertNotIn('const std::string aimed = ncmm::graphics_click_action();',
                                 (root / 'src/handle_action.cpp').read_text())
                self.assertTrue((root / "src/ncmm_graphics_snapshot_cache.hpp").is_file())
                self.assertIn('ncmm_wait.completed( *result );', (root / "src/input_context.cpp").read_text())
                self.assertIn('graphics_snapshot_external_callback( module_id );', (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn('#include "ncmm_graphics_scratch.hpp"', (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertEqual((root / "src/ncmm_graphics_scratch.hpp").read_bytes(), Path(bridge.__file__).with_name("ncmm_graphics_scratch.hpp").read_bytes())
                self.assertEqual((root / "src/ncmm_native_atlas_pixels.hpp").read_bytes(), Path(bridge.__file__).with_name("ncmm_native_atlas_pixels.hpp").read_bytes())
                self.assertIn("tile_values == targets.normal", (root / "src/tileset_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics.creatures.v1", (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_creature( this, 4 )", (root / "src/monster.cpp").read_text(encoding="utf-8"))
                self.assertIn("graphics_note_creature( origin, 2", (root / "src/ballistics.cpp").read_text(encoding="utf-8"))
                tiles = (root / "src/cata_tiles.cpp").read_text(encoding="utf-8")
                self.assertEqual(tiles.count("if( stable && ttype.animated ) seed = 0;"), 1)
                self.assertIn("cata_tiles::get_texture_draw_data", tiles)
                self.assertNotIn("ncmm_perspective_prepass", tiles)
                before = self.fingerprint(root)
                with self.assertRaises(ValueError): bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_changed_native_atlas_upload_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            (root/'src/tileset_loader.cpp').write_text('changed Native upload')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'Native atlas anchor differs'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_ambiguous_native_atlas_upload_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            p=root/'src/tileset_loader.cpp';p.write_text(p.read_text()*2)
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'Native atlas anchor differs'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_changed_native_compiler_predicate_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            (root/'src/vehicle_use.cpp').write_text('changed upstream predicate')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'electronic-toggle predicate changed'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))

    def test_missing_shutdown_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/main_menu.cpp").write_text("changed smoke entry")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "one exact anchor"): bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_changed_native_animation_input_loop_no_partial_patch(self):
        for anchor in ['input_context game::get_player_input(', 'static void rcdrive(']:
            with self.subTest(anchor=anchor), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);self.fixture(root);path=root/'src/handle_action.cpp'
                path.write_text(path.read_text().replace(anchor,'changed_native_input_loop('))
                before=self.fingerprint(root)
                with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                    with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
                self.assertEqual(before,self.fingerprint(root))
    def test_changed_native_debug_helper_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);(root/'src/debug.cpp').write_text('changed Native report helper')
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_deferred_setting_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/options.cpp'
            path.write_text(path.read_text().replace('ncmm_register_world_enum(', 'changed_native_registration('))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'four exact Native'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_creature_hook_no_partial_patch(self):
        for name in ("creature.h", "creature.cpp", "ballistics.cpp", "monster.cpp", "npc.cpp"):
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

    def test_changed_fps_native_input_anchors_no_partial_patch(self):
        for name,anchor in [
            ('input_context.cpp','        if( next_action.type == input_event_t::timeout ) {'),
            ('handle_action.cpp','        if( act == ACTION_INTERACT ) {'),
            ('handle_action.cpp','player_character.assign_activity( open_tile_activity_actor( std::nullopt ) );'),
            ('sdltiles.cpp','    if( needupdate ) {\n        try_sdl_update();')]:
            for ambiguous in [False,True]:
                with self.subTest(name=name,anchor=anchor,ambiguous=ambiguous), tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp);self.fixture(root);path=root/'src'/name
                    source=path.read_text();self.assertEqual(source.count(anchor),1)
                    path.write_text(source.replace(anchor,anchor*2 if ambiguous else 'changed_fps_anchor'))
                    before=self.fingerprint(root)
                    with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                        with self.assertRaisesRegex(ValueError,'FPS input requires one exact anchor'):bridge.apply(root)
                    self.assertEqual(before,self.fingerprint(root))

    def test_native_sprite_path_is_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);path=root/'src/cata_tiles.cpp'
            original='    const texture *sprite_tex = tileset_ptr->get_tile( sprite_index );'
            self.assertEqual(path.read_text().count(original),1)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
            self.assertEqual(path.read_text().count(original),1)
            self.assertNotIn('ncmm_perspective_prepass',path.read_text())

if __name__ == "__main__": unittest.main()
