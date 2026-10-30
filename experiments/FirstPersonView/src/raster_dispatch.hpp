#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace first_person::hybrid {
// Match the painter's interleaved sixteen-row strips. This schedules work only;
// it cannot alter triangle coverage, scanline arithmetic or submission order.
inline unsigned raster_worker_mask(int low,int high,int workers){
    const int first=low/16,last=high/16;
    if(last-first+1>=workers)return (1u<<workers)-1;
    unsigned mask=0;for(int strip=first;strip<=last;++strip)mask|=1u<<(strip%workers);
    return mask;
}
// Exact sample existence for a tiny projected triangle. Use the painter's
// original float intersections and inclusive edge tolerances, never an area or
// distance cutoff. A false result proves that no scanline can write a pixel.
template<class Point>bool triangle_has_scanline_sample(const std::array<Point,3> &p,int xmin,int xmax,int ymin,int ymax,int width){
    for(int y=ymin;y<=ymax;++y){
        float left=std::numeric_limits<float>::infinity(),right=-left;
        const float py=y+.5f;
        for(int e=0;e<3;++e){const auto &a=p[e],&b=p[(e+1)%3];
            if(py<std::min(a.y,b.y)||py>std::max(a.y,b.y)||std::abs(a.y-b.y)<.00001f)continue;
            const float x=a.x+(py-a.y)*(b.x-a.x)/(b.y-a.y);left=std::min(left,x);right=std::max(right,x);}
        if(!std::isfinite(left))continue;
        const int begin=std::max(xmin,int(std::ceil(std::clamp(left-.5001f,-1.0f,float(width)))));
        const int end=std::min(xmax,int(std::floor(std::clamp(right-.4999f,-1.0f,float(width)))));
        if(begin<=end)return true;
    }
    return false;
}
}
