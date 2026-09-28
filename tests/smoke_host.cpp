#include "ncmm_api.h"
#include "ncmm_fault_policy.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
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
    if( simulate_missing_contract && std::strcmp( cap, "world_options.v1" ) == 0 ) {
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
        "module.lifecycle.v1", "settings.typed.v2", "worldgen.bindings.v2"
    };
    return capabilities.count( cap ) != 0 ? 1 : 0;
}

const char *get_locale_fn()
{
    return "en";
}

const char *get_host_version_fn()
{
    return "0.8.1-smoke";
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
    "module.lifecycle.v1", "settings.typed.v2", "worldgen.bindings.v2"
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
size_t runtime_hook_binding_count = 0;
size_t modifier_definition_count = 0;
size_t event_subscription_count = 0;
std::set<std::string> registered_setting_ids;

int world_setting_register_bool_fn( const char *, const char *, const char *, const char *,
                                    int, uint32_t )
{
    return 1;
}

int world_setting_register_int_fn( const char *, const char *, const char *, const char *,
                                   int, int, int, uint32_t )
{
    return 1;
}

int world_setting_register_float_fn( const char *, const char *, const char *, const char *,
                                     double, double, double, double, uint32_t )
{
    return 1;
}

int world_setting_register_enum_fn( const char *, const char *setting_id, const char *, const char *,
                                    const char *const *, const char *const *, size_t,
                                    const char *, uint32_t )
{
    if( setting_id != nullptr ) {
        registered_setting_ids.insert( setting_id );
    }
    return 1;
}

int world_setting_get_bool_fn( const char *, int fallback ) { return fallback; }
int64_t world_setting_get_i64_fn( const char *, int64_t fallback ) { return fallback; }
double world_setting_get_f64_fn( const char *, double fallback ) { return fallback; }
const char *world_setting_get_string_fn( const char *, const char *fallback ) { return fallback; }

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
int world_mod_active_fn( const char * ) { return 0; }
size_t world_mod_count_fn() { return 0; }
const char *world_mod_id_fn( size_t ) { return nullptr; }

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

int modifier_define_v2_fn( const char *, const char *, double, double )
{
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

int runtime_hook_bind_modifier_v2_fn( const char *, const char *, uint32_t,
                                      const char *, const char * )
{
    ++runtime_hook_binding_count;
    return 1;
}

double runtime_hook_value_v2_fn( const char *, const char *, const char *, const char *, const char * )
{
    return 0.0;
}

int worldgen_hook_bind_setting_v2_fn( const char *, const char *, const char *, uint32_t )
{
    ++worldgen_binding_count;
    return 1;
}

int worldgen_hook_bool_v2_fn( const char *, int fallback ) { return fallback; }
int64_t worldgen_hook_i64_v2_fn( const char *, int64_t fallback ) { return fallback; }
double worldgen_hook_f64_v2_fn( const char *, double fallback ) { return fallback; }

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
        if( ui_stage == 0 && t.find( "Survivor Progression v0.12.0" ) != std::string::npos ) {
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
        if( ui_stage == 0 && t.find( "Survivor Progression v0.12.0" ) != std::string::npos ) {
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
        if( ui_stage == 0 && t.find( "Survivor Progression v0.12.0" ) != std::string::npos ) {
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
        if( std::strcmp( desc->version, "0.6.2" ) != 0 || worldgen_binding_count < 40 ) {
            std::cerr << "AWS Host API 2.0 registration coverage failed\n";
            return 22;
        }
        std::cout << "NCMM smoke test: PASS (AWS 0.6.2 legacy controls + Host API 2.0 geography bindings)\n";
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
        if( std::strcmp( desc->version, "0.12.0" ) != 0 ) {
            std::cerr << "Survivor Progression descriptor version mismatch\n";
            return 21;
        }
        if( modifier_definition_count == 0 || runtime_hook_binding_count == 0 ||
            event_subscription_count < 3 ) {
            std::cerr << "Survivor Host API 2.0 runtime registration failed\n";
            return 23;
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
    std::cout << "NCMM smoke test: PASS (Survivor Progression 0.12.0 Host API 2.0 registration + schema migration)\n";
        return 0;
    }

    std::cerr << "unknown module id\n";
    return 17;
}
