#pragma once
#include <algorithm>
#include <vector>
#include <string_view>
// Shared pure geometry policy; no Native objects or graphics devices.
namespace ncmm_graphics_vehicle_policy {
enum class archetype {utility,bicycle,motorcycle,buggy,car,suv,van,truck,bus,pickup,armored,trailer,platform,boat};
struct profile {archetype kind=archetype::utility;float cabin=1.28f;};
inline profile shape(bool narrow,int length,unsigned wheels,unsigned seats,unsigned roofs,unsigned cargo,unsigned controls,
    unsigned armor,unsigned floats,unsigned engines,unsigned bicycle,float radius){
    if(floats&&!wheels)return {archetype::boat,1.28f};
    if(narrow&&wheels)return {bicycle?archetype::bicycle:engines?archetype::motorcycle:archetype::utility,1.28f};
    if(!controls&&!seats&&cargo)return {wheels?archetype::trailer:archetype::platform,1.28f};
    if(armor>=3)return {archetype::armored,1.48f};
    if((seats>=6&&length>=6)||(roofs>=18&&length>=8))return {archetype::bus,1.9f};
    if(wheels>=6&&length>=6)return {archetype::truck,1.7f};
    if(cargo>=4&&roofs>=cargo&&length>=4)return {archetype::van,1.7f};
    if(cargo>=3&&roofs<cargo)return {length>=6?archetype::truck:archetype::pickup,length>=6?1.7f:1.35f};
    if(!roofs&&seats)return {archetype::buggy,1.28f};
    if(radius>.43f)return {archetype::suv,1.48f};
    if(roofs)return {archetype::car,1.28f};
    return {};
}
inline float chassis(std::vector<float> radii){
    if(radii.empty())return .20f;
    std::sort(radii.begin(),radii.end());return std::clamp(radii[radii.size()/2]+.12f,.18f,1.5f);
}
inline float seated_eye(float floor,float body_scale,float roof,float seat_surface=.28f){
    return std::clamp(std::min(floor+seat_surface+.75f*body_scale,roof-.12f),.35f,3.85f);
}
inline bool compact_seat(archetype kind,std::string_view id){
    return kind==archetype::bicycle||kind==archetype::motorcycle||id.find("saddle")!=id.npos;
}
inline float seat_surface(archetype kind,std::string_view id){return compact_seat(kind,id)?.515f:.28f;}
}
