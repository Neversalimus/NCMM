#include "ncmm_gpu_vertex_pack.hpp"
#include "gpu_scene.hpp"
#include "vehicle_frame_caps_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>

namespace gpu=first_person::gpu;
namespace pack=ncmm_gpu::vertex_pack;
static uint64_t checks=0,meshes=0,active=0,corners=0,original_vertices=0,packed_vertices=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"GPU vertex packing data: %s\n",why);std::exit(2);}}
template<class T>static bool bytes(const std::vector<T>&a,const std::vector<T>&b){return a.size()==b.size()&&(a.empty()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(T)));}
static gpu::owned_mesh grid(){
    gpu::owned_mesh mesh;ncmm_gpu_material_v1 material{};material.flags=NCMM_GPU_SMOOTH;material.finish=7;material.color[0]=.4f;material.color[1]=.3f;material.color[2]=.2f;material.color[3]=1;mesh.materials.push_back(material);
    const unsigned order[]={0,1,2,0,2,3};
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)for(unsigned n=0;n<6;++n){const unsigned corner=order[n];ncmm_gpu_vertex_v1 v{};
        v.position[0]=float(x+(corner==1||corner==2))*.1f;v.position[1]=float(y+(corner>=2))*.1f;v.normal[2]=v.face_normal[2]=1;
        v.uv[0]=float(n)+.125f;v.uv[1]=float(x+y+n)+.25f;mesh.indices.push_back(uint32_t(mesh.vertices.size()));mesh.vertices.push_back(v);}
    mesh.key=gpu::mesh_key(mesh);return mesh;
}
static void verify(const ncmm_gpu_mesh_v1 &source,const std::vector<uint32_t>&ordered){
    const auto saved=source;
    const std::vector<ncmm_gpu_vertex_v1> vertices(source.vertices,source.vertices+source.vertex_count);
    const std::vector<uint32_t> indices(source.indices,source.indices+source.index_count),order=ordered;
    const std::vector<ncmm_gpu_material_v1> materials(source.materials,source.materials+source.material_count);
    const auto output=pack::compact(source,ordered);++meshes;
    require(!std::memcmp(&saved,&source,sizeof(source)),"original ABI source identity, pointers and dimensions retained");
    require(!std::memcmp(vertices.data(),source.vertices,vertices.size()*sizeof(vertices[0]))&&
        !std::memcmp(indices.data(),source.indices,indices.size()*4)&&!std::memcmp(materials.data(),source.materials,materials.size()*sizeof(materials[0]))&&order==ordered,
        "every original callback vertex, index, material and ordered CPU/pick index bit retained");
    if(!output.active){require(output.vertices.empty()&&output.indices.empty(),"fallback publishes no alternate payload");return;}
    ++active;original_vertices+=source.vertex_count;packed_vertices+=output.vertices.size();
    require(output.indices.size()==ordered.size()&&output.vertices.size()+8<=source.vertex_count,"positive vertex saving with the complete original index count");
    for(size_t n=0;n<ordered.size();++n){require(output.indices[n]<output.vertices.size(),"each compact GPU index remains in bounds");
        auto a=source.vertices[ordered[n]],b=output.vertices[output.indices[n]];a.uv[0]=a.uv[1]=0;
        require(!std::memcmp(&a,&b,sizeof(a)),"every ordered indexed triangle corner retains all non-UV bits including hard normals/material identity/signed zeros");++corners;}
    const auto again=pack::compact(source,ordered);require(again.active&&bytes(output.vertices,again.vertices)&&output.indices==again.indices,"packing is deterministic and independent of allocation address");
}
static void fallbacks(){
    auto original=grid();const auto view=original.view();require(pack::compact(view,original.indices).active,"independent authored mesh positively exercises compact representation");
    for(unsigned n=0;n<40;++n){auto m=original;auto s=m.view();auto order=m.indices;
        switch(n){case 0:s.key=0;break;case 1:s.reserved=NCMM_GPU_DYNAMIC_MESH;break;case 2:s.vertices=nullptr;break;case 3:s.indices=nullptr;break;case 4:s.materials=nullptr;break;
            case 5:s.vertex_count=63;break;case 6:s.vertex_count=65537;break;case 7:s.index_count=0;break;case 8:--s.index_count;break;case 9:s.material_count=0;break;case 10:s.material_count=257;break;
            case 11:order.pop_back();break;case 12:order[0]=s.vertex_count;break;case 13:m.vertices[0].joint=1;break;case 14:m.vertices[0].flags=NCMM_GPU_BILLBOARD;break;case 15:m.vertices[0].reserved=1;break;
            case 16:m.vertices[0].material=1;break;case 17:m.vertices[0].pivot[0]=.001f;break;case 18:m.vertices[0].position[2]=INFINITY;break;case 19:m.vertices[0].normal[2]=NAN;break;
            case 20:m.vertices[0].face_normal[2]=-INFINITY;break;case 21:m.vertices[0].uv[0]=NAN;break;case 22:m.vertices[0].uv[1]=INFINITY;break;
            case 23:for(auto &v:m.vertices)v.uv[0]=v.uv[1]=0;break;case 24:m.materials[0].texture_key=1;break;case 25:m.materials[0].normal_key=1;break;case 26:m.materials[0].depth_key=1;break;
            case 27:m.materials[0].liquid=1;break;case 28:m.materials[0].banks=1;break;case 29:m.materials[0].impostor_span=1;break;case 30:m.materials[0].impostor_yaw=1;break;
            case 31:m.materials[0].color[3]=.999f;break;case 32:m.materials[0].color[0]=NAN;break;case 33:m.materials[0].color[1]=INFINITY;break;
            case 34:m.materials[0].color[2]=-.01f;break;case 35:m.materials[0].color[0]=1.01f;break;case 36:m.materials[0].finish=12;break;case 37:m.materials[0].flags=0;break;
            case 38:m.materials[0].reserved[0]=1;break;case 39:m.materials[0].reserved[1]=1;break;}
        const auto out=pack::compact(s,order);require(!out.active&&out.vertices.empty()&&out.indices.empty(),"unsupported whole mesh preserves original upload representation");}
    for(unsigned bit=0;bit<32;++bit){auto m=original;const uint32_t flag=uint32_t(1)<<bit;m.materials[0].flags|=flag;
        const bool allowed=(flag&(NCMM_GPU_SMOOTH|NCMM_GPU_CULL_BACK|NCMM_GPU_EMISSIVE))!=0;
        require(pack::compact(m.view(),m.indices).active==allowed,"every unsupported material flag, including unknown future flags, retains original representation");}
    for(uint32_t finish=0;finish<32;++finish){auto m=original;m.materials[0].finish=finish;
        const bool allowed=finish==0||finish==5||finish==7||finish==8||finish==9||finish==10||finish==11;
        require(pack::compact(m.view(),m.indices).active==allowed,"only current proved untextured material families activate");}
    auto no_seams=original;for(auto &v:no_seams.vertices)v.position[2]=float(&v-no_seams.vertices.data());
    require(!pack::compact(no_seams.view(),no_seams.indices).active,"no duplicate non-UV corners gives an exact fallback");
    for(auto &v:original.vertices)v.position[2]=-0.f;
    original.key=gpu::mesh_key(original);verify(original.view(),original.indices);
    for(unsigned material=0;material<8;++material){auto m=original;m.materials.resize(material+1,m.materials[0]);
        for(size_t n=0;n<m.vertices.size();++n)m.vertices[n].material=uint32_t(n/3)%(material+1);
        std::vector<uint32_t> ordered=m.indices;std::stable_sort(ordered.begin(),ordered.end(),[&](uint32_t a,uint32_t b){return m.vertices[a].material<m.vertices[b].material;});
        m.key=gpu::mesh_key(m);verify(m.view(),ordered);}
    // Original producer content keys are never replaced by compact-buffer keys.
    // Differently colored coincident surfaces retain the existing exact order.
    std::vector<uint64_t> before,after;
    for(unsigned n=0;n<64;++n){auto m=original;m.materials[0].color[0]=.1f+float(n%8)*.12f;m.materials[0].color[1]=.1f+float((n/8)%4)*.2f;m.materials[0].color[2]=.2f+float(n%5)*.15f;
        m.key=gpu::mesh_key(m);before.push_back(m.key);verify(m.view(),m.indices);after.push_back(m.key);require(m.key==gpu::mesh_key(m),"original content key remains valid after packing");}
    require(before==after,"all64 coplanar colored source keys remain in their original order");
    std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());require(before==after,"key-ordered backend batches remain exact for coincident colored surfaces");
}
static void actual_packets(){
    namespace fp=first_person;namespace v=fp::vehicle;
    std::vector<ncmm_graphics_cell_v1> cells(625);std::vector<ncmm_graphics_cell_v2> details(625);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,25,25,12.5f,23.5f,cells.data(),625};source.details=details.data();source.detail_count=625;source.environment.daylight=1;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(625);
    v::presentation vehicles;metadata.vehicles=&vehicles;gpu::scene_cache cache;fp::hybrid::frame statistics;ncmm_gpu_metrics_v1 metrics{};
    for(unsigned variant=0;variant<5;++variant){vehicles.reset();vehicles.input=frame_caps_fixture::construction();for(auto &c:cells)c.flags=NCMM_GFX_VISIBLE;
        if(variant==1)for(auto &p:vehicles.input.parts)if(p.mount_x==6)p.state|=NCMM_VP_TOPOLOGY_ONLY,p.cell_index=-1;
        if(variant==2)for(const auto &p:vehicles.input.parts)if(p.mount_x==6)cells[size_t(p.cell_index)].flags=0;
        if(variant==3)vehicles.input.vehicles[0].yaw=.37f;
        if(variant==4){++vehicles.input.scene.world_epoch;for(auto &p:vehicles.input.parts)p.health=.35f;}
        for(const auto &camera:frame_caps_fixture::cameras())for(bool join:{false,true}){source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
            gpu::packet packet;fp::hybrid::render_options options;options.join_gpu_frame_caps=join;
            require(cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,metrics,{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE,nullptr,options),"unchanged source66 real packet builder and validation");
            const auto &scene=packet.finish(nullptr);const std::vector<ncmm_gpu_draw_v1> draws(scene.draws,scene.draws+scene.draw_count);
            for(uint32_t i=0;i<scene.mesh_count;++i){const auto &mesh=scene.meshes[i];std::vector<uint32_t> ordered(mesh.indices,mesh.indices+mesh.index_count);
                std::stable_sort(ordered.begin(),ordered.end(),[&](uint32_t a,uint32_t b){return mesh.vertices[a].material<mesh.vertices[b].material;});verify(mesh,ordered);}
            require(!std::memcmp(draws.data(),scene.draws,draws.size()*sizeof(draws[0])),"all original public draw/mesh keys, passes, cells and transforms retained");}
    }
}
int main(){fallbacks();actual_packets();require(active>100&&corners>100000,"actual complete production packets positively exercise corner sharing");
    std::printf("{\"success\":true,\"checks\":%llu,\"meshes\":%llu,\"active_meshes\":%llu,\"ordered_corners\":%llu,\"original_active_vertices\":%llu,\"packed_active_vertices\":%llu,\"actual_packet_configurations\":100,\"whole_mesh_fallback_controls\":40,\"material_flag_controls\":32,\"material_finish_controls\":32,\"coplanar_color_keys\":64,\"no_device_or_raster\":true}\n",
        (unsigned long long)checks,(unsigned long long)meshes,(unsigned long long)active,(unsigned long long)corners,(unsigned long long)original_vertices,(unsigned long long)packed_vertices);}
