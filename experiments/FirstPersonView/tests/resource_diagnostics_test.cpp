#include "ncmm_gpu_resource_diagnostics.hpp"
#include <cstdio>
#include <stdexcept>

namespace diag=ncmm_gpu_resource_diagnostics;
namespace {
uint64_t ticks=0,reads=0,checks=0;
int64_t thread_ticks=0;
diag::stamp fake_now(diag::thread_reader){++reads;return {ticks,thread_ticks};}
void check(bool value){++checks;if(!value)throw std::runtime_error("resource diagnostic guard failed");}
void advance(uint64_t wall,int64_t thread){ticks+=wall;thread_ticks+=thread;}
void partition(const diag::packet &p){
    uint64_t wall=0,thread=0;
    for(const auto &part:p.phases){wall+=part.wall_ns;thread+=part.thread_ns;
        check(part.segments||(!part.wall_ns&&!part.thread_ns));}
    check(wall==p.wall_ns&&thread==p.thread_ns);
}
const diag::interval &part(const diag::packet &p,diag::phase phase){return p.phases[static_cast<std::size_t>(phase)];}
}
int main(){
    static_assert(diag::phase_count==12&&sizeof(diag::packet)<512,"bounded stack-only packet");
    // A null ordinary binding must not read either clock, even for nested work.
    {diag::capture disabled({},fake_now);{diag::span outer(disabled,diag::phase::mesh_data_cpu);
        diag::span inner(disabled,diag::phase::vertex_create);}disabled.finish(true);}
    check(reads==0);
    // Independent expected intervals: 11other, 23mesh, 17vertex, 31copy.
    for(uint64_t scale=1;scale<=128;++scale){ticks=0;thread_ticks=0;diag::packet p;
        {diag::capture capture({&p,nullptr},fake_now);advance(5*scale,2);
            {diag::span mesh(capture,diag::phase::mesh_data_cpu);advance(7*scale,3);
                {diag::span vertex(capture,diag::phase::vertex_create);advance(17*scale,4);}
                advance(16*scale,5);}
            advance(6*scale,6);
            {diag::span copy(capture,diag::phase::transfer_copy_cpu);advance(31*scale,7);}
            capture.finish(true);const auto old_reads=reads;capture.finish(false);check(reads==old_reads);}
        check(p.complete&&p.valid&&p.thread_available&&p.captures==1);
        check(p.wall_ns==82*scale&&p.thread_ns==27);
        check(part(p,diag::phase::other_cpu).wall_ns==11*scale);
        check(part(p,diag::phase::mesh_data_cpu).wall_ns==23*scale);
        check(part(p,diag::phase::vertex_create).wall_ns==17*scale);
        check(part(p,diag::phase::transfer_copy_cpu).wall_ns==31*scale);
        check(part(p,diag::phase::mesh_data_cpu).segments==2);
        partition(p);
    }
    // Unfinished/error paths are never presented as a completed preparation.
    diag::packet failed;ticks=0;thread_ticks=0;
    {diag::capture capture({&failed,nullptr},fake_now);advance(13,9);}
    check(!failed.complete&&failed.valid&&failed.wall_ns==13);partition(failed);
    // Reused output retains its invocation count; a second prepare cannot pass
    // the Native/parser exactly-one freshness gate by overwriting a first try.
    {diag::capture capture({&failed,nullptr},fake_now);advance(3,1);capture.finish(true);}
    check(failed.captures==2&&failed.complete&&failed.wall_ns==3);partition(failed);
    // Coarse CPU accounting can exceed a short wall interval. Never call the
    // difference a wait, clamp it to wall, or substitute whole-process time.
    diag::packet coarse;ticks=0;thread_ticks=0;
    {diag::capture capture({&coarse,nullptr},fake_now);advance(1,1000000);capture.finish(true);}
    check(coarse.thread_available&&coarse.thread_ns==1000000&&coarse.wall_ns==1);partition(coarse);
    for(int failure=0;failure<3;++failure){diag::packet p;ticks=100;thread_ticks=failure==0?-1:100;
        {diag::capture capture({&p,nullptr},fake_now);advance(7,8);
            {diag::span scope(capture,diag::phase::transfer_map);advance(9,4);
                if(failure==1)thread_ticks=-1;
                if(failure==2)thread_ticks=2;}
            advance(11,1);capture.finish(true);}
        check(p.complete&&p.valid&&!p.thread_available&&p.thread_ns==0&&p.wall_ns==27);
        for(const auto &v:p.phases)check(v.thread_ns==0);
        partition(p);
    }
    diag::packet backwards;ticks=100;thread_ticks=0;
    {diag::capture capture({&backwards,nullptr},fake_now);ticks=99;capture.finish(true);}
    check(!backwards.valid);
    diag::packet overflow;overflow.captures=std::numeric_limits<uint64_t>::max();
    {diag::capture capture({&overflow,nullptr},fake_now);capture.finish(true);}
    check(!overflow.valid);
    // Binding restoration must survive nested disabled scopes and exceptions.
    diag::packet outer,inner;
    check(diag::active.output==nullptr);
    {diag::binding_guard guard({&outer,nullptr});check(diag::active.output==&outer);
        try{diag::binding_guard nested({&inner,nullptr});check(diag::active.output==&inner);
            {diag::binding_guard disabled({});check(diag::active.output==nullptr);}
            check(diag::active.output==&inner);throw 1;
        }catch(int){}
        check(diag::active.output==&outer);
        {diag::capture capture(diag::active,fake_now);advance(1,1);capture.finish(true);}
        check(outer.captures==1&&inner.captures==0);}
    check(diag::active.output==nullptr);
    std::printf("RESOURCE_DIAGNOSTIC_CHECKS,%llu\n",static_cast<unsigned long long>(checks));
}
