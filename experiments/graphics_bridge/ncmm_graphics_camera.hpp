#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
namespace ncmm_graphics_camera {
constexpr float pi=3.14159265358979323846f;
struct offset {float x=0,y=0,z=0;};
// Visual displacement from the native destination. The first successful
// presentation starts easing; map extraction/render latency cannot consume it.
class motion {
    offset start_{};
    uint64_t begun_=0;
    bool pending_=false;
public:
    void reset(){start_={};begun_=0;pending_=false;}
    bool begin(offset value,uint64_t ticks){
        if(!std::isfinite(value.x)||!std::isfinite(value.y)||!std::isfinite(value.z)||
           std::abs(value.x)>4||std::abs(value.y)>4||std::abs(value.z)>5){reset();return false;}
        start_=value;begun_=ticks;pending_=std::abs(value.x)+std::abs(value.y)+std::abs(value.z)>.002f;return true;
    }
    uint64_t elapsed(uint64_t ticks)const{return pending_||ticks<begun_?0:ticks-begun_;}
    offset sample(uint64_t ticks)const{
        const float duration=std::abs(start_.z)>.002f?180.f:140.f;
        const float t=std::clamp(float(elapsed(ticks))/duration,0.f,1.f),factor=1-t*t*(3-2*t);
        return {start_.x*factor,start_.y*factor,start_.z*factor};
    }
    bool active(uint64_t ticks)const{const auto v=sample(ticks);return std::abs(v.x)+std::abs(v.y)+std::abs(v.z)>.002f;}
    void presented(uint64_t ticks){if(pending_){begun_=ticks;pending_=false;}}
};
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
