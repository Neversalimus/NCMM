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
    loader = replace(loader, '#include "ncmm_loader.h"', '#include "ncmm_loader.h"\n#include "ncmm_graphics.h"\n#include "ncmm_graphics_policy.hpp"\n#if defined(TILES)\n#include "cata_tiles.h"\n#include "sdltiles.h"\n#include "monster.h"\n#include "mtype.h"\n#include "creature_tracker.h"\n#include "lightmap.h"\n#include "map_memory.h"\n#endif')
    loader = replace(loader, "struct loaded_mod {", "void graphics_erase( const std::string &id );\nstruct loaded_mod {")
    loader = replace(loader, '    "core.v1",', '#if defined(TILES)\n    "graphics.viewport.v1",\n#endif\n    "core.v1",')
    loader = replace(loader, "const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )\n{",
        '#include "ncmm_graphics_bridge.inc"\n\nconst void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )\n{\n#if defined(TILES)\n    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_ID ) {\n        return min_major <= 1 && min_minor == 0 ? &graphics_api : nullptr;\n    }\n#endif')
    loader = replace(loader, "void clear_module_runtime_v2( const std::string &module_id )\n{",
        "void clear_module_runtime_v2( const std::string &module_id )\n{\n    graphics_erase( module_id );")
    loader = replace(loader, "    mod.fault.quarantine( kind );", "    graphics_erase( module_id );\n    mod.fault.quarantine( kind );")
    loader = replace(loader, "    if( !id.empty() ) {\n        erase_module_modifiers( id );",
        "    if( !id.empty() ) {\n        graphics_erase( id );\n        erase_module_modifiers( id );")
    loader = replace(loader, "void register_gameplay_actions( input_context &ctxt )\n{",
        "void register_gameplay_actions( input_context &ctxt )\n{\n    graphics_register_actions( ctxt );")
    loader = replace(loader, "bool handle_gameplay_action( const std::string &action )\n{",
        "bool handle_gameplay_action( const std::string &action )\n{\n    if( graphics_action( action ) ) return true;")
    loader = replace(loader, "void reset_world_lifecycle()\n{", "void reset_world_lifecycle()\n{\n    graphics_view.enabled = false;\n    graphics_gameplay_context = false;")
    loader = replace(loader, "void shutdown()\n{", "void shutdown()\n{\n    graphics_view = {};\n    graphics_frame = nullptr;\n    graphics_commands.clear();")
    loader = replace(loader, '        write_gameplay_smoke_result( true, "ok", aws_setting_count,',
        '        if( module_ids.count( "first_person_view" ) && !graphics_scene_smoke() ) {\n            write_gameplay_smoke_result( false, "first_person_graphics_failed", aws_setting_count, aws_hook_count, survivor_perk_count );\n            return 191;\n        }\n\n        write_gameplay_smoke_result( true, "ok", aws_setting_count,')
    loader = replace(loader, "void gameplay_metric_record_completed_craft( const Character &who )\n{",
        "void graphics_note_input_context( const std::string &category )\n{\n    const bool gameplay = category == \"DEFAULTMODE\";\n    if( gameplay != graphics_gameplay_context && g ) g->invalidate_main_ui_adaptor();\n    graphics_gameplay_context = gameplay;\n}\nbool graphics_preserves_destination( const std::string &action )\n{\n    return !graphics_view.owner.empty() && ( action == \"ncmm.open.\" + graphics_view.owner ||\n        action == \"ncmm.view.turn_left\" || action == \"ncmm.view.turn_right\" );\n}\nbool graphics_requires_terrain_pass()\n{\n    return graphics_view.enabled && graphics_gameplay_context;\n}\nbool draw_graphics_view( int x, int y, int width, int height )\n{\n    return graphics_draw( x, y, width, height );\n}\n\nvoid gameplay_metric_record_completed_craft( const Character &who )\n{")
    updates["ncmm_loader.cpp"] = loader
    header = read("ncmm_loader.h")
    updates["ncmm_loader.h"] = replace(header, "void initialize();", "void graphics_note_input_context( const std::string &category );\nbool graphics_preserves_destination( const std::string &action );\nbool graphics_requires_terrain_pass();\nbool draw_graphics_view( int x, int y, int width, int height );\nvoid initialize();")
    action = read("handle_action.cpp")
    action_anchor = "    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        player_character.clear_destination();\n        destination_preview.clear();\n        return false;\n    }"
    updates["handle_action.cpp"] = replace(action, action_anchor,
        "    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {\n        if( !ncmm::graphics_preserves_destination( action ) ) {\n            player_character.clear_destination();\n            destination_preview.clear();\n        }\n        return false;\n    }")
    context = read("input_context.cpp")
    context = replace(context, '#include "input_context.h"', '#include "input_context.h"\n#include "ncmm_loader.h"')
    updates["input_context.cpp"] = replace(context, "const std::string &input_context::handle_input( const int timeout )\n{",
        "const std::string &input_context::handle_input( const int timeout )\n{\n    ncmm::graphics_note_input_context( category );")
    sdl = read("sdltiles.cpp")
    anchor = "        tilecontext->draw(\n            point( win->pos.x * fontwidth, win->pos.y * fontheight ),\n            g->ter_view_p,\n            TERRAIN_WINDOW_TERM_WIDTH * font->width,\n            TERRAIN_WINDOW_TERM_HEIGHT * font->height,\n            overlay_strings,\n            color_blocks );"
    updates["sdltiles.cpp"] = replace(sdl, anchor,
        "        // Keep vanilla visibility, animation and map-memory updates before the perspective overlay.\n"+
        "        if( ncmm::graphics_requires_terrain_pass() ) {\n"+anchor+"\n        }\n"+
        "        if( !ncmm::draw_graphics_view( win->pos.x * fontwidth, win->pos.y * fontheight,\n"+
        "                TERRAIN_WINDOW_TERM_WIDTH * font->width, TERRAIN_WINDOW_TERM_HEIGHT * font->height ) ) {\n"+
        anchor+"\n        }")
    for name in ("ncmm_graphics.h", "ncmm_graphics_policy.hpp", "ncmm_graphics_bridge.inc", "ncmm_graphics_smoke.inc"):
        if (root / "src" / name).exists():
            raise ValueError("Experimental source already exists: " + name)
        updates[name] = (bridge / name).read_text(encoding="utf-8")
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
