#pragma once
#include "vehicle_geometry.hpp"
#include <unordered_map>
#include <unordered_set>

namespace first_person::vehicle {
struct transform {
    point origin{},x{1,0,0},y{0,1,0},z{0,0,1};
    point position(point p)const{return origin+x*p.x+y*p.y+z*p.z;}
    point normal(point p)const{return x*p.x+y*p.y+z*p.z;}
};
inline transform rigid(point origin,float yaw,float roll=0){
    return {origin,rotate({std::cos(roll),0,-std::sin(roll)},yaw),rotate({0,1,0},yaw),rotate({std::sin(roll),0,std::cos(roll)},yaw)};
}
struct submission {
    std::shared_ptr<const model> geometry;
    transform placement;
    uint64_t vehicle_id=0,part_id=0;
    int cell=-1;
};
class presentation {
    struct entry {std::shared_ptr<const model> geometry;uint64_t touched=0;};
    struct pose_state {
        uint64_t touched=0;double x=0,y=0,seconds=0;
        float angle=0,target=0,open=0;
    };
    std::unordered_map<uint64_t,entry> models;
    std::unordered_map<uint64_t,pose_state> poses;
    std::unordered_set<uint32_t> groups;
    uint64_t frame=0,world=0;
    size_t model_bytes=0;
    std::shared_ptr<const model> mesh(const topology &t,const ncmm_graphics_vehicle_part_v1 &p,unsigned lod){
        const auto key=t.recipe(p,lod);auto found=models.find(key);
        if(found!=models.end()){found->second.touched=frame;++reused;return found->second.geometry;}
        auto result=generate(input,t,p,lod);const auto needed=result->bytes();
        while(model_bytes+needed>cache_limit){auto oldest=models.end();
            for(auto it=models.begin();it!=models.end();++it)if(it->second.geometry.use_count()==1&&
                (oldest==models.end()||it->second.touched<oldest->second.touched))oldest=it;
            if(oldest==models.end())return {};
            model_bytes-=oldest->second.geometry->bytes();models.erase(oldest);++evicted;
        }
        model_bytes+=needed;models.emplace(key,entry{result,frame});++generated;return result;
    }
public:
    static constexpr size_t cache_limit=32u*1024u*1024u;
    snapshot input;
    std::vector<submission> draws;
    uint64_t generated=0,reused=0,evicted=0,revision=0,coverage_revision=0;
    bool active_animation=false;
    void reset(){input.clear();models.clear();poses.clear();groups.clear();draws.clear();
        frame=world=generated=reused=evicted=revision=coverage_revision=0;model_bytes=0;active_animation=false;}
    bool handles(uint32_t group)const{return input.enabled&&groups.count(group)!=0;}
    size_t bytes()const{return input.bytes()+model_bytes+draws.capacity()*sizeof(submission)+
        models.size()*(sizeof(entry)+sizeof(uint64_t)+3*sizeof(void *))+poses.size()*(sizeof(pose_state)+sizeof(uint64_t)+3*sizeof(void *));}
    void observe(point eye){
        draws.clear();groups.clear();active_animation=false;coverage_revision=1469598103934665603ull;
        if(!input.enabled)return;
        if(world!=input.scene.world_epoch){models.clear();poses.clear();model_bytes=0;world=input.scene.world_epoch;}
        ++frame;revision=input.scene.revision;
        for(const auto &v:input.vehicles){
            const topology t(input,v);std::vector<submission> vehicle_draws;bool complete=true;
            const point origin{v.origin_x,v.origin_y,v.origin_z};
            for(uint32_t k=0;k<v.part_count;++k){const auto &p=input.parts[v.first_part+k];
                if(p.state&NCMM_VP_TOPOLOGY_ONLY)continue;
                const auto center=origin+rotate({float(p.mount_x),float(p.mount_y),p.mount_z*input.scene.level_height},v.yaw);
                const float distance=std::sqrt(dot(center-eye,center-eye));
                const unsigned lod=distance<12?0u:distance<28?1u:2u;
                const auto geometry=mesh(t,p,lod);if(!geometry){complete=false;break;}
                if(geometry->triangles.empty())continue;
                point local{float(p.mount_x),float(p.mount_y),p.mount_z*input.scene.level_height};
                float yaw=v.yaw,roll=0;
                const auto kind=geometry->kind;
                auto &pose=poses[p.part_id];
                const bool fresh=!pose.touched||pose.touched+1!=frame||input.scene.seconds<pose.seconds;
                const double dt=fresh?0:std::clamp(input.scene.seconds-pose.seconds,0.0,.1);
                if(kind==component::door){
                    const float desired=(p.state&NCMM_VP_OPEN)?1.f:0.f;
                    if(fresh)pose.open=desired;
                    else pose.open+=std::clamp(desired-pose.open,-float(dt)/.18f,float(dt)/.18f);
                    const unsigned side=t.edge({p.mount_x,p.mount_y,p.mount_z});
                    const float direction=side==1||side==2?-1.f:1.f;
                    yaw+=pose.open*1.36f*direction;local=local+geometry->pivot;
                    active_animation|=std::abs(desired-pose.open)>.0001f;
                }
                if(kind==component::wheel){
                    // A mount identifies a Native cell, not an authored axle
                    // pivot. Fit the tire to that mount's actual outer contour.
                    if(!t.narrow){const int sign=p.mount_y==t.max_y?1:p.mount_y==t.min_y?-1:0;
                        local.y+=sign*std::max(0.f,.475f-p.wheel_width*.5f);}
                    local.z+=p.wheel_radius+.018f;
                    const auto world_center=origin+rotate(local,v.yaw);
                    const double x=double(input.scene.origin_x)+world_center.x,y=double(input.scene.origin_y)+world_center.y;
                    if(!fresh){const double dx=x-pose.x,dy=y-pose.y;
                        // Reset visual rolling on a discontinuity, never turn
                        // a teleport/window rebase into fictitious wheel speed.
                        if(dx*dx+dy*dy<16)pose.target+=float(dx*std::cos(v.yaw)+dy*std::sin(v.yaw))/p.wheel_radius;
                        else pose.target=pose.angle;
                    }
                    pose.x=x;pose.y=y;
                    if(fresh)pose.target=pose.angle=0;
                    const float step=std::clamp(pose.target-pose.angle,-float(dt)*45.f,float(dt)*45.f);
                    pose.angle+=step;roll=pose.angle;
                    if(std::abs(pose.angle)>1024){const float turns=std::floor(pose.angle/(2*pi))*(2*pi);pose.angle-=turns;pose.target-=turns;roll=pose.angle;}
                    active_animation|=std::abs(pose.target-pose.angle)>.0001f;
                    if((p.features&NCMM_VP_STEERABLE)&&(v.flags&NCMM_VEHICLE_STEERING_KNOWN))yaw+=std::clamp(v.steering,-.8f,.8f);
                }
                pose.touched=frame;pose.seconds=input.scene.seconds;
                const point placed=origin+rotate(local,v.yaw);
                const auto placement=rigid(placed,yaw,roll);
                vehicle_draws.push_back({geometry,placement,v.entity_id,p.part_id,p.cell_index});
                revision=(revision^geometry->recipe)*1099511628211ull;
                // CPU cache invalidation is based on real pose changes and
                // confirmed state interpolation, not a redraw-driven windup.
                for(float value:{placement.origin.x,placement.origin.y,placement.origin.z,yaw,roll}){
                    uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));revision=(revision^bits)*1099511628211ull;}
            }
            if(complete){groups.insert(v.legacy_group);coverage_revision=(coverage_revision^v.legacy_group)*1099511628211ull;draws.insert(draws.end(),vehicle_draws.begin(),vehicle_draws.end());}
        }
        for(auto it=poses.begin();it!=poses.end();)if(it->second.touched!=frame)it=poses.erase(it);else ++it;
    }
};
}
