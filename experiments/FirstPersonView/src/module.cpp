#include "ncmm_sdk_core.hpp"
#include "renderer.hpp"
#include "hybrid_renderer.hpp"
#include "scene_hash.hpp"
#include <chrono>
namespace {
constexpr const char *id="first_person_view";
const char *caps[]={"core.v1","api.versioning.v1","host_api.v2.core","module_hotkeys.v1",NCMM_GRAPHICS_CAP};
const ncmm_host_graphics_v1 *graphics=nullptr;
const ncmm_host_graphics_v2 *hybrid=nullptr;
const ncmm_host_graphics_controls_v1 *controls=nullptr;
const ncmm_host_graphics_presentation_v1 *presentation=nullptr;
first_person::hybrid::presentation_data metadata;
const ncmm_host_graphics_creatures_v1 *creatures=nullptr;
std::unique_ptr<first_person::creature::presentation> creature_state;
double yaw=-first_person::pi*0.5;
float pitch=0,look_sway_x=0,look_sway_y=0;
double look_sway_started=0,last_scene_seconds=0;
std::vector<ncmm_graphics_quad_v1> commands;
first_person::hybrid::frame pixels;
first_person::hybrid::frame world_pixels;
bool high_quality=false,frame_valid=false,gpu_effects=true;
uint64_t frame_key=0;
uint64_t world_key=0;
bool world_valid=false;
std::chrono::steady_clock::time_point frame_built{};
std::chrono::steady_clock::time_point world_built{};
uint64_t geometry_key(const ncmm_graphics_scene_v2 &s,bool dynamic=false){
    uint64_t h=1469598103934665603ull;
    auto bytes=[&](const void *p,size_t n){h=first_person::hash_bytes(h,p,n);};
    bytes(&yaw,sizeof(yaw));bytes(&pitch,sizeof(pitch));bytes(&high_quality,sizeof(high_quality));
    const auto &b=s.base;bytes(&b.pixel_width,sizeof(b.pixel_width)*4);bytes(&b.camera_x,sizeof(b.camera_x)*2);
    for(size_t i=0;i<b.cell_count;++i){
        if(b.cells[i].flags&NCMM_GFX_VISIBLE){
            if(dynamic){auto cell=b.cells[i];std::memset(cell.creature,0,sizeof(cell.creature));auto detail=s.details[i];detail.terrain_flags&=~(NCMM_CREATURE_TINY|NCMM_CREATURE_SMALL|NCMM_CREATURE_LARGE|NCMM_CREATURE_HUGE);bytes(&cell,sizeof(cell));bytes(&detail,sizeof(detail));}
            else{bytes(&b.cells[i],sizeof(b.cells[i]));bytes(&s.details[i],sizeof(s.details[i]));}}
        else bytes(&b.cells[i].flags,sizeof(b.cells[i].flags));}
    return h;
}
uint64_t scene_key(const ncmm_graphics_scene_v2 &s,uint64_t common,bool world=false){
    uint64_t h=common;auto bytes=[&](const void *p,size_t n){h=first_person::hash_bytes(h,p,n);};
    auto e=s.environment;
    e.animation_seconds=!world&&(e.precipitation>0||e.lightning)?std::floor(std::fmod(e.animation_seconds,100000.0)*30)/30:0;
    if(world){std::memset(e.held_item,0,sizeof(e.held_item));e.held_kind=0;}
    bytes(&e,sizeof(e));return h;
}
ncmm_graphics_control_state_v1 control_state(){
    ncmm_graphics_control_state_v1 state{};state.struct_size=sizeof(state);state.held_length=.6f;
    if(controls&&!controls->get_state(&state))state={sizeof(state),0,NCMM_GRIP_SINGLE,0,0,0,0,0,.6f};
    if(!std::isfinite(state.attack_started)||!std::isfinite(state.mouse_dx)||!std::isfinite(state.mouse_dy)||!std::isfinite(state.held_length)||state.attack_kind>2||state.grip_kind>NCMM_GRIP_GUN)state={sizeof(state),0,NCMM_GRIP_SINGLE,0,0,0,0,0,.6f};
    return state;
}
int render_hybrid(const ncmm_graphics_scene_v2 *scene,void *) {
    if(!hybrid||!scene||!first_person::hybrid::valid_scene(*scene,float(yaw),pitch))return 0;
    last_scene_seconds=scene->environment.animation_seconds;
    const auto now=std::chrono::steady_clock::now();const auto state=control_state();
    bool has_metadata=false;
    if(presentation){metadata.view={};metadata.view.struct_size=sizeof(metadata.view);metadata.cells.resize(scene->base.cell_count);
        has_metadata=presentation->get_scene(&metadata.view)&&presentation->get_cells(metadata.cells.data(),metadata.cells.size());
        if(!gpu_effects)metadata.view.flags&=~NCMM_PRESENT_GPU_EFFECTS;}
    bool has_creatures=false;
    if(creatures&&creature_state){auto &input=creature_state->input;input.view={};input.view.struct_size=sizeof(input.view);
        if(creatures->get_scene(&input.view)&&input.view.count<=scene->base.cell_count+256){input.creatures.resize(input.view.count);
            has_creatures=creatures->get_creatures(input.creatures.data(),input.creatures.size())&&first_person::creature::valid(input,scene->base.cell_count);}
        if(has_creatures)creature_state->observe();}
    auto pose=first_person::animation::sample(state,std::floor(scene->environment.animation_seconds*40)/40);
    if(has_metadata){const float t=float(std::fmod(scene->environment.animation_seconds,1000.0));
        const float lag=std::exp(-float(std::max(0.0,scene->environment.animation_seconds-look_sway_started))*9);
        pose.sway_x=std::sin(t*1.4f)*.0022f+std::sin(t*8)*metadata.view.movement*.008f+look_sway_x*lag;
        pose.sway_y=std::sin(t*2.2f)*.0032f+std::abs(std::sin(t*10))*metadata.view.movement*.010f+look_sway_y*lag;}
    const uint64_t common=geometry_key(*scene,has_creatures);
    uint64_t key=scene_key(*scene,common);
    for(float value:{pose.swing,pose.thrust,pose.recoil,pose.length,pose.sway_x,pose.sway_y}){uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));key=(key^bits)*1099511628211ull;}
    key=(key^pose.grip)*1099511628211ull;
    uint64_t wk=scene_key(*scene,common,true);
    if(has_creatures){const auto &input=creature_state->input;const auto phase=int64_t(std::floor(input.view.seconds*40));
        key=first_person::hash_bytes(key,&phase,sizeof(phase));key=first_person::hash_bytes(key,input.creatures.data(),input.creatures.size()*sizeof(ncmm_graphics_creature_v1));
        key^=creature_state->resources.statistics().generated;
        wk=first_person::hash_bytes(wk,&input.view.world_epoch,sizeof(input.view.world_epoch));wk=first_person::hash_bytes(wk,&input.view.tileset_epoch,sizeof(input.view.tileset_epoch));}
    if(has_metadata){auto hash=[&](const void *data,size_t n){wk=first_person::hash_bytes(wk,data,n);};
        // Appearance and movement only change composition, not static geometry.
        const uint64_t presentation_key=first_person::hash_bytes(1469598103934665603ull,&metadata.view,sizeof(metadata.view));
        key^=presentation_key;
        hash(&metadata.view.eye_height,sizeof(metadata.view.eye_height));hash(&metadata.view.sky_style,sizeof(metadata.view.sky_style));
        hash(&metadata.view.occupied_vehicle,sizeof(metadata.view.occupied_vehicle));if(has_creatures){for(auto detail:metadata.cells){detail.hostile=0;detail.creature_height=detail.creature_width=detail.creature_elevation=0;hash(&detail,sizeof(detail));}}
        else hash(metadata.cells.data(),metadata.cells.size()*sizeof(metadata.cells[0]));key^=wk;}
    const bool rebuild_world=!world_valid||wk!=world_key||now-world_built>std::chrono::milliseconds(250);
    // Keep complete owned pixels only; callback-scoped sprite pointers never
    // survive a frame. Periodic refresh also handles a changed tileset atlas.
    if(!frame_valid||rebuild_world||key!=frame_key||now-frame_built>std::chrono::milliseconds(250)){
        const first_person::hybrid::sprite_provider provider=[](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &s){return hybrid&&hybrid->get_sprite(i,layer,&s)!=0;};
        if(!first_person::hybrid::build_frame(*scene,float(yaw),pitch,provider,pixels,high_quality?1920:1280,high_quality?1080:800,pose,rebuild_world?nullptr:&world_pixels,rebuild_world?&world_pixels:nullptr,has_metadata?&metadata:nullptr,{},has_creatures?creature_state.get():nullptr)){frame_valid=world_valid=false;return 0;}
        if(rebuild_world){world_key=wk;world_built=now;world_valid=true;}
        frame_key=key;frame_built=now;frame_valid=true;
    }
    if(has_creatures&&creatures->submit_metrics){const auto stats=creature_state->resources.statistics();ncmm_graphics_creature_metrics_v1 m{};m.struct_size=sizeof(m);
        std::copy(pixels.creature_lods.begin(),pixels.creature_lods.end(),m.lod);m.fallback=pixels.creature_fallback;m.culled=pixels.creature_culled;
        m.cache_bytes=stats.bytes;m.cache_peak_bytes=stats.peak_bytes;m.generated=stats.generated;m.entries=stats.entries;m.queued=stats.pending;m.evictions=stats.evictions;m.failures=stats.failures;m.visible_pixels=std::count_if(pixels.creatures.begin(),pixels.creatures.end(),[](int32_t cell){return cell!=-1;});
        m.generation_ms=stats.last_generation_ms;m.generation_max_ms=stats.max_generation_ms;m.generation_total_ms=stats.total_generation_ms;
        std::copy(stats.anatomy_counts.begin(),stats.anatomy_counts.end(),m.anatomy);std::copy(stats.fallback_counts.begin(),stats.fallback_counts.end(),m.reasons);creatures->submit_metrics(&m);}
    if(presentation){presentation->set_target(pixels.aimed,pixels.label_x,pixels.label_y);
        if(!pixels.effects.empty()&&!presentation->submit_effects(pixels.effects.data(),pixels.effect_width,pixels.effect_height,pixels.effect_width)){
            ncmm_graphics_composite::blend(pixels.pixels,pixels.width,pixels.height,pixels.effects,pixels.effect_width,pixels.effect_height);frame_valid=false;}}
    return hybrid->submit_frame(pixels.pixels.data(),uint32_t(pixels.width),uint32_t(pixels.height),uint32_t(pixels.width));
}
int render(const ncmm_graphics_scene_v1 *scene,void *) {
    return graphics && scene && first_person::build_frame(*scene,yaw,commands) &&
        graphics->submit_quads(commands.data(),commands.size());
}
void input(uint32_t action,void *) {
    frame_valid=false;
    if(action==NCMM_GFX_ACCELERATION){gpu_effects=!gpu_effects;world_valid=false;}
    if(action==NCMM_GFX_QUALITY)high_quality=!high_quality;
    if(action==NCMM_GFX_TURN_LEFT) yaw-=first_person::pi/12.0;
    if(action==NCMM_GFX_TURN_RIGHT) yaw+=first_person::pi/12.0;
    if(action==NCMM_GFX_LOOK_UP) pitch=std::min(1.2f,pitch+first_person::hybrid::pi/18);
    if(action==NCMM_GFX_LOOK_DOWN) pitch=std::max(-1.2f,pitch-first_person::hybrid::pi/18);
    if(action==NCMM_GFX_MOUSE_LOOK){const auto state=control_state();
        look_sway_x=std::clamp(-state.mouse_dx*.00006f,-.015f,.015f);look_sway_y=std::clamp(state.mouse_dy*.00004f,-.01f,.01f);
        look_sway_started=metadata.view.struct_size?last_scene_seconds:0;yaw+=state.mouse_dx*.003;pitch=std::clamp(pitch-state.mouse_dy*.003f,-1.2f,1.2f);}
    yaw=std::remainder(yaw,first_person::pi*2.0);
    if(controls)controls->set_camera(float(yaw),pitch);
}
int init(const ncmm_host_api_v1 *api) {
    graphics=nullptr;
    hybrid=nullptr;
    controls=nullptr;presentation=nullptr;metadata={};creatures=nullptr;creature_state.reset();look_sway_x=look_sway_y=0;look_sway_started=last_scene_seconds=0;frame_valid=world_valid=false;high_quality=false;gpu_effects=true;
    const auto core=ncmm::sdk::require_core(api,caps,sizeof(caps)/sizeof(caps[0]),NCMM_SDK_CORE_FIELD_END(api_minor));
    if(!core) return 0;
    if(api->has_capability(NCMM_GRAPHICS_V2_CAP)) {
        auto *candidate=static_cast<const ncmm_host_graphics_v2 *>(api->query_interface(NCMM_GRAPHICS_V2_ID,2,0));
        if(!candidate || candidate->abi_version!=2 || candidate->major!=2 || candidate->struct_size<sizeof(ncmm_host_graphics_v2) ||
           !candidate->register_view || !candidate->set_enabled || !candidate->is_enabled || !candidate->get_sprite || !candidate->submit_frame) return 0;
        if(!candidate->register_view(id,render_hybrid,input,nullptr))return 0;
        hybrid=candidate;yaw=-first_person::pi*.5;pitch=0;
        if(api->has_capability(NCMM_GRAPHICS_PRESENTATION_CAP)){
            auto *p=static_cast<const ncmm_host_graphics_presentation_v1 *>(api->query_interface(NCMM_GRAPHICS_PRESENTATION_ID,1,0));
            if(p&&p->abi_version==1&&p->major==1&&p->struct_size>=sizeof(*p)&&p->get_scene&&p->get_cells&&p->set_target&&p->submit_effects)presentation=p;}
        if(api->has_capability(NCMM_GRAPHICS_CREATURES_CAP)){
            auto *c=static_cast<const ncmm_host_graphics_creatures_v1 *>(api->query_interface(NCMM_GRAPHICS_CREATURES_ID,1,0));
            if(c&&c->abi_version==1&&c->major==1&&c->struct_size>=sizeof(*c)&&c->get_scene&&c->get_creatures){creatures=c;creature_state=std::make_unique<first_person::creature::presentation>();}}
        if(api->has_capability(NCMM_GRAPHICS_CONTROLS_CAP)){
            auto *c=static_cast<const ncmm_host_graphics_controls_v1 *>(api->query_interface(NCMM_GRAPHICS_CONTROLS_ID,1,0));
            if(c&&c->abi_version==1&&c->major==1&&c->struct_size>=sizeof(*c)&&c->get_state&&c->set_camera){controls=c;controls->set_camera(float(yaw),pitch);}}
        return 1;
    }
    const auto *candidate=static_cast<const ncmm_host_graphics_v1 *>(api->query_interface(NCMM_GRAPHICS_ID,1,0));
    if(!candidate || candidate->abi_version!=NCMM_GRAPHICS_ABI || candidate->major!=1 ||
       candidate->struct_size<sizeof(ncmm_host_graphics_v1) || !candidate->register_view ||
       !candidate->set_enabled || !candidate->is_enabled || !candidate->submit_quads) return 0;
    if(!candidate->register_view(id,render,input,nullptr)) return 0;
    graphics=candidate;
    yaw=-first_person::pi*0.5;
    return 1;
}
void shutdown() {
    if(hybrid) hybrid->set_enabled(id,0);
    if(graphics) graphics->set_enabled(id,0);
    hybrid=nullptr;
    controls=nullptr;
    presentation=nullptr;metadata={};creatures=nullptr;creature_state.reset();
    graphics=nullptr;
    commands.clear();
    pixels={};world_pixels={};frame_valid=world_valid=false;
}
const ncmm_mod_descriptor_v1 descriptor={NCMM_ABI_VERSION,id,"First Person View","0.9.0-dev",
    caps,sizeof(caps)/sizeof(caps[0]),init,shutdown};
}
extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1(){return &descriptor;}
extern "C" NCMM_EXPORT void ncmm_open_ui_v1(const ncmm_host_api_v1 *) {
    if(hybrid){frame_valid=world_valid=false;hybrid->set_enabled(id,!hybrid->is_enabled(id));}
    if(graphics) graphics->set_enabled(id,!graphics->is_enabled(id));
}
