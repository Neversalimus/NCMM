// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L10515-L10571
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L10527-L10583
// Full Native outside-cache rules, including the one-cell indoor halo.
void map::build_outside_cache( const int zlev )
{
    auto *ch_lazy = get_cache_lazy( zlev );
    if( !ch_lazy || !ch_lazy->outside_cache_dirty ) {
        return;
    }
    level_cache &ch = *ch_lazy;

    // Make a bigger cache to avoid bounds checking
    // We will later copy it to our regular cache
    const size_t padded_w = MAPSIZE_X + 2;
    const size_t padded_h = MAPSIZE_Y + 2;
    cata::mdarray<bool, point_bub_ms, padded_w, padded_h> padded_cache;

    auto &outside_cache = ch.outside_cache;
    if( zlev < 0 ) {
        std::uninitialized_fill_n(
            &outside_cache[0][0], MAPSIZE_X * MAPSIZE_Y, false );
        return;
    }

    padded_cache.fill( true );

    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const submap *cur_submap = get_submap_at_grid( tripoint_rel_sm{ smx, smy, zlev } );
            if( cur_submap == nullptr ) {
                debugmsg( "Tried to build outside cache at (%d,%d,%d) but the submap is not loaded", smx, smy,
                          zlev );
                continue;
            }

            // BEGIN UNIFORM NATIVE OUTSIDE CACHE
            // All 144 predicates are identical for valid uniform definitions.
            // Preserve terrain-first short circuit, including missing f_null.
            if( cur_submap->is_uniform() ) {
                const ter_id uniform_terrain = cur_submap->get_ter( point_sm_ms::zero );
                if( static_cast<std::size_t>( uniform_terrain.to_i() ) < ter_t::count() ) {
                    bool uniform_indoors = uniform_terrain.obj().has_flag( ter_furn_flag::TFLAG_INDOORS );
                    bool resolved = uniform_indoors;
                    if( !uniform_indoors && furn_str_id::NULL_ID().is_valid() ) {
                        const furn_id uniform_furniture = cur_submap->get_furn( point_sm_ms::zero );
                        if( uniform_furniture.is_valid() ) {
                            uniform_indoors = uniform_furniture.obj().has_flag( ter_furn_flag::TFLAG_INDOORS );
                            resolved = true;
                        }
                    }
                    if( resolved ) {
                        if( uniform_indoors ) {
                            // Union of the original overlapping 3x3 stamps,
                            // retaining the one-cell halo and padded stride.
                            for( int sx = 0; sx < SEEX + 2; ++sx ) {
                                std::fill_n( &padded_cache[smx * SEEX + sx][smy * SEEY], SEEY + 2, false );
                            }
                        }
                        continue;
                    }
                }
            }
            // END UNIFORM NATIVE OUTSIDE CACHE
            for( int sx = 0; sx < SEEX; ++sx ) {
                for( int sy = 0; sy < SEEY; ++sy ) {
                    point_sm_ms sp( sx, sy );
                    if( cur_submap->get_ter( sp ).obj().has_flag( ter_furn_flag::TFLAG_INDOORS ) ||
                        cur_submap->get_furn( sp ).obj().has_flag( ter_furn_flag::TFLAG_INDOORS ) ) {
                        const point p( sx + smx * SEEX, sy + smy * SEEY );
                        // Add 1 to both coordinates, because we're operating on the padded cache
                        for( int dx = 0; dx <= 2; dx++ ) {
                            for( int dy = 0; dy <= 2; dy++ ) {
                                padded_cache[p.x + dx][p.y + dy] = false;
                            }
                        }
                    }
                }
            }
        }
    }

    // Copy the padded cache back to the proper one, but with no padding
    for( int x = 0; x < SEEX * my_MAPSIZE; x++ ) {
        std::copy_n( &padded_cache[x + 1][1], SEEX * my_MAPSIZE, &outside_cache[x][0] );
    }

    ch.outside_cache_dirty = false;
}
