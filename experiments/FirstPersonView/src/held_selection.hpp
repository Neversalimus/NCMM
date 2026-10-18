#pragma once
#include "held_item.hpp"
#include <array>
#include <cmath>
#include <cstring>
namespace first_person::held {
enum class match:uint32_t {fallback,canonical,looks_like,properties};
enum class failure:uint32_t {none,unmapped,invalid_metadata,incompatible,unsupported_attachment,missing_socket,dimensions,unreachable_hand};
struct installed {const model *geometry=nullptr;int16_t socket=-1;float scale=1;};
struct selection {
    const model *body=nullptr;std::array<installed,4> attachments{};uint32_t attachment_count=0;
    match matched=match::fallback;failure reason=failure::unmapped;float scale=1;bool magazine=false;
};
inline bool terminated(const char *text,size_t count){return std::memchr(text,0,count)!=nullptr;}
inline bool metadata_valid(const ncmm_held_item_v1 &value){
    if(value.struct_size<sizeof(value)||value.version!=1||value.looks_count>NCMM_HELD_LOOKS_LIMIT||value.mod_count>NCMM_HELD_MOD_LIMIT||
       value.gun_skill>NCMM_HELD_OTHER_GUN||!terminated(value.canonical,sizeof(value.canonical))||
       !std::isfinite(value.base_length_m)||!std::isfinite(value.mass_kg))return false;
    for(size_t i=0;i<value.looks_count;++i)if(!terminated(value.looks_like[i],sizeof(value.looks_like[i])))return false;
    for(auto quality:value.qualities)if(quality<0||quality>10000)return false;
    for(size_t i=0;i<value.mod_count;++i){const auto &mod=value.mods[i];
        if(!terminated(mod.canonical,sizeof(mod.canonical))||!terminated(mod.location,sizeof(mod.location))||
           mod.looks_count>NCMM_HELD_MOD_LOOKS_LIMIT||!std::isfinite(mod.length_m)||!std::isfinite(mod.loudness_multiplier))return false;
        for(size_t j=0;j<mod.looks_count;++j)if(!terminated(mod.looks_like[j],sizeof(mod.looks_like[j])))return false;
    }
    return true;
}
struct mapping {const char *native,*asset;};
inline const model *mapped(const char *id){
    static constexpr mapping entries[]={
        {"glock_17","pistol"},{"glock_19","pistol"},{"sw_619","revolver"},{"hk_mp5","smg"},
        {"scar_l","assault"},{"fn_fal_semi","battle"},{"remington_870","pump"},{"saiga_12","auto_shotgun"},
        {"knife_combat","knife"},{"machete","machete"},{"bat","bat"},{"fire_ax","fire_axe"},{"hammer_sledge","sledge"},
        {"crowbar","crowbar"},{"hatchet","hatchet"},{"hammer","hammer"},{"shovel","shovel"},{"pickaxe","pickaxe"},
        {"bottle_plastic","bottle"},{"saw","saw"},{"wrench","wrench"},{"screwdriver","screwdriver"},{"pliers","pliers"},
        {"jerrycan","gas_can"},{"1st_aid_box","medical"},{"spear_steel","spear"},{"flashlight","flashlight"},{"flashlight_on","flashlight"}
    };
    for(const auto &entry:entries)if(!std::strcmp(id,entry.native))return find(entry.asset);
    return nullptr;
}
inline bool compatible(const model &model,const ncmm_held_item_v1 &item){
    const bool gun=item.flags&NCMM_HELD_GUN,detachable=item.flags&NCMM_HELD_DETACHABLE,manual=item.flags&NCMM_HELD_MANUAL_ACTION;
    switch(model.kind){
        case family::pistol:return gun&&item.gun_skill==NCMM_HELD_PISTOL&&detachable&&!manual&&!(item.flags&(NCMM_HELD_RELOAD_ONE|NCMM_HELD_RELOAD_EJECT));
        case family::revolver:return gun&&item.gun_skill==NCMM_HELD_PISTOL&&!detachable&&!manual;
        case family::smg:return gun&&item.gun_skill==NCMM_HELD_SMG&&detachable&&!manual;
        case family::assault:case family::battle:return gun&&item.gun_skill==NCMM_HELD_RIFLE&&detachable&&!manual;
        case family::pump:return gun&&item.gun_skill==NCMM_HELD_SHOTGUN&&!detachable&&(item.flags&NCMM_HELD_PUMP_ACTION);
        case family::auto_shotgun:return gun&&item.gun_skill==NCMM_HELD_SHOTGUN&&detachable&&!manual;
        case family::spear:return !gun&&(item.flags&NCMM_HELD_SPEAR)&&(item.flags&NCMM_HELD_METAL);
        default:return !gun&&uint32_t(model.kind)<uint32_t(family::optic);
    }
}
inline const model *property_model(const ncmm_held_item_v1 &item){
    if(item.flags&NCMM_HELD_GUN){
        if(item.gun_skill==NCMM_HELD_PISTOL){const auto candidate=find("pistol");return candidate&&compatible(*candidate,item)?candidate:nullptr;}
        if(item.gun_skill==NCMM_HELD_SMG&&(item.flags&NCMM_HELD_AUTOMATIC)){const auto candidate=find("smg");return candidate&&compatible(*candidate,item)?candidate:nullptr;}
        if(item.gun_skill==NCMM_HELD_SHOTGUN&&(item.flags&NCMM_HELD_PUMP_ACTION)){const auto candidate=find("pump");return candidate&&compatible(*candidate,item)?candidate:nullptr;}
        return nullptr; // A rifle skill alone does not identify its action or layout.
    }
    if(item.flags&NCMM_HELD_POWERED)return nullptr;
    const auto q=[&](ncmm_held_quality_v1 kind){return item.qualities[kind];};
    if((item.flags&NCMM_HELD_SPEAR)&&(item.flags&NCMM_HELD_METAL))return find("spear");
    if(q(NCMM_HELD_AXE)>=2&&q(NCMM_HELD_SAW)==0)return find(item.base_length_m>.6f?"fire_axe":"hatchet");
    if(q(NCMM_HELD_MINE)>=1&&q(NCMM_HELD_DIG)>0&&q(NCMM_HELD_AXE)==0)return find("pickaxe");
    if(q(NCMM_HELD_DIG)>=3&&q(NCMM_HELD_MINE)==0)return find("shovel");
    if(q(NCMM_HELD_HAMMER)>=3&&q(NCMM_HELD_AXE)==0)return find("sledge");
    if(q(NCMM_HELD_PRY)>=3&&q(NCMM_HELD_HAMMER)<=1&&q(NCMM_HELD_AXE)==0)return find("crowbar");
    if(q(NCMM_HELD_WRENCH)>=2&&q(NCMM_HELD_CUT)==0&&q(NCMM_HELD_SCREW)==0)return find("wrench");
    if(q(NCMM_HELD_SCREW)==1&&q(NCMM_HELD_WRENCH)==0&&q(NCMM_HELD_CUT)==0&&q(NCMM_HELD_PRY)==0&&q(NCMM_HELD_SAW)==0)return find("screwdriver");
    return nullptr;
}
inline const model *accessory(const char *id){
    static constexpr mapping entries[]={{"red_dot_sight","red_dot"},{"suppressor","suppressor"},{"grip","foregrip"},
        {"mounted_flashlight","gun_light"},{"mounted_flashlight_on","gun_light"}};
    for(const auto &entry:entries)if(!std::strcmp(id,entry.native))return find(entry.asset);
    return nullptr;
}
inline selection select(const ncmm_held_item_v1 &item){
    selection result;if(!metadata_valid(item)){result.reason=failure::invalid_metadata;return result;}
    const model *candidate=mapped(item.canonical);
    if(candidate&&compatible(*candidate,item)){result.body=candidate;result.matched=match::canonical;}
    else {
        if(candidate)result.reason=failure::incompatible;
        if(!(item.flags&NCMM_HELD_LOOKS_CYCLE))for(size_t i=0;i<item.looks_count&&!result.body;++i){candidate=mapped(item.looks_like[i]);
            if(candidate&&compatible(*candidate,item)){result.body=candidate;result.matched=match::looks_like;}}
        if(!result.body){result.body=property_model(item);if(result.body)result.matched=match::properties;}
    }
    if(!result.body)return result;
    result.reason=failure::none;
    if(item.base_length_m>0){result.scale=item.base_length_m/result.body->nominal_length;
        if(result.scale<.70f||result.scale>1.35f){result.body=nullptr;result.reason=failure::dimensions;return result;}}
    result.magazine=!(item.flags&NCMM_HELD_DETACHABLE)||(item.flags&NCMM_HELD_MAGAZINE_PRESENT);
    // This pinned family mesh includes its authored, extended Stock bone.
    // SCAR's native default stock is represented by that base geometry, not
    // an extra accessory. Removed, folded or replaced stocks use fallback.
    const char *base_stock=result.body->kind==family::assault?"high_end_folding_stock":nullptr;
    bool stock_present=base_stock==nullptr;
    for(size_t i=0;i<item.mod_count;++i){const auto &mod=item.mods[i];candidate=accessory(mod.canonical);
        if(base_stock&&!std::strcmp(mod.canonical,base_stock)&&!std::strcmp(mod.location,"stock")){
            if(stock_present){result.body=nullptr;result.reason=failure::unsupported_attachment;return result;}
            stock_present=true;continue;
        }
        if(!candidate&&!(mod.flags&NCMM_HELD_LOOKS_CYCLE))for(size_t j=0;j<mod.looks_count&&!candidate;++j)candidate=accessory(mod.looks_like[j]);
        if(!candidate){
            // Internal mechanism/adapter changes have no external shape. An
            // unknown visible attachment must use the existing fallback.
            const bool internal=!std::strcmp(mod.location,"mechanism")||!std::strcmp(mod.location,"loading port")||
                !std::strcmp(mod.location,"rail mount")||!std::strcmp(mod.location,"sights mount")||!std::strcmp(mod.location,"sling");
            if(internal)continue;
            result.body=nullptr;result.reason=failure::unsupported_attachment;return result;
        }
        const int16_t socket=candidate->kind==family::optic?result.body->optic_socket:candidate->kind==family::suppressor?result.body->muzzle_socket:
            candidate->kind==family::foregrip?result.body->rail_socket:result.body->light_socket;
        if(socket<0||result.attachment_count==result.attachments.size()){result.body=nullptr;result.reason=failure::missing_socket;return result;}
        for(size_t j=0;j<result.attachment_count;++j)if(result.attachments[j].socket==socket){result.body=nullptr;result.reason=failure::unsupported_attachment;return result;}
        const float scale=mod.length_m>0?mod.length_m/candidate->nominal_length:1;
        if(scale<.65f||scale>1.5f){result.body=nullptr;result.reason=failure::dimensions;return result;}
        result.attachments[result.attachment_count++]={candidate,socket,scale};
    }
    if(!stock_present){result.body=nullptr;result.reason=failure::unsupported_attachment;}
    return result;
}
}
