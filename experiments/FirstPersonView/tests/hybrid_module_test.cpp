#include "ncmm_graphics_v2.h"
#include "ncmm_graphics_controls.h"
#include "ncmm_api.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_open_ui_v1(const ncmm_host_api_v1 *);
namespace {
int checks=0,registrations=0,submitted=0,sprite_calls=0,world_sprite_calls=0,camera_updates=0;bool enabled=false,capability=true,control_capability=false;
float camera_yaw=0,camera_pitch=0;uint64_t last_pixels=0;
ncmm_graphics_control_state_v1 state{sizeof(state),0,NCMM_GRIP_SINGLE,0,0,0,0,0,.6f};
ncmm_graphics_render_v2 callback=nullptr;ncmm_graphics_input_v1 input=nullptr;
ncmm_host_api_v2_core core{};uint32_t expected_width=640,expected_height=480;
void check(bool value,const char *name){++checks;if(!value)throw std::runtime_error(name);}
int reg(const char *id,ncmm_graphics_render_v2 r,ncmm_graphics_input_v1 i,void *){
    if(std::strcmp(id,"first_person_view"))return 0;
    ++registrations;callback=r;input=i;return 1;}
int enable(const char *,int yes){enabled=yes!=0;return 1;}
int is_enabled(const char *){return enabled;}
int sprite(uint32_t,uint32_t layer,ncmm_graphics_sprite_v2 *out){++sprite_calls;if(layer!=NCMM_GFX_HELD_ITEM){++world_sprite_calls;return 0;}
    static const uint32_t art[]={0,0x998877ff,0,0,0xbbbbbbff,0,0,0x998877ff,0};*out={sizeof(*out),3,3,3,art};return 1;}
int submit(const uint32_t *p,uint32_t w,uint32_t h,uint32_t pitch){if(!p||w!=expected_width||h!=expected_height||pitch!=w)return 0;++submitted;last_pixels=1469598103934665603ull;for(size_t n=0;n<size_t(w)*h;++n){last_pixels^=p[n];last_pixels*=1099511628211ull;}return 1;}
int get_state(ncmm_graphics_control_state_v1 *out){*out=state;return 1;}
int set_camera(float yaw,float pitch){camera_yaw=yaw;camera_pitch=pitch;++camera_updates;return 1;}
ncmm_host_graphics_controls_v1 controls{1,sizeof(controls),1,0,get_state,set_camera};
ncmm_host_graphics_v2 graphics{2,sizeof(graphics),2,0,reg,enable,is_enabled,sprite,submit};
int has(const char *cap){if(!std::strcmp(cap,NCMM_GRAPHICS_CONTROLS_CAP))return control_capability;return std::strcmp(cap,NCMM_GRAPHICS_CAP)||capability;}
uint32_t major(){return 1;}uint32_t minor(){return 9;}
const void *query(const char *id,uint32_t,uint32_t){if(!std::strcmp(id,NCMM_GRAPHICS_CONTROLS_ID))return &controls;return std::strcmp(id,NCMM_GRAPHICS_V2_ID)==0?static_cast<const void *>(&graphics):static_cast<const void *>(&core);}
}
int main(){try{
    core.struct_size=sizeof(core);core.abi_version=2;core.api_major=2;core.api_minor=3;
    ncmm_host_api_v1 api{};api.abi_version=1;api.has_capability=has;api.get_api_version_major=major;api.get_api_version_minor=minor;api.query_interface=query;
    const auto *mod=ncmm_get_descriptor_v1();check(!std::strcmp(mod->version,"0.12.7-dev"),"hybrid module version");
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
    expected_width=959;expected_height=592;
    check(callback(&s,nullptr),"balanced large viewport uses bounded raster with preserved aspect");
    input(NCMM_GFX_QUALITY,nullptr);expected_width=1280;expected_height=790;
    check(callback(&s,nullptr),"high quality restores native bounded raster");
    const int identity_calls=sprite_calls;cells[24].terrain[126]='X';
    check(callback(&s,nullptr)&&sprite_calls>identity_calls,"late visible identity bytes invalidate the shared-module cache");
    cells[24].terrain[126]=0;check(callback(&s,nullptr),"restored identity renders");
    const int weather_identity_calls=sprite_calls;std::strcpy(s.environment.weather_id,"changed_weather");
    check(callback(&s,nullptr)&&sprite_calls>weather_identity_calls,"environment identity invalidates geometry cache");
    ncmm_open_ui_v1(&api);check(!enabled,"F6 restores tiles");mod->shutdown();ncmm_open_ui_v1(&api);check(!enabled,"shutdown clears hybrid table");
    check(mod->init(&api)&&registrations==2,"module reload registers cleanly");mod->shutdown();
    control_capability=true;controls.struct_size=sizeof(controls)-1;
    check(mod->init(&api)&&camera_updates==0,"truncated optional controls preserve the V2 renderer");mod->shutdown();controls.struct_size=sizeof(controls);
    check(mod->init(&api)&&camera_updates==1,"controls extension synchronizes the initial camera");
    ncmm_open_ui_v1(&api);s.base.pixel_width=640;s.base.pixel_height=480;expected_width=640;expected_height=480;
    state.mouse_dx=200;state.mouse_dy=-100;input(NCMM_GFX_MOUSE_LOOK,nullptr);
    check(std::abs(camera_yaw-(-1.5707963268f+.6f))<.0001f&&std::abs(camera_pitch-.3f)<.0001f,"mouse deltas update horizontal and vertical camera angles");
    state.mouse_dx=0;state.mouse_dy=-2000;input(NCMM_GFX_MOUSE_LOOK,nullptr);
    check(camera_pitch==1.2f,"mouse pitch cannot pass the upper bound");
    state.mouse_dy=2000;input(NCMM_GFX_MOUSE_LOOK,nullptr);check(camera_pitch==-1.2f,"mouse pitch cannot pass the lower bound");
    state.mouse_dy=-400;input(NCMM_GFX_MOUSE_LOOK,nullptr);
    check(callback(&s,nullptr),"controlled scene renders");const int world_calls=world_sprite_calls;
    std::strcpy(s.environment.held_item,"naginata");s.environment.held_kind=2;state.grip_kind=NCMM_GRIP_POLE;state.held_length=2;
    s.environment.animation_seconds=10;check(callback(&s,nullptr)&&world_sprite_calls==world_calls,"changing held equipment composes without rebuilding world geometry");
    const uint64_t idle_pixels=last_pixels;state.attack_serial=1;state.attack_kind=1;state.attack_started=10;s.environment.animation_seconds=10.12;
    check(callback(&s,nullptr)&&last_pixels!=idle_pixels&&world_sprite_calls==world_calls,"attack changes the held pose over the retained world");
    s.environment.animation_seconds=10.6;check(callback(&s,nullptr)&&last_pixels==idle_pixels&&world_sprite_calls==world_calls,"attack recovery restores the exact idle frame");
    s.environment.precipitation=.7f;s.environment.weather_kind=NCMM_WEATHER_RAIN;s.environment.outdoors=1;
    check(callback(&s,nullptr),"weather begins over the retained world");const int weather_calls=world_sprite_calls;
    s.environment.animation_seconds=10.7;check(callback(&s,nullptr)&&world_sprite_calls==weather_calls,"weather animation reuses world geometry");
    state.mouse_dy=std::nanf("");input(NCMM_GFX_MOUSE_LOOK,nullptr);check(std::isfinite(camera_pitch),"malformed controls never corrupt camera state");mod->shutdown();
    std::cout<<"PASS: "<<checks<<" hybrid ABI and lifecycle checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
