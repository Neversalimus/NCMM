#include <chrono>
#include <iomanip>
#include <fstream>
#include "scene_hash.hpp"
#include "hybrid_renderer.hpp"
#include "ncmm_graphics_camera.hpp"
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
void viewmodel_benchmark(int rounds,const char *json_path){
    using clock=std::chrono::steady_clock;
    auto quantile=[](const std::vector<double> &sorted,double p){const double at=(sorted.size()-1)*p;const auto low=size_t(at);
        return sorted[low]+(sorted[std::min(low+1,sorted.size()-1)]-sorted[low])*(at-low);};
    std::ofstream out;if(json_path){out.open(json_path);require(bool(out),"viewmodel JSON");
        out<<"{\"schema\":1,\"timing_is_player_fps\":false,\"scope\":\"isolated CPU source animation sampling, two arm IK solves and exact weighted palette matrices; excludes materials, packet construction, world, rasterization, GPU and presentation\",\"rounds\":"<<rounds<<",\"records\":[";}
    double checksum=0;
    for(int mode=0;mode<3;++mode){
        first_person::viewmodel::animator animator;
        first_person::viewmodel::hand_pose right,left;
        std::vector<double> times;times.reserve(size_t(rounds));double first_prepare_ms=0;
        for(int n=-32;n<rounds;++n){
            first_person::animation::pose action;action.seconds=10+(n+32)/60.;action.attack_kind=mode==2?2:1;
            action.attack_serial=1;action.attack_phase=float((n+32)%60)/60;action.grip=mode==2?NCMM_GRIP_GUN:NCMM_GRIP_SINGLE;
            const auto start=clock::now();const auto &frame=animator.prepare(action,mode==0,mode==2);
            const first_person::viewmodel::point trigger{.192f,-.261f,.735f};
            require(frame.attach({.185f,-.267f,.68f},false,.525f,right,mode==2?&trigger:nullptr)&&
                    frame.attach({-.06f,-.28f,.75f},true,.525f,left),"viewmodel contacts");
            const double milliseconds=std::chrono::duration<double,std::milli>(clock::now()-start).count();
            if(n==-32)first_prepare_ms=milliseconds;
            checksum+=right.palette[size_t(n+32)%right.palette_count].columns[12]+left.palette[size_t(n+32)%left.palette_count].columns[13];
            if(n>=0)times.push_back(milliseconds);
        }
        auto sorted=times;std::sort(sorted.begin(),sorted.end());const char *name=mode==0?"empty":mode==1?"held":"firearm";
        std::cout<<name<<" source pose + two arms: median "<<quantile(sorted,.5)<<" ms, p95 "<<quantile(sorted,.95)<<" ms, p99 "<<quantile(sorted,.99)<<" ms, max "<<sorted.back()<<" ms\n";
        if(json_path){if(mode)out<<',';out<<"{\"state\":\""<<name<<"\",\"first_prepare_ms\":"<<first_prepare_ms<<",\"first_prepare_includes_initial_asset_validation\":"<<(mode==0?"true":"false")<<",\"median_ms\":"<<quantile(sorted,.5)<<",\"p95_ms\":"<<quantile(sorted,.95)<<",\"p99_ms\":"<<quantile(sorted,.99)<<",\"max_ms\":"<<sorted.back()<<",\"samples_ms\":[";
            for(size_t i=0;i<times.size();++i){if(i)out<<',';out<<times[i];}out<<"]}";}
    }
    require(std::isfinite(checksum),"viewmodel checksum");
    if(json_path){out<<"],\"checksum\":"<<checksum<<"}\n";require(bool(out),"viewmodel JSON write");}
}
void distance_benchmark(int rounds,const char *json_path){
    struct measurement {int radius,mode,cached;bool optimized;double median,p95;uint32_t cells,culled,sky;uint64_t samples;};
    std::vector<measurement> records;
    using clock=std::chrono::steady_clock;
    for(bool optimized:{false,true})for(int radius:{NCMM_GRAPHICS_RADIUS,NCMM_GRAPHICS_V2_RADIUS})for(int mode=0;mode<4;++mode){
        render_options settings;if(!optimized)settings.cull_geometry=settings.skip_hidden_sky=settings.skip_empty_volume=false;
        fixture a;const int side=radius*2+1;
        a.cells.assign(side*side,{});a.details.assign(side*side,{});a.meta.cells.assign(side*side,{});
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){const int i=y*side+x;
            if(!ncmm_graphics_camera::in_front(-pi*.5f,float(x-radius),float(y-radius)))continue;
            a.cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(a.cells[i].terrain,"t_floor");std::strcpy(a.details[i].ground,"t_floor");
            a.details[i].light_r=a.details[i].light_g=a.details[i].light_b=.9f;
            if(mode==1&&(x%5==1&&y%5==1))a.details[i].terrain_shape=NCMM_SHAPE_TREE;
            if(mode==2&&y<radius-4&&y%10<3&&(x%10==2||x%10==3)){
                auto &d=a.details[i];a.cells[i].flags|=NCMM_GFX_VEHICLE;
                d.vehicle_features=NCMM_VEH_FRAME|NCMM_VEH_BOARD|NCMM_VEH_ROOF|NCMM_VEH_WHEEL;
                d.vehicle_group=1+(y/10)*side+x/10;d.mount_x=y%10;d.mount_y=x%10-2;
                d.vehicle_x=float(x-(x%10-2))+.5f;d.vehicle_y=float(y-y%10)+.5f;d.vehicle_yaw=pi*.5f;d.vehicle_health=1;
            }
            if(mode==3&&y<radius-12&&y>radius-44&&x>radius-4&&x<radius+4){a.meta.cells[i].gas=NCMM_GAS_SMOKE;a.meta.cells[i].field_intensity=2;}
        }
        a.scene.base={sizeof(ncmm_graphics_scene_v1),1280,800,side,side,radius+.5f,radius+.5f,a.cells.data(),a.cells.size()};
        a.scene.details=a.details.data();a.scene.detail_count=a.details.size();a.meta.view.flags=NCMM_PRESENT_GPU_EFFECTS;
        frame world;require(build_frame(a.scene,-pi*.5f,0,a.provider(),a.f,1280,800,{},nullptr,&world,&a.meta,settings),"distance setup");
        for(int cached=0;cached<2;++cached){std::vector<double> times;
            for(int n=0;n<rounds;++n){a.scene.environment.animation_seconds=10+n*.025;const auto start=clock::now();
                require(build_frame(a.scene,-pi*.5f+(cached?0:std::sin(n*.1f)*.02f),0,a.provider(),a.f,1280,800,{},cached?&world:nullptr,nullptr,&a.meta,settings),"distance frame");
                times.push_back(std::chrono::duration<double,std::milli>(clock::now()-start).count());}
            std::sort(times.begin(),times.end());const double median=times[times.size()/2],p95=times[size_t(std::ceil(times.size()*.95))-1];
            records.push_back({radius,mode,cached,optimized,median,p95,a.f.geometry_cells,a.f.culled_cells,a.f.sky_pixels,a.f.volume_samples});
            std::cout<<(optimized?"optimized":"reference")<<" range "<<radius<<' '<<std::array<const char *,4>{"open","forest","vehicles","smoke"}[mode]<<' '<<(cached?"cached":"camera")<<' '<<std::fixed<<std::setprecision(2)<<median<<" ms, p95 "<<p95<<" ms; cells "<<a.f.geometry_cells<<", culled "<<a.f.culled_cells<<", sky pixels "<<a.f.sky_pixels<<", volume samples "<<a.f.volume_samples<<'\n'<<std::flush;
        }
    }
    if(json_path){std::ofstream out(json_path);require(bool(out),"JSON output");
        out<<"{\"schema\":1,\"version\":\""<<FIRST_PERSON_VERSION<<"\",\"width\":1280,\"height\":800,\"rounds\":"<<rounds<<",\"hardware_threads\":"<<std::thread::hardware_concurrency()<<",\"timing_is_player_fps\":false,\"records\":[";
        for(size_t i=0;i<records.size();++i){const auto &r=records[i];if(i)out<<',';
            out<<"{\"policy\":\""<<(r.optimized?"optimized":"unculled_reference")<<"\",\"radius\":"<<r.radius<<",\"scene\":\""<<std::array<const char *,4>{"open","forest","vehicles","smoke"}[r.mode]<<"\",\"cached\":"<<(r.cached?"true":"false")<<",\"median_ms\":"<<r.median<<",\"p95_ms\":"<<r.p95<<",\"geometry_cells\":"<<r.cells<<",\"culled_cells\":"<<r.culled<<",\"sky_pixels\":"<<r.sky<<",\"volume_samples\":"<<r.samples<<'}';}
        out<<"]}\n";require(bool(out),"JSON write");}
}
// Manual timing tool, deliberately not a CI pass/fail FPS threshold. Run on an
// otherwise idle machine and compare the same build type and round count.
int main(int argc,char **argv){
    using clock=std::chrono::steady_clock;
    const int rounds=argc>1?std::max(1,std::atoi(argv[1])):12;
    if(argc>2&&std::string(argv[2])=="viewmodel"){viewmodel_benchmark(rounds,argc>3?argv[3]:nullptr);return 0;}
    if(argc>2&&std::string(argv[2])=="distance"){distance_benchmark(rounds,argc>3?argv[3]:nullptr);return 0;}
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
