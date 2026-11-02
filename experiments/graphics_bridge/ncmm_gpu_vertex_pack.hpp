#pragma once
#include "ncmm_graphics_gpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace ncmm_gpu::vertex_pack {
struct payload {
    std::vector<ncmm_gpu_vertex_v1> vertices;
    std::vector<uint32_t> indices;
    bool active=false;
};
inline bool unused_uv(const ncmm_gpu_material_v1&m){
    constexpr uint32_t allowed=NCMM_GPU_SMOOTH|NCMM_GPU_CULL_BACK|NCMM_GPU_EMISSIVE;
    if(!(m.flags&NCMM_GPU_SMOOTH)||(m.flags&~allowed)||m.color[3]!=1.f||m.texture_key||m.normal_key||m.depth_key||m.liquid||m.banks||m.impostor_span!=0||m.impostor_yaw!=0||m.reserved[0]||m.reserved[1])return false;
    for(float color:m.color)if(!std::isfinite(color)||color<0||color>1)return false;
    return m.finish==0||m.finish==5||m.finish==7||m.finish==8||m.finish==9||m.finish==10||m.finish==11;
}
inline payload compact(const ncmm_gpu_mesh_v1 &source,const std::vector<uint32_t>&ordered){
    // This optional GPU representation never changes the public source key,
    // CPU vertices/indices, draw order, material order or picking geometry.
    // Small, animated, textured, transparent and unproved meshes retain their
    // complete original representation. Existing source upload limits apply.
    if(!source.key||source.reserved||!source.vertices||!source.materials||!source.indices||
       source.vertex_count<64||source.vertex_count>65536||!source.index_count||source.index_count%3||
       !source.material_count||source.material_count>256||ordered.size()!=source.index_count)return {};
    for(uint32_t n=0;n<source.material_count;++n)if(!unused_uv(source.materials[n]))return {};
    bool any_uv=false;
    for(uint32_t n=0;n<source.vertex_count;++n){const auto &v=source.vertices[n];
        if(v.joint||v.flags||v.reserved||v.material>=source.material_count)return {};
        for(float p:v.pivot)if(p!=0)return {};
        for(float p:v.position)if(!std::isfinite(p))return {};
        for(float p:v.normal)if(!std::isfinite(p))return {};
        for(float p:v.face_normal)if(!std::isfinite(p))return {};
        for(float p:v.uv){if(!std::isfinite(p))return {};any_uv|=p!=0;}
    }
    if(!any_uv||!std::all_of(ordered.begin(),ordered.end(),[&](uint32_t i){return i<source.vertex_count;}))return {};
    static_assert(sizeof(ncmm_gpu_vertex_v1)==72,"every original non-UV scalar participates in exact corner identity");
    size_t capacity=1;while(capacity<size_t(source.vertex_count)*2)capacity*=2;
    std::vector<uint32_t> slots(capacity,UINT32_MAX),remap(source.vertex_count);
    payload result;result.vertices.reserve(source.vertex_count);
    for(uint32_t n=0;n<source.vertex_count;++n){auto vertex=source.vertices[n];vertex.uv[0]=vertex.uv[1]=0;
        std::array<uint32_t,18> identity{};std::memcpy(identity.data(),&vertex,sizeof(vertex));
        uint64_t hash=1469598103934665603ull;for(uint32_t word:identity)hash=(hash^word)*1099511628211ull;
        hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;hash*=0xc4ceb9fe1a85ec53ull;hash^=hash>>33;
        const size_t mask=capacity-1;size_t at=size_t(hash)&mask;
        while(slots[at]!=UINT32_MAX&&std::memcmp(&result.vertices[slots[at]],&vertex,sizeof(vertex)))at=(at+1)&mask;
        if(slots[at]==UINT32_MAX){slots[at]=uint32_t(result.vertices.size());result.vertices.push_back(vertex);}
        remap[n]=slots[at];
    }
    const size_t saved=size_t(source.vertex_count)-result.vertices.size();
    if(saved<8||saved<size_t(source.vertex_count)/16)return {};
    result.indices.reserve(ordered.size());for(uint32_t index:ordered)result.indices.push_back(remap[index]);
    result.active=true;return result;
}
}
