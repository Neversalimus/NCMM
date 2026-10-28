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

// Borrow only this callback's canonical sparse coordinates. The four cursors
// own integer positions, never Native objects or data retained across frames.
class graphics_substrate_cursor {
    const ncmm_graphics_multilevel_scene_v1 *world;
    std::array<uint32_t,4> cursors{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}};
public:
    explicit graphics_substrate_cursor(const ncmm_graphics_multilevel_scene_v1 *source):world(source){}
    // Keep the search implementation outside the common cell traversal.
#if defined(_MSC_VER)
    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
    __attribute__((noinline))
#endif
    int find(int x,int y,int z,int dx,int dy){
        const int64_t ax=int64_t(world->origin_x)+x,ay=int64_t(world->origin_y)+y;
        if(ax<INT32_MIN||ax>INT32_MAX||ay<INT32_MIN||ay>INT32_MAX)return -1;
        const auto wanted=std::make_tuple(z,int32_t(ay),int32_t(ax));
        const size_t side=dy>0?0:dy<0?1:dx>0?2:3;
        const auto *positions=world->coordinates;const uint32_t count=world->cell_count;
        uint32_t lo=cursors[side],hi=count;
        if(lo==UINT32_MAX)lo=0;
        else{
            if(lo==count)return -1;
            if(!(ncmm_world::order(positions[lo])<wanted))
                return ncmm_world::order(positions[lo])==wanted?int(lo):-1;
            size_t step=1;
            while(step<size_t(count-lo)&&ncmm_world::order(positions[lo+step])<wanted)step*=2;
            hi=lo+uint32_t(std::min(step+1,size_t(count-lo)));
            lo+=uint32_t(step/2);
        }
        const auto *found=std::lower_bound(positions+lo,positions+hi,wanted,
            [](const ncmm_world_cell_v1 &cell,const auto &key){return ncmm_world::order(cell)<key;});
        const auto at=uint32_t(found-positions);cursors[side]=at;
        return at<count&&ncmm_world::order(positions[at])==wanted?int(at):-1;
    }
};

// Appearance is published only after this pass. Use the canonical sparse XYZ
// list directly; an indexed adapter would inspect the still-empty appearance.
// Only already-visible snapshot cells may supply a substrate texture.
template<class CopyId>
void graphics_populate_substrates(const ncmm_graphics_scene_v1 &base,std::vector<ncmm_graphics_cell_v2> &details,
    const ncmm_graphics_multilevel_scene_v1 *world,CopyId copy_id)
{
    const ncmm_world::coordinates world_coordinates(world);
    graphics_substrate_cursor increasing(world);
    const bool resume=world&&base.cell_count>=256;
    size_t first=0;
    if(resume){
        for(;first<base.cell_count;++first)
            if((base.cells[first].flags&NCMM_GFX_VISIBLE)&&ncmm_graphics_shapes::vertical(details[first].terrain_shape))break;
        if(first==base.cell_count)return;
    }
    for( size_t i = first; i < base.cell_count; ++i ) {
        if( !( base.cells[i].flags & NCMM_GFX_VISIBLE ) || !ncmm_graphics_shapes::vertical( details[i].terrain_shape ) ) continue;
        const int gx=world?int(int64_t(world->coordinates[i].x)-world->origin_x):int(i)%base.grid_width,
                  gy=world?int(int64_t(world->coordinates[i].y)-world->origin_y):int(i)/base.grid_width;
        for( const auto delta : std::array<std::array<int,2>,4>{{{{0,1}},{{0,-1}},{{1,0}},{{-1,0}}}} ) {
            const int nx = gx + delta[0], ny = gy + delta[1];
            if( nx < 0 || ny < 0 || nx >= base.grid_width || ny >= base.grid_height ) continue;
            const int other=world?(resume?increasing.find(nx,ny,world->coordinates[i].z,delta[0],delta[1]):world_coordinates.find(nx,ny,world->coordinates[i].z)):ny*base.grid_width+nx;if(other<0)continue;
            const size_t j=size_t(other);
            if( !( base.cells[j].flags & NCMM_GFX_VISIBLE ) || ncmm_graphics_shapes::vertical( details[j].terrain_shape ) ||
                ( base.cells[j].flags & NCMM_GFX_NO_FLOOR ) ) continue;
            const uint32_t shape = details[i].terrain_shape;
            const bool plant = shape == NCMM_SHAPE_TREE || shape == NCMM_SHAPE_SHRUB || shape == NCMM_SHAPE_PLANT;
            const std::string ground = base.cells[j].terrain;
            if( plant && !ncmm_graphics_shapes::contains( ground, "dirt" ) && !ncmm_graphics_shapes::contains( ground, "grass" ) &&
                !ncmm_graphics_shapes::contains( ground, "sand" ) && !ncmm_graphics_shapes::contains( ground, "water" ) &&
                !ncmm_graphics_shapes::contains( ground, "fungus" ) ) continue;
            std::fill( std::begin( details[i].ground ), std::end( details[i].ground ), '\0' );
            copy_id( details[i].ground, ground ); break;
        }
    }
}
