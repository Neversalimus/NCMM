#include "world_visual_fixture.hpp"
#include "ncmm_gpu_validation.hpp"
#include <iostream>
#include <set>
using namespace world_visual;
int main(int argc,char **argv){try{
    const std::string prefix=argc>1?argv[1]:"";fixture f;const int at=f.sample();
    for(size_t id=0;id<size_t(props::id::count);++id){const auto &mesh=props::get(props::id(id));
        require(mesh.count>0&&mesh.count<=8000,"bounded compiled source mesh");
        require(mesh.dimensions.x>0&&mesh.dimensions.y>0&&mesh.dimensions.z>0,"source dimensions positive");
        for(size_t n=0;n<mesh.count;++n)for(const auto &p:mesh.triangles[n].vertices)
            require(std::isfinite(p.x+p.y+p.z)&&p.z>=-.00001f&&p.z<=mesh.dimensions.z+.00001f,"prop rests on Native floor with finite authored coordinates");}
    for(const auto *id:objects){f.object(at,id);const auto geometry=collect(f);size_t count=0;hybrid::vec lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
        for(const auto &t:geometry.triangles)if(t.m.cell==at&&!t.m.grass)for(const auto &v:t.v){if(v.p.z>.02f)++count;
            lo={std::min(lo.x,v.p.x),std::min(lo.y,v.p.y),std::min(lo.z,v.p.z)};hi={std::max(hi.x,v.p.x),std::max(hi.y,v.p.y),std::max(hi.z,v.p.z)};}
        if(count<18||hi.z-lo.z<.15f)std::cerr<<"Object "<<id<<" vertices="<<count<<" height="<<hi.z-lo.z<<'\n';
        require(count>=18&&hi.x-lo.x>.1f&&hi.y-lo.y>.1f&&hi.z-lo.z>.15f,"every common or unknown furniture object has real depth and height");
        if(!prefix.empty())save(f.render(),prefix+"-"+id);f.cells[at].furniture[0]=0;f.details[at].furniture_shape=NCMM_SHAPE_FLAT;}
    // All fallback mechanisms and every attachment bit combination use their
    // own sight. The M27 screenshot exercises this path, not the SCAR asset.
    for(uint32_t kind=NCMM_GUN_PISTOL;kind<=NCMM_GUN_BOW;++kind)for(uint32_t mods=0;mods<512;++mods)for(float length:{.12f,.8f,2.f}){
        auto profile=weapon::fallback(kind,mods,length,"modular_m27_assault_rifle");animation::pose pose;pose.grip=NCMM_GRIP_GUN;pose.aim=1;
        const auto p=weapon::animate(profile.sight,pose,weapon::aim_offset(pose,profile));
        require(std::abs(p.x)<1e-6f&&std::abs(p.y)<1e-6f&&p.z>.08f,"all fallback sights centre on the reticle at every supported length/mod combination");}
    for(uint32_t kind=NCMM_GUN_PISTOL;kind<=NCMM_GUN_BOW;++kind)for(uint32_t mods:{0u,1u,511u})for(auto viewport:{std::pair<int,int>{448,640},{640,480},{1768,1084}}){
        animation::pose pose;pose.grip=NCMM_GRIP_GUN;pose.aim=1;pose.ammo_loaded=true;pose.length=.8f;
        std::strcpy(f.scene.environment.held_item,kind==NCMM_GUN_RIFLE?"modular_m27_assault_rifle":"unmapped_native_gun");
        f.appearance.view.gun_kind=kind;f.appearance.view.gunmods=mods;
        auto image=f.render(pose,-hybrid::pi*.5f,0,viewport.first,viewport.second);
        if(image.viewmodel_errors)std::cerr<<"fallback kind="<<kind<<" mods="<<mods<<" size="<<viewport.first<<'x'<<viewport.second<<'\n';
        require(!image.viewmodel_errors&&image.viewmodel_draws==2,"fallback gun/bow keeps both PSX hands in portrait and wide ADS");
        if(mods&NCMM_MOD_OPTIC&&kind!=NCMM_GUN_BOW){size_t clear=0;for(int y=viewport.second/2-2;y<=viewport.second/2+2;++y)for(int x=viewport.first/2-2;x<=viewport.first/2+2;++x)clear+=sky(image.pixels[size_t(y)*viewport.first+x]);
            require(clear>=10,"actual fallback optic aperture shows the world, not an opaque cap");}
        if(!prefix.empty()&&viewport.first==640)save(image,prefix+"-ads-"+std::to_string(kind)+"-"+std::to_string(mods));}
    f.scene.environment.held_item[0]=0;f.appearance.view.gun_kind=0;f.appearance.view.gunmods=0;
    for(const auto *id:{"t_sewage_pipe","t_sewage_pipe_v","t_sewage_pipe_h"}){f.object(at,id,true);const auto geometry=collect(f);
        std::set<int> y,z;for(const auto &t:geometry.triangles)if(t.m.cell==at&&!t.m.grass)for(const auto &v:t.v)if(v.p.z>.1f){y.insert(int(v.p.y*1000));z.insert(int(v.p.z*1000));}
        require(y.size()>=6&&z.size()>=6,"pipe has a round section rather than a four-corner billboard");}
    f.object(at-1,"t_sewage_pipe",true);f.object(at-f.size,"t_sewage_pipe",true);if(!prefix.empty())save(f.render(),prefix+"-pipe-junction");
    {fixture room(33);room.scene.base.camera_x=5.5f;room.scene.base.camera_y=10.5f;
        std::set<unsigned> chairs;for(int y=2;y<8;y+=2)for(int x=2;x<9;x+=2){const int cell=y*room.size+x;room.object(cell,"f_chair");chairs.insert(unsigned(cell));}
        gpu::scene_cache cache;gpu::packet cold;hybrid::frame stats;ncmm_gpu_metrics_v1 host{};host.resource_epoch=1;
        require(cache.build(room.scene,-hybrid::pi*.5f,-.2f,room.provider(),cold,stats,host,{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"collect repeated world props");
        require(!ncmm_gpu::validate(cold.finish(nullptr)),"world props obey unchanged GPU contract");std::set<uint64_t> models;
        for(const auto &d:cold.draws)if(chairs.count(d.cell)&&d.pass==NCMM_GPU_OPAQUE)models.insert(d.mesh_key);
        require(models.size()==1,"identical authored chairs in different cells share one exact immutable opaque mesh");
        gpu::packet warm;require(cache.build(room.scene,-hybrid::pi*.5f+.1f,-.2f,room.provider(),warm,stats,host,{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"turn around static props");
        require(warm.finish(nullptr).generated_cells==0,"camera turns do not rebuild static props");
        room.scene.base.camera_x=room.scene.base.camera_y=31.5f;gpu::packet distant;require(cache.build(room.scene,-hybrid::pi*.5f,-.2f,room.provider(),distant,stats,host,{},&room.appearance,nullptr,640,480,NCMM_GPU_FORCE),"cross prop distance LOD");
        require(distant.finish(nullptr).generated_cells>0,"crossing prop distance LOD invalidates the retained detailed geometry");}
    f=fixture(); // vector-backed scene pointers are rebound below, never copied.
    f.scene.base.cells=f.cells.data();f.scene.details=f.details.data();
    for(int y=0;y<f.size;++y)for(int x=0;x<f.size;++x)f.terrain(y*f.size+x,x<f.size/2?"t_pavement":"t_grass");
    auto edge=collect(f);size_t feather=0;for(const auto &t:edge.triangles){require(t.m.alpha>=.99f,"terrain feather needs no additional transparent geometry");if(t.m.cell==at&&!t.m.grass&&t.normal.z>.99f)++feather;}
    require(feather==2&&edge.floor_colors[at].size()>8,"smooth material blend stays in the two existing floor triangles");
    f.cells[at-1].flags=0;auto isolated=collect(f);require(isolated.floor_colors[at].size()==1,"unseen neighbour cannot supply a material transition");f.cells[at-1].flags=NCMM_GFX_VISIBLE;
    f.cells[at].flags|=NCMM_GFX_NO_FLOOR;isolated=collect(f);require(isolated.floor_colors[at].empty(),"transition cannot cap an open floor");f.cells[at].flags&=~NCMM_GFX_NO_FLOOR;
    for(unsigned mask=1;mask<16;++mask){const auto plan=ncmm_grass::patch(mask,0,91);
        for(auto p:{ncmm_grass::xy{.03f,.47f},{.56f,.98f},{.92f,.11f}}){const auto q=ncmm_grass::turn({p.x-.5f,p.y-.5f},plan.turns);
            require(std::abs(ncmm_grass::edge_scale(plan.mask,p.x,p.y)-ncmm_grass::edge_scale(mask,q.x+.5f,q.y+.5f))<.00001f,"canonical rotated fringe has the same edge height on CPU and GPU");}}
    require(ncmm_grass::edge_scale(4,.01f,.5f)<.3f&&ncmm_grass::edge_scale(4,.5f,.5f)>.99f,"grass tapers at the terrain seam while keeping dense interiors");
    if(!prefix.empty())save(f.render({},-hybrid::pi*.5f,-.4f),prefix+"-terrain-edges");
    for(auto &c:f.cells)c.flags=0;edge=collect(f);require(edge.triangles.empty(),"unseen cells cannot leak material edges or object geometry");
    // Muzzle gas starts only after a confirmed combustion shot. Every particle
    // retains its world origin after the view turns or the player moves.
    weapon_fx::timeline fx;animation::pose pose;pose.grip=NCMM_GRIP_GUN;pose.seconds=10;pose.combustion=true;
    ncmm_graphics_control_state_v1 state{};auto profile=weapon::fallback(NCMM_GUN_RIFLE,0,.8f,"test");
    auto view=weapon_fx::make_camera({0,0,1.55f},0,0,1);fx.observe(state,pose,profile,view,"test",1);
    state.attack_serial=pose.attack_serial=1;state.attack_kind=2;state.attack_started=10;fx.observe(state,pose,profile,view,"test",1);
    require(fx.active(10.01)&&!fx.active(11),"confirmed shot has bounded lifetime");
    std::vector<uint32_t> flash(640*480),smoke(flash.size()),turned(flash.size());
    require(fx.paint(flash,640,480,view,10.01,1,{1,1,1})>100,"brief flash paints visible pixels");
    require(fx.paint(smoke,640,480,view,10.2,1,{1,1,1})>100,"gas plume outlives the flash");
    weapon_fx::timeline suppressed;auto quiet=profile;quiet.mods=NCMM_MOD_SUPPRESSOR;suppressed.observe({},pose,quiet,view,"quiet",1);suppressed.observe(state,pose,quiet,view,"quiet",1);
    std::vector<uint32_t> quiet_pixels(flash.size());const auto small=suppressed.paint(quiet_pixels,640,480,view,10.01,1,{1,1,1});
    require(small<std::count_if(flash.begin(),flash.end(),[](uint32_t p){return p!=0;})/3,"suppressor moves emission to its exit and sharply reduces flash area");
    if(!prefix.empty()){fixture preview;std::strcpy(preview.scene.environment.held_item,"modular_m27_assault_rifle");preview.appearance.view.gun_kind=NCMM_GUN_RIFLE;
        for(double age:{.01,.18,.50}){auto frame=preview.render(pose,0,0);fx.paint(frame.pixels,640,480,view,10+age,1,{1,1,1});save(frame,prefix+"-fx-"+std::to_string(int(age*100)));}}
    auto away=weapon_fx::make_camera({0,0,1.55f},hybrid::pi,0,1);require(!fx.paint(turned,640,480,away,10.2,1,{1,1,1}),"smoke stays in world space when camera turns away");
    fx.observe(state,pose,profile,view,"test",1);std::vector<uint32_t> repeated(flash.size());fx.paint(repeated,640,480,view,10.2,1,{1,1,1});require(repeated==smoke,"redraw does not duplicate a confirmed shot");
    state.attack_serial=pose.attack_serial=2;state.attack_started=10.3;pose.seconds=10.3;pose.combustion=false;fx.reset();fx.observe({},pose,profile,view,"test",1);fx.observe(state,pose,profile,view,"test",1);
    require(!fx.active(10.31),"energy/pneumatic ammunition never gains a combustion flash");
    pose.combustion=true;profile.kind=NCMM_GUN_BOW;state.attack_serial=pose.attack_serial=3;fx.observe(state,pose,profile,view,"test",1);require(!fx.active(10.31),"bow never emits gunpowder effects");
    fx.observe(state,pose,profile,view,"test",2);require(!fx.active(10.31),"changing worlds cannot replay a stale shot");
    {fixture pool;connected_pool(pool);const auto image=pool.render({},-hybrid::pi*.5f,-.45f);if(!prefix.empty())save(image,prefix+"-connected-puddles");
        fixture range;for(uint32_t kind=NCMM_PROJECTILE_BULLET;kind<=NCMM_PROJECTILE_THROWN;++kind){auto flight=projectile_fixture(range,kind);
            range.appearance.projectiles={flight};const auto image=range.render({},-hybrid::pi*.5f,0);if(!prefix.empty())save(image,prefix+"-projectile-"+std::to_string(kind));}
        range.object(range.sample(),"t_wall",true);range.cells[range.sample()].flags|=NCMM_GFX_WALL;
        range.details[range.sample()].terrain_shape=NCMM_SHAPE_WALL;
        const auto flight=projectile_fixture(range,NCMM_PROJECTILE_BULLET);range.appearance.projectiles.clear();const auto blocked=range.render({},-hybrid::pi*.5f,0);
        range.appearance.projectiles={flight};const auto hidden=range.render({},-hybrid::pi*.5f,0);
        if(hidden.pixels!=blocked.pixels){save(blocked,"projectile-wall-cpu-base");save(hidden,"projectile-wall-cpu-failed");}
        require(hidden.pixels==blocked.pixels,"CPU world depth occludes projectiles behind a wall");}
    std::cout<<"World visuals: "<<checks<<" checks PASS; no Native/input/cursor calls\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
