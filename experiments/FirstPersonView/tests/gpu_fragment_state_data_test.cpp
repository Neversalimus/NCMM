#include "ncmm_gpu_fragment_state.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
static uint64_t checks=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"fragment state data: %s\n",why);std::exit(2);}}
int main(){
    ncmm_gpu::fragment_state_cache cache;std::array<uint32_t,12> words{};int tokens[16]{};
    std::array<ncmm_gpu::fragment_binding_identity,3> bindings{};
    require(cache.uniform_changed(words.data(),48)&&cache.samplers_changed(bindings),"zero bytes and null token pairs still require the first actual bind");
    require(!cache.uniform_changed(words.data(),48)&&!cache.samplers_changed(bindings),"only exact already initialized state may be retained");
    for(unsigned i=0;i<48;++i){auto changed=words;reinterpret_cast<unsigned char *>(changed.data())[i]^=0x80;
        require(cache.uniform_changed(changed.data(),48)&&!cache.uniform_changed(changed.data(),48),"every one of48 distinct uniform byte changes is observed");
        require(cache.uniform_changed(words.data(),48),"restoring any changed byte also requires a new upload");}
    for(unsigned i=0;i<6;++i){auto changed=bindings;if(i&1)changed[i/2].sampler=&tokens[i];else changed[i/2].texture=&tokens[i];
        require(cache.samplers_changed(changed)&&!cache.samplers_changed(changed),"all three texture and sampler identities are independently significant");require(cache.samplers_changed(bindings),"restoring the exact original binding triple is observed");}
    for(size_t n:{size_t(0),size_t(47),size_t(49)}){
        require(cache.uniform_changed(words.data(),n),"unproved uniform size cannot suppress a required upload");require(cache.uniform_changed(words.data(),48),"bad size invalidates any previously initialized uniform");}
    require(cache.uniform_changed(nullptr,48)&&cache.uniform_changed(words.data(),48),"null input cannot publish or retain a uniform state");
    words[0]=0x80000000u;require(cache.uniform_changed(words.data(),48),"negative zero is compared by its exact words");words[0]=0;require(cache.uniform_changed(words.data(),48),"positive zero remains distinct");
    words[0]=0x7fc00001u;require(cache.uniform_changed(words.data(),48),"a quiet NaN payload is an exact byte value");words[0]=0x7fc00002u;require(cache.uniform_changed(words.data(),48),"distinct NaN payload words never collapse");
    std::array<uint32_t,12> original{},applied{};std::array<ncmm_gpu::fragment_binding_identity,3> original_bindings{},applied_bindings{};
    uint32_t random=0x3250ab17u;uint64_t uniform_writes=0,sampler_writes=0,resets=0;
    for(unsigned frame=0;frame<200000;++frame){
        random=random*1664525u+1013904223u;
        const bool reset=frame%17==0||frame%331==0;if(reset){cache.invalidate();++resets;}
        if(frame%5==0)original[(random>>16)%12]=random;
        if(frame%7==0){const unsigned slot=(random>>9)%3;original_bindings[slot].texture=&tokens[(random>>4)%16];original_bindings[slot].sampler=&tokens[(random>>20)%16];}
        const bool uniform=cache.uniform_changed(original.data(),48),samplers=cache.samplers_changed(original_bindings);
        if(uniform){applied=original;++uniform_writes;}if(samplers){applied_bindings=original_bindings;++sampler_writes;}
        require(applied==original&&applied_bindings==original_bindings,"retained bytes and pair identities reconstruct every original simulated draw exactly");
        if(reset)require(uniform&&samplers,"each pass/pipeline/lifetime reset forces both first bindings");
        require(!cache.uniform_changed(original.data(),48)&&!cache.samplers_changed(original_bindings),"a second identical query can retain only the just initialized state");
    }
    require(uniform_writes<200000&&sampler_writes<200000&&resets>10000,"independent transitions exercise both mandatory writes and positive reuse");
    std::printf("{\"success\":true,\"checks\":%llu,\"simulated_draws\":200000,\"uniform_writes\":%llu,\"sampler_writes\":%llu,\"resets\":%llu,\"no_SDL_or_device\":true}\n",(unsigned long long)checks,(unsigned long long)uniform_writes,(unsigned long long)sampler_writes,(unsigned long long)resets);
}
