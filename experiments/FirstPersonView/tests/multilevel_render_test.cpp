#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <iostream>
#include <stdexcept>
using namespace first_person::hybrid;
namespace {
int checks=0;
void check(bool condition,const char *reason){++checks;if(!condition)throw std::runtime_error(reason);}
struct fixture {
    ncmm_graphics_multilevel_scene_v1 world{};
    std::vector<ncmm_graphics_cell_v1> appearance;
    std::vector<ncmm_graphics_cell_v2> details;
    std::vector<ncmm_world_cell_v1> positions;
    std::vector<ncmm_world_slice_v1> slices;
    fixture(int levels){
        world.struct_size=sizeof(world);world.version=1;world.world_epoch=world.revision=1;
        world.origin_x=1000000;world.origin_y=-1000000;world.loaded_min_z=0;world.loaded_max_z=levels-1;
        world.level_height=NCMM_WORLD_LEVEL_HEIGHT;
        for(int z=0;z<levels;++z){const auto first=uint32_t(positions.size());
            for(int y=0;y<3;++y)for(int x=0;x<24;++x){
                positions.push_back({world.origin_x+x,world.origin_y+y,z,NCMM_WORLD_BODY_VISIBLE,0,0,1});
                ncmm_graphics_cell_v1 c{};c.flags=NCMM_GFX_VISIBLE|NCMM_GFX_NO_FLOOR;c.light=1;std::strcpy(c.terrain,"t_open_air");appearance.push_back(c);
                ncmm_graphics_cell_v2 d{};d.light_r=d.light_g=d.light_b=.9f;std::strcpy(d.ground,"t_floor");details.push_back(d);
            }
            slices.push_back({z,first,uint32_t(positions.size())-first,0,uint64_t(z+1),1});
        }
        world.coordinates=positions.data();world.slices=slices.data();world.cell_count=uint32_t(positions.size());world.slice_count=uint32_t(slices.size());
        world.appearance.struct_size=sizeof(world.appearance);
        world.appearance.base={sizeof(ncmm_graphics_scene_v1),400,260,24,3,2.5f,1.5f,appearance.data(),appearance.size()};
        world.appearance.details=details.data();world.appearance.detail_count=details.size();world.appearance.environment.daylight=.8f;
        world.appearance.environment.sun_z=.8f;
    }
    int cell(int x,int y,int z){return ncmm_world::coordinates(&world).find(x,y,z);}
    void slab(int z){for(int y=0;y<3;++y)for(int x=0;x<24;++x){const int i=cell(x,y,z);
        positions[i].flags|=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;appearance[i].flags&=~NCMM_GFX_NO_FLOOR;}}
    bool draw(frame &out,float pitch){return build_frame(world.appearance,0,pitch,{},out,400,260,{},nullptr,nullptr,nullptr,{},nullptr,&world);}
};
struct collector:geometry_sink {
    std::vector<triangle> triangles;
    bool dirty(int)const override{return true;}
    void emit(const triangle &t)override{triangles.push_back(t);}
    void skin(const first_person::creature::model &,const first_person::creature::skeleton_pose &,int,vec,int,vec,bool,bool,float,vec)override{}
    void impostor(const first_person::creature::asset &,const first_person::creature::pose_state &,vec,int,int,bool)override{}
};
}
int main(){try{
    check(!std::strcmp(ncmm_graphics_shapes::facade_id(NCMM_SHAPE_WALL,"t_mod_secret_brick_partition"),"t_brick_wall"),"coarse upper brick finish matches the visible lower facade");
    check(!std::strcmp(ncmm_graphics_shapes::facade_id(NCMM_SHAPE_WALL,"t_hidden_material t_brick_wall"),"t_brick_wall"),"coarse facade respects declared material aliases");
    check(!std::strcmp(ncmm_graphics_shapes::facade_id(NCMM_SHAPE_DOOR_CLOSED,"t_hidden_brick_door"),"t_wall"),"coarse finish cannot publish the actual hidden terrain identity");
    for(int levels:{1,3,5,9}){
        fixture world(levels);world.slab(0);if(levels>1)world.slab(levels-1);
        check(ncmm_world::valid(world.world),"complete sparse world contract");frame image;
        check(world.draw(image,levels==1?-.7f:1.2f),"CPU renders all requested Z slices");
        if(levels>1){size_t upper=0;for(int owner:image.owners)if(owner>=0&&world.positions[owner].z==levels-1)++upper;
            check(upper>100,"actual upper slab appears in CPU depth/owner buffers");}
        collector sink;frame statistics;renderer(world.world.appearance,0,.2f,{},statistics,{},nullptr,{},nullptr,&world.world).collect(sink,400,260);
        check(!sink.triangles.empty(),"shared GPU geometry generator produces slabs");
        size_t bottom=0;for(const auto &t:sink.triangles){check(t.m.cell>=0&&size_t(t.m.cell)<world.positions.size(),"triangle retains unique XYZ source index");
            if(t.normal.z<-.9f){++bottom;const float expected=world.positions[t.m.cell].z*NCMM_WORLD_LEVEL_HEIGHT-.08f;
                for(const auto &v:t.v)check(std::abs(v.p.z-expected)<.0001f,"underside belongs to the actual upper floor");}}
        check(bottom>0,"slabs contain a separately materialled underside");
        first_person::gpu::scene_cache cache;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        first_person::gpu::packet packet;frame stats;
        bool ready=cache.build(world.world.appearance,0,.2f,{},packet,stats,host,{},nullptr,nullptr,400,260,NCMM_GPU_FORCE,&world.world);
        if(!ready){packet=first_person::gpu::packet{};ready=cache.build(world.world.appearance,0,.2f,{},packet,stats,host,{},nullptr,nullptr,400,260,NCMM_GPU_FORCE,&world.world);}
        check(ready,"bounded GPU scene cache warms the complete multi-Z snapshot");
        ncmm_gpu_multilevel_scene_v1 gpu{};gpu.struct_size=sizeof(gpu);gpu.version=1;gpu.base=packet.finish(nullptr);
        gpu.coordinates=world.world.coordinates;gpu.cell_count=world.world.cell_count;gpu.origin_x=world.world.origin_x;gpu.origin_y=world.world.origin_y;gpu.level_height=world.world.level_height;
        check(!ncmm_gpu::validate(gpu.base,&gpu),"sparse XYZ GPU wrapper validates without changing legacy cell ABI");
        for(const auto &draw:packet.draws){check(draw.cell<world.positions.size(),"GPU draw preserves sparse source cell");
            check(std::abs(draw.transform.columns[14]-world.positions[draw.cell].z*2.5f)<.0001f,"GPU mesh instance carries actual physical Z");}
        bool lower=false;for(uint32_t m=0;m<gpu.base.mesh_count;++m)for(uint32_t i=0;i<gpu.base.meshes[m].material_count;++i)lower|=(gpu.base.meshes[m].materials[i].flags&NCMM_GPU_SLAB_UNDERSIDE)!=0;
        check(lower,"GPU underside chooses separate lower-room RGB");
        if(levels>1)check(ncmm_gpu::validate(gpu.base),"legacy GPU path cannot silently accept sparse multi-Z cells");
        auto broken=gpu;broken.level_height=0;check(ncmm_gpu::validate(gpu.base,&broken),"invalid world height rejected");
        broken=gpu;broken.origin_x=INT32_MAX;check(ncmm_gpu::validate(gpu.base,&broken),"far absolute coordinates do not overflow validation");
    }
    {
        fixture slabs(2);slabs.slab(0);slabs.slab(1);const auto complete=slabs.positions;
        for(float height:{-2.5f,-1.59f,0.f,2.5f}){slabs.positions=complete;slabs.world.coordinates=slabs.positions.data();slabs.world.camera_dz=height;
            frame reference,optimized;const float pitch=height<0?.5f:-.5f;check(slabs.draw(reference,pitch),"unfiltered slab reference renders");
            for(auto &cell:slabs.positions){const auto sides=ncmm_world::slab_trace_sides(cell.flags,1.55f+height,cell.z*NCMM_WORLD_LEVEL_HEIGHT);
                if(!sides.top)cell.flags&=~NCMM_WORLD_FLOOR_TOP;if(!sides.bottom)cell.flags&=~NCMM_WORLD_FLOOR_BOTTOM;}
            check(slabs.draw(optimized,pitch),"one-sided slab snapshot renders");
            check(reference.pixels==optimized.pixels&&reference.depth==optimized.depth&&reference.owners==optimized.owners,"skipped opaque slab backs preserve every pixel, depth and owner above, below and beside an edge");
        }
    }
    fixture empty(3);for(auto &c:empty.appearance)c.flags|=NCMM_GFX_CEILING;
    frame image;check(empty.draw(image,1.2f),"open multi-Z world with misleading legacy interior flags");
    check(std::all_of(image.depth.begin(),image.depth.begin()+image.width*image.height/2,[](float z){return !std::isfinite(z);}),"inside flag cannot create phantom ceiling");
    check(empty.draw(image,-1.2f),"empty downward world renders without invented geometry");
    check(std::all_of(image.depth.begin(),image.depth.end(),[](float z){return !std::isfinite(z);}),"unseen space is not a fake floor or ceiling");
    const auto dark_pixels=std::count_if(image.pixels.begin(),image.pixels.end(),[](uint32_t p){return (p>>24)<32&&((p>>16)&255)<32&&((p>>8)&255)<32;});
    check(dark_pixels>image.pixels.size()*.99,"downward unseen space is dark, apart from the UI crosshair");
    empty.positions[empty.cell(4,1,0)].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
    collector stairs;frame statistics;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(stairs,400,260);
    check(stairs.triangles.size()>80,"semantic staircase is stepped volume, not a floor square");
    const int stair_cell=empty.cell(4,1,0);empty.details[stair_cell].terrain_shape=NCMM_SHAPE_WALL;
    collector hinted;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(hinted,400,260);
    check(hinted.triangles.size()==stairs.triangles.size(),"legacy wall hint cannot add a duplicate transition volume");
    empty.details[stair_cell].terrain_shape=NCMM_SHAPE_FLAT;
    float top=0;for(const auto &t:stairs.triangles)for(const auto &v:t.v)top=std::max(top,v.p.z);
    check(top>=NCMM_WORLD_LEVEL_HEIGHT,"staircase connects a full physical Z storey");
    const int stair=empty.cell(4,1,0);empty.positions[stair].flags|=NCMM_WORLD_RAMP;
    collector ramp;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(ramp,400,260);
    bool slope=false;for(const auto &t:ramp.triangles)if(t.normal.z>.1f&&t.normal.z<.9f)slope=true;
    check(slope,"semantic ramp has an actual inclined surface");
    empty.positions[stair].flags|=NCMM_WORLD_RAMP_END;
    const int landing=empty.cell(4,1,1);empty.positions[landing].flags|=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
    collector ramp_end;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(ramp_end,400,260);
    size_t support_faces=0;
    for(const auto &t:ramp_end.triangles)if(t.m.cell==stair){++support_faces;
        check(t.normal.z<.1f,"upper slab owns the ramp support cap");
        for(const auto &v:t.v)check(v.p.z<=NCMM_WORLD_LEVEL_HEIGHT-.08f+.0001f,"ramp support ends at the real slab underside without coplanar edge overlap");}
    check(support_faces>0,"covered ramp end retains its structural sides");
    empty.positions[stair].flags&=~NCMM_WORLD_RAMP_END;empty.positions[landing].flags=NCMM_WORLD_BODY_VISIBLE;
    empty.positions[stair].flags&=~NCMM_WORLD_RAMP;empty.positions[stair].flags|=NCMM_WORLD_LADDER;
    collector ladder;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(ladder,400,260);
    check(ladder.triangles.size()>100,"difficult-Z ladder has rails and rungs");
    empty.world.camera_dz=-2.5f;check(empty.draw(image,0),"visual camera interpolates independently of logical Z");
    check(empty.world.origin_z==0&&empty.positions[stair].z==0,"render leaves canonical game coordinates unchanged");
    fixture neutral(1);
    for(size_t i=0;i<neutral.positions.size();++i){
        neutral.positions[i].flags=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP;
        neutral.details[i].ground[0]='\0';std::strcpy(neutral.appearance[i].creature,"mon_zombie");
        std::strcpy(neutral.appearance[i].furniture,"f_bookcase");}
    check(neutral.draw(image,-.7f),"neutral support-only scene renders without room textures");
    for(int y=image.height/2+40;y<image.height-12;++y)for(int x=image.width/2-12;x<image.width/2+12;++x)
        check(std::isfinite(image.depth[size_t(y)*image.width+x]),"support-only floor has continuous depth instead of a sky gap");
    check(std::all_of(image.creatures.begin(),image.creatures.end(),[](int cell){return cell<0;}),"neutral support cannot expose a hidden creature identity");
    fixture facade(2);
    for(auto &p:facade.positions)p.flags=0;
    const int upper_wall=facade.cell(6,1,1);
    facade.positions[upper_wall].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE;
    facade.details[upper_wall].terrain_shape=NCMM_SHAPE_WALL;
    std::strcpy(facade.appearance[upper_wall].terrain,"t_wall");
    check(ncmm_world::valid(facade.world),"architecture-only permission preserves the sparse contract");
    check(facade.draw(image,.55f),"upper exterior wall renders without native room permission");
    check(std::count(image.owners.begin(),image.owners.end(),upper_wall)>100,"upper facade has actual CPU depth rather than floating floor planes");
    collector plain_facade;renderer(facade.world.appearance,0,.55f,{},statistics,{},nullptr,{},nullptr,&facade.world).collect(plain_facade,400,260);
    check(!plain_facade.triangles.empty(),"shared GPU collector retains architecture-only walls");
    std::strcpy(facade.appearance[upper_wall].furniture,"f_bookcase");facade.details[upper_wall].furniture_shape=NCMM_SHAPE_STORAGE;
    std::strcpy(facade.appearance[upper_wall].item,"rock");std::strcpy(facade.appearance[upper_wall].creature,"mon_zombie");
    std::strcpy(facade.details[upper_wall].field,"fd_fire");facade.details[upper_wall].field_shape=NCMM_SHAPE_BILLBOARD;
    collector private_room;renderer(facade.world.appearance,0,.55f,{},statistics,{},nullptr,{},nullptr,&facade.world).collect(private_room,400,260);
    check(private_room.triangles.size()==plain_facade.triangles.size(),"architecture permission cannot generate room contents, fields or creatures");
    check(facade.draw(image,.55f)&&std::all_of(image.creatures.begin(),image.creatures.end(),[](int cell){return cell<0;}),"architecture permission cannot create a creature picking target");
    first_person::gpu::scene_cache facade_cache;first_person::gpu::packet facade_packet;ncmm_gpu_metrics_v1 facade_host{};facade_host.resource_epoch=1;
    check(facade_cache.build(facade.world.appearance,0,.55f,{},facade_packet,statistics,facade_host,{},nullptr,nullptr,400,260,NCMM_GPU_FORCE,&facade.world),"architecture-only facade produces a GPU packet");
    ncmm_gpu_multilevel_scene_v1 facade_gpu{};facade_gpu.struct_size=sizeof(facade_gpu);facade_gpu.version=1;facade_gpu.base=facade_packet.finish(nullptr);
    facade_gpu.coordinates=facade.world.coordinates;facade_gpu.cell_count=facade.world.cell_count;facade_gpu.origin_x=facade.world.origin_x;facade_gpu.origin_y=facade.world.origin_y;facade_gpu.level_height=facade.world.level_height;
    check(!ncmm_gpu::validate(facade_gpu.base,&facade_gpu),"Host accepts the architecture permission without changing GPU V1 ABI");
    facade.positions[upper_wall].flags|=1u<<19;
    check(!ncmm_world::valid(facade.world)&&ncmm_gpu::validate(facade_gpu.base,&facade_gpu),"unknown world permission bits remain rejected");
    {
        using namespace ncmm_world;
        check(surface_permissions(NCMM_WORLD_OPEN_DOWN,1.55f,0)==0,"an open shaft never acquires an invented static slab");
        check(surface_permissions(NCMM_WORLD_FLOOR,1.55f,0)==NCMM_WORLD_FLOOR_TOP,"ground uses only its facing top");
        check(surface_permissions(NCMM_WORLD_FLOOR,1.55f,2.5f)==NCMM_WORLD_FLOOR_BOTTOM,"upper floor exposes its underside");
        check(surface_permissions(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR,1.55f,2.5f)==(NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM),"glass keeps both faces");
        check(!(surface_permissions(NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE,1.55f,0)&NCMM_WORLD_BODY_VISIBLE),"depth-driven geometry cannot grant contents");
        std::vector<ncmm_lod_patch_v1> clipped;float area=0;
        outside_detail(-12,-12,48,48,24,[&](float x,float y,float w,float h){
            check(x+w<=0||x>=24||y+h<=0||y>=24,"LOD strip cannot cover the detailed square");area+=w*h;
            clipped.push_back({x,y,0,w,h,0,{.3f,.4f,.2f},NCMM_LOD_FIELD});
        });
        check(clipped.size()==4&&std::abs(area-(48*48-24*24))<.01f,"disjoint LOD strips exactly cover the outer ring");
        ncmm_graphics_lod_scene_v1 distant{sizeof(distant),1,uint32_t(clipped.size()),1,7,0,0,clipped.data()};
        check(valid_lod(distant),"bounded coarse horizon validates");
        auto invalid=distant;invalid.count=NCMM_LOD_MAX_PATCHES+1;check(!valid_lod(invalid),"oversized horizon rejected before reads");
        invalid=distant;invalid.patches=nullptr;check(!valid_lod(invalid),"missing LOD array rejected");
        clipped[0].width=std::numeric_limits<float>::quiet_NaN();check(!valid_lod(distant),"NaN LOD cannot enter a renderer");clipped[0].width=48;
        fixture open(1);presentation_data data;data.view.struct_size=sizeof(data.view);data.view.eye_height=1.55f;data.view.body_scale=1;data.view.held_length=.6f;data.cells.resize(open.positions.size());data.horizon_enabled=true;data.horizon_revision=9;
        data.horizon={{30,-20,0,24,48,0,{.3f,.4f,.2f},NCMM_LOD_FIELD},{80,-10,0,24,24,5,{.4f,.4f,.4f},NCMM_LOD_BUILDING}};
        frame horizon_image;
        check(build_frame(open.world.appearance,0,-.08f,{},horizon_image,400,260,{},nullptr,nullptr,&data,{},nullptr,&open.world),"CPU renders distant ground and actual coarse silhouettes");
        const size_t middle=size_t(horizon_image.height/2-10)*horizon_image.width+horizon_image.width/2;
        check(std::isfinite(horizon_image.depth[middle])&&horizon_image.depth[middle]>24,"outer ground has depth beyond the near square");
        check(horizon_image.owners[middle]<0&&horizon_image.aimed<0,"LOD carries no interaction/creature target");
        check(!std::isfinite(horizon_image.depth[size_t(horizon_image.height-5)*horizon_image.width+horizon_image.width/2]),"distant LOD never fills a nearby shaft");
        first_person::gpu::scene_cache lod_cache;first_person::gpu::packet packet;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;frame stats;
        check(lod_cache.build(open.world.appearance,0,-.08f,{},packet,stats,host,{},&data,nullptr,400,260,NCMM_GPU_FORCE,&open.world),"GPU builds a cached horizon");
        size_t backgrounds=0;uint64_t lod_key=0;for(const auto &draw:packet.draws)if(draw.cell==UINT32_MAX&&draw.pass==NCMM_GPU_OPAQUE){++backgrounds;lod_key=draw.mesh_key;check(draw.pass==NCMM_GPU_OPAQUE,"horizon depth tests as opaque world");}
        check(backgrounds==1,"entire distant horizon uses one draw");
        ncmm_gpu_multilevel_scene_v1 lod_gpu{};lod_gpu.struct_size=sizeof(lod_gpu);lod_gpu.version=1;lod_gpu.base=packet.finish(nullptr);lod_gpu.coordinates=open.world.coordinates;lod_gpu.cell_count=open.world.cell_count;lod_gpu.origin_x=open.world.origin_x;lod_gpu.origin_y=open.world.origin_y;lod_gpu.level_height=open.world.level_height;
        check(!ncmm_gpu::validate(lod_gpu.base,&lod_gpu),"Host validates the non-targetable horizon draw");
        auto view=packet.finish(nullptr);bool lod_material=false;for(uint32_t i=0;i<view.mesh_count;++i)for(uint32_t j=0;j<view.meshes[i].material_count;++j)lod_material|=(view.meshes[i].materials[j].flags&NCMM_GPU_DISTANCE_LOD)!=0;
        check(lod_material,"GPU geography uses outdoor daylight rather than held/indoor light");
        first_person::gpu::packet warm;
        check(lod_cache.build(open.world.appearance,.5f,-.08f,{},warm,stats,host,{},&data,nullptr,400,260,NCMM_GPU_FORCE,&open.world),"horizon survives a camera turn");
        bool retained=false;for(const auto &draw:warm.draws)if(draw.cell==UINT32_MAX&&draw.mesh_key==lod_key)retained=true;
        check(retained,"camera turn reuses the same horizon mesh");
        // A near opaque wall must win over the entire distant layer.
        for(int y=0;y<3;++y){const int i=open.cell(5,y,0);open.positions[i].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP;open.details[i].terrain_shape=NCMM_SHAPE_WALL;}
        check(build_frame(open.world.appearance,0,0,{},horizon_image,400,260,{},nullptr,nullptr,&data,{},nullptr,&open.world),"near wall with distant world renders");
        check(horizon_image.depth[size_t(horizon_image.height/2)*horizon_image.width+horizon_image.width/2]<4,"near architecture correctly occludes the horizon");
    }
    std::cout<<"Multi-level rendering: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
