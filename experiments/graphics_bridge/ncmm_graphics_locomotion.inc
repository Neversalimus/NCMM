// Included after world extraction, before the FPS action dispatcher. All Native
// reads remain on its blocked main thread; the 3x3 stencil owns booleans only.
#if defined(TILES)
extern bool graphics_fps_audit_active;
std::atomic<uint32_t> graphics_locomotion_audit_frames{0};
struct graphics_locomotion_state {
    ncmm_graphics_camera::locomotion motion;
    std::optional<tripoint_abs_ms> origin,committed;
    std::array<bool,9> blocked{};
    uint64_t lease=0,revision=0,epoch=0,stencil_builds=0,stencil_reads=0;
    bool issued=false;float walking_speed=3;
    ncmm_graphics_camera::locomotion::point presented{};
} graphics_walk;
bool graphics_locomotion_continuous(){return graphics_input_router.continuous_walk();}
void graphics_locomotion_reset(){graphics_walk={};}
void graphics_locomotion_pause(uint64_t now){graphics_walk.motion.pause(now);}
bool graphics_locomotion_sync(bool consume){
    if(!character_state_available()){graphics_locomotion_reset();return false;}
    const auto absolute=get_avatar().pos_abs();
    if(!graphics_walk.origin)graphics_walk.origin=absolute;
    else if(*graphics_walk.origin!=absolute){
        const auto before=*graphics_walk.origin;
        const int64_t dx=int64_t(absolute.x())-before.x(),dy=int64_t(absolute.y())-before.y();
        if(graphics_walk.issued&&absolute.z()==before.z()&&std::abs(dx)<=1&&std::abs(dy)<=1&&
           graphics_walk.motion.acknowledge(int(dx),int(dy)))graphics_walk.committed=absolute;
        else{graphics_walk.motion.reset(SDL_GetTicks());graphics_walk.committed.reset();}
        graphics_walk.origin=absolute;graphics_walk.issued=false;graphics_walk.lease=0;
    }
    const bool committed=graphics_walk.committed&&*graphics_walk.committed==absolute;
    if(consume)graphics_walk.committed.reset();
    return committed;
}
bool graphics_locomotion_moving(){return graphics_locomotion_continuous()&&graphics_walk.motion.moving();}
float graphics_locomotion_movement(){
    return std::clamp(graphics_walk.motion.speed()/std::max(.1f,graphics_walk.walking_speed),0.f,1.f);
}
ncmm_graphics_camera::offset graphics_locomotion_offset(){
    if(!graphics_locomotion_continuous())return {};
    const auto p=graphics_walk.motion.position();
    // Small distance-driven footfall, independent of wall-clock idle animation.
    const float bob=std::sin(graphics_walk.motion.distance()*8.0f)*.009f*graphics_locomotion_movement()*
        (graphics_input_router.aiming()?.2f:1.f);
    return {p.x,p.y,bob};
}
void graphics_locomotion_presented(){
    graphics_walk.presented=graphics_walk.motion.position();
    if(graphics_fps_audit_active&&std::hypot(graphics_walk.presented.x,graphics_walk.presented.y)>.001f)
        graphics_locomotion_audit_frames.fetch_add(1);
}
void graphics_locomotion_tick(uint64_t now){
    graphics_locomotion_sync();
    if(!character_state_available())return;
    auto &walk=graphics_walk.motion;
    if(!graphics_locomotion_continuous()){
        if(walk.distance()!=0||walk.pending())walk.reset(now);else walk.clock(now);
        return;
    }
    if(!graphics_input_router.walking_owned()){
        if(!graphics_walk.issued)walk.pause(now);else walk.clock(now);return;
    }
    // Simulation processing, nested Native UI and legacy stairs never accrue
    // catch-up time or query mutable terrain between owned input lifetimes.
    if(!graphics_snapshot_lease.active()||graphics_world_camera_motion.active(now)){
        walk.clock(now);return;
    }
    const auto intent=graphics_input_router.walking();const auto fractional=walk.position();
    if(!intent.forward&&!intent.right&&!walk.moving()&&std::abs(fractional.x)+std::abs(fractional.y)<1e-6f){walk.clock(now);return;}
    avatar &you=get_avatar();map &here=get_map();
    if(you.in_vehicle){walk.reset(now);return;}
    const auto generation=graphics_snapshot_lease.generation();
    auto blocked=[&](int x,int y){return x< -1||x>1||y< -1||y>1||graphics_walk.blocked[size_t((y+1)*3+x+1)];};
    if(graphics_walk.lease!=generation||graphics_walk.revision!=graphics_world_revision||graphics_walk.epoch!=graphics_world_epoch){
        for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
            const auto p=you.pos_bub()+tripoint_rel_ms(x,y,0);bool solid=false;
            if(x||y){++graphics_walk.stencil_reads;
                solid=!here.ncmm_graphics_loaded(p)||!here.passable(p)||
                    (!here.has_floor_or_water(p)&&!here.has_flag(ter_furn_flag::TFLAG_GOES_DOWN,p)&&!here.has_flag(ter_furn_flag::TFLAG_RAMP,p))||
                    bool(here.veh_at(p))||get_creature_tracker().creature_at<Creature>(p)!=nullptr;
            }
            graphics_walk.blocked[size_t((y+1)*3+x+1)]=solid;
        }
        ++graphics_walk.stencil_builds;graphics_walk.lease=generation;
        graphics_walk.revision=graphics_world_revision;graphics_walk.epoch=graphics_world_epoch;
        walk.constrain(blocked,now);
    }
    const double configured=world_setting_get_f64("NCMM_FP_WALK_SPEED",3);
    const float base=std::isfinite(configured)?float(std::clamp(configured,1.0,6.0)):3.f;
    const float stance=you.is_prone()?.3f:you.is_crouching()?.55f:you.is_running()?1.65f:1.f;
    graphics_walk.walking_speed=base*stance*std::clamp(2.f/std::max(2,here.move_cost(you.pos_bub())),.25f,1.f);
    if(walk.advance(now,graphics_camera_yaw,intent.forward,intent.right,graphics_walk.walking_speed,blocked))graphics_mouse_dirty=true;
    const auto step=walk.pending();if(step)graphics_input_router.boundary(step.x,step.y);
}
void graphics_locomotion_completed(bool changed){
    graphics_locomotion_sync();
    if(graphics_walk.issued&&!changed){graphics_walk.motion.reject(SDL_GetTicks());graphics_walk.issued=false;graphics_walk.lease=0;}
    graphics_walk.motion.clock(SDL_GetTicks());
}
#else
void graphics_locomotion_reset(){}
void graphics_locomotion_pause(uint64_t){}
void graphics_locomotion_tick(uint64_t){}
void graphics_locomotion_presented(){}
bool graphics_locomotion_sync(bool){return false;}
bool graphics_locomotion_continuous(){return false;}
bool graphics_locomotion_moving(){return false;}
float graphics_locomotion_movement(){return 0;}
ncmm_graphics_camera::offset graphics_locomotion_offset(){return {};}
#endif
