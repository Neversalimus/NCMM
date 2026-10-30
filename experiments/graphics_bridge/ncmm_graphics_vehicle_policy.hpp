#pragma once
#include <algorithm>
#include <vector>
// Shared pure geometry policy; no Native objects or graphics devices.
namespace ncmm_graphics_vehicle_policy {
inline float chassis(std::vector<float> radii){
    if(radii.empty())return .20f;
    std::sort(radii.begin(),radii.end());return std::clamp(radii[radii.size()/2]+.12f,.18f,1.5f);
}
inline float seated_eye(float floor,float body_scale,float roof){
    return std::clamp(std::min(floor+.28f+.75f*body_scale,roof-.12f),.35f,3.85f);
}
}
