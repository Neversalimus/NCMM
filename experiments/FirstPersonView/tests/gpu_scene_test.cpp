#include "ncmm_graphics_performance.hpp"
#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include "world_visual_fixture.hpp"
#include "opaque_depth_fixture.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc,char **argv){
    auto require=[](bool ok,const char *why){if(!ok){std::fprintf(stderr,"Full GPU scene failed: %s (%s)\n",why,SDL_GetError());std::exit(1);}};
    // Exact affine AABB agrees with the eight-corner reference for reflection,
    // shear and nonuniform scale; outward rounding preserves every corner.
    {
        uint32_t seed=7319;auto random=[&](){seed=seed*1664525u+1013904223u;return float(int(seed%20001)-10000)/317.f;};
        for(int sample=0;sample<4000;++sample){ncmm_gpu::bounds local;
            for(int axis=0;axis<3;++axis){local.lo[axis]=random();local.hi[axis]=local.lo[axis]+std::abs(random());}
            ncmm_gpu_matrix_v1 matrix{};for(int column=0;column<4;++column)for(int row=0;row<3;++row)matrix.columns[column*4+row]=random();matrix.columns[15]=1;
            const auto fast=ncmm_gpu::transform_bounds(local,matrix);ncmm_gpu::bounds reference;
            for(int corner=0;corner<8;++corner){ncmm_gpu::point p{corner&1?local.hi[0]:local.lo[0],corner&2?local.hi[1]:local.lo[1],corner&4?local.hi[2]:local.lo[2]};
                p=ncmm_gpu::transform(matrix,p);float v[]={p.x,p.y,p.z};reference.include(v);}
            for(int axis=0;axis<3;++axis)require(fast.lo[axis]<=reference.lo[axis]&&fast.hi[axis]>=reference.hi[axis]&&
                reference.lo[axis]-fast.lo[axis]<.02f&&fast.hi[axis]-reference.hi[axis]<.02f,"affine bounds match corner reference conservatively");
        }
    }
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM GPU scene/skin qualification",640,480,SDL_WINDOW_HIDDEN);require(window!=nullptr,"native window");
#endif
    SDL_PropertiesID gpu_properties=SDL_CreateProperties();
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);
    // Match SDL's own renderer-created device, rather than a more permissive
    // standalone GPU device that hid unsupported pipeline requirements.
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(gpu_properties,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(gpu_properties);SDL_DestroyProperties(gpu_properties);require(device!=nullptr,"real GPU API device; fallback prohibited");
    auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared SDL renderer");
    auto *ui=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,640,480);require(ui&&SDL_SetRenderTarget(renderer,ui),"SDL UI target");
    // An owned signalled fence remains valid while command buffers are reused.
    // The initial fixture wait is not part of the ordinary renderer path.
    {
        auto *device=SDL_GetGPURendererDevice(renderer);
        auto *commands=SDL_AcquireGPUCommandBuffer(device);require(commands!=nullptr,"acquire fence lifetime fixture");
        auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(commands);require(fence!=nullptr,"own fixture fence");
        require(SDL_WaitForGPUFences(device,true,&fence,1)&&SDL_QueryGPUFence(device,fence),"fixture fence completes");
        for(int reuse=0;reuse<64;++reuse){
            commands=SDL_AcquireGPUCommandBuffer(device);require(commands&&SDL_SubmitGPUCommandBuffer(commands),"reuse command-buffer pool independently");
            require(SDL_QueryGPUFence(device,fence)&&SDL_WaitForGPUFences(device,true,&fence,1)&&SDL_QueryGPUFence(device,fence),
                "owned completed fence survives repeated SDK cleanup and command-buffer reuse");
        }
        SDL_ReleaseGPUFence(device,fence);
    }
    ncmm_gpu::backend engine;require(engine.bind(renderer,1),"borrow existing device");engine.profile();
    {
        // Warm Legacy chunks must be replaced on a Modular toggle. The part
        // mesh is instanced separately, so retaining Legacy here draws both.
        namespace fp=first_person;constexpr unsigned side=16,count=side*side,at=8*side+8;
        std::array<ncmm_graphics_cell_v1,count> cells{};std::array<ncmm_graphics_cell_v2,count> details{};
        for(unsigned i=0;i<count;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_pavement");
            std::strcpy(details[i].ground,"t_pavement");details[i].light_r=details[i].light_g=details[i].light_b=1;}
        details[at].vehicle_features=NCMM_VEH_FRAME|NCMM_VEH_BOARD;details[at].vehicle_group=1;details[at].vehicle_x=details[at].vehicle_y=8.5f;details[at].vehicle_health=1;
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,side,side,8.5f,12.5f,cells.data(),count};
        source.details=details.data();source.detail_count=count;source.environment.daylight=1;source.environment.sun_z=1;
        fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(count);
        fp::vehicle::presentation vehicle;vehicle.input.enabled=true;vehicle.input.scene.world_epoch=1;vehicle.input.scene.revision=1;vehicle.input.scene.level_height=3.2f;
        ncmm_graphics_vehicle_v1 car{};car.entity_id=7;car.structure_revision=car.state_revision=1;car.legacy_group=1;car.part_count=1;car.origin_x=car.origin_y=8.5f;vehicle.input.vehicles.push_back(car);
        ncmm_graphics_vehicle_part_v1 part{};part.part_id=9;part.cell_index=at;part.features=NCMM_VP_FRAME|NCMM_VP_FLOOR;part.state=NCMM_VP_EXISTS;part.health=1;
        std::strcpy(part.id,"frame");part.color[0]=part.color[1]=part.color[2]=.4f;part.color[3]=1;vehicle.input.parts.push_back(part);vehicle.observe({8.5f,12.5f,1.55f});
        fp::gpu::scene_cache cache;fp::hybrid::frame statistics;fp::gpu::packet legacy,modular,warm,restored;
        auto build=[&](fp::gpu::packet &packet){require(cache.build(source,-fp::hybrid::pi*.5f,-.1f,{},packet,statistics,engine.metrics(),{},&metadata,nullptr,640,480,NCMM_GPU_FORCE),cache.last_reason.c_str());};
        build(legacy);metadata.vehicles=&vehicle;build(modular);require(cache.generated_cells>0,"Legacy to Modular invalidates static vehicle chunks");
        const auto scans=cache.vehicle_pass_scans;require(scans==2,"opaque component checks its present and absent passes once");
        build(warm);require(cache.generated_cells==0,"unchanged Modular retains warm terrain chunks");
        require(cache.vehicle_pass_scans==scans,"warm opaque component neither scans nor allocates its absent transparent pass");
        vehicle.input.vehicles[0].origin_x+=.1f;vehicle.input.scene.seconds+=.1;vehicle.observe({8.5f,12.5f,1.55f});
        fp::gpu::packet moving;build(moving);require(cache.generated_cells==0,"instance motion does not rebuild static terrain");
        require(cache.vehicle_pass_scans==scans,"rigid motion reuses both positive and negative component pass results");
        vehicle.input.parts[0].features=NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD;
        ++vehicle.input.vehicles[0].structure_revision;++vehicle.input.vehicles[0].state_revision;
        ++vehicle.input.scene.revision;vehicle.observe({8.5f,12.5f,1.55f});
        fp::gpu::packet glass,glass_warm;build(glass);
        require(cache.vehicle_pass_scans>scans,"changed component recipe cannot inherit an old negative pass result");
        require(std::any_of(glass.draws.begin(),glass.draws.end(),[](const auto &d){return d.pass==NCMM_GPU_TRANSPARENT;}),
            "real glazing retains its transparent GPU pass after an opaque recipe");
        const auto glass_scans=cache.vehicle_pass_scans;build(glass_warm);
        require(cache.vehicle_pass_scans==glass_scans,"warm mixed/glass recipe also avoids repeated pass scans");
        metadata.vehicles=nullptr;build(restored);require(cache.generated_cells>0,"Modular to Legacy restores static vehicle geometry");
    }
    ncmm_gpu_scene_v1 scene{};scene.struct_size=sizeof(scene);scene.width=640;scene.height=480;scene.mode=NCMM_GPU_FORCE;scene.frame_serial=1;
    auto &camera=scene.camera;camera.right[0]=1;camera.up[2]=1;camera.forward[1]=1;camera.projection[0]=640;camera.projection[1]=480;camera.projection[2]=457;camera.projection[3]=512;
    camera.sun[2]=1;camera.sun[3]=1;camera.sky_top[2]=.4f;camera.sky_bottom[0]=.3f;camera.sky_bottom[1]=.4f;camera.sky_bottom[2]=.5f;camera.grid[0]=camera.grid[1]=camera.grid[2]=1;
    ncmm_gpu_cell_v1 cell{};cell.flags=1;cell.light[0]=cell.light[1]=cell.light[2]=1;scene.cells=&cell;scene.cell_count=1;
    std::array<ncmm_gpu_vertex_v1,6> vertices{};for(int i=0;i<6;++i){float z=i<3?2.f:4.f;int corner=i%3;auto &v=vertices[i];
        v.position[0]=(corner==0?-1.f:corner==1?1.f:0.f)*z;v.position[1]=z;v.position[2]=(corner==2?1.f:-1.f)*z;
        v.normal[1]=v.face_normal[1]=-1;v.material=i<3?0:1;v.joint=i<3?1:0;}
    const uint32_t indices[]={0,1,2,3,4,5};ncmm_gpu_material_v1 materials[2]{};materials[0].color[0]=.9f;materials[0].color[3]=1;materials[0].flags=NCMM_GPU_CREATURE|NCMM_GPU_CREATURE_VISUAL;
    materials[1].color[1]=.9f;materials[1].color[3]=1;
    ncmm_gpu_mesh_v1 mesh{11,vertices.data(),indices,materials,6,6,2,0};scene.meshes=&mesh;scene.mesh_count=1;
    ncmm_gpu_matrix_v1 bones[2]{};for(auto &b:bones)for(int i=0;i<4;++i)b.columns[i*5]=1;scene.bones=bones;scene.bone_count=2;
    ncmm_gpu_draw_v1 draw{};draw.mesh_key=11;draw.transform=bones[0];draw.bone_count=2;scene.draws=&draw;scene.draw_count=1;
    // A Native SDL pass may already be queued on the borrowed renderer. Its
    // target and pixels must survive early submission and the independent 3D
    // commands before the final GPU-to-GPU scene composition.
    SDL_FRect native_background{0,0,640,480};
    require(SDL_SetRenderDrawColor(renderer,23,117,211,255)&&SDL_RenderFillRect(renderer,&native_background),"queue Native 2D before scene preparation");
    require(engine.submit(scene),engine.metrics().reason);auto first=engine.metrics();require(first.active&&first.triangles==2&&first.cpu_raster_calls==0,"GPU triangles, hardware depth, CPU raster excluded");
    {
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"Native 2D survives scene submission");
        auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"Native 2D reference format");
        for(const auto position:std::array<std::array<int,2>,2>{{{{3,3}},{{636,476}}}}){
            const auto pixel=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+position[1]*image->pitch)[position[0]];
            require((pixel>>24)==23&&((pixel>>16)&255)==117&&((pixel>>8)&255)==211,"queued Native 2D target and complete colour are preserved");
        }
        SDL_DestroySurface(image);
    }
    require(engine.has(11,0),"persistent GPU mesh resource");require(first.target_cell==0,"geometric picking matches visible near creature");
    require(engine.resources().static_upload_bytes()>0,"initial immutable mesh upload is accounted separately");
    const auto first_static_upload=engine.resources().static_upload_bytes();
    require(engine.recording().static_upload_bytes==engine.resources().static_upload_bytes(),"private static upload counters report the current frame");
    const auto staging_capacity=engine.recording().frame_upload_capacity_bytes;
    const auto staging_creations=engine.recording().frame_upload_creations;
    require(staging_capacity>0&&staging_creations==1&&first.resource_bytes+staging_capacity+engine.recording().cell_cache_bytes<=NCMM_GPU_RESOURCE_LIMIT,"frame staging and source-cell snapshot have an explicit bounded capacity");
    require(engine.recording().cell_upload_bytes==112&&engine.recording().cell_cache_bytes>=112,"first frame uploads the complete owned source-cell layout");
    auto capture=[&](const char *suffix){require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"GPU-to-GPU final composition");
        SDL_FRect strip{0,0,640,20};require(SDL_SetRenderDrawColor(renderer,0,0,255,255)&&SDL_RenderFillRect(renderer,&strip),"ordinary SDL UI");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"test-only readback");auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"capture format");
        if(argc>1)require(SDL_SaveBMP(image,(std::string(argv[1])+suffix+".bmp").c_str()),"save real GPU scene");
        auto center=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+240*image->pitch)[320];
        auto top=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+10*image->pitch)[320];require(((top>>8)&255)>240,"SDL UI preserved");SDL_DestroySurface(image);return center;};
    auto center=capture("-near");require((center>>24)>120&&((center>>16)&255)<20,"near red wins over later far green in full pipeline");
    require(first.draw_calls==4,"dry scene bypasses empty volume and post-composition passes");
    auto completed=engine.metrics();require(completed.completed_frame_serial>=1&&completed.visible_creature_pixels>1000,"fenced small GPU visibility counter");
    require(engine.recording().retirement_calls==0,"initial frame has no completed resources to retire");
    for(int i=0;i<8;++i)require(engine.metrics().completed_frame_serial>=1&&engine.recording().retirement_calls==0,
        "asynchronous metrics never retire resources outside a submission");
    scene.mesh_count=0;scene.meshes=nullptr;scene.frame_serial=2;bones[1].columns[12]=5;
    require(engine.submit(scene),engine.metrics().reason);auto second=engine.metrics();require(second.active&&second.generated_meshes==0&&second.upload_bytes<1024,"only dynamic data uploaded for skeletal animation");
    require(engine.recording().retirement_calls==1,"next submission retires completed resources once");
    for(int i=0;i<8;++i){engine.metrics();require(engine.recording().retirement_calls==1,"metrics cannot hide more retirement work");}
    require(engine.resources().static_upload_bytes()==0,"warm skeletal animation uploads no immutable resources");
    require(second.upload_bytes+116==first.upload_bytes-first_static_upload,"unchanged cells and neutral texture are not uploaded for bone animation");
    require(engine.recording().cell_upload_bytes==0,"warm bone updates retain the unchanged source-cell buffer");
    require(engine.recording().static_upload_bytes==0,"private warm upload counters cannot underflow from the previous frame");
    require(engine.recording().frame_upload_capacity_bytes==staging_capacity&&engine.recording().frame_upload_creations==staging_creations,"bone updates reuse the same staging object");
    require(second.target_cell==-1,"bone animation updates exact picking without CPU pixel raster");center=capture("-skin");require(((center>>16)&255)>120&&(center>>24)<20,"GPU vertex skinning moves near mesh; far green becomes visible");
    require(engine.metrics().completed_frame_serial>=2,"animated frame fence completes without a main-path wait");
    // Queue distinct snapshots without any read-pixels/fence wait between them.
    // A non-cycling mapped buffer can overwrite poses still awaiting GPU copies.
    for(int frame=0;frame<16;++frame){
        scene.frame_serial=100+frame;bones[1].columns[12]=frame%2?5.f:0.f;
        require(engine.submit(scene),engine.metrics().reason);
        require(engine.metrics().upload_bytes==second.upload_bytes,"queued poses preserve the immutable neutral texture");
        require(engine.recording().frame_upload_creations==staging_creations,"queued poses retain one staging object");
        require(engine.recording().retirement_calls==1,"retained completed fence permits one cleanup even when newer readbacks are pending");
        SDL_FRect slot{float(frame%4*160),float(frame/4*120),160,120};
        require(SDL_RenderTexture(renderer,engine.output(),nullptr,&slot),"queue a distinct pose snapshot");
    }
    {
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read all queued poses once");
        auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"queued pose format");
        for(int frame=0;frame<16;++frame){const auto pixel=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+(frame/4*120+60)*image->pitch)[frame%4*160+80];
            require(frame%2?((pixel>>16)&255)>120&&(pixel>>24)<20:(pixel>>24)>120&&((pixel>>16)&255)<20,"each queued frame retains its own bone upload");}
        SDL_DestroySurface(image);
    }
    {
        // Cell data may change independently of geometry or bone palettes.
        const auto saved_cell=cell;bones[1].columns[12]=0;scene.frame_serial=200;
        require(engine.submit(scene)&&engine.recording().cell_upload_bytes==0,"unchanged cells remain resident while a pose changes");
        const auto bright=capture("-cell-bright");
        cell.light[0]=cell.light[1]=cell.light[2]=.15f;++scene.frame_serial;
        require(engine.submit(scene)&&engine.recording().cell_upload_bytes==112,"native lighting changes upload the complete source-cell record");
        const auto dark=capture("-cell-dark");
        require((bright>>24)>(dark>>24)+50,"changed source-cell lighting reaches actual GPU pixels");
        ++scene.frame_serial;require(engine.submit(scene)&&engine.recording().cell_upload_bytes==0,"unchanged dark cells do not retransmit");
        for(int frame=0;frame<4;++frame){cell.light[0]=cell.light[1]=cell.light[2]=frame%2?.15f:1.f;++scene.frame_serial;
            require(engine.submit(scene)&&engine.recording().cell_upload_bytes==112,"each changed queued cell snapshot is uploaded");
            SDL_FRect slot{float(frame%2*320),float(frame/2*240),320,240};
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,&slot),"queue distinct native lighting snapshots");}
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read queued source-cell snapshots once");
        auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"queued source-cell pixel format");
        for(int frame=0;frame<4;++frame){const auto pixel=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+(frame/2*240+120)*image->pitch)[frame%2*320+160];
            const auto expected=frame%2?dark:bright;
            require(std::abs(int(pixel>>24)-int(expected>>24))<=4,"each queued frame retains its own native cell lighting");}
        SDL_DestroySurface(image);cell=saved_cell;
    }
    scene.mode=NCMM_GPU_CPU;require(!engine.submit(scene)&&!engine.metrics().active,"explicit CPU selection never impersonates GPU");
    scene.mode=NCMM_GPU_FORCE;scene.frame_serial=3;bones[1].columns[12]=0;require(engine.submit(scene),engine.metrics().reason);require(engine.metrics().generated_meshes==0,"GPU resumes with cached geometry");
    require(engine.recording().cell_upload_bytes==112,"resuming GPU applies lighting changed after the previous queued frame");
    std::array<ncmm_gpu_vertex_v1,3> glass_vertices{};for(int i=0;i<3;++i){glass_vertices[i]=vertices[i];glass_vertices[i].position[0]*=.5f;glass_vertices[i].position[1]=1;glass_vertices[i].position[2]*=.5f;glass_vertices[i].joint=0;glass_vertices[i].material=0;}
    ncmm_gpu_material_v1 glass{};glass.color[2]=.9f;glass.color[3]=.3f;const uint32_t glass_indices[]={0,1,2};
    ncmm_gpu_mesh_v1 dynamic{12,glass_vertices.data(),glass_indices,&glass,3,3,1,NCMM_GPU_DYNAMIC_MESH};
    ncmm_gpu_draw_v1 draws[2]={draw,draw};draws[1].mesh_key=12;draws[1].bone_count=1;draws[1].pass=NCMM_GPU_TRANSPARENT;
    scene.meshes=&dynamic;scene.mesh_count=1;scene.draws=draws;scene.draw_count=2;scene.frame_serial=4;
    require(engine.submit(scene),engine.metrics().reason);require(!engine.has(12,0),"dynamic mesh explicitly requests per-frame data");
    center=capture("-glass");require((center>>24)>80&&(center>>24)<135&&((center>>8)&255)>30&&((center>>8)&255)<75,"transparent glass blends over opaque creature with depth preserved");
    require(engine.metrics().target_cell==0,"transparent glass does not erase target ownership");
    for(auto &vertex:glass_vertices)vertex.position[0]+=5;scene.frame_serial=5;require(engine.submit(scene),engine.metrics().reason);
    auto dynamic_stats=engine.metrics();require(dynamic_stats.dynamic_mesh_updates==1&&dynamic_stats.generated_meshes==0,"dynamic buffer cycles without recreating world geometry");
    require(engine.resources().static_upload_bytes()==0,"explicit dynamic mesh updates are not static uploads");
    require(engine.recording().dynamic_geometry_bytes==sizeof(glass_vertices)+sizeof(glass_indices),"private dynamic upload classification uses the existing mesh contract");
    center=capture("-dynamic");require((center>>24)>120&&((center>>8)&255)<20,"updated dynamic buffer moves transparent geometry");
    for(auto &vertex:glass_vertices)vertex.position[0]-=5;scene.frame_serial=6;require(engine.submit(scene),engine.metrics().reason);
    center=capture("-return");require((center>>24)>80&&(center>>24)<135&&((center>>8)&255)>30,"current dynamic bounds restore geometry entering the frustum");
    auto evict=scene;evict.mesh_count=evict.draw_count=0;ncmm_gpu_metrics_v1 eviction{};
    require(engine.resources().prepare(evict,1,eviction)&&engine.resources().bytes()==0&&!engine.has(11,0),"shrinking viewport cache allowance evicts unused warm resources");
    std::array<ncmm_gpu_mesh_v1,2> restore={mesh,dynamic};scene.meshes=restore.data();scene.mesh_count=2;scene.frame_serial=7;
    require(engine.submit(scene)&&engine.metrics().generated_meshes==2&&engine.resources().static_upload_bytes()>0,"bounded cache eviction restores and accounts exact static resources");
    scene.width=1280;scene.height=800;scene.camera.projection[0]=1280;scene.camera.projection[1]=800;scene.camera.projection[2]=914;scene.frame_serial=8;scene.mesh_count=0;
    // Dynamic meshes are still supplied after resizing; immutable world stays cached.
    scene.meshes=&dynamic;scene.mesh_count=1;
    require(engine.submit(scene)&&engine.metrics().generated_meshes==0&&engine.metrics().resource_bytes<=NCMM_GPU_RESOURCE_LIMIT,"viewport resize preserves world cache and allocation budget");
    const auto resized_upload=engine.metrics().upload_bytes;++scene.frame_serial;
    require(engine.submit(scene)&&engine.metrics().upload_bytes+4==resized_upload,"resized target initializes its neutral texture once");
    {
        // A valid frame-only transfer can exceed the old native smoke's 2 MiB
        // threshold without rebuilding or uploading any immutable geometry.
        std::vector<ncmm_gpu_matrix_v1> palette(NCMM_GPU_MAX_BONES,first_person::gpu::identity());
        auto large=scene;large.width=640;large.height=480;large.camera.projection[0]=640;large.camera.projection[1]=480;large.camera.projection[2]=457;
        large.meshes=nullptr;large.mesh_count=0;large.draws=&draw;large.draw_count=1;
        large.bones=palette.data();large.bone_count=uint32_t(palette.size());large.frame_serial=900;
        uint64_t large_capacity=0,large_creations=0;
        for(int frame=0;frame<2;++frame){++large.frame_serial;require(engine.submit(large),engine.metrics().reason);
            require(engine.metrics().upload_bytes>2*1024*1024&&engine.metrics().generated_meshes==0&&engine.resources().static_upload_bytes()==0,
                    "large dynamic transfer preserves the strict zero static upload gate");
            const auto &recording=engine.recording();require(engine.metrics().resource_bytes+recording.frame_upload_capacity_bytes+recording.cell_cache_bytes<=NCMM_GPU_RESOURCE_LIMIT,"large retained staging and cell snapshot respect the combined resource budget");
            if(!frame){large_capacity=recording.frame_upload_capacity_bytes;large_creations=recording.frame_upload_creations;
                require(large_capacity>staging_capacity&&large_creations>staging_creations,"large palettes grow staging capacity");}
            else require(recording.frame_upload_capacity_bytes==large_capacity&&recording.frame_upload_creations==large_creations,"repeated large palettes reuse staging storage");}
        // Complete this fixture before the next producer restarts its serial;
        // old-generation readbacks must not occupy all slots for its first frame.
        capture("-large-frame-data");
    }
    // Exercise the actual collector's reconstructed creature meshes on SDL_GPU,
    // rather than accepting the synthetic red/green triangle probe as proof.
    {
        namespace fp=first_person;namespace cr=first_person::creature;
        constexpr int side=9,creature_cell=4*side+4;
        std::vector<ncmm_graphics_cell_v1> ground(side*side);std::vector<ncmm_graphics_cell_v2> details(side*side);
        for(int i=0;i<side*side;++i){ground[i].flags=NCMM_GFX_VISIBLE;ground[i].light=1;std::strcpy(ground[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
        std::strcpy(ground[creature_cell].creature,"mod_arbitrary_creature");
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,side,side,4.5f,7.5f,ground.data(),ground.size()};
        source.details=details.data();source.detail_count=details.size();source.environment.daylight=source.environment.sun_z=1;source.environment.animation_seconds=10;
        std::array<uint32_t,16*32> art{};for(int y=1;y<31;++y)for(int x=3;x<13;++x)art[y*16+x]=y<8?0xbb8877ff:y<20?0x315e91ff:0x60522aff;
        const uint32_t floor=0x887766ff;
        fp::hybrid::sprite_provider provider=[&](uint32_t,uint32_t layer,ncmm_graphics_sprite_v2 &sprite){
            if(layer==NCMM_GFX_HELD_ITEM)return false;
            sprite=layer==NCMM_GFX_CREATURE?ncmm_graphics_sprite_v2{sizeof(sprite),16,32,16,art.data()}:ncmm_graphics_sprite_v2{sizeof(sprite),1,1,1,&floor};return true;};
        cr::presentation entities;entities.input.view={sizeof(ncmm_graphics_creature_scene_v1),1,1,1,10};
        fp::gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        uint64_t entity_id=0;double maximum_error=0;
        for(const char *body:{"human","dog","spider","insect","centipede","bird","lizard","snake","fish","blob","robot","migo","unknown_mod_body"}){
            ncmm_graphics_creature_v1 entity{};entity.entity_id=++entity_id;entity.cell=creature_cell;entity.world_x=entity.world_y=4;entity.liters=70;entity.kilograms=65;
            std::strcpy(entity.bodytype,body);std::strcpy(entity.type_id,ground[creature_cell].creature);
            if(!std::strcmp(body,"centipede"))std::strcpy(entity.species,"CENTIPEDE");
            if(!std::strcmp(body,"robot"))entity.flags=NCMM_CR_MECHANICAL|NCMM_CR_IMMOBILE;
            entities.input.creatures.resize(1);entities.input.creatures[0]=entity;entities.observe();
            entities.resources.request(cr::appearance_key(entity,1),cr::classify(entity),[&]{ncmm_graphics_sprite_v2 sprite{sizeof(sprite),16,32,16,art.data()};return cr::sprite::copy(sprite);});
            require(entities.resources.wait_idle(std::chrono::seconds(5)),"generated anatomy ready for real GPU qualification");
            for(int heading=0;heading<8;++heading){entities.input.creatures[0].heading=heading*fp::hybrid::pi/4;entities.observe();
                fp::gpu::packet packet;fp::hybrid::frame statistics,reference;
                require(cache.build(source,-fp::hybrid::pi*.5f,0,provider,packet,statistics,host,{},nullptr,&entities,640,480,NCMM_GPU_FORCE),"actual procedural collector produces complete GPU scene");
                require(engine.submit(packet.finish(nullptr)),engine.metrics().reason);
                require(statistics.pixels.empty()&&statistics.depth.empty()&&!engine.metrics().cpu_raster_calls,"procedural GPU path excludes CPU frame rasterization");
                require(fp::hybrid::build_frame(source,-fp::hybrid::pi*.5f,0,provider,reference,640,480,{},nullptr,nullptr,nullptr,{},&entities),"same-scene CPU reference");
                require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"procedural GPU output composed by SDL");
                auto *raw=SDL_RenderReadPixels(renderer,nullptr);auto *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;require(image&&image->w==640&&image->h==480,"actual procedural GPU capture");
                double error=0;for(int y=0;y<480;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);
                    for(int x=0;x<640;++x){const auto a=reference.pixels[size_t(y)*640+x],b=row[x];for(int shift:{8,16,24})error+=std::abs(int((a>>shift)&255)-int((b>>shift)&255))/(3.0*255);}}
                error/=640*480;maximum_error=std::max(maximum_error,error);
                require(error<.10,"procedural CPU/GPU geometry and material equivalence");
                const auto counters=engine.metrics();require(counters.visible_creature_pixels>2,"generated anatomy has actual GPU-visible pixels");
                require(counters.target_cell==reference.aimed,"procedural GPU picking agrees with CPU occlusion");
                if(argc>1&&heading==0)require(SDL_SaveBMP(image,(std::string(argv[1])+"-creature-"+body+".bmp").c_str()),"save actual generated GPU anatomy");
                SDL_DestroySurface(image);SDL_DestroySurface(raw);
            }
        }
        std::printf("Generated creature SDL_GPU: 13 anatomies x 8 headings; maximum CPU/GPU RGB MAE %.6f\n",maximum_error);
        for(float volume:{70.f,1.f,.1f,.001f}){
            ncmm_graphics_creature_v1 entity{};entity.entity_id=++entity_id;entity.cell=creature_cell;entity.world_x=entity.world_y=4;
            entity.liters=entity.kilograms=volume;entity.flags=NCMM_CR_FLIES;entity.elevation=.65f;
            std::strcpy(entity.bodytype,"bird");std::strcpy(entity.type_id,ground[creature_cell].creature);
            entities.input.creatures[0]=entity;entities.observe();const auto type=cr::classify(entity);
            entities.resources.request(cr::appearance_key(entity,1),type,[&]{ncmm_graphics_sprite_v2 sprite{sizeof(sprite),16,32,16,art.data()};return cr::sprite::copy(sprite);});
            require(entities.resources.wait_idle(std::chrono::seconds(5)),"generated distant bird ready");
            const float pitch=std::atan2(entity.elevation+type.height*.52f-1.55f,3.f);
            fp::gpu::packet packet;fp::hybrid::frame statistics,reference;
            require(cache.build(source,-fp::hybrid::pi*.5f,pitch,provider,packet,statistics,host,{},nullptr,&entities,640,480,NCMM_GPU_FORCE),"distant bird collection");
            require(engine.submit(packet.finish(nullptr)),engine.metrics().reason);
            require(fp::hybrid::build_frame(source,-fp::hybrid::pi*.5f,pitch,provider,reference,640,480,{},nullptr,nullptr,nullptr,{},&entities),"distant bird CPU reference");
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"distant bird GPU composition");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"distant bird capture");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);require(image!=nullptr,"distant bird capture format");
            if(argc>1)require(SDL_SaveBMP(image,(std::string(argv[1])+"-bird-"+std::to_string(volume)+".bmp").c_str()),"save distant bird");
            SDL_DestroySurface(image);SDL_DestroySurface(raw);const auto counters=engine.metrics();
            const auto cpu_pixels=std::count(reference.creatures.begin(),reference.creatures.end(),creature_cell);
            std::printf("Distant bird %.3f L: LOD %u/%u/%u/%u; CPU pixels %lld; GPU pixels %llu\n",volume,statistics.creature_lods[0],statistics.creature_lods[1],statistics.creature_lods[2],statistics.creature_lods[3],(long long)cpu_pixels,(unsigned long long)counters.visible_creature_pixels);
            require(counters.visible_creature_pixels>0,"distant bird retains actual visible GPU pixels");
        }
        // The actual pinned CDDA crow is 0.6 L, 18.4 cm high and 23.5 cm
        // wide. Exercise its 34-cell LOD3 silhouette and subpixel camera shifts.
        ground.resize(side*41);details.resize(side*41);
        for(int i=0;i<int(ground.size());++i){ground[i]={};ground[i].flags=NCMM_GFX_VISIBLE;ground[i].light=1;std::strcpy(ground[i].terrain,"t_floor");details[i]={};std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
        std::strcpy(ground[creature_cell].creature,"mod_arbitrary_creature");
        source.base.grid_height=41;source.base.cell_count=ground.size();source.base.cells=ground.data();source.details=details.data();source.detail_count=details.size();source.base.camera_y=38.5f;
        ncmm_graphics_creature_v1 crow{};crow.entity_id=++entity_id;crow.cell=creature_cell;crow.world_x=crow.world_y=4;crow.liters=.6f;crow.kilograms=.45f;
        crow.height=.184188f;crow.width=.235351f;crow.flags=NCMM_CR_FLIES;crow.elevation=.65f;std::strcpy(crow.bodytype,"bird");std::strcpy(crow.type_id,ground[creature_cell].creature);
        entities.input.creatures[0]=crow;entities.observe();
        entities.resources.request(cr::appearance_key(crow,1),cr::classify(crow),[&]{ncmm_graphics_sprite_v2 sprite{sizeof(sprite),16,32,16,art.data()};return cr::sprite::copy(sprite);});
        require(entities.resources.wait_idle(std::chrono::seconds(5)),"actual-sized crow ready");
        const float crow_pitch=std::atan2(.65f+.2f*.52f-1.55f,34.f);
        for(int offset=0;offset<8;++offset){source.base.camera_x=4.5f+(offset-4)*.017f;fp::gpu::packet packet;fp::hybrid::frame statistics,reference;
            require(cache.build(source,-fp::hybrid::pi*.5f,crow_pitch,provider,packet,statistics,host,{},nullptr,&entities,640,480,NCMM_GPU_FORCE),"actual-sized distant crow collection");
            require(statistics.creature_lods[3]==1&&engine.submit(packet.finish(nullptr)),"actual-sized crow executes GPU LOD3");
            require(fp::hybrid::build_frame(source,-fp::hybrid::pi*.5f,crow_pitch,provider,reference,640,480,{},nullptr,nullptr,nullptr,{},&entities),"actual-sized crow CPU reference");
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"actual-sized crow GPU composition");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"actual-sized crow capture");SDL_DestroySurface(raw);
            const auto cpu_pixels=std::count(reference.creatures.begin(),reference.creatures.end(),creature_cell);
            const auto gpu_pixels=engine.metrics().visible_creature_pixels;
            std::printf("Actual-sized crow offset %d: CPU %lld / GPU %llu pixels\n",offset,(long long)cpu_pixels,(unsigned long long)gpu_pixels);
            require(gpu_pixels>0,"LOD3 distant crow is present at each subpixel phase");
        }
    }
    {
        // A strongly minified red/blue impostor must retain the mean colour
        // at each subpixel phase. A base-only texelFetch returns alternating
        // red/blue here despite the resource owning a complete mip pyramid.
        std::vector<uint32_t> colour(64*64),normal(64*64,0x800180ff),depth(64*64,0x000000ff);
        for(int y=0;y<64;++y)for(int x=0;x<64;++x)colour[y*64+x]=((x+y)&1)?0xff0000ff:0x0000ffff;
        ncmm_gpu_texture_v1 textures[3]={{901,64,64,64,colour.data()},{902,64,64,64,normal.data()},{903,64,64,64,depth.data()}};
        ncmm_gpu_material_v1 material{};material.color[3]=1;material.flags=NCMM_GPU_IMPOSTOR|NCMM_GPU_CUTOUT|NCMM_GPU_CREATURE|NCMM_GPU_CREATURE_VISUAL;
        material.texture_key=901;material.normal_key=902;material.depth_key=903;material.impostor_span=1;
        std::array<ncmm_gpu_vertex_v1,3> v{};
        for(int i=0;i<3;++i){v[i].position[0]=i==0?-.09f:i==1?.09f:0;v[i].position[1]=8;v[i].position[2]=i==2?.11f:-.07f;
            v[i].normal[1]=v[i].face_normal[1]=-1;v[i].uv[0]=i==0?0:i==1?1:.5f;v[i].uv[1]=i==2?0:1;}
        const uint32_t indices[]={0,1,2};ncmm_gpu_mesh_v1 minified{904,v.data(),indices,&material,3,3,1,0};
        ncmm_gpu_draw_v1 d{};d.mesh_key=904;d.transform=first_person::gpu::identity();d.bone_count=1;
        ncmm_gpu_scene_v1 test=scene;test.meshes=&minified;test.mesh_count=1;test.textures=textures;test.texture_count=3;
        test.draws=&d;test.draw_count=1;test.bones=bones;test.bone_count=1;
        for(int phase=0;phase<8;++phase){test.frame_serial=1000+phase;d.transform.columns[12]=(phase-4)*.003f;
            require(engine.submit(test),engine.metrics().reason);const auto pixel=capture("-minified-impostor");
            const int red=int(pixel>>24),blue=int((pixel>>8)&255);
            require(red>60&&red<115&&blue>60&&blue<115&&std::abs(red-blue)<8,"impostor colour mip removes checker shimmer at every subpixel phase");
            require(engine.metrics().visible_creature_pixels>0&&engine.metrics().target_cell==0,"filtered impostor preserves silhouette and picking");}
    }
    {
        // A real shared-device multi-Z scene, including deliberately submitted
        // upper actors behind a physical slab. Hardware depth must hide them.
        namespace fp=first_person;
        ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=991;world.revision=1;
        world.origin_x=1000000;world.origin_y=-1000000;world.loaded_max_z=2;world.level_height=2.5f;
        std::vector<ncmm_graphics_cell_v1> appearances(147);std::vector<ncmm_graphics_cell_v2> details(147);std::vector<ncmm_world_cell_v1> xyz;
        ncmm_world_slice_v1 slices[3]{};
        for(int z=0;z<3;++z){slices[z]={z,uint32_t(z*49),49,0,uint64_t(z+1),1};
            for(int y=0;y<7;++y)for(int x=0;x<7;++x){const int i=z*49+y*7+x;auto &a=appearances[i];a.flags=NCMM_GFX_VISIBLE|NCMM_GFX_CEILING;a.light=1;std::strcpy(a.terrain,"t_floor");
                details[i].light_r=details[i].light_g=details[i].light_b=.8f;std::strcpy(details[i].ground,"t_floor");
                xyz.push_back({world.origin_x+x,world.origin_y+y,z,NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM,0,0,1,{.22f,.22f,.22f}});}}
        const int actor=49+2*7+3;std::strcpy(appearances[actor].creature,"mon_zombie");
        world.coordinates=xyz.data();world.cell_count=uint32_t(xyz.size());world.slices=slices;world.slice_count=3;
        world.appearance.struct_size=sizeof(world.appearance);world.appearance.base={sizeof(ncmm_graphics_scene_v1),640,480,7,7,3.5f,5.5f,appearances.data(),appearances.size()};world.appearance.details=details.data();world.appearance.detail_count=details.size();
        world.appearance.environment.daylight=.8f;
        fp::gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=engine.metrics().resource_epoch;
        const float pitch=std::atan2(2.5f+.85f-1.55f,3.f);
        for(int open=0;open<2;++open){if(open)for(int i=49;i<98;++i){xyz[i].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_OPEN_DOWN;appearances[i].flags=NCMM_GFX_VISIBLE|NCMM_GFX_NO_FLOOR;}
            fp::gpu::packet packet;fp::hybrid::frame statistics,reference;
            require(cache.build(world.appearance,-fp::hybrid::pi*.5f,pitch,{},packet,statistics,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,&world),"sparse XYZ GPU scene collection");
            ncmm_gpu_multilevel_scene_v1 submitted{};submitted.struct_size=sizeof(submitted);submitted.version=1;submitted.base=packet.finish(nullptr);submitted.coordinates=xyz.data();submitted.cell_count=uint32_t(xyz.size());submitted.origin_x=world.origin_x;submitted.origin_y=world.origin_y;submitted.level_height=2.5f;
            require(engine.submit(submitted.base,&submitted),engine.metrics().reason);capture(open?"-world-open-shaft":"-world-closed-floor");
            require(fp::hybrid::build_frame(world.appearance,-fp::hybrid::pi*.5f,pitch,{},reference,640,480,{},nullptr,nullptr,nullptr,{},nullptr,&world),"actual multi-Z CPU depth reference");
            const auto gpu_count=engine.metrics().visible_creature_pixels,cpu_count=uint64_t(std::count(reference.creatures.begin(),reference.creatures.end(),actor));
            std::printf("Multi-Z %s: CPU %llu / GPU %llu pixels; target CPU %d / GPU %d\n",open?"open":"closed",(unsigned long long)cpu_count,(unsigned long long)gpu_count,reference.aimed,engine.metrics().target_cell);
            require(open?(gpu_count>100&&cpu_count>100):(gpu_count==0&&cpu_count==0),"CPU and real GPU agree on vertical slab/shaft occlusion");
            require(engine.metrics().target_cell==(open?actor:-1)&&reference.aimed==(open?actor:-1),"XYZ creature picking agrees through the real opening");
            if(open)require(std::abs(engine.metrics().label_y-reference.label_y)<.04f,"upper-Z CPU/GPU label projection agrees");
        }
        for(int i=0;i<147;++i){xyz[i].flags=NCMM_WORLD_OPEN_DOWN;appearances[i].flags=NCMM_GFX_VISIBLE|NCMM_GFX_NO_FLOOR;appearances[i].creature[0]='\0';details[i].ground[0]='\0';}
        fp::gpu::packet dark_packet;fp::hybrid::frame dark_stats,dark_reference;
        require(cache.build(world.appearance,-fp::hybrid::pi*.5f,-1.2f,{},dark_packet,dark_stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,&world),"empty downward XYZ packet");
        ncmm_gpu_multilevel_scene_v1 dark_world{};dark_world.struct_size=sizeof(dark_world);dark_world.version=1;dark_world.base=dark_packet.finish(nullptr);dark_world.coordinates=xyz.data();dark_world.cell_count=147;dark_world.origin_x=world.origin_x;dark_world.origin_y=world.origin_y;dark_world.level_height=2.5f;
        require(engine.submit(dark_world.base,&dark_world),engine.metrics().reason);const auto dark_pixel=capture("-world-unseen-down");
        require((dark_pixel>>24)<32&&((dark_pixel>>16)&255)<32&&((dark_pixel>>8)&255)<32,"real GPU downward unseen space is dark rather than sky");
        require(fp::hybrid::build_frame(world.appearance,-fp::hybrid::pi*.5f,-1.2f,{},dark_reference,640,480,{},nullptr,nullptr,nullptr,{},nullptr,&world),"downward CPU reference");
        require(std::all_of(dark_reference.depth.begin(),dark_reference.depth.end(),[](float d){return !std::isfinite(d);})&&
                std::all_of(dark_packet.draws.begin(),dark_packet.draws.end(),[](const auto &d){return d.pass==NCMM_GPU_HELD;}),
                "CPU and GPU do not conceal a hole with invented world geometry; hands keep their independent pass");
    }
    {
        // A render-only controlled terrain stair: static geometry permission
        // never supplies Native BODY permission or reveals cell contents.
        namespace fp=first_person;
        constexpr int side=7,area=side*side,root=2*side+3,upper=area+root;
        std::vector<ncmm_graphics_cell_v1> cells(area*2);std::vector<ncmm_graphics_cell_v2> details(cells.size());
        std::vector<ncmm_world_cell_v1> xyz;xyz.reserve(cells.size());
        for(int z=0;z<2;++z)for(int y=0;y<side;++y)for(int x=0;x<side;++x){const int i=z*area+y*side+x;
            cells[i].flags=NCMM_GFX_VISIBLE|(z?NCMM_GFX_NO_FLOOR:0);std::strcpy(cells[i].terrain,z?"t_open_air":"t_floor");
            std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=.365478f;
            xyz.push_back({x,y,z,z?0u:uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP),0,0,1,{.365478f,.365478f,.365478f}});
        }
        xyz[root].flags|=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;xyz[root].direction=3;
        xyz[upper].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_DOWN|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
        std::strcpy(cells[root].terrain,"t_stairs_up");std::strcpy(cells[upper].terrain,"t_stairs_down");
        ncmm_world_slice_v1 slices[]={{0,0,area,0,1,1},{1,area,area,0,2,1}};
        ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=2083;world.revision=1;
        world.loaded_max_z=1;world.level_height=2.5f;world.coordinates=xyz.data();world.cell_count=uint32_t(xyz.size());world.slices=slices;world.slice_count=2;
        auto &source=world.appearance;source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,side,side,3.5f,5.5f,cells.data(),cells.size()};
        source.details=details.data();source.detail_count=details.size();
        fp::gpu::scene_cache cache;
        auto submit=[&](const char *name){++world.revision;fp::gpu::packet packet;fp::hybrid::frame stats;
            require(cache.build(source,-fp::hybrid::pi*.5f,-.1f,{},packet,stats,engine.metrics(),{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,&world),cache.last_reason.c_str());
            ncmm_gpu_multilevel_scene_v1 sent{};sent.struct_size=sizeof(sent);sent.version=1;sent.base=packet.finish(nullptr);
            sent.coordinates=xyz.data();sent.cell_count=uint32_t(xyz.size());sent.level_height=2.5f;
            require(engine.submit(sent.base,&sent),engine.metrics().reason);capture(name);
            require(engine.metrics().cpu_raster_calls==0&&engine.metrics().visible_creature_pixels==0&&engine.metrics().target_cell<0,
                "static stairs use GPU geometry without exposing creatures or invoking a CPU world raster");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);auto *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;
            SDL_DestroySurface(raw);require(image!=nullptr,"static stair GPU readback");std::vector<uint32_t> pixels;pixels.reserve(640*480);
            for(int y=0;y<480;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);
                pixels.insert(pixels.end(),row,row+640);}
            SDL_DestroySurface(image);return pixels;
        };
        const auto normal=submit("-stairs-static");
        xyz[root].flags&=~(NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP);xyz[root].direction=0;
        const auto missing=submit("-stairs-removed");size_t changed=0;
        for(size_t i=640*20;i<normal.size();++i){const auto a=normal[i],b=missing[i];
            const auto contrast=[&](int shift){return std::abs(int((a>>shift)&255)-int((b>>shift)&255));};
            changed+=std::max({contrast(24),contrast(16),contrast(8)})>=4;
        }
        require(changed>=24,"terrain stairs have actual GPU pixel coverage with BODY withheld");
        xyz[root].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;xyz[root].direction=3;
        const auto restored=submit("-stairs-restored");require(restored==normal,"restoring static navigation restores exact GPU pixels");
        std::strcpy(cells[root].furniture,"f_bookcase");details[root].furniture_shape=NCMM_SHAPE_STORAGE;
        std::strcpy(cells[root].item,"rock");std::strcpy(cells[root].creature,"mon_zombie");
        std::strcpy(details[root].field,"fd_fire");details[root].field_shape=NCMM_SHAPE_BILLBOARD;
        require(submit("-stairs-hidden-contents")==normal,"static stair permission cannot reveal stale supplied room contents on GPU");
        std::printf("Static-only terrain stairs GPU coverage: %zu pixels; BODY remains absent\n",changed);
    }
    {
        // Run the actual weighted PSX packets through the shared device, rather
        // than inferring GPU skinning from the producer's matrix unit test.
        namespace fp=first_person;
        std::array<ncmm_graphics_cell_v1,9> cells{};std::array<ncmm_graphics_cell_v2,9> details{};
        for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");
            std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,3,3,1.5f,1.5f,cells.data(),cells.size()};
        source.details=details.data();source.detail_count=details.size();source.environment.daylight=1;source.environment.sun_z=1;
        fp::hybrid::presentation_data appearance;appearance.view.struct_size=sizeof(appearance.view);appearance.view.eye_height=1.55f;appearance.view.body_scale=1;appearance.view.held_length=.6f;appearance.cells.resize(9);
        uint32_t art=0x17202aff;auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &sprite){sprite={sizeof(sprite),1,1,1,&art};return true;};
        fp::gpu::scene_cache cache;fp::hybrid::frame statistics;
        static ncmm_gpu::backend *resident_backend=nullptr;resident_backend=&engine;
        ncmm_host_graphics_gpu_v1 api{};api.has_resource=[](uint64_t key,uint32_t kind){return resident_backend->has(key,kind)?1:0;};
        std::vector<uint32_t> previous;
        engine.profile();
        for(int n=0;n<2;++n){fp::gpu::packet packet;fp::animation::pose pose;pose.seconds=n?.15:0;pose.attack_serial=n;pose.attack_kind=n;pose.attack_phase=n?.31f:-1;
            const bool built=cache.build(source,0,0,provider,packet,statistics,engine.metrics(),pose,&appearance,nullptr,640,480,NCMM_GPU_FORCE);
            require(built,cache.last_reason.c_str());
            const auto &submission=packet.finish(&api);
            if(n)require(submission.mesh_count==0&&submission.texture_count==0,"warm PSX packet sends only bone/instance data");
            require(engine.submit(submission),engine.metrics().reason);
            require(statistics.viewmodel_draws==2&&!statistics.viewmodel_errors&&engine.recording().held_bones==554&&engine.metrics().cpu_raster_calls==0,
                    "both source-weighted hands execute the actual GPU held pass");
            if(n)require(engine.resources().static_upload_bytes()==0&&engine.metrics().generated_meshes==0,"animated PSX arms never retransmit static resources");
            capture(n?"-psx-attack":"-psx-idle");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"PSX animation pixel capture");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"PSX capture pixels");
            std::vector<uint32_t> pixels(640*480);
            for(int y=0;y<480;++y)std::memcpy(pixels.data()+y*640,static_cast<const char *>(image->pixels)+y*image->pitch,640*sizeof(uint32_t));
            SDL_DestroySurface(image);
            if(n){size_t changed=0;for(size_t i=640*240;i<pixels.size();++i)changed+=pixels[i]!=previous[i];require(changed>100,"source bone animation changes real GPU hand pixels");}
            previous=std::move(pixels);
        }
        ncmm_gpu_scene_v1 missing{};missing.struct_size=sizeof(missing);missing.width=640;missing.height=480;missing.mesh_count=1;
        require(!engine.submit(missing)&&engine.recording().bones==0,"profiling validates a missing array before inspecting its elements");
        engine.profile(false);resident_backend=nullptr;
    }
    if(argc>1){std::ofstream out(std::string(argv[1])+".json");out<<"{\"success\":true,\"full_gpu_pipeline\":true,\"hardware_depth\":true,\"gpu_skinning\":true,\"psx_weighted_viewmodel\":true,\"impostor_colour_minification\":true,\"multilevel_depth_and_picking\":true,\"dynamic_upload_over_2mib_checked\":true,\"cpu_raster_calls\":0,\"animation_upload_bytes\":"<<second.upload_bytes<<",\"resource_bytes\":"<<second.resource_bytes<<",\"software_device\":"<<second.software_device<<",\"backend\":\""<<second.backend<<"\",\"device\":\""<<second.device<<"\"}\n";require(bool(out),"device evidence");}
    std::printf("Full GPU scene PASS: %s / %s; animation upload %llu bytes; CPU raster=0\n",second.backend,second.device,(unsigned long long)second.upload_bytes);
    {
        namespace fp=first_person;
        std::array<ncmm_graphics_cell_v1,17*17> cells{};std::array<ncmm_graphics_cell_v2,17*17> details{};
        for(int y=0;y<17;++y)for(int x=0;x<17;++x){const int i=y*17+x;cells[i].flags=NCMM_GFX_VISIBLE;
            const char *id=x==8?"t_pavement":y==8?"t_dirt":"t_grass";std::strcpy(cells[i].terrain,id);std::strcpy(details[i].ground,id);
            details[i].light_r=details[i].light_g=details[i].light_b=1;}
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,17,17,8.5f,13.5f,cells.data(),cells.size()};
        source.details=details.data();source.detail_count=details.size();source.environment.daylight=1;source.environment.sun_z=.8f;source.environment.sun_x=.4f;source.environment.wind_x=20;source.environment.wind_y=7;
        uint32_t green=0x3e5a23ff,brown=0x6b543aff,asphalt=0x494b4cff;
        auto provider=[&](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &sprite){
            if(layer!=NCMM_GFX_GROUND)return false;
            const auto *art=std::strcmp(details[i].ground,"t_grass")==0?&green:std::strcmp(details[i].ground,"t_dirt")==0?&brown:&asphalt;
            sprite={sizeof(sprite),1,1,1,art};return true;};
        fp::gpu::scene_cache cache;fp::hybrid::frame statistics;
        static ncmm_gpu::backend *lawn_backend=nullptr;lawn_backend=&engine;
        ncmm_host_graphics_gpu_v1 api{};api.has_resource=[](uint64_t key,uint32_t kind){return lawn_backend->has(key,kind)?1:0;};
        std::vector<uint32_t> previous;
        for(int n=0;n<2;++n){source.environment.animation_seconds=n?1.6:1;
            fp::gpu::packet packet;require(cache.build(source,-fp::hybrid::pi*.5f,-.45f,provider,packet,statistics,engine.metrics(),{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),cache.last_reason.c_str());
            const auto &submission=packet.finish(&api);
            if(n)require(!submission.generated_cells&&!submission.mesh_count&&!submission.texture_count,"wind reuses every GPU lawn mesh and texture");
            require(engine.submit(submission),engine.metrics().reason);
            std::printf("GPU grass field draws: %llu\n",(unsigned long long)engine.metrics().draw_calls);
            require(engine.metrics().draw_calls<=48,"grass field uses instanced three-shade models, not thousands of material draws");
            if(n)require(engine.resources().static_upload_bytes()==0&&engine.metrics().generated_meshes==0&&engine.metrics().cpu_raster_calls==0,"real GPU lawn animation uploads only dynamic transforms");
            capture(n?"-grass-wind":"-grass-rest");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"lawn pixels");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"lawn pixel format");
            std::vector<uint32_t> pixels(640*480);for(int y=0;y<480;++y)std::memcpy(pixels.data()+y*640,static_cast<const char *>(image->pixels)+y*image->pitch,640*sizeof(uint32_t));SDL_DestroySurface(image);
            if(n){size_t changed=0;for(size_t i=640*160;i<pixels.size();++i)changed+=pixels[i]!=previous[i];require(changed>100,"wind moves actual GPU grass pixels");std::printf("GPU lawn: %zu changed pixels, %llu triangles, no static upload\n",changed,(unsigned long long)engine.metrics().triangles);}
            previous=std::move(pixels);
        }
        // Keep the exact floor atlas and lighting while removing only blades.
        // Count real covered pixels above the hands, not generated vertices.
        for(auto &c:cells)if(std::strcmp(c.terrain,"t_grass")==0)std::strcpy(c.terrain,"t_dirt");
        fp::gpu::packet flat;require(cache.build(source,-fp::hybrid::pi*.5f,-.45f,provider,flat,statistics,engine.metrics(),{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),cache.last_reason.c_str());
        require(engine.submit(flat.finish(&api)),engine.metrics().reason);capture("-grass-flat-reference");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);auto *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;SDL_DestroySurface(raw);require(image!=nullptr,"grass coverage reference");
        size_t covered=0;for(int y=160;y<330;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);
            for(int x=0;x<640;++x)covered+=row[x]!=previous[y*640+x];}SDL_DestroySurface(image);
        require(covered>30000,"dense GPU grass covers at least thirty thousand pixels above the held hands");
        std::printf("GPU grass coverage: %zu / 108800 pixels above hands\n",covered);
        lawn_backend=nullptr;
    }
    {
        // Real GPU pixels for the additive radio horizon, with exactly the
        // same map, camera, time and light in the ground/lattice/V1 controls.
        namespace fp=first_person;
        world_visual::fixture f(9);auto &source=f.scene;source.base.camera_x=2.5f;source.base.camera_y=1.5f;
        std::vector<ncmm_world_cell_v1> xyz(f.cells.size());
        for(int y=0;y<f.size;++y)for(int x=0;x<f.size;++x){const int i=y*f.size+x;f.cells[i].flags=NCMM_GFX_VISIBLE|NCMM_GFX_NO_FLOOR;
            f.terrain(i,"t_open_air");xyz[i]={x,y,0,NCMM_WORLD_BODY_VISIBLE,0,0,1};}
        ncmm_world_slice_v1 slice{0,0,uint32_t(xyz.size()),0,1,1};ncmm_graphics_multilevel_scene_v1 world{};
        world.struct_size=sizeof(world);world.version=1;world.world_epoch=1817;world.revision=1;world.level_height=2.5f;
        world.coordinates=xyz.data();world.cell_count=uint32_t(xyz.size());world.slices=&slice;world.slice_count=1;world.appearance=source;
        auto &data=f.appearance;data.horizon_enabled=true;data.horizon_revision=81;
        data.horizon={{80,-4.5f,0,24,24,0,{.43f,.42f,.39f},NCMM_LOD_BUILDING}};
        ncmm_skyline_feature_v1 radio{};require(ncmm_world::standard_radio_feature("radio_tower",true,0,data.horizon[0],9,0,radio),"GPU fixture uses the real authored profile");
        fp::gpu::scene_cache cache;std::array<std::vector<uint32_t>,3> gpu_pixels;std::array<fp::hybrid::frame,3> cpu;
        const auto read=[&](){auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"radio GPU readback");
            auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"radio pixel format");
            std::vector<uint32_t> pixels(640*480);for(int y=0;y<480;++y)std::memcpy(pixels.data()+y*640,static_cast<const char *>(image->pixels)+y*image->pitch,640*sizeof(uint32_t));
            SDL_DestroySurface(image);return pixels;};
        const auto submit=[&](float yaw){fp::gpu::packet packet;fp::hybrid::frame stats;
            const bool built=cache.build(world.appearance,yaw,0,{},packet,stats,engine.metrics(),{},&data,nullptr,640,480,NCMM_GPU_FORCE,&world);
            require(built,cache.last_reason.c_str());
            ncmm_gpu_multilevel_scene_v1 submitted{};submitted.struct_size=sizeof(submitted);submitted.version=1;submitted.base=packet.finish(nullptr);
            submitted.coordinates=xyz.data();submitted.cell_count=uint32_t(xyz.size());submitted.level_height=2.5f;
            require(engine.submit(submitted.base,&submitted),engine.metrics().reason);
            require(engine.metrics().cpu_raster_calls==0,"radio qualification executes the real GPU rasterizer");};
        for(int mode=0;mode<3;++mode){data.skyline=mode==1?std::vector<ncmm_skyline_feature_v1>{radio}:std::vector<ncmm_skyline_feature_v1>{};
            data.horizon[0].height=mode==2?5.f:0.f;++data.horizon_revision;
            require(fp::hybrid::build_frame(world.appearance,0,0,{},cpu[mode],640,480,{},nullptr,nullptr,&data,{},nullptr,&world),"CPU radio reference uses the same frozen scene");
            submit(0);capture(mode==0?"-radio-ground":mode==1?"-radio-lattice":"-radio-legacy-box");gpu_pixels[mode]=read();
            require(engine.metrics().target_cell==-1,"distant radio has no actor/interaction target");
        }
        size_t gpu_coverage=0,cpu_coverage=0,gpu_high=0,cpu_high=0;
        for(int y=25;y<240;++y)for(int x=0;x<640;++x){const size_t i=size_t(y)*640+x;
            const bool g=gpu_pixels[1][i]!=gpu_pixels[0][i],c=cpu[1].pixels[i]!=cpu[0].pixels[i];
            gpu_coverage+=g;cpu_coverage+=c;gpu_high+=g&&y<210;cpu_high+=c&&y<210;}
        require(gpu_coverage>10&&gpu_coverage<3000&&gpu_high>2&&cpu_coverage>10&&cpu_coverage<3000&&cpu_high>2,
                "CPU and GPU pixels both retain the real tall open radio profile");
        require(gpu_pixels[1][200*640+320]==gpu_pixels[0][200*640+320]&&cpu[1].depth[200*640+320]==cpu[0].depth[200*640+320],
                "both rasterizers see through the actual empty centre of the radio lattice");
        require(gpu_pixels[1][230*640+320]==gpu_pixels[0][230*640+320]&&gpu_pixels[2][230*640+320]!=gpu_pixels[0][230*640+320],
                "typed radio removes the opaque five-metre V1 box instead of drawing over it");
        data.horizon[0].height=0;data.skyline={radio};++data.horizon_revision;submit(0);capture("-radio-restored");
        submit(.15f);require(engine.resources().static_upload_bytes()==0&&engine.metrics().generated_meshes==0,"camera turn reuses radio masks and mesh without static upload");capture("-radio-turned");
        for(int y=0;y<f.size;++y){const int i=y*f.size+5;f.terrain(i,"t_wall");f.details[i].terrain_shape=NCMM_SHAPE_WALL;
            xyz[i].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE;xyz[i].revision=2;}
        ++world.revision;submit(0);capture("-radio-near-wall");
        require(engine.focus_cell()==f.size+5,"exact near wall still owns GPU depth and focus in front of typed skyline");
        std::printf("GPU radio lattice: %zu pixels (%zu above V1 height); CPU: %zu (%zu high)\n",gpu_coverage,gpu_high,cpu_coverage,cpu_high);
    }
    {
        namespace fp=first_person;
        constexpr int side=9,area=side*side;
        std::vector<ncmm_graphics_cell_v1> cells(area*2);std::vector<ncmm_graphics_cell_v2> details(cells.size());std::vector<ncmm_world_cell_v1> xyz(cells.size());
        for(int z=-1;z<=0;++z)for(int y=0;y<side;++y)for(int x=0;x<side;++x){const int i=(z+1)*area+y*side+x;
            auto &a=cells[i];a.flags=NCMM_GFX_VISIBLE|NCMM_GFX_CEILING;std::strcpy(a.terrain,z==0?"t_floor":x==2||x==6?"t_railroad_track_on_tie_v":"t_railroad_rubble");
            std::strcpy(details[i].ground,a.terrain);xyz[i]={x,y,z,NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_ROOFED|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM,0,0,1};
            if(z==-1&&(x==0||x==8||y==0)){a.flags|=NCMM_GFX_WALL;details[i].terrain_shape=NCMM_SHAPE_WALL;std::strcpy(a.terrain,"t_concrete_wall");}}
        ncmm_world_slice_v1 slices[]={{-1,0,area,0,1,1},{0,area,area,0,2,1}};
        ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=1997;world.revision=1;
        world.origin_z=world.loaded_min_z=-1;world.loaded_max_z=0;world.level_height=2.5f;world.coordinates=xyz.data();world.cell_count=uint32_t(xyz.size());world.slices=slices;world.slice_count=2;
        auto &source=world.appearance;source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,side,side,4.5f,7.5f,cells.data(),cells.size()};source.details=details.data();source.detail_count=details.size();
        source.environment.daylight=1;source.environment.sun_z=.8f;source.environment.fog_density=.6f; // Surface weather must not fill a closed tunnel.
        fp::gpu::scene_cache cache;double brightness[3]{};
        for(int light=0;light<3;++light){
            source.environment.lightning=light==1; // A surface storm cannot light a sealed tunnel.
            for(int i=0;i<int(cells.size());++i){const float value=light==0?.85f:light==1?0.f:std::max(0.f,1.f-std::hypot(float(xyz[i].x-4),float(xyz[i].y-5))/4.f);
                details[i].light_r=value;details[i].light_g=value*.91f;details[i].light_b=value*.76f;
                std::copy_n(&details[i].light_r,3,xyz[i].lower_light);}
            ++world.revision;fp::gpu::packet packet;fp::hybrid::frame stats,reference,opaque;
            require(cache.build(source,-fp::hybrid::pi*.5f,-.2f,{},packet,stats,engine.metrics(),{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,&world),cache.last_reason.c_str());
            require(packet.scene.camera.time[2]==NCMM_SKY_VOID&&packet.scene.camera.sky_top[3]==1&&packet.scene.camera.sky_bottom[0]==0,"closed underground uses black sky on both hemispheres");
            ncmm_gpu_multilevel_scene_v1 submitted{};submitted.struct_size=sizeof(submitted);submitted.version=1;submitted.base=packet.finish(nullptr);submitted.coordinates=xyz.data();submitted.cell_count=uint32_t(xyz.size());submitted.origin_z=-1;submitted.level_height=2.5f;
            require(engine.submit(submitted.base,&submitted),engine.metrics().reason);capture(light==0?"-subway-lit":light==1?"-subway-dark":"-subway-local-light");
            auto *raw=SDL_RenderReadPixels(renderer,nullptr);auto *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;SDL_DestroySurface(raw);require(image!=nullptr,"subway lighting capture");
            // Compare the visible track bed, above the hands; the distant
            // unlit wall deliberately remains black in the local-light case.
            for(int y=250;y<355;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);for(int x=10;x<630;++x)brightness[light]+=(row[x]>>24)+((row[x]>>16)&255)+((row[x]>>8)&255);}
            brightness[light]/=105*620*3;SDL_DestroySurface(image);
            require(fp::hybrid::build_frame(source,-fp::hybrid::pi*.5f,-.2f,{},reference,640,480,{},nullptr,&opaque,nullptr,{},nullptr,&world),"subway CPU lighting reference");
            if(light==1){for(size_t i=0;i<opaque.pixels.size();++i)require((opaque.pixels[i]>>24)<4&&((opaque.pixels[i]>>16)&255)<4&&((opaque.pixels[i]>>8)&255)<4,"CPU dark tunnel has no ambient floor or outdoor fog");}
        }
        std::printf("Subway actual GPU brightness: lit %.2f / dark %.2f / local %.2f\n",brightness[0],brightness[1],brightness[2]);
        require(brightness[0]>35&&brightness[1]<2&&brightness[2]>brightness[1]+8&&brightness[2]<brightness[0],"real GPU tunnel preserves darkness and localized Native RGB contrast");
    }
    if(argc>2&&std::strcmp(argv[2],"--grass-benchmark")==0){
        // Identical owned field/camera for before/after measurements. Readback
        // and screenshots are outside the timed loop; no Native simulation.
        constexpr int side=41;
        std::vector<ncmm_graphics_cell_v1> cells(side*side);std::vector<ncmm_graphics_cell_v2> details(cells.size());
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){const int i=y*side+x;cells[i].flags=NCMM_GFX_VISIBLE;
            const char *id=x==20?"t_dirt":"t_grass";std::strcpy(cells[i].terrain,id);std::strcpy(details[i].ground,id);
            details[i].light_r=details[i].light_g=details[i].light_b=1;}
        ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),640,480,side,side,20.5f,32.5f,cells.data(),cells.size()};
        source.details=details.data();source.detail_count=details.size();source.environment.daylight=1;source.environment.sun_z=.8f;source.environment.sun_x=.4f;source.environment.wind_x=20;source.environment.wind_y=7;
        uint32_t green=0x3e5a23ff,brown=0x6b543aff;
        auto provider=[&](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &sprite){if(layer!=NCMM_GFX_GROUND)return false;
            sprite={sizeof(sprite),1,1,1,std::strcmp(cells[i].terrain,"t_grass")==0?&green:&brown};return true;};
        first_person::gpu::scene_cache cache;first_person::hybrid::frame statistics;
        static ncmm_gpu::backend *field_backend=nullptr;field_backend=&engine;
        ncmm_host_graphics_gpu_v1 api{};api.has_resource=[](uint64_t key,uint32_t kind){return field_backend->has(key,kind)?1:0;};
        std::vector<double> prepare,submit,record;uint64_t calls=0,triangles=0,meshes=0,upload=0,regenerated=0;engine.profile();
        for(int n=0;n<88;++n){source.environment.animation_seconds=1+n/30.;
            first_person::gpu::packet packet;const auto begun=std::chrono::steady_clock::now();
            require(cache.build(source,-first_person::hybrid::pi*.5f,-.18f,provider,packet,statistics,engine.metrics(),{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),cache.last_reason.c_str());
            const auto &submission=packet.finish(&api);const auto prepared=std::chrono::steady_clock::now();
            require(engine.submit(submission),engine.metrics().reason);const auto submitted=std::chrono::steady_clock::now();
            if(n>=8){prepare.push_back(std::chrono::duration<double,std::milli>(prepared-begun).count());submit.push_back(std::chrono::duration<double,std::milli>(submitted-prepared).count());
                record.push_back(engine.recording().world_ms);calls=std::max(calls,uint64_t(engine.metrics().draw_calls));triangles=std::max(triangles,uint64_t(engine.metrics().triangles));
                meshes=std::max(meshes,uint64_t(submission.mesh_count));upload=std::max(upload,engine.resources().static_upload_bytes());regenerated+=submission.generated_cells;}
            // Bound queued work equally in both revisions, outside CPU timings.
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr)&&SDL_FlushRenderer(renderer)&&SDL_WaitForGPUIdle(device),"bounded benchmark frame");
            if(n==8||n==87)capture(n==8?"-field-rest":"-field-wind");
        }
        auto median=[](std::vector<double> v){std::sort(v.begin(),v.end());return v[v.size()/2];};
        require(calls<=60&&regenerated==0&&upload==0&&meshes==0,"large warm grass field remains instanced with no geometry regeneration or static uploads");
        first_person::hybrid::frame cpu_static,cpu_first,cpu;std::vector<double> cpu_times;
        require(first_person::hybrid::build_frame(source,-first_person::hybrid::pi*.5f,-.18f,provider,cpu_first,640,480,{},nullptr,&cpu_static),"CPU benchmark world");
        for(int n=0;n<8;++n){source.environment.animation_seconds=1+n/30.;const auto begun=std::chrono::steady_clock::now();
            require(first_person::hybrid::build_frame(source,-first_person::hybrid::pi*.5f,-.18f,provider,cpu,640,480,{},&cpu_static),"CPU cached grass benchmark");
            if(n>=2)cpu_times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count());}
        std::ofstream report(std::string(argv[1])+"-grass-benchmark.json");
        report<<"{\"samples\":80,\"width\":640,\"height\":480,\"field_tiles\":1681,\"prepare_median_ms\":"<<median(prepare)<<",\"submit_median_ms\":"<<median(submit)
            <<",\"world_record_median_ms\":"<<median(record)<<",\"draw_calls_max\":"<<calls<<",\"triangles_max\":"<<triangles<<",\"warm_meshes_max\":"<<meshes
            <<",\"warm_static_upload_max\":"<<upload<<",\"warm_generated_cells_sum\":"<<regenerated<<",\"cpu_cached_median_ms\":"<<median(cpu_times)<<",\"cpu_submitted_triangles\":"<<cpu.submitted_triangles
            <<",\"background_games_running\":true,\"gpu_duration_measured\":false}\n";
        require(bool(report),"grass benchmark receipt");field_backend=nullptr;engine.profile(false);
    }
    // The imported red-dot lens uses alpha, rather than an opaque stand-in.
    // Two front lens surfaces precede an opaque held surface in material order:
    // they must blend after it and must not reject one another by writing depth.
    {
        ncmm_gpu_scene_v1 lens_scene{};lens_scene.struct_size=sizeof(lens_scene);lens_scene.width=640;lens_scene.height=480;lens_scene.mode=NCMM_GPU_FORCE;lens_scene.frame_serial=1900;
        auto &camera=lens_scene.camera;camera.right[0]=1;camera.up[2]=1;camera.forward[1]=1;
        camera.projection[0]=640;camera.projection[1]=480;camera.projection[2]=457;camera.projection[3]=512;camera.sun[2]=camera.sun[3]=1;camera.grid[0]=camera.grid[1]=camera.grid[2]=1;
        ncmm_gpu_cell_v1 lens_cell{};lens_cell.flags=1;lens_cell.light[0]=lens_cell.light[1]=lens_cell.light[2]=1;lens_scene.cells=&lens_cell;lens_scene.cell_count=1;
        std::array<ncmm_gpu_vertex_v1,9> lens_vertices{};std::array<uint32_t,9> lens_indices{};
        for(size_t i=0;i<9;++i){const size_t material=i/3,corner=i%3;const float depth=material==0?1.f:material==1?1.3f:2.f;
            auto &vertex=lens_vertices[i];vertex.position[0]=(corner==0?-1.f:corner==1?1.f:0.f)*depth;vertex.position[1]=depth;vertex.position[2]=(corner==2?1.f:-1.f)*depth;
            vertex.normal[1]=vertex.face_normal[1]=-1;vertex.material=uint32_t(material);lens_indices[i]=uint32_t(i);}
        ncmm_gpu_material_v1 lens_materials[3]{};lens_materials[0].color[0]=.95f;lens_materials[1].color[2]=.95f;lens_materials[2].color[1]=.95f;
        for(size_t i=0;i<3;++i){lens_materials[i].color[3]=i<2?.25f:1.f;lens_materials[i].flags=NCMM_GPU_HELD_MATERIAL|NCMM_GPU_SMOOTH;}
        ncmm_gpu_mesh_v1 lens_mesh{0x48454c444c454e53ull,lens_vertices.data(),lens_indices.data(),lens_materials,9,9,3,0};lens_scene.meshes=&lens_mesh;lens_scene.mesh_count=1;lens_scene.textures=nullptr;lens_scene.texture_count=0;
        const auto identity=first_person::gpu::identity();lens_scene.bones=&identity;lens_scene.bone_count=1;
        ncmm_gpu_draw_v1 lens_draw{};lens_draw.mesh_key=lens_mesh.key;lens_draw.transform=identity;lens_draw.bone_count=1;lens_draw.pass=NCMM_GPU_HELD;lens_scene.draws=&lens_draw;lens_scene.draw_count=1;
        require(engine.submit(lens_scene),engine.metrics().reason);const uint32_t pixel=capture("-held-lenses");
        const auto red=pixel>>24,green=(pixel>>16)&255,blue=(pixel>>8)&255;
        require(red>8&&blue>8&&green>red&&green>blue,"held glass blends both source layers over the visible opaque surface without writing depth");
    }
    // The same retained-mesh ray has two contracts: Native combat can see a
    // creature through glass; E must select the nearer interactive surface.
    {
        ncmm_gpu_scene_v1 focus_scene{};focus_scene.struct_size=sizeof(focus_scene);
        focus_scene.width=640;focus_scene.height=480;focus_scene.mode=NCMM_GPU_FORCE;focus_scene.frame_serial=1950;
        focus_scene.camera=camera;focus_scene.camera.projection[0]=640;focus_scene.camera.projection[1]=480;focus_scene.camera.projection[2]=457;
        std::array<ncmm_gpu_cell_v1,2> focus_cells{};
        for(auto &value:focus_cells){value.flags=1;value.light[0]=value.light[1]=value.light[2]=1;}
        focus_scene.cells=focus_cells.data();focus_scene.cell_count=2;focus_scene.camera.grid[0]=focus_scene.camera.grid[2]=2;
        auto near_vertices=vertices;for(auto &vertex:near_vertices){vertex.joint=0;vertex.material=0;vertex.position[1]=2;}
        auto far_vertices=near_vertices;for(auto &vertex:far_vertices)vertex.position[1]=4;
        ncmm_gpu_material_v1 glass{};glass.color[3]=.25f;
        ncmm_gpu_material_v1 actor{};actor.color[3]=1;actor.flags=NCMM_GPU_CREATURE;
        const ncmm_gpu_mesh_v1 focus_meshes[]={{0x46505301,near_vertices.data(),indices,&glass,3,3,1,0},
            {0x46505302,far_vertices.data(),indices,&actor,3,3,1,0}};
        focus_scene.meshes=focus_meshes;focus_scene.mesh_count=2;
        const auto identity=first_person::gpu::identity();focus_scene.bones=&identity;focus_scene.bone_count=1;
        ncmm_gpu_draw_v1 focus_draws[2]{};
        for(int i=0;i<2;++i){focus_draws[i].mesh_key=focus_meshes[i].key;focus_draws[i].cell=i;
            focus_draws[i].transform=identity;focus_draws[i].bone_count=1;}
        focus_scene.draws=focus_draws;focus_scene.draw_count=2;
        auto submit_focus=[&](){const bool accepted=engine.submit(focus_scene);require(accepted,engine.metrics().reason);};
        submit_focus();
        require(engine.focus_cell()==0&&engine.metrics().target_cell==1,"glass owns interaction focus without replacing the creature target");
        glass.color[3]=1;focus_scene.frame_serial++;focus_scene.mesh_count=1;focus_scene.meshes=focus_meshes;
        // A new immutable key is required when the material changes.
        auto wall_mesh=focus_meshes[0];wall_mesh.key=0x46505303;focus_scene.meshes=&wall_mesh;focus_draws[0].mesh_key=wall_mesh.key;
        submit_focus();
        require(engine.focus_cell()==0&&engine.metrics().target_cell==-1,"opaque wall blocks both Native combat picking and interaction picking");
        focus_draws[0].pass=NCMM_GPU_HELD;focus_scene.mesh_count=0;focus_scene.meshes=nullptr;focus_scene.frame_serial++;
        submit_focus();
        require(engine.focus_cell()==1&&engine.metrics().target_cell==1,"held overlays never steal the world interaction target");
    }
    {
        // Device test only: a real emitting lens must remain bright in an
        // unlit scene while the identical unpowered material stays dark.
        ncmm_gpu_scene_v1 lamp{};lamp.struct_size=sizeof(lamp);lamp.width=640;lamp.height=480;lamp.mode=NCMM_GPU_FORCE;lamp.frame_serial=1970;
        lamp.camera.right[0]=1;lamp.camera.up[2]=1;lamp.camera.forward[1]=1;
        lamp.camera.projection[0]=640;lamp.camera.projection[1]=480;lamp.camera.projection[2]=457;lamp.camera.projection[3]=512;
        lamp.camera.grid[0]=lamp.camera.grid[1]=lamp.camera.grid[2]=1;
        ncmm_gpu_cell_v1 dark{};dark.flags=NCMM_GFX_VISIBLE|NCMM_GFX_CEILING;lamp.cells=&dark;lamp.cell_count=1;
        std::array<ncmm_gpu_vertex_v1,3> points{};
        for(unsigned n=0;n<3;++n){points[n].position[0]=n==0?-1.f:n==1?1.f:0.f;points[n].position[1]=2;points[n].position[2]=n==2?1.f:-1.f;points[n].normal[1]=points[n].face_normal[1]=-1;}
        ncmm_gpu_material_v1 lens{};lens.color[0]=.95f;lens.color[1]=.85f;lens.color[2]=.60f;lens.color[3]=1;
        ncmm_gpu_mesh_v1 lamp_mesh{0x4c414d50,points.data(),indices,&lens,3,3,1,0};lamp.meshes=&lamp_mesh;lamp.mesh_count=1;
        const auto identity=first_person::gpu::identity();lamp.bones=&identity;lamp.bone_count=1;
        ncmm_gpu_draw_v1 command{};command.mesh_key=lamp_mesh.key;command.transform=identity;command.bone_count=1;lamp.draws=&command;lamp.draw_count=1;
        require(engine.submit(lamp),engine.metrics().reason);const auto unpowered=capture("-lamp-off");
        require((unpowered>>24)<20&&((unpowered>>16)&255)<20,"unpowered lens stays dark at zero ambient");
        lens.flags=NCMM_GPU_EMISSIVE;++lamp_mesh.key;command.mesh_key=lamp_mesh.key;++lamp.frame_serial;
        require(engine.submit(lamp),engine.metrics().reason);const auto powered=capture("-lamp-on");
        require((powered>>24)>180&&((powered>>16)&255)>150,"emitting lens stays bright at zero ambient without inventing surrounding illumination");
    }
    // Leave work and an SDL composition queued while replacing the backend.
    // No read-pixels or explicit fence wait may accidentally serialize this path.
    scene.meshes=&mesh;scene.mesh_count=1;scene.draws=&draw;scene.draw_count=1;scene.textures=nullptr;scene.texture_count=0;
    engine.profile();
    for(int lifecycle=0;lifecycle<32;++lifecycle){
        require(engine.bind(renderer,1),"teardown stress rebind");
        for(int frame=0;frame<4;++frame){scene.frame_serial=2000+lifecycle*4+frame;require(engine.submit(scene),engine.metrics().reason);
            require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"queue SDL sampling before teardown");}
        engine.clear();require(!engine.metrics().active&&engine.resources().bytes()==0&&engine.recording().frame_upload_capacity_bytes==0&&engine.recording().cell_cache_bytes==0,"in-flight teardown drains and clears resources including staging and source cells");
    }
    #include "vehicle_closed_gpu_test.inc"
    #include "world_visual_gpu_test.inc"
    #include "opaque_depth_gpu_test.inc"
    #include "glass_gpu_test.inc"
    engine.clear();require(!engine.bind(nullptr,2)&&!engine.available(),"unsupported renderer fails closed without claiming GPU");
    require(engine.bind(renderer,3),"same owned device can recover after fallback");engine.clear();
    SDL_DestroyTexture(ui);SDL_DestroyRenderer(renderer);SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
}
