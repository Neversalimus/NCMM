#pragma once
#include "vehicle_snapshot.hpp"
#include "ncmm_graphics_vehicle_policy.hpp"
#include <array>
#include <map>
#include <queue>
#include <tuple>

namespace first_person::vehicle {
using mount=std::tuple<int,int,int>;
enum class archetype {utility,bicycle,motorcycle,buggy,car,suv,van,truck,bus,pickup,armored,trailer,platform,boat};
struct station {
    uint32_t features=0;
    std::vector<uint32_t> parts;
    int component=-1;
};
// Topology is built for the whole actual published construction before any
// component is meshed. Missing/hidden mounts never become inferred panels.
class topology {
public:
    std::map<mount,station> mounts;
    std::array<uint32_t,23> feature_counts{};
    std::vector<uint32_t> wheels;
    struct wheel_size {int x,y,z;float radius;};
    std::map<uint32_t,wheel_size> sizes;
    archetype kind=archetype::utility;
    int components=0,min_x=0,max_x=0,min_y=0,max_y=0;
    float chassis=.20f,cabin=1.28f;
    bool narrow=false;
    explicit topology(const snapshot &s,const ncmm_graphics_vehicle_v1 &v){
        min_x=v.min_x;max_x=v.max_x;min_y=v.min_y;max_y=v.max_y;narrow=max_y==min_y;
        std::vector<float> radii;
        for(uint32_t k=0;k<v.part_count;++k){const uint32_t index=v.first_part+k;const auto &p=s.parts[index];
            auto &node=mounts[{p.mount_x,p.mount_y,p.mount_z}];node.parts.push_back(index);node.features|=p.features;
            for(unsigned bit=0;bit<23;++bit)if(p.features&(1u<<bit))++feature_counts[bit];
            if(p.features&NCMM_VP_WHEEL){wheels.push_back(index);radii.push_back(p.wheel_radius);
                sizes.emplace(index,wheel_size{p.mount_x,p.mount_y,p.mount_z,p.wheel_radius});}}
        chassis=ncmm_graphics_vehicle_policy::chassis(radii);
        std::sort(radii.begin(),radii.end());
        const auto count=[&](uint32_t f){uint32_t n=0;for(unsigned bit=0;bit<23;++bit)if(f&(1u<<bit))n+=feature_counts[bit];return n;};
        const auto seats=count(NCMM_VP_SEAT),roofs=count(NCMM_VP_ROOF),cargo=count(NCMM_VP_CARGO),controls=count(NCMM_VP_CONTROLS);
        if(count(NCMM_VP_FLOAT)&&wheels.empty())kind=archetype::boat;
        else if(narrow&&!wheels.empty())kind=count(NCMM_VP_ENGINE)?archetype::motorcycle:count(NCMM_VP_BICYCLE)?archetype::bicycle:archetype::utility;
        else if(!controls&&!seats&&cargo)kind=wheels.empty()?archetype::platform:archetype::trailer;
        else if(count(NCMM_VP_ARMOR)>=3)kind=archetype::armored;
        else if(seats>=6&&max_x-min_x>=6){kind=archetype::bus;cabin=1.9f;}
        else if(cargo>=4&&roofs>=cargo&&max_x-min_x>=4){kind=archetype::van;cabin=1.7f;}
        else if(cargo>=3&&roofs<cargo){kind=max_x-min_x>=6?archetype::truck:archetype::pickup;cabin=kind==archetype::truck?1.7f:1.35f;}
        else if(!roofs&&seats)kind=archetype::buggy;
        else if(!radii.empty()&&radii[radii.size()/2]>.43f){kind=archetype::suv;cabin=1.48f;}
        else if(roofs&&seats)kind=archetype::car;
        // Connected frame components stay separate, including asymmetric and
        // joined custom constructions. Decorative parts are not graph bridges.
        for(auto &[key,node]:mounts)if((node.features&NCMM_VP_FRAME)&&node.component<0){
            std::queue<mount> pending;pending.push(key);node.component=components;
            while(!pending.empty()){const auto at=pending.front();pending.pop();
                for(unsigned d=0;d<4;++d){const auto next=adjacent(at,d);auto found=mounts.find(next);
                    if(found!=mounts.end()&&(found->second.features&NCMM_VP_FRAME)&&found->second.component<0){found->second.component=components;pending.push(next);}}}
            ++components;
        }
    }
    static mount adjacent(mount at,unsigned side){const auto [x,y,z]=at;
        return {x+(side==0?1:side==2?-1:0),y+(side==1?1:side==3?-1:0),z};}
    uint32_t features(mount at)const{const auto found=mounts.find(at);return found==mounts.end()?0:found->second.features;}
    bool has(mount at,uint32_t f)const{return (features(at)&f)!=0;}
    unsigned edge(mount at)const{
        const auto [x,y,z]=at;(void)z;
        // Outward normal is chosen from the actual boundary; never from view
        // direction or a vehicle name. Interior partitions retain a local axis.
        if(y==max_y&&!has(adjacent(at,1),NCMM_VP_FRAME|NCMM_VP_PANEL))return 1;
        if(y==min_y&&!has(adjacent(at,3),NCMM_VP_FRAME|NCMM_VP_PANEL))return 3;
        if(x==max_x&&!has(adjacent(at,0),NCMM_VP_FRAME|NCMM_VP_PANEL))return 0;
        if(x==min_x&&!has(adjacent(at,2),NCMM_VP_FRAME|NCMM_VP_PANEL))return 2;
        for(unsigned d:{1u,3u,0u,2u})if(!features(adjacent(at,d)))return d;
        return 1;
    }
    uint64_t recipe(const ncmm_graphics_vehicle_part_v1 &p,unsigned lod)const{
        uint64_t h=1469598103934665603ull;
        auto add=[&](const auto &v){const auto *bytes=reinterpret_cast<const unsigned char *>(&v);for(size_t i=0;i<sizeof(v);++i)h=(h^bytes[i])*1099511628211ull;};
        const mount at{p.mount_x,p.mount_y,p.mount_z};
        for(uint32_t f:{p.features,p.material,lod})add(f);
        // Door/wheel transforms and vehicle motion are deliberately absent.
        const uint32_t visual_state=p.state&(NCMM_VP_BROKEN|((p.features&NCMM_VP_LIGHT)?uint32_t(NCMM_VP_LIGHT_ON):0u));add(visual_state);add(p.health);
        for(float c:p.color)add(c);
        if((p.features&NCMM_VP_WHEEL)&&!(p.features&(NCMM_VP_DOOR|NCMM_VP_WINDOW))){
            add(p.wheel_radius);add(p.wheel_width);return h?h:1;
        }
        add(chassis);
        const uint32_t shell=NCMM_VP_DOOR|NCMM_VP_WINDOW|NCMM_VP_ROOF|NCMM_VP_PANEL|NCMM_VP_ARMOR|NCMM_VP_SOLAR;
        if((p.features&shell)&&!(p.features&(NCMM_VP_SEAT|NCMM_VP_CONTROLS))){
            add(cabin);for(int value:{p.mount_x,p.mount_y,p.mount_z,min_x,max_x,min_y,max_y})add(value);
            add(features(at)&(NCMM_VP_ENGINE|NCMM_VP_SEAT|NCMM_VP_CONTROLS));
            for(unsigned side=0;side<4;++side){const auto f=features(adjacent(at,side));
                add(f&(NCMM_VP_FRAME|NCMM_VP_PANEL|NCMM_VP_ROOF));add(bool(f));}
            if(p.features&(NCMM_VP_PANEL|NCMM_VP_ARMOR))for(uint32_t n:wheels){const auto &wheel=sizes.at(n);
                if(wheel.z==p.mount_z&&wheel.y==p.mount_y&&std::abs(wheel.x-p.mount_x)<wheel.radius+.6f){add(wheel.x);add(wheel.radius);}}
        }else if(p.features&NCMM_VP_CARGO){for(unsigned side=0;side<4;++side)add(features(adjacent(at,side))&NCMM_VP_CARGO);}
        else if(p.features&(NCMM_VP_FRAME|NCMM_VP_CONTROLS))add(narrow);
        return h?h:1;
    }
};
}
