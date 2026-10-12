#pragma once
#include <algorithm>
#include <cmath>
#include <utility>
namespace ncmm_graphics_camera {
constexpr float pi=3.14159265358979323846f;
inline std::pair<int,int> movement(float yaw,int forward,int right){
    if(!std::isfinite(yaw)||(!forward&&!right))return {0,0};
    const float angle=yaw+std::atan2(float(right),float(forward));
    const int sector=int(std::lround(angle/(pi/4)));
    constexpr int x[]={1,1,0,-1,-1,-1,0,1},y[]={0,1,1,1,0,-1,-1,-1};
    const int n=(sector%8+8)%8;return{x[n],y[n]};
}
inline bool in_front(float yaw,float dx,float dy){
    if(!std::isfinite(yaw)||!std::isfinite(dx)||!std::isfinite(dy))return false;
    const float forward=dx*std::cos(yaw)+dy*std::sin(yaw);
    const float side=-dx*std::sin(yaw)+dy*std::cos(yaw);
    // Conservative padding retains near-clipped walls and wide car mounts.
    return forward>=-1.5f&&std::abs(side)<=std::max(0.0f,forward)*.71f+2.0f;
}
}
