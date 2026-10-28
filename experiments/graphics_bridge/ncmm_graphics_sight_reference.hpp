#pragma once
#if !defined(NCMM_SHADOWCAST_MODEL)
#include "shadowcasting.h"
#include "line.h"
#endif
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>

// Private diagnostic entrypoint. Ordinary Native visibility calls cast_zlight
// directly; this entrypoint is used only by an explicit isolated comparison.
namespace ncmm_graphics_shadowcasting {
void reference_cast( const array_of_grids_of<float> &output,
                     const array_of_grids_of<const float> &input,
                     const array_of_grids_of<const bool> &floors,
                     const tripoint_bub_ms &origin, int offset_distance,
                     float numerator, vertical_direction direction );
}
