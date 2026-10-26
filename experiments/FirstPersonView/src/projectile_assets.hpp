#pragma once
#include "ncmm_graphics_projectiles.h"
#include "weapon_view.hpp"
#include <array>
#include <cstring>
#include <vector>
namespace first_person::projectile_assets {
using point=viewmodel::point;
struct flight {uint64_t serial=0;uint32_t kind=0,count=0;double started=0;std::array<point,NCMM_PROJECTILE_POINTS> path{};float length=0;};
inline float length(point p){return std::sqrt(viewmodel::dot(p,p));}
inline float duration(const flight &f){
    if(f.kind==NCMM_PROJECTILE_LASER)return .085f;
    const float speed=f.kind==NCMM_PROJECTILE_ARROW||f.kind==NCMM_PROJECTILE_BOLT?45.f:f.kind==NCMM_PROJECTILE_THROWN?20.f:f.kind==NCMM_PROJECTILE_FLAME?35.f:220.f;
    // Readable visual flight only; the Native shot has already been resolved.
    return std::clamp(f.length/speed,.075f,1.4f);
}
inline bool valid(const ncmm_graphics_projectile_v1 &p){
    if(!p.serial||p.count<2||p.count>NCMM_PROJECTILE_POINTS||p.kind<NCMM_PROJECTILE_BULLET||p.kind>NCMM_PROJECTILE_THROWN||
       !std::isfinite(p.seconds)||!std::memchr(p.weapon,0,sizeof(p.weapon)))return false;
    for(uint32_t n=1;n<p.count;++n){const auto &a=p.path[n-1],&b=p.path[n];
        if(std::abs(int64_t(a.x)-b.x)>1||std::abs(int64_t(a.y)-b.y)>1||std::abs(int64_t(a.z)-b.z)>1)return false;}
    return true;
}
inline bool valid(const flight &f){
    if(!f.serial||f.kind<1||f.kind>NCMM_PROJECTILE_THROWN||f.count<2||f.count>NCMM_PROJECTILE_POINTS||
       !std::isfinite(f.started)||!std::isfinite(f.length)||f.length<=0||f.length>256)return false;
    for(uint32_t n=0;n<f.count;++n)for(float v:{f.path[n].x,f.path[n].y,f.path[n].z})if(!std::isfinite(v)||std::abs(v)>100000)return false;
    return true;
}
class mounts {
    struct entry {uint64_t serial=0,epoch=0;point muzzle{};int32_t x=0,y=0,z=0;};
    std::array<entry,NCMM_PROJECTILE_LIMIT> entries{};size_t cursor=0;
public:
    void reset(){entries={};cursor=0;}
    flight prepare(const ncmm_graphics_projectile_v1 &p,int32_t x,int32_t y,int32_t z,float level,point muzzle,const char *weapon){
        flight f;if(!valid(p)||!std::isfinite(level)||level<=0)return f;
        entry *mount=nullptr;for(auto &e:entries)if(e.serial==p.serial&&e.epoch==p.world_epoch){mount=&e;break;}
        if(!mount){mount=&entries[cursor++%entries.size()];
            const auto &source=p.path[0];const point start{float(int64_t(source.x)-x)+.5f,float(int64_t(source.y)-y)+.5f,float(int64_t(source.z)-z)*level+1.25f};
            *mount={p.serial,p.world_epoch,weapon&&std::strcmp(weapon,p.weapon)==0?muzzle:start,x,y,z};}
        f.serial=p.serial;f.kind=p.kind;f.count=p.count;f.started=p.seconds;
        f.path[0]=mount->muzzle+point{float(int64_t(mount->x)-x),float(int64_t(mount->y)-y),float(int64_t(mount->z)-z)*level};
        for(uint32_t n=1;n<p.count;++n){const auto &at=p.path[n];f.path[n]={float(int64_t(at.x)-x)+.5f,float(int64_t(at.y)-y)+.5f,float(int64_t(at.z)-z)*level+1.25f};f.length+=length(f.path[n]-f.path[n-1]);}
        if(!std::isfinite(f.length)||f.length<=.001f||f.length>256)f.serial=0;return f;
    }
};
inline point along(const flight &f,float distance){
    for(uint32_t n=1;n<f.count;++n){const float segment=length(f.path[n]-f.path[n-1]);if(distance<=segment)return f.path[n-1]+(f.path[n]-f.path[n-1])*(segment>.00001f?std::clamp(distance/segment,0.f,1.f):0.f);distance-=segment;}
    return f.path[f.count-1];
}
// Authored code-native assets: a short bullet streak, pellet, shaft + tip +
// fletching for arrows/bolts, a laser pulse, plasma packet and flame tongue.
// Emit world geometry, so every asset shares the actual scene depth buffer.
template<class Emit> void draw(const flight &f,double now,Emit emit,float minimum_width=0){
    if(!f.serial||f.count<2||f.count>NCMM_PROJECTILE_POINTS)return;
    const float age=float(now-f.started),time=duration(f);if(age<0||age>=time)return;
    const float head=std::min(f.length,f.length*age/time);
    float span=f.kind==NCMM_PROJECTILE_ARROW?.65f:f.kind==NCMM_PROJECTILE_BOLT?.36f:f.kind==NCMM_PROJECTILE_PELLET?.32f:f.kind==NCMM_PROJECTILE_LASER?f.length:1.25f;
    const auto tip=along(f,head),tail=along(f,std::max(0.f,head-span));const auto delta=tip-tail;const float d=length(delta);if(d<.002f)return;
    const auto forward=delta*(1/d);point right=viewmodel::cross(forward,{0,0,1});if(length(right)<.001f)right={1,0,0};else right=right*(1/length(right));
    const auto up=viewmodel::cross(right,forward);
    auto ribbon=[&](point a,point b,float width,point color,float minimum_scale=1){width=std::max(width,minimum_width*minimum_scale);emit(a-right*width,b-right*width,b+right*width,a+right*width,color);emit(a-up*width,b-up*width,b+up*width,a+up*width,color);};
    if(f.kind==NCMM_PROJECTILE_ARROW||f.kind==NCMM_PROJECTILE_BOLT){ribbon(tail,tip,.009f,{.48f,.31f,.16f});
        ribbon(tip-forward*std::min(.09f,d*.25f),tip,.024f,{.75f,.77f,.79f});
        ribbon(tail,tail+forward*std::min(.15f,d*.3f),.032f,{.68f,.67f,.54f});return;}
    const bool laser=f.kind==NCMM_PROJECTILE_LASER,plasma=f.kind==NCMM_PROJECTILE_PLASMA,flame=f.kind==NCMM_PROJECTILE_FLAME;
    const point color=laser?point{2.5f,.06f,.025f}:plasma?point{.08f,1.8f,2.5f}:flame?point{2.4f,.66f,.04f}:f.kind==NCMM_PROJECTILE_THROWN?point{.5f,.48f,.43f}:point{1.9f,1.15f,.35f};
    const float width=plasma?.045f:flame?.08f:f.kind==NCMM_PROJECTILE_THROWN?.035f:f.kind==NCMM_PROJECTILE_PELLET?.008f:.014f;
    ribbon(tail,tip,width,color);if(!flame)ribbon(tail,tip,width*.3f,{2.4f,2.2f,1.7f},.25f);
}
}
