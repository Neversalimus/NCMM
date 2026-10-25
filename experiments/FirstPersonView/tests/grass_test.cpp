#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <iostream>
#include <stdexcept>
using namespace first_person;
namespace {
void check(bool ok,const char *reason){if(!ok)throw std::runtime_error(reason);}
struct collector:hybrid::geometry_sink {
    std::vector<hybrid::triangle> blades;
    bool dirty(int)const override{return true;}
    void emit(const hybrid::triangle &t)override{if(t.m.grass)blades.push_back(t);}
    bool arms(const viewmodel::submission &)override{return true;}
    void skin(const creature::model &,const creature::skeleton_pose &,int,hybrid::vec,int,hybrid::vec,bool,bool,float,hybrid::vec)override{}
    void impostor(const creature::asset &,const creature::pose_state &,hybrid::vec,int,int,bool)override{}
};
uint64_t pixels(const hybrid::frame &f){return hash_bytes(1469598103934665603ull,f.pixels.data(),f.pixels.size()*4);}
}
int main(){try{
    for(const char *id:{"t_grass","t_grass_tall","t_grass_golf","t_grass_white","t_grass_dead"})check(ncmm_grass::terrain(id),"Native grass variant missing");
    for(const char *id:{"t_pavement","t_door_grass_curtain_c","t_moss","f_indoor_plant","t_grassland_wall"})check(!ncmm_grass::terrain(id),"non-grass object grew blades");
    std::vector<ncmm_graphics_cell_v1> cells(3*35);std::vector<ncmm_graphics_cell_v2> details(cells.size());
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_dirt");std::strcpy(details[i].ground,"t_dirt");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    const int grass=4*3+1;std::strcpy(cells[grass].terrain,"t_grass");
    std::strcpy(cells[grass-1].terrain,"t_grass");std::strcpy(cells[grass-1].furniture,"f_indoor_plant");
    std::strcpy(cells[grass+1].terrain,"t_grass");cells[grass+1].flags=0;
    std::strcpy(cells[grass+3].terrain,"t_pavement");std::strcpy(details[grass+3].ground,"t_grass");
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),480,320,3,35,1.5f,7.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=.8f;scene.environment.wind_x=20;scene.environment.animation_seconds=1;
    hybrid::frame statistics;collector near;
    hybrid::renderer(scene,-hybrid::pi*.5f,-.4f,{},statistics).collect(near,480,320);
    check(near.blades.size()==128,"128 broad blades on the visible unoccupied grass tile, half the previous triangle count");
    float blade_area=0;for(const auto &t:near.blades){const auto side=t.v[1].p-t.v[0].p,tip=t.v[2].p-t.v[0].p;
        blade_area+=std::sqrt(hybrid::dot(hybrid::cross(side,tip),hybrid::cross(side,tip)))*.5f;}
    check(blade_area>1.1f,"dense lawn has substantial leaf area rather than thin shoots");
    for(const auto &t:near.blades)for(const auto &v:t.v)check(t.m.cell==grass&&v.p.x>=1&&v.p.x<=2&&v.p.y>=4&&v.p.y<=5&&v.p.z>=.012f,"lawn roots and blades remain inside their Native tile");
    gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;gpu::packet first;
    check(cache.build(scene,-hybrid::pi*.5f,-.4f,{},first,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"initial GPU lawn");
    const auto &one=first.finish(nullptr);check(!ncmm_gpu::validate(one),"GPU lawn packet contract");
    const auto animated=std::find_if(first.draws.begin(),first.draws.end(),[&](const auto &d){return d.cell==unsigned(grass)&&d.bone_count==2;});
    check(animated!=first.draws.end(),"fixed roots and moving tips use only two GPU bones");const auto key=animated->mesh_key;
    auto prototype=std::find_if(one.meshes,one.meshes+one.mesh_count,[&](const auto &m){return m.key==key;});
    check(prototype!=one.meshes+one.mesh_count&&prototype->material_count==3&&prototype->index_count/3==128,"shared grass has three materials and no degenerate tip triangles");
    scene.environment.animation_seconds=1.5;gpu::packet second;
    check(cache.build(scene,-hybrid::pi*.5f,-.4f,{},second,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"animated GPU lawn");
    const auto &two=second.finish(nullptr);check(two.generated_cells==0,"wind cannot regenerate static world meshes");
    auto next=std::find_if(second.draws.begin(),second.draws.end(),[&](const auto &d){return d.cell==unsigned(grass)&&d.bone_count==2;});
    check(next!=second.draws.end()&&next->mesh_key==key,"animated lawn reuses the exact mesh");
    check(std::memcmp(&first.bones[animated->bone_offset+1],&second.bones[next->bone_offset+1],sizeof(ncmm_gpu_matrix_v1))!=0,"tips move with presentation time");
    check(std::memcmp(&second.bones[next->bone_offset],&second.bones[0],sizeof(ncmm_gpu_matrix_v1))==0,"wind keeps roots fixed");
    hybrid::frame cold,cached,changed;check(hybrid::build_frame(scene,-hybrid::pi*.5f,-.4f,{},cold,480,320,{},nullptr,&cached),"CPU lawn");
    scene.environment.animation_seconds=2.1;check(hybrid::build_frame(scene,-hybrid::pi*.5f,-.4f,{},changed,480,320,{},&cached),"CPU cached lawn");
    check(pixels(cold)!=pixels(changed),"CPU wind animates while reusing the static world");
    for(const auto pair:{std::pair<float,unsigned>{13.5f,16u},std::pair<float,unsigned>{20.5f,4u},std::pair<float,unsigned>{29.5f,0u}}){
        scene.base.camera_y=pair.first;collector far;hybrid::renderer(scene,-hybrid::pi*.5f,-.2f,{},statistics).collect(far,480,320);
        check(far.blades.size()==pair.second*2,"distance reduces lawn geometry to the flat native ground");
        gpu::packet distant;check(cache.build(scene,-hybrid::pi*.5f,-.2f,{},distant,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"GPU distance transition");
        const auto &packet=distant.finish(nullptr);check(!ncmm_gpu::validate(packet),"LOD packet remains valid");
        check(packet.generated_cells==0,"crossing a grass LOD boundary never rebuilds the Native floor or neighbours");
        if(pair.second==0)for(const auto &d:distant.draws)check(!(d.cell==unsigned(grass)&&d.bone_count==2),"far LOD discards animated draws");
    }
    // More cells with the same pattern share a model even at different world
    // coordinates/light levels. Per-instance cell IDs and roots stay distinct.
    std::vector<ncmm_graphics_cell_v1> meadow(21*21);std::vector<ncmm_graphics_cell_v2> meadow_details(meadow.size());
    for(size_t i=0;i<meadow.size();++i){meadow[i].flags=NCMM_GFX_VISIBLE;std::strcpy(meadow[i].terrain,"t_grass");meadow_details[i].light_r=meadow_details[i].light_g=meadow_details[i].light_b=.3f+.7f*float(i%3)/2;}
    scene.base.grid_width=scene.base.grid_height=21;scene.base.camera_x=scene.base.camera_y=10.5f;scene.base.cells=meadow.data();scene.base.cell_count=meadow.size();scene.details=meadow_details.data();scene.detail_count=meadow_details.size();
    gpu::packet field;check(cache.build(scene,-hybrid::pi*.5f,-.2f,{},field,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"instanced meadow");
    const auto &field_scene=field.finish(nullptr);check(!ncmm_gpu::validate(field_scene),"instanced meadow packet");
    std::set<uint64_t> lawn_models;size_t lawn_instances=0;for(const auto &d:field.draws)if(d.bone_count==2){++lawn_instances;lawn_models.insert(d.mesh_key);check(d.cell<meadow.size(),"each instance retains its Native cell ID");}
    check(lawn_instances==meadow.size()&&lawn_models.size()<=12,"hundreds of grass tiles use at most twelve shared models, not one model per tile");
    std::cout<<"Grass: Native tile filtering, fixed roots, CPU/GPU animation, immutable GPU meshes and distance LOD passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
