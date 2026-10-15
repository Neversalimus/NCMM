#pragma once
#include "ncmm_world_policy.hpp"
#include <cstring>
#include <vector>
namespace ncmm_world {
// A draw cannot mutate the avatar. Reuse the exact native sight_range result
// for repeated light levels, without retaining vision/equipment across frames.
// Direct-mapped collisions recompute; storage and work per lookup stay bounded.
class light_range_cache {
    struct entry {uint32_t key=0;int value=0;bool valid=false;};
    std::array<entry,128> entries{};
public:
    uint64_t evaluations=0,hits=0;
    template<class Read> int get(float light,Read read){
        uint32_t key;std::memcpy(&key,&light,sizeof(key));
        auto &slot=entries[(key^(key>>8)^(key>>16))&(entries.size()-1)];
        if(slot.valid&&slot.key==key){++hits;return slot.value;}
        const int value=read();++evaluations;slot={key,value,true};return value;
    }
};
// Derived geometry and roof facts are keyed by absolute XYZ and an explicit
// geometry revision. Preserve the canonical overlap when the avatar walks.
class geometry_cache {
    int32_t ox=0,oy=0,minz=0,maxz=-1;int width=0,height=0;uint64_t identity=0;
    std::vector<uint32_t> facts;std::vector<uint64_t> revisions;
    int slot(int x,int y,int z)const{const int64_t xx=int64_t(x)-ox,yy=int64_t(y)-oy;
        if(xx<0||yy<0||xx>=width||yy>=height||z<minz||z>maxz)return -1;
        return int((size_t(z-minz)*height+size_t(yy))*width+size_t(xx));}
public:
    uint64_t hits=0,misses=0;
    void clear(){*this=geometry_cache{};}
    bool prepare(int32_t x,int32_t y,int low,int high,int w,int h,uint64_t world){
        if(!world||w<1||h<1||w>97||h>97||low < -128||high > 128||low>high||int64_t(high)-low>=NCMM_WORLD_MAX_SLICES)return false;
        hits=misses=0;if(ox==x&&oy==y&&minz==low&&maxz==high&&width==w&&height==h&&identity==world)return true;
        const size_t count=size_t(high-low+1)*w*h;
        std::vector<uint32_t> next(count);std::vector<uint64_t> stamps(count);
        if(identity==world){const int64_t left=std::max(int64_t(x),int64_t(ox)),right=std::min(int64_t(x)+w,int64_t(ox)+width),
                top=std::max(int64_t(y),int64_t(oy)),bottom=std::min(int64_t(y)+h,int64_t(oy)+height);
            for(int z=std::max(low,minz);z<=std::min(high,maxz);++z)for(int64_t yy=top;yy<bottom;++yy){if(left>=right)continue;
                const size_t old=(size_t(z-minz)*height+size_t(yy-oy))*width+size_t(left-ox),at=(size_t(z-low)*h+size_t(yy-y))*w+size_t(left-x),n=size_t(right-left);
                std::copy_n(facts.begin()+old,n,next.begin()+at);std::copy_n(revisions.begin()+old,n,stamps.begin()+at);}}
        ox=x;oy=y;minz=low;maxz=high;width=w;height=h;identity=world;facts=std::move(next);revisions=std::move(stamps);return true;
    }
    template<class Read> uint32_t get(int x,int y,int z,uint64_t revision,Read read){const int i=slot(x,y,z);
        if(i<0)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED;
        if(revision&&revisions[i]==revision){++hits;return facts[i];}++misses;
        const uint32_t value=read();facts[i]=value;revisions[i]=revision;return value;
    }
    size_t bytes()const{return facts.capacity()*sizeof(uint32_t)+revisions.capacity()*sizeof(uint64_t);}
};
/* Main-thread private source facts; canonical intersections survive a shifted
 * viewport. No engine pointer, ID, item or entity is retained. */
class source_cache {
    int32_t ox=0,oy=0,minz=0,maxz=-1;int width=0,height=0;uint64_t identity=0;
    std::vector<uint32_t> facts;std::vector<uint64_t> revisions,ray_revisions;std::vector<uint8_t> rays;
    int slot(int x,int y,int z)const{const int64_t xx=int64_t(x)-ox,yy=int64_t(y)-oy;
        if(xx<0||yy<0||xx>=width||yy>=height||z<minz||z>maxz)return -1;
        return int((size_t(z-minz)*height+size_t(yy))*width+size_t(xx));}
public:
    uint64_t hits=0,misses=0,ray_hits=0,ray_misses=0,topology_revision=1;
    void clear(){*this=source_cache{};}
    bool prepare(int32_t x,int32_t y,int low,int high,int w,int h,uint64_t world){
        if(!world||w<1||h<1||w>97||h>97||low < -128||high > 128||low>high||int64_t(high)-low>=NCMM_WORLD_MAX_SLICES)return false;
        hits=misses=ray_hits=ray_misses=0;if(ox==x&&oy==y&&minz==low&&maxz==high&&width==w&&height==h&&identity==world)return true;
        const size_t count=size_t(high-low+1)*w*h;
        std::vector<uint32_t> next(count);std::vector<uint64_t> stamps(count),ray_stamps(count);std::vector<uint8_t> ray_values(count);
        if(identity==world){const int64_t left=std::max(int64_t(x),int64_t(ox)),right=std::min(int64_t(x)+w,int64_t(ox)+width),
                top=std::max(int64_t(y),int64_t(oy)),bottom=std::min(int64_t(y)+h,int64_t(oy)+height);
            for(int z=std::max(low,minz);z<=std::min(high,maxz);++z)for(int64_t yy=top;yy<bottom;++yy){if(left>=right)continue;
                const size_t old=(size_t(z-minz)*height+size_t(yy-oy))*width+size_t(left-ox),at=(size_t(z-low)*h+size_t(yy-y))*w+size_t(left-x),n=size_t(right-left);
                std::copy_n(facts.begin()+old,n,next.begin()+at);std::copy_n(revisions.begin()+old,n,stamps.begin()+at);
                std::copy_n(rays.begin()+old,n,ray_values.begin()+at);std::copy_n(ray_revisions.begin()+old,n,ray_stamps.begin()+at);}}
        ox=x;oy=y;minz=low;maxz=high;width=w;height=h;identity=world;
        facts=std::move(next);revisions=std::move(stamps);rays=std::move(ray_values);ray_revisions=std::move(ray_stamps);return true;}
    template<class Read> uint32_t get(int x,int y,int z,uint64_t revision,Read read){const int i=slot(x,y,z);
        if(i<0)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
        if(revision&&revisions[i]==revision){++hits;return facts[i];}++misses;const uint32_t value=read();
        if(!revisions[i]||facts[i]!=value)++topology_revision;
        facts[i]=value;revisions[i]=revision;return facts[i];}
    template<class Trace> uint8_t visibility(int x,int y,int z,uint64_t revision,Trace trace){const int i=slot(x,y,z);if(i<0)return 0;
        if(revision&&ray_revisions[i]==revision){++ray_hits;return rays[i];}++ray_misses;rays[i]=trace();ray_revisions[i]=revision;return rays[i];}
    size_t bytes()const{return facts.capacity()*sizeof(uint32_t)+(revisions.capacity()+ray_revisions.capacity())*sizeof(uint64_t)+rays.capacity();}
};
} // namespace ncmm_world
