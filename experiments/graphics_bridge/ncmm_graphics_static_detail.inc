// Callback-scoped Host static-detail extraction. Native definitions and the
// caller's shape cache are borrowed only until this helper returns. The BODY
// and legacy/world-null paths retain their original extraction below it.
template<class TerrainShape,class CopyId>
bool graphics_populate_static_detail(map &here,const tripoint_bub_ms &p,ncmm_graphics_cell_v2 &d,
    const ncmm_graphics_multilevel_scene_v1 *world,const std::vector<std::array<float,3>> *facade_lights,
    size_t i,TerrainShape terrain_shape,CopyId copy_id)
{
    if(!world||(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE))return false;
    const bool structure=(world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE)!=0;
    // Keep the original terrain-before-RGB order when both are consumed.
    // A lower-only/top-only slab never describes the terrain definition.
    const ter_t *terrain=structure?&here.ter(p).obj():nullptr;
    d.terrain_shape=d.furniture_shape=NCMM_SHAPE_FLAT;
    // A neutral structural surface carries no hidden room texture.
    // The shared slab collector uses its untextured fallback material.
    if(facade_lights&&structure){const auto &rgb=(*facade_lights)[i];d.light_r=rgb[0];d.light_g=rgb[1];d.light_b=rgb[2];}
    else if(world->coordinates[i].flags&(NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_STRUCTURE_VISIBLE)){
        const auto rgb=graphics_sample_rgb(p);d.light_r=rgb[0];d.light_g=rgb[1];d.light_b=rgb[2];
    }else{const auto &lower=world->coordinates[i].lower_light;d.light_r=lower[0];d.light_g=lower[1];d.light_b=lower[2];}
    if(structure){
        const ter_t &ter=*terrain;
        d.terrain_shape=terrain_shape(ter);
        // Opaque furniture/gas in this voxel is not an opaque window
        // panel. The terrain itself owns its opening and cover state.
        d.terrain_flags=ter.transparent?uint32_t(NCMM_SURFACE_TRANSPARENT):0u;
        d.terrain_flags|=graphics_structure_connections(p,d.terrain_shape,d.orientation,terrain_shape);
        copy_id(d.ground,"t_floor");
    }
    return true;
}
