#include <chrono>
#include <iomanip>
#include "hybrid_renderer.hpp"
#include <cstdlib>
#include <iostream>
using namespace first_person::hybrid;
struct fixture {
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(81);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(81);
    std::vector<uint32_t> art=std::vector<uint32_t>(1024,0x727a68ff);
    presentation_data meta;ncmm_graphics_scene_v2 scene{};frame f;
    fixture(){
        meta.view.struct_size=sizeof(meta.view);meta.view.eye_height=1.55f;meta.view.body_scale=meta.view.held_length=1;meta.cells.resize(81);
        for(int i=0;i<81;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.9f;}
        scene={sizeof(scene),{sizeof(ncmm_graphics_scene_v1),1280,800,9,9,4.5f,7.5f,cells.data(),cells.size()},details.data(),details.size(),{}};
        scene.environment.daylight=1;scene.environment.sun_z=scene.environment.sun_x=.7f;scene.environment.cloud_cover=.45f;scene.environment.animation_seconds=10;
    }
    sprite_provider provider(){return [&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &out){out={sizeof(out),32,32,32,art.data()};return true;};}
};
void require(bool value,const char *name){if(!value){std::cerr<<"Invalid benchmark "<<name<<'\n';std::exit(1);}}
// Manual timing tool, deliberately not a CI pass/fail FPS threshold. Run on an
// otherwise idle machine and compare the same build type and round count.
int main(int argc,char **argv){
    using clock=std::chrono::steady_clock;
    const int rounds=argc>1?std::max(1,std::atoi(argv[1])):12;
    for(int mode=0;mode<5;++mode){
        fixture a;a.scene.base.pixel_width=1280;a.scene.base.pixel_height=800;
        a.meta.view.flags=NCMM_PRESENT_GPU_EFFECTS;
        if(mode==1)for(int y=1;y<7;++y)for(int x:{1,7})a.details[y*9+x].terrain_shape=NCMM_SHAPE_WALL;
        if(mode==2)for(int y=0;y<7;++y)for(int x=0;x<9;++x){a.details[y*9+x].terrain_shape=NCMM_SHAPE_WATER;a.meta.cells[y*9+x].liquid=NCMM_LIQ_SEA;}
        if(mode==3||mode==4){for(int y=2;y<6;++y)for(int x=2;x<7;++x){a.meta.cells[y*9+x].gas=NCMM_GAS_SMOKE;a.meta.cells[y*9+x].field_intensity=2;}
            a.meta.cells[3*9+4].source_light=20;a.meta.cells[3*9+4].source_r=1;if(mode==3)a.meta.view.flags=0;}
        frame world;
        require(build_frame(a.scene,-pi*.5f,-.2f,a.provider(),a.f,1280,800,{},nullptr,&world,&a.meta),"setup");
        for(int cached=0;cached<2;++cached){
            auto start=clock::now();
            for(int n=0;n<rounds;++n){a.scene.environment.animation_seconds=10+n*.025;
                require(build_frame(a.scene,-pi*.5f+(cached?0:std::sin(n*.1f)*.1f),-.2f,a.provider(),a.f,1280,800,{},cached?&world:nullptr,nullptr,&a.meta),"frame");}
            const auto ms=std::chrono::duration<double,std::milli>(clock::now()-start).count()/rounds;
            std::cout<<std::array<const char *,5>{"clear","walls","water","smoke_cpu","smoke_gpu_layer"}[mode]<<' '<<(cached?"cached":"camera")<<' '<<std::fixed<<std::setprecision(2)<<ms<<" ms\n"<<std::flush;
        }
    }
}
