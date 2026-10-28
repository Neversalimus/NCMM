#pragma once
#include "held_pose.hpp"
#include "ncmm_graphics_presentation.h"
namespace first_person::weapon {
using point=viewmodel::point;
struct profile {
    uint32_t kind=NCMM_GUN_RIFLE,mods=0;float length=.8f,muzzle=.64f,support=.35f;
    point pivot{.185f,-.215f,.66f},primary{},secondary{},trigger{},sight{},tip{};
};
inline profile fallback(uint32_t kind,uint32_t mods,float length,const char *id){
    profile p;p.kind=kind;p.mods=mods;
    if(kind==NCMM_GUN_PISTOL){
        for(const char *revolver:{"raging_bull","colt_navy","sw_500","colt_saa","taurus_judge","ruger_lcr_22",
            "rough_rider_pistol","rough_rider_pistol_9","sw617","460_xvr","bfr","model_10_revolver","ruger_lcr_38",
            "colt_python","sw_619","chiappa_rhino","rp63","sw642","sw629","colt_anaconda_8","colt_anaconda_6","colt_anaconda_4","colt_army"})
            if(std::string_view(id).substr(0,std::string_view(id).find("_plus_"))==revolver){p.kind=NCMM_GUN_REVOLVER;break;}}
    const bool pistol=p.kind==NCMM_GUN_PISTOL||p.kind==NCMM_GUN_REVOLVER;
    p.length=std::clamp(length,pistol?.16f:.42f,pistol?.48f:1.8f);p.muzzle=pistol?p.length:p.length-.16f;
    p.support=std::clamp(p.muzzle*.55f,.18f,.38f);
    p.primary=p.pivot+point{0,-.079f,pistol?.020f:-.012f};
    p.secondary=p.pivot+(pistol?point{-.035f,-.087f,.030f}:point{0,mods&NCMM_MOD_FOREGRIP?-.09f:-.037f,p.support});
    p.trigger=p.pivot+point{.007f,-.046f,pistol?.075f:.045f};
    p.sight=p.pivot+point{0,mods&NCMM_MOD_OPTIC?.094f:.061f,mods&NCMM_MOD_OPTIC?.044f:p.muzzle-.022f};
    p.tip=p.pivot+point{0,.016f,p.muzzle+(mods&NCMM_MOD_SUPPRESSOR?.18f:.026f)};
    if(kind==NCMM_GUN_BOW){p.pivot={.25f,-.20f,.75f};p.primary=p.pivot;p.secondary=p.pivot+point{0,.02f,-.32f};
        p.sight=p.pivot+point{.014f,.026f,.1f};p.tip=p.pivot+point{.014f,.026f,.6f};}
    return p;
}
inline point aim_offset(const animation::pose &pose,const profile &fallback){
    point sight=fallback.sight;float depth=-.16f;
    if(pose.prepared_item&&pose.prepared_item->selected.body&&pose.prepared_item->count){
        const auto &prepared=*pose.prepared_item;const held::posed *entry=&prepared.entries[0];
        for(size_t i=1;i<prepared.count;++i)if(prepared.entries[i].geometry->kind==held::family::optic)entry=&prepared.entries[i];
        const auto &contact=entry->geometry->grips.sight;
        sight=viewmodel::position(entry->local,viewmodel::position(entry->bones.global[contact.node],contact.point));depth=-.24f;
    }
    return {-sight.x,-sight.y,depth};
}
inline point animate(point p,const animation::pose &pose,point offset,bool empty=false){
    const float steady=1-pose.aim*.9f;
    const point translation{pose.sway_x*steady,pose.sway_y*steady-.20f*(1-pose.equip_phase),-.06f*(1-pose.equip_phase)};
    if(empty)return p+translation;
    const point pivot{.27f,-.28f,.84f};p=p-pivot;
    const float co=std::cos(pose.swing),sn=std::sin(pose.swing);p={p.x*co-p.y*sn,p.x*sn+p.y*co,p.z};
    return p+pivot+translation+point{-.12f*pose.swing,.04f*pose.swing,pose.thrust-pose.recoil}+offset*pose.aim;
}
inline float hud_scale(float aspect,float body_scale=1){return std::max(1.f,aspect/.75f)*std::min(1.f,aspect*1.6f)*std::clamp(body_scale,.70f,1.25f);}
inline point muzzle(const animation::pose &pose,const profile &fallback){
    point result=fallback.tip;
    if(pose.prepared_item&&pose.prepared_item->selected.body&&pose.prepared_item->count){
        const auto &prepared=*pose.prepared_item;const held::posed *entry=&prepared.entries[0];
        for(size_t i=1;i<prepared.count;++i)if(prepared.entries[i].geometry->kind==held::family::suppressor)entry=&prepared.entries[i];
        const auto &contact=entry->geometry->grips.muzzle;
        result=viewmodel::position(entry->local,viewmodel::position(entry->bones.global[contact.node],contact.point));
        if(entry->geometry->kind==held::family::suppressor){
            float forward=-1e9f;point front{};unsigned count=0;
            const auto &mesh=entry->geometry->geometry;
            auto placed=[&](size_t i){const auto &v=mesh.vertices[i];return viewmodel::position(entry->local,viewmodel::position(entry->bones.palette[v.palette],v.position));};
            for(size_t i=0;i<mesh.vertex_count;++i)forward=std::max(forward,placed(i).z);
            for(size_t i=0;i<mesh.vertex_count;++i){const auto p=placed(i);if(forward-p.z<=.002f){front=front+p;++count;}}
            if(count)result=front*(1.f/count);
        }
    }
    return animate(result,pose,aim_offset(pose,fallback));
}
}
