#include "gpu_scene.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

int main(){try{
    int checks=0;auto check=[&](bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);};
    for(int count:{1,2,7,98}){
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
        first_person::gpu::scene_cache cache;first_person::hybrid::frame statistics;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        auto build=[&](first_person::gpu::packet &packet){return cache.build(world.appearance,0,-.4f,{},packet,statistics,host,{},nullptr,nullptr,160,120,NCMM_GPU_FORCE,&world);};
        first_person::gpu::packet first;
        check(build(first),"sparse XYZ packet builds without dense-grid neighbour reads");
        check(cache.generated_cells==uint32_t(count),"each sparse XYZ cell has its own static cache entry");
        check(statistics.pixels.empty(),"GPU preparation never allocates a CPU framebuffer");
        for(const auto &draw:first.draws){check(draw.cell<world.cell_count,"GPU draw owns a sparse frame index");
            check(draw.transform.columns[14]==coordinates[draw.cell].z*2.5f,"GPU cache preserves physical Z translation");}
        ++world.revision;first_person::gpu::packet retained;
        check(build(retained)&&!cache.generated_cells,"unchanged notification reuses GPU static geometry");
        coordinates.back().flags&=~NCMM_WORLD_FLOOR_BOTTOM;first_person::gpu::packet changed;
        check(build(changed)&&cache.generated_cells>0,"changed physical floor permissions invalidate neighbours");
    }
    std::cout<<"Sparse multilevel GPU cache: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
