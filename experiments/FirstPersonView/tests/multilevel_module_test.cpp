#include "ncmm_api.h"
#include "ncmm_graphics_multilevel.h"
#include "ncmm_graphics_controls.h"
#include "ncmm_graphics_presentation.h"
#include "ncmm_graphics_projectiles.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_open_ui_v1(const ncmm_host_api_v1 *);
namespace {
int checks=0,world_registrations=0,legacy_registrations=0,frames=0;
bool enabled=false;uint64_t pixels=0;
bool projectile_fixture=false,show_projectile=false;unsigned v2_reads=0,legacy_reads=0;
int32_t projectile_origin_x=1000000,projectile_origin_y=-1000000;uint64_t projectile_serial=41;
ncmm_gpu_cpu_metrics_v1 cpu_metrics{};
ncmm_graphics_render_multilevel_v1 callback=nullptr;
ncmm_graphics_input_v1 input=nullptr;
ncmm_host_api_v2_core core{};
void check(bool value,const char *reason){++checks;if(!value)throw std::runtime_error(reason);}
int reg(const char *,ncmm_graphics_render_multilevel_v1 r,ncmm_graphics_input_v1 i,void *){callback=r;input=i;++world_registrations;return 1;}
int old_reg(const char *,ncmm_graphics_render_v2,ncmm_graphics_input_v1,void *){++legacy_registrations;return 1;}
int enable(const char *,int yes){enabled=yes!=0;return 1;}
int is_enabled(const char *){return enabled;}
int sprite(uint32_t,uint32_t,ncmm_graphics_sprite_v2 *){return 0;}
int submit(const uint32_t *data,uint32_t w,uint32_t h,uint32_t pitch){check(w==160&&h==120&&pitch==w,"bounded CPU world frame");pixels=1469598103934665603ull;
    for(size_t i=0;i<size_t(w)*h;++i)pixels=(pixels^data[i])*1099511628211ull;++frames;return 1;}
int submit_gpu(const ncmm_gpu_multilevel_scene_v1 *){throw std::runtime_error("absent GPU may not submit a planar fallback");}
int gpu_unavailable(ncmm_gpu_metrics_v1 *){return 0;}
int gpu_submit(const ncmm_gpu_scene_v1 *){throw std::runtime_error("unavailable GPU submitted a frame");}
void gpu_invalidate(){}
int gpu_has_resource(uint64_t,uint32_t){return 0;}
int cpu_stats(const ncmm_gpu_cpu_metrics_v1 *data){cpu_metrics=*data;return 1;}
ncmm_host_graphics_gpu_v1 gpu_api{1,sizeof(gpu_api),1,0,gpu_unavailable,gpu_submit,gpu_unavailable,gpu_invalidate,gpu_has_resource,cpu_stats};
int metrics(ncmm_multilevel_metrics_v1 *){return 0;}
ncmm_host_graphics_v2 v2{2,sizeof(v2),2,0,old_reg,enable,is_enabled,sprite,submit};
ncmm_host_graphics_multilevel_v1 world_api{1,sizeof(world_api),1,0,reg,enable,is_enabled,sprite,submit,submit_gpu,metrics};
int pose_state(ncmm_graphics_control_state_v1 *out){*out={sizeof(*out),0,NCMM_GRIP_GUN,0,0,0,0,0,.8f};return 1;}
int camera(float,float){return 1;}
ncmm_host_graphics_controls_v1 controls{1,sizeof(controls),1,0,pose_state,camera};
int present_scene(ncmm_graphics_presentation_v1 *out){*out={};out->struct_size=sizeof(*out);out->eye_height=1.55f;out->body_scale=1;out->held_length=.8f;out->gun_kind=NCMM_GUN_RIFLE;return 1;}
int present_cells(ncmm_graphics_detail_v1 *out,size_t n){for(size_t i=0;i<n;++i)out[i]={};return 1;}
int target(int32_t,float,float){return 1;}int effects(const uint32_t *,uint32_t,uint32_t,uint32_t){return 1;}
ncmm_host_graphics_presentation_v1 presentation{1,sizeof(presentation),1,0,present_scene,present_cells,target,effects};
int paths2(ncmm_graphics_projectile_v2 *out,size_t n){check(n==NCMM_PROJECTILE_LIMIT,"bounded v2 projectile read");++v2_reads;if(!show_projectile)return 0;
    *out={};auto &p=out->base;p.serial=projectile_serial;p.world_epoch=1;p.seconds=10;p.kind=NCMM_PROJECTILE_BULLET;p.count=9;std::strcpy(p.weapon,"m27");
    for(unsigned i=0;i<p.count;++i)p.path[i]={projectile_origin_x+1,projectile_origin_y+1-int(i),0};
    out->aim={1,0,0,1.55f,-1.5707963268f,0};return 1;}
int paths1(ncmm_graphics_projectile_v1 *,size_t){++legacy_reads;return 0;}
ncmm_host_graphics_projectiles_v2 projectiles2{1,sizeof(projectiles2),2,0,paths2};
ncmm_host_graphics_projectiles_v1 projectiles1{1,sizeof(projectiles1),1,0,paths1};
int has(const char *){return 1;}uint32_t major(){return 1;}uint32_t minor(){return 9;}
const void *query(const char *id,uint32_t,uint32_t){
    if(projectile_fixture){
        if(!std::strcmp(id,NCMM_GRAPHICS_CONTROLS_ID))return &controls;
        if(!std::strcmp(id,NCMM_GRAPHICS_PRESENTATION_ID))return &presentation;
        if(!std::strcmp(id,NCMM_GRAPHICS_PROJECTILES_V2_ID))return &projectiles2;
        if(!std::strcmp(id,NCMM_GRAPHICS_PROJECTILES_ID))return &projectiles1;
    }
    if(!std::strcmp(id,NCMM_GRAPHICS_V2_ID))return &v2;
    if(!std::strcmp(id,NCMM_GRAPHICS_MULTILEVEL_ID))return &world_api;
    if(!std::strcmp(id,NCMM_GRAPHICS_GPU_ID))return &gpu_api;
    if(!std::strcmp(id,NCMM_HOST_API_V2_CORE_ID))return &core;
    return nullptr;
}
}
int main(){try{
    core.abi_version=2;core.struct_size=sizeof(core);core.api_major=2;core.api_minor=3;
    ncmm_host_api_v1 api{};api.abi_version=1;api.has_capability=has;api.query_interface=query;api.get_api_version_major=major;api.get_api_version_minor=minor;
    const auto *mod=ncmm_get_descriptor_v1();check(mod->init(&api),"world-capable shared module initializes");
    check(world_registrations==1&&legacy_registrations==0,"world callback selected before planar V2 callback");
    ncmm_open_ui_v1(&api);check(enabled,"F6 enables world provider");input(NCMM_GFX_ACCELERATION,nullptr);input(NCMM_GFX_LOOK_UP,nullptr);input(NCMM_GFX_LOOK_UP,nullptr);
    std::vector<ncmm_graphics_cell_v1> cells(27);std::vector<ncmm_graphics_cell_v2> details(27);std::vector<ncmm_world_cell_v1> coordinates;
    ncmm_world_slice_v1 slices[3]{};
    for(int z=0;z<3;++z){slices[z]={z,uint32_t(z*9),9,0,uint64_t(z+1),1};
        for(int y=0;y<3;++y)for(int x=0;x<3;++x){const int i=z*9+y*3+x;cells[i].flags=NCMM_GFX_VISIBLE|NCMM_GFX_CEILING;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
            details[i].light_r=details[i].light_g=details[i].light_b=.8f;std::strcpy(details[i].ground,"t_floor");
            coordinates.push_back({1000000+x,-1000000+y,z,NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM,0,0,1,{.15f,.15f,.15f}});}}
    ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;
    world.origin_x=1000000;world.origin_y=-1000000;world.loaded_max_z=2;world.level_height=2.5f;world.cell_count=27;world.coordinates=coordinates.data();world.slice_count=3;world.slices=slices;
    world.appearance.struct_size=sizeof(world.appearance);world.appearance.base={sizeof(ncmm_graphics_scene_v1),160,120,3,3,1.5f,1.5f,cells.data(),cells.size()};
    world.appearance.details=details.data();world.appearance.detail_count=details.size();world.appearance.environment.daylight=1;
    check(callback(&world,nullptr)&&frames==1,"unavailable GPU retains all three Z levels on CPU");const auto dark=pixels;
    check(cpu_metrics.raster_calls>0,"first CPU world frame rasterizes geometry");
    ++world.revision;
    check(callback(&world,nullptr)&&pixels==dark&&!cpu_metrics.raster_calls,"unchanged native dirty revision reuses owned CPU world pixels");
    for(int i=9;i<18;++i)for(float &light:coordinates[i].lower_light)light=.9f;
    check(callback(&world,nullptr)&&pixels!=dark,"lower-side RGB invalidates retained ceiling pixels");const auto bright=pixels;
    world.camera_dz=-.4f;check(callback(&world,nullptr)&&pixels!=bright,"visual Z camera offset reaches shared module");
    const auto before_fraction=pixels;world.camera_dx=.27f;world.camera_dy=-.19f;
    check(callback(&world,nullptr)&&pixels!=before_fraction&&cpu_metrics.raster_calls>0,"fractional XY camera invalidates cached CPU world pixels");
    const auto fractional=pixels;
    check(callback(&world,nullptr)&&pixels==fractional&&!cpu_metrics.raster_calls,"stopping at a fractional position reuses its owned CPU image");
    auto bad=world;bad.cell_count=26;const int before=frames;check(!callback(&bad,nullptr)&&frames==before,"malformed multi-Z frame cannot reuse previous visible pixels");
    ncmm_open_ui_v1(&api);check(!enabled,"F6 restores tiles");mod->shutdown();check(!callback(&world,nullptr),"callback-scoped world pointers do not survive shutdown");
    check(mod->init(&api)&&world_registrations==2,"world provider reloads cleanly");mod->shutdown();
    projectile_fixture=true;check(mod->init(&api),"projectile v2 provider initializes");ncmm_open_ui_v1(&api);
    world.cell_count=9;world.slice_count=1;world.loaded_max_z=0;world.camera_dx=world.camera_dy=world.camera_dz=0;
    world.appearance.base.cell_count=world.appearance.detail_count=9;std::strcpy(world.appearance.environment.held_item,"m27");
    world.appearance.environment.animation_seconds=10.01;
    check(callback(&world,nullptr),"projectile module baseline");const auto clear=pixels;
    show_projectile=true;check(callback(&world,nullptr)&&pixels!=clear&&cpu_metrics.raster_calls>0,"new projectile changes actual shared-module pixels at the same animation timestamp");
    const auto local_flight=pixels;
    projectile_origin_x=world.origin_x+=20000000;projectile_origin_y=world.origin_y-=20000000;++projectile_serial;
    for(auto &p:coordinates){p.x+=20000000;p.y-=20000000;}
    check(callback(&world,nullptr)&&pixels==local_flight,"moving the whole scene to distant world coordinates cannot move a bullet away from its visible barrel");
    show_projectile=false;check(callback(&world,nullptr)&&pixels==clear,"expired/removed projectile cannot remain in retained CPU pixels");
    check(v2_reads>=3&&legacy_reads==0,"current shared module uses the v2 firing ray instead of the legacy table");mod->shutdown();
    projectiles2.struct_size=sizeof(projectiles2)-1;check(mod->init(&api),"truncated optional v2 table falls back safely");ncmm_open_ui_v1(&api);
    check(callback(&world,nullptr)&&legacy_reads>0,"unchanged v1 projectile API remains supported");mod->shutdown();
    std::cout<<"Multi-level shared-module lifecycle: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
