#include "hybrid_renderer.hpp"
#include "ncmm_graphics_camera.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace animation = first_person::animation;
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
    for(int n=0;n<8;++n){const float angle=n*pi/4;
        auto forward=ncmm_graphics_camera::movement(angle,1,0),back=ncmm_graphics_camera::movement(angle,-1,0);
        auto left=ncmm_graphics_camera::movement(angle,0,-1),right=ncmm_graphics_camera::movement(angle,0,1);
        check(forward.first==-back.first&&forward.second==-back.second,"relative forward/back match in every grid sector");
        check(left.first==-right.first&&left.second==-right.second,"relative strafes match in every grid sector");
        check(forward.first*right.first+forward.second*right.second==0,"forward and strafe stay perpendicular");}
    check(ncmm_graphics_camera::movement(-pi*.5f,1,0)==std::make_pair(0,-1),"north-facing up arrow moves north");
    check(ncmm_graphics_camera::movement(0,1,0)==std::make_pair(1,0),"east-facing up arrow moves east");
    check(ncmm_graphics_camera::in_front(0,8,0)&&!ncmm_graphics_camera::in_front(0,-8,0)&&!ncmm_graphics_camera::in_front(0,0,8),"forward scene excludes distant rear and side tiles");
    ncmm_graphics_shapes::traits opaque_rock;opaque_rock.impassable=true;opaque_rock.transparent=false;
    check(ncmm_graphics_shapes::furniture("f_boulder_large",opaque_rock)==NCMM_SHAPE_ROCK,"large opaque boulders do not become wall cubes");
    check(ncmm_graphics_shapes::terrain("t_rock_wall",opaque_rock)==NCMM_SHAPE_WALL,"underground stone walls keep their wall geometry");
    ncmm_graphics_shapes::traits mesh_gate;mesh_gate.door=true;mesh_gate.impassable=true;
    for(const char *id:{"f_indoor_plant","f_indoor_plant_y","f_indoor_plant f_indoor_plant_y"})
        check(ncmm_graphics_shapes::furniture(id,{})==NCMM_SHAPE_PLANT,"Native indoor plant must not become a door");
    check(ncmm_graphics_shapes::furniture("f_sliding_door_c",mesh_gate)==NCMM_SHAPE_DOOR_CLOSED,"real furniture doors retain their geometry");
    check(ncmm_graphics_shapes::terrain("t_chaingate_c",mesh_gate)==NCMM_SHAPE_FENCE,"closed chain gate retains mesh despite its DOOR flag");
    ncmm_graphics_shapes::traits radio;radio.transparent=true;radio.impassable=true;
    check(ncmm_graphics_shapes::terrain("t_radio_tower",radio)==NCMM_SHAPE_LATTICE_TOWER,"native radio tower uses steel lattice");
    check(ncmm_graphics_shapes::terrain("t_radiosphere_radio_tower_liminal_light t_radio_tower",radio)==NCMM_SHAPE_LATTICE_TOWER,"mod tower aliases retain physical lattice");
    check(ncmm_graphics_shapes::terrain("t_treetop_evergreen",{})==NCMM_SHAPE_TREE_CANOPY,"treetop remains a crown without a second trunk");
    check(!ncmm_graphics_shapes::partial_support(NCMM_SHAPE_LATTICE_TOWER,0)&&ncmm_graphics_shapes::partial_support(NCMM_SHAPE_LATTICE_TOWER,1),"tower foundation keeps the native ground floor while its upper lattice stays open");
    check(ncmm_graphics_shapes::partial_support(NCMM_SHAPE_TREE_CANOPY,0),"foliage never becomes a concrete foundation");
    ncmm_graphics_shapes::traits flagged_window;flagged_window.window=true;flagged_window.door=true;
    check(ncmm_graphics_shapes::terrain("t_window_custom",flagged_window)==NCMM_SHAPE_WINDOW,"window semantic wins over a generic inherited door flag");
    ncmm_graphics_control_state_v1 attack{};attack.struct_size=sizeof(attack);attack.attack_serial=1;attack.attack_started=10;attack.attack_kind=1;attack.grip_kind=NCMM_GRIP_POLE;attack.held_length=2;
    check(animation::sample(attack,10).thrust==0&&animation::sample(attack,10.12).thrust>.20f,"pole timeline winds into a thrust");
    check(animation::sample(attack,10.50).thrust==0&&animation::sample(attack,9).thrust==0,"timeline recovers and rejects a future event");
    fixture stones;for(auto &d:stones.details)d.terrain_shape=NCMM_SHAPE_FLAT;
    stones.shape(7,7,NCMM_SHAPE_ROCK);frame original_rock,reindexed_rock;
    check(build_frame(stones.scene,-pi*.5f,0,stones.provider(),original_rock),"original camera-centred rock snapshot");
    stones.shape(7,7,NCMM_SHAPE_FLAT);stones.shape(8,7,NCMM_SHAPE_ROCK);stones.scene.base.camera_x+=1;
    check(build_frame(stones.scene,-pi*.5f,0,stones.provider(),reindexed_rock),"reindexed camera-centred rock snapshot");
    size_t rock_pixels=0,stable_rock_pixels=0;
    for(size_t n=0;n<original_rock.depth.size();++n)if(original_rock.owners[n]==7*15+7){++rock_pixels;
        stable_rock_pixels+=reindexed_rock.owners[n]==7*15+8&&std::abs(original_rock.depth[n]-reindexed_rock.depth[n])<.001f;}
    check(rock_pixels>100&&stable_rock_pixels*100>=rock_pixels*99,"boulder geometry stays stable when the local grid shifts around the camera");
    check(ncmm_graphics_shapes::physical_item_id("naginata_plus_two","naginata",2)=="naginata","enchanted melee uses its declared physical silhouette instead of grade icons");
    check(ncmm_graphics_shapes::physical_item_id("rifle_plus_one","rifle",1)=="rifle","enchanted gun resolves physical art generically");
    check(ncmm_graphics_shapes::physical_item_id("knife_combat","knife",2)=="knife_combat","normal weapon keeps its own authored sprite");
    check(ncmm_graphics_shapes::physical_item_id("naginata_plus_two","null",2)=="naginata_plus_two","missing physical alias retains the installed sprite fallback");
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
    check(f.focus==7*15+7,"centre interaction focus stops at transparent window glass");
    world.shape(7,7,NCMM_SHAPE_WINDOW_COVERED);draw();check(f.owners[at+25]==7*15+7,"covered window remains opaque and vertical");
    world.shape(7,7,NCMM_SHAPE_DOOR_FRAME);draw();check(f.owners[at]!=7*15+7,"destroyed door retains an open frame");
    world.shape(7,7,NCMM_SHAPE_WINDOW_BROKEN);draw();check(f.vertical_windows>0,"broken window remains vertical");
    world.shape(7,7,NCMM_SHAPE_FENCE);draw();check(f.vertical_fences>0,"wire fence vertical geometry");world.shape(7,7,NCMM_SHAPE_GATE_OPEN);draw();check(f.vertical_fences>0,"open gate stays vertical");
    world.shape(7,7,NCMM_SHAPE_PLANT);draw();check(f.plants>0,"plant has upright geometry");check(std::isfinite(f.depth[450*800+400]),"plant retains ground substrate");
    const auto rooted_plant=digest(f);const auto original_leaf=world.leaf;
    std::fill(world.leaf.begin(),world.leaf.end(),0u);world.leaf[16*32+16]=0x456b2cff;draw();
    check(digest(f)==rooted_plant,"transparent source padding cannot move or resize the rooted 3D plant");world.leaf=original_leaf;
    world.shape(7,7,NCMM_SHAPE_FLAT);for(int x=5;x<=9;++x){int i=7*15+x;world.cells[i].flags|=NCMM_GFX_VEHICLE;std::strcpy(world.cells[i].furniture,"vp_roof");auto &d=world.details[i];
        d.vehicle_group=1;d.vehicle_features=NCMM_VEH_FRAME|NCMM_VEH_ROOF|NCMM_VEH_BOARD|NCMM_VEH_WINDOW|NCMM_VEH_SEAT;d.mount_x=x-7;d.mount_y=0;d.vehicle_x=7.5f;d.vehicle_y=7.5f;d.vehicle_health=1;
        if(x==5||x==9)d.vehicle_features|=NCMM_VEH_WHEEL;}
    draw();check(f.vehicle_parts==5&&f.triangles>150,"car builds body roof glass seats and wheels");check(digest(f)!=empty,"modeled car appears");if(argc==2)save(f,std::string(argv[1])+"-car.ppm");
    for(int x=5;x<=9;++x){int i=8*15+x;auto &c=world.cells[i];auto &d=world.details[i];
        c.flags|=NCMM_GFX_VEHICLE;std::strcpy(c.furniture,"vp_door");d.vehicle_group=1;d.mount_x=x-7;d.mount_y=1;
        d.vehicle_features=NCMM_VEH_FRAME|NCMM_VEH_DOOR|NCMM_VEH_WINDOW;d.vehicle_x=7.5f;d.vehicle_y=7.5f;d.vehicle_health=1;}
    draw();size_t joined_roof=0;
    for(int x=300;x<500;++x)joined_roof+=f.owners[246*800+x]>=8*15+5&&f.owners[246*800+x]<=8*15+9;
    check(joined_roof>100,"visible central roof spans cover side-door mounts");
    for(int x=5;x<=9;++x){int i=8*15+x;world.details[i].vehicle_features=0;world.cells[i].flags&=~NCMM_GFX_VEHICLE;world.cells[i].furniture[0]=0;}
    world.scene.environment.sun_y=.60f;draw();const float facing_sun=unpack(f.pixels[380*800+400]).x;
    world.scene.environment.sun_y=-.60f;draw();const float facing_away=unpack(f.pixels[380*800+400]).x;
    check(facing_sun>facing_away+.02f,"vehicle panel normals receive sunlight from the outward side");
    world.scene.environment.sun_y=.60f;
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
        if(image.owners[i]==-1&&image.held_depth[i]<1)sum=sum+unpack(image.pixels[i]);}return sum;};
    const vec neutral_held=held_color(f);auto &camera_light=world.details[11*15+7];
    camera_light.light_r=.05f;camera_light.light_g=.1f;camera_light.light_b=.9f;draw();
    const vec blue_held=held_color(f);
    check(blue_held.z/(blue_held.x+.001f)>neutral_held.z/(neutral_held.x+.001f)*2,"camera RGB light shades the held item and hands");
    camera_light.light_r=camera_light.light_g=camera_light.light_b=.8f;
    int held_top=f.height;
    for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x)if(std::isfinite(f.held_depth[size_t(y)*f.width+x])){held_top=std::min(held_top,y);}
    size_t center_held=0;
    for(int y=f.height/2-12;y<=f.height/2+12;++y)for(int x=f.width/2-12;x<=f.width/2+12;++x)center_held+=std::isfinite(f.held_depth[size_t(y)*f.width+x]);
    check(center_held==0&&held_top>f.height/3,"rotated view model leaves the reticle and upper scene clear");
    check(f.depth[at]>1,"view model does not overwrite world occlusion depth");
    frame cached,rest,strike,recovered,idle_reference;
    const auto idle=animation::sample(attack,10);
    check(build_frame(world.scene,-pi*.5f,0,world.provider(),rest,800,500,idle,nullptr,&cached),"pole resting pose builds an owned world frame");
    check(build_frame(world.scene,-pi*.5f,0,world.provider(),strike,800,500,animation::sample(attack,10.12),&cached),"attack composes over owned world pixels");
    check(digest(rest)!=digest(strike)&&rest.depth==strike.depth,"attack changes hands while preserving world depth");
    check(build_frame(world.scene,-pi*.5f,0,world.provider(),recovered,800,500,animation::sample(attack,10.6),&cached),"recovery composition succeeds");
    auto no_attack=attack;no_attack.attack_serial=0;
    check(build_frame(world.scene,-pi*.5f,0,world.provider(),idle_reference,800,500,animation::sample(no_attack,10.6),&cached),"same-time breathing reference builds");
    check(digest(idle_reference)==digest(recovered),"attack returns exactly to the same-time idle pose without accumulating transforms");
    if(argc==2){save(rest,std::string(argv[1])+"-pole.ppm");save(strike,std::string(argv[1])+"-attack.ppm");}
    world.scene.environment.held_item[0]=0;draw();check(f.held_pixels==0,"empty hands do not retain previous weapon");
    check(f.viewmodel_draws==2&&digest(f)!=digest(rest)&&std::any_of(f.held_depth.begin(),f.held_depth.end(),[](float z){return std::isfinite(z);}),"unwielding replaces the weapon with two relaxed PSX hands");
    const int animal=9*15+7;std::strcpy(world.cells[animal].creature,"mon_aphid");
    world.details[animal].terrain_flags=NCMM_CREATURE_TINY;draw();size_t tiny_pixels=0;
    for(int y=0;y<400;++y)for(int x=300;x<500;++x)tiny_pixels+=f.owners[size_t(y)*800+x]==animal;
    world.details[animal].terrain_flags=0;draw();size_t adult_pixels=0;
    for(int y=0;y<400;++y)for(int x=300;x<500;++x)adult_pixels+=f.owners[size_t(y)*800+x]==animal;
    check(adult_pixels>tiny_pixels*2,"tiny insects do not become human-height central billboards");
    world.cells[animal].creature[0]=0;
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
