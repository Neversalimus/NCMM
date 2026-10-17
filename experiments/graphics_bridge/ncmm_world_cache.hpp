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
// Terrain/furniture definitions are immutable during one main-thread draw.
// Cache their combined semantics, never fields, vehicles, items or actors.
// Borrowed identities are integer keys and cannot survive the callback.
class material_semantics_cache {
    struct entry {std::uintptr_t terrain=0,furniture=0;uint32_t value=0;bool normal_floor=false,valid=false;};
    std::array<entry,256> entries{};
public:
    uint64_t evaluations=0,hits=0;
    template<class Read> uint32_t get(std::uintptr_t terrain,std::uintptr_t furniture,bool normal_floor,Read read){
        uint64_t key=(uint64_t(terrain)>>3)^(uint64_t(furniture)<<1);
        key^=key>>33;key*=0xff51afd7ed558ccdull;key^=key>>33;
        auto &slot=entries[(key^(normal_floor?0x9e3779b9u:0u))&(entries.size()-1)];
        if(slot.valid&&slot.terrain==terrain&&slot.furniture==furniture&&slot.normal_floor==normal_floor){++hits;return slot.value;}
        const uint32_t value=read();++evaluations;slot={terrain,furniture,value,normal_floor,true};return value;
    }
};
// Immutable definition results live for one main-thread extraction only.
// Integer identities are never dereferenced or retained into another draw.
class definition_cache {
    struct entry {std::uintptr_t key=0;uint32_t value=0;bool valid=false;};
    std::array<entry,256> entries{};
public:
    uint64_t evaluations=0,hits=0;
    template<class Read> uint32_t get(std::uintptr_t key,Read read){
        uint64_t hash=uint64_t(key);hash^=hash>>33;hash*=0xff51afd7ed558ccdull;hash^=hash>>33;
        auto &slot=entries[hash&(entries.size()-1)];
        if(slot.valid&&slot.key==key){++hits;return slot.value;}
        const uint32_t value=read();++evaluations;slot={key,value,true};return value;
    }
};
// A closed plane is a geometric proof only when every tile in the bounded
// source window has a full opaque slab. Open stairs, glass and water fail it.
class sealed_plane_cache {
    int32_t ox=0,oy=0;int low=0,high=-1,width=0,height=0;uint64_t world=0,revision=0;
    std::array<uint8_t,NCMM_WORLD_MAX_SLICES> state{};
    std::array<std::vector<uint32_t>,NCMM_WORLD_MAX_SLICES> openings;
public:
    void clear(){*this=sealed_plane_cache{};}
    bool prepare(int32_t x,int32_t y,int minz,int maxz,int w,int h,uint64_t epoch,uint64_t changed){
        if(!epoch||!changed||w<1||h<1||w>97||h>97||minz < -128||maxz>128||minz>maxz||int64_t(maxz)-minz>=NCMM_WORLD_MAX_SLICES||
           int64_t(x)+w-1>INT32_MAX||int64_t(y)+h-1>INT32_MAX){clear();return false;}
        if(ox!=x||oy!=y||low!=minz||high!=maxz||width!=w||height!=h||world!=epoch||revision!=changed){
            state.fill(0);for(auto &plane:openings)plane.clear();
        }
        ox=x;oy=y;low=minz;high=maxz;width=w;height=h;world=epoch;revision=changed;return true;
    }
    template<class Read> bool closed(int z,Read read){
        if(!world||z<low||z>high)return false;auto &cached=state[size_t(z-low)];if(cached)return cached==2;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(!full_opaque_slab(read(ox+x,oy+y,z))){cached=1;return false;}
        cached=2;return true;
    }
    // Every segment between the eye and a cell stays inside their XY bounding
    // rectangle. A full slab at every tile in that rectangle therefore blocks
    // the entire lower/higher voxel, even when an unrelated distant stair or
    // window prevents the whole source plane from being sealed.
    template<class Read> bool closed_rectangle(int z,int32_t left,int32_t top,int32_t right,int32_t bottom,Read read){
        if(!world||z<low||z>high||left>right||top>bottom)return false;
        const int64_t x0=int64_t(left)-ox,y0=int64_t(top)-oy,x1=int64_t(right)-ox,y1=int64_t(bottom)-oy;
        if(x0<0||y0<0||x1>=width||y1>=height)return false;
        const size_t level=size_t(z-low);if(state[level]==2)return true;
        auto &prefix=openings[level];const size_t stride=size_t(width)+1;
        if(prefix.empty()){
            prefix.assign(stride*(size_t(height)+1),0);
            for(int y=0;y<height;++y){uint32_t row=0;
                for(int x=0;x<width;++x){row+=full_opaque_slab(read(ox+x,oy+y,z))?0u:1u;
                    prefix[size_t(y+1)*stride+size_t(x+1)]=prefix[size_t(y)*stride+size_t(x+1)]+row;}}
            state[level]=prefix.back()?1:2;
        }
        const size_t a=size_t(y0)*stride+size_t(x0),b=size_t(y0)*stride+size_t(x1+1),
                     c=size_t(y1+1)*stride+size_t(x0),d=size_t(y1+1)*stride+size_t(x1+1);
        return uint64_t(prefix[d])+prefix[a]==uint64_t(prefix[b])+prefix[c];
    }
    size_t bytes()const{size_t total=sizeof(state);for(const auto &plane:openings)total+=plane.capacity()*sizeof(uint32_t);return total;}
};
// Derived geometry and roof facts are keyed by absolute XYZ and an explicit
// geometry revision. Walking rotates fixed storage and invalidates entering
// strips. A changed shape/Z window still preserves its canonical overlap.
class geometry_cache {
    int32_t ox=0,oy=0,minz=0,maxz=-1;int width=0,height=0,head_x=0,head_y=0;uint64_t identity=0;
    std::vector<uint32_t> facts;std::vector<uint64_t> revisions;
    int slot(int x,int y,int z)const{const int64_t xx=int64_t(x)-ox,yy=int64_t(y)-oy;
        if(xx<0||yy<0||xx>=width||yy>=height||z<minz||z>maxz)return -1;
        return int(relative_slot(int(xx),int(yy),z-minz));}
    size_t relative_slot(int x,int y,int level)const{
        int column=x+head_x,row=y+head_y;if(column>=width)column-=width;if(row>=height)row-=height;
        return (size_t(level)*height+size_t(row))*width+size_t(column);
    }
public:
    uint64_t hits=0,misses=0;
    void clear(){*this=geometry_cache{};}
    bool prepare(int32_t x,int32_t y,int low,int high,int w,int h,uint64_t world){
        if(!world||w<1||h<1||w>97||h>97||low < -128||high > 128||low>high||int64_t(high)-low>=NCMM_WORLD_MAX_SLICES)return false;
        hits=misses=0;
        if(ox==x&&oy==y&&minz==low&&maxz==high&&width==w&&height==h&&identity==world)return true;
        const int64_t dx=int64_t(x)-ox,dy=int64_t(y)-oy;
        const bool retained=identity==world&&minz==low&&maxz==high&&width==w&&height==h&&std::abs(dx)<w&&std::abs(dy)<h;
        if(retained){
            head_x=(head_x+int(dx)+w)%w;head_y=(head_y+int(dy)+h)%h;ox=x;oy=y;
            auto invalidate=[&](int gx,int gy,int z){const size_t at=relative_slot(gx,gy,z-minz);revisions[size_t(at)]=0;};
            const int left=dx>0?w-int(dx):0,right=dx>0?w:int(-dx),top=dy>0?h-int(dy):0,bottom=dy>0?h:int(-dy);
            if(dx)for(int z=low;z<=high;++z)for(int gy=0;gy<h;++gy)for(int gx=left;gx<right;++gx)invalidate(gx,gy,z);
            if(dy)for(int z=low;z<=high;++z)for(int gy=top;gy<bottom;++gy)for(int gx=0;gx<w;++gx)invalidate(gx,gy,z);
            return true;
        }
        const size_t count=size_t(high-low+1)*w*h;std::vector<uint32_t> next(count);std::vector<uint64_t> stamps(count);
        if(identity==world){const int64_t left=std::max(int64_t(x),int64_t(ox)),right=std::min(int64_t(x)+w,int64_t(ox)+width),
                top=std::max(int64_t(y),int64_t(oy)),bottom=std::min(int64_t(y)+h,int64_t(oy)+height);
            for(int z=std::max(low,minz);z<=std::min(high,maxz);++z)for(int64_t yy=top;yy<bottom;++yy)for(int64_t xx=left;xx<right;++xx){
                const size_t old=relative_slot(int(xx-ox),int(yy-oy),z-minz),at=(size_t(z-low)*h+size_t(yy-y))*w+size_t(xx-x);
                next[at]=facts[old];stamps[at]=revisions[old];}}
        ox=x;oy=y;minz=low;maxz=high;width=w;height=h;identity=world;head_x=head_y=0;
        facts=std::move(next);revisions=std::move(stamps);return true;
    }
    template<class Read> uint32_t get(int x,int y,int z,uint64_t revision,Read read){const int i=slot(x,y,z);
        if(i<0)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED;
        if(revision&&revisions[i]==revision){++hits;return facts[i];}++misses;
        const uint32_t value=read();facts[i]=value;revisions[i]=revision;return value;
    }
    // X/Y and level are already relative to the prepared window. Ray traversal
    // avoids converting them to absolute coordinates and back for every voxel.
    template<class Read> uint32_t get_relative(int x,int y,int level,uint64_t revision,Read read){
        if(x<0||y<0||x>=width||y>=height||level<0||level>maxz-minz)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE|NCMM_WORLD_ROOFED;
        const size_t i=relative_slot(x,y,level);
        if(revision&&revisions[i]==revision){++hits;return facts[i];}++misses;
        const uint32_t value=read();facts[i]=value;revisions[i]=revision;return value;
    }
    size_t bytes()const{return facts.capacity()*sizeof(uint32_t)+revisions.capacity()*sizeof(uint64_t);}
};
/* Main-thread private source facts; canonical intersections survive a shifted
 * viewport. No engine pointer, ID, item or entity is retained. */
class source_cache {
    int32_t ox=0,oy=0,minz=0,maxz=-1;int width=0,height=0,head_x=0,head_y=0;uint64_t identity=0;
    std::vector<uint32_t> facts;std::vector<uint64_t> revisions,ray_revisions;std::vector<uint16_t> rays;
    int slot(int x,int y,int z)const{const int64_t xx=int64_t(x)-ox,yy=int64_t(y)-oy;
        if(xx<0||yy<0||xx>=width||yy>=height||z<minz||z>maxz)return -1;
        return int(relative_slot(int(xx),int(yy),z-minz));}
    size_t relative_slot(int x,int y,int level)const{
        int column=x+head_x,row=y+head_y;if(column>=width)column-=width;if(row>=height)row-=height;
        return (size_t(level)*height+size_t(row))*width+size_t(column);
    }
public:
    uint64_t hits=0,misses=0,ray_hits=0,ray_misses=0,topology_revision=1;
    void clear(){*this=source_cache{};}
    bool prepare(int32_t x,int32_t y,int low,int high,int w,int h,uint64_t world){
        if(!world||w<1||h<1||w>97||h>97||low < -128||high > 128||low>high||int64_t(high)-low>=NCMM_WORLD_MAX_SLICES)return false;
        hits=misses=ray_hits=ray_misses=0;
        if(ox==x&&oy==y&&minz==low&&maxz==high&&width==w&&height==h&&identity==world)return true;
        const int64_t dx=int64_t(x)-ox,dy=int64_t(y)-oy;
        const bool retained=identity==world&&minz==low&&maxz==high&&width==w&&height==h&&std::abs(dx)<w&&std::abs(dy)<h;
        if(retained){
            head_x=(head_x+int(dx)+w)%w;head_y=(head_y+int(dy)+h)%h;ox=x;oy=y;
            auto invalidate=[&](int gx,int gy,int z){const size_t at=relative_slot(gx,gy,z-minz);revisions[size_t(at)]=0;ray_revisions[size_t(at)]=0;};
            const int left=dx>0?w-int(dx):0,right=dx>0?w:int(-dx),top=dy>0?h-int(dy):0,bottom=dy>0?h:int(-dy);
            if(dx)for(int z=low;z<=high;++z)for(int gy=0;gy<h;++gy)for(int gx=left;gx<right;++gx)invalidate(gx,gy,z);
            if(dy)for(int z=low;z<=high;++z)for(int gy=top;gy<bottom;++gy)for(int gx=0;gx<w;++gx)invalidate(gx,gy,z);
            return true;
        }
        const size_t count=size_t(high-low+1)*w*h;std::vector<uint32_t> next(count);std::vector<uint64_t> stamps(count),ray_stamps(count);std::vector<uint16_t> ray_values(count);
        if(identity==world){const int64_t left=std::max(int64_t(x),int64_t(ox)),right=std::min(int64_t(x)+w,int64_t(ox)+width),
                top=std::max(int64_t(y),int64_t(oy)),bottom=std::min(int64_t(y)+h,int64_t(oy)+height);
            for(int z=std::max(low,minz);z<=std::min(high,maxz);++z)for(int64_t yy=top;yy<bottom;++yy)for(int64_t xx=left;xx<right;++xx){
                const size_t old=relative_slot(int(xx-ox),int(yy-oy),z-minz),at=(size_t(z-low)*h+size_t(yy-y))*w+size_t(xx-x);
                next[at]=facts[old];stamps[at]=revisions[old];ray_values[at]=rays[old];ray_stamps[at]=ray_revisions[old];}}
        ox=x;oy=y;minz=low;maxz=high;width=w;height=h;identity=world;head_x=head_y=0;
        facts=std::move(next);revisions=std::move(stamps);rays=std::move(ray_values);ray_revisions=std::move(ray_stamps);return true;
    }
    template<class Read> uint32_t get(int x,int y,int z,uint64_t revision,Read read){const int i=slot(x,y,z);
        if(i<0)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
        if(revision&&revisions[i]==revision){++hits;return facts[i];}++misses;const uint32_t value=read();
        if(!revisions[i]||facts[i]!=value)++topology_revision;
        facts[i]=value;revisions[i]=revision;return facts[i];}
    template<class Trace> uint16_t visibility(int x,int y,int z,uint64_t revision,Trace trace){const int i=slot(x,y,z);if(i<0)return 0;
        if(revision&&ray_revisions[i]==revision){++ray_hits;return rays[i];}++ray_misses;rays[i]=trace();ray_revisions[i]=revision;return rays[i];}
    size_t bytes()const{return facts.capacity()*sizeof(uint32_t)+(revisions.capacity()+ray_revisions.capacity())*sizeof(uint64_t)+rays.capacity()*sizeof(uint16_t);}
};
} // namespace ncmm_world
