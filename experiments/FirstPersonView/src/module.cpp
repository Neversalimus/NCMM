#include "ncmm_sdk_core.hpp"
#include "renderer.hpp"
#include "hybrid_renderer.hpp"
#include <chrono>
namespace {
constexpr const char *id="first_person_view";
const char *caps[]={"core.v1","api.versioning.v1","host_api.v2.core","module_hotkeys.v1",NCMM_GRAPHICS_CAP};
const ncmm_host_graphics_v1 *graphics=nullptr;
const ncmm_host_graphics_v2 *hybrid=nullptr;
const ncmm_host_graphics_controls_v1 *controls=nullptr;
double yaw=-first_person::pi*0.5;
float pitch=0;
std::vector<ncmm_graphics_quad_v1> commands;
first_person::hybrid::frame pixels;
first_person::hybrid::frame world_pixels;
bool high_quality=false,frame_valid=false;
uint64_t frame_key=0;
uint64_t world_key=0;
bool world_valid=false;
std::chrono::steady_clock::time_point frame_built{};
std::chrono::steady_clock::time_point world_built{};
uint64_t scene_key(const ncmm_graphics_scene_v2 &s,bool world=false){
    uint64_t h=1469598103934665603ull;
    auto bytes=[&](const void *p,size_t n){const auto *b=static_cast<const unsigned char *>(p);for(size_t i=0;i<n;++i){h^=b[i];h*=1099511628211ull;}};
    bytes(&yaw,sizeof(yaw));bytes(&pitch,sizeof(pitch));bytes(&high_quality,sizeof(high_quality));
    const auto &b=s.base;bytes(&b.pixel_width,sizeof(b.pixel_width)*4);bytes(&b.camera_x,sizeof(b.camera_x)*2);
    for(size_t i=0;i<b.cell_count;++i){bytes(&b.cells[i].flags,sizeof(b.cells[i].flags));
        if(b.cells[i].flags&NCMM_GFX_VISIBLE){bytes(&b.cells[i],sizeof(b.cells[i]));bytes(&s.details[i],sizeof(s.details[i]));}}
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
    const auto now=std::chrono::steady_clock::now();const auto state=control_state();
    const auto pose=first_person::animation::sample(state,std::floor(scene->environment.animation_seconds*40)/40);
    uint64_t key=scene_key(*scene);
    for(float value:{pose.swing,pose.thrust,pose.recoil,pose.length}){uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));key=(key^bits)*1099511628211ull;}
    key=(key^pose.grip)*1099511628211ull;
    const uint64_t wk=scene_key(*scene,true);
    const bool rebuild_world=!world_valid||wk!=world_key||now-world_built>std::chrono::milliseconds(250);
    // Keep complete owned pixels only; callback-scoped sprite pointers never
    // survive a frame. Periodic refresh also handles a changed tileset atlas.
    if(!frame_valid||rebuild_world||key!=frame_key||now-frame_built>std::chrono::milliseconds(250)){
        const first_person::hybrid::sprite_provider provider=[](uint32_t i,uint32_t layer,ncmm_graphics_sprite_v2 &s){return hybrid&&hybrid->get_sprite(i,layer,&s)!=0;};
        if(!first_person::hybrid::build_frame(*scene,float(yaw),pitch,provider,pixels,high_quality?1920:1280,high_quality?1080:800,pose,rebuild_world?nullptr:&world_pixels,rebuild_world?&world_pixels:nullptr)){frame_valid=world_valid=false;return 0;}
        if(rebuild_world){world_key=wk;world_built=now;world_valid=true;}
        frame_key=key;frame_built=now;frame_valid=true;
    }
    return hybrid->submit_frame(pixels.pixels.data(),uint32_t(pixels.width),uint32_t(pixels.height),uint32_t(pixels.width));
}
int render(const ncmm_graphics_scene_v1 *scene,void *) {
    return graphics && scene && first_person::build_frame(*scene,yaw,commands) &&
        graphics->submit_quads(commands.data(),commands.size());
}
void input(uint32_t action,void *) {
    frame_valid=false;
    if(action==NCMM_GFX_QUALITY)high_quality=!high_quality;
    if(action==NCMM_GFX_TURN_LEFT) yaw-=first_person::pi/12.0;
    if(action==NCMM_GFX_TURN_RIGHT) yaw+=first_person::pi/12.0;
    if(action==NCMM_GFX_LOOK_UP) pitch=std::min(1.2f,pitch+first_person::hybrid::pi/18);
    if(action==NCMM_GFX_LOOK_DOWN) pitch=std::max(-1.2f,pitch-first_person::hybrid::pi/18);
    if(action==NCMM_GFX_MOUSE_LOOK){const auto state=control_state();yaw+=state.mouse_dx*.003;pitch=std::clamp(pitch-state.mouse_dy*.003f,-1.2f,1.2f);}
    yaw=std::remainder(yaw,first_person::pi*2.0);
    if(controls)controls->set_camera(float(yaw),pitch);
}
int init(const ncmm_host_api_v1 *api) {
    graphics=nullptr;
    hybrid=nullptr;
    controls=nullptr;frame_valid=world_valid=false;high_quality=false;
    const auto core=ncmm::sdk::require_core(api,caps,sizeof(caps)/sizeof(caps[0]),NCMM_SDK_CORE_FIELD_END(api_minor));
    if(!core) return 0;
    if(api->has_capability(NCMM_GRAPHICS_V2_CAP)) {
        auto *candidate=static_cast<const ncmm_host_graphics_v2 *>(api->query_interface(NCMM_GRAPHICS_V2_ID,2,0));
        if(!candidate || candidate->abi_version!=2 || candidate->major!=2 || candidate->struct_size<sizeof(ncmm_host_graphics_v2) ||
           !candidate->register_view || !candidate->set_enabled || !candidate->is_enabled || !candidate->get_sprite || !candidate->submit_frame) return 0;
        if(!candidate->register_view(id,render_hybrid,input,nullptr))return 0;
        hybrid=candidate;yaw=-first_person::pi*.5;pitch=0;
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
    graphics=nullptr;
    commands.clear();
    pixels={};world_pixels={};frame_valid=world_valid=false;
}
const ncmm_mod_descriptor_v1 descriptor={NCMM_ABI_VERSION,id,"First Person View","0.6.0-dev",
    caps,sizeof(caps)/sizeof(caps[0]),init,shutdown};
}
extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1(){return &descriptor;}
extern "C" NCMM_EXPORT void ncmm_open_ui_v1(const ncmm_host_api_v1 *) {
    if(hybrid){frame_valid=world_valid=false;hybrid->set_enabled(id,!hybrid->is_enabled(id));}
    if(graphics) graphics->set_enabled(id,!graphics->is_enabled(id));
}
