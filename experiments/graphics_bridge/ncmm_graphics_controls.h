#pragma once
/* Optional controls extension. The Core, graphics v1 and graphics v2 layouts
 * remain unchanged. Access is limited to the registered module's callbacks. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NCMM_GRAPHICS_CONTROLS_ID "ncmm.host_api.v1.first_person_controls"
#define NCMM_GRAPHICS_CONTROLS_CAP "graphics.controls.v1"
enum { NCMM_GFX_MOUSE_LOOK = 6 };
enum { NCMM_GRIP_SINGLE, NCMM_GRIP_POLE, NCMM_GRIP_GUN };
typedef struct ncmm_graphics_control_state_v1 {
    uint32_t struct_size, attack_kind, grip_kind, mouse_drag;
    uint64_t attack_serial;
    double attack_started;
    float mouse_dx, mouse_dy, held_length;
} ncmm_graphics_control_state_v1;
typedef struct ncmm_host_graphics_controls_v1 {
    uint32_t abi_version, struct_size, major, minor;
    int (*get_state)(ncmm_graphics_control_state_v1 *);
    int (*set_camera)(float yaw, float pitch);
} ncmm_host_graphics_controls_v1;
#ifdef __cplusplus
}
#endif
