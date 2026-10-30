#pragma once
#include "ncmm_graphics_identity.hpp"
#include <atomic>
#include <cstdint>
#include <limits>
#include <utility>

namespace ncmm_graphics_identity {
// A part's identity follows it through vector reallocation, erase and sorting.
// Copying installs a distinct presentation object; moving transfers ownership.
// Never serialized, never used by simulation or item targeting.
class vehicle_part_lifetime {
    inline static std::atomic<uint64_t> next_{1};
    static uint64_t allocate() noexcept {
        auto value=next_.load(std::memory_order_relaxed);
        while(value!=std::numeric_limits<uint64_t>::max())
            if(next_.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
        return 0;
    }
    uint64_t value_=allocate();
public:
    vehicle_part_lifetime() noexcept=default;
    vehicle_part_lifetime(const vehicle_part_lifetime &) noexcept:vehicle_part_lifetime(){}
    vehicle_part_lifetime(vehicle_part_lifetime &&other) noexcept:value_(std::exchange(other.value_,0)){}
    vehicle_part_lifetime &operator=(const vehicle_part_lifetime &other) noexcept {
        if(this!=&other){value_=allocate();}
        return *this;
    }
    vehicle_part_lifetime &operator=(vehicle_part_lifetime &&other) noexcept {
        if(this!=&other){value_=std::exchange(other.value_,0);}
        return *this;
    }
    uint64_t value()const noexcept{return value_;}
};
}
