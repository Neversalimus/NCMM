#include "hybrid_renderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace {
int checks=0;
void check(bool condition,const char *reason){++checks;if(!condition)throw std::runtime_error(reason);}
struct fixture {
    ncmm_graphics_multilevel_scene_v1 world{};
    std::vector<ncmm_graphics_cell_v1> appearance;
    std::vector<ncmm_graphics_cell_v2> details;
    std::vector<ncmm_world_cell_v1> positions;
    std::vector<ncmm_world_slice_v1> slices;
    fixture(int levels){
        world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;
        world.origin_x=1000000;world.origin_y=-1000000;world.loaded_min_z=0;world.loaded_max_z=levels-1;
        world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
        for(int z=0;z<levels;++z){const auto first=uint32_t(positions.size());
            for(int y=0;y<3;++y)for(int x=0;x<24;++x){
                positions.push_back({world.origin_x+x,world.origin_y+y,z,NCMM_WORLD_BODY_VISIBLE,0,0,1});
                ncmm_graphics_cell_v1 c{};c.flags=NCMM_GFX_VISIBLE|NCMM_GFX_NO_FLOOR;c.light=1;std::strcpy(c.terrain,"t_open_air");appearance.push_back(c);
                ncmm_graphics_cell_v2 d{};d.light_r=d.light_g=d.light_b=.9f;std::strcpy(d.ground,"t_floor");details.push_back(d);
            }
            slices.push_back({z,first,uint32_t(positions.size())-first,0,uint64_t(z+1),1});
        }
        world.coordinates=positions.data();world.slices=slices.data();world.cell_count=uint32_t(positions.size());world.slice_count=uint32_t(slices.size());
        world.appearance.struct_size=sizeof(world.appearance);
        world.appearance.base={sizeof(ncmm_graphics_scene_v1),400,260,24,3,2.5f,1.5f,appearance.data(),appearance.size()};
        world.appearance.details=details.data();world.appearance.detail_count=details.size();world.appearance.environment.daylight=.8f;
        world.appearance.environment.sun_z=.8f;
    }
    int cell(int x,int y,int z){return ncmm_world::coordinates(&world).find(x,y,z);}
    void slab(int z){for(int y=0;y<3;++y)for(int x=0;x<24;++x){const int i=cell(x,y,z);
        positions[i].flags|=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;appearance[i].flags&=~NCMM_GFX_NO_FLOOR;}}
    bool draw(frame &out,float pitch){return build_frame(world.appearance,0,pitch,{},out,400,260,{},nullptr,nullptr,nullptr,{},nullptr,&world);}
};
struct collector:geometry_sink {
    std::vector<triangle> triangles;
    bool dirty(int)const override{return true;}
    void emit(const triangle &t)override{triangles.push_back(t);}
    void skin(const first_person::creature::model &,const first_person::creature::skeleton_pose &,int,vec,int,vec,bool,bool,float,vec)override{}
    void impostor(const first_person::creature::asset &,const first_person::creature::pose_state &,vec,int,int,bool)override{}
};
}
int main(){try{
    for(int levels:{1,3,5,9}){
        fixture world(levels);world.slab(0);if(levels>1)world.slab(levels-1);
        check(ncmm_world::valid(world.world),"complete sparse world contract");frame image;
        check(world.draw(image,levels==1?-.7f:1.2f),"CPU renders all requested Z slices");
        if(levels>1){size_t upper=0;for(int owner:image.owners)if(owner>=0&&world.positions[owner].z==levels-1)++upper;
            check(upper>100,"actual upper slab appears in CPU depth/owner buffers");}
        collector sink;frame statistics;renderer(world.world.appearance,0,.2f,{},statistics,{},nullptr,{},nullptr,&world.world).collect(sink,400,260);
        check(!sink.triangles.empty(),"shared GPU geometry generator produces slabs");
        size_t bottom=0;for(const auto &t:sink.triangles){check(t.m.cell>=0&&size_t(t.m.cell)<world.positions.size(),"triangle retains unique XYZ source index");
            if(t.normal.z<-.9f){++bottom;const float expected=world.positions[t.m.cell].z*NCMM_WORLD_LEVEL_HEIGHT-.08f;
                for(const auto &v:t.v)check(std::abs(v.p.z-expected)<.0001f,"underside belongs to the actual upper floor");}}
        check(bottom>0,"slabs contain a separately materialled underside");
    }
    fixture empty(3);for(auto &c:empty.appearance)c.flags|=NCMM_GFX_CEILING;
    frame image;check(empty.draw(image,1.2f),"open multi-Z world with misleading legacy interior flags");
    check(std::all_of(image.depth.begin(),image.depth.begin()+image.width*image.height/2,[](float z){return !std::isfinite(z);}),"inside flag cannot create phantom ceiling");
    empty.positions[empty.cell(4,1,0)].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
    collector stairs;frame statistics;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(stairs,400,260);
    check(stairs.triangles.size()>80,"semantic staircase is stepped volume, not a floor square");
    float top=0;for(const auto &t:stairs.triangles)for(const auto &v:t.v)top=std::max(top,v.p.z);
    check(top>=NCMM_WORLD_LEVEL_HEIGHT,"staircase connects a full physical Z storey");
    const int stair=empty.cell(4,1,0);empty.positions[stair].flags|=NCMM_WORLD_RAMP;
    collector ramp;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(ramp,400,260);
    bool slope=false;for(const auto &t:ramp.triangles)if(t.normal.z>.1f&&t.normal.z<.9f)slope=true;
    check(slope,"semantic ramp has an actual inclined surface");
    empty.positions[stair].flags&=~NCMM_WORLD_RAMP;empty.positions[stair].flags|=NCMM_WORLD_LADDER;
    collector ladder;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(ladder,400,260);
    check(ladder.triangles.size()>100,"difficult-Z ladder has rails and rungs");
    empty.world.camera_dz=-2.5f;check(empty.draw(image,0),"visual camera interpolates independently of logical Z");
    check(empty.world.origin_z==0&&empty.positions[stair].z==0,"render leaves canonical game coordinates unchanged");
    std::cout<<"Multi-level rendering: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
