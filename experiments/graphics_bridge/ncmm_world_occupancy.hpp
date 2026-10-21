#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ncmm_world {
// An owned, callback-local superset of Native creature locations. An incomplete
// mask or any out-of-window query must defer to the canonical Native lookup.
template<std::size_t Side, std::size_t Levels>
class creature_occupancy {
    static_assert(Side>0 && Side<=257 && Levels>0 && Levels<=257,"bounded occupancy dimensions");
    static constexpr std::size_t cells=Side*Side*Levels;
    std::array<uint64_t,(cells+63)/64> words_{};
    int32_t ox_,oy_,low_;
    bool complete_=false;
    std::size_t index(int32_t x,int32_t y,int32_t z) const noexcept {
        const int64_t dx=int64_t(x)-ox_,dy=int64_t(y)-oy_,dz=int64_t(z)-low_;
        if(dx<0 || dy<0 || dz<0 || dx>=int64_t(Side) || dy>=int64_t(Side) || dz>=int64_t(Levels))return cells;
        return (std::size_t(dz)*Side+std::size_t(dy))*Side+std::size_t(dx);
    }
public:
    creature_occupancy(int32_t ox,int32_t oy,int32_t low) noexcept:ox_(ox),oy_(oy),low_(low){}
    void mark(int32_t x,int32_t y,int32_t z) noexcept {
        const auto i=index(x,y,z);if(i<cells)words_[i/64]|=uint64_t(1)<<(i%64);
    }
    void finish() noexcept {complete_=true;}
    void invalidate() noexcept {complete_=false;}
    bool possibly_occupied(int32_t x,int32_t y,int32_t z) const noexcept {
        if(!complete_)return true;
        const auto i=index(x,y,z);return i==cells || (words_[i/64]&(uint64_t(1)<<(i%64)))!=0;
    }
};
}
