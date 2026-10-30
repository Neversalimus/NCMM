#pragma once
#include "ncmm_graphics_vehicles.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <vector>

namespace first_person::vehicle {
struct snapshot {
    ncmm_graphics_vehicle_scene_v1 scene{};
    std::vector<ncmm_graphics_vehicle_v1> vehicles;
    std::vector<ncmm_graphics_vehicle_part_v1> parts;
    bool enabled=false;
    size_t validated_cells=0;
    void clear(){scene={};vehicles.clear();parts.clear();enabled=false;validated_cells=0;}
    size_t bytes()const{return vehicles.capacity()*sizeof(ncmm_graphics_vehicle_v1)+parts.capacity()*sizeof(ncmm_graphics_vehicle_part_v1);}
};
inline bool finite(float f){return std::isfinite(f);}
template<size_t N>inline bool terminated(const char(&v)[N]){return std::memchr(v,0,N)!=nullptr;}
inline bool valid(const snapshot &input,size_t cells){
    const auto &s=input.scene;
    if(s.struct_size<sizeof(s)||s.version!=1||!s.world_epoch||!s.revision||!std::isfinite(s.seconds)||s.seconds<0||
       !finite(s.level_height)||s.level_height<2||s.level_height>10||s.reserved||s.reserved_position||
       s.vehicle_count>NCMM_VEHICLE_MAX_VEHICLES||s.part_count>NCMM_VEHICLE_MAX_PARTS||
       s.vehicle_count!=input.vehicles.size()||s.part_count!=input.parts.size()||s.eye_valid>1||
       !finite(s.eye_x)||!finite(s.eye_y)||!finite(s.eye_z)||(s.eye_valid&&(!s.occupied_part||s.eye_z<.1f||s.eye_z>4)))return false;
    std::set<uint64_t> entities,parts;
    uint32_t offset=0;
    bool occupied=!s.occupied_vehicle,seat=!s.occupied_part;
    for(size_t n=0;n<input.vehicles.size();++n){const auto &v=input.vehicles[n];
        if(!v.entity_id||!entities.insert(v.entity_id).second||!v.structure_revision||!v.state_revision||
           v.first_part!=offset||!v.part_count||v.part_count>NCMM_VEHICLE_MAX_PARTS_PER_VEHICLE||
           v.part_count>s.part_count-offset||(v.flags&~3u)||!v.legacy_group||
           v.min_x>v.max_x||v.min_y>v.max_y||v.min_x<-1000||v.max_x>1000||v.min_y<-1000||v.max_y>1000)return false;
        for(float f:{v.origin_x,v.origin_y,v.origin_z,v.yaw,v.pivot_x,v.pivot_y,v.steering,v.native_velocity})if(!finite(f))return false;
        if(std::abs(v.origin_x)>4096||std::abs(v.origin_y)>4096||std::abs(v.origin_z)>128||
           std::abs(v.yaw)>32||std::abs(v.pivot_x)>1000||std::abs(v.pivot_y)>1000||std::abs(v.steering)>6.4f)return false;
        if(s.occupied_vehicle==v.entity_id)occupied=true;
        for(uint32_t k=0;k<v.part_count;++k){const auto &p=input.parts[offset+k];
            const bool topology_only=(p.state&NCMM_VP_TOPOLOGY_ONLY)!=0;
            if(!p.part_id||!parts.insert(p.part_id).second||p.vehicle_index!=n||
               (topology_only?p.cell_index!=-1:(p.cell_index<0||size_t(p.cell_index)>=cells))||p.mount_x<v.min_x||p.mount_x>v.max_x||
               p.mount_y<v.min_y||p.mount_y>v.max_y||p.mount_z<-16||p.mount_z>16||
               !(p.state&NCMM_VP_EXISTS)||(p.state&~511u)||((p.state&NCMM_VP_ROOF_SURFACE)&&(!(p.features&NCMM_VP_ROOF)||topology_only))||
               (p.features&~134217727u)||p.material>=NCMM_VM_COUNT||
               !p.id[0]||!terminated(p.id)||!terminated(p.variant)||!terminated(p.material_id))return false;
            for(float f:{p.health,p.wheel_radius,p.wheel_width,p.direction})if(!finite(f))return false;
            if(p.health<0||p.health>1||p.wheel_radius<0||p.wheel_radius>4||p.wheel_width<0||p.wheel_width>3||std::abs(p.direction)>32)return false;
            if((p.features&NCMM_VP_WHEEL)&&(p.wheel_radius<.04f||p.wheel_width<.01f))return false;
            for(float f:p.color)if(!finite(f)||f<0||f>1)return false;
            if(s.occupied_part==p.part_id){if(v.entity_id!=s.occupied_vehicle||!(p.features&(NCMM_VP_SEAT|NCMM_VP_FLOOR)))return false;seat=true;}
        }
        offset+=v.part_count;
    }
    return offset==s.part_count&&occupied&&seat;
}
// Fetch into temporary ownership. A failed or malformed optional extension
// never leaves a half-new car mixed with old parts: caller uses Legacy.
inline bool same_payload(const ncmm_graphics_vehicle_scene_v1 &a,const ncmm_graphics_vehicle_scene_v1 &b){
    return a.struct_size==b.struct_size&&a.version==b.version&&a.world_epoch==b.world_epoch&&a.revision==b.revision&&
        a.occupied_vehicle==b.occupied_vehicle&&a.occupied_part==b.occupied_part&&a.vehicle_count==b.vehicle_count&&
        a.part_count==b.part_count&&a.level_height==b.level_height&&a.reserved==b.reserved&&a.reserved_position==b.reserved_position&&
        a.origin_x==b.origin_x&&a.origin_y==b.origin_y&&a.origin_z==b.origin_z&&a.eye_valid==b.eye_valid&&
        a.eye_x==b.eye_x&&a.eye_y==b.eye_y&&a.eye_z==b.eye_z;
}
inline bool acquire(snapshot &out,const ncmm_host_graphics_vehicles_v1 *api,size_t cells){
    if(!api||api->abi_version!=1||api->struct_size<sizeof(*api)||api->major!=1||
       !api->get_scene||!api->get_vehicles||!api->get_parts){out.clear();return false;}
    snapshot next;next.scene.struct_size=sizeof(next.scene);
    if(!api->get_scene(&next.scene)||next.scene.vehicle_count>NCMM_VEHICLE_MAX_VEHICLES||
       next.scene.part_count>NCMM_VEHICLE_MAX_PARTS){out.clear();return false;}
    // The owned-copy API revision covers all vehicle/part payload values.
    // Monotonic presentation time is intentionally separate from that payload.
    if(out.enabled&&out.validated_cells==cells&&same_payload(out.scene,next.scene)&&std::isfinite(next.scene.seconds)&&next.scene.seconds>=0){
        out.scene.seconds=next.scene.seconds;return true;
    }
    next.vehicles.resize(next.scene.vehicle_count);next.parts.resize(next.scene.part_count);
    if(!api->get_vehicles(next.vehicles.data(),next.vehicles.size())||!api->get_parts(next.parts.data(),next.parts.size())||!valid(next,cells)){
        out.clear();return false;
    }
    next.enabled=true;next.validated_cells=cells;out=std::move(next);return true;
}
}
