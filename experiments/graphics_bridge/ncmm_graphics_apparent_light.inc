// Adapted from Cataclysm: Dark Days Ahead to reuse an existing Native integer.
// Copyright Cataclysm: DDA contributors; CC-BY-SA-3.0:
// https://creativecommons.org/licenses/by-sa/3.0/
// Exact upstream sources (the two function bodies are identical at both pins):
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/lightmap.cpp#L909-L1032
// Git blob 0b7b3aa8747b9eb0a570c012eb64c41ee43fc681.
// Full lightmap.cpp SHA256: 2bd442b3e7dcc9b582652c914d342d617fd0c79826dc47b1118e054ad3715601.
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/lightmap.cpp#L910-L1033
// Git blob 0852aeec3001f6c7a4f17b94baa4002face5f31a.
// Full lightmap.cpp SHA256: f2dea44164a08636f7209ffd621989016c1a93cad3bbd5b41c6b1c5c12aff07f.
// Original contiguous block: 5280 bytes, SHA256
// 7e570456c43c6dcf6b02d29bd4ef8a25a63a0889006855f28483870dd6a70a14.
// Included at the original definitions in Native lightmap.cpp. The file-local
// implementation accepts the same avatar and an already converted integer.
// The public two-argument fallback, guards, non-distance reads, float operation
// order and separate neighbour 2D distance calls retain their Native behavior.

static map::apparent_light_info ncmm_apparent_light_impl( const level_cache &map_cache,
        const tripoint_bub_ms &p, const avatar &u, const int dist )
{
    const float abs_vis =
        std::max( map_cache.seen_cache[p.x()][p.y()], map_cache.camera_cache[p.x()][p.y()] );
    const float vis = dist > u.unimpaired_range() ? map_cache.camera_cache[p.x()][p.y()] : abs_vis;
    const bool obstructed = vis <= LIGHT_TRANSPARENCY_SOLID + 0.1;
    const bool abs_obstructed = abs_vis <= LIGHT_TRANSPARENCY_SOLID + 0.1;

    auto is_opaque = [&map_cache]( const point_bub_ms & p ) {
        return map_cache.transparency_cache[p.x()][p.y()] <= LIGHT_TRANSPARENCY_SOLID &&
               map_cache.vision_transparency_cache[p.x()][p.y()] <= LIGHT_TRANSPARENCY_SOLID;
    };

    // possibly reduce view if aiming (also blocks light)
    if( get_avatar().recoil < MAX_RECOIL ) {
        if( get_avatar().cant_see( p ) ) {
            return { true, true, 0.0 };
        }
    }

    const bool p_opaque = is_opaque( p.xy() );
    float apparent_light;

    if( p_opaque && vis > 0 ) {
        // This is the complicated case.  We want to check which quadrants the
        // player can see the tile from, and only count light values from those
        // quadrants.
        struct offset_and_quadrants {
            point offset;
            std::array<quadrant, 2> quadrants;
        };
        static constexpr std::array<offset_and_quadrants, 8> adjacent_offsets = {{
                { point::south,      {{ quadrant::SE, quadrant::SW }} },
                { point::north,      {{ quadrant::NE, quadrant::NW }} },
                { point::east,       {{ quadrant::SE, quadrant::NE }} },
                { point::south_east, {{ quadrant::SE, quadrant::SE }} },
                { point::north_east, {{ quadrant::NE, quadrant::NE }} },
                { point::west,       {{ quadrant::SW, quadrant::NW }} },
                { point::south_west, {{ quadrant::SW, quadrant::SW }} },
                { point::north_west, {{ quadrant::NW, quadrant::NW }} },
            }
        };

        four_quadrants seen_from( 0 );
        for( const offset_and_quadrants &oq : adjacent_offsets ) {
            const point_bub_ms neighbour = p.xy() + oq.offset;

            if( !lightmap_boundaries.contains( neighbour ) ) {
                continue;
            }
            if( is_opaque( neighbour ) ) {
                continue;
            }
            if( ( rl_dist( u.pos_bub().xy(), neighbour ) > u.unimpaired_range() &&
                  map_cache.camera_cache[neighbour.x()][neighbour.y()] == 0 ) ||
                ( map_cache.seen_cache[neighbour.x()][neighbour.y()] == 0 &&
                  map_cache.camera_cache[neighbour.x()][neighbour.y()] == 0 ) ) {
                continue;
            }
            // This is a non-opaque visible neighbour, so count visibility from the relevant
            // quadrants
            seen_from[oq.quadrants[0]] = vis;
            seen_from[oq.quadrants[1]] = vis;
        }
        apparent_light = ( seen_from * map_cache.lm[p.x()][p.y()] ).max();
    } else {
        // This is the simple case, for a non-opaque tile light from all
        // directions is equivalent
        apparent_light = vis * map_cache.lm[p.x()][p.y()].max();
    }
    return { obstructed, abs_obstructed, apparent_light };
}

map::apparent_light_info map::apparent_light_helper( const level_cache &map_cache,
        const tripoint_bub_ms &p )
{
    avatar const &u = get_avatar();
    const int dist = rl_dist( u.pos_bub(), p );
    return ncmm_apparent_light_impl( map_cache, p, u, dist );
}

lit_level map::apparent_light_at( const tripoint_bub_ms &p,
                                  const visibility_variables &cache ) const
{
    Character &player_character = get_player_character();
    const int dist = rl_dist( player_character.pos_bub(), p );

    // Clairvoyance overrides everything.
    if( cache.u_clairvoyance > 0 && dist <= cache.u_clairvoyance ) {
        return lit_level::BRIGHT;
    }
    if( cache.clairvoyance_field && field_at( p ).find_field( *cache.clairvoyance_field ) ) {
        return lit_level::BRIGHT;
    }
    const level_cache &map_cache = get_cache_ref( p.z() );
    const apparent_light_info a = ncmm_apparent_light_impl( map_cache, p, get_avatar(), dist );

    // Unimpaired range is an override to strictly limit vision range based on various conditions,
    // but the player can still see light sources
    if( dist > player_character.unimpaired_range() && map_cache.camera_cache[p.x()][p.y()] == 0.0 ) {
        if( !a.abs_obstructed && map_cache.sm[p.x()][p.y()] > 0.0 ) {
            return lit_level::BRIGHT_ONLY;
        }
        return lit_level::BLANK;
    }

    if( a.obstructed ) {
        if( a.apparent_light > LIGHT_AMBIENT_LIT ) {
            if( a.apparent_light > cache.g_light_level ) {
                // This represents too hazy to see detail,
                // but enough light getting through to illuminate.
                return lit_level::BRIGHT_ONLY;
            }
        }
        return lit_level::BLANK;
    }
    // Then we just search for the light level in descending order.
    if( a.apparent_light > LIGHT_SOURCE_BRIGHT || map_cache.sm[p.x()][p.y()] > 0.0 ) {
        return lit_level::BRIGHT;
    }
    if( a.apparent_light > LIGHT_AMBIENT_LIT ) {
        return lit_level::LIT;
    }
    if( a.apparent_light >= cache.vision_threshold ) {
        return lit_level::LOW;
    } else {
        return lit_level::BLANK;
    }
}
