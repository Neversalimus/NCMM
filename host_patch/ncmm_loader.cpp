#include "ncmm_loader.h"
#include "ncmm_api.h"
#include "options.h"
#include "popup.h"
#include "system_locale.h"
#include "uilist.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
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
    std::filesystem::path directory;
    ncmm_on_locale_changed_v1_fn locale_changed = nullptr;
};

std::vector<loaded_mod> loaded;
std::ofstream log_file;
std::string locale_cache;

std::filesystem::path game_root()
{
    return std::filesystem::current_path();
}

std::string current_locale()
{
    std::string selected = get_option<std::string>( "USE_LANG" );
    if( selected.empty() ) {
        selected = SystemLocale::Language().value_or( "en" );
    }
    if( selected.empty() ) {
        selected = "en";
    }
    return selected;
}

bool russian_ui()
{
    const std::string locale = current_locale();
    return locale == "ru" || locale.rfind( "ru_", 0 ) == 0;
}

std::string tr_ui( const char *english, const char *russian )
{
    return russian_ui() ? russian : english;
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
    return cap == "core.v1" || cap == "world_options.v1" || cap == "locale.v1";
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

const char *get_locale()
{
    locale_cache = current_locale();
    return locale_cache.c_str();
}

const ncmm_host_api_v1 api = {
    NCMM_ABI_VERSION,
    &log_line,
    &has_capability,
    &can_expose_worldgen_option,
    &expose_worldgen_option,
    &get_locale
};

std::string read_text_file( const std::filesystem::path &path )
{
    std::ifstream in( path, std::ios::binary );
    if( !in ) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string manifest_string( const std::filesystem::path &directory, const std::string &key )
{
    const std::string text = read_text_file( directory / "mod.json" );
    if( text.empty() ) {
        return {};
    }
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = text.find( needle );
    if( key_pos == std::string::npos ) {
        return {};
    }
    const std::size_t colon = text.find( ':', key_pos + needle.size() );
    if( colon == std::string::npos ) {
        return {};
    }
    const std::size_t first_quote = text.find( '"', colon + 1 );
    if( first_quote == std::string::npos ) {
        return {};
    }
    const std::size_t second_quote = text.find( '"', first_quote + 1 );
    if( second_quote == std::string::npos ) {
        return {};
    }
    return text.substr( first_quote + 1, second_quote - first_quote - 1 );
}

const loaded_mod *find_loaded( const std::filesystem::path &directory )
{
    const std::filesystem::path wanted = directory.lexically_normal();
    for( const loaded_mod &mod : loaded ) {
        if( mod.directory.lexically_normal() == wanted ) {
            return &mod;
        }
    }
    return nullptr;
}

struct manager_entry {
    std::filesystem::path directory;
    std::string name;
    std::string version;
    bool disabled = false;
    bool loaded_now = false;
};

std::vector<manager_entry> manager_entries()
{
    std::vector<manager_entry> result;
    const std::filesystem::path mods_root = game_root() / "code_mods";
    if( !std::filesystem::exists( mods_root ) ) {
        return result;
    }

    std::vector<std::filesystem::path> dirs;
    for( const auto &entry : std::filesystem::directory_iterator( mods_root ) ) {
        if( entry.is_directory() && std::filesystem::exists( entry.path() / "ncmm_mod.dll" ) ) {
            dirs.push_back( entry.path() );
        }
    }
    std::sort( dirs.begin(), dirs.end() );

    for( const std::filesystem::path &dir : dirs ) {
        manager_entry entry;
        entry.directory = dir;
        entry.disabled = std::filesystem::exists( dir / "disabled" );
        const loaded_mod *runtime = find_loaded( dir );
        entry.loaded_now = runtime != nullptr;

        if( runtime && runtime->descriptor ) {
            entry.name = runtime->descriptor->name ? runtime->descriptor->name : dir.filename().string();
            entry.version = runtime->descriptor->version ? runtime->descriptor->version : "";
        } else {
            entry.name = manifest_string( dir, "name" );
            entry.version = manifest_string( dir, "version" );
            if( entry.name.empty() ) {
                entry.name = dir.filename().string();
            }
        }
        result.push_back( entry );
    }
    return result;
}

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

    auto locale_changed = reinterpret_cast<ncmm_on_locale_changed_v1_fn>(
                              GetProcAddress( module, NCMM_LOCALE_ENTRYPOINT ) );
    loaded.push_back( { module, desc, library.parent_path(), locale_changed } );
    log_line( NCMM_LOG_INFO, ( std::string( "Loaded module: " ) + desc->id + " " + desc->version ).c_str() );
}
#endif
} // namespace

std::string settings_menu_label()
{
    return tr_ui( "<N|n>CMM / Mod Configuration", "<N|n>CMM / Настройка модов" );
}

void show_manager()
{
    while( true ) {
        const std::vector<manager_entry> entries = manager_entries();
        if( entries.empty() ) {
            popup( tr_ui( "No NCMM code mods are installed.", "NCMM code-моды не установлены." ) );
            return;
        }

        uilist menu;
        menu.text = tr_ui(
                        "NCMM — Mod Configuration\nEnter: enable/disable selected code mod. Changes apply after restart.",
                        "NCMM — Настройка модов\nEnter: включить/выключить выбранный code-мод. Изменения применяются после перезапуска." );

        for( int i = 0; i < static_cast<int>( entries.size() ); ++i ) {
            const manager_entry &entry = entries[i];
            std::string state;
            if( entry.disabled ) {
                state = tr_ui( "OFF", "ВЫКЛ" );
            } else if( entry.loaded_now ) {
                state = tr_ui( "ON / loaded", "ВКЛ / загружен" );
            } else {
                state = tr_ui( "ON / not loaded", "ВКЛ / не загружен" );
            }

            std::string label = "[" + state + "] " + entry.name;
            if( !entry.version.empty() ) {
                label += "  " + entry.version;
            }
            menu.addentry( i, true, MENU_AUTOASSIGN, label );
        }

        menu.query();
        if( menu.ret < 0 || menu.ret >= static_cast<int>( entries.size() ) ) {
            return;
        }

        const manager_entry &entry = entries[menu.ret];
        const std::filesystem::path marker = entry.directory / "disabled";
        std::error_code ec;

        if( entry.disabled ) {
            std::filesystem::remove( marker, ec );
            if( ec ) {
                popup( tr_ui( "Could not enable the module.", "Не удалось включить модуль." ) );
            } else {
                popup( tr_ui( "Module enabled. Restart CDDA to apply.",
                              "Модуль включён. Перезапустите CDDA для применения." ) );
            }
        } else {
            std::ofstream out( marker, std::ios::trunc );
            if( !out ) {
                popup( tr_ui( "Could not disable the module.", "Не удалось выключить модуль." ) );
            } else {
                out << "Disabled by NCMM Mod Configuration. Restart required.\n";
                out.close();
                popup( tr_ui( "Module disabled. Restart CDDA to apply.",
                              "Модуль выключен. Перезапустите CDDA для применения." ) );
            }
        }
    }
}

void on_language_changed()
{
    for( loaded_mod &mod : loaded ) {
        if( mod.locale_changed ) {
            try {
                mod.locale_changed( &api );
            } catch( ... ) {
                if( mod.descriptor && mod.descriptor->id ) {
                    log_line( NCMM_LOG_WARN,
                              ( std::string( "Locale refresh failed for module: " ) +
                                mod.descriptor->id ).c_str() );
                }
            }
        }
    }
}

void initialize()
{
    std::filesystem::create_directories( game_root() / "ncmm" );
    log_line( NCMM_LOG_INFO, "NCMM 0.3 Host API v1 initializing." );
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
    log_line( NCMM_LOG_WARN, "NCMM native module loading is Windows-only; host continues without code mods." );
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
