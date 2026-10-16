#include "ncmm_world_cache.hpp"
#include <iostream>
#include <stdexcept>
int main(){try{
    using namespace ncmm_world;source_cache cache;int reads=0,checks=0;
    auto check=[&](bool v){++checks;if(!v)throw std::runtime_error("source cache invariant");};
    {
        geometry_cache memo;check(memo.prepare(1000000,-1000000,-2,3,17,17,1));int calls=0;
        auto facts=[](voxel p){return uint32_t((p.x==9&&p.y!=8?NCMM_WORLD_OPAQUE:0)|((p.z<=0||((p.x+p.y)%7==0))?NCMM_WORLD_FLOOR:0)|(p.x==4&&p.y==4?opaque_field_fact:0));};
        auto direct=[&](voxel p){if(p.x<0||p.y<0||p.x>=17||p.y>=17||p.z< -2||p.z>3)return uint32_t(NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED);return facts(p);};
        auto cached=[&](voxel p){return memo.get_relative(p.x,p.y,p.z+2,1,[&]{++calls;return facts(p);});};
        uint32_t seed=9731;auto random=[&](){seed=seed*1664525u+1013904223u;return seed;};
        for(int n=0;n<12000;++n){point eye{8.5f,8.5f,1.55f},target{float(random()%190)/10-1,float(random()%190)/10-1,float(int(random()%175)-50)/10};
            check(visible(eye,target,direct,n%2)==visible(eye,target,cached,n%2));
            voxel cell{int(random()%19)-1,int(random()%19)-1,int(random()%6)-2};
            check(facade_faces(eye,cell,direct)==facade_faces(eye,cell,cached));
        }
        check(calls<=17*17*6&&memo.bytes()==17*17*6*(sizeof(uint32_t)+sizeof(uint64_t)));
        check(cached({0,0,0})==facts({0,0,0}));
        check(memo.get_relative(0,0,2,2,[]{return 0u;})==0); // changed geometry revision is authoritative
        check(memo.get(1000000,-1000000,0,2,[]{return 99u;})==0); // both coordinate paths share facts
        geometry_cache invalid;check(!invalid.prepare(0,0,INT32_MIN,INT32_MAX,1000000,97,1));check(invalid.bytes()==0);
        check(invalid.get_relative(0,0,0,1,[]{throw std::runtime_error("invalid draw query");return 0u;})&NCMM_WORLD_UNKNOWN);
        geometry_cache maximum;check(maximum.prepare(0,0,-10,10,97,97,1));check(maximum.bytes()<3u*1024u*1024u);
    }
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
        material_semantics_cache materials;int calls=0;
        for(int n=0;n<120000;++n){const std::uintptr_t terrain=16+std::uintptr_t(n%3)*256,furniture=32+std::uintptr_t(n%2)*512;const bool normal=n%5!=0;
            const uint32_t expected=uint32_t(terrain*13+furniture*97+(normal?701:0));
            check(materials.get(terrain,furniture,normal,[&]{++calls;return expected;})==expected);
        }
        check(calls==12&&materials.evaluations==12&&materials.hits==119988);
        material_semantics_cache collisions;
        for(int n=0;n<4096;++n){const std::uintptr_t terrain=(std::uintptr_t(n)<<12)|16;
            check(collisions.get(terrain,32,false,[&]{return uint32_t(n+1000);})==uint32_t(n+1000));}
        check(collisions.get((std::uintptr_t(4095)<<12)|16,32,false,[]{throw std::runtime_error("material cache failed repeat");return 0u;})==5095);
        material_semantics_cache next_draw;
        check(next_draw.get(16,32,true,[]{return 7u;})==7); // definitions cannot leak between draws/worlds
        check(next_draw.get(16,32,false,[]{return 11u;})==11); // bottom/no-Z floor policy is distinct
        check(next_draw.get(UINTPTR_MAX,UINTPTR_MAX,true,[]{return UINT32_MAX;})==UINT32_MAX);
    }
    {
        sealed_plane_cache sealed;int reads=0;bool hole=false;
        auto plane=[&](int x,int y,int z){++reads;return hole&&x==4&&y==4&&z==0?uint32_t(NCMM_WORLD_DOWN):uint32_t(NCMM_WORLD_FLOOR);};
        check(sealed.prepare(0,0,-2,3,9,9,1,1));check(sealed.closed(0,plane)&&reads==81);
        check(sealed.closed(0,plane)&&reads==81); // repeated camera frames reuse the proof
        hole=true;check(sealed.prepare(0,0,-2,3,9,9,1,2));check(!sealed.closed(0,plane)&&reads==122);
        check(!sealed.closed(0,plane)&&reads==122); // negative proofs are memoized too
        hole=false;check(sealed.prepare(1,0,-2,3,9,9,1,2));check(sealed.closed(0,plane)&&reads==203);
        check(!sealed.closed(-3,plane)&&reads==203);
        check(sealed.prepare(1,0,-2,3,9,9,2,2));check(sealed.closed(0,plane)&&reads==284);
        check(!sealed.prepare(INT32_MAX,0,0,0,9,9,2,2));check(!sealed.closed(0,plane)&&reads==284);
        for(uint32_t opening:{0u,uint32_t(NCMM_WORLD_DOWN),uint32_t(NCMM_WORLD_RAMP),uint32_t(NCMM_WORLD_WATER),uint32_t(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR)}){
            check(sealed.prepare(0,0,0,0,1,1,3,opening+1));check(!sealed.closed(0,[&](int,int,int){return opening;}));
        }
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
    int face_mask_reads=0;check(cache.visibility(1000001,-1000000,0,77,[&]{++face_mask_reads;return uint16_t(0x3ff);})==0x3ff);
    check(cache.visibility(1000001,-1000000,0,77,[&]{++face_mask_reads;return uint16_t(0);})==0x3ff&&face_mask_reads==1);
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
