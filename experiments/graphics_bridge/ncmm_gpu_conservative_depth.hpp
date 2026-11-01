#pragma once
#include "ncmm_graphics_gpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

// Used only after proving an immutable unskinned rigid instance, zero heading
// and exact opaque specialized ranges. Unproved inputs use the original program.
// Original fragment depth/color and command order must remain unchanged.
namespace ncmm_gpu::conservative_depth {
using point=std::array<float,3>;
inline std::optional<float> lower_bound(point low,point high,const ncmm_gpu_matrix_v1 &m,point eye,point forward,float far){
    if(!std::isfinite(far)||far<4||far>512)return {};
    double norm=0;
    for(unsigned i=0;i<3;++i){if(!std::isfinite(low[i])||!std::isfinite(high[i])||low[i]>high[i]||
        !std::isfinite(eye[i])||std::abs(eye[i])>256||!std::isfinite(forward[i])||std::abs(forward[i])>1.001f)return {};norm+=double(forward[i])*forward[i];}
    if(std::abs(norm-1)>.002)return {};
    for(float f:m.columns)if(!std::isfinite(f))return {};
    if(m.columns[3]!=0||m.columns[7]!=0||m.columns[11]!=0||m.columns[15]!=1)return {};
    for(unsigned row=0;row<3;++row){double absolute=std::abs(double(m.columns[12+row]));
        for(unsigned col=0;col<3;++col)absolute+=std::max(std::abs(double(low[col])),std::abs(double(high[col])))*std::abs(double(m.columns[4*col+row]));
        // Bound intermediates too: cancellation of huge terms is not a proof.
        if(absolute>256)return {};
    }
    double minimum=1e300,maximum=-1e300;
    for(unsigned corner=0;corner<8;++corner){double depth=0;
        for(unsigned row=0;row<3;++row){double world=m.columns[12+row];
            for(unsigned col=0;col<3;++col)world+=double(m.columns[4*col+row])*(corner&(1u<<col)?high[col]:low[col]);
            depth+=(world-eye[row])*forward[row];}
        minimum=std::min(minimum,depth);maximum=std::max(maximum,depth);
    }
    if(minimum<2||maximum>double(far)-.25)return {};
    // 125 mm world-space slack is far larger than bounded FP32 matrix,
    // clipping/interpolation/view arithmetic error. The extra depth slack also
    // covers host/shader division rounding and constant clip-z multiplication.
    const double z=minimum-.125,f=far;
    const double value=f/(f-.08)-.08*f/((f-.08)*z)-.0001;
    if(!std::isfinite(value)||value<0||value>=1)return {};
    return float(value);
}
}
