#pragma once
#include "ncmm_gpu_device.hpp"
#include <array>
#include <cstddef>

namespace ncmm_gpu {
class programs {
    SDL_GPUDevice *device_=nullptr;
    SDL_GPUGraphicsPipeline *make(SDL_GPUShader *vs,SDL_GPUShader *fs,bool geometry,
                                  const SDL_GPUTextureFormat *formats,uint32_t colors,bool depth,bool write_depth,bool blend,bool write_id,bool cull=false){
        if(!vs||!fs)return nullptr;
        std::array<SDL_GPUColorTargetDescription,2> targets{};
        for(uint32_t i=0;i<colors;++i){targets[i].format=formats[i];
            auto &b=targets[i].blend_state;
            if(i==0&&blend){b.enable_blend=true;b.src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA;b.dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                b.src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE;b.dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;b.color_blend_op=b.alpha_blend_op=SDL_GPU_BLENDOP_ADD;}
            if(i==1&&!write_id){b.enable_color_write_mask=true;b.color_write_mask=0;}}
        SDL_GPUVertexBufferDescription layout{};layout.pitch=sizeof(ncmm_gpu_vertex_v1);layout.input_rate=SDL_GPU_VERTEXINPUTRATE_VERTEX;
        std::array<SDL_GPUVertexAttribute,8> attributes{};
        const std::array<Uint32,8> offsets{offsetof(ncmm_gpu_vertex_v1,position),offsetof(ncmm_gpu_vertex_v1,normal),offsetof(ncmm_gpu_vertex_v1,uv),offsetof(ncmm_gpu_vertex_v1,material),
            offsetof(ncmm_gpu_vertex_v1,joint),offsetof(ncmm_gpu_vertex_v1,face_normal),offsetof(ncmm_gpu_vertex_v1,flags),offsetof(ncmm_gpu_vertex_v1,pivot)};
        const std::array<SDL_GPUVertexElementFormat,8> types{SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            SDL_GPU_VERTEXELEMENTFORMAT_UINT,SDL_GPU_VERTEXELEMENTFORMAT_UINT,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,SDL_GPU_VERTEXELEMENTFORMAT_UINT,SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3};
        for(uint32_t i=0;i<8;++i){attributes[i].location=i;attributes[i].format=types[i];attributes[i].offset=offsets[i];}
        SDL_GPUGraphicsPipelineCreateInfo p{};p.vertex_shader=vs;p.fragment_shader=fs;
        // SDL renderer devices disable FEATURE_DEPTH_CLAMPING. Explicitly
        // clip depth instead of requesting the unsupported zero/default state.
        p.rasterizer_state.enable_depth_clip=true;p.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        if(geometry){p.vertex_input_state.vertex_buffer_descriptions=&layout;p.vertex_input_state.num_vertex_buffers=1;p.vertex_input_state.vertex_attributes=attributes.data();p.vertex_input_state.num_vertex_attributes=8;}
        p.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
        if(cull){p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_BACK;p.rasterizer_state.front_face=SDL_GPU_FRONTFACE_CLOCKWISE;}
        p.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;p.depth_stencil_state.enable_depth_test=depth;
        p.depth_stencil_state.enable_depth_write=write_depth;p.depth_stencil_state.compare_op=SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        p.target_info.color_target_descriptions=targets.data();p.target_info.num_color_targets=colors;p.target_info.has_depth_stencil_target=depth;p.target_info.depth_stencil_format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        return SDL_CreateGPUGraphicsPipeline(device_,&p);
    }
public:
    SDL_GPUGraphicsPipeline *opaque=nullptr,*opaque_culled=nullptr,*opaque_component=nullptr,*opaque_component_culled=nullptr,*opaque_reflective=nullptr,*opaque_reflective_culled=nullptr,*transparent=nullptr,*transparent_glass=nullptr,*held=nullptr,*held_alpha=nullptr,*sky=nullptr,*celestial=nullptr,*weather=nullptr,*volume=nullptr,*compose=nullptr;
    SDL_GPUComputePipeline *shadow=nullptr,*count=nullptr;
    SDL_GPUSampler *linear=nullptr,*nearest=nullptr,*pixel_art=nullptr;
    void clear(){if(device_){for(auto *p:{opaque,opaque_culled,opaque_component,opaque_component_culled,opaque_reflective,opaque_reflective_culled,transparent,transparent_glass,held,held_alpha,sky,celestial,weather,volume,compose})if(p)SDL_ReleaseGPUGraphicsPipeline(device_,p);
            for(auto *p:{shadow,count})if(p)SDL_ReleaseGPUComputePipeline(device_,p);
            if(linear)SDL_ReleaseGPUSampler(device_,linear);
            if(nearest)SDL_ReleaseGPUSampler(device_,nearest);
            if(pixel_art)SDL_ReleaseGPUSampler(device_,pixel_art);}
        opaque=opaque_culled=opaque_component=opaque_component_culled=opaque_reflective=opaque_reflective_culled=transparent=transparent_glass=held=held_alpha=sky=celestial=weather=volume=compose=nullptr;shadow=count=nullptr;linear=nearest=pixel_art=nullptr;device_=nullptr;}
    void abandon(){device_=nullptr;clear();}
    bool create(device_context &context){if(opaque)return true;device_=context.device();
#define NCMM_GPU_SHADER(NAME,STAGE,SAMPLERS,STORAGE,UNIFORMS) context.shader(shaders::NAME##_spv,sizeof(shaders::NAME##_spv),shaders::NAME##_dxil,sizeof(shaders::NAME##_dxil),STAGE,SAMPLERS,STORAGE,UNIFORMS)
        auto *gv=NCMM_GPU_SHADER(geometry_vert,SDL_GPU_SHADERSTAGE_VERTEX,0,2,2),*gf=NCMM_GPU_SHADER(geometry_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,3,2,2);
        auto *of=NCMM_GPU_SHADER(geometry_component_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,3,2,2);
        auto *rf=NCMM_GPU_SHADER(geometry_reflective_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,3,2,2);
        auto *tf=NCMM_GPU_SHADER(geometry_glass_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,3,2,2);
        auto *fv=NCMM_GPU_SHADER(fullscreen_vert,SDL_GPU_SHADERSTAGE_VERTEX,0,0,0),*sf=NCMM_GPU_SHADER(sky_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,0,0,1);
        auto *cv=NCMM_GPU_SHADER(celestial_vert,SDL_GPU_SHADERSTAGE_VERTEX,0,0,1),*cf=NCMM_GPU_SHADER(celestial_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,0,0,1);
        auto *wv=NCMM_GPU_SHADER(weather_vert,SDL_GPU_SHADERSTAGE_VERTEX,0,1,1),*wf=NCMM_GPU_SHADER(weather_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,0,0,0);
        auto *vf=NCMM_GPU_SHADER(volume_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,2,1,2),*pf=NCMM_GPU_SHADER(compose_frag,SDL_GPU_SHADERSTAGE_FRAGMENT,5,0,1);
#undef NCMM_GPU_SHADER
        const SDL_GPUTextureFormat world[]={SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,SDL_GPU_TEXTUREFORMAT_R32_UINT};
        const SDL_GPUTextureFormat effects[]={SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,SDL_GPU_TEXTUREFORMAT_R32_FLOAT};
        opaque=make(gv,gf,true,world,2,true,true,true,true);
        opaque_culled=make(gv,gf,true,world,2,true,true,true,true,true);
        // The existing component proof requires exact alpha one, no texture
        // or coverage modifiers, and the component fragment writes alpha one.
        // Its source-over blend is therefore an exact overwrite. General and
        // fractional opaque ranges retain their original blending pipelines.
        opaque_component=make(gv,of,true,world,2,true,true,false,true);
        opaque_component_culled=make(gv,of,true,world,2,true,true,false,true,true);
        opaque_reflective=make(gv,rf,true,world,2,true,true,false,true);
        opaque_reflective_culled=make(gv,rf,true,world,2,true,true,false,true,true);
        transparent=make(gv,gf,true,world,2,true,false,true,false);held=make(gv,gf,true,world,2,true,true,true,false);
        transparent_glass=make(gv,tf,true,world,2,true,false,true,false);
        // The imported optic lens uses the existing shader/blend contract.
        // Draw it after all opaque hands/items without hiding geometry behind it.
        held_alpha=make(gv,gf,true,world,2,true,false,true,false);
        sky=make(fv,sf,false,world,2,true,false,false,true);celestial=make(cv,cf,false,world,2,true,false,false,false);
        weather=make(wv,wf,false,world,2,true,false,true,false);volume=make(fv,vf,false,effects,2,false,false,false,true);compose=make(fv,pf,false,world,1,false,false,false,true);
        for(auto *s:{gv,gf,of,rf,tf,fv,sf,cv,cf,wv,wf,vf,pf})if(s)SDL_ReleaseGPUShader(device_,s);
        shadow=context.compute(shaders::shadow_comp_spv,sizeof(shaders::shadow_comp_spv),shaders::shadow_comp_dxil,sizeof(shaders::shadow_comp_dxil),0,1,1,1,64);
        count=context.compute(shaders::count_comp_spv,sizeof(shaders::count_comp_spv),shaders::count_comp_dxil,sizeof(shaders::count_comp_dxil),0,0,1,0,16,16,1);
        SDL_GPUSamplerCreateInfo sampler{};sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_LINEAR;sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        sampler.address_mode_u=sampler.address_mode_v=sampler.address_mode_w=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;sampler.max_lod=10;linear=SDL_CreateGPUSampler(device_,&sampler);
        // Packed signed depth bytes are data, not colors. Hardware-generated
        // mip averages can change their sign/carry; always read the base level.
        sampler.max_lod=0;sampler.min_filter=sampler.mag_filter=SDL_GPU_FILTER_NEAREST;sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;nearest=SDL_CreateGPUSampler(device_,&sampler);
        sampler.max_lod=10;
        sampler.min_filter=SDL_GPU_FILTER_LINEAR;sampler.mag_filter=SDL_GPU_FILTER_NEAREST;sampler.mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;pixel_art=SDL_CreateGPUSampler(device_,&sampler);
        bool ok=opaque&&opaque_culled&&opaque_component&&opaque_component_culled&&opaque_reflective&&opaque_reflective_culled&&transparent&&transparent_glass&&held&&held_alpha&&sky&&celestial&&weather&&volume&&compose&&shadow&&count&&linear&&nearest&&pixel_art;
        if(!ok){if(context.reason().empty())context.fail("create GPU render programs");clear();}return ok;
    }
};
}
