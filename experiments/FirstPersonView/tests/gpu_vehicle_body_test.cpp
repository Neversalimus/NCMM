#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include "vehicle_body_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(){
    const auto require=[](bool ok,const char *why){if(!ok){std::fprintf(stderr,"Vehicle body device failed: %s (%s)\n",why,SDL_GetError());std::exit(1);}};
    // Compile-only locally. Execution is exclusively on the actual CI devices.
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM whole opaque body controls",1280,800,SDL_WINDOW_HIDDEN);require(window!=nullptr,"hidden actual CI surface");
#endif
    const auto props=SDL_CreateProperties();
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(device!=nullptr,"actual GPU device without CPU fallback");
    auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"actual current renderer");
    auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);require(target&&SDL_SetRenderTarget(renderer,target),"full unchanged1280x800 target");
    ncmm_gpu::backend engine;require(engine.bind(renderer,8800),"owned exact CI backend");
    namespace fp=first_person;namespace v=fp::vehicle;
    std::vector<ncmm_graphics_cell_v1> cells(1600);std::vector<ncmm_graphics_cell_v2> details(1600);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");
        details[i].light_r=.35f+float(i%5)*.12f;details[i].light_g=.4f+float(i%3)*.13f;details[i].light_b=.5f;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,40,40,23.5f,30.5f,cells.data(),1600};
    source.details=details.data();source.detail_count=1600;source.environment.daylight=1;source.environment.sun_x=.3f;source.environment.sun_y=.4f;source.environment.sun_z=.8f;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(1600);
    v::presentation vehicles;metadata.vehicles=&vehicles;const auto construction=body_occlusion_fixture::construction();
    require(v::valid(construction,1600),"the real owned part rows use valid actual mount cells");
    fp::gpu::scene_cache cache;
    struct observation {std::vector<uint32_t> pixels;int focus=-1,target=-1;uint64_t triangles=0,draws=0,parts=0;};
    const auto read=[&](){observation o;require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose current body control");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read complete actual image");
        auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image&&image->w==1280&&image->h==800,"no resolution cut");
        o.pixels.resize(1280*800);for(int y=0;y<800;++y)std::memcpy(o.pixels.data()+1280*y,static_cast<const char *>(image->pixels)+image->pitch*y,1280*4);
        SDL_DestroySurface(image);o.focus=engine.focus_cell();o.target=engine.metrics().target_cell;return o;};
    const auto views=body_occlusion_fixture::cameras();std::ofstream report("vehicle-body-device-controls.json");require(bool(report),"dedicated source-bound receipt");
    report<<"{\"full_viewport\":[1280,800],\"original501_and750_controls_unchanged\":true,\"whole_opaque_passes_only\":true,\"pairs\":[";
    bool first=true,all_equal=true;unsigned pairs=0,active=0,fallback=0;uint64_t total_different=0,total_saved=0;
    for(unsigned variant=0;variant<6;++variant){vehicles.reset();vehicles.input=construction;body_occlusion_fixture::variant(vehicles.input,variant);
        for(auto &c:cells)c.flags=NCMM_GFX_VISIBLE;
        if(variant==2)for(const auto &p:vehicles.input.parts)if(p.mount_x==-5&&p.cell_index>=0)cells[size_t(p.cell_index)].flags=0;
        for(unsigned vi=0;vi<views.size();++vi){const auto &camera=views[vi];source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
            const auto submit=[&](bool enabled){fp::gpu::packet packet;fp::hybrid::frame statistics;fp::hybrid::render_options options;options.cull_gpu_vehicle_bodies=enabled;
                require(fp::hybrid::valid_scene(source,camera.yaw,camera.pitch)&&fp::hybrid::valid_presentation(metadata,1600),"unchanged full producer contracts");
                const bool built=cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,engine.metrics(),{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE,nullptr,options);
                if(!built)std::fprintf(stderr,"Vehicle body build variant=%u view=%u policy=%u\n",variant,vi,unsigned(enabled));require(built,cache.last_reason.c_str());
                const auto &scene=packet.finish(nullptr);uint64_t triangles=0;
                for(uint32_t i=0;i<scene.draw_count;++i)for(uint32_t j=0;j<scene.mesh_count;++j)if(scene.draws[i].mesh_key==scene.meshes[j].key)triangles+=scene.meshes[j].index_count/3;
                require(engine.submit(scene),engine.metrics().reason);auto o=read();o.triangles=triangles;o.draws=scene.draw_count;o.parts=statistics.vehicle_parts;return o;};
            observation a,b;if((variant+vi)%2){b=submit(true);a=submit(false);}else{a=submit(false);b=submit(true);}
            require(a.parts==b.parts,"all original Native permitted parts stay represented");
            require(b.triangles<=a.triangles&&b.draws<=a.draws,"whole-pass removal never adds source work");
            const uint64_t saved=a.triangles-b.triangles;if(saved)++active;else ++fallback;total_saved+=saved;
            if(variant==0&&vi==0)require(saved>0,"the actual side-facing bus control exercises genuine work removal");
            size_t different=0,first_difference=0;for(size_t i=0;i<a.pixels.size();++i)if(a.pixels[i]!=b.pixels[i]){if(!different)first_difference=i;++different;}
            const bool equal=!different&&a.focus==b.focus&&a.target==b.target;all_equal&=equal;total_different+=different;++pairs;
            if(!first)report<<",";first=false;
            report<<"{\"variant\":"<<variant<<",\"view\":"<<vi<<",\"saved_triangles\":"<<saved<<",\"saved_passes\":"<<(a.draws-b.draws)<<",\"actual_parts\":"<<b.parts
                <<",\"pixels_equal\":"<<(different?"false":"true")<<",\"different_pixels\":"<<different<<",\"focus_equal\":"<<(a.focus==b.focus?"true":"false")<<",\"target_equal\":"<<(a.target==b.target?"true":"false")<<"}";
            if(!equal)std::fprintf(stderr,"Vehicle body pixel mismatch variant=%u view=%u pixels=%zu first=%zu before=%08x after=%08x focus=%d/%d target=%d/%d\n",variant,vi,different,first_difference,a.pixels[first_difference],b.pixels[first_difference],a.focus,b.focus,a.target,b.target);
        }
    }
    require(pairs==60&&active>0&&fallback>0&&total_saved>0,"all60 full images cover active occlusion, holes, hidden cells, rotation, damage, doors, inside and near-plane fallback");
    report<<"],\"pair_count\":"<<pairs<<",\"active_pairs\":"<<active<<",\"fallback_pairs\":"<<fallback<<",\"saved_triangles\":"<<total_saved<<",\"different_pixels\":"<<total_different<<",\"success\":"<<(all_equal?"true":"false")<<"}\n";
    report.close();require(bool(report),"complete actual device receipt");
    engine.clear();SDL_SetRenderTarget(renderer,nullptr);SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
    require(all_equal,"every actual color pixel and both target queries equal complete geometry");
    std::printf("vehicle body device: %u full1280x800 pairs, %u active, %u fallback; all pixels and targets exact\n",pairs,active,fallback);
}
