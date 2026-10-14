#pragma once
#include "ncmm_graphics_multilevel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>

namespace ncmm_world {
struct point {float x=0,y=0,z=0;};
struct voxel {int x=0,y=0,z=0;};
inline bool source_window_fits(int32_t x,int32_t y,int32_t radius){
    if(radius<0||radius>NCMM_GRAPHICS_V2_RADIUS)return false;
    const int64_t border=int64_t(radius)+1; // Direction and supercover neighbours.
    return int64_t(x)-border>=INT32_MIN&&int64_t(x)+border<=INT32_MAX&&
           int64_t(y)-border>=INT32_MIN&&int64_t(y)+border<=INT32_MAX;
}
// Private source facts distinguish an opaque gas from solid architecture.
// Structural surfaces can continue behind a volume without publishing bodies.
constexpr uint32_t opaque_field_fact=1u<<31;
inline uint32_t structural_flags(uint32_t flags){return flags&opaque_field_fact?flags&~(NCMM_WORLD_OPAQUE|opaque_field_fact):flags;}
inline bool operator==(voxel a,voxel b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
struct traits {
    bool loaded=false,no_floor=false,no_floor_water=false,transparent_floor=false,z_transparent=false;
    bool opaque=false,goes_up=false,goes_down=false,ramp=false,ramp_up=false,ramp_down=false;
    bool difficult_z=false,canopy=false,roofed=false,ramp_end=false;
};
inline uint32_t semantics(const traits &t){
    if(!t.loaded)return NCMM_WORLD_UNKNOWN|NCMM_WORLD_FLOOR|NCMM_WORLD_OPAQUE;
    uint32_t out=t.opaque?NCMM_WORLD_OPAQUE:0;
    const bool ramp=t.ramp||t.ramp_up||t.ramp_down;
    // CDDA has_floor and its visual floor_cache are different contracts.
    // GOES_DOWN opens the visual shaft although it remains a walkable landing.
    // TRANSPARENT_FLOOR remains a physical slab and is not an empty air tile.
    if(!t.no_floor&&!t.no_floor_water&&!t.goes_down&&!t.ramp_down)out|=NCMM_WORLD_FLOOR;
    if(t.transparent_floor&&!ramp)out|=NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR;
    if(t.no_floor||t.goes_down||t.no_floor_water)out|=NCMM_WORLD_OPEN_DOWN;
    if(t.no_floor_water)out|=NCMM_WORLD_WATER;
    if(t.goes_up||t.ramp||t.ramp_up)out|=NCMM_WORLD_UP;
    if(t.goes_down||t.ramp_down)out|=NCMM_WORLD_DOWN;
    if(ramp)out|=NCMM_WORLD_RAMP;
    if(t.ramp_end)out|=NCMM_WORLD_RAMP_END;
    if(t.difficult_z&&(t.goes_up||t.goes_down))out|=NCMM_WORLD_LADDER;
    if(t.canopy)out|=NCMM_WORLD_CANOPY;
    if(t.roofed)out|=NCMM_WORLD_ROOFED;
    // Z_TRANSPARENT alone is not used by either pinned engine's floor cache;
    // preserve the physical slab unless TRANSPARENT_FLOOR/NO_FLOOR also applies.
    return out;
}
inline auto order(const ncmm_world_cell_v1 &c){return std::make_tuple(c.z,c.y,c.x);}
inline bool finite(point p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool valid(const ncmm_graphics_multilevel_scene_v1 &s){
    const auto &b=s.appearance.base;
    if(s.struct_size<sizeof(s)||s.version!=1||!s.world_epoch||
       s.cell_count>NCMM_WORLD_MAX_CELLS||!s.slice_count||s.slice_count>NCMM_WORLD_MAX_SLICES||
       s.cell_count!=b.cell_count||s.cell_count!=s.appearance.detail_count||
       (s.cell_count&&(!s.coordinates||!b.cells||!s.appearance.details))||!s.slices||
       s.loaded_min_z < -128||s.loaded_max_z > 128||s.loaded_min_z>s.loaded_max_z||int64_t(s.loaded_max_z)-s.loaded_min_z>=NCMM_WORLD_MAX_SLICES||
       s.origin_z<s.loaded_min_z||s.origin_z>s.loaded_max_z||
       !std::isfinite(s.level_height)||s.level_height!=NCMM_WORLD_LEVEL_HEIGHT||
       !finite({s.camera_dx,s.camera_dy,s.camera_dz})||std::abs(s.camera_dx)>4||std::abs(s.camera_dy)>4||std::abs(s.camera_dz)>s.level_height*2||
       b.grid_width<1||b.grid_height<1||b.grid_width>NCMM_GRAPHICS_V2_MAX_GRID_SIDE||b.grid_height>NCMM_GRAPHICS_V2_MAX_GRID_SIDE)return false;
    for(uint32_t i=0;i<s.cell_count;++i){const auto &c=s.coordinates[i];
        const int64_t x=int64_t(c.x)-s.origin_x,y=int64_t(c.y)-s.origin_y;
        if(x<0||y<0||x>=b.grid_width||y>=b.grid_height||c.z<s.loaded_min_z||c.z>s.loaded_max_z||c.direction>3||
           c.reserved||c.flags&~524287u||(i&&!(order(s.coordinates[i-1])<order(c))))return false;
        for(float light:c.lower_light)if(!std::isfinite(light)||light<0||light>1)return false;
    }
    uint32_t count=0;int64_t previous=int64_t(s.loaded_min_z)-1;
    for(uint32_t i=0;i<s.slice_count;++i){const auto &slice=s.slices[i];
        if(slice.reserved||slice.z<=previous||slice.z<s.loaded_min_z||slice.z>s.loaded_max_z||!slice.identity||
           slice.first_cell!=count||slice.cell_count>s.cell_count-count)return false;
        for(uint32_t j=0;j<slice.cell_count;++j)if(s.coordinates[count+j].z!=slice.z)return false;
        count+=slice.cell_count;previous=slice.z;
    }
    return count==s.cell_count;
}
class coordinates {
    const ncmm_graphics_multilevel_scene_v1 *scene=nullptr;
public:
    explicit coordinates(const ncmm_graphics_multilevel_scene_v1 *s=nullptr):scene(s){}
    int find(int x,int y,int z)const{
        if(!scene)return -1;
        const int64_t ax=int64_t(scene->origin_x)+x,ay=int64_t(scene->origin_y)+y;
        if(ax<INT32_MIN||ax>INT32_MAX||ay<INT32_MIN||ay>INT32_MAX)return -1;
        const auto wanted=std::make_tuple(z,int32_t(ay),int32_t(ax));
        uint32_t lo=0,hi=scene->cell_count;
        while(lo<hi){uint32_t mid=lo+(hi-lo)/2;if(order(scene->coordinates[mid])<wanted)lo=mid+1;else hi=mid;}
        return lo<scene->cell_count&&order(scene->coordinates[lo])==wanted?int(lo):-1;
    }
    point origin(uint32_t cell)const{
        const auto &p=scene->coordinates[cell];
        return {float(int64_t(p.x)-scene->origin_x),float(int64_t(p.y)-scene->origin_y),
                float(int64_t(p.z)-scene->origin_z)*scene->level_height};
    }
    int z(uint32_t cell)const{return scene->coordinates[cell].z;}
    int neighbor(uint32_t cell,int dx,int dy,int dz=0)const{
        const auto &p=scene->coordinates[cell];return find(int(int64_t(p.x)-scene->origin_x)+dx,int(int64_t(p.y)-scene->origin_y)+dy,p.z+dz);
    }
};
inline bool floor_blocks(uint32_t flags,float x,float y){
    if(flags&NCMM_WORLD_UNKNOWN)return true;
    if(flags&NCMM_WORLD_GLASS_FLOOR)return false;
    if(flags&NCMM_WORLD_FLOOR)return true;
    // Stair landing surrounds the actual opening. Ladders/shafts keep air open.
    if((flags&NCMM_WORLD_DOWN)&&!(flags&(NCMM_WORLD_LADDER|NCMM_WORLD_RAMP)))
        return x<.14f||x>.86f||y<.08f||y>.92f;
    return false;
}
/* Bounded supercover DDA. Query returns geometry semantics, never entity data.
 * Z is physical height divided by the common storey height. Unknown data closes
 * the ray; tied grid crossings inspect both sides to prevent corner leaks. */
template<class Query> bool visible(point eye,point target,Query query,bool target_surface=false){
    if(!finite(eye)||!finite(target))return false;
    for(float value:{eye.x,eye.y,eye.z,target.x,target.y,target.z})if(std::abs(value)>16384)return false;
    const point delta{target.x-eye.x,target.y-eye.y,target.z-eye.z};
    if(std::max({std::abs(delta.x),std::abs(delta.y),std::abs(delta.z)/NCMM_WORLD_LEVEL_HEIGHT})>128)return false;
    voxel at{int(std::floor(eye.x)),int(std::floor(eye.y)),int(std::floor(eye.z/NCMM_WORLD_LEVEL_HEIGHT))};
    const voxel end{int(std::floor(target.x)),int(std::floor(target.y)),int(std::floor(target.z/NCMM_WORLD_LEVEL_HEIGHT))};
    const std::array<float,3> p{eye.x,eye.y,eye.z/NCMM_WORLD_LEVEL_HEIGHT},d{delta.x,delta.y,delta.z/NCMM_WORLD_LEVEL_HEIGHT};
    std::array<int,3> step{};std::array<float,3> next{},increment{};
    for(int axis=0;axis<3;++axis){step[axis]=d[axis]>0?1:d[axis]<0?-1:0;
        if(!step[axis]){next[axis]=increment[axis]=std::numeric_limits<float>::infinity();continue;}
        const float edge=std::floor(p[axis])+(step[axis]>0?1:0);
        next[axis]=(edge-p[axis])/d[axis];increment[axis]=std::abs(1/d[axis]);}
    for(int iteration=0;iteration<384;++iteration){
        if(at==end)return !(query(at)&NCMM_WORLD_UNKNOWN);
        const float t=std::min({next[0],next[1],next[2]});if(t>1)return true;
        const point hit{eye.x+delta.x*t,eye.y+delta.y*t,eye.z+delta.z*t};
        const voxel previous=at;std::array<bool,3> crossed{};
        for(int axis=0;axis<3;++axis)if(next[axis]<=t+1e-6f){crossed[axis]=true;
            if(axis==0)at.x+=step[axis];else if(axis==1)at.y+=step[axis];else at.z+=step[axis];next[axis]+=increment[axis];}
        if(crossed[2]){
            voxel upper=step[2]>0?at:previous;
            // At an XY/Z tie neither candidate is allowed to create a leak.
            for(int mask=0;mask<4;++mask){voxel cell=upper;
                if(crossed[0])cell.x=(mask&1)?at.x:previous.x;
                if(crossed[1])cell.y=(mask&2)?at.y:previous.y;
                const auto flags=query(cell);
                const float fx=std::clamp(hit.x-float(cell.x),0.f,1.f),fy=std::clamp(hit.y-float(cell.y),0.f,1.f);
                if(floor_blocks(flags,fx,fy)&&!(target_surface&&cell==end&&t>=1-1e-4f))return false;
            }
        }
        if(crossed[0]&&crossed[1]){
            for(voxel side:{voxel{at.x,previous.y,at.z},voxel{previous.x,at.y,at.z}})
                if(query(side)&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))return false;
        }
        if(at==end)return !(query(at)&NCMM_WORLD_UNKNOWN);
        if(query(at)&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))return false;
    }
    return false;
}
} // namespace ncmm_world
