#pragma once
/* Optional owned-copy transport extension. Existing Core/Graphics V1/V2 and
 * presentation layouts are unchanged. No vehicle/item pointers cross the ABI.
 * The three getters are available only during the owning render callback. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_VEHICLES_ID "ncmm.host_api.v1.modular_vehicles"
#define NCMM_GRAPHICS_VEHICLES_CAP "graphics.vehicles.v1"
#define NCMM_VEHICLE_MAX_VEHICLES 256u
#define NCMM_VEHICLE_MAX_PARTS 32768u
#define NCMM_VEHICLE_MAX_PARTS_PER_VEHICLE 4096u

enum {
    NCMM_VP_FRAME=1u, NCMM_VP_ROOF=2u, NCMM_VP_PANEL=4u,
    NCMM_VP_WINDOW=8u, NCMM_VP_DOOR=16u, NCMM_VP_WHEEL=32u,
    NCMM_VP_SEAT=64u, NCMM_VP_CARGO=128u, NCMM_VP_ENGINE=256u,
    NCMM_VP_LIGHT=512u, NCMM_VP_CONTROLS=1024u, NCMM_VP_ARMOR=2048u,
    NCMM_VP_SOLAR=4096u, NCMM_VP_TURRET=8192u, NCMM_VP_FLOOR=16384u,
    NCMM_VP_STEERABLE=32768u, NCMM_VP_FLOAT=65536u,
    NCMM_VP_BICYCLE=131072u, NCMM_VP_MOTORCYCLE=262144u,
    NCMM_VP_AISLE=524288u, NCMM_VP_BATTERY=1048576u,
    NCMM_VP_FUEL_TANK=2097152u, NCMM_VP_RACK=4194304u,
    NCMM_VP_OPENABLE=8388608u, NCMM_VP_HALF_PANEL=16777216u,
    NCMM_VP_WINDSHIELD=33554432u, NCMM_VP_CURTAIN=67108864u
};
enum {
    NCMM_VP_EXISTS=1u, NCMM_VP_BROKEN=2u, NCMM_VP_OPEN=4u,
    NCMM_VP_ENABLED=8u, NCMM_VP_LIGHT_ON=16u,
    NCMM_VP_DIRECTION_KNOWN=32u, NCMM_VP_WHEEL_SIZE_KNOWN=64u,
    NCMM_VP_TOPOLOGY_ONLY=128u,
    NCMM_VP_ROOF_SURFACE=256u
};
enum {
    NCMM_VM_DEFAULT, NCMM_VM_PAINT, NCMM_VM_METAL, NCMM_VM_RUBBER,
    NCMM_VM_GLASS, NCMM_VM_PLASTIC, NCMM_VM_CLOTH, NCMM_VM_LEATHER,
    NCMM_VM_ARMOR, NCMM_VM_WOOD, NCMM_VM_COUNT
};
enum { NCMM_VEHICLE_PARTIAL=1u, NCMM_VEHICLE_STEERING_KNOWN=2u };

typedef struct ncmm_graphics_vehicle_part_v1 {
    uint64_t part_id;
    uint32_t vehicle_index;
    int32_t cell_index; /* Native-permitted BODY cell, visible upper roof surface
                        * with ROOF_SURFACE, or -1 with TOPOLOGY_ONLY.
                        * Topology-only parts cannot emit any geometry/labels.
                        * No container contents, fuel or inventory are copied. */
    int32_t mount_x, mount_y, mount_z;
    uint32_t features, state, material;
    float health, wheel_radius, wheel_width, direction;
    float color[4];
    char id[128], variant[64], material_id[64];
} ncmm_graphics_vehicle_part_v1;

typedef struct ncmm_graphics_vehicle_v1 {
    uint64_t entity_id, structure_revision, state_revision;
    uint32_t legacy_group, first_part, part_count, flags;
    /* Mount (0,0) in the current small window coordinates; native pivot is
     * retained separately. Apply orientation once to all mount-local meshes. */
    float origin_x, origin_y, origin_z, yaw, pivot_x, pivot_y;
    int32_t min_x, min_y, max_x, max_y;
    float steering, native_velocity;
} ncmm_graphics_vehicle_v1;

typedef struct ncmm_graphics_vehicle_scene_v1 {
    uint32_t struct_size, version;
    uint64_t world_epoch, revision, occupied_vehicle, occupied_part;
    uint32_t vehicle_count, part_count;
    float level_height;
    uint32_t reserved;
    int32_t origin_x, origin_y, origin_z, reserved_position;
    double seconds;
    float eye_x, eye_y, eye_z;
    uint32_t eye_valid;
} ncmm_graphics_vehicle_scene_v1;

typedef struct ncmm_graphics_vehicle_metrics_v1 {
    uint32_t struct_size, version, active_animation, submitted_parts;
    uint64_t generated, reused, evicted, topology_builds, owned_bytes;
    double preparation_ms;
} ncmm_graphics_vehicle_metrics_v1;

typedef struct ncmm_host_graphics_vehicles_v1 {
    uint32_t abi_version, struct_size, major, minor;
    int (*get_scene)(ncmm_graphics_vehicle_scene_v1 *);
    int (*get_vehicles)(ncmm_graphics_vehicle_v1 *, size_t);
    int (*get_parts)(ncmm_graphics_vehicle_part_v1 *, size_t);
    int (*get_focus_cell)(void);
    int (*set_focus)(uint64_t vehicle_id, uint64_t part_id, uint64_t revision);
    int (*submit_metrics)(const ncmm_graphics_vehicle_metrics_v1 *);
} ncmm_host_graphics_vehicles_v1;
#ifdef __cplusplus
}
#endif
