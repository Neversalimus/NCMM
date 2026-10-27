// Adapted from Cataclysm: Dark Days Ahead for opt-in diagnostic boundaries.
// Copyright Cataclysm: DDA contributors; CC-BY-SA-3.0:
// https://creativecommons.org/licenses/by-sa/3.0/
// Primary function bodies are identical at both exact pins:
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/game.cpp#L10228-L10321
// Git blob 6014e878bec6236dc0af13f96639cb008fd9fdf8.
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/game.cpp#L10229-L10322
// Git blob 91fb64810998668fb70671a7df6e4a254f66fbf0.
// Native calls, branches and arguments retain their order. The existing
// Stage14 cache-reset scope stays around exactly the same Native invalidation.
point_rel_sm game::update_map( int &x, int &y, bool z_level_changed )
{
    ncmm_graphics_move_diagnostics::update_scope ncmm_move_scope( x, y, z_level_changed );
    map &here = get_map();

    point_rel_sm shift;

    while( x < HALF_MAPSIZE_X ) {
        x += SEEX;
        shift.x()--;
    }
    while( x >= HALF_MAPSIZE_X + SEEX ) {
        x -= SEEX;
        shift.x()++;
    }
    while( y < HALF_MAPSIZE_Y ) {
        y += SEEY;
        shift.y()--;
    }
    while( y >= HALF_MAPSIZE_Y + SEEY ) {
        y -= SEEY;
        shift.y()++;
    }
    if( ncmm_move_scope ) ncmm_move_scope.route( shift.x(), shift.y() );

    if( shift == point_rel_sm::zero ) {
        // adjust player position
        u.setpos( here, tripoint_bub_ms( x, y, here.get_abs_sub().z() ) );
        if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::zero_position );
        if( z_level_changed ) {
            // Update what parts of the world map we can see
            // We may be able to see farther now that the z-level has changed.
            update_overmap_seen();
        }
        if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::zero_overmap );
        // Not actually shifting the submaps, all the stuff below would do nothing
        return point_rel_sm::zero;
    }

    // this handles loading/unloading submaps that have scrolled on or off the viewport
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    inclusive_rectangle<point_rel_sm> size_1( {-1, -1}, { 1, 1 } );
    point_rel_sm remaining_shift = shift;
    while( remaining_shift != point_rel_sm::zero ) {
        point_rel_sm this_shift = clamp( remaining_shift, size_1 );
        here.shift( point_rel_sm( this_shift ) );
        remaining_shift -= this_shift;
    }
    if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::shift );

    // Shift monsters
    shift_monsters( { shift, 0 } );
    const point_rel_ms shift_ms = coords::project_to<coords::ms>( shift );
    u.shift_destination( -shift_ms );

    // Shift NPCs
    for( auto it = critter_tracker->active_npc.begin(); it != critter_tracker->active_npc.end(); ) {
        ( *it )->shift( shift );
        const tripoint_bub_ms pos = ( *it )->pos_bub( here );

        // TODO: This could probably be replaced by an inbounds check.
        if( pos.x() < 0 || pos.x() >= MAPSIZE_X ||
            pos.y() < 0 || pos.y() >= MAPSIZE_Y ) {
            //Remove the npc from the active list. It remains in the overmap list.
            ( *it )->on_unload();
            it = critter_tracker->active_npc.erase( it );
        } else {
            ++it;
        }
    }

    scent.shift( shift_ms );

    // Also ensure the player is on current z-level
    // m.get_abs_sub().z should later be removed, when there is no longer such a thing
    // as "current z-level"
    u.setpos( here, tripoint_bub_ms( x, y, here.get_abs_sub().z() ) );

    // Only do the loading after all coordinates have been shifted.

    // Check for overmap saved npcs that should now come into view.
    // Put those in the active list.
    load_npcs();
    if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::rebase_load_npcs );

    // Make sure map cache is consistent since it may have shifted.
    for( int zlev = -OVERMAP_DEPTH; zlev <= OVERMAP_HEIGHT; ++zlev ) {
        const ncmm::graphics_native_cache_reset ncmm_source_reset( &here, zlev );
        here.invalidate_map_cache( zlev );
    }
    if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::invalidate );
    here.build_map_cache( here.get_abs_sub().z() );
    if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::build );

    // Spawn monsters if appropriate
    // This call will generate new monsters in addition to loading, so it's placed after NPC loading
    here.spawn_monsters( false ); // Static monsters
    if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::spawn );

    // Update what parts of the world map we can see
    update_overmap_seen();
    if( ncmm_move_scope ) ncmm_move_scope.mark( ncmm_graphics_move_diagnostics::boundary::overmap );

    return shift;
}
