#include "ncmm_graphics_v2.h"
#include "ncmm_api.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_open_ui_v1(const ncmm_host_api_v1 *);
namespace {
int checks=0,registrations=0,submitted=0,sprite_calls=0;bool enabled=false,capability=true;
ncmm_graphics_render_v2 callback=nullptr;ncmm_graphics_input_v1 input=nullptr;
ncmm_host_api_v2_core core{};uint32_t expected_width=640,expected_height=480;
void check(bool value,const char *name){++checks;if(!value)throw std::runtime_error(name);}
int reg(const char *id,ncmm_graphics_render_v2 r,ncmm_graphics_input_v1 i,void *){
    if(std::strcmp(id,"first_person_view"))return 0;
    ++registrations;callback=r;input=i;return 1;}
int enable(const char *,int yes){enabled=yes!=0;return 1;}
int is_enabled(const char *){return enabled;}
int sprite(uint32_t,uint32_t,ncmm_graphics_sprite_v2 *){++sprite_calls;return 0;}
int submit(const uint32_t *p,uint32_t w,uint32_t h,uint32_t pitch){if(!p||w!=expected_width||h!=expected_height||pitch!=w)return 0;++submitted;return 1;}
ncmm_host_graphics_v2 graphics{2,sizeof(graphics),2,0,reg,enable,is_enabled,sprite,submit};
int has(const char *cap){return std::strcmp(cap,NCMM_GRAPHICS_CAP)||capability;}
uint32_t major(){return 1;}uint32_t minor(){return 9;}
const void *query(const char *id,uint32_t,uint32_t){return std::strcmp(id,NCMM_GRAPHICS_V2_ID)==0?static_cast<const void *>(&graphics):static_cast<const void *>(&core);}
}
int main(){try{
    core.struct_size=sizeof(core);core.abi_version=2;core.api_major=2;core.api_minor=3;
    ncmm_host_api_v1 api{};api.abi_version=1;api.has_capability=has;api.get_api_version_major=major;api.get_api_version_minor=minor;api.query_interface=query;
    const auto *mod=ncmm_get_descriptor_v1();check(!std::strcmp(mod->version,"0.5.0-dev"),"hybrid module version");
    capability=false;check(!mod->init(&api)&&registrations==0,"missing graphics capability rejected");capability=true;
    graphics.struct_size=sizeof(graphics)-1;check(!mod->init(&api)&&registrations==0,"truncated v2 table rejected");graphics.struct_size=sizeof(graphics);
    graphics.abi_version=3;check(!mod->init(&api)&&registrations==0,"unknown v2 ABI rejected");graphics.abi_version=2;
    graphics.get_sprite=nullptr;check(!mod->init(&api)&&registrations==0,"missing CPU sprite function rejected");graphics.get_sprite=sprite;
    check(mod->init(&api)&&registrations==1,"v2 renderer registers");check(!enabled,"initially tiles");ncmm_open_ui_v1(&api);check(enabled,"F6 enables hybrid");
    std::vector<ncmm_graphics_cell_v1> cells(49);std::vector<ncmm_graphics_cell_v2> details(49);
    for(int i=0;i<49;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.8f;}
    ncmm_graphics_scene_v2 s{};s.struct_size=sizeof(s);s.base={sizeof(s.base),640,480,7,7,3.5f,3.5f,cells.data(),cells.size()};s.details=details.data();s.detail_count=details.size();s.environment.daylight=1;s.environment.sun_z=1;
    check(callback(&s,nullptr)&&submitted==1,"hybrid renderer submits complete pixels");
    const int calls=sprite_calls;
    check(callback(&s,nullptr)&&submitted==2&&sprite_calls==calls,"identical UI redraw reuses owned pixels without resolving sprites");
    details[24].light_b=.4f;check(callback(&s,nullptr)&&submitted==3&&sprite_calls>calls,"changed camera lighting invalidates cache immediately");
    const int lit_calls=sprite_calls;cells[0].flags=0;
    check(callback(&s,nullptr)&&submitted==4&&sprite_calls>lit_calls,"changed visibility never displays a cached visible cell");
    details[0].vehicle_features=NCMM_VEH_ROOF;
    const int hidden_calls=sprite_calls;
    check(callback(&s,nullptr)&&submitted==5&&sprite_calls==hidden_calls,"hidden identity changes cannot invalidate visible output");
    input(NCMM_GFX_QUALITY,nullptr);
    check(callback(&s,nullptr)&&submitted==6&&sprite_calls>hidden_calls,"quality switch rebuilds immediately");
    for(int i=0;i<30;++i)input(NCMM_GFX_LOOK_UP,nullptr);
    check(callback(&s,nullptr)&&submitted==7,"look up stays bounded");
    for(int i=0;i<60;++i)input(NCMM_GFX_LOOK_DOWN,nullptr);
    check(callback(&s,nullptr)&&submitted==8,"look down stays bounded");
    auto bad=s;bad.detail_count=0;check(!callback(&bad,nullptr)&&submitted==8,"failed scene submits no partial frame");
    s.base.pixel_width=1766;s.base.pixel_height=1090;input(NCMM_GFX_QUALITY,nullptr);
    expected_width=1280;expected_height=790;
    check(callback(&s,nullptr),"balanced large viewport uses bounded raster with preserved aspect");
    input(NCMM_GFX_QUALITY,nullptr);expected_width=1749;expected_height=1080;
    check(callback(&s,nullptr),"high quality restores native bounded raster");
    ncmm_open_ui_v1(&api);check(!enabled,"F6 restores tiles");mod->shutdown();ncmm_open_ui_v1(&api);check(!enabled,"shutdown clears hybrid table");
    check(mod->init(&api)&&registrations==2,"module reload registers cleanly");mod->shutdown();
    std::cout<<"PASS: "<<checks<<" hybrid ABI and lifecycle checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
