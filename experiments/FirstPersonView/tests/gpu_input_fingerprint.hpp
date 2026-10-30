#pragma once
#include "ncmm_graphics_multilevel.h"
#include <array>
#include <cstdint>

// Test-only fingerprints of successful submissions. No addresses, padding,
// timing counters, resources or GPU readbacks enter this pure data operation.
namespace gpu_input_diagnostic {
inline constexpr std::array<const char *,13> names{{"layout","camera","positions","normals","uv","vertex_state",
    "indices","materials","draws","bones","cells","textures","multilevel"}};
using result=std::array<uint64_t,names.size()>;
inline result fingerprint(const ncmm_gpu_scene_v1 &scene,const ncmm_gpu_multilevel_scene_v1 *world=nullptr){
    result values{};values.fill(1469598103934665603ull);
    auto bytes=[&](size_t group,const void *data,size_t size){const auto *p=static_cast<const uint8_t *>(data);
        for(size_t i=0;i<size;++i)values[group]=(values[group]^p[i])*1099511628211ull;};
    auto add=[&](size_t group,const auto &value){bytes(group,&value,sizeof(value));};
    for(uint32_t n:{scene.struct_size,scene.width,scene.height,scene.mode,scene.mesh_count,scene.texture_count,scene.draw_count,scene.bone_count,scene.cell_count})add(0,n);
    for(uint64_t n:{scene.world_epoch,scene.tileset_epoch,scene.frame_serial})add(0,n);
    // Camera consists only of thirteen four-float arrays.
    static_assert(sizeof(ncmm_gpu_camera_v1)==13*4*sizeof(float),"camera has no padding");add(1,scene.camera);
    for(uint32_t i=0;i<scene.mesh_count;++i){const auto &m=scene.meshes[i];add(0,m.key);
        for(uint32_t n:{m.vertex_count,m.index_count,m.material_count,m.reserved})add(0,n);
        for(uint32_t k=0;k<m.vertex_count;++k){const auto &v=m.vertices[k];add(2,v.position);add(3,v.normal);add(3,v.face_normal);add(4,v.uv);
            for(uint32_t n:{v.material,v.joint,v.flags})add(5,n);add(5,v.pivot);}
        bytes(6,m.indices,size_t(m.index_count)*sizeof(uint32_t));
        for(uint32_t k=0;k<m.material_count;++k){const auto &a=m.materials[k];add(7,a.color);
            for(uint64_t n:{a.texture_key,a.normal_key,a.depth_key})add(7,n);
            for(uint32_t n:{a.flags,a.finish,a.liquid,a.banks})add(7,n);add(7,a.impostor_span);add(7,a.impostor_yaw);}
    }
    for(uint32_t i=0;i<scene.draw_count;++i){const auto &d=scene.draws[i];add(8,d.mesh_key);add(8,d.transform.columns);
        for(uint32_t n:{d.bone_offset,d.bone_count,d.cell,d.pass})add(8,n);add(8,d.creature_yaw);}
    for(uint32_t i=0;i<scene.bone_count;++i)add(9,scene.bones[i].columns);
    for(uint32_t i=0;i<scene.cell_count;++i){const auto &c=scene.cells[i];add(10,c.light);add(10,c.source);add(10,c.volume);add(10,c.volume_color);add(10,c.flags);add(10,c.gas);}
    for(uint32_t i=0;i<scene.texture_count;++i){const auto &t=scene.textures[i];add(11,t.key);add(11,t.width);add(11,t.height);
        for(uint32_t y=0;y<t.height;++y)bytes(11,t.rgba+size_t(y)*t.pitch,size_t(t.width)*sizeof(uint32_t));}
    add(12,bool(world));if(world){add(12,world->version);add(12,world->cell_count);add(12,world->origin_x);add(12,world->origin_y);add(12,world->origin_z);add(12,world->level_height);
        for(uint32_t i=0;i<world->cell_count;++i){const auto &c=world->coordinates[i];add(12,c.x);add(12,c.y);add(12,c.z);add(12,c.flags);add(12,c.direction);add(12,c.revision);add(12,c.lower_light);}}
    return values;
}
template<class Base,void (*Trace)(const ncmm_gpu_scene_v1 &,const ncmm_gpu_multilevel_scene_v1 *)>
class traced_backend:public Base {
public:
    using Base::Base;
    bool submit(const ncmm_gpu_scene_v1 &scene,const ncmm_gpu_multilevel_scene_v1 *world=nullptr){
        const bool success=Base::submit(scene,world);if(success)Trace(scene,world);return success;
    }
};
}
