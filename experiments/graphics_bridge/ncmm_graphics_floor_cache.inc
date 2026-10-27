// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L10672-L10727
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L10684-L10739
// Exact Native cache rules; bounded uniform definitions may be resolved once.
bool map::build_floor_cache( const int zlev )
{
    auto *ch_lazy = get_cache_lazy( zlev );
    if( !ch_lazy || !ch_lazy->floor_cache_dirty ) {
        return false;
    }
    level_cache &ch = *ch_lazy;

    auto &floor_cache = ch.floor_cache;
    std::uninitialized_fill_n(
        &floor_cache[0][0], MAPSIZE_X * MAPSIZE_Y, true );
    bool &no_floor_gaps = ch.no_floor_gaps;
    no_floor_gaps = true;

    bool lowest_z_lev = zlev <= -OVERMAP_DEPTH;

    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const submap *cur_submap = get_submap_at_grid( tripoint_rel_sm{ smx, smy, zlev } );
            const submap *below_submap = !lowest_z_lev ? get_submap_at_grid( tripoint_rel_sm{ smx, smy, zlev - 1 } ) :
                                         nullptr;

            if( cur_submap == nullptr ) {
                debugmsg( "Tried to build floor cache at (%d,%d,%d) but the submap is not loaded", smx, smy, zlev );
                continue;
            }
            if( !lowest_z_lev && below_submap == nullptr ) {
                debugmsg( "Tried to build floor cache at (%d,%d,%d) but the submap is not loaded", smx, smy,
                          zlev - 1 );
                continue;
            }

            // BEGIN UNIFORM NATIVE FLOOR CACHE
            // A uniform submap has one terrain and the actual shared f_null.
            // Resolve their unchanged predicate once, retaining the original
            // loop for every mixed or invalid definition and its diagnostics.
            if( cur_submap->is_uniform() && ( !below_submap || below_submap->is_uniform() ) ) {
                const ter_id uniform_terrain = cur_submap->get_ter( point_sm_ms::zero );
                // The pinned terrain is_valid() has no external definition.
                // This is generic_factory's exact quiet integer bounds test.
                if( static_cast<std::size_t>( uniform_terrain.to_i() ) < ter_t::count() ) {
                    const ter_t &terrain = uniform_terrain.obj();
                    const bool has_gap = terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR ) ||
                                         terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR_WATER ) ||
                                         terrain.has_flag( ter_furn_flag::TFLAG_GOES_DOWN ) ||
                                         terrain.has_flag( ter_furn_flag::TFLAG_TRANSPARENT_FLOOR );
                    if( !has_gap ) {
                        continue;
                    }
                    // Missing string-ID conversion can itself diagnose. Fall
                    // back before that conversion to preserve every message.
                    if( !below_submap || furn_str_id::NULL_ID().is_valid() ) {
                        const furn_id uniform_furniture = below_submap ?
                                                          below_submap->get_furn( point_sm_ms::zero ) : furn_id();
                        if( !below_submap || uniform_furniture.is_valid() ) {
                            if( below_submap && uniform_furniture.obj().has_flag( ter_furn_flag::TFLAG_SUN_ROOF_ABOVE ) ) {
                                continue;
                            }
                            for( int sx = 0; sx < SEEX; ++sx ) {
                                std::fill_n( &floor_cache[smx * SEEX + sx][smy * SEEY], SEEY, false );
                            }
                            no_floor_gaps = false;
                            continue;
                        }
                    }
                }
            }
            // END UNIFORM NATIVE FLOOR CACHE
            for( int sx = 0; sx < SEEX; ++sx ) {
                for( int sy = 0; sy < SEEY; ++sy ) {
                    point_sm_ms sp( sx, sy );
                    const ter_t &terrain = cur_submap->get_ter( sp ).obj();
                    if( terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR ) ||
                        terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR_WATER ) ||
                        terrain.has_flag( ter_furn_flag::TFLAG_GOES_DOWN ) ||
                        terrain.has_flag( ter_furn_flag::TFLAG_TRANSPARENT_FLOOR ) ) {
                        if( below_submap &&
                            below_submap->get_furn( sp ).obj().has_flag( ter_furn_flag::TFLAG_SUN_ROOF_ABOVE ) ) {
                            continue;
                        }
                        const point p( sx + smx * SEEX, sy + smy * SEEY );
                        floor_cache[p.x][p.y] = false;
                        no_floor_gaps = false;
                    }
                }
            }
        }
    }

    ch.floor_cache_dirty = false;
    return zlevels;
}
