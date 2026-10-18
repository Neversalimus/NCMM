#pragma once
#include <cstdlib>
#include <cstring>
// Unmodified, header-only NVTX core. There is no linked NVTX/NGFX SDK library,
// required profiler DLL, or runtime dependency on a locally installed SDK.
// A developer tool may provide its injection implementation for opt-in traces.
#if defined(_WIN32)
#pragma push_macro("small")
#pragma push_macro("min")
#pragma push_macro("max")
#pragma push_macro("near")
#pragma push_macro("far")
#endif
#include "third_party/nvtx3/include/nvtx3/nvToolsExt.h"
#if defined(_WIN32)
#pragma pop_macro("far")
#pragma pop_macro("near")
#pragma pop_macro("max")
#pragma pop_macro("min")
#pragma pop_macro("small")
#endif
namespace ncmm_graphics_trace {
inline bool enabled()noexcept{
    static const bool value=[](){const auto *text=std::getenv("NCMM_FP_TRACE_MARKERS");return text&&std::strcmp(text,"1")==0;}();
    return value;
}
class cpu_zone {
    bool active_;
public:
    explicit cpu_zone(const char *label)noexcept:active_(enabled()){if(active_)nvtxRangePushA(label);}
    ~cpu_zone(){stop();}
    void stop()noexcept{if(active_){nvtxRangePop();active_=false;}}
    cpu_zone(const cpu_zone &)=delete;
    cpu_zone &operator=(const cpu_zone &)=delete;
};
}
