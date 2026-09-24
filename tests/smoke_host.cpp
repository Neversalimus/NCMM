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
bool simulate_missing_contract = false;
std::map<std::string, int64_t> character_state;
int ui_message_count = 0;

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
           std::strcmp( cap, "locale.v1" ) == 0 ||
           std::strcmp( cap, "module_contract.v1" ) == 0 ||
           std::strcmp( cap, "host_info.v1" ) == 0 ||
           std::strcmp( cap, "compatibility.v1" ) == 0 ||
           std::strcmp( cap, "events.turn.v1" ) == 0 ||
           std::strcmp( cap, "character_state.v1" ) == 0 ||
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
    return "0.5.1-smoke";
}

uint32_t get_loader_api_fn()
{
    return NCMM_LOADER_API_VERSION;
}

const char *smoke_caps[] = {
    "core.v1", "world_options.v1", "locale.v1", "module_contract.v1", "host_info.v1",
    "compatibility.v1", "events.turn.v1", "character_state.v1", "ui.basic.v1",
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
        "MONSTER_RESILIENCE", "EVOLUTION_INVERSE_MULTIPLIER"
    };
    return id != nullptr && allowed.count( id ) != 0 ? 1 : 0;
}

int expose_fn( const char *id, const char *, const char * )
{
    exposed.emplace_back( id );
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

int ui_choose_fn( const char *, const char *const *entries, size_t count )
{
    return entries != nullptr && count != 0 ? 0 : -1;
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
        &ui_message_fn
    };

    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        if( !api.has_capability( desc->required_capabilities[i] ) ) {
            if( simulate_missing_contract ) {
                std::cout << "NCMM missing-capability preflight: PASS\n";
                return 0;
            }
            std::cerr << "missing capability\n";
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
            "MONSTER_RESILIENCE", "EVOLUTION_INVERSE_MULTIPLIER"
        };
        const std::set<std::string> actual( exposed.begin(), exposed.end() );
        if( actual != expected ) {
            std::cerr << "unexpected AWS registration set\n";
            return 8;
        }
        std::cout << "NCMM smoke test: PASS (AWS 5/5 contracts registered)\n";
        return 0;
    }

    if( std::strcmp( desc->id, "survivor_progression" ) == 0 ) {
        auto on_turn = symbol<ncmm_on_turn_v1_fn>( lib, NCMM_TURN_ENTRYPOINT );
        auto open_ui = symbol<ncmm_open_ui_v1_fn>( lib, NCMM_OPEN_UI_ENTRYPOINT );
        if( !on_turn || !open_ui ) {
            std::cerr << "Survivor Progression callback export missing\n";
            return 9;
        }

        for( int i = 0; i < 1800; ++i ) {
            on_turn( &api );
        }

        const std::string prefix = "survivor_progression:";
        if( character_state[prefix + "level"] != 2 ||
            character_state[prefix + "perk_points"] != 0 ||
            character_state[prefix + "fast_learner"] != 1 ||
            character_state[prefix + "xp"] != 0 ) {
            std::cerr << "Survivor Progression level/perk vertical slice failed\n";
            return 10;
        }

        for( int i = 0; i < 60; ++i ) {
            on_turn( &api );
        }
        if( character_state[prefix + "xp"] != 2 ) {
            std::cerr << "Fast Learner effect did not double survival XP\n";
            return 11;
        }

        open_ui( &api );
        if( ui_message_count == 0 ) {
            std::cerr << "Survivor Progression UI/message path was not exercised\n";
            return 12;
        }

        std::cout << "NCMM smoke test: PASS (Survivor Progression 0.1 vertical slice)\n";
        return 0;
    }

    std::cerr << "unknown module id\n";
    return 13;
}
