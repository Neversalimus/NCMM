#include "gpu_scene.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace first_person::gpu {
struct scene_signature_probe {
    static size_t retained_content_bytes(const scene_cache &cache){return cache.normalized_bytes();}
    static size_t content_cell_limit(){return scene_cache::normalized_budget/(scene_cache::normalized_page_bytes+scene_cache::normalized_page_cells+sizeof(cache_record_pointer))*scene_cache::normalized_page_cells;}
    using cache_record_pointer=std::unique_ptr<scene_cache::normalized_record[]>;
    static size_t content_update_bytes(const scene_cache &cache){return cache.signature_content_updates*sizeof(scene_cache::normalized_record);}
    static std::vector<uint64_t> run(scene_cache &cache,const ncmm_graphics_scene_v2 &s,
        const ncmm_graphics_multilevel_scene_v1 *world,bool reference=false,bool creatures=true,int width=1768,const hybrid::presentation_data *metadata=nullptr){
        if(reference)cache.signature_cached=false; // original full 27/9-neighbour oracle
        const ncmm_world::coordinates xyz(world,true);ncmm_gpu_metrics_v1 host{};
        return cache.signatures(s,metadata,host,creatures,width,world,xyz);
    }
};
}

namespace {
int checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
struct fixture {
    ncmm_graphics_multilevel_scene_v1 world{};
    std::vector<ncmm_graphics_cell_v1> cells;
    std::vector<ncmm_graphics_cell_v2> details;
    std::vector<ncmm_world_cell_v1> coordinates;
    std::vector<ncmm_world_slice_v1> slices;
    void make(int width,int height,int low,int high,int32_t ox,int32_t oy,int holes=0,int sector=0){
        cells.clear();details.clear();coordinates.clear();slices.clear();
        world={};world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;world.level_height=2.5f;
        world.origin_x=ox;world.origin_y=oy;world.loaded_min_z=low;world.loaded_max_z=high;
        world.appearance.struct_size=sizeof(world.appearance);world.appearance.environment.daylight=1;
        world.appearance.base.struct_size=sizeof(ncmm_graphics_scene_v1);world.appearance.base.grid_width=width;world.appearance.base.grid_height=height;
        world.appearance.base.pixel_width=1768;world.appearance.base.pixel_height=1084;world.appearance.base.camera_x=width*.5f;world.appearance.base.camera_y=height*.5f;
        for(int z=low;z<=high;++z){
            const auto first=uint32_t(coordinates.size());
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                const uint32_t ax=uint32_t(int64_t(ox)+x),ay=uint32_t(int64_t(oy)+y);
                if(holes&&(ax*7+ay*11+uint32_t(z)*13)%uint32_t(holes)==0)continue;
                if(sector&&((x+y+sector)%19==0))continue;
                cells.emplace_back();details.emplace_back();auto &cell=cells.back();auto &detail=details.back();
                cell.flags=NCMM_GFX_VISIBLE;cell.light=1;std::strcpy(cell.terrain,"t_floor");
                detail.light_r=detail.light_g=detail.light_b=1;std::strcpy(detail.ground,"t_floor");
                coordinates.push_back({int32_t(int64_t(ox)+x),int32_t(int64_t(oy)+y),z,
                    NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM|NCMM_WORLD_BODY_VISIBLE,0,0,1,{.3f,.4f,.5f}});
            }
            slices.push_back({z,first,uint32_t(coordinates.size())-first,0,1,1});
        }
        bind();
    }
    void bind(){world.coordinates=coordinates.data();world.cell_count=uint32_t(coordinates.size());world.slices=slices.data();world.slice_count=uint32_t(slices.size());
        world.appearance.base.cells=cells.data();world.appearance.base.cell_count=cells.size();world.appearance.details=details.data();world.appearance.detail_count=details.size();}
};
}

int main(){try{
    using first_person::gpu::scene_cache;using first_person::gpu::scene_signature_probe;
    scene_cache cached,reference;fixture f;
    first_person::hybrid::presentation_data metadata;bool with_metadata=false;
    auto compare=[&](bool creatures=true,int width=1768,bool multilevel=true){
        f.bind();const auto *world=multilevel?&f.world:nullptr;
        if(with_metadata)metadata.cells.resize(f.cells.size());
        const auto *extra=with_metadata?&metadata:nullptr;
        const auto actual=scene_signature_probe::run(cached,f.world.appearance,world,false,creatures,width,extra);
        const auto expected=scene_signature_probe::run(reference,f.world.appearance,world,true,creatures,width,extra);
        check(actual==expected,"incremental result must match the original full ordered hash, including missing neighbours");
        check(actual.size()==f.cells.size(),"all current cells retain an index");
        check(scene_signature_probe::content_update_bytes(cached)<=512u*1024u,"cold and changed content updates stay within the same per-call byte budget");
        check(cached.signature_content_allocation_bytes<=512u*1024u,"page allocations have the same explicit per-call byte limit");
    };
    auto warm=[&](){for(int round=0;round<96;++round){compare();if(cached.signature_content_hashes==0)return;}throw std::runtime_error("bounded content cache did not warm");};
    f.make(97,97,0,0,10000,-20000);compare();
    check(cached.signature_content_hashes==9409&&cached.signature_content_reuses==0,"cold content is fully hashed before retention");
    const auto full_queries=cached.signature_neighbor_queries;check(full_queries==27*9409,"cold path is the original full pass");
    warm();
    compare();check(cached.signature_neighbor_queries==0&&cached.signature_reused_cells==9409,"unchanged snapshot reuses all exact signatures");
    check(cached.signature_content_hashes==0&&cached.signature_content_reuses==9409,"unchanged owned bytes avoid all content rehashing");
    f.cells[48*97+48].flags|=NCMM_GFX_WALL;compare();
    check(cached.signature_neighbor_queries==27*9&&cached.signature_reused_cells==9400,"one changed cell rehashes exactly its present neighbours");
    check(cached.signature_neighborhood_marks==9,"one-storey invalidation marks its bounded present plane");
    check(cached.signature_content_hashes==1&&cached.signature_content_reuses==9408,"one static edit rehashes only its changed content");
    f.make(97,97,0,0,10000,-20000);compare();
    f.make(97,97,0,0,10001,-20000);compare();
    const auto shifted_queries=cached.signature_neighbor_queries;
    check(shifted_queries<full_queries/8,"one-tile viewport shift avoids most neighbour lookups");
    check(cached.signature_content_hashes==97&&cached.signature_content_reuses==97*96,"viewport shift retains content only by the exact owned absolute position");
    for(auto &cell:f.cells)cell.flags|=NCMM_GFX_WALL;
    compare();check(cached.signature_neighbor_queries==full_queries,"scene-wide edits use the original full pass");
    warm();
    for(auto &cell:f.cells)cell.light=.15f;
    for(auto &detail:f.details){detail.light_r=.1f;detail.light_g=.2f;detail.light_b=.3f;detail.emission=.5f;}
    compare();check(cached.signature_neighbor_queries==0,"lighting changes keep existing normalized static semantics");
    check(cached.signature_content_hashes==0&&cached.signature_content_reuses==9409,"normalized dynamic lighting does not rehash static content");
    // Retained state is owned data. Replace every input allocation and object.
    f=fixture{};f.make(97,97,0,0,10001,-20000);for(auto &cell:f.cells)cell.flags|=NCMM_GFX_WALL;compare();
    check(cached.signature_neighbor_queries==0,"cache never depends on old callback addresses");
    with_metadata=true;compare();warm();
    auto &middle=metadata.cells[48*97+48];middle.terrain_material=NCMM_MAT_WOOD;compare();
    check(cached.signature_neighbor_queries==27*9,"presentation material invalidates only its old exact neighbourhood");
    middle.source_light=15;middle.source_r=.3f;middle.hostile=1;middle.creature_height=2;compare();
    check(cached.signature_neighbor_queries==0,"existing dynamic light/creature normalization remains unchanged");
    middle.gas=NCMM_GAS_SMOKE;middle.field_intensity=2;std::strcpy(middle.ceiling,"t_floor_wood");compare();
    middle.ceiling[127]='x';compare();
    ++metadata.view.occupied_vehicle;compare();
    check(cached.signature_neighbor_queries==full_queries,"occupied-vehicle changes preserve the full reference invalidation");
    check(cached.signature_content_hashes==9409,"occupied-vehicle identity participates in every retained metadata record");
    with_metadata=false;compare();with_metadata=true;compare();warm();compare();
    check(cached.signature_content_hashes==0&&cached.signature_content_reuses==9409,"metadata presence transitions never reuse an incompatible content hash");
    for(int levels:{1,3,21,32}){
        cached.clear();reference.clear();
        const int low=-1,high=low+levels-1;
        f.make(13,17,low,high,INT32_MIN,INT32_MAX-17,5);compare();
        for(int step=0;step<24;++step){
            f.make(13,17,low,high,INT32_MIN+step%7,INT32_MAX-17-step%3,5,step%4);compare();
            if(f.cells.size()>8){
                const auto i=f.cells.size()/2;
                f.coordinates[i].flags^=NCMM_WORLD_FLOOR_BOTTOM;compare();
                f.coordinates[i].direction=uint32_t(step%4);std::strcpy(f.cells[i].furniture,"f_chair");compare();
                f.details[i].vehicle_features=NCMM_VEH_ROOF;f.details[i].vehicle_x=4.5f;f.details[i].vehicle_y=8.5f;
                f.details[i].vehicle_yaw=.3f;f.details[i].mount_x=1;f.details[i].mount_y=-1;compare(true,800);
                compare(false,2480);
            }
        }
        f.make(13,17,low,high,INT32_MAX-13,INT32_MIN,3);compare(); // same-epoch extreme teleport
    }
    std::uint32_t random=0x15161314u;
    auto next=[&](){random=random*1664525u+1013904223u;return random;};
    f.make(17,19,-1,1,-50000,30000,7);cached.clear();reference.clear();compare();
    for(int frame=0;frame<1600;++frame){
        if(frame%23==0){f.make(17,19,-1,1,-50000+int(next()%9),30000+int(next()%9),int(3+next()%7),int(next()%8));}
        else if(!f.cells.empty()){
            const auto i=size_t(next()%f.cells.size());
            switch(next()%8){
                case 0:f.cells[i].flags^=NCMM_GFX_NO_FLOOR;break;
                case 1:f.coordinates[i].flags^=NCMM_WORLD_OPAQUE;break;
                case 2:std::strcpy(f.cells[i].terrain,frame%2?"t_floor_wood":"t_floor");break;
                case 3:std::strcpy(f.cells[i].creature,frame%2?"mon_zombie":"mon_dog");break;
                case 4:std::strcpy(f.details[i].field,frame%2?"fd_smoke":"fd_null");break;
                case 5:f.details[i].light_r=float(next()%256)/255;break;
                case 6:f.coordinates[i].flags^=NCMM_WORLD_FLOOR_TOP;break;
                case 7:f.cells[i].terrain[127]=char(next()&127);break; // trailing bytes remain significant
            }
        }
        if(frame%97==0){cached.clear();reference.clear();}
        if(with_metadata&&!metadata.cells.empty())metadata.cells[next()%metadata.cells.size()].field_intensity=next()%4;
        compare((frame/19)%2==0,frame%31==0?800:1768);
    }
    // Legacy dense-grid and sparse XYZ modes cannot share identity accidentally.
    for(int frame=0;frame<20;++frame){f.make(17,19,0,0,frame,-frame);compare(true,1768,frame%2==0);}
    f.make(3,3,0,0,0,0);compare();f.cells[4].flags|=NCMM_GFX_WALL;compare();
    check(scene_signature_probe::retained_content_bytes(cached)==0,"small scenes release the optional content storage");
    const int tall=int(scene_signature_probe::content_cell_limit()/(97*97))+1;
    f.make(97,97,0,tall-1,0,0);compare();
    check(cached.signature_content_reuses==0&&cached.signature_content_hashes==f.cells.size(),"over-budget scenes use the complete original content hashing path");
    check(scene_signature_probe::retained_content_bytes(cached)==0,"large scenes release content storage instead of exceeding its budget");
    f.make(97,97,0,0,0,0);compare();warm();compare();
    check(scene_signature_probe::retained_content_bytes(cached)<=32u*1024u*1024u&&cached.signature_content_hashes==0,"retention can resume after a bounded fallback");
    for(int height:{53,54}){f.make(92,height,0,5,-10000,-20000);compare();
        check(scene_signature_probe::retained_content_bytes(cached)<=32u*1024u*1024u,"validity-mask capacity growth is included in the strict owned-byte limit");}
    cached.clear();check(cached.bytes()==0,"world/resource clear releases every added owned array");
    std::cout<<"Scene signature differential checks="<<checks<<"; 97x97 first="<<full_queries
        <<" lookups, one-column shift="<<shifted_queries<<"; full hashes identical\n";
    return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
