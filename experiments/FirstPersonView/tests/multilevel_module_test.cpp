#include "ncmm_api.h"
#include "ncmm_graphics_multilevel.h"
#include "ncmm_graphics_controls.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_open_ui_v1(const ncmm_host_api_v1 *);
namespace {
int checks=0,world_registrations=0,legacy_registrations=0,frames=0;
bool enabled=false;uint64_t pixels=0;
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
int metrics(ncmm_multilevel_metrics_v1 *){return 0;}
ncmm_host_graphics_v2 v2{2,sizeof(v2),2,0,old_reg,enable,is_enabled,sprite,submit};
ncmm_host_graphics_multilevel_v1 world_api{1,sizeof(world_api),1,0,reg,enable,is_enabled,sprite,submit,submit_gpu,metrics};
int has(const char *){return 1;}uint32_t major(){return 1;}uint32_t minor(){return 9;}
const void *query(const char *id,uint32_t,uint32_t){
    if(!std::strcmp(id,NCMM_GRAPHICS_V2_ID))return &v2;
    if(!std::strcmp(id,NCMM_GRAPHICS_MULTILEVEL_ID))return &world_api;
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
    for(int i=9;i<18;++i)for(float &light:coordinates[i].lower_light)light=.9f;
    check(callback(&world,nullptr)&&pixels!=dark,"lower-side RGB invalidates retained ceiling pixels");const auto bright=pixels;
    world.camera_dz=-.4f;check(callback(&world,nullptr)&&pixels!=bright,"visual Z camera offset reaches shared module");
    auto bad=world;bad.cell_count=26;const int before=frames;check(!callback(&bad,nullptr)&&frames==before,"malformed multi-Z frame cannot reuse previous visible pixels");
    ncmm_open_ui_v1(&api);check(!enabled,"F6 restores tiles");mod->shutdown();check(!callback(&world,nullptr),"callback-scoped world pointers do not survive shutdown");
    check(mod->init(&api)&&world_registrations==2,"world provider reloads cleanly");mod->shutdown();
    std::cout<<"Multi-level shared-module lifecycle: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
