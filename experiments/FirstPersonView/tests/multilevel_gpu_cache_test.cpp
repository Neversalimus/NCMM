#include "gpu_scene.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

int main(){try{
    int checks=0;auto check=[&](bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);};
    for(int levels:{1,3,21,32}){
        ncmm_graphics_multilevel_scene_v1 world{};world.origin_x=INT32_MIN;world.origin_y=INT32_MAX-6;world.loaded_min_z=-10;world.loaded_max_z=-10+levels-1;
        world.appearance.base.grid_width=9;world.appearance.base.grid_height=7;std::vector<ncmm_world_cell_v1> coordinates;
        for(int z=world.loaded_min_z;z<=world.loaded_max_z;++z)for(int y=0;y<7;++y)for(int x=0;x<9;++x)
            if((x*7+y*13+z*17)%5!=0)coordinates.push_back({world.origin_x+x,world.origin_y+y,z,0,0,0,1});
        world.coordinates=coordinates.data();world.cell_count=uint32_t(coordinates.size());
        ncmm_world::coordinates indexed(&world,true),reference(&world);
        for(uint32_t i=0;i<world.cell_count;++i)for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            check(indexed.neighbor(i,dx,dy,dz)==reference.neighbor(i,dx,dy,dz),"indexed sparse holes and extreme canonical coordinates match the original lookup");
        world.loaded_max_z=world.loaded_min_z+32;ncmm_world::coordinates invalid(&world,true);
        check(invalid.bytes()==0&&invalid.neighbor(0,0,0)==-1,"invalid index span cannot allocate or reveal a neighbour");
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

        // Renderer identifiers end at NUL. Bytes after it are not an item,
        // texture or shape change and must not force a static rebuild.
        auto padding=[](auto &text){auto end=std::find(std::begin(text),std::end(text),'\0');
            if(end!=std::end(text))std::fill(end+1,std::end(text),char(0x5a));};
        for(size_t i=0;i<cells.size();++i){padding(cells[i].terrain);padding(cells[i].furniture);padding(cells[i].item);padding(cells[i].creature);
            padding(details[i].ground);padding(details[i].field);}
        first_person::gpu::packet padded;
        check(build(padded)&&!cache.generated_cells,"unused identifier padding does not rebuild static geometry");
        auto static_keys=[](const first_person::gpu::packet &packet){std::vector<std::tuple<uint64_t,uint32_t,uint32_t>> keys;
            for(const auto &draw:packet.draws)if(draw.pass!=NCMM_GPU_HELD)keys.emplace_back(draw.mesh_key,draw.cell,draw.pass);
            std::sort(keys.begin(),keys.end());return keys;};
        first_person::gpu::scene_cache independent;first_person::gpu::packet fresh;
        check(independent.build(world.appearance,0,-.4f,{},fresh,statistics,host,{},nullptr,nullptr,160,120,NCMM_GPU_FORCE,&world),"fresh padded source collection builds");
        check(static_keys(lit)==static_keys(padded)&&static_keys(padded)==static_keys(fresh),"cached and independently rebuilt physical meshes ignore identifier padding identically");

        coordinates.back().flags&=~NCMM_WORLD_FLOOR_BOTTOM;first_person::gpu::packet changed;
        check(build(changed)&&cache.generated_cells>0,"changed physical floor permissions invalidate neighbours");
        std::strcpy(cells.back().terrain,"t_grass");std::strcpy(details.back().ground,"t_grass");first_person::gpu::packet renamed;
        check(build(renamed)&&cache.generated_cells>0,"actual identifier bytes still invalidate physical meshes");
    }
    std::cout<<"Sparse multilevel GPU cache: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
