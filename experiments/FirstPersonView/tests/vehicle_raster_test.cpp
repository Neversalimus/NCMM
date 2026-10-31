#include "hybrid_renderer.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace h=first_person::hybrid;
namespace v=first_person::vehicle;
namespace {
int checks=0;
void require(bool value,const char *message){++checks;if(!value){std::fprintf(stderr,"vehicle raster: %s\n",message);std::exit(1);}}
template<class T> bool identical(const std::vector<T> &a,const std::vector<T> &b){
    return a.size()==b.size()&&(a.empty()||std::memcmp(a.data(),b.data(),a.size()*sizeof(T))==0);
}
void compare(const h::frame &a,const h::frame &b){
    require(a.width==b.width&&a.height==b.height,"same internal viewport, no resolution reduction");
    require(identical(a.pixels,b.pixels),"scheduling variants preserve the complete colour buffer bit for bit");
    require(identical(a.depth,b.depth)&&identical(a.held_depth,b.held_depth),"world/held depth bits are unchanged");
    require(a.owners==b.owners&&a.creatures==b.creatures,"cell and creature picking preserve every pixel");
    require(a.focus==b.focus&&a.focus_depth==b.focus_depth&&a.aimed==b.aimed,"crosshair selection/depth are unchanged");
    require(a.submitted_triangles==b.submitted_triangles&&a.vehicle_parts==b.vehicle_parts,"all source triangles/components are retained");
}
struct fixture {
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(625);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(625);
    ncmm_graphics_scene_v2 scene{};h::presentation_data metadata;v::presentation vehicles;
    fixture(){
        for(size_t i=0;i<cells.size();++i){cells[i].flags=NCMM_GFX_VISIBLE;cells[i].light=1;
            std::strcpy(cells[i].terrain,"t_floor");std::strcpy(details[i].ground,"t_floor");
            details[i].light_r=.8f;details[i].light_g=.7f;details[i].light_b=.6f;}
        scene.struct_size=sizeof(scene);scene.base={sizeof(ncmm_graphics_scene_v1),1280,800,25,25,1.5f,12.5f,cells.data(),cells.size()};
        scene.details=details.data();scene.detail_count=details.size();scene.environment.daylight=1;
        scene.environment.sun_x=.3f;scene.environment.sun_y=.4f;scene.environment.sun_z=.8f;
        scene.environment.animation_seconds=42;scene.environment.moon_z=-1;
        metadata.vehicles=&vehicles;metadata.cells.resize(cells.size());metadata.view.eye_height=1.55f;
        v::snapshot input;input.scene.level_height=3.2f;ncmm_graphics_vehicle_v1 car{};car.entity_id=7;car.part_count=3;
        input.vehicles.push_back(car);
        for(uint32_t i=0;i<3;++i){ncmm_graphics_vehicle_part_v1 p{};p.part_id=100+i;p.state=NCMM_VP_EXISTS;p.health=1;
            p.features=i==0?NCMM_VP_FRAME:i==1?NCMM_VP_ROOF:NCMM_VP_WINDOW;p.color[0]=.7f;p.color[1]=.2f;p.color[2]=.1f;p.color[3]=1;
            p.material=NCMM_VM_PAINT;std::strcpy(p.id,i==0?"frame":i==1?"roof":"windshield");input.parts.push_back(p);}
        const v::topology shape(input,input.vehicles[0]);
        for(uint32_t i=0;i<20;++i){auto p=input.parts[0];p.features=i%4==0?NCMM_VP_WINDOW:i%4==1?NCMM_VP_WHEEL:i%4==2?NCMM_VP_CONTROLS:NCMM_VP_PANEL;
            p.wheel_radius=.32f;p.wheel_width=.18f;
            const auto mesh=v::generate(input,shape,p,0);
            vehicles.draws.push_back({mesh,v::rigid({12+float(i%5)*.4f,11.7f+float(i/5)*.4f,.12f},i*.37f),7,100+i,312,false,false,true});}
        v::builder tiny(input.parts[0]);
        for(unsigned i=0;i<20;++i){const float y=i*.03f;
            tiny.face({0,y,0},{0,y+.0008f,0},{0,y,.0008f},{0,y,0},tiny.paint);}
        vehicles.draws.push_back({tiny.finish(),v::rigid({12,12,1.2f},0),7,150,312,false,false,true});
    }
    h::frame render(int dispatch,float yaw,float pitch,const h::frame *cache=nullptr,h::frame *world=nullptr){
        h::render_options options;options.stream_vehicle_raster=dispatch==2;options.prune_vehicle_workers=dispatch==1||dispatch==3;
        options.prune_empty_vehicle_samples=dispatch==3;options.prune_unused_vehicle_bands=dispatch==3||dispatch==4;
        options.compact_vehicle_records=dispatch==3||dispatch==5||dispatch==6;
        options.vector_vehicle_pixels=dispatch!=6;h::frame result;
        h::renderer painter(scene,yaw,pitch,{},result,{},&metadata,options);painter.run(1280,800,cache,world);return result;
    }
};
}
int main(){
    fixture f;uint32_t skipped=0,empty=0,bands=0,compact=0,clipped=0;uint64_t compact_bytes=0,full_bytes=0,vector_groups=0;
    for(float yaw:{-.17f,0.f,.17f})for(float pitch:{-.15f,0.f,.12f}){
        h::frame old_world,new_world;const auto before=f.render(0,yaw,pitch,nullptr,&old_world),after=f.render(1,yaw,pitch,nullptr,&new_world);
        compare(before,after);skipped+=after.vehicle_worker_skips;
        require(before.triangles==after.triangles,"parallel worker pruning retains every participating triangle visit");
        require(after.vehicle_streamed_triangles==0,"ordinary optimized path keeps parallel pixel work");
        const auto cached_before=f.render(0,yaw,pitch,&old_world),cached_after=f.render(1,yaw,pitch,&new_world);compare(cached_before,cached_after);
        const auto covered=f.render(3,yaw,pitch);compare(before,covered);empty+=covered.vehicle_empty_triangles;
        compare(cached_before,f.render(3,yaw,pitch,&old_world));
        const auto bandless=f.render(4,yaw,pitch);compare(before,bandless);bands+=bandless.vehicle_band_skips;
        const auto packed=f.render(5,yaw,pitch);compare(before,packed);compare(cached_before,f.render(5,yaw,pitch,&old_world));
        const auto scalar_packed=f.render(6,yaw,pitch);compare(before,scalar_packed);compare(packed,scalar_packed);
        require(scalar_packed.vehicle_vector_groups==0&&before.vehicle_vector_groups==0,"both original and compact scalar references remain scalar");
        vector_groups+=packed.vehicle_vector_groups;
        compact+=packed.vehicle_compact_records;compact_bytes+=packed.vehicle_projection_bytes;full_bytes+=packed.vehicle_reference_projection_bytes;
        require(packed.triangles==before.triangles,"compact storage retains every participating triangle visit");
    }
    if(std::thread::hardware_concurrency()>1)require(skipped>0,"equivalence actually rejects empty worker strips");
    require(empty>0,"exact sample pruning actually rejects projected zero-fragment triangles without dropping source geometry");
    require(bands>0,"unused band pruning actually avoids per-corner calculations while preserving reflective colour and all pixels");
    require(compact>0&&compact_bytes<full_bytes,"compact equivalence actually reduces logical projected-record bytes");
#if NCMM_VEHICLE_PIXEL_SSE2
    require(vector_groups>0,"full colour/depth/picking equivalence actually executes four-lane vehicle arithmetic");
#endif
    const auto reference=f.render(0,0,0),streamed=f.render(2,0,0);compare(reference,streamed);
    require(streamed.vehicle_streamed_triangles>0,"disabled streaming experiment remains an explicit correctness reference");
    f.scene.base.camera_x=12.5f;
    ncmm_graphics_vehicle_part_v1 part{};part.health=1;part.color[0]=.4f;part.color[3]=1;
    v::builder inside(part);inside.cushion({0,0,0},{2,2,2},.1f,inside.paint);
    f.vehicles.draws.push_back({inside.finish(),v::rigid({12.5f,12.5f,1.55f},0),7,999,312,false,false,true});
    for(float yaw:{0.f,1.2f,3.1f}){const auto before=f.render(0,yaw,-.12f),after=f.render(1,yaw,-.12f);compare(before,after);
        compare(before,f.render(3,yaw,-.12f));
        const auto packed=f.render(5,yaw,-.12f);compare(before,packed);clipped+=packed.vehicle_clipped_records;
        compare(before,f.render(6,yaw,-.12f));
        require(after.vehicle_streamed_triangles==0,"near-plane/inside component conservatively keeps parallel path");}
    f.scene.base.camera_x=1.5f;f.cells[312].flags=0;
    require(clipped>=2,"clipped-vertex ownership is exercised across pool growth near and inside geometry");
    const auto hidden_before=f.render(0,0,0),hidden_after=f.render(1,0,0);compare(hidden_before,hidden_after);
    require(hidden_after.vehicle_parts==0&&hidden_after.vehicle_streamed_triangles==0,"hidden Native cell cannot submit vehicle geometry");
    std::printf("vehicle raster: %d checks passed; full 1280x800 colour/depth/picking equivalence\n",checks);
    std::printf("vehicle projection logical bytes: %llu compact, %llu original; %u clipped records\n",
        static_cast<unsigned long long>(compact_bytes),static_cast<unsigned long long>(full_bytes),clipped);
}
