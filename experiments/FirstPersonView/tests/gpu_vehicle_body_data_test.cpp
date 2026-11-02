// The real GPU packet producer is exercised as pure CPU data preparation.
// No renderer, framebuffer, SDL initialization or device is created/executed.
#include "gpu_scene.hpp"
#include "vehicle_body_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>
namespace fp=first_person;namespace v=first_person::vehicle;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"body packet data: %s\n",why);std::exit(2);}}
struct mesh_data {std::vector<ncmm_gpu_vertex_v1> vertices;std::vector<ncmm_gpu_material_v1> materials;std::vector<uint32_t> indices;uint32_t reserved=0;};
struct snapshot {std::vector<ncmm_gpu_draw_v1> draws;std::map<uint64_t,mesh_data> meshes;uint64_t triangles=0,parts=0;};
template<class T>static bool same(const std::vector<T> &a,const std::vector<T> &b){return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);}
int main(){
    std::vector<ncmm_graphics_cell_v1> cells(1600);std::vector<ncmm_graphics_cell_v2> details(1600);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,40,40,23.5f,30.5f,cells.data(),1600};source.details=details.data();source.detail_count=1600;source.environment.daylight=1;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(1600);
    v::presentation vehicles;metadata.vehicles=&vehicles;fp::gpu::scene_cache cache;fp::hybrid::frame statistics;ncmm_gpu_metrics_v1 metrics{};
    const auto construction=body_occlusion_fixture::construction();require(v::valid(construction,1600),"the owned fixture uses valid actual mount cells and dimensions");
    unsigned active=0,fallback=0,paired=0;uint64_t saved=0,surviving=0,alpha_draws=0;
    const auto views=body_occlusion_fixture::cameras();
    for(unsigned variant=0;variant<6;++variant){vehicles.reset();vehicles.input=construction;body_occlusion_fixture::variant(vehicles.input,variant);
        for(auto &c:cells)c.flags=NCMM_GFX_VISIBLE;
        if(variant==2)for(const auto &p:vehicles.input.parts)if(p.mount_x==-5&&p.cell_index>=0)cells[size_t(p.cell_index)].flags=0;
        for(unsigned vi=0;vi<views.size();++vi){const auto &camera=views[vi];source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
            const auto build=[&](bool enabled){fp::gpu::packet packet;fp::hybrid::render_options options;options.cull_gpu_vehicle_bodies=enabled;
                require(fp::hybrid::valid_scene(source,camera.yaw,camera.pitch),"all exact view and source fields satisfy the existing scene contract");
                require(fp::hybrid::valid_presentation(metadata,1600),"the complete presentation retains all original optional contracts");
                statistics={};
                const bool ok=cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,metrics,{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE,nullptr,options);
                if(!ok)std::fprintf(stderr,"body packet variant=%u view=%u policy=%u reason=%s\n",variant,vi,unsigned(enabled),cache.last_reason.c_str());
                require(ok,"the actual producer builds both complete and whole-pass-only packets");
                const auto &scene=packet.finish(nullptr);snapshot result;result.parts=statistics.vehicle_parts;result.draws.assign(scene.draws,scene.draws+scene.draw_count);
                for(uint32_t mi=0;mi<scene.mesh_count;++mi){const auto &m=scene.meshes[mi];mesh_data copy;copy.reserved=m.reserved;
                    copy.vertices.assign(m.vertices,m.vertices+m.vertex_count);copy.materials.assign(m.materials,m.materials+m.material_count);copy.indices.assign(m.indices,m.indices+m.index_count);
                    require(result.meshes.emplace(m.key,std::move(copy)).second,"original content keys still uniquely identify mesh resources");}
                for(const auto &d:result.draws){const auto found=result.meshes.find(d.mesh_key);require(found!=result.meshes.end(),"every submitted pass owns its original mesh");result.triangles+=found->second.indices.size()/3;}
                return result;};
            snapshot before,after;if((variant+vi)%2){after=build(true);before=build(false);}else{before=build(false);after=build(true);}
            require(after.parts==before.parts,"actual Native permitted part counts stay intact even when an opaque pass is hidden");
            require(after.triangles<=before.triangles&&after.draws.size()<=before.draws.size(),"whole-pass omission cannot add geometry or draws");
            for(const auto &[key,m]:after.meshes){const auto found=before.meshes.find(key);require(found!=before.meshes.end(),"every surviving public resource keeps its exact original content key");const auto &old=found->second;
                require(m.reserved==old.reserved&&same(m.vertices,old.vertices)&&same(m.materials,old.materials)&&m.indices==old.indices,"all surviving original vertex material index bytes and resource metadata remain exact");}
            size_t cursor=0;for(const auto &draw:after.draws){while(cursor<before.draws.size()&&std::memcmp(&draw,&before.draws[cursor],sizeof(draw))!=0)++cursor;
                require(cursor<before.draws.size(),"all surviving public draws are an exact original ordered subsequence, including cells, passes and rigid transforms");++cursor;++surviving;}
            std::vector<ncmm_gpu_draw_v1> old_alpha,new_alpha;
            for(const auto &d:before.draws)if(d.pass==NCMM_GPU_TRANSPARENT)old_alpha.push_back(d);
            for(const auto &d:after.draws)if(d.pass==NCMM_GPU_TRANSPARENT)new_alpha.push_back(d);
            require(same(old_alpha,new_alpha),"every original transparent draw remains byte exact and in the original order");alpha_draws+=new_alpha.size();
            const auto delta=before.triangles-after.triangles;saved+=delta;++paired;if(delta)++active;else ++fallback;
            if(variant==0&&vi==0){
                std::vector<const v::submission *> available;for(const auto &d:vehicles.draws)available.push_back(&d);
                const v::point direction{std::cos(camera.yaw)*std::cos(camera.pitch),std::sin(camera.yaw)*std::cos(camera.pitch),std::sin(camera.pitch)};
                const auto plan=v::obscured_vehicle_materials(available,camera.eye,direction,1280/(2*std::tan(70*v::pi/360)));
                require(plan.complete&&plan.opaque_passes.size()==vehicles.draws.size(),"the actual stock construction passes all bounded planner controls");
                for(size_t i=0;i<vehicles.draws.size();++i)if(vehicles.draws[i].geometry->kind==v::component::wheel)require(!plan.opaque_passes[i],"every stock98material wheel stays on the full original path");
                std::fprintf(stderr,"body side diagnostic parts=%zu actual=%llu draws=%zu/%zu tris=%llu/%llu quads=%zu polys=%zu ranges=%zu whole=%zu valid=%u\n",vehicles.draws.size(),(unsigned long long)after.parts,before.draws.size(),after.draws.size(),(unsigned long long)before.triangles,(unsigned long long)after.triangles,plan.quads,plan.polygons,plan.hidden_ranges,plan.hidden_passes,unsigned(plan.complete));
                require(delta>0,"the actual side-facing bus configuration exercises real whole-pass work removal");}
        }
    }
    require(paired==60&&active>0&&fallback>0&&saved>0&&alpha_draws>0,"all60paired configurations exercise actual culling, conservative fallback and unchanged alpha work");
    std::printf("{\"success\":true,\"checks\":%llu,\"paired_configurations\":%u,\"active\":%u,\"fallback\":%u,\"saved_submitted_triangles\":%llu,\"surviving_exact_draws\":%llu,\"unchanged_transparent_draws\":%llu,\"no_local_raster_Native_device_game_input_or_focus_execution\":true}\n",(unsigned long long)checks,paired,active,fallback,(unsigned long long)saved,(unsigned long long)surviving,(unsigned long long)alpha_draws);
}
