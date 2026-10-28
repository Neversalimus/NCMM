// The Host's original bounded, terrain-only sampler. The diagnostic observer
// reuses the same reads; no furniture, field, vehicle or discovery data enters.
static uint32_t graphics_world_terrain_stair_flags(map &here,int ax,int ay,int z,
        int ox,int oy,int width,int height,int minimum_z,int maximum_z)
{
    const int64_t x=int64_t(ax)-ox,y=int64_t(ay)-oy;
    if(x<0||y<0||x>=width||y>=height||z<minimum_z||z>maximum_z)return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);
    const auto p=here.get_bub(tripoint_abs_ms(ax,ay,z));
    if(!here.ncmm_graphics_loaded(p))return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);
    const auto tile=std::as_const(here).maptile_at(p);const auto &ter=tile.get_ter_t();
    ncmm_world::traits t{};t.loaded=true;
    t.no_floor=ter.has_flag(ter_furn_flag::TFLAG_NO_FLOOR);t.no_floor_water=ter.has_flag(ter_furn_flag::TFLAG_NO_FLOOR_WATER);
    if(!(here.supports_zlevels()&&z>=-OVERMAP_DEPTH+1))t.no_floor=t.no_floor_water=false;
    t.transparent_floor=ter.has_flag(ter_furn_flag::TFLAG_TRANSPARENT_FLOOR);t.z_transparent=ter.has_flag(ter_furn_flag::TFLAG_Z_TRANSPARENT);
    t.goes_up=ter.has_flag(ter_furn_flag::TFLAG_GOES_UP);t.goes_down=ter.has_flag(ter_furn_flag::TFLAG_GOES_DOWN);
    t.ramp=ter.has_flag(ter_furn_flag::TFLAG_RAMP);t.ramp_up=ter.has_flag(ter_furn_flag::TFLAG_RAMP_UP);
    t.ramp_down=ter.has_flag(ter_furn_flag::TFLAG_RAMP_DOWN);t.ramp_end=ter.has_flag(ter_furn_flag::TFLAG_RAMP_END);
    t.difficult_z=ter.has_flag(ter_furn_flag::TFLAG_DIFFICULT_Z);t.opaque=!ter.transparent;
    const auto shape=ncmm_graphics_shapes::terrain(ter.id.str()+" "+ter.looks_like,graphics_traits(ter,!ter.close.is_null()));
    t.partial_support=ncmm_graphics_shapes::partial_support(shape,z);
    return ncmm_world::semantics(t);
}
