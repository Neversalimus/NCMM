#include "ncmm_loader.h"
#include "ncmm_api.h"
#include "options.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ncmm
{
namespace
{
struct loaded_mod {
#ifdef _WIN32
    HMODULE handle = nullptr;
#endif
    const ncmm_mod_descriptor_v1 *descriptor = nullptr;
};

std::vector<loaded_mod> loaded;
std::ofstream log_file;

std::filesystem::path game_root()
{
    return std::filesystem::current_path();
}

void log_line( ncmm_log_level_v1 level, const char *message )
{
    if( !log_file.is_open() ) {
        std::filesystem::create_directories( game_root() / "ncmm" );
        log_file.open( game_root() / "ncmm" / "ncmm.log", std::ios::app );
    }
    if( log_file.is_open() ) {
        const char *prefix = level == NCMM_LOG_ERROR ? "ERROR" : level == NCMM_LOG_WARN ? "WARN" : "INFO";
        log_file << "[" << prefix << "] " << ( message ? message : "" ) << '\n';
        log_file.flush();
    }
}

int has_capability( const char *capability )
{
    if( capability == nullptr ) {
        return 0;
    }
    const std::string cap( capability );
    return cap == "core.v1" || cap == "world_options.v1";
}

int can_expose_worldgen_option( const char *option_id )
{
    if( !option_id ) {
        return 0;
    }
    return get_options().ncmm_can_expose_worldgen_option( option_id ) ? 1 : 0;
}

int expose_worldgen_option( const char *option_id, const char *display_name, const char *tooltip )
{
    if( !option_id || !display_name || !tooltip ) {
        return 0;
    }
    return get_options().ncmm_expose_worldgen_option( option_id,
            to_translation( display_name ), to_translation( tooltip ) ) ? 1 : 0;
}

const ncmm_host_api_v1 api = {
    NCMM_ABI_VERSION,
    &log_line,
    &has_capability,
    &can_expose_worldgen_option,
    &expose_worldgen_option
};

#ifdef _WIN32
void load_one( const std::filesystem::path &library )
{
    HMODULE module = LoadLibraryW( library.wstring().c_str() );
    if( module == nullptr ) {
        log_line( NCMM_LOG_WARN, ( "Failed to load " + library.string() ).c_str() );
        return;
    }

    auto get_descriptor = reinterpret_cast<ncmm_get_descriptor_v1_fn>(
                              GetProcAddress( module, NCMM_ENTRYPOINT ) );
    if( get_descriptor == nullptr ) {
        log_line( NCMM_LOG_WARN, ( "Missing NCMM v1 entrypoint: " + library.string() ).c_str() );
        FreeLibrary( module );
        return;
    }

    const ncmm_mod_descriptor_v1 *desc = get_descriptor();
    if( desc == nullptr || desc->abi_version != NCMM_ABI_VERSION || desc->init == nullptr ) {
        log_line( NCMM_LOG_WARN, ( "Rejected incompatible module: " + library.string() ).c_str() );
        FreeLibrary( module );
        return;
    }

    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        if( !has_capability( desc->required_capabilities[i] ) ) {
            log_line( NCMM_LOG_WARN, ( std::string( "Disabled module due to missing capability: " ) + desc->id ).c_str() );
            FreeLibrary( module );
            return;
        }
    }

    if( !desc->init( &api ) ) {
        log_line( NCMM_LOG_WARN, ( std::string( "Module init failed; disabled: " ) + desc->id ).c_str() );
        FreeLibrary( module );
        return;
    }

    loaded.push_back( { module, desc } );
    log_line( NCMM_LOG_INFO, ( std::string( "Loaded module: " ) + desc->id + " " + desc->version ).c_str() );
}
#endif
} // namespace

void initialize()
{
    std::filesystem::create_directories( game_root() / "ncmm" );
    log_line( NCMM_LOG_INFO, "NCMM Host API v1 initializing." );
    std::atexit( &shutdown );

#ifdef _WIN32
    const std::filesystem::path mods_root = game_root() / "code_mods";
    if( std::filesystem::exists( mods_root ) ) {
        std::vector<std::filesystem::path> libraries;
        for( const auto &entry : std::filesystem::directory_iterator( mods_root ) ) {
            if( !entry.is_directory() ) {
                continue;
            }
            const auto lib = entry.path() / "ncmm_mod.dll";
            const auto disabled = entry.path() / "disabled";
            if( std::filesystem::exists( lib ) && !std::filesystem::exists( disabled ) ) {
                libraries.push_back( lib );
            }
        }
        std::sort( libraries.begin(), libraries.end() );
        for( const auto &lib : libraries ) {
            load_one( lib );
        }
    }
#else
    log_line( NCMM_LOG_WARN, "NCMM v0.2 native module loading is Windows-only; host continues without code mods." );
#endif

    mark_ready();
}

void mark_ready()
{
    std::error_code ec;
    std::filesystem::remove( game_root() / "ncmm" / "boot.pending", ec );
    std::ofstream ready( game_root() / "ncmm" / "boot.ready", std::ios::trunc );
    ready << "ready\n";
}

void shutdown()
{
#ifdef _WIN32
    for( auto it = loaded.rbegin(); it != loaded.rend(); ++it ) {
        if( it->descriptor && it->descriptor->shutdown ) {
            it->descriptor->shutdown();
        }
        if( it->handle ) {
            FreeLibrary( it->handle );
        }
    }
#endif
    loaded.clear();
}
} // namespace ncmm
