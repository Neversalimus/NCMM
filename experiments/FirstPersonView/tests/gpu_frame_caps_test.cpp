#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include "vehicle_frame_caps_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(){
    const auto require=[](bool ok,const char *why){if(!ok){std::fprintf(stderr,"Frame cap device failed: %s (%s)\n",why,SDL_GetError());std::exit(1);}};
    // This entire executable is compile-only locally and runs only on CI devices.
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM frame cap device controls",1280,800,SDL_WINDOW_HIDDEN);require(window!=nullptr,"hidden native device surface");
#endif
    const auto props=SDL_CreateProperties();
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(device!=nullptr,"actual GPU device, no fallback");
    auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared renderer");
    auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);require(target&&SDL_SetRenderTarget(renderer,target),"full 1280x800 target");
    ncmm_gpu::backend engine;require(engine.bind(renderer,8100),"owned actual CI backend");
    namespace fp=first_person;namespace v=fp::vehicle;
    std::vector<ncmm_graphics_cell_v1> cells(625);std::vector<ncmm_graphics_cell_v2> details(625);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");
        details[i].light_r=.35f+float(i%5)*.12f;details[i].light_g=.4f+float(i%3)*.13f;details[i].light_b=.5f;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,25,25,12.5f,23.5f,cells.data(),625};
    source.details=details.data();source.detail_count=625;source.environment.daylight=1;source.environment.sun_x=.3f;source.environment.sun_y=.4f;source.environment.sun_z=.8f;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(625);
    v::presentation vehicles;metadata.vehicles=&vehicles;const auto construction=frame_caps_fixture::construction();
    fp::gpu::scene_cache cache;fp::hybrid::frame statistics;
    struct observation{std::vector<uint32_t> pixels;int focus=-1,target=-1;uint64_t triangles=0;};
    const auto read=[&](){observation o;require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose frame cap control");auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read full cap device image");
        auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image&&image->w==1280&&image->h==800,"no resolution cut");
        o.pixels.resize(1280*800);for(int y=0;y<800;++y)std::memcpy(o.pixels.data()+1280*y,static_cast<const char *>(image->pixels)+image->pitch*y,1280*4);
        SDL_DestroySurface(image);o.focus=engine.focus_cell();o.target=engine.metrics().target_cell;return o;};
    const auto views=frame_caps_fixture::cameras();
    std::ofstream report("frame-cap-device-controls.json");require(bool(report),"own dedicated device control receipt");
    report<<"{\"full_viewport\":[1280,800],\"original501_pipeline_unchanged\":true,\"pairs\":[";
    bool first=true,all_equal=true;unsigned pairs=0,active=0,fallback=0;uint64_t total_different=0;
    for(unsigned variant=0;variant<5;++variant){
        vehicles.reset();vehicles.input=construction;
        for(auto &c:cells)c.flags=NCMM_GFX_VISIBLE;
        if(variant==1)for(auto &p:vehicles.input.parts)if(p.mount_x==6)p.state|=NCMM_VP_TOPOLOGY_ONLY,p.cell_index=-1;
        if(variant==2)for(const auto &p:vehicles.input.parts)if(p.mount_x==6)cells[size_t(p.cell_index)].flags=0;
        if(variant==3)vehicles.input.vehicles[0].yaw=.37f;
        if(variant==4){vehicles.input.scene.world_epoch+=1;for(auto &p:vehicles.input.parts)p.health=.35f;}
        for(unsigned n=0;n<views.size();++n){const auto &camera=views[n];source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
            const auto submit=[&](bool join){fp::gpu::packet packet;fp::hybrid::render_options options;options.join_gpu_frame_caps=join;
                require(fp::hybrid::valid_scene(source,camera.yaw,camera.pitch),"dedicated fixture scene is valid before GPU work");
                require(fp::hybrid::valid_presentation(metadata,625),"dedicated fixture presentation is valid before GPU work");
                const bool built=cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,engine.metrics(),{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE,nullptr,options);
                if(!built)std::fprintf(stderr,"Frame cap build context variant=%u view=%u join=%u\n",variant,n,unsigned(join));
                require(built,cache.last_reason.c_str());
                const auto &submission=packet.finish(nullptr);uint64_t triangles=0;
                for(uint32_t i=0;i<submission.draw_count;++i)for(uint32_t j=0;j<submission.mesh_count;++j)if(submission.draws[i].mesh_key==submission.meshes[j].key)triangles+=submission.meshes[j].index_count/3;
                require(engine.submit(submission),engine.metrics().reason);auto o=read();o.triangles=triangles;return o;};
            // Alternating order also exercises cached full/cap-mask variants.
            observation a,b;if((n+variant)%2){b=submit(true);a=submit(false);}else{a=submit(false);b=submit(true);}
            require(b.triangles<=a.triangles,"cap planner can only remove explicitly counted indices");
            const auto saved=a.triangles-b.triangles;if(saved)++active;else ++fallback;
            if(variant==0&&n<2)require(saved==1664,"dedicated original-pixel control really removes both rail caps at full size");
            if(variant==3)require(saved==0,"uncertified rotation is an exact complete-geometry fallback");
            size_t different=0,first_difference=0;for(size_t i=0;i<a.pixels.size();++i)if(a.pixels[i]!=b.pixels[i]){if(!different)first_difference=i;++different;}
            const bool equal=!different&&a.focus==b.focus&&a.target==b.target;all_equal&=equal;total_different+=different;++pairs;
            if(!first)report<<",";first=false;report<<"{\"variant\":"<<variant<<",\"view\":"<<n<<",\"saved_triangles\":"<<saved<<",\"pixels_equal\":"<<(different?"false":"true")<<",\"different_pixels\":"<<different<<",\"focus_equal\":"<<(a.focus==b.focus?"true":"false")<<",\"target_equal\":"<<(a.target==b.target?"true":"false")<<"}";
            if(!equal)std::fprintf(stderr,"Frame cap pixel mismatch variant=%u view=%u pixels=%zu first=%zu before=%08x after=%08x\n",variant,n,different,first_difference,a.pixels[first_difference],b.pixels[first_difference]);
        }
    }
    require(pairs==50&&active>0&&fallback>0,"all full-image views exercise active joins, missing/hidden parts, rotation and inside fallback");
    report<<"],\"pair_count\":"<<pairs<<",\"active_pairs\":"<<active<<",\"fallback_pairs\":"<<fallback<<",\"different_pixels\":"<<total_different<<",\"success\":"<<(all_equal?"true":"false")<<"}\n";report.close();require(bool(report),"complete device control receipt");
    engine.clear();SDL_SetRenderTarget(renderer,nullptr);SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
    require(all_equal,"every current full-image color pixel and target must equal its complete-geometry control");
    std::printf("frame cap device: %u full 1280x800 pairs, %u active, %u fallback, every pixel and target exact\n",pairs,active,fallback);
}
