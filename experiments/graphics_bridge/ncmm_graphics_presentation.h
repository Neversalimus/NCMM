#pragma once
/* Optional, callback-scoped presentation metadata. No Core/V1/V2 ABI change,
 * game object pointers, mutable game state, or GPU device handles. */
#include "ncmm_graphics_v2.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_PRESENTATION_ID "ncmm.host_api.v1.first_person_presentation"
#define NCMM_GRAPHICS_PRESENTATION_CAP "graphics.presentation.v1"
enum { NCMM_SKY_EARTH, NCMM_SKY_VOID, NCMM_SKY_HIGHLANDS, NCMM_SKY_SCARLET,
       NCMM_SKY_GREY, NCMM_SKY_STARS, NCMM_SKY_RADIOSPHERE, NCMM_SKY_FUNGAL };
enum { NCMM_MAT_DEFAULT, NCMM_MAT_WOOD, NCMM_MAT_METAL, NCMM_MAT_GLASS,
       NCMM_MAT_STONE, NCMM_MAT_RESIN, NCMM_MAT_CLOTH };
enum { NCMM_LIQ_NONE, NCMM_LIQ_FRESH, NCMM_LIQ_SEA, NCMM_LIQ_SEWAGE,
       NCMM_LIQ_BLOOD, NCMM_LIQ_ACID, NCMM_LIQ_SLIME, NCMM_LIQ_LAVA,
       NCMM_LIQ_OIL, NCMM_LIQ_OTHER };
enum { NCMM_GAS_NONE, NCMM_GAS_SMOKE, NCMM_GAS_FOG, NCMM_GAS_TOXIC,
       NCMM_GAS_DUST, NCMM_GAS_SPORES, NCMM_GAS_FIRE };
enum { NCMM_GUN_NONE, NCMM_GUN_PISTOL, NCMM_GUN_REVOLVER, NCMM_GUN_RIFLE,
       NCMM_GUN_SHOTGUN, NCMM_GUN_SMG, NCMM_GUN_LAUNCHER, NCMM_GUN_BOW };
enum { NCMM_MOD_OPTIC=1u, NCMM_MOD_SUPPRESSOR=2u, NCMM_MOD_FOREGRIP=4u,
       NCMM_MOD_BIPOD=8u, NCMM_MOD_LASER=16u, NCMM_MOD_LIGHT=32u,
       NCMM_MOD_BAYONET=64u, NCMM_MOD_UNDERBARREL=128u, NCMM_MOD_STOCK=256u };
enum { NCMM_HAND_FUR=1u, NCMM_HAND_SCALES=2u, NCMM_HAND_CHITIN=4u,
       NCMM_HAND_CLAWS=8u, NCMM_HAND_BIONIC=16u, NCMM_HAND_WEBBED=32u };
enum { NCMM_PRESENT_GPU_EFFECTS=1u };
enum { NCMM_GFX_SLEEVE_LEFT=8, NCMM_GFX_SLEEVE_RIGHT=9,
       NCMM_GFX_GLOVE_LEFT=10, NCMM_GFX_GLOVE_RIGHT=11 };
typedef struct ncmm_graphics_detail_v1 {
    uint32_t terrain_material, furniture_material, ceiling_material;
    uint32_t liquid, field_liquid, gas, field_intensity;
    uint32_t hostile, vehicle_inside;
    float creature_height, creature_width, creature_elevation;
    float source_light, source_r, source_g, source_b;
    char ceiling[128];
} ncmm_graphics_detail_v1;
typedef struct ncmm_graphics_presentation_v1 {
    uint32_t struct_size, flags, sky_style, occupied_vehicle, gun_kind, gunmods;
    uint32_t hand_features, arm_features, sleeve_material[2], glove_material[2];
    float eye_height, body_scale, held_length;
    char dimension[128], sleeve[2][128], glove[2][128];
} ncmm_graphics_presentation_v1;
typedef struct ncmm_host_graphics_presentation_v1 {
    uint32_t abi_version, struct_size, major, minor;
    int (*get_scene)(ncmm_graphics_presentation_v1 *);
    int (*get_cells)(ncmm_graphics_detail_v1 *, size_t);
    /* Select a visible sprite, using normalized screen coordinates above it.
     * The Host resolves/localizes its name, revalidates aim before an attack. */
    int (*set_target)(int32_t cell, float label_x, float label_y);
    int (*submit_effects)(const uint32_t *, uint32_t, uint32_t, uint32_t);
} ncmm_host_graphics_presentation_v1;
#ifdef __cplusplus
}
#endif
