#pragma once
#include "vehicle_snapshot.hpp"
#include "ncmm_graphics_vehicle_policy.hpp"
#include <array>
#include <map>
#include <queue>
#include <tuple>
#include <string_view>

namespace first_person::vehicle {
using mount=std::tuple<int,int,int>;
using archetype=ncmm_graphics_vehicle_policy::archetype;
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
    std::array<uint32_t,31> feature_counts{};
    std::vector<uint32_t> wheels;
    struct wheel_size {int x,y,z;float radius;};
    std::map<uint32_t,wheel_size> sizes;
    archetype kind=archetype::utility;
    int components=0,min_x=0,max_x=0,min_y=0,max_y=0;
    float chassis=.20f,cabin=1.28f;
    bool narrow=false;
    bool body_paint_known=false;
    std::array<float,3> body_paint{{.28f,.36f,.40f}};
    int cabin_min_x=1001,cabin_max_x=-1001;
    explicit topology(const snapshot &s,const ncmm_graphics_vehicle_v1 &v){
        min_x=v.min_x;max_x=v.max_x;min_y=v.min_y;max_y=v.max_y;narrow=max_y==min_y;
        std::vector<float> radii;unsigned dedicated_cargo=0;std::map<std::array<float,3>,unsigned> paints;
        for(uint32_t k=0;k<v.part_count;++k){const uint32_t index=v.first_part+k;const auto &p=s.parts[index];
            auto &node=mounts[{p.mount_x,p.mount_y,p.mount_z}];node.parts.push_back(index);node.features|=p.features;
            for(unsigned bit=0;bit<31;++bit)if(p.features&(1u<<bit))++feature_counts[bit];
            if(p.material==NCMM_VM_PAINT&&(p.features&(NCMM_VP_PANEL|NCMM_VP_ROOF))&&!(p.features&(NCMM_VP_DOOR|NCMM_VP_WINDOW))){
                paints[{p.color[0],p.color[1],p.color[2]}]+=(p.features&NCMM_VP_PANEL)?2u:1u;}
            if(p.features&(NCMM_VP_SEAT|NCMM_VP_CONTROLS)){cabin_min_x=std::min(cabin_min_x,p.mount_x);cabin_max_x=std::max(cabin_max_x,p.mount_x);}
            if((p.features&NCMM_VP_CARGO)&&!(p.features&(NCMM_VP_SEAT|NCMM_VP_DOOR|NCMM_VP_CONTROLS|NCMM_VP_ENGINE|NCMM_VP_PANEL)))++dedicated_cargo;
            if(p.features&NCMM_VP_WHEEL){wheels.push_back(index);radii.push_back(p.wheel_radius);
                sizes.emplace(index,wheel_size{p.mount_x,p.mount_y,p.mount_z,p.wheel_radius});}}
        chassis=ncmm_graphics_vehicle_policy::chassis(radii);
        if(!paints.empty()){body_paint=std::max_element(paints.begin(),paints.end(),[](const auto &a,const auto &b){return a.second<b.second;})->first;body_paint_known=true;}
        std::sort(radii.begin(),radii.end());
        const auto count=[&](uint32_t f){uint32_t n=0;for(unsigned bit=0;bit<31;++bit)if(f&(1u<<bit))n+=feature_counts[bit];return n;};
        const auto profile=ncmm_graphics_vehicle_policy::shape(narrow,max_x-min_x,unsigned(wheels.size()),count(NCMM_VP_SEAT),count(NCMM_VP_ROOF),
            dedicated_cargo,count(NCMM_VP_CONTROLS),count(NCMM_VP_ARMOR),count(NCMM_VP_FLOAT),count(NCMM_VP_ENGINE),count(NCMM_VP_BICYCLE),
            radii.empty()?0:radii[radii.size()/2]);kind=profile.kind;cabin=profile.cabin;
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
    bool outer(mount at,unsigned side)const{return !has(adjacent(at,side),NCMM_VP_FRAME);}
    unsigned part_edge(const ncmm_graphics_vehicle_part_v1 &p)const{
        const std::string_view variant(p.variant);
        if(p.features&NCMM_VP_WINDSHIELD){
            if(cabin_max_x>=cabin_min_x){if(p.mount_x>cabin_max_x)return 0;if(p.mount_x<cabin_min_x)return 2;}
        }
        if(variant.find("horizontal")!=variant.npos)return p.mount_x*2>=min_x+max_x?0:2;
        if(variant.find("vertical_left")!=variant.npos||variant=="left")return 3;
        if(variant.find("vertical_right")!=variant.npos||variant=="right")return 1;
        if(variant=="front")return 0;if(variant=="rear")return 2;
        return edge({p.mount_x,p.mount_y,p.mount_z});
    }
    bool hood(const ncmm_graphics_vehicle_part_v1 &p)const{
        return (p.features&NCMM_VP_HALF_PANEL)&&cabin_max_x>=cabin_min_x&&p.mount_x>cabin_max_x+1;
    }
    bool generic_door_paint(const ncmm_graphics_vehicle_part_v1 &p)const{
        const std::string_view id(p.id);
        return body_paint_known&&p.material==NCMM_VM_PAINT&&(p.features&NCMM_VP_DOOR)&&
            (id=="door"||id=="door_opaque"||id=="door_trunk"||id=="door_heavy"||id=="door_sliding");
    }
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
        for(const char *text:{p.id,p.variant})for(;*text;++text)add(*text);
        // Door/wheel transforms and vehicle motion are deliberately absent.
        const uint32_t visual_state=p.state&(NCMM_VP_BROKEN|((p.features&NCMM_VP_LIGHT)?uint32_t(NCMM_VP_LIGHT_ON):0u)|
            ((p.features&NCMM_VP_CURTAIN)?uint32_t(NCMM_VP_OPEN):0u));add(visual_state);add(p.health);
        for(float c:p.color)add(c);
        if(generic_door_paint(p))for(float c:body_paint)add(c);
        if((p.features&NCMM_VP_WHEEL)&&!(p.features&(NCMM_VP_DOOR|NCMM_VP_WINDOW))){
            add(p.wheel_radius);add(p.wheel_width);return h?h:1;
        }
        add(chassis);
        if((p.features&NCMM_VP_LIGHT)&&(p.features&NCMM_VP_LIGHT_OMNI))add(cabin);
        const uint32_t shell=NCMM_VP_DOOR|NCMM_VP_WINDOW|NCMM_VP_ROOF|NCMM_VP_PANEL|NCMM_VP_ARMOR|NCMM_VP_SOLAR|NCMM_VP_FLOAT;
        if((p.features&shell)&&!(p.features&(NCMM_VP_SEAT|NCMM_VP_CONTROLS))){
            add(cabin);for(int value:{p.mount_x,p.mount_y,p.mount_z,min_x,max_x,min_y,max_y,cabin_min_x,cabin_max_x})add(value);
            add(features(at)&(NCMM_VP_ENGINE|NCMM_VP_SEAT|NCMM_VP_CONTROLS|NCMM_VP_HALF_PANEL|NCMM_VP_PANEL|NCMM_VP_DOOR));
            for(unsigned side=0;side<4;++side){const auto f=features(adjacent(at,side));
                add(f&(NCMM_VP_FRAME|NCMM_VP_PANEL|NCMM_VP_ROOF|NCMM_VP_FLOAT));add(bool(f));}
            if(p.features&(NCMM_VP_PANEL|NCMM_VP_ARMOR))for(uint32_t n:wheels){const auto &wheel=sizes.at(n);
                if(wheel.z==p.mount_z&&wheel.y==p.mount_y&&std::abs(wheel.x-p.mount_x)<wheel.radius+.6f){add(wheel.x);add(wheel.radius);}}
        }else if(p.features&NCMM_VP_CARGO){for(unsigned side=0;side<4;++side)add(features(adjacent(at,side))&NCMM_VP_CARGO);}
        else if(p.features&(NCMM_VP_FRAME|NCMM_VP_CONTROLS|NCMM_VP_AXLE)){
            add(narrow);add(features(at)&(NCMM_VP_FLOOR|NCMM_VP_AISLE|NCMM_VP_SEAT|NCMM_VP_WHEEL|NCMM_VP_STEERABLE));
            for(unsigned side:{0u,2u})add(features(adjacent(at,side))&(NCMM_VP_FRAME|NCMM_VP_SEAT|NCMM_VP_WHEEL));
            for(const auto &row:sizes){const auto &wheel=row.second;if(wheel.x==p.mount_x&&wheel.y==p.mount_y&&wheel.z==p.mount_z)add(wheel.radius);}
        }
        return h?h:1;
    }
};
}
