#include "gpu_fixture_producer.hpp"

namespace first_person::gpu {
bool fixture_scene_cache::build(const ncmm_graphics_scene_v2 &s,float yaw,float pitch,const hybrid::sprite_provider &provider,packet &out,
    hybrid::frame &statistics,const ncmm_gpu_metrics_v1 &host,animation::pose pose,const hybrid::presentation_data *metadata,
    creature::presentation *creatures,int width,int height,uint32_t mode,const ncmm_graphics_multilevel_scene_v1 *levels,
    hybrid::render_options options){
    return scene_cache::build(s,yaw,pitch,provider,out,statistics,host,pose,metadata,creatures,width,height,mode,levels,options);
}
}
