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

        coordinates.back().flags&=~NCMM_WORLD_FLOOR_BOTTOM;first_person::gpu::packet changed;
        check(build(changed)&&cache.generated_cells>0,"changed physical floor permissions invalidate neighbours");
    }
    // A stationary camera revisits sparse sector boundaries during a full turn.
    // Their neighbour signatures differ even when all actual world data is
    // unchanged. Repeated sectors must reuse their exact geometry and remain
    // equivalent to an independent fresh collector after real topology changes.
    {
        constexpr int side=9;
        std::vector<ncmm_graphics_cell_v1> cells;
        std::vector<ncmm_graphics_cell_v2> details;
        std::vector<ncmm_world_cell_v1> coordinates;
        ncmm_world_slice_v1 slice{};
        ncmm_graphics_multilevel_scene_v1 world{};
        world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;world.level_height=2.5f;
        world.appearance.struct_size=sizeof(world.appearance);world.appearance.environment.daylight=1;world.appearance.environment.sun_z=1;
        ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        first_person::gpu::scene_cache cache;
        auto source=[&](bool half,bool changed){
            coordinates.clear();
            for(int y=0;y<side;++y)for(int x=0;x<side;++x)if(!half||x>=4){
                uint32_t flags=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM|NCMM_WORLD_BODY_VISIBLE;
                if(changed&&x==4&&y==4)flags&=~NCMM_WORLD_FLOOR_BOTTOM;
                coordinates.push_back({x,y,0,flags,0,0,1,{.2f,.2f,.2f}});
            }
            cells.assign(coordinates.size(),{});details.assign(coordinates.size(),{});
            for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;std::strcpy(cells[i].terrain,"t_floor");
                std::strcpy(details[i].ground,"t_floor");details[i].light_r=details[i].light_g=details[i].light_b=1;}
            world.appearance.base={sizeof(ncmm_graphics_scene_v1),160,120,side,side,4.5f,4.5f,cells.data(),cells.size()};
            world.appearance.details=details.data();world.appearance.detail_count=details.size();
            world.coordinates=coordinates.data();world.cell_count=uint32_t(coordinates.size());
            slice={0,0,world.cell_count,0,1,1};world.slices=&slice;world.slice_count=1;
        };
        auto topology=[&](const first_person::gpu::packet &packet){
            std::map<std::tuple<int32_t,int32_t,int32_t,uint32_t>,uint64_t> result;
            for(const auto &draw:packet.draws)if(draw.pass!=NCMM_GPU_HELD&&draw.cell<coordinates.size()){
                const auto &cell=coordinates[draw.cell];result[{cell.x,cell.y,cell.z,draw.pass}]=draw.mesh_key;
            }
            return result;
        };
        auto build=[&](first_person::gpu::scene_cache &owner,first_person::gpu::packet &packet,float yaw){
            first_person::hybrid::frame statistics;
            return owner.build(world.appearance,yaw,-.4f,{},packet,statistics,host,{},nullptr,nullptr,160,120,NCMM_GPU_FORCE,&world);
        };
        for(int repeat=0;repeat<4;++repeat)for(bool half:{false,true}){
            source(half,false);first_person::gpu::packet retained,fresh;first_person::gpu::scene_cache independent;
            check(build(cache,retained,float(repeat)*1.7f)&&build(independent,fresh,float(repeat)*1.7f),"sparse sector collectors build");
            check(topology(retained)==topology(fresh),"retained sparse sector exactly matches fresh physical meshes");
            if(repeat)check(cache.generated_cells==0,"revisited unchanged sector does not regenerate static cells");
        }
        source(false,true);first_person::gpu::packet changed,fresh;first_person::gpu::scene_cache independent;
        check(build(cache,changed,0)&&build(independent,fresh,0),"changed floor collectors build");
        check(cache.generated_cells>0&&topology(changed)==topology(fresh),"real floor permission change invalidates geometry and matches fresh collection");
        cache.clear();source(false,false);first_person::gpu::packet reset;
        check(build(cache,reset,0)&&cache.generated_cells==world.cell_count,"world/device reset cannot revive a stale geometry variant");
    }
    std::cout<<"Sparse multilevel GPU cache: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
