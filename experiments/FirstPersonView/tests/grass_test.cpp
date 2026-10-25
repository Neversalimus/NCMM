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
    check(near.blades.size()==16*4*4,"dense lawn only on the visible unoccupied grass tile");
    for(const auto &t:near.blades)for(const auto &v:t.v)check(t.m.cell==grass&&v.p.x>=1&&v.p.x<=2&&v.p.y>=4&&v.p.y<=5&&v.p.z>=.012f,"lawn roots and blades remain inside their Native tile");
    gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;gpu::packet first;
    check(cache.build(scene,-hybrid::pi*.5f,-.4f,{},first,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"initial GPU lawn");
    const auto &one=first.finish(nullptr);check(!ncmm_gpu::validate(one),"GPU lawn packet contract");
    const auto animated=std::find_if(first.draws.begin(),first.draws.end(),[&](const auto &d){return d.cell==unsigned(grass)&&d.bone_count==3;});
    check(animated!=first.draws.end(),"wind uses the existing three-bone GPU path");const auto key=animated->mesh_key;
    scene.environment.animation_seconds=1.5;gpu::packet second;
    check(cache.build(scene,-hybrid::pi*.5f,-.4f,{},second,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"animated GPU lawn");
    const auto &two=second.finish(nullptr);check(two.generated_cells==0,"wind cannot regenerate static world meshes");
    auto next=std::find_if(second.draws.begin(),second.draws.end(),[&](const auto &d){return d.cell==unsigned(grass)&&d.bone_count==3;});
    check(next!=second.draws.end()&&next->mesh_key==key,"animated lawn reuses the exact mesh");
    check(std::memcmp(&first.bones[animated->bone_offset+2],&second.bones[next->bone_offset+2],sizeof(ncmm_gpu_matrix_v1))!=0,"tips move with presentation time");
    check(std::memcmp(&second.bones[next->bone_offset],&second.bones[0],sizeof(ncmm_gpu_matrix_v1))==0,"wind keeps roots fixed");
    hybrid::frame cold,cached,changed;check(hybrid::build_frame(scene,-hybrid::pi*.5f,-.4f,{},cold,480,320,{},nullptr,&cached),"CPU lawn");
    scene.environment.animation_seconds=2.1;check(hybrid::build_frame(scene,-hybrid::pi*.5f,-.4f,{},changed,480,320,{},&cached),"CPU cached lawn");
    check(pixels(cold)!=pixels(changed),"CPU wind animates while reusing the static world");
    for(const auto pair:{std::pair<float,unsigned>{13.5f,4u},std::pair<float,unsigned>{20.5f,1u},std::pair<float,unsigned>{29.5f,0u}}){
        scene.base.camera_y=pair.first;collector far;hybrid::renderer(scene,-hybrid::pi*.5f,-.2f,{},statistics).collect(far,480,320);
        check(far.blades.size()==pair.second*4*4,"distance reduces lawn geometry to the flat native ground");
        gpu::packet distant;check(cache.build(scene,-hybrid::pi*.5f,-.2f,{},distant,statistics,host,{},nullptr,nullptr,480,320,NCMM_GPU_FORCE),"GPU distance transition");
        const auto &packet=distant.finish(nullptr);check(!ncmm_gpu::validate(packet),"LOD packet remains valid");
        if(pair.second==0)for(const auto &d:distant.draws)check(!(d.cell==unsigned(grass)&&d.bone_count==3),"far LOD discards animated draws");
    }
    std::cout<<"Grass: Native tile filtering, fixed roots, CPU/GPU animation, immutable GPU meshes and distance LOD passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
