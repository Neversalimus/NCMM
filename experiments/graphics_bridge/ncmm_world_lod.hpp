#pragma once
#include "ncmm_graphics_multilevel.h"
#include <cmath>
#include <algorithm>
#include <cstring>
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
// These numbers describe the pinned standard radio_tower mapgen only. In
// particular, substring "tower", outlines and other radio layouts cannot
// select this profile. Rotating a 24 m site rotates cell centres about (12,12).
inline constexpr float radio_width=5.81f,radio_height=17.5f;
inline bool standard_radio_feature(const char *type,bool fully_disclosed,uint32_t rotation,
                                   const ncmm_lod_patch_v1 &p,float detail_side,uint32_t patch,
                                   ncmm_skyline_feature_v1 &out){
    if(!type||std::strcmp(type,"radio_tower")||!fully_disclosed||rotation>3||
       p.kind!=NCMM_LOD_BUILDING||p.width!=24||p.depth!=24||
       std::max(std::abs(p.x+12-detail_side*.5f),std::abs(p.y+12-detail_side*.5f))<=72)return false;
    constexpr float centers[4][2]={{13,6},{18,13},{11,18},{6,11}};
    out={p.x+centers[rotation][0],p.y+centers[rotation][1],p.z,
         radio_width,radio_width,radio_height,NCMM_SKYLINE_RADIO_STANDARD,rotation,patch,0};
    return true;
}
inline bool valid_skyline(const ncmm_graphics_skyline_scene_v1 &s){
    if(s.struct_size<sizeof(s)||s.version!=1||s.reserved||s.count>NCMM_SKYLINE_MAX_FEATURES||
       (s.count&&!s.features)||!valid_lod(s.geography)||(!s.geography.enabled&&s.count))return false;
    // One feature per site prevents duplicate silhouettes and bounds all work.
    bool used[NCMM_LOD_MAX_PATCHES]{};
    constexpr float centers[4][2]={{13,6},{18,13},{11,18},{6,11}};
    for(uint32_t i=0;i<s.count;++i){const auto &f=s.features[i];
        for(float v:{f.x,f.y,f.z,f.width,f.depth,f.height})if(!std::isfinite(v))return false;
        if(f.kind!=NCMM_SKYLINE_RADIO_STANDARD||f.rotation>3||f.reserved||
           f.ground_patch>=s.geography.count||used[f.ground_patch]||
           f.width!=radio_width||f.depth!=radio_width||f.height!=radio_height||
           std::abs(f.x)>512||std::abs(f.y)>512||std::abs(f.z)>320||f.z+f.height>320)return false;
        used[f.ground_patch]=true;const auto &p=s.geography.patches[f.ground_patch];
        if(p.kind!=NCMM_LOD_BUILDING||p.width!=24||p.depth!=24||p.height!=0||
           f.x!=p.x+centers[f.rotation][0]||f.y!=p.y+centers[f.rotation][1]||f.z!=p.z)return false;
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
