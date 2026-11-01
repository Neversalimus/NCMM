#pragma once
#include "vehicle_scene.hpp"
#include <map>
#include <tuple>

namespace first_person::vehicle {
struct frame_cap_plan {
    // Aligned with the caller's exact actually permitted submission list.
    std::vector<uint8_t> sides;
    size_t removed_triangles=0,joined_ends=0,eligible_frames=0;
};
inline bool frame_cap_eligible(const submission &draw,point eye,point forward){
    if(!draw.geometry||!draw.geometry->indexed_frame_caps()||!draw.vehicle_id||!draw.part_id||draw.cell<0||draw.roof_surface||draw.exterior_surface)return false;
    const auto &p=draw.placement;
    // A deliberately exact initial domain: no angular or positional epsilon.
    // Cardinal endpoint arithmetic contains only the original two nonzero terms.
    if(!(p.x.x==1&&p.x.y==0&&p.x.z==0&&p.y.x==0&&p.y.y==1&&p.y.z==0&&p.z.x==0&&p.z.y==0&&p.z.z==1))return false;
    for(float f:{p.origin.x,p.origin.y,p.origin.z,eye.x,eye.y,eye.z,forward.x,forward.y,forward.z})if(!std::isfinite(f))return false;
    const auto local_eye=eye-p.origin;
    return draw.geometry->closed_shell&&!draw.geometry->contains(local_eye)&&draw.geometry->ahead_of_near_plane(local_eye,forward);
}
inline frame_cap_plan joined_frame_caps(const std::vector<const submission *> &permitted,point eye,point forward,size_t minimum_saved_triangles=1024){
    frame_cap_plan result;
    // No per-frame temporary for small constructions or an empty permission set.
    if(permitted.size()<32)return result;
    using key=std::tuple<uint64_t,float,float,float>;
    std::map<key,size_t> by_origin;
    std::map<uint64_t,size_t> savings;
    std::vector<uint8_t> candidates(permitted.size());
    for(size_t i=0;i<permitted.size();++i){
        const auto *draw=permitted[i];if(!draw||!frame_cap_eligible(*draw,eye,forward))continue;
        ++result.eligible_frames;const auto p=draw->placement.origin;
        auto found=by_origin.emplace(key{draw->vehicle_id,p.x,p.y,p.z},i);
        // Two authored frames at one origin are ambiguous: neither is a join.
        if(!found.second)found.first->second=SIZE_MAX;
    }
    for(const auto &entry:by_origin){const auto i=entry.second;if(i==SIZE_MAX)continue;const auto &draw=*permitted[i];const auto p=draw.placement.origin;
        for(unsigned side=0;side<2;++side){const float sign=side?1.f:-1.f;
            const float wanted=p.x+sign;if(wanted==p.x||!std::isfinite(wanted))continue;
            const auto found=by_origin.find(key{draw.vehicle_id,wanted,p.y,p.z});
            if(found==by_origin.end()||found->second==SIZE_MAX)continue;
            const auto &other=*permitted[found->second];
            if(other.placement.origin.x-sign!=p.x||p.x+sign*.5f!=other.placement.origin.x-sign*.5f||
                draw.geometry->frame_caps->boundary!=other.geometry->frame_caps->boundary)continue;
            candidates[i]|=uint8_t(1u<<side);savings[draw.vehicle_id]+=16;
        }
    }
    for(size_t i=0;i<candidates.size();++i){if(!candidates[i])continue;
        if(savings[permitted[i]->vehicle_id]<minimum_saved_triangles){candidates[i]=0;continue;}
        const unsigned count=unsigned(bool(candidates[i]&1u))+unsigned(bool(candidates[i]&2u));
        result.joined_ends+=count;result.removed_triangles+=16*count;
    }
    if(result.removed_triangles)result.sides=std::move(candidates);
    return result;
}
}
