// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L9822-L9886
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L9834-L9898
void map::actualize( const tripoint_rel_sm &grid )
{
    submap *const tmpsub = get_submap_at_grid( grid );
    if( tmpsub == nullptr ) {
        debugmsg( "Actualize called on null submap (%d,%d,%d)", grid.x(), grid.y(), grid.z() );
        return;
    }

    for( const std::unique_ptr<vehicle> &veh : tmpsub->vehicles ) {
        // spill out items too large, MIGRATION pockets etc from vehicle parts
        for( const vpart_reference &vp : veh->get_all_parts() ) {
            const item &base_const = vp.part().get_base();
            const_cast<item &>( base_const ).overflow( *this, vp.pos_bub( *this ) );
        }
        veh->refresh( );
    }

    const time_duration time_since_last_actualize = calendar::turn - tmpsub->last_touched;
    const bool do_funnels = grid.z() >= 0;

    // check spoiled stuff, and fill up funnels while we're at it
    process_items_in_vehicles( *tmpsub );
    process_items_in_submap( *tmpsub, grid );
    explosion_handler::process_explosions();
    // BEGIN PROVEN UNIFORM UNDERGROUND NOOP
    // Run every vehicle/item/explosion callback first. Revalidate the mapping
    // afterwards; an earlier callback can replace or materialize the submap.
    // Keep out-of-bounds and non-Z-level alias behavior on the Native path.
    if( zlevels && grid.z() >= -OVERMAP_DEPTH && grid.z() < 0 &&
        grid.x() >= 0 && grid.x() < my_MAPSIZE && grid.y() >= 0 && grid.y() < my_MAPSIZE &&
        get_submap_at_grid( grid ) == tmpsub && tmpsub->is_uniform() ) {
        const ter_id uniform_terrain = tmpsub->get_ter( point_sm_ms::zero );
        // Preserve invalid-definition/conversion diagnostics in the full loop.
        // The pinned ter_id::is_valid() has no external definition.
        if( static_cast<std::size_t>( uniform_terrain.to_i() ) < ter_t::count() &&
            furn_str_id::NULL_ID().is_valid() ) {
            const furn_id uniform_furniture = tmpsub->get_furn( point_sm_ms::zero );
            if( uniform_furniture.is_valid() ) {
                const ter_t &terrain = uniform_terrain.obj();
                const furn_t &furniture = uniform_furniture.obj();
                // Uniform storage supplies no explicit traps or radiation.
                // All remaining helpers return before effects/RNG here. Funnels
                // are disabled underground. Inspect the mutable shared field:
                // uniform storage alone does not prove that object is empty.
                if( furniture.emissions.empty() && terrain.emissions.empty() && terrain.trap == tr_null &&
                    !furniture.has_flag( ter_furn_flag::TFLAG_PLANT ) &&
                    !terrain.has_flag( ter_furn_flag::TFLAG_HARVESTED ) &&
                    uniform_terrain != ter_t_tree_maple_tapped ) {
                    auto &fields = tmpsub->get_field( point_sm_ms::zero );
                    if( fields.begin() == fields.end() ) {
                        tmpsub->last_touched = calendar::turn;
                        return;
                    }
                }
            }
        }
    }
    // END PROVEN UNIFORM UNDERGROUND NOOP
    for( int x = 0; x < SEEX; x++ ) {
        for( int y = 0; y < SEEY; y++ ) {
            const tripoint_bub_ms pnt =  rebase_bub( coords::project_to<coords::ms>( grid ) + point( x, y ) );
            const point_sm_ms p( x, y );
            const furn_t &furn = *this->furn( pnt );
            const ter_t &terr = *this->ter( pnt );
            if( !furn.emissions.empty() ) {
                field_furn_locs.push_back( pnt );
            }
            if( !terr.emissions.empty() ) {
                field_ter_locs.push_back( pnt );
            }

            const trap_id trap_here = tmpsub->get_trap( p );
            if( trap_here != tr_null ) {
                traplocs[trap_here.to_i()].push_back( pnt );
            }
            const ter_t &ter = tmpsub->get_ter( p ).obj();
            if( ter.trap != tr_null ) {
                traplocs[ter.trap.to_i()].push_back( pnt );
            }

            if( do_funnels ) {
                fill_funnels( pnt, tmpsub->last_touched );
            }

            grow_plant( pnt );

            restock_fruits( pnt, time_since_last_actualize );

            produce_sap( pnt, time_since_last_actualize );

            rad_scorch( pnt, time_since_last_actualize );

            decay_cosmetic_fields( pnt, time_since_last_actualize );
        }
    }

    // the last time we touched the submap, is right now.
    tmpsub->last_touched = calendar::turn;
}
