#pragma once
#include "viewmodel_asset.hpp"

namespace first_person::viewmodel {
constexpr size_t maximum_nodes=128, maximum_palettes=1024;
struct pose_buffer {
    std::array<local_pose,maximum_nodes> local{};
    std::array<matrix,maximum_nodes> global{}, skin{};
    std::array<matrix,maximum_palettes> palette{};
};
point operator+(point a,point b) noexcept;
point operator-(point a,point b) noexcept;
point operator*(point a,float scale) noexcept;
float dot(point a,point b) noexcept;
point cross(point a,point b) noexcept;
point unit(point value,point fallback={0,0,1}) noexcept;
matrix identity() noexcept;
matrix multiply(const matrix &a,const matrix &b) noexcept;
matrix transform(const local_pose &pose) noexcept;
bool inverse(const matrix &input,matrix &output) noexcept;
point position(const matrix &m,point value) noexcept;
point direction(const matrix &m,point value) noexcept;
quaternion slerp(quaternion a,quaternion b,float alpha) noexcept;
bool valid_asset(const asset &source) noexcept;
int clip_index(const asset &source,const char *name) noexcept;
int node_index(const asset &source,const char *name) noexcept;
// No allocation or sampling state: identical times produce identical poses.
bool sample_locals(const asset &source,size_t clip,double seconds,bool loop,
                   std::array<local_pose,maximum_nodes> &output) noexcept;
void blend_locals(const asset &source,const std::array<local_pose,maximum_nodes> &from,
                  std::array<local_pose,maximum_nodes> &to,float alpha) noexcept;
bool evaluate_globals(const asset &source,const std::array<local_pose,maximum_nodes> &local,
                      std::array<matrix,maximum_nodes> &global) noexcept;
bool evaluate(const asset &source,pose_buffer &output) noexcept;
bool bind_pose(const asset &source,pose_buffer &output) noexcept;
bool sample(const asset &source,size_t clip,double seconds,bool loop,pose_buffer &output) noexcept;
}
