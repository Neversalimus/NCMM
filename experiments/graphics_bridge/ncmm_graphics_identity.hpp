#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
namespace ncmm_graphics_identity {
// A native object's lifetime, not its address, position, save ID or copy source.
// This presentation-only member is not serialized. The world epoch also
// separates loads. Exhaustion fails closed rather than reusing IDs.
class lifetime {
    inline static std::atomic<uint64_t> next_{1};
    static uint64_t allocate() noexcept {
        auto value=next_.load(std::memory_order_relaxed);
        while(value!=std::numeric_limits<uint64_t>::max()) {
            if(next_.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
        }
        return 0;
    }
    uint64_t value_=allocate();
public:
    lifetime() noexcept=default;
    lifetime(const lifetime &) noexcept:lifetime(){}
    lifetime(lifetime &&) noexcept:lifetime(){}
    lifetime &operator=(const lifetime &) noexcept{return *this;}
    lifetime &operator=(lifetime &&) noexcept{return *this;}
    uint64_t value() const noexcept{return value_;}
};
}
