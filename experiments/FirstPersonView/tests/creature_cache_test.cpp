#include "creature_cache.hpp"
#include <cstdio>
#include <cstdlib>
using namespace first_person::creature;
int main(){int n=0;auto check=[&](bool v){++n;if(!v){std::fprintf(stderr,"cache check %d failed\n",n);std::exit(1);}};
    ncmm_graphics_creature_v1 c{};c.liters=c.kilograms=70;std::strcpy(c.bodytype,"human");const auto key=appearance_key(c,1);c.entity_id=42;c.flags=NCMM_CR_DEAD;c.event_serial=123;check(key==appearance_key(c,1));check(key!=appearance_key(c,2));c.flags=0;
    cache resources;resources.begin_frame(1,1);int copies=0;auto source=[&]{++copies;return sprite{};};check(!resources.request(key,classify(c),source));check(!resources.request(key,classify(c),source));check(copies==1);check(resources.wait_idle(std::chrono::seconds(5)));
    auto first=resources.request(key,classify(c),source);check(bool(first)&&bool(first->atlas));check(copies==1);auto stats=resources.statistics();check(stats.generated==1&&stats.entries==1&&stats.bytes<cache::memory_limit);
    resources.begin_frame(2,1);check(resources.statistics().entries==0);check(!first->geometry.lod[0].faces.empty()); // external immutable snapshot stays safe
    for(int round=0;round<6;++round){resources.begin_frame(2,1);for(int i=0;i<100;++i)resources.request(uint64_t(100+round*100+i),classify(c),source);}
    check(resources.statistics().pending<=cache::queue_limit);resources.reset(3,2);check(resources.statistics().bytes==0);
    resources.shutdown();check(!resources.request(900,classify(c),source));
    scene s;s.view.struct_size=sizeof(s.view);s.view.count=1;c.cell=0;s.creatures={c};check(valid(s,1));s.creatures.push_back(c);s.view.count=2;check(!valid(s,1));
    std::printf("Creature cache: %d checks PASS\n",n);
}
