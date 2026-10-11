#pragma once
#include "ncmm_graphics_controls.h"
#include <algorithm>
#include <cmath>
namespace first_person::animation {
struct pose {float swing=0,thrust=0,recoil=0,sway_x=0,sway_y=0;uint32_t grip=NCMM_GRIP_SINGLE;float length=.6f;};
inline float smooth(float x){x=std::clamp(x,0.0f,1.0f);return x*x*(3-2*x);}
inline pose sample(const ncmm_graphics_control_state_v1 &s,double now){
    pose p;p.grip=s.grip_kind;p.length=std::clamp(s.held_length,.15f,2.7f);
    const double age=now-s.attack_started;
    if(!s.attack_serial||!std::isfinite(age)||age<0||age>=.48)return p;
    const float t=float(age/.48),strike=t<.24f?smooth(t/.24f):1-smooth((t-.24f)/.76f);
    if(s.attack_kind==2)p.recoil=strike*.065f;
    else if(p.grip==NCMM_GRIP_POLE)p.thrust=strike*.30f;
    else p.swing=strike*.95f;
    return p;
}
}
