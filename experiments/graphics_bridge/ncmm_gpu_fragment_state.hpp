#pragma once
#include <array>
#include <cstddef>
#include <cstring>

namespace ncmm_gpu {
struct fragment_binding_identity {
    const void *texture=nullptr,*sampler=nullptr;
    bool operator==(const fragment_binding_identity &b)const{return texture==b.texture&&sampler==b.sampler;}
};
// A single draw_batches invocation owns this cache. The caller also invalidates
// it at every real geometry-pipeline bind. Never retain it across a render pass,
// frame, resource preparation, failed submission or replacement device.
class fragment_state_cache {
    std::array<unsigned char,48> uniform_{};
    std::array<fragment_binding_identity,3> samplers_{};
    bool uniform_valid_=false,samplers_valid_=false;
public:
    void invalidate(){uniform_valid_=samplers_valid_=false;}
    bool uniform_changed(const void *data,size_t size){
        if(!data||size!=uniform_.size()){uniform_valid_=false;return true;}
        if(uniform_valid_&&!std::memcmp(uniform_.data(),data,size))return false;
        std::memcpy(uniform_.data(),data,size);uniform_valid_=true;return true;
    }
    bool samplers_changed(const std::array<fragment_binding_identity,3> &values){
        if(samplers_valid_&&samplers_==values)return false;
        samplers_=values;samplers_valid_=true;return true;
    }
};
}
