#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

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
inline unsigned tufts(unsigned lod){return lod==0?64u:lod==1?16u:lod==2?4u:0u;}
inline unsigned shade(unsigned lod,unsigned source){return lod>=2?1u:lod==1?std::min(source,1u):source;}
struct layout {unsigned mask=0,turns=0,variant=0;};
inline layout patch(unsigned mask,unsigned lod,uint32_t seed){
    // Edge taper is subpixel at the outer LODs. Rotate one canonical near
    // fringe instead of multiplying all four patterns by sixteen masks.
    if(lod)return {0,lod==2?seed&3u:0,lod==2?0u:seed&3u};
    if(!mask)return {0,0,seed&3u};
    layout result{mask,0,0};unsigned rotated=mask;
    for(unsigned turn=1;turn<4;++turn){rotated=((rotated<<1)|(rotated>>3))&15u;
        if(rotated<result.mask)result={rotated,(4-turn)&3u,0};}
    return result;
}
struct xy {float x,y;};
inline xy turn(xy p,unsigned n){switch(n&3u){case 1:return {-p.y,p.x};case 2:return {-p.x,-p.y};case 3:return {p.y,-p.x};default:return p;}}
inline float edge_scale(unsigned mask,float x,float y){
    float distance=1;
    if(mask&1)distance=std::min(distance,1-x);
    if(mask&2)distance=std::min(distance,1-y);
    if(mask&4)distance=std::min(distance,x);
    if(mask&8)distance=std::min(distance,y);
    const float t=std::clamp(distance/.28f,0.f,1.f);return .06f+.94f*t*t*(3-2*t);
}
inline float height(const char *id){
    return std::strstr(id,"golf")||std::strstr(id,"grazed")?.055f:
           std::strstr(id,"tall")?.65f:std::strstr(id,"long")?.44f:.30f;
}
inline unsigned tint(const char *id){return std::strstr(id,"dead")?1u:std::strstr(id,"white")?2u:std::strstr(id,"alien")?3u:0u;}
struct colour {float r,g,b;};
inline colour color(unsigned tint,unsigned shade){
    constexpr colour palette[]={{.29f,.43f,.12f},{.55f,.46f,.24f},{.48f,.55f,.38f},{.22f,.37f,.28f}};
    const auto c=palette[tint%4];const float k=shade==0?.78f:shade==1?1.f:1.22f;return {c.r*k,c.g*k,c.b*k};
}
// Four reusable, tile-local patterns. Two broad tapered triangles per tuft
// cover more ground than the old four narrow segmented shoots. Three fixed
// shades permit real GPU instancing instead of a material/draw per blade.
struct blade {float x,y,wx,wy,tx,ty,h,nx,ny;unsigned shade;};
inline const std::vector<blade> &pattern(unsigned variant,unsigned lod){
    static const auto patterns=[](){std::array<std::array<std::vector<blade>,3>,4> result;
        for(unsigned v=0;v<4;++v)for(unsigned l=0;l<3;++l){auto &out=result[v][l];out.reserve(tufts(l)*2);
            for(unsigned t=0;t<tufts(l);++t){const uint32_t k=v*7919u+t*127u;
                // Nested, interleaved subsets spread every LOD across the tile.
                const unsigned sx=(((t&1)<<2)|((t&4)>>1)|((t&16)>>4))^1u;
                const unsigned sy=(((t&2)<<1)|((t&8)>>2)|((t&32)>>5))^1u;
                const float x=.10f+sx*(.8f/7)+(random(k+1)-.5f)*.035f,y=.10f+sy*(.8f/7)+(random(k+2)-.5f)*.035f;
                const float angle=random(k+3)*6.283185307f;
                for(unsigned b=0;b<2;++b){const float a=angle+b*1.65f,c=std::cos(a),s=std::sin(a);
                    const float width=(.032f+random(k+b+29)*.020f)*(l==0?1.f:l==1?1.6f:2.5f);
                    const float lean=.035f+random(k+b+37)*.08f;
                    const float root_x=std::clamp(x+(random(k+b+41)-.5f)*.035f,.14f,.86f);
                    const float root_y=std::clamp(y+(random(k+b+43)-.5f)*.035f,.14f,.86f);
                    out.push_back({root_x,root_y,c*width,s*width,std::clamp(root_x-s*lean,.09f,.91f),std::clamp(root_y+c*lean,.09f,.91f),
                        .70f+random(k+b+7)*.65f,-s,c,hash(k+b+19)%3});}
            }}return result;}();
    return patterns[variant%4][std::min(lod,2u)];
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
