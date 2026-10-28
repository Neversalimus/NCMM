// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L9822-L9886
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L9834-L9898
// BEGIN FROZEN NATIVE ACTUALIZE
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
// END FROZEN NATIVE ACTUALIZE

// Frozen early-return prefixes from those same exact primary sources.
#if 0
// BEGIN FROZEN NATIVE ACTUALIZE PREFIX grow_plant
constexpr const char *prefix_grow_plant = R"NCMM(void map::grow_plant( const tripoint_bub_ms &p )
{
    const furn_t &furn = this->furn( p ).obj();
    if( !furn.has_flag( ter_furn_flag::TFLAG_PLANT ) ) {
        return;
    }
)NCMM";
// END FROZEN NATIVE ACTUALIZE PREFIX grow_plant
// BEGIN FROZEN NATIVE ACTUALIZE PREFIX restock_fruits
constexpr const char *prefix_restock_fruits = R"NCMM(void map::restock_fruits( const tripoint_bub_ms &p, const time_duration &time_since_last_actualize )
{
    const ter_t &ter = this->ter( p ).obj();
    if( !ter.has_flag( ter_furn_flag::TFLAG_HARVESTED ) ) {
        return; // Already harvestable. Do nothing.
    }
)NCMM";
// END FROZEN NATIVE ACTUALIZE PREFIX restock_fruits
// BEGIN FROZEN NATIVE ACTUALIZE PREFIX produce_sap
constexpr const char *prefix_produce_sap = R"NCMM(void map::produce_sap( const tripoint_bub_ms &p, const time_duration &time_since_last_actualize )
{
    if( time_since_last_actualize <= 0_turns ) {
        return;
    }

    if( !( ter( p ) == ter_t_tree_maple_tapped ) ) {
        return;
    }

)NCMM";
// END FROZEN NATIVE ACTUALIZE PREFIX produce_sap
// BEGIN FROZEN NATIVE ACTUALIZE PREFIX rad_scorch
constexpr const char *prefix_rad_scorch = R"NCMM(void map::rad_scorch( const tripoint_bub_ms &p, const time_duration &time_since_last_actualize )
{
    const int rads = get_radiation( p );
    if( rads == 0 ) {
        return;
    }

)NCMM";
// END FROZEN NATIVE ACTUALIZE PREFIX rad_scorch
// BEGIN FROZEN NATIVE ACTUALIZE PREFIX decay_cosmetic_fields
constexpr const char *prefix_decay_cosmetic_fields = R"NCMM(void map::decay_cosmetic_fields( const tripoint_bub_ms &p,
                                 const time_duration &time_since_last_actualize )
{
    for( auto &pr : field_at( p ) ) {
)NCMM";
// END FROZEN NATIVE ACTUALIZE PREFIX decay_cosmetic_fields
#endif
