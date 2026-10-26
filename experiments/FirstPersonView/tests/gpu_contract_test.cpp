#include "ncmm_gpu_validation.hpp"
#include "gpu_packet.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

int main() {
    static_assert(std::is_standard_layout_v<ncmm_gpu_scene_v1>);
    static_assert(sizeof(ncmm_gpu_vertex_v1)==72);
    static_assert(sizeof(ncmm_gpu_matrix_v1)==64);
    static_assert(sizeof(ncmm_gpu_cell_v1)==80);
    static_assert(sizeof(ncmm_gpu_camera_v1)==208);
    int checks=0;auto check=[&](bool ok){if(!ok){std::fprintf(stderr,"GPU contract check %d failed\n",checks);std::exit(1);}++checks;};
    ncmm_gpu_scene_v1 s{};s.struct_size=sizeof(s);s.width=640;s.height=480;
    s.camera.projection[0]=640;s.camera.projection[1]=480;s.camera.projection[2]=457;s.camera.projection[3]=512;
    s.camera.grid[0]=s.camera.grid[1]=s.camera.grid[2]=1;
    std::array<ncmm_gpu_vertex_v1,3> vertices{};const uint32_t indices[]={0,1,2};
    ncmm_gpu_material_v1 material{};material.color[3]=1;
    ncmm_gpu_mesh_v1 mesh{};mesh.key=1;mesh.vertices=vertices.data();mesh.indices=indices;mesh.materials=&material;
    mesh.vertex_count=3;mesh.index_count=3;mesh.material_count=1;
    ncmm_gpu_matrix_v1 identity{};for(int i=0;i<4;++i)identity.columns[i*5]=1;
    ncmm_gpu_draw_v1 draw{};draw.mesh_key=mesh.key;draw.transform=identity;draw.bone_count=1;
    ncmm_gpu_cell_v1 cell{};cell.light[0]=cell.light[1]=cell.light[2]=1;
    s.meshes=&mesh;s.mesh_count=1;s.draws=&draw;s.draw_count=1;s.bones=&identity;s.bone_count=1;s.cells=&cell;s.cell_count=1;
    check(!ncmm_gpu::validate(s));
    vertices[1].position[0]=std::numeric_limits<float>::quiet_NaN();check(ncmm_gpu::validate(s));vertices[1].position[0]=0;
    uint32_t bad[]={0,1,3};mesh.indices=bad;check(ncmm_gpu::validate(s));mesh.indices=indices;
    mesh.vertex_count=NCMM_GPU_MAX_VERTICES+1;check(ncmm_gpu::validate(s));mesh.vertex_count=3;
    vertices[0].material=1;check(ncmm_gpu::validate(s));vertices[0].material=0;
    material.flags=~4095u;check(ncmm_gpu::validate(s));material.flags=0;
    draw.bone_offset=UINT32_MAX;check(ncmm_gpu::validate(s));draw.bone_offset=0;
    draw.cell=1;check(ncmm_gpu::validate(s));draw.cell=0;
    draw.transform.columns[2]=std::numeric_limits<float>::infinity();check(ncmm_gpu::validate(s));draw.transform=identity;
    s.draws=nullptr;check(ncmm_gpu::validate(s));s.draws=&draw;
    s.camera.projection[3]=.01f;check(ncmm_gpu::validate(s));s.camera.projection[3]=512;
    ncmm_gpu_texture_v1 t{};t.key=4;t.width=t.height=1;t.pitch=1;uint32_t pixel=0xffffffff;t.rgba=&pixel;s.textures=&t;s.texture_count=1;
    check(!ncmm_gpu::validate(s));t.pitch=0;check(ncmm_gpu::validate(s));t.pitch=1;
    t.width=513;check(ncmm_gpu::validate(s));t.width=1;
    s.mode=NCMM_GPU_CPU;check(!ncmm_gpu::validate(s));s.mode=3;check(ncmm_gpu::validate(s));
    s.mode=NCMM_GPU_FORCE;material.flags=NCMM_GPU_TEXTURED;check(ncmm_gpu::validate(s));material.texture_key=4;check(!ncmm_gpu::validate(s));
    material.flags=NCMM_GPU_IMPOSTOR;check(ncmm_gpu::validate(s));material.normal_key=material.depth_key=4;material.impostor_span=1;check(!ncmm_gpu::validate(s));
    material.impostor_span=0;check(ncmm_gpu::validate(s));
    {
        using namespace first_person::gpu;
        packet reused;auto owner=std::make_shared<owned_mesh>();owner->key=71;
        owner->vertices.assign(vertices.begin(),vertices.end());owner->indices={0,1,2};owner->materials.resize(1);
        std::weak_ptr<const owned_mesh> released=owner;
        reused.draw(owner,identity,0,NCMM_GPU_OPAQUE);reused.cells.resize(64);reused.finish(nullptr);
        const auto *draw_storage=reused.draws.data();const auto *cell_storage=reused.cells.data();
        const size_t scratch=reused.scratch_bytes();owner.reset();check(!released.expired());
        reused.reset();check(released.expired());
        check(reused.draws.data()==draw_storage&&reused.cells.data()==cell_storage&&reused.scratch_bytes()==scratch);
        const auto &empty=reused.finish(nullptr);check(!empty.draw_count&&!empty.cell_count&&!empty.mesh_count&&!empty.texture_count&&empty.bone_count==1);
        check(!empty.world_epoch&&!empty.frame_serial&&!empty.width&&!empty.height);
        reused.draws.reserve(packet::scratch_limit/sizeof(ncmm_gpu_draw_v1)+1);reused.reset();
        check(reused.scratch_bytes()<=sizeof(ncmm_gpu_matrix_v1));
    }
    std::printf("GPU submission contract: %d checks PASS\n",checks);return 0;
}
