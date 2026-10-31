#pragma once
#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP>=2)
#include <emmintrin.h>
#define NCMM_VEHICLE_PIXEL_SSE2 1
#else
#define NCMM_VEHICLE_PIXEL_SSE2 0
#endif

// Four independent numeric samples. No framebuffer, renderer, SDL or device is
// accessed here. The caller retains scan coverage, scalar weight increments,
// row ownership, material guards and the original one-to-three-sample tail.
namespace first_person::vehicle_pixels {
#if NCMM_VEHICLE_PIXEL_SSE2
struct result4 {__m128 depth;__m128i color;unsigned active=0;};
template<class Vertices,class RGB>
inline result4 shade4(const float *w0,const float *w1,const float *inverse,const float *depth,
                     const Vertices &vertices,RGB color,RGB fog_color,float fog_density){
    const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f);
    const auto a0=_mm_loadu_ps(w0),a1=_mm_loadu_ps(w1),a2=_mm_sub_ps(_mm_sub_ps(one,a0),a1);
    const auto i0=_mm_set1_ps(inverse[0]),i1=_mm_set1_ps(inverse[1]),i2=_mm_set1_ps(inverse[2]);
    const auto reciprocal=_mm_add_ps(_mm_add_ps(_mm_mul_ps(a0,i0),_mm_mul_ps(a1,i1)),_mm_mul_ps(a2,i2));
    const auto skipped=_mm_cmple_ps(reciprocal,zero);
    // The scalar reference never divides nonpositive samples. Do not introduce
    // a divide-by-zero status exception in an inactive lane of a mixed group.
    const auto denominator=_mm_or_ps(_mm_and_ps(skipped,one),_mm_andnot_ps(skipped,reciprocal));
    result4 result{};result.depth=_mm_div_ps(one,denominator);
    const auto visible=_mm_andnot_ps(skipped,_mm_cmpngt_ps(result.depth,_mm_add_ps(_mm_loadu_ps(depth),_mm_set1_ps(.0002f))));
    result.active=unsigned(_mm_movemask_ps(visible));if(!result.active)return result;
    const auto a=_mm_mul_ps(_mm_mul_ps(a0,i0),result.depth),b=_mm_mul_ps(_mm_mul_ps(a1,i1),result.depth),
               c=_mm_mul_ps(_mm_mul_ps(a2,i2),result.depth);
    auto fog=_mm_mul_ps(_mm_sub_ps(result.depth,_mm_set1_ps(2.f)),_mm_set1_ps(fog_density));
    fog=_mm_min_ps(_mm_max_ps(fog,zero),one);const auto clear=_mm_sub_ps(one,fog);
    const auto channel=[&](float base,float l0,float l1,float l2,float sky){
        const auto light=_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(l0),a),_mm_mul_ps(_mm_set1_ps(l1),b)),_mm_mul_ps(_mm_set1_ps(l2),c));
        const auto lit=_mm_mul_ps(_mm_set1_ps(base),light);
        const auto mixed=_mm_add_ps(_mm_mul_ps(lit,clear),_mm_mul_ps(_mm_set1_ps(sky),fog));
        return _mm_cvttps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(mixed,zero),one),_mm_set1_ps(255.f)));
    };
    const auto red=channel(color.x,vertices[0].light.x,vertices[1].light.x,vertices[2].light.x,fog_color.x);
    const auto green=channel(color.y,vertices[0].light.y,vertices[1].light.y,vertices[2].light.y,fog_color.y);
    const auto blue=channel(color.z,vertices[0].light.z,vertices[1].light.z,vertices[2].light.z,fog_color.z);
    result.color=_mm_or_si128(_mm_or_si128(_mm_slli_epi32(red,24),_mm_slli_epi32(green,16)),
                            _mm_or_si128(_mm_slli_epi32(blue,8),_mm_set1_epi32(255)));
    return result;
}
// The joint world/component queue also contains ordinary flat-lit opaque
// faces. Preserve their constant light rather than interpolating it: even
// equal corner lights cannot be substituted into shade4 without changing bits.
template<class RGB>
inline result4 shade4_flat(const float *w0,const float *w1,const float *inverse,const float *depth,
                          RGB light,RGB color,RGB fog_color,float fog_density){
    const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f);
    const auto a0=_mm_loadu_ps(w0),a1=_mm_loadu_ps(w1),a2=_mm_sub_ps(_mm_sub_ps(one,a0),a1);
    const auto i0=_mm_set1_ps(inverse[0]),i1=_mm_set1_ps(inverse[1]),i2=_mm_set1_ps(inverse[2]);
    const auto reciprocal=_mm_add_ps(_mm_add_ps(_mm_mul_ps(a0,i0),_mm_mul_ps(a1,i1)),_mm_mul_ps(a2,i2));
    const auto skipped=_mm_cmple_ps(reciprocal,zero);
    const auto denominator=_mm_or_ps(_mm_and_ps(skipped,one),_mm_andnot_ps(skipped,reciprocal));
    result4 result{};result.depth=_mm_div_ps(one,denominator);
    const auto visible=_mm_andnot_ps(skipped,_mm_cmpngt_ps(result.depth,_mm_add_ps(_mm_loadu_ps(depth),_mm_set1_ps(.0002f))));
    result.active=unsigned(_mm_movemask_ps(visible));if(!result.active)return result;
    auto fog=_mm_mul_ps(_mm_sub_ps(result.depth,_mm_set1_ps(2.f)),_mm_set1_ps(fog_density));
    fog=_mm_min_ps(_mm_max_ps(fog,zero),one);const auto clear=_mm_sub_ps(one,fog);
    const auto channel=[&](float base,float illumination,float sky){
        const auto lit=_mm_mul_ps(_mm_set1_ps(base),_mm_set1_ps(illumination));
        const auto mixed=_mm_add_ps(_mm_mul_ps(lit,clear),_mm_mul_ps(_mm_set1_ps(sky),fog));
        return _mm_cvttps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(mixed,zero),one),_mm_set1_ps(255.f)));
    };
    const auto red=channel(color.x,light.x,fog_color.x),green=channel(color.y,light.y,fog_color.y),blue=channel(color.z,light.z,fog_color.z);
    result.color=_mm_or_si128(_mm_or_si128(_mm_slli_epi32(red,24),_mm_slli_epi32(green,16)),
                            _mm_or_si128(_mm_slli_epi32(blue,8),_mm_set1_epi32(255)));
    return result;
}
// Coverage/UV/filtering remain the caller's original per-sample operations.
// Only the flat light, fog, clamp and packing arithmetic is evaluated in lanes.
template<class RGB,class Sample>
inline result4 shade4_flat_sampled(const float *w0,const float *w1,const float *inverse,const float *depth,
                                  RGB light,RGB fog_color,float fog_density,Sample sample){
    const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f);
    const auto a0=_mm_loadu_ps(w0),a1=_mm_loadu_ps(w1),a2=_mm_sub_ps(_mm_sub_ps(one,a0),a1);
    const auto i0=_mm_set1_ps(inverse[0]),i1=_mm_set1_ps(inverse[1]),i2=_mm_set1_ps(inverse[2]);
    const auto reciprocal=_mm_add_ps(_mm_add_ps(_mm_mul_ps(a0,i0),_mm_mul_ps(a1,i1)),_mm_mul_ps(a2,i2));
    const auto skipped=_mm_cmple_ps(reciprocal,zero);
    const auto denominator=_mm_or_ps(_mm_and_ps(skipped,one),_mm_andnot_ps(skipped,reciprocal));
    result4 result{};result.depth=_mm_div_ps(one,denominator);
    const auto visible=_mm_andnot_ps(skipped,_mm_cmpngt_ps(result.depth,_mm_add_ps(_mm_loadu_ps(depth),_mm_set1_ps(.0002f))));
    result.active=unsigned(_mm_movemask_ps(visible));if(!result.active)return result;
    RGB colors[4]{};float distances[4];_mm_storeu_ps(distances,result.depth);
    for(unsigned lane=0;lane<4;++lane)if(result.active&(1u<<lane))colors[lane]=sample(lane,distances[lane]);
    auto fog=_mm_mul_ps(_mm_sub_ps(result.depth,_mm_set1_ps(2.f)),_mm_set1_ps(fog_density));
    fog=_mm_min_ps(_mm_max_ps(fog,zero),one);const auto clear=_mm_sub_ps(one,fog);
    const auto channel=[&](__m128 base,float illumination,float sky){
        const auto lit=_mm_mul_ps(base,_mm_set1_ps(illumination));
        const auto mixed=_mm_add_ps(_mm_mul_ps(lit,clear),_mm_mul_ps(_mm_set1_ps(sky),fog));
        return _mm_cvttps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(mixed,zero),one),_mm_set1_ps(255.f)));
    };
    const auto red=channel(_mm_set_ps(colors[3].x,colors[2].x,colors[1].x,colors[0].x),light.x,fog_color.x);
    const auto green=channel(_mm_set_ps(colors[3].y,colors[2].y,colors[1].y,colors[0].y),light.y,fog_color.y);
    const auto blue=channel(_mm_set_ps(colors[3].z,colors[2].z,colors[1].z,colors[0].z),light.z,fog_color.z);
    result.color=_mm_or_si128(_mm_or_si128(_mm_slli_epi32(red,24),_mm_slli_epi32(green,16)),
                            _mm_or_si128(_mm_slli_epi32(blue,8),_mm_set1_epi32(255)));
    return result;
}
#endif
}
