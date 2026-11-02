#pragma once
#include <cstdint>
#include <cmath>
namespace ncmm_gpu {
// The caller invalidates on every backing-buffer lifetime change or write.
// Only a successfully submitted constant-one prefix may be reused.
class constant_shadow_cache {
    uint32_t initialized_prefix_=0;
public:
    bool reusable(bool enabled,float world_mode,uint32_t count)const noexcept {
        return enabled&&std::isfinite(world_mode)&&world_mode>.5f&&count>0&&count<=initialized_prefix_;
    }
    uint32_t initialized_prefix()const noexcept{return initialized_prefix_;}
    void invalidate()noexcept{initialized_prefix_=0;}
    void submitted_constant(uint32_t count)noexcept{initialized_prefix_=count;}
};
}
