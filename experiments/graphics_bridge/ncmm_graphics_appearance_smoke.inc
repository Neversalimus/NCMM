// Paired appearance qualification in a disposable, real CDDA/SDL world.
// The same fixture also runs with the exact unmodified main 0.10 CPU DLL.
if( std::getenv( "NCMM_FP_APPEARANCE_SMOKE" ) ) {
    const bool before = std::getenv( "NCMM_FP_APPEARANCE_BEFORE" ) != nullptr;
    const std::string appearance_mode = before ? "before-cpu" : gpu_gallery ? "after-gpu" : "after-cpu";
    const auto saved_clock = graphics_smoke_seconds;
    const uint64_t saved_identity = graphics_creature_next_id;
    const tripoint_bub_ms saved_position = you.pos_bub();
    const float saved_appearance_yaw = graphics_camera_yaw, saved_appearance_pitch = graphics_camera_pitch;
    // Fish WATER_CAMOUFLAGE is gated by the observer's randomized perception
    // even with debug night vision. Normalize the disposable observer, rather
    // than changing creature flags or bypassing CDDA's visibility checks.
    struct restore_perception {
        Character &observer; int limit, base, bonus;
        ~restore_perception() { character_max_per = limit; observer.set_per_base( base ); observer.set_per_bonus( bonus ); }
    } perception_guard{ you, character_max_per, you.get_per_base(), you.get_per_bonus() };
    character_max_per = std::max( 64, character_max_per );
    you.healall( 10000 );
    you.set_per_base( 64 ); you.set_per_bonus( 0 );
    require( you.get_eff_per() >= 48, "appearance_observer_perception_not_normalized" );
    for( int y = -48; y <= 48; ++y ) for( int x = -48; x <= 48; ++x ) {
        const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
        here.i_clear( p ); here.clear_fields( p );
    }
    std::ofstream appearance_stream( game_root() / "ncmm" / ( "first-person-appearance-" + appearance_mode + ".json" ) );
    JsonOut appearance_report( appearance_stream ); appearance_report.start_object();
    appearance_report.member( "schema", 1 ); appearance_report.member( "actual_cdda_sdl", true );
    appearance_report.member( "renderer", appearance_mode ); appearance_report.member( "tileset", get_option<std::string>( "TILES" ) );
    appearance_report.member( "baseline_commit", "7f8a48a714e082daf67176e5e4a9806f878eef42" );
    appearance_report.member( "fixed_presentation_clock", true ); appearance_report.member( "frames" ); appearance_report.start_array();
    std::string appearance_case;
    auto appearance_settled = [&]() {
        for( int attempt = 0; attempt < 200; ++attempt ) {
            draw();
            if( !graphics_creature_metrics.queued && !graphics_creature_metrics.fallback &&
                ( !gpu_gallery || ( graphics_gpu_frame && graphics_gpu_engine ) ) ) {
                if( gpu_gallery && graphics_gpu_engine ) {
                    SDL_Surface *frame = SDL_RenderReadPixels( renderer, nullptr );
                    require( frame != nullptr, "appearance_gpu_sync_failed" ); SDL_DestroySurface( frame );
                    const auto counters = graphics_gpu_engine->metrics();
                    require( counters.active && !counters.cpu_raster_calls, "appearance_gpu_used_cpu_raster" );
                    graphics_creature_metrics.visible_pixels = counters.visible_creature_pixels;
                }
                if( graphics_creature_metrics.visible_pixels ) return;
            }
            SDL_Delay( 3 );
        }
        capture_frame( "first-person-appearance-unready-" + appearance_mode + ".bmp" );
        log_line( NCMM_LOG_ERROR, ( "Appearance not ready: " + appearance_case + "; snapshots=" +
                  std::to_string( graphics_creature_snapshots.size() ) + "; pixels=" +
                  std::to_string( graphics_creature_metrics.visible_pixels ) + "; queued=" +
                  std::to_string( graphics_creature_metrics.queued ) + "; fallback=" +
                  std::to_string( graphics_creature_metrics.fallback ) ).c_str() );
        require( false, "appearance_frame_not_ready" );
    };
    // Including a definition supplied by an isolated JSON fixture verifies the
    // unknown-mod path through CDDA loading and the real texture provider.
    const char *appearance_examples[] = { "mon_zombie", "mon_zombie_hulk", "mon_dog", "mon_spider_jumping_giant",
        "mon_ant", "mon_centipede_giant", "mon_crow", "mon_fish_salmon", "mon_rattlesnake", "mon_mi_go",
        "mon_turret", "mon_blob", "npc-clothed", "mon_ncmm_appearance_unknown" };
    for( const char *id : appearance_examples ) {
        const bool npc_case = std::string( id ) == "npc-clothed";
        ncmm_graphics_creature_v1 data = {};
        if( npc_case ) { data.flags = NCMM_CR_NPC; data.height = 1.75f; data.width = .62f; data.liters = data.kilograms = 70; }
        else { require( mtype_id( id ).is_valid(), "appearance_definition_missing" ); graphics_creature_type( *mtype_id( id ), data ); }
        const auto type = anatomy::classify( data );
        for( int distance_band = 0; distance_band < 3; ++distance_band ) for( int lighting = 0; lighting < 2; ++lighting ) for( size_t direction = 0; direction < directions.size(); ++direction ) {
            const float target_pixels = distance_band == 0 ? 180.f : distance_band == 1 ? 50.f : 16.f;
            const auto d = directions[direction]; const float diagonal = std::hypot( float( d.x ), float( d.y ) );
            const float projection = capture.w / ( 2 * std::tan( 70 * ncmm_graphics_camera::pi / 360 ) );
            const int distance = std::clamp( int( std::ceil( projection * std::max( { type.height, type.width, type.length } ) / target_pixels / diagonal ) ), distance_band == 0 ? 1 : distance_band == 1 ? 3 : 8, 32 );
            const tripoint_bub_ms view_position( center.x() + d.x * distance, center.y() + d.y * distance, 0 );
            appearance_case = std::string( id ) + "-d" + std::to_string( distance_band ) +
                              "-l" + std::to_string( lighting ) + "-a" + std::to_string( direction );
            // Repeat the exact observed state with one stable entity identity.
            // Resources remain warm: this does not reset the world/cache epoch.
            clear_monsters(); graphics_creature_records.clear(); graphics_creature_next_id = 0;
            graphics_smoke_seconds = 2.0; calendar::turn = lighting ? start_time + 12_hours : start_time;
            lamp->light_emitted = 60; lamp->light_color = { .05f, .25f, 1.f };
            const tripoint_bub_ms light_position( center.x() + 1, center.y() - 1, 0 );
            here.furn_set( light_position, lighting ? lamp->id : furn_id( "f_null" ) );
            if( data.flags & NCMM_CR_AQUATIC ) here.ter_set( center, ter_id( "t_water_sh" ) );
            shared_ptr_fast<monster> subject;
            shared_ptr_fast<npc> person;
            if( npc_case ) {
                person = make_shared_fast<npc>(); person->normalize(); person->clear_mutations(); person->clear_effects(); person->clear_worn();
                person->male = true; person->name = "Appearance NPC"; person->set_base_height( 175 ); person->set_movement_mode( move_mode_id( "walk" ) );
                for( const char *clothing : { "tshirt", "jeans", "sneakers" } ) {
                    // Item construction picks a random cosmetic variant. Keep
                    // the same clothes across angles, renderers and processes.
                    item garment( itype_id( clothing ), calendar::turn );
                    garment.clear_itype_variant();
                    require( person->wear_item( garment, false ).has_value(), "appearance_npc_clothing_failed" );
                }
                person->spawn_at_precise( here.get_abs( center ) ); overmap_buffer.insert_npc( person ); g->load_npcs(); person->setpos( here, center, false ); person->facing = FacingDirection::RIGHT;
            } else { subject = spawn( id, center ); subject->facing = FacingDirection::RIGHT; }
            you.setpos( here, view_position, false ); g->ter_view_p = view_position;
            aim_at( center, type.height, data.flags & NCMM_CR_FLIES ? .65f : 0.f ); appearance_settled();
            auto capture_appearance = [&]( const char *pose ) {
                const std::string filename = "first-person-appearance-" + std::string( id ) + "-d" + std::to_string( distance_band ) + "-l" + std::to_string( lighting ) + "-a" + std::to_string( direction ) + "-" + pose + "-" + appearance_mode + ".bmp";
                capture_frame( filename ); appearance_report.start_object(); appearance_report.member( "file", filename );
                appearance_report.member( "id", id ); appearance_report.member( "angle", direction ); appearance_report.member( "distance_band", distance_band );
                appearance_report.member( "distance_cells", distance * diagonal ); appearance_report.member( "lighting", lighting ? "blue-night" : "day" );
                appearance_report.member( "pose", pose ); appearance_report.member( "pixels", graphics_creature_metrics.visible_pixels );
                appearance_report.member( "clock", *graphics_smoke_seconds ); appearance_report.member( "camera_yaw", graphics_camera_yaw ); appearance_report.member( "camera_pitch", graphics_camera_pitch );
                appearance_report.member( "camera_x", you.pos_bub().x() ); appearance_report.member( "camera_y", you.pos_bub().y() );
                require( graphics_creature_snapshots.size() == 1, "appearance_subject_not_unique" );
                const auto &observed = graphics_creature_snapshots.front();
                appearance_report.member( "subject_flags", observed.flags ); appearance_report.member( "subject_heading", observed.heading );
                appearance_report.member( "subject_height", observed.height ); appearance_report.member( "subject_width", observed.width );
                appearance_report.member( "subject_liters", observed.liters ); appearance_report.member( "subject_kilograms", observed.kilograms );
                appearance_report.member( "observer_perception", you.get_eff_per() );
                // Read the already-owned Host cache, outside measured frames.
                // This also checks the NPC's composed clothes, not just its base.
                uint64_t source_hash = 0; uint32_t source_width = 0, source_height = 0;
                const auto source_position = person ? person->pos_bub() : subject->pos_bub();
                const auto category = npc_case ? TILE_CATEGORY::NONE : TILE_CATEGORY::MONSTER;
                auto atlas = tilecontext->ncmm_get_texture_draw_data( observed.type_id, category, source_position, false, true );
                if( atlas ) {
                    const auto &r = atlas->dimensions;
                    const graphics_sprite_key key{ reinterpret_cast<uintptr_t>( atlas->texture ), r.x, r.y, r.w, r.h };
                    auto cached = graphics_cpu_sprites.find( key );
                    if( cached != graphics_cpu_sprites.end() ) {
                        const graphics_cpu_sprite *source = &cached->second;
                        if( person ) {
                            uint64_t revision = 1469598103934665603ull;
                            for( uint32_t pixel : source->pixels ) revision = ( revision ^ pixel ) * 1099511628211ull;
                            auto composite = graphics_creature_composites.find( graphics_creature_overlay_revision( *person, revision ) );
                            source = composite == graphics_creature_composites.end() ? nullptr : &composite->second;
                        }
                        if( source ) {
                            source_width = source->width; source_height = source->height;
                            source_hash = 1469598103934665603ull;
                            for( uint32_t pixel : source->pixels ) source_hash = ( source_hash ^ pixel ) * 1099511628211ull;
                        }
                    }
                }
                appearance_report.member( "source_width", source_width ); appearance_report.member( "source_height", source_height );
                appearance_report.member( "source_pixel_hash", std::to_string( source_hash ) );
                appearance_report.member( "lod", std::vector<uint32_t>( std::begin( graphics_creature_metrics.lod ), std::end( graphics_creature_metrics.lod ) ) ); appearance_report.end_object();
            };
            capture_appearance( "idle" );
            if( distance_band == 1 && !( data.flags & NCMM_CR_IMMOBILE ) ) {
                if( subject ) subject->setpos( here, tripoint_bub_ms( center.x() + 1, center.y(), 0 ), false );
                else person->setpos( here, tripoint_bub_ms( center.x() + 1, center.y(), 0 ), false );
                // Observe the authoritative move at its start, then sample the
                // half-step. First drawing at 2.09 would capture phase zero.
                graphics_smoke_seconds = 2.0; draw();
                graphics_smoke_seconds = 2.09; appearance_settled(); capture_appearance( "moving" );
            }
            if( npc_case && distance_band == 1 ) { person->set_movement_mode( move_mode_id( "prone" ) ); graphics_smoke_seconds = 2.25; appearance_settled(); capture_appearance( "prone" ); }
            if( person ) { const auto identity = person->getID(); g->remove_npc( identity ); overmap_buffer.remove_npc( identity ); }
            clear_monsters(); here.ter_set( center, ter_id( "t_pavement" ) ); here.furn_set( light_position, furn_id( "f_null" ) );
        }
    }
    appearance_report.end_array(); appearance_report.end_object();
    lamp->light_emitted = restore.emission; lamp->light_color = restore.color; calendar::turn = start_time;
    graphics_smoke_seconds = saved_clock; graphics_creature_records.clear(); graphics_creature_next_id = std::max( graphics_creature_next_id, saved_identity );
    you.setpos( here, saved_position, false ); g->ter_view_p = saved_position; camera( saved_appearance_yaw, saved_appearance_pitch );
    log_line( NCMM_LOG_INFO, ( "Paired creature appearance matrix completed: " + appearance_mode ).c_str() );
}
