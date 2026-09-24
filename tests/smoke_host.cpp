#include "ncmm_api.h"

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
    return std::strcmp( cap, "core.v1" ) == 0 ||
           std::strcmp( cap, "world_options.v1" ) == 0 ||
           std::strcmp( cap, "world_options.layout.v1" ) == 0 ||
           std::strcmp( cap, "locale.v1" ) == 0 ||
           std::strcmp( cap, "module_contract.v1" ) == 0 ||
           std::strcmp( cap, "host_info.v1" ) == 0 ||
           std::strcmp( cap, "compatibility.v1" ) == 0 ||
           std::strcmp( cap, "events.turn.v1" ) == 0 ||
           std::strcmp( cap, "character_state.v1" ) == 0 ||
           std::strcmp( cap, "character.modifiers.v1" ) == 0 ||
           std::strcmp( cap, "ui.basic.v1" ) == 0 ||
           std::strcmp( cap, "module_hotkeys.v1" ) == 0 ||
           std::strcmp( cap, "ingame_manager.v1" ) == 0;
}

const char *get_locale_fn()
{
    return "en";
}

const char *get_host_version_fn()
{
    return "0.6.0-smoke";
}

uint32_t get_loader_api_fn()
{
    return NCMM_LOADER_API_VERSION;
}

const char *smoke_caps[] = {
    "core.v1", "world_options.v1", "world_options.layout.v1", "locale.v1",
    "module_contract.v1", "host_info.v1", "compatibility.v1", "events.turn.v1",
    "character_state.v1", "character.modifiers.v1", "ui.basic.v1",
    "module_hotkeys.v1", "ingame_manager.v1"
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

int ui_choose_fn( const char *title, const char *const *entries, size_t count )
{
    if( title == nullptr || entries == nullptr || count == 0 ) {
        return -1;
    }
    const std::string t( title );

    if( ui_script == 1 ) {
        // Buy Combat -> Power Training.
        if( ui_stage == 0 && t.find( "Survivor Progression v0.8.0" ) != std::string::npos ) {
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
        if( ui_stage == 0 && t.find( "Survivor Progression v0.8.0" ) != std::string::npos ) {
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
        &modifier_clear_fn
    };

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
        if( actual != expected || groups.size() != 2 ||
            groups[0] != "aws_advanced" || groups[1] != "aws_experimental" ) {
            std::cerr << "AWS grouped registration failed\n";
            return 8;
        }
        const std::vector<std::string> expected_time = { "normal", "day", "night" };
        if( fixed_time_values != expected_time ) {
            std::cerr << "fixed-time selector choices are incorrect\n";
            return 14;
        }
        std::cout << "NCMM smoke test: PASS (AWS 0.5.0 grouped controls)\n";
        return 0;
    }

    if( std::strcmp( desc->id, "survivor_progression" ) == 0 ) {
        auto on_turn = symbol<ncmm_on_turn_v1_fn>( lib, NCMM_TURN_ENTRYPOINT );
        auto open_ui = symbol<ncmm_open_ui_v1_fn>( lib, NCMM_OPEN_UI_ENTRYPOINT );
        if( !on_turn || !open_ui ) {
            std::cerr << "Survivor Progression callback export missing\n";
            return 9;
        }

        // 30 minutes -> level 2, one perk point.
        for( int i = 0; i < 1800; ++i ) {
            on_turn( &api );
        }
        const std::string prefix = "survivor_progression:";
        if( character_state[prefix + "level"] != 2 ||
            character_state[prefix + "perk_points"] != 1 ) {
            std::cerr << "Survivor level-2 progression failed\n";
            return 10;
        }

        // Buy Combat -> Power Training and verify the host modifier bridge.
        ui_script = 1;
        ui_stage = 0;
        open_ui( &api );
        if( character_state[prefix + "p_c_power"] != 1 ||
            modifiers["survivor_progression:str_flat"] != 1.0 ) {
            std::cerr << "Combat perk / modifier bridge failed\n";
            return 11;
        }

        // 45 more minutes -> level 3, another perk point.
        ui_script = 0;
        for( int i = 0; i < 2700; ++i ) {
            on_turn( &api );
        }
        if( character_state[prefix + "level"] != 3 ||
            character_state[prefix + "perk_points"] != 1 ) {
            std::cerr << "Survivor level-3 progression failed\n";
            return 12;
        }

        // Buy Fast Learner, then verify +100% minute XP.
        ui_script = 2;
        ui_stage = 0;
        open_ui( &api );
        if( character_state[prefix + "p_a_fast"] != 1 ) {
            std::cerr << "Fast Learner purchase failed\n";
            return 13;
        }
        const int64_t before = character_state[prefix + "xp"];
        ui_script = 0;
        for( int i = 0; i < 60; ++i ) {
            on_turn( &api );
        }
        if( character_state[prefix + "xp"] - before != 2 ) {
            std::cerr << "Fast Learner XP effect failed\n";
            return 15;
        }

        if( ui_message_count == 0 ) {
            std::cerr << "Survivor UI/message path was not exercised\n";
            return 16;
        }

        std::cout << "NCMM smoke test: PASS (Survivor Progression 0.8.0 full-system slice)\n";
        return 0;
    }

    std::cerr << "unknown module id\n";
    return 17;
}
