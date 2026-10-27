// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L10804-L10913
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L10816-L10925
// Same Native code and calls; opt-in adjacent wall probes, including local cleanup.
void map::build_map_cache( const int zlev, bool skip_lightmap )
{
    const int minz = zlevels ? -OVERMAP_DEPTH : zlev;
    const int maxz = zlevels ? OVERMAP_HEIGHT : zlev;
    bool seen_cache_dirty = false;
    bool camera_cache_dirty = false;
    for( int z = minz; z <= maxz; z++ ) {
        build_outside_cache( z );
        build_transparency_cache( z );
        bool floor_cache_was_dirty = build_floor_cache( z );
        seen_cache_dirty |= floor_cache_was_dirty;
        seen_cache_dirty |= get_cache( z ).seen_cache_dirty;
    }
    // needs a separate pass as it changes the caches on neighbour z-levels (e.g. floor_cache);
    // otherwise such changes might be overwritten by main cache-building logic
    for( int z = minz; z <= maxz; z++ ) {
        do_vehicle_caching( z );
    }
    for( int z = minz; z <= maxz; z++ ) {
        seen_cache_dirty |= build_vision_transparency_cache( z );
    }

    if( seen_cache_dirty ) {
        skew_vision_cache.clear();
        skew_vision_wo_fields_cache.clear();
    }
    avatar &u = get_avatar();
    Character::moncam_cache_t mcache = u.get_active_moncams();
    Character::moncam_cache_t diff;
    std::set_symmetric_difference( u.moncam_cache.begin(), u.moncam_cache.end(), mcache.begin(),
                                   mcache.end(), std::inserter( diff, diff.end() ) );
    camera_cache_dirty |= !diff.empty();
    // Initial value is illegal player position.
    const tripoint_abs_ms p = get_player_character().pos_abs();
    int const sr = u.unimpaired_range();
    static tripoint_abs_ms player_prev_pos;
    static int player_prev_range( 0 );
    seen_cache_dirty |= player_prev_pos != p || sr != player_prev_range || camera_cache_dirty;
    if( seen_cache_dirty ) {
        if( inbounds( p ) ) {
            build_seen_cache( get_bub( p ), zlev, sr );
        }
        player_prev_pos = p;
        player_prev_range = sr;
        camera_cache_dirty = true;
#if defined(TILES)
        if( !test_mode ) {
            // Mark cata_tiles draw caches as dirty
            tilecontext->set_draw_cache_dirty();
        }
#endif
    }
    if( camera_cache_dirty ) {
        u.moncam_cache = mcache;
        bool cumulative = seen_cache_dirty;
        for( Character::cached_moncam const &mon : u.moncam_cache ) {
            if( inbounds( mon.second ) ) {
                int const range = mon.first->type->vision_day;
                build_seen_cache( get_bub( mon.second ), mon.second.z(), range, cumulative,
                                  true, std::max( MAX_VIEW_DISTANCE - range, 0 ) );
                cumulative = true;
            }
        }
    }
    // Detect character/NPC light changes reactively.
    // Covers equipment, effects, trade/dialogue mutations, and position changes.
    {
        struct char_light_state {
            float light;
            tripoint_bub_ms pos;
            bool operator==( const char_light_state &o ) const {
                return light == o.light && pos == o.pos;
            }
            bool operator!=( const char_light_state &o ) const {
                return !( *this == o );
            }
        };
        auto compute = []( const Character & ch ) -> char_light_state {
            float light = ch.active_light();
            if( ch.has_effect( effect_onfire ) )
            {
                light += 8.0f;
            } else if( ch.has_effect( effect_haslight ) )
            {
                light += 4.0f;
            }
            return { light, ch.pos_bub() };
        };

        static std::vector<char_light_state> cached_char_lights;
        std::vector<char_light_state> current_lights;
        current_lights.push_back( compute( get_player_character() ) );
        for( const npc &guy : g->all_npcs() ) {
            current_lights.push_back( compute( guy ) );
        }
        if( current_lights != cached_char_lights ) {
            for( const char_light_state &s : cached_char_lights ) {
                set_lightmap_cache_dirty( s.pos.z() );
            }
            for( const char_light_state &s : current_lights ) {
                set_lightmap_cache_dirty( s.pos.z() );
            }
            cached_char_lights = std::move( current_lights );
        }
    }

    if( !skip_lightmap ) {
        generate_lightmap( zlev );
    }
}
