#pragma once
#include "vehicle_geometry.hpp"
#include <unordered_map>
#include <unordered_set>

namespace first_person::vehicle {
struct transform {
    point origin{},x{1,0,0},y{0,1,0},z{0,0,1};
    point position(point p)const{return origin+x*p.x+y*p.y+z*p.z;}
    point normal(point p)const{return x*p.x+y*p.y+z*p.z;}
    std::array<point,2> bounds(point low,point high)const{
        const auto center=position((low+high)*.5f),half=(high-low)*.5f;
        // Exact rigid-box extent, enlarged for float boundary round-off.
        const point radius{std::abs(x.x)*half.x+std::abs(y.x)*half.y+std::abs(z.x)*half.z+.001f,
            std::abs(x.y)*half.x+std::abs(y.y)*half.y+std::abs(z.y)*half.z+.001f,
            std::abs(x.z)*half.x+std::abs(y.z)*half.y+std::abs(z.z)*half.z+.001f};
        return {{center-radius,center+radius}};
    }
};
inline transform rigid(point origin,float yaw,float roll=0){
    return {origin,rotate({std::cos(roll),0,-std::sin(roll)},yaw),rotate({0,1,0},yaw),rotate({std::sin(roll),0,std::cos(roll)},yaw)};
}
struct submission {
    std::shared_ptr<const model> geometry;
    transform placement;
    uint64_t vehicle_id=0,part_id=0;
    int cell=-1;
    bool roof_surface=false;
    bool exterior_surface=false,surface_outside=false;
};
class presentation {
    struct entry {std::shared_ptr<const model> geometry;uint64_t touched=0;};
    struct pose_state {
        uint64_t touched=0;double x=0,y=0,seconds=0;
        float angle=0,target=0,open=0;
    };
    std::unordered_map<uint64_t,entry> models;
    std::unordered_map<uint64_t,pose_state> poses;
    struct construction {uint64_t revision=0,touched=0;uint32_t first=0,count=0;std::unique_ptr<topology> shape;};
    std::unordered_map<uint64_t,construction> layouts;
    std::unordered_set<uint32_t> groups;
    std::unordered_set<int> roof_cells;
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
    bool cacheable_opaque()const{
        if(draws.empty()||active_animation)return false;
        for(const auto &v:input.vehicles)if(!std::isfinite(v.native_velocity)||v.native_velocity!=0)return false;
        for(const auto &draw:draws)if(draw.geometry)for(const auto &m:draw.geometry->materials)
            if(!std::isfinite(m.color[3])||(m.color[3]>=.99f&&m.color[3]!=1.f))return false;
        return true;
    }
    uint64_t visual_revision()const{
        // Snapshot serials prove callback provenance, not changed pixels.
        // Hash the actual immutable model and each current draw's full pose,
        // identity and Native permission/cell. Nothing is quantized away.
        uint64_t hash=1469598103934665603ull;
        auto bytes=[&](const auto &value){const auto *p=reinterpret_cast<const unsigned char *>(&value);
            for(size_t n=0;n<sizeof(value);++n)hash=(hash^p[n])*1099511628211ull;};
        bytes(input.scene.world_epoch);bytes(draws.size());
        for(const auto &draw:draws){bytes(draw.vehicle_id);bytes(draw.part_id);bytes(draw.cell);
            for(bool value:{draw.roof_surface,draw.exterior_surface,draw.surface_outside})bytes(uint8_t(value));
            for(const auto basis:{draw.placement.origin,draw.placement.x,draw.placement.y,draw.placement.z})
                for(float value:{basis.x,basis.y,basis.z})bytes(value);
            const uint64_t recipe=draw.geometry?draw.geometry->recipe:0;bytes(recipe);
            // Hand-authored internal test models may have no recipe. Their
            // complete immutable data still participate in invalidation.
            if(draw.geometry&&!recipe){const auto &model=*draw.geometry;bytes(model.triangles.size());bytes(model.materials.size());
                bytes(uint8_t(model.closed_shell));
                for(float value:{model.minimum.x,model.minimum.y,model.minimum.z,model.maximum.x,model.maximum.y,model.maximum.z})bytes(value);
                for(const auto &m:model.materials){for(float value:m.color)bytes(value);bytes(m.kind);bytes(uint8_t(m.emissive));}
                for(const auto &t:model.triangles){bytes(t.material);for(const auto &v:t.vertices){
                    for(float value:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.u,v.v})bytes(value);}}
            }
        }
        return hash;
    }
    struct selection {uint64_t vehicle=0,part=0;float distance=std::numeric_limits<float>::infinity();};
    selection pick(int cell,point origin,point direction)const{
        selection result;if(cell<0)return result;
        // Restrict intersections to the Native cell selected by the actual
        // displayed depth/picking pass. This cannot select through a wall or
        // choose an unperceived part from the topology-only construction.
        for(const auto &draw:draws)if(draw.cell==cell&&!draw.roof_surface&&!draw.exterior_surface)for(const auto &triangle:draw.geometry->triangles){
            const auto a=draw.placement.position(triangle.vertices[0].position),
                b=draw.placement.position(triangle.vertices[1].position),c=draw.placement.position(triangle.vertices[2].position);
            const auto e1=b-a,e2=c-a,h=cross(direction,e2);const float determinant=dot(e1,h);
            if(std::abs(determinant)<1e-7f)continue;
            const float inverse=1/determinant;const auto s=origin-a;const float u=inverse*dot(s,h);if(u<0||u>1)continue;
            const auto q=cross(s,e1);const float v=inverse*dot(direction,q);if(v<0||u+v>1)continue;
            const float distance=inverse*dot(e2,q);
            if(distance>.025f&&distance<result.distance)result={draw.vehicle_id,draw.part_id,distance};
        }
        return result;
    }
    static constexpr size_t cache_limit=32u*1024u*1024u;
    snapshot input;
    std::vector<submission> draws;
    uint64_t generated=0,reused=0,evicted=0,revision=0,coverage_revision=0,topology_builds=0;
    bool active_animation=false;
    void reset(){input.clear();models.clear();poses.clear();layouts.clear();groups.clear();roof_cells.clear();draws.clear();
        frame=world=generated=reused=evicted=revision=coverage_revision=topology_builds=0;model_bytes=0;active_animation=false;}
    bool handles(uint32_t group)const{return input.enabled&&groups.count(group)!=0;}
    bool handles_roof(int cell)const{return input.enabled&&roof_cells.count(cell)!=0;}
    size_t bytes()const{return input.bytes()+model_bytes+draws.capacity()*sizeof(submission)+
        models.size()*(sizeof(entry)+sizeof(uint64_t)+3*sizeof(void *))+poses.size()*(sizeof(pose_state)+sizeof(uint64_t)+3*sizeof(void *))+layout_bytes();}
    size_t layout_bytes()const{size_t result=layouts.size()*(sizeof(construction)+sizeof(uint64_t)+3*sizeof(void *));
        for(const auto &row:layouts)if(row.second.shape){const auto &t=*row.second.shape;result+=sizeof(t)+t.wheels.capacity()*sizeof(uint32_t)+
            t.sizes.size()*(sizeof(uint32_t)+sizeof(topology::wheel_size)+3*sizeof(void *));
            for(const auto &node:t.mounts)result+=sizeof(mount)+sizeof(station)+3*sizeof(void *)+node.second.parts.capacity()*sizeof(uint32_t);}
        return result;
    }
    void observe(point eye){
        draws.clear();groups.clear();roof_cells.clear();active_animation=false;coverage_revision=1469598103934665603ull;
        if(!input.enabled)return;
        if(world!=input.scene.world_epoch){models.clear();poses.clear();layouts.clear();model_bytes=0;world=input.scene.world_epoch;}
        ++frame;revision=input.scene.revision;
        for(const auto &v:input.vehicles){
            auto &construction=layouts[v.entity_id];
            if(!construction.shape||construction.revision!=v.structure_revision||construction.first!=v.first_part||construction.count!=v.part_count){
                construction.shape=std::make_unique<topology>(input,v);construction.revision=v.structure_revision;
                construction.first=v.first_part;construction.count=v.part_count;++topology_builds;
            }
            construction.touched=frame;const auto &t=*construction.shape;std::vector<submission> vehicle_draws;bool complete=true;
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
                    const unsigned side=t.part_edge(p);
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
                        if(dx*dx+dy*dy<1e-10&&v.native_velocity==0)pose.target=pose.angle;
                    }
                    pose.x=x;pose.y=y;
                    if(fresh)pose.target=pose.angle=0;
                    if(p.state&NCMM_VP_BROKEN)pose.target=pose.angle=0;
                    const float step=std::clamp(pose.target-pose.angle,-float(dt)*45.f,float(dt)*45.f);
                    pose.angle+=step;roll=pose.angle;
                    if(std::abs(pose.angle)>1024){const float turns=std::floor(pose.angle/(2*pi))*(2*pi);pose.angle-=turns;pose.target-=turns;roll=pose.angle;}
                    active_animation|=std::abs(pose.target-pose.angle)>.0001f;
                    if((p.features&NCMM_VP_STEERABLE)&&(v.flags&NCMM_VEHICLE_STEERING_KNOWN))yaw+=std::clamp(v.steering,-.8f,.8f);
                }
                pose.touched=frame;pose.seconds=input.scene.seconds;
                if((kind==component::turret||(kind==component::light&&!(p.features&NCMM_VP_LIGHT_OMNI)))&&(p.state&NCMM_VP_DIRECTION_KNOWN))yaw+=p.direction;
                if(kind==component::axle&&t.narrow&&(p.features&NCMM_VP_STEERABLE)&&(v.flags&NCMM_VEHICLE_STEERING_KNOWN))yaw+=std::clamp(v.steering,-.8f,.8f);
                const point placed=origin+rotate(local,v.yaw);
                const auto placement=rigid(placed,yaw,roll);
                vehicle_draws.push_back({geometry,placement,v.entity_id,p.part_id,p.cell_index,bool(p.state&NCMM_VP_ROOF_SURFACE),bool(p.state&NCMM_VP_EXTERIOR_SURFACE),bool(p.surface_faces&16u)});
                revision=(revision^geometry->recipe)*1099511628211ull;
                // CPU cache invalidation is based on real pose changes and
                // confirmed state interpolation, not a redraw-driven windup.
                for(float value:{placement.origin.x,placement.origin.y,placement.origin.z,yaw,roll}){
                    uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));revision=(revision^bits)*1099511628211ull;}
            }
            if(complete){groups.insert(v.legacy_group);coverage_revision=(coverage_revision^v.legacy_group)*1099511628211ull;
                for(const auto &draw:vehicle_draws)if(draw.roof_surface)roof_cells.insert(draw.cell);
                draws.insert(draws.end(),vehicle_draws.begin(),vehicle_draws.end());}
        }
        for(auto it=poses.begin();it!=poses.end();)if(it->second.touched!=frame)it=poses.erase(it);else ++it;
        for(auto it=layouts.begin();it!=layouts.end();)if(it->second.touched!=frame)it=layouts.erase(it);else ++it;
    }
};
}
