#pragma once
#include "ncmm_graphics_controls.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
namespace first_person::animation {
struct pose {float swing=0,thrust=0,recoil=0,sway_x=0,sway_y=0;uint32_t grip=NCMM_GRIP_SINGLE;float length=.6f;
    double seconds=0;float attack_phase=-1,equip_phase=1;uint64_t attack_serial=0;uint32_t attack_kind=0;};
inline float smooth(float x){x=std::clamp(x,0.0f,1.0f);return x*x*(3-2*x);}
inline pose sample(const ncmm_graphics_control_state_v1 &s,double now){
    pose p;p.grip=s.grip_kind;p.length=std::clamp(s.held_length,.15f,2.7f);p.seconds=now;p.attack_serial=s.attack_serial;p.attack_kind=s.attack_kind;
    const double age=now-s.attack_started;
    if(!s.attack_serial||!std::isfinite(age)||age<0||age>=.48)return p;
    const float t=float(age/.48),strike=t<.24f?smooth(t/.24f):1-smooth((t-.24f)/.76f);p.attack_phase=t;
    if(s.attack_kind==2)p.recoil=strike*.065f;
    else if(p.grip==NCMM_GRIP_POLE)p.thrust=strike*.30f;
    else p.swing=strike*.95f;
    return p;
}
// Only presentation state is retained. Absolute Host time drives every clip;
// redraw frequency cannot restart an attack or advance native simulation.
class timeline {
    std::string item;
    uint64_t epoch=0,last_serial=0,blocked_serial=0;
    double previous=0,equip_started=-1;
    bool initialized=false;
public:
    void reset()noexcept {initialized=false;item.clear();}
    pose observe(const ncmm_graphics_control_state_v1 &state,double now,std::string_view held,uint64_t world){
        if(!initialized||world!=epoch||now<previous){
            item.assign(held);epoch=world;blocked_serial=state.attack_serial;equip_started=now-1;initialized=true;
        }else if(std::string_view(item)!=held){
            item.assign(held);equip_started=now;
            // A new serial on this same frame is a confirmed action with the
            // new item. A repeated old serial belongs to the previous item.
            if(state.attack_serial==last_serial)blocked_serial=state.attack_serial;
        }
        previous=now;last_serial=state.attack_serial;
        auto confirmed=state;if(confirmed.attack_serial==blocked_serial)confirmed.attack_serial=0;
        auto result=sample(confirmed,now);result.equip_phase=smooth(float((now-equip_started)/.24));return result;
    }
};
}
