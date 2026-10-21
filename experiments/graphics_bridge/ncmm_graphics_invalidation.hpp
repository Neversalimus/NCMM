#pragma once
class map;
namespace ncmm {
// Only the exact native cache-flag reset blocks use this scope. Real terrain,
// field and vehicle mutations outside them retain their full invalidation.
// Borrowed map addresses exist for this synchronous scope only; no map is read.
class graphics_native_cache_reset {
    inline static thread_local const map *owner_=nullptr;
    inline static thread_local int level_=0;
    const map *previous_owner;
    int previous_level;
public:
    graphics_native_cache_reset(const map *source,int z)noexcept:
        previous_owner(owner_),previous_level(level_){owner_=source;level_=z;}
    ~graphics_native_cache_reset(){owner_=previous_owner;level_=previous_level;}
    graphics_native_cache_reset(const graphics_native_cache_reset &)=delete;
    graphics_native_cache_reset &operator=(const graphics_native_cache_reset &)=delete;
    static bool contains(const map *source,int z)noexcept{return owner_&&owner_==source&&level_==z;}
};
}
