#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifdef NCMM_MOD_BUILD
#    define NCMM_EXPORT __declspec(dllexport)
#  else
#    define NCMM_EXPORT
#  endif
#else
#  define NCMM_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define NCMM_ABI_VERSION 1u
#define NCMM_LOADER_API_VERSION 1u
#define NCMM_ENTRYPOINT "ncmm_get_descriptor_v1"
#define NCMM_LOCALE_ENTRYPOINT "ncmm_on_locale_changed_v1"
#define NCMM_TURN_ENTRYPOINT "ncmm_on_turn_v1"
#define NCMM_OPEN_UI_ENTRYPOINT "ncmm_open_ui_v1"

typedef enum ncmm_log_level_v1 {
    NCMM_LOG_INFO = 0,
    NCMM_LOG_WARN = 1,
    NCMM_LOG_ERROR = 2
} ncmm_log_level_v1;

typedef struct ncmm_host_api_v1 {
    uint32_t abi_version;
    void ( *log )( ncmm_log_level_v1 level, const char *message );
    int ( *has_capability )( const char *capability );
    int ( *can_expose_worldgen_option )( const char *option_id );
    int ( *expose_worldgen_option )( const char *option_id,
                                     const char *display_name,
                                     const char *tooltip );
    const char *( *get_locale )( void );

    /* NCMM 0.4 tail extension. */
    const char *( *get_host_version )( void );
    uint32_t ( *get_loader_api )( void );
    size_t ( *get_capability_count )( void );
    const char *( *get_capability )( size_t index );

    /*
     * NCMM 0.5 tail extension. Modules must require the matching capability
     * before using these fields. Existing ABI v1 modules keep the old prefix.
     */
    int ( *character_state_available )( void );
    int64_t ( *character_state_get_i64 )( const char *module_id,
                                          const char *key,
                                          int64_t fallback );
    int ( *character_state_set_i64 )( const char *module_id,
                                      const char *key,
                                      int64_t value );
    int ( *ui_choose )( const char *title,
                        const char *const *entries,
                        size_t count );
    void ( *ui_message )( const char *message );
} ncmm_host_api_v1;

typedef int ( *ncmm_mod_init_v1 )( const ncmm_host_api_v1 *api );
typedef void ( *ncmm_mod_shutdown_v1 )( void );
typedef void ( *ncmm_on_locale_changed_v1_fn )( const ncmm_host_api_v1 *api );
typedef void ( *ncmm_on_turn_v1_fn )( const ncmm_host_api_v1 *api );
typedef void ( *ncmm_open_ui_v1_fn )( const ncmm_host_api_v1 *api );

typedef struct ncmm_mod_descriptor_v1 {
    uint32_t abi_version;
    const char *id;
    const char *name;
    const char *version;
    const char *const *required_capabilities;
    size_t required_capability_count;
    ncmm_mod_init_v1 init;
    ncmm_mod_shutdown_v1 shutdown;
} ncmm_mod_descriptor_v1;

typedef const ncmm_mod_descriptor_v1 *( *ncmm_get_descriptor_v1_fn )( void );

#ifdef __cplusplus
}
#endif
