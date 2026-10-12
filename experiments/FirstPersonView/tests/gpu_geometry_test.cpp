#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

int main(){int checks=0;auto require=[&](bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"GPU geometry: %s\n",message);std::exit(1);}};
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);auto &base=scene.base;base.struct_size=sizeof(base);base.grid_width=base.grid_height=5;base.cell_count=25;base.pixel_width=640;base.pixel_height=480;base.camera_x=base.camera_y=2.5f;
    std::vector<ncmm_graphics_cell_v1> cells(25);std::vector<ncmm_graphics_cell_v2> details(25);base.cells=cells.data();scene.details=details.data();scene.detail_count=25;
    for(int i=0;i<25;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    scene.environment.daylight=1;scene.environment.sun_z=1;
    uint32_t pixel=0x998877ff;int sprite_calls=0;auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &sprite){++sprite_calls;sprite={sizeof(sprite),1,1,1,&pixel};return true;};
    first_person::gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.world_origin_x=200;host.world_origin_y=300;host.resource_epoch=1;first_person::hybrid::frame stats;
    first_person::gpu::packet first;require(cache.build(scene,0,0,provider,first,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"owned scene extraction");const auto &a=first.finish(nullptr);
    require(!ncmm_gpu::validate(a),"valid GPU packet");require(stats.pixels.empty()&&stats.depth.empty()&&stats.effects.empty(),"GPU collection never allocates CPU pixel/depth/effect rasters");
    require(cache.generated_cells==25&&a.draw_count==25,"initial world geometry");require(a.mesh_count==1&&a.texture_count==1,"identical floors share one mesh and texture");
    const int before=sprite_calls;first_person::gpu::packet second;
    require(cache.build(scene,1.2f,.3f,provider,second,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"camera turn");const auto &b=second.finish(nullptr);
    require(cache.generated_cells==0&&sprite_calls==before,"camera turn does not generate world geometry or read atlas sprites");require(b.mesh_count==a.mesh_count&&b.meshes[0].key==a.meshes[0].key,"stable GPU resource keys");
    details[12].light_r=.2f;cells[12].light=.3f;first_person::gpu::packet light;
    require(cache.build(scene,1.2f,.3f,provider,light,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"dynamic light");require(cache.generated_cells==0&&light.cells[12].light[0]==.2f,"lighting changes only cell data");
    std::strcpy(cells[12].terrain,"t_wall");details[12].terrain_shape=NCMM_SHAPE_WALL;first_person::gpu::packet wall;
    require(cache.build(scene,1.2f,.3f,provider,wall,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"partial geometry update");require(cache.generated_cells==9,"one changed cell rebuilds only its local neighbour support");
    const auto &w=wall.finish(nullptr);require(w.mesh_count==2&&w.draw_count==25,"wall appears without erasing floors");
    require(!ncmm_gpu::validate(w),"updated bounded packet");cache.clear();require(cache.bytes()==0,"F6/shutdown resource ownership clears");
    base.grid_width=base.grid_height=97;base.cell_count=9409;base.camera_x=base.camera_y=48.5f;cells.assign(9409,{});details.assign(9409,{});base.cells=cells.data();scene.details=details.data();scene.detail_count=9409;
    for(int i=0;i<9409;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    bool complete=false;int warming_frames=0;
    for(int i=0;i<24&&!complete;++i){first_person::gpu::packet next;complete=cache.build(scene,0,0,provider,next,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE);
        require(cache.generated_cells<=512,"cold scene generation has a bounded per-frame cell budget");if(!complete)++warming_frames;}
    require(complete&&warming_frames>0,"large scene becomes complete after bounded warming");
    const int warmed_calls=sprite_calls;first_person::gpu::packet turned;require(cache.build(scene,2.5f,.4f,provider,turned,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"large camera turn stays ready");
    require(cache.generated_cells==0&&sprite_calls==warmed_calls,"97x97 camera turn reuses the complete static scene");require(stats.pixels.empty(),"warming does not use the CPU pixel renderer");
    std::printf("GPU geometry/cache: %d checks PASS\n",checks);
}
