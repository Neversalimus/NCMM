#include "ncmm_gpu_constant_shadow.hpp"
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
static uint64_t checks=0,reuses=0,dispatches=0,submissions=0,losses=0;
static void require(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"constant shadow state: %s\n",why);std::exit(2);}}
static bool ones(const std::vector<float>&buffer,uint32_t n){
    const float one=1;for(uint32_t i=0;i<n;++i)if(std::memcmp(&buffer[i],&one,4))return false;return true;
}
int main(){
    uint32_t seed=21973;const auto random=[&](){seed=seed*1664525u+1013904223u;return seed;};
    const uint32_t counts[]={1,2,17,63,64,65,127,128,257,625,1025,2048,4096};
    ncmm_gpu::constant_shadow_cache cache;std::vector<float> memory;uint32_t capacity=0;
    for(unsigned frame=0;frame<120000;++frame){
        const bool enabled=frame%11!=0;const float mode=frame%7<5?1.f:0.f;const uint32_t count=counts[(frame/19)%13];
        const bool replaced=frame%137==0||capacity<count;
        if(replaced){capacity=std::max(16u,count);memory.assign(capacity,std::numeric_limits<float>::quiet_NaN());cache.invalidate();++losses;}
        if(frame%199==0){cache.invalidate();memory.assign(capacity,-.13f);++losses;}
        const bool reused=cache.reusable(enabled,mode,count);
        if(reused){++reuses;require(mode>.5f&&enabled&&ones(memory,count),"each accepted reuse independently sees the exact initialized constant-one prefix");}
        else{
            ++dispatches;cache.invalidate();
            // Cycling or a failed/cancelled command can discard previous GPU
            // backing. Deliberately poison it before the simulated submission.
            memory.assign(capacity,std::numeric_limits<float>::quiet_NaN());
            const bool submitted=random()%17!=0;
            if(submitted){++submissions;
                if(mode>.5f){std::fill_n(memory.begin(),count,1.f);cache.submitted_constant(count);}
                else for(uint32_t i=0;i<count;++i)memory[i]=(i+frame)%3?.22f:.73f;
            }
            if(!submitted)require(!cache.reusable(true,1,count),"failed/cancelled writes cannot publish initialized GPU contents");
        }
        if(mode>.5f&&reused)require(ones(memory,count),"camera/time/light changes do not alter the shader's constant-one branch");
        require(!cache.reusable(true,0,count)&&!cache.reusable(true,.5f,count),"the variable shadow path never borrows constant contents");
        require(!cache.reusable(true,std::numeric_limits<float>::quiet_NaN(),count)&&!cache.reusable(true,std::numeric_limits<float>::infinity(),count),"invalid uniform modes have no reuse proof");
        require(!cache.reusable(true,1,0),"empty public scenes need no fabricated valid prefix");
        require(cache.initialized_prefix()<=capacity&&(!cache.initialized_prefix()||ones(memory,cache.initialized_prefix())),"reported initialized prefix is owned, in bounds and independently bit-exact");
    }
    cache.invalidate();cache.submitted_constant(4096);require(cache.reusable(true,1,2048),"a smaller active grid may borrow a fully written larger prefix");
    cache.invalidate();cache.submitted_constant(17);require(!cache.reusable(true,1,18),"one missing tail element forces actual initialization");
    // Independent negative control: if a caller forgets buffer replacement,
    // even a correct state helper can lie about poisoned GPU storage.
    cache.invalidate();cache.submitted_constant(128);memory.assign(128,.22f);
    require(cache.reusable(true,1,128)&&!ones(memory,128),"unnotified GPU buffer loss is detected as an unsafe integration, not accepted by this model");
    cache.invalidate();require(!cache.reusable(true,1,128),"the mandatory lifetime notification repairs the negative control");
    require(reuses>10000&&dispatches>10000&&losses>100,"both optimized and conservative paths are positively exercised");
    std::printf("{\"success\":true,\"checks\":%llu,\"frames\":120000,\"accepted_reuses\":%llu,\"dispatches\":%llu,\"successful_simulated_submissions\":%llu,\"buffer_loss_notifications\":%llu,\"missing_lifetime_notification_negative_control\":true,\"no_device_or_rendering\":true}\n",(unsigned long long)checks,(unsigned long long)reuses,(unsigned long long)dispatches,(unsigned long long)submissions,(unsigned long long)losses);
}
