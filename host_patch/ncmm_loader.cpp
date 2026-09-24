#include "ncmm_loader.h"
#include "ncmm_api.h"
#include "avatar.h"
#include "game.h"
#include "input.h"
#include "input_context.h"
#include "options.h"
#include "output.h"
#include "system_locale.h"
#include "uilist.h"
#include "worldfactory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
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
    ncmm_on_turn_v1_fn on_turn = nullptr;
    ncmm_open_ui_v1_fn open_ui = nullptr;
    std::string action_id;
    std::string default_hotkey;
};

std::vector<loaded_mod> loaded;
std::ofstream log_file;
std::string locale_cache;

struct module_state {
    std::filesystem::path directory;
    std::string id;
    std::string name;
    std::string version;
    std::string state;
    std::string reason;
    std::string default_hotkey;
};

std::vector<module_state> module_states;
std::set<std::string> module_ids;
std::map<std::string, size_t> manifest_id_counts;
std::map<std::string, std::map<std::string, double>> character_modifier_values;

const std::set<std::string> supported_character_modifiers = {
    "str_flat", "dex_flat", "per_flat", "int_flat",
    "speed_pct", "move_cost_pct", "stamina_max_pct", "carry_weight_pct",
    "dodge_flat", "melee_hit_flat", "healing_pct", "read_speed_pct",
    "craft_speed_pct"
};

const char *const host_capabilities[] = {
    "core.v1",
    "world_options.v1",
    "locale.v1",
    "module_contract.v1",
    "host_info.v1",
    "compatibility.v1",
    "events.turn.v1",
    "character_state.v1",
    "ui.basic.v1",
    "module_hotkeys.v1",
    "ingame_manager.v1",
    "world_options.layout.v1",
    "character.modifiers.v1"
};

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
    for( const char *cap : host_capabilities ) {
        if( std::string( capability ) == cap ) {
            return 1;
        }
    }
    return 0;
}

const char *get_host_version()
{
    return "0.6.0";
}

uint32_t get_loader_api()
{
    return NCMM_LOADER_API_VERSION;
}

size_t get_capability_count()
{
    return sizeof( host_capabilities ) / sizeof( host_capabilities[0] );
}

const char *get_capability( size_t index )
{
    return index < get_capability_count() ? host_capabilities[index] : nullptr;
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

int worldgen_group_begin( const char *group_id, const char *display_name, const char *tooltip )
{
    if( !group_id || !display_name || !tooltip ) {
        return 0;
    }
    return get_options().ncmm_begin_worldgen_group(
               group_id, to_translation( display_name ), to_translation( tooltip ) ) ? 1 : 0;
}

void worldgen_group_end()
{
    get_options().ncmm_end_worldgen_group();
}

int worldgen_set_string_choices( const char *option_id, const char *const *value_ids,
                                 const char *const *display_names, size_t count )
{
    if( !option_id || !value_ids || !display_names || count == 0 || count > 32 ) {
        return 0;
    }
    std::vector<options_manager::id_and_option> items;
    items.reserve( count );
    for( size_t i = 0; i < count; ++i ) {
        if( !value_ids[i] || !display_names[i] ) {
            return 0;
        }
        items.emplace_back( value_ids[i], to_translation( display_names[i] ) );
    }
    return get_options().ncmm_set_worldgen_string_choices( option_id, items ) ? 1 : 0;
}

const char *get_locale()
{
    locale_cache = current_locale();
    return locale_cache.c_str();
}

bool safe_state_token( const char *value )
{
    if( value == nullptr ) {
        return false;
    }
    const std::string text( value );
    if( text.empty() || text.size() > 64 ) {
        return false;
    }
    for( unsigned char c : text ) {
        if( !( std::islower( c ) || std::isdigit( c ) || c == '_' || c == '-' || c == '.' ) ) {
            return false;
        }
    }
    return true;
}

std::string character_state_key( const char *module_id, const char *key )
{
    return "ncmm." + std::string( module_id ) + "." + std::string( key );
}

int character_state_available()
{
    return g != nullptr && world_generator != nullptr && world_generator->active_world != nullptr ? 1 : 0;
}

int64_t character_state_get_i64( const char *module_id, const char *key, int64_t fallback )
{
    if( !character_state_available() || !safe_state_token( module_id ) || !safe_state_token( key ) ) {
        return fallback;
    }

    const auto &values = get_avatar().get_values();
    const auto it = values.find( character_state_key( module_id, key ) );
    if( it == values.end() || !it->second.is_str() ) {
        return fallback;
    }

    try {
        std::size_t consumed = 0;
        const std::string &raw = it->second.str();
        const long long parsed = std::stoll( raw, &consumed, 10 );
        return consumed == raw.size() ? static_cast<int64_t>( parsed ) : fallback;
    } catch( ... ) {
        return fallback;
    }
}

int character_state_set_i64( const char *module_id, const char *key, int64_t value )
{
    if( !character_state_available() || !safe_state_token( module_id ) || !safe_state_token( key ) ) {
        return 0;
    }

    get_avatar().get_values()[character_state_key( module_id, key )] =
        diag_value( std::to_string( value ) );
    return 1;
}

int ui_choose( const char *title, const char *const *entries, size_t count )
{
    if( title == nullptr || entries == nullptr || count == 0 || count > 64 ) {
        return -1;
    }

    uilist menu;
    menu.text = title;
    for( size_t i = 0; i < count; ++i ) {
        if( entries[i] == nullptr ) {
            return -1;
        }
        menu.addentry( static_cast<int>( i ), true, MENU_AUTOASSIGN, entries[i] );
    }
    menu.query();
    return menu.ret >= 0 && static_cast<size_t>( menu.ret ) < count ? menu.ret : -1;
}

void ui_message( const char *message )
{
    if( message != nullptr ) {
        popup( "%s", message );
    }
}

int character_modifier_set( const char *module_id, const char *modifier_id, double value )
{
    if( !safe_state_token( module_id ) || modifier_id == nullptr ||
        supported_character_modifiers.count( modifier_id ) == 0 ||
        !std::isfinite( value ) || std::abs( value ) > 500.0 ) {
        return 0;
    }
    character_modifier_values[module_id][modifier_id] = value;
    return 1;
}

int character_modifier_clear_module( const char *module_id )
{
    if( !safe_state_token( module_id ) ) {
        return 0;
    }
    character_modifier_values.erase( module_id );
    return 1;
}

const ncmm_host_api_v1 api = {
    NCMM_ABI_VERSION,
    &log_line,
    &has_capability,
    &can_expose_worldgen_option,
    &expose_worldgen_option,
    &get_locale,
    &get_host_version,
    &get_loader_api,
    &get_capability_count,
    &get_capability,
    &character_state_available,
    &character_state_get_i64,
    &character_state_set_i64,
    &ui_choose,
    &ui_message,
    &worldgen_group_begin,
    &worldgen_group_end,
    &worldgen_set_string_choices,
    &character_modifier_set,
    &character_modifier_clear_module
};

std::string read_text_file( const std::filesystem::path &path )
{
    std::ifstream in( path, std::ios::binary );
    if( !in ) {
        return {};
    }

    in.seekg( 0, std::ios::end );
    const std::streamoff size = in.tellg();
    if( size < 0 || size > 64 * 1024 ) {
        return {};
    }
    in.seekg( 0, std::ios::beg );

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

uint32_t manifest_uint( const std::filesystem::path &directory, const std::string &key )
{
    const std::string text = read_text_file( directory / "mod.json" );
    if( text.empty() ) {
        return 0;
    }
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = text.find( needle );
    if( key_pos == std::string::npos ) {
        return 0;
    }
    const std::size_t colon = text.find( ':', key_pos + needle.size() );
    if( colon == std::string::npos ) {
        return 0;
    }

    std::size_t pos = colon + 1;
    while( pos < text.size() && std::isspace( static_cast<unsigned char>( text[pos] ) ) ) {
        ++pos;
    }
    uint32_t value = 0;
    bool any = false;
    while( pos < text.size() && std::isdigit( static_cast<unsigned char>( text[pos] ) ) ) {
        any = true;
        value = value * 10u + static_cast<uint32_t>( text[pos] - '0' );
        ++pos;
    }
    return any ? value : 0;
}

std::vector<std::string> manifest_string_array( const std::filesystem::path &directory,
        const std::string &key )
{
    std::vector<std::string> result;
    const std::string text = read_text_file( directory / "mod.json" );
    if( text.empty() ) {
        return result;
    }
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = text.find( needle );
    if( key_pos == std::string::npos ) {
        return result;
    }
    const std::size_t colon = text.find( ':', key_pos + needle.size() );
    const std::size_t open = colon == std::string::npos ? std::string::npos : text.find( '[', colon + 1 );
    const std::size_t close = open == std::string::npos ? std::string::npos : text.find( ']', open + 1 );
    if( open == std::string::npos || close == std::string::npos ) {
        return result;
    }

    std::size_t pos = open + 1;
    while( pos < close ) {
        const std::size_t first = text.find( '"', pos );
        if( first == std::string::npos || first >= close ) {
            break;
        }
        const std::size_t second = text.find( '"', first + 1 );
        if( second == std::string::npos || second > close ) {
            break;
        }
        result.push_back( text.substr( first + 1, second - first - 1 ) );
        pos = second + 1;
    }
    return result;
}

struct manifest_contract {
    std::string id;
    std::string name;
    std::string version;
    std::string failure_policy;
    std::string ui_hotkey;
    uint32_t loader_api = 0;
    std::vector<std::string> requires;
};

manifest_contract read_manifest( const std::filesystem::path &directory )
{
    manifest_contract result;
    result.id = manifest_string( directory, "id" );
    result.name = manifest_string( directory, "name" );
    result.version = manifest_string( directory, "version" );
    result.failure_policy = manifest_string( directory, "failure_policy" );
    result.ui_hotkey = manifest_string( directory, "ui_hotkey" );
    result.loader_api = manifest_uint( directory, "loader_api" );
    result.requires = manifest_string_array( directory, "requires" );
    return result;
}

bool valid_module_id( const std::string &id )
{
    if( id.empty() || id.size() > 64 ) {
        return false;
    }
    for( unsigned char c : id ) {
        if( !( std::islower( c ) || std::isdigit( c ) || c == '_' || c == '-' || c == '.' ) ) {
            return false;
        }
    }
    return true;
}

bool valid_ui_hotkey( const std::string &value )
{
    if( value.empty() ) {
        return true;
    }
    if( value.size() < 2 || value.size() > 3 || value[0] != 'F' ) {
        return false;
    }
    int number = 0;
    for( std::size_t i = 1; i < value.size(); ++i ) {
        const unsigned char c = static_cast<unsigned char>( value[i] );
        if( !std::isdigit( c ) ) {
            return false;
        }
        number = number * 10 + static_cast<int>( c - '0' );
    }
    return number >= 1 && number <= 12;
}

int ui_hotkey_keycode( const std::string &value )
{
    if( !valid_ui_hotkey( value ) || value.empty() ) {
        return 0;
    }
    int number = 0;
    for( std::size_t i = 1; i < value.size(); ++i ) {
        number = number * 10 + static_cast<int>( value[i] - '0' );
    }
    return keycode::f1 + number - 1;
}

bool validate_manifest( const manifest_contract &manifest, std::string &reason )
{
    if( !valid_module_id( manifest.id ) || manifest.name.empty() || manifest.name.size() > 128 ||
        manifest.version.empty() || manifest.version.size() > 64 || manifest.loader_api == 0 ) {
        reason = "invalid_manifest";
        return false;
    }
    if( manifest.failure_policy != "disable" ) {
        reason = "unsupported_failure_policy";
        return false;
    }
    if( manifest.requires.empty() || manifest.requires.size() > 32 ) {
        reason = "invalid_capability_list";
        return false;
    }

    std::set<std::string> unique;
    bool has_core = false;
    for( const std::string &capability : manifest.requires ) {
        if( capability.empty() || capability.size() > 64 || !unique.insert( capability ).second ) {
            reason = "invalid_capability_list";
            return false;
        }
        if( capability == "core.v1" ) {
            has_core = true;
        }
    }
    if( !has_core ) {
        reason = "core_capability_required";
        return false;
    }
    if( !valid_ui_hotkey( manifest.ui_hotkey ) ) {
        reason = "invalid_ui_hotkey";
        return false;
    }
    if( !manifest.ui_hotkey.empty() &&
        unique.count( "module_hotkeys.v1" ) == 0 ) {
        reason = "ui_hotkey_capability_required";
        return false;
    }
    return true;
}

bool same_capabilities( const manifest_contract &manifest, const ncmm_mod_descriptor_v1 *desc )
{
    if( desc == nullptr ) {
        return false;
    }
    if( desc->required_capability_count != 0 && desc->required_capabilities == nullptr ) {
        return false;
    }

    std::set<std::string> manifest_caps( manifest.requires.begin(), manifest.requires.end() );
    std::set<std::string> descriptor_caps;
    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        if( desc->required_capabilities[i] == nullptr ) {
            return false;
        }
        descriptor_caps.insert( desc->required_capabilities[i] );
    }
    return manifest_caps == descriptor_caps;
}

std::string json_escape( const std::string &value )
{
    std::string result;
    result.reserve( value.size() + 8 );
    for( unsigned char c : value ) {
        switch( c ) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if( c >= 0x20 ) {
                    result.push_back( static_cast<char>( c ) );
                }
                break;
        }
    }
    return result;
}

void record_module_state( const std::filesystem::path &directory, const manifest_contract &manifest,
                          const std::string &state, const std::string &reason )
{
    module_state entry;
    entry.directory = directory;
    entry.id = manifest.id.empty() ? directory.filename().string() : manifest.id;
    entry.name = manifest.name.empty() ? entry.id : manifest.name;
    entry.version = manifest.version;
    entry.state = state;
    entry.reason = reason;
    entry.default_hotkey = manifest.ui_hotkey;
    module_states.push_back( std::move( entry ) );
}

const module_state *find_module_state( const std::filesystem::path &directory )
{
    const std::filesystem::path wanted = directory.lexically_normal();
    for( const module_state &state : module_states ) {
        if( state.directory.lexically_normal() == wanted ) {
            return &state;
        }
    }
    return nullptr;
}

void write_modules_state()
{
    std::filesystem::create_directories( game_root() / "ncmm" );
    const std::filesystem::path path = game_root() / "ncmm" / "modules.state.json";
    const std::filesystem::path temp = game_root() / "ncmm" / "modules.state.json.tmp";

    std::ofstream out( temp, std::ios::trunc | std::ios::binary );
    if( !out ) {
        log_line( NCMM_LOG_WARN, "Could not write modules.state.json." );
        return;
    }

    out << "{\n"
        << "  \"schema\": 1,\n"
        << "  \"host_version\": \"0.6.0\",\n"
        << "  \"loader_api\": " << NCMM_LOADER_API_VERSION << ",\n"
        << "  \"capabilities\": [";
    for( size_t i = 0; i < get_capability_count(); ++i ) {
        if( i != 0 ) {
            out << ", ";
        }
        out << "\"" << json_escape( get_capability( i ) ) << "\"";
    }
    out << "],\n  \"modules\": [\n";
    for( size_t i = 0; i < module_states.size(); ++i ) {
        const module_state &state = module_states[i];
        out << "    {\"id\":\"" << json_escape( state.id )
            << "\",\"name\":\"" << json_escape( state.name )
            << "\",\"version\":\"" << json_escape( state.version )
            << "\",\"state\":\"" << json_escape( state.state )
            << "\",\"reason\":\"" << json_escape( state.reason )
            << "\",\"default_hotkey\":\"" << json_escape( state.default_hotkey ) << "\"}";
        if( i + 1 != module_states.size() ) {
            out << ',';
        }
        out << '\n';
    }
    out << "  ]\n}\n";
    out.close();

#ifdef _WIN32
    if( !MoveFileExW( temp.wstring().c_str(), path.wstring().c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
        log_line( NCMM_LOG_WARN, "Could not atomically publish modules.state.json." );
    }
#else
    std::error_code ec;
    std::filesystem::remove( path, ec );
    ec.clear();
    std::filesystem::rename( temp, path, ec );
    if( ec ) {
        log_line( NCMM_LOG_WARN, "Could not publish modules.state.json." );
    }
#endif
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
    std::string runtime_state;
    std::string reason;
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
        const module_state *state = find_module_state( dir );
        entry.loaded_now = runtime != nullptr;
        if( state ) {
            entry.runtime_state = state->state;
            entry.reason = state->reason;
        }

        if( runtime && runtime->descriptor ) {
            entry.name = runtime->descriptor->name ? runtime->descriptor->name : dir.filename().string();
            entry.version = runtime->descriptor->version ? runtime->descriptor->version : "";
        } else if( state ) {
            entry.name = state->name;
            entry.version = state->version;
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
    const std::filesystem::path directory = library.parent_path();
    const manifest_contract manifest = read_manifest( directory );

    std::string manifest_reason;
    if( !validate_manifest( manifest, manifest_reason ) ) {
        record_module_state( directory, manifest, "rejected", manifest_reason );
        log_line( NCMM_LOG_WARN,
                  ( "Rejected module manifest: " + directory.string() + " -> " + manifest_reason ).c_str() );
        return;
    }
    const auto count_it = manifest_id_counts.find( manifest.id );
    if( count_it != manifest_id_counts.end() && count_it->second > 1 ) {
        record_module_state( directory, manifest, "rejected", "duplicate_module_id" );
        log_line( NCMM_LOG_WARN, ( "Rejected duplicate module id: " + manifest.id ).c_str() );
        return;
    }
    if( manifest.loader_api != NCMM_LOADER_API_VERSION ) {
        record_module_state( directory, manifest, "rejected", "loader_api_mismatch" );
        log_line( NCMM_LOG_WARN, ( "Rejected module due to loader_api mismatch: " + manifest.id ).c_str() );
        return;
    }
    for( const std::string &capability : manifest.requires ) {
        if( !has_capability( capability.c_str() ) ) {
            record_module_state( directory, manifest, "rejected", "missing_capability:" + capability );
            log_line( NCMM_LOG_WARN,
                      ( "Rejected module due to missing manifest capability: " + manifest.id + " -> " +
                        capability ).c_str() );
            return;
        }
    }
    if( module_ids.count( manifest.id ) != 0 ) {
        record_module_state( directory, manifest, "rejected", "duplicate_module_id_runtime" );
        log_line( NCMM_LOG_WARN, ( "Rejected duplicate module id at runtime: " + manifest.id ).c_str() );
        return;
    }
    module_ids.insert( manifest.id );

    HMODULE module = LoadLibraryW( library.wstring().c_str() );
    if( module == nullptr ) {
        record_module_state( directory, manifest, "failed", "load_library_failed" );
        log_line( NCMM_LOG_WARN, ( "Failed to load " + library.string() ).c_str() );
        return;
    }

    auto get_descriptor = reinterpret_cast<ncmm_get_descriptor_v1_fn>(
                              GetProcAddress( module, NCMM_ENTRYPOINT ) );
    if( get_descriptor == nullptr ) {
        record_module_state( directory, manifest, "rejected", "entrypoint_missing" );
        log_line( NCMM_LOG_WARN, ( "Missing NCMM v1 entrypoint: " + library.string() ).c_str() );
        FreeLibrary( module );
        return;
    }

    const ncmm_mod_descriptor_v1 *desc = get_descriptor();
    if( desc == nullptr || desc->abi_version != NCMM_ABI_VERSION || desc->init == nullptr ||
        desc->id == nullptr || desc->version == nullptr ) {
        record_module_state( directory, manifest, "rejected", "descriptor_incompatible" );
        log_line( NCMM_LOG_WARN, ( "Rejected incompatible module: " + library.string() ).c_str() );
        FreeLibrary( module );
        return;
    }

    if( manifest.id != desc->id || manifest.version != desc->version ) {
        record_module_state( directory, manifest, "rejected", "manifest_descriptor_mismatch" );
        log_line( NCMM_LOG_WARN,
                  ( "Rejected module because mod.json and DLL descriptor disagree: " + manifest.id ).c_str() );
        FreeLibrary( module );
        return;
    }
    if( !same_capabilities( manifest, desc ) ) {
        record_module_state( directory, manifest, "rejected", "capability_contract_mismatch" );
        log_line( NCMM_LOG_WARN,
                  ( "Rejected module because manifest/descriptor capabilities disagree: " + manifest.id ).c_str() );
        FreeLibrary( module );
        return;
    }

    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        const char *required = desc->required_capabilities[i];
        if( required == nullptr || !has_capability( required ) ) {
            record_module_state( directory, manifest, "rejected",
                                 required ? std::string( "missing_capability:" ) + required :
                                 "invalid_required_capability" );
            log_line( NCMM_LOG_WARN,
                      ( std::string( "Disabled module due to missing/invalid capability: " ) + desc->id ).c_str() );
            FreeLibrary( module );
            return;
        }
    }

    auto locale_changed = reinterpret_cast<ncmm_on_locale_changed_v1_fn>(
                              GetProcAddress( module, NCMM_LOCALE_ENTRYPOINT ) );
    auto on_turn = reinterpret_cast<ncmm_on_turn_v1_fn>(
                       GetProcAddress( module, NCMM_TURN_ENTRYPOINT ) );
    auto open_ui = reinterpret_cast<ncmm_open_ui_v1_fn>(
                       GetProcAddress( module, NCMM_OPEN_UI_ENTRYPOINT ) );

    if( !manifest.ui_hotkey.empty() && open_ui == nullptr ) {
        record_module_state( directory, manifest, "rejected", "ui_hotkey_without_ui" );
        log_line( NCMM_LOG_WARN,
                  ( std::string( "Rejected module hotkey without UI callback: " ) + manifest.id ).c_str() );
        FreeLibrary( module );
        return;
    }

    if( !desc->init( &api ) ) {
        record_module_state( directory, manifest, "failed", "init_failed" );
        log_line( NCMM_LOG_WARN, ( std::string( "Module init failed; disabled: " ) + desc->id ).c_str() );
        FreeLibrary( module );
        return;
    }

    const std::string action_id = open_ui != nullptr && !manifest.ui_hotkey.empty() ?
                                  "ncmm.open." + manifest.id : std::string();
    loaded.push_back( { module, desc, directory, locale_changed, on_turn, open_ui,
                        action_id, manifest.ui_hotkey } );
    record_module_state( directory, manifest, "loaded", "ok" );
    log_line( NCMM_LOG_INFO, ( std::string( "Loaded module: " ) + desc->id + " " + desc->version ).c_str() );
}
#endif
} // namespace

double gameplay_modifier( const char *modifier_id )
{
    if( modifier_id == nullptr || supported_character_modifiers.count( modifier_id ) == 0 ) {
        return 0.0;
    }
    double total = 0.0;
    for( const auto &module : character_modifier_values ) {
        const auto it = module.second.find( modifier_id );
        if( it != module.second.end() ) {
            total += it->second;
        }
    }
    return std::max( -500.0, std::min( 500.0, total ) );
}

std::string settings_menu_label()
{
    return tr_ui( "<N|n>CMM / Mod Configuration", "<N|n>CMM / Настройка модов" );
}

void register_gameplay_actions( input_context &ctxt )
{
    const std::string manager_name = tr_ui( "NCMM / Mod Configuration",
                                           "NCMM / Настройка модов" );
    inp_mngr.ncmm_register_default_action(
        "ncmm.manager",
        no_translation( manager_name ),
        input_event( keycode::f2, input_event_t::keyboard_code ) );
    ctxt.register_action( "ncmm.manager", no_translation( manager_name ) );

    for( const loaded_mod &mod : loaded ) {
        if( mod.open_ui == nullptr || mod.action_id.empty() ) {
            continue;
        }

        const int keycode_value = ui_hotkey_keycode( mod.default_hotkey );
        std::string action_name = mod.descriptor && mod.descriptor->name ?
                                  mod.descriptor->name : mod.action_id;
        action_name += tr_ui( " UI", " — интерфейс" );

        if( keycode_value != 0 ) {
            inp_mngr.ncmm_register_default_action(
                mod.action_id,
                no_translation( action_name ),
                input_event( keycode_value, input_event_t::keyboard_code ) );
        }
        ctxt.register_action( mod.action_id, no_translation( action_name ) );
    }
}

bool handle_gameplay_action( const std::string &action )
{
    if( action == "ncmm.manager" ) {
        show_manager();
        return true;
    }

    for( const loaded_mod &mod : loaded ) {
        if( mod.action_id.empty() || action != mod.action_id || mod.open_ui == nullptr ) {
            continue;
        }
        try {
            mod.open_ui( &api );
        } catch( ... ) {
            const std::string name = mod.descriptor && mod.descriptor->name ?
                                     mod.descriptor->name : mod.action_id;
            log_line( NCMM_LOG_WARN, ( "Module UI callback failed: " + name ).c_str() );
            popup( tr_ui( "Module UI callback failed.", "Ошибка callback интерфейса мода." ) );
        }
        return true;
    }

    return false;
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
                        "NCMM — Mod Configuration\nIn game the manager is a normal remappable keybinding (F2 by default). Modules marked [UI] can be opened with Enter.",
                        "NCMM — Настройка модов\nВ игре менеджер — обычное переназначаемое действие (по умолчанию F2). Модули с [UI] открываются через Enter." );

        for( int i = 0; i < static_cast<int>( entries.size() ); ++i ) {
            const manager_entry &entry = entries[i];
            std::string state;
            if( entry.disabled ) {
                state = tr_ui( "OFF", "ВЫКЛ" );
            } else if( entry.loaded_now ) {
                state = tr_ui( "ON / loaded", "ВКЛ / загружен" );
            } else if( entry.runtime_state == "rejected" ) {
                state = tr_ui( "ON / rejected", "ВКЛ / отклонён" );
            } else if( entry.runtime_state == "failed" ) {
                state = tr_ui( "ON / failed", "ВКЛ / ошибка" );
            } else {
                state = tr_ui( "ON / not loaded", "ВКЛ / не загружен" );
            }

            std::string label = "[" + state + "] " + entry.name;
            if( !entry.version.empty() ) {
                label += "  " + entry.version;
            }
            if( !entry.reason.empty() && !entry.loaded_now && !entry.disabled ) {
                label += " - " + entry.reason;
            }
            const loaded_mod *runtime = find_loaded( entry.directory );
            if( runtime != nullptr && runtime->open_ui != nullptr ) {
                label += " [UI]";
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

        const loaded_mod *runtime = find_loaded( entry.directory );
        if( !entry.disabled && runtime != nullptr && runtime->open_ui != nullptr ) {
            uilist action;
            action.text = entry.name;
            action.addentry( 0, true, MENU_AUTOASSIGN, tr_ui( "Open module UI", "Открыть интерфейс мода" ) );
            action.addentry( 1, true, MENU_AUTOASSIGN, tr_ui( "Disable module", "Выключить модуль" ) );
            action.query();
            if( action.ret == 0 ) {
                try {
                    runtime->open_ui( &api );
                } catch( ... ) {
                    log_line( NCMM_LOG_WARN, ( "Module UI callback failed: " + entry.name ).c_str() );
                    popup( tr_ui( "Module UI callback failed.", "Ошибка callback интерфейса мода." ) );
                }
                continue;
            }
            if( action.ret != 1 ) {
                continue;
            }
        }

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

void on_turn()
{
    for( loaded_mod &mod : loaded ) {
        if( mod.on_turn ) {
            try {
                mod.on_turn( &api );
            } catch( ... ) {
                if( mod.descriptor && mod.descriptor->id ) {
                    log_line( NCMM_LOG_WARN,
                              ( std::string( "Turn callback failed for module: " ) +
                                mod.descriptor->id ).c_str() );
                }
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
    loaded.clear();
    module_states.clear();
    module_ids.clear();
    manifest_id_counts.clear();
    log_line( NCMM_LOG_INFO, "NCMM 0.6.0 Host API v1 / Module Contract v1 initializing." );
    std::atexit( &shutdown );

#ifdef _WIN32
    const std::filesystem::path mods_root = game_root() / "code_mods";
    if( std::filesystem::exists( mods_root ) ) {
        std::vector<std::filesystem::path> directories;
        for( const auto &entry : std::filesystem::directory_iterator( mods_root ) ) {
            if( entry.is_directory() && std::filesystem::exists( entry.path() / "ncmm_mod.dll" ) ) {
                directories.push_back( entry.path() );
            }
        }
        std::sort( directories.begin(), directories.end() );

        // Phase 1: count manifest IDs before any DLL is loaded. Duplicate IDs reject all
        // conflicting modules instead of silently favoring directory sort order.
        for( const std::filesystem::path &directory : directories ) {
            const manifest_contract manifest = read_manifest( directory );
            if( !manifest.id.empty() ) {
                ++manifest_id_counts[manifest.id];
            }
        }

        // Phase 2: validate contracts and load only unambiguous modules.
        for( const std::filesystem::path &directory : directories ) {
            const auto lib = directory / "ncmm_mod.dll";
            const auto disabled = directory / "disabled";
            if( std::filesystem::exists( disabled ) ) {
                record_module_state( directory, read_manifest( directory ), "disabled", "user_disabled" );
                continue;
            }
            load_one( lib );
        }
    }
#else
    log_line( NCMM_LOG_WARN, "NCMM native module loading is Windows-only; host continues without code mods." );
#endif

    write_modules_state();
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
    module_states.clear();
    module_ids.clear();
    manifest_id_counts.clear();
    character_modifier_values.clear();
}
} // namespace ncmm
