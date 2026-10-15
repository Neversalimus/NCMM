#include "ncmm_world_cache.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{
    using namespace ncmm_world;source_cache cache;int reads=0,checks=0;
    auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("source cache invariant");};
    {
        light_range_cache ranges;int calls=0;
        for(int n=0;n<20000;++n){const float light=n%2?0.f:120.f;
            check(ranges.get(light,[&]{++calls;return light==0?1:60;})==(light==0?1:60));}
        check(calls==2&&ranges.evaluations==2&&ranges.hits==19998);
        // Many unrelated float values force direct-map collisions. An old
        // result may never be reused for a different native light argument.
        light_range_cache collisions;
        for(int n=0;n<8192;++n){const float light=float(n)*.03125f;
            check(collisions.get(light,[&]{++calls;return n+1000;})==n+1000);}
        light_range_cache next_frame;
        check(next_frame.get(120.f,[]{return 7;})==7&&next_frame.evaluations==1);
        check(ranges.get(120.f,[]{return 60;})==60); // no changed avatar state crosses frames
    }
    {
        geometry_cache derived;int computations=0;
        auto value=[](int x,int y,int z){return uint32_t((x+200)*13+(y+200)*97+(z+20)*701);};
        auto visit=[&](int left,int top,int low,int high,int revision){
            for(int z=low;z<=high;++z)for(int y=top;y<top+9;++y)for(int x=left;x<left+9;++x)
                check(derived.get(x,y,z,revision,[&]{++computations;return value(x,y,z);})==value(x,y,z));
        };
        check(derived.prepare(-4,-4,-3,3,9,9,1));visit(-4,-4,-3,3,1);check(computations==567);
        check(derived.prepare(-3,-4,-3,3,9,9,1));visit(-3,-4,-3,3,1);check(computations==630);
        check(derived.prepare(-4,-3,-3,3,9,9,1));visit(-4,-3,-3,3,1);check(computations==749);
        visit(-4,-3,-3,3,2);check(computations==1316); // real geometry invalidation
        check(derived.prepare(-4,-3,-2,4,9,9,1));visit(-4,-3,-2,4,2);check(computations==1397);
        check(derived.prepare(-4,-3,-2,4,9,9,2));visit(-4,-3,-2,4,2);check(computations==1964); // another realm
        check(derived.get(INT32_MIN,INT32_MAX,0,2,[]{throw std::runtime_error("outside derived cache");return 0u;})&NCMM_WORLD_UNKNOWN);
        check(derived.prepare(0,0,-10,10,97,97,3));check(derived.bytes()<3u*1024u*1024u);
        check(!derived.prepare(0,0,0,32,97,97,3));check(!derived.prepare(0,0,INT32_MIN,INT32_MAX,9,9,3));
        check(!derived.prepare(0,0,0,1,1000000,97,3));derived.clear();check(derived.bytes()==0);
    }
    check(cache.prepare(1000000,-1000000,-4,4,3,3,1));
    for(int z=-4;z<=4;++z)for(int y=0;y<3;++y)for(int x=0;x<3;++x){const uint32_t expected=uint32_t((z+4)*9+y*3+x);
        check(cache.get(1000000+x,-1000000+y,z,1,[&]{++reads;return expected;})==expected);}
    check(reads==81);
    check(cache.prepare(1000001,-1000000,-4,4,3,3,1));
    for(int z=-4;z<=4;++z)for(int y=0;y<3;++y)for(int x=1;x<4;++x){const uint32_t expected=uint32_t((z+4)*9+y*3+x);
        check(cache.get(1000000+x,-1000000+y,z,1,[&]{++reads;return expected;})==expected);}
    check(reads==108); // 54 canonical cells reused, only the new strip read.
    check(cache.get(1000001,-1000000,0,2,[&]{++reads;return 999u;})==999);
    const auto topology=cache.topology_revision;
    check(cache.get(1000001,-1000000,0,3,[&]{++reads;return 999u;})==999&&cache.topology_revision==topology);
    check(cache.get(1000001,-1000000,0,4,[&]{++reads;return 998u;})==998&&cache.topology_revision==topology+1);
    check(cache.get(1000001,-1000000,-1,1,[&]{throw std::runtime_error("other floor invalidated");return 0u;})!=999);
    int rays=0;check(cache.visibility(1000001,-1000000,0,1,[&]{++rays;return uint8_t(7);})==7);
    check(cache.visibility(1000001,-1000000,0,1,[&]{++rays;return uint8_t(0);})==7&&rays==1);
    check(cache.visibility(1000001,-1000000,0,2,[&]{++rays;return uint8_t(0);})==0&&rays==2);
    check(cache.prepare(1000001,-1000000,-4,4,3,3,2));
    check(cache.get(1000001,-1000000,0,1,[&]{++reads;return 42u;})==42); // dimensions cannot share data by Z.
    check(cache.get(INT32_MIN,INT32_MAX,0,1,[]{throw std::runtime_error("out of bounds read");return 0u;})&NCMM_WORLD_UNKNOWN);
    check(cache.prepare(0,0,-10,10,97,97,3));check(cache.bytes()<8u*1024u*1024u);
    check(!cache.prepare(0,0,0,32,97,97,3));check(!cache.prepare(0,0,INT32_MIN,INT32_MAX,3,3,3));
    check(!cache.prepare(0,0,0,1,1000000,97,3));cache.clear();check(cache.bytes()==0);
    std::cout<<"Multi-level source cache: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
