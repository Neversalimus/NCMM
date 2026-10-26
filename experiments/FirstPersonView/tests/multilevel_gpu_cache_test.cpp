#include "gpu_scene.hpp"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace index_allocation_probe {
thread_local size_t expected=0,count=0,bytes=0;
void observe(size_t size)noexcept{
    // MSVC overallocates a large vector by at most 39 bytes for alignment.
    // The controlled sparse fixture has no other allocation of this size.
    if(expected&&size>=expected&&size<=expected+128){++count;bytes+=size;}
}
struct scope {
    explicit scope(size_t size){count=bytes=0;expected=size;}
    ~scope(){expected=0;}
};
}
void *operator new(size_t size){if(void *value=std::malloc(size?size:1)){index_allocation_probe::observe(size);return value;}throw std::bad_alloc();}
void *operator new[](size_t size){return ::operator new(size);}
void operator delete(void *value)noexcept{std::free(value);}
void operator delete[](void *value)noexcept{::operator delete(value);}
void operator delete(void *value,size_t)noexcept{::operator delete(value);}
void operator delete[](void *value,size_t)noexcept{::operator delete(value);}

namespace {
struct index_fixture {
    ncmm_graphics_multilevel_scene_v1 world{};
    std::vector<ncmm_graphics_cell_v1> cells;
    std::vector<ncmm_graphics_cell_v2> details;
    std::vector<ncmm_world_cell_v1> positions;
    std::vector<ncmm_world_slice_v1> slices;
    index_fixture(){
        world.struct_size=sizeof(world);world.version=1;world.world_epoch=11;world.revision=1;world.level_height=2.5f;
        world.origin_x=-10000;world.origin_y=20000;world.loaded_min_z=-10;world.loaded_max_z=10;
        auto &s=world.appearance;s.struct_size=sizeof(s);s.environment.daylight=1;s.environment.sun_z=1;
        s.base={sizeof(s.base),160,120,97,97,48.5f,48.5f,nullptr,0};
        for(int y=46;y<=50;++y)for(int x=46;x<=50;++x)if((x+y)%5)
            positions.push_back({world.origin_x+x,world.origin_y+y,0,
                NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM|NCMM_WORLD_BODY_VISIBLE,0,0,1,{.2f,.2f,.2f}});
        bind();
    }
    void bind(){
        cells.resize(positions.size());details.resize(positions.size());
        for(size_t i=0;i<positions.size();++i){cells[i]={};details[i]={};cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;
            std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
        slices.clear();uint32_t current=0;
        for(int z=world.loaded_min_z;z<=world.loaded_max_z;++z){const auto first=current;while(current<positions.size()&&positions[current].z==z)++current;slices.push_back({z,first,current-first,0,uint64_t(z+129),1});}
        world.coordinates=positions.data();world.cell_count=uint32_t(positions.size());world.slices=slices.data();world.slice_count=uint32_t(slices.size());
        world.appearance.base.cells=cells.data();world.appearance.base.cell_count=cells.size();world.appearance.details=details.data();world.appearance.detail_count=details.size();
    }
    size_t dense_bytes()const{return size_t(world.loaded_max_z-world.loaded_min_z+1)*world.appearance.base.grid_width*world.appearance.base.grid_height*sizeof(int32_t);}
};
uint64_t packet_digest(const first_person::gpu::packet &packet,const ncmm_gpu_scene_v1 &view){
    using first_person::hash_bytes;uint64_t h=1469598103934665603ull;
    for(uint32_t i=0;i<view.mesh_count;++i)h=hash_bytes(h,&view.meshes[i].key,sizeof(view.meshes[i].key));
    for(uint32_t i=0;i<view.texture_count;++i)h=hash_bytes(h,&view.textures[i].key,sizeof(view.textures[i].key));
    h=hash_bytes(h,packet.draws.data(),packet.draws.size()*sizeof(ncmm_gpu_draw_v1));
    h=hash_bytes(h,packet.bones.data(),packet.bones.size()*sizeof(ncmm_gpu_matrix_v1));
    return hash_bytes(h,packet.cells.data(),packet.cells.size()*sizeof(ncmm_gpu_cell_v1));
}
template<class Check> void retained_index_checks(Check check,bool benchmark){
    using first_person::gpu::scene_cache;
    index_fixture f;scene_cache cached;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
    ncmm_world::coordinate_index_storage lookup;
    struct mesh_snapshot {uint64_t key;std::vector<ncmm_gpu_vertex_v1> vertices;std::vector<uint32_t> indices;std::vector<ncmm_gpu_material_v1> materials;};
    struct result {uint64_t digest;size_t allocations,allocated_bytes;std::array<uint64_t,5> parts;
        std::vector<uint64_t> meshes;std::vector<ncmm_gpu_draw_v1> draws;std::vector<ncmm_gpu_vertex_v1> first_world_vertices;
        std::vector<mesh_snapshot> world_meshes;ncmm_gpu_camera_v1 camera;};
    auto build=[&](scene_cache &cache){first_person::gpu::packet packet;first_person::hybrid::frame stats;bool accepted=false;
        {index_allocation_probe::scope allocation(f.dense_bytes());accepted=cache.build(f.world.appearance,0,-.4f,{},packet,stats,host,{},nullptr,nullptr,160,120,NCMM_GPU_FORCE,&f.world);}
        check(accepted,"retained-index sparse scene remains accepted");
        const auto &view=packet.finish(nullptr);result out{packet_digest(packet,view),index_allocation_probe::count,index_allocation_probe::bytes,{},{},packet.draws,{},{},view.camera};
        for(uint32_t i=0;i<view.mesh_count;++i)out.meshes.push_back(view.meshes[i].key);
        for(uint32_t i=0;i<view.mesh_count;++i){const auto &m=view.meshes[i];
            if(std::none_of(packet.draws.begin(),packet.draws.end(),[&](const auto &draw){return draw.pass!=NCMM_GPU_HELD&&draw.mesh_key==m.key;}))continue;
            mesh_snapshot copy{m.key,{},{},{}};copy.vertices.assign(m.vertices,m.vertices+m.vertex_count);copy.indices.assign(m.indices,m.indices+m.index_count);copy.materials.assign(m.materials,m.materials+m.material_count);out.world_meshes.push_back(std::move(copy));}
        const auto first=std::find_if(packet.draws.begin(),packet.draws.end(),[](const auto &draw){return draw.pass!=NCMM_GPU_HELD;});
        if(first!=packet.draws.end())for(uint32_t i=0;i<view.mesh_count;++i)if(view.meshes[i].key==first->mesh_key){out.first_world_vertices.assign(view.meshes[i].vertices,view.meshes[i].vertices+view.meshes[i].vertex_count);break;}
        uint64_t textures=1469598103934665603ull;for(uint32_t i=0;i<view.texture_count;++i)textures=first_person::hash_bytes(textures,&view.textures[i].key,sizeof(uint64_t));
        out.parts={{first_person::hash_bytes(1469598103934665603ull,out.meshes.data(),out.meshes.size()*sizeof(uint64_t)),textures,
            first_person::hash_bytes(1469598103934665603ull,packet.draws.data(),packet.draws.size()*sizeof(ncmm_gpu_draw_v1)),
            first_person::hash_bytes(1469598103934665603ull,packet.bones.data(),packet.bones.size()*sizeof(ncmm_gpu_matrix_v1)),
            first_person::hash_bytes(1469598103934665603ull,packet.cells.data(),packet.cells.size()*sizeof(ncmm_gpu_cell_v1))}};
        return out;};
    auto compare=[&](const char *label){f.bind();const ncmm_world::coordinates retained(&f.world,lookup),binary(&f.world);
        for(uint32_t i=0;i<f.world.cell_count;++i)for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
            check(retained.neighbor(i,x,y,z)==binary.neighbor(i,x,y,z),"retained mapping matches the exact binary-search oracle for all 27 neighbours");
        check(retained.bytes()==0&&lookup.bytes()>=f.dense_bytes()+f.positions.size()*3*sizeof(int32_t),"callback adapter owns no Host data while storage accounts for dense slots and copied positions");
        const auto current=build(cached);scene_cache reference;const auto expected=build(reference);
        if(current.digest!=expected.digest){
            std::cerr<<"Index differential transition: "<<label<<"; actual "<<current.digest<<", expected "<<expected.digest
                     <<"; mesh counts "<<current.meshes.size()<<"/"<<expected.meshes.size()<<"; draws "<<current.draws.size()<<"/"<<expected.draws.size()<<'\n';
            const std::array<const char *,5> names{{"meshes","textures","draws","bones","cells"}};
            for(size_t i=0;i<names.size();++i)if(current.parts[i]!=expected.parts[i])std::cerr<<"  "<<names[i]<<": "<<current.parts[i]<<"/"<<expected.parts[i]<<'\n';
            for(size_t i=0;i<std::min(current.draws.size(),expected.draws.size());++i)if(std::memcmp(&current.draws[i],&expected.draws[i],sizeof(ncmm_gpu_draw_v1))){
                const auto &a=current.draws[i],&b=expected.draws[i];std::cerr<<"  first different draw "<<i<<": cell "<<a.cell<<"/"<<b.cell<<", pass "<<a.pass<<"/"<<b.pass<<", mesh "<<a.mesh_key<<"/"<<b.mesh_key<<'\n';break;}
            for(size_t i=0;i<std::min(current.first_world_vertices.size(),expected.first_world_vertices.size());++i)if(std::memcmp(&current.first_world_vertices[i],&expected.first_world_vertices[i],sizeof(ncmm_gpu_vertex_v1))){
                const auto &a=current.first_world_vertices[i],&b=expected.first_world_vertices[i];std::cerr.precision(10);
                std::cerr<<"  first world vertex "<<i<<": position "<<a.position[0]<<','<<a.position[1]<<','<<a.position[2]<<" / "<<b.position[0]<<','<<b.position[1]<<','<<b.position[2]<<"; uv "<<a.uv[0]<<','<<a.uv[1]<<" / "<<b.uv[0]<<','<<b.uv[1]<<'\n';break;}
        }
        check(current.digest==expected.digest,"retained lookup preserves exact packet geometry, resources, draws, poses and cell data");return current;};
    const auto cold=compare("cold");const auto warm=compare("unchanged");
    std::cout<<"Dense index allocations: cold="<<cold.allocations<<" ("<<cold.allocated_bytes<<" bytes), warm="<<warm.allocations<<" ("<<warm.allocated_bytes<<" bytes)\n";
    check(cold.allocations==1&&warm.allocations==0,"unchanged sparse input must not allocate and refill the complete loaded-Z index each frame");
    auto replacement=f.positions;const auto *former=f.positions.data();f.positions.swap(replacement);
    check(f.positions.data()!=former,"callback replacement uses a genuinely new source allocation");
    for(auto &p:replacement){p.x=INT32_MIN;p.y=INT32_MIN;p.z=-128;}
    check(compare("new callback allocation").allocations==0,"equal owned positions reuse the index after old callback coordinates are overwritten");
    ++f.world.revision;f.world.camera_dx=.1f;f.world.camera_dz=.2f;
    check(compare("camera offset and Native revision").allocations==0,"camera and Native source revisions alone do not rebuild position lookup");
    f.positions[3].flags^=NCMM_WORLD_FLOOR_BOTTOM;f.positions[3].direction=2;f.positions[3].lower_light[0]=.8f;
    check(compare("floor permission, direction and light").allocations==0,"flags, light and direction changes retain the exact unchanged position mapping");
    // Keep the count but change one hole. Every later index is reassigned.
    auto first=f.positions.front();f.positions.erase(f.positions.begin());first.x=f.world.origin_x+51;first.y=f.world.origin_y+50;f.positions.push_back(first);
    compare("same-count hole with reassigned indices");
    std::swap(f.positions[0],f.positions[1]);f.bind();first_person::gpu::packet rejected;first_person::hybrid::frame stats;
    check(!cached.build(f.world.appearance,0,-.4f,{},rejected,stats,host,{},nullptr,nullptr,160,120,NCMM_GPU_FORCE,&f.world),"noncanonical ordering is rejected before using a retained index");
    check(!lookup.prepare(&f.world)&&lookup.bytes()==0,"noncanonical input cannot retain or expose a previous lookup");
    std::swap(f.positions[0],f.positions[1]);compare("canonical order restored");
    f.positions.erase(f.positions.begin()+3);compare("removed cell reassigns later indices");
    ++f.world.world_epoch;compare("world epoch");
    f.world.loaded_min_z=-16;f.world.loaded_max_z=15;compare("loaded Z grows to32");
    f.world.loaded_min_z=-1;f.world.loaded_max_z=1;compare("loaded Z shrinks to3");
    f.world.appearance.base.grid_height=96;compare("grid height");
    for(const auto &origin:std::array<std::array<int32_t,2>,3>{{{{INT32_MIN,INT32_MAX-97}},{{INT32_MAX-97,INT32_MIN}},{{-10000,20000}}}}){
        const int64_t dx=int64_t(origin[0])-f.world.origin_x,dy=int64_t(origin[1])-f.world.origin_y;
        for(auto &p:f.positions){p.x=int32_t(int64_t(p.x)+dx);p.y=int32_t(int64_t(p.y)+dy);}f.world.origin_x=origin[0];f.world.origin_y=origin[1];compare("extreme origin XY");}
    // origin_z changes the current physical adapter, not the integer mapping.
    // The former callback-local index reproduced the same 7e-8 slab rounding:
    // (at.z-.08)-at.z has different float bits at0 and-2.5. Preserve exact
    // mapping/topology/materials/transforms without forcing a cold rebuild
    // solely to change those pre-existing sub-micrometre local vertex bits.
    f.world.origin_z=1;f.bind();const ncmm_world::coordinates moved(&f.world,lookup),binary(&f.world);
    for(uint32_t i=0;i<f.world.cell_count;++i){const auto origin=moved.origin(i),expected=binary.origin(i);
        check(origin.x==expected.x&&origin.y==expected.y&&origin.z==expected.z&&origin.z==(f.positions[i].z-f.world.origin_z)*f.world.level_height,"physical origin Z comes from the current callback rather than retained scene data");
        for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)check(moved.neighbor(i,x,y,z)==binary.neighbor(i,x,y,z),"physical origin rebasing preserves exact integer neighbours");}
    const auto rebased=build(cached);scene_cache rebased_reference;const auto expected_rebased=build(rebased_reference);
    check(rebased.allocations==0&&rebased.meshes.size()==expected_rebased.meshes.size()&&rebased.draws.size()==expected_rebased.draws.size()&&
          rebased.parts[1]==expected_rebased.parts[1]&&rebased.parts[3]==expected_rebased.parts[3]&&rebased.parts[4]==expected_rebased.parts[4]&&
          !std::memcmp(&rebased.camera,&expected_rebased.camera,sizeof(ncmm_gpu_camera_v1)),"physical origin rebase preserves source cells, textures, poses and camera without refilling the integer index");
    float maximum_roundoff=0;
    for(size_t i=0;i<rebased.draws.size();++i){auto a=rebased.draws[i],b=expected_rebased.draws[i];
        if(a.pass==NCMM_GPU_HELD){check(!std::memcmp(&a,&b,sizeof(a)),"physical origin rebase preserves exact held submissions");continue;}
        const auto old_key=a.mesh_key,new_key=b.mesh_key;a.mesh_key=b.mesh_key=0;
        check(!std::memcmp(&a,&b,sizeof(a))&&a.cell<f.world.cell_count&&a.transform.columns[14]==(f.positions[a.cell].z-f.world.origin_z)*f.world.level_height,"physical origin rebase publishes the exact current draw transforms and cell ownership");
        const auto old_mesh=std::find_if(rebased.world_meshes.begin(),rebased.world_meshes.end(),[&](const auto &m){return m.key==old_key;});
        const auto new_mesh=std::find_if(expected_rebased.world_meshes.begin(),expected_rebased.world_meshes.end(),[&](const auto &m){return m.key==new_key;});
        check(old_mesh!=rebased.world_meshes.end()&&new_mesh!=expected_rebased.world_meshes.end(),"each rebased draw retains its own complete immutable mesh");
        check(old_mesh->vertices.size()==new_mesh->vertices.size()&&old_mesh->indices==new_mesh->indices&&old_mesh->materials.size()==new_mesh->materials.size()&&
              !std::memcmp(old_mesh->materials.data(),new_mesh->materials.data(),old_mesh->materials.size()*sizeof(ncmm_gpu_material_v1)),"physical origin rebase preserves exact mesh topology and materials");
        for(size_t n=0;n<old_mesh->vertices.size();++n){auto old_vertex=old_mesh->vertices[n],new_vertex=new_mesh->vertices[n];
            maximum_roundoff=std::max(maximum_roundoff,std::abs(old_vertex.position[2]-new_vertex.position[2]));old_vertex.position[2]=new_vertex.position[2];
            check(!std::memcmp(&old_vertex,&new_vertex,sizeof(old_vertex)),"physical origin rebase changes no vertex field other than established local Z rounding");}
    }
    check(maximum_roundoff<=1e-6f,"physical origin numeric difference stays below one micrometre, rather than a mapping error");
    std::cout<<"Existing physical-origin slab roundoff: maximum local Z delta="<<maximum_roundoff<<"; exact integer mapping/topology/materials/current transforms preserved\n";
    index_fixture short_range,tall_range;short_range.world.loaded_min_z=short_range.world.loaded_max_z=0;short_range.bind();
    tall_range.bind();scene_cache short_cache,tall_cache;f=std::move(short_range);f.bind();build(short_cache);
    f=std::move(tall_range);f.bind();build(tall_cache);
    check(tall_cache.bytes()>=short_cache.bytes()+20u*97*97*sizeof(int32_t),"producer cache metrics include every retained dense lookup slot");
    if(benchmark){
        constexpr unsigned frames=512;
        auto measure_fixture=[&](const index_fixture &input,const char *label){ncmm_world::coordinate_index_storage warm;warm.prepare(&input.world);
            auto measure=[&](bool reuse){uint64_t digest=0;const auto begun=std::chrono::steady_clock::now();
                {index_allocation_probe::scope allocation(input.dense_bytes());
                    for(unsigned i=0;i<frames;++i){
                        if(reuse){const ncmm_world::coordinates index(&input.world,warm);digest+=uint32_t(index.neighbor(i%input.world.cell_count,1,0));}
                        else{const ncmm_world::coordinates index(&input.world,true);digest+=uint32_t(index.neighbor(i%input.world.cell_count,1,0));}}}
                return std::make_tuple(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count(),digest,index_allocation_probe::count,index_allocation_probe::bytes);};
            const auto before=measure(false),after=measure(true);
            check(std::get<1>(before)==std::get<1>(after)&&std::get<2>(before)==frames&&std::get<2>(after)==0,"isolated index benchmark preserves lookup results while removing repeated dense allocations");
            std::cout<<"Coordinates index pure benchmark ["<<label<<", "<<input.world.cell_count<<" cells / 21 loaded levels]: "<<frames<<" calls, reference "<<std::get<0>(before)<<" ms / "<<std::get<2>(before)<<" allocations / "<<std::get<3>(before)
                     <<" bytes; retained "<<std::get<0>(after)<<" ms / "<<std::get<2>(after)<<" allocations / "<<std::get<3>(after)<<" bytes; lookup digest "<<std::get<1>(after)<<" unchanged\n";
        };
        measure_fixture(f,"sparse holes");
        index_fixture visible_floor;visible_floor.positions.clear();visible_floor.positions.reserve(97u*97u);
        for(int y=0;y<97;++y)for(int x=0;x<97;++x)visible_floor.positions.push_back({visible_floor.world.origin_x+x,visible_floor.world.origin_y+y,0,
            NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM|NCMM_WORLD_BODY_VISIBLE,0,0,1,{.2f,.2f,.2f}});
        visible_floor.bind();measure_fixture(visible_floor,"97x97 visible floor");
    }
    cached.clear();short_cache.clear();tall_cache.clear();lookup.clear();
    check(cached.bytes()==0&&short_cache.bytes()==0&&tall_cache.bytes()==0&&lookup.bytes()==0,"clear releases all retained index capacity and accounting");
}
}

int main(int argc,char **argv){try{
    int checks=0;auto check=[&](bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);};
    retained_index_checks(check,argc>1&&!std::strcmp(argv[1],"--index-benchmark"));
    for(int levels:{1,3,21,32}){
        ncmm_graphics_multilevel_scene_v1 world{};world.origin_x=INT32_MIN;world.origin_y=INT32_MAX-6;world.loaded_min_z=-10;world.loaded_max_z=-10+levels-1;
        world.appearance.base.grid_width=9;world.appearance.base.grid_height=7;std::vector<ncmm_world_cell_v1> coordinates;
        for(int z=world.loaded_min_z;z<=world.loaded_max_z;++z)for(int y=0;y<7;++y)for(int x=0;x<9;++x)
            if((x*7+y*13+z*17)%5!=0)coordinates.push_back({world.origin_x+x,world.origin_y+y,z,0,0,0,1});
        world.coordinates=coordinates.data();world.cell_count=uint32_t(coordinates.size());
        ncmm_world::coordinate_index_storage storage;
        ncmm_world::coordinates indexed(&world,true),reference(&world),retained(&world,storage);
        for(uint32_t i=0;i<world.cell_count;++i)for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            check(indexed.neighbor(i,dx,dy,dz)==reference.neighbor(i,dx,dy,dz)&&retained.neighbor(i,dx,dy,dz)==reference.neighbor(i,dx,dy,dz),"local and retained sparse holes and extreme canonical coordinates match the original lookup");
        world.loaded_max_z=world.loaded_min_z+32;ncmm_world::coordinates invalid(&world,true);
        check(invalid.bytes()==0&&invalid.neighbor(0,0,0)==-1,"invalid index span cannot allocate or reveal a neighbour");
        ncmm_world::coordinates retained_invalid(&world,storage);
        check(storage.bytes()==0&&retained_invalid.neighbor(0,0,0)==-1,"invalid loaded-Z span releases retained indices instead of exposing stale cells");
    }
    for(int count:{1,2,7,98,1940}){
        std::vector<ncmm_graphics_cell_v1> cells(count);
        std::vector<ncmm_graphics_cell_v2> details(count);
        std::vector<ncmm_world_cell_v1> coordinates;
        std::vector<ncmm_world_slice_v1> slices;
        for(int i=0;i<count;++i){
            const int x=count==7?i%3:i%97,y=count==7?0:i/97,z=count==7?i/3:0;
            cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
            details[i].light_r=details[i].light_g=details[i].light_b=1;std::strcpy(details[i].ground,"t_floor");
            coordinates.push_back({x,y,z,NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM|NCMM_WORLD_BODY_VISIBLE,0,0,1,{.2f,.2f,.2f}});
            if(slices.empty()||slices.back().z!=z)slices.push_back({z,uint32_t(i),0,0,1,1});
            ++slices.back().cell_count;
        }
        ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;world.level_height=2.5f;
        world.loaded_max_z=coordinates.back().z;world.cell_count=uint32_t(count);world.coordinates=coordinates.data();world.slice_count=uint32_t(slices.size());world.slices=slices.data();
        world.appearance.struct_size=sizeof(world.appearance);world.appearance.base={sizeof(ncmm_graphics_scene_v1),160,120,97,97,.5f,.5f,cells.data(),cells.size()};
        world.appearance.details=details.data();world.appearance.detail_count=details.size();world.appearance.environment.daylight=1;world.appearance.environment.sun_z=1;
        ncmm_world::coordinates indexed(&world,true),reference(&world);
        check(indexed.bytes()<=size_t(NCMM_WORLD_MAX_SLICES)*97*97*sizeof(int32_t),"neighbour index has a bounded frame lifetime");
        for(uint32_t i=0;i<world.cell_count;++i)for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            check(indexed.neighbor(i,dx,dy,dz)==reference.neighbor(i,dx,dy,dz),"indexed neighbour exactly matches sparse binary search including absent cells and levels");
        check(indexed.neighbor(UINT32_MAX,0,0,0)==-1,"invalid neighbour owner closes index");
        first_person::gpu::scene_cache cache;first_person::hybrid::frame statistics;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        auto build=[&](first_person::gpu::packet &packet){return cache.build(world.appearance,0,-.4f,{},packet,statistics,host,{},nullptr,nullptr,160,120,NCMM_GPU_FORCE,&world);};
        first_person::gpu::packet first;
        check(build(first),"sparse XYZ packet builds without dense-grid neighbour reads");
        check(cache.generated_cells==uint32_t(count),"each sparse XYZ cell has its own static cache entry");
        check(statistics.pixels.empty(),"GPU preparation never allocates a CPU framebuffer");
        for(const auto &draw:first.draws){if(draw.pass==NCMM_GPU_HELD)continue;check(draw.cell<world.cell_count,"GPU draw owns a sparse frame index");
            check(draw.transform.columns[14]==coordinates[draw.cell].z*2.5f,"GPU cache preserves physical Z translation");}
        ++world.revision;first_person::gpu::packet retained;
        check(build(retained)&&!cache.generated_cells,"unchanged notification reuses GPU static geometry");
        check(cache.signature_reuses==1,"unchanged geometry skips the complete 27-neighbour pass after a revision notification");
        cells.back().light=.7f;details.back().light_r=.7f;first_person::gpu::packet lit;
        check(build(lit)&&!cache.generated_cells&&cache.signature_reuses==2,"live lighting updates reuse static adjacency without rebuilding geometry");
        check(lit.cells.back().light[0]==.7f,"retained geometry still publishes the current light buffer");

        coordinates.back().flags&=~NCMM_WORLD_FLOOR_BOTTOM;first_person::gpu::packet changed;
        check(build(changed)&&cache.generated_cells>0,"changed physical floor permissions invalidate neighbours");
    }
    std::cout<<"Sparse multilevel GPU cache: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
