#pragma once
#include "ncmm_graphics_gpu.h"
#include "ncmm_gpu_winding.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
namespace ncmm_gpu::material_palette {
inline constexpr uint32_t maximum_materials=128;
struct alignas(16) entry {float color[4];uint32_t state[4];};
using table=std::array<entry,maximum_materials>;
static_assert(sizeof(entry)==32&&offsetof(entry,state)==16&&sizeof(table)==4096,"exact bounded std140 colour/finish table");
inline bool material(const ncmm_gpu_material_v1 &m){
    if(m.flags!=NCMM_GPU_SMOOTH||m.color[3]!=1.f||m.texture_key||m.normal_key||m.depth_key||m.liquid||m.banks||m.impostor_span||m.impostor_yaw||m.reserved[0]||m.reserved[1])return false;
    for(float x:m.color)if(!std::isfinite(x)||x<0||x>1)return false;
    return m.finish==0||m.finish==8||m.finish==9;
}
inline bool eligible(const ncmm_gpu_mesh_v1 &m){
    if(!m.key||m.reserved||!m.vertices||!m.indices||!m.materials||m.material_count<8||m.material_count>maximum_materials||
       m.vertex_count<64||m.vertex_count>65536||!m.index_count||m.index_count>196608||m.index_count%3)return false;
    for(uint32_t i=0;i<m.material_count;++i)if(!material(m.materials[i]))return false;
    for(uint32_t i=0;i<m.vertex_count;++i){const auto &v=m.vertices[i];
        if(v.material>=m.material_count||v.joint||v.flags||v.reserved||v.uv[0]!=0||v.uv[1]!=0||v.pivot[0]!=0||v.pivot[1]!=0||v.pivot[2]!=0)return false;
        for(float p:v.position)if(!std::isfinite(p)||std::abs(p)>16)return false;
        for(float p:v.normal)if(!std::isfinite(p))return false;
        for(float p:v.face_normal)if(!std::isfinite(p))return false;
    }
    for(uint32_t i=0;i<m.index_count;i+=3){uint32_t slot=UINT32_MAX;
        for(unsigned j=0;j<3;++j){const auto index=m.indices[i+j];if(index>=m.vertex_count)return false;
            const auto value=m.vertices[index].material;if(j&&value!=slot)return false;slot=value;}}
    return true;
}
inline table encode(const std::vector<ncmm_gpu_material_v1> &materials){table result{};
    if(materials.size()>maximum_materials)return result;
    for(size_t i=0;i<materials.size();++i){const auto &m=materials[i];std::memcpy(result[i].color,m.color,16);
        result[i].state[0]=m.flags;result[i].state[1]=m.finish;result[i].state[2]=m.liquid;result[i].state[3]=m.banks;}
    return result;
}
struct projected_bounds {double x0=1e100,x1=-1e100,y0=1e100,y1=-1e100,z0=1e100,z1=-1e100;};
// The new primitive order is allowed only after a separate caller has proved
// opaque world cells, identity bones and the exact complete mesh eligibility.
// A single instance retains its original material-sorted triangle order.
inline bool instance_order(const ncmm_gpu_mesh_v1 &mesh,const std::vector<ncmm_gpu_matrix_v1> &poses,const ncmm_gpu_camera_v1 &camera){
    if(poses.empty()||poses.size()>16||!mesh.vertices||!mesh.vertex_count)return false;
    for(const auto &p:poses)if(!winding_pose(p))return false;
    if(poses.size()==1)return true;
    for(const float *axis:{camera.eye,camera.right,camera.up,camera.forward})for(unsigned i=0;i<3;++i)if(!std::isfinite(axis[i])||std::abs(axis[i])>128)return false;
    for(const float *axis:{camera.right,camera.up,camera.forward}){double n=0;for(unsigned i=0;i<3;++i)n+=double(axis[i])*axis[i];if(std::abs(n-1)>1e-5)return false;}
    for(unsigned i=0;i<3;++i)for(unsigned j=i+1;j<3;++j){const float *axes[]={camera.right,camera.up,camera.forward};double d=0;
        for(unsigned k=0;k<3;++k)d+=double(axes[i][k])*axes[j][k];
        if(std::abs(d)>1e-5)return false;}
    for(float p:camera.projection)if(!std::isfinite(p)||p<=0||p>16384)return false;
    if(camera.projection[2]>4096)return false;
    std::array<double,3> lo{{1e100,1e100,1e100}},hi{{-1e100,-1e100,-1e100}};
    for(uint32_t i=0;i<mesh.vertex_count;++i)for(unsigned a=0;a<3;++a){const double p=mesh.vertices[i].position[a];
        if(!std::isfinite(p)||std::abs(p)>16)return false;
        lo[a]=std::min(lo[a],p);hi[a]=std::max(hi[a],p);}
    std::vector<projected_bounds> bounds;bounds.reserve(poses.size());
    for(const auto &pose:poses){projected_bounds b;
        for(unsigned mask=0;mask<8;++mask){double world[3]{},q[3]{};
            for(unsigned a=0;a<3;++a){world[a]=pose.columns[12+a];for(unsigned axis=0;axis<3;++axis)world[a]+=double(pose.columns[axis*4+a])*((mask&(1u<<axis))?hi[axis]:lo[axis]);
                if(!std::isfinite(world[a])||std::abs(world[a])>128)return false;
                world[a]-=camera.eye[a];}
            const float *axes[]={camera.right,camera.up,camera.forward};for(unsigned a=0;a<3;++a)for(unsigned k=0;k<3;++k)q[a]+=world[k]*axes[a][k];
            // Fixed conservative safety margins, not a change to rendering
            // distance, clipping, detail or visibility. Unproved groups fall back.
            if(!(q[2]>.5&&q[2]<std::min(64.0,double(camera.projection[3])-.1)))return false;
            const double x=q[0]/q[2]*camera.projection[2],y=q[1]/q[2]*camera.projection[2];
            b.x0=std::min(b.x0,x);b.x1=std::max(b.x1,x);b.y0=std::min(b.y0,y);b.y1=std::max(b.y1,y);b.z0=std::min(b.z0,q[2]);b.z1=std::max(b.z1,q[2]);
        }bounds.push_back(b);
    }
    for(size_t i=0;i<bounds.size();++i)for(size_t j=i+1;j<bounds.size();++j){const auto &a=bounds[i],&b=bounds[j];
        const bool separate=a.x1+8<b.x0||b.x1+8<a.x0||a.y1+8<b.y0||b.y1+8<a.y0||a.z1+.10<b.z0||b.z1+.10<a.z0;
        if(!separate)return false;}
    return true;
}
}
