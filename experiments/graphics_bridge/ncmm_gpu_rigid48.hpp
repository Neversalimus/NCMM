#pragma once
#include "ncmm_graphics_gpu.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

namespace ncmm_gpu::rigid48 {
struct vertex {float position[3],normal[3],face_normal[3],zero[3];};
static_assert(sizeof(vertex)==48&&offsetof(vertex,position)==0&&offsetof(vertex,normal)==12&&offsetof(vertex,face_normal)==24&&offsetof(vertex,zero)==36);
inline constexpr std::array<uint32_t,8> offsets{{0,12,36,36,36,24,36,36}};
// This private format is usable only after the existing complete opaque/UV
// proof. The unchanged geometry programs do not consume material_index;
// per-range fragment materials and all original CPU vertices remain separate.
inline bool eligible(const std::vector<ncmm_gpu_vertex_v1> &vertices){
    if(vertices.empty()||vertices.size()>65536)return false;
    for(const auto &v:vertices){
        if(v.joint||v.flags||v.reserved||v.uv[0]!=0||v.uv[1]!=0||v.pivot[0]!=0||v.pivot[1]!=0||v.pivot[2]!=0)return false;
        for(float f:v.position)if(!std::isfinite(f))return false;
        for(float f:v.normal)if(!std::isfinite(f))return false;
        for(float f:v.face_normal)if(!std::isfinite(f))return false;
    }
    return true;
}
inline bool encode(const std::vector<ncmm_gpu_vertex_v1> &vertices,unsigned char *destination,size_t bytes){
    if(!destination||bytes!=vertices.size()*sizeof(vertex)||!eligible(vertices))return false;
    for(size_t n=0;n<vertices.size();++n){vertex v{};
        std::memcpy(v.position,vertices[n].position,12);std::memcpy(v.normal,vertices[n].normal,12);std::memcpy(v.face_normal,vertices[n].face_normal,12);
        std::memcpy(destination+n*sizeof(vertex),&v,sizeof(v));
    }
    return true;
}
}
