#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace first_person::liquid_surface {
// Neighbour order: W, E, N, S, NW, NE, SW, SE. A missing/hidden cell is
// unknown water extent, not a beach. Distances meet across shared tile edges.
inline float bank_distance(uint32_t banks,float u,float v){
    float d=2;
    if(banks&1)d=std::min(d,u);
    if(banks&2)d=std::min(d,1-u);
    if(banks&4)d=std::min(d,v);
    if(banks&8)d=std::min(d,1-v);
    if(banks&16)d=std::min(d,std::sqrt(u*u+v*v));
    if(banks&32)d=std::min(d,std::sqrt((1-u)*(1-u)+v*v));
    if(banks&64)d=std::min(d,std::sqrt(u*u+(1-v)*(1-v)));
    if(banks&128)d=std::min(d,std::sqrt((1-u)*(1-u)+(1-v)*(1-v)));
    return std::max(0.0f,d);
}
inline float spill_coverage(float squared_radius,float squared_distance,float pixel_footprint){
    return std::clamp((squared_radius-squared_distance)/std::max(.001f,pixel_footprint*.6f),0.0f,1.0f);
}
}
