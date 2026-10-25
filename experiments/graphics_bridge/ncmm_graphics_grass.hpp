#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ncmm_grass {
// Native lawn variants only. A grass curtain, moss wall, indoor plant or the
// ground alias underneath pavement must never grow a second lawn.
inline bool terrain(const char *id) {
    return id&&std::strncmp(id,"t_grass",7)==0&&(id[7]=='\0'||id[7]=='_');
}
inline uint32_t hash(uint32_t n){n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);}
inline uint32_t seed(int32_t x,int32_t y,int32_t z){return hash(uint32_t(x)*374761393u^uint32_t(y)*668265263u^uint32_t(z)*2246822519u);}
inline float random(uint32_t n){return float(hash(n)&0xffffffu)/16777216.f;}
inline unsigned band(float distance){return distance<5?0u:distance<12?1u:distance<22?2u:3u;}
inline unsigned tufts(unsigned lod){return lod==0?16u:lod==1?4u:lod==2?1u:0u;}
inline float height(const char *id){
    return std::strstr(id,"golf")||std::strstr(id,"grazed")?.055f:
           std::strstr(id,"tall")?.65f:std::strstr(id,"long")?.38f:.19f;
}
struct bend {float x=0,y=0;};
inline bend wind(uint32_t key,double seconds,float wx,float wy){
    const float speed=std::sqrt(wx*wx+wy*wy),phase=random(key)*6.283185307f;
    const float time=float(std::fmod(seconds,100000.0));
    const float gust=std::sin(time*(1.2f+std::min(speed,30.f)*.025f)+phase)*.65f+
                     std::sin(time*2.31f+phase*1.7f)*.35f;
    const float amplitude=.008f+std::min(speed,30.f)*.0022f;
    const float dx=speed>.01f?wx/speed:.8f,dy=speed>.01f?wy/speed:.6f;
    return {dx*amplitude*gust,dy*amplitude*gust};
}
}
