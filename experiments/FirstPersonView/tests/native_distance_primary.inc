// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/line.h
// Exact Native numerical functions; line.h is byte-identical at both pins.
// Git blob76fcdf986985d54d9aba475dce55ce02bc139152, complete header SHA256
// 9412f1ae82fcbc92b7bd09aa5d9f0a01edb0eabcbc50f25f0d78605e7c883c58.
// Raw tripoint operators and coordinate forwarding in the model are owned.
inline float trig_dist( const tripoint &loc1, const tripoint &loc2 )
{
    return std::sqrt( static_cast<double>( ( loc1.x - loc2.x ) * ( loc1.x - loc2.x ) ) +
                      ( ( loc1.y - loc2.y ) * ( loc1.y - loc2.y ) ) +
                      ( ( loc1.z - loc2.z ) * ( loc1.z - loc2.z ) ) );
}

inline int square_dist( const tripoint &loc1, const tripoint &loc2 )
{
    const tripoint d = ( loc1 - loc2 ).abs();
    return std::max( { d.x, d.y, d.z } );
}

inline int rl_dist( const tripoint &loc1, const tripoint &loc2 )
{
    if( trigdist ) {
        return trig_dist( loc1, loc2 );
    }
    return square_dist( loc1, loc2 );
}
