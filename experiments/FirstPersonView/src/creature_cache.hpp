#pragma once
#include "creature_lod.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <functional>
namespace first_person::creature {
inline uint64_t appearance_key(const ncmm_graphics_creature_v1 &s,uint64_t tileset_epoch){
    uint64_t h=1469598103934665603ull;
    auto add=[&](const void *p,size_t n){h=hash_bytes(h,p,n);};
    add(&generator_version,sizeof(generator_version));add(&tileset_epoch,sizeof(tileset_epoch));
    add(&s.appearance_revision,sizeof(s.appearance_revision));add(s.type_id,sizeof(s.type_id));add(s.bodytype,sizeof(s.bodytype));add(s.species,sizeof(s.species));
    // State, location, entity ID, heading and event clocks do not rebuild assets.
    const uint32_t traits=s.flags&~(NCMM_CR_PRONE|NCMM_CR_SUBMERGED|NCMM_CR_DEAD);add(&traits,sizeof(traits));
    for(float value:{s.liters,s.kilograms,s.height,s.width})add(&value,sizeof(value));
    return h;
}
struct asset {model geometry;std::unique_ptr<impostor_atlas> atlas;double generation_ms=0;size_t bytes()const{return sizeof(asset)+geometry.bytes()+(atlas?sizeof(impostor_atlas):0);}};
struct cache_statistics {size_t bytes=0,peak_bytes=0,entries=0,pending=0;uint64_t generated=0,hits=0,misses=0,evictions=0,cancelled=0,failures=0;double last_generation_ms=0,total_generation_ms=0,max_generation_ms=0;std::array<uint64_t,size_t(anatomy::count)> anatomy_counts{};std::array<uint64_t,5> fallback_counts{};};
class cache {
public:
    static constexpr size_t memory_limit=64*1024*1024,queue_limit=16;
private:
    struct entry {std::shared_ptr<const asset> value;uint64_t touched=0;};
    struct job {uint64_t key=0,epoch=0;phenotype type; sprite source;};
    std::unordered_map<uint64_t,entry> entries;
    std::unordered_set<uint64_t> pending;
    std::deque<job> jobs;
    mutable std::mutex mutex;std::condition_variable changed;std::thread worker;
    std::atomic<uint64_t> epoch{1};bool stopping=false,available=false;
    uint64_t clock=0,world=0,tileset=0;size_t requested_this_frame=0;
    cache_statistics counters;
    void work(){
        for(;;){job input;
            {std::unique_lock<std::mutex> lock(mutex);changed.wait(lock,[&]{return stopping||!jobs.empty();});if(stopping)return;input=std::move(jobs.front());jobs.pop_front();}
            std::shared_ptr<asset> result;
            try{const auto started=std::chrono::steady_clock::now();result=std::make_shared<asset>();result->geometry=builder{}.generate(input.type,input.source);result->atlas=std::make_unique<impostor_atlas>();
                if(!bake_atlas(result->geometry,*result->atlas,&epoch,input.epoch))result.reset();
                if(result)result->generation_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
            }catch(const std::exception &){result.reset();}
            std::lock_guard<std::mutex> lock(mutex);
            if(input.epoch!=epoch.load(std::memory_order_relaxed)){++counters.cancelled;continue;}
            pending.erase(input.key);
            if(!result){++counters.failures;continue;}
            const size_t bytes=result->bytes();
            while(!entries.empty()&&counters.bytes+bytes>memory_limit){auto victim=entries.end();
                for(auto it=entries.begin();it!=entries.end();++it)if(it->second.value.use_count()==1&&(victim==entries.end()||it->second.touched<victim->second.touched))victim=it;
                if(victim==entries.end())break;
                counters.bytes-=victim->second.value->bytes();entries.erase(victim);++counters.evictions;
            }
            if(counters.bytes+bytes>memory_limit){++counters.failures;continue;}
            counters.bytes+=bytes;counters.peak_bytes=std::max(counters.peak_bytes,counters.bytes);++counters.generated;
            counters.last_generation_ms=result->generation_ms;counters.total_generation_ms+=result->generation_ms;counters.max_generation_ms=std::max(counters.max_generation_ms,result->generation_ms);
            ++counters.anatomy_counts[size_t(result->geometry.type.kind)];++counters.fallback_counts[size_t(result->geometry.type.reason)];
            entries.emplace(input.key,entry{std::move(result),++clock});changed.notify_all();
        }
    }
public:
    cache(){try{worker=std::thread([this]{work();});available=true;}catch(const std::system_error &){available=false;}}
    cache(const cache &)=delete;cache &operator=(const cache &)=delete;
    ~cache(){shutdown();}
    void shutdown(){
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;epoch.fetch_add(1,std::memory_order_relaxed);jobs.clear();pending.clear();}
        changed.notify_all();if(worker.joinable())worker.join();available=false;
    }
    void reset(uint64_t world_epoch,uint64_t tileset_epoch){
        std::lock_guard<std::mutex> lock(mutex);world=world_epoch;tileset=tileset_epoch;epoch.fetch_add(1,std::memory_order_relaxed);entries.clear();jobs.clear();pending.clear();counters.bytes=0;requested_this_frame=0;
    }
    void begin_frame(uint64_t world_epoch,uint64_t tileset_epoch){if(world!=world_epoch||tileset!=tileset_epoch)reset(world_epoch,tileset_epoch);requested_this_frame=0;}
    std::shared_ptr<const asset> request(uint64_t key,const phenotype &type,const std::function<sprite()> &source){
        {std::lock_guard<std::mutex> lock(mutex);auto found=entries.find(key);if(found!=entries.end()){found->second.touched=++clock;++counters.hits;return found->second.value;}
            ++counters.misses;if(!available||stopping||pending.count(key)||pending.size()>=queue_limit||requested_this_frame>=4)return {};
            pending.insert(key);++requested_this_frame;}
        // Source pixels are copied on the callback thread before any worker can
        // see them. Neither the provider nor CDDA/SDL pointers enter the queue.
        sprite owned;
        try{owned=source();}catch(const std::exception &){std::lock_guard<std::mutex> lock(mutex);pending.erase(key);++counters.failures;return {};}
        {std::lock_guard<std::mutex> lock(mutex);jobs.push_back({key,epoch.load(std::memory_order_relaxed),type,std::move(owned)});}
        changed.notify_one();return {};
    }
    cache_statistics statistics()const{std::lock_guard<std::mutex> lock(mutex);auto result=counters;result.entries=entries.size();result.pending=pending.size();return result;}
    // Bounded wait is used only by native/SDL regression fixtures, never by a
    // gameplay callback. It gives deterministic screenshots after generation.
    bool wait_idle(std::chrono::milliseconds limit){std::unique_lock<std::mutex> lock(mutex);return changed.wait_for(lock,limit,[&]{return pending.empty();});}
};
struct scene {
    ncmm_graphics_creature_scene_v1 view{};std::vector<ncmm_graphics_creature_v1> creatures;
};
inline bool valid(const scene &s,size_t cells){
    if(s.view.struct_size<sizeof(s.view)||s.view.count!=s.creatures.size()||s.view.count>cells+256||!std::isfinite(s.view.seconds))return false;
    std::unordered_set<uint64_t> ids;
    for(const auto &c:s.creatures){if(!c.entity_id||!ids.insert(c.entity_id).second||c.cell>=cells||c.event>NCMM_CR_DIE||c.flags&~131071u||!std::isfinite(c.event_seconds)||!std::memchr(c.type_id,0,sizeof(c.type_id))||!std::memchr(c.bodytype,0,sizeof(c.bodytype))||!std::memchr(c.species,0,sizeof(c.species)))return false;
        for(float value:{c.liters,c.kilograms,c.height,c.width,c.elevation,c.speed,c.heading})if(!std::isfinite(value))return false;
        if(c.height<0||c.height>7||c.width<0||c.width>9||c.elevation<0||c.elevation>5)return false;
    }
    return true;
}
}
