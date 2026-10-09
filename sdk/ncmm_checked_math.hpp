#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace ncmm { namespace checked {
inline int64_t nonnegative( int64_t value ) noexcept { return std::max<int64_t>( 0, value ); }
inline int64_t add( int64_t a, int64_t b ) noexcept
{
    a = nonnegative( a ); b = nonnegative( b );
    return a > std::numeric_limits<int64_t>::max() - b ? std::numeric_limits<int64_t>::max() : a + b;
}
inline int64_t multiply( int64_t a, int64_t b ) noexcept
{
    a = nonnegative( a ); b = nonnegative( b );
    return b != 0 && a > std::numeric_limits<int64_t>::max() / b ?
           std::numeric_limits<int64_t>::max() : a * b;
}
// Scale without overflowing the intermediate product; keep fractional progress.
inline int64_t percent( int64_t raw, int64_t rate, int64_t &fraction ) noexcept
{
    raw = nonnegative( raw ); rate = nonnegative( rate );
    fraction = nonnegative( fraction ) % 100;
    const int64_t tail = ( raw % 100 ) * ( rate % 100 ) + fraction;
    fraction = tail % 100;
    return add( multiply( raw / 100, rate ),
                add( ( raw % 100 ) * ( rate / 100 ), tail / 100 ) );
}
} }
