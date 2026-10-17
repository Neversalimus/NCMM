// Isolated source/geometry-cache shift timing, not a native frame/FPS benchmark.
// MSVC: cl /O2 /EHsc /std:c++17 /Iexperiments/graphics_bridge experiments/FirstPersonView/tests/cache_shift_benchmark.cpp
// GCC/Clang: c++ -O2 -std=c++17 -Iexperiments/graphics_bridge experiments/FirstPersonView/tests/cache_shift_benchmark.cpp
#include "ncmm_world_cache.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
int main(){ncmm_world::source_cache source;ncmm_world::geometry_cache geometry,roofs;int x=0,y=0;uint64_t checksum=0;std::vector<double> times;
 for(int step=0;step<1001;++step){int dx=step%4==0?1:step%4==2?-1:0,dy=step%4==1?1:step%4==3?-1:0;x+=dx;y+=dy;auto begin=std::chrono::steady_clock::now();
  source.prepare(x,y,-10,10,97,97,1);geometry.prepare(x,y,-10,10,97,97,1);roofs.prepare(x,y,0,0,97,97,1);
  for(int i=0;i<97;++i){int px=x+i,py=y+(i*7)%97;checksum+=source.get(px,py,0,1,[&]{return uint32_t(px+py);});checksum+=geometry.get(px,py,0,1,[&]{return uint32_t(px+py);});}
  double time=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();if(step)times.push_back(time);
 }
 std::sort(times.begin(),times.end());printf("median_ms=%.6f p95_ms=%.6f checksum=%llu bytes=%zu\n",times[500],times[950],(unsigned long long)checksum,source.bytes()+geometry.bytes()+roofs.bytes());
}
