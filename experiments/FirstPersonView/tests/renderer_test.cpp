#include "renderer.hpp"
#include "ncmm_graphics_policy.hpp"
#include "ncmm_api.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

extern "C" const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1();
extern "C" void ncmm_open_ui_v1(const ncmm_host_api_v1 *);
namespace {
int checks=0;
void check(bool value,const char *name) { ++checks; if(!value) throw std::runtime_error(name); }
std::vector<ncmm_graphics_cell_v1> room() {
    std::vector<ncmm_graphics_cell_v1> cells(81);
    for(int y=0;y<9;++y) for(int x=0;x<9;++x) {
        auto &c=cells[y*9+x]; c.flags=NCMM_GFX_VISIBLE;c.light=1;
        std::strcpy(c.terrain,"t_floor");
        if(x==0 || x==8 || y==0 || y==8) {c.flags|=NCMM_GFX_WALL;std::strcpy(c.terrain,"t_wall");}
    }
    std::strcpy(cells[4*9+3].furniture,"f_chair");
    std::strcpy(cells[2*9+5].creature,"mon_zombie");
    std::strcpy(cells[6*9+5].item,"bottle_plastic");
    return cells;
}
ncmm_graphics_scene_v1 scene(std::vector<ncmm_graphics_cell_v1> &cells,int width=960,int height=540) {
    return {sizeof(ncmm_graphics_scene_v1),width,height,9,9,4.5f,7.5f,cells.data(),cells.size()};
}
bool enabled=false,capability=true; int registrations=0,submitted=0;
ncmm_graphics_render_v1 render_callback=nullptr;
ncmm_graphics_input_v1 input_callback=nullptr;
ncmm_host_api_v2_core core{};
int reg(const char *id,ncmm_graphics_render_v1 r,ncmm_graphics_input_v1 i,void *) {
    if(std::strcmp(id,"first_person_view"))return 0;
    ++registrations;render_callback=r;input_callback=i;return 1;
}
int enable(const char *,int yes){enabled=yes!=0;return 1;}
int is_enabled(const char *){return enabled;}
int submit(const ncmm_graphics_quad_v1 *q,size_t count){submitted=static_cast<int>(count);return q && count>0;}
ncmm_host_graphics_v1 graphics{1,sizeof(ncmm_host_graphics_v1),1,0,reg,enable,is_enabled,submit};
int has(const char *cap){return std::strcmp(cap,NCMM_GRAPHICS_CAP) || capability;}
uint32_t major(){return 1;} uint32_t minor(){return 9;}
const void *query(const char *id,uint32_t,uint32_t){return std::strcmp(id,NCMM_GRAPHICS_ID)==0 ? static_cast<const void *>(&graphics) : &core;}
}
int main(int argc,char **argv) {
    try {
        auto cells=room();auto s=scene(cells);
        std::vector<ncmm_graphics_quad_v1> quads;
        check(first_person::build_frame(s,-first_person::pi/2,quads),"room render");
        check(quads.size()<NCMM_GRAPHICS_MAX_QUADS,"bounded commands");
        for(const auto &q:quads) check(ncmm_graphics_policy::valid_quad(q,s),"valid submitted geometry");
        check(std::abs(first_person::trace(s,0,-1).distance-6.5)<1e-6,"cardinal distance");
        check(first_person::trace(s,1,0).cell==7*9+8,"east ray");
        auto &door=cells[4*9+4];door.flags|=NCMM_GFX_WALL;
        check(std::abs(first_person::trace(s,0,-1).distance-2.5)<1e-6,"closed door occlusion");
        std::strcpy(cells[3*9+4].creature,"mon_zombie");
        check(first_person::build_frame(s,-first_person::pi/2,quads),"closed room render");
        bool behind=false;for(const auto &q:quads) behind|=q.cell_index==3*9+4 && q.layer==NCMM_GFX_CREATURE;
        check(!behind,"entity behind closed door hidden");
        door.flags&=~NCMM_GFX_WALL;
        check(first_person::build_frame(s,-first_person::pi/2,quads),"open door render");
        behind=false;for(const auto &q:quads) behind|=q.cell_index==3*9+4 && q.layer==NCMM_GFX_CREATURE;
        check(behind,"open door reveals visible entity");
        cells[3*9+4].flags=0; cells[3*9+4].light=0;
        check(first_person::build_frame(s,-first_person::pi/2,quads),"fog render");
        for(const auto &q:quads) check(q.cell_index!=3*9+4,"unknown cell identity never used");
        check(!ncmm_graphics_policy::valid_quad({0,0,10,10,0,0,1,1,0xffffffff,31,NCMM_GFX_CREATURE},s),"Host rejects hidden cell command");
        auto bad=s;bad.cell_count=1;check(!first_person::build_frame(bad,0,quads),"bad grid size");
        bad=s;bad.camera_x=1e30f;check(!first_person::build_frame(bad,0,quads),"bad camera bounds");
        check(!first_person::build_frame(s,std::numeric_limits<double>::quiet_NaN(),quads),"NaN yaw");
        bad=s;bad.pixel_width=100000;check(!first_person::build_frame(bad,0,quads),"oversize viewport");
        cells=room();s=scene(cells);
        const auto begin=std::chrono::steady_clock::now();
        for(int angle=0;angle<72;++angle) {
            check(first_person::build_frame(s,angle*first_person::pi/36,quads),"all directions");
            for(const auto &q:quads) check(ncmm_graphics_policy::valid_quad(q,s),"all direction command bounds");
        }
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
        check(elapsed<10000,"bounded CPU render budget");
        for(const auto &dims: {std::pair<int,int>{1,1},{3440,1440},{8192,8192},{320,1000}}) {
            s=scene(cells,dims.first,dims.second);
            check(first_person::build_frame(s,-1.4,quads),"viewport size/aspect");
            for(const auto &q:quads) check(ncmm_graphics_policy::valid_quad(q,s),"viewport geometry");
        }
        core.struct_size=sizeof(core);core.abi_version=2;core.api_major=2;core.api_minor=3;
        ncmm_host_api_v1 api{};api.abi_version=1;api.has_capability=has;
        api.get_api_version_major=major;api.get_api_version_minor=minor;api.query_interface=query;
        const auto *descriptor=ncmm_get_descriptor_v1();
        capability=false;check(!descriptor->init(&api) && registrations==0,"missing graphics capability fail closed");
        capability=true;graphics.struct_size=16;
        check(!descriptor->init(&api) && registrations==0,"truncated graphics table fail closed");
        graphics.struct_size=sizeof(graphics);
        check(descriptor->init(&api) && registrations==1,"module registration");
        check(!enabled,"initially tiles");
        ncmm_open_ui_v1(&api);check(enabled,"single key toggles on");
        s=scene(cells);check(render_callback(&s,nullptr) && submitted>0,"module submits frame");
        input_callback(NCMM_GFX_TURN_LEFT,nullptr);check(render_callback(&s,nullptr),"camera rotation redraw");
        ncmm_open_ui_v1(&api);check(!enabled,"single key returns to tiles");
        descriptor->shutdown();ncmm_open_ui_v1(&api);check(!enabled,"shutdown disables view");
        if(argc==2) {
            cells=room();s=scene(cells);first_person::build_frame(s,-first_person::pi/2,quads);
            std::ofstream out(argv[1]);out<<"[";
            for(size_t i=0;i<quads.size();++i){const auto &q=quads[i];if(i)out<<",";
                out<<"["<<q.x0<<","<<q.y0<<","<<q.x1<<","<<q.y1<<","<<q.u0<<","<<q.v0<<","<<q.u1<<","<<q.v1<<","<<q.rgba<<","<<q.cell_index<<","<<q.layer<<"]";}
            out<<"]\n";
        }
        std::cout<<"PASS: "<<checks<<" checks; 72 frames incl. validation: "<<elapsed<<" ms\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
