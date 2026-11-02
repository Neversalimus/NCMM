#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include "vehicle_frame_caps_fixture.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace gpu=first_person::gpu;
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"Vertex pack device: %s (%s)\n",why,SDL_GetError());std::exit(1);}}
static gpu::owned_mesh grid(){
    gpu::owned_mesh mesh;ncmm_gpu_material_v1 material{};material.flags=NCMM_GPU_SMOOTH;material.finish=7;material.color[0]=.4f;material.color[1]=.3f;material.color[2]=.2f;material.color[3]=1;mesh.materials.push_back(material);
    const unsigned order[]={0,1,2,0,2,3};
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)for(unsigned n=0;n<6;++n){const unsigned corner=order[n];ncmm_gpu_vertex_v1 v{};
        v.position[0]=float(x+(corner==1||corner==2))*.1f;v.position[1]=float(y+(corner>=2))*.1f;v.normal[2]=v.face_normal[2]=1;
        v.uv[0]=float(n)+.125f;v.uv[1]=float(x+y+n)+.25f;mesh.indices.push_back(uint32_t(mesh.vertices.size()));mesh.vertices.push_back(v);}
    mesh.key=gpu::mesh_key(mesh);return mesh;
}
static std::vector<unsigned char> download(SDL_GPUDevice *device,SDL_GPUBuffer *buffer,uint32_t size){
    SDL_GPUTransferBufferCreateInfo info{};info.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;info.size=size;
    auto *transfer=SDL_CreateGPUTransferBuffer(device,&info);require(transfer!=nullptr,"create bounded resource readback");
    auto *commands=SDL_AcquireGPUCommandBuffer(device);require(commands!=nullptr,"acquire resource readback");
    auto *copy=SDL_BeginGPUCopyPass(commands);require(copy!=nullptr,"begin resource readback");
    SDL_GPUBufferRegion from{buffer,0,size};SDL_GPUTransferBufferLocation to{transfer,0};SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);
    auto *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(commands);require(fence!=nullptr,"submit resource readback");require(SDL_WaitForGPUFences(device,true,&fence,1),"qualification-only readback wait");SDL_ReleaseGPUFence(device,fence);
    auto *mapped=static_cast<unsigned char *>(SDL_MapGPUTransferBuffer(device,transfer,false));require(mapped!=nullptr,"map resource readback");
    std::vector<unsigned char> result(mapped,mapped+size);SDL_UnmapGPUTransferBuffer(device,transfer);SDL_ReleaseGPUTransferBuffer(device,transfer);return result;
}
static void verify_gpu(SDL_GPUDevice *device,const ncmm_gpu::mesh_resource &m){
    const auto vertices=download(device,m.vertices,uint32_t(m.gpu_vertex_data().size()*sizeof(ncmm_gpu_vertex_v1)));
    const auto indices=download(device,m.indices,uint32_t(m.gpu_index_data().size()*4));
    require(!std::memcmp(vertices.data(),m.gpu_vertex_data().data(),vertices.size()),"actual GPU vertex allocation equals the proved private upload representation");
    require(!std::memcmp(indices.data(),m.gpu_index_data().data(),indices.size()),"actual GPU indices equal the complete ordered remap");
}
static void resources(SDL_GPUDevice *device){
    auto a=grid(),b=a,unproved=a;b.materials[0].color[0]=.8f;unproved.materials[0].texture_key=77;unproved.materials[0].flags|=NCMM_GPU_TEXTURED;
    const auto packed=ncmm_gpu::vertex_pack::compact(a.view(),a.indices);require(packed.active,"resource fixture positively packs");
    const uint64_t vb=a.vertices.size()*sizeof(ncmm_gpu_vertex_v1),pvb=packed.vertices.size()*sizeof(ncmm_gpu_vertex_v1),ib=a.indices.size()*4;
    ncmm_gpu::resource_cache cache;cache.bind(device);require(cache.use_compact_opaque_vertices(true),"explicit current-backend upload mode");
    std::vector<ncmm_gpu_mesh_v1> meshes{a.view(),b.view(),unproved.view()};meshes[0].key=1;meshes[1].key=2;meshes[2].key=3;
    std::vector<ncmm_gpu_draw_v1> draws(3);for(unsigned n=0;n<3;++n){draws[n].mesh_key=n+1;draws[n].bone_count=1;}
    std::array<uint32_t,1> pixel{{0xffffffffu}};ncmm_gpu_texture_v1 texture{77,1,1,1,pixel.data()};
    ncmm_gpu_scene_v1 scene{};scene.meshes=meshes.data();scene.mesh_count=3;scene.draws=draws.data();scene.draw_count=3;scene.textures=&texture;scene.texture_count=1;ncmm_gpu_metrics_v1 metrics{};
    const uint64_t exact=pvb+vb+2*ib+4;
    require(cache.prepare(scene,exact,metrics),"exact budget with two packed leases and one equal-vertex textured fallback");
    const auto *ma=cache.mesh(1),*mb=cache.mesh(2),*mu=cache.mesh(3);
    require(ma&&mb&&mu&&ma->compacted&&mb->compacted&&!mu->compacted,"proved and unproved whole-mesh representations stay separate");
    require(ma->vertices==mb->vertices&&ma->static_vertices==mb->static_vertices&&ma->packed_vertices==mb->packed_vertices,"material-independent packed leases share both owned CPU and GPU data");
    require(ma->vertices!=mu->vertices&&ma->static_vertices!=mu->static_vertices,"unproved textured mesh cannot borrow a packed allocation with equal raw vertices");
    require(ma->indices==mb->indices&&ma->indices!=mu->indices,"exact remapped index pool remains separate from original ordered indices");
    require(ma->vertex_data().size()==a.vertices.size()&&!std::memcmp(ma->vertex_data().data(),a.vertices.data(),size_t(vb))&&ma->cpu_indices==a.indices,"original CPU picking vertices and indices stay owned and exact");
    require(cache.bytes()==exact&&cache.static_vertex_pool_bytes()==vb+pvb&&cache.static_index_pool_bytes()==2*ib,"each distinct GPU allocation is charged exactly once");
    require(cache.static_vertex_creations()==2&&cache.static_vertex_reuses()==1,"packed/original creation and sharing counters are exact");verify_gpu(device,*ma);verify_gpu(device,*mu);
    require(!cache.use_compact_opaque_vertices(false),"a live cache cannot silently change its upload interpretation");
    const auto *retained=mb->vertices;scene.meshes=nullptr;scene.mesh_count=0;scene.textures=nullptr;scene.texture_count=0;scene.draws=draws.data()+1;scene.draw_count=2;metrics={};
    require(cache.prepare(scene,exact,metrics)&&metrics.upload_bytes==0&&cache.static_upload_bytes()==0,"fully warm packed and original leases upload zero bytes");
    // Budget can evict the no-longer-drawn first owner without losing the shared
    // CPU picking array or compact GPU buffer retained by the second material.
    scene.draws=draws.data()+1;scene.draw_count=1;
    require(cache.prepare(scene,pvb+ib,metrics)&&cache.mesh(1)==nullptr&&cache.mesh(3)==nullptr,"last unused raw allocation and first packed lease evict under the original budget rule");
    require(cache.mesh(2)->vertices==retained&&cache.bytes()==pvb+ib&&cache.mesh(2)->cpu_indices==b.indices,"surviving packed lease keeps its original CPU geometry and exact accounting");verify_gpu(device,*cache.mesh(2));
    cache.clear();require(cache.bytes()==0&&cache.cpu_bytes()==0&&cache.static_vertex_pool_bytes()==0&&cache.static_index_pool_bytes()==0,"clear releases final packed CPU/GPU arrays and both shared pools");
    require(cache.use_compact_opaque_vertices(false),"mode may change only once all resources have gone");meshes={a.view()};meshes[0].key=1;draws[0].mesh_key=1;scene={};scene.meshes=meshes.data();scene.mesh_count=1;scene.draws=draws.data();scene.draw_count=1;metrics={};
    require(cache.prepare(scene,vb+ib,metrics)&&!cache.mesh(1)->compacted&&cache.bytes()==vb+ib,"reference mode uploads complete original arrays after clear");verify_gpu(device,*cache.mesh(1));
    cache.clear();require(cache.use_compact_opaque_vertices(true),"enable again after complete clear");meshes[0].reserved=NCMM_GPU_DYNAMIC_MESH;
    require(cache.prepare(scene,vb+ib,metrics)&&!cache.mesh(1)->compacted,"dynamic mesh uses original independent allocation");a.vertices[0].position[2]=.123f;meshes[0]=a.view();meshes[0].key=1;meshes[0].reserved=NCMM_GPU_DYNAMIC_MESH;metrics={};
    require(cache.prepare(scene,vb+ib,metrics)&&metrics.dynamic_mesh_updates==1&&!cache.mesh(1)->compacted,"dynamic update never retains a stale compact remap");verify_gpu(device,*cache.mesh(1));
    cache.clear();require(cache.bytes()==0&&cache.cpu_bytes()==0,"dynamic and packed retirement leaves no owned payload");cache.abandon();
}
int main(){
    // This entire executable is compile-only locally. Actual device operations
    // and full image/resource controls execute only on the three CI platforms.
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM vertex packing device controls",1280,800,SDL_WINDOW_HIDDEN);require(window!=nullptr,"hidden CI surface");
#endif
    const auto props=SDL_CreateProperties();SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,true);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN,false);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN,false);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN,false);SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN,false);
    auto *device=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);require(device!=nullptr,"actual CI GPU without fallback");auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared renderer");
    auto *target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,800);require(target&&SDL_SetRenderTarget(renderer,target),"complete1280x800 target");
    ncmm_gpu::backend original(false),current(true);require(original.bind(renderer,8200)&&current.bind(renderer,8200),"separate original-upload and compact-upload backend instances");
    namespace fp=first_person;namespace v=fp::vehicle;
    std::vector<ncmm_graphics_cell_v1> cells(625);std::vector<ncmm_graphics_cell_v2> details(625);
    for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_pavement");std::strcpy(details[i].ground,"t_pavement");details[i].light_r=.35f+float(i%5)*.12f;details[i].light_g=.4f+float(i%3)*.13f;details[i].light_b=.5f;}
    ncmm_graphics_scene_v2 source{};source.struct_size=sizeof(source);source.base={sizeof(source.base),1280,800,25,25,12.5f,23.5f,cells.data(),625};source.details=details.data();source.detail_count=625;
    source.environment.daylight=1;source.environment.sun_x=.3f;source.environment.sun_y=.4f;source.environment.sun_z=.8f;
    fp::hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(625);
    v::presentation vehicles;metadata.vehicles=&vehicles;gpu::scene_cache cache;fp::hybrid::frame statistics;
    struct observation{std::vector<uint32_t> pixels;int focus=-1,target=-1;};
    const auto read=[&](ncmm_gpu::backend &engine){observation o;require(SDL_SetRenderDrawColor(renderer,0,0,0,255)&&SDL_RenderClear(renderer)&&SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"compose complete current or control image");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"read complete image");auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image&&image->w==1280&&image->h==800,"original resolution retained");
        o.pixels.resize(1280*800);for(int y=0;y<800;++y)std::memcpy(o.pixels.data()+1280*y,static_cast<const char *>(image->pixels)+image->pitch*y,1280*4);SDL_DestroySurface(image);o.focus=engine.focus_cell();o.target=engine.metrics().target_cell;return o;};
    std::ofstream report("vertex-pack-device-controls.json");require(bool(report),"own separate device receipt");report<<"{\"full_viewport\":[1280,800],\"original501_and150_cap_controls_unchanged\":true,\"pairs\":[";
    bool first=true,equal=true;unsigned pairs=0,active=0,fallback=0;uint64_t total_different=0,saved_total=0;
    const auto views=frame_caps_fixture::cameras();
    for(unsigned variant=0;variant<5;++variant){vehicles.reset();vehicles.input=frame_caps_fixture::construction();vehicles.input.scene.world_epoch=variant+1;
        if(variant==1||variant==2){struct item{const char *id;uint32_t flags;};const item items[]={{"seat",NCMM_VP_SEAT|NCMM_VP_FLOOR|NCMM_VP_CARGO},{"controls",NCMM_VP_CONTROLS},{"horn_big",0},{"integrated_cooler",0},{"cam_control",0},{"door",NCMM_VP_PANEL|NCMM_VP_DOOR|NCMM_VP_OPENABLE},{"windshield",NCMM_VP_PANEL|NCMM_VP_WINDOW|NCMM_VP_WINDSHIELD},{"wheel",NCMM_VP_WHEEL}};
            for(unsigned n=0;n<8;++n){auto p=vehicles.input.parts[n*3];p.part_id=vehicles.input.parts.size()+1;p.features=items[n].flags;std::strcpy(p.id,items[n].id);p.wheel_radius=.32f;p.wheel_width=.18f;vehicles.input.parts.push_back(p);}vehicles.input.vehicles[0].part_count=vehicles.input.scene.part_count=uint32_t(vehicles.input.parts.size());}
        if(variant==2)for(auto &p:vehicles.input.parts)p.health=.35f;
        for(unsigned n=0;n<10;++n){const auto &camera=views[n];source.base.camera_x=camera.eye.x;source.base.camera_y=camera.eye.y;metadata.view.eye_height=camera.eye.z;vehicles.observe(camera.eye);
            gpu::packet packet;require(cache.build(source,camera.yaw,camera.pitch,{},packet,statistics,current.metrics(),{},&metadata,nullptr,1280,800,NCMM_GPU_FORCE),"unchanged production packet builder");
            if(variant>=3){const auto saved_scene=packet.scene;const auto saved_cells=packet.cells;packet.reset();packet.scene=saved_scene;packet.scene.world_epoch=variant+101;packet.cells=saved_cells;
                const unsigned count=variant==3?64:5;
                for(unsigned j=0;j<count;++j){auto mesh=grid();mesh.materials[0].color[0]=.1f+float(j%8)*.12f;mesh.materials[0].color[1]=.1f+float((j/8)%4)*.2f;mesh.materials[0].color[2]=.2f+float(j%5)*.15f;
                    if(variant==4){if(j==0)mesh.flags=NCMM_GPU_DYNAMIC_MESH;if(j==1)mesh.materials[0].color[3]=.5f;if(j==2)for(auto &vertex:mesh.vertices)vertex.uv[0]=vertex.uv[1]=0;if(j==3)mesh.materials[0].finish=12;if(j==4){mesh.vertices.resize(3);mesh.indices={0,1,2};}}
                    mesh.key=gpu::mesh_key(mesh);auto owner=std::make_shared<gpu::owned_mesh>(std::move(mesh));packet.mesh(owner);
                    ncmm_gpu_draw_v1 draw{};draw.mesh_key=owner->key;draw.transform=gpu::identity();draw.transform.columns[12]=12.3f;draw.transform.columns[13]=11.3f;draw.transform.columns[14]=.7f+(variant==4?float(j)*.1f:0);draw.cell=287;draw.bone_count=1;draw.pass=owner->materials[0].color[3]<1?NCMM_GPU_TRANSPARENT:NCMM_GPU_OPAQUE;packet.draws.push_back(draw);}}
            const auto &submission=packet.finish(nullptr);const std::vector<ncmm_gpu_draw_v1> original_draws(submission.draws,submission.draws+submission.draw_count);
            const auto submit=[&](ncmm_gpu::backend &engine){require(engine.submit(submission),engine.metrics().reason);return read(engine);};
            observation a,b;if((variant+n)%2){b=submit(current);a=submit(original);}else{a=submit(original);b=submit(current);}
            require(!std::memcmp(original_draws.data(),submission.draws,original_draws.size()*sizeof(original_draws[0])),"exact public source draw keys and order retained");
            uint64_t saved=0,packed_meshes=0;for(uint32_t m=0;m<submission.mesh_count;++m){const auto &input=submission.meshes[m];const auto *before=original.resources().mesh(input.key),*after=current.resources().mesh(input.key);
                require(before&&after&&!before->compacted,"reference cache uses complete original vertices for every identical public key");
                require(before->cpu_indices==after->cpu_indices&&before->vertex_data().size()==after->vertex_data().size()&&!std::memcmp(before->vertex_data().data(),after->vertex_data().data(),before->vertex_data().size()*sizeof(ncmm_gpu_vertex_v1)),"complete original CPU picking arrays remain equal");
                if(after->compacted){++packed_meshes;saved+=(after->vertex_data().size()-after->gpu_vertex_data().size())*sizeof(ncmm_gpu_vertex_v1);}}
            if(variant<4)require(saved>0&&packed_meshes>0,"image comparison positively exercises actual compact GPU allocations");else require(saved==0&&packed_meshes==0,"all explicit unsupported meshes use original GPU bytes");
            if(saved)++active;else ++fallback;saved_total+=saved;size_t different=0;for(size_t p=0;p<a.pixels.size();++p)different+=a.pixels[p]!=b.pixels[p];
            const bool match=!different&&a.focus==b.focus&&a.target==b.target;equal&=match;total_different+=different;++pairs;
            if(!first)report<<",";
            first=false;report<<"{\"variant\":"<<variant<<",\"view\":"<<n<<",\"saved_vertex_bytes\":"<<saved<<",\"packed_meshes\":"<<packed_meshes<<",\"pixels_equal\":"<<(different?"false":"true")<<",\"different_pixels\":"<<different<<",\"focus_equal\":"<<(a.focus==b.focus?"true":"false")<<",\"target_equal\":"<<(a.target==b.target?"true":"false")<<"}";
            if(!match)std::fprintf(stderr,"Vertex pack mismatch variant=%u view=%u pixels=%zu focus=%d/%d target=%d/%d\n",variant,n,different,a.focus,b.focus,a.target,b.target);
        }
    }
    require(pairs==50&&active==40&&fallback==10&&saved_total>0,"all original-size views,64 coplanar colors and explicit fallbacks exercised");
    original.clear();current.clear();const auto before_resources=checks;resources(device);const auto lifecycle=checks-before_resources;
    report<<"],\"pair_count\":"<<pairs<<",\"active_pairs\":"<<active<<",\"fallback_pairs\":"<<fallback<<",\"saved_vertex_bytes_sum\":"<<saved_total<<",\"resource_lifecycle_checks\":"<<lifecycle<<",\"different_pixels\":"<<total_different<<",\"success\":"<<(equal?"true":"false")<<"}\n";report.close();require(bool(report),"complete exact-image and resource receipt");
    SDL_SetRenderTarget(renderer,nullptr);SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);require(SDL_WaitForGPUIdle(device),"qualification teardown");SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
    require(equal,"all complete color pixels, focus and target equal with no weakened original primary controls");std::printf("vertex pack device: %llu checks,50 full1280x800 pairs,40 active,10 fallback; exact pixels,stable coplanar order and real resource lifecycle\n",(unsigned long long)checks);
}
