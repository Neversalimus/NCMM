#include "ncmm_world_policy.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::cerr<<message<<'\n';std::exit(1);}}
struct fixture {
    std::map<std::tuple<int,int,int>,uint32_t> flags;
    uint32_t operator()(ncmm_world::voxel p)const{
        auto it=flags.find({p.x,p.y,p.z});return it==flags.end()?uint32_t(NCMM_WORLD_UNKNOWN):it->second;
    }
    fixture(int levels){for(int z=-levels;z<=levels;++z)for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)flags[{x,y,z}]=0;}
};
}
int main(){
    using namespace ncmm_world;
    traits t{};require(semantics(t)==(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE),"unloaded closes world");
    t.loaded=true;require(semantics(t)&NCMM_WORLD_FLOOR,"ordinary physical floor");
    t.no_floor=true;require(!(semantics(t)&NCMM_WORLD_FLOOR),"NO_FLOOR opens shaft");
    t.no_floor=false;t.no_floor_water=true;require(semantics(t)&NCMM_WORLD_WATER,"water is not a solid slab");
    t.no_floor_water=false;t.z_transparent=true;require(semantics(t)&NCMM_WORLD_FLOOR,"unused Z_TRANSPARENT does not invent a hole");
    t.transparent_floor=true;require((semantics(t)&(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR))==(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR),"transparent slab remains physical");
    t.transparent_floor=false;t.goes_down=true;require(!(semantics(t)&NCMM_WORLD_FLOOR)&&semantics(t)&NCMM_WORLD_OPEN_DOWN,"CDDA visual stairs gap");
    t.difficult_z=true;require(semantics(t)&NCMM_WORLD_LADDER,"difficult vertical connector");
    t.ramp_up=true;require(semantics(t)&NCMM_WORLD_RAMP,"inclined vertical connector");
    for(int layers:{1,3,5,9}){
        fixture f(layers);const point low{.5f,.5f,.55f},high{.5f,.5f,float(layers)*NCMM_WORLD_LEVEL_HEIGHT+.5f};
        require(visible(low,high,f),"open multi-storey shaft upwards");require(visible(high,low,f),"open multi-storey shaft downwards");
        f.flags[{0,0,1}]=NCMM_WORLD_FLOOR;
        require(!visible(low,high,f),"solid upper slab blocks upwards");require(!visible(high,low,f),"same slab blocks downwards");
        f.flags[{0,0,1}]=NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR;
        require(visible(low,high,f),"transparent slab transmits visual ray");
        f.flags[{0,0,1}]=NCMM_WORLD_DOWN;
        require(visible(low,high,f),"stairs central opening");
        require(!visible({.05f,.5f,.55f},{.05f,.5f,float(layers)*NCMM_WORLD_LEVEL_HEIGHT+.5f},f),"stairs landing occludes edge ray");
        f.flags[{0,0,1}]=NCMM_WORLD_UNKNOWN;
        require(!visible(low,high,f),"missing level never opens unknown world");
    }
    fixture f(3);f.flags[{1,0,0}]=NCMM_WORLD_OPAQUE;
    require(!visible({.5f,.5f,1.55f},{2.5f,.5f,1.55f},f),"opaque intermediate wall");
    require(visible({.5f,.5f,1.55f},{1.5f,.5f,1.55f},f),"first wall surface is visible");
    require(!visible({.5f,.5f,1.55f},{1.5f,1.5f,1.55f},f),"diagonal corner cannot leak");
    f.flags[{1,0,0}]=NCMM_WORLD_OPAQUE|opaque_field_fact;
    auto structure=[&](voxel p){return structural_flags(f(p));};
    require(!visible({.5f,.5f,1.55f},{2.5f,.5f,.003f},f,true),"opaque gas retains the body information barrier");
    require(visible({.5f,.5f,1.55f},{2.5f,.5f,.003f},structure,true),"neutral floor geometry continues behind gas instead of exposing sky");
    f.flags[{1,0,0}]=NCMM_WORLD_OPAQUE;
    require(!visible({.5f,.5f,1.55f},{2.5f,.5f,.003f},structure,true),"structural floor query never opens a solid wall");
    f.flags[{1,0,0}]=0;f.flags[{0,0,1}]=NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|opaque_field_fact;
    require(!visible({.5f,.5f,1.55f},{.5f,.5f,3.5f},structure),"gas cannot erase an actual upper slab");
    f.flags[{0,0,1}]=NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|opaque_field_fact;
    require(!visible({.5f,.5f,1.55f},{.5f,.5f,3.5f},structure),"gas cannot reveal an unknown level");
    require(!visible({INFINITY,0,0},{0,0,0},f),"infinite ray rejected");
    require(!visible({1e20f,0,0},{1e20f,0,0},f),"finite int-overflow ray rejected");
    require(!visible({0,0,0},{200,0,0},f),"bounded DDA cannot grow without limit");
    std::vector<ncmm_world_cell_v1> cells;
    std::vector<ncmm_graphics_cell_v1> appearance;
    std::vector<ncmm_graphics_cell_v2> details;
    std::vector<ncmm_world_slice_v1> slices;
    for(int z=-4;z<=4;++z){const uint32_t first=uint32_t(cells.size());
        for(int y=0;y<3;++y)for(int x=0;x<3;++x)cells.push_back({1000000+x,-1000000+y,z,NCMM_WORLD_FLOOR,0,0,1});
        slices.push_back({z,first,9,0,uint64_t(z+5),1});}
    appearance.resize(cells.size());details.resize(cells.size());
    ncmm_graphics_multilevel_scene_v1 scene{};scene.struct_size=sizeof(scene);scene.version=1;scene.world_epoch=1;
    scene.origin_x=1000000;scene.origin_y=-1000000;scene.loaded_min_z=-4;scene.loaded_max_z=4;scene.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    auto &base=scene.appearance.base;base.grid_width=base.grid_height=3;base.cell_count=cells.size();base.cells=appearance.data();
    scene.appearance.details=details.data();scene.appearance.detail_count=details.size();scene.coordinates=cells.data();scene.cell_count=uint32_t(cells.size());
    scene.slices=slices.data();scene.slice_count=uint32_t(slices.size());
    require(valid(scene),"nine sparse slices preserve canonical absolute coordinates");
    coordinates lookup(&scene);
    for(int z=-4;z<=4;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){
        const int i=lookup.find(x,y,z);require(i==(z+4)*9+y*3+x,"unique XYZ index across same XY");
        require(lookup.origin(i).z==z*NCMM_WORLD_LEVEL_HEIGHT,"consistent physical storey scale");
        require(lookup.neighbor(i,0,0,1)==(z==4?-1:i+9),"neighbors link actual levels");
    }
    require(lookup.find(3,0,0)==-1,"out-of-grid lookup");require(lookup.find(INT32_MAX,0,0)==-1,"absolute overflow rejected");
    std::swap(cells[0],cells[1]);require(!valid(scene),"unsorted coordinate alias rejected");std::swap(cells[0],cells[1]);
    cells[1]=cells[0];require(!valid(scene),"duplicate XYZ rejected");cells[1].x++;
    slices[1].first_cell++;require(!valid(scene),"slice index mismatch rejected");slices[1].first_cell--;
    scene.level_height=3;require(!valid(scene),"different floor scale rejected");scene.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    scene.camera_dz=10;require(!valid(scene),"teleport-sized camera interpolation rejected");scene.camera_dz=0;
    require(valid(scene),"repaired scene remains valid");
    std::cout<<"Multi-level policy: "<<checks<<" checks passed\n";
}
