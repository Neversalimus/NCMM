#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

// Private qualification only. A null binding performs no clock reads, retains
// no packet and allocates nothing. No public GPU ABI or resource policy changes.
namespace ncmm_gpu_resource_diagnostics {
enum class phase : std::size_t { other_cpu, mesh_data_cpu, texture_create,
    vertex_create, index_create, transfer_create, transfer_map, transfer_copy_cpu,
    transfer_unmap, upload_acquire, upload_record, upload_submit_release, count };
constexpr std::size_t phase_count=static_cast<std::size_t>(phase::count);
inline constexpr std::array<const char *,phase_count> names={"other_cpu","mesh_data_cpu",
    "texture_create","vertex_create","index_create","transfer_create","transfer_map",
    "transfer_copy_cpu","transfer_unmap","upload_acquire","upload_record","upload_submit_release"};
struct interval {uint64_t wall_ns=0,thread_ns=0,segments=0;};
struct packet {
    std::array<interval,phase_count> phases{};
    uint64_t wall_ns=0,thread_ns=0,captures=0;
    bool complete=false,valid=false,thread_available=false;
};
using thread_reader=int64_t (*)(); // Negative means unavailable, never process CPU.
struct binding {packet *output=nullptr;thread_reader thread=nullptr;};
inline thread_local binding active{};
class binding_guard {
    binding previous_;
public:
    explicit binding_guard(binding next):previous_(active){if(next.output)*next.output={};active=next;}
    binding_guard(const binding_guard &)=delete;binding_guard &operator=(const binding_guard &)=delete;
    ~binding_guard(){active=previous_;}
};
struct stamp {uint64_t wall_ns=0;int64_t thread_ns=-1;};
using stamp_reader=stamp (*)(thread_reader);
inline stamp now(thread_reader thread){
    const auto wall=std::chrono::steady_clock::now().time_since_epoch();
    return {uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(wall).count()),thread?thread():-1};
}
class capture {
    binding target_;stamp_reader reader_;stamp previous_{};
    phase active_=phase::other_cpu;bool good_=true,thread_good_=true,finished_=false;
    bool add(uint64_t &to,uint64_t value){
        if(value>std::numeric_limits<uint64_t>::max()-to){good_=false;return false;}
        to+=value;return true;
    }
    void flush(){
        const auto next=reader_(target_.thread);auto &p=*target_.output;
        auto &part=p.phases[static_cast<std::size_t>(active_)];
        if(next.wall_ns<previous_.wall_ns)good_=false;
        else{const auto delta=next.wall_ns-previous_.wall_ns;add(part.wall_ns,delta);add(p.wall_ns,delta);}
        if(previous_.thread_ns<0||next.thread_ns<previous_.thread_ns)thread_good_=false;
        else if(thread_good_){const auto delta=uint64_t(next.thread_ns-previous_.thread_ns);add(part.thread_ns,delta);add(p.thread_ns,delta);}
        previous_=next;
    }
public:
    explicit capture(binding target,stamp_reader reader=now):target_(target),reader_(reader){
        if(target_.output){const auto count=target_.output->captures;*target_.output={};
            if(count==std::numeric_limits<uint64_t>::max())good_=false;
            else target_.output->captures=count+1;
            previous_=reader_(target_.thread);
            thread_good_=previous_.thread_ns>=0;target_.output->phases[0].segments=1;}
    }
    capture(const capture &)=delete;capture &operator=(const capture &)=delete;
    ~capture(){finish(false);}
    phase select(phase value){
        const auto prior=active_;
        if(target_.output&&!finished_){flush();active_=value;
            add(target_.output->phases[static_cast<std::size_t>(value)].segments,1);}
        return prior;
    }
    void finish(bool complete){
        if(!target_.output||finished_)return;
        flush();auto &p=*target_.output;
        p.complete=complete;p.valid=good_;p.thread_available=thread_good_;
        if(!thread_good_){p.thread_ns=0;for(auto &part:p.phases)part.thread_ns=0;}
        finished_=true;
    }
};
class span {
    capture &owner_;phase previous_;
public:
    span(capture &owner,phase value):owner_(owner),previous_(owner.select(value)){}
    span(const span &)=delete;span &operator=(const span &)=delete;
    ~span(){owner_.select(previous_);}
};
}
