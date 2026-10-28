#pragma once
#include "ncmm_world_policy.hpp"

namespace ncmm_projectile {
// Reticle ray in physical world units, relative to the avatar's Native cell.
// Only the caller queries its loaded map; no Native state is retained here.
template<class Read> std::optional<ncmm_world::voxel> free_target(
        ncmm_world::point eye,float yaw,float pitch,float range,float level,Read read){
    if(!std::isfinite(eye.x+eye.y+eye.z+yaw+pitch+range+level)||range<=0||level<=0||
       std::abs(eye.x)>2||std::abs(eye.y)>2||eye.z<0||eye.z>level*2||std::abs(pitch)>1.35f)return std::nullopt;
    const float cp=std::cos(pitch);
    const ncmm_world::point direction{std::cos(yaw)*cp,std::sin(yaw)*cp,std::sin(pitch)};
    ncmm_world::voxel previous{int(std::floor(eye.x)),int(std::floor(eye.y)),int(std::floor(eye.z/level))};
    uint32_t previous_flags=0;bool sampled=false;
    std::optional<ncmm_world::voxel> last;
    for(int n=1;n<=int(std::ceil(std::min(range,128.f)*8));++n){
        const float distance=std::min(range,n*.125f);
        const ncmm_world::point p{eye.x+direction.x*distance,eye.y+direction.y*distance,eye.z+direction.z*distance};
        ncmm_world::voxel cell{int(std::floor(p.x)),int(std::floor(p.y)),int(std::floor(p.z/level))};
        // The physical map is unchanged during this synchronous ray. Reuse
        // the same voxel instead of repeating Native field/vehicle queries
        // at every 1/8-metre sample of an automatic shot.
        const uint32_t flags=sampled&&cell==previous?previous_flags:read(cell.x,cell.y,cell.z);
        if(flags&NCMM_WORLD_UNKNOWN)return last;
        if(cell.z!=previous.z){
            const int upper=std::max(cell.z,previous.z);
            const uint32_t slab=upper==cell.z?flags:sampled&&cell.x==previous.x&&cell.y==previous.y&&upper==previous.z?
                previous_flags:read(cell.x,cell.y,upper);
            if(ncmm_world::floor_blocks(slab,p.x-std::floor(p.x),p.y-std::floor(p.y))){
                cell.z=upper;return cell;
            }
        }
        if(cell.x||cell.y||cell.z)last=cell;
        if(ncmm_world::structural_flags(flags)&NCMM_WORLD_OPAQUE)return last;
        previous=cell;previous_flags=flags;sampled=true;
    }
    return last;
}
}
