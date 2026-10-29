#pragma once
#include "ncmm_graphics_multilevel.h"
#include "ncmm_graphics_presentation.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace ncmm_graphics_snapshot {
// This is a lease on CDDA's blocked main thread, not an inference from its turn
// number. Unknown actions, nested UI and all native mutation notifications end
// the generation. No native object or atlas address is part of the cache.
class input_lease {
    std::array<uint64_t,32> tokens{};
    uint64_t next_token=0,generation_=1;
    uint64_t native_loop_token=0;
    size_t depth=0;
    bool allowed=false;
public:
    uint64_t generation()const noexcept{return generation_;}
    bool active()const noexcept{return allowed&&(depth==1||
        (depth==2&&native_loop_token&&tokens[0]==native_loop_token));}
    void invalidate()noexcept{if(++generation_==0)++generation_;}
    void reset()noexcept{invalidate();depth=0;native_loop_token=0;allowed=false;tokens.fill(0);}
    uint64_t begin(const std::string &category)noexcept{
        const bool delegated=native_loop_token&&depth==1&&tokens[0]==native_loop_token&&category=="DEFAULTMODE";
        if((depth&&!delegated)||category!="DEFAULTMODE"){invalidate();allowed=false;}
        // A modal may have revoked the outer loop. Its next actual default
        // input wait is a fresh, safe read lifetime; revocation is not a latch.
        else if(!depth||delegated)allowed=true;
        if(depth==tokens.size()){reset();return 0;}
        if(++next_token==0)++next_token;
        tokens[depth++]=next_token;return next_token;
    }
    // game::get_player_input owns the real animation/input loop. Its 125 ms
    // polls do not execute an avatar action or advance the simulation. Only
    // this explicit outer scope can delegate DEFAULTMODE polling lifetimes.
    uint64_t begin_native_loop()noexcept{
        const bool outer=depth==0;
        if(!outer){invalidate();allowed=false;}
        const auto token=begin("DEFAULTMODE");
        if(outer)native_loop_token=token;
        else allowed=false; // A recursive loop is not an ordinary delegated poll.
        return token;
    }
    static bool camera_action(const std::string &action)noexcept{
        return action=="ncmm.view.turn_left"||action=="ncmm.view.turn_right"||
            action=="ncmm.view.look_up"||action=="ncmm.view.look_down"||
            action=="ncmm.view.mouse_look";
    }
    void end(uint64_t token,const std::string &action)noexcept{
        if(!token||!depth||tokens[depth-1]!=token)return;
        const bool ending_loop=token==native_loop_token;
        const bool delegated=native_loop_token&&depth==2;
        tokens[--depth]=0;
        if(ending_loop)native_loop_token=0;
        if(delegated){
            if(action!="TIMEOUT"&&!camera_action(action)){allowed=false;invalidate();}
        }else{
            allowed=false;
            if(depth||!camera_action(action))invalidate();
        }
    }
};

struct entry {
    ncmm_world_cell_v1 world{};
    // Payload lifetime starts with its complete trivial assignment, before
    // the matching ready flag is set. Anonymous unions retain trivial owned
    // representation copies when vector growth relocates unpublished entries.
    union { ncmm_graphics_cell_v1 cell; };
    union { ncmm_graphics_cell_v2 detail; };
    union { ncmm_graphics_detail_v1 presentation; };
    std::array<float,3> facade_light{};
    bool base_ready=false,detail_ready=false,presentation_ready=false,has_source_light=false;
    entry()noexcept{} // Only permissions/light/readiness exist before publication.
};
struct counters {
    uint64_t generations=0,hits=0,empty_hits=0,misses=0,stored=0,budget_misses=0;
    uint64_t detail_hits=0,presentation_hits=0,verification_checks=0,verification_mismatches=0;
    std::array<uint64_t,16> key_changes{};
    uint64_t window_changes=0,clears=0;
};
// Keys include the lease, source revision, position/eye, loaded levels, viewport,
// tileset, renderer/device and native texture generation. Camera matrices are
// deliberately absent: permissions are evaluated in physical XYZ space.
using key=std::array<uint64_t,16>;
inline bool same_world_cell(const ncmm_world_cell_v1 &a,const ncmm_world_cell_v1 &b)noexcept{
    return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.flags==b.flags&&a.direction==b.direction&&
        a.reserved==b.reserved&&a.revision==b.revision&&
        std::equal(std::begin(a.lower_light),std::end(a.lower_light),std::begin(b.lower_light));
}
class cache {
    key key_{};
    int32_t ox_=0,oy_=0,lo_=0,hi_=-1;
    uint32_t side_=0;
    std::vector<uint32_t> slots;
    std::vector<entry> entries;
    size_t stored_entries=0;
    static constexpr uint32_t empty_slot=std::numeric_limits<uint32_t>::max();
    static constexpr size_t maximum_bytes=96u*1024u*1024u;
    size_t index(int32_t x,int32_t y,int32_t z)const noexcept{
        const int64_t dx=int64_t(x)-ox_,dy=int64_t(y)-oy_;
        if(!enabled||dx<0||dy<0||dx>=side_||dy>=side_||z<lo_||z>hi_)return slots.size();
        return (size_t(z-lo_)*side_+size_t(dy))*side_+size_t(dx);
    }
public:
    bool enabled=false,verify=false;
    counters totals{};
    void suspend()noexcept{enabled=false;}
    void clear()noexcept{
        ++totals.clears;
        enabled=false;key_={};ox_=oy_=lo_=0;hi_=-1;side_=0;stored_entries=0;
        std::vector<uint32_t>().swap(slots);std::vector<entry>().swap(entries);
    }
    bool prepare(const key &identity,int32_t ox,int32_t oy,int32_t lo,int32_t hi,uint32_t side){
        enabled=false;
        if(!side||side>97||hi<lo||int64_t(hi)-lo>=NCMM_WORLD_MAX_SLICES)return false;
        const size_t count=size_t(int64_t(hi)-lo+1)*side*side;
        if(identity!=key_||ox!=ox_||oy!=oy_||lo!=lo_||hi!=hi_||side!=side_){
            for(size_t i=0;i<identity.size();++i)totals.key_changes[i]+=identity[i]!=key_[i];
            totals.window_changes+=ox!=ox_||oy!=oy_||lo!=lo_||hi!=hi_||side!=side_;
            key_=identity;ox_=ox;oy_=oy;lo_=lo;hi_=hi;side_=side;
            slots.assign(count,0);stored_entries=0;++totals.generations;
        }
        enabled=true;return true;
    }
    bool known(int32_t x,int32_t y,int32_t z)const noexcept{
        const size_t at=index(x,y,z);return at<slots.size()&&slots[at]!=0;
    }
    entry *find(int32_t x,int32_t y,int32_t z)noexcept{
        const size_t at=index(x,y,z);if(at>=slots.size())return nullptr;
        const auto slot=slots[at];return slot&&slot!=empty_slot?&entries[slot-1]:nullptr;
    }
    const entry *find(int32_t x,int32_t y,int32_t z)const noexcept{
        return const_cast<cache *>(this)->find(x,y,z);
    }
    void reject(int32_t x,int32_t y,int32_t z)noexcept{
        const size_t at=index(x,y,z);if(at>=slots.size())return;
        if(verify&&slots[at]){++totals.verification_checks;if(slots[at]!=empty_slot)++totals.verification_mismatches;}
        if(!slots[at])slots[at]=empty_slot;
    }
    entry *store(const ncmm_world_cell_v1 &world,const std::array<float,3> &light){
        const size_t at=index(world.x,world.y,world.z);if(at>=slots.size())return nullptr;
        if(slots[at]==empty_slot){if(verify)++totals.verification_mismatches;return nullptr;}
        if(slots[at])return &entries[slots[at]-1];
        const size_t limit=std::min<size_t>(NCMM_WORLD_MAX_CELLS,(maximum_bytes-slots.capacity()*sizeof(uint32_t))/sizeof(entry));
        if(stored_entries>=limit){++totals.budget_misses;return nullptr;}
        if(stored_entries==entries.size()){
            if(entries.size()==entries.capacity()){
                const size_t capacity=std::min(limit,std::max<size_t>(256,entries.capacity()*2));
                entries.reserve(capacity);
            }
            entries.emplace_back();
        }
        // A new snapshot revokes all old field readiness. Reuse only owned
        // storage; each payload is overwritten before its ready flag is set.
        auto &out=entries[stored_entries++];out.world=world;out.facade_light=light;
        out.base_ready=out.detail_ready=out.presentation_ready=out.has_source_light=false;
        slots[at]=uint32_t(stored_entries);++totals.stored;return &out;
    }
    size_t bytes()const noexcept{return slots.capacity()*sizeof(uint32_t)+entries.capacity()*sizeof(entry);}
    size_t size()const noexcept{return stored_entries;}
};
// Only native, camera-independent rejection is remembered. Frustum rejection
// never sets this flag, so a new sector can always acquire missing data.
struct rejected_probe {
    cache *owner;
    int32_t x,y,z;
    bool remember=false;
    ~rejected_probe(){if(owner&&remember)owner->reject(x,y,z);}
};
}
