#include "hybrid_renderer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace h=first_person::hybrid;
namespace v=first_person::vehicle;
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"vehicle raster: %s\n",message);std::exit(1);}}
template<class T> bool identical(const std::vector<T> &a,const std::vector<T> &b){
    return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);
}
void compare(const h::frame &a,const h::frame &b){
    require(a.width==b.width&&a.height==b.height,"same internal viewport, no resolution reduction");
    require(identical(a.pixels,b.pixels),"streamed and parallel full colour buffers are bitwise equal");
    require(identical(a.depth,b.depth)&&identical(a.held_depth,b.held_depth),"world/held depth bits are unchanged");
    require(a.owners==b.owners&&a.creatures==b.creatures,"cell and creature picking preserve every pixel");
    require(a.focus==b.focus&&a.focus_depth==b.focus_depth&&a.aimed==b.aimed,"crosshair selection/depth are unchanged");
    require(a.submitted_triangles==b.submitted_triangles&&a.vehicle_parts==b.vehicle_parts,"all source triangles/components are retained");
}
struct fixture {
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(625);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(625);
    ncmm_graphics_scene_v2 scene{};h::presentation_data metadata;v::presentation vehicles;
    fixture(){
        for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;
            std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");
            details[i].light_r=.8f;details[i].light_g=.7f;details[i].light_b=.6f;}
        scene.struct_size=sizeof(scene);scene.base={sizeof(ncmm_graphics_scene_v1),1280,800,25,25,1.5f,12.5f,cells.data(),cells.size()};
        scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;
        scene.environment.sun_x=.3f;scene.environment.sun_y=.4f;scene.environment.sun_z=.8f;
        scene.environment.animation_seconds=42;scene.environment.moon_z=-1;
        metadata.vehicles=&vehicles;metadata.cells.resize(cells.size());metadata.view.eye_height=1.55f;
        v::snapshot input;input.scene.level_height=3.2f;ncmm_graphics_vehicle_v1 car{};car.entity_id=7;car.part_count=3;
        input.vehicles.push_back(car);
        for(uint32_t i=0;i<3;++i){ncmm_graphics_vehicle_part_v1 p{};p.part_id=100+i;p.state=NCMM_VP_EXISTS;p.health=1;
            p.features=i==0?NCMM_VP_FRAME:i==1?NCMM_VP_ROOF:NCMM_VP_WINDOW;p.color[0]=.7f;p.color[1]=.2f;p.color[2]=.1f;p.color[3]=1;
            p.material=NCMM_VM_PAINT;std::strcpy(p.id,i==0?"frame":i==1?"roof":"windshield");input.parts.push_back(p);}
        const v::topology shape(input,input.vehicles[0]);
        for(uint32_t i=0;i<20;++i){auto p=input.parts[0];p.features=i%4==0?NCMM_VP_WINDOW:i%4==1?NCMM_VP_WHEEL:i%4==2?NCMM_VP_CONTROLS:NCMM_VP_PANEL;
            p.wheel_radius=.32f;p.wheel_width=.18f;
            const auto mesh=v::generate(input,shape,p,0);
            vehicles.draws.push_back({mesh,v::rigid({12+float(i%5)*.4f,11.7f+float(i/5)*.4f,.12f},i*.37f),7,100+i,312,false,false,true});}
    }
    h::frame render(bool streamed,float yaw,float pitch,const h::frame *cache=nullptr,h::frame *world=nullptr){
        h::render_options options;options.stream_vehicle_raster=streamed;h::frame result;
        h::renderer painter(scene,yaw,pitch,{},result,{},&metadata,options);painter.run(1280,800,cache,world);return result;
    }
};
}
int main(){
    fixture f;uint32_t streamed=0;
    for(float yaw:{-.17f,0.f,.17f})for(float pitch:{-.15f,0.f,.12f}){
        h::frame old_world,new_world;const auto before=f.render(false,yaw,pitch,nullptr,&old_world),after=f.render(true,yaw,pitch,nullptr,&new_world);
        compare(before,after);streamed+=after.vehicle_streamed_triangles;
        const auto cached_before=f.render(false,yaw,pitch,&old_world),cached_after=f.render(true,yaw,pitch,&new_world);compare(cached_before,cached_after);
    }
    require(streamed>0,"equivalence actually executes the streamed vehicle painter");
    f.scene.base.camera_x=12.5f;
    ncmm_graphics_vehicle_part_v1 part{};part.health=1;part.color[0]=.4f;part.color[3]=1;
    v::builder inside(part);inside.cushion({0,0,0},{2,2,2},.1f,inside.paint);
    f.vehicles.draws.push_back({inside.finish(),v::rigid({12.5f,12.5f,1.55f},0),7,999,312,false,false,true});
    for(float yaw:{0.f,1.2f,3.1f}){const auto before=f.render(false,yaw,-.12f),after=f.render(true,yaw,-.12f);compare(before,after);
        require(after.vehicle_streamed_triangles==0,"near-plane/inside component conservatively keeps parallel path");}
    f.scene.base.camera_x=1.5f;f.cells[312].flags=0;
    const auto hidden_before=f.render(false,0,0),hidden_after=f.render(true,0,0);compare(hidden_before,hidden_after);
    require(hidden_after.vehicle_parts==0&&hidden_after.vehicle_streamed_triangles==0,"hidden Native cell cannot submit vehicle geometry");
    std::printf("vehicle raster: %d checks passed; full 1280x800 colour/depth/picking equivalence\n",checks);
}
