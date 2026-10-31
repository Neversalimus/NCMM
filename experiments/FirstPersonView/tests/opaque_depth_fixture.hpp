#pragma once
#include "ncmm_gpu_winding.hpp"
#include "ncmm_gpu_validation.hpp"
#include <array>

// Owned pure data shared by the device fixture and its device-free contract
// test. The sparse-world mode, coordinate lifetime and ceiling light agree.
struct opaque_depth_fixture {
    ncmm_gpu_scene_v1 scene{};
    ncmm_gpu_multilevel_scene_v1 world{};
    ncmm_world_cell_v1 coordinate{};
    ncmm_gpu_cell_v1 cell{};
    ncmm_gpu_matrix_v1 bone{};
    std::array<ncmm_gpu_material_v1,2> materials{};
    std::array<ncmm_gpu_vertex_v1,6> vertices{};
    std::array<uint32_t,6> indices{0,1,2,3,4,5};
    ncmm_gpu_mesh_v1 mesh{};
    ncmm_gpu_draw_v1 draw{};
    opaque_depth_fixture(){
        scene.struct_size=sizeof(scene);scene.width=640;scene.height=480;scene.mode=NCMM_GPU_FORCE;scene.frame_serial=5000;
        scene.camera.right[0]=-1;scene.camera.up[2]=1;scene.camera.forward[1]=1;
        scene.camera.projection[0]=640;scene.camera.projection[1]=480;scene.camera.projection[2]=457;scene.camera.projection[3]=512;
        scene.camera.grid[0]=scene.camera.grid[1]=scene.camera.grid[2]=scene.camera.grid[3]=1;
        cell.flags=NCMM_GFX_VISIBLE|NCMM_GFX_CEILING;scene.cells=&cell;scene.cell_count=1;
        for(unsigned n=0;n<4;++n)bone.columns[n*5]=1;scene.bones=&bone;scene.bone_count=1;
        materials[0].flags=NCMM_GPU_EMISSIVE;materials[0].color[0]=materials[0].color[3]=1;
        materials[1].flags=NCMM_GPU_SMOOTH|NCMM_GPU_EMISSIVE;materials[1].color[2]=materials[1].color[3]=1;
        mesh={0x444550540000u,vertices.data(),indices.data(),materials.data(),6,6,2,0};scene.meshes=&mesh;scene.mesh_count=1;
        draw.transform=bone;draw.bone_count=1;scene.draws=&draw;scene.draw_count=1;
        coordinate.flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_ROOFED;
        world.struct_size=sizeof(world);world.version=1;world.coordinates=&coordinate;world.cell_count=1;world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    }
    opaque_depth_fixture(const opaque_depth_fixture&)=delete;
    opaque_depth_fixture &operator=(const opaque_depth_fixture&)=delete;
    void observe(float distance,bool coplanar){
        for(unsigned n=0;n<6;++n){const unsigned corner=n%3;auto &v=vertices[n];v={};v.material=n/3;
            v.position[0]=(corner==0?-1.f:corner==1?1.f:0.f)*distance*.5f;
            v.position[1]=distance+(n>=3&&!coplanar?distance*.001f:0.f);
            v.position[2]=(corner==2?1.f:-1.f)*distance*.5f;v.normal[1]=v.face_normal[1]=-1;}
        draw.mesh_key=++mesh.key;++scene.frame_serial;world.base=scene;
    }
    void component_case(unsigned which){
        materials[0].flags=0;materials[0].color[0]=materials[0].color[3]=1;
        materials[1].flags=NCMM_GPU_SMOOTH|(which==2?4096u:0u);materials[1].color[2]=1;
        materials[1].color[3]=which==3?.991f:1.f;
        cell.light[0]=1;cell.light[1]=.8f;cell.light[2]=.65f;
        draw.cell=which==0?UINT32_MAX:0;observe(1.f,true);
    }
};
