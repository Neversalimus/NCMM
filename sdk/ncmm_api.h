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
#define NCMM_API_VERSION_MINOR 9u
#define NCMM_HOST_API_V2_CORE_ID "ncmm.host_api.v2.core"
#define NCMM_HOST_API_V2_CORE_ABI 2u
#define NCMM_HOST_API_V2_CORE_MAJOR 2u
#define NCMM_HOST_API_V2_CORE_MINOR 1u
/* capability: world_settings.v2 */
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
    NCMM_UI_TREE_SHOW_CARDS = -2,
    NCMM_UI_CARD_SHOW_TREE = -2
};

typedef enum ncmm_world_setting_scope_v2 {
    NCMM_WORLD_SETTING_LIVE = 0,
    NCMM_WORLD_SETTING_RELOAD = 1,
    NCMM_WORLD_SETTING_NEW_MAP = 2,
    NCMM_WORLD_SETTING_NEW_WORLD = 3
} ncmm_world_setting_scope_v2;
typedef enum ncmm_ui_color_v1 {
    NCMM_UI_COLOR_DEFAULT = 0,
    NCMM_UI_COLOR_RED = 1,
    NCMM_UI_COLOR_GREEN = 2,
    NCMM_UI_COLOR_CYAN = 3,
    NCMM_UI_COLOR_YELLOW = 4,
    NCMM_UI_COLOR_BLUE = 5,
    NCMM_UI_COLOR_MAGENTA = 6
} ncmm_ui_color_v1;

typedef enum ncmm_ui_theme_flags_v1 {
    NCMM_UI_THEME_NONE = 0u,
    NCMM_UI_THEME_STRONG_BORDER = 1u << 0,
    NCMM_UI_THEME_WIDE_NODES = 1u << 1,
    NCMM_UI_THEME_HORIZONTAL_VIEWPORT = 1u << 2,
    NCMM_UI_THEME_SECTIONED_DETAIL = 1u << 3
} ncmm_ui_theme_flags_v1;

typedef struct ncmm_ui_theme_v1 {
    uint32_t accent;
    uint32_t flags;
    int32_t preferred_node_width;
    int32_t preferred_detail_width;
    const uint32_t *item_accents;
    size_t item_accent_count;
} ncmm_ui_theme_v1;

typedef enum ncmm_ui_border_style_v1 {
    NCMM_UI_BORDER_AUTO = 0u,
    NCMM_UI_BORDER_NORMAL = 1u,
    NCMM_UI_BORDER_MAJOR = 2u,
    NCMM_UI_BORDER_PRIME = 3u,
    NCMM_UI_BORDER_EXCLUDED = 4u
} ncmm_ui_border_style_v1;

typedef struct ncmm_ui_theme_ex_v1 {
    uint32_t accent;
    uint32_t flags;
    int32_t preferred_node_width;
    int32_t preferred_detail_width;
    const uint32_t *item_accents;
    size_t item_accent_count;
    const uint32_t *item_border_styles;
    size_t item_border_style_count;
} ncmm_ui_theme_ex_v1;
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

    /*
     * NCMM Host API 1.4 tail.  gameplay.metrics.v1 exposes monotonic,
     * read-only counters derived from the native CDDA event bus. Modules
     * persist only their own deltas; the host never writes module state.
     */
    int64_t ( *gameplay_metric_get_i64 )( const char *metric_id );
    int ( *world_mod_active )( const char *mod_id );

    /* NCMM API 1.6: World Settings API v2. Additive ABI-v1 tail. */
    int ( *world_setting_register_bool )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           int default_value, uint32_t scope );
    int ( *world_setting_register_int )( const char *module_id, const char *setting_id,
                                          const char *display_name, const char *tooltip,
                                          int min_value, int max_value, int default_value,
                                          uint32_t scope );
    int ( *world_setting_register_float )( const char *module_id, const char *setting_id,
                                            const char *display_name, const char *tooltip,
                                            double min_value, double max_value,
                                            double default_value, double step,
                                            uint32_t scope );
    int ( *world_setting_register_enum )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           const char *const *value_ids,
                                           const char *const *display_names, size_t count,
                                           const char *default_value, uint32_t scope );
    int ( *world_setting_get_bool )( const char *setting_id, int fallback );
    int64_t ( *world_setting_get_i64 )( const char *setting_id, int64_t fallback );
    double ( *world_setting_get_f64 )( const char *setting_id, double fallback );
    const char *( *world_setting_get_string )( const char *setting_id, const char *fallback );

    /* NCMM API 1.7: dedicated experimental world-settings page. */
    int ( *worldgen_experimental_group_begin )( const char *group_id,
                                                 const char *display_name,
                                                 const char *tooltip );

    /* NCMM API 1.7: explicit, scoped UI themes; old card/tree APIs remain unchanged. */
    int ( *ui_card_choose_themed )( const char *title, const char *summary,
                                    const ncmm_ui_progress_v1 *progress,
                                    const ncmm_ui_card_v1 *cards, size_t count,
                                    size_t columns, const ncmm_ui_theme_v1 *theme );
    int ( *ui_tree_choose_themed )( const char *title, const char *summary,
                                    const ncmm_ui_progress_v1 *progress,
                                    const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                    const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                    const ncmm_ui_theme_v1 *theme );

    /* NCMM API 1.8: enumerate active world mods through a stable host registry. */
    size_t ( *world_mod_count )();
    const char *( *world_mod_id )( size_t index );

    /* v8.7 additive UI host extension: per-node border styles, horizontal tree viewport,
       and structured/sectioned detail panes for RPG-like progression screens. */
    int ( *ui_card_choose_rpg )( const char *title, const char *summary,
                                 const ncmm_ui_progress_v1 *progress,
                                 const ncmm_ui_card_v1 *cards, size_t count,
                                 size_t columns, const ncmm_ui_theme_ex_v1 *theme );
    int ( *ui_tree_choose_rpg )( const char *title, const char *summary,
                                 const ncmm_ui_progress_v1 *progress,
                                 const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                 const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                 const ncmm_ui_theme_ex_v1 *theme );

    /* Legacy ABI-v1 bridge into stable Host API 2.x interface tables. */
    const void *( *query_interface )( const char *interface_id,
                                      uint32_t min_major, uint32_t min_minor );
} ncmm_host_api_v1;
typedef enum ncmm_event_id_v2 {
    NCMM_EVENT_HOST_READY_V2 = 1u,
    NCMM_EVENT_WORLD_LOADED_V2 = 2u,
    NCMM_EVENT_WORLD_UNLOADED_V2 = 3u,
    NCMM_EVENT_TURN_V2 = 4u,
    NCMM_EVENT_LOCALE_CHANGED_V2 = 5u,
    NCMM_EVENT_PLAYER_KILL_V2 = 6u
} ncmm_event_id_v2;

typedef enum ncmm_rule_selector_v2 {
    NCMM_SELECTOR_ANY_V2 = 0u,
    NCMM_SELECTOR_SUBJECT_ID_V2 = 1u,
    NCMM_SELECTOR_SOURCE_MOD_V2 = 2u,
    NCMM_SELECTOR_SOURCE_SPECIES_V2 = 3u,
    NCMM_SELECTOR_TARGET_SPECIES_V2 = 4u
} ncmm_rule_selector_v2;

typedef enum ncmm_worldgen_value_type_v2 {
    NCMM_WORLDGEN_BOOL_V2 = 1u,
    NCMM_WORLDGEN_INT_V2 = 2u,
    NCMM_WORLDGEN_FLOAT_V2 = 3u
} ncmm_worldgen_value_type_v2;

/* Generic typed-setting value kinds used by runtime setting bindings. */
typedef enum ncmm_setting_value_type_v2 {
    NCMM_SETTING_BOOL_V2 = 1u,
    NCMM_SETTING_INT_V2 = 2u,
    NCMM_SETTING_FLOAT_V2 = 3u
} ncmm_setting_value_type_v2;

typedef enum ncmm_virtual_item_flags_v2 {
    NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2 = 1u << 0,
    NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2 = 1u << 1,
    NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2 = 1u << 2,
    NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2 = 1u << 3,
    NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2 = 1u << 4
} ncmm_virtual_item_flags_v2;

typedef void ( *ncmm_event_callback_v2 )( uint32_t event_id, void *user_data );

typedef struct ncmm_host_api_v2_core {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t api_major;
    uint32_t api_minor;
    const ncmm_host_api_v1 *legacy_v1;

    void ( *log )( ncmm_log_level_v1 level, const char *message );
    int ( *has_capability )( const char *capability );
    const char *( *get_host_version )( void );
    const char *( *current_module_id )( void );

    int ( *event_available )( uint32_t event_id );
    int ( *event_subscribe )( const char *module_id, uint32_t event_id,
                              ncmm_event_callback_v2 callback, void *user_data );
    int ( *event_unsubscribe_all )( const char *module_id );

    int ( *world_setting_register_bool )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           int default_value, uint32_t scope );
    int ( *world_setting_register_int )( const char *module_id, const char *setting_id,
                                          const char *display_name, const char *tooltip,
                                          int min_value, int max_value, int default_value,
                                          uint32_t scope );
    int ( *world_setting_register_float )( const char *module_id, const char *setting_id,
                                            const char *display_name, const char *tooltip,
                                            double min_value, double max_value,
                                            double default_value, double step,
                                            uint32_t scope );
    int ( *world_setting_register_enum )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           const char *const *value_ids,
                                           const char *const *display_names, size_t count,
                                           const char *default_value, uint32_t scope );
    int ( *world_setting_get_bool )( const char *setting_id, int fallback );
    int64_t ( *world_setting_get_i64 )( const char *setting_id, int64_t fallback );
    double ( *world_setting_get_f64 )( const char *setting_id, double fallback );
    const char *( *world_setting_get_string )( const char *setting_id, const char *fallback );

    size_t ( *world_mod_count )( void );
    const char *( *world_mod_id )( size_t index );
    int ( *world_mod_active )( const char *mod_id );

    int ( *character_state_available )( void );
    int64_t ( *character_state_get_i64 )( const char *module_id, const char *key,
                                          int64_t fallback );
    int ( *character_state_set_i64 )( const char *module_id, const char *key,
                                      int64_t value );

    int ( *module_is_loaded )( const char *module_id );
    const char *( *module_version )( const char *module_id );
    const char *( *module_state )( const char *module_id );

    int ( *modifier_define )( const char *module_id, const char *modifier_id,
                              double min_value, double max_value );
    int ( *modifier_set )( const char *module_id, const char *modifier_id, double value );
    int ( *modifier_clear_module )( const char *module_id );
    double ( *modifier_get_total )( const char *modifier_id );

    int ( *runtime_hook_bind_modifier )( const char *module_id, const char *hook_id,
                                          uint32_t selector_kind, const char *selector_value,
                                          const char *modifier_id );
    double ( *runtime_hook_value )( const char *hook_id, const char *subject_id,
                                    const char *source_mod_id,
                                    const char *source_species_id,
                                    const char *target_species_id );

    int ( *worldgen_hook_bind_setting )( const char *module_id, const char *hook_id,
                                          const char *setting_id, uint32_t value_type );
    int ( *worldgen_hook_bool )( const char *hook_id, int fallback );
    int64_t ( *worldgen_hook_i64 )( const char *hook_id, int64_t fallback );
    double ( *worldgen_hook_f64 )( const char *hook_id, double fallback );

    /*
     * Host API 2.0 additive tail: bind module-owned LIVE/RELOAD typed settings
     * to generic engine-facing runtime hook IDs. Consumers must gate this tail
     * with capability runtime_settings.bindings.v2 and struct_size.
     */
    int ( *runtime_hook_bind_setting )( const char *module_id, const char *hook_id,
                                        const char *setting_id, uint32_t value_type );
    int ( *runtime_hook_bool )( const char *hook_id, int fallback );
    int64_t ( *runtime_hook_i64 )( const char *hook_id, int64_t fallback );
    double ( *runtime_hook_f64 )( const char *hook_id, double fallback );

    /*
     * Host API 2.1 additive tail: logical item slots keep real CDDA items in their
     * vanilla locations and bind them by a persistent marker + item UID.
     * capability: character.virtual_items.v1
     */
    int ( *virtual_item_choose )( const char *module_id, const char *slot_id,
                                  const char *title, uint32_t flags );
    int ( *virtual_item_clear )( const char *module_id, const char *slot_id );
    const char *( *virtual_item_name )( const char *module_id, const char *slot_id );
    int64_t ( *virtual_item_uid )( const char *module_id, const char *slot_id );
} ncmm_host_api_v2_core;
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
