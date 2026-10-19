// Callback-scoped sparse XYZ snapshots, built only on CDDA's main thread.
ncmm_multilevel_metrics_v1 graphics_world_metrics = {};
uint64_t graphics_world_range_evaluations=0,graphics_world_range_hits=0;
uint64_t graphics_world_material_evaluations=0,graphics_world_material_hits=0;
uint64_t graphics_world_epoch=1,graphics_world_revision=1,graphics_world_serial=0;
std::array<uint64_t,257> graphics_world_level_revisions = {};
void graphics_world_changed_impl(const map *source,int z)
{
#if defined(TILES)
    if(!g||source!=&get_map())return;
    graphics_snapshot_lease.invalidate();
    ++graphics_world_revision;
    if(z==INT32_MIN)for(auto &revision:graphics_world_level_revisions)++revision;
    else if(z>=-128&&z<=128)++graphics_world_level_revisions[size_t(z+128)];
#else
    (void)source;(void)z;
#endif
}
#if defined(TILES)
#include "ncmm_graphics_lod.inc"
ncmm_world::source_cache graphics_world_sources;
const map *graphics_world_map_identity=nullptr; // comparison only
std::string graphics_world_realm;
uint64_t graphics_world_ray_epoch=1,graphics_world_ray_revision=0;
ncmm_world::geometry_cache graphics_world_roofs,graphics_world_derived;
ncmm_world::sealed_plane_cache graphics_world_sealed;
int graphics_world_cached_low=0,graphics_world_cached_high=-1;
ncmm_world::point graphics_world_ray_eye{};
std::optional<tripoint_abs_ms> graphics_world_previous_position,graphics_world_ray_position;
ncmm_graphics_camera::motion graphics_world_camera_motion;
bool graphics_world_transition_active=false,graphics_world_vertical_transition_active=false;
bool graphics_world_pending_present=false;
constexpr uint32_t graphics_world_architecture=1u<<28,graphics_world_renderable=1u<<29,graphics_world_vehicle_roof=ncmm_world::vehicle_roof_fact;
void graphics_clear_world()
{
    graphics_snapshot_lease.invalidate();graphics_camera_cache.clear();
    graphics_world_sources.clear();graphics_world_roofs.clear();graphics_world_derived.clear();graphics_world_sealed.clear();graphics_world_map_identity=nullptr;graphics_world_realm.clear();
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
bool graphics_world_in_visual_range(const tripoint_bub_ms &p,int maximum,bool no_sight,ncmm_world::light_range_cache *ranges=nullptr,const tripoint_bub_ms *surface_light=nullptr)
{
    map &here=get_map();avatar &you=get_avatar();if(!here.ncmm_graphics_loaded(p)||std::abs(p.z()-you.posz())>fov_3d_z_range)return false;
    const auto &lit=surface_light?*surface_light:p;if(!here.ncmm_graphics_loaded(lit))return false;
    const float raw=here.ambient_light_at(lit);const int current=ranges?ranges->get(raw,[&]{return you.sight_range(raw);}):you.sight_range(raw),minimum=std::min(current,maximum);
    const int wanted=rl_dist(you.pos_bub(),p);const bool artificial=raw>here.get_cache_ref(p.z()).natural_light_level_cache;
    if(!(wanted<=minimum||(wanted<=maximum&&artificial)))return false;
    return wanted<=(no_sight?1:artificial?MAX_VIEW_DISTANCE:minimum);
}
bool graphics_world_in_visual_range(const tripoint_bub_ms &p){const auto &you=get_avatar();
    return graphics_world_in_visual_range(p,std::max(you.sight_range(default_daylight_level()),you.sight_range(0)),you.has_effect(efftype_id("no_sight")));}
bool graphics_world_perception(const tripoint_bub_ms &p){return graphics_world_in_visual_range(p)&&get_avatar().sees(get_map(),p);}
bool graphics_draw_world(int x,int y,int width,int height)
{
    ncmm_graphics_trace::cpu_zone snapshot_zone("Snapshot");
    graphics_camera_cache.suspend();
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
    // Roof bits and neighbour semantics depend on the loaded Z window.
    if(graphics_world_cached_low!=loaded.front()||graphics_world_cached_high!=loaded.back()){
        graphics_world_roofs.clear();graphics_world_derived.clear();
        graphics_world_cached_low=loaded.front();graphics_world_cached_high=loaded.back();
    }
    if(!graphics_world_sources.prepare(ox,oy,loaded.front(),loaded.back(),side,side,graphics_world_epoch)||
       !graphics_world_derived.prepare(ox,oy,loaded.front(),loaded.back(),side,side,graphics_world_epoch)||
       !graphics_world_roofs.prepare(ox,oy,0,0,side,side,graphics_world_epoch)||
       !graphics_world_sealed.prepare(ox,oy,loaded.front(),loaded.back(),side,side,graphics_world_epoch,graphics_world_revision))return false;
    graphics_world_metrics={};graphics_world_metrics.struct_size=sizeof(graphics_world_metrics);
    graphics_world_metrics.available_slices=uint32_t(loaded.size());
    ncmm_world::material_semantics_cache material_definitions,elevated_material_definitions;
    auto base_flags=[&](int ax,int ay,int z){
        if(z<loaded.front()||z>loaded.back())return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);
        return graphics_world_sources.get(ax,ay,z,graphics_world_level_revisions[size_t(z+128)]+1,[&]{
            const auto p=here.get_bub(tripoint_abs_ms(ax,ay,z));if(!here.ncmm_graphics_loaded(p))return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE);
            const auto &ter=here.ter(p).obj();const auto &furn=here.furn(p).obj();
            const bool normal_floor=here.supports_zlevels()&&z>=-OVERMAP_DEPTH+1;
            // A tower keeps its native foundation floor, while upper lattice
            // cells are open. Keep those two material contexts in separate caches.
            auto &materials=z>0?elevated_material_definitions:material_definitions;
            uint32_t value=materials.get(reinterpret_cast<std::uintptr_t>(&ter),reinterpret_cast<std::uintptr_t>(&furn),normal_floor,[&]{
                // On both pinned engines map::has_flag(ter_furn_flag,p) is
                // exactly this OR after the loaded/in-bounds check above.
                auto has=[&](auto flag){return ter.has_flag(flag)||furn.has_flag(flag);};
                ncmm_world::traits t{};t.loaded=true;
                t.no_floor=has(ter_furn_flag::TFLAG_NO_FLOOR);t.no_floor_water=has(ter_furn_flag::TFLAG_NO_FLOOR_WATER);
                if(!normal_floor)t.no_floor=t.no_floor_water=false;
                t.transparent_floor=ter.has_flag(ter_furn_flag::TFLAG_TRANSPARENT_FLOOR);t.z_transparent=ter.has_flag(ter_furn_flag::TFLAG_Z_TRANSPARENT);
                t.goes_up=has(ter_furn_flag::TFLAG_GOES_UP);t.goes_down=has(ter_furn_flag::TFLAG_GOES_DOWN);
                t.ramp=has(ter_furn_flag::TFLAG_RAMP);t.ramp_up=has(ter_furn_flag::TFLAG_RAMP_UP);
                t.ramp_down=has(ter_furn_flag::TFLAG_RAMP_DOWN);t.ramp_end=has(ter_furn_flag::TFLAG_RAMP_END);
                t.difficult_z=has(ter_furn_flag::TFLAG_DIFFICULT_Z);t.canopy=furn.has_flag(ter_furn_flag::TFLAG_SUN_ROOF_ABOVE);
                t.opaque=!(ter.transparent&&furn.transparent);
                const auto shape=ncmm_graphics_shapes::terrain(ter.id.str()+" "+ter.looks_like,graphics_traits(ter,!ter.close.is_null()));
                t.partial_support=ncmm_graphics_shapes::partial_support(shape,z);
                uint32_t flags=ncmm_world::semantics(t);
                if(ncmm_graphics_shapes::structural_surface(shape))flags|=graphics_world_architecture;
                if(ncmm_graphics_shapes::vertical(shape))flags|=graphics_world_renderable;
                return flags;
            });
            const bool solid_architecture=(value&NCMM_WORLD_OPAQUE)!=0;bool opaque_field=false;
            const auto &fields=here.field_at(p);
            for(const auto &field:fields){const auto &level=field.second.get_intensity_level();
                if(!level.transparent&&level.translucency<=LIGHT_TRANSPARENCY_SOLID)opaque_field=true;}
            if(opaque_field){value|=NCMM_WORLD_OPAQUE;if(!solid_architecture)value|=ncmm_world::opaque_field_fact;}
            if(here.has_furn(p)||fields.field_count())value|=graphics_world_renderable;
            if(const auto vp=here.veh_at(p)){value|=graphics_world_renderable;if(vp->part_with_feature("ROOF",false))value|=graphics_world_vehicle_roof;}
            return value;
        });
    };
    // Lazy absolute-coordinate facts survive ordinary viewport shifts. Native
    // geometry notifications invalidate derived neighbours and roof columns.
    auto derive_flags=[&](int ax,int ay,int z){
        uint32_t value=base_flags(ax,ay,z);
        const uint32_t below=z>loaded.front()?base_flags(ax,ay,z-1):0,above=z<loaded.back()?base_flags(ax,ay,z+1):0;
        if(below&NCMM_WORLD_CANOPY)value=(value|NCMM_WORLD_FLOOR)&~(NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_GLASS_FLOOR);
        if((above&NCMM_WORLD_DOWN)&&(above&NCMM_WORLD_RAMP)&&!(value&NCMM_WORLD_UNKNOWN))value|=NCMM_WORLD_UP|NCMM_WORLD_RAMP;
        if((value&NCMM_WORLD_RAMP)&&(value&NCMM_WORLD_UP)&&(value&NCMM_WORLD_OPAQUE))value|=NCMM_WORLD_RAMP_END;
        if((value&NCMM_WORLD_GLASS_FLOOR)&&(below&NCMM_WORLD_UP)&&(below&NCMM_WORLD_RAMP))value&=~NCMM_WORLD_GLASS_FLOOR;
        if((value&NCMM_WORLD_UP)&&((above&NCMM_WORLD_OPEN_DOWN)||((above&NCMM_WORLD_DOWN)&&(above&NCMM_WORLD_RAMP))))value|=NCMM_WORLD_CONNECTED_UP;
        if((value&NCMM_WORLD_DOWN)&&(below&NCMM_WORLD_UP))value|=NCMM_WORLD_CONNECTED_DOWN;
        return value;
    };
    auto flags=[&](int ax,int ay,int z){
        const int64_t x=int64_t(ax)-ox,y=int64_t(ay)-oy;
        if(x<0||y<0||x>=side||y>=side||z<loaded.front()||z>loaded.back())return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED);
        return graphics_world_derived.get(ax,ay,z,graphics_world_revision,[&]{return derive_flags(ax,ay,z);});
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
    auto query=[&](ncmm_world::voxel p){return graphics_world_derived.get_relative(p.x,p.y,absolute.z()+p.z-loaded.front(),graphics_world_revision,
        [&]{return derive_flags(ox+p.x,oy+p.y,absolute.z()+p.z);});};
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
    // Walking reuses the authoritative eye instead of retracing the world for
    // each subpixel sample. A stair camera still traces its displayed height:
    // using the destination storey would remove the floor around the camera
    // before it has passed through the actual stair opening.
    const ncmm_world::point visibility_eye=graphics_world_vertical_transition_active?eye:destination;
    const char *cache_control=std::getenv("NCMM_FP_CAMERA_CACHE");
    const char *cache_verify=std::getenv("NCMM_FP_CAMERA_CACHE_VERIFY");
    graphics_camera_cache.verify=cache_verify&&std::string(cache_verify)=="1";
    if(graphics_snapshot_lease.active()&&!graphics_world_transition_active&&!you.in_vehicle&&
       !(cache_control&&std::string(cache_control)=="0")){
        auto float_key=[](float value){uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));return uint64_t(bits);};
        const auto generation=renderer_coordinator.texture_generations();
        const ncmm_graphics_snapshot::key identity={graphics_snapshot_lease.generation(),graphics_world_epoch,graphics_world_revision,
            tilecontext->ncmm_graphics_tileset_epoch(),generation.instance,generation.textures,
            float_key(visibility_eye.x),float_key(visibility_eye.y),float_key(visibility_eye.z),
            uint64_t(width),uint64_t(height),uint64_t(absolute.z()),graphics_creature_events,graphics_attack_serial,0,0};
        graphics_camera_cache.prepare(identity,ox,oy,loaded.front(),loaded.back(),side);
    }
    // Explicit source invalidation precedes every ray lookup. Lazy discovery
    // of unchanged facts cannot spuriously invalidate the following frame.
    if(position_changed||graphics_world_ray_revision!=graphics_world_revision||
       std::abs(visibility_eye.x-graphics_world_ray_eye.x)>.0001f||std::abs(visibility_eye.y-graphics_world_ray_eye.y)>.0001f||std::abs(visibility_eye.z-graphics_world_ray_eye.z)>.0001f){
        ++graphics_world_ray_epoch;graphics_world_ray_revision=graphics_world_revision;graphics_world_ray_eye=visibility_eye;}
    struct candidate {ncmm_world_cell_v1 world{};tripoint_bub_ms pos;uint32_t permission=0;float distance=0;std::array<float,3> facade_light{};};
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
    ncmm_graphics_trace::cpu_zone visibility_zone("Visibility");
    const int visual_maximum=std::max(you.sight_range(default_daylight_level()),you.sight_range(0));
    const bool visual_no_sight=you.has_effect(efftype_id("no_sight"));
    ncmm_world::light_range_cache visual_ranges;
    auto in_visual_range=[&](const tripoint_bub_ms &at){return graphics_world_in_visual_range(at,visual_maximum,visual_no_sight,&visual_ranges);};
    // Every ray to a lower storey's body/slab/facade must cross the plane
    // below the eye, and every ray past the upper storey crosses its plane.
    // Keep the nearest upper slab for its visible underside. A single opening
    // defeats the proof, so shafts, glass and real multi-Z views are unchanged.
    const int eye_level=absolute.z()+int(std::floor(visibility_eye.z/NCMM_WORLD_LEVEL_HEIGHT));
    const bool sealed_below=graphics_world_sealed.closed(eye_level,flags),sealed_above=graphics_world_sealed.closed(eye_level+1,flags);
    // Include the displayed easing eye as well as the authoritative sight eye.
    // A rectangle proof may discard geometry only when every possible crossing
    // of the intervening plane is a real opaque slab.
    const int eye_left=int(std::floor(std::min(eye.x,visibility_eye.x))),eye_right=int(std::floor(std::max(eye.x,visibility_eye.x))),
              eye_top=int(std::floor(std::min(eye.y,visibility_eye.y))),eye_bottom=int(std::floor(std::max(eye.y,visibility_eye.y)));
    for(int z:loaded)if(ncmm_world::candidate_level_in_range(z,absolute.z(),fov_3d_z_range)&&
        (z==absolute.z()||!((sealed_below&&z<eye_level)||(sealed_above&&z>eye_level+1))))for(int gy=0;gy<side;++gy)for(int gx=0;gx<side;++gx){
        ++graphics_world_metrics.processed_cells;
        const float dx=gx+.5f-eye.x,dy=gy+.5f-eye.y,dz=(z-absolute.z())*NCMM_WORLD_LEVEL_HEIGHT+1.25f-eye.z,
            forward=(dx*cy+dy*sy)*cp+dz*sp,screen_x=-dx*sy+dy*cy,screen_y=dz*cp-(dx*cy+dy*sy)*sp;
        const bool close_to_eye=dx*dx+dy*dy+dz*dz<16;
        const float horizontal_limit=std::max(0.f,forward)*horizontal,vertical_limit=std::max(0.f,forward)*vertical;
        const auto outside=[&](float margin){return forward < -margin||std::abs(screen_x)>horizontal_limit+margin||std::abs(screen_y)>vertical_limit+margin;};
        // Reject only beyond the widest existing creature margin before doing
        // tracker/death lookups. Large and airborne actors retain the same bounds.
        if(!close_to_eye&&outside(20.f)){++graphics_world_metrics.frustum_rejected;continue;}
        const int ax=ox+gx,ay=oy+gy;
        if(graphics_camera_cache.enabled&&graphics_camera_cache.known(ax,ay,z)){
            if(!graphics_camera_cache.verify){
                ++graphics_camera_cache.totals.hits;
                if(const auto *owned=graphics_camera_cache.find(ax,ay,z)){
                    // Static cached cells use the original narrow margin. Actors
                    // and their death clips always take the live bounded path.
                    if(!close_to_eye&&outside(5.f)){++graphics_world_metrics.frustum_rejected;continue;}
                    candidate c{};c.world=owned->world;c.pos=here.get_bub(tripoint_abs_ms(ax,ay,z));
                    c.permission=c.world.flags&(NCMM_WORLD_BODY_VISIBLE|NCMM_WORLD_STRUCTURE_VISIBLE|NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_FLOOR_BOTTOM);
                    c.distance=dx*dx+dy*dy+dz*dz;c.facade_light=owned->facade_light;candidates.push_back(c);
                }else ++graphics_camera_cache.totals.empty_hits;
                continue;
            }
        }else if(graphics_camera_cache.enabled)++graphics_camera_cache.totals.misses;
        // The widest conservative bound is independent of map contents. Avoid
        // bubble conversion/loading probes for cells that cannot contribute.
        const auto p=here.get_bub(tripoint_abs_ms(ax,ay,z));
        if(!here.ncmm_graphics_loaded(p))continue;
        const Creature *entity=get_creature_tracker().creature_at<Creature>(p);
        const bool observed_death=std::binary_search(observed_deaths.begin(),observed_deaths.end(),std::array<int32_t,3>{ax,ay,z});
        const float margin=entity||observed_death?20.f:5.f;
        if(!close_to_eye&&outside(margin)){
            ++graphics_world_metrics.frustum_rejected;continue;}
        ncmm_graphics_snapshot::rejected_probe rejected{&graphics_camera_cache,ax,ay,z,!entity&&!observed_death};
        // Tall/airborne entities can extend beyond a source voxel. Leave their
        // normal native/body visibility path authoritative.
        if(!entity&&!observed_death&&(z<eye_level||z>eye_level+1)&&graphics_world_sealed.closed_rectangle(z<eye_level?eye_level:eye_level+1,
            ox+std::min(gx,eye_left),oy+std::min(gy,eye_top),ox+std::max(gx,eye_right),oy+std::max(gy,eye_bottom),flags)){
            ++graphics_world_metrics.occlusion_rejected;continue;
        }
        uint32_t semantic=flags(ax,ay,z);
        const bool air=!(semantic&(NCMM_WORLD_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_DOWN|NCMM_WORLD_UP|NCMM_WORLD_CANOPY|graphics_world_renderable));
        const bool local_air=air&&!entity&&!observed_death&&here.i_at(p).empty();
        // Frustum and local-air rejection precede native light/perception work.
        if(local_air&&(!atmosphere||dx*dx+dy*dy>16.f*16.f||z<eye_level||z>eye_level+2))continue;
        const auto below=tripoint_bub_ms(p.xy(),z-1);
        const bool current_in_range=in_visual_range(p),lower_in_range=in_visual_range(below);
        // Architecture is continuous inside the physical sight radius, even
        // when a dark room/tile centre has never passed the top-down FOV test.
        // Its light and the raster depth still determine what reaches the eye.
        const bool structure_range=rl_dist(you.pos_bub(),p)<=(visual_no_sight?1:visual_maximum);
        const bool static_surface=structure_range&&(semantic&(graphics_world_architecture|NCMM_WORLD_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_DOWN));
        // No permission below can survive if neither side is within native
        // visual range. In particular, don't trace thousands of dark buried
        // floors. Their private structural facts still occlude other rays.
        if(!static_surface&&!current_in_range&&!lower_in_range&&p!=you.pos_bub()){
            const uint32_t basic=base_flags(ax,ay,z);bool possible_face=false;
            if(basic&graphics_world_architecture)for(unsigned face=0;face<4&&!possible_face;++face){
                const bool facing=face==0?visibility_eye.x<gx:face==1?visibility_eye.x>gx+1:face==2?visibility_eye.y<gy:visibility_eye.y>gy+1;
                if(!facing)continue;
                const auto adjacent=ncmm_world::facade_neighbor({ax,ay,z},face);
                if(ncmm_world::structural_flags(base_flags(adjacent.x,adjacent.y,adjacent.z))&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))continue;
                const auto lit=here.get_bub(tripoint_abs_ms(adjacent.x,adjacent.y,adjacent.z));
                possible_face=graphics_world_in_visual_range(p,visual_maximum,visual_no_sight,&visual_ranges,&lit);
            }
            if(!possible_face){++graphics_world_metrics.occlusion_rejected;continue;}
        }
        // Static surfaces use the same physical depth buffer as their meshes.
        // Retracing five samples of every wall/slab after a step was both slow
        // and incomplete at openings. Native perception still owns all contents.
        const float level=(z-absolute.z())*NCMM_WORLD_LEVEL_HEIGHT;
        uint32_t permission=ncmm_world::surface_permissions(semantic,visibility_eye.z,level);
        bool facade_in_range=structure_range;std::array<float,3> facade_light{};
        if(semantic&graphics_world_architecture){
            facade_light=graphics_world_rgb(p);
            for(unsigned face=0;face<4;++face){
                const bool facing=face==0?visibility_eye.x<gx:face==1?visibility_eye.x>gx+1:face==2?visibility_eye.y<gy:visibility_eye.y>gy+1;
                if(!facing)continue;
                const auto adjacent=ncmm_world::facade_neighbor({ax,ay,z},face);
                if(ncmm_world::structural_flags(base_flags(adjacent.x,adjacent.y,adjacent.z))&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))continue;
                const auto lit=here.get_bub(tripoint_abs_ms(adjacent.x,adjacent.y,adjacent.z));
                if(!graphics_world_in_visual_range(p,visual_maximum,visual_no_sight,&visual_ranges,&lit))continue;
                facade_in_range=true;const auto rgb=graphics_world_rgb(lit);
                for(size_t channel=0;channel<3;++channel)facade_light[channel]=std::max(facade_light[channel],rgb[channel]);
            }
            if(facade_in_range)permission|=NCMM_WORLD_STRUCTURE_VISIBLE;
        }
        if(!structure_range&&!current_in_range)permission&=~NCMM_WORLD_FLOOR_TOP;
        if(!structure_range&&!lower_in_range)permission&=~NCMM_WORLD_FLOOR_BOTTOM;
        if((permission&NCMM_WORLD_STRUCTURE_VISIBLE)&&(semantic&NCMM_WORLD_FLOOR))
            permission|=ncmm_world::facade_slab_permissions(semantic,visibility_eye.z,level);
        // Air is needed only in the local precipitation volume. Do not trace
        // thousands of empty upper-Z voxels just because it is raining.
        if(local_air){
            if(current_in_range){permission|=NCMM_WORLD_STRUCTURE_VISIBLE;facade_light=graphics_world_rgb(p);}
        }else if(current_in_range&&you.sees(here,p)){
            // Bodies/loot retain exact 3D occlusion and native sight, including
            // glass, stairs, smoke and blindness. Empty structural slabs do not
            // need body rays merely to provide a ground/ceiling surface.
            const bool contents=(semantic&graphics_world_renderable)||entity||observed_death||!here.i_at(p).empty();
            const bool perceived=!contents||graphics_world_sources.visibility(ax,ay,z,graphics_world_ray_epoch,[&]{
                for(const auto &offset:probes)if(ncmm_world::visible(visibility_eye,{gx+offset[0],gy+offset[1],level+1.25f},query))return uint16_t(1);
                return uint16_t(0);
            });
            if(perceived)permission|=NCMM_WORLD_BODY_VISIBLE;
        }
        if(p==you.pos_bub())permission|=NCMM_WORLD_BODY_VISIBLE;
        if(!permission){++graphics_world_metrics.occlusion_rejected;continue;}
        if(atmosphere||(permission&NCMM_WORLD_BODY_VISIBLE)){
            const uint32_t column=graphics_world_roofs.get(ax,ay,0,graphics_world_revision,[&]{
                return ncmm_world::roof_column(loaded.front(),loaded.back(),[&](int height){return base_flags(ax,ay,height);});
            });
            if(column&(uint32_t(1)<<(z-loaded.front())))semantic|=NCMM_WORLD_ROOFED;
        }
        semantic&=262143u;
        if(!(permission&NCMM_WORLD_BODY_VISIBLE)){
            const bool landing=(semantic&NCMM_WORLD_DOWN)&&!(semantic&(NCMM_WORLD_LADDER|NCMM_WORLD_RAMP));
            semantic&=NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_OPEN_DOWN|NCMM_WORLD_WATER|NCMM_WORLD_UNKNOWN|
                ((permission&NCMM_WORLD_STRUCTURE_VISIBLE)?uint32_t(NCMM_WORLD_OPAQUE):0u);
            if(landing)semantic|=NCMM_WORLD_DOWN;
            if(local_air){if(graphics_world_roofs.get(ax,ay,0,graphics_world_revision,[&]{return ncmm_world::roof_column(loaded.front(),loaded.back(),[&](int h){return base_flags(ax,ay,h);});})&(uint32_t(1)<<(z-loaded.front())))semantic|=NCMM_WORLD_ROOFED;}
            else semantic|=NCMM_WORLD_ROOFED; // No hidden room navigation facts.
        }
        candidate c{};c.world={ax,ay,z,semantic|permission,0,0,graphics_world_level_revisions[size_t(z+128)]+1,{0,0,0}};
        c.pos=p;c.permission=permission;c.distance=dx*dx+dy*dy+dz*dz;c.facade_light=facade_light;
        if(permission&NCMM_WORLD_FLOOR_BOTTOM){const auto rgb=graphics_world_rgb(below);std::copy(rgb.begin(),rgb.end(),c.world.lower_light);}
        if((permission&NCMM_WORLD_BODY_VISIBLE)&&(semantic&(NCMM_WORLD_UP|NCMM_WORLD_RAMP|NCMM_WORLD_LADDER)))for(uint32_t direction=0;direction<4;++direction){const int nx=direction==0?1:direction==2?-1:0,ny=direction==1?1:direction==3?-1:0;
            const uint32_t next=flags(ax+nx,ay+ny,z);
            if((semantic&NCMM_WORLD_RAMP)&&(next&NCMM_WORLD_RAMP_END)){c.world.direction=direction;break;}
            if(!(semantic&NCMM_WORLD_RAMP)&&!(next&(NCMM_WORLD_UNKNOWN|NCMM_WORLD_OPAQUE))){c.world.direction=direction;break;}}
        candidates.push_back(c);
        rejected.remember=false;
        if(auto *owned=graphics_camera_cache.store_if(c.world,c.facade_light,[&]{return !entity&&!observed_death&&!here.veh_at(p);})){
            if(graphics_camera_cache.verify&&owned->base_ready){++graphics_camera_cache.totals.verification_checks;
                if(!ncmm_graphics_snapshot::same_world_cell(owned->world,c.world)||owned->facade_light!=c.facade_light)++graphics_camera_cache.totals.verification_mismatches;}
        }
    }
    graphics_world_metrics.visibility_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-visibility_started).count();
    visibility_zone.stop();
    graphics_world_range_evaluations=visual_ranges.evaluations;graphics_world_range_hits=visual_ranges.hits;
    graphics_world_material_evaluations=material_definitions.evaluations+elevated_material_definitions.evaluations;
    graphics_world_material_hits=material_definitions.hits+elevated_material_definitions.hits;
    if(candidates.size()>NCMM_WORLD_MAX_CELLS){std::stable_sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.distance<b.distance;});
        graphics_world_metrics.budget_rejected=uint32_t(candidates.size()-NCMM_WORLD_MAX_CELLS);candidates.resize(NCMM_WORLD_MAX_CELLS);
        std::sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return ncmm_world::order(a.world)<ncmm_world::order(b.world);});
    } // The uncapped z/y/x traversal is already in canonical order.
    std::vector<ncmm_graphics_cell_v1> cells(candidates.size());std::vector<tripoint_bub_ms> positions;
    std::vector<ncmm_world_cell_v1> coordinates;std::vector<ncmm_world_slice_v1> slices;std::vector<std::array<float,3>> facade_lights;
    positions.reserve(cells.size());coordinates.reserve(cells.size());facade_lights.reserve(cells.size());graphics_ceiling_cells=graphics_snapshot_far_cells=graphics_snapshot_light_queries=0;
    auto copy=[](char (&out)[128],const std::string &id){if(id.size()<sizeof(out))std::copy(id.begin(),id.end(),out);};
    for(size_t i=0;i<candidates.size();++i){const auto &source=candidates[i];const auto &p=source.pos;auto &c=cells[i];
        positions.push_back(p);coordinates.push_back(source.world);facade_lights.push_back(source.facade_light);c.flags=NCMM_GFX_VISIBLE;c.light=1;
        if(source.world.flags&NCMM_WORLD_FLOOR_BOTTOM)++graphics_ceiling_cells;
        if(std::max(std::abs(p.x()-you.pos_bub().x()),std::abs(p.y()-you.pos_bub().y()))>NCMM_GRAPHICS_RADIUS)++graphics_snapshot_far_cells;
        auto *owned=graphics_camera_cache.find(source.world.x,source.world.y,source.world.z);
        if(owned&&owned->base_ready&&!graphics_camera_cache.verify){c=owned->cell;continue;}
        struct store_base {
            ncmm_graphics_snapshot::entry *owned;ncmm_graphics_snapshot::cache &cache;ncmm_graphics_cell_v1 &value;
            ~store_base(){if(!owned)return;if(owned->base_ready&&cache.verify){++cache.totals.verification_checks;if(std::memcmp(&owned->cell,&value,sizeof(value)))++cache.totals.verification_mismatches;}
                owned->cell=value;owned->base_ready=true;}
        } save_base{owned,graphics_camera_cache,c};
        if(!(source.world.flags&NCMM_WORLD_FLOOR))c.flags|=NCMM_GFX_NO_FLOOR;
        if(source.world.flags&NCMM_WORLD_ROOFED)c.flags|=NCMM_GFX_CEILING;
        if(source.permission&NCMM_WORLD_BODY_VISIBLE)copy(c.terrain,here.ter(p).id().str());
        else if(source.permission&NCMM_WORLD_STRUCTURE_VISIBLE){
            const auto &ter=here.ter(p).obj();
            copy(c.terrain,ter.id.str()); // Same physical facade before/after 2D discovery.
            if(source.world.flags&NCMM_WORLD_OPAQUE)c.flags|=NCMM_GFX_WALL;
        }
        else{const auto kind=ncmm_graphics_presentation_policy::material(here.ter(p).id().str());
            copy(c.terrain,kind==NCMM_MAT_WOOD?"t_floor_wood":kind==NCMM_MAT_METAL?"t_floor_metal":"t_floor");}
        if(!(source.permission&NCMM_WORLD_BODY_VISIBLE))continue;
        if(source.world.flags&NCMM_WORLD_OPAQUE)c.flags|=NCMM_GFX_WALL;
        if(here.has_furn(p))copy(c.furniture,here.furn(p).id().str());
        if(const auto vp=here.veh_at(p))if(const auto displayed=vp->part_displayed()){
            c.flags|=NCMM_GFX_VEHICLE;std::fill(std::begin(c.furniture),std::end(c.furniture),'\0');copy(c.furniture,vpart_display(displayed->part()).get_tileset_id());}
        if(ncmm_world::supported_item(source.world.flags,here.has_furn(p)||bool(here.veh_at(p)))&&here.sees_some_items(p,you)&&you.sees(here,p)){const auto items=here.i_at(p);if(!items.empty())copy(c.item,items.begin()->typeId().str());}
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
    graphics_world_metrics.submitted_cells=uint32_t(cells.size());graphics_world_metrics.source_cache_bytes=graphics_world_sources.bytes()+graphics_world_roofs.bytes()+graphics_world_derived.bytes()+graphics_world_sealed.bytes();
    graphics_world_metrics.source_cache_hits=graphics_world_sources.hits;graphics_world_metrics.source_cache_misses=graphics_world_sources.misses;
    graphics_world_metrics.snapshot_bytes=cells.capacity()*sizeof(cells[0])+positions.capacity()*sizeof(positions[0])+coordinates.capacity()*sizeof(coordinates[0])+slices.capacity()*sizeof(slices[0])+graphics_camera_cache.bytes();
    graphics_world_metrics.extraction_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();
    graphics_build_lod(ox,oy,absolute.z(),side,!visual_no_sight&&visual_maximum>5);
    snapshot_zone.stop();
    const bool presented=graphics_draw_hybrid(base,positions,x,y,&world,&facade_lights);
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
int graphics_get_lod(ncmm_graphics_lod_scene_v1 *){return 0;}
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
const ncmm_host_graphics_multilevel_v1 graphics_world_api={1,sizeof(ncmm_host_graphics_multilevel_v1),1,1,
    graphics_register_world,graphics_enable,graphics_enabled,graphics_get_sprite,graphics_submit_frame,graphics_gpu_submit_world,graphics_get_world_metrics,graphics_get_lod};

