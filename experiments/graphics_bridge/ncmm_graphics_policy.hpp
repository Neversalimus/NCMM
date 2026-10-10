#pragma once
#include "ncmm_graphics.h"
#include <cmath>
#include <initializer_list>
namespace ncmm_graphics_policy {
inline bool valid_quad(const ncmm_graphics_quad_v1 &q,const ncmm_graphics_scene_v1 &s) {
    for(float v:{q.x0,q.y0,q.x1,q.y1,q.u0,q.v0,q.u1,q.v1}) if(!std::isfinite(v)) return false;
    if(q.x0<0 || q.y0<0 || q.x1>s.pixel_width+0.01f || q.y1>s.pixel_height+0.01f ||
       q.x1<=q.x0 || q.y1<=q.y0 || q.u0<0 || q.v0<0 || q.u1>1 || q.v1>1 || q.u1<q.u0 || q.v1<q.v0 ||
       q.layer>NCMM_GFX_CREATURE) return false;
    if(q.layer==NCMM_GFX_SOLID) return q.cell_index==UINT32_MAX;
    return q.cell_index<s.cell_count && (s.cells[q.cell_index].flags&NCMM_GFX_VISIBLE);
}
}
