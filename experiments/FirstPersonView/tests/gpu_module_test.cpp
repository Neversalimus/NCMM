#include "ncmm_api.h"
#include "ncmm_graphics_v2.h"
#include "ncmm_graphics_gpu.h"
#include "ncmm_graphics_controls.h"
#include "ncmm_gpu_validation.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_open_ui_v1(const ncmm_host_api_v1 *);

namespace {
int checks=0,cpu_frames=0,gpu_frames=0,invalidations=0,availability_calls=0;
bool enabled=false,provide_gpu=true,compatible=true,reject_submission=false;
std::string selection="auto";
std::vector<std::string> warnings;
ncmm_graphics_render_v2 render=nullptr;
ncmm_graphics_input_v1 input=nullptr;
ncmm_host_api_v2_core core{};
ncmm_gpu_metrics_v1 last{};

void require(bool condition,const char *message){++checks;if(!condition)throw std::runtime_error(message);}
int register_view(const char *,ncmm_graphics_render_v2 callback,ncmm_graphics_input_v1 action,void *){render=callback;input=action;return 1;}
int enable(const char *,int value){enabled=value!=0;return 1;}
int is_enabled(const char *){return enabled;}
int sprite(uint32_t,uint32_t,ncmm_graphics_sprite_v2 *){return 0;}
int submit_cpu(const uint32_t *pixels,uint32_t width,uint32_t height,uint32_t pitch){
    if(!pixels||width!=640||height!=480||pitch!=width)return 0;
    ++cpu_frames;return 1;
}
int available(ncmm_gpu_metrics_v1 *out){
    ++availability_calls;*out={};out->struct_size=sizeof(*out);out->resource_epoch=1;
    if(!compatible)std::strcpy(out->reason,"SDL renderer has no SDL_GPU device");
    return compatible;
}
int metrics(ncmm_gpu_metrics_v1 *out){*out=last;return 1;}
int submit_gpu(const ncmm_gpu_scene_v1 *scene){
    require(scene&&!ncmm_gpu::validate(*scene),"shipping DLL submits a valid owned GPU packet");
    require(scene->draw_count>0,"GPU scene contains world geometry");
    last={};last.struct_size=sizeof(last);
    if(reject_submission){std::strcpy(last.reason,"create GPU shader pipeline: injected failure");return 0;}
    last.active=1;last.target_cell=-1;++gpu_frames;return 1;
}
void invalidate(){++invalidations;last={};}
int has_resource(uint64_t,uint32_t){return 0;}
void log(ncmm_log_level_v1 level,const char *message){if(level==NCMM_LOG_WARN)warnings.emplace_back(message);}
int register_setting(const char *module,const char *id,const char *,const char *,const char *const *values,const char *const *,size_t count,const char *initial,uint32_t scope){
    require(std::strcmp(module,"first_person_view")==0&&std::strcmp(id,"NCMM_FIRST_PERSON_RENDERER")==0,"renderer setting belongs to NCMM namespace");
    require(count==3&&std::strcmp(values[0],"auto")==0&&std::strcmp(initial,"auto")==0&&scope==NCMM_WORLD_SETTING_LIVE,"Auto/GPU/CPU setting is live with Auto default");return 1;
}
const char *get_setting(const char *,const char *){return selection.c_str();}
int has_capability(const char *cap){return std::strcmp(cap,NCMM_GRAPHICS_CAP)==0||std::strcmp(cap,NCMM_GRAPHICS_V2_CAP)==0||std::strcmp(cap,"core.v1")==0||std::strcmp(cap,"api.versioning.v1")==0||std::strcmp(cap,"host_api.v2.core")==0||std::strcmp(cap,"module_hotkeys.v1")==0;}
uint32_t major(){return 1;}uint32_t minor(){return 9;}
ncmm_host_graphics_v2 graphics{2,sizeof(graphics),2,0,register_view,enable,is_enabled,sprite,submit_cpu};
ncmm_host_graphics_gpu_v1 gpu{1,sizeof(gpu),1,0,available,submit_gpu,metrics,invalidate,has_resource,nullptr};
const void *query(const char *id,uint32_t,uint32_t){
    if(!std::strcmp(id,NCMM_GRAPHICS_V2_ID))return &graphics;
    if(!std::strcmp(id,NCMM_GRAPHICS_GPU_ID))return provide_gpu?&gpu:nullptr;
    if(!std::strcmp(id,"ncmm.host_api.v2.core"))return &core;
    return nullptr;
}
}

int main(){try{
    core.struct_size=sizeof(core);core.abi_version=2;core.api_major=2;core.api_minor=3;
    core.log=log;core.world_setting_register_enum=register_setting;core.world_setting_get_string=get_setting;
    ncmm_host_api_v1 api{};api.abi_version=1;api.has_capability=has_capability;api.get_api_version_major=major;api.get_api_version_minor=minor;api.query_interface=query;
    const auto *module=ncmm_get_descriptor_v1();
    std::vector<ncmm_graphics_cell_v1> cells(25);std::vector<ncmm_graphics_cell_v2> details(25);
    for(int i=0;i<25;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),640,480,5,5,2.5f,2.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=scene.environment.sun_z=1;
    require(module->init(&api),"GPU-capable shipping module initializes");ncmm_open_ui_v1(&api);
    require(render(&scene,nullptr)&&gpu_frames==1&&!cpu_frames,"Auto uses GPU without uploading a CPU framebuffer");
    reject_submission=true;
    require(render(&scene,nullptr)&&cpu_frames==1,"pipeline failure delivers a complete CPU fallback");
    require(warnings.size()==1&&warnings.back().find("injected failure")!=std::string::npos,"fallback logs the actual pipeline failure");
    require(render(&scene,nullptr)&&warnings.size()==1,"repeated fallback does not flood diagnostics");
    reject_submission=false;
    require(render(&scene,nullptr)&&gpu_frames==2,"GPU recovers after rejected submission");
    selection="cpu";const int probes=availability_calls;
    require(render(&scene,nullptr)&&cpu_frames==3&&invalidations==1&&availability_calls==probes,"live CPU selection releases GPU resources and bypasses GPU preparation");
    selection="gpu";
    require(render(&scene,nullptr)&&gpu_frames==3,"live GPU selection restores direct geometry");
    input(NCMM_GFX_ACCELERATION,nullptr);
    require(render(&scene,nullptr)&&gpu_frames==4,"F12 cycles explicit GPU to Auto");
    input(NCMM_GFX_ACCELERATION,nullptr);
    require(render(&scene,nullptr)&&cpu_frames==4&&invalidations==2,"F12 cycles Auto to CPU and releases resources once");
    selection="auto";
    require(render(&scene,nullptr)&&gpu_frames==5,"changed NCMM setting supersedes hotkey override");
    compatible=false;
    require(render(&scene,nullptr)&&cpu_frames==5&&warnings.back().find("no SDL_GPU device")!=std::string::npos,"unsupported renderer falls back with its concrete reason");
    module->shutdown();provide_gpu=false;selection="gpu";
    require(module->init(&api),"older Host without optional GPU interface still loads");ncmm_open_ui_v1(&api);
    require(render(&scene,nullptr)&&cpu_frames==6&&warnings.back().find("Graphics GPU/Mesh API v1")!=std::string::npos,"explicit GPU on older Host reports complete CPU fallback");
    module->shutdown();
    std::cout<<"GPU shipping-module recovery: "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}}
