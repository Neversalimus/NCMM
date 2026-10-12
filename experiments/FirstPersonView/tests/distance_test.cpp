#include "hybrid_renderer.hpp"
#include "renderer.hpp"
#include "ncmm_graphics_policy.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace {
int checks=0;
void require(bool ok,const char *name){++checks;if(!ok)throw std::runtime_error(name);}
struct fixture {
    static constexpr int radius=NCMM_GRAPHICS_V2_RADIUS,side=NCMM_GRAPHICS_V2_MAX_GRID_SIDE;
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(side*side);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(side*side);
    std::array<uint32_t,64> floor{},wall{},creature{};
    ncmm_graphics_scene_v2 scene{};frame f;size_t hidden_requests=0;
    fixture(){
        floor.fill(0x789064ff);wall.fill(0x9e927aff);creature.fill(0xc83f32ff);
        for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;
            std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");
            details[i].light_r=details[i].light_g=details[i].light_b=.9f;}
        scene={sizeof(scene),{sizeof(ncmm_graphics_scene_v1),640,480,side,side,radius+.5f,radius+.5f,cells.data(),cells.size()},details.data(),details.size(),{}};
        scene.environment.daylight=1;scene.environment.sun_z=1;scene.environment.animation_seconds=10;
    }
    int ahead(int distance,int offset=0)const{return(radius-distance)*side+radius+offset;}
    sprite_provider provider(){return [&](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &out){
        if(i==UINT32_MAX)return false;
        if(i>=cells.size()||!(cells[i].flags&NCMM_GFX_VISIBLE)){++hidden_requests;return false;}
        const auto &pixels=layer==NCMM_GFX_CREATURE?creature:layer==NCMM_GFX_TERRAIN?wall:floor;
        out={sizeof(out),8,8,8,pixels.data()};return true;
    };}
    void draw(float yaw=-pi*.5f){require(build_frame(scene,yaw,0,provider(),f,640,480),"extended scene renders");}
};
void save(const frame &f,const std::string &path){std::ofstream out(path,std::ios::binary);out<<"P6\n"<<f.width<<' '<<f.height<<"\n255\n";
    for(uint32_t p:f.pixels){const char rgb[]={char(p>>24),char(p>>16),char(p>>8)};out.write(rgb,3);}}
}
int main(int argc,char **argv){try{
    using ncmm_graphics_policy::weather_fog_density;
    require(weather_fog_density(1,0,false)==0,"normal CDDA sight multiplier does not invent fog");
    require(weather_fog_density(.5f,0,false)==0,"better visibility does not produce negative fog");
    require(weather_fog_density(1.2f,.45f,false)>.005f,"rain and reduced visibility still contribute haze");
    require(weather_fog_density(1,0,true)>=.055f,"actual fog or mist retains its minimum density");
    require(weather_fog_density(20,1,false)==.2f,"extreme weather fog remains bounded");
    fixture a;const int far=a.ahead(40),barrier=a.ahead(24);
    a.scene.environment.fog_density=weather_fog_density(1,0,false);
    std::strcpy(a.cells[far].creature,"mon_zombie");
    a.details[a.ahead(36,-14)].terrain_shape=NCMM_SHAPE_TREE;
    a.details[a.ahead(36,14)].terrain_shape=NCMM_SHAPE_TREE;
    a.draw();
    require(a.f.aimed==far,"creature forty cells ahead can be picked by real depth and alpha");
    size_t distant_ground=0;
    for(size_t i=0;i<a.f.owners.size();++i){const int owner=a.f.owners[i];
        if(owner>=0&&fixture::radius-owner/fixture::side>NCMM_GRAPHICS_RADIUS&&std::isfinite(a.f.depth[i]))++distant_ground;}
    require(distant_ground>300,"rendered distant world extends beyond the former sixteen-cell edge");
    if(argc>1)save(a.f,std::string(argv[1])+"-distance.ppm");
    a.details[barrier].terrain_shape=NCMM_SHAPE_WALL;a.cells[barrier].flags|=NCMM_GFX_WALL;a.draw();
    require(a.f.aimed<0,"distant creature cannot be picked through an intervening wall");
    require(a.f.owners[size_t(a.f.height/2)*a.f.width+a.f.width/2]==barrier,"wall beyond the old draw distance owns the reticle pixel");
    if(argc>1)save(a.f,std::string(argv[1])+"-occluded.ppm");
    a.details[barrier].terrain_shape=NCMM_SHAPE_FLAT;a.cells[barrier].flags=NCMM_GFX_VISIBLE;
    a.cells[far].flags=0;a.draw();
    require(a.f.aimed<0&&a.hidden_requests==0,"unavailable distant cells never request sprites or expose creatures");
    for(int n=0;n<8;++n){a.draw(n*pi/4);require(a.f.pixels.size()==640u*480u,"extended grid supports every camera direction");}
    fixture gas;presentation_data meta;meta.cells.resize(gas.cells.size());
    meta.view.struct_size=sizeof(meta.view);meta.view.flags=NCMM_PRESENT_GPU_EFFECTS;
    meta.view.eye_height=1.55f;meta.view.body_scale=meta.view.held_length=1;
    meta.cells[gas.ahead(40)].gas=NCMM_GAS_SMOKE;meta.cells[gas.ahead(40)].field_intensity=3;
    require(build_frame(gas.scene,-pi*.5f,0,gas.provider(),gas.f,640,480,{},nullptr,nullptr,&meta),"distant gas scene renders");
    require(!gas.f.effects.empty()&&(gas.f.effects[size_t(gas.f.effect_height/2)*gas.f.effect_width+gas.f.effect_width/2]&255)>5,
            "smoke forty cells ahead survives atmosphere integration");
    auto invalid=a.scene;invalid.base.grid_width=NCMM_GRAPHICS_V2_MAX_GRID_SIDE+1;
    require(!build_frame(invalid,0,0,a.provider(),a.f),"oversized V2 grid is rejected before reading cells");
    invalid=a.scene;--invalid.detail_count;require(!build_frame(invalid,0,0,a.provider(),a.f),"extended grid rejects mismatched detail storage");
    std::vector<ncmm_graphics_quad_v1> commands;
    require(!first_person::build_frame(a.scene.base,0,commands),"V1 renderer retains its original grid limit");
    std::cout<<"First person distance tests: "<<checks<<" checks passed\n";
    return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
