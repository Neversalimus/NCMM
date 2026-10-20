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
int main(int argc,char **argv)
{
    const bool capture=argc>1&&std::strcmp(argv[1],"capture")==0;
#if defined(_WIN32)
    _putenv_s("NCMM_FP_TRACE_CAPTURE_SCENE",capture?"quiet":"");
#else
    if(capture)setenv("NCMM_FP_TRACE_CAPTURE_SCENE","quiet",1);else unsetenv("NCMM_FP_TRACE_CAPTURE_SCENE");
#endif
    ncmm_graphics_trace::note_present();ncmm_graphics_trace::note_present();
    if(ncmm_graphics_trace::presented_frames!=(capture?2u:0u))return 2;
    if(capture&&std::strcmp(ncmm_graphics_trace::capture_label(),"FP_SCENE:quiet"))return 3;
    ncmm_graphics_trace::cpu_zone scope("Trace SDK include compatibility");
    return native_distance::near == native_distance::far;
}
