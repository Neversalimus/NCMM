// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L8879-L8989
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L8890-L9000
// The adaptation supplies each pin's exact original cursor expression.
void map::reconcile_loaded_items( const reconcile_scope scope )
{
    item_wakeup_manager &wakeups = get_item_wakeups();
    craft_reservation_index &index = get_craft_reservations();
    if( scope == reconcile_scope::full_rebuild ) {
        index.clear();
    }

    std::vector<int64_t> seen;
    auto reconcile_one = [&wakeups, &index, &seen]( item_location loc ) {
        item *it = loc.get_item();
        if( it == nullptr ) {
            return;
        }
        if( it->is_craft() ) {
            // On the token as well as the live step: the stale branch keeps the token and
            // clears passive_started_at, and that record still needs cleaning.
            const bool live = it->get_passive_started_at() != calendar::before_time_starts;
            const int64_t token = it->peek_reservation_owner_token();
            if( live || token != 0 ) {
                // A pre-feature save was stamped before reservations existed, so nothing
                // would ever call acquisition and the step would run unreserved to
                // completion.  The wakeup rebuild below picks up the cursor set here.
                if( live && token == 0 &&
                    it->get_env_check_at() == calendar::before_time_starts ) {
                    it->set_env_check_at( calendar::turn );
                }
                index.rebuild_for_craft( loc );
                if( it->peek_reservation_owner_token() != 0 ) {
                    seen.push_back( it->peek_reservation_owner_token() );
                }
            }
        }
        wakeups.rebuild_for_item( loc );
    };

    auto walk_recursive = [&reconcile_one]( auto & self, item_location loc ) -> void {
        if( !loc || loc.get_item() == nullptr )
        {
            return;
        }
        reconcile_one( loc );
        for( item *child : loc.get_item()->all_items_top() )
        {
            self( self, item_location( loc, child ) );
        }
    };

    // Submaps rather than tiles, and the z range guarded by zlevels: get_nonant drops the z
    // index on a map that does not support z levels, so every level would alias the one the
    // map holds and the walk would repeat itself once per level.
    for( int gridx = 0; gridx < my_MAPSIZE; gridx++ ) {
        for( int gridy = 0; gridy < my_MAPSIZE; gridy++ ) {
            const int zmin = zlevels ? -OVERMAP_DEPTH : abs_sub.z();
            const int zmax = zlevels ? OVERMAP_HEIGHT : abs_sub.z();
            for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                const tripoint_rel_sm grid( gridx, gridy, gridz );
                submap *const sm = get_submap_at_grid( grid );
                if( sm == nullptr ) {
                    continue;
                }
                // BEGIN EMPTY UNIFORM NATIVE ITEMS
                // Uniform getters share the mutable sentinel. Only an empty
                // one proves all 144 tile walks have no callback to execute.
                // Recheck each submap after any earlier item callbacks.
                if( sm->is_uniform() && sm->get_items( point_sm_ms::zero ).empty() ) {
                    continue;
                }
                // END EMPTY UNIFORM NATIVE ITEMS
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        for( item &it : sm->get_items( { sx, sy } ) ) {
                            const tripoint_bub_ms p( sx + gridx * SEEX,
                                                     sy + gridy * SEEY, gridz );
                            item_location loc( NCMM_GRAPHICS_RECONCILE_MAP_CURSOR( p ), &it );
                            walk_recursive( walk_recursive, loc );
                        }
                    }
                }
            }
        }
    }

    // Every part that holds items, not only the cargo part a tile resolves to, or a second
    // cargo part at the same mount is never walked.
    for( wrapped_vehicle &wv : get_vehicles() ) {
        if( wv.v == nullptr ) {
            continue;
        }
        for( const vpart_reference &vpr : wv.v->get_all_parts() ) {
            vehicle_part &vp = wv.v->part( vpr.part_index() );
            for( item &it : wv.v->get_items( vp ) ) {
                item_location loc( vehicle_cursor( *wv.v, vpr.part_index() ), &it );
                walk_recursive( walk_recursive, loc );
            }
        }
    }

    auto walk_character = [&reconcile_one]( Character & c ) {
        // all_items_loc() is already recursive; reconcile per location directly.
        for( item_location &loc : c.all_items_loc() ) {
            if( loc && loc.get_item() != nullptr ) {
                reconcile_one( loc );
            }
        }
    };
    walk_character( get_avatar() );
    for( npc &n : g->all_npcs() ) {
        walk_character( n );
    }

    // Crafts this pass did not walk are left alone: the index retains earlier entries.
    for( const int64_t token : seen ) {
        const craft_reservation_index::record *rec = index.find( token );
        if( rec != nullptr && rec->empty() ) {
            index.erase( token );
        }
    }
}
