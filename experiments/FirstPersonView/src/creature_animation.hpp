#pragma once
#include "creature_mesh.hpp"
namespace first_person::creature {
struct transform {
    point x{1,0,0},y{0,1,0},z{0,0,1},position{};
    point vector(point p)const{return x*p.x+y*p.y+z*p.z;}
    point apply(point p)const{return position+vector(p);}
};
inline transform rotation(point a){
    const float cx=std::cos(a.x),sx=std::sin(a.x),cy=std::cos(a.y),sy=std::sin(a.y),cz=std::cos(a.z),sz=std::sin(a.z);
    return {{cz*cy,sz*cy,-sy},{cz*sy*sx-sz*cx,sz*sy*sx+cz*cx,cy*sx},{cz*sy*cx+sz*sx,sz*sy*cx-cz*sx,cy*cx},{}};
}
inline transform compose(const transform &a,const transform &b){return {a.vector(b.x),a.vector(b.y),a.vector(b.z),a.apply(b.position)};}
inline float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
inline float angle_delta(float a,float b){return std::remainder(b-a,2*pi);}
struct instance {
    uint64_t id=0,event_serial=0;point previous{},target{};
    double move_at=-100,last_seen=0,event_at=-100,turn_at=-100;
    float from_heading=0,heading=0,step=0,gait_base=0;uint32_t event=NCMM_CR_IDLE,flags=0;int lod=-1;
};
inline point display_position(const instance &a,double now){return a.previous+(a.target-a.previous)*ease(float((now-a.move_at)/.18));}
inline float display_heading(const instance &a,double now){return a.from_heading+angle_delta(a.from_heading,a.heading)*ease(float((now-a.turn_at)/.15));}
inline void observe(instance &a,const ncmm_graphics_creature_v1 &s,double now){
    point target{float(s.world_x),float(s.world_y),float(s.world_z)};
    if(a.id!=s.entity_id||now<a.last_seen||now-a.last_seen>1){
        a={};a.id=s.entity_id;a.previous=a.target=target;a.heading=a.from_heading=std::isfinite(s.heading)?s.heading:0;
    }
    const point delta=target-a.target;const float distance=std::sqrt(dot(delta,delta));
    if(distance>0){
        a.previous=display_position(a,now);a.target=target;
        // Teleports, stairs and fresh visibility snap to the authoritative place.
        // They do not animate an invented route through hidden cells or walls.
        if(distance>1.5f||delta.z!=0){a.previous=a.target;a.step=0;}
        else{a.gait_base+=std::min(float(std::max(0.0,now-a.move_at))/.18f,1.8f)*pi*a.step;a.step=distance;}
        a.move_at=now;
        if(delta.x||delta.y){a.from_heading=display_heading(a,now);a.heading=std::atan2(delta.y,delta.x);a.turn_at=now;}
    }
    if(s.event_serial!=a.event_serial){a.event_serial=s.event_serial;a.event=s.event;a.event_at=s.event_seconds;
        if((s.event==NCMM_CR_MELEE||s.event==NCMM_CR_SHOOT)&&std::isfinite(s.heading)){
            a.from_heading=display_heading(a,now);a.heading=s.heading;a.turn_at=now;}
    }
    a.flags=s.flags;a.last_seen=now;
}
struct pose_state {float movement=0,phase=0,breath=0,melee=0,shoot=0,hurt=0,death=0,heading=0;point offset{};bool prone=false;};
inline pose_state sample(const instance &a,double now){
    pose_state p;const float since=float(std::max(0.0,now-a.move_at)),action=float(now-a.event_at);
    p.movement=(1-ease((since-.15f)/.20f))*std::min(1.f,a.step);
    p.phase=a.gait_base+std::min(since/.18f,1.8f)*pi*a.step;
    // Only flight/swim/body breathing runs independently of a gameplay move.
    p.breath=std::sin(float(std::fmod(now,10000.0))*2.1f+float(a.id%31));
    if(action>=0){
        if(a.event==NCMM_CR_MELEE&&action<.40f)p.melee=std::sin(pi*action/.40f);
        if(a.event==NCMM_CR_SHOOT&&action<.28f)p.shoot=std::sin(pi*action/.28f);
        if(a.event==NCMM_CR_HURT&&action<.30f)p.hurt=std::sin(pi*action/.30f);
        if(a.event==NCMM_CR_DIE)p.death=ease(action/.42f);
    }
    if(a.flags&NCMM_CR_DEAD)p.death=std::max(p.death,.01f);
    p.heading=display_heading(a,now);p.offset=display_position(a,now)-a.target;p.prone=(a.flags&NCMM_CR_PRONE)!=0;return p;
}
struct skeleton_pose {std::array<transform,128> joints{};size_t count=0;};
inline skeleton_pose animate(const model &m,const pose_state &p,double seconds,bool simplified=false){
    skeleton_pose out;out.count=std::min(m.bones.size(),out.joints.size());
    const float h=m.type.height;const bool airborne=m.type.flags&NCMM_CR_FLIES,swims=m.type.kind==anatomy::fish;
    const float time=float(std::fmod(seconds,10000.0));
    for(size_t i=0;i<out.count;++i){const auto &b=m.bones[i];point angles{},translation=b.origin;
        const float cycle=p.phase+b.phase,swing=std::sin(cycle)*p.movement;
        switch(b.role){
            case motion::root:angles.z=p.heading;translation=translation+p.offset;break;
            case motion::body:
                translation.z+=h*(p.breath*.003f+std::abs(std::sin(p.phase))*p.movement*.018f);
                angles.y=p.hurt*.12f+p.melee*.06f+p.death*pi*.48f;
                translation.z-=h*.39f*p.death;
                if(p.prone){angles.y+=pi*.40f;translation.z-=h*.32f;}
                translation.x-=h*.035f*p.shoot;
                if(airborne)translation.z+=h*.025f*std::sin(time*4);
                break;
            case motion::head:angles.z=simplified?0:p.breath*.025f;angles.y=-p.hurt*.10f;break;
            case motion::thigh:angles.y=swing*.55f;break;
            case motion::shin:angles.y=-std::max(0.f,-swing)*.75f;break;
            case motion::arm:angles.y=-swing*.35f-p.melee*1.05f-p.shoot*1.25f;angles.z=p.melee*.16f;break;
            case motion::forearm:angles.y=-.06f-p.melee*.60f-p.shoot*.45f;break;
            case motion::wing:angles.x=(b.phase==0?1.f:-1.f)*(airborne?std::sin(time*(m.type.kind==anatomy::insect?22.f:7.f))*.65f:swims?std::sin(time*4)*.15f:.10f);break;
            case motion::rotor:angles.z=time*24+b.phase;break;
            case motion::tail:angles.z=std::sin((swims?time*5:p.phase)+b.phase)*(.05f+p.movement*.18f+(swims?.18f:0));break;
            case motion::segment:angles.z=std::sin(p.phase+b.phase)*p.movement*(m.type.kind==anatomy::serpent?.12f:.035f);break;
            case motion::tentacle:angles.y=std::sin(p.phase+b.phase)*(.025f+p.movement*.18f);angles.z=std::cos(p.phase+b.phase)*p.movement*.10f;break;
            case motion::fixed:break;
        }
        // Never re-evaluate parent matrices per vertex. The hierarchy is built
        // in parent-before-child order and joint surfaces overlap at rest/bend.
        transform local=rotation(angles);local.position=translation;
        out.joints[i]=b.parent<0?local:compose(out.joints[size_t(b.parent)],local);
    }
    return out;
}
}
