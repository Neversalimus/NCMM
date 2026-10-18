bool graphics_scene_smoke()
{
#if defined(TILES)
    size_t checks = 0;
    size_t frames = 0;
    std::array<size_t,2> view_key_discarded{},view_key_requeued{};
    int distance_radius = 0;
    size_t distance_cells = 0, distance_light_queries = 0;
    float distance_fog = 0;
    std::string reason = "ok";
    auto require = [&]( bool ok, const char *failure ) {
        ++checks;
        if( !ok ) throw std::runtime_error( failure );
    };
    try {
        require( graphics_view.owner == "first_person_view" && (graphics_view.hybrid_render||graphics_view.world_render),
                 "graphics_provider_missing" );
        require( tilecontext && tilecontext->is_valid(), "tileset_unavailable" );
        // Keep the complete established CPU qualification on the same new Host.
        // The GPU equivalence matrix below explicitly selects GPU separately.
        if( get_options().has_option( "NCMM_FIRST_PERSON_RENDERER" ) ) get_options().get_option( "NCMM_FIRST_PERSON_RENDERER" ).setValue( "cpu" );
        map &here = get_map();
        here.load( tripoint_abs_sm( 0, 0, 0 ), true );
        auto clear_fixture_monsters = [&]() {
            const auto monsters = get_creature_tracker().get_monsters_list();
            for( const auto &mon : monsters ) get_creature_tracker().remove( *mon );
            require( get_creature_tracker().get_monsters_list().empty(), "graphics_fixture_monster_reset_incomplete" );
        };
        clear_fixture_monsters();
        for( const auto &vehicle : here.get_vehicles() ) here.destroy_vehicle( vehicle.v );
        avatar &you = get_avatar();
        // Quickstart traits and effects must not randomize this isolated visual
        // fixture's sight, posture or camera height. The creature gallery uses
        // the same normalization before its own explicit appearance cases.
        you.clear_mutations(); you.clear_effects(); you.set_underwater( false );
        you.set_base_height( 175 ); you.set_movement_mode( move_mode_id( "walk" ) );
        const tripoint_bub_ms center( 60, 60, 0 );
        calendar::turn = calendar::turn_zero + 12_hours;
        get_weather().weather_id = weather_type_id( "sunny" );
        for( int y = -8; y <= 8; ++y ) for( int x = -8; x <= 8; ++x ) {
            const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
            here.furn_set( p, furn_id( "f_null" ) );
            here.ter_set( p, ter_id( std::abs( x ) == 4 || std::abs( y ) == 4 ? "t_wall" : "t_floor" ) );
            here.clear_fields( p );
            here.remove_trap( p );
            // The loaded map is procedurally generated. Keep this daylight
            // fixture independent of any inherited roof or smoke fields.
            const tripoint_bub_ms above( p.x(), p.y(), 1 );
            here.furn_set( above, furn_id( "f_null" ) );
            here.ter_set( above, ter_id( "t_open_air" ) );
            here.clear_fields( above );
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
        const auto start_absolute_pos = you.pos_abs();
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
        auto press_view_key = [&](SDL_Keycode key) {
            require(key==SDLK_F6||key==SDLK_F12,"unexpected_diagnostic_view_key");
            const bool acceleration=key==SDLK_F12;const size_t slot=acceleration?1:0;
            const std::string expected=acceleration?"ncmm.view.acceleration":"ncmm.open.first_person_view";
            const char *name=acceleration?"F12":"F6";
            SDL_Event event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.windowID = SDL_GetWindowID( get_sdl_window() );
            event.key.key = key;
            event.key.scancode = acceleration?SDL_SCANCODE_F12:SDL_SCANCODE_F6;
            std::string action;
            // A pre-poll redraw/recovery can pump and discard a queued key.
            // Each bounded input attempt needs its own key; a failed action
            // must never be accepted as a successful module toggle.
            for(int pending=0;pending<8;++pending){
                require( SDL_PushEvent( &event ), acceleration?"audit_key_not_queued":"f6_sdl_event_not_queued" );
                if(view_key_discarded[slot]==0){
                    const bool enabled=graphics_view.enabled;
                    inp_mngr.pump_events(); // Real CDDA pre-poll drain, deliberately losing the first test key.
                    require(graphics_view.enabled==enabled,acceleration?"f12_pre_poll_drain_changed_module_state":"f6_pre_poll_drain_changed_module_state");
                    ++view_key_discarded[slot];
                }
                action=gameplay.handle_input(250);if(action==expected)break;
                ++view_key_requeued[slot];
                log_line(NCMM_LOG_INFO,(std::string(name)+" diagnostic input attempt "+std::to_string(pending+1)+" returned: "+action).c_str());
            }
            require( action == expected, acceleration?"audit_f12_dispatch_failed":"f6_sdl_dispatch_failed" );
            require( ncmm::handle_gameplay_action( action ), acceleration?"audit_f12_dispatch_failed":"f6_gameplay_action_missing" );
        };
        auto press_f6=[&](){press_view_key(SDLK_F6);};
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        require( !graphics_view.enabled, "view_not_initially_disabled" );
        press_f6();
        require( graphics_view.enabled, "view_did_not_enable" );
        if( std::getenv( "NCMM_FP_SAVE_ONLY" ) ) {
            #include "ncmm_graphics_multilevel_save_smoke.inc"
        } else if( std::getenv( "NCMM_FP_CAMERA_INPUT" ) ) {
            #include "ncmm_graphics_camera_input.inc"
            press_f6();require(!graphics_view.enabled,"camera_input_f6_restore_failed");
        } else if( std::getenv( "NCMM_FP_HELD_GALLERY_ONLY" ) ) {
            graphics_run_held_gallery(you,here,require,frames);
            press_f6();require(!graphics_view.enabled,"held_gallery_f6_restore_failed");
        } else if( std::getenv( "NCMM_FP_MOVEMENT_ONLY" ) ) {
            #include "ncmm_graphics_movement_benchmark.inc"
            press_f6();require(!graphics_view.enabled,"movement_benchmark_f6_restore_failed");
        } else if( std::getenv( "NCMM_FP_GPU_BENCHMARK_ONLY" ) ) {
            #include "ncmm_gpu_benchmark.inc"
            press_f6();require(!graphics_view.enabled,"focused_benchmark_f6_restore_failed");
        } else if( std::getenv( "NCMM_FP_GAMEPLAY_AUDIT" ) ) {
            #include "ncmm_graphics_gameplay_audit.inc"
            #include "ncmm_graphics_multilevel_save_smoke.inc"
        } else {
        // Retain captures in an owned target while also exercising real window
        // presentation. Reading a swapped window backbuffer is not reliable.
        SDL_Renderer *smoke_renderer=get_sdl_renderer().get();
        SDL_Texture *smoke_previous=SDL_GetRenderTarget(smoke_renderer);
        SDL_Texture *smoke_canvas=SDL_CreateTexture(smoke_renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,640,480);
        require(smoke_canvas!=nullptr,"smoke_canvas_failed");
        struct smoke_canvas_guard {SDL_Renderer *renderer;SDL_Texture *canvas,*previous;
            ~smoke_canvas_guard(){SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(canvas);}
        } smoke_target_guard{smoke_renderer,smoke_canvas,smoke_previous};
        auto draw = [&]() {
            const auto draw_started = std::chrono::steady_clock::now();
            std::multimap<point, formatted_text> overlays;
            color_block_overlay_container blocks;
            // The fixture changes calendar/weather without executing a turn.
            // Refresh the game's natural-light cache as a real turn would.
            g->reset_light_level();
            here.invalidate_map_cache( 0 );
            here.build_map_cache( 0 );
            here.invalidate_visibility_cache();
            // The real viewport hook keeps this pass before drawing first person.
            here.update_visibility_cache( 0 );
            const auto caches_ready = std::chrono::steady_clock::now();
            tilecontext->draw( point::zero, center, 640, 480, overlays, blocks );
            const auto tiles_ready = std::chrono::steady_clock::now();
            require(SDL_SetRenderTarget(smoke_renderer,smoke_canvas)&&SDL_SetRenderLogicalPresentation(smoke_renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&SDL_SetRenderScale(smoke_renderer,1,1)&&SDL_SetRenderViewport(smoke_renderer,nullptr)&&SDL_SetRenderClipRect(smoke_renderer,nullptr),"smoke_canvas_rebind_failed");
            require( graphics_draw( 0, 0, 640, 480 ), "real_sdl_render_failed" );
            const auto overlay_ready = std::chrono::steady_clock::now();
            require( frames > 0 || graphics_resolved_textures >= 2, "atlas_textures_missing" );
            require( SDL_FlushRenderer( get_sdl_renderer().get() ), "sdl_frame_flush_failed" );
            require(SDL_SetRenderTarget(smoke_renderer,nullptr)&&SDL_SetRenderViewport(smoke_renderer,nullptr)&&SDL_SetRenderClipRect(smoke_renderer,nullptr)&&SDL_RenderTexture(smoke_renderer,smoke_canvas,nullptr,nullptr)&&SDL_RenderPresent(smoke_renderer),"smoke_actual_present_failed");
            graphics_world_frame_presented();
            require(SDL_SetRenderTarget(smoke_renderer,smoke_canvas),"smoke_capture_target_restore_failed");
            const auto flushed = std::chrono::steady_clock::now();
            const auto ms = []( auto end, auto begin ) { return std::chrono::duration<double, std::milli>( end - begin ).count(); };
            if( ms( flushed, draw_started ) > 1000 && ( frames < 5 || frames % 32 == 0 ) ) {
                const auto gpu = graphics_gpu_engine ? graphics_gpu_engine->metrics() : ncmm_gpu_metrics_v1{};
                log_line( NCMM_LOG_WARN, ( "SDL smoke slow frame=" + std::to_string( frames ) +
                    " mode=" + ( graphics_gpu_frame ? std::string( "gpu" ) : std::string( "cpu" ) ) +
                    " total_ms=" + std::to_string( ms( flushed, draw_started ) ) +
                    " native_cache_ms=" + std::to_string( ms( caches_ready, draw_started ) ) +
                    " native_tiles_ms=" + std::to_string( ms( tiles_ready, caches_ready ) ) +
                    " overlay_ms=" + std::to_string( ms( overlay_ready, tiles_ready ) ) +
                    " flush_ms=" + std::to_string( ms( flushed, overlay_ready ) ) +
                    " extraction_ms=" + std::to_string( graphics_world_metrics.extraction_ms ) +
                    " visibility_ms=" + std::to_string( graphics_world_metrics.visibility_ms ) +
                    " callback_ms=" + std::to_string( graphics_callback_ms ) +
                    " gpu_prepare_ms=" + std::to_string( gpu.geometry_ms ) +
                    " gpu_submit_ms=" + std::to_string( gpu.submit_ms ) +
                    " generated_cells=" + std::to_string( gpu.generated_cells ) +
                    " submitted_cells=" + std::to_string( graphics_world_metrics.submitted_cells ) ).c_str() );
            }
            ++frames;
        };
        draw();
        SDL_Texture *stream = graphics_stream_texture;
        draw();
        require( stream && graphics_stream_texture == stream, "streaming_texture_not_reused" );
        auto arrow = [&]( const char *key ) {
            return input_event( inp_mngr.get_keycode( input_event_t::keyboard_code, key ), input_event_t::keyboard_code );
        };
        require( ncmm::graphics_relative_action( "UP", arrow( "UP" ), false ) == "UP" &&
                 ncmm::graphics_relative_action( "DOWN", arrow( "DOWN" ), false ) == "DOWN", "north_relative_arrows_wrong" );
        for( int i = 0; i < 6; ++i ) ncmm::handle_gameplay_action( "ncmm.view.turn_right" );
        require( ncmm::graphics_relative_action( "UP", arrow( "UP" ), false ) == "RIGHT" &&
                 ncmm::graphics_relative_action( "DOWN", arrow( "DOWN" ), false ) == "LEFT" &&
                 ncmm::graphics_relative_action( "LEFT", arrow( "LEFT" ), false ) == "UP" &&
                 ncmm::graphics_relative_action( "RIGHT", arrow( "RIGHT" ), false ) == "DOWN", "east_relative_arrows_wrong" );
        require( ncmm::graphics_relative_action( "UP", arrow( "UP" ), true ) == "UP", "vehicle_controls_rotated" );
        ncmm::graphics_note_input_context( "INVENTORY" );
        require( ncmm::graphics_relative_action( "UP", arrow( "UP" ), false ) == "UP", "menu_arrows_rotated" );
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        for( int i = 0; i < 6; ++i ) ncmm::handle_gameplay_action( "ncmm.view.turn_left" );
        require( ncmm::graphics_mouse_event( 1, 320, 240, 0, 0, true ), "mouse_drag_did_not_capture" );
        const float before_mouse_yaw = graphics_camera_yaw;
        SDL_Event motion = {}; motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID( get_sdl_window() );
        motion.motion.x = 320; motion.motion.y = 240; motion.motion.xrel = 100; motion.motion.yrel = 0;
        require( SDL_PushEvent( &motion ), "mouse_motion_not_queued" );
        SDL_Event look = {}; look.type = SDL_EVENT_KEY_DOWN;
        look.key.windowID = SDL_GetWindowID( get_sdl_window() ); look.key.key = SDLK_F9; look.key.scancode = SDL_SCANCODE_F9;
        require( SDL_PushEvent( &look ), "mouse_pump_key_not_queued" );
        require( gameplay.handle_input( 2000 ) == "ncmm.view.look_up", "mouse_event_consumed_gameplay_key" );
        require( std::abs( graphics_camera_yaw - before_mouse_yaw - .3f ) < .001f, "sdl_mouse_motion_did_not_rotate_camera" );
        require( ncmm::graphics_mouse_event( 3, 320, 240, -100, 0, true ), "mouse_return_motion_failed" );
        ncmm::graphics_note_input_context( "INVENTORY" );
        require( !graphics_mouse_drag && !SDL_GetWindowRelativeMouseMode( get_sdl_window() ), "menu_did_not_release_cursor" );
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        require( you.pos_bub() == start_pos && you.get_moves() == start_moves && calendar::turn == start_time,
                 "mouse_or_relative_camera_spent_turn" );
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
        #include "ncmm_gpu_probe_smoke.inc"
        #include "ncmm_gpu_game_smoke.inc"
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
        require( !graphics_draw( 0, 0, 640, 480 ), "coordinate_picker_did_not_suspend_view" );
        ncmm::graphics_note_input_context( "DEFAULTMODE" );
        {
            ncmm::graphics_targeting_scope aiming( true );
            require( graphics_gameplay_context && !graphics_control_context, "aim_first_redraw_lost_backdrop" );
            ncmm::graphics_note_input_context( "TARGET" );
            draw();
            require( graphics_view.enabled && graphics_gameplay_context && !graphics_control_context,
                     "native_aim_context_changed_user_mode" );
        }
        require( graphics_control_context, "aim_exit_did_not_restore_gameplay_context" );
        draw();
        require( here.open_door( you, door, true ), "real_door_did_not_open" );
        require( here.ter( door ).id().str() == "t_door_o", "door_terrain_not_updated" );
        here.build_map_cache( 0 );
        draw();
        require( you.get_memorized_tile( here.get_abs( door ) ).get_ter_id() == "t_door_o",
                 "open_door_memory_not_updated" );
        // Build a street with a real car, like the player's CatLauncher screenshot.
        for( int y = -NCMM_GRAPHICS_V2_RADIUS; y <= NCMM_GRAPHICS_V2_RADIUS; ++y ) for( int x = -NCMM_GRAPHICS_V2_RADIUS; x <= NCMM_GRAPHICS_V2_RADIUS; ++x ) {
            const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
            here.furn_set( p, furn_id( "f_null" ) );
            here.ter_set( p, ter_id( "t_pavement" ) );
            here.clear_fields( p );
            here.ter_set( tripoint_bub_ms( p.x(), p.y(), 1 ), ter_id( "t_open_air" ) );
            here.furn_set( tripoint_bub_ms( p.x(), p.y(), 1 ), furn_id( "f_null" ) );
            here.clear_fields( tripoint_bub_ms( p.x(), p.y(), 1 ) );
        }
        auto capture_frame = [&]( const std::string &name ) {
            SDL_Surface *image = SDL_RenderReadPixels( renderer, &capture );
            require( image != nullptr, "hybrid_capture_failed" );
            const bool written = SDL_SaveBMP( image, ( game_root() / "ncmm" / name ).string().c_str() );
            SDL_DestroySurface( image );
            require( written, "hybrid_capture_not_written" );
        };
        // A real expanded snapshot, not a camera-only setting: distant terrain
        // must reach the live DLL and survive the validated SDL submission.
        for( int x = 14; x <= 22; ++x ) here.ter_set( tripoint_bub_ms( center.x() + x, center.y() - 44, 0 ), ter_id( "t_wall" ) );
        draw();
        require( graphics_snapshot_radius == NCMM_GRAPHICS_V2_RADIUS && graphics_snapshot_far_cells > 1000,
                 "expanded_world_snapshot_missing" );
        distance_radius = graphics_snapshot_radius;
        distance_cells = graphics_snapshot_far_cells;
        distance_light_queries = graphics_snapshot_light_queries;
        require( distance_light_queries < size_t( NCMM_GRAPHICS_V2_MAX_GRID_SIDE * NCMM_GRAPHICS_V2_MAX_GRID_SIDE / 2 ),
                 "rear_cells_still_query_lighting" );
        distance_fog = graphics_hybrid_last_environment.fog_density;
        require( distance_fog < .005f, "clear_weather_invented_distance_fog" );
        capture_frame( "first-person-distance.bmp" );
        // Review material faces with the installed tileset, then restore the
        // deterministic street used by vehicle and distance regression cases.
        const tripoint_bub_ms brick( center.x() - 3, center.y() - 5, 0 );
        const tripoint_bub_ms plaster( center.x() + 3, center.y() - 5, 0 );
        here.ter_set( brick, ter_id( "t_brick_wall" ) );here.ter_set( plaster, ter_id( "t_wall" ) );
        here.invalidate_map_cache( 0 );here.build_map_cache( 0 );draw();
        capture_frame( "first-person-materials.bmp" );
        here.ter_set( brick, ter_id( "t_pavement" ) );here.ter_set( plaster, ter_id( "t_pavement" ) );
        here.invalidate_map_cache( 0 );here.build_map_cache( 0 );draw();
        constexpr int range_side = NCMM_GRAPHICS_V2_MAX_GRID_SIDE;
        std::vector<uint8_t> transparency( range_side * range_side );
        for( int y = -NCMM_GRAPHICS_V2_RADIUS; y <= NCMM_GRAPHICS_V2_RADIUS; ++y ) for( int x = -NCMM_GRAPHICS_V2_RADIUS; x <= NCMM_GRAPHICS_V2_RADIUS; ++x ) {
            const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
            transparency[( y + NCMM_GRAPHICS_V2_RADIUS ) * range_side + x + NCMM_GRAPHICS_V2_RADIUS] =
                here.inbounds( p ) && here.ter( p ).obj().transparent && ( !here.has_furn( p ) || here.furn( p ).obj().transparent );
        }
        for( int y = -NCMM_GRAPHICS_V2_RADIUS; y <= 0; y += 6 ) for( int x = -NCMM_GRAPHICS_V2_RADIUS; x <= NCMM_GRAPHICS_V2_RADIUS; x += 6 ) {
            const tripoint_bub_ms target( center.x() + x, center.y() + y, 0 );
            bool reference = ncmm_graphics_camera::in_front( graphics_camera_yaw, float( x ), float( y ) );
            if( reference ) for( const tripoint &raw : line_to( center.raw(), target.raw() ) ) {
                const tripoint_bub_ms p( raw );
                if( p == center || p == target ) continue;
                if( !here.inbounds( p ) || !here.ter( p ).obj().transparent || ( here.has_furn( p ) && !here.furn( p ).obj().transparent ) ) { reference = false; break; }
            }
            require( graphics_forward_visible( center, target ) == reference &&
                     graphics_forward_visible( center, target, &transparency, NCMM_GRAPHICS_V2_RADIUS ) == reference,
                     "cached_distance_visibility_differs_from_engine_line" );
        }
        require( !graphics_forward_visible( center, tripoint_bub_ms( center.x(), -1, 0 ) ), "snapshot_read_beyond_loaded_map" );
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
        capture_frame( "first-person-car.bmp" );
        const tripoint_bub_ms ahead( center.x(), center.y() - 12, 0 );
        const tripoint_bub_ms behind( center.x(), center.y() + 12, 0 );
        require( graphics_forward_visible( center, ahead ) && !graphics_forward_visible( center, behind ),
                 "forward_camera_cone_or_vehicle_roof_truncated" );
        const tripoint_bub_ms barrier( center.x() + 5, center.y() - 6, 0 );
        const tripoint_bub_ms beyond_barrier( center.x() + 10, center.y() - 12, 0 );
        here.ter_set( barrier, ter_id( "t_wall" ) );
        require( !graphics_forward_visible( center, beyond_barrier ), "opaque_terrain_did_not_block_forward_snapshot" );
        here.ter_set( barrier, ter_id( "t_pavement" ) );
        press_f6();
        require( !graphics_view.enabled && !graphics_draw( 0, 0, 640, 480 ), "tiles_fallback_missing" );
        require( !ncmm::graphics_mouse_event( 1, 320, 240, 0, 0, true ) &&
                 ncmm::graphics_relative_action( "UP", arrow( "UP" ), false ) == "UP", "disabled_view_captured_controls" );
        press_f6();
        require( graphics_view.enabled, "f6_near_vehicle_did_not_enable" );
        draw();
        require( graphics_resolved_vehicle_textures > 0, "vehicle_disappeared_after_toggle" );
        bool interior_checked=false;
        const auto street_position=here.get_abs(center);
        for(const vpart_reference &part:car->get_all_parts())if(part.has_feature("SEAT")){
            const auto seat=part.pos_bub(here);const auto vp=here.veh_at(seat);
            if(!vp||!vp->part_with_feature("ROOF",false))continue;
            you.setpos(here,seat,false);here.board_vehicle(seat,&you);draw();
            require(you.in_vehicle&&graphics_presentation.occupied_vehicle>0&&graphics_presentation.eye_height<=1.18f,"occupied_vehicle_camera_or_ceiling_missing");
            capture_frame("first-person-car-interior.bmp");
            // Boarding can recenter the reality bubble. Never reuse the old
            // bubble coordinate to unboard or restore the test avatar.
            here.unboard_vehicle(you.pos_bub());you.setpos(here,here.get_bub(street_position),false);g->update_map(you);
            require(!you.in_vehicle&&here.get_abs(you.pos_bub())==street_position,"vehicle_fixture_did_not_restore_absolute_position");
            interior_checked=true;break;
        }
        require(interior_checked,"roofed_vehicle_seat_fixture_missing");
        log_line(NCMM_LOG_INFO,"First person smoke checkpoint: roofed interior and bubble recenter restored.");
        here.destroy_vehicle( car );
        // update_map() above may spawn inhabitants of newly loaded submaps.
        // A random spider at the scripted attack tile must not block the test.
        clear_fixture_monsters();
        g->ter_view_p = center;
        // Audit all definitions without querying the identities of unseen cells.
        size_t terrain_count = 0, furniture_count = 0, creature_count=0, gas_count=0;
        for(const auto &mon:MonsterGenerator::generator().get_all_mtypes()){
            const auto size=ncmm_graphics_presentation_policy::creature(units::to_milliliter(mon.volume)/1000.0f,int(mon.size)-1,mon.bodytype);
            require(std::isfinite(size.height)&&std::isfinite(size.width)&&size.height>0&&size.height<=7&&size.width>0&&size.width<=9,"creature_scale_audit_invalid");++creature_count;
        }
        for(const auto &ft:field_types::get_all())if(ft.phase==phase_id::GAS){
            require(ncmm_graphics_presentation_policy::gas(ft.id.str(),true)!=NCMM_GAS_NONE,"game_gas_has_no_volume");++gas_count;
        }
        require(creature_count>500&&gas_count>5,"creature_or_gas_audit_incomplete");
        log_line(NCMM_LOG_INFO,"First person smoke checkpoint: loaded creature and gas definitions audited.");
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
        audit << "],\"creatures\":" << std::to_string(creature_count) << ",\"gases\":" << std::to_string(gas_count) << "}\n";
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
        capture_frame( "first-person-pole-grip.bmp" );
        const tripoint_bub_ms attack_cell( center.x(), center.y() - 1, 0 );
        // Keep the scripted target independent of inherited terrain hazards or
        // a quickstart NPC as well as monsters; do not bypass spawn validation.
        here.ter_set( attack_cell, ter_id( "t_floor" ) ); here.furn_set( attack_cell, furn_id( "f_null" ) );
        here.clear_fields( attack_cell ); here.remove_trap( attack_cell );
        if( const monster *previous = get_creature_tracker().creature_at<monster>( attack_cell ) ) get_creature_tracker().remove( *previous );
        if( npc *previous = get_creature_tracker().creature_at<npc>( attack_cell ) ) {
            const auto previous_id = previous->getID(); g->remove_npc( previous_id ); overmap_buffer.remove_npc( previous_id );
        }
        require( here.passable( attack_cell ) && !get_creature_tracker().creature_at( attack_cell ), "attack_fixture_cell_not_empty" );
        monster *target = g->place_critter_at( mtype_id( "mon_zombie" ), attack_cell );
        require( target != nullptr, "attack_target_not_created" );
        ncmm::handle_gameplay_action("ncmm.view.look_down");draw();
        require(graphics_aim.has_value()&&graphics_aim_name==target->disp_name(),"visible_aim_label_missing");
        require(ncmm::graphics_mouse_event(1,320,240,0,0,true),"aim_mouse_capture_failed");
        require(ncmm::graphics_click_action()=="fire","lmb_reach_action_not_routed");
        // If aimed-click acceptance regresses, cancel the interactive targeting
        // loop and fail immediately instead of waiting for the CI deadline.
        SDL_Event cancel={};cancel.type=SDL_EVENT_KEY_DOWN;cancel.key.windowID=SDL_GetWindowID(get_sdl_window());cancel.key.key=SDLK_ESCAPE;cancel.key.scancode=SDL_SCANCODE_ESCAPE;
        require(SDL_PushEvent(&cancel),"reach_guard_event_not_queued");
        const auto trajectory=target_handler::mode_reach(you,you.get_wielded_item());SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
        require(!trajectory.empty()&&trajectory.back()==target->pos_bub()&&!graphics_click_pending,"first_person_click_did_not_accept_normal_reach_trajectory");
        ncmm::graphics_mouse_event(2,320,240,0,0,true);
        capture_frame("first-person-aim-label.bmp");
        ncmm::handle_gameplay_action("ncmm.view.look_up");
        const uint64_t serial = graphics_attack_serial;
        const int attack_moves = you.get_moves();
        you.melee_attack( *target, false );
        require( graphics_attack_serial > serial && graphics_attack_kind == 1 && you.get_moves() < attack_moves,
                 "real_melee_attack_did_not_start_animation" );
        require( calendar::turn == start_time && you.pos_bub() == start_pos, "attack_animation_changed_time_or_position" );
        // Simulate a slow world turn without sleeping or spending game time.
        graphics_attack_started = SDL_GetTicks() / 1000.0 - 2;
        const double presented_after = SDL_GetTicks() / 1000.0;
        draw();
        require( !graphics_attack_pending && graphics_attack_started >= presented_after,
                 "slow_turn_consumed_unseen_attack_animation" );
        graphics_attack_started = SDL_GetTicks() / 1000.0 - .12;
        draw(); capture_frame( "first-person-attack.bmp" );
        graphics_attack_started = SDL_GetTicks() / 1000.0 - .6;
        draw(); capture_frame( "first-person-attack-recovered.bmp" );
        get_creature_tracker().remove( *target );
        you.set_moves( start_moves );
        here.furn_set( tripoint_bub_ms( center.x() - 1, center.y() - 2, 0 ), furn_id( "f_boulder_medium" ) );
        draw(); require( graphics_hybrid_shape_counts[NCMM_SHAPE_ROCK] > 0, "real_boulder_lost_rock_shape" );
        capture_frame( "first-person-rock.bmp" );
        here.furn_set( tripoint_bub_ms( center.x() - 1, center.y() - 2, 0 ), furn_id( "f_null" ) );
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
        // Give each geometry sample its own clear backdrop. The previous
        // window/fence row changes connection axes and hides floor-level samples.
        for(int x=-2;x<=2;++x)here.ter_set(tripoint_bub_ms(center.x()+x,center.y()-3,0),ter_id("t_pavement"));
        const std::pair<const char *,uint32_t> geometry_cases[]={
            {"t_guardrail",NCMM_SHAPE_GUARDRAIL},{"t_reinforced_glass",NCMM_SHAPE_GLASS_WALL},
            {"t_resin_hole_c",NCMM_SHAPE_RESIN_PORTAL},{"t_resin_hole_o",NCMM_SHAPE_RESIN_PORTAL},
            {"t_scrap_wall_halfway",NCMM_SHAPE_SCRAP_FENCE}};
        const tripoint_bub_ms sample(center.x(),center.y()-2,0);
        for(const auto &entry:geometry_cases){here.ter_set(sample,ter_id(entry.first));draw();
            require(graphics_hybrid_shape_counts[entry.second]>0,"reported_geometry_regression");
            capture_frame(std::string("first-person-")+entry.first+".bmp");}
        // The DLL owns the camera. Changing the Host's echo alone leaves the
        // rendered view unchanged and can put these floor samples off screen.
        const float sample_pitch=graphics_camera_pitch;
        for(int n=0;n<4;++n)require(ncmm::handle_gameplay_action("ncmm.view.look_down"),"floor_sample_look_down_missing");
        require(graphics_camera_pitch<-.4f&&graphics_camera_pitch>-.7f,"floor_samples_outside_camera_view");
        here.ter_set(sample,ter_id("t_water_dp"));here.furn_set(sample,furn_id("f_scrap_bridge"));draw();
        require(graphics_hybrid_shape_counts[NCMM_SHAPE_BRIDGE]>0,"scrap_bridge_not_horizontal");capture_frame("first-person-scrap-bridge.bmp");
        here.furn_set(sample,furn_id("f_null"));draw();capture_frame("first-person-water.bmp");
        here.ter_set(sample,ter_id("t_floor"));here.i_clear(sample);
        here.ter_set(tripoint_bub_ms(sample.x(),sample.y(),1),ter_id("t_open_air"));here.invalidate_map_cache(1);
        item spilled_water(itype_id("water"),calendar::turn);spilled_water.charges=4;
        item &placed_water=here.add_item_or_charges(sample,spilled_water);
        require(!placed_water.is_null(),"spilled_water_fixture_not_placed");
        // map::add_item processes the generated weather's temperature. Set
        // this fixture's phase after insertion instead of assuming warm weather.
        placed_water.set_item_temperature(units::from_celsius(20));
        require(placed_water.made_of(phase_id::LIQUID),"spilled_water_fixture_not_liquid");draw();
        require(std::any_of(graphics_presentation_cells.begin(),graphics_presentation_cells.end(),[](const auto &v){return v.item_liquid==NCMM_LIQ_FRESH;}),"actual_spilled_water_item_not_a_puddle");
        capture_frame("first-person-water-spill.bmp");
        const auto warm_liquids=std::count_if(graphics_presentation_cells.begin(),graphics_presentation_cells.end(),[](const auto &v){return v.item_liquid!=NCMM_LIQ_NONE;});
        placed_water.set_item_temperature(units::from_celsius(-20));
        require(placed_water.made_of(phase_id::SOLID),"frozen_water_fixture_not_solid");draw();
        require(std::count_if(graphics_presentation_cells.begin(),graphics_presentation_cells.end(),[](const auto &v){return v.item_liquid!=NCMM_LIQ_NONE;})==warm_liquids-1,"frozen_water_kept_puddle_model");
        capture_frame("first-person-frozen-water.bmp");here.i_clear(sample);
        for(int n=0;n<4;++n)require(ncmm::handle_gameplay_action("ncmm.view.look_up"),"floor_sample_look_restore_missing");
        require(std::abs(graphics_camera_pitch-sample_pitch)<.0001f,"floor_sample_camera_not_restored");
        here.add_field(sample,field_type_str_id("fd_smoke").id(),3);draw();
        require(std::any_of(graphics_presentation_cells.begin(),graphics_presentation_cells.end(),[](const auto &v){return v.gas==NCMM_GAS_SMOKE&&v.field_intensity==3;}),"real_smoke_volume_missing");
        capture_frame("first-person-smoke-volume.bmp");
        // Explicitly exercise the Host recovery path, independent of whether the
        // CI runner has hardware acceleration or only the SDL software backend.
        graphics_effect_pixels.assign(4,0x20408040u);graphics_effect_width=graphics_effect_height=2;
        graphics_effect_failed=true;const auto fallback_count=graphics_effect_cpu_frames;
        const SDL_FRect full={0,0,640,480};
        require(graphics_present_effects(renderer,graphics_stream_texture,full)&&graphics_effect_cpu_frames==fallback_count+1,"atmosphere_cpu_recovery_failed");
        graphics_effect_failed=false;here.clear_fields(sample);draw();
        item rifle(itype_id("modular_m27_assault_rifle"),calendar::turn);
        rifle.force_insert_item(item(itype_id("rifle_scope"),calendar::turn),pocket_type::MOD);
        rifle.force_insert_item(item(itype_id("suppressor"),calendar::turn),pocket_type::MOD);
        rifle.force_insert_item(item(itype_id("grip"),calendar::turn),pocket_type::MOD);
        you.set_wielded_item(rifle);draw();
        require(graphics_presentation.gun_kind==NCMM_GUN_RIFLE&&(graphics_presentation.gunmods&(NCMM_MOD_OPTIC|NCMM_MOD_SUPPRESSOR|NCMM_MOD_FOREGRIP))==(NCMM_MOD_OPTIC|NCMM_MOD_SUPPRESSOR|NCMM_MOD_FOREGRIP),"actual_firearm_attachments_missing");
        capture_frame("first-person-rifle-attachments.bmp");
        ncmm_draw_perspective_label(u8"Зомби / zombie / 僵尸",320,80,capture);SDL_FlushRenderer(renderer);capture_frame("first-person-unicode-label.bmp");
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
        #include "ncmm_graphics_creature_smoke.inc"
        if( get_options().has_option( "NCMM_FIRST_PERSON_RENDERER" ) && !std::getenv( "NCMM_FP_APPEARANCE_BEFORE" ) ) {
            get_options().get_option( "NCMM_FIRST_PERSON_RENDERER" ).setValue( "gpu" ); graphics_smoke_gpu_gallery = true;
            #include "ncmm_graphics_creature_smoke.inc"
            graphics_smoke_gpu_gallery = false; get_options().get_option( "NCMM_FIRST_PERSON_RENDERER" ).setValue( "cpu" ); draw();
        }
        #include "ncmm_gpu_benchmark.inc"
        #include "ncmm_graphics_multilevel_smoke.inc"
        press_f6();
        require( !graphics_view.enabled, "f6_final_toggle_off_failed" );
        // Native stairs recenter the reality bubble. Gameplay position is
        // absolute; the same tile can have a different local coordinate.
        require( you.pos_abs() == start_absolute_pos && you.get_moves() == start_moves && calendar::turn == start_time,
                 "vehicle_camera_tests_changed_gameplay_state" );
        #include "ncmm_graphics_multilevel_save_smoke.inc"
        }
        require(view_key_discarded[0]==1&&view_key_requeued[0]>0,"f6_pre_poll_drain_regression_not_exercised");
        if(std::getenv("NCMM_FP_GAMEPLAY_AUDIT"))require(view_key_discarded[1]==1&&view_key_requeued[1]>0,"f12_pre_poll_drain_regression_not_exercised");
    } catch( const std::exception &err ) {
        reason = err.what();
        graphics_view.enabled = false;
    }
    const bool success = reason == "ok";
    std::ofstream out( game_root() / "ncmm" / "first-person-smoke.json", std::ios::binary | std::ios::trunc );
    out.imbue(std::locale::classic()); // JSON decimals are invariant under the player's UI/OS locale.
    out << "{\"schema\":1,\"success\":" << ( success ? "true" : "false" )
        << ",\"reason\":\"" << reason << "\",\"checks\":" << std::to_string( checks )
        << ",\"sdl_frames\":" << std::to_string( frames ) << ",\"gpu_effect_frames\":" << std::to_string(graphics_effect_gpu_frames) << ",\"cpu_recovery_frames\":" << std::to_string(graphics_effect_cpu_frames)
        << ",\"f6_discarded_key_regressions\":" << std::to_string(view_key_discarded[0]) << ",\"f6_requeued_attempts\":" << std::to_string(view_key_requeued[0])
        << ",\"f12_discarded_key_regressions\":" << std::to_string(view_key_discarded[1]) << ",\"f12_requeued_attempts\":" << std::to_string(view_key_requeued[1])
        << ",\"draw_radius\":" << std::to_string( distance_radius ) << ",\"distant_cells\":" << std::to_string( distance_cells )
        << ",\"light_queries\":" << std::to_string( distance_light_queries )
        << ",\"clear_weather_fog\":" << distance_fog << "}\n";
    log_line( success ? NCMM_LOG_INFO : NCMM_LOG_ERROR,
              ( "First Person View actual engine smoke: " + reason ).c_str() );
    return success;
#else
    return false;
#endif
}
