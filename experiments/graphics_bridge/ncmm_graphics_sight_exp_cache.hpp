#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

// Private to shadowcasting.cpp, after the pinned shadowcasting.h definition.
namespace ncmm_graphics_shadowcasting {
struct standard_exp {
    float operator()( float exponent ) const { return std::exp( exponent ); }
};
template<class Evaluate = standard_exp> class exact_exp_cache {
    std::uint32_t key_ = 0;
    float value_ = 0.0f;
    bool valid_ = false;
public:
    float operator()( float exponent ) {
        // Leave exceptional/near-overflow/underflow inputs on the original
        // path every time, including their errno/floating-error behaviour.
        // A segment is synchronous and the pinned sight/check/accumulate
        // functions never alter the rounding mode or call world callbacks.
        if( !( exponent >= -80.0f && exponent <= 80.0f ) )
            return Evaluate{}( exponent );
        std::uint32_t key;
        static_assert( sizeof( key ) == sizeof( exponent ) );
        std::memcpy( &key, &exponent, sizeof( key ) );
        if( valid_ && key == key_ ) return value_;
        const float value = Evaluate{}( exponent );
        // Default exp is normal over the guarded interval. Retain this check
        // for evaluators/platforms that return an exceptional result anyway.
        valid_ = std::isnormal( value );
        key_ = key;
        value_ = value;
        return value;
    }
};
template<class T, T( *Calc )( const T &, const T &, const int & ),
         class Evaluate = standard_exp> class calc_cache {
public:
    T operator()( const T &numerator, const T &transparency, const int &distance ) {
        return Calc( numerator, transparency, distance );
    }
};
template<class Evaluate> class calc_cache<float, ::sight_calc, Evaluate> {
    // One entry on the segment's stack: no allocation, TLS, retained map data,
    // approximate math or warm state carried across actions/casts/Z levels.
    exact_exp_cache<Evaluate> denominator_;
public:
    float operator()( const float &numerator, const float &transparency, const int &distance ) {
        return numerator / denominator_( transparency * distance );
    }
};
} // namespace ncmm_graphics_shadowcasting
