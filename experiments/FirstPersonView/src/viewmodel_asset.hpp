#pragma once
#include "ncmm_graphics_gpu.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace first_person::viewmodel {
struct point { float x=0, y=0, z=0; };
struct quaternion { float x=0, y=0, z=0, w=1; };
using matrix = ncmm_gpu_matrix_v1;
struct local_pose { point translation; quaternion rotation; point scale{1,1,1}; };
struct node { const char *name; int32_t parent; local_pose bind; };
struct weights { std::array<uint16_t,4> joints; std::array<float,4> values; };
struct vertex {
    point position, normal;
    float u, v;
    uint16_t palette;
    uint8_t side, region; // source .R=0/.L=1; forearm=0/hand=1
};
enum class channel : uint8_t { translation, rotation, scale };
enum class interpolation : uint8_t { linear, step };
struct keyframe { float time; std::array<float,4> value; };
struct track { uint16_t target; channel property; interpolation mode; uint32_t first, count; };
struct clip { const char *name; float duration; uint32_t first, count; };
struct asset {
    const vertex *vertices; size_t vertex_count;
    const uint32_t *indices; size_t index_count;
    const node *nodes; const uint16_t *order; size_t node_count;
    const uint16_t *joints; const matrix *inverse_bind; size_t joint_count;
    const weights *palettes; size_t palette_count;
    const keyframe *keys; size_t key_count;
    const track *tracks; size_t track_count;
    const clip *clips; size_t clip_count;
    const uint32_t *bare_pixels, *glove_pixels;
    uint32_t texture_width, texture_height;
};
// Typed immutable tables, generated offline from the pinned author's archive.
// Texture words are compiled by CMake; neither GLB nor PNG is parsed in-game.
extern const uint32_t psx_bare_pixels[512*512];
extern const uint32_t psx_glove_pixels[512*512];
const asset &imported_asset() noexcept;
}
