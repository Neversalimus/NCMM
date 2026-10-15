// Callback-scoped sparse XYZ snapshots, built only on CDDA's main thread.
ncmm_multilevel_metrics_v1 graphics_world_metrics = {};
uint64_t graphics_world_epoch=1,graphics_world_revision=1,graphics_world_serial=0;
std::array<uint64_t,257> graphics_world_level_revisions = {};
void graphics_world_changed_impl(const map *source,int z)
{
#if defined(TILES)
    if(!g||source!=&get_map())return;
    ++graphics_world_revision;
    if(z==INT32_MIN)for(auto &revision:graphics_world_level_revisions)++revision;
    else if(z>=-128&&z<=128)++graphics_world_level_revisions[size_t(z+128)];
#else
    (void)source;(void)z;
#endif
}
#if defined(TILES)
ncmm_world::source_cache graphics_world_sources;
const map *graphics_world_map_identity=nullptr; // comparison only
std::string graphics_world_realm;
uint64_t graphics_world_ray_epoch=1,graphics_world_ray_revision=0,graphics_world_roof_revision=0;
std::vector<uint32_t> graphics_world_roof_columns;
int32_t graphics_world_roof_x=0,graphics_world_roof_y=0,graphics_world_roof_low=0,graphics_world_roof_high=0;
ncmm_world::point graphics_world_ray_eye{};
std::optional<tripoint_abs_ms> graphics_world_previous_position,graphics_world_ray_position;
ncmm_graphics_camera::motion graphics_world_camera_motion;
bool graphics_world_transition_active=false,graphics_world_vertical_transition_active=false;
bool graphics_world_pending_present=false;
constexpr uint32_t graphics_world_architecture=1u<<28,graphics_world_renderable=1u<<29,graphics_world_vehicle_roof=1u<<30;
void graphics_clear_world()
{
    graphics_world_sources.clear();std::vector<uint32_t>().swap(graphics_world_roof_columns);graphics_world_roof_revision=0;graphics_world_map_identity=nullptr;graphics_world_realm.clear();
    graphics_world_previous_position.reset();graphics_world_ray_position.reset();graphics_world_camera_motion.reset();
    graphics_world_transition_active=graphics_world_vertical_transition_active=false;graphics_world_pending_present=false;graphics_multilevel_frame=nullptr;
    ++graphics_world_epoch;
}
void graphics_world_frame_presented()
{
    if(graphics_frame_pending_present&&graphics_view.enabled){
        const auto ticks=SDL_GetTicks();
        if(graphics_world_pending_present)graphics_world_camera_motion.presented(ticks);
        if(graphics_attack_pending){graphics_attack_started=ticks/1000.0;graphics_attack_pending=false;}
    }
    graphics_world_pending_present=graphics_frame_pending_present=false;
}
std::array<float,3> graphics_world_rgb(const tripoint_bub_ms &p){return graphics_sample_rgb(p);}
bool graphics_world_in_visual_range(const tripoint_bub_ms &p)
{
    map &here=get_map();avatar &you=get_avatar();if(!here.ncmm_graphics_loaded(p)||std::abs(p.z()-you.posz())>fov_3d_z_range)return false;
    const float raw=here.ambient_light_at(p);const int current=you.sight_range(raw),
        maximum=std::max(you.sight_range(default_daylight_level()),you.sight_range(0)),minimum=std::min(current,maximum);
    const int wanted=rl_dist(you.pos_bub(),p);const bool artificial=raw>here.get_cache_ref(p.z()).natural_light_level_cache;
    if(!(wanted<=minimum||(wanted<=maximum&&artificial)))return false;
    return wanted<=(you.has_effect(efftype_id("no_sight"))?1:artificial?MAX_VIEW_DISTANCE:minimum);
}
bool graphics_world_perception(const tripoint_bub_ms &p){return graphics_world_in_visual_range(p)&&get_avatar().sees(get_map(),p);}
bool graphics_draw_world(int x,int y,int width,int height)
{
    const auto begun=std::chrono::steady_clock::now();avatar &you=get_avatar();map &here=get_map();
    // Ordinary transient caches only. update_visibility_cache() writes discovery
    // and is not called to reveal additional levels for graphics.
    here.build_map_cache(you.posz());
    const auto absolute=you.pos_abs();const int radius=NCMM_GRAPHICS_V2_RADIUS,side=radius*2+1;
    if(!ncmm_world::source_window_fits(absolute.x(),absolute.y(),radius))return false;
    const int32_t ox=absolute.x()-radius,oy=absolute.y()-radius;
    const int low=here.supports_zlevels()?-OVERMAP_DEPTH:you.posz(),high=here.supports_zlevels()?OVERMAP_HEIGHT:you.posz();
    std::vector<int> loaded;
    for(int z=low;z<=high;++z)if(here.ncmm_graphics_loaded(tripoint_bub_ms(you.pos_bub().xy(),z)))loaded.push_back(z);
    if(loaded.empty()||loaded.size()>NCMM_WORLD_MAX_SLICES)return false;
    const std::string realm=(world_generator&&world_generator->active_world?world_generator->active_world->world_name:std::string{})+"/"+g->get_dimension_prefix().str();
    if(graphics_world_map_identity!=&here||graphics_world_realm!=realm){
        graphics_clear_world();graphics_clear_creatures();graphics_world_map_identity=&here;graphics_world_realm=realm;}
    if(!graphics_world_sources.prepare(ox,oy,loaded.front(),loaded.back(),side,side,graphics_world_epoch))return false;
    graphics_world_metrics={};graphics_world_metrics.struct_size=sizeof(graphics_world_metrics);
    graphics_world_metrics.available_slices=uint32_t(loaded.size());
    auto base_flags=[&](int ax,int ay,int z){
        if(z<loaded.front()||z>loaded.back())return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);
        return graphics_world_sources.get(ax,ay,z,graphics_world_level_revisions[size_t(z+128)]+1,[&]{
            const auto p=here.get_bub(tripoint_abs_ms(ax,ay,z));if(!here.ncmm_graphics_loaded(p))return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);
            const auto &ter=here.ter(p).obj();const auto &furn=here.furn(p).obj();ncmm_world::traits t{};t.loaded=true;
            t.no_floor=here.has_flag(ter_furn_flag::TFLAG_NO_FLOOR,p);t.no_floor_water=here.has_flag(ter_furn_flag::TFLAG_NO_FLOOR_WATER,p);
            if(!here.supports_zlevels()||z<-OVERMAP_DEPTH+1)t.no_floor=t.no_floor_water=false;
            t.transparent_floor=ter.has_flag(ter_furn_flag::TFLAG_TRANSPARENT_FLOOR);t.z_transparent=ter.has_flag(ter_furn_flag::TFLAG_Z_TRANSPARENT);
            t.goes_up=here.has_flag(ter_furn_flag::TFLAG_GOES_UP,p);t.goes_down=here.has_flag(ter_furn_flag::TFLAG_GOES_DOWN,p);
            t.ramp=here.has_flag(ter_furn_flag::TFLAG_RAMP,p);t.ramp_up=here.has_flag(ter_furn_flag::TFLAG_RAMP_UP,p);
            t.ramp_down=here.has_flag(ter_furn_flag::TFLAG_RAMP_DOWN,p);t.ramp_end=here.has_flag(ter_furn_flag::TFLAG_RAMP_END,p);
            t.difficult_z=here.has_flag(ter_furn_flag::TFLAG_DIFFICULT_Z,p);t.canopy=furn.has_flag(ter_furn_flag::TFLAG_SUN_ROOF_ABOVE);
            t.opaque=!(ter.transparent&&furn.transparent);const bool solid_architecture=t.opaque;bool opaque_field=false;
            for(const auto &field:here.field_at(p)){const auto &level=field.second.get_intensity_level();
                if(!level.transparent&&level.translucency<=LIGHT_TRANSPARENCY_SOLID)t.opaque=opaque_field=true;}
            uint32_t value=ncmm_world::semantics(t);
            if(opaque_field&&!solid_architecture)value|=ncmm_world::opaque_field_fact;
            const auto shape=ncmm_graphics_shapes::terrain(ter.id.str()+" "+ter.looks_like,graphics_traits(ter,!ter.close.is_null()));
            if(ncmm_graphics_shapes::structural_surface(shape))value|=graphics_world_architecture;
            if(ncmm_graphics_shapes::vertical(shape)||here.has_furn(p)||here.field_at(p).field_count())value|=graphics_world_renderable;
            if(const auto vp=here.veh_at(p)){value|=graphics_world_renderable;if(vp->part_with_feature("ROOF",false))value|=graphics_world_vehicle_roof;}
            return value;
        });
    };
    // Private absolute-column roof masks reuse the already loaded facts. A
    // rotating camera does not rescan every upper floor for every visible cell.
    if(graphics_world_roof_revision!=graphics_world_revision||graphics_world_roof_x!=ox||graphics_world_roof_y!=oy||graphics_world_roof_low!=loaded.front()||graphics_world_roof_high!=loaded.back()||graphics_world_roof_columns.size()!=size_t(side*side)){
        graphics_world_roof_columns.assign(size_t(side*side),0);
        for(int y=0;y<side;++y)for(int x=0;x<side;++x){bool roof=false;auto &mask=graphics_world_roof_columns[size_t(y*side+x)];
            for(int z=loaded.back();z>=loaded.front();--z){const uint32_t raw=base_flags(ox+x,oy+y,z);
                if(roof||(raw&(NCMM_WORLD_CANOPY|graphics_world_vehicle_roof)))mask|=uint32_t(1)<<(z-loaded.front());
                if(raw&(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_CANOPY|NCMM_WORLD_UNKNOWN|graphics_world_vehicle_roof))roof=true;}}
        graphics_world_roof_revision=graphics_world_revision;graphics_world_roof_x=ox;graphics_world_roof_y=oy;graphics_world_roof_low=loaded.front();graphics_world_roof_high=loaded.back();
    }
    auto flags=[&](int ax,int ay,int z){
        uint32_t value=base_flags(ax,ay,z);
        const uint32_t below=z>loaded.front()?base_flags(ax,ay,z-1):0,above=z<loaded.back()?base_flags(ax,ay,z+1):0;
        if(below&NCMM_WORLD_CANOPY)value=(value|NCMM_WORLD_FLOOR)&~(NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_GLASS_FLOOR);
        if((above&NCMM_WORLD_DOWN)&&(above&NCMM_WORLD_RAMP)&&!(value&NCMM_WORLD_UNKNOWN))value|=NCMM_WORLD_UP|NCMM_WORLD_RAMP;
        if((value&NCMM_WORLD_RAMP)&&(value&NCMM_WORLD_UP)&&(value&NCMM_WORLD_OPAQUE))value|=NCMM_WORLD_RAMP_END;
        if((value&NCMM_WORLD_GLASS_FLOOR)&&(below&NCMM_WORLD_UP)&&(below&NCMM_WORLD_RAMP))value&=~NCMM_WORLD_GLASS_FLOOR;
        if((value&NCMM_WORLD_UP)&&((above&NCMM_WORLD_OPEN_DOWN)||((above&NCMM_WORLD_DOWN)&&(above&NCMM_WORLD_RAMP))))value|=NCMM_WORLD_CONNECTED_UP;
        if((value&NCMM_WORLD_DOWN)&&(below&NCMM_WORLD_UP))value|=NCMM_WORLD_CONNECTED_DOWN;
        const int64_t x=int64_t(ax)-ox,y=int64_t(ay)-oy;
        const bool roof=x<0||y<0||x>=side||y>=side||z<loaded.front()||z>loaded.back()||
            (graphics_world_roof_columns[size_t(y*side+x)]&(uint32_t(1)<<(z-loaded.front())));
        if(roof)value|=NCMM_WORLD_ROOFED;return value;
    };
    const uint64_t ticks=SDL_GetTicks();
    auto continuing=graphics_world_camera_motion.sample(ticks);
    if(graphics_world_previous_position&&*graphics_world_previous_position!=absolute){
        const auto previous=*graphics_world_previous_position;const int dz=absolute.z()-previous.z();
        const int64_t dx=int64_t(absolute.x())-previous.x(),dy=int64_t(absolute.y())-previous.y();
        const bool same_xy=absolute.xy()==previous.xy(),adjacent=std::abs(int64_t(absolute.x())-previous.x())+std::abs(int64_t(absolute.y())-previous.y())<=1;
        const bool can_interpolate=adjacent&&std::abs(dz)==1;
        const uint32_t from=flags(previous.x(),previous.y(),previous.z()),to=flags(absolute.x(),absolute.y(),absolute.z());
        const bool passage=dz>0?((from&NCMM_WORLD_CONNECTED_UP)&&((same_xy&&(to&NCMM_WORLD_OPEN_DOWN))||((from&NCMM_WORLD_RAMP)&&(to&(NCMM_WORLD_RAMP|NCMM_WORLD_FLOOR))))):((to&NCMM_WORLD_CONNECTED_UP)&&((same_xy&&(from&NCMM_WORLD_OPEN_DOWN))||((to&NCMM_WORLD_RAMP)&&(from&(NCMM_WORLD_RAMP|NCMM_WORLD_FLOOR)))));
        const bool walk=dz==0&&std::abs(dx)<=1&&std::abs(dy)<=1&&std::abs(continuing.z)<.002f&&!you.in_vehicle;
        if((can_interpolate&&passage)||walk)
            graphics_world_camera_motion.begin({continuing.x-float(dx),continuing.y-float(dy),continuing.z-dz*NCMM_WORLD_LEVEL_HEIGHT},ticks);
        else graphics_world_camera_motion.reset();
    }
    const bool position_changed=!graphics_world_previous_position||*graphics_world_previous_position!=absolute;
    graphics_world_previous_position=absolute;
    const uint32_t ceiling=flags(absolute.x(),absolute.y(),absolute.z()+1);
    const float raw_eye=graphics_eye_height(),safe_eye=(ceiling&(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_UNKNOWN))?std::min(raw_eye,NCMM_WORLD_LEVEL_HEIGHT-.18f):raw_eye;
    auto animated=graphics_world_camera_motion.sample(ticks);
    auto query=[&](ncmm_world::voxel p){return flags(ox+p.x,oy+p.y,absolute.z()+p.z);};
    auto motion_query=[&](ncmm_world::voxel p){
        uint32_t value=ncmm_world::structural_flags(query(p));
        const auto tile=here.get_bub(tripoint_abs_ms(ox+p.x,oy+p.y,absolute.z()+p.z));
        if(!here.ncmm_graphics_loaded(tile)||here.veh_at(tile))value|=NCMM_WORLD_UNKNOWN|NCMM_WORLD_OPAQUE;
        return value;
    };
    const ncmm_world::point destination{radius+.5f,radius+.5f,safe_eye},start_eye{destination.x+animated.x,destination.y+animated.y,destination.z+animated.z};
    if(graphics_world_camera_motion.active(ticks)){
        const ncmm_world::voxel from{int(std::floor(start_eye.x)),int(std::floor(start_eye.y)),int(std::floor(start_eye.z/NCMM_WORLD_LEVEL_HEIGHT))};
        if((motion_query(from)&(NCMM_WORLD_UNKNOWN|NCMM_WORLD_OPAQUE))||!ncmm_world::visible(start_eye,destination,motion_query)){
            graphics_world_camera_motion.reset();animated={};
        }
    }
    // Headroom correction and accumulated easing share the existing ABI bound.
    // A rapid tall-avatar stair sequence must cancel its visual lag rather than
    // publish an invalid scene and disable the renderer.
    if(std::abs(animated.z+safe_eye-raw_eye)>NCMM_WORLD_LEVEL_HEIGHT*2){graphics_world_camera_motion.reset();animated={};}
    const float camera_dz=animated.z+safe_eye-raw_eye;
    graphics_world_transition_active=graphics_world_camera_motion.active(ticks);graphics_world_vertical_transition_active=std::abs(animated.z)>.002f;
    const ncmm_world::point eye{radius+.5f+animated.x,radius+.5f+animated.y,safe_eye+animated.z};
    if(position_changed||graphics_world_ray_revision!=graphics_world_sources.topology_revision||
       std::abs(eye.x-graphics_world_ray_eye.x)>.0001f||std::abs(eye.y-graphics_world_ray_eye.y)>.0001f||std::abs(eye.z-graphics_world_ray_eye.z)>.0001f){
        ++graphics_world_ray_epoch;graphics_world_ray_revision=graphics_world_sources.topology_revision;graphics_world_ray_eye=eye;}
    auto structural_query=[&](ncmm_world::voxel p){return ncmm_world::structural_flags(query(p));};
    struct candidate {ncmm_world_cell_v1 world{};tripoint_bub_ms pos;uint32_t permission=0;float distance=0;};
    std::vector<candidate> candidates;
    const float cy=std::cos(graphics_camera_yaw),sy=std::sin(graphics_camera_yaw),cp=std::cos(graphics_camera_pitch),sp=std::sin(graphics_camera_pitch),
        horizontal=std::tan(70.f*3.14159265f/360.f),vertical=horizontal*height/width;
    const std::array<std::array<float,2>,5> probes={{{.5f,.5f},{.12f,.12f},{.88f,.12f},{.12f,.88f},{.88f,.88f}}};
    const auto &weather=get_weather().weather_id.obj();const bool atmosphere=weather.precip==precip_class::heavy||weather.precip==precip_class::light||weather.precip==precip_class::very_light||weather.sight_penalty>1||weather.sound_category==weather_sound_category::portal_storm;
    std::vector<std::array<int32_t,3>> observed_deaths;
    const double presentation_now=graphics_presentation_seconds();
    for(const auto &entry:graphics_creature_records){const auto &record=entry.second;const auto &s=record.snapshot;
        if(first_person::creature::death_clip_pending(s,record.presented_event_serial==s.event_serial,presentation_now))
            observed_deaths.push_back({s.world_x,s.world_y,s.world_z});}
    std::sort(observed_deaths.begin(),observed_deaths.end());
    const auto visibility_started=std::chrono::steady_clock::now();
    for(int z:loaded)for(int gy=0;gy<side;++gy)for(int gx=0;gx<side;++gx){
        ++graphics_world_metrics.processed_cells;
        const float dx=gx+.5f-eye.x,dy=gy+.5f-eye.y,dz=(z-absolute.z())*NCMM_WORLD_LEVEL_HEIGHT+1.25f-eye.z,
            forward=(dx*cy+dy*sy)*cp+dz*sp,screen_x=-dx*sy+dy*cy,screen_y=dz*cp-(dx*cy+dy*sy)*sp;
        const bool close_to_eye=dx*dx+dy*dy+dz*dz<16;
        const float horizontal_limit=std::max(0.f,forward)*horizontal,vertical_limit=std::max(0.f,forward)*vertical;
        const auto outside=[&](float margin){return forward < -margin||std::abs(screen_x)>horizontal_limit+margin||std::abs(screen_y)>vertical_limit+margin;};
        // Reject only beyond the widest existing creature margin before doing
        // tracker/death lookups. Large and airborne actors retain the same bounds.
        if(!close_to_eye&&outside(20.f)){++graphics_world_metrics.frustum_rejected;continue;}
        // The widest conservative bound is independent of map contents. Avoid
        // bubble conversion/loading probes for cells that cannot contribute.
        const int ax=ox+gx,ay=oy+gy;const auto p=here.get_bub(tripoint_abs_ms(ax,ay,z));
        if(!here.ncmm_graphics_loaded(p))continue;
        const Creature *entity=get_creature_tracker().creature_at<Creature>(p);
        const bool observed_death=std::binary_search(observed_deaths.begin(),observed_deaths.end(),std::array<int32_t,3>{ax,ay,z});
        const float margin=entity||observed_death?20.f:5.f;
        if(!close_to_eye&&outside(margin)){
            ++graphics_world_metrics.frustum_rejected;continue;}
        uint32_t semantic=flags(ax,ay,z);
        const bool air=!(semantic&(NCMM_WORLD_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_DOWN|NCMM_WORLD_UP|NCMM_WORLD_CANOPY|graphics_world_renderable));
        if(air&&!entity&&!observed_death&&!atmosphere&&p!=you.pos_bub()&&here.i_at(p).empty())continue;
        const uint8_t seen=graphics_world_sources.visibility(ax,ay,z,graphics_world_ray_epoch,[&]{
            uint8_t result=0;const float level=(z-absolute.z())*NCMM_WORLD_LEVEL_HEIGHT;
            for(const auto &offset:probes){const float xx=gx+offset[0],yy=gy+offset[1];
                if(!(result&1)&&ncmm_world::visible(eye,{xx,yy,level+1.25f},query))result|=1;
                if(!(result&2)&&ncmm_world::visible(eye,{xx,yy,level+.003f},query,true))result|=2;
                if((semantic&NCMM_WORLD_FLOOR)&&!(result&8)&&((result&2)||ncmm_world::visible(eye,{xx,yy,level+.003f},structural_query,true)))result|=8;
                if(!(result&4)&&ncmm_world::visible(eye,{xx,yy,level-.083f},query,true))result|=4;}
            if((semantic&NCMM_WORLD_FLOOR)&&!(result&16))for(const auto &offset:probes)
                if(ncmm_world::visible(eye,{gx+offset[0],gy+offset[1],level-.083f},structural_query,true)){result|=16;break;}
            return result;
        });
        const auto below=tripoint_bub_ms(p.xy(),z-1);uint32_t permission=0;
        if((seen&1)&&graphics_world_perception(p))permission|=NCMM_WORLD_BODY_VISIBLE;
        // Native tile FOV can reject an upper wall because its tile has a
        // floor. A physical ray can still see its exterior face from below.
        // This permission carries only coarse architecture, never contents.
        if((seen&1)&&(semantic&graphics_world_architecture)&&graphics_world_in_visual_range(p))permission|=NCMM_WORLD_STRUCTURE_VISIBLE;
        if((seen&2)&&graphics_world_perception(p))permission|=NCMM_WORLD_FLOOR_TOP;
        // Unseen contents remain absent. Only a neutral, structurally verified
        // support surface continues behind gas instead of becoming a sky hole.
        if(!(permission&NCMM_WORLD_FLOOR_TOP)&&(seen&8)&&(semantic&NCMM_WORLD_FLOOR)&&graphics_world_in_visual_range(p))permission|=NCMM_WORLD_FLOOR_TOP;
        if((seen&4)&&graphics_world_perception(below))permission|=NCMM_WORLD_FLOOR_BOTTOM;
        if(!(permission&NCMM_WORLD_FLOOR_BOTTOM)&&(seen&16)&&(semantic&NCMM_WORLD_FLOOR)&&graphics_world_in_visual_range(below))permission|=NCMM_WORLD_FLOOR_BOTTOM;
        if(p==you.pos_bub())permission|=NCMM_WORLD_BODY_VISIBLE;
        if(!permission){++graphics_world_metrics.occlusion_rejected;continue;}
        semantic&=262143u;
        if(!(permission&NCMM_WORLD_BODY_VISIBLE)){
            const bool landing=(semantic&NCMM_WORLD_DOWN)&&!(semantic&(NCMM_WORLD_LADDER|NCMM_WORLD_RAMP));
            semantic&=NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_WATER|NCMM_WORLD_UNKNOWN|
                ((permission&NCMM_WORLD_STRUCTURE_VISIBLE)?uint32_t(NCMM_WORLD_OPAQUE):0u);
            if(landing)semantic|=NCMM_WORLD_DOWN;
            semantic|=NCMM_WORLD_ROOFED; // No hidden room roof/navigation facts.
        }
        candidate c{};c.world={ax,ay,z,semantic|permission,0,0,graphics_world_level_revisions[size_t(z+128)]+1,{0,0,0}};
        c.pos=p;c.permission=permission;c.distance=dx*dx+dy*dy+dz*dz;
        if(permission&NCMM_WORLD_FLOOR_BOTTOM){const auto rgb=graphics_world_rgb(below);std::copy(rgb.begin(),rgb.end(),c.world.lower_light);}
        if((permission&NCMM_WORLD_BODY_VISIBLE)&&(semantic&(NCMM_WORLD_UP|NCMM_WORLD_RAMP|NCMM_WORLD_LADDER)))for(uint32_t direction=0;direction<4;++direction){const int nx=direction==0?1:direction==2?-1:0,ny=direction==1?1:direction==3?-1:0;
            const uint32_t next=flags(ax+nx,ay+ny,z);
            if((semantic&NCMM_WORLD_RAMP)&&(next&NCMM_WORLD_RAMP_END)){c.world.direction=direction;break;}
            if(!(semantic&NCMM_WORLD_RAMP)&&!(next&(NCMM_WORLD_UNKNOWN|NCMM_WORLD_OPAQUE))){c.world.direction=direction;break;}}
        candidates.push_back(c);
    }
    graphics_world_metrics.visibility_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-visibility_started).count();
    if(candidates.size()>NCMM_WORLD_MAX_CELLS){std::stable_sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.distance<b.distance;});
        graphics_world_metrics.budget_rejected=uint32_t(candidates.size()-NCMM_WORLD_MAX_CELLS);candidates.resize(NCMM_WORLD_MAX_CELLS);}
    std::sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return ncmm_world::order(a.world)<ncmm_world::order(b.world);});
    std::vector<ncmm_graphics_cell_v1> cells(candidates.size());std::vector<tripoint_bub_ms> positions;
    std::vector<ncmm_world_cell_v1> coordinates;std::vector<ncmm_world_slice_v1> slices;
    positions.reserve(cells.size());coordinates.reserve(cells.size());graphics_ceiling_cells=graphics_snapshot_far_cells=graphics_snapshot_light_queries=0;
    auto copy=[](char (&out)[128],const std::string &id){if(id.size()<sizeof(out))std::copy(id.begin(),id.end(),out);};
    for(size_t i=0;i<candidates.size();++i){const auto &source=candidates[i];const auto &p=source.pos;auto &c=cells[i];
        positions.push_back(p);coordinates.push_back(source.world);c.flags=NCMM_GFX_VISIBLE;c.light=1;
        if(!(source.world.flags&NCMM_WORLD_FLOOR))c.flags|=NCMM_GFX_NO_FLOOR;
        if(source.world.flags&NCMM_WORLD_ROOFED)c.flags|=NCMM_GFX_CEILING;
        if(source.world.flags&NCMM_WORLD_FLOOR_BOTTOM)++graphics_ceiling_cells;
        if(std::max(std::abs(p.x()-you.pos_bub().x()),std::abs(p.y()-you.pos_bub().y()))>NCMM_GRAPHICS_RADIUS)++graphics_snapshot_far_cells;
        if(source.permission&NCMM_WORLD_BODY_VISIBLE)copy(c.terrain,here.ter(p).id().str());
        else if(source.permission&NCMM_WORLD_STRUCTURE_VISIBLE){
            const auto &ter=here.ter(p).obj();const auto shape=ncmm_graphics_shapes::terrain(ter.id.str()+" "+ter.looks_like,graphics_traits(ter,!ter.close.is_null()));
            copy(c.terrain,shape==NCMM_SHAPE_WINDOW||shape==NCMM_SHAPE_WINDOW_BROKEN||shape==NCMM_SHAPE_WINDOW_COVERED||shape==NCMM_SHAPE_GLASS_WALL?"t_window":"t_wall");
            if(source.world.flags&NCMM_WORLD_OPAQUE)c.flags|=NCMM_GFX_WALL;
        }
        else{const auto kind=ncmm_graphics_presentation_policy::material(here.ter(p).id().str());
            copy(c.terrain,kind==NCMM_MAT_WOOD?"t_floor_wood":kind==NCMM_MAT_METAL?"t_floor_metal":"t_floor");}
        if(!(source.permission&NCMM_WORLD_BODY_VISIBLE))continue;
        if(source.world.flags&NCMM_WORLD_OPAQUE)c.flags|=NCMM_GFX_WALL;
        if(here.has_furn(p))copy(c.furniture,here.furn(p).id().str());
        if(const auto vp=here.veh_at(p))if(const auto displayed=vp->part_displayed()){
            c.flags|=NCMM_GFX_VEHICLE;std::fill(std::begin(c.furniture),std::end(c.furniture),'\0');copy(c.furniture,vpart_display(displayed->part()).get_tileset_id());}
        if(here.sees_some_items(p,you)&&you.sees(here,p)){const auto items=here.i_at(p);if(!items.empty())copy(c.item,items.begin()->typeId().str());}
        if(const auto *cr=get_creature_tracker().creature_at<Creature>(p))if(!cr->is_avatar()&&!cr->is_dead_state()&&you.sees(here,*cr)){
            if(const auto *mon=cr->as_monster())copy(c.creature,mon->type->id.str());
            else if(const auto *ch=cr->as_character())if(cr->is_npc())copy(c.creature,ch->male?"npc_male":"npc_female");}
    }
    for(int z:loaded){const uint32_t first=uint32_t(std::lower_bound(coordinates.begin(),coordinates.end(),z,[](const auto &c,int value){return c.z<value;})-coordinates.begin()),
        last=uint32_t(std::upper_bound(coordinates.begin(),coordinates.end(),z,[](int value,const auto &c){return value<c.z;})-coordinates.begin());
        slices.push_back({z,first,last-first,0,(graphics_world_epoch<<8)^uint64_t(z+129),graphics_world_level_revisions[size_t(z+128)]+1});
        if(last>first)++graphics_world_metrics.visible_slices;
    }
    ncmm_graphics_multilevel_scene_v1 world{};world.struct_size=sizeof(world);world.version=1;world.world_epoch=graphics_world_epoch;
    world.revision=graphics_world_revision;world.frame_serial=++graphics_world_serial;world.origin_x=ox;world.origin_y=oy;world.origin_z=absolute.z();
    world.loaded_min_z=loaded.front();world.loaded_max_z=loaded.back();world.level_height=NCMM_WORLD_LEVEL_HEIGHT;world.camera_dx=animated.x;world.camera_dy=animated.y;world.camera_dz=camera_dz;
    world.coordinates=coordinates.data();world.cell_count=uint32_t(coordinates.size());world.slices=slices.data();world.slice_count=uint32_t(slices.size());
    const ncmm_graphics_scene_v1 base={sizeof(ncmm_graphics_scene_v1),width,height,side,side,radius+.5f,radius+.5f,cells.data(),cells.size()};
    graphics_viewport={x,y,width,height};graphics_snapshot_radius=radius;graphics_snapshot_begun=begun;
    graphics_world_metrics.submitted_cells=uint32_t(cells.size());graphics_world_metrics.source_cache_bytes=graphics_world_sources.bytes()+graphics_world_roof_columns.capacity()*sizeof(uint32_t);
    graphics_world_metrics.source_cache_hits=graphics_world_sources.hits;graphics_world_metrics.source_cache_misses=graphics_world_sources.misses;
    graphics_world_metrics.snapshot_bytes=cells.capacity()*sizeof(cells[0])+positions.capacity()*sizeof(positions[0])+coordinates.capacity()*sizeof(coordinates[0])+slices.capacity()*sizeof(slices[0]);
    graphics_world_metrics.extraction_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();
    const bool presented=graphics_draw_hybrid(base,positions,x,y,&world);
    graphics_world_pending_present=presented;
    graphics_world_metrics.transition_ms=graphics_world_transition_active?double(graphics_world_camera_motion.elapsed(ticks)):0;
    return presented;
}
bool graphics_world_target_visible(const tripoint_bub_ms &origin,const tripoint_bub_ms &target)
{
    if(graphics_world_vertical_transition_active||!get_map().ncmm_graphics_loaded(target))return false;
    get_map().build_map_cache(get_avatar().posz());
    // Native visibility and the normal targeting UI still own reach/trajectory.
    // This guard never turns an unsupported vertical goal into an XY attack.
    return get_map().sees(origin,target,MAX_VIEW_DISTANCE)&&graphics_world_perception(target);
}
#else
void graphics_clear_world(){graphics_multilevel_frame=nullptr;}
#endif
bool graphics_world_needs_redraw(){
#if defined(TILES)
    return graphics_world_transition_active;
#else
    return false;
#endif
}
int graphics_register_world(const char *id,ncmm_graphics_render_multilevel_v1 render,ncmm_graphics_input_v1 input,void *data)
{
    if(graphics_render_active||!active_module_matches(id)||!module_ids.count(id)||!render||(!graphics_view.owner.empty()&&graphics_view.owner!=id))return 0;
    graphics_clear_world();graphics_view={id,nullptr,input,data,false,nullptr,render};return 1;
}
void graphics_world_extracted(double total){
    graphics_world_metrics.extraction_ms=std::max(0.0,total-graphics_world_metrics.visibility_ms);
    graphics_world_metrics.snapshot_bytes+=graphics_presentation_cells.capacity()*sizeof(graphics_presentation_cells[0])+graphics_creature_snapshots.capacity()*sizeof(graphics_creature_snapshots[0]);
}
int graphics_get_world_metrics(ncmm_multilevel_metrics_v1 *out)
{
    if(!graphics_multilevel_frame||active_module_id!=graphics_view.owner||!out||out->struct_size<sizeof(*out))return 0;
    *out=graphics_world_metrics;return 1;
}
int graphics_gpu_submit_world(const ncmm_gpu_multilevel_scene_v1 *scene);
const ncmm_host_graphics_multilevel_v1 graphics_world_api={1,sizeof(ncmm_host_graphics_multilevel_v1),1,0,
    graphics_register_world,graphics_enable,graphics_enabled,graphics_get_sprite,graphics_submit_frame,graphics_gpu_submit_world,graphics_get_world_metrics};

