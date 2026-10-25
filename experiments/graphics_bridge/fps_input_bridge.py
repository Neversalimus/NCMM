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
        'namespace ncmm\n{\nvoid graphics_fps_poll();\nnamespace { struct module_setting_meta; }\n'
        'static std::string manager_setting_value(const module_setting_meta &);\n'
        'static bool manager_adjust_setting(const module_setting_meta &,int);\n'
        'static bool manager_persist_settings();\n'
        'static bool manager_reset_setting(const module_setting_meta &);\n'
        'namespace\n{')
    loader=once(loader,'#include "ncmm_graphics_camera.hpp"',
        '#include "ncmm_graphics_camera.hpp"\n#include "ncmm_graphics_grass.hpp"\n#include "ncmm_graphics_input.hpp"\n#include "ncmm_graphics_focus.h"\n#include "action.h"\n#include "gun_mode.h"\n#if defined(TILES)\n#include "cata_imgui.h"\n#endif')
    loader=once(loader,'    if( !graphics_control_context ) graphics_release_mouse();','    graphics_fps_sync();')
    loader=once(loader,'    if( graphics_fire_capture.active ) graphics_fire_input_context.store( (category == "DEFAULTMODE" ? 1u : category == "TARGET" ? 2u : 3u) | (text ? 4u : 0u) );',
        '    graphics_fps_input_begin(category);\n'
        '    graphics_fire_wait_pending=graphics_fire_capture.active?((category == "DEFAULTMODE" ? 1u : category == "TARGET" ? 2u : 3u) | (text ? 4u : 0u)):0u;\n'
        '    if(graphics_fire_capture.active)graphics_fire_input_context.store(0);')
    loader=once(loader,'    const auto waiting = graphics_fire_capture.active ? graphics_fire_input_context.exchange( 0 ) : 0u;',
        '    graphics_fps_input_end(action);\n    const auto waiting = graphics_fire_capture.active ? graphics_fire_input_context.exchange( 0 ) : 0u;')
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
        'bool graphics_cursor_captured()\n{\n    return graphics_mouse_drag;\n}\n'
        'bool graphics_take_reach_target(std::optional<tripoint_bub_ms> &target)\n{\n'
        '    return graphics_take_reach_target_impl(target);\n}\n'
        'bool graphics_take_fps_gunshot(std::optional<tripoint_bub_ms> &target,int &aim_moves)\n{\n'
        '    return graphics_take_fps_gunshot_impl(target,aim_moves);\n}\n'
        'void graphics_fps_poll()\n{\n    graphics_fps_poll_impl();\n}\n'
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
        'bool graphics_cursor_captured();\n'
        'bool graphics_take_reach_target(std::optional<tripoint_bub_ms> &);\n'
        'bool graphics_take_fps_gunshot(std::optional<tripoint_bub_ms> &,int &);\n'
        'bool graphics_route_sdl(const SDL_Event &,const SDL_Event &,bool);\n'
        'bool graphics_fps_pending();\n'
        'const std::string *graphics_fps_take_action(const std::string &);\n'
        'std::optional<tripoint_bub_ms> graphics_take_fps_target(const std::string &);\n'
        'std::optional<tripoint_bub_ms> graphics_targeted_open(const std::optional<tripoint_bub_ms> &);\n'
        'void initialize();')
    header=once(header,'void initialize();', '''// Legacy Native any-key windows do not create input_context::handle_input.
// Their existing wait still needs the same modal ownership and input lease.
class graphics_raw_input_scope {
    graphics_input_wait_scope wait_{"NCMM_RAW_INPUT"};
public:
    explicit graphics_raw_input_scope(bool text) {
        graphics_note_input_context("NCMM_RAW_INPUT");
        graphics_note_input_wait("NCMM_RAW_INPUT", text);
    }
    ~graphics_raw_input_scope() {
        graphics_note_input_result("NCMM_RAW_INPUT", "ANY_INPUT");
        wait_.completed("ANY_INPUT");
    }
    graphics_raw_input_scope(const graphics_raw_input_scope &) = delete;
    graphics_raw_input_scope &operator=(const graphics_raw_input_scope &) = delete;
};
void initialize();''')
    updates['ncmm_loader.h']=header
    native_input=updates['input.cpp'] if 'input.cpp' in updates else (root/'src/input.cpp').read_text(encoding='utf-8')
    if '#include "ncmm_loader.h"' not in native_input:
        native_input=once(native_input,'#include "input.h"',
                          '#include "input.h"\n#include "ncmm_loader.h"')
    native_input=once(native_input,'void input_manager::wait_for_any_key()\n{',
                      'void input_manager::wait_for_any_key()\n{\n'
                      '    ncmm::graphics_raw_input_scope ncmm_raw_input(\n'
                      '        actual_keyboard_mode(keyboard_mode::keycode) == keyboard_mode::keychar);')
    updates['input.cpp']=native_input
    native_debug=updates['debug.cpp'] if 'debug.cpp' in updates else (root/'src/debug.cpp').read_text(encoding='utf-8')
    if '#include "ncmm_loader.h"' not in native_debug:
        native_debug=once(native_debug,'#include "debug.h"',
                          '#include "debug.h"\n#include "ncmm_loader.h"')
    native_debug=once(native_debug,'    for( bool stop = false; !stop; ) {',
                      '    ncmm::graphics_raw_input_scope ncmm_raw_input(\n'
                      '        input_manager::actual_keyboard_mode(keyboard_mode::keycode) == keyboard_mode::keychar);\n'
                      '    for( bool stop = false; !stop; ) {')
    updates['debug.cpp']=native_debug
    context=updates['input_context.cpp']
    context=once(context,'        if( next_action.type == input_event_t::timeout ) {',
        '        if( next_action.type == input_event_t::timeout ) {\n'
        '            if(const auto *fps=ncmm::graphics_fps_take_action(category)){result=fps;break;}')
    updates['input_context.cpp']=context
    sdl=updates['sdltiles.cpp']
    # A private diagnostic acknowledgement after Native's pre-poll redraw,
    # without another shared-header dependency or another SDL loop.
    sdl='namespace ncmm { void graphics_fps_poll(); }\n'+sdl
    poll_anchor='    } else if( ui_adaptor::has_imgui() ) {\n        try_sdl_update();\n    }'
    sdl=once(sdl,poll_anchor,poll_anchor+'\n    ncmm::graphics_fps_poll();')
    legacy='''        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );
        const bool ncmm_mouse_allowed = !cataimgui::client::want_capture_mouse();
        if( ev.type == SDL_EVENT_WINDOW_FOCUS_LOST ) ncmm::graphics_mouse_event( 4, 0, 0, 0, 0, false );
        if( ev.type == SDL_EVENT_MOUSE_MOTION && ncmm::graphics_mouse_event( 3, ev_display.motion.x, ev_display.motion.y, ev.motion.xrel, ev.motion.yrel, ncmm_mouse_allowed ) ) continue;
        if( ( ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP ) && ev.button.button == SDL_BUTTON_RIGHT &&
            ncmm::graphics_mouse_event( ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? 1u : 2u, ev_display.button.x, ev_display.button.y, 0, 0, ncmm_mouse_allowed ) ) continue;'''
    sdl=once(sdl,legacy,'''        // One owner before the event reaches ImGui or Native key translation.
        if(ncmm::graphics_route_sdl(ev,ev_display,!cataimgui::client::want_capture_mouse()&&!cataimgui::client::want_capture_keyboard()))continue;
        imclient->process_input( &ev_display, imgui_buf_w, imgui_buf_h, scaling_factor );''')
    sdl=once(sdl,'    ImGuiIO &io = ImGui::GetIO();\n    if( !mouse.enabled ) {',
        '    ImGuiIO &io = ImGui::GetIO();\n'
        '    if(ncmm::graphics_cursor_captured()) {\n'
        '        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;\n'
        '        HideCursor();\n        return;\n    }\n'
        '    if( !mouse.enabled ) {')
    sdl=once(sdl,'    if( needupdate ) {\n        try_sdl_update();',
        '    // Drain the existing SDL batch before resolving held chords. A\n'
        '    // Native key (F6, menu, etc.) wins over our synthetic wakeup.\n'
        '    if(last_input.type==input_event_t::error&&ncmm::graphics_fps_pending())last_input.type=input_event_t::timeout;\n'
        '    if( needupdate ) {\n        try_sdl_update();')
    updates['sdltiles.cpp']=sdl
    action=updates['handle_action.cpp']
    action=once(action,'static void reach_attack( avatar &you )\n{\n',
        'static void reach_attack( avatar &you )\n{\n'
        '    std::optional<tripoint_bub_ms> fps_target;\n'
        '    if(ncmm::graphics_take_reach_target(fps_target)) {\n'
        '        if(fps_target)you.reach_attack(*fps_target);\n'
        '        return;\n    }\n')
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
    firing=updates.get('avatar_action.cpp')
    if firing is None:firing=(root/'src/avatar_action.cpp').read_text(encoding='utf-8')
    if '#include "ncmm_loader.h"' not in firing:
        firing=once(firing,'#include "avatar_action.h"','#include "avatar_action.h"\n#include "ncmm_loader.h"')
    firing=once(firing,'    you.assign_activity( aim_activity_actor::use_wielded() );', '''    // A consumed FPS request never falls through to a Native target selector,
    // including stale focus, empty weapons and AIM_AFTER_FIRING. Native F/Tab
    // still uses the ordinary activity below.
    std::optional<tripoint_bub_ms> fps_target;
    int fps_aim_moves = 0;
    if( ncmm::graphics_take_fps_gunshot( fps_target, fps_aim_moves ) ) {
        if( !fps_target ) return;
        map &here = get_map();
        gun_mode mode = weapon->gun_current_mode();
        if( !mode || mode.melee() ) return;
        std::vector<std::string> reasons;
        const bool common = gunmode_checks_common( you, here, reasons, mode );
        const bool ready = gunmode_checks_weapon( you, here, reasons, mode );
        if( !common || !ready ) {
            for( const auto &reason : reasons ) add_msg( m_info, reason );
            return;
        }
        item_location reload_ammo;
        if( mode->has_flag( flag_id( "RELOAD_AND_SHOOT" ) ) && !mode->ammo_remaining() ) {
            // Use Native reachability/ammunition enumeration, never its
            // selection UI. Reuse the player's valid choice when possible.
            if( you.ammo_location && square_dist( you.pos_abs(), you.ammo_location.pos_abs() ) <= 1 &&
                mode->can_reload_with( *you.ammo_location, false ) ) reload_ammo = you.ammo_location;
            else {
                const item_location used_gun = &*mode != &*weapon ? item_location( weapon, &*mode ) : weapon;
                std::vector<item::reload_option> ammunition;
                you.list_ammo( used_gun, ammunition, false );
                for( const auto &option : ammunition ) if( option ) { reload_ammo = option.ammo; break; }
            }
        }
        if( ( !mode->ammo_sufficient( &you ) && !reload_ammo ) || !can_fire_weapon( you, here, *weapon ) ) return;
        const Target_attributes attributes( you.pos_bub(), *fps_target );
        const aim_mods_cache aim_cache = you.gen_aim_mods_cache( *mode );
        for( int move = 0; move < fps_aim_moves && you.recoil > 0; ++move ) {
            const double amount = you.aim_per_move( *mode, you.recoil, attributes, aim_cache );
            if( amount <= 0 ) break;
            you.recoil = std::max( 0.0, you.recoil - amount );
            you.mod_moves( -1 );
        }
        // Native performs the RAS load, reload cost and stamina use itself.
        you.fire_gun( here, *fps_target, mode.qty, *mode, reload_ammo );
        return;
    }
    you.assign_activity( aim_activity_actor::use_wielded() );''')
    updates['avatar_action.cpp']=firing
    for name in ('ncmm_graphics_input.hpp','ncmm_graphics_grass.hpp','ncmm_graphics_focus.h','ncmm_graphics_fps.inc','ncmm_graphics_fps_audit.inc'):
        if (root/'src'/name).exists():raise ValueError('FPS experimental source already exists: '+name)
        updates[name]=(bridge/name).read_text(encoding='utf-8')
