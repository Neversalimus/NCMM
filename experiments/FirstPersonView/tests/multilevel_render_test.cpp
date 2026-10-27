#include "gpu_scene.hpp"
#include "ncmm_gpu_validation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "skyline_host_fixture.hpp"
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
    bool arms(const first_person::viewmodel::submission &)override{return true;} // This collector audits world slabs only.
    void skin(const first_person::creature::model &,const first_person::creature::skeleton_pose &,int,vec,int,vec,bool,bool,float,vec)override{}
    void impostor(const first_person::creature::asset &,const first_person::creature::pose_state &,vec,int,int,bool)override{}
};
void static_stair_permission_regression(){
    // Feed the shared CPU/GPU producer a terrain-only Host permission. BODY
    // stays absent, including when stale/malicious content IDs are supplied.
    for(int offset:{0,1})for(uint32_t direction=0;direction<4;++direction){
        fixture scene(2);scene.slab(0);scene.slab(1);
        const int root=scene.cell(6,1,0),upper=scene.cell(6+offset,1,1);
        auto &source=scene.positions[root];source.flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP|NCMM_WORLD_STRUCTURE_VISIBLE;
        source.direction=direction;std::strcpy(scene.appearance[root].terrain,"t_stairs_up");
        scene.positions[upper].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_DOWN|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
        scene.appearance[upper].flags|=NCMM_GFX_NO_FLOOR;std::strcpy(scene.appearance[upper].terrain,"t_stairs_down");
        render_options options;options.contact_shadows=false;
        const auto geometry=[&](){collector out;frame stats;
            renderer(scene.world.appearance,0,.15f,{},stats,{},nullptr,options,nullptr,&scene.world).collect(out,400,260);return out;};
        const auto raster=[&](){frame out;
            check(build_frame(scene.world.appearance,0,.15f,{},out,400,260,{},nullptr,nullptr,nullptr,options,nullptr,&scene.world),"static stair CPU raster validates");return out;};
        const auto equal=[&](const collector &a,const collector &b){
            check(a.triangles.size()==b.triangles.size(),"static terrain permission retains complete stair geometry without BODY");
            for(size_t i=0;i<a.triangles.size();++i){const auto &x=a.triangles[i],&y=b.triangles[i];
                check(x.m.cell==y.m.cell&&x.m.texture==y.m.texture&&x.m.finish==y.m.finish&&x.m.alpha==y.m.alpha,
                      "static terrain permission preserves materials and source ownership");
                for(int v=0;v<3;++v)check(x.v[v].p.x==y.v[v].p.x&&x.v[v].p.y==y.v[v].p.y&&x.v[v].p.z==y.v[v].p.z&&
                    x.v[v].u==y.v[v].u&&x.v[v].v==y.v[v].v&&x.v[v].light.x==y.v[v].light.x&&x.v[v].light.y==y.v[v].light.y&&x.v[v].light.z==y.v[v].light.z,
                    "static terrain permission preserves exact positions, UV and RGB");
            }
        };
        const auto visible=geometry();const auto reference=raster();source.flags&=~NCMM_WORLD_BODY_VISIBLE;
        const auto retained=geometry();equal(visible,retained);const auto normal=raster();
        check(reference.pixels==normal.pixels&&reference.depth==normal.depth,"static stair pixels and depth match visible terrain at fixed RGB");
        check(std::count(normal.owners.begin(),normal.owners.end(),root)>24,"static-only staircase has actual visible CPU pixels");
        std::strcpy(scene.appearance[root].furniture,"f_bookcase");scene.details[root].furniture_shape=NCMM_SHAPE_STORAGE;
        std::strcpy(scene.appearance[root].item,"rock");std::strcpy(scene.appearance[root].creature,"mon_zombie");
        std::strcpy(scene.details[root].field,"fd_fire");scene.details[root].field_shape=NCMM_SHAPE_BILLBOARD;
        const auto hidden=geometry();equal(retained,hidden);const auto private_contents=raster();
        check(private_contents.pixels==normal.pixels&&private_contents.depth==normal.depth,"static stair permission cannot reveal supplied furniture, inventory or fields");
        check(private_contents.aimed<0&&std::all_of(private_contents.creatures.begin(),private_contents.creatures.end(),[](int i){return i<0;}),
              "static stair permission cannot reveal or select a creature");
        scene.appearance[root].furniture[0]=scene.appearance[root].item[0]=scene.appearance[root].creature[0]=scene.details[root].field[0]='\0';
        scene.details[root].furniture_shape=scene.details[root].field_shape=NCMM_SHAPE_FLAT;
        source.flags&=~NCMM_WORLD_STRUCTURE_VISIBLE;const auto denied=geometry();
        check(denied.triangles.size()+24<retained.triangles.size(),"navigation alone cannot bypass static permission");
        if(offset){
            const int support=scene.cell(7,1,0);scene.positions[support].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE;
            scene.details[support].terrain_shape=NCMM_SHAPE_WALL;std::strcpy(scene.appearance[support].terrain,"t_wall");
            const auto support_mesh=[&](){auto mesh=geometry();mesh.triangles.erase(std::remove_if(mesh.triangles.begin(),mesh.triangles.end(),
                [&](const triangle &t){return t.m.cell!=support;}),mesh.triangles.end());return mesh;};
            const auto with_unpermitted_nav=support_mesh();
            source.flags&=~(NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP);const auto without_nav=support_mesh();
            equal(with_unpermitted_nav,without_nav);
            check(!with_unpermitted_nav.triangles.empty(),"an offset stair without permission cannot cut the permitted wall below its exit");
            source.flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
            scene.positions[support].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
            scene.details[support].terrain_shape=NCMM_SHAPE_FLAT;std::strcpy(scene.appearance[support].terrain,"t_floor");
        }
        source.flags|=NCMM_WORLD_STRUCTURE_VISIBLE;
        const int wall=scene.cell(4,1,0);scene.positions[wall].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE;
        scene.details[wall].terrain_shape=NCMM_SHAPE_WALL;std::strcpy(scene.appearance[wall].terrain,"t_wall");
        const auto occluded=raster();check(std::count(occluded.owners.begin(),occluded.owners.end(),root)==0,
            "foreground architecture still occludes static-only stairs in depth");
    }
}
void skyline_host_regression(){
    namespace host=skyline_host_fixture;
    using ncmm_world::valid_skyline;
    host::graphics_clear_lod();check(!host::graphics_lod_owned_bytes(),"provider clear releases all owned geography/feature capacity");
    for(int seen=0;seen<=4;++seen){
        host::vision=host::om_vision_level(seen);host::terrain_reads=0;
        host::graphics_build_lod(0,0,0,24,true);
        ncmm_graphics_lod_scene_v1 old{};old.struct_size=sizeof(old);check(host::graphics_get_lod(&old),"legacy Host LOD callback remains available");
        const std::vector<ncmm_lod_patch_v1> frozen(old.patches,old.patches+old.count);const uint64_t revision=old.revision;
        ncmm_graphics_skyline_scene_v1 skyline{};skyline.struct_size=sizeof(skyline);
        const auto retained=host::graphics_lod_owned_bytes(),accounted=host::graphics_world_metrics.snapshot_bytes;
        check(host::graphics_get_skyline(&skyline)&&valid_skyline(skyline),"actual Host skyline producer validates");
        check(host::graphics_world_metrics.snapshot_bytes-accounted==host::graphics_lod_owned_bytes()-retained,
              "first lazy replacement allocation is included immediately in active Host snapshot ownership");
        check(skyline.count==(seen==4?1u:0u),"only fully disclosed standard radio sites publish a typed silhouette");
        if(seen<4)check(skyline.geography.patches==old.patches&&host::graphics_skyline_patches.capacity()==0,
            "ordinary/vague geography borrows V1 storage without another allocation or copy");
        check(host::terrain_reads==(seen?1:0),"unseen terrain is never read to infer a radio profile");
        if(seen==4){const auto &f=skyline.features[0];
            check(f.x==133&&f.y==6&&f.z==0&&f.width==5.81f&&f.depth==5.81f&&f.height==17.5f,
                  "Host profile matches pinned standard mapgen cell centres, footprint and seven real storeys");
            check(skyline.geography.patches[f.ground_patch].height==0&&frozen[f.ground_patch].height==5,
                  "replacement horizon removes exactly the duplicate V1 building silhouette");
            check(skyline.geography.revision!=revision,"replacement ground has its own content revision without altering legacy revision");
            const ncmm_lod_patch_v1 expected{120,0,0,24,24,5,{.43f,.42f,.39f},NCMM_LOD_BUILDING};
            check(!std::memcmp(&frozen[f.ground_patch],&expected,sizeof(expected)),"V1 radio remains its original BUILDING box byte for byte");
        }
        old.struct_size=sizeof(old);check(host::graphics_get_lod(&old)&&old.revision==revision&&old.count==frozen.size()&&
            !std::memcmp(old.patches,frozen.data(),frozen.size()*sizeof(frozen[0])),"new callback cannot mutate the legacy array or revision");
    }
    host::vision=host::om_vision_level::full;
    for(const char *id:{"radio_tower_1","radio_tower_quest","station_radio","office_tower","mod_radio_tower","tower"}){
        host::type=id;host::graphics_build_lod(0,0,0,24,true);
        ncmm_graphics_skyline_scene_v1 s{};s.struct_size=sizeof(s);check(host::graphics_get_skyline(&s)&&valid_skyline(s)&&!s.count,"unknown radio layouts and generic towers retain legacy geography only");
    }
    host::type="radio_tower";host::resident=false;host::terrain_reads=0;host::graphics_build_lod(0,0,0,24,true);
    ncmm_graphics_skyline_scene_v1 s{};s.struct_size=sizeof(s);
    check(host::graphics_get_skyline(&s)&&!s.count&&!host::terrain_reads,"unresident overmaps are neither loaded nor used for typed skyline");host::resident=true;
    constexpr float centers[4][2]={{13,6},{18,13},{11,18},{6,11}};
    for(int turn=0;turn<4;++turn){host::rotation=turn;host::target_x=-5;host::target_y=-3;host::graphics_build_lod(-7,11,2,24,true);s.struct_size=sizeof(s);
        check(host::graphics_get_skyline(&s)&&valid_skyline(s)&&s.count==1,"rotated radio at negative coordinates is still a valid authored site");
        check(s.features[0].x==-120+7+centers[turn][0]&&s.features[0].y==-72-11+centers[turn][1]&&s.features[0].z==-5,
              "far origin subtraction and all four rotations preserve actual cell-centre anchors");
    }
    host::rotation=0;host::target_y=0;
    for(int x:{0,1,2,3}){host::target_x=x;host::graphics_build_lod(0,0,0,24,true);s.struct_size=sizeof(s);
        check(host::graphics_get_skyline(&s)&&valid_skyline(s)&&!s.count,"typed silhouette obeys the same near-boundary and distance eligibility as V1");}
    host::target_x=0;host::graphics_build_lod(13,0,0,24,true);s.struct_size=sizeof(s);
    check(host::graphics_get_skyline(&s)&&!s.count,"partly clipped OMT cannot publish a full radio silhouette");
    host::target_x=5;
    for(bool enabled:{false,true}){host::graphics_build_lod(0,0,enabled?-1:0,24,enabled);s.struct_size=sizeof(s);
        check(host::graphics_get_skyline(&s)&&valid_skyline(s)&&!s.count&&!s.geography.enabled,"disabled/underground rebuild clears retained typed skyline");}
    host::graphics_build_lod(0,0,0,24,true);s.struct_size=sizeof(s);check(host::graphics_get_skyline(&s)&&s.count==1,"skyline can rebuild after a clear");
    host::graphics_multilevel_frame=false;check(!host::graphics_get_skyline(&s),"skyline pointers are callback-scoped");host::graphics_multilevel_frame=true;
    host::active_module_id=2;check(!host::graphics_get_skyline(&s),"another module cannot read callback-owned skyline");host::active_module_id=1;
    check(!host::graphics_get_skyline(nullptr),"null caller output is rejected");s.struct_size=sizeof(s)-1;
    check(!host::graphics_get_skyline(&s),"truncated caller output is rejected before writing");
    check(host::graphics_lod_owned_bytes()<256u*1024u,"retained Host geography/skyline arrays have a small finite capacity bound");
    std::cout<<"Radio Host retained geography/feature bytes: "<<host::graphics_lod_owned_bytes()<<" (clear releases all)\n";
    host::graphics_clear_lod();check(!host::graphics_lod_owned_bytes(),"provider clear releases feature and replacement-ground storage");
}
void skyline_render_regression(){
    ncmm_lod_patch_v1 ground{80,-4.5f,0,24,24,0,{.43f,.42f,.39f},NCMM_LOD_BUILDING};
    ncmm_skyline_feature_v1 radio{};
    check(ncmm_world::standard_radio_feature("radio_tower",true,0,ground,24,0,radio),"exact distant radio profile is eligible");
    ncmm_graphics_skyline_scene_v1 source{sizeof(source),1,1,0,31,
        {sizeof(ncmm_graphics_lod_scene_v1),1,1,1,30,0,0,&ground},&radio};
    check(ncmm_world::valid_skyline(source),"typed skyline validates its complete replacement geography");
    auto bad=source;bad.struct_size=sizeof(bad)-1;check(!ncmm_world::valid_skyline(bad),"truncated skyline payload rejected");
    bad=source;bad.version=2;check(!ncmm_world::valid_skyline(bad),"unknown skyline payload version rejected");
    bad=source;bad.reserved=1;check(!ncmm_world::valid_skyline(bad),"unknown skyline payload flags rejected");
    bad=source;bad.count=NCMM_SKYLINE_MAX_FEATURES+1;check(!ncmm_world::valid_skyline(bad),"feature count is bounded before any array access");
    bad=source;bad.features=nullptr;check(!ncmm_world::valid_skyline(bad),"nonempty skyline requires an array");
    bad=source;bad.geography.patches=nullptr;check(!ncmm_world::valid_skyline(bad),"replacement geography requires its own array");
    bad=source;bad.geography.enabled=0;check(!ncmm_world::valid_skyline(bad),"disabled geography cannot expose a feature");
    bad=source;bad.geography.version=2;check(!ncmm_world::valid_skyline(bad),"new skyline does not bypass V1 geography validation");
    const auto good_radio=radio;
    for(float ncmm_skyline_feature_v1::*member:{&ncmm_skyline_feature_v1::x,&ncmm_skyline_feature_v1::y,&ncmm_skyline_feature_v1::z,
         &ncmm_skyline_feature_v1::width,&ncmm_skyline_feature_v1::depth,&ncmm_skyline_feature_v1::height}){
        for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
            radio=good_radio;radio.*member=value;check(!ncmm_world::valid_skyline(source),"every published geometric field must be finite");}
    }
    radio=good_radio;radio.x=513;check(!ncmm_world::valid_skyline(source),"out-of-range feature position rejected");
    radio=good_radio;radio.z=305;check(!ncmm_world::valid_skyline(source),"feature top cannot exceed bounded world elevation");
    radio=good_radio;radio.height=18;check(!ncmm_world::valid_skyline(source),"generic guessed tower height is not a standard profile");
    radio=good_radio;radio.width=6;check(!ncmm_world::valid_skyline(source),"generic guessed tower footprint is not accepted");
    radio=good_radio;radio.kind=2;check(!ncmm_world::valid_skyline(source),"unknown typed skyline profile rejected");
    radio=good_radio;radio.rotation=4;check(!ncmm_world::valid_skyline(source),"invalid skyline rotation rejected");
    radio=good_radio;radio.reserved=1;check(!ncmm_world::valid_skyline(source),"unknown feature flags rejected");
    radio=good_radio;radio.ground_patch=1;check(!ncmm_world::valid_skyline(source),"feature cannot refer past its ground array");
    radio=good_radio;radio.x+=1;check(!ncmm_world::valid_skyline(source),"profile centre must match the authored rotated site");radio=good_radio;
    ground.height=5;check(!ncmm_world::valid_skyline(source),"duplicate legacy box cannot accompany a typed radio");ground.height=0;
    ground.kind=6;check(!ncmm_world::valid_lod(source.geography)&&!ncmm_world::valid_skyline(source),"V1 kind 6 remains invalid after the additive extension");ground.kind=NCMM_LOD_BUILDING;
    std::array<ncmm_skyline_feature_v1,2> duplicates{{radio,radio}};bad=source;bad.count=2;bad.features=duplicates.data();
    check(!ncmm_world::valid_skyline(bad),"one ground patch cannot carry two radio silhouettes");
    fixture open(1);presentation_data data;
    data.view.struct_size=sizeof(data.view);data.view.eye_height=1.55f;data.view.body_scale=1;data.view.held_length=.6f;
    data.cells.resize(open.positions.size());data.horizon_enabled=true;data.horizon_revision=source.revision;data.horizon={ground};data.skyline={radio};
    collector geometry;frame stats;
    renderer(open.world.appearance,0,0,{},stats,{},&data,{},nullptr,&open.world).collect(geometry,400,260);
    vec lo{1000,1000,1000},hi{-1000,-1000,-1000};size_t triangles=0,upper_triangles=0;
    for(const auto &t:geometry.triangles)if(t.m.texture==&radio_lattice_impostor(false)||t.m.texture==&radio_lattice_impostor(true)){
        ++triangles;upper_triangles+=t.m.texture==&radio_lattice_impostor(true);
        check(t.m.cutout&&t.m.distance_lod&&t.m.cell==-1,"radio cutouts are ordinary non-targetable distant depth geometry");
        for(const auto &v:t.v){lo={std::min(lo.x,v.p.x),std::min(lo.y,v.p.y),std::min(lo.z,v.p.z)};hi={std::max(hi.x,v.p.x),std::max(hi.y,v.p.y),std::max(hi.z,v.p.z)};
            if(t.m.texture==&radio_lattice_impostor(true))check(std::abs(v.p.x-radio.x)<=.906f&&std::abs(v.p.y-radio.y)<=.906f&&v.p.z>=15,
                "top remains narrow above six vertical lower storeys");}
    }
    check(triangles==16&&upper_triangles==8,"entire radio profile needs sixteen fixed cutout triangles");
    check(std::abs(hi.x-lo.x-5.81f)<.0001f&&std::abs(hi.y-lo.y-5.81f)<.0001f&&lo.z==0&&hi.z==17.5f,
          "actual emitted envelope matches the real near tower profile");
    for(bool top:{false,true}){const auto &image=radio_lattice_impostor(top);const auto &s=image.data;
        size_t holes=0,wires=0;for(uint32_t y=0;y<s.height;++y)for(uint32_t x=0;x<s.width;++x){holes+=(s.rgba[y*s.pitch_pixels+x]&255)==0;wires+=(s.rgba[y*s.pitch_pixels+x]&255)>64;}
        check(holes>s.width*s.height*.70&&wires>100,"radio projection retains real lattice openings and actual wires");
        check((image.sample(.5f,.5f)&255)==0,"empty middle of the real mast is not filled by invented wide diagonal braces");
    }
    frame radio_pixels,ground_pixels;
    check(build_frame(open.world.appearance,0,0,{},radio_pixels,400,260,{},nullptr,nullptr,&data,{},nullptr,&open.world),"CPU renders the authored skyline");
    auto flat=data;flat.skyline.clear();++flat.horizon_revision;
    check(build_frame(open.world.appearance,0,0,{},ground_pixels,400,260,{},nullptr,nullptr,&flat,{},nullptr,&open.world),"CPU ground-only control retains the same camera and light");
    size_t covered=0,high=0;
    for(size_t i=0;i<radio_pixels.depth.size();++i)if(radio_pixels.depth[i]!=ground_pixels.depth[i]&&std::isfinite(radio_pixels.depth[i])){
        ++covered;high+=i<size_t(radio_pixels.height/2-25)*radio_pixels.width;check(radio_pixels.owners[i]<0,"radio never publishes a furniture or creature owner");}
    check(covered>10&&high>2&&covered<3000,"real CPU pixels contain a tall open lattice, not a five-metre broad box");
    const size_t centre=size_t(radio_pixels.height/2-12)*radio_pixels.width+radio_pixels.width/2;
    check(radio_pixels.depth[centre]==ground_pixels.depth[centre],"CPU sees through the physical centre gap of the lattice");
    first_person::gpu::scene_cache cache;first_person::gpu::packet packet;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
    const bool collected=cache.build(open.world.appearance,0,0,{},packet,stats,host,{},&data,nullptr,400,260,NCMM_GPU_FORCE,&open.world);
    check(collected,("GPU collector builds the same authored profile: "+cache.last_reason).c_str());
    auto view=packet.finish(nullptr);size_t backgrounds=0;uint64_t key=0;bool cutout=false;std::vector<uint64_t> masks;
    for(const auto &draw:packet.draws)if(draw.cell==UINT32_MAX&&draw.pass==NCMM_GPU_OPAQUE){++backgrounds;key=draw.mesh_key;}
    for(uint32_t i=0;i<view.mesh_count;++i)for(uint32_t j=0;j<view.meshes[i].material_count;++j){const auto &m=view.meshes[i].materials[j];
        if((m.flags&NCMM_GPU_CUTOUT)&&(m.flags&NCMM_GPU_DISTANCE_LOD)){cutout=true;if(std::find(masks.begin(),masks.end(),m.texture_key)==masks.end())masks.push_back(m.texture_key);}}
    check(backgrounds==1&&cutout&&masks.size()==2,"typed skyline shares the existing single horizon draw and two retained masks");
    for(const auto mask:masks){bool leased=false;for(uint32_t i=0;i<view.texture_count;++i)leased|=view.textures[i].key==mask;
        check(leased,"both radio masks retain their owning texture leases in the cold packet");}
    ncmm_gpu_multilevel_scene_v1 world_gpu{};world_gpu.struct_size=sizeof(world_gpu);world_gpu.version=1;world_gpu.base=view;
    world_gpu.coordinates=open.world.coordinates;world_gpu.cell_count=open.world.cell_count;world_gpu.origin_x=open.world.origin_x;world_gpu.origin_y=open.world.origin_y;world_gpu.level_height=2.5f;
    check(!ncmm_gpu::validate(view,&world_gpu),"unchanged GPU V1 contract accepts the typed horizon geometry");
    first_person::gpu::packet warm;check(cache.build(open.world.appearance,.2f,.01f,{},warm,stats,host,{},&data,nullptr,400,260,NCMM_GPU_FORCE,&open.world),"camera turn keeps authored radio geometry resident");
    bool retained=false;for(const auto &draw:warm.draws)retained|=draw.cell==UINT32_MAX&&draw.mesh_key==key;
    check(retained,"fixed radio face cards do not rebuild on a camera turn");
    first_person::gpu::packet removed;check(cache.build(open.world.appearance,0,0,{},removed,stats,host,{},&flat,nullptr,400,260,NCMM_GPU_FORCE,&open.world),"radio removal invalidates the cached horizon");
    bool changed=false;for(const auto &draw:removed.draws)changed|=draw.cell==UINT32_MAX&&draw.mesh_key!=key;
    check(changed,"removed feature cannot remain in retained GPU geometry");
    presentation_data worst=data;worst.horizon.clear();worst.skyline.clear();++worst.horizon_revision;
    for(int y=-15;y<=15;++y)for(int x=-15;x<=15;++x){
        const uint32_t index=uint32_t(worst.horizon.size());worst.horizon.push_back({float(x*24),float(y*24),0,24,24,0,{.43f,.42f,.39f},NCMM_LOD_BUILDING});
        auto feature=radio;feature.x=x*24+13.f;feature.y=y*24+6.f;feature.ground_patch=index;worst.skyline.push_back(feature);}
    first_person::gpu::scene_cache worst_cache;first_person::gpu::packet bounded;
    check(worst_cache.build(open.world.appearance,0,0,{},bounded,stats,host,{},&worst,nullptr,400,260,NCMM_GPU_FORCE,&open.world),"all 961 possible radio sites stay inside the existing first-frame geometry budget");
    const auto worst_view=bounded.finish(nullptr);uint64_t bytes=0;
    for(uint32_t i=0;i<worst_view.mesh_count;++i){const auto &mesh=worst_view.meshes[i];bool distant=false;
        for(uint32_t j=0;j<mesh.material_count;++j)distant|=(mesh.materials[j].flags&NCMM_GPU_DISTANCE_LOD)!=0;
        if(distant)bytes+=uint64_t(mesh.vertex_count)*sizeof(ncmm_gpu_vertex_v1)+uint64_t(mesh.index_count)*sizeof(uint32_t)+uint64_t(mesh.material_count)*sizeof(ncmm_gpu_material_v1);}
    check(bytes<4u*1024u*1024u,"maximum typed horizon leaves upload headroom for the nearby world");
    for(int y=0;y<3;++y){const int i=open.cell(5,y,0);open.positions[i].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP;open.details[i].terrain_shape=NCMM_SHAPE_WALL;}
    check(build_frame(open.world.appearance,0,0,{},radio_pixels,400,260,{},nullptr,nullptr,&data,{},nullptr,&open.world),"radio and exact foreground architecture render together");
    check(radio_pixels.depth[size_t(radio_pixels.height/2)*radio_pixels.width+radio_pixels.width/2]<4,"typed horizon cannot change near wall depth or fill a near shaft");
    check(radio_lattice_owned_bytes()>0&&radio_lattice_owned_bytes()<2u*1024u*1024u&&horizon_owned_bytes(data)>=radio_lattice_owned_bytes()+
        data.horizon.capacity()*sizeof(ncmm_lod_patch_v1)+data.skyline.capacity()*sizeof(ncmm_skyline_feature_v1),"CPU/GPU accounting includes owned masks and retained metadata arrays");
    std::cout<<"Radio masks: lower "<<radio_lattice_impostor(false).data.width<<'x'<<radio_lattice_impostor(false).data.height
             <<", upper "<<radio_lattice_impostor(true).data.width<<'x'<<radio_lattice_impostor(true).data.height
             <<"; owned mask bytes "<<radio_lattice_owned_bytes()<<"; masks plus retained horizon metadata "<<horizon_owned_bytes(data)
             <<"; 961-site GPU horizon geometry "<<bytes<<" bytes; radio-only triangles "<<triangles<<'\n';
    clear_radio_lattices();check(!radio_lattice_owned_bytes(),"module teardown can release all procedural radio masks");
}
struct chain_wire {vec from,to;};
// Recover the centreline from the two actual triangles of each narrow wire
// quad. Posts and rails have a different section; this does not duplicate the
// generator's phase formula or mistake coincident bounds for a joined mesh.
std::vector<chain_wire> chain_wires(fixture &scene){
    collector mesh;frame stats;
    renderer(scene.world.appearance,0,0,{},stats,{},nullptr,{},nullptr,&scene.world).collect(mesh,400,260);
    std::vector<chain_wire> result;
    for(size_t i=0;i+1<mesh.triangles.size();i+=2){const auto &a=mesh.triangles[i],&b=mesh.triangles[i+1];
        if(a.m.finish!=surface::metal||std::abs(a.normal.z)>.001f)continue;
        const vec first=b.v[2].p-a.v[0].p,last=a.v[2].p-a.v[1].p;
        if(std::abs(dot(first,first)-.000128f)>.0000001f||std::abs(dot(last,last)-.000128f)>.0000001f)continue;
        const vec from=(a.v[0].p+b.v[2].p)*.5f,to=(a.v[1].p+a.v[2].p)*.5f;
        if(dot(to-from,to-from)>.002f)result.push_back({from,to});
    }
    check(result.size()>=20,"chain-link regression actually samples diagonal wire geometry");return result;
}
struct chain_end {float height,slope;};
std::vector<chain_end> chain_ends(const std::vector<chain_wire> &wires,vec seam,vec direction,bool before){
    std::vector<chain_end> result;
    for(const auto &wire:wires){const float a=dot(wire.from-seam,direction),b=dot(wire.to-seam,direction);
        const float side_a=cross(wire.from-seam,direction).z,side_b=cross(wire.to-seam,direction).z;
        if(std::abs(side_a)>.0001f||std::abs(side_b)>.0001f||std::abs(a-b)<.05f)continue;
        const bool first=std::abs(a)<.0001f,last=std::abs(b)<.0001f;
        if(!first&&!last)continue;const float other=first?b:a;
        if(before?other>=-.05f:other<=.05f)continue;
        const float height=first?wire.from.z:wire.to.z;
        // Keep the sampling cut between physical wire heights. A line at
        // exactly .20 m can otherwise land on opposite sides of a float
        // comparison after rotating a reflected half-panel.
        if(height<.213f||height>1.637f)continue;
        result.push_back({height,(wire.to.z-wire.from.z)/(b-a)});
    }
    return result;
}
void chain_seam(const std::vector<chain_wire> &wires,vec seam,vec direction){
    const auto before=chain_ends(wires,seam,direction,true),after=chain_ends(wires,seam,direction,false);
    check(before.size()>=12&&before.size()==after.size(),"both sides of a fence seam retain the same real wire endpoints");
    for(const auto &end:before){check(std::abs(std::abs(end.slope)-1)<.0001f,"diagonal wire keeps its physical pitch");
        check(std::any_of(after.begin(),after.end(),[&](const auto &next){return std::abs(next.height-end.height)<.0001f&&std::abs(next.slope-end.slope)<.0001f;}),
              "straight chain-link wires must continue collinearly across a half-panel or cell seam");}
}
void chain_same(const std::vector<chain_wire> &before,const std::vector<chain_wire> &after,vec shift){
    check(before.size()==after.size(),"snapshot origin changes cannot add or remove fence wires");
    for(size_t i=0;i<before.size();++i)for(const auto &pair:{std::pair<vec,vec>{before[i].from,after[i].from},std::pair<vec,vec>{before[i].to,after[i].to}}){
        const vec delta=pair.second-pair.first-shift;
        check(dot(delta,delta)<.0000001f,"the same absolute fence keeps its wire phase after a snapshot origin shift");}
}
void chain_link_regression(){
    for(const auto absolute:{std::pair<int,int>{17,29},{-113,-197},{-20000003,19999991}})for(bool vertical:{false,true}){
        fixture scene(1);const int ax=scene.world.origin_x,ay=scene.world.origin_y;
        for(auto &p:scene.positions){p.x+=absolute.first-ax;p.y+=absolute.second-ay;}
        scene.world.origin_x=absolute.first;scene.world.origin_y=absolute.second;
        const uint32_t mask=vertical?NCMM_SURFACE_CONNECT_N|NCMM_SURFACE_CONNECT_S:NCMM_SURFACE_CONNECT_E|NCMM_SURFACE_CONNECT_W;
        for(int offset=-1;offset<=1;++offset){const int at=scene.cell(8+(vertical?0:offset),1+(vertical?offset:0),0);
            std::strcpy(scene.appearance[at].terrain,"t_chainfence");scene.details[at].terrain_shape=NCMM_SHAPE_FENCE;
            scene.details[at].terrain_flags=NCMM_SURFACE_CONNECTIONS|mask;}
        check(ncmm_world::valid(scene.world),"chain-link regression uses valid positive, negative and distant world coordinates");
        const auto wires=chain_wires(scene);const vec along=vertical?vec{0,1,0}:vec{1,0,0},center{8.5f,1.5f,0};
        for(float sign:{-1.f,1.f}){chain_seam(wires,center,along*sign);chain_seam(wires,center+along*.5f,along*sign);}
        // Keep all physical cells unchanged while a wider snapshot starts at a
        // different origin. The same geometry moves locally, never in world Z.
        scene.world.origin_x-=3;scene.world.origin_y-=2;scene.world.appearance.base.grid_width+=6;scene.world.appearance.base.grid_height+=4;
        scene.world.appearance.base.camera_x+=3;scene.world.appearance.base.camera_y+=2;
        check(ncmm_world::valid(scene.world),"shifted sparse fence snapshot remains a valid world");
        chain_same(wires,chain_wires(scene),{3,2,0});
    }
    for(unsigned turn=0;turn<4;++turn){fixture corner(1);const int at=corner.cell(8,1,0);
        corner.details[at].terrain_shape=NCMM_SHAPE_FENCE;corner.details[at].terrain_flags=NCMM_SURFACE_CONNECTIONS|
            (NCMM_SURFACE_CONNECT_E<<turn)|(NCMM_SURFACE_CONNECT_E<<((turn+1)%4));
        const auto wires=chain_wires(corner);const vec center{8.5f,1.5f,0};
        const vec from=quarter_turn({1,0,0},turn),to=quarter_turn({1,0,0},turn+1);
        const auto a=chain_ends(wires,center,from,false),b=chain_ends(wires,center,to,false);
        check(a.size()>=12&&a.size()==b.size(),"both leaves of a chain-link corner have complete endpoint coverage");
        for(const auto &end:a)check(std::any_of(b.begin(),b.end(),[&](const auto &next){return std::abs(next.height-end.height)<.0001f;}),
                                  "wire heights meet when the fence turns through a corner");
    }
    for(unsigned turn=0;turn<2;++turn){fixture gate(1);const int at=gate.cell(8,1,0);
        gate.details[at].terrain_shape=NCMM_SHAPE_GATE_OPEN;gate.details[at].orientation=turn;
        const auto wires=chain_wires(gate);const float angle=turn*pi*.5f+pi*.46f;const vec along{std::cos(angle),std::sin(angle),0};
        for(const auto &wire:wires){const vec delta=wire.to-wire.from;const float run=dot(delta,along);
            check(std::abs(std::abs(delta.z/run)-1)<.0001f&&std::abs(cross(delta,along).z)<.0001f,"opened gate keeps a rigid, hinged wire panel");
            check(wire.from.z>=.0799f&&wire.to.z<=1.8001f,"gate wires stay inside the existing vertical limits");}
        gate.world.origin_x-=3;gate.world.origin_y-=2;gate.world.appearance.base.grid_width+=6;gate.world.appearance.base.grid_height+=4;
        gate.world.appearance.base.camera_x+=3;gate.world.appearance.base.camera_y+=2;
        chain_same(wires,chain_wires(gate),{3,2,0});
    }
}
// Optional offline readback of the real CPU renderer. No window, input,
// alternate wire rasterizer or image post-processing participates here.
void chain_captures(const std::filesystem::path &directory){
    std::filesystem::create_directories(directory);
    for(const std::string name:{"straight-front","straight-oblique","straight-vertical","corner","gate-open"}){
        fixture scene(1);
        for(int y=3;y<24;++y)for(int x=0;x<24;++x){
            scene.positions.push_back({scene.world.origin_x+x,scene.world.origin_y+y,0,NCMM_WORLD_BODY_VISIBLE,0,0,1});
            ncmm_graphics_cell_v1 appearance{};appearance.flags=NCMM_GFX_VISIBLE;appearance.light=1;scene.appearance.push_back(appearance);
            ncmm_graphics_cell_v2 detail{};detail.light_r=detail.light_g=detail.light_b=.9f;scene.details.push_back(detail);
        }
        for(size_t i=0;i<scene.positions.size();++i){scene.positions[i].flags|=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP;
            scene.appearance[i].flags=NCMM_GFX_VISIBLE;std::strcpy(scene.appearance[i].terrain,"t_floor");std::strcpy(scene.details[i].ground,"t_floor");}
        scene.world.cell_count=uint32_t(scene.positions.size());scene.world.coordinates=scene.positions.data();
        scene.slices[0].cell_count=scene.world.cell_count;
        auto &base=scene.world.appearance.base;base.grid_height=24;base.cells=scene.appearance.data();base.cell_count=scene.appearance.size();
        base.pixel_width=1280;base.pixel_height=800;
        scene.world.appearance.details=scene.details.data();scene.world.appearance.detail_count=scene.details.size();
        scene.world.appearance.environment.daylight=.95f;scene.world.appearance.environment.sun_x=.6f;
        scene.world.appearance.environment.sun_y=-.4f;scene.world.appearance.environment.sun_z=.69f;
        auto fence=[&](int x,int y,uint32_t connections){const int i=scene.cell(x,y,0);
            std::strcpy(scene.appearance[i].terrain,"t_chainfence");scene.details[i].terrain_shape=NCMM_SHAPE_FENCE;
            scene.details[i].terrain_flags=NCMM_SURFACE_CONNECTIONS|connections;};
        float yaw=pi*.5f,pitch=-.17f;base.camera_x=12.5f;base.camera_y=6.5f;
        if(name=="straight-vertical"){
            for(int y=3;y<=20;++y)fence(12,y,(y>3?uint32_t(NCMM_SURFACE_CONNECT_N):0u)|(y<20?uint32_t(NCMM_SURFACE_CONNECT_S):0u));
            base.camera_x=9.5f;base.camera_y=12.5f;yaw=0;
        }else if(name=="corner"){
            for(int x=12;x<=20;++x)fence(x,10,(x>12?uint32_t(NCMM_SURFACE_CONNECT_W):uint32_t(NCMM_SURFACE_CONNECT_S))|(x<20?uint32_t(NCMM_SURFACE_CONNECT_E):0u));
            for(int y=11;y<=19;++y)fence(12,y,NCMM_SURFACE_CONNECT_N|(y<19?uint32_t(NCMM_SURFACE_CONNECT_S):0u));
            base.camera_x=9.0f;base.camera_y=7.0f;yaw=pi*.25f;pitch=-.12f;
        }else{
            for(int x=3;x<=21;++x)fence(x,10,(x>3?uint32_t(NCMM_SURFACE_CONNECT_W):0u)|(x<21?uint32_t(NCMM_SURFACE_CONNECT_E):0u));
            if(name=="straight-oblique"){base.camera_x=6.5f;base.camera_y=7.5f;yaw=.72f;pitch=-.12f;}
            if(name=="gate-open"){const int gate=scene.cell(12,10,0);
                std::strcpy(scene.appearance[gate].terrain,"t_chaingate_o");scene.details[gate].terrain_shape=NCMM_SHAPE_GATE_OPEN;
                base.camera_x=15.5f;base.camera_y=7.0f;yaw=std::atan2(3.8f,-3.3f);pitch=-.12f;}
        }
        frame image;
        if(!build_frame(scene.world.appearance,yaw,pitch,{},image,1280,800,{},nullptr,nullptr,nullptr,{},nullptr,&scene.world))
            throw std::runtime_error("chain capture could not render its physical fixture");
        std::ofstream out(directory/(name+".ppm"),std::ios::binary);out<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";
        for(const uint32_t pixel:image.pixels){const char rgb[]={char(pixel>>24),char(pixel>>16),char(pixel>>8)};out.write(rgb,3);}
        if(!out)throw std::runtime_error("chain capture could not write its CPU readback");
        std::cout<<"CPU chain capture "<<name<<" "<<image.width<<'x'<<image.height<<'\n';
    }
}
}
int main(int argc,char **argv){try{
    if(argc>1){
        if(argc!=3||std::string(argv[1])!="--chain-captures")throw std::runtime_error("usage: first_person_multilevel_render_tests [--chain-captures directory]");
        chain_captures(std::filesystem::path(argv[2]));return 0;
    }
    static_stair_permission_regression();chain_link_regression();skyline_host_regression();skyline_render_regression();
    for(const auto &test:{std::pair<const char *,unsigned>{"t_railroad_track_h",1},{"t_railroad_track_on_tie_v",2},{"t_railroad_track_d1",4},{"t_railroad_track_d2",8}}){
        fixture rail(1);rail.slab(0);const int at=rail.cell(6,1,0);std::strcpy(rail.appearance[at].terrain,test.first);
        check(ncmm_rail::track(test.first)&&ncmm_rail::axes(test.first)==test.second,"Native rail suffix selects its physical direction");
        collector geometry;frame stats;renderer(rail.world.appearance,0,-.2f,{},stats,{},nullptr,{},nullptr,&rail.world).collect(geometry,400,260);
        vec lo{100,100,100},hi{-100,-100,-100};size_t raised=0;
        for(const auto &t:geometry.triangles)if(t.m.cell==at&&t.m.finish==surface::metal)for(const auto &v:t.v){++raised;
            lo={std::min(lo.x,v.p.x),std::min(lo.y,v.p.y),std::min(lo.z,v.p.z)};hi={std::max(hi.x,v.p.x),std::max(hi.y,v.p.y),std::max(hi.z,v.p.z)};}
        check(raised>50&&std::abs(hi.z-.19f)<.0001f&&lo.z>.06f,"rails have a raised web, foot and head rather than a floor decal");
        check(test.second==1?hi.x-lo.x>.99f&&hi.y-lo.y<.16f:test.second==2?hi.y-lo.y>.99f&&hi.x-lo.x<.16f:true,"straight rails join the correct adjacent tiles");
    }
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
        // The borrowed lookup must emit exactly the same ordered geometry as
        // the CPU-owned lookup across camera directions and loaded Z windows.
        ncmm_world::coordinates shared_index(&world.world,true);
        for(float yaw:{0.f,.6f,1.4f})for(float pitch:{-.4f,.2f,1.2f}){
            collector owned,shared;frame a,b;
            renderer(world.world.appearance,yaw,pitch,{},a,{},nullptr,{},nullptr,&world.world).collect(owned,400,260);
            renderer(world.world.appearance,yaw,pitch,{},b,{},nullptr,{},nullptr,&world.world,&shared_index).collect(shared,400,260);
            check(owned.triangles.size()==shared.triangles.size(),"shared XYZ lookup preserves triangle count");
            for(size_t i=0;i<owned.triangles.size();++i){const auto &left=owned.triangles[i],&right=shared.triangles[i];
                check(left.m.cell==right.m.cell&&left.m.finish==right.m.finish&&left.m.alpha==right.m.alpha&&left.m.texture==right.m.texture,"shared XYZ lookup preserves material and ownership");
                for(int v=0;v<3;++v){const auto &x=left.v[v],&y=right.v[v];
                    check(x.p.x==y.p.x&&x.p.y==y.p.y&&x.p.z==y.p.z&&x.u==y.u&&x.v==y.v&&x.light.x==y.light.x&&x.light.y==y.light.y&&x.light.z==y.light.z,"shared XYZ lookup preserves exact geometry UV and light");}
            }
        }
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
        for(const auto &draw:packet.draws){if(draw.pass==NCMM_GPU_HELD)continue;check(draw.cell<world.positions.size(),"GPU draw preserves sparse source cell");
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
    // The visible PSX hands have their own depth pass. Audit unseen world
    // pixels independently so a correctly framed hand cannot imitate a floor.
    size_t background=0,dark_pixels=0,held_pixels=0;
    for(size_t i=0;i<image.pixels.size();++i){if(std::isfinite(image.held_depth[i])){++held_pixels;continue;}
        ++background;const auto p=image.pixels[i];dark_pixels+=(p>>24)<32&&((p>>16)&255)<32&&((p>>8)&255)<32;}
    check(held_pixels>0&&image.viewmodel_draws==2,"both real hands remain visible over unseen space");
    check(background>0&&dark_pixels>background*.99,"downward unseen world is dark, apart from the UI crosshair");
    empty.positions[empty.cell(4,1,0)].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
    collector stairs;frame statistics;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(stairs,400,260);
    check(stairs.triangles.size()>80,"semantic staircase is stepped volume, not a floor square");
    const int stair_cell=empty.cell(4,1,0);empty.details[stair_cell].terrain_shape=NCMM_SHAPE_WALL;
    collector hinted;renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(hinted,400,260);
    check(hinted.triangles.size()==stairs.triangles.size(),"legacy wall hint cannot add a duplicate transition volume");
    empty.details[stair_cell].terrain_shape=NCMM_SHAPE_FLAT;
    float top=0;for(const auto &t:stairs.triangles)for(const auto &v:t.v)top=std::max(top,v.p.z);
    check(top>=NCMM_WORLD_LEVEL_HEIGHT,"staircase connects a full physical Z storey");
    for(uint32_t facing=0;facing<4;++facing){
        empty.positions[stair_cell].direction=facing;collector turned;
        renderer(empty.world.appearance,0,0,{},statistics,{},nullptr,{},nullptr,&empty.world).collect(turned,400,260);
        unsigned tread=0;
        for(const auto &t:turned.triangles)if(t.m.cell==stair_cell&&t.normal.z>.9f&&std::abs(t.v[0].p.z-NCMM_WORLD_LEVEL_HEIGHT)<.0001f){
            ++tread;for(const auto &v:t.v)check(v.p.x>=4.0799f&&v.p.x<=4.9201f&&v.p.y>=1.0799f&&v.p.y<=1.9201f,"rotated top tread fits the real stair aperture");
        }
        check(tread>0,"every staircase facing reaches its upper opening");
    }
    empty.positions[stair_cell].direction=0;
    {
        // The rural church's first basement UP is one tile away from its
        // ground-floor DOWN; a rock-wall tile sits below that upper exit.
        fixture shifted(2);shifted.slab(0);shifted.slab(1);
        const int root=shifted.cell(4,1,0),wall=shifted.cell(5,1,0),upper=shifted.cell(5,1,1);
        shifted.positions[root].flags|=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
        shifted.positions[wall].flags|=NCMM_WORLD_OPAQUE;shifted.details[wall].terrain_shape=NCMM_SHAPE_WALL;
        std::strcpy(shifted.appearance[wall].terrain,"t_rock_wall");
        shifted.positions[upper].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_DOWN|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
        collector generated;renderer(shifted.world.appearance,0,.3f,{},statistics,{},nullptr,{},nullptr,&shifted.world).collect(generated,400,260);
        unsigned top_steps=0,cut_wall=0;
        for(const auto &t:generated.triangles){
            if(t.m.cell==root&&t.normal.z>.9f&&std::abs(t.v[0].p.z-2.5f)<.0001f){++top_steps;
                for(const auto &v:t.v)check(v.p.x>=5.0799f&&v.p.x<=5.9201f,"offset staircase ends in the actual upper stair tile");}
            if(t.m.cell==wall){++cut_wall;for(const auto &v:t.v)if(v.p.z>1.1001f&&std::abs(v.p.y-1.5f)<.379f)
                check(v.p.x>=5.9399f,"upper flight is not filled by the Native wall's assumed full-height cube");}
        }
        check(top_steps&&cut_wall,"offset staircase and its Native wall support both retain geometry");
        first_person::gpu::scene_cache cache;first_person::gpu::packet a,b;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        check(cache.build(shifted.world.appearance,0,.3f,{},a,statistics,host,{},nullptr,nullptr,400,260,NCMM_GPU_FORCE,&shifted.world),"offset stair GPU cache");
        uint64_t old_mesh=0;for(const auto &draw:a.draws)if(draw.cell==unsigned(root))old_mesh=draw.mesh_key;
        shifted.positions[upper].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
        const int other=shifted.cell(3,1,1);shifted.positions[other].flags=NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_DOWN|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM;
        check(cache.build(shifted.world.appearance,0,.3f,{},b,statistics,host,{},nullptr,nullptr,400,260,NCMM_GPU_FORCE,&shifted.world),"changed offset stair GPU cache");
        uint64_t new_mesh=0;for(const auto &draw:b.draws)if(draw.cell==unsigned(root))new_mesh=draw.mesh_key;
        check(old_mesh&&new_mesh&&old_mesh!=new_mesh,"neighbour exit changes invalidate the otherwise unchanged stair mesh");
    }
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
        // The worst known-forest snapshot must fit beside the detailed world
        // inside the Host's 16 MiB per-frame geometry upload allowance.
        presentation_data forest=data;forest.horizon.clear();forest.horizon_revision=10;
        for(int y=-15;y<=15;++y)for(int x=-15;x<=15;++x)
            forest.horizon.push_back({float(x*24),float(y*24),0,24,24,5.5f,{.2f,.35f,.14f},NCMM_LOD_FOREST});
        first_person::gpu::scene_cache forest_cache;first_person::gpu::packet forest_packet;
        check(forest_cache.build(open.world.appearance,0,-.08f,{},forest_packet,stats,host,{},&forest,nullptr,400,260,NCMM_GPU_FORCE,&open.world),"complete known-forest horizon builds without warming or CPU fallback");
        auto forest_view=forest_packet.finish(nullptr);uint64_t forest_bytes=0;bool forest_billboards=false;
        for(uint32_t i=0;i<forest_view.mesh_count;++i){const auto &mesh=forest_view.meshes[i];
            bool distant=false;for(uint32_t j=0;j<mesh.material_count;++j)distant|=(mesh.materials[j].flags&NCMM_GPU_DISTANCE_LOD)!=0;
            if(!distant)continue;
            forest_bytes+=uint64_t(mesh.vertex_count)*sizeof(ncmm_gpu_vertex_v1)+uint64_t(mesh.index_count)*sizeof(uint32_t)+uint64_t(mesh.material_count)*sizeof(ncmm_gpu_material_v1);
            for(uint32_t j=0;j<mesh.vertex_count;++j)forest_billboards|=(mesh.vertices[j].flags&NCMM_GPU_BILLBOARD)!=0;
        }
        check(forest_billboards,"far forest uses shader-facing geometric impostors");
        check(forest_bytes<3u*1024u*1024u,"worst-case forest leaves first-frame upload capacity for the nearby world");
        // A near opaque wall must win over the entire distant layer.
        for(int y=0;y<3;++y){const int i=open.cell(5,y,0);open.positions[i].flags=NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_OPAQUE|NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP;open.details[i].terrain_shape=NCMM_SHAPE_WALL;}
        check(build_frame(open.world.appearance,0,0,{},horizon_image,400,260,{},nullptr,nullptr,&data,{},nullptr,&open.world),"near wall with distant world renders");
        check(horizon_image.depth[size_t(horizon_image.height/2)*horizon_image.width+horizon_image.width/2]<4,"near architecture correctly occludes the horizon");
    }
    {
        fixture scene(3);const int wire=scene.cell(6,1,0),tower=scene.cell(10,1,0),crown=scene.cell(15,1,1),loot=scene.cell(5,1,1);
        scene.details[wire].terrain_shape=NCMM_SHAPE_FENCE;
        scene.details[wire].terrain_flags=NCMM_SURFACE_CONNECTIONS|NCMM_SURFACE_CONNECT_E|NCMM_SURFACE_CONNECT_N;
        scene.details[tower].terrain_shape=NCMM_SHAPE_LATTICE_TOWER;
        scene.details[crown].terrain_shape=NCMM_SHAPE_TREE_CANOPY;
        std::strcpy(scene.appearance[crown].terrain,"t_treetop_evergreen");
        std::strcpy(scene.appearance[loot].item,"9mm_casing");
        scene.positions[loot].flags|=NCMM_WORLD_FLOOR|NCMM_WORLD_FLOOR_TOP;
        collector mesh;frame stats;renderer(scene.world.appearance,0,.1f,{},stats,{},nullptr,{},nullptr,&scene.world).collect(mesh,400,260);
        bool east=false,north=false,canopy=false,inventory=false;float tower_area=0;
        for(const auto &t:mesh.triangles){
            if(t.m.cell==wire)for(const auto &v:t.v){east|=v.p.x>6.99f;north|=v.p.y<1.01f;}
            if(t.m.cell==tower){tower_area+=std::abs(cross(t.v[1].p-t.v[0].p,t.v[2].p-t.v[0].p).y)*.5f;
                for(const auto &v:t.v)check(v.p.x>10.05f&&v.p.x<10.95f,"tower is an open lattice inside its physical cell");}
            if(t.m.cell==crown){canopy=true;check(t.m.cutout,"tree crown cannot emit an opaque concrete plate");
                check(t.m.texture==&canopy_impostor(true),"upper evergreen crown uses foliage instead of a top-down log atlas fallback");
                size_t leaves=0,clear=0;for(auto pixel:t.m.texture->filtered){leaves+=(pixel&255)>0;clear+=(pixel&255)==0;}
                check(leaves>1000&&clear>1000,"upper crown has a real foliage silhouette with transparent corners");}
            if(t.m.cell==loot&&t.m.cutout){inventory=true;check(!t.m.gpu_billboard&&t.m.gpu_backface,"floor loot is a top-facing physical surface");
                for(const auto &v:t.v)check(std::abs(v.p.z-(NCMM_WORLD_LEVEL_HEIGHT+.018f))<.0001f,"upper loot is anchored to its true supporting level");}
        }
        check(east&&north,"an L shaped chain-link corner reaches both shared cell boundaries");
        check(tower_area>0&&tower_area<4,"lattice projected steel area is below the two solid box faces");
        check(canopy&&inventory,"canopy and elevated supported loot regression meshes are exercised");
        const auto &tree=forest_impostor();size_t leaves=0,clear=0;
        for(int y=0;y<128;++y)for(int x=0;x<96;++x){const auto alpha=tree.data.rgba[y*96+x]&255;leaves+=alpha>0;clear+=alpha==0;}
        check(leaves>1500&&clear>1500,"forest impostor has an irregular alpha silhouette rather than a solid polygon");
        check(tree.data.rgba[127*96+48]&255,"distant tree trunk reaches its ground pivot");
    }
    std::cout<<"Multi-level rendering: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
