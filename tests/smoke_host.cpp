#include "ncmm_api.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
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

void log_fn( ncmm_log_level_v1, const char *message )
{
    std::cout << message << '\n';
}

int has_capability_fn( const char *cap )
{
    return std::strcmp( cap, "core.v1" ) == 0 ||
           std::strcmp( cap, "world_options.v1" ) == 0 ||
           std::strcmp( cap, "locale.v1" ) == 0 ||
           std::strcmp( cap, "module_contract.v1" ) == 0 ||
           std::strcmp( cap, "host_info.v1" ) == 0;
}

const char *get_locale_fn()
{
    return "en";
}

const char *get_host_version_fn()
{
    return "0.4.0-smoke";
}

uint32_t get_loader_api_fn()
{
    return NCMM_LOADER_API_VERSION;
}

const char *smoke_caps[] = {
    "core.v1", "world_options.v1", "locale.v1", "module_contract.v1", "host_info.v1"
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
    if( id == nullptr || allowed.count( id ) == 0 ) {
        return 0;
    }
    if( simulate_missing_contract && std::strcmp( id, "EVOLUTION_INVERSE_MULTIPLIER" ) == 0 ) {
        return 0;
    }
    return 1;
}

int expose_fn( const char *id, const char *, const char * )
{
    exposed.emplace_back( id );
    return 1;
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
    HMODULE lib = LoadLibraryA( argv[1] );
    if( !lib ) {
        std::cerr << "LoadLibrary failed\n";
        return 3;
    }
    auto get_desc = reinterpret_cast<ncmm_get_descriptor_v1_fn>( GetProcAddress( lib, NCMM_ENTRYPOINT ) );
#else
    void *lib = dlopen( argv[1], RTLD_NOW | RTLD_LOCAL );
    if( !lib ) {
        std::cerr << dlerror() << '\n';
        return 3;
    }
    auto get_desc = reinterpret_cast<ncmm_get_descriptor_v1_fn>( dlsym( lib, NCMM_ENTRYPOINT ) );
#endif
    if( !get_desc ) {
        std::cerr << "entrypoint missing\n";
        return 4;
    }

    const ncmm_mod_descriptor_v1 *desc = get_desc();
    if( !desc || desc->abi_version != NCMM_ABI_VERSION ) {
        std::cerr << "ABI mismatch\n";
        return 5;
    }

    ncmm_host_api_v1 api{ NCMM_ABI_VERSION, &log_fn, &has_capability_fn, &can_expose_fn, &expose_fn,
                          &get_locale_fn, &get_host_version_fn, &get_loader_api_fn,
                          &get_capability_count_fn, &get_capability_fn };
    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        if( !api.has_capability( desc->required_capabilities[i] ) ) {
            std::cerr << "missing capability\n";
            return 6;
        }
    }
    const int init_ok = desc->init( &api );
    if( simulate_missing_contract ) {
        if( init_ok != 0 || !exposed.empty() ) {
            std::cerr << "fail-closed preflight test failed\n";
            return 7;
        }
        std::cout << "NCMM fail-closed test: PASS (0 partial registrations)\n";
        return 0;
    }
    if( !init_ok ) {
        std::cerr << "module disabled itself\n";
        return 7;
    }

    const std::set<std::string> expected = {
        "SPAWN_DENSITY", "ITEM_SPAWNRATE", "MONSTER_SPEED",
        "MONSTER_RESILIENCE", "EVOLUTION_INVERSE_MULTIPLIER"
    };
    const std::set<std::string> actual( exposed.begin(), exposed.end() );
    if( actual != expected ) {
        std::cerr << "unexpected registration set\n";
        return 8;
    }
    std::cout << "NCMM smoke test: PASS (5/5 AWS contracts registered)\n";
    return 0;
}
