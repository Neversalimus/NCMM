bool graphics_scene_smoke()
{
#if defined(TILES)
    size_t checks = 0;
    size_t frames = 0;
    std::string reason = "ok";
    auto require = [&]( bool ok, const char *failure ) {
        ++checks;
        if( !ok ) throw std::runtime_error( failure );
    };
    try {
        require( graphics_view.owner == "first_person_view" && graphics_view.render,
                 "graphics_provider_missing" );
        require( tilecontext && tilecontext->is_valid(), "tileset_unavailable" );
        map &here = get_map();
        here.load( tripoint_abs_sm( 0, 0, 0 ), true );
        for( const auto &vehicle : here.get_vehicles() ) here.destroy_vehicle( vehicle.v );
        avatar &you = get_avatar();
        const tripoint_bub_ms center( 60, 60, 0 );
        for( int y = -8; y <= 8; ++y ) for( int x = -8; x <= 8; ++x ) {
            const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
            here.furn_set( p, furn_id( "f_null" ) );
            here.ter_set( p, ter_id( std::abs( x ) == 4 || std::abs( y ) == 4 ? "t_wall" : "t_floor" ) );
        }
        const tripoint_bub_ms door( center.x() - 1, center.y() - 4, 0 );
        here.ter_set( door, ter_id( "t_door_c" ) );
        here.furn_set( tripoint_bub_ms( center.x() + 1, center.y() - 2, 0 ), furn_id( "f_chair" ) );
        you.setpos( here, center, false );
        if( !you.has_trait( trait_id( "DEBUG_NIGHTVISION" ) ) ) you.toggle_trait( trait_id( "DEBUG_NIGHTVISION" ) );
        g->ter_view_p = center;
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        const auto start_pos = you.pos_bub();
        const int start_moves = you.get_moves();
        const time_point start_time = calendar::turn;
        input_context gameplay( "DEFAULTMODE", keyboard_mode::keycode );
        ncmm::register_gameplay_actions( gameplay );
        for( const input_event_t type : { input_event_t::keyboard_code, input_event_t::keyboard_char } ) {
            require( gameplay.input_to_action( input_event( inp_mngr.get_keycode( type, "F6" ), type ) ) ==
                     "ncmm.open.first_person_view", "f6_default_binding_missing" );
        }
        auto press_f6 = [&]() {
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.windowID = SDL_GetWindowID( get_sdl_window() );
            event.key.key = SDLK_F6;
            event.key.scancode = SDL_SCANCODE_F6;
            require( SDL_PushEvent( &event ), "f6_sdl_event_not_queued" );
            const std::string action = gameplay.handle_input( 2000 );
            require( action == "ncmm.open.first_person_view", "f6_sdl_dispatch_failed" );
            require( ncmm::handle_gameplay_action( action ), "f6_gameplay_action_missing" );
        };
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        require( !graphics_view.enabled, "view_not_initially_disabled" );
        press_f6();
        require( graphics_view.enabled, "view_did_not_enable" );
        auto draw = [&]() {
            std::multimap<point, formatted_text> overlays;
            color_block_overlay_container blocks;
            // The real viewport hook keeps this pass before drawing first person.
            here.update_visibility_cache( 0 );
            tilecontext->draw( point::zero, center, 640, 480, overlays, blocks );
            require( graphics_draw( 0, 0, 640, 480 ), "real_sdl_render_failed" );
            require( graphics_resolved_textures >= 2, "atlas_textures_missing" );
            require( SDL_FlushRenderer( get_sdl_renderer().get() ), "sdl_frame_flush_failed" );
            ++frames;
        };
        draw();
        require( you.get_memorized_tile( here.get_abs( door ) ).get_ter_id() == "t_door_c",
                 "closed_door_not_memorized" );
        SDL_Renderer *renderer = get_sdl_renderer().get();
        const SDL_Rect capture = { 0, 0, 640, 480 };
        SDL_Surface *surface = SDL_RenderReadPixels( renderer, &capture );
        require( surface != nullptr, "frame_capture_failed" );
        const bool saved = SDL_SaveBMP( surface, ( game_root() / "ncmm" / "first-person-room.bmp" ).string().c_str() );
        SDL_DestroySurface( surface );
        require( saved, "frame_capture_write_failed" );
        for( int i = 0; i < 24; ++i ) {
            require( ncmm::handle_gameplay_action( "ncmm.view.turn_left" ), "left_action_missing" );
            draw();
        }
        for( int i = 0; i < 24; ++i ) {
            require( ncmm::handle_gameplay_action( "ncmm.view.turn_right" ), "right_action_missing" );
            draw();
        }
        require( you.pos_bub() == start_pos && you.get_moves() == start_moves && calendar::turn == start_time,
                 "camera_changed_gameplay_state" );
        ncmm::graphics_note_input_context( "INVENTORY" );
        require( !graphics_draw( 0, 0, 640, 480 ) && graphics_view.enabled, "inventory_did_not_suspend_view" );
        ncmm::graphics_note_input_context( "TARGETING" );
        require( !graphics_draw( 0, 0, 640, 480 ), "targeting_did_not_suspend_view" );
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        draw();
        require( here.open_door( you, door, true ), "real_door_did_not_open" );
        require( here.ter( door ).id().str() == "t_door_o", "door_terrain_not_updated" );
        here.build_map_cache( 0 );
        draw();
        require( you.get_memorized_tile( here.get_abs( door ) ).get_ter_id() == "t_door_o",
                 "open_door_memory_not_updated" );
        // Build a street with a real car, like the player's CatLauncher screenshot.
        for( int y = -16; y <= 16; ++y ) for( int x = -16; x <= 16; ++x ) {
            const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
            here.furn_set( p, furn_id( "f_null" ) );
            here.ter_set( p, ter_id( "t_pavement" ) );
        }
        vehicle *car = here.add_vehicle( vproto_id( "car" ),
            tripoint_bub_ms( center.x(), center.y() - 6, 0 ), 0_degrees, 0, veh_spawn_status::UNDAMAGED, false );
        require( car != nullptr, "street_vehicle_not_created" );
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        // A shifted normal-map camera must not silently cancel first person.
        g->ter_view_p = tripoint_bub_ms( center.x() + 2, center.y(), 0 );
        draw();
        require( graphics_resolved_vehicle_textures > 0, "visible_vehicle_material_missing" );
        press_f6();
        require( !graphics_view.enabled && !graphics_draw( 0, 0, 640, 480 ), "tiles_fallback_missing" );
        press_f6();
        require( graphics_view.enabled, "f6_near_vehicle_did_not_enable" );
        draw();
        require( graphics_resolved_vehicle_textures > 0, "vehicle_disappeared_after_toggle" );
        here.destroy_vehicle( car );
        g->ter_view_p = center;
        for( int y = -4; y <= 4; ++y ) for( int x = -4; x <= 4; ++x ) {
            here.ter_set( tripoint_bub_ms( center.x() + x, center.y() + y, 0 ), ter_id( "t_floor" ) );
            here.ter_set( tripoint_bub_ms( center.x() + x, center.y() + y, 1 ), ter_id( "t_floor" ) );
        }
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        draw();
        require( graphics_ceiling_cells > 0, "real_indoor_ceiling_missing" );
        press_f6();
        require( !graphics_view.enabled, "f6_final_toggle_off_failed" );
        require( you.pos_bub() == start_pos && you.get_moves() == start_moves && calendar::turn == start_time,
                 "vehicle_camera_tests_changed_gameplay_state" );
    } catch( const std::exception &err ) {
        reason = err.what();
        graphics_view.enabled = false;
    }
    const bool success = reason == "ok";
    std::ofstream out( game_root() / "ncmm" / "first-person-smoke.json", std::ios::binary | std::ios::trunc );
    out << "{\"schema\":1,\"success\":" << ( success ? "true" : "false" )
        << ",\"reason\":\"" << reason << "\",\"checks\":" << checks << ",\"sdl_frames\":" << frames << "}\n";
    log_line( success ? NCMM_LOG_INFO : NCMM_LOG_ERROR,
              ( "First Person View actual engine smoke: " + reason ).c_str() );
    return success;
#else
    return false;
#endif
}
