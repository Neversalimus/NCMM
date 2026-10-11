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
        require( graphics_view.owner == "first_person_view" && graphics_view.hybrid_render,
                 "graphics_provider_missing" );
        require( tilecontext && tilecontext->is_valid(), "tileset_unavailable" );
        map &here = get_map();
        here.load( tripoint_abs_sm( 0, 0, 0 ), true );
        for( const auto &vehicle : here.get_vehicles() ) here.destroy_vehicle( vehicle.v );
        avatar &you = get_avatar();
        const tripoint_bub_ms center( 60, 60, 0 );
        calendar::turn = calendar::turn_zero + 12_hours;
        get_weather().weather_id = weather_type_id( "sunny" );
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
            require( gameplay.input_to_action( input_event( inp_mngr.get_keycode( type, "F9" ), type ) ) ==
                     "ncmm.view.look_up", "f9_default_binding_missing" );
            require( gameplay.input_to_action( input_event( inp_mngr.get_keycode( type, "F10" ), type ) ) ==
                     "ncmm.view.look_down", "f10_default_binding_missing" );
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
            // The fixture changes calendar/weather without executing a turn.
            // Refresh the game's natural-light cache as a real turn would.
            g->reset_light_level();
            here.invalidate_map_cache( 0 );
            // The real viewport hook keeps this pass before drawing first person.
            here.update_visibility_cache( 0 );
            tilecontext->draw( point::zero, center, 640, 480, overlays, blocks );
            require( graphics_draw( 0, 0, 640, 480 ), "real_sdl_render_failed" );
            require( frames > 0 || graphics_resolved_textures >= 2, "atlas_textures_missing" );
            require( SDL_FlushRenderer( get_sdl_renderer().get() ), "sdl_frame_flush_failed" );
            ++frames;
        };
        draw();
        const float daytime_light = g->natural_light_level( 0 );
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
        require( graphics_draw( 0, 0, 640, 480 ) && graphics_view.enabled, "inventory_backdrop_changed_to_map" );
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
            here.ter_set( tripoint_bub_ms( p.x(), p.y(), 1 ), ter_id( "t_open_air" ) );
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
        require( graphics_hybrid_vehicle_cells > 0 && graphics_hybrid_width == 640 && graphics_hybrid_height == 480,
                 "hybrid_vehicle_or_native_frame_missing" );
        auto capture_frame = [&]( const std::string &name ) {
            SDL_Surface *image = SDL_RenderReadPixels( renderer, &capture );
            require( image != nullptr, "hybrid_capture_failed" );
            const bool written = SDL_SaveBMP( image, ( game_root() / "ncmm" / name ).string().c_str() );
            SDL_DestroySurface( image );
            require( written, "hybrid_capture_not_written" );
        };
        capture_frame( "first-person-car.bmp" );
        press_f6();
        require( !graphics_view.enabled && !graphics_draw( 0, 0, 640, 480 ), "tiles_fallback_missing" );
        press_f6();
        require( graphics_view.enabled, "f6_near_vehicle_did_not_enable" );
        draw();
        require( graphics_resolved_vehicle_textures > 0, "vehicle_disappeared_after_toggle" );
        here.destroy_vehicle( car );
        g->ter_view_p = center;
        // Audit all definitions without querying the identities of unseen cells.
        size_t terrain_count = 0, furniture_count = 0;
        std::array<size_t, NCMM_SHAPE_COUNT> taxonomy = {};
        for( const auto &ter : mapdata::terrain_data.get_all() ) {
            const auto traits = graphics_traits( ter, !ter.close.is_null() );
            const uint32_t shape = ncmm_graphics_shapes::terrain( ter.id.str() + " " + ter.looks_like, traits );
            require( shape < NCMM_SHAPE_COUNT, "terrain_shape_out_of_range" );
            if( traits.tree || traits.shrub || traits.window || ( traits.door && !traits.flat && !ncmm_graphics_shapes::contains( ter.id.str(), "trapdoor" ) ) ) require( ncmm_graphics_shapes::vertical( shape ), "semantic_terrain_became_floor" );
            ++taxonomy[shape]; ++terrain_count;
        }
        for( const auto &furn : mapdata::furniture_data.get_all() ) {
            const auto traits = graphics_traits( furn, !furn.close.is_null() );
            const uint32_t shape = ncmm_graphics_shapes::furniture( furn.id.str() + " " + furn.looks_like, traits );
            require( shape < NCMM_SHAPE_COUNT, "furniture_shape_out_of_range" );
            if( traits.tree || traits.shrub || traits.window || ( traits.door && !traits.flat && !ncmm_graphics_shapes::contains( furn.id.str(), "trapdoor" ) ) ) require( ncmm_graphics_shapes::vertical( shape ), "semantic_furniture_became_floor" );
            ++taxonomy[shape]; ++furniture_count;
        }
        std::ofstream audit( game_root() / "ncmm" / "first-person-shapes-audit.json" );
        // The game locale can insert thousands separators; JSON integers cannot contain them.
        audit << "{\"terrain\":" << std::to_string( terrain_count ) << ",\"furniture\":" << std::to_string( furniture_count ) << ",\"shape_counts\":[";
        for( size_t i = 0; i < taxonomy.size(); ++i ) { if( i ) audit << ','; audit << std::to_string( taxonomy[i] ); }
        audit << "]}\n";
        require( terrain_count > 500 && furniture_count > 200, "taxonomy_audit_incomplete" );
        for( const std::string action : { "open", "close", "examine", "smash", "ACTIONMENU", "TIMEOUT", "UNKNOWN" } ) {
            graphics_action( action );
            require( graphics_view.enabled && graphics_gameplay_context, "ordinary_command_flickered_to_map" );
        }
        ncmm::graphics_note_input_context( "CHOOSE_ADJACENT" ); draw();
        require( graphics_view.enabled && graphics_gameplay_context, "adjacent_picker_flickered_to_map" );
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        for( int y = -16; y <= 16; ++y ) for( int x = -16; x <= 16; ++x ) here.ter_set(
            tripoint_bub_ms( center.x() + x, center.y() + y, 1 ), ter_id( "t_open_air" ) );
        here.ter_set( tripoint_bub_ms( center.x() - 2, center.y() - 3, 0 ), ter_id( "t_door_o" ) );
        here.ter_set( tripoint_bub_ms( center.x() - 1, center.y() - 3, 0 ), ter_id( "t_window" ) );
        here.ter_set( tripoint_bub_ms( center.x(), center.y() - 3, 0 ), ter_id( "t_window_empty" ) );
        here.ter_set( tripoint_bub_ms( center.x() + 1, center.y() - 3, 0 ), ter_id( "t_chainfence" ) );
        here.ter_set( tripoint_bub_ms( center.x() + 2, center.y() - 3, 0 ), ter_id( "t_shrub" ) );
        you.set_wielded_item( item( itype_id( "naginata" ), calendar::turn ) );
        here.invalidate_map_cache( 0 ); here.invalidate_map_cache( 1 ); here.build_map_cache( 0 ); draw();
        require( graphics_hybrid_shape_counts[NCMM_SHAPE_DOOR_OPEN] > 0, "real_open_door_lost_height" );
        require( graphics_hybrid_shape_counts[NCMM_SHAPE_WINDOW] + graphics_hybrid_shape_counts[NCMM_SHAPE_WINDOW_COVERED] > 0,
                 "real_window_became_floor" );
        require( graphics_hybrid_shape_counts[NCMM_SHAPE_WINDOW_BROKEN] > 0, "broken_window_became_floor" );
        require( graphics_hybrid_shape_counts[NCMM_SHAPE_FENCE] > 0, "chainlink_became_floor" );
        require( graphics_hybrid_shape_counts[NCMM_SHAPE_SHRUB] > 0, "real_shrub_became_floor" );
        require( graphics_hybrid_held_sprites > 0 && std::string( graphics_hybrid_last_environment.held_item ) == "naginata",
                 "wielded_tileset_item_missing" );
        capture_frame( "first-person-objects.bmp" );
        const tripoint_bub_ms aphid_pos( center.x(), center.y() - 1, 0 );
        monster *aphid = g->place_critter_at( mtype_id( "mon_aphid" ), aphid_pos );
        require( aphid != nullptr, "tiny_creature_fixture_missing" );
        draw(); require( graphics_hybrid_tiny_creatures > 0, "tiny_creature_size_missing" );
        capture_frame( "first-person-tiny-creature.bmp" );
        get_creature_tracker().remove( *aphid );
        require( ncmm::handle_gameplay_action( "ncmm.view.quality" ), "quality_action_missing" );
        require( you.pos_bub() == center, "quality_changed_avatar_position" );
        draw();
        require( ncmm::handle_gameplay_action( "ncmm.view.quality" ), "balanced_action_missing" );
        draw();
        you.set_wielded_item( item( itype_id( "knife_combat" ), calendar::turn ) ); draw();
        require( graphics_hybrid_held_sprites > 0 && std::string( graphics_hybrid_last_environment.held_item ) == "knife_combat",
                 "wielded_item_sprite_stale" );
        capture_frame( "first-person-held.bmp" );
        for( int i = 0; i < 6; ++i ) require( ncmm::handle_gameplay_action( "ncmm.view.look_up" ), "look_up_missing" );
        draw(); capture_frame( "first-person-sky.bmp" );
        const auto true_sun = sun_azimuth_altitude( calendar::turn );
        require( std::abs( graphics_hybrid_last_environment.sun_z - std::sin( units::to_radians( true_sun.second ) ) ) < .0001,
                 "sun_not_from_game_calendar" );
        for( int i = 0; i < 6; ++i ) require( ncmm::handle_gameplay_action( "ncmm.view.look_down" ), "look_down_missing" );
        calendar::turn = start_time + 14_days + 12_hours;
        here.invalidate_map_cache( 0 ); here.build_map_cache( 0 ); draw();
        require( graphics_hybrid_last_environment.daylight < .2f && graphics_hybrid_last_environment.moon_phase == uint32_t( get_moon_phase( calendar::turn ) ),
                 "night_or_lunar_phase_not_from_calendar" );
        require( g->natural_light_level( 0 ) < daytime_light * .2f, "fixture_kept_daytime_light_at_night" );
        capture_frame( "first-person-night.bmp" );
        // Verify actual CDDA RGB propagation, not a synthetic renderer tint.
        furn_t *lamp = nullptr;
        for( const auto &furn : mapdata::furniture_data.get_all() ) {
            if( furn.light_emitted > 0 ) { lamp = &const_cast<furn_t &>( furn ); break; }
        }
        require( lamp != nullptr, "game_light_fixture_missing" );
        const tripoint_bub_ms lamp_pos( center.x() + 1, center.y() - 1, 0 );
        struct lamp_restore {
            furn_t &definition; int emission; light_color_rgb color;
            ~lamp_restore() { definition.light_emitted = emission; definition.light_color = color; }
        } restore{ *lamp, lamp->light_emitted, lamp->light_color };
        lamp->light_emitted = 60; lamp->light_color = { .05f, .25f, 1.0f };
        here.furn_set( lamp_pos, lamp->id );
        here.invalidate_map_cache( 0 ); here.build_map_cache( 0 ); draw();
        const auto &rgb = here.get_cache_ref( 0 ).light_color_cache[lamp_pos.x()][lamp_pos.y()];
        require( rgb.b > rgb.r && graphics_hybrid_colored_cells > 0, "game_rgb_light_not_in_hybrid_scene" );
        capture_frame( "first-person-colored-light.bmp" );
        here.furn_set( lamp_pos, furn_id( "f_null" ) );
        lamp->light_emitted = restore.emission; lamp->light_color = restore.color;
        calendar::turn = start_time;
        for( const std::pair<const char *, uint32_t> weather_case : {
                 std::pair<const char *, uint32_t>( "rain", NCMM_WEATHER_RAIN ),
                 { "snowstorm", NCMM_WEATHER_SNOW }, { "portal_storm", NCMM_WEATHER_PORTAL } } ) {
            get_weather().weather_id = weather_type_id( weather_case.first );
            require( get_weather().weather_id.is_valid(), "weather_fixture_invalid" );
            here.invalidate_map_cache( 0 ); here.build_map_cache( 0 ); draw();
            require( graphics_hybrid_last_environment.weather_kind == weather_case.second && graphics_hybrid_last_environment.precipitation > 0,
                     "actual_game_weather_missing" );
            capture_frame( std::string( "first-person-weather-" ) + weather_case.first + ".bmp" );
        }
        get_weather().weather_id = weather_type_id( "sunny" );
        for( int y = -4; y <= 4; ++y ) for( int x = -4; x <= 4; ++x ) {
            here.ter_set( tripoint_bub_ms( center.x() + x, center.y() + y, 0 ), ter_id( "t_floor" ) );
            here.ter_set( tripoint_bub_ms( center.x() + x, center.y() + y, 1 ), ter_id( "t_floor" ) );
        }
        here.invalidate_map_cache( 0 );
        here.build_map_cache( 0 );
        draw();
        require( graphics_ceiling_cells > 0, "real_indoor_ceiling_missing" );
        require( !graphics_hybrid_last_environment.outdoors, "roof_exposure_not_from_game" );
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
        << ",\"reason\":\"" << reason << "\",\"checks\":" << std::to_string( checks )
        << ",\"sdl_frames\":" << std::to_string( frames ) << "}\n";
    log_line( success ? NCMM_LOG_INFO : NCMM_LOG_ERROR,
              ( "First Person View actual engine smoke: " + reason ).c_str() );
    return success;
#else
    return false;
#endif
}
