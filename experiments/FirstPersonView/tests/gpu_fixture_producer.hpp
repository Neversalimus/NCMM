#pragma once
#include "gpu_scene.hpp"

// Paired fixture only. Both backends link exactly this one compiled producer
// object; compiling different inline backend bodies cannot specialize it.
// This adds no work, synchronization or instrumentation to a submission.
namespace first_person::gpu {
class fixture_scene_cache:public scene_cache {
public:
    using scene_cache::scene_cache;
    bool build(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const hybrid::sprite_provider &provider,packet &out,
        hybrid::frame &statistics,const ncmm_gpu_metrics_v1 &host,animation::pose pose,const hybrid::presentation_data *metadata,
        creature::presentation *creatures,int width,int height,uint32_t mode,const ncmm_graphics_multilevel_scene_v1 *levels=nullptr,
        hybrid::render_options options={});
};
static_assert(sizeof(fixture_scene_cache)==sizeof(scene_cache),"the fixture bridge adds no producer state");
static_assert(alignof(fixture_scene_cache)==alignof(scene_cache),"unchanged producer alignment");
}
