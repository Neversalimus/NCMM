#pragma once
#include "viewmodel_animation.hpp"
#include "ncmm_graphics_held.h"
#include <cstddef>
#include <cstdint>
#include <iterator>
namespace first_person::held {
constexpr uint32_t asset_version=1;
enum class family:uint32_t {pistol,revolver,smg,assault,battle,pump,auto_shotgun,knife,machete,bat,axe,hatchet,sledge,spear,
    hammer,crowbar,shovel,pickaxe,saw,wrench,screwdriver,pliers,bottle,gas_can,medical,flashlight,optic,suppressor,foregrip,gun_light};
enum class part:uint8_t {body,magazine,round};
struct contact {uint16_t node=0;viewmodel::point point;};
struct contacts {
    contact primary,secondary,trigger,muzzle,sight,pivot;
    viewmodel::point primary_axis{0,1,0},secondary_axis{0,1,0};
};
struct material {
    float color[4];uint32_t finish;
    const uint32_t *pixels;uint32_t width,height;
    uint64_t texture_identity;
};
struct model {
    uint32_t version;const char *id;uint64_t identity;family kind;
    viewmodel::asset geometry;
    const uint16_t *vertex_material;const part *triangle_part;
    const material *materials;size_t material_count;
    contacts grips;
    int16_t optic_socket,muzzle_socket,rail_socket,light_socket,trigger_node,slide_node;
    viewmodel::point slide_translation;
    float nominal_length;bool two_handed;
};
// Immutable tables prepared offline. No model/texture decoding in the game.
const model *const *models(size_t &count) noexcept;
const model *find(const char *id) noexcept;
bool valid(const model &value) noexcept;
}
