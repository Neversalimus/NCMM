#include "ncmm_gpu_winding.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {
unsigned checks=0;
void require(bool value,const char *why){++checks;if(!value){std::fprintf(stderr,"winding data proof: %s\n",why);std::exit(1);}}
ncmm_gpu_matrix_v1 identity(){ncmm_gpu_matrix_v1 m{};for(unsigned n=0;n<4;++n)m.columns[n*5]=1;return m;}
}
int main(){
    auto pose=identity();require(ncmm_gpu::winding_pose(pose),"identity is a proper rigid pose");
    require(ncmm_gpu::winding_bone(pose),"the exact static root bone is accepted");
    for(unsigned n=0;n<20000;++n){const float yaw=float(n)*.037f,pitch=float(n%101)*.013f;
        const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
        pose=identity();pose.columns[0]=cy;pose.columns[1]=sy;pose.columns[4]=-sy;pose.columns[5]=cy;pose.columns[12]=float(n%300)-150;
        require(ncmm_gpu::winding_pose(pose),"real yaw/translation keeps the normal/index handedness");
        auto mirror=pose;for(unsigned j=0;j<3;++j)mirror.columns[j]=-mirror.columns[j];
        require(!ncmm_gpu::winding_pose(mirror),"reflected draws retain two-sided rasterization");
        auto scale=pose;for(unsigned j=0;j<3;++j)scale.columns[j]*=1.1f;
        require(!ncmm_gpu::winding_pose(scale),"nonrigid scale retains the original shader path");
        ncmm_gpu_camera_v1 camera{};
        camera.right[0]=-sy;camera.right[1]=cy;camera.up[0]=-cy*sp;camera.up[1]=-sy*sp;camera.up[2]=cp;
        camera.forward[0]=cy*cp;camera.forward[1]=sy*cp;camera.forward[2]=sp;
        require(ncmm_gpu::winding_camera(camera),"the actual right/up/forward view has the proven clockwise front");
        camera.up[2]+=1.f;require(!ncmm_gpu::winding_camera(camera),"unproved projection basis falls back");
        auto animated=identity();animated.columns[12]=.01f;
        require(!ncmm_gpu::winding_bone(animated),"animated bones cannot use the static proof");
    }
    std::array<ncmm_gpu_vertex_v1,3> vertices{};vertices[1].position[0]=1.5f;vertices[2].position[1]=1.5f;
    for(auto &v:vertices)v.face_normal[2]=1;
    std::array<uint32_t,3> indices{0,1,2};ncmm_gpu_material_v1 material{};material.flags=NCMM_GPU_CULL_BACK|NCMM_GPU_SMOOTH;material.color[3]=1;
    const ncmm_gpu_mesh_v1 mesh{1,vertices.data(),indices.data(),&material,3,3,1,0};
    auto proof=[&]{return ncmm_gpu::winding_range(mesh,0,0,3);};
    require(proof(),"the normalized actual cube face proves its existing shader side condition");
    std::swap(indices[1],indices[2]);require(!proof(),"opposite index winding is not silently reinterpreted");std::swap(indices[1],indices[2]);
    vertices[2].flags=NCMM_GPU_BILLBOARD;require(!proof(),"billboard positions need the original vertex path");vertices[2].flags=0;
    vertices[2].joint=1;require(!proof(),"skinned corners need the original path");vertices[2].joint=0;
    vertices[2].face_normal[2]=std::nextafter(1.f,2.f);require(!proof(),"even one unmatched normal bit rejects optimization");vertices[2].face_normal[2]=1;
    material.color[3]=.995f;require(!proof(),"fractional opaque alpha cannot change its raster state");material.color[3]=1;
    material.flags|=NCMM_GPU_IMPOSTOR;require(!proof(),"depth-altering impostors remain unchanged");material.flags&=~NCMM_GPU_IMPOSTOR;
    material.flags&=~NCMM_GPU_CULL_BACK;require(!proof(),"two-sided surfaces remain two-sided");material.flags|=NCMM_GPU_CULL_BACK;
    vertices[2].position[0]=std::numeric_limits<float>::quiet_NaN();require(!proof(),"nonfinite source data never proves eligibility");vertices[2].position[0]=0;
    require(!ncmm_gpu::winding_range(mesh,0,1,3)&&!ncmm_gpu::winding_range(mesh,1,0,3),"range and material bounds remain strict");
    pose=identity();pose.columns[3]=.01f;require(!ncmm_gpu::winding_pose(pose),"perspective instance matrices fall back");
    for(unsigned n=0;n<10000;++n){vertices={};const float angle=float(n)*.071f;
        vertices[1].position[0]=1;vertices[1].position[1]=.1f+.4f*std::sin(angle);vertices[1].position[2]=.2f;
        vertices[2].position[0]=.3f;vertices[2].position[1]=1.1f;vertices[2].position[2]=.4f+.1f*std::cos(angle);
        const auto *x=vertices[1].position,*y=vertices[2].position;
        const float normal[]={x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]};
        const float inverse=1/std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
        for(auto &v:vertices)for(unsigned j=0;j<3;++j)v.face_normal[j]=normal[j]*inverse;
        require(proof(),"curved-source unit directions use exactly the producer's multiply-by-inverse arithmetic");
        for(auto &v:vertices)v.face_normal[0]=std::nextafter(v.face_normal[0],2.f);
        require(!proof(),"a uniformly perturbed plane is not admitted through a normal tolerance");
    }
    std::printf("GPU winding: %u pure data/math checks passed; no SDL/device execution\n",checks);
}
