#!/usr/bin/env python3
"""Prepare an opt-in, non-shipping Host after NCMM's canonical patch stack.

All identity/anchor checks complete before writing. Run on a clean disposable
worktree; this intentionally rejects unknown upstream identities and reapplication.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

SOURCES = {
    "3f7fb352bf492ba521bd9408a0c9f6ce239e8d83": "cdda-experimental-2026-10-01-1040",
    "074aa98bd5be3de4c35f154082db32a0e63bb0f1": "cdda-experimental-2026-10-06-1807",
}

def apply(root: Path):
    bridge = Path(__file__).resolve().parent
    repo = bridge.parents[1]
    identity = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if identity not in SOURCES:
        raise ValueError(f"Unqualified experimental source: {identity}")
    updates = {}
    def read(name):
        return (root / "src" / name).read_text(encoding="utf-8-sig")
    def replace(text, before, after):
        if text.count(before) != 1:
            raise ValueError(f"Experimental bridge requires one exact anchor: {before[:100]}")
        return text.replace(before, after, 1)
    loader = read("ncmm_loader.cpp")
    # This exact generic event helper is appended by the canonical reactive layer.
    event_helper = "\nvoid runtime_player_kill_notify()\n{\n    dispatch_event_v2( NCMM_EVENT_PLAYER_KILL_V2 );\n}\n"
    if loader.replace(event_helper, "") != (repo / "host_patch/ncmm_loader.cpp").read_text(encoding="utf-8-sig"):
        raise ValueError("Apply the current canonical NCMM Host first; loader identity differs")
    # Keep the preview fixture independent of random quickstart enchantments.
    loader = replace(loader, '#include "mapdata.h"\n', '#include "mapdata.h"\n#include "magic_enchantment.h"\n')
    loader = replace(loader, '        const size_t suspension = find_perk_index( "mom_telekinetic_suspension" );\n        const auto suspension_fail = [&]() {\n            write_gameplay_smoke_result( false, "telekinetic_suspension_encumbrance",\n                                         aws_setting_count, aws_hook_count, survivor_perk_count );\n            return 153;\n        };\n        if( suspension >= survivor_perk_count || perk_max_rank( suspension ) != 3 ) {\n            return suspension_fail();\n        }\n        npc encumbrance_npc;\n        const auto body_parts = get_avatar().get_all_body_parts();\n        if( body_parts.empty() ) return suspension_fail();\n        for( int rank : { 0, 1, 2, 3, 0 } ) {\n            if( !perk_set_rank( suspension, rank ) || !perk_recalc() ||\n                std::abs( gameplay_modifier( "encumbrance_pct" ) + 5.0 * rank ) > 0.000001 ) {\n                return suspension_fail();\n            }\n            for( int base : { 0, 1, 7, 20, 40, 100 } ) {\n                encumbrance_data fixture;\n                fixture.encumbrance = base;\n                for( const bodypart_id &bp : body_parts ) {\n                    get_avatar().set_part_encumbrance_data( bp, fixture );\n                    const int expected = ( base * ( 100 - 5 * rank ) + 50 ) / 100;\n                    // Both action accessor and direct limb-score consumer, twice:\n                    // no stale equipment cache or repeated multiplicative reduction.\n                    for( int repeat = 0; repeat < 2; ++repeat ) {\n                        if( get_avatar().encumb( bp ) != expected ||\n                            get_avatar().get_part( bp )->get_final_encumbrance( get_avatar() ) != expected ) {\n                            return suspension_fail();\n                        }\n                    }\n                    if( encumbrance_npc.has_part( bp ) ) {\n                        encumbrance_npc.set_part_encumbrance_data( bp, fixture );\n                        if( encumbrance_npc.encumb( bp ) != base ) return suspension_fail();\n                    }\n                }\n            }\n        }\n        get_avatar().calc_encumbrance();\n        if( !perk_reset() || !perk_recalc() ) return suspension_fail();\n        log_line( NCMM_LOG_INFO, "Telekinetic Suspension: all body parts, ranks 0/1/2/3/reset, rounding and NPC isolation PASS." );\n\n',
        '        const size_t suspension = find_perk_index( "mom_telekinetic_suspension" );\n        const auto suspension_fail = [&]() {\n            write_gameplay_smoke_result( false, "telekinetic_suspension_encumbrance",\n                                         aws_setting_count, aws_hook_count, survivor_perk_count );\n            return 153;\n        };\n        if( suspension >= survivor_perk_count || perk_max_rank( suspension ) != 3 ) {\n            return suspension_fail();\n        }\n        npc encumbrance_npc;\n        const auto body_parts = get_avatar().get_all_body_parts();\n        if( body_parts.empty() ) return suspension_fail();\n        for( int rank : { 0, 1, 2, 3, 0 } ) {\n            if( !perk_set_rank( suspension, rank ) || !perk_recalc() ||\n                std::abs( gameplay_modifier( "encumbrance_pct" ) + 5.0 * rank ) > 0.000001 ) {\n                return suspension_fail();\n            }\n            for( int base : { 0, 1, 7, 20, 40, 100 } ) {\n                encumbrance_data fixture;\n                fixture.encumbrance = base;\n                for( const bodypart_id &bp : body_parts ) {\n                    get_avatar().set_part_encumbrance_data( bp, fixture );\n                    // Quickstart generates a random avatar, whose mutations or\n                    // equipment can modify vanilla encumbrance before our perk.\n                    const int vanilla = static_cast<int>( std::max( 0.0,\n                                        get_avatar().enchantment_cache->modify_encumbrance( bp.id(), base ) ) );\n                    const int expected = ( vanilla * ( 100 - 5 * rank ) + 50 ) / 100;\n                    // Both action accessor and direct limb-score consumer, twice:\n                    // no stale equipment cache or repeated multiplicative reduction.\n                    for( int repeat = 0; repeat < 2; ++repeat ) {\n                        if( get_avatar().encumb( bp ) != expected ||\n                            get_avatar().get_part( bp )->get_final_encumbrance( get_avatar() ) != expected ) {\n                            log_line( NCMM_LOG_ERROR, ( "Suspension fixture mismatch: " + bp.id().str() +\n                                      " rank=" + std::to_string( rank ) + " vanilla=" + std::to_string( vanilla ) +\n                                      " expected=" + std::to_string( expected ) +\n                                      " actual=" + std::to_string( get_avatar().encumb( bp ) ) ).c_str() );\n                            return suspension_fail();\n                        }\n                    }\n                    if( encumbrance_npc.has_part( bp ) ) {\n                        encumbrance_npc.set_part_encumbrance_data( bp, fixture );\n                        const int npc_vanilla = static_cast<int>( std::max( 0.0,\n                                                encumbrance_npc.enchantment_cache->modify_encumbrance( bp.id(), base ) ) );\n                        if( encumbrance_npc.encumb( bp ) != npc_vanilla ) return suspension_fail();\n                    }\n                }\n            }\n        }\n        get_avatar().calc_encumbrance();\n        if( !perk_reset() || !perk_recalc() ) return suspension_fail();\n        log_line( NCMM_LOG_INFO, "Telekinetic Suspension: all body parts, ranks 0/1/2/3/reset, rounding and NPC isolation PASS." );\n\n')
    loader = replace(loader, '#include "ncmm_loader.h"', '#include "ncmm_loader.h"\n#include "ncmm_graphics.h"\n#include "ncmm_graphics_v2.h"\n#include "ncmm_graphics_shapes.hpp"\n#include "ncmm_graphics_policy.hpp"\n#if defined(TILES)\n#include "cata_tiles.h"\n#include "sdltiles.h"\n#include "monster.h"\n#include "mtype.h"\n#include "creature_tracker.h"\n#include "lightmap.h"\n#include "level_cache.h"\n#include "map_memory.h"\n#include "mapdata.h"\n#include "weather.h"\n#include "weather_type.h"\n#include "field.h"\n#include "item.h"\n#include "item_location.h"\n#include "vehicle.h"\n#include "veh_type.h"\n#include "vpart_position.h"\n#include "messages.h"\n#endif')
    loader = replace(loader, '#include "ncmm_graphics_v2.h"', '#include "ncmm_graphics_v2.h"\n#include "generic_factory.h"')
    loader = replace(loader, "struct loaded_mod {", "void graphics_erase( const std::string &id );\nstruct loaded_mod {")
    loader = replace(loader, '    "core.v1",', '#if defined(TILES)\n    "graphics.viewport.v1",\n    "graphics.viewport.v2",\n#endif\n    "core.v1",')
    loader = replace(loader, "const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )\n{",
        '#include "ncmm_graphics_bridge.inc"\n\nconst void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )\n{\n#if defined(TILES)\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_V2_ID ) {\n        return min_major <= 2 && min_minor == 0 ? &graphics_hybrid_api : nullptr;\n    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_ID ) {\n        return min_major <= 1 && min_minor == 0 ? &graphics_api : nullptr;\n    }\n#endif')
    loader = replace(loader, "void clear_module_runtime_v2( const std::string &module_id )\n{",
        "void clear_module_runtime_v2( const std::string &module_id )\n{\n    graphics_erase( module_id );")
    loader = replace(loader, "    mod.fault.quarantine( kind );", "    graphics_erase( module_id );\n    mod.fault.quarantine( kind );")
    loader = replace(loader, "    if( !id.empty() ) {\n        erase_module_modifiers( id );",
        "    if( !id.empty() ) {\n        graphics_erase( id );\n        erase_module_modifiers( id );")
    loader = replace(loader, "void register_gameplay_actions( input_context &ctxt )\n{",
        "void register_gameplay_actions( input_context &ctxt )\n{\n    graphics_register_actions( ctxt );")
    loader = replace(loader, "bool handle_gameplay_action( const std::string &action )\n{",
        "bool handle_gameplay_action( const std::string &action )\n{\n    if( graphics_action( action ) ) return true;")
    loader = replace(loader, "void reset_world_lifecycle()\n{", "void reset_world_lifecycle()\n{\n    graphics_view.enabled = false;\n    graphics_clear_hybrid();\n    graphics_gameplay_context = false;")
    loader = replace(loader, "void shutdown()\n{", "void shutdown()\n{\n    graphics_view = {};\n    graphics_frame = nullptr;\n    graphics_commands.clear();\n    graphics_clear_hybrid();")
    loader = replace(loader, '        write_gameplay_smoke_result( true, "ok", aws_setting_count,',
        '        if( module_ids.count( "first_person_view" ) && !graphics_scene_smoke() ) {\n            write_gameplay_smoke_result( false, "first_person_graphics_failed", aws_setting_count, aws_hook_count, survivor_perk_count );\n            return 191;\n        }\n\n        write_gameplay_smoke_result( true, "ok", aws_setting_count,')
    loader = replace(loader, "void gameplay_metric_record_completed_craft( const Character &who )\n{",
        "void graphics_note_input_context( const std::string &category )\n{\n    const bool gameplay = ncmm_graphics_shapes::stays_perspective( category );\n    if( gameplay != graphics_gameplay_context && g ) g->invalidate_main_ui_adaptor();\n    graphics_gameplay_context = gameplay;\n}\nbool graphics_preserves_destination( const std::string &action )\n{\n    return !graphics_view.owner.empty() && ( action == \"ncmm.open.\" + graphics_view.owner ||\n        action == \"ncmm.view.turn_left\" || action == \"ncmm.view.turn_right\" ||\n        action == \"ncmm.view.look_up\" || action == \"ncmm.view.look_down\" || action == \"ncmm.view.quality\" );\n}\nbool graphics_requires_terrain_pass()\n{\n    return graphics_view.enabled && graphics_gameplay_context;\n}\nbool draw_graphics_view( int x, int y, int width, int height )\n{\n    return graphics_draw( x, y, width, height );\n}\n\nvoid gameplay_metric_record_completed_craft( const Character &who )\n{")
    loader = replace(loader, '#include "generic_factory.h"', '#include "generic_factory.h"\n#include "ncmm_graphics_controls.h"\n#include "ncmm_graphics_camera.hpp"\n#include "line.h"\n#if defined(TILES)\n#include "sdl_renderer_recovery.h"\n#endif')
    loader = replace(loader, '    "graphics.viewport.v2",', '    "graphics.viewport.v2",\n    "graphics.controls.v1",')
    loader = replace(loader, '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_V2_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CONTROLS_ID ) {\n        return min_major <= 1 && min_minor == 0 ? &graphics_controls_api : nullptr;\n    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_V2_ID ) {')
    loader = replace(loader, "    graphics_gameplay_context = false;", "    graphics_gameplay_context = false;\n    graphics_control_context = false;\n    graphics_release_mouse();\n    graphics_cancel_animation();\n    graphics_attack_serial = 0;")
    loader = replace(loader, "void shutdown()\n{\n    graphics_view = {};", "void shutdown()\n{\n    graphics_release_mouse();\n    graphics_cancel_animation();\n    graphics_view = {};")
    loader = replace(loader, "    graphics_gameplay_context = gameplay;", "    graphics_gameplay_context = gameplay;\n    graphics_control_context = category == \"DEFAULTMODE\";\n    if( !graphics_control_context ) graphics_release_mouse();")
    loader = replace(loader, 'action == "ncmm.view.quality" );', 'action == "ncmm.view.quality" || action == "ncmm.view.mouse_look" || action == "ncmm.view.acceleration" );')
    loader = replace(loader, "bool graphics_requires_terrain_pass()\n{", "std::string graphics_relative_action( const std::string &action, const input_event &event, bool vehicle_control )\n{\n    return graphics_map_relative_action( action, event, vehicle_control );\n}\nvoid graphics_note_attack( uint32_t kind )\n{\n    graphics_notify_attack( kind );\n}\nbool graphics_mouse_event( uint32_t kind, float x, float y, float dx, float dy, bool allowed )\n{\n    return graphics_process_mouse( kind, x, y, dx, dy, allowed );\n}\nbool graphics_animation_redraw_due()\n{\n    return graphics_redraw_due();\n}\nbool graphics_requires_terrain_pass()\n{")
    loader = replace(loader, '#include "ncmm_graphics_camera.hpp"', '#include "ncmm_graphics_camera.hpp"\n#include "ncmm_graphics_presentation_policy.hpp"\n#include "ncmm_graphics_composite.hpp"\n#include "itype.h"\n#include "field_type.h"\n#include "skill.h"\n#include "monstergenerator.h"\n#include "ranged.h"')
    loader = replace(loader, '    "graphics.controls.v1",', '    "graphics.controls.v1",\n    "graphics.presentation.v1",')
    loader = replace(loader, '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CONTROLS_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_PRESENTATION_ID ) {\n        return min_major <= 1 && min_minor == 0 ? &graphics_presentation_api : nullptr;\n    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CONTROLS_ID ) {')
    loader = replace(loader, "bool graphics_requires_terrain_pass()\n{", "std::string graphics_click_action()\n{\n    return graphics_click_action_impl();\n}\nbool graphics_consume_aimed_fire(int x,int y,int z)\n{\n    return graphics_consume_click_impl(x,y,z);\n}\nbool graphics_requires_terrain_pass()\n{")
    loader = replace(loader, '    graphics_control_context = category == "DEFAULTMODE";', '    graphics_control_context = category == "DEFAULTMODE";\n    if(graphics_control_context)graphics_click_pending=false;')
    loader = replace(loader, '#include "ncmm_graphics_presentation_policy.hpp"', '#include "ncmm_graphics_presentation_policy.hpp"\n#include "ncmm_graphics_creatures.h"\n#include "ncmm_graphics_creature_policy.hpp"\n#include "ncmm_graphics_performance.hpp"\n#include "json.h"\n#include "ballistics.h"\n#include "projectile.h"\n#include "dispersion.h"')
    loader = replace(loader, '    "graphics.presentation.v1",', '    "graphics.presentation.v1",\n    "graphics.creatures.v1",')
    loader = replace(loader, '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_PRESENTATION_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CREATURES_ID ) {\n        return min_major <= 1 && min_minor == 0 ? &graphics_creatures_api : nullptr;\n    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_PRESENTATION_ID ) {')
    loader = replace(loader, "bool graphics_requires_terrain_pass()\n{", "void graphics_note_creature( Creature *who, uint32_t event, float heading )\n{\n#if defined(TILES)\n    graphics_notify_creature( who, event, heading );\n#else\n    (void)who; (void)event; (void)heading;\n#endif\n}\nbool graphics_requires_terrain_pass()\n{")
    loader = replace(loader, '#include "ncmm_graphics_camera.hpp"',
                     '#include "ncmm_graphics_camera.hpp"\n#include "ncmm_graphics_context.hpp"\n#if defined(TILES)\n#include <SDL3_image/SDL_image.h>\n#endif')
    loader = replace(loader, '    const bool gameplay = ncmm_graphics_shapes::stays_perspective( category );\n    if( gameplay != graphics_gameplay_context && g ) g->invalidate_main_ui_adaptor();\n    graphics_gameplay_context = gameplay;\n    graphics_control_context = category == "DEFAULTMODE";',
                     '    const bool previous = graphics_gameplay_context;\n    graphics_context.note( category );\n    if( previous != graphics_gameplay_context && g ) g->invalidate_main_ui_adaptor();')
    loader = replace(loader, '    graphics_gameplay_context = false;',
                     '    graphics_context.reset();\n    graphics_gameplay_context = false;')
    loader = replace(loader, 'void shutdown()\n{\n    graphics_release_mouse();',
                     'void shutdown()\n{\n    graphics_context.reset();\n    graphics_release_mouse();')
    loader = replace(loader, 'bool graphics_requires_terrain_pass()\n{',
                     'uint64_t graphics_begin_targeting()\n{\n    if( graphics_fire_audit_legacy_context ) return 0;\n    const auto token = graphics_context.begin_aiming();\n    graphics_release_mouse();\n    if( g ) g->invalidate_main_ui_adaptor();\n    return token;\n}\nvoid graphics_end_targeting( uint64_t token )\n{\n    graphics_context.end_aiming( token );\n    if( g ) g->invalidate_main_ui_adaptor();\n}\nbool graphics_requires_terrain_pass()\n{')
    updates["ncmm_loader.cpp"] = loader
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], '#include "ncmm_graphics_performance.hpp"',
        '#include "ncmm_graphics_performance.hpp"\n#include "ncmm_graphics_gpu.h"\n#if defined(TILES)\n#include "ncmm_gpu_probe.hpp"\n#include "ncmm_gpu_backend.hpp"\n#endif')
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], '    "graphics.creatures.v1",', '    "graphics.creatures.v1",\n    "graphics.gpu.v1",')
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CREATURES_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_GPU_ID ) {\n        return min_major <= 1 && min_minor == 0 ? &graphics_gpu_api : nullptr;\n    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CREATURES_ID ) {')
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], "bool graphics_requires_terrain_pass()\n{", "void graphics_release_gpu()\n{\n    graphics_clear_gpu();\n#if defined(TILES)\n    // Module disable may already have released our private backend. The SDL\n    // renderer can still own queued presentation copies against its swapchain.\n    if( auto *renderer = get_sdl_renderer().get() ) {\n        SDL_FlushRenderer( renderer );\n        if( auto *device = SDL_GetGPURendererDevice( renderer ) ) SDL_WaitForGPUIdle( device );\n    }\n#endif\n}\nbool graphics_requires_terrain_pass()\n{")
    header = read("ncmm_loader.h")
    header = replace(header, 'void initialize();', '''// Private native UI lifetime; no public module ABI extension.
uint64_t graphics_begin_targeting();
void graphics_end_targeting( uint64_t token );
class graphics_targeting_scope {
    uint64_t token_;
public:
    explicit graphics_targeting_scope( bool preserve ) : token_( preserve ? graphics_begin_targeting() : 0 ) {}
    ~graphics_targeting_scope() { if( token_ ) graphics_end_targeting( token_ ); }
    graphics_targeting_scope( const graphics_targeting_scope & ) = delete;
    graphics_targeting_scope &operator=( const graphics_targeting_scope & ) = delete;
};
void initialize();''')
    updates["ncmm_loader.h"] = replace(header, "void initialize();", "void graphics_note_input_context( const std::string &category );\nbool graphics_preserves_destination( const std::string &action );\nbool graphics_requires_terrain_pass();\nbool draw_graphics_view( int x, int y, int width, int height );\nvoid initialize();")
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "class input_context;", "class input_context;\nstruct input_event;")
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "void initialize();", "std::string graphics_relative_action( const std::string &, const input_event &, bool );\nvoid graphics_note_attack( uint32_t kind );\nbool graphics_mouse_event( uint32_t kind, float x, float y, float dx, float dy, bool allowed );\nbool graphics_animation_redraw_due();\nvoid initialize();")
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "void initialize();", "void graphics_note_creature( Creature *, uint32_t event, float heading = 100.f );\nvoid initialize();")
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "void initialize();", "void graphics_release_gpu();\nvoid initialize();")
    action = read("handle_action.cpp")
    action_anchor = "    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }"
    updates["handle_action.cpp"] = replace(action, action_anchor,
        "    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        if( !ncmm::graphics_preserves_destination( action ) ) {\n            player_character.clear_destination();\n            destination_preview.clear();\n        }\n        return false;\n    }")
    updates["handle_action.cpp"] = replace(updates["handle_action.cpp"], "    if( act == ACTION_NULL ) {\n        act = look_up_action( action );", "    if( act == ACTION_NULL ) {\n        action = ncmm::graphics_relative_action( action, ctxt.get_raw_input(), veh_ctrl );\n        act = look_up_action( action );")
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "void initialize();", "std::string graphics_click_action();\nbool graphics_consume_aimed_fire(int x,int y,int z);\nvoid initialize();")
    updates["handle_action.cpp"] = replace(updates["handle_action.cpp"], "        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {",
        "        if( act == ACTION_SELECT && ncmm::graphics_requires_terrain_pass() ) {\n            const std::string aimed = ncmm::graphics_click_action();\n            if( aimed.empty() ) return false;\n            act = look_up_action( aimed );\n        }\n        if( act == ACTION_SELECT || act == ACTION_SEC_SELECT ) {")
    melee = read("melee.cpp")
    updates["melee.cpp"] = replace(melee, "    const bool hits = hit_spread >= 0;", "    if( is_avatar() ) ncmm::graphics_note_attack( 1 );\n    const bool hits = hit_spread >= 0;")
    ranged = read("ranged.cpp")
    ranged = replace(ranged, 'target_handler::trajectory target_ui::run()\n{',
                     'target_handler::trajectory target_ui::run()\n{\n    ncmm::graphics_targeting_scope ncmm_aiming( mode == TargetMode::Fire || mode == TargetMode::Reach ||\n            mode == TargetMode::TurretManual || mode == TargetMode::Turrets );')
    updates["ranged.cpp"] = replace(ranged, "        curshot++;\n\n        if( !gun.is_gun() )", "        curshot++;\n        if( is_avatar() ) ncmm::graphics_note_attack( 2 );\n\n        if( !gun.is_gun() )")
    updates["ranged.cpp"] = replace(updates["ranged.cpp"], "    bool skip_redraw = false;",
        "    bool skip_redraw = false;\n    // LMB already picked and confirmed a visible hostile sprite. Keep every\n    // ordinary firing/aiming readiness and trajectory check in this UI.\n    if((mode==TargetMode::Fire||mode==TargetMode::Reach)&&status==Status::Good&&\n        ncmm::graphics_consume_aimed_fire(dst.x(),dst.y(),dst.z())){\n        init_window_and_input();ui.position_from_window(w_target);\n        action=\"FIRE\";skip_redraw=true;\n    }")
    # Observe successful game pathways only. No move costs, targets, damage,
    # AI, saves or return values are modified by presentation hooks.
    creature = read("creature.cpp")
    creature = replace(creature, "int Creature::deal_melee_attack( Creature *source, int hitroll )\n{", "int Creature::deal_melee_attack( Creature *source, int hitroll )\n{\n    if( source ) ncmm::graphics_note_creature( source, 1, std::atan2( float( pos_abs().y() - source->pos_abs().y() ), float( pos_abs().x() - source->pos_abs().x() ) ) );")
    updates["creature.cpp"] = replace(creature, "    apply_damage( source, bp, total_damage );", "    apply_damage( source, bp, total_damage );\n    if( total_damage > 0 ) ncmm::graphics_note_creature( this, 3 );")
    ballistic = read("ballistics.cpp")
    ballistic = replace(ballistic, '#include "ballistics.h"', '#include "ballistics.h"\n#include "ncmm_loader.h"')
    updates["ballistics.cpp"] = replace(ballistic, '    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );', '    if( origin ) ncmm::graphics_note_creature( origin, 2, std::atan2( float( target_arg.y() - source.y() ), float( target_arg.x() - source.x() ) ) );\n    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );')
    updates["monster.cpp"] = replace(read("monster.cpp"), "    // We were carrying a creature, deposit the rider", "    ncmm::graphics_note_creature( this, 4 );\n    // We were carrying a creature, deposit the rider")
    updates["npc.cpp"] = replace(read("npc.cpp"), "    prevent_death_reminder = false;\n    dialogue d( get_talker_for( this ),", "    ncmm::graphics_note_creature( this, 4 );\n    prevent_death_reminder = false;\n    dialogue d( get_talker_for( this ),")
    context = read("input_context.cpp")
    context = replace(context, '#include "input_context.h"', '#include "input_context.h"\n#include "ncmm_loader.h"')
    updates["input_context.cpp"] = replace(context, "const std::string &input_context::handle_input( const int timeout )\n{",
        "const std::string &input_context::handle_input( const int timeout )\n{\n    ncmm::graphics_note_input_context( category );")
    updates["input_context.cpp"] = replace(updates["input_context.cpp"], '    return *result;',
        '    ncmm::graphics_note_input_result( category, *result );\n    return *result;')
    sdl = read("sdltiles.cpp")
    anchor = "        tilecontext->draw(\n            point( win->pos.x * fontwidth, win->pos.y * fontheight ),\n            g->ter_view_p,\n            TERRAIN_WINDOW_TERM_WIDTH * font->width,\n            TERRAIN_WINDOW_TERM_HEIGHT * font->height,\n            overlay_strings,\n            color_blocks );"
    updates["sdltiles.cpp"] = replace(sdl, anchor,
        "        // Retain the full native pass and its one-shot animation pixels.\n"+
        "        // A failed perspective callback must not consume those queues twice.\n"+
        "        bool ncmm_native_pixels_ready = false;\n"
        "        if( ncmm::graphics_requires_terrain_pass() ) {\n"+anchor+"\n            ncmm_native_pixels_ready = true;\n        }\n"+
        "        if( !ncmm::draw_graphics_view( win->pos.x * fontwidth, win->pos.y * fontheight,\n"+
        "                TERRAIN_WINDOW_TERM_WIDTH * font->width, TERRAIN_WINDOW_TERM_HEIGHT * font->height ) && !ncmm_native_pixels_ready ) {\n"+
        anchor+"\n        }")
    updates["sdltiles.cpp"] = replace(updates["sdltiles.cpp"],
        '        if( !ncmm::draw_graphics_view( win->pos.x * fontwidth, win->pos.y * fontheight,\n                TERRAIN_WINDOW_TERM_WIDTH * font->width, TERRAIN_WINDOW_TERM_HEIGHT * font->height ) && !ncmm_native_pixels_ready ) {',
        '        const bool ncmm_perspective_drawn = ncmm::draw_graphics_view( win->pos.x * fontwidth, win->pos.y * fontheight,\n                TERRAIN_WINDOW_TERM_WIDTH * font->width, TERRAIN_WINDOW_TERM_HEIGHT * font->height );\n        ncmm::graphics_note_terrain_composed( ncmm_perspective_drawn );\n        if( !ncmm_perspective_drawn && !ncmm_native_pixels_ready ) {')
    updates["sdltiles.cpp"] = replace(updates["sdltiles.cpp"], "static void CheckMessages()\n{", "static void CheckMessages()\n{\n    // Presentation redraws never return a gameplay TIMEOUT or advance a turn.\n    if( ncmm::graphics_animation_redraw_due() ) {\n        ui_manager::redraw_invalidated();\n        if( needupdate ) refresh_display();\n    }")
    wrappers = replace(read("sdl_wrappers.cpp"), '#include "sdl_wrappers.h"',
        '#include "sdl_wrappers.h"\n// Private presentation hooks; no shared engine header or module ABI change.\nnamespace ncmm { void graphics_frame_presented(); void graphics_before_present(); }')
    updates["sdl_wrappers.cpp"] = replace(wrappers,
        '    printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" );',
        '    ncmm::graphics_before_present();\n    if( !printErrorIf( !SDL_RenderPresent( renderer.get() ), "SDL_RenderPresent failed" ) ) {\n'
        '        ncmm::graphics_frame_presented();\n    }')
    # Every renderer teardown, including recovery and native test fixtures,
    # releases our children while the borrowed device is still alive.
    renderer_resets = updates["sdltiles.cpp"].count("renderer.reset();")
    if renderer_resets != 4:
        raise ValueError("expected one exact anchor set for four SDL renderer teardown sites")
    import re
    updates["sdltiles.cpp"] = re.sub(r'(?m)^(\s*)renderer\.reset\(\);', r'\1ncmm::graphics_release_gpu();\n\1renderer.reset();', updates["sdltiles.cpp"])
    updates["sdltiles.cpp"] = replace(updates["sdltiles.cpp"], "        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );", "        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );\n        const bool ncmm_mouse_allowed = !cataimgui::client::want_capture_mouse();\n        if( ev.type == SDL_EVENT_WINDOW_FOCUS_LOST ) ncmm::graphics_mouse_event( 4, 0, 0, 0, 0, false );\n        if( ev.type == SDL_EVENT_MOUSE_MOTION && ncmm::graphics_mouse_event( 3, ev_display.motion.x, ev_display.motion.y, ev.motion.xrel, ev.motion.yrel, ncmm_mouse_allowed ) ) continue;\n        if( ( ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP ) && ev.button.button == SDL_BUTTON_RIGHT &&\n            ncmm::graphics_mouse_event( ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 1u : 2u, ev_display.button.x, ev_display.button.y, 0, 0, ncmm_mouse_allowed ) ) continue;")
    updates["sdltiles.h"] = replace(read("sdltiles.h"), "SDL_Window *get_sdl_window();", "SDL_Window *get_sdl_window();\nvoid ncmm_draw_perspective_label( const std::string &, int, int, const SDL_Rect & );")
    updates["sdltiles.cpp"] += """
#if defined(TILES)
// Reuse CDDA's configured font and Unicode fallback list, not a Latin-only bitmap.
void ncmm_draw_perspective_label( const std::string &text, int x, int y, const SDL_Rect &clip )
{
    if( !font || text.empty() ) return;
    const int cells=std::max( 1,( clip.w-16 )/font->width );
    const std::string label=utf8_truncate( remove_color_tags( text ),cells );
    const int width=utf8_width( label )*font->width;
    x=std::clamp( x-width/2,clip.x+4,std::max( clip.x+4,clip.x+clip.w-width-4 ) );
    y=std::clamp( y-font->height-4,clip.y+4,std::max( clip.y+4,clip.y+clip.h-font->height-4 ) );
    curses_text_batch.begin_pass( renderer,!text_atlas_enabled( renderer ) );
    int offset=0;
    for( const char32_t ch : utf8_view( label ) ) {
        const std::string glyph=utf32_to_utf8( ch );
        const point at( x+offset,y );
        font->queue_char( curses_text_batch,renderer,glyph,at+point( 1,1 ),0 );
        font->queue_char( curses_text_batch,renderer,glyph,at,15 );
        offset+=std::max( 0,mk_wcwidth( ch ) )*font->width;
    }
    curses_text_batch.flush( renderer );
}
#endif
"""
    menu = read("main_menu.cpp")
    updates["main_menu.cpp"] = replace(menu,
        "        std::exit( ncmm::run_gameplay_smoke() );",
        "        const int smoke_result = ncmm::run_gameplay_smoke();\n"
        "        // Match the normal game/SDL teardown before process-lifetime destructors.\n"
        "        ncmm::shutdown();\n"
        "        deinitDebug();\n"
        "        g.reset();\n"
        "        catacurses::endwin();\n"
        "        std::exit( smoke_result );")
    tiles_h = read("cata_tiles.h")
    tiles_declaration = "        std::optional<texture_draw_data> get_texture_draw_data( const std::string &id,\n                TILE_CATEGORY category, const tripoint_bub_ms &p );"
    updates["cata_tiles.h"] = replace(tiles_h, tiles_declaration, tiles_declaration +
        "\n        std::optional<texture_draw_data> ncmm_get_texture_draw_data( const std::string &id,\n                TILE_CATEGORY category, const tripoint_bub_ms &p, bool background, bool stable = false );\n"
        "        bool ncmm_find_overlay( bool male, const std::string &overlay, const std::string &variant, std::string &id ) { return find_overlay_looks_like( male, overlay, variant, id ); }\n        void ncmm_unbind_gpu_state() { flush_sprite_shader_for_untextured_draw(); }\n        uint64_t ncmm_graphics_epoch = 0;\n        uint64_t ncmm_graphics_tileset_epoch() const { return ncmm_graphics_epoch; }\n        const std::string &ncmm_graphics_tileset_id() const { return tileset_ptr->get_tileset_id(); }")
    tiles_cpp = read("cata_tiles.cpp")
    start = tiles_cpp.index("std::optional<texture_draw_data> cata_tiles::get_texture_draw_data(")
    end = tiles_cpp.index("std::unordered_set<std::string> cata_tiles::get_all_portrait_tile_ids", start)
    lookup = tiles_cpp[start:end]
    lookup = replace(lookup, "cata_tiles::get_texture_draw_data", "cata_tiles::ncmm_get_texture_draw_data")
    lookup = replace(lookup, "TILE_CATEGORY category, const tripoint_bub_ms &p )", "TILE_CATEGORY category, const tripoint_bub_ms &p, const bool background, const bool stable )")
    lookup = replace(lookup, "    const std::vector<int> *indices = ttype.fg.pick( seed );",
        "    // Procedural anatomy animates its own joints; a 2D animation tick is not a new appearance.\n"
        "    if( stable && ttype.animated ) seed = 0;\n"
        "    // Paired qualification cannot use MONSTER's allocator-address variant seed.\n"
        "    if( stable && category == TILE_CATEGORY::MONSTER && std::getenv( \"NCMM_FP_APPEARANCE_SMOKE\" ) ) seed = 0;\n"
        "    const std::vector<int> *indices = background ? ttype.bg.pick( seed ) : ttype.fg.pick( seed );\n"
        "    if( background && !indices ) indices = ttype.fg.pick( seed );")
    tiles_cpp = tiles_cpp[:start] + lookup + tiles_cpp[start:]
    load_anchor = "                               const bool force, const bool pump_events, const bool terrain )\n{"
    updates["cata_tiles.cpp"] = replace(tiles_cpp, load_anchor, load_anchor + "\n    ++ncmm_graphics_epoch;")
    for name in ("ncmm_graphics.h", "ncmm_graphics_v2.h", "ncmm_graphics_shapes.hpp", "ncmm_graphics_policy.hpp", "ncmm_graphics_controls.h", "ncmm_graphics_camera.hpp", "ncmm_graphics_controls.inc", "ncmm_graphics_presentation.h", "ncmm_graphics_presentation_policy.hpp", "ncmm_graphics_presentation.inc", "ncmm_graphics_creatures.h", "ncmm_graphics_creature_policy.hpp", "ncmm_graphics_performance.hpp", "ncmm_graphics_creatures.inc", "ncmm_graphics_creature_smoke.inc", "ncmm_graphics_appearance_smoke.inc", "ncmm_graphics_composite.hpp", "ncmm_graphics_gpu.h", "ncmm_gpu_validation.hpp", "ncmm_gpu_device.hpp", "ncmm_gpu_shader_bytes.hpp", "ncmm_gpu_probe.hpp", "ncmm_gpu_probe_smoke.inc", "ncmm_gpu_game_smoke.inc", "ncmm_gpu_benchmark.inc", "ncmm_gpu_backend.hpp", "ncmm_gpu_geometry.hpp", "ncmm_gpu_programs.hpp", "ncmm_gpu_resources.hpp", "ncmm_graphics_gpu.inc", "ncmm_graphics_effects.inc", "ncmm_graphics_bridge.inc", "ncmm_graphics_hybrid.inc", "ncmm_graphics_smoke.inc"):
        if (root / "src" / name).exists():
            raise ValueError("Experimental source already exists: " + name)
        updates[name] = (bridge / name).read_text(encoding="utf-8")
    # Optional world-space interface, independent of the old V1/V2 layouts.
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], '#include "ncmm_graphics_v2.h"',
        '#include "ncmm_graphics_v2.h"\n#include "ncmm_graphics_multilevel.h"\n#include "ncmm_world_cache.hpp"\n#include "ncmm_world_lod.hpp"\n#include "overmap.h"\n#include "overmapbuffer.h"\n#include "omdata.h"\n#include "avatar_action.h"\n#include "game_inventory.h"\n#include "pickup.h"\n#include "visitable.h"\n#include "debug.h"\n#include "gamemode.h"\n#include <functional>\n#include <atomic>\n#include <thread>\n#include <unordered_map>\n#include <string_view>\n#include <locale>')
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], '    "graphics.viewport.v2",',
        '    "graphics.viewport.v2",\n    "graphics.multilevel.v1",')
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"],
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_GPU_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_MULTILEVEL_ID ) {\n'
        '        return min_major <= 1 && min_minor <= 1 ? &graphics_world_api : nullptr;\n'
        '    }\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_GPU_ID ) {')
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"],
        "bool graphics_requires_terrain_pass()\n{",
        "void graphics_note_input_result( const std::string &category, const std::string &action )\n{\n#if defined(TILES)\n"
        "    if( graphics_fire_capture.active && action != \"TIMEOUT\" ) {\n"
        "        std::ofstream journal( game_root() / \"ncmm\" / \"first-person-fire-inputs.jsonl\", std::ios::app );\n"
        "        JsonOut out( journal ); out.start_object(); out.member( \"ticks\", SDL_GetTicks() );\n"
        "        out.member( \"category\", category ); out.member( \"action\", action ); out.end_object(); journal << '\\n';\n    }\n"
        "    if( graphics_fire_capture.active && category == \"DEFAULTMODE\" && action != \"TIMEOUT\" ) {\n"
        "        graphics_fire_capture.native_command = action; ++graphics_fire_capture.native_command_sequence;\n    }\n"
        "#else\n    (void)category; (void)action;\n#endif\n}\n"
        "void graphics_note_terrain_composed( bool perspective )\n{\n#if defined(TILES)\n    if( graphics_fire_capture.active ) { graphics_fire_capture.last_perspective = perspective; graphics_fire_capture.last_gpu = perspective && graphics_gpu_frame; }\n#else\n    (void)perspective;\n#endif\n}\n"
        "void graphics_before_present()\n{\n#if defined(TILES)\n    graphics_capture_before_present();\n#endif\n}\n"
        "void graphics_world_changed( const map *source, int z )\n{\n"
        "    graphics_world_changed_impl( source, z );\n}\n"
        "void graphics_frame_presented()\n{\n#if defined(TILES)\n"
        "    if( graphics_fire_capture.active && !graphics_fire_capture.frames.empty() ) graphics_fire_capture.frames.back().presented = true;\n"
        "    graphics_world_frame_presented();\n#endif\n}\n"
        "bool graphics_requires_terrain_pass()\n{")
    updates["ncmm_loader.cpp"] = replace(updates["ncmm_loader.cpp"], '    graphics_context.note( category );',
        '    graphics_context.note( category );\n#if defined(TILES)\n'
        '    if( graphics_fire_capture.active ) graphics_fire_input_context.store( category == "DEFAULTMODE" ? 1u : category == "TARGET" ? 2u : 3u );\n#endif')
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "class input_context;", "class map;\nclass input_context;")
    updates["ncmm_loader.h"] = replace(updates["ncmm_loader.h"], "void initialize();",
        "void graphics_note_input_result( const std::string &, const std::string & );\nvoid graphics_note_terrain_composed( bool perspective );\nvoid graphics_world_changed( const map *, int z );\nvoid initialize();")
    updates["game.h"]=replace(read("game.h"), "        bool save();",
        "        bool save();\n"
        "        // Isolated qualification gateway; not part of a module API.\n"
        "        bool ncmm_graphics_smoke_handle_action() { return handle_action(); }\n"
        "        void ncmm_graphics_smoke_prepare_turns();\n"
        "        bool ncmm_graphics_smoke_reload( const save_t &name ) {\n"
        "            return load( name );\n        }\n"
        "        void ncmm_graphics_smoke_examine( const tripoint_bub_ms &p ) {\n"
        "            examine( p, false );\n        }\n"
        "        void ncmm_graphics_smoke_controls( const tripoint_bub_ms &p ) {\n"
        "            control_vehicle( p );\n        }\n")
    updates["ncmm_loader.cpp"]=replace(updates["ncmm_loader.cpp"], "} // namespace ncmm\n",
        "} // namespace ncmm\n\n"
        "// The canonical API smoke builds an avatar without game::start_game().\n"
        "// Native-turn diagnostics require its ordinary default game mode.\n"
        "void game::ncmm_graphics_smoke_prepare_turns()\n{\n"
        "    if( !gamemode ) gamemode = std::make_unique<special_game>();\n}\n")
    updates["overmapbuffer.h"]=replace(read("overmapbuffer.h"), "        bool has( const point_abs_om &p );",
        "        bool has( const point_abs_om &p );\n"
        "        // Renderer probe: memory only; never reads disk or generates maps.\n"
        "        const overmap *ncmm_graphics_loaded_overmap( const point_abs_om &p ) const {\n"
        "            const auto found = overmaps.find( p );\n"
        "            return found == overmaps.end() ? nullptr : found->second.get();\n        }\n")
    map_h=read("map.h")
    updates["map.h"]=replace(map_h, "        int supports_zlevels() const {",
        "        // Read-only loaded-submap probe; never calls load or mapgen.\n"
        "        bool ncmm_graphics_loaded( const tripoint_bub_ms &p ) const {\n"
        "            return inbounds( p ) && unsafe_get_submap_at( p ) != nullptr;\n"
        "        }\n        int supports_zlevels() const {")
    map_cpp=replace(read("map.cpp"), '#include "map.h"', '#include "map.h"\n#include "ncmm_loader.h"')
    for signature,level in (
        ("void map::set_transparency_cache_dirty( const int zlev )", "zlev"),
        ("void map::set_transparency_cache_dirty( const tripoint_bub_ms &p, bool field )", "p.z()"),
        ("void map::set_outside_cache_dirty( const int zlev )", "zlev"),
        ("void map::set_floor_cache_dirty( const int zlev )", "zlev"),
    ):
        map_cpp=replace(map_cpp, signature+"\n{", signature+"\n{\n    ncmm::graphics_world_changed( this, "+level+" );")
    updates["map.cpp"]=map_cpp
    for name in ("ncmm_graphics_context.hpp","ncmm_graphics_fire_audit.inc","ncmm_graphics_multilevel.h","ncmm_world_policy.hpp","ncmm_world_cache.hpp","ncmm_world_lod.hpp","ncmm_graphics_lod.inc","ncmm_graphics_world.inc","ncmm_graphics_multilevel_smoke.inc","ncmm_graphics_multilevel_save_smoke.inc","ncmm_graphics_gameplay_audit.inc","ncmm_graphics_movement_benchmark.inc"):
        if (root / "src" / name).exists():
            raise ValueError("Experimental source already exists: " + name)
        updates[name]=(bridge/name).read_text(encoding="utf-8")

    # No writes above this line. No save/installation/feed/catalog changes.
    for name, content in updates.items():
        (root / "src" / name).write_text(content, encoding="utf-8", newline="\n")
    evidence = {"status": "experimental-not-certified", "source_commit": identity, "tag": SOURCES[identity],
        "inputs": {name: hashlib.sha256(content.encode()).hexdigest() for name, content in updates.items()}}
    (root / "first-person-source.json").write_text(json.dumps(evidence, indent=2)+"\n", encoding="utf-8")
    print(f"Experimental graphics bridge prepared for {SOURCES[identity]} ({identity})")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_root", type=Path)
    apply(parser.parse_args().source_root.resolve())
