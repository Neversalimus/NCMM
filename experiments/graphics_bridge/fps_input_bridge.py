"""One atomic Native SDL/input-context integration for both pinned hosts."""
from pathlib import Path

def once(source,old,new):
    if source.count(old)!=1:raise ValueError("FPS input requires one exact anchor: "+old[:100])
    return source.replace(old,new)

def prepare(root,updates,bridge):
    loader=updates['ncmm_loader.cpp']
    # The existing manager helpers live in ncmm, outside the loader's anonymous
    # namespace. Forward-declare there, using the same private metadata type.
    loader=once(loader,'namespace ncmm\n{\nnamespace\n{',
        'namespace ncmm\n{\nnamespace { struct module_setting_meta; }\n'
        'static std::string manager_setting_value(const module_setting_meta &);\n'
        'static bool manager_adjust_setting(const module_setting_meta &,int);\n'
        'static bool manager_persist_settings();\n'
        'static bool manager_reset_setting(const module_setting_meta &);\n'
        'namespace\n{')
    loader=once(loader,'#include "ncmm_graphics_camera.hpp"',
        '#include "ncmm_graphics_camera.hpp"\n#include "ncmm_graphics_input.hpp"\n#include "ncmm_graphics_focus.h"\n#include "action.h"\n#if defined(TILES)\n#include "cata_imgui.h"\n#endif')
    loader=once(loader,'    if( !graphics_control_context ) graphics_release_mouse();','    graphics_fps_sync();')
    loader=once(loader,'    if( graphics_fire_capture.active ) graphics_fire_input_context.store(',
        '    graphics_fps_input_begin(category);\n    if( graphics_fire_capture.active ) graphics_fire_input_context.store(')
    loader=once(loader,'    const auto waiting = graphics_fire_capture.active ? graphics_fire_input_context.exchange( 0 ) : 0u;',
        '    graphics_fps_input_end(action);\n    const auto waiting = graphics_fire_capture.active ? graphics_fire_input_context.exchange( 0 ) : 0u;')
    loader=once(loader,'(text ? 4u : 0u) );','(text ? 4u : 0u) | (graphics_input_router.command_mode()?8u:0u) );')
    loader=once(loader,'uint64_t graphics_native_input_loop_begin()\n{\n    return graphics_snapshot_lease.begin_native_loop();\n}',
        'uint64_t graphics_native_input_loop_begin()\n{\n    graphics_fps_native_begin();\n    return graphics_snapshot_lease.begin_native_loop();\n}')
    loader=once(loader,'    return graphics_draw( x, y, width, height );',
        '    const bool result=graphics_draw(x,y,width,height);graphics_fps_rendered(result);return result;')
    loader=once(loader,'    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CONTROLS_ID ) {',
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_FOCUS_ID ) {\n'
        '        return min_major<=1&&min_minor==0?&graphics_focus_api:nullptr;\n    }\n'
        '    if( interface_id && std::string( interface_id ) == NCMM_GRAPHICS_CONTROLS_ID ) {')
    loader=loader.replace('    graphics_context.reset();','    graphics_fps_reset();\n    graphics_context.reset();')
    updates['ncmm_loader.cpp']=loader
    loader=once(loader,'std::string graphics_click_action()\n{',
        'bool graphics_route_sdl(const SDL_Event &event,const SDL_Event &display,bool allowed)\n{\n'
        '    return graphics_route_sdl_impl(event,display,allowed);\n}\n'
        'bool graphics_fps_pending()\n{\n    return graphics_fps_pending_impl();\n}\n'
        'const std::string *graphics_fps_take_action(const std::string &category)\n{\n'
        '    return graphics_fps_take_action_impl(category);\n}\n'
        'std::optional<tripoint_bub_ms> graphics_take_fps_target(const std::string &action)\n{\n'
        '    return graphics_take_fps_target_impl(action);\n}\n'
        'std::optional<tripoint_bub_ms> graphics_targeted_open(const std::optional<tripoint_bub_ms> &target)\n{\n'
        '    return graphics_targeted_open_impl(target);\n}\n'
        'std::string graphics_click_action()\n{')
    updates['ncmm_loader.cpp']=loader
    header=updates['ncmm_loader.h']
    header=once(header,'#include <cstdint>','#include <cstdint>\n#include <optional>\n#include "coordinates.h"\nunion SDL_Event;')
    header=once(header,'void initialize();',
        'bool graphics_route_sdl(const SDL_Event &,const SDL_Event &,bool);\n'
        'bool graphics_fps_pending();\n'
        'const std::string *graphics_fps_take_action(const std::string &);\n'
        'std::optional<tripoint_bub_ms> graphics_take_fps_target(const std::string &);\n'
        'std::optional<tripoint_bub_ms> graphics_targeted_open(const std::optional<tripoint_bub_ms> &);\n'
        'void initialize();')
    updates['ncmm_loader.h']=header
    context=updates['input_context.cpp']
    context=once(context,'        if( next_action.type == input_event_t::timeout ) {',
        '        if( next_action.type == input_event_t::timeout ) {\n'
        '            if(const auto *fps=ncmm::graphics_fps_take_action(category)){result=fps;break;}')
    updates['input_context.cpp']=context
    sdl=updates['sdltiles.cpp']
    legacy='''        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );
        const bool ncmm_mouse_allowed = !cataimgui::client::want_capture_mouse();
        if( ev.type == SDL_EVENT_WINDOW_FOCUS_LOST ) ncmm::graphics_mouse_event( 4, 0, 0, 0, 0, false );
        if( ev.type == SDL_EVENT_MOUSE_MOTION && ncmm::graphics_mouse_event( 3, ev_display.motion.x, ev_display.motion.y, ev.motion.xrel, ev.motion.yrel, ncmm_mouse_allowed ) ) continue;
        if( ( ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP ) && ev.button.button == SDL_BUTTON_RIGHT &&
            ncmm::graphics_mouse_event( ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 1u : 2u, ev_display.button.x, ev_display.button.y, 0, 0, ncmm_mouse_allowed ) ) continue;'''
    sdl=once(sdl,legacy,'''        // One owner before the event reaches ImGui or Native key translation.
        if(ncmm::graphics_route_sdl(ev,ev_display,!cataimgui::client::want_capture_mouse()&&!cataimgui::client::want_capture_keyboard()))continue;
        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );''')
    sdl=once(sdl,'    if( needupdate ) {\n        try_sdl_update();',
        '    // Drain the existing SDL batch before resolving held chords. A\n'
        '    // Native key (F6, menu, etc.) wins over our synthetic wakeup.\n'
        '    if(last_input.type==input_event_t::error&&ncmm::graphics_fps_pending())last_input.type=input_event_t::timeout;\n'
        '    if( needupdate ) {\n        try_sdl_update();')
    updates['sdltiles.cpp']=sdl
    action=updates['handle_action.cpp']
    # The SDL router is now the sole FPS mouse owner. Retaining the old late
    # SELECT-to-fire adapter would reinterpret Native Command Mode clicks.
    action=once(action,
        '        if( act == ACTION_SELECT && ncmm::graphics_requires_terrain_pass() ) {\n'
        '            const std::string aimed = ncmm::graphics_click_action();\n'
        '            if( aimed.empty() ) return false;\n'
        '            act = look_up_action( aimed );\n'
        '        }\n','')
    action=once(action,'        if( act == ACTION_INTERACT ) {',
        '        if(const auto fps_target=ncmm::graphics_take_fps_target(action))mouse_target=fps_target;\n'
        '        if( act == ACTION_INTERACT ) {')
    action=once(action,'player_character.assign_activity( open_tile_activity_actor( std::nullopt ) );',
        'player_character.assign_activity( open_tile_activity_actor( ncmm::graphics_targeted_open(mouse_target) ) );')
    updates['handle_action.cpp']=action
    for name in ('ncmm_graphics_input.hpp','ncmm_graphics_focus.h','ncmm_graphics_fps.inc','ncmm_graphics_fps_audit.inc'):
        if (root/'src'/name).exists():raise ValueError('FPS experimental source already exists: '+name)
        updates[name]=(bridge/name).read_text(encoding='utf-8')
