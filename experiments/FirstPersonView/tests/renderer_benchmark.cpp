#include <chrono>
#include <iomanip>
#include <fstream>
#include "scene_hash.hpp"
#include "hybrid_renderer.hpp"
#include "gpu_scene.hpp"
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
uint64_t gpu_preparation_digest(const ncmm_gpu_scene_v1 &view,uint64_t digest=1469598103934665603ull){
    auto hash=[&](const void *p,size_t bytes){digest=first_person::hash_bytes(digest,p,bytes);};
    // Descriptors contain callback pointers; hash immutable contents instead.
    hash(view.draws,size_t(view.draw_count)*sizeof(view.draws[0]));
    hash(view.cells,size_t(view.cell_count)*sizeof(view.cells[0]));
    hash(view.bones,size_t(view.bone_count)*sizeof(view.bones[0]));
    for(uint32_t i=0;i<view.mesh_count;++i){const auto &mesh=view.meshes[i];hash(&mesh.key,sizeof(mesh.key));
        hash(mesh.vertices,size_t(mesh.vertex_count)*sizeof(mesh.vertices[0]));hash(mesh.indices,size_t(mesh.index_count)*sizeof(mesh.indices[0]));
        hash(mesh.materials,size_t(mesh.material_count)*sizeof(mesh.materials[0]));}
    for(uint32_t i=0;i<view.texture_count;++i){const auto &texture=view.textures[i];hash(&texture.key,sizeof(texture.key));
        hash(texture.rgba,size_t(texture.pitch)*texture.height*sizeof(texture.rgba[0]));}
    return digest;
}
float gpu_preparation_cold_oracle(const ncmm_gpu_scene_v1 &actual,const ncmm_gpu_scene_v1 &fresh){
    require(actual.width==fresh.width&&actual.height==fresh.height&&actual.mode==fresh.mode&&actual.world_epoch==fresh.world_epoch&&actual.tileset_epoch==fresh.tileset_epoch&&
        actual.draw_count==fresh.draw_count&&actual.cell_count==fresh.cell_count&&actual.bone_count==fresh.bone_count&&actual.texture_count==fresh.texture_count,"static boundary cold scene counts");
    require(!std::memcmp(&actual.camera,&fresh.camera,sizeof(actual.camera))&&
        !std::memcmp(actual.cells,fresh.cells,size_t(actual.cell_count)*sizeof(actual.cells[0]))&&
        !std::memcmp(actual.bones,fresh.bones,size_t(actual.bone_count)*sizeof(actual.bones[0])),"static boundary cold camera, lighting and poses");
    float maximum_roundoff=0;
    auto mesh=[](const ncmm_gpu_scene_v1 &scene,uint64_t key){for(uint32_t i=0;i<scene.mesh_count;++i)if(scene.meshes[i].key==key)return &scene.meshes[i];return static_cast<const ncmm_gpu_mesh_v1 *>(nullptr);};
    for(uint32_t i=0;i<actual.draw_count;++i){auto a=actual.draws[i],b=fresh.draws[i];
        const auto *am=mesh(actual,a.mesh_key),*bm=mesh(fresh,b.mesh_key);a.mesh_key=b.mesh_key=0;
        require(!std::memcmp(&a,&b,sizeof(a)),"static boundary cold exact transforms, cells and passes");
        require(am&&bm&&am->vertex_count==bm->vertex_count&&am->index_count==bm->index_count&&am->material_count==bm->material_count&&am->reserved==bm->reserved,"static boundary cold mesh counts");
        require(!std::memcmp(am->indices,bm->indices,size_t(am->index_count)*sizeof(am->indices[0]))&&
            !std::memcmp(am->materials,bm->materials,size_t(am->material_count)*sizeof(am->materials[0])),"static boundary cold exact topology and materials");
        for(uint32_t n=0;n<am->vertex_count;++n){auto av=am->vertices[n],bv=bm->vertices[n];
            for(unsigned axis=0;axis<3;++axis){const float delta=std::abs(av.position[axis]-bv.position[axis]);
                require(std::isfinite(delta)&&delta<=1e-6f,"static boundary local position differs by more than one micrometre");
                maximum_roundoff=std::max(maximum_roundoff,delta);av.position[axis]=bv.position[axis]=0;}
            require(!std::memcmp(&av,&bv,sizeof(av)),"static boundary cold exact non-position vertex fields");
        }
    }
    for(uint32_t i=0;i<actual.texture_count;++i){const auto &a=actual.textures[i];const ncmm_gpu_texture_v1 *b=nullptr;
        for(uint32_t n=0;n<fresh.texture_count;++n)if(fresh.textures[n].key==a.key)b=&fresh.textures[n];
        require(b&&a.width==b->width&&a.height==b->height&&a.pitch==b->pitch&&
            !std::memcmp(a.rgba,b->rgba,size_t(a.pitch)*a.height*sizeof(a.rgba[0])),"static boundary cold exact texture contents");
    }
    return maximum_roundoff;
}
void gpu_preparation_benchmark(int rounds,const char *json_path,bool floor_templates=true){
    using clock=std::chrono::steady_clock;
    constexpr int side=97;
    std::vector<ncmm_graphics_cell_v1> cells(side*side);
    std::vector<ncmm_graphics_cell_v2> details(cells.size());
    presentation_data meta;meta.cells.resize(cells.size());meta.view.struct_size=sizeof(meta.view);
    meta.view.eye_height=1.55f;meta.view.body_scale=meta.view.held_length=1;
    auto populate=[&](int32_t ox,int32_t oy){
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){const size_t i=size_t(y)*side+x;
            const int64_t absolute_recipe=(int64_t(oy)-300+y)*side+int64_t(ox)-200+x;
            cells[i]={};details[i]={};cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");
            std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.9f;
            if(absolute_recipe%223==0){std::strcpy(cells[i].furniture,"f_table");details[i].furniture_shape=NCMM_SHAPE_TABLE;}
            if(absolute_recipe%229==0)details[i].terrain_shape=NCMM_SHAPE_WALL;
        }
    };
    populate(200,300);
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);
    scene.base={sizeof(ncmm_graphics_scene_v1),1280,800,side,side,48.5f,48.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;
    scene.environment.sun_z=1;scene.environment.animation_seconds=10;
    uint32_t pixel=0x727a68ff;uint64_t sprite_calls=0;
    auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &out){++sprite_calls;out={sizeof(out),1,1,1,&pixel};return true;};
    auto quantile=[](std::vector<double> v,double p){std::sort(v.begin(),v.end());const double at=(v.size()-1)*p;const size_t low=size_t(at);
        return v[low]+(v[std::min(low+1,v.size()-1)]-v[low])*(at-low);};
    std::ofstream out;if(json_path){out.open(json_path);require(bool(out),"GPU preparation JSON");
        out<<"{\"schema\":1,\"scope\":\"pure GPU producer CPU preparation, no device, upload, raster or presentation; cache clear and digest excluded\",\"timing_is_player_fps\":false,\"floor_templates\":"<<(floor_templates?"true":"false")<<",\"width\":1280,\"height\":800,\"side\":97,\"rounds\":"<<rounds<<",\"records\":[";}
    for(int mode=0;mode<5;++mode){
        first_person::gpu::scene_cache cache;first_person::gpu::packet packet;frame stats;
        cache.floor_templates_enabled=floor_templates;
        ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;host.world_origin_x=200;host.world_origin_y=300;
        if(mode==4)populate(host.world_origin_x,host.world_origin_y);
        require(cache.build(scene,0,0,provider,packet,stats,host,{},&meta,nullptr,1280,800,NCMM_GPU_FORCE),"GPU preparation warmup");
        std::vector<double> samples;std::vector<uint32_t> rebuilt;uint64_t digest=1469598103934665603ull;
        uint64_t overlap_checks=0;unsigned cold_oracles=0;float cold_position_roundoff=0;
        const uint64_t before=sprite_calls;
        for(int n=0;n<rounds;++n){
            if(mode==0)cache.clear();
            if(mode==2)host.world_origin_x+=n%2?-1:1;
            if(mode==3)details[side*48+48].terrain_shape=n%2?NCMM_SHAPE_WALL:NCMM_SHAPE_FLAT;
            if(mode==4){
                // Preserve absolute world contents across a moving snapshot,
                // unlike the original origin-only stress case in mode 2.
                const auto old_cells=cells;const auto old_details=details;
                const int step=n%2?-1:1;host.world_origin_x+=step;
                populate(host.world_origin_x,host.world_origin_y);
                for(int y=0;y<side;++y)for(int x=0;x<side;++x){const int former_x=x+step;if(former_x<0||former_x>=side)continue;
                    const size_t i=size_t(y)*side+x,former=size_t(y)*side+former_x;
                    require(!std::memcmp(&cells[i],&old_cells[former],sizeof(cells[i]))&&
                            !std::memcmp(&details[i],&old_details[former],sizeof(details[i])),"static boundary changed an overlapping world cell");
                    ++overlap_checks;
                }
            }
            packet.reset();const auto start=clock::now();
            require(cache.build(scene,mode==1?float(n)*.037f:0,0,provider,packet,stats,host,{},&meta,nullptr,1280,800,NCMM_GPU_FORCE),"GPU preparation frame");
            const auto &view=packet.finish(nullptr);
            samples.push_back(std::chrono::duration<double,std::milli>(clock::now()-start).count());
            rebuilt.push_back(cache.generated_cells);
            digest=gpu_preparation_digest(view,digest);
            if(mode==4){
                // Compare every first changed packet to a fresh original
                // generator. Existing world-to-local float roundoff may
                // change mesh keys/positions by micrometres as XY shifts;
                // every other render field is exact. This work is not timed.
                first_person::gpu::scene_cache reference;reference.floor_templates_enabled=false;
                first_person::gpu::packet expected;frame expected_stats;
                const uint64_t reads_before_oracle=sprite_calls;
                require(reference.build(scene,0,0,provider,expected,expected_stats,host,{},&meta,nullptr,1280,800,NCMM_GPU_FORCE),"static boundary cold oracle");
                const auto &expected_view=expected.finish(nullptr);
                cold_position_roundoff=std::max(cold_position_roundoff,gpu_preparation_cold_oracle(view,expected_view));
                sprite_calls=reads_before_oracle;++cold_oracles;
            }
        }
        const char *name=std::array<const char *,5>{"cold","camera_turn","tile_boundary","local_mutation","tile_boundary_static_world"}[mode];
        std::cout<<name<<": first "<<samples.front()<<" ms, median "<<quantile(samples,.5)<<" ms, p95 "<<quantile(samples,.95)
                 <<" ms, p99 "<<quantile(samples,.99)<<" ms; digest "<<digest<<", atlas reads "<<sprite_calls-before<<'\n'<<std::flush;
        if(mode==4)std::cout<<"Static overlap cells checked="<<overlap_checks<<", cold rebuilds="<<cold_oracles<<", maximum local position roundoff="<<cold_position_roundoff<<"; other render fields exact\n";
        if(json_path){if(mode)out<<',';out<<"{\"scene\":\""<<name<<"\",\"first_ms\":"<<samples.front()<<",\"median_ms\":"<<quantile(samples,.5)
            <<",\"p95_ms\":"<<quantile(samples,.95)<<",\"p99_ms\":"<<quantile(samples,.99)<<",\"digest\":\""<<digest<<"\",\"sprite_reads\":"<<sprite_calls-before
            <<",\"samples_ms\":[";for(size_t i=0;i<samples.size();++i){if(i)out<<',';out<<samples[i];}
            out<<"],\"generated_cells\":[";for(size_t i=0;i<rebuilt.size();++i){if(i)out<<',';out<<rebuilt[i];}
            out<<"],\"static_overlap_cells_checked\":"<<overlap_checks<<",\"cold_rebuilds_checked\":"<<cold_oracles<<",\"cold_rebuild_max_position_delta\":"<<cold_position_roundoff<<"}";}
        details[side*48+48].terrain_shape=NCMM_SHAPE_FLAT;
    }
    if(json_path){out<<"]}\n";require(bool(out),"GPU preparation JSON write");}
}
// Manual timing tool, deliberately not a CI pass/fail FPS threshold. Run on an
// otherwise idle machine and compare the same build type and round count.
int main(int argc,char **argv){
    using clock=std::chrono::steady_clock;
    const int rounds=argc>1?std::max(1,std::atoi(argv[1])):12;
    if(argc>2&&std::string(argv[2])=="gpu-preparation"){gpu_preparation_benchmark(rounds,argc>3?argv[3]:nullptr);return 0;}
    if(argc>2&&std::string(argv[2])=="gpu-preparation-reference-floor"){gpu_preparation_benchmark(rounds,argc>3?argv[3]:nullptr,false);return 0;}
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
