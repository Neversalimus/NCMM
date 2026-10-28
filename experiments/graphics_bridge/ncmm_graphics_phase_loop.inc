// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// Exact supported pins: 3f7fb352bf492ba521bd9408a0c9f6ce239e8d83 and
// 074aa98bd5be3de4c35f154082db32a0e63bb0f1; src/map.cpp and src/mapgen.cpp.
        // BEGIN PROVEN UNIFORM PHASE NOOP
        // This runs after every load/vehicle/tree callback and weather setup.
        // Original-terrain history is independent of uniform storage: restoring
        // it must remain on the full Native path, even for one recorded cell.
        if( zlevels && z >= -OVERMAP_DEPTH && z <= OVERMAP_HEIGHT &&
            grid.x() >= 0 && grid.x() < my_MAPSIZE && grid.y() >= 0 && grid.y() < my_MAPSIZE &&
            get_submap_at_grid( tripoint_rel_sm{ grid.x(), grid.y(), z } ) == tmpsub &&
            tmpsub && tmpsub->is_uniform() && !tmpsub->ncmm_graphics_has_original_terrain() ) {
            const ter_id ncmm_uniform_terrain = tmpsub->get_ter( point_sm_ms::zero );
            // Preserve invalid integer/string conversion diagnostics in Native.
            if( static_cast<std::size_t>( ncmm_uniform_terrain.to_i() ) < ter_t::count() ) {
                const ter_str_id &ncmm_uniform_name = ncmm_uniform_terrain.id();
                if( ncmm_uniform_name.is_valid() && ncmm_uniform_name.obj().phase_method.empty() ) {
                    continue;
                }
            }
        }
        // END PROVEN UNIFORM PHASE NOOP
        for( int sx = 0; sx < SEEX; ++sx ) {
            for( int sy = 0; sy < SEEX; ++sy ) {
                temp_based_phase_change_at( sm_origin + point( sx, sy ), wgen );
            }
        }
