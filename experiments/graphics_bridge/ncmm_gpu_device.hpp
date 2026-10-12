#pragma once
#include <SDL3/SDL.h>
#include "ncmm_graphics_gpu.h"
#ifndef NCMM_GPU_SHADER_HEADER
#define NCMM_GPU_SHADER_HEADER "ncmm_gpu_shader_bytes.hpp"
#endif
#include NCMM_GPU_SHADER_HEADER
#include <cstring>
#include <string>

namespace ncmm_gpu {
/* Borrows exactly the SDL renderer's device. It never claims a window, acquires
 * a swapchain texture or destroys that device. All work is offscreen. */
class device_context {
    SDL_Renderer *renderer_=nullptr;
    SDL_GPUDevice *device_=nullptr;
    uint64_t generation_=0;
    SDL_Texture *output_=nullptr;
    SDL_GPUTexture *depth_=nullptr;
    uint32_t width_=0,height_=0;
    std::string reason_;
public:
    device_context()=default;
    device_context(const device_context &)=delete;
    device_context &operator=(const device_context &)=delete;
    ~device_context(){clear();}
    SDL_GPUDevice *device()const{return device_;}
    SDL_Renderer *renderer()const{return renderer_;}
    SDL_Texture *output()const{return output_;}
    SDL_GPUTexture *color()const{return output_?static_cast<SDL_GPUTexture *>(SDL_GetPointerProperty(SDL_GetTextureProperties(output_),SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER,nullptr)):nullptr;}
    SDL_GPUTexture *depth()const{return depth_;}
    const std::string &reason()const{return reason_;}
    bool fail(const char *stage){reason_=std::string(stage)+": "+SDL_GetError();return false;}
    void clear() {
        if(output_)SDL_DestroyTexture(output_);
        if(device_&&depth_)SDL_ReleaseGPUTexture(device_,depth_);
        output_=nullptr;depth_=nullptr;renderer_=nullptr;device_=nullptr;width_=height_=0;
    }
    // A destroyed SDL renderer already invalidated all of its children. Do not
    // release stale addresses against a replacement device.
    void abandon(){output_=nullptr;depth_=nullptr;renderer_=nullptr;device_=nullptr;width_=height_=0;}
    bool bind(SDL_Renderer *renderer,uint64_t generation) {
        if(renderer_&&generation_!=generation)abandon();
        if(renderer_==renderer&&device_)return true;
        clear();generation_=generation;
        if(!renderer){reason_="SDL renderer unavailable";return false;}
        auto *device=SDL_GetGPURendererDevice(renderer);
        if(!device){reason_="SDL renderer has no SDL_GPU device";return false;}
        renderer_=renderer;device_=device;reason_.clear();return true;
    }
    bool targets(uint32_t width,uint32_t height) {
        if(!device_||!width||!height||width>3840||height>2160)return false;
        if(output_&&width_==width&&height_==height)return true;
        if(output_)SDL_DestroyTexture(output_);output_=nullptr;
        if(depth_)SDL_ReleaseGPUTexture(device_,depth_);depth_=nullptr;
        output_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,int(width),int(height));
        if(!output_||!color())return fail("create GPU output texture");
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;
        info.format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;info.width=width;info.height=height;
        info.layer_count_or_depth=1;info.num_levels=1;info.sample_count=SDL_GPU_SAMPLECOUNT_1;
        info.usage=SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET|SDL_GPU_TEXTUREUSAGE_SAMPLER;
        depth_=SDL_CreateGPUTexture(device_,&info);
        if(!depth_)return fail("create hardware depth attachment");
        width_=width;height_=height;return true;
    }
    SDL_GPUShader *shader(const unsigned char *spv,size_t spv_size,const unsigned char *dxil,size_t dxil_size,
                         SDL_GPUShaderStage stage,uint32_t samplers,uint32_t storage,uint32_t uniforms) {
        if(!device_)return nullptr;
        const auto supported=SDL_GetGPUShaderFormats(device_);
        SDL_GPUShaderCreateInfo info{};info.entrypoint="main";info.stage=stage;
        if((supported&SDL_GPU_SHADERFORMAT_DXIL)&&dxil_size>20){info.code=dxil;info.code_size=dxil_size;info.format=SDL_GPU_SHADERFORMAT_DXIL;}
        else if((supported&SDL_GPU_SHADERFORMAT_SPIRV)&&spv_size>20){info.code=spv;info.code_size=spv_size;info.format=SDL_GPU_SHADERFORMAT_SPIRV;}
        else{reason_="compiled shader format unsupported by SDL_GPU device";return nullptr;}
        info.num_samplers=samplers;info.num_storage_buffers=storage;info.num_uniform_buffers=uniforms;
        auto *result=SDL_CreateGPUShader(device_,&info);if(!result)fail("create GPU shader");return result;
    }
    void describe(ncmm_gpu_metrics_v1 &m)const {
        auto copy=[](char *out,size_t size,const char *s){if(!s)s="";std::strncpy(out,s,size-1);out[size-1]=0;};
        copy(m.reason,sizeof(m.reason),reason_.c_str());
        if(!device_)return;
        const auto props=SDL_GetGPUDeviceProperties(device_);
        copy(m.backend,sizeof(m.backend),SDL_GetGPUDeviceDriver(device_));
        copy(m.device,sizeof(m.device),SDL_GetStringProperty(props,SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown device"));
        const std::string name=m.device;
        m.software_device=name.find("Microsoft Basic")!=std::string::npos||name.find("llvmpipe")!=std::string::npos||name.find("SwiftShader")!=std::string::npos;
        // The pinned public SDL_GPU API exposes fences, not timestamp queries.
        m.timestamp_queries=0;m.gpu_ms=-1;
    }
};
}
