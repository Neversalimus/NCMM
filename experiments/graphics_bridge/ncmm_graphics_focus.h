#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Optional, callback-scoped reticle extension. Existing public tables retain
 * their exact layouts and creature-only target semantics. */
#define NCMM_GRAPHICS_FOCUS_ID "ncmm.host_api.v1.first_person_focus"
typedef struct ncmm_host_graphics_focus_v1 {
    uint32_t abi_version,struct_size,major,minor;
    int (*set_focus)(int32_t cell);
} ncmm_host_graphics_focus_v1;
#ifdef __cplusplus
}
#endif
