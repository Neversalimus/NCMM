#include "ncmm_api.h"
#include "ncmm_fault_policy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
bool runtime_fault_policy_smoke()
{
    ncmm::runtime_fault_policy policy;
    if( policy.modifiers_quarantined ||
        policy.callback_quarantined( ncmm::runtime_callback_kind::turn ) ||
        policy.callback_quarantined( ncmm::runtime_callback_kind::locale ) ||
        policy.callback_quarantined( ncmm::runtime_callback_kind::ui ) ) {
        return false;
    }

    if( !policy.quarantine( ncmm::runtime_callback_kind::turn ) ||
        !policy.turn_quarantined || !policy.modifiers_quarantined ||
        policy.locale_quarantined || policy.ui_quarantined ) {
        return false;
    }

    if( policy.quarantine( ncmm::runtime_callback_kind::turn ) ) {
        return false;
    }

    if( !policy.quarantine( ncmm::runtime_callback_kind::ui ) ||
        !policy.ui_quarantined ||
        !policy.callback_quarantined( ncmm::runtime_callback_kind::ui ) ) {
        return false;
    }
    return true;
}

std::vector<std::string> exposed;
std::vector<std::string> groups;
std::map<std::string, std::string> option_groups;
std::string active_group;
std::vector<std::string> fixed_time_values;
bool simulate_missing_contract = false;
std::map<std::string, int64_t> character_state;
std::map<std::string, double> modifiers;
int ui_message_count = 0;
int ui_script = 0;
int ui_stage = 0;

void log_fn( ncmm_log_level_v1, const char *message )
{
    std::cout << ( message ? message : "" ) << '\n';
}

int has_capability_fn( const char *cap )
{
    if( cap == nullptr ) {
        return 0;
    }
    if( simulate_missing_contract &&
        ( std::strcmp( cap, "world_options.v1" ) == 0 ||
          std::strcmp( cap, "runtime_settings.bindings.v2" ) == 0 ) ) {
        return 0;
    }
    static const std::set<std::string> capabilities = {
        "core.v1", "world_options.v1", "world_options.layout.v1", "world_settings.v2",
        "world_options.experimental.v1", "locale.v1", "module_contract.v1", "host_info.v1",
        "compatibility.v1", "events.turn.v1", "character_state.v1", "character.modifiers.v1",
        "ui.basic.v1", "ui.tiles.v1", "ui.cards.v1", "ui.tree.v1", "gameplay.metrics.v1",
        "active_mods.v1", "ui.theme.v1", "active_mods.registry.v2", "host_api.v2.core",
        "events.core.v2", "character.modifiers.v2", "runtime_hooks.registry.v2",
        "ui.layout.v1", "module_hotkeys.v1", "module_hotkeys.context.v1",
        "ingame_manager.v1", "api.versioning.v1", "state.migration.v1",
        "module.lifecycle.v1", "settings.typed.v2", "worldgen.bindings.v2",
        "runtime_settings.bindings.v2"
    };
    return capabilities.count( cap ) != 0 ? 1 : 0;
}

const char *get_locale_fn()
{
    return "en";
}

const char *get_host_version_fn()
{
    return "0.8.2-smoke";
}

uint32_t get_loader_api_fn()
{
    return NCMM_LOADER_API_VERSION;
}

uint32_t get_api_version_major_fn()
{
    return NCMM_API_VERSION_MAJOR;
}

uint32_t get_api_version_minor_fn()
{
    return NCMM_API_VERSION_MINOR;
}

const char *smoke_caps[] = {
    "core.v1", "world_options.v1", "world_options.layout.v1", "world_settings.v2",
    "world_options.experimental.v1", "locale.v1", "module_contract.v1", "host_info.v1",
    "compatibility.v1", "events.turn.v1", "character_state.v1", "character.modifiers.v1",
    "ui.basic.v1", "ui.tiles.v1", "ui.cards.v1", "ui.tree.v1", "gameplay.metrics.v1",
    "active_mods.v1", "ui.theme.v1", "active_mods.registry.v2", "host_api.v2.core",
    "events.core.v2", "character.modifiers.v2", "runtime_hooks.registry.v2",
    "ui.layout.v1", "module_hotkeys.v1", "module_hotkeys.context.v1",
    "ingame_manager.v1", "api.versioning.v1", "state.migration.v1",
    "module.lifecycle.v1", "settings.typed.v2", "worldgen.bindings.v2",
    "runtime_settings.bindings.v2"
};

size_t get_capability_count_fn()
{
    return sizeof( smoke_caps ) / sizeof( smoke_caps[0] );
}

const char *get_capability_fn( size_t index )
{
    return index < get_capability_count_fn() ? smoke_caps[index] : nullptr;
}

int can_expose_fn( const char *id )
{
    static const std::set<std::string> allowed = {
        "SPAWN_DENSITY", "ITEM_SPAWNRATE", "MONSTER_SPEED",
        "MONSTER_RESILIENCE", "EVOLUTION_INVERSE_MULTIPLIER",
        "SEASON_LENGTH", "CONSTRUCTION_SCALING", "ETERNAL_SEASON",
        "ETERNAL_TIME_OF_DAY"
    };
    return id != nullptr && allowed.count( id ) != 0 ? 1 : 0;
}

int expose_fn( const char *id, const char *, const char * )
{
    if( id == nullptr ) {
        return 0;
    }
    exposed.emplace_back( id );
    if( !active_group.empty() ) {
        option_groups[id] = active_group;
    }
    return 1;
}

int group_begin_fn( const char *group_id, const char *, const char * )
{
    if( group_id == nullptr || *group_id == '\0' || !active_group.empty() ) {
        return 0;
    }
    active_group = group_id;
    groups.push_back( active_group );
    return 1;
}

void group_end_fn()
{
    active_group.clear();
}

int set_string_choices_fn( const char *option_id, const char *const *value_ids,
                           const char *const *display_names, size_t count )
{
    if( option_id == nullptr || value_ids == nullptr || display_names == nullptr ||
        std::strcmp( option_id, "ETERNAL_TIME_OF_DAY" ) != 0 || count != 3 ) {
        return 0;
    }
    fixed_time_values.clear();
    for( size_t i = 0; i < count; ++i ) {
        if( value_ids[i] == nullptr || display_names[i] == nullptr ) {
            return 0;
        }
        fixed_time_values.emplace_back( value_ids[i] );
    }
    return 1;
}

std::string state_key( const char *module_id, const char *key )
{
    return std::string( module_id ? module_id : "" ) + ":" + ( key ? key : "" );
}

int character_state_available_fn()
{
    return 1;
}

int64_t character_state_get_i64_fn( const char *module_id, const char *key, int64_t fallback )
{
    const auto it = character_state.find( state_key( module_id, key ) );
    return it == character_state.end() ? fallback : it->second;
}

int character_state_set_i64_fn( const char *module_id, const char *key, int64_t value )
{
    if( module_id == nullptr || key == nullptr ) {
        return 0;
    }
    character_state[state_key( module_id, key )] = value;
    return 1;
}

int modifier_set_fn( const char *module_id, const char *modifier_id, double value )
{
    if( module_id == nullptr || modifier_id == nullptr ) {
        return 0;
    }
    modifiers[std::string( module_id ) + ":" + modifier_id] = value;
    return 1;
}

int modifier_clear_fn( const char *module_id )
{
    if( module_id == nullptr ) {
        return 0;
    }
    const std::string prefix = std::string( module_id ) + ":";
    for( auto it = modifiers.begin(); it != modifiers.end(); ) {
        if( it->first.rfind( prefix, 0 ) == 0 ) {
            it = modifiers.erase( it );
        } else {
            ++it;
        }
    }
    return 1;
}

size_t worldgen_binding_count = 0;
size_t runtime_setting_binding_count = 0;
size_t runtime_hook_binding_count = 0;
size_t modifier_definition_count = 0;
size_t event_subscription_count = 0;
std::set<std::string> registered_setting_ids;
std::set<std::string> defined_v2_modifiers;
std::multimap<std::string, std::string> runtime_hooks_by_modifier;

enum class smoke_setting_kind {
    boolean,
    integer,
    floating,
    enumeration
};

struct smoke_setting_meta {
    smoke_setting_kind kind = smoke_setting_kind::integer;
    double min_value = 0.0;
    double max_value = 0.0;
    double default_value = 0.0;
    double step = 0.0;
    uint32_t scope = 0;
    std::vector<std::string> choices;
    std::string default_string;
};

struct smoke_worldgen_binding {
    std::string setting_id;
    uint32_t type = 0;
};

std::map<std::string, smoke_setting_meta> setting_meta;
std::map<std::string, int64_t> setting_i64_values;
std::map<std::string, double> setting_f64_values;
std::map<std::string, std::string> setting_string_values;
std::map<std::string, smoke_worldgen_binding> worldgen_bindings;
std::map<std::string, smoke_worldgen_binding> runtime_setting_bindings;
std::set<std::string> active_world_mods;

int world_setting_register_bool_fn( const char *, const char *setting_id, const char *, const char *,
                                    int default_value, uint32_t scope )
{
    if( setting_id == nullptr || *setting_id == '\0' ) return 0;
    smoke_setting_meta meta;
    meta.kind = smoke_setting_kind::boolean;
    meta.min_value = 0.0;
    meta.max_value = 1.0;
    meta.default_value = default_value ? 1.0 : 0.0;
    meta.step = 1.0;
    meta.scope = scope;
    setting_meta[setting_id] = meta;
    setting_i64_values[setting_id] = default_value ? 1 : 0;
    registered_setting_ids.insert( setting_id );
    return 1;
}

int world_setting_register_int_fn( const char *, const char *setting_id, const char *, const char *,
                                   int min_value, int max_value, int default_value, uint32_t scope )
{
    if( setting_id == nullptr || *setting_id == '\0' || min_value > max_value ||
        default_value < min_value || default_value > max_value ) return 0;
    smoke_setting_meta meta;
    meta.kind = smoke_setting_kind::integer;
    meta.min_value = min_value;
    meta.max_value = max_value;
    meta.default_value = default_value;
    meta.step = 1.0;
    meta.scope = scope;
    setting_meta[setting_id] = meta;
    setting_i64_values[setting_id] = default_value;
    registered_setting_ids.insert( setting_id );
    return 1;
}

int world_setting_register_float_fn( const char *, const char *setting_id, const char *, const char *,
                                     double min_value, double max_value, double default_value,
                                     double step, uint32_t scope )
{
    if( setting_id == nullptr || *setting_id == '\0' || min_value > max_value ||
        default_value < min_value || default_value > max_value || step <= 0.0 ) return 0;
    smoke_setting_meta meta;
    meta.kind = smoke_setting_kind::floating;
    meta.min_value = min_value;
    meta.max_value = max_value;
    meta.default_value = default_value;
    meta.step = step;
    meta.scope = scope;
    setting_meta[setting_id] = meta;
    setting_f64_values[setting_id] = default_value;
    registered_setting_ids.insert( setting_id );
    return 1;
}

int world_setting_register_enum_fn( const char *, const char *setting_id, const char *, const char *,
                                    const char *const *value_ids, const char *const *display_names,
                                    size_t count, const char *default_value, uint32_t scope )
{
    if( setting_id == nullptr || *setting_id == '\0' || value_ids == nullptr ||
        display_names == nullptr || count == 0 || default_value == nullptr ) return 0;
    smoke_setting_meta meta;
    meta.kind = smoke_setting_kind::enumeration;
    meta.scope = scope;
    meta.default_string = default_value;
    bool found_default = false;
    for( size_t i = 0; i < count; ++i ) {
        if( value_ids[i] == nullptr || display_names[i] == nullptr ) return 0;
        meta.choices.emplace_back( value_ids[i] );
        if( meta.choices.back() == default_value ) found_default = true;
    }
    if( !found_default ) return 0;
    setting_meta[setting_id] = meta;
    setting_string_values[setting_id] = default_value;
    registered_setting_ids.insert( setting_id );
    return 1;
}

int world_setting_get_bool_fn( const char *setting_id, int fallback )
{
    if( setting_id == nullptr ) return fallback;
    const auto it = setting_i64_values.find( setting_id );
    return it == setting_i64_values.end() ? fallback : ( it->second != 0 ? 1 : 0 );
}

int64_t world_setting_get_i64_fn( const char *setting_id, int64_t fallback )
{
    if( setting_id == nullptr ) return fallback;
    const auto it = setting_i64_values.find( setting_id );
    return it == setting_i64_values.end() ? fallback : it->second;
}

double world_setting_get_f64_fn( const char *setting_id, double fallback )
{
    if( setting_id == nullptr ) return fallback;
    const auto it = setting_f64_values.find( setting_id );
    return it == setting_f64_values.end() ? fallback : it->second;
}

const char *world_setting_get_string_fn( const char *setting_id, const char *fallback )
{
    if( setting_id == nullptr ) return fallback;
    const auto it = setting_string_values.find( setting_id );
    return it == setting_string_values.end() ? fallback : it->second.c_str();
}

int ui_tile_choose_fn( const char *, const char *const *, const char *const *, size_t, size_t )
{
    return -1;
}

int ui_card_choose_fn( const char *, const char *, const ncmm_ui_progress_v1 *,
                       const ncmm_ui_card_v1 *, size_t, size_t )
{
    return -1;
}

int ui_tree_choose_fn( const char *, const char *, const ncmm_ui_progress_v1 *,
                       const ncmm_ui_tree_node_v1 *, size_t,
                       const ncmm_ui_tree_edge_v1 *, size_t )
{
    return NCMM_UI_TREE_CANCEL;
}

int ui_card_choose_themed_fn( const char *, const char *, const ncmm_ui_progress_v1 *,
                              const ncmm_ui_card_v1 *, size_t, size_t,
                              const ncmm_ui_theme_v1 * )
{
    return -1;
}

int ui_tree_choose_themed_fn( const char *, const char *, const ncmm_ui_progress_v1 *,
                              const ncmm_ui_tree_node_v1 *, size_t,
                              const ncmm_ui_tree_edge_v1 *, size_t,
                              const ncmm_ui_theme_v1 * )
{
    return NCMM_UI_TREE_CANCEL;
}

int ui_card_choose_rpg_fn( const char *, const char *, const ncmm_ui_progress_v1 *,
                           const ncmm_ui_card_v1 *, size_t, size_t,
                           const ncmm_ui_theme_ex_v1 * )
{
    return -1;
}

int ui_tree_choose_rpg_fn( const char *, const char *, const ncmm_ui_progress_v1 *,
                           const ncmm_ui_tree_node_v1 *, size_t,
                           const ncmm_ui_tree_edge_v1 *, size_t,
                           const ncmm_ui_theme_ex_v1 * )
{
    return NCMM_UI_TREE_CANCEL;
}

int64_t gameplay_metric_get_i64_fn( const char * ) { return 0; }

int world_mod_active_fn( const char *mod_id )
{
    return mod_id != nullptr && active_world_mods.count( mod_id ) != 0 ? 1 : 0;
}

size_t world_mod_count_fn()
{
    return active_world_mods.size();
}

const char *world_mod_id_fn( size_t index )
{
    if( index >= active_world_mods.size() ) return nullptr;
    auto it = active_world_mods.begin();
    std::advance( it, static_cast<long>( index ) );
    return it->c_str();
}

int event_available_v2_fn( uint32_t event_id )
{
    return event_id == NCMM_EVENT_TURN_V2 ||
           event_id == NCMM_EVENT_WORLD_LOADED_V2 ||
           event_id == NCMM_EVENT_WORLD_UNLOADED_V2 ||
           event_id == NCMM_EVENT_LOCALE_CHANGED_V2 ||
           event_id == NCMM_EVENT_PLAYER_KILL_V2;
}

int event_subscribe_v2_fn( const char *, uint32_t event_id, ncmm_event_callback_v2, void * )
{
    if( !event_available_v2_fn( event_id ) ) {
        return 0;
    }
    ++event_subscription_count;
    return 1;
}

int event_unsubscribe_all_v2_fn( const char * ) { return 1; }

int modifier_define_v2_fn( const char *, const char *modifier_id, double, double )
{
    if( modifier_id == nullptr || *modifier_id == '\0' ) return 0;
    defined_v2_modifiers.insert( modifier_id );
    ++modifier_definition_count;
    return 1;
}

int modifier_set_v2_fn( const char *module_id, const char *modifier_id, double value )
{
    return modifier_set_fn( module_id, modifier_id, value );
}

int modifier_clear_v2_fn( const char *module_id )
{
    return modifier_clear_fn( module_id );
}

double modifier_get_total_v2_fn( const char * ) { return 0.0; }

int runtime_hook_bind_modifier_v2_fn( const char *, const char *hook_id, uint32_t,
                                      const char *, const char *modifier_id )
{
    if( hook_id == nullptr || *hook_id == '\0' || modifier_id == nullptr || *modifier_id == '\0' ) {
        return 0;
    }
    runtime_hooks_by_modifier.emplace( modifier_id, hook_id );
    ++runtime_hook_binding_count;
    return 1;
}

double runtime_hook_value_v2_fn( const char *, const char *, const char *, const char *, const char * )
{
    return 0.0;
}

int worldgen_hook_bind_setting_v2_fn( const char *, const char *hook_id,
                                          const char *setting_id, uint32_t type )
{
    if( hook_id == nullptr || *hook_id == '\0' || setting_id == nullptr || *setting_id == '\0' ) {
        return 0;
    }
    smoke_worldgen_binding binding;
    binding.setting_id = setting_id;
    binding.type = type;
    worldgen_bindings[hook_id] = binding;
    ++worldgen_binding_count;
    return 1;
}

int worldgen_hook_bool_v2_fn( const char *hook_id, int fallback )
{
    if( hook_id == nullptr ) return fallback;
    const auto binding = worldgen_bindings.find( hook_id );
    if( binding == worldgen_bindings.end() || binding->second.type != NCMM_WORLDGEN_BOOL_V2 ) {
        return fallback;
    }
    return world_setting_get_bool_fn( binding->second.setting_id.c_str(), fallback );
}

int64_t worldgen_hook_i64_v2_fn( const char *hook_id, int64_t fallback )
{
    if( hook_id == nullptr ) return fallback;
    const auto binding = worldgen_bindings.find( hook_id );
    if( binding == worldgen_bindings.end() || binding->second.type != NCMM_WORLDGEN_INT_V2 ) {
        return fallback;
    }
    return world_setting_get_i64_fn( binding->second.setting_id.c_str(), fallback );
}

double worldgen_hook_f64_v2_fn( const char *hook_id, double fallback )
{
    if( hook_id == nullptr ) return fallback;
    const auto binding = worldgen_bindings.find( hook_id );
    if( binding == worldgen_bindings.end() || binding->second.type != NCMM_WORLDGEN_FLOAT_V2 ) {
        return fallback;
    }
    return world_setting_get_f64_fn( binding->second.setting_id.c_str(), fallback );
}

int runtime_hook_bind_setting_v2_fn( const char *, const char *hook_id,
                                     const char *setting_id, uint32_t type )
{
    if( hook_id == nullptr || *hook_id == '\0' || setting_id == nullptr || *setting_id == '\0' ) {
        return 0;
    }
    smoke_worldgen_binding binding;
    binding.setting_id = setting_id;
    binding.type = type;
    runtime_setting_bindings[hook_id] = binding;
    ++runtime_setting_binding_count;
    return 1;
}

int runtime_hook_bool_setting_v2_fn( const char *hook_id, int fallback )
{
    if( hook_id == nullptr ) return fallback;
    const auto binding = runtime_setting_bindings.find( hook_id );
    if( binding == runtime_setting_bindings.end() || binding->second.type != NCMM_SETTING_BOOL_V2 ) {
        return fallback;
    }
    return world_setting_get_bool_fn( binding->second.setting_id.c_str(), fallback );
}

int64_t runtime_hook_i64_setting_v2_fn( const char *hook_id, int64_t fallback )
{
    if( hook_id == nullptr ) return fallback;
    const auto binding = runtime_setting_bindings.find( hook_id );
    if( binding == runtime_setting_bindings.end() || binding->second.type != NCMM_SETTING_INT_V2 ) {
        return fallback;
    }
    return world_setting_get_i64_fn( binding->second.setting_id.c_str(), fallback );
}

double runtime_hook_f64_setting_v2_fn( const char *hook_id, double fallback )
{
    if( hook_id == nullptr ) return fallback;
    const auto binding = runtime_setting_bindings.find( hook_id );
    if( binding == runtime_setting_bindings.end() || binding->second.type != NCMM_SETTING_FLOAT_V2 ) {
        return fallback;
    }
    return world_setting_get_f64_fn( binding->second.setting_id.c_str(), fallback );
}

const char *current_module_id_v2_fn() { return "smoke_host"; }
int module_is_loaded_v2_fn( const char * ) { return 0; }
const char *module_version_v2_fn( const char * ) { return nullptr; }
const char *module_state_v2_fn( const char * ) { return nullptr; }

ncmm_host_api_v2_core smoke_host2{};

const void *query_interface_fn( const char *interface_id, uint32_t min_major, uint32_t min_minor )
{
    if( interface_id == nullptr || std::strcmp( interface_id, NCMM_HOST_API_V2_CORE_ID ) != 0 ||
        min_major > NCMM_HOST_API_V2_CORE_MAJOR ||
        ( min_major == NCMM_HOST_API_V2_CORE_MAJOR && min_minor > NCMM_HOST_API_V2_CORE_MINOR ) ) {
        return nullptr;
    }
    return &smoke_host2;
}

int ui_choose_fn( const char *title, const char *const *entries, size_t count )
{
    if( title == nullptr || entries == nullptr || count == 0 ) {
        return -1;
    }
    const std::string t( title );

    if( ui_script == 1 ) {
        // Buy Combat -> Power Training.
        if( ui_stage == 0 && t.find( "Survivor Progression v0.14.0" ) != std::string::npos ) {
            ++ui_stage;
            return 0;
        }
        if( ui_stage == 1 && t.find( "Combat" ) != std::string::npos ) {
            ++ui_stage;
            return 0;
        }
        if( ui_stage == 2 && t.find( "Power Training" ) != std::string::npos ) {
            ++ui_stage;
            return 0;
        }
        return -1;
    }

    if( ui_script == 2 ) {
        // Buy Mastery -> Fast Learner.
        if( ui_stage == 0 && t.find( "Survivor Progression v0.14.0" ) != std::string::npos ) {
            ++ui_stage;
            return 5;
        }
        if( ui_stage == 1 && t.find( "Mastery" ) != std::string::npos ) {
            ++ui_stage;
            return 0;
        }
        if( ui_stage == 2 && t.find( "Fast Learner" ) != std::string::npos ) {
            ++ui_stage;
            return 0;
        }
        return -1;
    }

    if( ui_script == 3 ) {
        // Root -> Respec all perks -> confirm.
        if( ui_stage == 0 && t.find( "Survivor Progression v0.14.0" ) != std::string::npos ) {
            ++ui_stage;
            return 7;
        }
        if( ui_stage == 1 && t.find( "Refund: 2P / 0M" ) != std::string::npos ) {
            ++ui_stage;
            return 0;
        }
        return -1;
    }

    return -1;
}

void ui_message_fn( const char * )
{
    ++ui_message_count;
}

template<typename T>
T symbol( void *lib, const char *name )
{
#ifdef _WIN32
    return reinterpret_cast<T>( GetProcAddress( static_cast<HMODULE>( lib ), name ) );
#else
    return reinterpret_cast<T>( dlsym( lib, name ) );
#endif
}


// EBM-specific fault injection delegates successful calls to the shared host mocks.
namespace equipment_body_map_smoke
{
size_t setting_calls = 0;
size_t binding_calls = 0;
size_t fail_setting_call = 0;
size_t fail_binding_call = 0;
const char *locale = "en";
std::map<std::string, std::string> labels;

int register_bool( const char *module, const char *id, const char *label,
                   const char *tooltip, int value, uint32_t scope )
{
    ++setting_calls;
    if( setting_calls == fail_setting_call ) return 0;
    if( !module || std::strcmp( module, "equipment_body_map" ) != 0 ||
        !id || !label || !tooltip || !*tooltip ) return 0;
    labels[id] = label;
    return world_setting_register_bool_fn( module, id, label, tooltip, value, scope );
}

int bind_setting( const char *module, const char *hook, const char *id, uint32_t type )
{
    ++binding_calls;
    if( binding_calls == fail_binding_call ) return 0;
    if( !module || std::strcmp( module, "equipment_body_map" ) != 0 ) return 0;
    return runtime_hook_bind_setting_v2_fn( module, hook, id, type );
}

const char *get_locale() { return locale; }
uint32_t old_api_minor() { return 8u; }
int missing_runtime_capability( const char *cap )
{
    return cap && std::strcmp( cap, "runtime_settings.bindings.v2" ) != 0 &&
           has_capability_fn( cap );
}

bool check( bool condition, const char *scenario, const char *detail )
{
    if( !condition ) {
        std::cerr << "Equipment Body Map [" << scenario << "]: " << detail << '\n';
    }
    return condition;
}

bool state_matches( size_t settings, size_t bindings )
{
    const std::array<const char *, 3> ids = {
        "NCMM_EBM_ENABLED", "NCMM_EBM_SHOW_LAYERS", "NCMM_EBM_RIGHT_ARROW_GEAR"
    };
    const std::array<const char *, 3> hooks = {
        "inventory.body_map.enabled", "inventory.body_map.show_layers",
        "inventory.body_map.right_arrow_gear"
    };
    if( registered_setting_ids.size() != settings || setting_meta.size() != settings ||
        setting_i64_values.size() != settings || labels.size() != settings ||
        runtime_setting_bindings.size() != bindings || runtime_setting_binding_count != bindings ) {
        return false;
    }
    for( size_t i = 0; i < settings; ++i ) {
        if( registered_setting_ids.count( ids[i] ) != 1 || setting_meta.count( ids[i] ) != 1 ||
            setting_i64_values.count( ids[i] ) != 1 || labels.count( ids[i] ) != 1 ) return false;
        const auto &meta = setting_meta.at( ids[i] );
        if( meta.kind != smoke_setting_kind::boolean || meta.default_value != 1.0 ||
            meta.scope != NCMM_WORLD_SETTING_LIVE || setting_i64_values.at( ids[i] ) != 1 ) return false;
    }
    for( size_t i = 0; i < bindings; ++i ) {
        const auto binding = runtime_setting_bindings.find( hooks[i] );
        if( binding == runtime_setting_bindings.end() || binding->second.setting_id != ids[i] ||
            binding->second.type != NCMM_SETTING_BOOL_V2 ||
            runtime_hook_bool_setting_v2_fn( hooks[i], 0 ) != 1 ) return false;
    }
    return true;
}

bool run( void *lib, const ncmm_mod_descriptor_v1 *desc, ncmm_host_api_v1 api )
{
    const auto on_locale = symbol<ncmm_on_locale_changed_v1_fn>( lib, NCMM_LOCALE_ENTRYPOINT );
    if( !check( desc->id && std::strcmp( desc->id, "equipment_body_map" ) == 0 &&
                desc->version && std::strcmp( desc->version, "0.1.0" ) == 0 &&
                desc->init && desc->shutdown && on_locale, "descriptor",
                "expected id/version, init/shutdown and locale export" ) ) return false;

    const auto original_core = smoke_host2;
    // Restore shared API state even if an assertion fails.
    struct cleanup {
        const ncmm_mod_descriptor_v1 *desc;
        ncmm_host_api_v2_core core;
        ~cleanup() { desc->shutdown(); smoke_host2 = core; }
    } restore{ desc, original_core };
    const auto original_api = api;
    const auto reset = [&]() {
        desc->shutdown();
        smoke_host2 = original_core;
        api = original_api;
        api.get_locale = &get_locale;
        smoke_host2.legacy_v1 = &api;
        smoke_host2.world_setting_register_bool = &register_bool;
        smoke_host2.runtime_hook_bind_setting = &bind_setting;
        setting_calls = binding_calls = fail_setting_call = fail_binding_call = 0;
        locale = "en";
        labels.clear();
        registered_setting_ids.clear();
        setting_meta.clear();
        setting_i64_values.clear();
        runtime_setting_bindings.clear();
        runtime_setting_binding_count = 0;
    };
    enum class fault { capability, short_tail, null_bind, null_bool, null_register,
                       first_setting, second_setting, third_setting,
                       first_binding, second_binding, third_binding, old_api };
    struct test_case {
        const char *name;
        fault injection;
        size_t settings;
        size_t bindings;
        size_t setting_attempts;
        size_t binding_attempts;
    };
    // Failed calls do not mutate the mock host. Successful earlier registrations
    // remain: this API exposes no setting/binding rollback operation to the module.
    const test_case cases[] = {
        { "missing runtime capability", fault::capability, 0, 0, 0, 0 },
        { "short v2 tail", fault::short_tail, 0, 0, 0, 0 },
        { "null runtime bind", fault::null_bind, 0, 0, 0, 0 },
        { "null runtime bool", fault::null_bool, 0, 0, 0, 0 },
        { "null setting register", fault::null_register, 0, 0, 0, 0 },
        { "first setting failure", fault::first_setting, 0, 0, 1, 0 },
        { "second setting failure", fault::second_setting, 1, 0, 2, 0 },
        { "third setting failure", fault::third_setting, 2, 0, 3, 0 },
        { "first binding failure", fault::first_binding, 3, 0, 3, 1 },
        { "second binding failure", fault::second_binding, 3, 1, 3, 2 },
        { "third binding failure", fault::third_binding, 3, 2, 3, 3 },
        { "API 1.8", fault::old_api, 0, 0, 0, 0 }
    };
    for( const auto &test : cases ) {
        // The existing --missing-contract invocation also exercises module init,
        // rather than returning at the harness capability preflight.
        if( simulate_missing_contract && test.injection != fault::capability ) continue;
        reset();
        switch( test.injection ) {
            case fault::capability: api.has_capability = &missing_runtime_capability; break;
            case fault::short_tail:
                smoke_host2.struct_size = offsetof( ncmm_host_api_v2_core, runtime_hook_f64 ) +
                                          sizeof( smoke_host2.runtime_hook_f64 ) - 1;
                break;
            case fault::null_bind: smoke_host2.runtime_hook_bind_setting = nullptr; break;
            case fault::null_bool: smoke_host2.runtime_hook_bool = nullptr; break;
            case fault::null_register: smoke_host2.world_setting_register_bool = nullptr; break;
            case fault::first_setting: fail_setting_call = 1; break;
            case fault::second_setting: fail_setting_call = 2; break;
            case fault::third_setting: fail_setting_call = 3; break;
            case fault::first_binding: fail_binding_call = 1; break;
            case fault::second_binding: fail_binding_call = 2; break;
            case fault::third_binding: fail_binding_call = 3; break;
            case fault::old_api: api.get_api_version_minor = &old_api_minor; break;
        }
        if( !check( desc->init( &api ) == 0, test.name, "init must reject host" ) ||
            !check( setting_calls == test.setting_attempts && binding_calls == test.binding_attempts,
                    test.name, "unexpected registration attempts / failure was not short-circuited" ) ||
            !check( state_matches( test.settings, test.bindings ), test.name,
                    "unexpected partial host state after init failure" ) ) return false;
        locale = "ru";
        on_locale( &api );
        desc->shutdown();
        on_locale( &api );
        if( !check( setting_calls == test.setting_attempts && binding_calls == test.binding_attempts &&
                    state_matches( test.settings, test.bindings ), test.name,
                    "locale/shutdown changed registrations after rejected init" ) ) return false;
        std::cout << "Equipment Body Map [" << test.name << "]: PASS\n";
    }
    if( simulate_missing_contract ) return true;

    reset();
    if( !check( desc->init( &api ) == 1, "happy path", "init failed" ) ||
        !check( setting_calls == 3 && binding_calls == 3 && state_matches( 3, 3 ),
                "happy path", "expected exactly three true LIVE bool settings and matching bindings" ) ||
        !check( labels.at( "NCMM_EBM_ENABLED" ) == "Show equipment body map" &&
                labels.at( "NCMM_EBM_SHOW_LAYERS" ) == "Show selected item layers" &&
                labels.at( "NCMM_EBM_RIGHT_ARROW_GEAR" ) == "Right arrow: jump to equipped gear",
                "EN metadata", "English labels missing" ) ) return false;
    locale = "ru";
    on_locale( &api );
    if( !check( setting_calls == 6 && binding_calls == 3 && state_matches( 3, 3 ),
                "EN -> RU", "metadata must be re-registered without rebinding" ) ||
        !check( labels.at( "NCMM_EBM_ENABLED" ) == "Показывать схему экипировки" &&
                labels.at( "NCMM_EBM_SHOW_LAYERS" ) == "Показывать слои выбранной вещи" &&
                labels.at( "NCMM_EBM_RIGHT_ARROW_GEAR" ) == "Стрелка вправо: переход к экипировке",
                "EN -> RU", "Russian labels were not passed to host" ) ) return false;
    desc->shutdown();
    locale = "en";
    on_locale( &api );
    if( !check( setting_calls == 6 && binding_calls == 3 && state_matches( 3, 3 ) &&
                labels.at( "NCMM_EBM_ENABLED" ) == "Показывать схему экипировки" &&
                labels.at( "NCMM_EBM_SHOW_LAYERS" ) == "Показывать слои выбранной вещи" &&
                labels.at( "NCMM_EBM_RIGHT_ARROW_GEAR" ) == "Стрелка вправо: переход к экипировке",
                "shutdown", "locale callback must be inert after shutdown" ) ) return false;
    std::cout << "Equipment Body Map [happy path, EN -> RU, shutdown]: PASS\n";
    return true;
}
} // namespace equipment_body_map_smoke

bool nearly_equal( double left, double right, double epsilon = 1.0e-8 )
{
    const double scale = std::max( 1.0, std::max( std::fabs( left ), std::fabs( right ) ) );
    return std::fabs( left - right ) <= epsilon * scale;
}

uint32_t semantic_rng_next( uint32_t &state )
{
    state = state * 1664525u + 1013904223u;
    return state;
}

bool aws_semantic_matrix()
{
    if( setting_meta.size() != 50 || worldgen_bindings.size() != 50 ||
        worldgen_binding_count != 50 ) {
        std::cerr << "AWS semantic coverage mismatch: settings=" << setting_meta.size()
                  << " bindings=" << worldgen_bindings.size()
                  << " calls=" << worldgen_binding_count << '\n';
        return false;
    }

    for( const auto &entry : setting_meta ) {
        const smoke_setting_meta &meta = entry.second;
        if( meta.scope != NCMM_WORLD_SETTING_NEW_MAP ) {
            std::cerr << "AWS setting has wrong scope: " << entry.first << '\n';
            return false;
        }
        if( meta.kind == smoke_setting_kind::integer ||
            meta.kind == smoke_setting_kind::floating ) {
            if( meta.min_value > meta.max_value ||
                meta.default_value < meta.min_value ||
                meta.default_value > meta.max_value ) {
                std::cerr << "AWS setting range/default invalid: " << entry.first << '\n';
                return false;
            }
        }
    }

    const auto custom_it = setting_meta.find( "NCMM_AWS_CUSTOM_GEOGRAPHY" );
    const auto railroad_it = setting_meta.find( "NCMM_AWS_PLACE_RAILROADS" );
    const auto ravine_depth_it = setting_meta.find( "NCMM_AWS_RAVINE_DEPTH" );
    if( custom_it == setting_meta.end() || custom_it->second.kind != smoke_setting_kind::boolean ||
        custom_it->second.default_value != 0.0 ) {
        std::cerr << "AWS custom geography must remain opt-in by default\n";
        return false;
    }
    if( railroad_it == setting_meta.end() || railroad_it->second.kind != smoke_setting_kind::boolean ||
        railroad_it->second.default_value != 0.0 ) {
        std::cerr << "AWS railroad default must preserve vanilla default-region OFF semantics\n";
        return false;
    }
    if( ravine_depth_it == setting_meta.end() || ravine_depth_it->second.kind != smoke_setting_kind::integer ||
        ravine_depth_it->second.min_value != -10.0 || ravine_depth_it->second.max_value != -1.0 ||
        ravine_depth_it->second.default_value != -3.0 ) {
        std::cerr << "AWS ravine depth range must stay within supported overmap Z bounds\n";
        return false;
    }

    const char *scope_ids[] = {
        "NCMM_AWS_SCOPE_CITIES",
        "NCMM_AWS_SCOPE_ECOLOGY",
        "NCMM_AWS_SCOPE_WATER",
        "NCMM_AWS_SCOPE_TRANSPORT"
    };
    for( const char *scope_id : scope_ids ) {
        const auto scope = setting_meta.find( scope_id );
        if( scope == setting_meta.end() ||
            scope->second.kind != smoke_setting_kind::boolean ||
            scope->second.default_value != 1.0 ) {
            std::cerr << "AWS selective scope missing or not backward-compatible: "
                      << scope_id << '\n';
            return false;
        }
    }

    if( setting_meta.count( "NCMM_AWS_PLACE_SPECIALS" ) != 0 ||
        setting_meta.count( "NCMM_AWS_NEIGHBOR_CONNECTIONS" ) != 0 ||
        worldgen_bindings.count( "geography.specials.enabled" ) != 0 ||
        worldgen_bindings.count( "geography.neighbor_connections.enabled" ) != 0 ) {
        std::cerr << "AWS unsafe worldgen suppression controls must not be exposed\n";
        return false;
    }

    const std::map<std::string, std::string> expected_binding_ids = {
        { "geography.custom.enabled", "NCMM_AWS_CUSTOM_GEOGRAPHY" },
        { "geography.scope.cities.enabled", "NCMM_AWS_SCOPE_CITIES" },
        { "geography.scope.ecology.enabled", "NCMM_AWS_SCOPE_ECOLOGY" },
        { "geography.scope.water.enabled", "NCMM_AWS_SCOPE_WATER" },
        { "geography.scope.transport.enabled", "NCMM_AWS_SCOPE_TRANSPORT" },
        { "geography.city.size", "NCMM_AWS_CITY_SIZE" },
        { "geography.city.spacing", "NCMM_AWS_CITY_SPACING" },
        { "geography.city.max_urbanity", "NCMM_AWS_MAX_URBANITY" },
        { "geography.city.megacity", "NCMM_AWS_MEGACITY" },
        { "geography.city.shop_radius", "NCMM_AWS_SHOP_RADIUS" },
        { "geography.city.shop_sigma", "NCMM_AWS_SHOP_SIGMA" },
        { "geography.city.park_radius", "NCMM_AWS_PARK_RADIUS" },
        { "geography.city.park_sigma", "NCMM_AWS_PARK_SIGMA" },
        { "geography.roads.enabled", "NCMM_AWS_PLACE_ROADS" },
        { "geography.railroads.enabled", "NCMM_AWS_PLACE_RAILROADS" },
        { "geography.forests.enabled", "NCMM_AWS_ENABLE_FORESTS" },
        { "geography.forests.threshold", "NCMM_AWS_FOREST_THRESHOLD" },
        { "geography.forests.thick_threshold", "NCMM_AWS_FOREST_THICK_THRESHOLD" },
        { "geography.swamps.enabled", "NCMM_AWS_ENABLE_SWAMPS" },
        { "geography.swamps.adjacent_threshold", "NCMM_AWS_SWAMP_ADJ_THRESHOLD" },
        { "geography.swamps.isolated_threshold", "NCMM_AWS_SWAMP_ISOLATED_THRESHOLD" },
        { "geography.swamps.floodplain_min", "NCMM_AWS_FLOODPLAIN_MIN" },
        { "geography.swamps.floodplain_max", "NCMM_AWS_FLOODPLAIN_MAX" },
        { "geography.trails.enabled", "NCMM_AWS_ENABLE_TRAILS" },
        { "geography.trails.chance", "NCMM_AWS_TRAIL_CHANCE" },
        { "geography.trails.min_forest", "NCMM_AWS_TRAIL_MIN_FOREST" },
        { "geography.trails.trailhead_chance", "NCMM_AWS_TRAILHEAD_CHANCE" },
        { "geography.trails.road_distance", "NCMM_AWS_TRAILHEAD_ROAD_DISTANCE" },
        { "geography.rivers.enabled", "NCMM_AWS_ENABLE_RIVERS" },
        { "geography.rivers.scale", "NCMM_AWS_RIVER_SCALE" },
        { "geography.rivers.frequency", "NCMM_AWS_RIVER_FREQUENCY" },
        { "geography.rivers.branch_chance", "NCMM_AWS_RIVER_BRANCH_CHANCE" },
        { "geography.rivers.remerge_chance", "NCMM_AWS_RIVER_REMERGE_CHANCE" },
        { "geography.rivers.branch_scale_decrease", "NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE" },
        { "geography.lakes.enabled", "NCMM_AWS_ENABLE_LAKES" },
        { "geography.lakes.threshold", "NCMM_AWS_LAKE_THRESHOLD" },
        { "geography.lakes.min_size", "NCMM_AWS_LAKE_MIN_SIZE" },
        { "geography.oceans.enabled", "NCMM_AWS_ENABLE_OCEANS" },
        { "geography.oceans.threshold", "NCMM_AWS_OCEAN_THRESHOLD" },
        { "geography.oceans.min_size", "NCMM_AWS_OCEAN_MIN_SIZE" },
        { "geography.highways.enabled", "NCMM_AWS_ENABLE_HIGHWAYS" },
        { "geography.highways.grid_row", "NCMM_AWS_HIGHWAY_GRID_ROW" },
        { "geography.highways.grid_column", "NCMM_AWS_HIGHWAY_GRID_COLUMN" },
        { "geography.highways.grid_variance", "NCMM_AWS_HIGHWAY_GRID_VARIANCE" },
        { "geography.highways.straightness", "NCMM_AWS_HIGHWAY_STRAIGHTNESS" },
        { "geography.ravines.enabled", "NCMM_AWS_ENABLE_RAVINES" },
        { "geography.ravines.count", "NCMM_AWS_RAVINE_COUNT" },
        { "geography.ravines.range", "NCMM_AWS_RAVINE_RANGE" },
        { "geography.ravines.width", "NCMM_AWS_RAVINE_WIDTH" },
        { "geography.ravines.depth", "NCMM_AWS_RAVINE_DEPTH" },
    };
    if( expected_binding_ids.size() != worldgen_bindings.size() ) {
        std::cerr << "AWS exact binding identity count mismatch\n";
        return false;
    }
    for( const auto &expected : expected_binding_ids ) {
        const auto actual = worldgen_bindings.find( expected.first );
        if( actual == worldgen_bindings.end() || actual->second.setting_id != expected.second ) {
            std::cerr << "AWS binding identity mismatch: " << expected.first << " -> "
                      << expected.second << '\n';
            return false;
        }
    }

    for( const auto &entry : worldgen_bindings ) {
        const auto setting = setting_meta.find( entry.second.setting_id );
        if( setting == setting_meta.end() ) {
            std::cerr << "AWS worldgen binding references unregistered setting: "
                      << entry.first << " -> " << entry.second.setting_id << '\n';
            return false;
        }
        const smoke_setting_kind kind = setting->second.kind;
        const bool type_ok =
            ( kind == smoke_setting_kind::boolean && entry.second.type == NCMM_WORLDGEN_BOOL_V2 ) ||
            ( kind == smoke_setting_kind::integer && entry.second.type == NCMM_WORLDGEN_INT_V2 ) ||
            ( kind == smoke_setting_kind::floating && entry.second.type == NCMM_WORLDGEN_FLOAT_V2 );
        if( !type_ok ) {
            std::cerr << "AWS worldgen binding type mismatch: " << entry.first << '\n';
            return false;
        }
    }

    // Defaults must already flow through the bound Host hook surface.
    for( const auto &entry : worldgen_bindings ) {
        const smoke_setting_meta &meta = setting_meta.at( entry.second.setting_id );
        if( entry.second.type == NCMM_WORLDGEN_BOOL_V2 ) {
            const int actual = worldgen_hook_bool_v2_fn( entry.first.c_str(), -9 );
            if( actual != static_cast<int>( meta.default_value != 0.0 ) ) {
                std::cerr << "AWS default bool binding mismatch: " << entry.first << '\n';
                return false;
            }
        } else if( entry.second.type == NCMM_WORLDGEN_INT_V2 ) {
            const int64_t actual = worldgen_hook_i64_v2_fn( entry.first.c_str(), -999999 );
            if( actual != static_cast<int64_t>( meta.default_value ) ) {
                std::cerr << "AWS default int binding mismatch: " << entry.first << '\n';
                return false;
            }
        } else if( entry.second.type == NCMM_WORLDGEN_FLOAT_V2 ) {
            const double actual = worldgen_hook_f64_v2_fn( entry.first.c_str(), -999999.0 );
            if( !nearly_equal( actual, meta.default_value ) ) {
                std::cerr << "AWS default float binding mismatch: " << entry.first << '\n';
                return false;
            }
        }
    }

    const auto verify_bound_values = [&]( const char *label ) {
        for( const auto &entry : worldgen_bindings ) {
            const std::string &hook = entry.first;
            const std::string &setting_id = entry.second.setting_id;
            if( entry.second.type == NCMM_WORLDGEN_BOOL_V2 ) {
                const int expected = setting_i64_values.at( setting_id ) != 0 ? 1 : 0;
                if( worldgen_hook_bool_v2_fn( hook.c_str(), -9 ) != expected ) {
                    std::cerr << "AWS " << label << " bool mismatch hook=" << hook << '\n';
                    return false;
                }
            } else if( entry.second.type == NCMM_WORLDGEN_INT_V2 ) {
                const int64_t expected = setting_i64_values.at( setting_id );
                if( worldgen_hook_i64_v2_fn( hook.c_str(), -999999 ) != expected ) {
                    std::cerr << "AWS " << label << " int mismatch hook=" << hook << '\n';
                    return false;
                }
            } else if( entry.second.type == NCMM_WORLDGEN_FLOAT_V2 ) {
                const double expected = setting_f64_values.at( setting_id );
                if( !nearly_equal( worldgen_hook_f64_v2_fn( hook.c_str(), -999999.0 ), expected ) ) {
                    std::cerr << "AWS " << label << " float mismatch hook=" << hook << '\n';
                    return false;
                }
            }
        }
        return true;
    };

    // Exercise both ends of every registered numeric range.  Random cases alone
    // are unlikely to hit exact boundaries such as ravine depth -20 or lake size 1000.
    for( const auto &entry : setting_meta ) {
        const std::string &id = entry.first;
        const smoke_setting_meta &meta = entry.second;
        if( meta.kind == smoke_setting_kind::boolean ) {
            setting_i64_values[id] = 0;
        } else if( meta.kind == smoke_setting_kind::integer ) {
            setting_i64_values[id] = static_cast<int64_t>( meta.min_value );
        } else if( meta.kind == smoke_setting_kind::floating ) {
            setting_f64_values[id] = meta.min_value;
        } else if( meta.kind == smoke_setting_kind::enumeration && !meta.choices.empty() ) {
            setting_string_values[id] = meta.choices.front();
        }
    }
    if( !verify_bound_values( "minimum-boundary" ) ) return false;

    for( const auto &entry : setting_meta ) {
        const std::string &id = entry.first;
        const smoke_setting_meta &meta = entry.second;
        if( meta.kind == smoke_setting_kind::boolean ) {
            setting_i64_values[id] = 1;
        } else if( meta.kind == smoke_setting_kind::integer ) {
            setting_i64_values[id] = static_cast<int64_t>( meta.max_value );
        } else if( meta.kind == smoke_setting_kind::floating ) {
            setting_f64_values[id] = meta.max_value;
        } else if( meta.kind == smoke_setting_kind::enumeration && !meta.choices.empty() ) {
            setting_string_values[id] = meta.choices.back();
        }
    }
    if( !verify_bound_values( "maximum-boundary" ) ) return false;

    // Deterministic property-style coverage.  The seed is stable so failures are
    // reproducible, while each case exercises a different valid world-setting set.
    uint32_t rng = 0xA7C0FFEEu;
    constexpr int random_cases = 32;
    for( int case_index = 0; case_index < random_cases; ++case_index ) {
        for( const auto &entry : setting_meta ) {
            const std::string &id = entry.first;
            const smoke_setting_meta &meta = entry.second;
            if( meta.kind == smoke_setting_kind::boolean ) {
                setting_i64_values[id] = static_cast<int64_t>( semantic_rng_next( rng ) & 1u );
            } else if( meta.kind == smoke_setting_kind::integer ) {
                const int64_t lo = static_cast<int64_t>( meta.min_value );
                const int64_t hi = static_cast<int64_t>( meta.max_value );
                const uint64_t span = static_cast<uint64_t>( hi - lo ) + 1u;
                setting_i64_values[id] = lo + static_cast<int64_t>(
                                             static_cast<uint64_t>( semantic_rng_next( rng ) ) % span );
            } else if( meta.kind == smoke_setting_kind::floating ) {
                const double steps_raw = ( meta.max_value - meta.min_value ) / meta.step;
                const uint32_t steps = static_cast<uint32_t>( std::floor( steps_raw + 1.0e-9 ) );
                const uint32_t pick = steps == 0 ? 0 : semantic_rng_next( rng ) % ( steps + 1u );
                setting_f64_values[id] = std::min( meta.max_value,
                                                  meta.min_value + meta.step * pick );
            } else if( meta.kind == smoke_setting_kind::enumeration && !meta.choices.empty() ) {
                const size_t pick = semantic_rng_next( rng ) % meta.choices.size();
                setting_string_values[id] = meta.choices[pick];
            }
        }

        // Keep related minimum/maximum controls semantically valid as a pair.
        auto flood_min = setting_i64_values.find( "NCMM_AWS_FLOODPLAIN_MIN" );
        auto flood_max = setting_i64_values.find( "NCMM_AWS_FLOODPLAIN_MAX" );
        if( flood_min != setting_i64_values.end() && flood_max != setting_i64_values.end() &&
            flood_min->second > flood_max->second ) {
            std::swap( flood_min->second, flood_max->second );
        }

        for( const auto &entry : worldgen_bindings ) {
            const std::string &hook = entry.first;
            const std::string &setting_id = entry.second.setting_id;
            if( entry.second.type == NCMM_WORLDGEN_BOOL_V2 ) {
                const int expected = setting_i64_values.at( setting_id ) != 0 ? 1 : 0;
                if( worldgen_hook_bool_v2_fn( hook.c_str(), -9 ) != expected ) {
                    std::cerr << "AWS randomized bool mismatch case=" << case_index
                              << " hook=" << hook << '\n';
                    return false;
                }
            } else if( entry.second.type == NCMM_WORLDGEN_INT_V2 ) {
                const int64_t expected = setting_i64_values.at( setting_id );
                if( worldgen_hook_i64_v2_fn( hook.c_str(), -999999 ) != expected ) {
                    std::cerr << "AWS randomized int mismatch case=" << case_index
                              << " hook=" << hook << '\n';
                    return false;
                }
            } else if( entry.second.type == NCMM_WORLDGEN_FLOAT_V2 ) {
                const double expected = setting_f64_values.at( setting_id );
                if( !nearly_equal( worldgen_hook_f64_v2_fn( hook.c_str(), -999999.0 ), expected ) ) {
                    std::cerr << "AWS randomized float mismatch case=" << case_index
                              << " hook=" << hook << '\n';
                    return false;
                }
            }
        }
    }

    std::cout << "AWS semantic matrix: PASS (50/50 exact typed geography bindings, selective scopes, protected worldgen invariants, min/max boundaries, 32 deterministic randomized cases)\n";
    return true;
}

using sp_test_count_fn = size_t (*)();
using sp_test_id_fn = const char *(*)( size_t );
using sp_test_int_index_fn = int (*)( size_t );
using sp_test_double_index_fn = double (*)( size_t );
using sp_test_rank_multiplier_fn = double (*)( size_t, int );
using sp_test_effect_id_fn = const char *(*)( size_t, int );
using sp_test_effect_value_fn = double (*)( size_t, int );
using sp_test_reset_fn = int (*)();
using sp_test_set_rank_fn = int (*)( size_t, int );
using sp_test_recalculate_fn = int (*)();
using sp_test_current_xp_fn = int (*)();
using sp_test_dispatch_event_fn = void (*)( uint32_t );

double survivor_modifier_value( const std::string &id )
{
    const auto it = modifiers.find( std::string( "survivor_progression:" ) + id );
    return it == modifiers.end() ? 0.0 : it->second;
}

bool survivor_modifiers_empty()
{
    const std::string prefix = "survivor_progression:";
    for( const auto &entry : modifiers ) {
        if( entry.first.rfind( prefix, 0 ) == 0 && !nearly_equal( entry.second, 0.0 ) ) {
            return false;
        }
    }
    return true;
}

bool survivor_semantic_matrix( void *lib )
{
    const auto count = symbol<sp_test_count_fn>( lib, "ncmm_test_perk_count_v1" );
    const auto perk_id = symbol<sp_test_id_fn>( lib, "ncmm_test_perk_id_v1" );
    const auto branch = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_branch_v1" );
    const auto currency = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_currency_v1" );
    const auto kind = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_kind_v1" );
    const auto scaling = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_scaling_v1" );
    const auto integration = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_integration_v1" );
    const auto max_rank = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_max_rank_v1" );
    const auto rank_multiplier = symbol<sp_test_rank_multiplier_fn>(
                                     lib, "ncmm_test_perk_rank_multiplier_v1" );
    const auto effect_count = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_effect_count_v1" );
    const auto effect_id = symbol<sp_test_effect_id_fn>( lib, "ncmm_test_perk_effect_id_v1" );
    const auto effect_value = symbol<sp_test_effect_value_fn>( lib, "ncmm_test_perk_effect_value_v1" );
    const auto xp_bonus = symbol<sp_test_int_index_fn>( lib, "ncmm_test_perk_xp_bonus_v1" );
    const auto branch_amp = symbol<sp_test_double_index_fn>( lib, "ncmm_test_perk_branch_amp_v1" );
    const auto global_amp = symbol<sp_test_double_index_fn>( lib, "ncmm_test_perk_global_amp_v1" );
    const auto reset = symbol<sp_test_reset_fn>( lib, "ncmm_test_reset_all_perks_v1" );
    const auto set_rank = symbol<sp_test_set_rank_fn>( lib, "ncmm_test_set_perk_rank_v1" );
    const auto recalculate = symbol<sp_test_recalculate_fn>( lib, "ncmm_test_recalculate_v1" );
    const auto current_xp = symbol<sp_test_current_xp_fn>( lib, "ncmm_test_current_xp_bonus_v1" );
    const auto dispatch_event = symbol<sp_test_dispatch_event_fn>(
                                    lib, "ncmm_test_dispatch_event_v1" );

    if( !count || !perk_id || !branch || !currency || !kind || !scaling || !integration ||
        !max_rank || !rank_multiplier || !effect_count || !effect_id || !effect_value ||
        !xp_bonus || !branch_amp || !global_amp || !reset || !set_rank || !recalculate ||
        !current_xp || !dispatch_event ) {
        std::cerr << "Survivor semantic diagnostic export missing\n";
        return false;
    }

    const size_t perk_count = count();
    if( perk_count != 372 ) {
        std::cerr << "Survivor perk catalog count changed unexpectedly: " << perk_count << '\n';
        return false;
    }

    active_world_mods = {
        "magiclysm", "mindovermatter", "xedra_evolved", "aftershock_exoplanet",
        "aftershock_prime", "secronom", "secronom_lore_expansion"
    };

    // Conditional integrations must be inert when their content mod is absent,
    // even if corrupt/legacy state claims the perk is owned.
    const std::set<std::string> all_supported_world_mods = active_world_mods;
    size_t integration_inert_cases = 0;
    active_world_mods.clear();
    for( size_t i = 0; i < perk_count; ++i ) {
        if( integration( i ) == 0 ) continue;
        ++integration_inert_cases;
        if( !reset() || !set_rank( i, 1 ) || !recalculate() ) {
            std::cerr << "Survivor integration inert-state setup failed: " << perk_id( i ) << '\n';
            return false;
        }
        if( !survivor_modifiers_empty() || current_xp() != 0 ) {
            std::cerr << "Survivor integration perk leaked without required world mod: "
                      << perk_id( i ) << '\n';
            return false;
        }
    }
    active_world_mods = all_supported_world_mods;
    if( !reset() ) return false;

    std::map<std::string, size_t> index;
    std::set<std::string> unique_ids;
    for( size_t i = 0; i < perk_count; ++i ) {
        const char *raw = perk_id( i );
        if( raw == nullptr || *raw == '\0' || !unique_ids.insert( raw ).second ) {
            std::cerr << "Survivor perk id is empty or duplicated at index " << i << '\n';
            return false;
        }
        index[raw] = i;
        if( branch( i ) < 0 || branch( i ) > 5 || currency( i ) < 0 || currency( i ) > 1 ||
            kind( i ) < 0 || kind( i ) > 1 || scaling( i ) < 0 || scaling( i ) > 2 ||
            max_rank( i ) < 1 || effect_count( i ) < 0 || effect_count( i ) > 4 ) {
            std::cerr << "Survivor perk metadata invalid: " << raw << '\n';
            return false;
        }
        for( int e = 0; e < effect_count( i ); ++e ) {
            const char *eid = effect_id( i, e );
            if( eid == nullptr || *eid == '\0' || nearly_equal( effect_value( i, e ), 0.0 ) ) {
                std::cerr << "Survivor perk has invalid declared effect: " << raw << '\n';
                return false;
            }
        }
    }

    const auto mana_vamp_it = index.find( "mg_mana_vampirism" );
    if( mana_vamp_it == index.end() ) {
        std::cerr << "Survivor Magiclysm mana-vampirism perk missing from catalog\n";
        return false;
    }
    const size_t mana_vamp = mana_vamp_it->second;
    if( integration( mana_vamp ) == 0 || max_rank( mana_vamp ) != 5 ||
        effect_count( mana_vamp ) != 1 ||
        std::strcmp( effect_id( mana_vamp, 0 ), "mg_melee_mana_vamp_pct" ) != 0 ||
        !nearly_equal( effect_value( mana_vamp, 0 ), 1.0 ) ) {
        std::cerr << "Survivor Magiclysm mana-vampirism metadata mismatch\n";
        return false;
    }
    for( int rank = 1; rank <= 5; ++rank ) {
        if( !nearly_equal( rank_multiplier( mana_vamp, rank ), static_cast<double>( rank ) ) ) {
            std::cerr << "Survivor Magiclysm mana-vampirism rank scaling mismatch at rank "
                      << rank << '\n';
            return false;
        }
    }

    const std::set<std::string> legacy_character_modifiers = {
        "str_flat", "dex_flat", "per_flat", "int_flat", "speed_pct", "move_cost_pct",
        "stamina_max_pct", "carry_weight_pct", "dodge_flat", "melee_hit_flat",
        "healing_pct", "read_speed_pct", "craft_speed_pct"
    };
    std::set<std::string> declared_effect_ids;
    for( size_t i = 0; i < perk_count; ++i ) {
        for( int e = 0; e < effect_count( i ); ++e ) {
            declared_effect_ids.insert( effect_id( i, e ) );
        }
    }
    for( const std::string &effect : declared_effect_ids ) {
        if( legacy_character_modifiers.count( effect ) != 0 ) continue;
        if( defined_v2_modifiers.count( effect ) == 0 ) {
            std::cerr << "Survivor effect has no Host v2 modifier definition: " << effect << '\n';
            return false;
        }
        if( runtime_hooks_by_modifier.count( effect ) == 0 ) {
            std::cerr << "Survivor effect has no consuming runtime hook: " << effect << '\n';
            return false;
        }
    }

    const auto find_index = [&]( const char *id ) -> size_t {
        const auto it = index.find( id );
        return it == index.end() ? perk_count : it->second;
    };
    const size_t c_power = find_index( "c_power" );
    const size_t c_veteran = find_index( "c_veteran" );
    const size_t s_survivor = find_index( "s_survivor" );
    const size_t predator = find_index( "cr_predator_momentum" );
    const size_t relentless = find_index( "cr_relentless_momentum" );
    const size_t momentum_engine = find_index( "ar_momentum_engine" );
    if( c_power >= perk_count || c_veteran >= perk_count || s_survivor >= perk_count ||
        predator >= perk_count || relentless >= perk_count || momentum_engine >= perk_count ) {
        std::cerr << "Survivor semantic fixtures are missing from catalog\n";
        return false;
    }

    size_t direct_cases = 0;
    size_t amplifier_cases = 0;
    size_t special_cases = 0;
    const std::set<std::string> special_ids = {
        "cr_predator_momentum", "ar_momentum_engine"
    };

    for( size_t i = 0; i < perk_count; ++i ) {
        const std::string id = perk_id( i );
        const int effects = effect_count( i );
        const int xp = xp_bonus( i );
        const double b_amp = branch_amp( i );
        const double g_amp = global_amp( i );
        const bool direct = effects > 0 || xp != 0;
        const bool amplifier = !nearly_equal( b_amp, 0.0 ) || !nearly_equal( g_amp, 0.0 );
        const bool special = special_ids.count( id ) != 0;

        if( !direct && !amplifier && !special ) {
            std::cerr << "Survivor perk has no covered semantic path: " << id << '\n';
            return false;
        }

        if( direct ) {
            ++direct_cases;
            for( int rank = 1; rank <= max_rank( i ); ++rank ) {
                if( !reset() ) return false;

                double scale_count = 1.0;
                if( scaling( i ) == 1 ) {
                    if( integration( i ) != 0 ) {
                        if( !set_rank( c_power, 1 ) ) return false;
                    }
                    scale_count = 1.0;
                } else if( scaling( i ) == 2 ) {
                    const size_t reference_major = id == "c_veteran" ? s_survivor : c_veteran;
                    if( !set_rank( reference_major, 1 ) ) return false;
                    scale_count = currency( i ) == 1 ? 2.0 : 1.0;
                }

                std::map<std::string, double> baseline_modifiers = modifiers;
                const int baseline_xp = current_xp();
                if( !set_rank( i, rank ) || !recalculate() ) {
                    std::cerr << "Survivor could not activate/recalculate perk: " << id
                              << " rank=" << rank << '\n';
                    return false;
                }

                const double rank_scale = rank_multiplier( i, rank );
                const double expected_scale = rank_scale * scale_count;
                for( int e = 0; e < effects; ++e ) {
                    const std::string eid = effect_id( i, e );
                    const std::string key = std::string( "survivor_progression:" ) + eid;
                    const auto before_it = baseline_modifiers.find( key );
                    const double before = before_it == baseline_modifiers.end() ? 0.0 : before_it->second;
                    const double actual_delta = survivor_modifier_value( eid ) - before;
                    const double expected_delta = effect_value( i, e ) * expected_scale;
                    if( !nearly_equal( actual_delta, expected_delta, 1.0e-7 ) ) {
                        std::cerr << "Survivor effect mismatch perk=" << id << " rank=" << rank
                                  << " effect=" << eid << " expected_delta=" << expected_delta
                                  << " actual_delta=" << actual_delta << '\n';
                        return false;
                    }
                }

                const int expected_xp_delta = static_cast<int>(
                                                  std::llround( xp * expected_scale ) );
                if( current_xp() - baseline_xp != expected_xp_delta ) {
                    std::cerr << "Survivor XP effect mismatch perk=" << id << " rank=" << rank
                              << " expected_delta=" << expected_xp_delta
                              << " actual_delta=" << ( current_xp() - baseline_xp ) << '\n';
                    return false;
                }

                if( !reset() || !survivor_modifiers_empty() || current_xp() != 0 ) {
                    std::cerr << "Survivor perk cleanup failed after " << id << '\n';
                    return false;
                }
            }
        }

        if( amplifier ) {
            ++amplifier_cases;
            if( !reset() ) return false;
            const char *reference_ids[] = {
                "c_power", "s_hardy", "m_light", "f_hands", "g_observer", "a_focus"
            };
            const int ref_branch = std::max( 0, std::min( 5, branch( i ) ) );
            const size_t reference = find_index( reference_ids[ref_branch] );
            if( reference >= perk_count || effect_count( reference ) <= 0 ) {
                std::cerr << "Survivor amplifier reference missing for " << id << '\n';
                return false;
            }
            if( !set_rank( reference, 1 ) ) return false;
            const std::string ref_effect = effect_id( reference, 0 );
            const double baseline = survivor_modifier_value( ref_effect );
            if( nearly_equal( baseline, 0.0 ) ) {
                std::cerr << "Survivor amplifier baseline is zero for " << id << '\n';
                return false;
            }

            const int amp_rank = max_rank( i );
            if( !set_rank( i, amp_rank ) ) return false;
            const double amp_scale = rank_multiplier( i, amp_rank );
            const double expected_factor = ( 1.0 + g_amp * amp_scale / 100.0 ) *
                                           ( 1.0 + b_amp * amp_scale / 100.0 );
            const double expected = baseline * expected_factor;
            const double actual = survivor_modifier_value( ref_effect );
            if( !nearly_equal( actual, expected, 1.0e-7 ) ) {
                std::cerr << "Survivor amplifier mismatch perk=" << id
                          << " expected=" << expected << " actual=" << actual << '\n';
                return false;
            }
            if( !reset() || !survivor_modifiers_empty() ) {
                std::cerr << "Survivor amplifier cleanup failed after " << id << '\n';
                return false;
            }
        }

        if( special ) {
            ++special_cases;
        }
    }

    // Full-catalog and deterministic combination stress.  This intentionally bypasses
    // purchase/exclusivity rules: the purpose is to prove the released DLL can
    // combine every declared perk effect without silent loss or stale modifiers.
    const auto verify_rank_vector = [&]( const std::vector<int> &ranks, const char *label ) {
        if( ranks.size() != perk_count || !reset() ) return false;

        std::array<bool, 6> active_branch = {{ false, false, false, false, false, false }};
        int active_branches = 0;
        int major_owned = 0;
        std::array<double, 6> branch_factor = {{ 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 }};
        double global_factor = 1.0;

        for( size_t i = 0; i < perk_count; ++i ) {
            if( ranks[i] <= 0 ) continue;
            if( integration( i ) == 0 ) active_branch[branch( i )] = true;
            if( currency( i ) == 1 ) ++major_owned;
            if( kind( i ) == 1 ) {
                const double rm = rank_multiplier( i, ranks[i] );
                branch_factor[branch( i )] += branch_amp( i ) * rm / 100.0;
                global_factor += global_amp( i ) * rm / 100.0;
            }
        }
        for( bool active : active_branch ) if( active ) ++active_branches;

        std::map<std::string, double> expected_modifiers;
        int expected_xp = 0;
        const double stat_power = static_cast<double>(
                                      world_setting_get_i64_fn( "NCMM_SP_STAT_POWER", 100 ) ) / 100.0;

        for( size_t i = 0; i < perk_count; ++i ) {
            const int rank_value = ranks[i];
            if( rank_value <= 0 ) continue;

            double scale = 1.0;
            if( scaling( i ) == 1 ) {
                scale = static_cast<double>( active_branches );
            } else if( scaling( i ) == 2 ) {
                // Production Survivor caps per-owned-major scaling at twelve majors.
                scale = static_cast<double>( std::min( major_owned, 12 ) );
            }
            scale *= rank_multiplier( i, rank_value );
            if( kind( i ) == 0 ) {
                scale *= global_factor * branch_factor[branch( i )] * stat_power;
            }

            expected_xp += static_cast<int>( std::llround( xp_bonus( i ) * scale ) );
            for( int e = 0; e < effect_count( i ); ++e ) {
                expected_modifiers[effect_id( i, e )] += effect_value( i, e ) * scale;
            }
        }
        expected_xp = std::max( -100, std::min( 5000, expected_xp ) );

        for( size_t i = 0; i < perk_count; ++i ) {
            if( ranks[i] > 0 && !set_rank( i, ranks[i] ) ) {
                std::cerr << "Survivor " << label << " could not set perk=" << perk_id( i )
                          << " rank=" << ranks[i] << '\n';
                return false;
            }
        }
        if( !recalculate() ) return false;

        for( const std::string &effect : declared_effect_ids ) {
            const auto expected_it = expected_modifiers.find( effect );
            const double expected = expected_it == expected_modifiers.end() ? 0.0 : expected_it->second;
            const double actual = survivor_modifier_value( effect );
            if( !nearly_equal( actual, expected, 1.0e-7 ) ) {
                std::cerr << "Survivor " << label << " aggregate mismatch effect=" << effect
                          << " expected=" << expected << " actual=" << actual << '\n';
                return false;
            }
        }
        if( current_xp() != expected_xp ) {
            std::cerr << "Survivor " << label << " aggregate XP mismatch expected="
                      << expected_xp << " actual=" << current_xp() << '\n';
            return false;
        }

        if( !reset() || !survivor_modifiers_empty() || current_xp() != 0 ) {
            std::cerr << "Survivor " << label << " aggregate cleanup failed\n";
            return false;
        }
        return true;
    };

    std::vector<int> all_max_ranks( perk_count, 0 );
    for( size_t i = 0; i < perk_count; ++i ) all_max_ranks[i] = max_rank( i );
    if( !verify_rank_vector( all_max_ranks, "all-perks-max-rank" ) ) return false;

    uint32_t combo_rng = 0x51A7C0DEu;
    constexpr int combination_cases = 24;
    for( int combo = 0; combo < combination_cases; ++combo ) {
        std::vector<int> ranks( perk_count, 0 );
        for( size_t i = 0; i < perk_count; ++i ) {
            const uint32_t draw = semantic_rng_next( combo_rng );
            if( ( draw & 3u ) == 0u ) {
                const int cap = max_rank( i );
                ranks[i] = 1 + static_cast<int>( semantic_rng_next( combo_rng ) %
                                                static_cast<uint32_t>( cap ) );
            }
        }
        if( !verify_rank_vector( ranks, "deterministic-combination" ) ) {
            std::cerr << "Survivor deterministic combination seed=0x51A7C0DE case="
                      << combo << '\n';
            return false;
        }
    }

    // Stateful event mechanic: Predator Momentum must cap, apply, expire, and
    // interact with both Relentless Momentum and Unbroken Momentum.
    if( !reset() || !set_rank( predator, 1 ) ) return false;
    for( int i = 0; i < 5; ++i ) dispatch_event( NCMM_EVENT_PLAYER_KILL_V2 );
    const std::string prefix = "survivor_progression:";
    if( character_state[prefix + "momentum_stacks"] != 3 ||
        character_state[prefix + "momentum_turns"] != 12 ||
        !nearly_equal( survivor_modifier_value( "sp_damage_dealt_pct" ), 9.0 ) ||
        !nearly_equal( survivor_modifier_value( "speed_pct" ), 3.0 ) ) {
        std::cerr << "Survivor Predator Momentum base behavior failed\n";
        return false;
    }
    for( int i = 0; i < 12; ++i ) dispatch_event( NCMM_EVENT_TURN_V2 );
    if( character_state[prefix + "momentum_stacks"] != 0 ||
        character_state[prefix + "momentum_turns"] != 0 ||
        !nearly_equal( survivor_modifier_value( "sp_damage_dealt_pct" ), 0.0 ) ||
        !nearly_equal( survivor_modifier_value( "speed_pct" ), 0.0 ) ) {
        std::cerr << "Survivor Predator Momentum expiry failed\n";
        return false;
    }

    if( !reset() || !set_rank( predator, 1 ) || !set_rank( relentless, 1 ) ) return false;
    for( int i = 0; i < 8; ++i ) dispatch_event( NCMM_EVENT_PLAYER_KILL_V2 );
    if( character_state[prefix + "momentum_stacks"] != 5 ||
        character_state[prefix + "momentum_turns"] != 20 ||
        !nearly_equal( survivor_modifier_value( "sp_damage_dealt_pct" ), 15.0 ) ||
        !nearly_equal( survivor_modifier_value( "speed_pct" ), 5.0 ) ) {
        std::cerr << "Survivor Relentless Momentum interaction failed\n";
        return false;
    }

    if( !reset() || !set_rank( predator, 1 ) || !set_rank( momentum_engine, 1 ) ) return false;
    for( int i = 0; i < 8; ++i ) dispatch_event( NCMM_EVENT_PLAYER_KILL_V2 );
    if( character_state[prefix + "momentum_stacks"] != 5 ||
        character_state[prefix + "momentum_turns"] != 12 ||
        !nearly_equal( survivor_modifier_value( "sp_damage_dealt_pct" ), 20.0 ) ||
        !nearly_equal( survivor_modifier_value( "speed_pct" ), 10.0 ) ) {
        std::cerr << "Survivor Unbroken Momentum interaction failed\n";
        return false;
    }

    if( !reset() || !survivor_modifiers_empty() || current_xp() != 0 ) {
        std::cerr << "Survivor final semantic cleanup failed\n";
        return false;
    }

    std::cout << "Survivor semantic matrix: PASS (" << perk_count
              << "/372 perks covered; direct=" << direct_cases
              << ", amplifiers=" << amplifier_cases
              << ", stateful=" << special_cases
              << ", conditional-inert=" << integration_inert_cases
              << ", consumed-effects=" << declared_effect_ids.size()
              << ", all-perks-max=PASS, deterministic-combinations=24)\n";
    return true;
}
}

int main( int argc, char **argv )
{
    if( argc < 2 || argc > 3 ) {
        std::cerr << "usage: ncmm_smoke_host <module> [--missing-contract]\n";
        return 2;
    }

    if( !runtime_fault_policy_smoke() ) {
        std::cerr << "NCMM runtime fault policy smoke test failed\n";
        return 20;
    }
    std::cout << "NCMM runtime fault policy: PASS\n";
    if( argc == 3 && std::strcmp( argv[2], "--missing-contract" ) == 0 ) {
        simulate_missing_contract = true;
    }

#ifdef _WIN32
    HMODULE native = LoadLibraryA( argv[1] );
    void *lib = native;
    if( !native ) {
        std::cerr << "LoadLibrary failed\n";
        return 3;
    }
#else
    void *lib = dlopen( argv[1], RTLD_NOW | RTLD_LOCAL );
    if( !lib ) {
        std::cerr << dlerror() << '\n';
        return 3;
    }
#endif

    auto get_desc = symbol<ncmm_get_descriptor_v1_fn>( lib, NCMM_ENTRYPOINT );
    if( !get_desc ) {
        std::cerr << "entrypoint missing\n";
        return 4;
    }

    const ncmm_mod_descriptor_v1 *desc = get_desc();
    if( !desc || desc->abi_version != NCMM_ABI_VERSION || desc->id == nullptr ) {
        std::cerr << "ABI/descriptor mismatch\n";
        return 5;
    }

    ncmm_host_api_v1 api{
        NCMM_ABI_VERSION,
        &log_fn,
        &has_capability_fn,
        &can_expose_fn,
        &expose_fn,
        &get_locale_fn,
        &get_host_version_fn,
        &get_loader_api_fn,
        &get_capability_count_fn,
        &get_capability_fn,
        &character_state_available_fn,
        &character_state_get_i64_fn,
        &character_state_set_i64_fn,
        &ui_choose_fn,
        &ui_message_fn,
        &group_begin_fn,
        &group_end_fn,
        &set_string_choices_fn,
        &modifier_set_fn,
        &modifier_clear_fn,
        &get_api_version_major_fn,
        &get_api_version_minor_fn
    };

    api.ui_tile_choose = &ui_tile_choose_fn;
    api.ui_card_choose = &ui_card_choose_fn;
    api.ui_tree_choose = &ui_tree_choose_fn;
    api.gameplay_metric_get_i64 = &gameplay_metric_get_i64_fn;
    api.world_mod_active = &world_mod_active_fn;
    api.world_setting_register_bool = &world_setting_register_bool_fn;
    api.world_setting_register_int = &world_setting_register_int_fn;
    api.world_setting_register_float = &world_setting_register_float_fn;
    api.world_setting_register_enum = &world_setting_register_enum_fn;
    api.world_setting_get_bool = &world_setting_get_bool_fn;
    api.world_setting_get_i64 = &world_setting_get_i64_fn;
    api.world_setting_get_f64 = &world_setting_get_f64_fn;
    api.world_setting_get_string = &world_setting_get_string_fn;
    api.worldgen_experimental_group_begin = &group_begin_fn;
    api.ui_card_choose_themed = &ui_card_choose_themed_fn;
    api.ui_tree_choose_themed = &ui_tree_choose_themed_fn;
    api.world_mod_count = &world_mod_count_fn;
    api.world_mod_id = &world_mod_id_fn;
    api.ui_card_choose_rpg = &ui_card_choose_rpg_fn;
    api.ui_tree_choose_rpg = &ui_tree_choose_rpg_fn;
    api.query_interface = &query_interface_fn;

    smoke_host2 = {};
    smoke_host2.struct_size = sizeof( smoke_host2 );
    smoke_host2.abi_version = NCMM_HOST_API_V2_CORE_ABI;
    smoke_host2.api_major = NCMM_HOST_API_V2_CORE_MAJOR;
    smoke_host2.api_minor = NCMM_HOST_API_V2_CORE_MINOR;
    smoke_host2.legacy_v1 = &api;
    smoke_host2.log = &log_fn;
    smoke_host2.has_capability = &has_capability_fn;
    smoke_host2.get_host_version = &get_host_version_fn;
    smoke_host2.current_module_id = &current_module_id_v2_fn;
    smoke_host2.event_available = &event_available_v2_fn;
    smoke_host2.event_subscribe = &event_subscribe_v2_fn;
    smoke_host2.event_unsubscribe_all = &event_unsubscribe_all_v2_fn;
    smoke_host2.world_setting_register_bool = &world_setting_register_bool_fn;
    smoke_host2.world_setting_register_int = &world_setting_register_int_fn;
    smoke_host2.world_setting_register_float = &world_setting_register_float_fn;
    smoke_host2.world_setting_register_enum = &world_setting_register_enum_fn;
    smoke_host2.world_setting_get_bool = &world_setting_get_bool_fn;
    smoke_host2.world_setting_get_i64 = &world_setting_get_i64_fn;
    smoke_host2.world_setting_get_f64 = &world_setting_get_f64_fn;
    smoke_host2.world_setting_get_string = &world_setting_get_string_fn;
    smoke_host2.world_mod_count = &world_mod_count_fn;
    smoke_host2.world_mod_id = &world_mod_id_fn;
    smoke_host2.world_mod_active = &world_mod_active_fn;
    smoke_host2.character_state_available = &character_state_available_fn;
    smoke_host2.character_state_get_i64 = &character_state_get_i64_fn;
    smoke_host2.character_state_set_i64 = &character_state_set_i64_fn;
    smoke_host2.module_is_loaded = &module_is_loaded_v2_fn;
    smoke_host2.module_version = &module_version_v2_fn;
    smoke_host2.module_state = &module_state_v2_fn;
    smoke_host2.modifier_define = &modifier_define_v2_fn;
    smoke_host2.modifier_set = &modifier_set_v2_fn;
    smoke_host2.modifier_clear_module = &modifier_clear_v2_fn;
    smoke_host2.modifier_get_total = &modifier_get_total_v2_fn;
    smoke_host2.runtime_hook_bind_modifier = &runtime_hook_bind_modifier_v2_fn;
    smoke_host2.runtime_hook_value = &runtime_hook_value_v2_fn;
    smoke_host2.worldgen_hook_bind_setting = &worldgen_hook_bind_setting_v2_fn;
    smoke_host2.worldgen_hook_bool = &worldgen_hook_bool_v2_fn;
    smoke_host2.worldgen_hook_i64 = &worldgen_hook_i64_v2_fn;
    smoke_host2.worldgen_hook_f64 = &worldgen_hook_f64_v2_fn;
    smoke_host2.runtime_hook_bind_setting = &runtime_hook_bind_setting_v2_fn;
    smoke_host2.runtime_hook_bool = &runtime_hook_bool_setting_v2_fn;
    smoke_host2.runtime_hook_i64 = &runtime_hook_i64_setting_v2_fn;
    smoke_host2.runtime_hook_f64 = &runtime_hook_f64_setting_v2_fn;

    if( std::strcmp( desc->id, "equipment_body_map" ) == 0 ) {
        if( !equipment_body_map_smoke::run( lib, desc, api ) ) return 44;
        std::cout << "NCMM smoke test: PASS (Equipment Body Map Host API failure/lifecycle matrix)\n";
        return 0;
    }

    if( std::strcmp( desc->id, "item_glyphs" ) == 0 ) {
        if( !desc->version || std::strcmp( desc->version, "0.1.0" ) != 0 ||
            !desc->init || !desc->shutdown ) {
            std::cerr << "Item Glyphs descriptor mismatch\n";
            return 47;
        }
        const int initialized = desc->init( &api );
        if( simulate_missing_contract ) {
            if( initialized != 0 || !registered_setting_ids.empty() ||
                !setting_meta.empty() || !runtime_setting_bindings.empty() ||
                runtime_setting_binding_count != 0 ) {
                std::cerr << "Item Glyphs missing-capability init must fail without registrations\n";
                return 48;
            }
        } else {
            const auto binding = runtime_setting_bindings.find( "inventory.item_glyphs.enabled" );
            const auto setting = setting_meta.find( "NCMM_IG_ENABLED" );
            if( initialized != 1 || registered_setting_ids != std::set<std::string>{ "NCMM_IG_ENABLED" } ||
                setting_meta.size() != 1 || setting == setting_meta.end() ||
                setting->second.kind != smoke_setting_kind::boolean ||
                setting->second.default_value != 1 || setting->second.scope != NCMM_WORLD_SETTING_LIVE ||
                runtime_setting_bindings.size() != 1 || runtime_setting_binding_count != 1 ||
                binding == runtime_setting_bindings.end() ||
                binding->second.setting_id != "NCMM_IG_ENABLED" || binding->second.type != NCMM_SETTING_BOOL_V2 ||
                runtime_hook_bool_setting_v2_fn( "inventory.item_glyphs.enabled", 0 ) != 1 ) {
                std::cerr << "Item Glyphs setting/binding/default contract failed\n";
                return 49;
            }
        }
        desc->shutdown();
        std::cout << "NCMM smoke test: PASS (Item Glyphs 0.1.0 "
                  << ( simulate_missing_contract ? "missing capability" : "runtime binding" ) << ")\n";
        return 0;
    }

    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        if( !api.has_capability( desc->required_capabilities[i] ) ) {
            if( simulate_missing_contract ) {
                std::cout << "NCMM missing-capability preflight: PASS\n";
                return 0;
            }
            std::cerr << "missing capability: " << desc->required_capabilities[i] << '\n';
            return 6;
        }
    }

    if( !desc->init( &api ) ) {
        std::cerr << "module disabled itself\n";
        return 7;
    }

    if( std::strcmp( desc->id, "advanced_world_settings" ) == 0 ) {
        const std::set<std::string> expected = {
            "SPAWN_DENSITY", "ITEM_SPAWNRATE", "MONSTER_SPEED",
            "MONSTER_RESILIENCE", "EVOLUTION_INVERSE_MULTIPLIER",
            "SEASON_LENGTH", "CONSTRUCTION_SCALING", "ETERNAL_SEASON",
            "ETERNAL_TIME_OF_DAY"
        };
        const std::set<std::string> actual( exposed.begin(), exposed.end() );
        if( actual != expected || groups.size() < 2 ||
            groups[0] != "aws_difficulty" || groups[1] != "aws_time" ) {
            std::cerr << "AWS grouped registration failed\n";
            return 8;
        }
        const std::vector<std::string> expected_time = { "normal", "day", "night" };
        if( fixed_time_values != expected_time ) {
            std::cerr << "fixed-time selector choices are incorrect\n";
            return 14;
        }
        if( std::strcmp( desc->version, "0.6.4" ) != 0 || worldgen_binding_count != 50 ) {
            std::cerr << "AWS Host API 2.0 registration coverage failed\n";
            return 22;
        }
        if( !aws_semantic_matrix() ) {
            return 39;
        }
        std::cout << "NCMM smoke test: PASS (AWS 0.6.4 selective scopes + Host API 2.0 geography bindings)\n";
        return 0;
    }

    if( std::strcmp( desc->id, "ballistic_hit_chance" ) == 0 ) {
        if( std::strcmp( desc->version, "0.1.0" ) != 0 ) {
            std::cerr << "Ballistic Hit Chance descriptor version mismatch\n";
            return 41;
        }
        if( registered_setting_ids.count( "NCMM_BHC_ENABLED" ) == 0 ||
            registered_setting_ids.count( "NCMM_BHC_DECIMAL" ) == 0 ||
            runtime_setting_binding_count != 2 ||
            runtime_setting_bindings.count( "targeting.hit_probability.enabled" ) == 0 ||
            runtime_setting_bindings.count( "targeting.hit_probability.decimal" ) == 0 ) {
            std::cerr << "Ballistic Hit Chance runtime setting registration failed\n";
            return 42;
        }
        if( runtime_hook_bool_setting_v2_fn( "targeting.hit_probability.enabled", 0 ) != 1 ||
            runtime_hook_bool_setting_v2_fn( "targeting.hit_probability.decimal", 1 ) != 0 ) {
            std::cerr << "Ballistic Hit Chance default setting values failed\n";
            return 43;
        }
        std::cout << "NCMM smoke test: PASS (Ballistic Hit Chance 0.1.0 runtime bindings)\n";
        return 0;
    }

    if( std::strcmp( desc->id, "survivor_progression" ) == 0 ) {
        auto on_turn = symbol<ncmm_on_turn_v1_fn>( lib, NCMM_TURN_ENTRYPOINT );
        auto open_ui = symbol<ncmm_open_ui_v1_fn>( lib, NCMM_OPEN_UI_ENTRYPOINT );
        auto migrate = symbol<ncmm_migrate_state_v1_fn>( lib, NCMM_MIGRATE_STATE_ENTRYPOINT );
        if( !on_turn || !open_ui || !migrate ) {
            std::cerr << "Survivor Progression callback export missing\n";
            return 9;
        }
        if( std::strcmp( desc->version, "0.14.0" ) != 0 ) {
            std::cerr << "Survivor Progression descriptor version mismatch\n";
            return 21;
        }
        if( modifier_definition_count == 0 || runtime_hook_binding_count == 0 ||
            event_subscription_count < 3 ) {
            std::cerr << "Survivor Host API 2.0 runtime registration failed\n";
            return 23;
        }

        bool mana_vamp_hook_registered = false;
        const auto mana_vamp_range = runtime_hooks_by_modifier.equal_range( "mg_melee_mana_vamp_pct" );
        for( auto it = mana_vamp_range.first; it != mana_vamp_range.second; ++it ) {
            if( it->second == "combat.melee_mana_vamp_pct" ) {
                mana_vamp_hook_registered = true;
                break;
            }
        }
        if( !mana_vamp_hook_registered ) {
            std::cerr << "Survivor Magiclysm mana-vampirism runtime hook missing\n";
            return 51;
        }

        bool mana_hands_hook_registered = false;
        const auto mana_hands_range = runtime_hooks_by_modifier.equal_range( "mg_virtual_hand_count" );
        for( auto it = mana_hands_range.first; it != mana_hands_range.second; ++it ) {
            if( it->second == "magic.virtual_hand_count" ) {
                mana_hands_hook_registered = true;
                break;
            }
        }
        if( !mana_hands_hook_registered ) {
            std::cerr << "Survivor Magiclysm virtual mana-hands runtime hook missing\n";
            return 52;
        }

        const std::string prefix = "survivor_progression:";
        character_state[prefix + "schema"] = 2;
        character_state[prefix + "xp_fraction"] = 250;
        if( !migrate( &api, 2, 8 ) ||
            character_state[prefix + "schema"] != 8 ) {
            std::cerr << "Survivor state-schema migration to 8 failed\n";
            return 24;
        }

        on_turn( &api );
        if( registered_setting_ids.count( "NCMM_SP_XP_RATE" ) == 0 ||
            registered_setting_ids.count( "NCMM_SP_STAT_POWER" ) == 0 ) {
            std::cerr << "Survivor Progression did not register both live balance settings\n";
            return 38;
        }
        if( !survivor_semantic_matrix( lib ) ) {
            return 40;
        }
        std::cout << "NCMM smoke test: PASS (Survivor Progression 0.14.0 Host API 2.0 registration + schema migration)\n";
        return 0;
    }

    std::cerr << "unknown module id\n";
    return 17;
}
