// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/lightmap.cpp#L263-L342
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/lightmap.cpp#L263-L342
// Full Native vision-cache rules, preserving every existing float write.
bool map::build_vision_transparency_cache( int zlev )
{
    level_cache &map_cache = get_cache( zlev );

    // We copy the transparency_cache so we need to recalc if it's dirty
    if( map_cache.transparency_cache_dirty.none() /*&& map_cache.vision_transparency_cache_dirty.none()*/ ) {
        return false;
    }

    const cata::mdarray<float, point_bub_ms> &transparency_cache = map_cache.transparency_cache;
    cata::mdarray<float, point_bub_ms> &vision_transparency_cache = map_cache.vision_transparency_cache;

    // TODO: Should only copy if transparency_cache was dirty
    memcpy( &vision_transparency_cache, &transparency_cache, sizeof( transparency_cache ) );

    const Character &player_character = get_player_character();
    const tripoint_bub_ms p = player_character.pos_bub();
    const bool is_player_z = p.z() == zlev;

    bool dirty = false;

    if( is_player_z ) {
        // This segment handles vision when the player is crouching or prone. It only checks adjacent tiles.
        // If you change this, also consider creature::sees and map::obstacle_coverage.
        // TODO: Is fairly nonsense because it changes vision for everyone only (eg if you @ crouch behind the window W then the NPC N and monster M can't see each other bc the window is counted as opaque)
        // .N.
        // .@.
        // #W#
        // .M.
        const bool is_crouching = player_character.is_crouching();
        const bool low_profile = player_character.has_effect( effect_quadruped_full ) &&
                                 player_character.is_running();
        const bool is_prone = player_character.is_prone();
        if( is_crouching || is_prone || low_profile ) {
            for( const tripoint_bub_ms &loc : points_in_radius( p, 1 ) ) {
                if( loc != p && coverage( loc ) >= 30 ) {
                    // If we're crouching or prone behind an obstacle, we can't see past it.
                    dirty |= vision_transparency_cache[loc.x()][loc.y()] != LIGHT_TRANSPARENCY_SOLID;
                    vision_transparency_cache[loc.x()][loc.y()] = LIGHT_TRANSPARENCY_SOLID;
                }
            }
        }
    }

    // This segment handles blocking vision through TRANSLUCENT flagged terrain.
    // Traverse the submaps in order (else map::ter() calls get_submap each time)
    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const submap *cur_submap = get_submap_at_grid( tripoint_rel_sm{smx, smy, zlev} );
            if( cur_submap == nullptr ) {
                debugmsg( "Tried to build transparency cache at (%d,%d,%d) but the submap is not loaded", smx, smy,
                          zlev );
                continue;
            }
            if( !map_cache.transparency_cache_dirty[smx * MAPSIZE + smy] ) {
                continue;
            }
            // BEGIN UNIFORM NATIVE VISION TRANSPARENCY
            // A valid uniform definition without TRANSLUCENT cannot write
            // any tile in the original loop. Keep all other cases verbatim.
            if( cur_submap->is_uniform() ) {
                const ter_id uniform_terrain = cur_submap->get_ter( point_sm_ms::zero );
                if( static_cast<std::size_t>( uniform_terrain.to_i() ) < ter_t::count() &&
                    !uniform_terrain.obj().has_flag( ter_furn_flag::TFLAG_TRANSLUCENT ) ) {
                    continue;
                }
            }
            // END UNIFORM NATIVE VISION TRANSPARENCY
            for( int smi = 0; smi < SEEX; smi++ ) {
                for( int smj = 0; smj < SEEY; smj++ ) {
                    if( cur_submap->get_ter( point_sm_ms{smi, smj} ).obj().has_flag(
                            ter_furn_flag::TFLAG_TRANSLUCENT ) ) {
                        const int i = smi + ( smx * SEEX );
                        const int j = smj + ( smy * SEEY );
                        dirty |= vision_transparency_cache[i][j] != LIGHT_TRANSPARENCY_SOLID;
                        vision_transparency_cache[i][j] = LIGHT_TRANSPARENCY_SOLID;
                    }
                }
            }
        }
    }

    // The tile player is standing on should always be visible
    // Shouldn't this be handled in the player's seen cache instead??
    if( is_player_z && inbounds( p ) ) {
        vision_transparency_cache[p.x()][p.y()] = LIGHT_TRANSPARENCY_OPEN_AIR;
    }

    map_cache.transparency_cache_dirty.reset();
    return dirty;
}
