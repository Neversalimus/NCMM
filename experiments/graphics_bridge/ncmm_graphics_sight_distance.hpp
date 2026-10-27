#pragma once

namespace ncmm_graphics_shadowcasting {
// Native's integer circular distance equals these exact floor roots throughout
// the guarded 60-cell domain, including its double -> float -> int rounding.
// All supported XYZ combinations are compared to the pinned Native functions.
static_assert( MAX_VIEW_DISTANCE == 60 );
constexpr int largest_squared_distance = 3 * MAX_VIEW_DISTANCE * MAX_VIEW_DISTANCE;
struct circular_distance_table {
    unsigned char values[largest_squared_distance + 1]{};
    constexpr circular_distance_table() {
        // Fill by root intervals to stay within default compiler constexpr
        // limits. This immutable table has no first-use runtime initialization.
        for( unsigned root = 0; root * root <= largest_squared_distance; ++root ) {
            for( unsigned squared = root * root;
                 squared < ( root + 1 ) * ( root + 1 ) && squared <= largest_squared_distance;
                 ++squared ) {
                values[squared] = static_cast<unsigned char>( root );
            }
        }
    }
};
inline constexpr circular_distance_table circular_distances{};
inline int bounded_circular_distance( int x, int y, int z ) {
    // Only the standard pure callback triple with a nonnegative offset enters
    // this path. The guarded caster bounds all three positive deltas by60.
    return circular_distances.values[x * x + y * y + z * z];
}
}
