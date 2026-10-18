#if defined(_WIN32) && !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include "ncmm_graphics_trace.hpp"
#if defined(_WIN32)
// CDDA includes other SDK headers after the trace wrapper. FAR must retain its
// Win32 meaning even when the native near/far identifiers are restored.
#include <objbase.h>
#endif
enum class native_distance { near, far };
int main()
{
    ncmm_graphics_trace::cpu_zone scope("Trace SDK include compatibility");
    return native_distance::near == native_distance::far;
}
