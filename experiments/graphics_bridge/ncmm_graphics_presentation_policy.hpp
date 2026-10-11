#pragma once
#include "ncmm_graphics_presentation.h"
#include "ncmm_graphics_shapes.hpp"
#include <cmath>
namespace ncmm_graphics_presentation_policy {
using ncmm_graphics_shapes::contains;
inline uint32_t material(const std::string &id) {
    if(contains(id,"resin")||contains(id,"flesh")||contains(id,"chitin"))return NCMM_MAT_RESIN;
    if(contains(id,"glass"))return NCMM_MAT_GLASS;
    if(contains(id,"metal")||contains(id,"steel")||contains(id,"iron")||contains(id,"aluminum")||contains(id,"scrap")||contains(id,"chain")||contains(id,"barred"))return NCMM_MAT_METAL;
    if(contains(id,"stone")||contains(id,"concrete")||contains(id,"rock")||contains(id,"cement"))return NCMM_MAT_STONE;
    if(contains(id,"curtain")||contains(id,"canvas")||contains(id,"cotton")||contains(id,"leather")||contains(id,"wool"))return NCMM_MAT_CLOTH;
    if(contains(id,"wood")||contains(id,"plank")||contains(id,"door_c")||contains(id,"door_o"))return NCMM_MAT_WOOD;
    return NCMM_MAT_DEFAULT;
}
inline uint32_t liquid(const std::string &id,bool fluid=false) {
    if(contains(id,"lava"))return NCMM_LIQ_LAVA;
    if(contains(id,"acid"))return NCMM_LIQ_ACID;
    if(contains(id,"blood")||contains(id,"scarlet"))return NCMM_LIQ_BLOOD;
    if(contains(id,"sewage"))return NCMM_LIQ_SEWAGE;
    if(contains(id,"slime")||contains(id,"sludge")||contains(id,"ooze"))return NCMM_LIQ_SLIME;
    if(contains(id,"oil")||contains(id,"gasoline")||contains(id,"diesel"))return NCMM_LIQ_OIL;
    if(contains(id,"salt")||contains(id,"ocean")||contains(id,"sea_water"))return NCMM_LIQ_SEA;
    if(contains(id,"water")||contains(id,"river")||contains(id,"pool")||contains(id,"lake"))return NCMM_LIQ_FRESH;
    return fluid?NCMM_LIQ_OTHER:NCMM_LIQ_NONE;
}
inline uint32_t gas(const std::string &id,bool gaseous=false) {
    if(contains(id,"fire")||contains(id,"plasma"))return NCMM_GAS_FIRE;
    if(contains(id,"spore")||contains(id,"fungal"))return NCMM_GAS_SPORES;
    if(contains(id,"dust")||contains(id,"ash"))return NCMM_GAS_DUST;
    if(contains(id,"smoke"))return NCMM_GAS_SMOKE;
    if(contains(id,"fog")||contains(id,"mist")||contains(id,"steam"))return NCMM_GAS_FOG;
    return gaseous||contains(id,"gas")||contains(id,"fume")?NCMM_GAS_TOXIC:NCMM_GAS_NONE;
}
inline uint32_t sky(const std::string &id) {
    if(id=="abyss"||id=="spider_dimension"||id.find("netherum_labyrinth")==0||id=="portal_storm_dungeon")return NCMM_SKY_VOID;
    if(id=="highlands"||id=="imperial_road"||id=="cabins")return NCMM_SKY_HIGHLANDS;
    if(id=="scarlet")return NCMM_SKY_SCARLET;
    if(id=="inner_cabins")return NCMM_SKY_GREY;
    if(id=="string_dimension"||id=="string_dimension_ka")return NCMM_SKY_STARS;
    if(id=="radiosphere")return NCMM_SKY_RADIOSPHERE;
    if(id=="mycus_world")return NCMM_SKY_FUNGAL;
    return NCMM_SKY_EARTH;
}
struct dimensions {float height,width;};
// Game volume and body plan set physical extents. Category limits tolerate
// abstract monsters while preventing a padded tileset sprite from shrinking it.
inline dimensions creature(float liters,int category,const std::string &body) {
    const int c=std::clamp(category,0,4);
    const float low[]={.035f,.18f,.45f,.9f,1.3f},high[]={.5f,1.15f,2.25f,3.8f,7.0f};
    const float fallback[]={1,15,70,220,1200};
    if(!std::isfinite(liters)||liters<=0)liters=fallback[c];
    const float scale=std::cbrt(std::clamp(liters,.001f,60000.0f)/70);
    float h=1.75f,w=.65f;
    if(body=="quadruped"||body=="dog"||body=="pig"||body=="bear"||body=="lizard"){h=.82f;w=1.35f;}
    else if(body=="snake"||body=="fish"||body=="blob"){h=.40f;w=1.45f;}
    else if(body=="spider"||body=="insect"||body=="crab"){h=.48f;w=1.55f;}
    else if(body=="bird"||body=="flying insect"){h=.9f;w=1.15f;}
    return {std::clamp(h*scale,low[c],high[c]),std::clamp(w*scale,.04f,9.0f)};
}
}
