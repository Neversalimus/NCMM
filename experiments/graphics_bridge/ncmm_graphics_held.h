#pragma once
#include <stddef.h>
#include <stdint.h>
#define NCMM_GRAPHICS_HELD_ID "ncmm.graphics.held.v1"
#define NCMM_GRAPHICS_HELD_CAP "graphics.held.v1"
#define NCMM_HELD_LOOKS_LIMIT 16u
#define NCMM_HELD_MOD_LIMIT 16u
#define NCMM_HELD_MOD_LOOKS_LIMIT 4u
#ifdef __cplusplus
extern "C" {
#endif
enum ncmm_held_flags_v1 {
    NCMM_HELD_GUN=1u,NCMM_HELD_DETACHABLE=2u,NCMM_HELD_MAGAZINE_PRESENT=4u,
    NCMM_HELD_RELOAD_ONE=8u,NCMM_HELD_RELOAD_EJECT=16u,NCMM_HELD_AUTOMATIC=32u,
    NCMM_HELD_POWERED=64u,NCMM_HELD_SPEAR=128u,NCMM_HELD_METAL=256u,
    NCMM_HELD_LIGHT_ACTIVE=512u,NCMM_HELD_LOOKS_CYCLE=1024u,NCMM_HELD_TRUNCATED=2048u,
    NCMM_HELD_MANUAL_ACTION=4096u,NCMM_HELD_PUMP_ACTION=8192u
};
enum ncmm_held_skill_v1 {NCMM_HELD_NO_GUN,NCMM_HELD_PISTOL,NCMM_HELD_SMG,NCMM_HELD_RIFLE,NCMM_HELD_SHOTGUN,NCMM_HELD_OTHER_GUN};
enum ncmm_held_quality_v1 {NCMM_HELD_AXE,NCMM_HELD_HAMMER,NCMM_HELD_PRY,NCMM_HELD_DIG,NCMM_HELD_MINE,NCMM_HELD_SAW,NCMM_HELD_WRENCH,NCMM_HELD_SCREW,NCMM_HELD_CUT,NCMM_HELD_QUALITY_COUNT};
typedef struct ncmm_held_mod_v1 {
    char canonical[128],looks_like[NCMM_HELD_MOD_LOOKS_LIMIT][128],location[64];
    uint32_t looks_count,flags;
    int32_t sight_dispersion,field_of_view;
    float loudness_multiplier,length_m;
} ncmm_held_mod_v1;
typedef struct ncmm_held_item_v1 {
    uint32_t struct_size,version,flags,gun_skill,looks_count,mod_count;
    float base_length_m,mass_kg;
    char canonical[128],looks_like[NCMM_HELD_LOOKS_LIMIT][128];
    int32_t qualities[NCMM_HELD_QUALITY_COUNT];
    ncmm_held_mod_v1 mods[NCMM_HELD_MOD_LIMIT];
} ncmm_held_item_v1;
typedef struct ncmm_held_metrics_v1 {
    uint32_t struct_size,version,matched,fallback_reason,attachments,draws,pose_matrices,static_builds,static_reuses;
    uint64_t model_identity;
    char model[64];
    uint64_t accessory_identities[4];
    char accessories[4][64];
} ncmm_held_metrics_v1;
typedef struct ncmm_host_graphics_held_v1 {
    uint32_t version,struct_size,major,minor;
    // Owned POD copy, available only to the active perspective callback.
    int (*get_item)(ncmm_held_item_v1 *);
    int (*submit_metrics)(const ncmm_held_metrics_v1 *);
} ncmm_host_graphics_held_v1;
#ifdef __cplusplus
}
#endif
