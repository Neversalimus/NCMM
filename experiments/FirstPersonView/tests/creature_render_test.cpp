#include "hybrid_renderer.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace cr=first_person::creature;
namespace {
int checks=0;void check(bool ok,const char *why){++checks;if(!ok)throw std::runtime_error(why);}
struct fixture {
    static constexpr int side=97;
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(side*side);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(side*side);
    std::vector<uint32_t> sprite=std::vector<uint32_t>(16*32,0);
    ncmm_graphics_scene_v2 scene{};cr::presentation entities;frame result,world;
    int index=0,hidden_requests=0;uint64_t next_id=0;
    fixture(){for(size_t n=0;n<cells.size();++n){cells[n].flags=NCMM_GFX_VISIBLE;cells[n].light=1;std::strcpy(cells[n].terrain,"t_floor");std::strcpy(details[n].ground,"t_floor");details[n].light_r=details[n].light_g=details[n].light_b=.95f;}
        scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),320,240,side,side,48.5f,93.5f,cells.data(),cells.size()};scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=1;scene.environment.animation_seconds=10;
        for(int y=1;y<31;++y)for(int x=3;x<13;++x)sprite[y*16+x]=y<8?0xbb8877ff:y<20?0x315e91ff:0x60522aff;
        entities.input.view={sizeof(ncmm_graphics_creature_scene_v1),1,1,1,10};
    }
    sprite_provider provider(){return [&](uint32_t cell,uint32_t layer,ncmm_graphics_sprite_v2 &out){if(layer!=NCMM_GFX_CREATURE)return false;
        if(cell>=cells.size()||!(cells[cell].flags&NCMM_GFX_VISIBLE)){++hidden_requests;return false;}
        out={sizeof(out),16,32,16,sprite.data()};return true;};}
    void creature(const char *body,int distance=3){if(index)cells[index].creature[0]=0;index=(93-distance)*side+48;std::strcpy(cells[index].creature,"mod_arbitrary_creature");
        ncmm_graphics_creature_v1 c{};c.entity_id=++next_id;c.cell=index;c.world_x=48;c.world_y=93-distance;c.liters=70;c.kilograms=65;c.heading=pi*.5f;std::strcpy(c.bodytype,body);std::strcpy(c.type_id,cells[index].creature);entities.input.creatures={c};entities.input.view.count=1;
        entities.observe();const auto key=cr::appearance_key(c,1);entities.resources.request(key,cr::classify(c),[&]{ncmm_graphics_sprite_v2 v{sizeof(v),16,32,16,sprite.data()};return cr::sprite::copy(v);});check(entities.resources.wait_idle(std::chrono::seconds(5)),"bounded creature generation");}
    void draw(const frame *cached=nullptr,frame *output=nullptr){entities.observe();check(build_frame(scene,-pi*.5f,0,provider(),result,320,240,{},cached,output,nullptr,{},&entities),"real renderer creature frame");check(hidden_requests==0,"no sprite request for hidden creature");}
    size_t pixels()const{return size_t(std::count(result.creatures.begin(),result.creatures.end(),index));}
};
void save(const frame &f,const std::string &path){std::ofstream out(path,std::ios::binary);out<<"P6\n"<<f.width<<" "<<f.height<<"\n255\n";for(uint32_t p:f.pixels){char rgb[]={char(p>>24),char(p>>16),char(p>>8)};out.write(rgb,3);}}
}
int main(int argc,char **argv){try{fixture f;
    const char *bodies[]={"human","dog","spider","insect","bird","lizard","snake","fish","blob","migo","unknown_mod_body"};
    for(const char *body:bodies){f.creature(body);for(int view=0;view<8;++view){auto &c=f.entities.input.creatures[0];c.entity_id=++f.next_id;c.heading=view*pi/4;f.draw();check(f.pixels()>8,"visible volume in every anatomy/view");if(argc>1)save(f.result,std::string(argv[1])+"-"+body+"-"+std::to_string(view)+".ppm");}}
    f.creature("human",3);f.draw(nullptr,&f.world);check(f.world.dynamic_world&&bool(f.world.transparency),"cached world retains deferred transparency");check(std::count(f.world.creatures.begin(),f.world.creatures.end(),f.index)==0,"cached world contains no creature silhouette");
    f.entities.input.view.seconds+=.12;f.scene.environment.animation_seconds=f.entities.input.view.seconds;f.draw(&f.world);auto cached=f.result.pixels;f.draw();check(cached==f.result.pixels,"cached composition equals fresh dynamic render");
    const int obstacle=f.index+fixture::side;f.details[obstacle].terrain_shape=NCMM_SHAPE_WINDOW;f.draw(nullptr,&f.world);check(f.pixels()>0,"creature remains pickable through glass");auto glass=f.result.pixels;f.draw(&f.world);check(glass==f.result.pixels,"cached glass composites over current creature depth");
    f.details[obstacle].terrain_shape=NCMM_SHAPE_WALL;f.draw();check(f.pixels()==0,"opaque wall completely occludes creature");f.details[obstacle].terrain_shape=NCMM_SHAPE_FLAT;
    f.cells[f.index].flags=0;f.draw();check(f.pixels()==0,"hidden entity never renders");f.cells[f.index].flags=NCMM_GFX_VISIBLE;
    for(int distance:{2,7,18,40}){f.creature("human",distance);f.draw();check(f.pixels()>0,"all four LOD ranges retain the creature");}
    f.creature("human",3);auto &c=f.entities.input.creatures[0];c.event=NCMM_CR_DIE;c.event_serial=1;c.event_seconds=f.entities.input.view.seconds;c.flags|=NCMM_CR_DEAD;
    f.entities.input.view.seconds+=.25;f.draw();check(f.result.aimed==-1&&f.pixels()==0,"death presentation cannot be attacked");check(f.result.creature_lods[0]+f.result.creature_lods[1]+f.result.creature_lods[2]+f.result.creature_lods[3]==1,"death remains visible in presentation pass");
    f.entities.input.view.count=0;f.entities.input.creatures.clear();f.cells[f.index].creature[0]=0;f.draw();check(f.entities.instances.empty(),"disappeared entities release pose state");
    f.entities.input.view.world_epoch++;f.draw();check(f.entities.resources.statistics().entries==0,"world reset drops generated resources");
    std::cout<<"Creature renderer: "<<checks<<" checks PASS\n";
}catch(const std::exception &e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
