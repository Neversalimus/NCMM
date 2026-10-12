#pragma once
#include <stddef.h>
#include <stdint.h>

/* Optional, callback-scoped submission. The Host owns every GPU object and the
 * SDL renderer's device. No device, texture or command-buffer handle crosses
 * this interface. Existing Core / Graphics V1 / Graphics V2 tables are intact. */
#define NCMM_GRAPHICS_GPU_ID "ncmm.host_api.v1.graphics_gpu"
#define NCMM_GPU_RESOURCE_LIMIT (256u * 1024u * 1024u)
#define NCMM_GPU_MAX_VERTICES 2000000u
#define NCMM_GPU_MAX_INDICES 6000000u
#define NCMM_GPU_MAX_DRAWS 16384u
#define NCMM_GPU_MAX_TEXTURES 2048u
#define NCMM_GPU_MAX_BONES 32768u
enum ncmm_gpu_mode { NCMM_GPU_AUTO, NCMM_GPU_FORCE, NCMM_GPU_CPU };
enum ncmm_gpu_pass { NCMM_GPU_OPAQUE, NCMM_GPU_TRANSPARENT, NCMM_GPU_HELD };
enum ncmm_gpu_material_flags {
    NCMM_GPU_TEXTURED=1u, NCMM_GPU_CUTOUT=2u, NCMM_GPU_OPAQUE_TEXTURE=4u,
    NCMM_GPU_SMOOTH=8u, NCMM_GPU_CREATURE=16u, NCMM_GPU_SPILL=32u,
    NCMM_GPU_IMPOSTOR=64u, NCMM_GPU_SILHOUETTE=128u, NCMM_GPU_HELD_MATERIAL=256u
};
typedef struct ncmm_gpu_vertex_v1 {
    float position[3],normal[3],uv[2];
    uint32_t material,joint;
} ncmm_gpu_vertex_v1;
typedef struct ncmm_gpu_matrix_v1 { float columns[16]; } ncmm_gpu_matrix_v1;
typedef struct ncmm_gpu_material_v1 {
    float color[4];
    uint64_t texture_key,normal_key,depth_key;
    uint32_t flags,finish,liquid,banks;
    float impostor_span,impostor_yaw;
    uint32_t reserved[2];
} ncmm_gpu_material_v1;
typedef struct ncmm_gpu_texture_v1 {
    uint64_t key;
    uint32_t width,height,pitch;
    const uint32_t *rgba;
} ncmm_gpu_texture_v1;
typedef struct ncmm_gpu_mesh_v1 {
    uint64_t key;
    const ncmm_gpu_vertex_v1 *vertices;
    const uint32_t *indices;
    const ncmm_gpu_material_v1 *materials;
    uint32_t vertex_count,index_count,material_count,reserved;
} ncmm_gpu_mesh_v1;
typedef struct ncmm_gpu_draw_v1 {
    uint64_t mesh_key;
    ncmm_gpu_matrix_v1 transform;
    uint32_t bone_offset,bone_count,cell,pass;
} ncmm_gpu_draw_v1;
typedef struct ncmm_gpu_cell_v1 {
    float light[4],source[4],volume[4],volume_color[4];
    uint32_t flags,gas,reserved[2];
} ncmm_gpu_cell_v1;
/* vec4 fields deliberately match shader std140 blocks. */
typedef struct ncmm_gpu_camera_v1 {
    float eye[4],right[4],up[4],forward[4];
    float projection[4],sun[4],moon[4],sky_top[4],sky_bottom[4];
    float weather[4],wind[4],time[4],grid[4];
} ncmm_gpu_camera_v1;
typedef struct ncmm_gpu_scene_v1 {
    uint32_t struct_size,width,height,mode;
    uint64_t world_epoch,tileset_epoch,frame_serial;
    ncmm_gpu_camera_v1 camera;
    const ncmm_gpu_mesh_v1 *meshes;
    const ncmm_gpu_texture_v1 *textures;
    const ncmm_gpu_draw_v1 *draws;
    const ncmm_gpu_matrix_v1 *bones;
    const ncmm_gpu_cell_v1 *cells;
    uint32_t mesh_count,texture_count,draw_count,bone_count,cell_count,reserved;
} ncmm_gpu_scene_v1;
typedef struct ncmm_gpu_metrics_v1 {
    uint32_t struct_size,active,software_device,timestamp_queries;
    uint64_t frame_serial,draw_calls,triangles,resource_bytes,upload_bytes;
    uint64_t generated_meshes,mesh_hits,texture_hits,cpu_raster_calls;
    double resource_ms,upload_ms,submit_ms,gpu_ms;
    int32_t target_cell;
    float label_x,label_y;
    char backend[32],device[256],reason[256];
} ncmm_gpu_metrics_v1;
typedef struct ncmm_host_graphics_gpu_v1 {
    uint32_t version,struct_size,api_major,api_minor;
    int (*available)(ncmm_gpu_metrics_v1 *out);
    int (*submit_scene)(const ncmm_gpu_scene_v1 *scene);
    int (*metrics)(ncmm_gpu_metrics_v1 *out);
    void (*invalidate)(void);
} ncmm_host_graphics_gpu_v1;
