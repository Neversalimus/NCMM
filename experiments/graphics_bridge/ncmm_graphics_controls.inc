// Host-owned input and presentation state; this extension never moves an avatar.
float graphics_camera_yaw = -ncmm_graphics_camera::pi * .5f;
float graphics_camera_pitch = 0;
bool &graphics_control_context = graphics_context.controls;
bool graphics_mouse_drag = false, graphics_mouse_dirty = false;
bool graphics_attack_pending = false;
bool graphics_frame_pending_present = false;
bool graphics_cursor_was_visible = true;
float graphics_mouse_dx = 0, graphics_mouse_dy = 0;
float graphics_deferred_dx = 0, graphics_deferred_dy = 0;
uint64_t graphics_attack_serial = 0, graphics_last_redraw = 0;
uint32_t graphics_attack_kind = 0;
double graphics_attack_started = 0;
struct { int x=0,y=0,w=0,h=0; } graphics_viewport;

extern bool graphics_click_pending;
void graphics_cancel_animation()
{
    graphics_attack_pending = false; graphics_frame_pending_present=false;graphics_attack_started = -1;graphics_click_pending=false;
}

bool graphics_apply_mouse_capture(bool capture)
{
#if defined(TILES)
    if(capture==graphics_mouse_drag)return true;
    if(capture) {
        graphics_cursor_was_visible=SDL_CursorVisible();
        if(!SDL_SetWindowRelativeMouseMode(get_sdl_window(),true))return false;
        SDL_HideCursor();
    } else {
        SDL_SetWindowRelativeMouseMode(get_sdl_window(),false);
        if(graphics_cursor_was_visible)SDL_ShowCursor();
    }
#endif
    graphics_mouse_drag=capture;graphics_mouse_dx=graphics_mouse_dy=0;graphics_deferred_dx=graphics_deferred_dy=0;return true;
}
void graphics_release_mouse()
{
    graphics_input_router.release();
    graphics_apply_mouse_capture(false);
}

int graphics_set_camera( float yaw, float pitch )
{
    if( active_module_id != graphics_view.owner || graphics_view.owner.empty() ||
        !std::isfinite( yaw ) || !std::isfinite( pitch ) || std::abs( pitch ) > 1.35f ) return 0;
    graphics_camera_yaw = std::remainder( yaw, 2 * ncmm_graphics_camera::pi );
    graphics_camera_pitch = pitch;
    return 1;
}

int graphics_get_controls( ncmm_graphics_control_state_v1 *out )
{
    if( active_module_id != graphics_view.owner || graphics_view.owner.empty() || !out ||
        out->struct_size < sizeof( *out ) ) return 0;
    double attack_start = graphics_attack_started;
#if defined(TILES)
    // Simulation can take longer than a clip. Start presentation on the first
    // renderer sample. A successful SDL window presentation commits the
    // clock, so neither a slow turn nor a slow initial raster consumes the clip.
    if( graphics_attack_pending && graphics_render_active && graphics_view.enabled ) {
        attack_start = SDL_GetTicks() / 1000.0;
    }
#endif
    *out = { sizeof( *out ), graphics_attack_kind, NCMM_GRIP_SINGLE, graphics_mouse_drag ? 1u : 0u,
        graphics_attack_serial, attack_start, graphics_mouse_dx, graphics_mouse_dy, .6f };
#if defined(TILES)
    if( character_state_available() ) if( const auto held = get_avatar().get_wielded_item() ) {
        out->held_length = std::clamp( float( units::to_millimeter( held->length() ) ) / 1000, .15f, 2.7f );
        out->grip_kind = held->is_gun() ? NCMM_GRIP_GUN :
            held->reach_range( get_avatar() ).first > 1 ||
            ( held->is_two_handed( get_avatar() ) && out->held_length > .8f ) ? NCMM_GRIP_POLE : NCMM_GRIP_SINGLE;
    }
#endif
    return 1;
}
const ncmm_host_graphics_controls_v1 graphics_controls_api = { 1, sizeof( ncmm_host_graphics_controls_v1 ),
    1, 0, graphics_get_controls, graphics_set_camera };

// Independent of the 2D light/visibility mask and of opaque vehicle parts.
// Terrain/furniture walls still block the forward scene; 3D car geometry owns
// car occlusion, so a windscreen never erases the remainder of its own roof.
bool graphics_forward_visible( const tripoint_bub_ms &origin, const tripoint_bub_ms &target,
                               const std::vector<uint8_t> *transparency = nullptr, int radius = 0 )
{
#if defined(TILES)
    const float dx = float( target.x() - origin.x() ), dy = float( target.y() - origin.y() );
    if( !ncmm_graphics_camera::in_front( graphics_camera_yaw, dx, dy ) ) return false;
    map &here = get_map();
    if( origin.z() != target.z() || !here.inbounds( origin ) || !here.inbounds( target ) ) return false;
    bool clear = true;
    const int side = radius * 2 + 1;
    // Use the same engine Bresenham path as line_to(), stopping at the first
    // blocker without allocating a vector for every distant target. Within a
    // draw the map is unchanged; a byte grid avoids repeated terrain/furniture
    // lookups along thousands of overlapping rays. Standalone callers still
    // consult the live map. The target surface itself remains drawable.
    bresenham( origin, target, 0, 0, [&]( const tripoint_bub_ms &p ) {
        if( p == target || p == origin ) return true;
        if( transparency ) {
            const int x = p.x() - origin.x() + radius, y = p.y() - origin.y() + radius;
            clear = x >= 0 && y >= 0 && x < side && y < side &&
                    size_t( y * side + x ) < transparency->size() && ( *transparency )[y * side + x];
        } else {
            clear = here.inbounds( p ) && here.ter( p ).obj().transparent &&
                    ( !here.has_furn( p ) || here.furn( p ).obj().transparent );
        }
        return clear;
    } );
    return clear;
#else
    ( void )origin; ( void )target; ( void )transparency; ( void )radius; return false;
#endif
}

std::string graphics_map_relative_action( const std::string &action, const input_event &event, bool vehicle_control )
{
    if( !graphics_view.enabled || !graphics_control_context || vehicle_control || !graphics_fps_enabled || graphics_input_router.command_mode() ||
        event.modifiers.size() || ( event.type != input_event_t::keyboard_code && event.type != input_event_t::keyboard_char ) ) return action;
    int forward = 0, right = 0;
    const int key = event.get_first_input();
    if( key == inp_mngr.get_keycode( event.type, "UP" ) && action == "UP" ) forward = 1;
    else if( key == inp_mngr.get_keycode( event.type, "DOWN" ) && action == "DOWN" ) forward = -1;
    else if( key == inp_mngr.get_keycode( event.type, "LEFT" ) && action == "LEFT" ) right = -1;
    else if( key == inp_mngr.get_keycode( event.type, "RIGHT" ) && action == "RIGHT" ) right = 1;
    else return action;
    const auto delta = ncmm_graphics_camera::movement( graphics_camera_yaw, forward, right );
    return action_ident(get_movement_action_from_delta(tripoint_rel_ms(delta.first,delta.second,0),iso_rotate::yes));
}

void graphics_notify_attack( uint32_t kind )
{
#if defined(TILES)
    if( !graphics_view.enabled ) return;
    ++graphics_attack_serial; graphics_attack_kind = kind;
    graphics_attack_started = SDL_GetTicks() / 1000.0;
    graphics_attack_pending = true;
    graphics_mouse_dirty = true;
    if( g ) g->invalidate_main_ui_adaptor();
#else
    ( void )kind;
#endif
}

bool graphics_process_mouse( uint32_t kind, float x, float y, float dx, float dy, bool allowed )
{
#if defined(TILES)
    if( kind == 4 || !allowed || !graphics_view.enabled || !graphics_control_context ) {
        graphics_release_mouse(); return false;
    }
    if( kind == 1 ) {
        if( x < graphics_viewport.x || y < graphics_viewport.y || x >= graphics_viewport.x + graphics_viewport.w ||
            y >= graphics_viewport.y + graphics_viewport.h ) return false;
        return graphics_apply_mouse_capture(true);
    }
    if( kind == 2 ) { const bool was_dragging = graphics_mouse_drag; graphics_release_mouse(); return was_dragging; }
    if( kind == 3 && graphics_mouse_drag && std::isfinite( dx ) && std::isfinite( dy ) ) {
        // SDL can be pumped during presentation. Never re-enter a module's
        // input callback while its render callback owns borrowed scene data.
        if(graphics_render_active){
            graphics_deferred_dx=std::clamp(graphics_deferred_dx+dx,-32000.f,32000.f);
            graphics_deferred_dy=std::clamp(graphics_deferred_dy+dy,-32000.f,32000.f);
            return true;
        }
        const float previous_yaw=graphics_camera_yaw;
        graphics_mouse_dx = std::clamp( dx, -2000.0f, 2000.0f )*float(graphics_fps_sensitivity);
        graphics_mouse_dy = std::clamp( dy, -2000.0f, 2000.0f )*float(graphics_fps_sensitivity)*(graphics_fps_invert?-1.f:1.f);
        graphics_action( "ncmm.view.mouse_look" ); graphics_mouse_dx = graphics_mouse_dy = 0;
        if(graphics_camera_input_probe.active){++graphics_camera_input_probe.mouse_events;
            graphics_camera_input_probe.rotation_radians+=std::abs(std::remainder(graphics_camera_yaw-previous_yaw,2*ncmm_graphics_camera::pi));}
        graphics_mouse_dirty = true; return true;
    }
#else
    ( void )kind; ( void )x; ( void )y; ( void )dx; ( void )dy; ( void )allowed;
#endif
    return false;
}

bool graphics_redraw_due()
{
#if defined(TILES)
    if( !graphics_view.enabled || !graphics_control_context || !character_state_available() ) return false;
    const uint64_t now = SDL_GetTicks();
    const double age = now / 1000.0 - graphics_attack_started;
    // One recovery frame after the last keyframe restores the idle pose.
    const bool attack = graphics_attack_serial && age >= 0 && age < .53;
    // Slow ambient animation while idle; simulation time still never advances.
    if( now - graphics_last_redraw < ( graphics_mouse_dirty || attack || graphics_world_needs_redraw() ? 16u : 90u ) ) return false;
    graphics_last_redraw = now; graphics_mouse_dirty = false;
    if( g ) g->invalidate_main_ui_adaptor();
    return true;
#endif
    return false;
}
