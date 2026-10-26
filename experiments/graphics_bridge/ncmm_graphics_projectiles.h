#pragma once
/* Optional owned copies of confirmed Native projectile paths. Presentation
 * never fires a shot, rolls dispersion, changes damage or reads game pointers. */
#include "ncmm_graphics_v2.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_PROJECTILES_ID "ncmm.host_api.v1.projectile_presentation"
#define NCMM_GRAPHICS_PROJECTILES_V2_ID "ncmm.host_api.v2.projectile_presentation"
#define NCMM_PROJECTILE_LIMIT 32u
#define NCMM_PROJECTILE_POINTS 64u
enum { NCMM_PROJECTILE_BULLET=1,NCMM_PROJECTILE_ARROW,NCMM_PROJECTILE_BOLT,
       NCMM_PROJECTILE_PELLET,NCMM_PROJECTILE_LASER,NCMM_PROJECTILE_PLASMA,NCMM_PROJECTILE_FLAME,NCMM_PROJECTILE_THROWN };
typedef struct ncmm_projectile_point_v1 { int32_t x,y,z; } ncmm_projectile_point_v1;
typedef struct ncmm_graphics_projectile_v1 {
    uint64_t serial,world_epoch;
    double seconds;
    uint32_t kind,count;
    char weapon[128];
    ncmm_projectile_point_v1 path[NCMM_PROJECTILE_POINTS];
} ncmm_graphics_projectile_v1;
typedef struct ncmm_host_graphics_projectiles_v1 {
    uint32_t abi_version,struct_size,major,minor;
    int (*get_paths)(ncmm_graphics_projectile_v1 *,size_t);
} ncmm_host_graphics_projectiles_v1;
// Additive optional interface. The original v1 layout and entry remain intact.
typedef struct ncmm_projectile_aim_v1 {
    uint32_t free_ray;
    float offset_x,offset_y,eye_height,yaw,pitch;
} ncmm_projectile_aim_v1;
typedef struct ncmm_graphics_projectile_v2 {
    ncmm_graphics_projectile_v1 base;
    ncmm_projectile_aim_v1 aim;
} ncmm_graphics_projectile_v2;
typedef struct ncmm_host_graphics_projectiles_v2 {
    uint32_t abi_version,struct_size,major,minor;
    int (*get_paths)(ncmm_graphics_projectile_v2 *,size_t);
} ncmm_host_graphics_projectiles_v2;
#ifdef __cplusplus
}
namespace ncmm_projectile {
// Start flight at the first actual presentation. A slow first CPU raster must
// not consume the complete animation before the player can see it.
struct presentation_clock {
    double start=0;bool sampled=false,committed=false;
    double sample(double now){sampled=true;return (committed?start:now)-.01;}
    void presented(double now){if(sampled&&!committed){start=now;committed=true;}}
};
}
#endif
