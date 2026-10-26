#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>

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
struct point {float x=0,y=0;};
// W,E,N,S,NW,NE,SW,SE. A separable occupancy filter gives identical values
// on both sides of a shared edge, including diagonals and concave corners.
inline float spill_density(uint32_t neighbors,float u,float v){
    constexpr std::array<std::array<int,2>,8> offsets{{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}}};
    auto weight=[](float d){const float t=std::clamp(1-std::abs(d),0.f,1.f);return t*t*(3-2*t);};
    float value=weight(u-.5f)*weight(v-.5f);
    for(size_t n=0;n<offsets.size();++n)if(neighbors&(1u<<n))value+=weight(u-.5f-offsets[n][0])*weight(v-.5f-offsets[n][1]);
    return value;
}
template<class Emit> void spill_mesh(uint32_t neighbors,float world_x,float world_y,Emit emit){
    if((neighbors&255u)==255u){emit(point{0,0},point{1,0},point{1,1});emit(point{0,0},point{1,1},point{0,1});return;}
    constexpr int grid=6;std::array<float,(grid+1)*(grid+1)> values{};
    for(int y=0;y<=grid;++y)for(int x=0;x<=grid;++x){const float u=float(x)/grid,v=float(y)/grid;
        // World-space variation meets exactly at shared edges. No per-tile
        // random radius or fragment discard can reopen a seam in a pool.
        const float wobble=.025f*std::sin((world_x+u)*7.3f+(world_y+v)*4.7f)*std::sin((world_y+v)*9.1f-(world_x+u)*3.2f);
        values[size_t(y)*(grid+1)+x]=spill_density(neighbors,u,v)-(.54f+wobble);}
    auto triangle=[&](std::array<point,3> p,std::array<float,3> d){
        std::array<point,4> clipped{};size_t count=0;
        for(size_t n=0;n<3;++n){const size_t j=(n+1)%3;if(d[n]>=0)clipped[count++]=p[n];
            if((d[n]>=0)!=(d[j]>=0)){const float t=d[n]/(d[n]-d[j]);clipped[count++]={p[n].x+(p[j].x-p[n].x)*t,p[n].y+(p[j].y-p[n].y)*t};}}
        for(size_t n=1;n+1<count;++n)emit(clipped[0],clipped[n],clipped[n+1]);
    };
    for(int y=0;y<grid;++y)for(int x=0;x<grid;++x){const size_t at=size_t(y)*(grid+1)+x;
        const point a{float(x)/grid,float(y)/grid},b{float(x+1)/grid,float(y)/grid},c{float(x+1)/grid,float(y+1)/grid},d{float(x)/grid,float(y+1)/grid};
        triangle({a,b,c},{values[at],values[at+1],values[at+grid+2]});triangle({a,c,d},{values[at],values[at+grid+2],values[at+grid+1]});}
}
}
