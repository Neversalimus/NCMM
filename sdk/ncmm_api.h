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
#define NCMM_API_VERSION_MAJOR 1u
#define NCMM_API_VERSION_MINOR 3u
#define NCMM_ENTRYPOINT "ncmm_get_descriptor_v1"
#define NCMM_LOCALE_ENTRYPOINT "ncmm_on_locale_changed_v1"
#define NCMM_TURN_ENTRYPOINT "ncmm_on_turn_v1"
#define NCMM_OPEN_UI_ENTRYPOINT "ncmm_open_ui_v1"
#define NCMM_MIGRATE_STATE_ENTRYPOINT "ncmm_migrate_state_v1"

typedef enum ncmm_log_level_v1 {
    NCMM_LOG_INFO = 0,
    NCMM_LOG_WARN = 1,
    NCMM_LOG_ERROR = 2
} ncmm_log_level_v1;

typedef enum ncmm_ui_card_flags_v1 {
    NCMM_UI_CARD_NONE = 0u,
    NCMM_UI_CARD_OWNED = 1u << 0,
    NCMM_UI_CARD_LOCKED = 1u << 1,
    NCMM_UI_CARD_MAJOR = 1u << 2,
    NCMM_UI_CARD_EFFECT = 1u << 3,
    NCMM_UI_CARD_ACCENT = 1u << 4
} ncmm_ui_card_flags_v1;

typedef struct ncmm_ui_progress_v1 {
    const char *label;
    int64_t current;
    int64_t maximum;
} ncmm_ui_progress_v1;

/*
 * icon_key is intentionally a logical asset identifier rather than a file path.
 * NCMM 0.7.2's text renderer preserves this metadata but does not rasterize it yet.
 * A future graphics backend can resolve the same key without changing module data.
 */
typedef struct ncmm_ui_card_v1 {
    const char *id;
    const char *title;
    const char *subtitle;
    const char *body;
    const char *badge;
    const char *icon_key;
    uint32_t flags;
} ncmm_ui_card_v1;

/*
 * ui.tree.v1 is a semantic graph layout.  row/column describe logical node
 * placement; the host owns clipping, scrolling, navigation and connection drawing.
 * The same NCMM_UI_CARD_* flags style tree nodes.
 */
typedef struct ncmm_ui_tree_node_v1 {
    const char *id;
    const char *title;
    const char *subtitle;
    const char *body;
    const char *badge;
    const char *icon_key;
    uint32_t flags;
    int32_t row;
    int32_t column;
} ncmm_ui_tree_node_v1;

typedef struct ncmm_ui_tree_edge_v1 {
    size_t from_index;
    size_t to_index;
} ncmm_ui_tree_edge_v1;

enum {
    NCMM_UI_TREE_CANCEL = -1,
    NCMM_UI_TREE_SHOW_CARDS = -2
};

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

    /*
     * NCMM 0.5.2 world-options layout tail. Modules must require
     * world_options.layout.v1 before using these fields.
     */
    int ( *worldgen_group_begin )( const char *group_id,
                                   const char *display_name,
                                   const char *tooltip );
    void ( *worldgen_group_end )( void );
    int ( *worldgen_set_string_choices )( const char *option_id,
                                          const char *const *value_ids,
                                          const char *const *display_names,
                                          size_t count );

    /*
     * NCMM 0.6 character modifier tail. Modules must require
     * character.modifiers.v1 before using these fields.
     */
    int ( *character_modifier_set )( const char *module_id,
                                     const char *modifier_id,
                                     double value );
    int ( *character_modifier_clear_module )( const char *module_id );

    /*
     * NCMM 0.7 semantic API tail. The binary ABI remains v1; modules must
     * require api.versioning.v1 before using these fields.
     */
    uint32_t ( *get_api_version_major )( void );
    uint32_t ( *get_api_version_minor )( void );

    /*
     * NCMM validation tail extension. Modules must require ui.tiles.v1 before
     * using this field. The ABI v1 prefix remains unchanged.
     */
    int ( *ui_tile_choose )( const char *title,
                             const char *const *labels,
                             const char *const *details,
                             size_t count,
                             size_t columns );
    /*
     * NCMM 0.7.2 card/layout tail. Modules must require ui.cards.v1 before use.
     * icon_key fields are forward-compatible graphics metadata in 0.7.2.
     */
    int ( *ui_card_choose )( const char *title,
                             const char *summary,
                             const ncmm_ui_progress_v1 *progress,
                             const ncmm_ui_card_v1 *cards,
                             size_t count,
                             size_t columns );

    /*
     * NCMM Host API 1.3 tail. Modules must require ui.tree.v1 before use.
     * Returns a node index, NCMM_UI_TREE_CANCEL, or NCMM_UI_TREE_SHOW_CARDS.
     */
    int ( *ui_tree_choose )( const char *title,
                             const char *summary,
                             const ncmm_ui_progress_v1 *progress,
                             const ncmm_ui_tree_node_v1 *nodes,
                             size_t node_count,
                             const ncmm_ui_tree_edge_v1 *edges,
                             size_t edge_count );
} ncmm_host_api_v1;

typedef int ( *ncmm_mod_init_v1 )( const ncmm_host_api_v1 *api );
typedef void ( *ncmm_mod_shutdown_v1 )( void );
typedef void ( *ncmm_on_locale_changed_v1_fn )( const ncmm_host_api_v1 *api );
typedef void ( *ncmm_on_turn_v1_fn )( const ncmm_host_api_v1 *api );
typedef void ( *ncmm_open_ui_v1_fn )( const ncmm_host_api_v1 *api );
typedef int ( *ncmm_migrate_state_v1_fn )( const ncmm_host_api_v1 *api,
                                           uint32_t from_schema,
                                           uint32_t to_schema );

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
