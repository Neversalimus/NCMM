#include "ncmm_world_policy.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::cerr<<message<<'\n';std::exit(1);}}
struct fixture {
    std::map<std::tuple<int,int,int>,uint32_t> flags;
    uint32_t operator()(ncmm_world::voxel p)const{
        auto it=flags.find({p.x,p.y,p.z});return it==flags.end()?uint32_t(NCMM_WORLD_UNKNOWN):it->second;
    }
    fixture(int levels){for(int z=-levels;z<=levels;++z)for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)flags[{x,y,z}]=0;}
};
}
int main(){
    using namespace ncmm_world;
    for(int eye_z=-10;eye_z<=10;++eye_z)for(int range=0;range<=8;++range)for(int z=-15;z<=15;++z){
        const bool reference=std::abs(z-eye_z)<=range||std::abs(z-1-eye_z)<=range;
        require(candidate_level_in_range(z,eye_z,range)==reference,"candidate level pruning retains every body and lower-slab range");
    }
    require(!candidate_level_in_range(0,0,-1),"negative native range closes candidates");
    {
        uint32_t random=0x89abcdef;
        auto next=[&](){random=random*1664525u+1013904223u;return random;};
        std::array<uint32_t,17*17> cells{};
        auto query=[&](voxel p){return p.x < -8||p.y < -8||p.x>8||p.y>8?uint32_t(NCMM_WORLD_UNKNOWN):cells[size_t(p.y+8)*17+size_t(p.x+8)];};
        const std::array<float,6> fractions={0.f,.00001f,.12f,.5f,.88f,.99999f};
        const std::array<float,9> heights={0.f,.0000001f,.000001f,.003f,.55f,1.55f,2.49999f,2.499999f,2.4999999f};
        for(int world=0;world<128;++world){
            for(auto &cell:cells){const auto kind=next();cell=(kind%17==0?uint32_t(NCMM_WORLD_UNKNOWN):kind%7==0?uint32_t(NCMM_WORLD_OPAQUE):0u)|NCMM_WORLD_FLOOR;}
            for(int ray=0;ray<256;++ray){
                const int z=ray%3-1;const float level=z*NCMM_WORLD_LEVEL_HEIGHT;
                const point eye{float(int(next()%15)-7)+fractions[next()%fractions.size()],float(int(next()%15)-7)+fractions[next()%fractions.size()],level+heights[next()%heights.size()]};
                const point target{float(int(next()%15)-7)+fractions[next()%fractions.size()],float(int(next()%15)-7)+fractions[next()%fractions.size()],level+heights[next()%heights.size()]};
                require(visible(eye,target,query,ray%2)==visible<false>(eye,target,query,ray%2),"horizontal fast path preserves general supercover visibility");
            }
        }
        int slab_reads=0;
        const auto glass=[&](voxel p){if(p.z==1)++slab_reads;return uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR);};
        require(visible({.3f,.4f,1.55f},{.3f,.4f,4.f},glass)&&slab_reads==2,"vertical crossing queries each distinct glass voxel once plus endpoint");
    }
    {
        uint32_t random=0x1234abcdu;auto next=[&](){random=random*1664525u+1013904223u;return random;};
        for(int plane:{0,1})for(int sample=0;sample<128;++sample){
            const point eye{.01f+float(next()%8950)/1000,.01f+float(next()%8950)/1000,.001f+float(next()%2498)/1000};
            auto closed=[&](voxel p){return p.z==plane?uint32_t(NCMM_WORLD_FLOOR):0u;};
            auto structure=[&](voxel p){return structural_flags(closed(p)|opaque_field_fact);};
            for(int z:plane?std::array<int,3>{2,3,4}:std::array<int,3>{-1,-2,-3}){
                const float x=.01f+float(next()%8950)/1000,y=.01f+float(next()%8950)/1000;
                for(float height:{.003f,1.25f,-.083f,.025f,NCMM_WORLD_LEVEL_HEIGHT-.025f,NCMM_WORLD_LEVEL_HEIGHT*.5f}){
                    const point target{x,y,z*NCMM_WORLD_LEVEL_HEIGHT+height};
                    require(!visible(eye,target,closed,true),"a sealed plane blocks every pruned body/slab/facade probe");
                    require(!visible(eye,target,structure,true),"structural field policy preserves the sealed slab proof");
                }
                require(!facade_visible(eye,{int(x),int(y),z},structure),"the sealed plane also blocks the complete facade probe set");
            }
        }
        require(full_opaque_slab(NCMM_WORLD_FLOOR)&&full_opaque_slab(NCMM_WORLD_UNKNOWN),"full floors and unknown geometry close a plane");
        for(uint32_t opening:{0u,uint32_t(NCMM_WORLD_DOWN),uint32_t(NCMM_WORLD_WATER),uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR)})
            require(!full_opaque_slab(opening),"open or transparent cells cannot prove a sealed plane");
    }
    {
        traits branches{};branches.loaded=true;branches.partial_support=true;
        const auto support=semantics(branches);
        require(!(support&NCMM_WORLD_FLOOR)&&(support&NCMM_WORLD_OPEN_DOWN),"tree branches/lattice do not create full floating plates");
        require(!full_opaque_slab(support),"partial natural/steel support cannot seal a vertical visibility plane");
        require(!supported_item(NCMM_WORLD_OPEN_DOWN),"unsupported air cannot publish hovering inventory icons");
        require(supported_item(NCMM_WORLD_FLOOR)&&supported_item(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR)&&supported_item(0,true),"real floors, glass and platforms retain inventory");
    }
    require(source_window_fits(0,0,48),"ordinary world window fits");
    require(!source_window_fits(INT32_MAX-48,0,48),"east direction neighbour cannot overflow");
    require(source_window_fits(INT32_MAX-49,0,48),"last safe east window is accepted");
    require(!source_window_fits(INT32_MIN+48,0,48),"west direction neighbour cannot overflow");
    require(source_window_fits(INT32_MIN+49,0,48),"last safe west window is accepted");
    require(!source_window_fits(0,INT32_MAX-48,48)&&!source_window_fits(0,INT32_MIN+48,48),"north and south neighbours remain representable");
    require(!source_window_fits(0,0,-1)&&!source_window_fits(0,0,49),"invalid source radius is rejected");
    traits t{};require(semantics(t)==(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE),"unloaded closes world");
    t.loaded=true;require(semantics(t)&NCMM_WORLD_FLOOR,"ordinary physical floor");
    t.no_floor=true;require(!(semantics(t)&NCMM_WORLD_FLOOR),"NO_FLOOR opens shaft");
    t.no_floor=false;t.no_floor_water=true;require(semantics(t)&NCMM_WORLD_WATER,"water is not a solid slab");
    t.no_floor_water=false;t.z_transparent=true;require(semantics(t)&NCMM_WORLD_FLOOR,"unused Z_TRANSPARENT does not invent a hole");
    t.transparent_floor=true;require((semantics(t)&(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR))==(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR),"transparent slab remains physical");
    t.transparent_floor=false;t.goes_down=true;require(!(semantics(t)&NCMM_WORLD_FLOOR)&&semantics(t)&NCMM_WORLD_OPEN_DOWN,"CDDA visual stairs gap");
    t.difficult_z=true;require(semantics(t)&NCMM_WORLD_LADDER,"difficult vertical connector");
    t.ramp_up=true;require(semantics(t)&NCMM_WORLD_RAMP,"inclined vertical connector");
    {
        const auto above=slab_trace_sides(NCMM_WORLD_FLOOR,1.55f,0),below=slab_trace_sides(NCMM_WORLD_FLOOR,1.55f,2.5f),edge=slab_trace_sides(NCMM_WORLD_FLOOR,-.04f,0);
        require(above.top&&!above.bottom&&!below.top&&below.bottom,"opaque slab traces only its exposed side");
        require(edge.top&&edge.bottom,"camera beside the slab edge retains conservative visibility");
        for(uint32_t flags:{uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR),uint32_t(NCMM_WORLD_DOWN),uint32_t(NCMM_WORLD_WATER),0u}){
            const auto sides=slab_trace_sides(flags,1.55f,0);require(sides.top&&sides.bottom,"glass and openings keep both visibility paths");}
        uint32_t random=0x12345678;
        const std::array<uint32_t,8> kinds={0u,NCMM_WORLD_FLOOR,NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR,NCMM_WORLD_WATER,NCMM_WORLD_CANOPY,NCMM_WORLD_UNKNOWN,vehicle_roof_fact,NCMM_WORLD_OPAQUE|opaque_field_fact};
        for(int levels:{1,3,21,32})for(int column=0;column<256;++column){const int low=-10,high=low+levels-1;std::vector<uint32_t> values(size_t(levels),0);
            for(auto &value:values){random=random*1664525u+1013904223u;value=kinds[(random>>24)%kinds.size()];}
            uint32_t reference=0;bool roof=false;
            for(int z=high;z>=low;--z){const auto value=values[size_t(z-low)];
                if(roof||(value&(NCMM_WORLD_CANOPY|vehicle_roof_fact)))reference|=uint32_t(1)<<(z-low);
                if(value&(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_CANOPY|NCMM_WORLD_UNKNOWN|vehicle_roof_fact))roof=true;}
            int reads=0;const auto lazy=roof_column(low,high,[&](int z){++reads;return values[size_t(z-low)];});
            require(lazy==reference,"lazy roof prefix exactly matches exhaustive multi-level masks");require(reads<=levels,"roof prefix has a bounded source-read count");
        }
        int reads=0;const auto ground=roof_column(-10,10,[&](int z){++reads;require(z>=0,"ground roof proof never reads buried levels");return z==0?uint32_t(NCMM_WORLD_FLOOR):0u;});
        require(reads==11&&ground==1023,"first solid floor proves all lower roof bits");
        require(roof_column(-10,22,[&](int){require(false,"invalid roof span cannot query the map");return 0u;})==UINT32_MAX,"invalid roof mask closes conservatively");
    }
    for(int layers:{1,3,5,9}){
        fixture f(layers);const point low{.5f,.5f,.55f},high{.5f,.5f,float(layers)*NCMM_WORLD_LEVEL_HEIGHT+.5f};
        require(visible(low,high,f),"open multi-storey shaft upwards");require(visible(high,low,f),"open multi-storey shaft downwards");
        f.flags[{0,0,1}]=NCMM_WORLD_FLOOR;
        require(!visible(low,high,f),"solid upper slab blocks upwards");require(!visible(high,low,f),"same slab blocks downwards");
        f.flags[{0,0,1}]=NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR;
        require(visible(low,high,f),"transparent slab transmits visual ray");
        f.flags[{0,0,1}]=NCMM_WORLD_DOWN;
        require(visible(low,high,f),"stairs central opening");
        require(!visible({.05f,.5f,.55f},{.05f,.5f,float(layers)*NCMM_WORLD_LEVEL_HEIGHT+.5f},f),"stairs landing occludes edge ray");
        f.flags[{0,0,1}]=NCMM_WORLD_UNKNOWN;
        require(!visible(low,high,f),"missing level never opens unknown world");
    }
    fixture f(3);f.flags[{1,0,0}]=NCMM_WORLD_OPAQUE;
    require(!visible({.5f,.5f,1.55f},{2.5f,.5f,1.55f},f),"opaque intermediate wall");
    require(visible({.5f,.5f,1.55f},{1.5f,.5f,1.55f},f),"first wall surface is visible");
    require(!visible({.5f,.5f,1.55f},{1.5f,1.5f,1.55f},f),"diagonal corner cannot leak");
    {
        fixture facade(3);
        facade.flags[{1,0,1}]=NCMM_WORLD_FLOOR;
        facade.flags[{2,0,1}]=NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
        require(!visible({.5f,.5f,1.55f},{2.5f,.5f,3.75f},facade),"overhang hides upper wall centre");
        require(facade_visible({.5f,.5f,1.55f},{2,0,1},facade),"upper facade above overhang remains visible instead of a floating roof");
        facade.flags[{1,0,1}]|=NCMM_WORLD_OPAQUE;
        require(!facade_visible({.5f,.5f,1.55f},{2,0,1},facade),"facade probes cannot reveal a wall behind solid architecture");
    }
    { fixture face(3);face.flags[{2,0,1}]=NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
      const auto mask=facade_faces({.5f,.5f,1.55f},{2,0,1},face);
      require(mask==1,"only the physically exposed west face may supply facade light");
      face.flags[{1,0,1}]=NCMM_WORLD_OPAQUE;
      require(facade_faces({.5f,.5f,1.55f},{2,0,1},face)==0,"hidden face light cannot qualify a facade");
      require(facade_slab_permissions(NCMM_WORLD_FLOOR,1.55f,2.5f)==NCMM_WORLD_FLOOR_BOTTOM,"visible facade retains its real underside");
      require(facade_slab_permissions(NCMM_WORLD_OPEN_DOWN,1.55f,2.5f)==0,"an actual opening never receives an invented support slab");
    }
    f.flags[{1,0,0}]=NCMM_WORLD_OPAQUE|opaque_field_fact;
    auto structure=[&](voxel p){return structural_flags(f(p));};
    require(!visible({.5f,.5f,1.55f},{2.5f,.5f,.003f},f,true),"opaque gas retains the body information barrier");
    require(visible({.5f,.5f,1.55f},{2.5f,.5f,.003f},structure,true),"neutral floor geometry continues behind gas instead of exposing sky");
    f.flags[{1,0,0}]=NCMM_WORLD_OPAQUE;
    require(!visible({.5f,.5f,1.55f},{2.5f,.5f,.003f},structure,true),"structural floor query never opens a solid wall");
    f.flags[{1,0,0}]=0;f.flags[{0,0,1}]=NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|opaque_field_fact;
    require(!visible({.5f,.5f,1.55f},{.5f,.5f,3.5f},structure),"gas cannot erase an actual upper slab");
    f.flags[{0,0,1}]=NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|opaque_field_fact;
    require(!visible({.5f,.5f,1.55f},{.5f,.5f,3.5f},structure),"gas cannot reveal an unknown level");
    require(!visible({INFINITY,0,0},{0,0,0},f),"infinite ray rejected");
    require(!visible({1e20f,0,0},{1e20f,0,0},f),"finite int-overflow ray rejected");
    require(!visible({0,0,0},{200,0,0},f),"bounded DDA cannot grow without limit");
    std::vector<ncmm_world_cell_v1> cells;
    std::vector<ncmm_graphics_cell_v1> appearance;
    std::vector<ncmm_graphics_cell_v2> details;
    std::vector<ncmm_world_slice_v1> slices;
    for(int z=-4;z<=4;++z){const uint32_t first=uint32_t(cells.size());
        for(int y=0;y<3;++y)for(int x=0;x<3;++x)cells.push_back({1000000+x,-1000000+y,z,NCMM_WORLD_FLOOR,0,0,1});
        slices.push_back({z,first,9,0,uint64_t(z+5),1});}
    appearance.resize(cells.size());details.resize(cells.size());
    ncmm_graphics_multilevel_scene_v1 scene{};scene.struct_size=sizeof(scene);scene.version=1;scene.world_epoch=1;
    scene.origin_x=1000000;scene.origin_y=-1000000;scene.loaded_min_z=-4;scene.loaded_max_z=4;scene.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    auto &base=scene.appearance.base;base.grid_width=base.grid_height=3;base.cell_count=cells.size();base.cells=appearance.data();
    scene.appearance.details=details.data();scene.appearance.detail_count=details.size();scene.coordinates=cells.data();scene.cell_count=uint32_t(cells.size());
    scene.slices=slices.data();scene.slice_count=uint32_t(slices.size());
    require(valid(scene),"nine sparse slices preserve canonical absolute coordinates");
    coordinates lookup(&scene);coordinates indexed_lookup(&scene,true);
    require(indexed_lookup.bytes()==cells.size()*sizeof(int32_t),"indexed coordinates are bounded by the validated address window");
    for(int z=-4;z<=4;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){
        const int i=lookup.find(x,y,z);require(indexed_lookup.find(x,y,z)==i,"dense lookup matches sparse binary search");require(i==(z+4)*9+y*3+x,"unique XYZ index across same XY");
        require(lookup.origin(i).z==z*NCMM_WORLD_LEVEL_HEIGHT,"consistent physical storey scale");
        require(lookup.neighbor(i,0,0,1)==(z==4?-1:i+9),"neighbors link actual levels");
    }
    require(lookup.find(3,0,0)==-1,"out-of-grid lookup");require(lookup.find(INT32_MAX,0,0)==-1,"absolute overflow rejected");
    std::swap(cells[0],cells[1]);require(!valid(scene),"unsorted coordinate alias rejected");std::swap(cells[0],cells[1]);
    cells[1]=cells[0];require(!valid(scene),"duplicate XYZ rejected");cells[1].x++;
    slices[1].first_cell++;require(!valid(scene),"slice index mismatch rejected");slices[1].first_cell--;
    scene.level_height=3;require(!valid(scene),"different floor scale rejected");scene.level_height=NCMM_WORLD_LEVEL_HEIGHT;
    scene.camera_dz=10;require(!valid(scene),"teleport-sized camera interpolation rejected");scene.camera_dz=0;
    require(valid(scene),"repaired scene remains valid");
    std::cout<<"Multi-level policy: "<<checks<<" checks passed\n";
}
