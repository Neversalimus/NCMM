#pragma once
#include "ncmm_graphics_multilevel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>
#include <climits>

namespace ncmm_world {
struct point {float x=0,y=0,z=0;};
struct voxel {int x=0,y=0,z=0;};
inline bool candidate_level_in_range(int z,int eye_z,int range){
    const int64_t dz=int64_t(z)-eye_z;
    // A slab may expose its underside from the in-range level immediately below.
    return range>=0&&dz>=-int64_t(range)&&dz<=int64_t(range)+1;
}
inline bool source_window_fits(int32_t x,int32_t y,int32_t radius){
    if(radius<0||radius>NCMM_GRAPHICS_V2_RADIUS)return false;
    const int64_t border=int64_t(radius)+1; // Direction and supercover neighbours.
    return int64_t(x)-border>=INT32_MIN&&int64_t(x)+border<=INT32_MAX&&
           int64_t(y)-border>=INT32_MIN&&int64_t(y)+border<=INT32_MAX;
}
// Private source facts distinguish an opaque gas from solid architecture.
// Structural surfaces can continue behind a volume without publishing bodies.
constexpr uint32_t opaque_field_fact=1u<<31;
constexpr uint32_t vehicle_roof_fact=1u<<30;
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
    std::vector<int32_t> dense;
    int width=0,height=0;
    int slot(int64_t x,int64_t y,int64_t z)const{
        if(!scene||x<0||y<0||x>=width||y>=height||z<scene->loaded_min_z||z>scene->loaded_max_z)return -1;
        return int((size_t(z-scene->loaded_min_z)*height+size_t(y))*width+size_t(x));
    }
public:
    explicit coordinates(const ncmm_graphics_multilevel_scene_v1 *s=nullptr,bool indexed=false):scene(s){
        if(!indexed||!s)return;
        if(s->cell_count>NCMM_WORLD_MAX_CELLS||(s->cell_count&&!s->coordinates)){scene=nullptr;return;}
        const auto &b=s->appearance.base;const int64_t levels=int64_t(s->loaded_max_z)-s->loaded_min_z+1;
        if(b.grid_width<1||b.grid_height<1||b.grid_width>NCMM_GRAPHICS_V2_MAX_GRID_SIDE||b.grid_height>NCMM_GRAPHICS_V2_MAX_GRID_SIDE||
           s->loaded_min_z < -128||s->loaded_max_z>128||levels<1||levels>NCMM_WORLD_MAX_SLICES){scene=nullptr;return;}
        width=b.grid_width;height=b.grid_height;dense.assign(size_t(levels)*width*height,-1);
        for(uint32_t i=0;i<s->cell_count;++i){const auto &c=s->coordinates[i];const int at=slot(int64_t(c.x)-s->origin_x,int64_t(c.y)-s->origin_y,c.z);
            if(at>=0&&dense[size_t(at)]<0)dense[size_t(at)]=int32_t(i);}
    }
    int find(int x,int y,int z)const{
        if(!scene||(scene->cell_count&&!scene->coordinates))return -1;
        const int64_t ax=int64_t(scene->origin_x)+x,ay=int64_t(scene->origin_y)+y;
        if(ax<INT32_MIN||ax>INT32_MAX||ay<INT32_MIN||ay>INT32_MAX)return -1;
        if(!dense.empty()){const int at=slot(x,y,z);return at<0?-1:dense[size_t(at)];}
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
        if(!scene||cell>=scene->cell_count||!scene->coordinates)return -1;
        const auto &p=scene->coordinates[cell];const int64_t x=int64_t(p.x)-scene->origin_x+dx,y=int64_t(p.y)-scene->origin_y+dy,z=int64_t(p.z)+dz;
        if(x<INT_MIN||x>INT_MAX||y<INT_MIN||y>INT_MAX||z<INT_MIN||z>INT_MAX)return -1;
        return find(int(x),int(y),int(z));
    }
    size_t bytes()const{return dense.capacity()*sizeof(int32_t);}
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
// Only a full opaque horizontal slab can prove an entire plane closed.
inline bool full_opaque_slab(uint32_t flags){return (flags&NCMM_WORLD_UNKNOWN)||((flags&NCMM_WORLD_FLOOR)&&!(flags&NCMM_WORLD_GLASS_FLOOR));}
struct slab_sides {bool top=true,bottom=true;};
inline slab_sides slab_trace_sides(uint32_t flags,float eye_z,float level_z){
    // A full opaque slab hides its opposite face. Preserve both tests for
    // glass, stair openings and the thin band where its edge is visible.
    if((flags&NCMM_WORLD_FLOOR)&&!(flags&NCMM_WORLD_GLASS_FLOOR)){
        if(eye_z>level_z+.0001f)return {true,false};
        if(eye_z<level_z-.0801f)return {false,true};
    }
    return {};
}
// A visible architectural cell owns its actual supporting slab as well.
// Point samples of its top/bottom may be hidden by the cell's own door/window
// volume; dropping that slab leaves a sky crack in a recessed facade.
inline uint32_t facade_slab_permissions(uint32_t flags,float eye_z,float level_z){
    if(!(flags&NCMM_WORLD_FLOOR))return 0;
    const auto sides=slab_trace_sides(flags,eye_z,level_z);
    return (sides.top?uint32_t(NCMM_WORLD_FLOOR_TOP):0u)|(sides.bottom?uint32_t(NCMM_WORLD_FLOOR_BOTTOM):0u);
}
template<class Read> uint32_t roof_column(int low,int high,Read read){
    if(low < -128||high>128||low>high||int64_t(high)-low>=32)return UINT32_MAX;
    uint32_t mask=0;
    for(int z=high;z>=low;--z){const uint32_t value=read(z),bit=uint32_t(1)<<(z-low);
        if(value&(NCMM_WORLD_CANOPY|vehicle_roof_fact))mask|=bit;
        // The first roof covers every lower level. No engine reads below it
        // are needed to establish this column's roof mask.
        if(value&(NCMM_WORLD_FLOOR|NCMM_WORLD_GLASS_FLOOR|NCMM_WORLD_WATER|NCMM_WORLD_CANOPY|NCMM_WORLD_UNKNOWN|vehicle_roof_fact)){
            mask|=bit-1;break;
        }
    }
    return mask;
}
/* Bounded supercover DDA. Query returns geometry semantics, never entity data.
 * Z is physical height divided by the common storey height. Unknown data closes
 * the ray; tied grid crossings inspect both sides to prevent corner leaks. */
template<bool horizontal_fast=true,class Query> bool visible(point eye,point target,Query query,bool target_surface=false){
    if(!finite(eye)||!finite(target))return false;
    for(float value:{eye.x,eye.y,eye.z,target.x,target.y,target.z})if(std::abs(value)>16384)return false;
    const point delta{target.x-eye.x,target.y-eye.y,target.z-eye.z};
    if(std::max({std::abs(delta.x),std::abs(delta.y),std::abs(delta.z)/NCMM_WORLD_LEVEL_HEIGHT})>128)return false;
    voxel at{int(std::floor(eye.x)),int(std::floor(eye.y)),int(std::floor(eye.z/NCMM_WORLD_LEVEL_HEIGHT))};
    const voxel end{int(std::floor(target.x)),int(std::floor(target.y)),int(std::floor(target.z/NCMM_WORLD_LEVEL_HEIGHT))};
    if constexpr(horizontal_fast){
        const float ez=eye.z/NCMM_WORLD_LEVEL_HEIGHT,dz=delta.z/NCMM_WORLD_LEVEL_HEIGHT;
        const float z_exit=dz?(std::floor(ez)+(dz>0?1:0)-ez)/dz:std::numeric_limits<float>::infinity();
        if(at.z==end.z&&z_exit>1.f+1e-6f){
        // No storey boundary can be crossed. Keep the exact supercover corner
        // rule, but avoid all vertical arithmetic and slab queries in this case.
        const int sx=delta.x>0?1:delta.x<0?-1:0,sy=delta.y>0?1:delta.y<0?-1:0;
        const float inf=std::numeric_limits<float>::infinity();
        float tx=sx?(std::floor(eye.x)+(sx>0?1:0)-eye.x)/delta.x:inf,
              ty=sy?(std::floor(eye.y)+(sy>0?1:0)-eye.y)/delta.y:inf;
        const float ix=sx?std::abs(1/delta.x):inf,iy=sy?std::abs(1/delta.y):inf;
        for(int iteration=0;iteration<384;++iteration){
            if(at==end)return !(query(at)&NCMM_WORLD_UNKNOWN);
            const float t=std::min(tx,ty);if(t>1)return true;
            const voxel previous=at;const bool cross_x=tx<=t+1e-6f,cross_y=ty<=t+1e-6f;
            if(cross_x){at.x+=sx;tx+=ix;}if(cross_y){at.y+=sy;ty+=iy;}
            if(cross_x&&cross_y){
                for(voxel side:{voxel{at.x,previous.y,at.z},voxel{previous.x,at.y,at.z}})
                    if(query(side)&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))return false;
            }
            if(at==end)return !(query(at)&NCMM_WORLD_UNKNOWN);
            if(query(at)&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))return false;
        }
        return false;
    }}
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
            const int distinct=(crossed[0]?1:0)|(crossed[1]?2:0);
            for(int mask=0;mask<4;++mask){if(mask&~distinct)continue;voxel cell=upper;
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
// Exposed faces are traced separately so their adjacent light may illuminate
// an opaque facade without revealing bodies or an unobserved room's light.
inline voxel facade_neighbor(voxel cell,unsigned side){
    if(side==0)--cell.x;else if(side==1)++cell.x;else if(side==2)--cell.y;else ++cell.y;
    return cell;
}
template<class Query> uint8_t facade_faces(point eye,voxel cell,Query query){
    const float z=cell.z*NCMM_WORLD_LEVEL_HEIGHT;
    const std::array<float,4> heights{std::clamp(eye.z,z+.025f,z+NCMM_WORLD_LEVEL_HEIGHT-.025f),
        z+.025f,z+NCMM_WORLD_LEVEL_HEIGHT-.025f,z+NCMM_WORLD_LEVEL_HEIGHT*.5f};
    uint8_t result=0;
    for(unsigned side=0;side<4;++side){
        const bool facing=side==0?eye.x<cell.x:side==1?eye.x>cell.x+1:side==2?eye.y<cell.y:eye.y>cell.y+1;
        if(!facing||query(facade_neighbor(cell,side))&(NCMM_WORLD_OPAQUE|NCMM_WORLD_UNKNOWN))continue;
        bool hit=false;
        for(float height:heights){for(float along:{.001f,.5f,.999f}){
            const point target=side<2?point{cell.x+(side==0?.001f:.999f),cell.y+along,height}:
                point{cell.x+along,cell.y+(side==2?.001f:.999f),height};
            if(visible(eye,target,query)){hit=true;break;}
        }if(hit)break;}
        if(hit)result|=uint8_t(1u<<side);
    }
    return result;
}
template<class Query> bool facade_visible(point eye,voxel cell,Query query){return facade_faces(eye,cell,query)!=0;}

} // namespace ncmm_world
