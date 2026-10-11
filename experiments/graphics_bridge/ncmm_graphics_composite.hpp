#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace ncmm_graphics_composite {
// Same straight-alpha layer contract for accelerated SDL and the CPU path.
inline bool blend(std::vector<uint32_t> &dst,uint32_t w,uint32_t h,
                  const std::vector<uint32_t> &src,uint32_t sw,uint32_t sh){
    if(!w||!h||!sw||!sh||dst.size()!=size_t(w)*h||src.size()!=size_t(sw)*sh)return false;
    for(uint32_t y=0;y<h;++y){const float py=(y+.5f)*sh/h-.5f;const int iy=int(std::floor(py));const float fy=py-iy;
        const int y0=std::clamp(iy,0,int(sh)-1),y1=std::clamp(iy+1,0,int(sh)-1);
        for(uint32_t x=0;x<w;++x){const float px=(x+.5f)*sw/w-.5f;const int ix=int(std::floor(px));const float fx=px-ix;
            const int x0=std::clamp(ix,0,int(sw)-1),x1=std::clamp(ix+1,0,int(sw)-1);
            const uint32_t a=src[size_t(y0)*sw+x0],b=src[size_t(y0)*sw+x1],c=src[size_t(y1)*sw+x0],d=src[size_t(y1)*sw+x1];
            if(!((a|b|c|d)&255u))continue;
            auto channel=[&](int shift){return (((a>>shift)&255)*(1-fx)+((b>>shift)&255)*fx)*(1-fy)+(((c>>shift)&255)*(1-fx)+((d>>shift)&255)*fx)*fy;};
            const float alpha=channel(0)/255;uint32_t &out=dst[size_t(y)*w+x];uint32_t result=255;
            for(int shift:{8,16,24})result|=uint32_t(std::clamp(channel(shift)*alpha+((out>>shift)&255)*(1-alpha),0.0f,255.0f))<<shift;
            out=result;
        }
    }
    return true;
}
}
