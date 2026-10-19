#include "ncmm_graphics_performance.hpp"
#include <iostream>
#include <limits>

int main(){
    using namespace ncmm_graphics_performance;
    unsigned checks=0;
    auto check=[&](bool ok,const char *message){++checks;if(!ok)std::cerr<<message<<'\n';return ok;};
    if(!check(!cycle_difference({7,false},{9,true}).available,"unavailable beginning became a measurement"))return 1;
    if(!check(!cycle_difference({7,true},{9,false}).available,"unavailable end became a measurement"))return 1;
    if(!check(!cycle_difference({9,true},{7,true}).available,"backward counter was clamped into a valid measurement"))return 1;
    const auto positive=cycle_difference({7,true},{19,true}),zero=cycle_difference({7,true},{7,true});
    if(!check(positive.available&&positive.count==12,"raw counter difference changed units"))return 1;
    if(!check(zero.available&&zero.count==0,"valid zero confused with unavailable counter"))return 1;
    const auto maximum=std::numeric_limits<uint64_t>::max();
    const auto boundary=cycle_difference({maximum-7,true},{maximum,true});
    if(!check(boundary.available&&boundary.count==7,"unsigned boundary corrupted the counter"))return 1;
    if(!check(!movement_cycles.active,"cycle collection enabled during ordinary runtime"))return 1;
    {
        movement_cycle_frame frame(false);
        movement_cycle_phase phase(movement_cycles.visibility);
        phase.stop();
        if(!check(movement_cycles.visibility==0&&movement_cycles.available,"inactive phase queried or invalidated a counter"))return 1;
    }
    {
        movement_cycle_frame frame(true);
        movement_cycle_phase phase(movement_cycles.visibility);
        volatile uint64_t work=1;for(unsigned i=0;i<4096;++i)work=(work*33)^i;
        phase.stop();const auto saved=movement_cycles.visibility;
        phase.stop();
        if(!check(movement_cycles.visibility==saved,"phase retirement counted the same work twice"))return 1;
#if defined(_WIN32) && defined(_MSC_VER)
        if(!check(thread_cycles().available&&movement_cycles.available&&saved>0,"Windows current-thread cycle counter unavailable"))return 1;
#else
        if(!check(!thread_cycles().available&&!movement_cycles.available&&saved==0,"unsupported counter was reported as observed CPU activity"))return 1;
#endif
    }
    if(!check(!movement_cycles.active,"collection escaped the benchmark frame"))return 1;
    {
        movement_cycle_frame frame(false);
        if(!check(movement_cycles.visibility==0&&movement_cycles.extraction==0&&movement_cycles.callback==0,"next frame inherited old phase counters"))return 1;
    }
    std::cout<<checks<<" thread-cycle checks passed; raw cycles are not elapsed milliseconds.\n";
    return 0;
}
