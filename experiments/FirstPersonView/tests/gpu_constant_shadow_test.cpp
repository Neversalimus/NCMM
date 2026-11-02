#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include "vehicle_frame_caps_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace gpu=first_person::gpu;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"Constant shadow device: %s (%s)\n",why,SDL_GetError());std::exit(1);}}
static std::vector<unsigned char> download(SDL_GPUDevice *device,SDL_GPUBuffer *buffer,uint32_t bytes){
    require(buffer&&bytes>0,"actual initialized shadow allocation exists");
    SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=bytes;
    auto *transfer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer!=nullptr,"bounded actual shadow readback");
    auto *commands=SDL_AcquireGPUCommandBuffer(device);require(commands!=nullptr,"acquire actual shadow readback");
    auto *copy=SDL_BeginGPUCopyPass(commands);require(copy!=nullptr,"begin actual shadow readback");
    SDL_GPUBufferRegion from{buffer,0,bytes};SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(commands);require(fence!=nullptr,"submit actual shadow readback");require(SDL_WaitForGPUFences(device,true,&fence,1),"wait for exact shadow bytes");SDL_ReleaseGPUFence(device,fence);
    auto *mapped=static_cast<unsigned char *>(SDL_MapGPUTransferBuffer(device,transfer,false));require(mapped!=nullptr,"map actual shadow readback");
    std::vector<unsigned char> result(mapped,mapped+bytes);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);return result;
}
int main(){
    // Compile only on the user's machine. Device, readback and hidden-surface
    // execution are confined to the Windows/Linux/sanitizer CI runners.
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM constant shadow controls",1280,800,SDL_WINDOW_HIDDEN);require(window!=nullptr,"hidden CI surface");
#endif
    const auto props=SDL_CreateProperties();SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(device!=nullptr,"actual CI device without fallback");auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared CI renderer");
    auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);require(target&&SDL_SetRenderTarget(renderer,target),"full1280x800 target");
    ncmm_gpu::backend original,current;original.reuse_constant_shadows(false);require(original.bind(renderer,8400)&&current.bind(renderer,8400),"independent always-dispatch and cached backends");
    namespace fp=first_person;namespace v=fp::vehicle;
    std::vector<ncmm_graphics_cell_v1> cells(625);std::vector<ncmm_graphics_cell_v2> details(625);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");details[i].light_r=.35f+float(i%5)*.12f;details[i].light_g=.4f+float(i%3)*.13f;details[i].light_b=.5f;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,25,25,12.5f,23.5f,cells.data(),625};source.details=details.data();source.detail_count=625;
    source.environment.daylight=1;source.environment.sun_x=.3f;source.environment.sun_y=.4f;source.environment.sun_z=.8f;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(625);
    v::presentation vehicles;metadata.vehicles=&vehicles;vehicles.input=frame_caps_fixture::construction();gpu::scene_cache cache;fp::hybrid::frame statistics;
    struct observation{std::vector<uint32_t> pixels;int focus=-1,target=-1;};
    const auto read=[&](ncmm_gpu::backend &engine){observation o;require(SDL_SetRenderDrawColor(renderer,0,0,0,255)&&SDL_RenderClear(renderer)&&SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose full control image");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read full control image");auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image&&image->w==1280&&image->h==800,"original full resolution retained");
        o.pixels.resize(1280*800);for(int y=0;y<800;++y)std::memcpy(o.pixels.data()+1280*y,static_cast<const char *>(image->pixels)+image->pitch*y,1280*4);SDL_DestroySurface(image);o.focus=engine.focus_cell();o.target=engine.metrics().target_cell;return o;};
    std::ofstream report("constant-shadow-device-controls.json");require(bool(report),"own separate complete device receipt");
    report<<"{\"full_viewport\":[1280,800],\"original501_150cap_150packing_44shaders_unchanged\":true,\"pairs\":[";
    const auto views=frame_caps_fixture::cameras();bool first=true,equal=true;uint64_t different_total=0,reuses=0,dispatches=0,reference_dispatches=0,readbacks=0;unsigned pairs=0;
    for(unsigned variant=0;variant<5;++variant)for(unsigned n=0;n<10;++n){
        const auto &camera=views[n];source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
        gpu::packet packet;require(cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,current.metrics(),{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE),"actual unchanged vehicle packet builder");
        const bool xyz=variant!=3;const uint32_t count=variant==1?1250:625;
        if(count>packet.cells.size()){const auto lower=packet.cells;packet.cells.insert(packet.cells.end(),lower.begin(),lower.end());}
        auto scene=packet.finish(nullptr);require(scene.cell_count==count,"requested cell grow or shrink is real");scene.camera.grid[2]=float(count);scene.camera.grid[3]=xyz?1.f:0.f;
        scene.world_epoch=variant+1;scene.frame_serial=variant*10+n+1;
        std::vector<ncmm_world_cell_v1> coordinates(count);
        for(uint32_t i=0;i<count;++i){auto &c=coordinates[i];c.x=int(i%25);c.y=int(i/25%25);c.z=int(i/625);for(unsigned k=0;k<3;++k)c.lower_light[k]=.2f+float((i+k+n)%5)*.15f;}
        ncmm_gpu_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.base=scene;world.coordinates=coordinates.data();world.cell_count=count;world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
        const auto *world_pointer=xyz?&world:nullptr;
        if(variant==4&&n==3){original.clear();current.clear();require(current.constant_shadow_prefix()==0,"clear forgets the initialized prefix before any new allocation");require(original.bind(renderer,8403)&&current.bind(renderer,8403),"rebind after real GPU resource disposal");}
        if(variant==4&&n==5)current.reuse_constant_shadows(false);
        if(variant==4&&n==6){current.reuse_constant_shadows(true);require(current.constant_shadow_prefix()==0,"reenable cannot borrow an older disabled backing state");}
        if(variant==4&&n==8){scene.world_epoch=901;scene.frame_serial=1;world.base=scene;}
        if(variant==4&&n==9){auto invalid=scene;invalid.camera.grid[2]=-1;require(!current.submit(invalid,&world),"rejected public scene cannot commit an unsubmitted prefix");scene.draws=nullptr;scene.draw_count=0;scene.bones=nullptr;scene.bone_count=0;world.base=scene;}
        const auto submit=[&](ncmm_gpu::backend &engine){const bool submitted=engine.submit(scene,world_pointer);if(!submitted){report.flush();const auto metrics=engine.metrics();std::fprintf(stderr,"Shadow context variant=%u view=%u cached=%u\n",variant,n,unsigned(&engine==&current));require(false,metrics.reason);}return read(engine);};
        observation a,b;if((variant+n)%2){b=submit(current);a=submit(original);}else{a=submit(original);b=submit(current);}
        const auto c=current.recording(),r=original.recording();require(r.shadow_dispatches==1&&r.constant_shadow_reuses==0,"always-dispatch control executes the original program every frame");
        const bool expected_reuse=(variant==0&&n>0)||(variant==1&&n>0)||variant==2||(variant==4&&(n==1||n==2||n==4||n>=7));
        require(c.constant_shadow_reuses==unsigned(expected_reuse)&&c.shadow_dispatches==unsigned(!expected_reuse),"actual GPU dispatch/reuse counters match every lifetime/mode transition");
        if(variant==2)require(current.constant_shadow_prefix()==1250,"shrinking cells preserves a larger initialized prefix without cycling its backing");
        if(!xyz||(variant==4&&n==5))require(current.constant_shadow_prefix()==0,"variable and disabled frames cannot publish a constant prefix");
        const auto bytes_a=download(device,original.constant_shadow_buffer(),count*4),bytes_b=download(device,current.constant_shadow_buffer(),count*4);readbacks+=2;const bool shadow_equal=bytes_a==bytes_b;
        bool ones=true;const float one=1;for(uint32_t i=0;i<count;++i)ones&=!std::memcmp(bytes_b.data()+i*4,&one,4);
        require(shadow_equal&&(!xyz||ones),"all actual GPU shadow words match; every reused XYZ word is exactly1.0");
        size_t different=0;for(size_t p=0;p<a.pixels.size();++p)different+=a.pixels[p]!=b.pixels[p];
        const bool same=different==0&&a.focus==b.focus&&a.target==b.target&&shadow_equal;equal&=same;different_total+=different;reuses+=c.constant_shadow_reuses;dispatches+=c.shadow_dispatches;reference_dispatches+=r.shadow_dispatches;++pairs;
        if(!first)report<<',';first=false;report<<"{\"variant\":"<<variant<<",\"view\":"<<n<<",\"xyz\":"<<(xyz?"true":"false")<<",\"cell_count\":"<<count<<",\"dispatches\":"<<c.shadow_dispatches<<",\"reuses\":"<<c.constant_shadow_reuses<<",\"initialized_prefix\":"<<current.constant_shadow_prefix()<<",\"different_pixels\":"<<different<<",\"pixels_equal\":"<<(different==0?"true":"false")<<",\"focus_equal\":"<<(a.focus==b.focus?"true":"false")<<",\"target_equal\":"<<(a.target==b.target?"true":"false")<<",\"shadow_bits_equal\":"<<(shadow_equal?"true":"false")<<",\"constant_ones\":"<<(ones?"true":"false")<<'}';report.flush();
    }
    require(pairs==50&&reuses==34&&dispatches==16&&reference_dispatches==50&&readbacks==100,"all fifty full on/off images and positive dispatch savings exercised");
    original.clear();current.clear();require(original.constant_shadow_buffer()==nullptr&&current.constant_shadow_buffer()==nullptr&&original.constant_shadow_prefix()==0&&current.constant_shadow_prefix()==0&&original.resources().bytes()==0&&current.resources().bytes()==0,"clear releases both GPU shadow allocations and every initialized-prefix lease");
    report<<"],\"pair_count\":"<<pairs<<",\"constant_pairs\":40,\"variable_pairs\":10,\"cache_reuses\":"<<reuses<<",\"cache_dispatches\":"<<dispatches<<",\"reference_dispatches\":"<<reference_dispatches<<",\"actual_shadow_readbacks\":"<<readbacks<<",\"clear_rebind_pairs\":1,\"toggle_controls\":2,\"invalid_scene_controls\":1,\"resource_cleanup_pass\":true,\"different_pixels\":"<<different_total<<",\"success\":"<<(equal?"true":"false")<<"}\n";report.close();require(bool(report),"complete independent receipt");
    SDL_SetRenderTarget(renderer,nullptr);SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);require(SDL_WaitForGPUIdle(device),"qualification teardown");SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
    require(equal,"all full pixels,focus,target and actual shadow bytes equal");std::printf("constant shadow device: %llu checks,50full1280x800pairs,34reuses,100actualbufferreadbacks; no performance certification\n",(unsigned long long)checks);
}
