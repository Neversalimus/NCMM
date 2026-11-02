#include "gpu_scene.hpp"
#include "ncmm_gpu_material_palette.hpp"
#include "vehicle_body_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
namespace fp=first_person;namespace v=first_person::vehicle;
namespace palette=ncmm_gpu::material_palette;
static uint64_t checks=0;static unsigned order_proved=0,order_fallback=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"material palette guard: %s\n",why);std::exit(4);}}
static void inspect_palette(const ncmm_gpu_mesh_v1 &mesh,const ncmm_gpu_scene_v1 &scene,uint32_t pass){
    require(pass==NCMM_GPU_OPAQUE&&palette::eligible(mesh),"actual92colour wheel packet proves all indexed opaque uniform-table requirements");
    const std::vector<ncmm_gpu_material_v1> materials(mesh.materials,mesh.materials+mesh.material_count);const auto table=palette::encode(materials);
    for(size_t i=0;i<materials.size();++i){const auto &m=materials[i];require(std::memcmp(table[i].color,m.color,16)==0,"all original colour float words survive without rounding or quantization");
        require(table[i].state[0]==m.flags&&table[i].state[1]==m.finish&&table[i].state[2]==m.liquid&&table[i].state[3]==m.banks,"every original material finish and state word survives");}
    for(uint32_t i=0;i<mesh.index_count;++i){const auto mi=mesh.vertices[mesh.indices[i]].material;require(std::memcmp(table[mi].color,materials[mi].color,16)==0,"every original indexed corner still selects its exact original colour");}
    auto view=mesh;auto changed=materials;
    const auto reject_material=[&](ncmm_gpu_material_v1 m){changed=materials;changed[0]=m;view=mesh;view.materials=changed.data();require(!palette::eligible(view),"unproved appearance retains the original per-material fallback");};
    auto m=materials[0];m.color[3]=.99f;reject_material(m);m=materials[0];m.color[0]=std::numeric_limits<float>::quiet_NaN();reject_material(m);
    m=materials[0];m.texture_key=3;reject_material(m);m=materials[0];m.normal_key=2;reject_material(m);m=materials[0];m.depth_key=5;reject_material(m);
    m=materials[0];m.flags|=NCMM_GPU_EMISSIVE;reject_material(m);m=materials[0];m.flags|=NCMM_GPU_CULL_BACK;reject_material(m);
    m=materials[0];m.finish=12;reject_material(m);m=materials[0];m.liquid=1;reject_material(m);m=materials[0];m.banks=4;reject_material(m);m=materials[0];m.impostor_span=1;reject_material(m);
    std::vector<ncmm_gpu_vertex_v1> vertices(mesh.vertices,mesh.vertices+mesh.vertex_count);view=mesh;view.vertices=vertices.data();vertices[0].joint=1;require(!palette::eligible(view),"animated corners retain original representation");
    vertices[0]=mesh.vertices[0];vertices[0].flags=NCMM_GPU_BILLBOARD;require(!palette::eligible(view),"billboard corners retain original representation");
    vertices[0]=mesh.vertices[0];vertices[0].material=mesh.material_count;require(!palette::eligible(view),"invalid material indices are rejected before uniform access");
    vertices[0]=mesh.vertices[0];vertices[0].position[0]=std::numeric_limits<float>::infinity();require(!palette::eligible(view),"nonfinite bounds retain fallback");
    view=mesh;view.material_count=129;require(!palette::eligible(view),"palette upload capacity is strictly bounded");view=mesh;view.reserved=1;require(!palette::eligible(view),"dynamic resources retain their original path");
    std::vector<uint32_t> indices(mesh.indices,mesh.indices+mesh.index_count);view=mesh;view.indices=indices.data();indices[0]=mesh.vertex_count;require(!palette::eligible(view),"invalid triangle corners are rejected before dereference");
    std::vector<ncmm_gpu_matrix_v1> poses;for(uint32_t i=0;i<scene.draw_count;++i)if(scene.draws[i].mesh_key==mesh.key&&scene.draws[i].pass==pass)poses.push_back(scene.draws[i].transform);
    require(poses.size()==6,"the owned real bus fixture retains every one of its six actual wheel instances");
    const bool safe=palette::instance_order(mesh,poses,scene.camera);order_proved+=safe;order_fallback+=!safe;
    std::vector<ncmm_gpu_matrix_v1> one{poses[0]};require(palette::instance_order(mesh,one,scene.camera),"one instance retains the exact original material-sorted primitive order");
    std::vector<ncmm_gpu_matrix_v1> identical{poses[0],poses[0]};require(!palette::instance_order(mesh,identical,scene.camera),"overlapping equal-depth instances retain the original range-major order");
    auto invalid=poses;invalid[0].columns[0]=std::numeric_limits<float>::quiet_NaN();require(!palette::instance_order(mesh,invalid,scene.camera),"invalid transforms cannot prove reordered instances");
    auto camera=scene.camera;camera.forward[0]=std::numeric_limits<float>::infinity();require(!palette::instance_order(mesh,identical,camera),"invalid cameras retain the original ordering");
    camera=scene.camera;camera.projection[2]=0;require(!palette::instance_order(mesh,identical,camera),"unknown projection retains original ordering");
    require(!palette::instance_order(mesh,{},scene.camera),"empty or unproved groups do not activate the new path");
}
int main(){
    std::vector<ncmm_graphics_cell_v1> cells(1600);std::vector<ncmm_graphics_cell_v2> details(1600);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,40,40,23.5f,30.5f,cells.data(),1600};source.details=details.data();source.detail_count=1600;source.environment.daylight=1;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(1600);
    v::presentation vehicles;metadata.vehicles=&vehicles;fp::gpu::scene_cache cache;fp::hybrid::frame statistics;ncmm_gpu_metrics_v1 metrics{};vehicles.input=body_occlusion_fixture::construction();
    const auto views=body_occlusion_fixture::cameras();
    for(unsigned vi=0;vi<views.size();++vi){const auto &c=views[vi];source.base.camera_x=c.eye.x;source.base.camera_y=c.eye.y;metadata.view.eye_height=c.eye.z;vehicles.observe(c.eye);
        fp::gpu::packet packet;fp::hybrid::render_options options;
        if(!cache.build(source,c.yaw,c.pitch,{},packet,statistics,metrics,{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE,nullptr,options)){std::fprintf(stderr,"packet failed: %s\n",cache.last_reason.c_str());return 2;}
        const auto &scene=packet.finish(nullptr);std::map<uint64_t,const ncmm_gpu_mesh_v1 *> meshes;
        for(uint32_t i=0;i<scene.mesh_count;++i)meshes.emplace(scene.meshes[i].key,&scene.meshes[i]);
        std::map<std::pair<uint32_t,uint64_t>,uint32_t> groups;for(uint32_t i=0;i<scene.draw_count;++i)++groups[{scene.draws[i].pass,scene.draws[i].mesh_key}];
        uint64_t ranges=0,palette_ranges=0,palette_triangles=0;unsigned palette_groups=0,largest=0;
        for(const auto &[key,count]:groups){const auto &mesh=*meshes.at(key.second);std::set<uint32_t> used;for(uint32_t i=0;i<mesh.index_count;++i)used.insert(mesh.vertices[mesh.indices[i]].material);
            ranges+=used.size()*(key.first==NCMM_GPU_TRANSPARENT?count:1u);
            if(used.size()>=20){inspect_palette(mesh,scene,key.first);++palette_groups;palette_ranges+=used.size();palette_triangles+=mesh.index_count/3;largest=std::max(largest,unsigned(used.size()));
                for(auto mi:used){const auto &m=mesh.materials[mi];if(m.color[3]!=1||m.texture_key||m.normal_key||m.depth_key||m.liquid||m.banks||(m.flags&~uint32_t(NCMM_GPU_SMOOTH|NCMM_GPU_CULL_BACK))){std::fprintf(stderr,"palette is not proved opaque untextured fixed material\n");return 3;}}
            }
        }
    }
    std::printf("palette_guard_checks=%llu order_proved=%u fallback=%u\n",(unsigned long long)checks,order_proved,order_fallback);
    std::fprintf(stderr,"Pure actual producer packet only; material-range upper bounds precede backend frustum/depth culling. No actual draw calls, image or FPS measured.\n");
}
