#include "ncmm_sdk_core.hpp"
#include "ncmm_graphics_creatures.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
namespace {
ncmm_graphics_render_v2 callback=nullptr;ncmm_graphics_input_v1 input=nullptr;
ncmm_host_api_v2_core core{};ncmm_graphics_creature_metrics_v1 metrics{};
ncmm_graphics_creature_scene_v1 view{sizeof(view),1,1,1,10};std::vector<ncmm_graphics_creature_v1> entities;
int checks=0,metrics_calls=0,world_textures=0,creature_textures=0,submissions=0;bool enabled=false,volume=true;
void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
int reg(const char *,ncmm_graphics_render_v2 r,ncmm_graphics_input_v1 i,void *){callback=r;input=i;return 1;}
int enable(const char *,int yes){enabled=yes!=0;return 1;}int is_enabled(const char *){return enabled;}
int get_scene(ncmm_graphics_creature_scene_v1 *out){*out=view;return 1;}
int get_creatures(ncmm_graphics_creature_v1 *out,size_t count){if(count!=entities.size())return 0;std::copy(entities.begin(),entities.end(),out);return 1;}
int submit_metrics(const ncmm_graphics_creature_metrics_v1 *m){++metrics_calls;metrics=*m;return 1;}
int register_setting(const char *,const char *,const char *,const char *,const char *const *,const char *const *,size_t,const char *,uint32_t){return 1;}
const char *get_setting(const char *key,const char *){return std::strcmp(key,"NCMM_FIRST_PERSON_CREATURE_ART")==0?(volume?"volume":"tileset"):"cpu";}
int sprite(uint32_t,uint32_t layer,ncmm_graphics_sprite_v2 *out){if(layer!=NCMM_GFX_CREATURE){++world_textures;return 0;}++creature_textures;
    static const uint32_t art[16]={0x783126ff,0xb39375ff,0xb39375ff,0x783126ff,0x285f91ff,0x285f91ff,0x285f91ff,0x285f91ff,0x50422eff,0x50422eff,0x50422eff,0x50422eff,0x303030ff,0x303030ff,0x303030ff,0x303030ff};*out={sizeof(*out),4,4,4,art};return 1;}
int submit(const uint32_t *p,uint32_t w,uint32_t h,uint32_t pitch){++submissions;return p&&w==320&&h==240&&pitch==w;}
ncmm_host_graphics_v2 graphics{2,sizeof(graphics),2,0,reg,enable,is_enabled,sprite,submit};
ncmm_host_graphics_creatures_v1 creatures{1,sizeof(creatures),1,0,get_scene,get_creatures,submit_metrics};
int has(const char *cap){return std::strcmp(cap,"graphics.controls.v1")&&std::strcmp(cap,"graphics.presentation.v1");}
uint32_t major(){return 1;}uint32_t minor(){return 9;}
const void *query(const char *id,uint32_t,uint32_t){if(!std::strcmp(id,NCMM_GRAPHICS_CREATURES_ID))return &creatures;if(!std::strcmp(id,NCMM_GRAPHICS_V2_ID))return &graphics;return &core;}
}
int main(){try{
    core.struct_size=sizeof(core);core.abi_version=2;core.api_major=2;core.api_minor=3;
    core.world_setting_register_enum=register_setting;core.world_setting_get_string=get_setting;
    ncmm_host_api_v1 api{};api.abi_version=1;api.has_capability=has;api.get_api_version_major=major;api.get_api_version_minor=minor;api.query_interface=query;
    const auto *module=ncmm_get_descriptor_v1();std::vector<ncmm_graphics_cell_v1> cells(121);std::vector<ncmm_graphics_cell_v2> details(121);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.9f;}
    int index=6*11+5;std::strcpy(cells[index].creature,"unknown_mod_zombie");ncmm_graphics_creature_v1 c{};c.entity_id=1;c.cell=index;c.world_x=5;c.world_y=6;c.liters=70;c.kilograms=70;std::strcpy(c.bodytype,"human");std::strcpy(c.type_id,cells[index].creature);entities={c};
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),320,240,11,11,5.5f,9.5f,cells.data(),cells.size()};scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=1;
    auto draw=[&]{scene.environment.animation_seconds=view.seconds;check(callback(&scene,nullptr),"shipping DLL renders creature snapshot");};
    creatures.struct_size=sizeof(creatures)-1;check(module->init(&api),"truncated optional API preserves legacy renderer");draw();check(metrics_calls==0,"truncated extension is never called");module->shutdown();creatures.struct_size=sizeof(creatures);
    check(module->init(&api),"shipping DLL accepts optional creature API");draw();check(metrics.struct_size==sizeof(metrics)&&metrics.fallback==1,"first appearance presents an immediate volume");
    for(int attempt=0;attempt<100&&(metrics.queued||metrics.fallback);++attempt){std::this_thread::sleep_for(std::chrono::milliseconds(2));view.seconds+=.025;draw();}
    check(metrics.generated==1&&metrics.queued==0&&metrics.fallback==0&&metrics.visible_pixels>10,"background model replaces placeholder in the shipping DLL");check(creature_textures<=2,"sprite source is copied only once per appearance");
    const int background=world_textures;cells[index].creature[0]=0;index++;std::strcpy(cells[index].creature,c.type_id);entities[0].cell=index;entities[0].world_x++;view.seconds+=.05;draw();
    check(world_textures==background,"moving creature does not rebuild cached static world");check(metrics.visible_pixels>0,"movement preserves visible geometry");
    cells[index].flags=0;view.seconds+=.05;draw();check(metrics.visible_pixels==0,"hidden Host record cannot reveal a creature");cells[index].flags=NCMM_GFX_VISIBLE;
    entities[0].flags=NCMM_CR_DEAD;entities[0].event=NCMM_CR_DIE;entities[0].event_serial=1;entities[0].event_seconds=view.seconds;cells[index].creature[0]=0;view.seconds+=.1;draw();check(metrics.visible_pixels>0,"visible death renders over a now-empty cell");
    entities.clear();view.count=0;view.seconds+=1;draw();check(metrics.visible_pixels==0,"expired death cannot leave stale cached pixels");
    view.count=1;entities={c};cells[c.cell].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[c.cell].creature,c.type_id);view.world_epoch++;view.seconds+=.025;draw();check(metrics.fallback==1,"new world discards old model assets");
    module->shutdown();check(module->init(&api),"worker and resources reload after shutdown");view.seconds+=.025;draw();check(metrics.fallback==1,"new module lifecycle starts clean");module->shutdown();
    volume=false;check(module->init(&api),"native artwork mode initializes");view.seconds+=.025;draw();
    check(metrics.generated==1&&!metrics.fallback&&!metrics.queued&&metrics.visible_pixels>10,"native art is ready on the first frame without model generation");
    const int source_reads=creature_textures;view.seconds+=.025;draw();check(creature_textures==source_reads,"native artwork is copied only once per appearance");
    entities[0].flags|=NCMM_CR_DEAD;entities[0].event=NCMM_CR_DIE;entities[0].event_serial=2;entities[0].event_seconds=view.seconds;cells[c.cell].creature[0]=0;view.seconds+=.1;draw();
    check(metrics.visible_pixels>0&&creature_textures==source_reads,"owned native art survives a death after the cell identity disappears");
    module->shutdown();
    std::cout<<"Creature shipping DLL: "<<checks<<" checks PASS\n";
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
