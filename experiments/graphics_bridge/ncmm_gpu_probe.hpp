#pragma once
#include "ncmm_gpu_device.hpp"
#include <array>
#include <cstddef>

namespace ncmm_gpu {
/* Startup/qualification pass. Uses actual vertex/index buffers and depth
 * testing on the shared SDL_GPU device; never paints CPU frame pixels. */
inline bool probe(device_context &context,ncmm_gpu_metrics_v1 &metrics) {
    auto *device=context.device();if(!device||!context.targets(640,480))return false;
    SDL_GPUShader *vs=context.shader(shaders::probe_vert_spv,sizeof(shaders::probe_vert_spv),shaders::probe_vert_dxil,sizeof(shaders::probe_vert_dxil),SDL_GPU_SHADERSTAGE_VERTEX,0,0,1);
    SDL_GPUShader *fs=context.shader(shaders::probe_frag_spv,sizeof(shaders::probe_frag_spv),shaders::probe_frag_dxil,sizeof(shaders::probe_frag_dxil),SDL_GPU_SHADERSTAGE_FRAGMENT,0,0,0);
    SDL_GPUGraphicsPipeline *pipeline=nullptr;SDL_GPUBuffer *vertices=nullptr,*indices=nullptr;SDL_GPUTransferBuffer *transfer=nullptr;
    SDL_GPUCommandBuffer *commands=nullptr;
    auto release=[&] {
        if(commands)SDL_CancelGPUCommandBuffer(commands);
        if(transfer)SDL_ReleaseGPUTransferBuffer(device,transfer);
        if(vertices)SDL_ReleaseGPUBuffer(device,vertices);
        if(indices)SDL_ReleaseGPUBuffer(device,indices);
        if(pipeline)SDL_ReleaseGPUGraphicsPipeline(device,pipeline);
        if(vs)SDL_ReleaseGPUShader(device,vs);
        if(fs)SDL_ReleaseGPUShader(device,fs);
    };
    if(!vs||!fs){release();return false;}
    SDL_GPUVertexBufferDescription layout{};layout.slot=0;layout.pitch=sizeof(ncmm_gpu_vertex_v1);layout.input_rate=SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute attributes[2]{};
    attributes[0].location=0;attributes[0].format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;attributes[0].offset=offsetof(ncmm_gpu_vertex_v1,position);
    attributes[1].location=1;attributes[1].format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;attributes[1].offset=offsetof(ncmm_gpu_vertex_v1,normal);
    SDL_GPUColorTargetDescription color{};color.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo p{};p.vertex_shader=vs;p.fragment_shader=fs;
        // SDL renderer devices disable FEATURE_DEPTH_CLAMPING. Explicitly
        // clip depth instead of requesting the unsupported zero/default state.
        p.rasterizer_state.enable_depth_clip=true;
    p.vertex_input_state.vertex_buffer_descriptions=&layout;p.vertex_input_state.num_vertex_buffers=1;
    p.vertex_input_state.vertex_attributes=attributes;p.vertex_input_state.num_vertex_attributes=2;
    p.primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;p.rasterizer_state.fill_mode=SDL_GPU_FILLMODE_FILL;p.rasterizer_state.cull_mode=SDL_GPU_CULLMODE_NONE;
    p.multisample_state.sample_count=SDL_GPU_SAMPLECOUNT_1;
    p.depth_stencil_state.enable_depth_test=true;p.depth_stencil_state.enable_depth_write=true;p.depth_stencil_state.compare_op=SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    p.target_info.color_target_descriptions=&color;p.target_info.num_color_targets=1;p.target_info.has_depth_stencil_target=true;p.target_info.depth_stencil_format=SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pipeline=SDL_CreateGPUGraphicsPipeline(device,&p);
    if(!pipeline){context.fail("create probe pipeline");release();return false;}
    // A near red triangle is submitted BEFORE the matching far green one.
    // The visible red result therefore demonstrates real hardware depth test.
    std::array<ncmm_gpu_vertex_v1,6> data{};
    for(int i=0;i<6;++i){const float z=i<3?2.f:4.f;const int corner=i%3;
        data[i].position[0]=(corner==0?-1.f:corner==1?1.f:0.f)*z;
        data[i].position[1]=z;data[i].position[2]=(corner==2?1.f:-1.f)*z;
        data[i].normal[i<3?0:1]=1;}
    const uint32_t index_data[]={0,1,2,3,4,5};
    SDL_GPUBufferCreateInfo buffer{};buffer.usage=SDL_GPU_BUFFERUSAGE_VERTEX;buffer.size=sizeof(data);vertices=SDL_CreateGPUBuffer(device,&buffer);
    buffer.usage=SDL_GPU_BUFFERUSAGE_INDEX;buffer.size=sizeof(index_data);indices=SDL_CreateGPUBuffer(device,&buffer);
    SDL_GPUTransferBufferCreateInfo upload{};upload.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;upload.size=sizeof(data)+sizeof(index_data);transfer=SDL_CreateGPUTransferBuffer(device,&upload);
    if(!vertices||!indices||!transfer){context.fail("create probe buffers");release();return false;}
    auto *mapped=static_cast<unsigned char *>(SDL_MapGPUTransferBuffer(device,transfer,false));
    if(!mapped){context.fail("map probe transfer");release();return false;}
    std::memcpy(mapped,data.data(),sizeof(data));std::memcpy(mapped+sizeof(data),index_data,sizeof(index_data));SDL_UnmapGPUTransferBuffer(device,transfer);
    if(!SDL_FlushRenderer(context.renderer())){context.fail("flush SDL before GPU pass");release();return false;}
    commands=SDL_AcquireGPUCommandBuffer(device);
    if(!commands){context.fail("acquire probe commands");release();return false;}
    auto *copy=SDL_BeginGPUCopyPass(commands);SDL_GPUTransferBufferLocation from{transfer,0};SDL_GPUBufferRegion to{vertices,0,sizeof(data)};
    SDL_UploadToGPUBuffer(copy,&from,&to,false);from.offset=sizeof(data);to={indices,0,sizeof(index_data)};SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
    SDL_GPUColorTargetInfo target{};target.texture=context.color();target.load_op=SDL_GPU_LOADOP_CLEAR;target.store_op=SDL_GPU_STOREOP_STORE;target.clear_color={.02f,.03f,.04f,1};target.cycle=true;
    SDL_GPUDepthStencilTargetInfo depth{};depth.texture=context.depth();depth.load_op=SDL_GPU_LOADOP_CLEAR;depth.store_op=SDL_GPU_STOREOP_STORE;depth.clear_depth=1;depth.cycle=true;
        depth.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;depth.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
    auto *pass=SDL_BeginGPURenderPass(commands,&target,1,&depth);
    if(!pass){context.fail("begin probe render pass");release();return false;}
    SDL_BindGPUGraphicsPipeline(pass,pipeline);SDL_GPUBufferBinding vb{vertices,0},ib{indices,0};
    SDL_BindGPUVertexBuffers(pass,0,&vb,1);SDL_BindGPUIndexBuffer(pass,&ib,SDL_GPU_INDEXELEMENTSIZE_32BIT);
    struct camera {float eye[4],right[4],up[4],forward[4],projection[4];} c{};
    c.right[0]=1;c.up[2]=1;c.forward[1]=1;c.projection[0]=1;c.projection[1]=.08f;c.projection[2]=512;c.projection[3]=640.f/480;
    SDL_PushGPUVertexUniformData(commands,0,&c,sizeof(c));
    SDL_DrawGPUIndexedPrimitives(pass,6,1,0,0,0);SDL_EndGPURenderPass(pass);
    const bool ok=SDL_SubmitGPUCommandBuffer(commands);commands=nullptr;
    if(!ok)context.fail("submit probe commands");
    if(ok){metrics.active=1;metrics.draw_calls=1;metrics.triangles=2;metrics.upload_bytes=sizeof(data)+sizeof(index_data);metrics.cpu_raster_calls=0;context.describe(metrics);}
    release();return ok;
}
}
