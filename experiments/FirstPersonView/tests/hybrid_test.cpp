#include "hybrid_renderer.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace {
int checks=0;
void check(bool v,const char *name){++checks;if(!v)throw std::runtime_error(name);}
struct fixture {
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(225);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(225);
    std::vector<uint32_t> floor=std::vector<uint32_t>(1024),leaf=std::vector<uint32_t>(1024),part=std::vector<uint32_t>(1024),weapon=std::vector<uint32_t>(1024),transparent_wall=std::vector<uint32_t>(1024);
    ncmm_graphics_scene_v2 scene{};uint32_t hidden_requests=0;
    fixture(){for(int y=0;y<15;++y)for(int x=0;x<15;++x){int i=y*15+x;cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;
        std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.9f;
        if(x==0||y==0||x==14||y==14){cells[i].flags|=NCMM_GFX_WALL;details[i].terrain_shape=NCMM_SHAPE_WALL;}}
        for(int y=0;y<32;++y)for(int x=0;x<32;++x){floor[y*32+x]=((x/8+y/8)&1)?0x77705cff:0x9e906bff;
            float d=std::hypot(x-15.5f,y-15.5f);leaf[y*32+x]=d<13?0x456b2cff:0;
            part[y*32+x]=0x983526ff;weapon[y*32+x]=(std::abs(x-y)<3&&y<26)?0xb0b3b6ff:0;}
        scene.struct_size=sizeof(scene);scene.base={sizeof(ncmm_graphics_scene_v1),800,500,15,15,7.5f,11.5f,cells.data(),cells.size()};
        scene.details=details.data();scene.detail_count=details.size();scene.environment.sun_x=.35f;scene.environment.sun_y=.60f;scene.environment.sun_z=.72f;
        scene.environment.daylight=1;scene.environment.cloud_cover=.20f;scene.environment.animation_seconds=42;scene.environment.moon_phase=4;scene.environment.moon_z=-1;
    }
    sprite_provider provider(){return[this](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &out){
        if(layer!=NCMM_GFX_HELD_ITEM && (i>=cells.size()||!(cells[i].flags&NCMM_GFX_VISIBLE))){++hidden_requests;return false;}
        const uint32_t *p=layer==NCMM_GFX_TERRAIN&&details[i].terrain_shape==NCMM_SHAPE_WALL?transparent_wall.data():layer==NCMM_GFX_HELD_ITEM?weapon.data():layer==NCMM_GFX_FURNITURE&&(cells[i].flags&NCMM_GFX_VEHICLE)?part.data():layer==NCMM_GFX_TERRAIN&&details[i].terrain_shape>=NCMM_SHAPE_TREE&&details[i].terrain_shape<=NCMM_SHAPE_PLANT?leaf.data():floor.data();
        out={sizeof(out),32,32,32,p};return true;};}
    void shape(int x,int y,uint32_t type){details[y*15+x].terrain_shape=type;}
};
uint64_t digest(const frame &f){uint64_t h=1469598103934665603ull;for(uint32_t p:f.pixels){h^=p;h*=1099511628211ull;}return h;}
void save(const frame &f,const std::string &path){std::ofstream out(path,std::ios::binary);out<<"P6\n"<<f.width<<" "<<f.height<<"\n255\n";
    for(uint32_t p:f.pixels){char c[]={char(p>>24),char(p>>16),char(p>>8)};out.write(c,3);}}
}
int main(int argc,char **argv){try{
    std::array<uint32_t,2> edge_pixels{0xff4000ff,0x00ffffff};edge_pixels[1]&=0xffffff00u;
    image edge;edge.data={sizeof(edge.data),2,1,2,edge_pixels.data()};edge.analyse();
    const auto filtered=edge.sample(.5f,0);check((filtered&255)>100&&(filtered&255)<150,"filtered silhouette retains fractional coverage");
    check((filtered>>24)>245&&((filtered>>8)&255)<5,"transparent texel RGB does not contaminate the filtered edge");
    const auto coarse=edge.sample(.5f,0,true);check((coarse>>24)>245&&((coarse>>8)&255)<5,"mip filtering also preserves edge color");
    std::array<uint32_t,9> odd_pixels{};odd_pixels[8]=0x00ff00ff;
    image odd;odd.data={sizeof(odd.data),3,3,3,odd_pixels.data()};odd.analyse();
    check(odd.sample(1,1,true)==0x00ff00ff,"odd-sized sprite mip preserves the last row and column");
    fixture world;frame f;auto draw=[&](float yaw=-pi*.5f,float pitch=0){check(build_frame(world.scene,yaw,pitch,world.provider(),f),"hybrid frame");
        check(f.pixels.size()==size_t(f.width)*f.height&&f.pixels.size()<=1920u*1080u,"bounded complete raster");check(world.hidden_requests==0,"no hidden texture requests");};
    draw();auto empty=digest(f);check(f.width==800&&f.height==500,"native floor resolution");
    size_t holes=0;for(int y=350;y<499;++y)for(int x=50;x<750;++x)holes+=!std::isfinite(f.depth[size_t(y)*f.width+x]);
    check(holes==0,"continuous perspective floor without scanline gaps");
    world.shape(7,7,NCMM_SHAPE_WALL);draw();
    check(f.owners[250*800+400]==7*15+7,"transparent wall art still occludes the room behind it");
    world.shape(7,7,NCMM_SHAPE_DOOR_CLOSED);draw();auto closed=digest(f);int at=250*800+400;check(f.owners[at]==7*15+7,"closed vertical door blocks center");
    world.shape(7,7,NCMM_SHAPE_DOOR_OPEN);draw();check(digest(f)!=closed&&f.owners[at]!=7*15+7,"open hinged door reveals space");check(f.vertical_doors>0,"open door retains frame and panel");
    world.shape(7,7,NCMM_SHAPE_WINDOW);draw();check(f.vertical_windows>0&&f.owners[at+25]!=7*15+7,"window glass preserves sight through aperture");
    world.shape(7,7,NCMM_SHAPE_WINDOW_COVERED);draw();check(f.owners[at+25]==7*15+7,"covered window remains opaque and vertical");
    world.shape(7,7,NCMM_SHAPE_DOOR_FRAME);draw();check(f.owners[at]!=7*15+7,"destroyed door retains an open frame");
    world.shape(7,7,NCMM_SHAPE_WINDOW_BROKEN);draw();check(f.vertical_windows>0,"broken window remains vertical");
    world.shape(7,7,NCMM_SHAPE_FENCE);draw();check(f.vertical_fences>0,"wire fence vertical geometry");world.shape(7,7,NCMM_SHAPE_GATE_OPEN);draw();check(f.vertical_fences>0,"open gate stays vertical");
    world.shape(7,7,NCMM_SHAPE_PLANT);draw();check(f.plants>0,"plant has upright geometry");check(std::isfinite(f.depth[450*800+400]),"plant retains ground substrate");
    int leaf_bottom=-1;for(int y=0;y<500;++y)for(int x=395;x<=405;++x){vec color=unpack(f.pixels[size_t(y)*800+x]);
        if(color.y>color.x*1.3f&&color.y>color.z*1.5f)leaf_bottom=std::max(leaf_bottom,y);}
    const float ground_y=250+800/(2*std::tan(70*pi/360))*1.55f/4;
    check(std::abs(leaf_bottom-ground_y)<3,"transparent sprite padding does not lift a plant above the ground");
    world.shape(7,7,NCMM_SHAPE_FLAT);for(int x=5;x<=9;++x){int i=7*15+x;world.cells[i].flags|=NCMM_GFX_VEHICLE;std::strcpy(world.cells[i].furniture,"vp_roof");auto &d=world.details[i];
        d.vehicle_group=1;d.vehicle_features=NCMM_VEH_FRAME|NCMM_VEH_ROOF|NCMM_VEH_BOARD|NCMM_VEH_WINDOW|NCMM_VEH_SEAT;d.mount_x=x-7;d.mount_y=0;d.vehicle_x=7.5f;d.vehicle_y=7.5f;d.vehicle_health=1;
        if(x==5||x==9)d.vehicle_features|=NCMM_VEH_WHEEL;}
    draw();check(f.vehicle_parts==5&&f.triangles>150,"car builds body roof glass seats and wheels");check(digest(f)!=empty,"modeled car appears");if(argc==2)save(f,std::string(argv[1])+"-car.ppm");
    world.cells[7*15+4].flags=0;draw();auto visible_car=digest(f);
    auto &hidden_part=world.details[7*15+4];hidden_part.vehicle_group=1;hidden_part.mount_x=-3;
    hidden_part.vehicle_features=NCMM_VEH_ROOF|NCMM_VEH_ENGINE|NCMM_VEH_BOARD;hidden_part.vehicle_health=.1f;
    draw();check(digest(f)==visible_car,"hidden neighboring vehicle parts cannot change visible silhouette or paint");
    hidden_part.vehicle_features=0;world.cells[7*15+4].flags=NCMM_GFX_VISIBLE;
    for(int x=5;x<=9;++x)world.details[7*15+x].vehicle_yaw=pi*.25f;
    draw();check(digest(f)!=visible_car,"automatic body details rotate with the vehicle's actual yaw");
    for(int x=5;x<=9;++x)world.details[7*15+x].vehicle_yaw=0;
    for(int x=5;x<=9;++x)world.cells[7*15+x].flags|=NCMM_GFX_CEILING;
    draw();check(!std::isfinite(f.depth[115*800+400]),"vehicle interior does not generate a floating building ceiling");
    for(int x=5;x<=9;++x)world.details[7*15+x].terrain_flags|=NCMM_SURFACE_TERRAIN_CEILING;
    draw();check(std::isfinite(f.depth[115*800+400]),"actual garage ceiling remains above a vehicle");
    for(int x=5;x<=9;++x){world.cells[7*15+x].flags&=~NCMM_GFX_CEILING;world.details[7*15+x].terrain_flags=0;}
    world.details[7*15+5].vehicle_features|=NCMM_VEH_DOOR|NCMM_VEH_OPEN;draw();
    for(auto &c:world.cells)c.flags=0;
    draw();check(world.hidden_requests==0&&f.vehicle_parts==0,"hidden vehicle parts excluded");
    for(auto &c:world.cells)c.flags=NCMM_GFX_VISIBLE;
    for(auto &d:world.details)d.vehicle_features=0;
    world.scene.environment.sun_x=0;world.scene.environment.sun_y=-.98f;world.scene.environment.sun_z=.20f;draw();auto day=digest(f);
    world.scene.environment.daylight=0;world.scene.environment.sun_z=-.8f;world.scene.environment.moon_y=-.9f;world.scene.environment.moon_z=.43f;draw(-pi*.5f,.3f);auto full=digest(f);
    check(full!=day,"calendar sky changes from day to night");world.scene.environment.moon_phase=2;draw(-pi*.5f,.3f);check(full!=digest(f),"moon phase changes rendered disk");
    for(auto &d:world.details){d.light_r=.15f;d.light_g=.55f;d.light_b=.95f;}draw();auto colored=digest(f);for(auto &d:world.details)d.light_r=d.light_g=d.light_b=.8f;draw();check(digest(f)!=colored,"engine colored light changes surfaces");
    world.scene.environment.weather_kind=NCMM_WEATHER_RAIN;world.scene.environment.precipitation=.9f;draw();check(f.weather_particles>0,"rain draws outside world particles");auto rain=digest(f);
    world.scene.environment.weather_kind=NCMM_WEATHER_SNOW;draw();check(digest(f)!=rain&&f.weather_particles>0,"snow differs from rain");
    for(auto &c:world.cells)c.flags|=NCMM_GFX_CEILING;
    draw();check(f.weather_particles==0,"weather stays outside roofs");
    world.scene.environment.precipitation=0;std::strcpy(world.scene.environment.held_item,"naginata");world.scene.environment.held_kind=2;
    draw();check(f.held_pixels>0,"current wielded silhouette appears in hands");if(argc==2)save(f,std::string(argv[1])+"-room.ppm");
    size_t held_samples=0;for(float z:f.held_depth)held_samples+=std::isfinite(z);
    check(held_samples>100,"item and curved hands populate the independent view-model depth buffer");
    world.shape(7,11,NCMM_SHAPE_WALL);draw();size_t wall_held_samples=0;for(float z:f.held_depth)wall_held_samples+=std::isfinite(z);
    check(wall_held_samples==held_samples,"nearby world geometry does not cut into the hands or wielded item");
    world.shape(7,11,NCMM_SHAPE_FLAT);
    auto held_color=[](const frame &image){vec sum{};for(size_t i=0;i<image.pixels.size();++i){
        if(image.owners[i]==-1&&image.depth[i]<1)sum=sum+unpack(image.pixels[i]);}return sum;};
    const vec neutral_held=held_color(f);auto &camera_light=world.details[11*15+7];
    camera_light.light_r=.05f;camera_light.light_g=.1f;camera_light.light_b=.9f;draw();
    const vec blue_held=held_color(f);
    check(blue_held.z/(blue_held.x+.001f)>neutral_held.z/(neutral_held.x+.001f)*2,"camera RGB light shades the held item and hands");
    camera_light.light_r=camera_light.light_g=camera_light.light_b=.8f;
    world.scene.environment.held_item[0]=0;draw();check(f.held_pixels==0,"empty hands do not retain previous weapon");
    check(std::none_of(f.held_depth.begin(),f.held_depth.end(),[](float z){return std::isfinite(z);}),"unwielding clears view-model depth as well as geometry");
    check(ncmm_graphics_shapes::terrain("t_door_o",{})==NCMM_SHAPE_DOOR_OPEN,"open door classification");
    check(ncmm_graphics_shapes::terrain("t_window_frame",{})==NCMM_SHAPE_WINDOW_BROKEN,"broken window classification");
    check(ncmm_graphics_shapes::terrain("t_chainfence",{})==NCMM_SHAPE_FENCE,"chain fence classification");
    check(ncmm_graphics_shapes::terrain("t_chainfence_gate_o",{})==NCMM_SHAPE_GATE_OPEN,"open mesh gate classification");
    check(ncmm_graphics_shapes::terrain("t_door_b",{})==NCMM_SHAPE_DOOR_FRAME,"destroyed door is a frame");
    check(ncmm_graphics_shapes::terrain("t_doormat",{})==NCMM_SHAPE_FLAT,"doormat stays on floor");
    check(ncmm_graphics_shapes::terrain("t_chainfence_posts",{})==NCMM_SHAPE_POST,"destroyed fence retains upright posts");
    check(ncmm_graphics_shapes::furniture("f_null ",{})==NCMM_SHAPE_FLAT,"empty furniture never becomes geometry");
    for(const char *c:{"DEFAULTMODE","INVENTORY","OPEN","CLOSE","EXAMINE","CHOOSE_ADJACENT","ACTIONMENU"})check(ncmm_graphics_shapes::stays_perspective(c),"ordinary command keeps perspective");
    check(!ncmm_graphics_shapes::stays_perspective("TARGETING"),"coordinate targeting uses map context");
    auto bad=world.scene;bad.detail_count=0;check(!build_frame(bad,0,0,world.provider(),f),"truncated scene rejected");bad=world.scene;bad.environment.daylight=std::numeric_limits<float>::quiet_NaN();check(!build_frame(bad,0,0,world.provider(),f),"NaN environment rejected");
    bad=world.scene;std::memset(bad.environment.held_item,'x',sizeof(bad.environment.held_item));
    check(!build_frame(bad,0,0,world.provider(),f),"unterminated held ID is rejected before sprite access");
    std::memset(world.details[0].ground,'x',sizeof(world.details[0].ground));
    check(!build_frame(world.scene,0,0,world.provider(),f),"unterminated ground ID is rejected before material classification");
    std::memset(world.details[0].ground,0,sizeof(world.details[0].ground));std::strcpy(world.details[0].ground,"t_floor");
    check(!build_frame(world.scene,0,2,world.provider(),f),"extreme pitch rejected");
    fixture sky_world;sky_world.scene.environment.cloud_cover=0;sky_world.scene.environment.sun_z=-1;sky_world.scene.environment.moon_phase=0;
    frame horizon,zenith;
    check(build_frame(sky_world.scene,-pi*.5f,0,sky_world.provider(),horizon),"horizon sky fixture");
    check(build_frame(sky_world.scene,-pi*.5f,1.0f,sky_world.provider(),zenith),"zenith sky fixture");
    auto blue_ratio=[](uint32_t p){return float((p>>8)&255)/float((p>>24)+1);};
    check(blue_ratio(zenith.pixels[50*800+400])>blue_ratio(horizon.pixels[50*800+400]),"looking up approaches the deeper blue zenith");
    auto begin=std::chrono::steady_clock::now();for(int angle=0;angle<24;++angle)draw(angle*pi/12,(angle%3-1)*.5f);auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
    check(ms<15000,"hybrid CPU budget for 24 full-resolution frames");
    for(auto dims:{std::pair<int,int>{1,1},{3440,1440},{8192,8192},{320,1000}}){world.scene.base.pixel_width=dims.first;world.scene.base.pixel_height=dims.second;draw();}
    std::cout<<"PASS: "<<checks<<" hybrid checks; 24 native-resolution frames: "<<ms<<" ms\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
