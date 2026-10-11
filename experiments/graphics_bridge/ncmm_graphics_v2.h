#pragma once
/* Opt-in experimental software-rendered scene. The v1/Core ABI is untouched.
 * CPU sprites and scene pointers are read-only, callback scoped, and never GPU handles.
 * The Host validates and copies the complete frame before touching SDL. */
#include "ncmm_graphics.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_V2_ID "ncmm.host_api.v2.graphics"
#define NCMM_GRAPHICS_V2_CAP "graphics.viewport.v2"
#define NCMM_GRAPHICS_V2_ABI 2u
#define NCMM_GRAPHICS_MAX_FRAME_WIDTH 1920u
#define NCMM_GRAPHICS_MAX_FRAME_HEIGHT 1080u
enum { NCMM_GFX_GROUND = 5, NCMM_GFX_HELD_ITEM = 6, NCMM_GFX_FIELD = 7 };
enum { NCMM_SHAPE_FLAT, NCMM_SHAPE_WALL, NCMM_SHAPE_DOOR_CLOSED,
       NCMM_SHAPE_DOOR_OPEN, NCMM_SHAPE_WINDOW, NCMM_SHAPE_WINDOW_BROKEN,
       NCMM_SHAPE_FENCE, NCMM_SHAPE_GATE_OPEN, NCMM_SHAPE_TREE,
       NCMM_SHAPE_SHRUB, NCMM_SHAPE_PLANT, NCMM_SHAPE_ROCK,
       NCMM_SHAPE_TABLE, NCMM_SHAPE_STORAGE, NCMM_SHAPE_SEAT,
       NCMM_SHAPE_BILLBOARD, NCMM_SHAPE_WATER, NCMM_SHAPE_WINDOW_COVERED, NCMM_SHAPE_POST,
       NCMM_SHAPE_SIGN, NCMM_SHAPE_DOOR_FRAME, NCMM_SHAPE_COUNT };
enum { NCMM_SURFACE_TRANSPARENT = 1u, NCMM_SURFACE_TERRAIN_CEILING = 2u };
enum { NCMM_CREATURE_TINY = 4u, NCMM_CREATURE_SMALL = 8u,
       NCMM_CREATURE_LARGE = 16u, NCMM_CREATURE_HUGE = 32u };
enum { NCMM_VEH_FRAME = 1u, NCMM_VEH_ROOF = 2u, NCMM_VEH_BOARD = 4u,
       NCMM_VEH_WINDOW = 8u, NCMM_VEH_DOOR = 16u, NCMM_VEH_OPEN = 32u,
       NCMM_VEH_WHEEL = 64u, NCMM_VEH_SEAT = 128u, NCMM_VEH_CARGO = 256u,
       NCMM_VEH_ENGINE = 512u, NCMM_VEH_LIGHT = 1024u, NCMM_VEH_BROKEN = 2048u,
       NCMM_VEH_CONTROLS = 4096u };
enum { NCMM_WEATHER_DRY, NCMM_WEATHER_RAIN, NCMM_WEATHER_SNOW,
       NCMM_WEATHER_ACID, NCMM_WEATHER_SLEET, NCMM_WEATHER_DUST,
       NCMM_WEATHER_PORTAL };
enum { NCMM_GFX_LOOK_UP = 3, NCMM_GFX_LOOK_DOWN = 4, NCMM_GFX_QUALITY = 5 };
typedef struct ncmm_graphics_sprite_v2 {
    uint32_t struct_size, width, height, pitch_pixels;
    const uint32_t *rgba; /* packed 0xRRGGBBAA, unlit source pixels */
} ncmm_graphics_sprite_v2;
typedef struct ncmm_graphics_cell_v2 {
    uint32_t terrain_shape, furniture_shape, orientation, vehicle_features;
    uint32_t vehicle_group;
    uint32_t field_shape;
    uint32_t terrain_flags;
    float light_r, light_g, light_b, emission;
    float vehicle_x, vehicle_y, vehicle_yaw, vehicle_health;
    int32_t mount_x, mount_y;
    char ground[128], field[128];
} ncmm_graphics_cell_v2;
typedef struct ncmm_graphics_environment_v2 {
    float sun_x, sun_y, sun_z, moon_x, moon_y, moon_z;
    float daylight, cloud_cover, fog_density, precipitation;
    float wind_x, wind_y;
    float sky_r, sky_g, sky_b, sky_tint;
    double animation_seconds;
    uint32_t moon_phase, weather_kind, lightning, outdoors, held_kind;
    char held_item[128], weather_id[128];
} ncmm_graphics_environment_v2;
typedef struct ncmm_graphics_scene_v2 {
    uint32_t struct_size;
    ncmm_graphics_scene_v1 base;
    const ncmm_graphics_cell_v2 *details;
    size_t detail_count;
    ncmm_graphics_environment_v2 environment;
} ncmm_graphics_scene_v2;
typedef int (*ncmm_graphics_render_v2)(const ncmm_graphics_scene_v2 *, void *);
typedef struct ncmm_host_graphics_v2 {
    uint32_t abi_version, struct_size, major, minor;
    int (*register_view)(const char *, ncmm_graphics_render_v2, ncmm_graphics_input_v1, void *);
    int (*set_enabled)(const char *, int);
    int (*is_enabled)(const char *);
    int (*get_sprite)(uint32_t cell_index, uint32_t layer, ncmm_graphics_sprite_v2 *);
    int (*submit_frame)(const uint32_t *rgba, uint32_t width, uint32_t height, uint32_t pitch_pixels);
} ncmm_host_graphics_v2;
#ifdef __cplusplus
}
#endif
