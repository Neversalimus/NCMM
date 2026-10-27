#pragma once
/* Optional world-space extension. Core, Graphics V1/V2 and GPU/Mesh V1 keep
 * their original layouts. Every pointer is immutable and callback-scoped.
 * Cells are sparse, ordered by absolute (z,y,x), and have a unique frame index.
 * The nested V2 arrays carry appearance only; their index is never a position. */
#include "ncmm_graphics_v2.h"
#include "ncmm_graphics_gpu.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_MULTILEVEL_ID "ncmm.host_api.v1.graphics_multilevel"
#define NCMM_GRAPHICS_MULTILEVEL_CAP "graphics.multilevel.v1"
#define NCMM_WORLD_MAX_CELLS 65536u
#define NCMM_WORLD_MAX_SLICES 32u
#define NCMM_WORLD_LEVEL_HEIGHT 2.5f
enum ncmm_world_flags {
    NCMM_WORLD_FLOOR=1u, NCMM_WORLD_GLASS_FLOOR=2u, NCMM_WORLD_OPEN_DOWN=4u,
    NCMM_WORLD_OPAQUE=8u, NCMM_WORLD_BODY_VISIBLE=16u,
    NCMM_WORLD_FLOOR_TOP=32u, NCMM_WORLD_FLOOR_BOTTOM=64u,
    NCMM_WORLD_ROOFED=128u, NCMM_WORLD_UP=256u, NCMM_WORLD_DOWN=512u,
    NCMM_WORLD_RAMP=1024u, NCMM_WORLD_LADDER=2048u,
    NCMM_WORLD_WATER=4096u, NCMM_WORLD_UNKNOWN=8192u,
    NCMM_WORLD_CONNECTED_UP=16384u, NCMM_WORLD_CONNECTED_DOWN=32768u,
    NCMM_WORLD_CANOPY=65536u, NCMM_WORLD_RAMP_END=131072u,
    /* Static architecture submitted for physical raster occlusion; no contents/targets. */
    NCMM_WORLD_STRUCTURE_VISIBLE=262144u
};
typedef struct ncmm_world_cell_v1 {
    int32_t x,y,z; /* canonical absolute CDDA map-square coordinates */
    uint32_t flags,direction,reserved; /* direction: +X,+Y,-X,-Y */
    uint64_t revision;
    float lower_light[3]; /* actual RGB on the room side of a slab */
} ncmm_world_cell_v1;
typedef struct ncmm_world_slice_v1 {
    int32_t z;
    uint32_t first_cell,cell_count,reserved;
    uint64_t identity,revision;
} ncmm_world_slice_v1;
typedef struct ncmm_graphics_multilevel_scene_v1 {
    uint32_t struct_size,version;
    uint64_t world_epoch,revision,frame_serial;
    int32_t origin_x,origin_y,origin_z,loaded_min_z,loaded_max_z;
    float level_height,camera_dx,camera_dy,camera_dz;
    ncmm_graphics_scene_v2 appearance;
    const ncmm_world_cell_v1 *coordinates;
    const ncmm_world_slice_v1 *slices;
    uint32_t cell_count,slice_count;
} ncmm_graphics_multilevel_scene_v1;
typedef struct ncmm_gpu_multilevel_scene_v1 {
    uint32_t struct_size,version;
    ncmm_gpu_scene_v1 base;
    /* Same sorted cells as the source callback. Host validates exact identity
     * and packs an owned GPU buffer; no pointer survives submission. */
    const ncmm_world_cell_v1 *coordinates;
    uint32_t cell_count,reserved;
    int32_t origin_x,origin_y,origin_z,reserved_origin;
    float level_height;
} ncmm_gpu_multilevel_scene_v1;
/* Optional minor-1 horizon data. No entities, items or exact room topology.
 * Patches are coarse world-space boxes/ground sheets relative to scene origin.
 * Unknown overmap tiles carry only neutral atmospheric ground. */
#define NCMM_LOD_MAX_PATCHES 2048u
enum ncmm_lod_kind {NCMM_LOD_UNKNOWN=0,NCMM_LOD_FIELD=1,NCMM_LOD_FOREST=2,NCMM_LOD_WATER=3,NCMM_LOD_ROAD=4,NCMM_LOD_BUILDING=5};
typedef struct ncmm_lod_patch_v1 {
    float x,y,z,width,depth,height;
    float color[3];
    uint32_t kind;
} ncmm_lod_patch_v1;
typedef struct ncmm_graphics_lod_scene_v1 {
    uint32_t struct_size,version,count,enabled;
    uint64_t revision;
    float ground_z,reserved;
    const ncmm_lod_patch_v1 *patches;
} ncmm_graphics_lod_scene_v1;

/* Optional minor-2 replacement horizon. V1 get_lod and its kinds 0..5 are
 * unchanged. A feature refers to a ground patch in this complete geography;
 * that patch has no generic silhouette. No instance contents are published.
 * RADIO_STANDARD is the authored, fully disclosed standard radio_tower site:
 * four vertical corner stacks for six 2.5 m levels and a narrow seventh level.
 * Its nominal profile is 5.81 x 5.81 x 17.5 m, not a measured generic tower. */
#define NCMM_SKYLINE_MAX_FEATURES 961u
enum ncmm_skyline_kind {NCMM_SKYLINE_RADIO_STANDARD=1};
typedef struct ncmm_skyline_feature_v1 {
    float x,y,z,width,depth,height; /* centre XY, base Z, scene-relative metres */
    uint32_t kind,rotation,ground_patch,reserved; /* rotation: north/east/south/west */
} ncmm_skyline_feature_v1;
typedef struct ncmm_graphics_skyline_scene_v1 {
    uint32_t struct_size,version,count,reserved;
    uint64_t revision;
    ncmm_graphics_lod_scene_v1 geography;
    const ncmm_skyline_feature_v1 *features;
} ncmm_graphics_skyline_scene_v1;
typedef struct ncmm_multilevel_metrics_v1 {
    uint32_t struct_size,available_slices,visible_slices,processed_cells;
    uint32_t frustum_rejected,occlusion_rejected,budget_rejected,submitted_cells;
    uint64_t snapshot_bytes,source_cache_bytes,source_cache_hits,source_cache_misses;
    double extraction_ms,visibility_ms,transition_ms;
} ncmm_multilevel_metrics_v1;
typedef int (*ncmm_graphics_render_multilevel_v1)(const ncmm_graphics_multilevel_scene_v1 *,void *);
typedef struct ncmm_host_graphics_multilevel_v1 {
    uint32_t abi_version,struct_size,major,minor;
    int (*register_view)(const char *,ncmm_graphics_render_multilevel_v1,ncmm_graphics_input_v1,void *);
    int (*set_enabled)(const char *,int);
    int (*is_enabled)(const char *);
    int (*get_sprite)(uint32_t,uint32_t,ncmm_graphics_sprite_v2 *);
    int (*submit_frame)(const uint32_t *,uint32_t,uint32_t,uint32_t);
    int (*submit_gpu)(const ncmm_gpu_multilevel_scene_v1 *);
    int (*get_metrics)(ncmm_multilevel_metrics_v1 *);
    int (*get_lod)(ncmm_graphics_lod_scene_v1 *); /* optional minor 1, size-gated */
    int (*get_skyline)(ncmm_graphics_skyline_scene_v1 *); /* optional minor 2, size-gated; complete replacement */
} ncmm_host_graphics_multilevel_v1;
#ifdef __cplusplus
}
#endif
