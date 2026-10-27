// Explicit movement-diagnostic opt-in only, once per scene and before the
// movement timing matrix. Both paths read the same real Native cache pointers;
// outputs are owned here. No Native cache, world, avatar or renderer is written.
{
    using sight_grid = cata::mdarray<float, point_bub_ms>;
    using floor_grid = cata::mdarray<bool, point_bub_ms>;
    static_assert( std::is_trivially_copyable_v<sight_grid> );
    static_assert( sizeof( sight_grid ) == sizeof( float ) * MAPSIZE_X * MAPSIZE_Y );
    constexpr int pair_count = 9, repetitions = 8;
    constexpr size_t plane_elements = MAPSIZE_X * MAPSIZE_Y;
    const auto profile_origin = you.pos_bub();
    const int profile_moves = you.get_moves();
    const auto profile_time = calendar::turn;
    const bool profile_circular_distance = trigdist;
    const auto direction = here.get_cache_ref( profile_origin.z() ).no_floor_gaps ?
                           vertical_direction::UP : vertical_direction::BOTH;
    std::array<std::unique_ptr<sight_grid>, OVERMAP_LAYERS> original, cached;
    array_of_grids_of<float> original_output{}, cached_output{};
    array_of_grids_of<const float> input{};
    array_of_grids_of<const bool> floors{};
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
        const size_t index = z + OVERMAP_DEPTH;
        const auto &native = here.get_cache_ref( z );
        input[index] = &native.vision_transparency_cache;
        floors[index] = &native.floor_cache;
        original[index] = std::make_unique<sight_grid>();
        cached[index] = std::make_unique<sight_grid>();
        original_output[index] = original[index].get();
        cached_output[index] = cached[index].get();
    }
    std::vector<unsigned char> protected_bytes;
    protected_bytes.reserve( OVERMAP_LAYERS * ( 3 * sizeof( sight_grid ) + sizeof( floor_grid ) ) );
    auto native_identity = [&]( bool capture ) {
        uint64_t digest = 1469598103934665603ULL;
        size_t offset = 0;
        bool equal = true;
        auto block = [&]( const void *address, size_t bytes ) {
            const auto *data = static_cast<const unsigned char *>( address );
            if( capture ) protected_bytes.insert( protected_bytes.end(), data, data + bytes );
            else equal &= offset + bytes <= protected_bytes.size() &&
                          std::memcmp( data, protected_bytes.data() + offset, bytes ) == 0;
            for( size_t i = 0; i < bytes; ++i ) digest = ( digest ^ data[i] ) * 1099511628211ULL;
            offset += bytes;
        };
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            const auto &native = here.get_cache_ref( z );
            block( &native.vision_transparency_cache, sizeof( sight_grid ) );
            block( &native.floor_cache, sizeof( floor_grid ) );
            block( &native.seen_cache, sizeof( sight_grid ) );
            block( &native.camera_cache, sizeof( sight_grid ) );
        }
        return std::make_pair( equal && offset == protected_bytes.size(), std::to_string( digest ) );
    };
    const auto initial_identity = native_identity( true );
    auto output_identity = [&]( const array_of_grids_of<float> &output ) {
        uint64_t digest = 1469598103934665603ULL;
        for( const auto *grid : output ) {
            const auto *bytes = reinterpret_cast<const unsigned char *>( grid );
            for( size_t i = 0; i < sizeof( sight_grid ); ++i ) digest = ( digest ^ bytes[i] ) * 1099511628211ULL;
        }
        return std::to_string( digest );
    };
    auto measure = [&]( auto cast, const array_of_grids_of<float> &output ) {
        // Grid reset, allocation, hashing, validation and JSON IO are outside
        // the measured scope. Caster-owned PLF allocation remains inside it.
        for( auto *grid : output ) std::fill_n( &( *grid )[0][0], plane_elements, LIGHT_TRANSPARENCY_SOLID );
        ( *output[profile_origin.z() + OVERMAP_DEPTH] )[profile_origin.x()][profile_origin.y()] = VISIBILITY_FULL;
        const auto started = std::chrono::steady_clock::now();
        for( int n = 0; n < repetitions; ++n ) cast( output, input, floors, profile_origin, 0, 1.0f, direction );
        return std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - started ).count();
    };
    const auto path = game_root() / "ncmm" / ( "first-person-sight-cache-" + scene + ".json" );
    std::ofstream sight_stream( path );
    JsonOut sight_report( sight_stream );
    sight_report.start_object();
    sight_report.member( "schema", 3 );
    sight_report.member( "scene", scene );
    sight_report.member( "actual_cdda_sdl", true );
    sight_report.member( "manual_playthrough", false );
    sight_report.member( "same_process", true );
    sight_report.member( "scope", "native_3d_caster_only" );
    sight_report.member( "timing", "steady_clock_wall_batch_ms" );
    sight_report.member( "world_state", "warmed_native_caches_no_simulation_turns" );
    sight_report.member( "gpu_execution_measured", false );
    sight_report.member( "first_correct_present_measured", false );
    sight_report.member( "reference", "original_sight_formula_generic_forwarding" );
    sight_report.member( "candidate", "exact_standard_integer_distance_original_exponent" );
    sight_report.member( "distance_metric", profile_circular_distance ? "circular" : "square" );
    sight_report.member( "candidate_shortcut", profile_circular_distance ? "circular_table" : "square_row" );
    sight_report.member( "width", MAPSIZE_X );
    sight_report.member( "height", MAPSIZE_Y );
    sight_report.member( "layers", OVERMAP_LAYERS );
    sight_report.member( "max_view_distance", MAX_VIEW_DISTANCE );
    sight_report.member( "offset_distance", 0 );
    sight_report.member( "numerator", 1.0f );
    sight_report.member( "origin", std::vector<int>{profile_origin.x(), profile_origin.y(), profile_origin.z()} );
    sight_report.member( "direction", direction == vertical_direction::UP ? "up" : "both" );
    sight_report.member( "repetitions", repetitions );
    sight_report.member( "protected_native_bytes", protected_bytes.size() );
    sight_report.member( "native_before", initial_identity.second );
    sight_report.member( "samples" );
    sight_report.start_array();
    bool comparison_ok = initial_identity.first;
    for( int sample = 0; sample < pair_count; ++sample ) {
        progress( "sight_compare_" + scene + "_" + std::to_string( sample ) );
        double reference_ms = 0.0, cached_ms = 0.0;
        const bool reference_first = sample % 2 == 0;
        auto reference_pass = [&]() {
            reference_ms = measure( &ncmm_graphics_shadowcasting::reference_cast, original_output );
        };
        auto cached_pass = [&]() {
            cached_ms = measure( &cast_zlight<float, sight_calc, sight_check, accumulate_transparency>, cached_output );
        };
        if( reference_first ) { reference_pass(); cached_pass(); }
        else { cached_pass(); reference_pass(); }
        bool same_bits = true;
        for( size_t z = 0; z < OVERMAP_LAYERS; ++z )
            same_bits &= std::memcmp( original[z].get(), cached[z].get(), sizeof( sight_grid ) ) == 0;
        const auto identity = native_identity( false );
        const bool metric_preserved = trigdist == profile_circular_distance;
        comparison_ok &= same_bits && identity.first && identity.second == initial_identity.second && metric_preserved;
        sight_report.start_object();
        sight_report.member( "sample", sample );
        sight_report.member( "reference_first", reference_first );
        sight_report.member( "reference_wall_ms", reference_ms );
        sight_report.member( "candidate_wall_ms", cached_ms );
        sight_report.member( "output_bits_equal", same_bits );
        sight_report.member( "output_floats_compared", OVERMAP_LAYERS * plane_elements );
        sight_report.member( "reference_digest", output_identity( original_output ) );
        sight_report.member( "candidate_digest", output_identity( cached_output ) );
        sight_report.member( "protected_native_bytes_equal", identity.first );
        sight_report.member( "metric_preserved", metric_preserved );
        sight_report.member( "native_after", identity.second );
        sight_report.end_object();
        if( !comparison_ok ) break;
    }
    const bool origin_preserved = you.pos_bub() == profile_origin;
    const bool moves_preserved = you.get_moves() == profile_moves;
    const bool calendar_preserved = calendar::turn == profile_time;
    const bool metric_preserved = trigdist == profile_circular_distance;
    comparison_ok &= origin_preserved && moves_preserved && calendar_preserved && metric_preserved;
    sight_report.end_array();
    sight_report.member( "origin_preserved", origin_preserved );
    sight_report.member( "moves_preserved", moves_preserved );
    sight_report.member( "calendar_preserved", calendar_preserved );
    sight_report.member( "metric_preserved", metric_preserved );
    sight_report.member( "success", comparison_ok );
    sight_report.end_object();
    sight_stream.flush();
    require( sight_stream.good(), "native_sight_comparison_write_failed" );
    require( comparison_ok, "native_sight_comparison_changed_output_or_world" );
}
