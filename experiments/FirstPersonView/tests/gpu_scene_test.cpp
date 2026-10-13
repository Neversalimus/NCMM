#include "ncmm_graphics_performance.hpp"
#include "ncmm_gpu_backend.hpp"
#include "gpu_scene.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc,char **argv){
    auto require=[](bool ok,const char *why){if(!ok){std::fprintf(stderr,"Full GPU scene failed: %s (%s)\n",why,SDL_GetError());std::exit(1);}};
    require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");SDL_Window *window=nullptr;
#if defined(_WIN32)
    window=SDL_CreateWindow("NCMM GPU scene/skin qualification",640,480,SDL_WINDOW_HIDDEN);require(window!=nullptr,"native window");
#endif
    auto *device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL,false,nullptr);require(device!=nullptr,"real GPU API device; fallback prohibited");
    auto *renderer=SDL_CreateGPURenderer(device,window);require(renderer!=nullptr,"shared SDL renderer");
    auto *ui=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,640,480);require(ui&&SDL_SetRenderTarget(renderer,ui),"SDL UI target");
    ncmm_gpu::backend engine;require(engine.bind(renderer,1),"borrow existing device");
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
    require(engine.submit(scene),engine.metrics().reason);auto first=engine.metrics();require(first.active&&first.triangles==2&&first.cpu_raster_calls==0,"GPU triangles, hardware depth, CPU raster excluded");
    require(engine.has(11,0),"persistent GPU mesh resource");require(first.target_cell==0,"geometric picking matches visible near creature");
    auto capture=[&](const char *suffix){require(SDL_RenderTexture(renderer,engine.output(),nullptr,nullptr),"GPU-to-GPU final composition");
        SDL_FRect strip{0,0,640,20};require(SDL_SetRenderDrawColor(renderer,0,0,255,255)&&SDL_RenderFillRect(renderer,&strip),"ordinary SDL UI");
        auto *raw=SDL_RenderReadPixels(renderer,nullptr);require(raw!=nullptr,"test-only readback");auto *image=SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888);SDL_DestroySurface(raw);require(image!=nullptr,"capture format");
        if(argc>1)require(SDL_SaveBMP(image,(std::string(argv[1])+suffix+".bmp").c_str()),"save real GPU scene");
        auto center=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+240*image->pitch)[320];
        auto top=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+10*image->pitch)[320];require(((top>>8)&255)>240,"SDL UI preserved");SDL_DestroySurface(image);return center;};
    auto center=capture("-near");require((center>>24)>120&&((center>>16)&255)<20,"near red wins over later far green in full pipeline");
    require(first.draw_calls==4,"dry scene bypasses empty volume and post-composition passes");
    auto completed=engine.metrics();require(completed.completed_frame_serial>=1&&completed.visible_creature_pixels>1000,"fenced small GPU visibility counter");
    scene.mesh_count=0;scene.meshes=nullptr;scene.frame_serial=2;bones[1].columns[12]=5;
    require(engine.submit(scene),engine.metrics().reason);auto second=engine.metrics();require(second.active&&second.generated_meshes==0&&second.upload_bytes<1024,"only dynamic data uploaded for skeletal animation");
    require(second.target_cell==-1,"bone animation updates exact picking without CPU pixel raster");center=capture("-skin");require(((center>>16)&255)>120&&(center>>24)<20,"GPU vertex skinning moves near mesh; far green becomes visible");
    require(engine.metrics().completed_frame_serial>=2,"animated frame fence completes without a main-path wait");
    scene.mode=NCMM_GPU_CPU;require(!engine.submit(scene)&&!engine.metrics().active,"explicit CPU selection never impersonates GPU");
    scene.mode=NCMM_GPU_FORCE;scene.frame_serial=3;bones[1].columns[12]=0;require(engine.submit(scene),engine.metrics().reason);require(engine.metrics().generated_meshes==0,"GPU resumes with cached geometry");
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
    center=capture("-dynamic");require((center>>24)>120&&((center>>8)&255)<20,"updated dynamic buffer moves transparent geometry");
    for(auto &vertex:glass_vertices)vertex.position[0]-=5;scene.frame_serial=6;require(engine.submit(scene),engine.metrics().reason);
    center=capture("-return");require((center>>24)>80&&(center>>24)<135&&((center>>8)&255)>30,"current dynamic bounds restore geometry entering the frustum");
    auto evict=scene;evict.mesh_count=evict.draw_count=0;ncmm_gpu_metrics_v1 eviction{};
    require(engine.resources().prepare(evict,1,eviction)&&engine.resources().bytes()==0&&!engine.has(11,0),"shrinking viewport cache allowance evicts unused warm resources");
    std::array<ncmm_gpu_mesh_v1,2> restore={mesh,dynamic};scene.meshes=restore.data();scene.mesh_count=2;scene.frame_serial=7;
    require(engine.submit(scene)&&engine.metrics().generated_meshes==2,"bounded cache eviction restores exact scene resources");
    scene.width=1280;scene.height=800;scene.camera.projection[0]=1280;scene.camera.projection[1]=800;scene.camera.projection[2]=914;scene.frame_serial=8;scene.mesh_count=0;
    // Dynamic meshes are still supplied after resizing; immutable world stays cached.
    scene.meshes=&dynamic;scene.mesh_count=1;
    require(engine.submit(scene)&&engine.metrics().generated_meshes==0&&engine.metrics().resource_bytes<=NCMM_GPU_RESOURCE_LIMIT,"viewport resize preserves world cache and allocation budget");
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
    if(argc>1){std::ofstream out(std::string(argv[1])+".json");out<<"{\"success\":true,\"full_gpu_pipeline\":true,\"hardware_depth\":true,\"gpu_skinning\":true,\"impostor_colour_minification\":true,\"cpu_raster_calls\":0,\"animation_upload_bytes\":"<<second.upload_bytes<<",\"resource_bytes\":"<<second.resource_bytes<<",\"software_device\":"<<second.software_device<<",\"backend\":\""<<second.backend<<"\",\"device\":\""<<second.device<<"\"}\n";require(bool(out),"device evidence");}
    std::printf("Full GPU scene PASS: %s / %s; animation upload %llu bytes; CPU raster=0\n",second.backend,second.device,(unsigned long long)second.upload_bytes);
    engine.clear();require(!engine.bind(nullptr,2)&&!engine.available(),"unsupported renderer fails closed without claiming GPU");
    require(engine.bind(renderer,3),"same owned device can recover after fallback");engine.clear();
    SDL_DestroyTexture(ui);SDL_DestroyRenderer(renderer);SDL_DestroyGPUDevice(device);if(window)SDL_DestroyWindow(window);SDL_Quit();
}
