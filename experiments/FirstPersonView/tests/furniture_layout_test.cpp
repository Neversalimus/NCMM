#include "world_visual_fixture.hpp"
#include "ncmm_gpu_validation.hpp"
#include <iostream>
using namespace world_visual;
using namespace first_person;
int main(int argc,char **argv){try{
    const std::string prefix=argc>1?argv[1]:"";
    const char *storage[]={"f_filing_cabinet","f_bookcase","f_locker","f_fridge","f_washer","f_sink","f_toilet","f_cabinet","f_vending_c","f_piano"};
    for(const auto *id:storage)for(unsigned side=0;side<4;++side){
        std::array<furniture::sample,4> around{};for(auto &n:around)n.known=true;
        around[(side+2)&3u].wall=around[(side+2)&3u].blocked=true;
        require(furniture::front(id,around)==side,"storage fronts face the passage with backs at all four wall orientations");
        around[side].known=false;around[side].wall=around[side].blocked=false;
        require(furniture::front(id,around)==side,"an unknown tile never invents a wall or changes the cabinet back");
    }
    for(unsigned side=0;side<4;++side){std::array<furniture::sample,4> around{};for(auto &n:around)n.known=true;
        around[side].kind=furniture::role::work;require(furniture::front("f_chair",around)==side,"chair faces the adjacent work surface");
        around[side].kind=furniture::role::seat;require(furniture::front("f_desk",around)==side,"desk working edge faces its chair");
        around[side].blocked=true;require(furniture::front("f_chair",around)!=side,"opaque terrain wins over an inaccessible desk cue");
        around={};require(furniture::front(std::array<const char *,4>{"f_chair_east","f_chair_south","f_chair_west","f_chair_north"}[side],around)==side,"explicit mod-facing identifiers survive neighbour inference");
    }
    // Rows along each wall retain an open front, with no camera dependent yaw.
    for(unsigned side=0;side<4;++side){std::array<furniture::sample,4> around{};for(auto &n:around)n.known=true;
        around[(side+2)&3u].wall=around[(side+2)&3u].blocked=true;
        for(unsigned n:{(side+1)&3u,(side+3)&3u})around[n].kind=furniture::role::storage;
        require(furniture::front("f_filing_cabinet",around)==side,"entire filing rows face the aisle");}
    {std::array<furniture::sample,4> around{};for(auto &n:around)n.known=true;
        around[2].kind=furniture::role::divider;around[2].blocked=true;
        require(furniture::front("f_filing_cabinet",around)==0,"a single cubicle cabinet faces away from its partition");}
    fixture f;const int at=f.sample();
    for(unsigned mask=0;mask<16;++mask){f.object(at,"f_room_divider");
        for(unsigned side=0;side<4;++side){const int n=at+furniture::dx(side)+furniture::dy(side)*f.size;
            f.cells[n].furniture[0]=0;f.details[n].furniture_shape=NCMM_SHAPE_FLAT;if(mask&(1u<<side))f.object(n,"f_room_divider");}
        const auto geometry=collect(f);hybrid::vec lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
        for(const auto &t:geometry.triangles)if(t.m.cell==at&&t.v[0].p.z>.05f)for(const auto &v:t.v){
            lo={std::min(lo.x,v.p.x),std::min(lo.y,v.p.y),std::min(lo.z,v.p.z)};hi={std::max(hi.x,v.p.x),std::max(hi.y,v.p.y),std::max(hi.z,v.p.z)};}
        require(hi.z>1.8f&&hi.z<2.0f,"room divider is tall furniture, never a low voxel block");
        if(mask==0||mask==5||mask==10)require(std::min(hi.x-lo.x,hi.y-lo.y)<.13f,"straight divider has a thin physical section");
        if(!prefix.empty()&&(mask==0||mask==3||mask==7))save(f.render({},-hybrid::pi*.5f,-.12f),prefix+"-divider-"+std::to_string(mask));
    }
    // Existing one-neighbour GPU invalidation must replace the changed row,
    // retain unchanged chunks and exactly share repeat furniture/floor cells.
    fixture room(17);std::set<unsigned> chairs;
    for(int y=2;y<14;y+=3)for(int x=2;x<14;x+=3){const int i=y*room.size+x;room.object(i,"f_chair");chairs.insert(unsigned(i));}
    gpu::scene_cache cache;gpu::packet cold;hybrid::frame stats;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
    require(cache.build(room.scene,-hybrid::pi*.5f,-.2f,room.provider(),cold,stats,host,{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"cold furniture layout");
    require(!ncmm_gpu::validate(cold.finish(nullptr)),"unchanged GPU contract accepts retained prop templates");
    require(cache.prop_template_builds==1&&cache.prop_template_reuses==chairs.size()-1,"repeated chairs expand one complete floor/model template");
    require(cache.prop_source_triangles==props::get(props::id::chair).count,"only one source chair is triangulated");
    std::set<uint64_t> models;uint64_t old=0;const unsigned changed=*chairs.begin();
    for(const auto &d:cold.draws)if(chairs.count(d.cell)&&d.pass==NCMM_GPU_OPAQUE){models.insert(d.mesh_key);if(d.cell==changed)old=d.mesh_key;}
    require(models.size()==1,"same floor/model cells share one mesh and one opaque draw per cell");
    room.object(int(changed)+1,"f_desk");gpu::packet edited;
    require(cache.build(room.scene,-hybrid::pi*.5f,-.2f,room.provider(),edited,stats,host,{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"neighbour desk changes layout");
    uint64_t now=0;for(const auto &d:edited.draws)if(d.cell==changed&&d.pass==NCMM_GPU_OPAQUE)now=d.mesh_key;
    require(now&&now!=old,"placing a desk invalidates and turns the adjacent chair");
    gpu::packet warm;require(cache.build(room.scene,.7f,-.2f,room.provider(),warm,stats,host,{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"warm camera change");
    require(cache.generated_cells==0&&cache.prop_source_triangles==0&&cache.prop_template_builds==0,"warm camera pose rebuilds no furniture");
    cache.clear();require(cache.prop_template_builds==0&&cache.prop_source_triangles==0,"resource reset clears template accounting");
    std::cout<<"Furniture layout: "<<checks<<" checks PASS; no Native/window/input calls\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
