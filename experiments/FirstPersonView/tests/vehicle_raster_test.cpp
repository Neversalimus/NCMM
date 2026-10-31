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
void compare_cached(const h::frame &a,const h::frame &b){
    // Cache hits intentionally submit fewer triangles. Their complete displayed
    // buffers and Native ownership still have to equal the uncached reference.
    require(a.width==b.width&&a.height==b.height,"composite cache keeps the full original viewport");
    require(identical(a.pixels,b.pixels),"composite cache preserves every displayed colour bit");
    require(identical(a.depth,b.depth)&&identical(a.held_depth,b.held_depth),"composite cache preserves complete world and held depth");
    require(a.owners==b.owners&&a.creatures==b.creatures,"composite cache preserves every Native owner and creature ID");
    require(a.focus==b.focus&&a.focus_depth==b.focus_depth&&a.aimed==b.aimed,"composite cache preserves actual crosshair selection");
    require(a.vehicle_parts==b.vehicle_parts,"cached images still represent every visible component");
}
struct fixture {
    std::vector<ncmm_graphics_cell_v1> cells=std::vector<ncmm_graphics_cell_v1>(625);
    std::vector<ncmm_graphics_cell_v2> details=std::vector<ncmm_graphics_cell_v2>(625);
    ncmm_graphics_scene_v2 scene{};h::presentation_data metadata;v::presentation vehicles;
    bool textured_ground=false;
    const std::array<uint32_t,16> ground_texture{{0x917b61ff,0x315a2900,0x47a12280,0x8580b4ff,
        0x20d04540,0x704b23ff,0x172331ff,0x556e2690,0xf0b87aff,0x12427a20,0x316a31ff,0x409060ff,
        0x442a19ff,0x49a52a80,0x91357200,0x376451ff}};
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
        vehicles.input.vehicles.push_back(car);vehicles.input.scene.world_epoch=1;
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
        auto roof_part=input.parts[2];
        vehicles.draws.push_back({v::generate(input,shape,roof_part,0),v::rigid({11.8f,12,0},.17f),7,151,312,false,false,true});
        // One shared model in distinct Native cells must not share its light,
        // picking owner or appearance storage across these component draws.
        auto neighbor=vehicles.draws[3];neighbor.cell=313;neighbor.part_id=160;neighbor.placement.origin.y+=.35f;
        vehicles.draws.push_back(neighbor);details[313].light_r=.2f;details[313].light_g=.9f;details[313].light_b=.3f;
        // Broad cloth/rubber surfaces force visible fine-detail samples rather
        // than relying only on small imported wheel or cockpit fragments.
        v::builder detailed(input.parts[0]);
        detailed.face({0,-.7f,0},{0,.7f,0},{0,.7f,1.1f},{0,-.7f,1.1f},detailed.rubber);
        detailed.face({.06f,-.6f,0},{.06f,.6f,0},{.06f,.6f,.9f},{.06f,-.6f,.9f},detailed.fabric);
        vehicles.draws.push_back({detailed.finish(),v::rigid({7,12.5f,.1f},0),7,170,312,false,false,true});
    }
    h::frame render(int dispatch,float yaw,float pitch,const h::frame *cache=nullptr,h::frame *world=nullptr){
        const bool compose=dispatch>=9,borrow=dispatch==7||dispatch==8||(compose&&dispatch!=10),optimized=borrow||compose;
        h::render_options options;options.stream_vehicle_raster=dispatch==2;options.prune_vehicle_workers=dispatch==1||dispatch==3||optimized;
        options.prune_empty_vehicle_samples=dispatch==3||optimized;options.prune_unused_vehicle_bands=dispatch==3||dispatch==4||optimized;
        options.compact_vehicle_records=dispatch==3||dispatch==5||dispatch==6||optimized;
        options.vector_vehicle_pixels=dispatch!=6&&dispatch!=8&&dispatch!=11;options.borrow_vehicle_materials=borrow;options.cache_static_vehicles=compose;h::frame result;
        const h::sprite_provider provider=[&](uint32_t cell,uint32_t layer,ncmm_graphics_sprite_v2 &sprite){
            if(!textured_ground||cell>=cells.size()||layer!=NCMM_GFX_GROUND)return false;
            sprite={sizeof(sprite),4,4,4,ground_texture.data()};return true;};
        h::renderer painter(scene,yaw,pitch,provider,result,{},&metadata,options);painter.run(1280,800,cache,world);return result;
    }
    h::frame render_without_local_reuse(float yaw,float pitch){
        const auto original=vehicles.draws;
        for(auto &draw:vehicles.draws){auto model=std::make_shared<v::model>(*draw.geometry);
            model->shared_normals.clear();model->normal_indices.clear();model->face_directions.clear();draw.geometry=std::move(model);}
        auto result=render(0,yaw,pitch);vehicles.draws=original;return result;
    }
};
}
int main(){
    fixture f;uint32_t skipped=0,empty=0,bands=0,compact=0,clipped=0;uint64_t compact_bytes=0,full_bytes=0,vector_groups=0,world_vectors=0,texture_vectors=0,source_bytes=0,full_source_bytes=0;
    for(float yaw:{-.17f,0.f,.17f})for(float pitch:{-.15f,0.f,.12f}){
        h::frame old_world,new_world;const auto before=f.render(0,yaw,pitch,nullptr,&old_world),after=f.render(1,yaw,pitch,nullptr,&new_world);
        compare(before,f.render_without_local_reuse(yaw,pitch));
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
        const auto borrowed=f.render(7,yaw,pitch);compare(before,borrowed);compare(packed,borrowed);
        compare(cached_before,f.render(7,yaw,pitch,&old_world));compare(borrowed,f.render(8,yaw,pitch));
        require(borrowed.vehicle_source_bytes>0&&borrowed.vehicle_source_bytes<borrowed.vehicle_reference_source_bytes,
            "the pixel-identical path actually borrows owned materials with smaller source storage");
        require(before.vehicle_source_bytes==0&&packed.vehicle_source_bytes==0,"both full-field and compact source-copy references are retained");
        source_bytes+=borrowed.vehicle_source_bytes;full_source_bytes+=borrowed.vehicle_reference_source_bytes;
        h::frame composite_world;const auto composite=f.render(9,yaw,pitch,nullptr,&composite_world);
        world_vectors+=composite.world_vector_groups;
        texture_vectors+=composite.world_texture_vector_groups;
        compare(before,composite);compare(before,f.render(10,yaw,pitch));compare(before,f.render(11,yaw,pitch));
        const auto reused=f.render(9,yaw,pitch,&composite_world);compare_cached(cached_before,reused);
        require(reused.vehicle_composite&&reused.vehicle_visual_revision==f.vehicles.visual_revision(),"warm frame actually reuses a source-bound vehicle composite");
        require(reused.submitted_triangles<cached_before.submitted_triangles&&reused.vehicle_source_bytes==0,
            "warm vehicle pixels are reused rather than rebuilt or quietly omitted");
        compare_cached(cached_before,f.render(9,yaw,pitch,&old_world));
    }
    if(std::thread::hardware_concurrency()>1)require(skipped>0,"equivalence actually rejects empty worker strips");
    require(empty>0,"exact sample pruning actually rejects projected zero-fragment triangles without dropping source geometry");
    require(bands>0,"unused band pruning actually avoids per-corner calculations while preserving reflective colour and all pixels");
    require(compact>0&&compact_bytes<full_bytes,"compact equivalence actually reduces logical projected-record bytes");
#if NCMM_VEHICLE_PIXEL_SSE2
    require(vector_groups>0,"full colour/depth/picking equivalence actually executes four-lane vehicle arithmetic");
#endif
    {
        // Retain all original texture-free scenes above. Add RGBA-filtered
        // ground and an independent flat-lit wall under the same exact checks.
        constexpr size_t wall=10*25+6;const auto old_cell=f.cells[wall];const auto old_detail=f.details[wall];
        f.textured_ground=true;std::strcpy(f.cells[wall].terrain,"t_wall");f.details[wall].terrain_shape=NCMM_SHAPE_WALL;
        for(float yaw:{-.17f,0.f,.17f}){const auto before=f.render(0,yaw,0),after=f.render(9,yaw,0);compare(before,after);
            compare(before,f.render(11,yaw,0));world_vectors+=after.world_vector_groups;texture_vectors+=after.world_texture_vector_groups;}
#if NCMM_VEHICLE_PIXEL_SSE2
        require(world_vectors>0,"complete scalar-reference comparisons actually execute flat-lit SIMD on world faces");
        require(texture_vectors>0,"full scalar-reference colour/depth/picking checks execute original RGBA sampling before SIMD light/fog");
#endif
        f.textured_ground=false;f.cells[wall]=old_cell;f.details[wall]=old_detail;
    }
    const auto reference=f.render(0,0,0),streamed=f.render(2,0,0);compare(reference,streamed);
    require(streamed.vehicle_streamed_triangles>0,"disabled streaming experiment remains an explicit correctness reference");
    f.scene.base.camera_x=12.5f;
    ncmm_graphics_vehicle_part_v1 part{};part.health=1;part.color[0]=.4f;part.color[3]=1;
    v::builder inside(part);inside.cushion({0,0,0},{2,2,2},.1f,inside.paint);
    f.vehicles.draws.push_back({inside.finish(),v::rigid({12.5f,12.5f,1.55f},0),7,999,312,false,false,true});
    for(float yaw:{0.f,1.2f,3.1f}){const auto before=f.render(0,yaw,-.12f),after=f.render(1,yaw,-.12f);compare(before,after);
        compare(before,f.render_without_local_reuse(yaw,-.12f));
        compare(before,f.render(3,yaw,-.12f));
        const auto packed=f.render(5,yaw,-.12f);compare(before,packed);clipped+=packed.vehicle_clipped_records;
        compare(before,f.render(6,yaw,-.12f));
        compare(before,f.render(7,yaw,-.12f));compare(before,f.render(8,yaw,-.12f));
        compare(before,f.render(9,yaw,-.12f));
        require(after.vehicle_streamed_triangles==0,"near-plane/inside component conservatively keeps parallel path");}
    f.scene.base.camera_x=1.5f;f.cells[312].flags=0;f.cells[313].flags=0;
    require(clipped>=2,"clipped-vertex ownership is exercised across pool growth near and inside geometry");
    const auto hidden_before=f.render(0,0,0),hidden_after=f.render(1,0,0);compare(hidden_before,hidden_after);
    require(hidden_after.vehicle_parts==0&&hidden_after.vehicle_streamed_triangles==0,"hidden Native cell cannot submit vehicle geometry");
    const auto hidden_borrowed=f.render(7,0,0);compare(hidden_before,hidden_borrowed);
    require(hidden_borrowed.vehicle_source_bytes==0,"no callback material ownership survives the hidden-cell fallback");
    compare_cached(hidden_before,f.render(9,0,0));
    f.cells[312].flags=f.cells[313].flags=NCMM_GFX_VISIBLE;
    {
        h::frame cache;f.render(9,0,0,nullptr,&cache);const auto original=f.vehicles.draws;
        f.vehicles.draws[0].placement.origin.x+=.2f;compare(f.render(0,0,0),f.render(9,0,0,&cache));
        f.vehicles.draws=original;f.vehicles.draws.pop_back();compare(f.render(0,0,0),f.render(9,0,0,&cache));f.vehicles.draws=original;
        f.vehicles.active_animation=true;const auto active=f.render(9,0,0,&cache);compare(f.render(0,0,0),active);
        require(!active.vehicle_composite,"active hinges/wheels invalidate a previously stationary image");f.vehicles.active_animation=false;
        f.vehicles.input.vehicles[0].native_velocity=2;const auto moving=f.render(9,0,0,&cache);compare(f.render(0,0,0),moving);
        require(!moving.vehicle_composite,"a moving Native vehicle retains the independent geometry path");f.vehicles.input.vehicles[0].native_velocity=0;
        auto fractional=std::make_shared<v::model>(*f.vehicles.draws[0].geometry);
        for(auto &m:fractional->materials)if(m.color[3]>=.99f)m.color[3]=.995f;
        f.vehicles.draws[0].geometry=fractional;compare(f.render(0,0,0),f.render(9,0,0,&cache));f.vehicles.draws=original;
        f.scene.environment.daylight=0;f.scene.environment.cloud_cover=.8f;f.scene.environment.animation_seconds+=.04;
        f.details[312].light_r=.15f;f.details[312].light_g=.3f;f.details[312].light_b=.5f;
        h::frame dark_world;const auto dark=f.render(9,0,0,nullptr,&dark_world);compare(f.render(0,0,0),dark);
        compare(dark,f.render_without_local_reuse(0,0));
        compare_cached(f.render(0,0,0),f.render(9,0,0,&dark_world));
    }
    std::printf("vehicle raster: %d checks passed; full 1280x800 colour/depth/picking equivalence\n",checks);
    std::printf("vehicle projection logical bytes: %llu compact, %llu original; %u clipped records\n",
        static_cast<unsigned long long>(compact_bytes),static_cast<unsigned long long>(full_bytes),clipped);
    std::printf("vehicle source logical bytes: %llu borrowed, %llu original\n",
        static_cast<unsigned long long>(source_bytes),static_cast<unsigned long long>(full_source_bytes));
}
