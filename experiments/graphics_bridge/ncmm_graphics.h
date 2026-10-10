#pragma once
/* Experimental, queried interface. The shipping Loader ABI/Core table is unchanged.
 * Pointers in a scene are read-only and valid only during render(). No engine or
 * GPU pointer crosses this boundary. Coordinates/UVs are viewport-relative. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_ID "ncmm.host_api.v1.graphics"
#define NCMM_GRAPHICS_CAP "graphics.viewport.v1"
#define NCMM_GRAPHICS_ABI 1u
#define NCMM_GRAPHICS_MAX_QUADS 50000u
#define NCMM_GRAPHICS_RADIUS 16
enum { NCMM_GFX_VISIBLE = 1u, NCMM_GFX_WALL = 2u, NCMM_GFX_NO_FLOOR = 4u };
enum { NCMM_GFX_SOLID, NCMM_GFX_TERRAIN, NCMM_GFX_FURNITURE, NCMM_GFX_ITEM, NCMM_GFX_CREATURE };
enum { NCMM_GFX_TURN_LEFT = 1, NCMM_GFX_TURN_RIGHT = 2 };
typedef struct ncmm_graphics_cell_v1 {
    uint32_t flags;
    float light;
    char terrain[128], furniture[128], item[128], creature[128];
} ncmm_graphics_cell_v1;
typedef struct ncmm_graphics_scene_v1 {
    uint32_t struct_size;
    int32_t pixel_width, pixel_height, grid_width, grid_height;
    float camera_x, camera_y;
    const ncmm_graphics_cell_v1 *cells;
    size_t cell_count;
} ncmm_graphics_scene_v1;
typedef struct ncmm_graphics_quad_v1 {
    float x0, y0, x1, y1, u0, v0, u1, v1;
    uint32_t rgba; /* 0xRRGGBBAA */
    uint32_t cell_index, layer;
} ncmm_graphics_quad_v1;
typedef int (*ncmm_graphics_render_v1)(const ncmm_graphics_scene_v1 *, void *);
typedef void (*ncmm_graphics_input_v1)(uint32_t, void *);
typedef struct ncmm_host_graphics_v1 {
    uint32_t abi_version, struct_size, major, minor;
    int (*register_view)(const char *module_id, ncmm_graphics_render_v1,
                         ncmm_graphics_input_v1, void *user_data);
    int (*set_enabled)(const char *module_id, int enabled);
    int (*is_enabled)(const char *module_id);
    /* Copy commands into a staging buffer. Failed/throwing renders are discarded
     * before touching the SDL target. Only visible scene cells may be referenced. */
    int (*submit_quads)(const ncmm_graphics_quad_v1 *, size_t count);
} ncmm_host_graphics_v1;
#ifdef __cplusplus
}
#endif
