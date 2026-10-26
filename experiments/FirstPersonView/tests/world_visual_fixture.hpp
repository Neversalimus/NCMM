#pragma once
#include "gpu_scene.hpp"
#include "weapon_effects.hpp"
#include <fstream>
#include <set>
#include <stdexcept>
namespace world_visual {
using namespace first_person;
inline size_t checks=0;
inline void require(bool ok,const char *reason){++checks;if(!ok)throw std::runtime_error(reason);}
struct fixture {
    int size=11;std::vector<ncmm_graphics_cell_v1> cells;std::vector<ncmm_graphics_cell_v2> details;
    ncmm_graphics_scene_v2 scene{};hybrid::presentation_data appearance;
    explicit fixture(int size_=11):size(size_),cells(size*size),details(size*size){
        for(int i=0;i<size*size;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;
            std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
        scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),640,480,size,size,size*.5f,size-1.5f,cells.data(),cells.size()};
        scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=.8f;scene.environment.sun_y=-.5f;
        appearance.view.struct_size=sizeof(appearance.view);appearance.view.eye_height=1.55f;appearance.view.body_scale=1;appearance.view.held_length=.8f;appearance.cells.resize(cells.size());
    }
    int sample()const{return (size-4)*size+size/2;}
    void terrain(int i,const char *id){std::strcpy(cells[i].terrain,id);std::strcpy(details[i].ground,id);}
    void object(int i,const char *id,bool terrain_layer=false){
        ncmm_graphics_shapes::traits traits;traits.impassable=true;
        if(terrain_layer){std::strcpy(cells[i].terrain,id);details[i].terrain_shape=ncmm_graphics_shapes::terrain(id,traits);}
        else{std::strcpy(cells[i].furniture,id);details[i].furniture_shape=ncmm_graphics_shapes::furniture(id,traits);}
    }
    hybrid::sprite_provider provider(){return [this](uint32_t at,uint32_t layer,ncmm_graphics_sprite_v2 &out){
        static const uint32_t asphalt=0x333638ff,grass=0x6b803dff,dirt=0x856643ff,floor=0x8b8a7dff,object=0x82754fff;
        const uint32_t *pixel=&object;
        if(layer==NCMM_GFX_GROUND){const auto *id=cells[at].terrain;pixel=std::strstr(id,"pavement")?&asphalt:std::strstr(id,"grass")?&grass:std::strstr(id,"dirt")?&dirt:&floor;}
        out={sizeof(out),1,1,1,pixel};return true;};}
    hybrid::frame render(animation::pose pose={},float yaw=-hybrid::pi*.5f,float pitch=-.25f,int width=640,int height=480){
        scene.base.pixel_width=width;scene.base.pixel_height=height;hybrid::frame image;
        require(hybrid::build_frame(scene,yaw,pitch,provider(),image,width,height,pose,nullptr,nullptr,&appearance),"render-only world fixture");return image;}
};
struct collector:hybrid::geometry_sink {
    std::vector<hybrid::triangle> triangles;
    std::map<int,std::set<uint32_t>> floor_colors;
    bool dirty(int)const override{return true;}
    void emit(const hybrid::triangle &t)override{if(!t.m.overlay){triangles.push_back(t);
        if(t.normal.z>.99f&&!t.m.grass&&t.m.texture&&t.m.texture->valid()){
            const auto &image=t.m.texture->data;auto &colors=floor_colors[t.m.cell];
            for(uint32_t y=0;y<image.height;++y)for(uint32_t x=0;x<image.width;++x)colors.insert(image.rgba[size_t(y)*image.pitch_pixels+x]);
        }}}
    bool arms(const viewmodel::submission &)override{return true;}
    bool item(const held::submission &)override{return true;}
    void skin(const creature::model &,const creature::skeleton_pose &,int,hybrid::vec,int,hybrid::vec,bool,bool,float,hybrid::vec)override{}
    void impostor(const creature::asset &,const creature::pose_state &,hybrid::vec,int,int,bool)override{}
};
inline collector collect(fixture &f){hybrid::frame stats;collector out;hybrid::renderer(f.scene,-hybrid::pi*.5f,-.25f,f.provider(),stats,{},&f.appearance).collect(out,640,480);return out;}
inline void save(const hybrid::frame &frame,const std::string &name){if(name.empty())return;
    std::ofstream out(name+".ppm",std::ios::binary);out<<"P6\n"<<frame.width<<' '<<frame.height<<"\n255\n";
    for(const auto p:frame.pixels){const char rgb[]={char(p>>24),char(p>>16),char(p>>8)};out.write(rgb,3);}require(bool(out),"save pure renderer preview");}
inline bool sky(uint32_t p){return int((p>>8)&255)>int(p>>24)+15;}
inline const char *objects[]={"f_chair","f_armchair","f_sofa","f_bed","f_bunk_bed","f_bench","f_stool","f_table","f_desk","f_counter",
    "f_bookcase","f_locker","f_cabinet","f_crate_c","f_trashcan","f_toilet","f_bathtub","f_shower","f_sink","f_washer","f_dryer",
    "f_fridge","f_freezer","f_oven","f_microwave","f_indoor_plant","f_console","f_vending_c","f_atm","f_piano","f_rack","f_unknown_modded_object"};
}
