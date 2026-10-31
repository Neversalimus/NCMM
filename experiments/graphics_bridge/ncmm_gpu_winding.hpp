#pragma once
#include "ncmm_graphics_gpu.h"
#include <cmath>
#include <initializer_list>

namespace ncmm_gpu {
// Pure immutable-data proofs, before selecting an optional rasterizer state.
// Every unproved mesh, skin, billboard, camera or pose keeps the old pipeline.
inline bool winding_basis(const float *a,const float *b,const float *c,bool positive){
    auto dot=[](const float *x,const float *y){return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];};
    for(const auto *v:{a,b,c})for(unsigned n=0;n<3;++n)if(!std::isfinite(v[n]))return false;
    if(std::abs(dot(a,a)-1)>1.e-5f||std::abs(dot(b,b)-1)>1.e-5f||std::abs(dot(c,c)-1)>1.e-5f||
       std::abs(dot(a,b))>1.e-5f||std::abs(dot(a,c))>1.e-5f||std::abs(dot(b,c))>1.e-5f)return false;
    const float cross[]={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
    return positive?dot(cross,c)>.9999f:dot(cross,c)<-.9999f;
}
inline bool winding_pose(const ncmm_gpu_matrix_v1 &m){
    for(float n:m.columns)if(!std::isfinite(n))return false;
    return m.columns[3]==0&&m.columns[7]==0&&m.columns[11]==0&&m.columns[15]==1&&
        winding_basis(m.columns,m.columns+4,m.columns+8,true);
}
inline bool winding_camera(const ncmm_gpu_camera_v1 &c){
    // The existing project_point shader uses right/up/forward, with forward
    // into the screen. Its proper view basis makes outward faces clockwise.
    return winding_basis(c.right,c.up,c.forward,true);
}
inline bool winding_bone(const ncmm_gpu_matrix_v1 &m){
    for(unsigned n=0;n<16;++n)if(m.columns[n]!=(n%5==0?1.f:0.f))return false;
    return true;
}
inline bool translation_pose(const ncmm_gpu_matrix_v1 &m){
    // Exact equality, including the affine bottom row. A tiny rotation/scale
    // still needs the general transform; only finite translations are free.
    for(unsigned n=0;n<16;++n){
        if(n>=12&&n<15){if(!std::isfinite(m.columns[n]))return false;}
        else if(m.columns[n]!=(n%5==0?1.f:0.f))return false;
    }
    return true;
}
inline bool rigid_opaque_range(const ncmm_gpu_mesh_v1 &mesh,uint32_t material,uint32_t first,uint32_t count){
    // Ordinary component corners use only the batch's proven identity root.
    // Keep the general vertex shader for every unproved skin, billboard,
    // material or corner. Fragment shading and depth are always unchanged.
    if(!mesh.vertices||!mesh.indices||!mesh.materials||material>=mesh.material_count||!count||count%3||
       count>mesh.index_count||first>mesh.index_count-count)return false;
    const auto &m=mesh.materials[material];
    constexpr uint32_t allowed=NCMM_GPU_CULL_BACK|NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE;
    if(!(m.flags&NCMM_GPU_SMOOTH)||(m.flags&~allowed)||m.color[3]!=1.f||m.finish==12||m.finish==16||
       m.texture_key||m.normal_key||m.depth_key)return false;
    for(uint32_t n=first;n<first+count;++n){if(mesh.indices[n]>=mesh.vertex_count)return false;
        const auto &v=mesh.vertices[mesh.indices[n]];if(v.material!=material||v.joint||v.flags)return false;
        for(float value:v.position)if(!std::isfinite(value))return false;
    }
    return true;
}
inline bool winding_range(const ncmm_gpu_mesh_v1 &mesh,uint32_t material,uint32_t first,uint32_t count){
    if(!mesh.vertices||!mesh.indices||!mesh.materials||material>=mesh.material_count||!count||count%3||
       count>mesh.index_count||first>mesh.index_count-count)return false;
    const auto &m=mesh.materials[material];
    constexpr uint32_t allowed=NCMM_GPU_CULL_BACK|NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE;
    if(!(m.flags&NCMM_GPU_CULL_BACK)||(m.flags&~allowed)||m.color[3]!=1.f)return false;
    bool surface=false;
    for(uint32_t n=first;n<first+count;n+=3){
        for(unsigned v=0;v<3;++v)if(mesh.indices[n+v]>=mesh.vertex_count)return false;
        const auto &a=mesh.vertices[mesh.indices[n]],&b=mesh.vertices[mesh.indices[n+1]],&c=mesh.vertices[mesh.indices[n+2]];
        for(const auto *v:{&a,&b,&c}){
            if(v->material!=material||v->joint||v->flags)return false;
            for(unsigned j=0;j<3;++j)if(!std::isfinite(v->position[j])||v->face_normal[j]!=a.face_normal[j])return false;
        }
        const float x[]={b.position[0]-a.position[0],b.position[1]-a.position[1],b.position[2]-a.position[2]},
            y[]={c.position[0]-a.position[0],c.position[1]-a.position[1],c.position[2]-a.position[2]},
            normal[]={x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]};
        const float length=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
        if(!std::isfinite(length))return false;
        if(length==0)continue; // A zero-area source triangle has no fragments.
        const float inverse=1/length;
        for(unsigned j=0;j<3;++j)if(normal[j]*inverse!=a.face_normal[j])return false;
        surface=true;
    }
    return surface;
}
}
