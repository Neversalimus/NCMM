#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {
thread_local unsigned fail_at=0,new_calls=0;
std::array<SDL_GPUBuffer*,128> live_buffers{};
void *allocate(size_t size){if(fail_at&&++new_calls==fail_at)throw std::bad_alloc();if(auto *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
SDL_GPUBuffer *tracked_create(SDL_GPUDevice *device,const SDL_GPUBufferCreateInfo *info){
    auto *result=SDL_CreateGPUBuffer(device,info);if(!result)return nullptr;
    for(auto &slot:live_buffers)if(!slot){slot=result;return result;}
    std::abort();
}
void tracked_release(SDL_GPUDevice *device,SDL_GPUBuffer *buffer){
    for(auto &slot:live_buffers)if(slot==buffer){slot=nullptr;SDL_ReleaseGPUBuffer(device,buffer);return;}
    std::fprintf(stderr,"Unexpected duplicate or unowned GPU release\n");std::abort();
}
unsigned outstanding(){unsigned result=0;for(auto *value:live_buffers)result+=value!=nullptr;return result;}
void require(bool value,const char *message){if(!value){std::fprintf(stderr,"Allocation failure probe: %s (%s)\n",message,SDL_GetError());std::exit(2);}}
}
void *operator new(size_t size){return allocate(size);}
void *operator new[](size_t size){return allocate(size);}
void operator delete(void *value)noexcept{std::free(value);}
void operator delete[](void *value)noexcept{std::free(value);}
void operator delete(void *value,size_t)noexcept{std::free(value);}
void operator delete[](void *value,size_t)noexcept{std::free(value);}

#define SDL_CreateGPUBuffer tracked_create
#define SDL_ReleaseGPUBuffer tracked_release
#include "ncmm_gpu_resources.hpp"
#undef SDL_ReleaseGPUBuffer
#undef SDL_CreateGPUBuffer

int main(int argc,char **argv){
    const bool expect_leak=argc==2&&std::strcmp(argv[1],"--expect-leak")==0;
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
    auto *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_SPIRV,true,nullptr);require(device!=nullptr,"real GPU device");
    const std::string backend=SDL_GetGPUDeviceDriver(device),adapter=SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown");
    std::array<ncmm_gpu_vertex_v1,3> vertices{};for(unsigned i=0;i<3;++i){vertices[i].position[0]=float(i);vertices[i].normal[2]=1;}
    std::array<uint32_t,3> indices{{0,1,2}};ncmm_gpu_material_v1 material{};
    ncmm_gpu_mesh_v1 mesh{1,vertices.data(),indices.data(),&material,3,3,1,0};
    ncmm_gpu_draw_v1 draw{};draw.mesh_key=1;draw.bone_count=1;
    ncmm_gpu_scene_v1 scene{};scene.meshes=&mesh;scene.mesh_count=1;scene.draws=&draw;scene.draw_count=1;
    unsigned checked=0,leaking_cases=0,leaked_handles=0,successful_point=0;
    for(unsigned point=1;point<=64;++point){
        require(outstanding()==0,"previous probe was not cleaned up");
        ncmm_gpu::resource_cache cache;cache.bind(device);ncmm_gpu_metrics_v1 metrics{};bool threw=false,ready=false;
        fail_at=point;new_calls=0;
        try{ready=cache.prepare(scene,NCMM_GPU_RESOURCE_LIMIT,metrics);}catch(const std::bad_alloc &){threw=true;}
        fail_at=0;
        require(threw||ready,"unexpected resource preparation failure");
        cache.clear();const auto leaked=outstanding();
        if(threw)++checked;else successful_point=point;
        if(leaked){++leaking_cases;leaked_handles+=leaked;std::printf("LEAK at C++ allocation %u: %u application GPU buffer handles remain after cache.clear()\n",point,leaked);}
        // The probe explicitly releases observed orphan handles so its expected
        // negative control cannot exhaust the device or hide later leak cases.
        for(auto &slot:live_buffers)if(slot){SDL_ReleaseGPUBuffer(device,slot);slot=nullptr;}
        if(!threw)break;
    }
    require(checked>0&&successful_point>0,"all reachable C++ allocation failures must be checked");
    require(SDL_WaitForGPUIdle(device),"qualification teardown");SDL_DestroyGPUDevice(device);SDL_Quit();
    std::printf("Allocation failure probe: checked=%u successful_point=%u leaking_cases=%u leaked_handles=%u expected_negative=%u; %s / %s\n",checked,successful_point,leaking_cases,leaked_handles,unsigned(expect_leak),backend.c_str(),adapter.c_str());
    return (expect_leak?leaking_cases>0:leaking_cases==0)?0:1;
}
