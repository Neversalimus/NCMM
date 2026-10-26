#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

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
    const bool owner_only=argc>1&&!std::strcmp(argv[1],"--texture-owners");texture_ownership(require,owner_only);
    if(owner_only){std::printf("GPU texture ownership: %d checks PASS\n",checks);return 0;}
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
