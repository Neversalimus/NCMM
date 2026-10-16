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
#define NCMM_GPU_MAX_DRAWS 32768u
#define NCMM_GPU_MAX_TEXTURES 2048u
#define NCMM_GPU_MAX_BONES 32768u
#define NCMM_GPU_DYNAMIC_MESH 1u
enum ncmm_gpu_mode { NCMM_GPU_AUTO, NCMM_GPU_FORCE, NCMM_GPU_CPU };
enum ncmm_gpu_pass { NCMM_GPU_OPAQUE, NCMM_GPU_TRANSPARENT, NCMM_GPU_HELD };
enum ncmm_gpu_material_flags {
    NCMM_GPU_TEXTURED=1u, NCMM_GPU_CUTOUT=2u, NCMM_GPU_OPAQUE_TEXTURE=4u,
    NCMM_GPU_SMOOTH=8u, NCMM_GPU_CREATURE=16u, NCMM_GPU_SPILL=32u,
    NCMM_GPU_IMPOSTOR=64u, NCMM_GPU_SILHOUETTE=128u, NCMM_GPU_HELD_MATERIAL=256u,
    NCMM_GPU_CULL_BACK=512u, NCMM_GPU_BILLBOARD=1024u, NCMM_GPU_CREATURE_VISUAL=2048u, NCMM_GPU_SLAB_UNDERSIDE=4096u, NCMM_GPU_DISTANCE_LOD=8192u
};
typedef struct ncmm_gpu_vertex_v1 {
    float position[3],normal[3],uv[2];
    uint32_t material,joint;
    float face_normal[3];
    uint32_t flags;
    float pivot[3];
    uint32_t reserved;
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
    float creature_yaw;
    uint32_t reserved[3];
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
    double geometry_ms;
    uint64_t producer_cache_bytes;
    uint32_t generated_cells,reserved_tail;
} ncmm_gpu_scene_v1;
typedef struct ncmm_gpu_metrics_v1 {
    uint32_t struct_size,active,software_device,timestamp_queries;
    uint64_t frame_serial,draw_calls,triangles,resource_bytes,upload_bytes;
    uint64_t generated_meshes,mesh_hits,texture_hits,cpu_raster_calls;
    double resource_ms,upload_ms,submit_ms,gpu_ms;
    int32_t target_cell;
    float label_x,label_y;
    char backend[32],device[256],reason[256];
    int32_t world_origin_x,world_origin_y,world_origin_z;
    uint32_t reserved;
    uint64_t resource_epoch,visible_creature_pixels,completed_frame_serial;
    uint64_t producer_cache_bytes,host_cpu_cache_bytes,dynamic_mesh_updates,generated_cells;
    double geometry_ms,render_record_ms;
} ncmm_gpu_metrics_v1;
typedef struct ncmm_gpu_cpu_metrics_v1 {
    uint32_t struct_size,width,height,raster_calls;
    uint64_t triangles,cache_bytes;
    double frame_ms,geometry_ms,raster_ms,effects_ms;
} ncmm_gpu_cpu_metrics_v1;
typedef struct ncmm_host_graphics_gpu_v1 {
    uint32_t version,struct_size,api_major,api_minor;
    int (*available)(ncmm_gpu_metrics_v1 *out);
    int (*submit_scene)(const ncmm_gpu_scene_v1 *scene);
    int (*metrics)(ncmm_gpu_metrics_v1 *out);
    void (*invalidate)(void);
    int (*has_resource)(uint64_t key,uint32_t texture);
    int (*submit_cpu_metrics)(const ncmm_gpu_cpu_metrics_v1 *metrics);
} ncmm_host_graphics_gpu_v1;
