// Local diagnostic predicate, shared verbatim with the pure smoke regression.
// Native includes this at function scope; no module/Host ABI or permission is
// added here. Keep the architecture branch identical to the original observer.
const auto ncmm_multilevel_observer_structure = [](const ncmm_world_cell_v1 &cell,
        const ncmm_graphics_cell_v1 &appearance, const ncmm_graphics_cell_v2 &detail,
        const ncmm_world::terrain_stair_proof &terrain, const char *physical_terrain) {
    if(ncmm_graphics_shapes::structural_surface(detail.terrain_shape))return true;
    constexpr uint32_t excluded=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_UNKNOWN|NCMM_WORLD_RAMP|NCMM_WORLD_LADDER|NCMM_WORLD_RAMP_END;
    constexpr uint32_t ordinary=NCMM_WORLD_UP|NCMM_WORLD_DOWN|NCMM_WORLD_CONNECTED_UP|NCMM_WORLD_CONNECTED_DOWN;
    if(detail.terrain_shape!=NCMM_SHAPE_FLAT||!(cell.flags&NCMM_WORLD_STRUCTURE_VISIBLE)||(cell.flags&excluded)||
       !(terrain.navigation&(NCMM_WORLD_UP|NCMM_WORLD_DOWN))||(terrain.navigation&~ordinary)||
       (terrain.terrain_opaque&~uint32_t(NCMM_WORLD_OPAQUE)))return false;
    return (cell.flags&ncmm_world::terrain_stair_navigation_mask)==terrain.navigation&&cell.direction==terrain.direction&&
           (cell.flags&NCMM_WORLD_OPAQUE)==terrain.terrain_opaque&&physical_terrain&&
           std::strcmp(appearance.terrain,physical_terrain)==0&&!appearance.furniture[0]&&!appearance.item[0]&&!appearance.creature[0]&&
           !(appearance.flags&NCMM_GFX_VEHICLE)&&!detail.field[0]&&detail.field_shape==NCMM_SHAPE_FLAT&&detail.furniture_shape==NCMM_SHAPE_FLAT;
};
// Catch only the test observer's inspection. A real provider exception leaves
// this function and reaches the unchanged production callback quarantine.
const auto ncmm_multilevel_observer_invoke = [](const ncmm_graphics_multilevel_scene_v1 *world,
        const auto &inspect,ncmm_graphics_render_multilevel_v1 provider,void *user,std::string &failure)->int {
    failure.clear();
    if(!world){failure="multilevel_real_world_contract_invalid";return 0;}
    try{inspect(*world);}catch(const std::exception &error){failure=error.what();return 0;}
    return provider(world,user);
};
