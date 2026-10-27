import hashlib
import json
import re
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

APPARENT_LIGHT_DECLARATIONS = """        struct apparent_light_info {
            bool obstructed;
            bool abs_obstructed;
            float apparent_light;
        };
        static apparent_light_info apparent_light_helper( const level_cache &map_cache,
                const tripoint_bub_ms &p );
        lit_level apparent_light_at( const tripoint_bub_ms &p,
                                    const visibility_variables &cache ) const;
"""


def apparent_light_original():
    # The primary-source oracle stays frozen when the extracted implementation
    # changes. Do not derive an upstream patch fixture from the changed inc.
    frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_cache_test.cpp").read_text(encoding="utf-8")
    begin = "// BEGIN FROZEN NATIVE APPARENT LIGHT\n"
    end = "// END FROZEN NATIVE APPARENT LIGHT\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native apparent-light fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]


def native_update_map_original():
    frozen = (REPO / "experiments/FirstPersonView/tests/trace_platform_test.cpp").read_text(encoding="utf-8")
    begin, end = "// BEGIN FROZEN NATIVE UPDATE MAP\n", "// END FROZEN NATIVE UPDATE MAP\n"
    if frozen.count(begin) != 1 or frozen.count(end) != 1:
        raise ValueError("Frozen Native update_map fixture differs")
    return frozen.split(begin, 1)[1].split(end, 1)[0]

class ApplyTests(unittest.TestCase):
    def test_update_map_primary_and_source_preserving_markers(self):
        original = native_update_map_original()
        self.assertEqual(len(original.encode()), 3205)
        self.assertEqual(hashlib.sha256(original.encode()).hexdigest(),
                         'bfb01cf4e07c99f1b41ebd460c81531fb8ff33e0c9800e58cf13e30201253a4b')
        source = Path(bridge.__file__).with_name('ncmm_graphics_update_map.inc').read_text(encoding='utf-8')
        source = source[source.index('point_rel_sm game::update_map'):]
        stripped = ''.join(line for line in source.splitlines(keepends=True) if 'ncmm_move_scope' not in line)
        self.assertEqual(stripped.replace(bridge.NATIVE_UPDATE_MAP_RESET_SCOPE, '', 1), original)
        self.assertEqual(stripped.count(bridge.NATIVE_UPDATE_MAP_RESET_SCOPE), 1)

    def test_update_map_both_pins_receipts_and_global_tls_include(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                with patch.object(bridge.subprocess, 'check_output', return_value=pin): bridge.apply(root)
                receipt = json.loads((root/'first-person-source.json').read_text(encoding='utf-8'))
                for name in ('ncmm_graphics_move_diagnostics.hpp', 'ncmm_graphics_update_map.inc'):
                    content = Path(bridge.__file__).with_name(name).read_bytes()
                    self.assertEqual((root/'src'/name).read_bytes(), content)
                    self.assertEqual(receipt['inputs'][name], hashlib.sha256(content).hexdigest())
                for name, anchor in (('game.cpp', '#include "game.h"'), ('ncmm_loader.cpp', '#include "ncmm_loader.h"')):
                    source = (root/'src'/name).read_text(encoding='utf-8')
                    self.assertEqual(source.count('#include "ncmm_graphics_move_diagnostics.hpp"'), 1)
                    self.assertLess(source.index(anchor), source.index('#include "ncmm_graphics_move_diagnostics.hpp"'))
                    namespace = re.search(r'namespace\s*\{', source)
                    if namespace: self.assertLess(source.index('#include "ncmm_graphics_move_diagnostics.hpp"'), namespace.start())
                source = (root/'src/game.cpp').read_text(encoding='utf-8')
                self.assertEqual(source.count('#include "ncmm_graphics_update_map.inc"'), 1)
                self.assertIn('// Native continuation retained.', source)

    def test_update_map_changed_and_duplicate_primary_are_atomic(self):
        for pin in bridge.SOURCES:
            for duplicate in (False, True):
                with self.subTest(pin=pin, duplicate=duplicate), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root); game = root/'src/game.cpp'
                    text = game.read_text(encoding='utf-8')
                    game.write_text(text+native_update_map_original() if duplicate else text.replace('load_npcs();', 'different_load_npcs();'), encoding='utf-8')
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, 'check_output', return_value=pin):
                        with self.assertRaisesRegex(ValueError, 'update_map|cache.reset'): bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))

    def test_update_map_missing_or_existing_include_is_atomic(self):
        for pin in bridge.SOURCES:
            for name in ('ncmm_graphics_move_diagnostics.hpp', 'ncmm_graphics_update_map.inc'):
                for existing in (False, True):
                    with self.subTest(pin=pin, name=name, existing=existing), tempfile.TemporaryDirectory() as tmp:
                        root = Path(tmp); self.fixture(root)
                        if existing: (root/'src'/name).write_text('existing file\n')
                        before = self.fingerprint(root); original_read = Path.read_text
                        def read(path, *args, **kwargs):
                            if not existing and path == Path(bridge.__file__).with_name(name): raise FileNotFoundError('missing diagnostic include')
                            return original_read(path, *args, **kwargs)
                        with patch.object(bridge.subprocess, 'check_output', return_value=pin), patch.object(Path, 'read_text', read):
                            with self.assertRaises((ValueError, FileNotFoundError)): bridge.apply(root)
                        self.assertEqual(before, self.fingerprint(root))

    def fixture(self, root):
        (root / "src").mkdir()
        for name in ("ncmm_loader.cpp", "ncmm_loader.h"):
            (root / "src" / name).write_bytes((REPO / "host_patch" / name).read_bytes())
        (root / "src/game.h").write_text("        bool save();\n")
        (root / "src/game.cpp").write_text('#include "game.h"\n'+native_update_map_original()+'\n// Native continuation retained.\n')
        (root / "src/vehicle_use.cpp").write_text(NATIVE_TOGGLE)
        (root / "src/debug.cpp").write_text('#include "debug.h"\nstd::string ncmm_debug_report_text( const std::string &text )\n{ return text; }\nvoid native_debug_window() {\n    for( bool stop = false; !stop; ) {\n    }\n}\n')
        options='static void ncmm_apply_deferred_option_value( const std::string &, options_manager::cOpt & );\n// NCMM v8.7.6.6 stale world-option repair:\n'
        for kind in ['bool','int','float','enum']:
            options+='bool options_manager::ncmm_register_world_'+kind+'()\n{\n    if( missing ) {\n        return true;\n    }\n    cOpt &opt = it->second;\n    if( world_options.has_value() ) { }\n    return true;\n}\n'
        options+='\nvoid options_manager::update_global_locale() { }\n'
        (root/'src/options.cpp').write_text(options)
        (root / "src/creature.h").write_text('#include <array>\nclass Creature {\n    public:\n        ~Creature() override;\n};\n')
        (root / "src/overmapbuffer.h").write_text("        bool has( const point_abs_om &p );\n")
        (root / "src/map.h").write_text("        int supports_zlevels() const {\n            return zlevels;\n        }\n" + APPARENT_LIGHT_DECLARATIONS)
        (root / "src/lightmap.cpp").write_text('#include "map.h"\n' + apparent_light_original() + '\n// Native continuation retained.\n')
        map_cpp='#include "map.h"\n'
        for signature in ("void map::set_transparency_cache_dirty( const int zlev )",
                          "void map::set_transparency_cache_dirty( const tripoint_bub_ms &p, bool field )",
                          "void map::set_outside_cache_dirty( const int zlev )",
                          "void map::set_floor_cache_dirty( const int zlev )"):
            map_cpp+=signature+"\n{\n}\n"
        (root / "src/map.cpp").write_text(map_cpp+bridge.SUBMAP_CACHE_RESET+'\n    current_submap->set_ter( l, new_terrain );\n    current_submap->set_furn( l, new_target_furniture );\n'+'void map::on_field_modified( const tripoint_bub_ms &p, const field_type &fd_type )\n{\n}\n')
        (root / "src/input_context.cpp").write_text('#include "input_context.h"\nconst std::string &input_context::handle_input( const int timeout )\n{\n        next_action = inp_mngr.get_input_event( preferred_keyboard_mode );\n        if( next_action.type == input_event_t::timeout ) {\n        }\n    inp_mngr.set_timeout( old_timeout );\n    return *result;\n}\n')
        (root / "src/input.cpp").write_text('#include "input.h"\nvoid input_manager::wait_for_any_key()\n{\n}\n')
        (root / "src/sdltiles.cpp").write_text(SDL_ANCHOR+'\nstatic void CheckMessages()\n{\n        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );\n    if( needupdate ) {\n        try_sdl_update();\n    }\n}\n')
        (root / "src/sdl_wrappers.cpp").write_text('#include "sdl_wrappers.h"\nvoid RenderPresent() {\n    printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" );\n}\n')
        with (root / "src/sdltiles.cpp").open('a') as f:
            f.write('void refresh_mouse_config() {\n    ImGuiIO &io = ImGui::GetIO();\n    if( !mouse.enabled ) {}\n}\n')
            f.write('\n'.join('void teardown_'+str(i)+'() {\n    renderer.reset();\n}' for i in range(4))+'\n')
            f.write('    } else if( ui_adaptor::has_imgui() ) {\n        try_sdl_update();\n    }\n')
        (root / "src/main_menu.cpp").write_text("        std::exit( ncmm::run_gameplay_smoke() );")
        (root / "src/sdltiles.h").write_text("SDL_Window *get_sdl_window();")
        (root / "src/handle_action.cpp").write_text("    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }")
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n    if( act == ACTION_NULL ) {\n        act = look_up_action( action );\n    }')
        with (root / "src/handle_action.cpp").open('a') as f: f.write('\n        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {}\ninput_context game::get_player_input( std::string &action )\n{\n    return ctxt;\n}\n\nstatic void rcdrive( ) {}')
        with (root / "src/handle_action.cpp").open('a') as f:f.write('\n        if( act == ACTION_INTERACT ) {}\nplayer_character.assign_activity( open_tile_activity_actor( std::nullopt ) );\n')
        with (root / "src/handle_action.cpp").open('a') as f:f.write('\nstatic void reach_attack( avatar &you )\n{\n}\n')
        (root / "src/melee.cpp").write_text('    const bool hits = hit_spread >= 0;')
        (root / "src/animation.cpp").write_text('#include "animation.h"\nclass basic_animation {\n        void draw() const {\n            static_popup popup;\n        }\n};\nclass bullet_animation {\n        void progress() const {\n            draw();\n\n            // Skip artificial animation delay\n        }\n};\n')
        (root / "src/avatar_action.cpp").write_text('#include "avatar_action.h"\nvoid avatar_action::fire_wielded_weapon( avatar &you )\n{\n    const item_location weapon = ncmm::select_ranged_weapon( you, ncmm::ranged_weapon_action::fire, "select", "select" );\n    if( weapon == you.get_wielded_item() ) {\n        you.assign_activity( aim_activity_actor::use_wielded() );\n    } else {\n        you.assign_activity( aim_activity_actor::use_item_location( weapon ) );\n    }\n}\n')
        (root / "src/ranged.cpp").write_text('target_handler::trajectory target_ui::run()\n{\n}\n        curshot++;\n\n        if( !gun.is_gun() ) {}\n    bool skip_redraw = false;')
        (root / "src/creature.cpp").write_text("int Creature::deal_melee_attack( Creature *source, int hitroll )\n{\n}\n    apply_damage( source, bp, total_damage );")
        (root / "src/ballistics.cpp").write_text('#include "ballistics.h"\n    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );\n        // Done with the trajectory!')
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
    def test_static_detail_frozen_oracle_is_independent_and_unchanged(self):
        frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_cache_test.cpp").read_text(encoding="utf-8")
        begin, end = "// BEGIN FROZEN STATIC DETAIL\n", "// END FROZEN STATIC DETAIL\n"
        self.assertEqual(frozen.count(begin), 1)
        self.assertEqual(frozen.count(end), 1)
        original = frozen.split(begin, 1)[1].split(end, 1)[0]
        self.assertEqual(len(original.encode("utf-8")), 1759)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), "f4013a7f84a0163079fb9fd73881459707da474e82587b732ede4f8440a0317d")
        self.assertNotIn("graphics_populate_static_detail", original)
    def test_static_detail_both_pins_and_source_receipt(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_static_detail.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                self.assertEqual((root / "src" / helper.name).read_bytes(), helper.read_bytes())
                hybrid = (root / "src/ncmm_graphics_hybrid.inc").read_text(encoding="utf-8")
                self.assertEqual(hybrid.count('#include "ncmm_graphics_static_detail.inc"'), 1)
                call = "if(graphics_populate_static_detail(here,p,d,world,facade_lights,i,terrain_shape,copy_id))continue;"
                self.assertEqual(hybrid.count(call), 1)
                self.assertIn(call + "\n        const ter_t &ter = here.ter( p ).obj(); const furn_t &furn = here.furn( p ).obj();", hybrid)
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                for name in (helper.name, "ncmm_graphics_hybrid.inc"):
                    self.assertEqual(receipt["inputs"][name], hashlib.sha256((root / "src" / name).read_bytes()).hexdigest())
    def test_static_detail_existing_include_failure_is_atomic(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                (root / "src/ncmm_graphics_static_detail.inc").write_text("existing extraction\n", encoding="utf-8")
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    with self.assertRaisesRegex(ValueError, "Experimental source already exists: ncmm_graphics_static_detail.inc"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_static_detail_missing_helper_failure_is_atomic(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_static_detail.inc")
        original_read = Path.read_text
        def read(path, *args, **kwargs):
            if path == helper:
                raise FileNotFoundError("missing static-detail source")
            return original_read(path, *args, **kwargs)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                    with self.assertRaisesRegex(FileNotFoundError, "missing static-detail source"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_terrain_stair_sampler_frozen_oracle_is_independent_and_unchanged(self):
        frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_render_test.cpp").read_text(encoding="utf-8")
        begin, end = "// BEGIN FROZEN TERRAIN STAIR SAMPLER\n", "// END FROZEN TERRAIN STAIR SAMPLER\n"
        self.assertEqual(frozen.count(begin), 1)
        self.assertEqual(frozen.count(end), 1)
        original = frozen.split(begin, 1)[1].split(end, 1)[0]
        self.assertEqual(len(original.encode("utf-8")), 1682)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), "8678d4be60bcb162394c0353dff16fc90949d7e9c7a37b6bca6d2fc2a3322b3a")
        self.assertNotIn("graphics_world_terrain_stair_flags", original)
    def test_multilevel_observer_sources_both_pins_and_receipt(self):
        names = ("ncmm_graphics_terrain_stair.inc", "ncmm_graphics_multilevel_observer.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                for name in names:
                    self.assertEqual((root / "src" / name).read_bytes(), Path(bridge.__file__).with_name(name).read_bytes())
                world = (root / "src/ncmm_graphics_world.inc").read_text(encoding="utf-8")
                observer = (root / "src/ncmm_graphics_multilevel_smoke.inc").read_text(encoding="utf-8")
                self.assertEqual(world.count('#include "ncmm_graphics_terrain_stair.inc"'), 1)
                self.assertEqual(observer.count('#include "ncmm_graphics_multilevel_observer.inc"'), 1)
                self.assertIn("graphics_world_terrain_stair_flags(here,ax,ay,z,ox,oy,side,side,loaded.front(),loaded.back())", world)
                self.assertIn("world.loaded_min_z,world.loaded_max_z", observer)
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                for name in names + ("ncmm_graphics_world.inc", "ncmm_graphics_multilevel_smoke.inc"):
                    self.assertEqual(receipt["inputs"][name], hashlib.sha256((root / "src" / name).read_bytes()).hexdigest())
    def test_multilevel_observer_existing_source_failure_is_atomic(self):
        for pin in bridge.SOURCES:
            for name in ("ncmm_graphics_terrain_stair.inc", "ncmm_graphics_multilevel_observer.inc"):
                with self.subTest(pin=pin, source=name), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root)
                    (root / "src" / name).write_text("existing observer source\n", encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "Experimental source already exists: " + re.escape(name)):
                            bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))
    def test_multilevel_observer_missing_source_failure_is_atomic(self):
        original_read = Path.read_text
        for pin in bridge.SOURCES:
            for name in ("ncmm_graphics_terrain_stair.inc", "ncmm_graphics_multilevel_observer.inc"):
                helper = Path(bridge.__file__).with_name(name)
                def read(path, *args, **kwargs):
                    if path == helper:
                        raise FileNotFoundError("missing observer source")
                    return original_read(path, *args, **kwargs)
                with self.subTest(pin=pin, source=name), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root)
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                        with self.assertRaisesRegex(FileNotFoundError, "missing observer source"):
                            bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))
    def test_candidate_range_frozen_oracle_is_independent_and_unchanged(self):
        frozen = (REPO / "experiments/FirstPersonView/tests/multilevel_cache_test.cpp").read_text(encoding="utf-8")
        begin, end = "// BEGIN FROZEN CANDIDATE RANGE\n", "// END FROZEN CANDIDATE RANGE\n"
        self.assertEqual(frozen.count(begin), 1)
        self.assertEqual(frozen.count(end), 1)
        original = frozen.split(begin, 1)[1].split(end, 1)[0]
        self.assertEqual(len(original.encode("utf-8")), 839)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), "9f5d494805a4f67005db9636c92ee80cbfe677a6ad492a9497349738936b37b6")
        self.assertIn("lower_in_range=in_visual_range(below);", original)
    def test_candidate_range_both_pins_and_source_receipt(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_candidate_range.inc")
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                self.assertEqual((root / "src" / helper.name).read_bytes(), helper.read_bytes())
                world = (root / "src/ncmm_graphics_world.inc").read_text(encoding="utf-8")
                self.assertEqual(world.count('#include "ncmm_graphics_candidate_range.inc"'), 1)
                self.assertIn("if(!static_surface&&!current_in_range&&!lower_in_range&&p!=you.pos_bub())", world)
                self.assertIn("if(!structure_range&&!lower_in_range)permission&=~NCMM_WORLD_FLOOR_BOTTOM;", world)
                self.assertIn("if(permission&NCMM_WORLD_FLOOR_BOTTOM){const auto rgb=graphics_world_rgb(below);", world)
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                for name in (helper.name, "ncmm_graphics_world.inc"):
                    self.assertEqual(receipt["inputs"][name], hashlib.sha256((root / "src" / name).read_bytes()).hexdigest())
    def test_candidate_range_existing_include_failure_is_atomic(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                (root / "src/ncmm_graphics_candidate_range.inc").write_text("existing candidate evaluation\n", encoding="utf-8")
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    with self.assertRaisesRegex(ValueError, "Experimental source already exists: ncmm_graphics_candidate_range.inc"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_candidate_range_missing_helper_failure_is_atomic(self):
        helper = Path(bridge.__file__).with_name("ncmm_graphics_candidate_range.inc")
        original_read = Path.read_text
        def read(path, *args, **kwargs):
            if path == helper:
                raise FileNotFoundError("missing candidate-range source")
            return original_read(path, *args, **kwargs)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                before = self.fingerprint(root)
                with patch.object(bridge.subprocess, "check_output", return_value=pin), patch.object(Path, "read_text", read):
                    with self.assertRaisesRegex(FileNotFoundError, "missing candidate-range source"):
                        bridge.apply(root)
                self.assertEqual(before, self.fingerprint(root))
    def test_native_apparent_light_adaptation_retains_both_primary_pin_bodies(self):
        original = apparent_light_original()
        digest = "7e570456c43c6dcf6b02d29bd4ef8a25a63a0889006855f28483870dd6a70a14"
        self.assertEqual(len(original.encode("utf-8")), 5280)
        self.assertEqual(hashlib.sha256(original.encode("utf-8")).hexdigest(), digest)
        self.assertEqual(set(bridge.APPARENT_LIGHT_ORIGINALS), set(bridge.SOURCES))
        extracted = Path(bridge.__file__).with_name("ncmm_graphics_apparent_light.inc").read_text(encoding="utf-8")
        signature = "map::apparent_light_info map::apparent_light_helper( const level_cache &map_cache,\n        const tripoint_bub_ms &p )\n"
        implementation = "static map::apparent_light_info ncmm_apparent_light_impl( const level_cache &map_cache,\n        const tripoint_bub_ms &p, const avatar &u, const int dist )\n"
        native_locals = "    avatar const &u = get_avatar();\n    const int dist = rl_dist( u.pos_bub(), p );\n"
        helper, outer = original.split("\nlit_level map::apparent_light_at(", 1)
        self.assertEqual(helper.count(native_locals), 1)
        adapted_helper = helper.replace(signature, implementation, 1).replace(native_locals, "", 1)
        wrapper = signature + "{\n" + native_locals + "    return ncmm_apparent_light_impl( map_cache, p, u, dist );\n}\n"
        adapted_outer = ("lit_level map::apparent_light_at(" + outer).replace(
            "    const apparent_light_info a = apparent_light_helper( map_cache, p );",
            "    const apparent_light_info a = ncmm_apparent_light_impl( map_cache, p, get_avatar(), dist );", 1)
        # All remaining Native body text, including guards and float order, is
        # exact. The public wrapper retains its original getter/distance pair.
        self.assertEqual(extracted[extracted.index(implementation):], adapted_helper + "\n" + wrapper + "\n" + adapted_outer)
        self.assertTrue(extracted.startswith("// Adapted from Cataclysm: Dark Days Ahead"))
        self.assertIn("https://creativecommons.org/licenses/by-sa/3.0/", extracted)
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin):
                self.assertEqual(bridge.APPARENT_LIGHT_ORIGINALS[pin], (5280, digest))
                self.assertIn("/blob/" + pin + "/src/lightmap.cpp", extracted)
    def test_native_apparent_light_both_pins_and_source_receipt(self):
        for pin in bridge.SOURCES:
            with self.subTest(pin=pin), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); self.fixture(root)
                original = (root / "src/lightmap.cpp").read_text(encoding="utf-8")
                with patch.object(bridge.subprocess, "check_output", return_value=pin):
                    bridge.apply(root)
                actual = (root / "src/lightmap.cpp").read_text(encoding="utf-8")
                self.assertEqual(actual, original.replace(apparent_light_original(), '#include "ncmm_graphics_apparent_light.inc"\n', 1))
                self.assertEqual((root / "src/ncmm_graphics_apparent_light.inc").read_bytes(), Path(bridge.__file__).with_name("ncmm_graphics_apparent_light.inc").read_bytes())
                self.assertIn(APPARENT_LIGHT_DECLARATIONS, (root / "src/map.h").read_text(encoding="utf-8"))
                receipt = json.loads((root / "first-person-source.json").read_text(encoding="utf-8"))
                self.assertEqual(receipt["source_commit"], pin)
                self.assertIn("lightmap.cpp", receipt["inputs"])
                self.assertIn("ncmm_graphics_apparent_light.inc", receipt["inputs"])
                for name, digest in receipt["inputs"].items():
                    self.assertEqual(hashlib.sha256((root / "src" / name).read_bytes()).hexdigest(), digest)
    def test_native_apparent_light_anchor_failure_is_atomic_including_receipt(self):
        original = apparent_light_original()
        for pin in bridge.SOURCES:
            for changed in (original.replace("avatar const &u = get_avatar();", "avatar const &u = changed_avatar();"),
                            original.replace("a.apparent_light >= cache.vision_threshold", "a.apparent_light > cache.vision_threshold"),
                            original + original, "changed Native lightmap"):
                with self.subTest(pin=pin, changed=changed[:80]), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp); self.fixture(root)
                    (root / "src/lightmap.cpp").write_text(changed, encoding="utf-8")
                    (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
                    before = self.fingerprint(root)
                    with patch.object(bridge.subprocess, "check_output", return_value=pin):
                        with self.assertRaisesRegex(ValueError, "Native apparent-light source differs"):
                            bridge.apply(root)
                    self.assertEqual(before, self.fingerprint(root))
    def test_native_apparent_light_existing_include_failure_is_atomic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/ncmm_graphics_apparent_light.inc").write_text("existing Native include\n", encoding="utf-8")
            (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError, "Experimental source already exists: ncmm_graphics_apparent_light.inc"):
                    bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_native_apparent_light_missing_source_is_atomic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            (root / "src/lightmap.cpp").unlink()
            (root / "first-person-source.json").write_text('{"unchanged_receipt":true}\n', encoding="utf-8")
            before = self.fingerprint(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                with self.assertRaises(FileNotFoundError):
                    bridge.apply(root)
            self.assertEqual(before, self.fingerprint(root))
    def test_multilevel_minor_two_dispatch_preserves_legacy_and_rejects_newer(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            with patch.object(bridge.subprocess, "check_output", return_value=next(iter(bridge.SOURCES))):
                bridge.apply(root)
            loader = (root / "src/ncmm_loader.cpp").read_text(encoding="utf-8")
            # Inspect the actual generated dispatch, not a second negotiation
            # helper which could pass while the inserted Native branch drifted.
            limits = re.search(r'if\( interface_id && std::string\( interface_id \) == NCMM_GRAPHICS_MULTILEVEL_ID \) \{\s*'
                               r'return min_major <= (\d+) && min_minor <= (\d+) \? &graphics_world_api : nullptr;', loader)
            self.assertIsNotNone(limits)
            major, minor = map(int, limits.groups())
            for requested_major, requested_minor, accepted in ((1,0,True),(1,1,True),(1,2,True),(1,3,False),(2,0,False)):
                with self.subTest(major=requested_major, minor=requested_minor):
                    self.assertEqual(requested_major <= major and requested_minor <= minor, accepted)
            published = (root / "src/ncmm_graphics_world.inc").read_text(encoding="utf-8")
            self.assertIn('graphics_world_api={1,sizeof(ncmm_host_graphics_multilevel_v1),1,2,', published)
            self.assertIn('graphics_get_world_metrics,graphics_get_lod,graphics_get_skyline}', published)
    def test_changed_submap_cache_reset_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/map.cpp';target.write_text(target.read_text().replace('set_pathfinding_cache_dirty( z );','changed_native_cache_reset();'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_native_firing_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/avatar_action.cpp'
            target.write_text(target.read_text().replace('void avatar_action::fire_wielded_weapon( avatar &you )', 'void changed_native_firing( avatar &you )'))
            before=self.fingerprint(root)
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):
                with self.assertRaisesRegex(ValueError,'one exact anchor'):bridge.apply(root)
            self.assertEqual(before,self.fingerprint(root))
    def test_changed_native_animation_anchor_no_partial_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root)
            target=root/'src/animation.cpp';target.write_text(target.read_text().replace('static_popup popup;', 'changed_native_animation();'))
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
                firing=(root/'src/avatar_action.cpp').read_text(encoding='utf-8')
                self.assertLess(firing.index('graphics_take_fps_gunshot'),firing.index('ncmm::select_ranged_weapon'))
                self.assertIn('aim_activity_actor::use_item_location( weapon )',firing)
                animation=(root/'src/animation.cpp').read_text(encoding='utf-8')
                self.assertLess(animation.index('ncmm::graphics_cursor_captured()'),animation.index('static_popup popup;'))
                self.assertIn('ui_manager::redraw_invalidated();',animation)
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
    def test_blocking_native_wait_has_presentation_deadline(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);sdl=root/'src/sdltiles.cpp'
            sdl.write_text(sdl.read_text()+'\nstatic int input_wait_ms( const std::optional<uint32_t> &input_ms ) {\n    return input_wait_timeout_ms( s );\n}\n')
            with patch.object(bridge.subprocess,'check_output',return_value=next(iter(bridge.SOURCES))):bridge.apply(root)
            self.assertIn('return ncmm::graphics_input_wait_timeout( input_wait_timeout_ms( s ) );',sdl.read_text())
            ballistic=(root/'src/ballistics.cpp').read_text()
            self.assertIn('&& !ncmm::graphics_projectile_presentation()',ballistic)
            self.assertIn('graphics_note_projectile(origin,*here,proj_arg,source,tp,t_copy)',ballistic)
            self.assertIn('if(ncmm::graphics_projectile_presentation())return;', (root/'src/animation.cpp').read_text())
            self.assertEqual((root/'src/ncmm_loader.cpp').read_text(encoding='utf-8').count('#include "ncmm_graphics_projectiles.h"'),1)
    def test_changed_blocking_wait_anchor_is_atomic(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.fixture(root);sdl=root/'src/sdltiles.cpp'
            sdl.write_text(sdl.read_text()+'\nstatic int input_wait_ms( int changed ) { return changed; }\n');before=self.fingerprint(root)
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
            ('input.cpp','void input_manager::wait_for_any_key()\n{'),
            ('input.cpp','#include "input.h"'),
            ('debug.cpp','    for( bool stop = false; !stop; ) {'),
            ('debug.cpp','#include "debug.h"'),
            ('input_context.cpp','        if( next_action.type == input_event_t::timeout ) {'),
            ('handle_action.cpp','        if( act == ACTION_INTERACT ) {'),
            ('handle_action.cpp','static void reach_attack( avatar &you )\n{\n'),
            ('handle_action.cpp','player_character.assign_activity( open_tile_activity_actor( std::nullopt ) );'),
            ('sdltiles.cpp','    } else if( ui_adaptor::has_imgui() ) {\n        try_sdl_update();\n    }'),
            ('sdltiles.cpp','    ImGuiIO &io = ImGui::GetIO();\n    if( !mouse.enabled ) {'),
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
