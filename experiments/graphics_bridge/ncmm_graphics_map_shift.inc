// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L9059-L9185
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L9070-L9197
// Native statements retain their exact order. Only the later pin's memory
// sweep statement differs; the caller binds that one private macro per pin.
// Timings include probe overhead, callbacks and local cleanup, never pure CPU.
void map::shift( const point_rel_sm &sp )
{
    ncmm_graphics_move_diagnostics::shift_scope ncmm_shift_scope( sp.x(), sp.y() );
    if( !zlevels ) {
        debugmsg( "map::shift called from map that doesn't support Z levels" );
        return;
    }

    // Special case of 0-shift; refresh the map
    if( sp == point_rel_sm::zero ) {
        return; // Skip this?
    }

    if( std::abs( sp.x() ) > 1 || std::abs( sp.y() ) > 1 ) {
        debugmsg( "map::shift called with a shift of more than one submap" );
        return;
    }

    if( ncmm_shift_scope ) ncmm_shift_scope.accept();
    const tripoint_abs_sm abs = get_abs_sub();
    std::vector<tripoint_rel_sm> loaded_grids;

    const int zmin = -OVERMAP_DEPTH;
    const int zmax = OVERMAP_HEIGHT;

    const int x_start = sp.x() >= 0 ? 0 : my_MAPSIZE - 1;
    const int x_stop = sp.x() >= 0 ? my_MAPSIZE : -1;
    const int x_step = sp.x() >= 0 ? 1 : -1;
    const int y_start = sp.y() >= 0 ? 0 : my_MAPSIZE - 1;
    const int y_stop = sp.y() >= 0 ? my_MAPSIZE : -1;
    const int y_step = sp.y() >= 0 ? 1 : -1;

    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::setup );
    // Have to run on_unload before changing the abs_sub for the map.
    // TODO: can probably skip a bunch of iteration here.
    for( int gridx = x_start; gridx != x_stop; gridx += x_step ) {
        for( int gridy = y_start; gridy != y_stop; gridy += y_step ) {
            // The sp.x() and sp.y() check here establishes that we are moving
            // in the x or y direction respectively. If we are not moving in the x direction,
            // we don't want to call on_unload on grid points where gridx == x_start.
            if( ( sp.x() && gridx == x_start ) || ( sp.y() && gridy == y_start ) ) {
                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    const tripoint_rel_sm grid( gridx, gridy, gridz );
                    on_unload( grid );
                }
            }
        }
    }
    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::unload );
    set_abs_sub( abs + sp );

    g->shift_destination_preview( { -sp.x() * SEEX, -sp.y() * SEEY } );

    shift_traps( sp );

    vehicle *remoteveh = g->remoteveh();

    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::origin_traps_remote );
    for( int gridz = zmin; gridz <= zmax; gridz++ ) {
        level_cache *cache = get_cache_lazy( gridz );
        if( !cache ) {
            continue;
        }
        for( vehicle *veh : cache->vehicle_list ) {
            veh->zones_dirty = true;
        }
    }

    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::vehicle_zones );
    // Shift the map sx submaps to the right and sy submaps down.
    // sx and sy should never be bigger than +/-1.
    // absx and absy are our position in the world, for saving/loading purposes.
    clear_vehicle_level_caches();

    for( int gridz = zmin; gridz <= zmax; gridz++ ) {
        // Clear vehicle list and rebuild after shift
        // mlangsdorf 2020 - this is kind of insane, building the cache is not free, why are
        // we doing this?
        clear_vehicle_list( gridz );
        level_cache *cache = get_cache_lazy( gridz );
        if( cache ) {
            shift_bitset_cache<MAPSIZE_X, SEEX>( cache->map_memory_cache_dec, sp );
            shift_bitset_cache<MAPSIZE_X, SEEX>( cache->map_memory_cache_ter, sp );
            NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(cache)
            shift_bitset_cache<MAPSIZE, 1>( cache->field_cache, sp );
        }
    }

    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::cache_rebase );
    for( int gridx = x_start; gridx != x_stop; gridx += x_step ) {
        for( int gridy = y_start; gridy != y_stop; gridy += y_step ) {
            if( gridx + sp.x() != x_stop && gridy + sp.y() != y_stop ) {
                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    const tripoint_rel_sm grid( gridx, gridy, gridz );

                    copy_grid( grid, grid + sp );
                    submap *const cur_submap = get_submap_at_grid( grid );
                    if( cur_submap == nullptr ) {
                        debugmsg( "Tried to update vehicle list at %s but the submap is not loaded", grid.to_string() );
                        continue;
                    }
                    update_vehicle_list( cur_submap, gridz );
                }
            } else {
                loadn( { gridx, gridy }, true );

                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    loaded_grids.emplace_back( gridx, gridy, gridz );
                }
            }
        }
    }

    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::grid_copy_load );
    rebuild_vehicle_level_caches();

    g->setremoteveh( remoteveh );

    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::vehicle_levels_remote );
    if( !support_cache_dirty.empty() ) {
        // BEGIN REUSED NATIVE SUPPORT NODES
        ncmm_graphics_support_cache::rebase( support_cache_dirty, point( -sp.x() * SEEX, -sp.y() * SEEY ) );
        // END REUSED NATIVE SUPPORT NODES
    }
    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::support_rebase );
    // actualize after loading all submaps to prevent errors
    // with entities at the edges
    for( tripoint_rel_sm loaded_grid : loaded_grids ) {
        actualize( loaded_grid );
    }
    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::actualize );
    if( !loaded_grids.empty() ) {
        // A shift walks the bubble it now has, so the crafts it scrolled away from are
        // not seen and must keep the claims their leases still cover.
        reconcile_loaded_items( reconcile_scope::additive );
    }
    if( ncmm_shift_scope ) ncmm_shift_scope.mark( ncmm_graphics_move_diagnostics::shift_boundary::reconcile );
}
