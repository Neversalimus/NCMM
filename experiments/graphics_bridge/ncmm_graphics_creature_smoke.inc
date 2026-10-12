// Included in the isolated real-engine smoke fixture, after ordinary FP tests.
// All captures below pass through CDDA's visibility, tileset, module and SDL.
{
    namespace anatomy = first_person::creature;
    const bool billboard_baseline = std::getenv( "NCMM_FP_BILLBOARD_BASELINE" ) != nullptr;
    const bool gpu_gallery = graphics_smoke_gpu_gallery;
    const std::string mode = billboard_baseline ? "billboard-081" : gpu_gallery ? "procedural-gpu" : "procedural";
    // The quickstart avatar is randomized. Myopia still limits sight to 12
    // cells with DEBUG_NIGHTVISION, so normalize this isolated visual fixture.
    you.clear_mutations(); you.clear_effects(); you.set_underwater( false );
    you.set_base_height( 175 ); you.set_movement_mode( move_mode_id( "walk" ) );
    you.toggle_trait( trait_id( "DEBUG_NIGHTVISION" ) ); you.recalc_sight_limits();
    require( you.unimpaired_range() >= 48, "creature_fixture_sight_is_limited" );
    const float saved_yaw = graphics_camera_yaw, saved_pitch = graphics_camera_pitch;
    auto camera = [&]( float yaw, float pitch ) {
        graphics_mouse_dx = std::remainder( yaw - graphics_camera_yaw, 2 * ncmm_graphics_camera::pi ) / .003f;
        graphics_mouse_dy = ( graphics_camera_pitch - pitch ) / .003f;
        require( graphics_action( "ncmm.view.mouse_look" ), "creature_gallery_camera_failed" );
        graphics_mouse_dx = graphics_mouse_dy = 0;
    };
    auto clear_monsters = [&]() {
        const auto monsters = get_creature_tracker().get_monsters_list();
        for( const auto &mon : monsters ) get_creature_tracker().remove( *mon );
    };
    clear_monsters();
    for( int y = -48; y <= 48; ++y ) for( int x = -48; x <= 48; ++x ) {
        const tripoint_bub_ms p( center.x() + x, center.y() + y, 0 );
        here.ter_set( p, ter_id( "t_pavement" ) ); here.furn_set( p, furn_id( "f_null" ) ); here.clear_fields( p );
        const tripoint_bub_ms above( p.x(), p.y(), 1 );
        here.ter_set( above, ter_id( "t_open_air" ) ); here.furn_set( above, furn_id( "f_null" ) ); here.clear_fields( above );
    }
    calendar::turn = start_time; get_weather().weather_id = weather_type_id( "sunny" );
    // Audit the loaded, inheritance-resolved definitions, including installed mods.
    std::ofstream coverage_stream( game_root() / "ncmm" / "first-person-creature-coverage.json" );
    JsonOut coverage( coverage_stream ); coverage.start_object(); coverage.member( "schema", 1 );
    coverage.member( "classifier_version", anatomy::generator_version ); coverage.member( "definitions" ); coverage.start_array();
    std::array<size_t, size_t( anatomy::anatomy::count )> counts = {};
    std::array<size_t, 5> reasons = {};
    for( const auto &type : MonsterGenerator::generator().get_all_mtypes() ) {
        ncmm_graphics_creature_v1 data = {}; graphics_creature_type( type, data );
        const auto classified = anatomy::classify( data );
        ++counts[size_t( classified.kind )]; ++reasons[size_t( classified.reason )];
        require( std::isfinite( classified.height ) && classified.height > 0 && classified.width > 0 && classified.length > 0, "loaded_creature_anatomy_invalid" );
        coverage.start_object(); coverage.member( "id", type.id.str() ); coverage.member( "bodytype", type.bodytype );
        coverage.member( "anatomy", anatomy::name( classified.kind ) ); coverage.member( "reason", anatomy::reason_name( classified.reason ) );
        coverage.member( "height", classified.height ); coverage.member( "width", classified.width ); coverage.member( "length", classified.length ); coverage.end_object();
    }
    coverage.end_array(); coverage.member( "anatomy_counts", counts ); coverage.member( "fallback_counts", reasons ); coverage.end_object();
    auto settled = [&]() {
        for( int attempt = 0; attempt < 160; ++attempt ) {
            draw();
            require( !graphics_creature_snapshots.empty(), "creature_fixture_has_no_visible_entities" );
            if( billboard_baseline || ( graphics_creature_metrics.struct_size && !graphics_creature_metrics.queued && !graphics_creature_metrics.fallback ) ) {
                if( !gpu_gallery ) return;
                if( graphics_gpu_frame && graphics_gpu_engine ) {
                    // Qualification-only synchronization/readback, outside the
                    // measured loop and never used by ordinary game rendering.
                    SDL_Surface *settled_frame = SDL_RenderReadPixels( renderer, nullptr );
                    require( settled_frame != nullptr, "gpu_gallery_settled_frame_missing" ); SDL_DestroySurface( settled_frame );
                    const auto counters = graphics_gpu_engine->metrics();
                    require( counters.active && !counters.cpu_raster_calls, "gpu_gallery_used_cpu_raster" );
                    graphics_creature_metrics.visible_pixels = counters.visible_creature_pixels;
                    // A real distant LOD3 bird can cover one or two hardware
                    // sample centers. Require a visible sample, not an arbitrary
                    // three-pixel silhouette; a missing GPU creature still fails.
                    if( counters.visible_creature_pixels >= ( graphics_creature_metrics.lod[3] ? 1u : 3u ) ) return;
                }
            }
            SDL_Delay( 3 );
        }
        if( gpu_gallery && graphics_gpu_engine ) {
            const auto counters = graphics_gpu_engine->metrics();
            log_line( NCMM_LOG_ERROR, ( "GPU creature settle: active=" + std::to_string( counters.active ) +
                " pixels=" + std::to_string( counters.visible_creature_pixels ) +
                " completed=" + std::to_string( counters.completed_frame_serial ) +
                " frame=" + std::to_string( counters.frame_serial ) +
                " lod=" + std::to_string( graphics_creature_metrics.lod[0] ) + "," + std::to_string( graphics_creature_metrics.lod[1] ) +
                "," + std::to_string( graphics_creature_metrics.lod[2] ) + "," + std::to_string( graphics_creature_metrics.lod[3] ) +
                " reason=" + counters.reason ).c_str() );
            capture_frame( "first-person-creature-gpu-settle-failed.bmp" );
        }
        require( false, "procedural_creature_generation_did_not_settle" );
    };
    auto spawn = [&]( const char *id, const tripoint_bub_ms &p ) {
        const mtype_id type( id ); require( type.is_valid(), "creature_gallery_definition_missing" );
        auto owned = make_shared_fast<monster>( type );
        monster *mon = g->place_critter_around( owned, p, 0, true );
        require( mon != nullptr, "creature_gallery_spawn_failed" ); return owned;
    };
    auto aim_at = [&]( const tripoint_bub_ms &target, float height, float elevation ) {
        const auto eye = you.pos_bub(); const float dx = float( target.x() - eye.x() ), dy = float( target.y() - eye.y() );
        const float eye_height = std::clamp( you.height() * .00885f, .5f, 3.5f );
        camera( std::atan2( dy, dx ), std::clamp( std::atan2( elevation + height * .52f - eye_height, std::hypot( dx, dy ) ), -1.1f, 1.1f ) );
    };
    const char *examples[] = { "mon_zombie", "mon_dog", "mon_spider_jumping_giant", "mon_ant", "mon_centipede_giant", "mon_crow", "mon_rattlesnake", "mon_fish_salmon", "mon_zombie_hulk", "mon_blob", "mon_mi_go", "mon_turret" };
    std::ofstream gallery_stream( game_root() / "ncmm" / ( "first-person-creature-gallery-" + mode + ".json" ) );
    JsonOut gallery( gallery_stream ); gallery.start_object(); gallery.member( "schema", 1 ); gallery.member( "renderer", mode );
    gallery.member( "actual_cdda_sdl", true ); gallery.member( "frames" ); gallery.start_array();
    const std::array<point, 8> directions{ point( 0, 1 ), point( 1, 1 ), point( 1, 0 ), point( 1, -1 ), point( 0, -1 ), point( -1, -1 ), point( -1, 0 ), point( -1, 1 ) };
    for( const char *id : examples ) {
        clear_monsters(); auto mon = spawn( id, center ); ncmm_graphics_creature_v1 data = {}; graphics_creature_type( *mon->type, data );
        const auto type = anatomy::classify( data ); const int distance = std::clamp( int( std::ceil( std::max( type.height, type.width ) * 2.2f ) ), 2, 12 );
        if( data.flags & NCMM_CR_AQUATIC ) here.ter_set( center, ter_id( "t_water_sh" ) );
        for( size_t direction = 0; direction < directions.size(); ++direction ) {
            const auto d = directions[direction]; const tripoint_bub_ms pos( center.x() + d.x * distance, center.y() + d.y * distance, 0 );
            you.setpos( here, pos, false ); g->ter_view_p = pos;
            aim_at( center, type.height, data.flags & NCMM_CR_FLIES ? .65f : 0.f ); settled();
            require( std::any_of( graphics_creature_snapshots.begin(), graphics_creature_snapshots.end(), [&]( const auto &c ) { return c.type_id == std::string( id ); } ), "visible_gallery_creature_not_in_snapshot" );
            if( !billboard_baseline ) require( graphics_creature_metrics.visible_pixels > 2, "procedural_gallery_creature_has_no_pixels" );
            const std::string filename = "first-person-creature-" + std::string( id ) + "-a" + std::to_string( direction ) + "-" + mode + ".bmp";
            capture_frame( filename );
            gallery.start_object(); gallery.member( "file", filename ); gallery.member( "id", id ); gallery.member( "angle", direction );
            gallery.member( "anatomy", anatomy::name( type.kind ) ); gallery.member( "lod", std::vector<uint32_t>( std::begin( graphics_creature_metrics.lod ), std::end( graphics_creature_metrics.lod ) ) );
            gallery.member( "pixels", graphics_creature_metrics.visible_pixels ); gallery.end_object();
        }
        here.ter_set( center, ter_id( "t_pavement" ) );
    }
    // A real tracked NPC exercises clothing/mutation overlays and live posture
    // without regenerating appearance for a mere movement-mode change.
    clear_monsters(); you.setpos( here, tripoint_bub_ms( center.x(), center.y() + 3, 0 ), false );
    auto person = make_shared_fast<npc>(); person->normalize(); person->randomize();
    person->spawn_at_precise( here.get_abs( center ) ); overmap_buffer.insert_npc( person ); g->load_npcs();
    person->setpos( here, center, false ); aim_at( center, 1.75f, 0 ); settled();
    require( graphics_creature_records.count( person.get() ) != 0, "npc_presentation_missing" );
    const auto npc_before = graphics_creature_records[person.get()].snapshot;
    require( npc_before.flags & NCMM_CR_NPC, "npc_semantic_flag_missing" );
    capture_frame( "first-person-creature-npc-clothing-" + mode + ".bmp" );
    person->toggle_trait( trait_id( "FUR" ) ); settled();
    require( graphics_creature_records[person.get()].snapshot.appearance_revision != npc_before.appearance_revision, "npc_mutation_did_not_invalidate_appearance" );
    capture_frame( "first-person-creature-npc-mutation-" + mode + ".bmp" );
    person->set_movement_mode( move_mode_id( "prone" ) ); settled();
    require( graphics_creature_records[person.get()].snapshot.flags & NCMM_CR_PRONE, "npc_live_posture_missing" );
    capture_frame( "first-person-creature-npc-prone-" + mode + ".bmp" );
    person->set_movement_mode( move_mode_id( "walk" ) ); settled();
    require( !( graphics_creature_records[person.get()].snapshot.flags & NCMM_CR_PRONE ), "npc_live_posture_stuck" );
    capture_frame( "first-person-creature-npc-standing-" + mode + ".bmp" );
    g->remove_npc( person->getID() ); overmap_buffer.remove_npc( person->getID() ); draw();
    require( std::none_of( graphics_creature_snapshots.begin(), graphics_creature_snapshots.end(), [&]( const auto &c ) { return c.entity_id == npc_before.entity_id; } ), "unloaded_npc_presentation_stale" );
    // Real movement and combat pathways: observe the live Host events and capture
    // their visual responses. Tests never advance AI turns to manufacture motion.
    clear_monsters(); you.setpos( here, tripoint_bub_ms( center.x(), center.y() + 3, 0 ), false );
    auto actor = spawn( "mon_zombie", center ); aim_at( center, 1.75f, 0 ); settled();
    const uint64_t actor_id = graphics_creature_records[actor.get()].snapshot.entity_id;
    actor->setpos( here, tripoint_bub_ms( center.x() + 1, center.y(), 0 ), false ); draw(); SDL_Delay( 70 ); draw();
    capture_frame( "first-person-creature-movement-" + mode + ".bmp" );
    actor->deal_damage( &you, bodypart_id( "torso" ), damage_instance( damage_type_id( "bash" ), 2.f ) );
    require( graphics_creature_records[actor.get()].snapshot.event == NCMM_CR_HURT, "real_creature_hurt_event_missing" );
    const double hurt_reported = graphics_creature_records[actor.get()].snapshot.event_seconds;
    SDL_Delay( 420 ); draw();
    require( graphics_creature_records[actor.get()].snapshot.event_seconds > hurt_reported + .35, "delayed_hurt_clip_expired_before_presentation" );
    SDL_Delay( 60 ); draw(); capture_frame( "first-person-creature-hurt-" + mode + ".bmp" );
    actor->setpos( here, tripoint_bub_ms( you.pos_bub().x(), you.pos_bub().y() - 1, 0 ), false ); draw();
    actor->melee_attack( you ); require( graphics_creature_records[actor.get()].snapshot.event == NCMM_CR_MELEE, "real_creature_melee_event_missing" );
    draw();
    SDL_Delay( 80 ); draw(); capture_frame( "first-person-creature-melee-" + mode + ".bmp" );
    projectile shot; shot.speed = 1000; shot.range = 8; shot.impact = damage_instance( damage_type_id( "bash" ), 1.f );
    dealt_projectile_attack projectile_result;
    projectile_attack( projectile_result, shot, &here, actor->pos_bub(), tripoint_bub_ms( actor->pos_bub().x() + 4, actor->pos_bub().y(), 0 ), dispersion_sources{ 0 }, actor.get() );
    require( graphics_creature_records[actor.get()].snapshot.event == NCMM_CR_SHOOT, "real_creature_projectile_event_missing" );
    draw();
    SDL_Delay( 70 ); draw(); capture_frame( "first-person-creature-ranged-" + mode + ".bmp" );
    actor->die( &here, nullptr ); SDL_Delay( 650 ); draw();
    require( std::any_of( graphics_creature_snapshots.begin(), graphics_creature_snapshots.end(), [&]( const auto &c ) { return c.entity_id == actor_id && ( c.flags & NCMM_CR_DEAD ); } ), "visible_death_presentation_missing" );
    SDL_Delay( 120 ); draw();
    capture_frame( "first-person-creature-death-" + mode + ".bmp" );
    clear_monsters(); SDL_Delay( 560 ); draw();
    require( std::none_of( graphics_creature_snapshots.begin(), graphics_creature_snapshots.end(), []( const auto &c ) { return c.flags & NCMM_CR_DEAD; } ), "death_presentation_not_expired" );
    // A real melee hit can inflict temporary blindness/knockdown. Subsequent
    // visual-distance cases test LOD, not that randomized combat consequence.
    you.clear_effects(); you.set_movement_mode( move_mode_id( "walk" ) ); you.recalc_sight_limits();
    require( you.unimpaired_range() >= 48, "creature_fixture_sight_changed_after_combat" );
    // LOD, night/colored illumination, and water use real map and light data.
    for( int step = 0; step < 4; ++step ) {
        clear_monsters(); const bool tiny = step == 3; actor = spawn( tiny ? "mon_crow" : "mon_zombie", center );
        const int distances[] = { 2, 14, 34, 34 }; you.setpos( here, tripoint_bub_ms( center.x(), center.y() + distances[step], 0 ), false );
        aim_at( center, tiny ? .2f : 1.75f, tiny ? .65f : 0.f ); settled();
        capture_frame( "first-person-creature-lod" + std::to_string( step ) + "-" + mode + ".bmp" );
        if( !billboard_baseline && !graphics_creature_metrics.lod[step] ) {
            log_line( NCMM_LOG_ERROR, ( "Creature LOD expected=" + std::to_string( step ) +
                " observed=" + std::to_string( graphics_creature_metrics.lod[0] ) + "," + std::to_string( graphics_creature_metrics.lod[1] ) +
                "," + std::to_string( graphics_creature_metrics.lod[2] ) + "," + std::to_string( graphics_creature_metrics.lod[3] ) +
                " visible=" + std::to_string( graphics_creature_snapshots.size() ) + " sight=" + std::to_string( you.unimpaired_range() ) ).c_str() );
            require( false, "actual_creature_lod_level_missing" );
        }
    }
    clear_monsters(); actor = spawn( "mon_zombie", center ); you.setpos( here, tripoint_bub_ms( center.x(), center.y() + 4, 0 ), false ); aim_at( center, 1.75f, 0 );
    for( int lighting = 0; lighting < 3; ++lighting ) {
        calendar::turn = lighting ? start_time + 12_hours : start_time;
        lamp->light_emitted = 60; lamp->light_color = { .05f, .25f, 1.0f };
        here.furn_set( tripoint_bub_ms( center.x() + 1, center.y(), 0 ), lighting == 2 ? lamp->id : furn_id( "f_null" ) );
        settled(); capture_frame( "first-person-creature-light" + std::to_string( lighting ) + "-" + mode + ".bmp" );
    }
    lamp->light_emitted = restore.emission; lamp->light_color = restore.color;
    here.furn_set( tripoint_bub_ms( center.x() + 1, center.y(), 0 ), furn_id( "f_null" ) ); calendar::turn = start_time;
    const tripoint_bub_ms barrier( center.x(), center.y() + 2, 0 );
    for( const char *obstacle : { "t_window", "t_wall" } ) {
        here.ter_set( barrier, ter_id( obstacle ) ); draw(); capture_frame( "first-person-creature-" + std::string( obstacle ) + "-" + mode + ".bmp" );
        if( std::string( obstacle ) == "t_wall" ) require( graphics_creature_snapshots.empty(), "wall_revealed_hidden_creature" );
    }
    here.ter_set( barrier, ter_id( "t_pavement" ) );
    gallery.end_array(); gallery.end_object();
    // Paired real SDL render-loop benchmark. The older 0.8.1 DLL is built from
    // its exact commit and run in the same Host by CI; these are not player FPS.
    std::ofstream timing_stream( game_root() / "ncmm" / ( "first-person-creature-timing-" + mode + ".json" ) );
    JsonOut timing( timing_stream ); timing.start_object(); timing.member( "schema", 1 ); timing.member( "renderer", mode );
    timing.member( "actual_cdda_sdl", true ); timing.member( "player_fps", false ); timing.member( "width", 640 ); timing.member( "height", 480 );
    timing.member( "backend", SDL_GetRendererName( renderer ) ); timing.member( "records" ); timing.start_array();
    const char *mixed[] = { "mon_zombie", "mon_dog", "mon_spider_jumping_giant", "mon_ant", "mon_crow", "mon_blob", "mon_mi_go", "mon_zombie_hulk" };
    for( int amount : { 1, 10, 50, 100 } ) for( int scenario = 0; scenario < 3; ++scenario ) {
        clear_monsters(); std::vector<shared_ptr_fast<monster>> group;
        you.setpos( here, tripoint_bub_ms( center.x(), center.y() + 25, 0 ), false ); camera( -ncmm_graphics_camera::pi * .5f, -.035f );
        calendar::turn = scenario == 2 ? start_time + 12_hours : start_time;
        for( int n = 0; n < amount; ++n ) group.push_back( spawn( mixed[n % 8], tripoint_bub_ms( center.x() - 9 + ( n % 10 ) * 2, center.y() + 7 - ( n / 10 ) * 2, 0 ) ) );
        graphics_clear_creatures();
        const auto generation_started = std::chrono::steady_clock::now(); settled();
        require( graphics_creature_snapshots.size() == size_t( amount ), "benchmark_group_not_fully_visible" );
        const double first_ready_ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - generation_started ).count();
        std::vector<double> samples; const double cpu_before = ncmm_graphics_performance::cpu_seconds(); const auto wall_before = std::chrono::steady_clock::now();
        const auto video_before = ncmm_graphics_performance::vram();
        for( int frame = 0; frame < 48; ++frame ) {
            if( scenario && frame % 4 == 0 ) for( size_t n = 0; n < group.size(); ++n ) {
                const int shift = ( frame / 4 ) % 2;
                group[n]->setpos( here, tripoint_bub_ms( center.x() - 9 + int( n % 10 ) * 2 + shift, center.y() + 7 - int( n / 10 ) * 2, 0 ), false );
            }
            const auto begin = std::chrono::steady_clock::now(); draw(); SDL_RenderPresent( renderer );
            samples.push_back( std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - begin ).count() );
        }
        const double cpu_seconds = ncmm_graphics_performance::cpu_seconds() - cpu_before;
        const double wall_seconds = std::chrono::duration<double>( std::chrono::steady_clock::now() - wall_before ).count();
        const auto video_after = ncmm_graphics_performance::vram(); std::sort( samples.begin(), samples.end() );
        timing.start_object(); timing.member( "creatures", amount ); timing.member( "scenario", scenario == 0 ? "idle-day" : scenario == 1 ? "moving-day" : "moving-night" );
        timing.member( "frames", samples.size() ); timing.member( "median_ms", ( samples[23] + samples[24] ) * .5 ); timing.member( "p95_ms", samples[45] );
        timing.member( "cpu_seconds", cpu_seconds ); timing.member( "wall_seconds", wall_seconds ); timing.member( "cpu_core_equivalents", cpu_seconds / std::max( .00001, wall_seconds ) );
        timing.member( "vram_available", video_before.available && video_after.available ); timing.member( "vram_process_local_before", video_before.bytes ); timing.member( "vram_process_local_after", video_after.bytes );
        timing.member( "cache_bytes", graphics_creature_metrics.cache_bytes ); timing.member( "cache_peak_bytes", graphics_creature_metrics.cache_peak_bytes ); timing.member( "generation_max_ms", graphics_creature_metrics.generation_max_ms );
        timing.member( "generated", graphics_creature_metrics.generated ); timing.member( "cache_entries", graphics_creature_metrics.entries ); timing.member( "cache_evictions", graphics_creature_metrics.evictions ); timing.member( "generation_failures", graphics_creature_metrics.failures );
        timing.member( "generation_anatomy_counts", std::vector<uint64_t>( std::begin( graphics_creature_metrics.anatomy ), std::end( graphics_creature_metrics.anatomy ) ) );
        timing.member( "generation_reason_counts", std::vector<uint64_t>( std::begin( graphics_creature_metrics.reasons ), std::end( graphics_creature_metrics.reasons ) ) );
        timing.member( "model_cache_was_cold", true ); timing.member( "first_ready_ms", first_ready_ms ); timing.member( "visible_snapshot", graphics_creature_snapshots.size() ); timing.member( "fallback", graphics_creature_metrics.fallback );
        timing.end_object(); draw(); capture_frame( "first-person-creature-group-" + std::to_string( amount ) + "-" + std::to_string( scenario ) + "-" + mode + ".bmp" );
        if( !billboard_baseline ) require( graphics_creature_metrics.cache_bytes <= 64 * 1024 * 1024 && !graphics_creature_metrics.fallback, "creature_group_cache_budget_failed" );
    }
    timing.end_array(); timing.end_object();
    clear_monsters(); you.setpos( here, start_pos, false ); you.set_moves( start_moves ); calendar::turn = start_time; g->ter_view_p = center; camera( saved_yaw, saved_pitch );
    #include "ncmm_graphics_appearance_smoke.inc"
    log_line( NCMM_LOG_INFO, ( "Creature SDL gallery and real render-loop benchmark completed: " + mode ).c_str() );
}
