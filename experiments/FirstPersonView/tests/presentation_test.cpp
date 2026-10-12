#include "hybrid_renderer.hpp"
#include <iostream>
#include <fstream>
#include <set>
#include <stdexcept>
using namespace first_person::hybrid;
namespace policy=ncmm_graphics_presentation_policy;
int checks=0;
void require(bool v,const char *message){++checks;if(!v)throw std::runtime_error(message);}
uint64_t digest(const frame &f){uint64_t h=0;for(uint32_t p:f.pixels)h=(h^p)*1099511628211ull;return h;}
void save(const frame &f,const std::string &name){std::ofstream out(name,std::ios::binary);out<<"P6\n"<<f.width<<' '<<f.height<<"\n255\n";for(auto p:f.pixels){char b[]={char(p>>24),char(p>>16),char(p>>8)};out.write(b,3);}}
struct fixture{
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(81);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(81);
    presentation_data meta;ncmm_graphics_scene_v2 scene{};frame f;
    std::vector<uint32_t> art=std::vector<uint32_t>(1024,0x727a68ff),sprite=std::vector<uint32_t>(1024,0);
    fixture(){meta.view.struct_size=sizeof(meta.view);meta.view.eye_height=1.55f;meta.view.body_scale=1;meta.view.held_length=1;meta.cells.resize(81);
        for(int i=0;i<81;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.9f;}
        for(int y=4;y<28;++y)for(int x=10;x<22;++x)sprite[y*32+x]=0x778852ff;
        scene={sizeof(scene),{sizeof(ncmm_graphics_scene_v1),640,480,9,9,4.5f,7.5f,cells.data(),cells.size()},details.data(),details.size(),{}};
        scene.environment.daylight=1;scene.environment.sun_z=.7f;scene.environment.sun_x=.7f;scene.environment.cloud_cover=.45f;scene.environment.animation_seconds=10;
    }
    sprite_provider provider(){return [&](uint32_t,uint32_t layer,ncmm_graphics_sprite_v2 &out){out={sizeof(out),32,32,32,layer==NCMM_GFX_CREATURE?sprite.data():art.data()};return true;};}
    void draw(float pitch=0){require(build_frame(scene,-pi*.5f,pitch,provider(),f,640,480,{},nullptr,nullptr,&meta),"presentation scene renders");}
};
int main(int argc,char **argv){try{
    ncmm_graphics_shapes::traits transparent;transparent.transparent=true;transparent.impassable=true;
    require(ncmm_graphics_shapes::terrain("t_glass_reinforced",transparent)==NCMM_SHAPE_GLASS_WALL,"ballistic glass is transparent full-height glass");
    transparent.bridge=true;require(ncmm_graphics_shapes::furniture("f_mod_walkway",transparent)==NCMM_SHAPE_BRIDGE,"BRIDGE semantics survive custom identifiers");transparent.bridge=false;
    require(ncmm_graphics_shapes::furniture("f_scrap_bridge",{})==NCMM_SHAPE_BRIDGE,"scrap bridge remains horizontal");
    require(ncmm_graphics_shapes::terrain("t_resin_hole_o",{})==NCMM_SHAPE_RESIN_PORTAL,"open resin hatch has its own model");
    require(ncmm_graphics_shapes::terrain("t_guardrail_bg_dp",{})==NCMM_SHAPE_GUARDRAIL,"guardrail has height");
    require(ncmm_graphics_shapes::terrain("t_scrap_wall_halfway",{})==NCMM_SHAPE_SCRAP_FENCE,"partial scrap barricade is a thin fence");
    for(const std::string body:{"human","dog","horse","cow","spider","fish","snake","bird","blob","migo","apeirohedra"})for(int c=0;c<5;++c)for(float volume:{.001f,1.0f,15.0f,70.0f,1000.0f,100000.0f}){
        const auto v=policy::creature(volume,c,body);require(std::isfinite(v.height)&&v.height>0&&v.height<=7&&v.width>0&&v.width<=9,"all body/category/volume combinations bounded");}
    auto human=policy::creature(70,2,"human"),spider=policy::creature(70,2,"spider");
    require(std::abs(human.height-1.75f)<.001f&&spider.height<human.height&&spider.width>human.width,"human scale and low wide body plans differ");
    fixture aim;const int target=5*9+4;std::strcpy(aim.cells[target].creature,"mon_zombie");aim.meta.cells[target].creature_height=1.75f;aim.meta.cells[target].creature_width=.7f;
    aim.draw();require(aim.f.aimed==target&&aim.f.label_y<.5f,"aim picks actual opaque creature pixels and positions label above head");
    aim.details[6*9+4].terrain_shape=NCMM_SHAPE_GLASS_WALL;aim.draw();require(aim.f.aimed==target,"transparent glass keeps aimed creature visible");
    aim.details[6*9+4].terrain_shape=NCMM_SHAPE_WALL;aim.draw();require(aim.f.aimed==-1,"wall blocks name and creature picking");
    std::set<uint64_t> doors;fixture room;room.details[target].terrain_shape=NCMM_SHAPE_DOOR_CLOSED;
    for(uint32_t m:{NCMM_MAT_WOOD,NCMM_MAT_METAL,NCMM_MAT_GLASS,NCMM_MAT_STONE,NCMM_MAT_CLOTH}){room.meta.cells[target].terrain_material=m;room.draw();doors.insert(digest(room.f));}
    require(doors.size()==5,"door materials produce distinct geometry and shading");
    fixture interior;interior.meta.view.eye_height=1.18f;interior.meta.view.occupied_vehicle=1;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){const int i=(7+dy)*9+4+dx;
        auto &d=interior.details[i];d.vehicle_group=1;d.mount_x=dx;d.mount_y=dy;d.vehicle_x=4.5f;d.vehicle_y=7.5f;d.vehicle_health=1;
        d.vehicle_features=NCMM_VEH_FRAME|((dx||dy)?NCMM_VEH_DOOR|NCMM_VEH_WINDOW:NCMM_VEH_ROOF|NCMM_VEH_SEAT);
        interior.cells[i].flags|=NCMM_GFX_VEHICLE|NCMM_GFX_CEILING;}
    interior.draw();require(std::isfinite(interior.f.depth[320]),"roof-backed side panels retain headlining over the passenger");
    interior.details[7*9+4].vehicle_features&=~NCMM_VEH_ROOF;
    interior.draw();require(!std::isfinite(interior.f.depth[320]),"an actually roofless vehicle retains open sky");
    std::set<uint64_t> skies;fixture sky;
    for(uint32_t style=0;style<=NCMM_SKY_FUNGAL;++style){sky.meta.view.sky_style=style;sky.draw(.7f);skies.insert(digest(sky.f));}
    require(skies.size()==8,"dimension sky profiles are visually distinct");
    require(policy::sky("inner_cabins")==NCMM_SKY_GREY&&policy::sky("string_dimension")==NCMM_SKY_STARS,"dimension descriptions select grey and starry skies");
    require(policy::sky("unknown_custom_dimension")==NCMM_SKY_EARTH,"unknown dimensions retain weather-driven fallback");
    fixture daylight;daylight.draw();
    require(std::all_of(daylight.f.sun_shadows.begin(),daylight.f.sun_shadows.end(),[](float v){return v==1;}),"snapshot edges do not invent shadows over empty ground");
    daylight.cells[target].flags=0;daylight.details[target].terrain_shape=NCMM_SHAPE_WALL;daylight.draw();
    require(std::all_of(daylight.f.sun_shadows.begin(),daylight.f.sun_shadows.end(),[](float v){return v==1;}),"unknown cells do not turn a camera boundary into a shadow caster");
    daylight.cells[target].flags=NCMM_GFX_VISIBLE;daylight.draw();
    require(daylight.f.shadow_cells>0,"visible walls still cast directional sunlight shadows");
    for(int n=0;n<=20;++n){const float u=float(n)/20;
        require(std::abs(first_person::liquid_surface::bank_distance(1,u,1)-first_person::liquid_surface::bank_distance(16,u,0))<.00001f,"shore distance continues across a tile edge at the end of a bank");
        require(std::abs(first_person::liquid_surface::bank_distance(2,u,1)-first_person::liquid_surface::bank_distance(32,u,0))<.00001f,"opposite shoreline corner is continuous");}
    require(first_person::liquid_surface::bank_distance(0,.5f,.5f)>1,"unknown water extent creates no shoreline");
    require(first_person::liquid_surface::spill_coverage(.13f,.2f,.01f)==0&&first_person::liquid_surface::spill_coverage(.13f,0,.01f)==1,"puddle exterior is dry and centre opaque");
    const float edge_alpha=first_person::liquid_surface::spill_coverage(.13f,.128f,.01f);
    require(edge_alpha>0&&edge_alpha<1,"puddle edge has subpixel coverage instead of a jagged cutout");
    fixture bank;const int bank_water=5*9+4;
    for(int i=0;i<81;++i){bank.details[i].terrain_shape=NCMM_SHAPE_WATER;bank.meta.cells[i].liquid=NCMM_LIQ_FRESH;}
    bank.cells[6*9+4].flags|=NCMM_GFX_NO_FLOOR;bank.details[6*9+4].terrain_shape=NCMM_SHAPE_FLAT;bank.meta.cells[6*9+4].liquid=0;
    bank.draw(-.5f);const auto no_bank=bank.f;
    bank.cells[6*9+4].flags&=~NCMM_GFX_NO_FLOOR;bank.draw(-.5f);
    size_t shoreline_pixels=0;
    for(size_t i=0;i<bank.f.pixels.size();++i)if(bank.f.owners[i]==bank_water&&no_bank.owners[i]==bank_water&&bank.f.pixels[i]!=no_bank.pixels[i])++shoreline_pixels;
    require(shoreline_pixels>50,"an actual adjacent bank shades the water itself, not only the land tile");
    fixture fluid;fluid.details[target].terrain_shape=NCMM_SHAPE_WATER;fluid.meta.cells[target].liquid=NCMM_LIQ_FRESH;fluid.draw(-.25f);const auto fresh=digest(fluid.f);
    fluid.meta.cells[target].liquid=NCMM_LIQ_BLOOD;fluid.draw(-.25f);require(digest(fluid.f)!=fresh,"otherworld fluid differs from fresh water");
    fixture spill;std::strcpy(spill.cells[target].item,"water");spill.draw(-.35f);const auto solid_item=digest(spill.f);
    spill.meta.cells[target].item_liquid=NCMM_LIQ_FRESH;spill.draw(-.35f);
    require(digest(spill.f)!=solid_item,"a liquid item becomes a horizontal spill instead of a loose-item billboard");
    spill.meta.cells[target].item_liquid=0;spill.draw(-.35f);
    require(digest(spill.f)==solid_item,"a solid container or frozen item retains its ordinary silhouette");
    fixture gas;gas.meta.view.flags=NCMM_PRESENT_GPU_EFFECTS;gas.meta.cells[target].gas=NCMM_GAS_SMOKE;gas.meta.cells[target].field_intensity=3;
    gas.draw();require(!gas.f.effects.empty(),"gas produces a depth-clipped volumetric effect layer");const auto accelerated=gas.f;
    auto composed=accelerated.pixels;require(ncmm_graphics_composite::blend(composed,accelerated.width,accelerated.height,accelerated.effects,accelerated.effect_width,accelerated.effect_height),"CPU compositor accepts the GPU layer");
    gas.meta.view.flags=0;gas.draw();require(gas.f.effects.empty()&&gas.f.pixels==composed,"CPU and GPU inputs reconstruct the same image");
    // Odd sizes, one-pixel layers, mixed alpha and threaded full-size output
    // must preserve the Host reference reconstruction byte for byte.
    for(const auto size:std::vector<std::array<uint32_t,4>>{{1,1,1,1},{13,9,1,1},{17,11,3,2},{320,203,81,51},{1280,800,256,160}}){
        const auto w=size[0],h=size[1],sw=size[2],sh=size[3];
        std::vector<uint32_t> layer(size_t(sw)*sh),original(size_t(w)*h);
        for(size_t i=0;i<layer.size();++i)layer[i]=hash(uint32_t(i)+17);
        for(size_t i=0;i<original.size();++i)original[i]=hash(uint32_t(i)+493)|255u;
        auto expected=original;
        require(ncmm_graphics_composite::blend(expected,w,h,layer,sw,sh)&&first_person::effects::blend(original,w,h,layer,sw,sh)&&original==expected,"parallel reconstruction matches Host on irregular alpha layers");
    }
    fixture invalid;invalid.meta.view.eye_height=std::numeric_limits<float>::quiet_NaN();
    require(!build_frame(invalid.scene,0,0,invalid.provider(),invalid.f,640,480,{},nullptr,nullptr,&invalid.meta),"nonfinite presentation rejected before rendering");
    invalid.meta.view.eye_height=1.55f;invalid.meta.cells[0].vehicle_controls_x=std::numeric_limits<float>::infinity();
    require(!build_frame(invalid.scene,0,0,invalid.provider(),invalid.f,640,480,{},nullptr,nullptr,&invalid.meta),"invalid vehicle layout cannot enter worker threads");
    fixture gun;std::strcpy(gun.scene.environment.held_item,"m27");gun.scene.environment.held_kind=1;gun.meta.view.gun_kind=NCMM_GUN_RIFLE;gun.meta.view.gunmods=NCMM_MOD_OPTIC|NCMM_MOD_SUPPRESSOR|NCMM_MOD_FOREGRIP;
    first_person::animation::pose pose;pose.grip=NCMM_GRIP_GUN;pose.length=1;
    require(build_frame(gun.scene,-pi*.5f,0,gun.provider(),gun.f,640,480,pose,nullptr,nullptr,&gun.meta),"firearm with attachments renders");
    size_t held=0;for(float z:gun.f.held_depth)held+=std::isfinite(z);require(held>300&&held<640u*480u/3,"physical firearm stays within a bounded portion of the viewport");
    auto gun_frame=[&](uint32_t mods){gun.meta.view.gunmods=mods;require(build_frame(gun.scene,-pi*.5f,0,gun.provider(),gun.f,640,480,pose,nullptr,nullptr,&gun.meta),"attachment variant renders");return digest(gun.f);};
    const auto bare_gun=gun_frame(0);
    for(uint32_t mod:{NCMM_MOD_OPTIC,NCMM_MOD_SUPPRESSOR,NCMM_MOD_FOREGRIP,NCMM_MOD_BIPOD,NCMM_MOD_LASER,NCMM_MOD_LIGHT,NCMM_MOD_BAYONET,NCMM_MOD_UNDERBARREL,NCMM_MOD_STOCK})
        require(gun_frame(mod)!=bare_gun,("installed firearm accessory changes the visible model: "+std::to_string(mod)).c_str());
    const auto flashlight=gun_frame(NCMM_MOD_LIGHT);
    require(gun_frame(NCMM_MOD_LIGHT|NCMM_MOD_LASER)!=flashlight,"simultaneous light and laser are both represented");
    gun.meta.view.gun_kind=NCMM_GUN_PISTOL;std::strcpy(gun.scene.environment.held_item,"glock_19");const auto pistol=gun_frame(0);
    std::strcpy(gun.scene.environment.held_item,"sw_500");const auto revolver=gun_frame(0);
    require(revolver!=pistol,"canonical revolver ID gets a cylinder despite lacking the word revolver");
    std::strcpy(gun.scene.environment.held_item,"sw_500_plus_2");require(gun_frame(0)==revolver,"enchantment suffix preserves the physical revolver profile");
    if(argc>1){save(gun.f,std::string(argv[1])+"-gun.ppm");save(gas.f,std::string(argv[1])+"-gas.ppm");save(fluid.f,std::string(argv[1])+"-liquid.ppm");}
    std::cout<<"PASS: "<<checks<<" presentation, geometry, scale, aim and atmosphere checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
