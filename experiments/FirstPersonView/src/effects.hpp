#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <system_error>
#include <thread>
#include <vector>

namespace first_person::effects {
// Every worker owns complete rows. Inputs are copied/analysed before dispatch;
// no worker calls the Host, SDL or a sprite provider.
template<class F> void rows(uint32_t height,size_t work,F run){
    const int count=work>=16000?int(std::clamp(std::thread::hardware_concurrency(),1u,4u)):1;
    std::array<std::thread,3> threads;
    auto band=[&](int n){run(height*n/count,height*(n+1)/count);};
    for(int n=1;n<count;++n){try{threads[n-1]=std::thread(band,n);}catch(const std::system_error &){band(n);}}
    band(0);for(auto &t:threads)if(t.joinable())t.join();
}

// Fast module-side reconstruction. Keep the Host's small reference compositor
// for exceptional device recovery; tests compare every output byte against it.
inline bool blend(std::vector<uint32_t> &dst,uint32_t w,uint32_t h,
                  const std::vector<uint32_t> &src,uint32_t sw,uint32_t sh){
    if(!w||!h||!sw||!sh||dst.size()!=size_t(w)*h||src.size()!=size_t(sw)*sh)return false;
    struct column{int a,b;float weight;};std::vector<column> columns(w);
    for(uint32_t x=0;x<w;++x){const float px=(x+.5f)*sw/w-.5f;const int ix=int(std::floor(px));
        columns[x]={std::clamp(ix,0,int(sw)-1),std::clamp(ix+1,0,int(sw)-1),px-ix};}
    rows(h,size_t(w)*h,[&](uint32_t begin,uint32_t end){
        for(uint32_t y=begin;y<end;++y){const float py=(y+.5f)*sh/h-.5f;const int iy=int(std::floor(py));const float fy=py-iy;
            const auto *row0=src.data()+size_t(std::clamp(iy,0,int(sh)-1))*sw;
            const auto *row1=src.data()+size_t(std::clamp(iy+1,0,int(sh)-1))*sw;
            auto *row=dst.data()+size_t(y)*w;
            for(uint32_t x=0;x<w;++x){const auto &q=columns[x];const float fx=q.weight;
                const uint32_t a=row0[q.a],b=row0[q.b],c=row1[q.a],d=row1[q.b];
                if(!((a|b|c|d)&255u))continue;
                auto channel=[&](int shift){return (((a>>shift)&255)*(1-fx)+((b>>shift)&255)*fx)*(1-fy)+(((c>>shift)&255)*(1-fx)+((d>>shift)&255)*fx)*fy;};
                const float alpha=channel(0)/255;const uint32_t old=row[x];
                auto mix=[&](int shift){return uint32_t(std::clamp(channel(shift)*alpha+((old>>shift)&255)*(1-alpha),0.0f,255.0f));};
                row[x]=(mix(24)<<24)|(mix(16)<<16)|(mix(8)<<8)|255u;
            }
        }
    });return true;
}
}
