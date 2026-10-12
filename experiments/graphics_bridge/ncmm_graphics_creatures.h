#pragma once
/* Optional read-only creature presentation. Core/Graphics V1/V2 are unchanged.
 * The Host returns only currently visible creatures, plus short, visibility-
 * revalidated death presentations. IDs are opaque and scoped to world_epoch.
 * No game pointers, save fields, physics or simulation updates cross this ABI. */
#include "ncmm_graphics_v2.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_CREATURES_ID "ncmm.host_api.v1.creature_presentation"
#define NCMM_GRAPHICS_CREATURES_CAP "graphics.creatures.v1"
enum {
    NCMM_CR_HUMAN=1u, NCMM_CR_FLIES=2u, NCMM_CR_SWIMS=4u,
    NCMM_CR_AQUATIC=8u, NCMM_CR_BURROWS=16u, NCMM_CR_AMORPHOUS=32u,
    NCMM_CR_MECHANICAL=64u, NCMM_CR_NOHEAD=128u, NCMM_CR_IMMOBILE=256u,
    NCMM_CR_RANGED=512u, NCMM_CR_FUR=1024u, NCMM_CR_SCALES=2048u,
    NCMM_CR_CHITIN=4096u, NCMM_CR_NPC=8192u, NCMM_CR_PRONE=16384u,
    NCMM_CR_SUBMERGED=32768u, NCMM_CR_DEAD=65536u
};
enum { NCMM_CR_IDLE, NCMM_CR_MELEE, NCMM_CR_SHOOT, NCMM_CR_HURT, NCMM_CR_DIE };
typedef struct ncmm_graphics_creature_scene_v1 {
    uint32_t struct_size, count;
    uint64_t world_epoch, tileset_epoch;
    double seconds;
} ncmm_graphics_creature_scene_v1;
typedef struct ncmm_graphics_creature_v1 {
    uint64_t entity_id, appearance_revision, event_serial;
    uint32_t cell, flags, size_category, event;
    /* Absolute coordinates keep movement stable when the map bubble shifts. */
    int32_t world_x, world_y, world_z;
    float liters, kilograms, height, width, elevation, speed, heading;
    double event_seconds;
    char type_id[128], bodytype[48], species[256];
} ncmm_graphics_creature_v1;
/* Presentation diagnostics only. These counters never feed gameplay. */
typedef struct ncmm_graphics_creature_metrics_v1 {
    uint32_t struct_size, lod[4], fallback, culled;
    uint64_t cache_bytes, cache_peak_bytes, generated, entries, queued, evictions, failures, visible_pixels;
    double generation_ms, generation_max_ms, generation_total_ms;
    uint64_t anatomy[13], reasons[5];
} ncmm_graphics_creature_metrics_v1;
typedef struct ncmm_host_graphics_creatures_v1 {
    uint32_t abi_version, struct_size, major, minor;
    int (*get_scene)(ncmm_graphics_creature_scene_v1 *);
    int (*get_creatures)(ncmm_graphics_creature_v1 *, size_t);
    int (*submit_metrics)(const ncmm_graphics_creature_metrics_v1 *);
} ncmm_host_graphics_creatures_v1;
#ifdef __cplusplus
}
#endif
