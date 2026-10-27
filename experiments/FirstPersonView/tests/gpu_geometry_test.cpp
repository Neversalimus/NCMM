#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>

namespace floor_allocation_probe {
thread_local bool active=false;
thread_local size_t calls=0,bytes=0;
void observe(size_t size)noexcept{if(active){++calls;bytes+=size;}}
struct scope {
    scope(){calls=bytes=0;active=true;}
    ~scope(){active=false;}
};
}
void *operator new(size_t size){if(void *value=std::malloc(size?size:1)){floor_allocation_probe::observe(size);return value;}throw std::bad_alloc();}
void *operator new[](size_t size){return ::operator new(size);}
void *operator new(size_t size,const std::nothrow_t &)noexcept{try{return ::operator new(size);}catch(...){return nullptr;}}
void *operator new[](size_t size,const std::nothrow_t &)noexcept{try{return ::operator new[](size);}catch(...){return nullptr;}}
void operator delete(void *value)noexcept{std::free(value);}
void operator delete[](void *value)noexcept{::operator delete(value);}
void operator delete(void *value,size_t)noexcept{::operator delete(value);}
void operator delete[](void *value,size_t)noexcept{::operator delete(value);}
void operator delete(void *value,const std::nothrow_t &)noexcept{::operator delete(value);}
void operator delete[](void *value,const std::nothrow_t &)noexcept{::operator delete[](value);}

namespace first_person::gpu {
struct floor_template_probe {
    static size_t entries(const scene_cache &cache){return cache.floor_templates.size();}
    static constexpr size_t limit(){return scene_cache::floor_template_limit;}
};
}

std::array<uint64_t,4> floor_packet_digest(const ncmm_gpu_scene_v1 &view){std::array<uint64_t,4> out{};out.fill(1469598103934665603ull);
    for(uint32_t i=0;i<view.mesh_count;++i){const auto &m=view.meshes[i];
        out[0]=first_person::hash_bytes(out[0],&m.key,sizeof(m.key));
        out[0]=first_person::hash_bytes(out[0],m.vertices,size_t(m.vertex_count)*sizeof(m.vertices[0]));
        out[0]=first_person::hash_bytes(out[0],m.indices,size_t(m.index_count)*sizeof(m.indices[0]));
        out[1]=first_person::hash_bytes(out[1],&m.key,sizeof(m.key));
        out[1]=first_person::hash_bytes(out[1],m.materials,size_t(m.material_count)*sizeof(m.materials[0]));}
    for(uint32_t i=0;i<view.texture_count;++i){const auto &t=view.textures[i];
        for(uint64_t value:{t.key,uint64_t(t.width),uint64_t(t.height),uint64_t(t.pitch)})out[2]=first_person::hash_bytes(out[2],&value,sizeof(value));
        for(uint32_t y=0;y<t.height;++y)out[2]=first_person::hash_bytes(out[2],t.rgba+size_t(y)*t.pitch,size_t(t.width)*sizeof(uint32_t));}
    out[3]=first_person::hash_bytes(out[3],view.draws,size_t(view.draw_count)*sizeof(view.draws[0]));
    out[3]=first_person::hash_bytes(out[3],view.bones,size_t(view.bone_count)*sizeof(view.bones[0]));
    out[3]=first_person::hash_bytes(out[3],view.cells,size_t(view.cell_count)*sizeof(view.cells[0]));
    out[3]=first_person::hash_bytes(out[3],&view.camera,sizeof(view.camera));return out;
}

template<class Require> void floor_template_benchmark(Require require){
    namespace gpu=first_person::gpu;
    // Fresh scene_cache instances measure cold producer work. Immutable input
    // construction, packet inspection and validation stay outside the clock
    // and ordinary operator-new allocation scope, identically before/after.
    constexpr int side=97,count=side*side,samples=9;
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);auto &base=scene.base;
    std::vector<ncmm_graphics_cell_v1> cells(count);std::vector<ncmm_graphics_cell_v2> details(count);
    base={sizeof(base),640,480,side,side,48.5f,48.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;scene.environment.sun_z=1;
    for(int i=0;i<count;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
        std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;
    world.origin_x=-20000003;world.origin_y=19999991;world.loaded_min_z=-10;world.loaded_max_z=10;world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    std::vector<ncmm_world_cell_v1> positions;positions.reserve(count);
    for(int y=0;y<side;++y)for(int x=0;x<side;++x)positions.push_back({world.origin_x+x,world.origin_y+y,0,
        NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP,0,0,1});
    ncmm_world_slice_v1 slice{0,0,count,0,1,1};world.coordinates=positions.data();world.cell_count=count;world.slices=&slice;world.slice_count=1;world.appearance=scene;
    ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;host.world_origin_x=world.origin_x;host.world_origin_y=world.origin_y;
    uint32_t pixel=0x998877ff;size_t sprite_calls=0;
    auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &sprite){++sprite_calls;sprite={sizeof(sprite),1,1,1,&pixel};return true;};
    struct result {double ms;size_t allocations,bytes,sprites;uint32_t meshes,draws,textures;
        std::array<uint64_t,4> digest;};
    auto percentile=[](std::vector<double> values,double fraction){std::sort(values.begin(),values.end());const double at=fraction*(values.size()-1);
        const size_t lo=size_t(at),hi=std::min(lo+1,values.size()-1);return values[lo]+(values[hi]-values[lo])*(at-lo);};
    for(bool multilevel:{false,true}){std::vector<double> times;result first{};
        const char *label=multilevel?"native-top-21Z":"legacy-flat";
        for(int sample=0;sample<samples;++sample){gpu::scene_cache cache;gpu::packet packet;first_person::hybrid::frame stats;
            const auto before_sprites=sprite_calls;bool accepted=false;double milliseconds=0;
            {floor_allocation_probe::scope allocation;const auto start=std::chrono::steady_clock::now();
                accepted=cache.build(multilevel?world.appearance:scene,0,-.2f,provider,packet,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr);
                milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
            const size_t allocations=floor_allocation_probe::calls,allocated_bytes=floor_allocation_probe::bytes;
            require(accepted,"cold floor benchmark publishes its complete first GPU packet");
            const auto &view=packet.finish(nullptr);ncmm_gpu_multilevel_scene_v1 wrapped{};wrapped.struct_size=sizeof(wrapped);wrapped.version=1;
            wrapped.base=view;wrapped.coordinates=world.coordinates;wrapped.cell_count=count;wrapped.origin_x=world.origin_x;wrapped.origin_y=world.origin_y;wrapped.level_height=world.level_height;
            require(!ncmm_gpu::validate(view,multilevel?&wrapped:nullptr),"cold floor benchmark obeys the existing GPU contract");
            require(cache.generated_cells==count&&stats.pixels.empty()&&stats.depth.empty(),"cold floor benchmark is complete producer work without CPU pixel rendering");
            const result current{milliseconds,allocations,allocated_bytes,sprite_calls-before_sprites,view.mesh_count,view.draw_count,view.texture_count,floor_packet_digest(view)};
            if(!sample)first=current;else require(current.digest==first.digest&&current.meshes==first.meshes&&current.draws==first.draws&&current.textures==first.textures,
                                                "repeated cold floor fixtures preserve exact meshes, materials, textures and commands");
            times.push_back(milliseconds);
            std::printf("Cold floor [%s] sample=%d ms=%.6f allocations=%zu allocated_bytes=%zu sprites=%zu meshes=%u draws=%u textures=%u geometry=%016llx materials=%016llx resources=%016llx commands=%016llx\n",
                        label,sample,milliseconds,current.allocations,current.bytes,current.sprites,current.meshes,current.draws,current.textures,
                        static_cast<unsigned long long>(current.digest[0]),static_cast<unsigned long long>(current.digest[1]),static_cast<unsigned long long>(current.digest[2]),static_cast<unsigned long long>(current.digest[3]));
            std::printf("Floor work [%s] sample=%d templates=%u reuses=%u source_triangles=%llu\n",label,sample,
                        cache.floor_template_builds,cache.floor_template_reuses,static_cast<unsigned long long>(cache.floor_source_triangles));
            cache.clear();require(cache.bytes()==0,"clearing the cold floor producer releases all retained state");
        }
        std::printf("Cold floor [%s] 97x97, %d fresh producer builds, median=%.6f p95=%.6f max=%.6f ms; ordinary operator-new scope; no game/GPU execution timing\n",
                    label,samples,percentile(times,.5),percentile(times,.95),*std::max_element(times.begin(),times.end()));
    }
}

template<class Require> void floor_guard_records(Require require,bool records=true){
    namespace gpu=first_person::gpu;namespace hybrid=first_person::hybrid;
    const std::array<const char *,20> cases{{"legacy-flat","legacy-furniture","legacy-field-flat","legacy-item","legacy-liquid-item",
        "legacy-vehicle","legacy-ceiling","legacy-procedural-flat","legacy-rail","legacy-atmosphere","legacy-liquid-override",
        "legacy-atlas-variants","legacy-missing-sprite","legacy-feather","native-top","native-bottom","native-rim","native-glass","native-water","native-stair"}};
    for(const std::string name:cases){constexpr int count=25,center=12;const bool multilevel=name.rfind("native-",0)==0;
        std::vector<ncmm_graphics_cell_v1> cells(count);std::vector<ncmm_graphics_cell_v2> details(count);
        ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);scene.base={sizeof(scene.base),640,480,5,5,2.5f,2.5f,cells.data(),cells.size()};
        scene.details=details.data();scene.detail_count=count;scene.environment.daylight=1;scene.environment.sun_z=1;
        hybrid::presentation_data metadata;metadata.view.struct_size=sizeof(metadata.view);metadata.view.eye_height=1.55f;metadata.view.body_scale=1;metadata.view.held_length=.6f;metadata.cells.resize(count);
        for(int i=0;i<count;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
            details[i].light_r=details[i].light_g=details[i].light_b=1;std::strcpy(details[i].ground,"t_floor");}
        if(name=="legacy-furniture"){std::strcpy(cells[center].furniture,"f_chair");details[center].furniture_shape=NCMM_SHAPE_SEAT;}
        if(name=="legacy-field-flat"){std::strcpy(details[center].field,"fd_blood");details[center].field_shape=NCMM_SHAPE_FLAT;}
        if(name=="legacy-item"||name=="legacy-liquid-item"){std::strcpy(cells[center].item,name=="legacy-item"?"rock":"blood");
            if(name=="legacy-liquid-item")metadata.cells[center].item_liquid=NCMM_LIQ_BLOOD;}
        if(name=="legacy-vehicle"){cells[center].flags|=NCMM_GFX_VEHICLE;details[center].vehicle_features=NCMM_VEH_FRAME;
            details[center].vehicle_group=1;details[center].vehicle_x=details[center].vehicle_y=2.5f;details[center].vehicle_health=1;}
        if(name=="legacy-ceiling")cells[center].flags|=NCMM_GFX_CEILING;
        // These IDs intentionally retain FLAT: procedural objects and rail
        // detail are emitted before the renderer's vertical-shape test.
        if(name=="legacy-procedural-flat")std::strcpy(cells[center].terrain,"t_sewage_pipe");
        if(name=="legacy-rail")std::strcpy(cells[center].terrain,"t_railroad_track_h");
        if(name=="legacy-atmosphere"){std::strcpy(details[center].field,"fd_smoke");details[center].field_shape=NCMM_SHAPE_BILLBOARD;
            metadata.cells[center].gas=NCMM_GAS_SMOKE;metadata.cells[center].field_intensity=3;}
        if(name=="legacy-liquid-override")metadata.cells[center].liquid=NCMM_LIQ_FRESH;
        if(name=="legacy-feather")for(int i=0;i<count;++i){const char *id=i%5<2?"t_pavement":"t_grass";std::strcpy(cells[i].terrain,id);std::strcpy(details[i].ground,id);}
        ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;
        world.origin_x=-113;world.origin_y=-197;world.loaded_min_z=0;world.loaded_max_z=1;world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
        std::vector<ncmm_world_cell_v1> positions;for(int y=0;y<5;++y)for(int x=0;x<5;++x)positions.push_back({world.origin_x+x,world.origin_y+y,0,
            NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP,0,0,1});
        if(name=="native-bottom")positions[center].flags|=NCMM_WORLD_FLOOR_BOTTOM;
        if(name=="native-rim"){positions[center+1].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_OPEN_DOWN;cells[center+1].flags|=NCMM_GFX_NO_FLOOR;std::strcpy(cells[center+1].terrain,"t_open_air");}
        if(name=="native-glass")positions[center].flags|=NCMM_WORLD_GLASS_FLOOR;
        if(name=="native-water"){positions[center].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_WATER|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_OPEN_DOWN;
            cells[center].flags|=NCMM_GFX_NO_FLOOR;std::strcpy(cells[center].terrain,"t_water_sh");details[center].terrain_shape=NCMM_SHAPE_WATER;metadata.cells[center].liquid=NCMM_LIQ_FRESH;}
        if(name=="native-stair"){std::strcpy(cells[center].terrain,"t_stairs_up");positions[center].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;}
        ncmm_world_slice_v1 slice{0,0,count,0,1,1};world.coordinates=positions.data();world.cell_count=count;world.slices=&slice;world.slice_count=1;world.appearance=scene;
        uint32_t pixels[3]={0x998877ff,0x446633ff,0x778899ff};size_t sprite_calls=0;
        auto provider=[&](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &sprite){++sprite_calls;
            if(name=="legacy-missing-sprite"&&i==center&&layer==NCMM_GFX_GROUND)return false;
            const unsigned variant=name=="legacy-atlas-variants"&&i<count&&layer==NCMM_GFX_GROUND?1+i%2:0;
            sprite={sizeof(sprite),1,1,1,&pixels[variant]};return true;};
        ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;host.world_origin_x=world.origin_x;host.world_origin_y=world.origin_y;
        gpu::scene_cache cache;gpu::packet packet;hybrid::frame stats;
        require(cache.build(multilevel?world.appearance:scene,0,-.2f,provider,packet,stats,host,{},&metadata,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr),"floor guard fixture publishes complete geometry");
        const auto &view=packet.finish(nullptr);ncmm_gpu_multilevel_scene_v1 wrapped{};wrapped.struct_size=sizeof(wrapped);wrapped.version=1;
        wrapped.base=view;wrapped.coordinates=world.coordinates;wrapped.cell_count=count;wrapped.origin_x=world.origin_x;wrapped.origin_y=world.origin_y;wrapped.level_height=world.level_height;
        require(!ncmm_gpu::validate(view,multilevel?&wrapped:nullptr)&&stats.pixels.empty(),"floor guard records are actual GPU producer packets without raster shortcuts");
        const auto digest=floor_packet_digest(view);const auto optimized_sprites=sprite_calls;
        gpu::scene_cache reference;reference.floor_templates_enabled=false;gpu::packet original;hybrid::frame reference_stats;
        require(reference.build(multilevel?world.appearance:scene,0,-.2f,provider,original,reference_stats,host,{},&metadata,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr),
                "unmodified triangle expansion remains a complete floor oracle");
        const auto &old=original.finish(nullptr);
        require(digest==floor_packet_digest(old)&&view.mesh_count==old.mesh_count&&view.draw_count==old.draw_count&&view.texture_count==old.texture_count&&sprite_calls-optimized_sprites==optimized_sprites,
                "floor templates preserve exact emitted geometry, materials, resources, commands and sprite resolution in guarded cells");
        unsigned expected=multilevel?9u:25u;
        if(name=="native-bottom"||name=="native-stair")expected=8;
        else if(name=="native-rim")expected=5;
        else if(name=="native-glass"||name=="native-water")expected=4;
        else if(!multilevel&&name!="legacy-flat"&&name!="legacy-atlas-variants"&&name!="legacy-missing-sprite"&&name!="legacy-feather")expected=24;
        require(cache.floor_template_builds+cache.floor_template_reuses==expected&&cache.floor_source_triangles==2ull*cache.floor_template_builds,
                "only empty opaque TOP cells bypass triangle expansion; real procedural FLAT objects and late field/item/volume work remain intact");
        require(!reference.floor_template_builds&&!reference.floor_template_reuses&&!gpu::floor_template_probe::entries(reference),
                "ablation uses the original generator without creating template state");
        if(records){std::printf("Floor guard [%s] meshes=%u draws=%u textures=%u sprites=%zu geometry=%016llx materials=%016llx resources=%016llx commands=%016llx\n",
                    name.c_str(),view.mesh_count,view.draw_count,view.texture_count,optimized_sprites,static_cast<unsigned long long>(digest[0]),static_cast<unsigned long long>(digest[1]),
                    static_cast<unsigned long long>(digest[2]),static_cast<unsigned long long>(digest[3]));
            std::printf("Floor guard work [%s] templates=%u reuses=%u source_triangles=%llu\n",name.c_str(),cache.floor_template_builds,cache.floor_template_reuses,
                        static_cast<unsigned long long>(cache.floor_source_triangles));}
    }
}

template<class Require> void floor_template_lifecycle(Require require){
    namespace gpu=first_person::gpu;namespace hybrid=first_person::hybrid;
    constexpr int side=5,count=side*side;
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);auto &base=scene.base;
    std::vector<ncmm_graphics_cell_v1> cells(count);std::vector<ncmm_graphics_cell_v2> details(count);
    base={sizeof(base),640,480,side,side,2.5f,2.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=count;scene.environment.daylight=1;scene.environment.sun_z=1;
    for(int i=0;i<count;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
        std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    uint32_t pixel=0x998877ff;size_t sprites=0;
    auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &sprite){++sprites;sprite={sizeof(sprite),1,1,1,&pixel};return true;};
    ncmm_gpu_metrics_v1 host{};host.world_origin_x=-20000003;host.world_origin_y=19999991;host.resource_epoch=1;
    ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;
    world.origin_x=host.world_origin_x;world.origin_y=host.world_origin_y;world.origin_z=-4;world.loaded_min_z=-4;world.loaded_max_z=3;world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    std::vector<ncmm_world_cell_v1> positions;for(int y=0;y<side;++y)for(int x=0;x<side;++x)positions.push_back({world.origin_x+x,world.origin_y+y,-3,
        NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP,0,0,1});
    ncmm_world_slice_v1 slice{-3,0,count,0,1,1};world.coordinates=positions.data();world.cell_count=count;world.slices=&slice;world.slice_count=1;world.appearance=scene;
    for(bool multilevel:{false,true}){gpu::scene_cache cache;hybrid::frame stats;
        auto build=[&](float yaw,float pitch,gpu::packet &packet){world.appearance=scene;return cache.build(multilevel?world.appearance:scene,yaw,pitch,provider,packet,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr);};
        // The oracle executes the same renderer with only the internal early
        // template hook disabled, including current live light and origins.
        auto matches=[&](gpu::packet &packet,float yaw,float pitch){gpu::scene_cache reference;reference.floor_templates_enabled=false;
            gpu::packet old;hybrid::frame old_stats;world.appearance=scene;
            require(reference.build(multilevel?world.appearance:scene,yaw,pitch,provider,old,old_stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr),"floor lifecycle reference publishes its complete packet");
            require(floor_packet_digest(packet.finish(nullptr))==floor_packet_digest(old.finish(nullptr)),"cold/warm/epoch/shifted floor packets exactly match ordinary triangle expansion");};
        gpu::packet cold;require(build(0,-.2f,cold),"floor lifecycle cold build succeeds");
        require(cache.floor_template_builds==1&&cache.floor_template_reuses==(multilevel?8u:24u),"uniform floors expand exactly one TOP template");matches(cold,0,-.2f);
        const auto cold_digest=floor_packet_digest(cold.finish(nullptr));
        const auto before=sprites;gpu::packet camera;require(build(.8f,.15f,camera),"floor lifecycle camera-only build succeeds");
        require(cache.generated_cells==0&&!cache.floor_template_builds&&!cache.floor_template_reuses&&sprites==before,"camera changes neither expand floors nor resolve atlas images");matches(camera,.8f,.15f);
        details[12].light_r=.2f;cells[12].light=.3f;gpu::packet light;require(build(.8f,.15f,light),"floor lifecycle dynamic light build succeeds");
        require(cache.generated_cells==0&&light.cells[12].light[0]==.2f&&!cache.floor_template_builds&&!cache.floor_template_reuses,"live floor light stays outside the immutable template");matches(light,.8f,.15f);
        ++host.resource_epoch;pixel=0x668844ff;gpu::packet epoch;require(build(.8f,.15f,epoch)&&cache.generated_cells==count&&cache.floor_template_builds==1,"resource epoch invalidates resolved floor templates and atlas owners");matches(epoch,.8f,.15f);
        require(floor_packet_digest(cold.finish(nullptr))==cold_digest,"already published floor resources retain their immutable atlas lease after epoch replacement");
        if(multilevel){++world.world_epoch;gpu::packet replacement;require(build(.8f,.15f,replacement)&&cache.generated_cells==count&&cache.floor_template_builds==1,"world replacement clears the previous floor recipe lifecycle");matches(replacement,.8f,.15f);}
        ++host.world_origin_x;++world.origin_x;for(auto &p:positions)++p.x;++world.revision;gpu::packet shifted;
        require(build(.8f,.15f,shifted),"floor lifecycle negative/far origin shift succeeds");matches(shifted,.8f,.15f);
        const auto bytes=cache.bytes();require(bytes>0&&gpu::floor_template_probe::entries(cache)<=gpu::floor_template_probe::limit(),"floor template memory accounting includes bounded live recipes");
        cache.clear();require(cache.bytes()==0&&!gpu::floor_template_probe::entries(cache),"world clear releases all floor template lookup state");
        for(bool texture_failure:{false,true}){gpu::scene_cache limited(texture_failure?gpu::resources::mesh_limit:1,texture_failure?0:gpu::resources::texture_limit);
            for(int attempt=0;attempt<2;++attempt){gpu::packet rejected;hybrid::frame failed_stats;world.appearance=scene;
                require(!limited.build(multilevel?world.appearance:scene,0,0,provider,rejected,failed_stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr),"floor mesh/atlas budget failure cannot become a partial warm scene");
                require(limited.bytes()==0&&!gpu::floor_template_probe::entries(limited)&&!limited.last_reason.empty(),"failed floor resources clear chunks, templates and owners while retaining diagnostics");}
            limited=gpu::scene_cache();gpu::packet recovered;hybrid::frame recovered_stats;
            require(limited.build(multilevel?world.appearance:scene,0,0,provider,recovered,recovered_stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE,multilevel?&world:nullptr)&&limited.generated_cells==count,
                    "restored floor budget reconstructs every cell after failure");matches(recovered,0,0);}
    }
    // Many actual ground atlases exceed the recipe bound. Clearing a weak
    // lookup must not alter any resource, floor draw or immutable owner.
    constexpr int many_side=33,many_count=many_side*many_side;
    cells.assign(many_count,{});details.assign(many_count,{});base={sizeof(base),640,480,many_side,many_side,16.5f,16.5f,cells.data(),cells.size()};
    scene.details=details.data();scene.detail_count=many_count;std::vector<uint32_t> pixels(many_count);
    for(int i=0;i<many_count;++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
        std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;pixels[i]=0x440000ffu+(uint32_t(i)<<8);}
    auto varied=[&](uint32_t i,uint32_t,ncmm_graphics_sprite_v2 &sprite){if(i>=pixels.size())return false;sprite={sizeof(sprite),1,1,1,&pixels[i]};return true;};
    gpu::scene_cache bounded,reference;reference.floor_templates_enabled=false;gpu::packet current,original;hybrid::frame stats,old_stats;
    require(bounded.build(scene,0,0,varied,current,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE)&&reference.build(scene,0,0,varied,original,old_stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),
            "more than 512 distinct floor recipes build complete packets");
    require(bounded.floor_template_builds==many_count&&bounded.floor_source_triangles==2ull*many_count&&gpu::floor_template_probe::entries(bounded)<=gpu::floor_template_probe::limit(),
            "distinct resolved materials retain a bounded weak template lookup");
    require(floor_packet_digest(current.finish(nullptr))==floor_packet_digest(original.finish(nullptr)),"recipe bound turnover preserves exact atlas leases, meshes and draw commands");
    bounded.clear();reference.clear();require(bounded.bytes()==0&&reference.bytes()==0&&!gpu::floor_template_probe::entries(bounded),"recipe-bound cleanup releases all retained lookup state");
}

template<class Require> void texture_ownership(Require require,bool benchmark){
    namespace gpu=first_person::gpu;
    constexpr unsigned triangle_count=4096;
    auto texture=std::make_shared<gpu::owned_texture>();texture->key=0x7465787475726531ull;
    texture->width=texture->height=1;texture->pixels={0x998877ff};
    gpu::mesh_builder legacy,current;unsigned legacy_growths=0,current_growths=0;
    ncmm_gpu_material_v1 material{};material.texture_key=texture->key;material.flags=NCMM_GPU_TEXTURED;
    material.color[0]=material.color[1]=material.color[2]=material.color[3]=1;
    const auto legacy_material=legacy.material(material),current_material=current.material(material);
    for(unsigned i=0;i<triangle_count;++i){
        first_person::hybrid::triangle triangle{};const float x=float(i%64),y=float(i/64);
        triangle.v={first_person::hybrid::vertex{{x,y,0},0,0,{}},
                    first_person::hybrid::vertex{{x+1,y,0},1,0,{}},
                    first_person::hybrid::vertex{{x,y+1,0},0,1,{}}};
        const auto old_capacity=legacy.mesh.textures.capacity(),new_capacity=current.mesh.textures.capacity();
        // Exact former collect() ownership path: append the already prepared
        // atlas owner once per triangle, even when its material is shared.
        legacy.mesh.textures.push_back(texture);current.texture(texture);
        legacy_growths+=legacy.mesh.textures.capacity()!=old_capacity;
        current_growths+=current.mesh.textures.capacity()!=new_capacity;
        legacy.triangle(triangle,legacy_material,{});current.triangle(triangle,current_material,{});
    }
    const auto legacy_digest=gpu::mesh_key(legacy.mesh),current_digest=gpu::mesh_key(current.mesh);
    require(legacy_digest==current_digest&&
            legacy.mesh.vertices.size()==current.mesh.vertices.size()&&
            !std::memcmp(legacy.mesh.vertices.data(),current.mesh.vertices.data(),current.mesh.vertices.size()*sizeof(ncmm_gpu_vertex_v1))&&
            legacy.mesh.indices==current.mesh.indices&&
            !std::memcmp(legacy.mesh.materials.data(),current.mesh.materials.data(),sizeof(ncmm_gpu_material_v1)),
            "texture ownership preserves exact geometry, indices, materials and resource key");
    const size_t legacy_owners=legacy.mesh.textures.size(),current_owners=current.mesh.textures.size();
    const size_t legacy_owner_bytes=legacy.mesh.textures.capacity()*sizeof(legacy.mesh.textures[0]),
                 current_owner_bytes=current.mesh.textures.capacity()*sizeof(current.mesh.textures[0]);
    legacy.mesh.key=legacy_digest;current.mesh.key=current_digest;
    auto old_mesh=std::make_shared<gpu::owned_mesh>(std::move(legacy.mesh)),
         new_mesh=std::make_shared<gpu::owned_mesh>(std::move(current.mesh));
    gpu::packet old_packet,new_packet;old_packet.draw(old_mesh,gpu::identity(),0,NCMM_GPU_OPAQUE);new_packet.draw(new_mesh,gpu::identity(),0,NCMM_GPU_OPAQUE);
    const auto &old_view=old_packet.finish(nullptr),&new_view=new_packet.finish(nullptr);
    require(old_view.mesh_count==1&&new_view.mesh_count==1&&old_view.texture_count==1&&new_view.texture_count==1&&
            old_view.meshes[0].key==new_view.meshes[0].key&&
            !std::memcmp(old_view.draws,new_view.draws,sizeof(ncmm_gpu_draw_v1))&&
            old_view.textures[0].key==new_view.textures[0].key&&old_view.textures[0].rgba[0]==new_view.textures[0].rgba[0],
            "texture ownership preserves exact draw and texture submission");
    std::printf("Texture owners: %zu -> %zu; pointer capacity bytes %zu -> %zu; vector growths %u -> %u; digest %016llx unchanged\n",
                legacy_owners,current_owners,legacy_owner_bytes,current_owner_bytes,legacy_growths,current_growths,
                static_cast<unsigned long long>(current_digest));
    if(benchmark){
        constexpr unsigned frames=512;
        auto measure=[&](const std::shared_ptr<const gpu::owned_mesh> &mesh){gpu::packet frame;uint64_t digest=0;
            const auto begun=std::chrono::steady_clock::now();
            for(unsigned i=0;i<frames;++i){frame.reset();frame.draw(mesh,gpu::identity(),0,NCMM_GPU_OPAQUE);const auto &view=frame.finish(nullptr);digest^=view.meshes[0].key+i;}
            return std::make_pair(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count(),digest);};
        const auto before=measure(old_mesh),after=measure(new_mesh);
        require(before.second==after.second,"warm ownership benchmark submits identical resources");
        std::printf("Texture-owner pure packet benchmark: %u frames, reference %.3f ms, current %.3f ms; owner visits %zu -> %zu\n",
                    frames,before.first,after.first,frames*legacy_owners,frames*current_owners);
    }
    require(legacy_owners==triangle_count&&current_owners==1,"each mesh retains each immutable texture once, independent of triangle count");
    require(current_owner_bytes<legacy_owner_bytes&&current_growths<legacy_growths,"duplicate atlas owners no longer grow retained mesh storage");
    gpu::mesh_builder shared;shared.texture(nullptr);
    require(shared.mesh.textures.empty(),"absent textures create no retained owners");
    shared.texture(texture);material.color[0]=.5f;shared.material(material,texture);material.color[0]=.7f;shared.material(material,texture);
    require(shared.mesh.materials.size()==2&&shared.mesh.textures.size()==1,"different materials sharing an atlas retain one owner");
    auto same_content=std::make_shared<gpu::owned_texture>(*texture);shared.texture(same_content);
    require(shared.mesh.textures.size()==1,"content identity, rather than callback address, deduplicates owned textures");
    auto normal=std::make_shared<gpu::owned_texture>(*texture);++normal->key;shared.texture(normal);
    auto depth=std::make_shared<gpu::owned_texture>(*texture);depth->key+=2;shared.texture(depth);
    require(shared.mesh.textures.size()==3,"independent colour, normal and depth resources retain all leases");
    gpu::mesh_builder lifetime;std::weak_ptr<const gpu::owned_texture> lease;
    {auto transient=std::make_shared<gpu::owned_texture>(*texture);lease=transient;lifetime.texture(transient);lifetime.texture(transient);}
    require(!lease.expired()&&lifetime.mesh.textures.size()==1,"deduplicated ownership survives callback-source release");
    lifetime.mesh={};require(lease.expired(),"destroying the mesh releases its last immutable texture lease");
}

int main(int argc,char **argv){int checks=0;auto require=[&](bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"GPU geometry: %s\n",message);std::exit(1);}};
    if(argc>1&&!std::strcmp(argv[1],"--floor-templates")){floor_template_benchmark(require);std::printf("GPU cold floor benchmark: %d checks PASS\n",checks);return 0;}
    if(argc>1&&!std::strcmp(argv[1],"--floor-guards")){floor_guard_records(require);std::printf("GPU floor guard records: %d checks PASS\n",checks);return 0;}
    if(argc>1&&!std::strcmp(argv[1],"--floor-lifecycle")){floor_template_lifecycle(require);std::printf("GPU floor lifecycle: %d checks PASS\n",checks);return 0;}
    const bool owner_only=argc>1&&!std::strcmp(argv[1],"--texture-owners");texture_ownership(require,owner_only);
    if(owner_only){std::printf("GPU texture ownership: %d checks PASS\n",checks);return 0;}
    floor_guard_records(require,false);floor_template_lifecycle(require);
    ncmm_graphics_scene_v2 scene{};scene.struct_size=sizeof(scene);auto &base=scene.base;base.struct_size=sizeof(base);base.grid_width=base.grid_height=5;base.cell_count=25;base.pixel_width=640;base.pixel_height=480;base.camera_x=base.camera_y=2.5f;
    std::vector<ncmm_graphics_cell_v1> cells(25);std::vector<ncmm_graphics_cell_v2> details(25);base.cells=cells.data();scene.details=details.data();scene.detail_count=25;
    for(int i=0;i<25;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    scene.environment.daylight=1;scene.environment.sun_z=1;
    uint32_t pixel=0x998877ff;int sprite_calls=0;auto provider=[&](uint32_t,uint32_t,ncmm_graphics_sprite_v2 &sprite){++sprite_calls;sprite={sizeof(sprite),1,1,1,&pixel};return true;};
    first_person::gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.world_origin_x=200;host.world_origin_y=300;host.resource_epoch=1;first_person::hybrid::frame stats;
    first_person::gpu::packet first;require(cache.build(scene,0,0,provider,first,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"owned scene extraction");const auto &a=first.finish(nullptr);
    require(!ncmm_gpu::validate(a),"valid GPU packet");require(stats.pixels.empty()&&stats.depth.empty()&&stats.effects.empty(),"GPU collection never allocates CPU pixel/depth/effect rasters");
    const uint32_t hand_meshes=a.mesh_count-1;
    std::printf("PSX packet: %u immutable hand meshes, %u draws, %u textures, %u matrices\n",hand_meshes,a.draw_count,a.texture_count,a.bone_count);
    require(cache.generated_cells==25&&hand_meshes>=2&&hand_meshes<=8&&a.draw_count==25+hand_meshes,"complete world and both palette-bounded PSX hands");
    require(a.texture_count==2,"floors and authored bare arms each share one immutable texture");
    const int before=sprite_calls;first_person::gpu::packet second;
    require(cache.build(scene,1.2f,.3f,provider,second,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"camera turn");const auto &b=second.finish(nullptr);
    require(cache.generated_cells==0&&sprite_calls==before,"camera turn does not generate world geometry or read atlas sprites");require(b.mesh_count==a.mesh_count&&b.meshes[0].key==a.meshes[0].key,"stable GPU resource keys");
    details[12].light_r=.2f;cells[12].light=.3f;first_person::gpu::packet light;
    require(cache.build(scene,1.2f,.3f,provider,light,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"dynamic light");require(cache.generated_cells==0&&light.cells[12].light[0]==.2f,"lighting changes only cell data");
    std::strcpy(cells[12].terrain,"t_wall");details[12].terrain_shape=NCMM_SHAPE_WALL;first_person::gpu::packet wall;
    require(cache.build(scene,1.2f,.3f,provider,wall,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"partial geometry update");require(cache.generated_cells==9,"one changed cell rebuilds only its local neighbour support");
    const auto &w=wall.finish(nullptr);require(w.mesh_count==2+hand_meshes&&w.draw_count==25+hand_meshes,"wall appears without erasing floors or hands");
    require(!ncmm_gpu::validate(w),"updated bounded packet");cache.clear();require(cache.bytes()==0,"F6/shutdown resource ownership clears");
    first_person::gpu::scene_cache limited(1024);
    for(int attempt=0;attempt<2;++attempt){first_person::gpu::packet rejected;
        require(!limited.build(scene,0,0,provider,rejected,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"failed mesh never becomes a silently incomplete warm GPU scene");
        require(limited.bytes()==0&&!limited.last_reason.empty(),"failed producer resources invalidate chunks and retain diagnostics");}
    std::strcpy(cells[12].terrain,"t_floor");details[12].terrain_shape=0;
    limited=first_person::gpu::scene_cache(); // Restore the deliberately exhausted test budget.
    first_person::gpu::packet recovered;
    require(limited.build(scene,0,0,provider,recovered,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE)&&limited.generated_cells==25&&limited.last_reason.empty(),"restored producer budget rebuilds the complete scene after resource failure");
    cache.clear();
    first_person::creature::presentation entities;entities.input.view={sizeof(ncmm_graphics_creature_scene_v1),1,1,1,10};
    std::strcpy(cells[7].creature,"mod_arbitrary_creature");
    uint64_t entity_id=0;
    for(const char *body:{"human","dog","spider","insect","centipede","bird","lizard","snake","fish","blob","robot","migo","unknown_mod_body"}){
        ncmm_graphics_creature_v1 entity{};entity.entity_id=++entity_id;entity.cell=7;entity.world_x=host.world_origin_x+2;entity.world_y=host.world_origin_y+1;
        entity.liters=70;entity.kilograms=65;std::strcpy(entity.bodytype,body);std::strcpy(entity.type_id,cells[7].creature);
        if(!std::strcmp(body,"centipede"))std::strcpy(entity.species,"CENTIPEDE");
        if(!std::strcmp(body,"robot"))entity.flags=NCMM_CR_MECHANICAL|NCMM_CR_IMMOBILE;
        entities.input.creatures.resize(1);entities.input.creatures[0]=entity;entities.observe();
        const auto key=first_person::creature::appearance_key(entity,1);
        entities.resources.request(key,first_person::creature::classify(entity),[&]{ncmm_graphics_sprite_v2 sprite{sizeof(sprite),1,1,1,&pixel};return first_person::creature::sprite::copy(sprite);});
        require(entities.resources.wait_idle(std::chrono::seconds(5)),"real procedural creature resource generation completes");
        for(int view=0;view<8;++view){entities.input.creatures[0].heading=view*first_person::hybrid::pi/4;entities.observe();first_person::gpu::packet animal;
            require(cache.build(scene,-first_person::hybrid::pi*.5f,0,provider,animal,stats,host,{},nullptr,&entities,640,480,NCMM_GPU_FORCE),"procedural creature collects into GPU geometry");
            const auto &data=animal.finish(nullptr);require(!ncmm_gpu::validate(data)&&stats.creature_lods[0]==1&&!stats.creature_fallback,"real near creature has a valid skinned GPU mesh");
            for(uint32_t n=0;n<data.mesh_count;++n){const auto &mesh=data.meshes[n];for(uint32_t j=0;j<mesh.index_count;j+=3){const auto material=mesh.vertices[mesh.indices[j]].material;
                require(material==mesh.vertices[mesh.indices[j+1]].material&&material==mesh.vertices[mesh.indices[j+2]].material,"anatomical seam triangles retain the CPU renderer's face material");}}
        }
    }
    cells[7].creature[0]=0;cache.clear();
    base.grid_width=base.grid_height=97;base.cell_count=9409;base.camera_x=base.camera_y=48.5f;cells.assign(9409,{});details.assign(9409,{});base.cells=cells.data();scene.details=details.data();scene.detail_count=9409;
    for(int i=0;i<9409;++i){cells[i].flags=NCMM_GFX_VISIBLE;std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
    first_person::gpu::packet cold;
    require(cache.build(scene,0,0,provider,cold,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"large cold scene is complete in its first GPU frame");
    require(cache.generated_cells==9409,"complete validated snapshot avoids any software warmup frame");
    require(!ncmm_gpu::validate(cold.finish(nullptr)),"complete cold scene remains within GPU packet limits");
    const int warmed_calls=sprite_calls;first_person::gpu::packet turned;require(cache.build(scene,2.5f,.4f,provider,turned,stats,host,{},nullptr,nullptr,640,480,NCMM_GPU_FORCE),"large camera turn stays ready");
    require(cache.generated_cells==0&&sprite_calls==warmed_calls,"97x97 camera turn reuses the complete static scene");require(stats.pixels.empty(),"warming does not use the CPU pixel renderer");
    std::printf("GPU geometry/cache: %d checks PASS\n",checks);
}
