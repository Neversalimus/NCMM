// Included at global scope after the complete pinned shadowcasting.cpp body.
#include "ncmm_graphics_sight_reference.hpp"
#ifndef NCMM_GRAPHICS_REFERENCE_PRIMARY
#define NCMM_GRAPHICS_REFERENCE_PRIMARY ::cast_zlight
#define NCMM_GRAPHICS_REFERENCE_PRIMARY_LOCAL
#endif
namespace ncmm_graphics_shadowcasting {
namespace {
float reference_sight_calc( const float &numerator, const float &transparency,
                            const int &distance )
{
    // This distinct function pointer selects the original distance path.
    // Both Native distance and sight formula execute on every request.
    return sight_calc( numerator, transparency, distance );
}
}
void reference_cast( const array_of_grids_of<float> &output,
                     const array_of_grids_of<const float> &input,
                     const array_of_grids_of<const bool> &floors,
                     const tripoint_bub_ms &origin, int offset_distance,
                     float numerator, vertical_direction direction )
{
    NCMM_GRAPHICS_REFERENCE_PRIMARY<float, reference_sight_calc, sight_check,
                                    accumulate_transparency>(
        output, input, floors, origin, offset_distance, numerator, direction );
}
}
#if defined(NCMM_GRAPHICS_REFERENCE_PRIMARY_LOCAL)
#undef NCMM_GRAPHICS_REFERENCE_PRIMARY
#undef NCMM_GRAPHICS_REFERENCE_PRIMARY_LOCAL
#endif
