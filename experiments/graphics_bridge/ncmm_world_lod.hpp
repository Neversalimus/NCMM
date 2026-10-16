#pragma once
#include "ncmm_graphics_multilevel.h"
#include <cmath>
#include <algorithm>
namespace ncmm_world {
inline bool valid_lod(const ncmm_graphics_lod_scene_v1 &s){
    if(s.struct_size<sizeof(s)||s.version!=1||s.enabled>1||s.count>NCMM_LOD_MAX_PATCHES||
       (s.count&&!s.patches)||!std::isfinite(s.ground_z)||std::abs(s.ground_z)>320||s.reserved)return false;
    if(!s.enabled&&s.count)return false;
    for(uint32_t i=0;i<s.count;++i){const auto &p=s.patches[i];
        for(float v:{p.x,p.y,p.z,p.width,p.depth,p.height,p.color[0],p.color[1],p.color[2]})if(!std::isfinite(v))return false;
        if(std::abs(p.x)>512||std::abs(p.y)>512||std::abs(p.z)>320||p.width<=0||p.depth<=0||p.width>48||p.depth>48||
           p.height<0||p.height>40||p.kind>NCMM_LOD_BUILDING)return false;
        for(float c:p.color)if(c<0||c>1)return false;
    }
    return true;
}
// Subtract the detailed square; the resulting disjoint strips cannot cover a
// true stair/shaft inside it, nor z-fight with the detailed ground at its edge.
template<class Emit> void outside_detail(float x,float y,float w,float h,float side,Emit emit){
    const float x1=x+w,y1=y+h,l=std::max(0.f,x),r=std::min(side,x1),t=std::max(0.f,y),b=std::min(side,y1);
    if(l>=r||t>=b){emit(x,y,w,h);return;}
    if(y<t)emit(x,y,w,t-y);
    if(b<y1)emit(x,b,w,y1-b);
    if(x<l)emit(x,t,l-x,b-t);
    if(r<x1)emit(r,t,x1-r,b-t);
}
}
