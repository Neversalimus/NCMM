#pragma once
#include "ncmm_graphics_gpu.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace ncmm_gpu {
inline bool matrix_valid(const ncmm_gpu_matrix_v1 &m) {
    return std::all_of(m.columns,m.columns+16,[](float x){return std::isfinite(x)&&std::abs(x)<=1e7f;});
}
inline const char *validate(const ncmm_gpu_scene_v1 &s) {
    if(s.struct_size<sizeof(s)||!s.width||!s.height||s.width>3840||s.height>2160||s.mode>NCMM_GPU_CPU)
        return "invalid-frame-contract";
    if(s.mesh_count>NCMM_GPU_MAX_DRAWS||s.draw_count>NCMM_GPU_MAX_DRAWS||s.texture_count>NCMM_GPU_MAX_TEXTURES||s.bone_count>NCMM_GPU_MAX_BONES||s.cell_count>9409)
        return "scene-budget-exceeded";
    if((s.mesh_count&&!s.meshes)||(s.draw_count&&!s.draws)||(s.texture_count&&!s.textures)||(s.bone_count&&!s.bones)||(s.cell_count&&!s.cells))
        return "missing-scene-data";
    const float *camera=reinterpret_cast<const float *>(&s.camera);
    for(size_t i=0;i<sizeof(s.camera)/sizeof(float);++i)if(!std::isfinite(camera[i])||std::abs(camera[i])>1e7f)return "invalid-camera";
    if(s.camera.projection[0]<=0||s.camera.projection[1]<.001f||s.camera.projection[2]<=s.camera.projection[1])return "invalid-projection";
    size_t bytes=0,vertices=0,indices=0;
    std::unordered_set<uint64_t> keys;
    for(uint32_t n=0;n<s.mesh_count;++n) {
        const auto &m=s.meshes[n];
        if(!m.key||!keys.insert(m.key).second||!m.vertices||!m.indices||!m.materials||!m.vertex_count||!m.index_count||m.index_count%3||!m.material_count||m.material_count>4096)return "invalid-mesh-contract";
        vertices+=m.vertex_count;indices+=m.index_count;
        if(vertices>NCMM_GPU_MAX_VERTICES||indices>NCMM_GPU_MAX_INDICES)return "geometry-budget-exceeded";
        bytes+=size_t(m.vertex_count)*sizeof(*m.vertices)+size_t(m.index_count)*sizeof(*m.indices)+size_t(m.material_count)*sizeof(*m.materials);
        for(uint32_t i=0;i<m.vertex_count;++i) {
            const auto &v=m.vertices[i];
            for(float f:v.position)if(!std::isfinite(f)||std::abs(f)>1e7f)return "invalid-vertex-position";
            for(float f:v.normal)if(!std::isfinite(f)||std::abs(f)>2)return "invalid-vertex-normal";
            for(float f:v.uv)if(!std::isfinite(f)||std::abs(f)>1024)return "invalid-vertex-uv";
            if(v.material>=m.material_count||v.joint>=128)return "invalid-vertex-reference";
        }
        for(uint32_t i=0;i<m.index_count;++i)if(m.indices[i]>=m.vertex_count)return "invalid-index";
        for(uint32_t i=0;i<m.material_count;++i) {
            const auto &v=m.materials[i];
            for(float f:v.color)if(!std::isfinite(f)||f<0||f>1)return "invalid-material-color";
            if(v.flags&~511u||v.finish>18||v.liquid>10||v.banks>255||!std::isfinite(v.impostor_span)||v.impostor_span<0||!std::isfinite(v.impostor_yaw))return "invalid-material";
        }
    }
    keys.clear();
    for(uint32_t n=0;n<s.texture_count;++n) {
        const auto &t=s.textures[n];
        if(!t.key||!keys.insert(t.key).second||!t.rgba||!t.width||!t.height||t.width>512||t.height>512||t.pitch<t.width||t.pitch>4096)return "invalid-texture";
        bytes+=size_t(t.width)*t.height*4;
    }
    if(bytes>NCMM_GPU_RESOURCE_LIMIT)return "upload-budget-exceeded";
    for(uint32_t i=0;i<s.bone_count;++i)if(!matrix_valid(s.bones[i]))return "invalid-bone-transform";
    for(uint32_t i=0;i<s.draw_count;++i) {
        const auto &d=s.draws[i];
        if(!d.mesh_key||!matrix_valid(d.transform)||!d.bone_count||d.bone_count>128||d.bone_offset>s.bone_count||d.bone_count>s.bone_count-d.bone_offset||d.cell>=s.cell_count||d.pass>NCMM_GPU_HELD)return "invalid-draw-command";
    }
    for(uint32_t i=0;i<s.cell_count;++i) {
        const auto &c=s.cells[i];
        for(const float *p:{c.light,c.source,c.volume,c.volume_color})for(int j=0;j<4;++j)if(!std::isfinite(p[j])||p[j]<0||p[j]>500)return "invalid-cell-light-volume";
    }
    return nullptr;
}
}
