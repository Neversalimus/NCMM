#include "ncmm_loader.h"
#include "ncmm_api.h"
#include "ncmm_fault_policy.h"
#include "ncmm_manifest_policy.h"
#include "avatar.h"
#include "game.h"
#include "event_bus.h"
#include "event_subscriber.h"
#include "type_id.h"
#include "input.h"
#include "input_context.h"
#include "options.h"
#include "output.h"
#include "system_locale.h"
#include "uilist.h"
#include "ui_manager.h"
#include "worldfactory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
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
    ncmm_migrate_state_v1_fn migrate_state = nullptr;
    uint32_t state_schema = 0;
    uint32_t state_min_supported = 0;
    bool migration_ready = false;
    bool migration_suspended = false;
    std::string default_hotkey;
    runtime_fault_policy fault;
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
    std::string lifecycle;
    std::string reason;
    std::string default_hotkey;
};

std::vector<module_state> module_states;
std::set<std::string> module_ids;
std::set<std::string> hotkey_registration_logged;
std::map<std::string, size_t> manifest_id_counts;
std::map<std::string, std::map<std::string, double>> character_modifier_values;
std::map<std::string, int64_t> gameplay_metric_values;
character_id gameplay_avatar_id;
bool gameplay_avatar_id_ready = false;
thread_local std::string active_module_id;
bool shutdown_registered = false;
bool gameplay_metrics_subscribed = false;

class gameplay_metric_subscriber : public event_subscriber
{
    public:
        using event_subscriber::notify;

        void notify( const cata::event &e ) override
        {
            if( e.type() == event_type::game_load ) {
                gameplay_metric_values.clear();
                gameplay_avatar_id = character_id();
                gameplay_avatar_id_ready = false;
                return;
            }
            if( e.type() == event_type::game_avatar_new ) {
                gameplay_metric_values.clear();
                gameplay_avatar_id = e.get<character_id>( "avatar_id" );
                gameplay_avatar_id_ready = true;
                return;
            }

            if( e.type() == event_type::avatar_moves ) {
                ++gameplay_metric_values["mobility.steps"];
                return;
            }
            if( e.type() == event_type::avatar_enters_omt ) {
                ++gameplay_metric_values["scavenging.omt"];
                return;
            }

            if( !gameplay_avatar_id_ready ) {
                return;
            }

            switch( e.type() ) {
                case event_type::character_kills_monster:
                    if( e.get<character_id>( "killer" ) == gameplay_avatar_id ) {
                        ++gameplay_metric_values["combat.kills"];
                        gameplay_metric_values["combat.kill_xp"] +=
                            std::max( 0, e.get<int>( "exp" ) );
                    }
                    break;
                case event_type::character_kills_character:
                    if( e.get<character_id>( "killer" ) == gameplay_avatar_id ) {
                        ++gameplay_metric_values["combat.kills"];
                        gameplay_metric_values["combat.kill_xp"] += 100;
                    }
                    break;
                case event_type::character_heals_damage:
                    if( e.get<character_id>( "character" ) == gameplay_avatar_id ) {
                        gameplay_metric_values["survival.healing"] +=
                            std::max( 0, e.get<int>( "damage" ) );
                    }
                    break;
                case event_type::character_finished_activity:
                    if( e.get<character_id>( "character" ) == gameplay_avatar_id &&
                        !e.get<bool>( "canceled" ) ) {
                        const std::string activity = e.get<activity_id>( "activity" ).str();
                        if( activity == "ACT_CRAFT" || activity == "ACT_MULTIPLE_CRAFT" ) {
                            ++gameplay_metric_values["crafting.completed"];
                        }
                    }
                    break;
                case event_type::gains_skill_level:
                    if( e.get<character_id>( "character" ) == gameplay_avatar_id ) {
                        ++gameplay_metric_values["mastery.skill_levels"];
                    }
                    break;
                default:
                    break;
            }
        }
};

gameplay_metric_subscriber gameplay_metrics;

class module_call_scope
{
    public:
        explicit module_call_scope( const char *module_id ) : previous_( active_module_id )
        {
            active_module_id = module_id ? module_id : "";
        }

        module_call_scope( const module_call_scope & ) = delete;
        module_call_scope &operator=( const module_call_scope & ) = delete;

        ~module_call_scope()
        {
            active_module_id = previous_;
        }

    private:
        std::string previous_;
};

const std::map<std::string, std::pair<double, double>> character_modifier_limits = {
    { "str_flat", { -20.0, 20.0 } },
    { "dex_flat", { -20.0, 20.0 } },
    { "per_flat", { -20.0, 20.0 } },
    { "int_flat", { -20.0, 20.0 } },
    { "speed_pct", { -75.0, 200.0 } },
    { "move_cost_pct", { -75.0, 300.0 } },
    { "stamina_max_pct", { -90.0, 500.0 } },
    { "carry_weight_pct", { -90.0, 500.0 } },
    { "dodge_flat", { -20.0, 20.0 } },
    { "melee_hit_flat", { -20.0, 20.0 } },
    { "healing_pct", { -100.0, 500.0 } },
    { "read_speed_pct", { -90.0, 500.0 } },
    { "craft_speed_pct", { -90.0, 500.0 } }
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
    "ui.tiles.v1",
    "ui.cards.v1",
    "ui.tree.v1",
    "gameplay.metrics.v1",
    "module_hotkeys.context.v1",
    "module_hotkeys.v1",
    "ingame_manager.v1",
    "world_options.layout.v1",
    "character.modifiers.v1",
    "api.versioning.v1",
    "state.migration.v1",
    "module.lifecycle.v1"
};

std::filesystem::path game_root()
{
    return std::filesystem::current_path();
}

std::string ncmm_escape_printf_percents( const std::string &text )
{
    std::string result;
    result.reserve( text.size() + 8 );
    for( const char ch : text ) {
        if( ch == '%' ) {
            result.push_back( '%' );
        }
        result.push_back( ch );
    }
    return result;
}

void ncmm_trim_and_print_literal( const catacurses::window &w, const point &begin,
                                  int width, const nc_color &base_color,
                                  const std::string &text )
{
    // CDDA trim_and_print -> print_colored_text -> wprintz -> wprintw.
    // wprintw treats '%' as printf syntax even when the input is already a
    // complete UI string. Trim first using the real visible text, then double
    // percent characters only for the final printf-backed write.
    const std::string clipped = trim_by_length( text, width );
    const std::string escaped = ncmm_escape_printf_percents( clipped );
    nc_color current = base_color;
    print_colored_text( w, begin, current, base_color, escaped );
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
    return "0.7.2";
}

uint32_t get_loader_api()
{
    return NCMM_LOADER_API_VERSION;
}

uint32_t get_api_version_major()
{
    return NCMM_API_VERSION_MAJOR;
}

uint32_t get_api_version_minor()
{
    return NCMM_API_VERSION_MINOR;
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

bool active_module_matches( const char *module_id )
{
    return safe_state_token( module_id ) && !active_module_id.empty() &&
           active_module_id == module_id;
}

bool module_modifiers_quarantined( const char *module_id )
{
    if( module_id == nullptr ) {
        return false;
    }
    for( const loaded_mod &mod : loaded ) {
        if( mod.descriptor && mod.descriptor->id &&
            std::string( mod.descriptor->id ) == module_id ) {
            return mod.fault.modifiers_quarantined;
        }
    }
    return false;
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
    if( !character_state_available() || !active_module_matches( module_id ) ||
        !safe_state_token( key ) ) {
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
    if( !character_state_available() || !active_module_matches( module_id ) ||
        !safe_state_token( key ) ) {
        return 0;
    }

    get_avatar().get_values()[character_state_key( module_id, key )] =
        diag_value( std::to_string( value ) );
    return 1;
}

int64_t gameplay_metric_get_i64( const char *metric_id )
{
    if( metric_id == nullptr || active_module_id.empty() || !character_state_available() ) {
        return 0;
    }

    // get_event_bus() is a game-owned object.  NCMM initialize() runs from
    // catacurses::init_interface, before game/event-bus lifetime is established.
    // Subscribe only after character/world availability proves gameplay exists.
    if( !gameplay_metrics_subscribed ) {
        get_event_bus().subscribe( &gameplay_metrics );
        gameplay_metrics_subscribed = true;
        log_line( NCMM_LOG_INFO, "gameplay.metrics.v1 event subscription armed in gameplay." );
    }

    if( !gameplay_avatar_id_ready ) {
        gameplay_avatar_id = get_avatar().getID();
        gameplay_avatar_id_ready = true;
    }

    const auto it = gameplay_metric_values.find( metric_id );
    return it == gameplay_metric_values.end() ? 0 : std::max<int64_t>( 0, it->second );
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

int ui_tile_choose( const char *title, const char *const *labels,
                    const char *const *details, size_t count, size_t requested_columns )
{
    if( title == nullptr || labels == nullptr || count == 0 || count > 16 ||
        requested_columns == 0 || requested_columns > 4 ) {
        return -1;
    }
    for( size_t i = 0; i < count; ++i ) {
        if( labels[i] == nullptr ) {
            return -1;
        }
    }

    // Very small terminals keep the proven vertical selector instead of clipping tiles.
    if( TERMX < 60 || TERMY < 18 ) {
        return ui_choose( title, labels, count );
    }

    int columns = static_cast<int>( std::min( requested_columns, count ) );
    constexpr int gap = 1;
    constexpr int tile_height = 5;
    constexpr int header_height = 4;

    while( columns > 1 ) {
        const int candidate = ( TERMX - 4 - gap * ( columns - 1 ) ) / columns;
        if( candidate >= 18 ) {
            break;
        }
        --columns;
    }

    const int rows = ( static_cast<int>( count ) + columns - 1 ) / columns;
    const int tile_width = std::max( 18, std::min( 30,
                           ( TERMX - 4 - gap * ( columns - 1 ) ) / columns ) );
    const int frame_width = columns * tile_width + gap * ( columns - 1 ) + 2;
    const int frame_height = header_height + rows * tile_height + 2;

    if( frame_width > TERMX || frame_height > TERMY ) {
        return ui_choose( title, labels, count );
    }

    const point origin( ( TERMX - frame_width ) / 2, ( TERMY - frame_height ) / 2 );
    catacurses::window frame = catacurses::newwin( frame_height, frame_width, origin );

    std::vector<catacurses::window> tiles;
    tiles.reserve( count );
    for( size_t i = 0; i < count; ++i ) {
        const int col = static_cast<int>( i ) % columns;
        const int row = static_cast<int>( i ) / columns;
        const point pos( origin.x + 1 + col * ( tile_width + gap ),
                         origin.y + header_height + row * tile_height );
        tiles.push_back( catacurses::newwin( tile_height, tile_width, pos ) );
    }

    input_context ctxt( "NCMM_TILE_CHOOSE", keyboard_mode::keychar );
    ctxt.register_cardinal();
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    int selected = 0;
    ui_adaptor ui;
    ui.position_from_window( frame );
    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( frame );
        draw_border( frame, BORDER_COLOR );
        fold_and_print( frame, point( 2, 1 ), frame_width - 4, c_light_gray, title );
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ), frame_width - 4, c_dark_gray,
                        tr_ui( "Arrows: select  Enter: open  Esc: close",
                               "Стрелки: выбор  Enter: открыть  Esc: закрыть" ) );
        wnoutrefresh( frame );

        for( size_t i = 0; i < tiles.size(); ++i ) {
            catacurses::window &tile = tiles[i];
            werase( tile );
            const bool active = static_cast<int>( i ) == selected;
            draw_border( tile, active ? c_light_green : BORDER_COLOR );
            ncmm_trim_and_print_literal( tile, point( 2, 1 ), tile_width - 4,
                            active ? c_white : c_light_gray, labels[i] );
            if( details != nullptr && details[i] != nullptr && details[i][0] != '\0' ) {
                ncmm_trim_and_print_literal( tile, point( 2, 2 ), tile_width - 4,
                                active ? c_cyan : c_dark_gray, details[i] );
            }
            if( active ) {
                mvwprintz( tile, point( 1, 1 ), c_light_green, ">" );
            }
            wnoutrefresh( tile );
        }
    } );

    while( true ) {
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        const int col = selected % columns;
        const int row = selected / columns;

        if( action == "LEFT" ) {
            if( col > 0 ) {
                --selected;
            }
        } else if( action == "RIGHT" ) {
            if( col + 1 < columns && selected + 1 < static_cast<int>( count ) ) {
                ++selected;
            }
        } else if( action == "UP" ) {
            if( row > 0 ) {
                selected -= columns;
            }
        } else if( action == "DOWN" ) {
            const int next = selected + columns;
            if( next < static_cast<int>( count ) ) {
                selected = next;
            }
        } else if( action == "CONFIRM" ) {
            return selected;
        } else if( action == "QUIT" ) {
            return -1;
        }
    }
}

int ui_card_choose( const char *title, const char *summary,
                    const ncmm_ui_progress_v1 *progress,
                    const ncmm_ui_card_v1 *cards, size_t count, size_t requested_columns )
{
    if( title == nullptr || cards == nullptr || count == 0 || count > 128 ||
        requested_columns == 0 || requested_columns > 4 ) {
        return -1;
    }
    for( size_t i = 0; i < count; ++i ) {
        if( cards[i].title == nullptr ) {
            return -1;
        }
    }

    if( TERMX < 64 || TERMY < 20 ) {
        std::vector<const char *> fallback;
        fallback.reserve( count );
        for( size_t i = 0; i < count; ++i ) {
            fallback.push_back( cards[i].title );
        }
        return ui_choose( title, fallback.data(), fallback.size() );
    }

    int columns = static_cast<int>( std::min( requested_columns, count ) );
    constexpr int gap = 1;
    constexpr int card_height = 7;
    constexpr int header_height = 5;
    constexpr int footer_height = 2;

    while( columns > 1 ) {
        const int candidate = ( TERMX - 4 - gap * ( columns - 1 ) ) / columns;
        if( candidate >= 26 ) {
            break;
        }
        --columns;
    }

    const int card_width = std::max( 26, std::min( 42,
                           ( TERMX - 4 - gap * ( columns - 1 ) ) / columns ) );
    const int frame_width = columns * card_width + gap * ( columns - 1 ) + 2;
    const int total_rows = ( static_cast<int>( count ) + columns - 1 ) / columns;
    const int max_frame_height = std::max( header_height + card_height + footer_height,
                                          TERMY - 2 );
    int visible_rows = std::max( 1, std::min( total_rows,
                            ( max_frame_height - header_height - footer_height ) / card_height ) );
    const int frame_height = header_height + visible_rows * card_height + footer_height;

    if( frame_width > TERMX || frame_height > TERMY ) {
        std::vector<const char *> fallback;
        fallback.reserve( count );
        for( size_t i = 0; i < count; ++i ) {
            fallback.push_back( cards[i].title );
        }
        return ui_choose( title, fallback.data(), fallback.size() );
    }

    const point origin( ( TERMX - frame_width ) / 2, ( TERMY - frame_height ) / 2 );
    catacurses::window frame = catacurses::newwin( frame_height, frame_width, origin );

    std::vector<catacurses::window> slots;
    slots.reserve( static_cast<size_t>( visible_rows * columns ) );
    for( int row = 0; row < visible_rows; ++row ) {
        for( int col = 0; col < columns; ++col ) {
            const point pos( origin.x + 1 + col * ( card_width + gap ),
                             origin.y + header_height + row * card_height );
            slots.push_back( catacurses::newwin( card_height, card_width, pos ) );
        }
    }

    input_context ctxt( "NCMM_CARD_CHOOSE", keyboard_mode::keychar );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "HOME" );
    ctxt.register_action( "END" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    int selected = 0;
    int first_row = 0;

    auto keep_visible = [&]() {
        const int row = selected / columns;
        if( row < first_row ) {
            first_row = row;
        } else if( row >= first_row + visible_rows ) {
            first_row = row - visible_rows + 1;
        }
        const int max_first = std::max( 0, total_rows - visible_rows );
        first_row = std::max( 0, std::min( first_row, max_first ) );
    };

    ui_adaptor ui;
    ui.position_from_window( frame );
    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( frame );
        draw_border( frame, BORDER_COLOR );
        ncmm_trim_and_print_literal( frame, point( 2, 1 ), frame_width - 4, c_white, title );

        if( summary != nullptr && summary[0] != '\0' ) {
            ncmm_trim_and_print_literal( frame, point( 2, 2 ), frame_width - 4, c_light_gray, summary );
        }

        if( progress != nullptr && progress->maximum > 0 ) {
            const int64_t maximum = std::max<int64_t>( 1, progress->maximum );
            const int64_t current = std::max<int64_t>( 0, std::min( progress->current, maximum ) );
            const long double ratio = static_cast<long double>( current ) /
                                      static_cast<long double>( maximum );
            const int bar_width = std::max( 8, std::min( 28, frame_width - 28 ) );
            const int filled = std::max( 0, std::min( bar_width,
                                    static_cast<int>( std::llround( ratio * bar_width ) ) ) );
            std::string bar = "[";
            bar.append( static_cast<size_t>( filled ), '=' );
            bar.append( static_cast<size_t>( bar_width - filled ), '.' );
            bar += "] ";
            bar += std::to_string( static_cast<int>( std::llround( ratio * 100.0L ) ) );
            bar += "%";
            std::string line = progress->label ? progress->label : "";
            if( !line.empty() ) {
                line += "  ";
            }
            line += bar;
            ncmm_trim_and_print_literal( frame, point( 2, 3 ), frame_width - 4, c_light_green, line );
        }

        std::string footer = tr_ui(
            "Arrows: select  Enter: open  Esc: back  PgUp/PgDn: scroll",
            "Стрелки: выбор  Enter: открыть  Esc: назад  PgUp/PgDn: прокрутка" );
        footer += "  " + std::to_string( selected + 1 ) + "/" + std::to_string( count );
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ), frame_width - 4,
                        c_dark_gray, footer );

        // Stage the parent first. The cards are separate curses windows inside
        // the frame; refreshing the blank parent after them erases their cells.
        wnoutrefresh( frame );

        const int first_index = first_row * columns;
        for( size_t slot = 0; slot < slots.size(); ++slot ) {
            catacurses::window &card_win = slots[slot];
            werase( card_win );
            const int index = first_index + static_cast<int>( slot );
            if( index >= static_cast<int>( count ) ) {
                wnoutrefresh( card_win );
                continue;
            }

            const ncmm_ui_card_v1 &card = cards[index];
            const bool active = index == selected;
            const bool owned = ( card.flags & NCMM_UI_CARD_OWNED ) != 0;
            const bool locked = ( card.flags & NCMM_UI_CARD_LOCKED ) != 0;
            const bool effect = ( card.flags & NCMM_UI_CARD_EFFECT ) != 0;
            const bool major = ( card.flags & NCMM_UI_CARD_MAJOR ) != 0;
            const bool accent = ( card.flags & NCMM_UI_CARD_ACCENT ) != 0;

            const nc_color border = active ? c_light_green :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta :
                                    accent ? c_light_blue : BORDER_COLOR;
            const nc_color title_color = locked ? c_dark_gray :
                                          active ? c_white : c_light_gray;
            draw_border( card_win, border );

            ncmm_trim_and_print_literal( card_win, point( 2, 1 ), card_width - 4,
                            title_color, card.title ? card.title : "" );
            if( card.subtitle != nullptr && card.subtitle[0] != '\0' ) {
                ncmm_trim_and_print_literal( card_win, point( 2, 2 ), card_width - 4,
                                locked ? c_dark_gray : c_light_gray, card.subtitle );
            }

            if( card.body != nullptr && card.body[0] != '\0' ) {
                const std::vector<std::string> folded = foldstring( card.body, card_width - 4 );
                for( size_t line = 0; line < std::min<size_t>( 2, folded.size() ); ++line ) {
                    ncmm_trim_and_print_literal( card_win, point( 2, 3 + static_cast<int>( line ) ),
                                    card_width - 4,
                                    locked ? c_dark_gray : active ? c_cyan : c_light_gray,
                                    folded[line] );
                }
            }

            if( card.badge != nullptr && card.badge[0] != '\0' ) {
                ncmm_trim_and_print_literal( card_win, point( 2, card_height - 2 ), card_width - 4,
                                owned ? c_cyan : major ? c_yellow : effect ? c_magenta :
                                locked ? c_dark_gray : c_green,
                                card.badge );
            }
            if( active ) {
                mvwprintz( card_win, point( 1, 1 ), c_light_green, ">" );
            }
            wnoutrefresh( card_win );
        }


    } );

    while( true ) {
        keep_visible();
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        const int col = selected % columns;
        const int row = selected / columns;

        if( action == "LEFT" ) {
            if( col > 0 ) {
                --selected;
            }
        } else if( action == "RIGHT" ) {
            if( col + 1 < columns && selected + 1 < static_cast<int>( count ) ) {
                ++selected;
            }
        } else if( action == "UP" ) {
            if( row > 0 ) {
                selected -= columns;
            }
        } else if( action == "DOWN" ) {
            const int next = selected + columns;
            if( next < static_cast<int>( count ) ) {
                selected = next;
            }
        } else if( action == "PAGE_UP" ) {
            selected = std::max( 0, selected - visible_rows * columns );
        } else if( action == "PAGE_DOWN" ) {
            selected = std::min( static_cast<int>( count ) - 1,
                                 selected + visible_rows * columns );
        } else if( action == "HOME" ) {
            selected = 0;
        } else if( action == "END" ) {
            selected = static_cast<int>( count ) - 1;
        } else if( action == "CONFIRM" ) {
            return selected;
        } else if( action == "QUIT" ) {
            return -1;
        }
    }
}

int ui_tree_choose( const char *title, const char *summary,
                    const ncmm_ui_progress_v1 *progress,
                    const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                    const ncmm_ui_tree_edge_v1 *edges, size_t edge_count )
{
    if( title == nullptr || nodes == nullptr || node_count == 0 || node_count > 64 ||
        edge_count > 128 || ( edge_count > 0 && edges == nullptr ) ) {
        return NCMM_UI_TREE_CANCEL;
    }

    int max_row = 0;
    int max_col = 0;
    for( size_t i = 0; i < node_count; ++i ) {
        if( nodes[i].title == nullptr || nodes[i].row < 0 || nodes[i].column < 0 ||
            nodes[i].row > 31 || nodes[i].column > 7 ) {
            return NCMM_UI_TREE_CANCEL;
        }
        max_row = std::max( max_row, nodes[i].row );
        max_col = std::max( max_col, nodes[i].column );
    }
    for( size_t i = 0; i < edge_count; ++i ) {
        if( edges[i].from_index >= node_count || edges[i].to_index >= node_count ) {
            return NCMM_UI_TREE_CANCEL;
        }
    }

    std::vector<int> layout_x2( node_count, 0 );
    for( size_t i = 0; i < node_count; ++i ) {
        layout_x2[i] = nodes[i].column * 2;
    }
    for( int pass = 0; pass < 3; ++pass ) {
        for( size_t i = 0; i < node_count; ++i ) {
            int parent_count = 0;
            int parent_min = 1000000;
            int parent_max = -1000000;
            size_t single_parent = 0;
            for( size_t e = 0; e < edge_count; ++e ) {
                if( edges[e].to_index != i ) {
                    continue;
                }
                const size_t parent = edges[e].from_index;
                parent_min = std::min( parent_min, layout_x2[parent] );
                parent_max = std::max( parent_max, layout_x2[parent] );
                single_parent = parent;
                ++parent_count;
            }
            if( parent_count >= 2 ) {
                layout_x2[i] = ( parent_min + parent_max ) / 2;
            } else if( parent_count == 1 &&
                       nodes[i].column == nodes[single_parent].column ) {
                layout_x2[i] = layout_x2[single_parent];
            }
        }
    }
    int max_layout_x2 = 0;
    for( int x2 : layout_x2 ) {
        max_layout_x2 = std::max( max_layout_x2, x2 );
    }

    // Tree mode remains an enhancement; narrow terminals keep the proven cards UI.
    if( TERMX < 118 || TERMY < 28 ) {
        std::vector<ncmm_ui_card_v1> cards;
        cards.reserve( node_count );
        for( size_t i = 0; i < node_count; ++i ) {
            cards.push_back( {
                nodes[i].id, nodes[i].title, nodes[i].subtitle, nodes[i].body,
                nodes[i].badge, nodes[i].icon_key, nodes[i].flags
            } );
        }
        return ui_card_choose( title, summary, progress, cards.data(), cards.size(), 2 );
    }

    constexpr int node_width = 24;
    constexpr int node_height = 5;
    constexpr int hgap = 2;
    constexpr int vgap = 1;
    constexpr int header_height = 5;
    constexpr int footer_height = 2;
    constexpr int detail_width = 42;

    const int lane_step = node_width + hgap;
    const int logical_tree_width = node_width + ( max_layout_x2 * lane_step + 1 ) / 2;
    const int frame_width = std::min( TERMX - 2, logical_tree_width + detail_width + 7 );
    const int canvas_width = frame_width - detail_width - 4;
    const int available_height = TERMY - 2 - header_height - footer_height;
    const int max_visible_rows = std::max( 1, ( available_height + vgap ) /
                                          ( node_height + vgap ) );
    const int visible_rows = std::min( max_row + 1, max_visible_rows );
    const int tree_height = visible_rows * node_height +
                            std::max( 0, visible_rows - 1 ) * vgap;
    const int frame_height = std::min( TERMY - 2,
                                      header_height + tree_height + footer_height );

    const point origin( ( TERMX - frame_width ) / 2, ( TERMY - frame_height ) / 2 );
    catacurses::window frame = catacurses::newwin( frame_height, frame_width, origin );

    input_context ctxt( "NCMM_TREE_CHOOSE", keyboard_mode::keychar );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "HOME" );
    ctxt.register_action( "END" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    int selected = 0;
    int first_row = 0;

    auto keep_visible = [&]() {
        const int row = nodes[selected].row;
        if( row < first_row ) {
            first_row = row;
        } else if( row >= first_row + visible_rows ) {
            first_row = row - visible_rows + 1;
        }
        first_row = std::max( 0, std::min( first_row,
                         std::max( 0, max_row - visible_rows + 1 ) ) );
    };

    auto node_x = [&]( size_t i ) {
        return 2 + ( layout_x2[i] * lane_step + 1 ) / 2;
    };
    auto node_y = [&]( size_t i ) {
        return header_height + ( nodes[i].row - first_row ) * ( node_height + vgap );
    };
    auto visible = [&]( size_t i ) {
        return nodes[i].row >= first_row && nodes[i].row < first_row + visible_rows &&
               node_x( i ) + node_width < canvas_width + 2;
    };

    auto select_direction = [&]( int row_sign, int col_sign ) {
        int best = -1;
        int best_score = 1000000;
        const int sr = nodes[selected].row;
        const int sc = layout_x2[selected];
        for( size_t i = 0; i < node_count; ++i ) {
            if( static_cast<int>( i ) == selected ) {
                continue;
            }
            const int dr = nodes[i].row - sr;
            const int dc = layout_x2[i] - sc;
            if( row_sign < 0 && dr >= 0 ) continue;
            if( row_sign > 0 && dr <= 0 ) continue;
            if( col_sign < 0 && dc >= 0 ) continue;
            if( col_sign > 0 && dc <= 0 ) continue;

            const int primary = row_sign != 0 ? std::abs( dr ) : std::abs( dc );
            const int secondary = row_sign != 0 ? std::abs( dc ) : std::abs( dr );
            const int score = primary * 100 + secondary * 10;
            if( score < best_score ) {
                best_score = score;
                best = static_cast<int>( i );
            }
        }
        if( best >= 0 ) {
            selected = best;
        }
    };

    ui_adaptor ui;
    ui.position_from_window( frame );
    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( frame );
        draw_border( frame, BORDER_COLOR );
        ncmm_trim_and_print_literal( frame, point( 2, 1 ), frame_width - 4, c_white, title );
        if( summary != nullptr && summary[0] != '\0' ) {
            ncmm_trim_and_print_literal( frame, point( 2, 2 ), frame_width - 4, c_light_gray, summary );
        }

        if( progress != nullptr && progress->maximum > 0 ) {
            const int64_t maximum = std::max<int64_t>( 1, progress->maximum );
            const int64_t current = std::max<int64_t>( 0, std::min( progress->current, maximum ) );
            const int percent = static_cast<int>( std::llround(
                static_cast<long double>( current ) * 100.0L /
                static_cast<long double>( maximum ) ) );
            std::string line = progress->label ? progress->label : "";
            if( !line.empty() ) line += "  ";
            line += "[" + std::to_string( percent ) + "%]";
            ncmm_trim_and_print_literal( frame, point( 2, 3 ), frame_width - 4, c_light_green, line );
        }

        const int divider_x = frame_width - detail_width - 2;
        for( int x = 1; x < divider_x; ++x ) {
            mvwprintz( frame, point( x, header_height - 1 ), c_dark_gray, "-" );
        }
        for( int y = header_height - 1; y < frame_height - footer_height; ++y ) {
            mvwprintz( frame, point( divider_x, y ), c_dark_gray, "|" );
        }
        ncmm_trim_and_print_literal( frame, point( divider_x + 2, header_height - 1 ),
                        detail_width - 3, c_dark_gray, tr_ui( "DETAIL", "ДЕТАЛИ" ) );

        // Connections are staged first so node boxes remain visually dominant.
        for( size_t e = 0; e < edge_count; ++e ) {
            const size_t from = edges[e].from_index;
            const size_t to = edges[e].to_index;
            if( !visible( from ) || !visible( to ) ) {
                continue;
            }
            const int x1 = node_x( from ) + node_width / 2;
            const int y1 = node_y( from ) + node_height;
            const int x2 = node_x( to ) + node_width / 2;
            const int y2 = node_y( to ) - 1;
            const int mid = y1 + std::max( 0, ( y2 - y1 ) / 2 );
            const bool edge_locked = ( nodes[to].flags & NCMM_UI_CARD_LOCKED ) != 0;
            const bool edge_owned = ( nodes[to].flags & NCMM_UI_CARD_OWNED ) != 0;
            const bool edge_major = ( nodes[to].flags & NCMM_UI_CARD_MAJOR ) != 0;
            const bool edge_effect = ( nodes[to].flags & NCMM_UI_CARD_EFFECT ) != 0;
            const nc_color edge_color = edge_locked ? c_dark_gray :
                                        edge_owned ? c_cyan :
                                        edge_major ? c_yellow :
                                        edge_effect ? c_magenta : c_light_gray;
            for( int y = y1; y <= mid && y < frame_height - footer_height; ++y ) {
                mvwprintz( frame, point( x1, y ), edge_color, "|" );
            }
            const int left = std::min( x1, x2 );
            const int right = std::max( x1, x2 );
            for( int x = left; x <= right && x < divider_x; ++x ) {
                mvwprintz( frame, point( x, mid ), edge_color, "-" );
            }
            for( int y = mid; y <= y2 && y < frame_height - footer_height; ++y ) {
                mvwprintz( frame, point( x2, y ), edge_color, "|" );
            }
            if( y2 >= header_height && y2 < frame_height - footer_height ) {
                mvwprintz( frame, point( x2, y2 ), edge_color, "v" );
            }
        }

        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) {
                continue;
            }
            const int x = node_x( i );
            const int y = node_y( i );
            const bool active = static_cast<int>( i ) == selected;
            const bool owned = ( nodes[i].flags & NCMM_UI_CARD_OWNED ) != 0;
            const bool locked = ( nodes[i].flags & NCMM_UI_CARD_LOCKED ) != 0;
            const bool major = ( nodes[i].flags & NCMM_UI_CARD_MAJOR ) != 0;
            const bool effect = ( nodes[i].flags & NCMM_UI_CARD_EFFECT ) != 0;

            const nc_color border = active ? c_light_green :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = locked ? c_dark_gray :
                                        active ? c_white : c_light_gray;

            std::string horizontal( static_cast<size_t>( node_width - 2 ), '-' );
            mvwprintz( frame, point( x, y ), border, "+" + horizontal + "+" );
            for( int line = 1; line < node_height - 1; ++line ) {
                mvwprintz( frame, point( x, y + line ), border, "|" );
                mvwprintz( frame, point( x + node_width - 1, y + line ), border, "|" );
            }
            mvwprintz( frame, point( x, y + node_height - 1 ), border, "+" + horizontal + "+" );

            ncmm_trim_and_print_literal( frame, point( x + 2, y + 1 ), node_width - 4,
                            text_color, nodes[i].title ? nodes[i].title : "" );
            ncmm_trim_and_print_literal( frame, point( x + 2, y + 2 ), node_width - 4,
                            locked ? c_dark_gray : c_light_gray,
                            nodes[i].subtitle ? nodes[i].subtitle : "" );
            ncmm_trim_and_print_literal( frame, point( x + 2, y + 3 ), node_width - 4,
                            major ? c_yellow : effect ? c_magenta :
                            owned ? c_cyan : locked ? c_dark_gray : c_green,
                            nodes[i].badge ? nodes[i].badge : "" );
            if( active ) {
                mvwprintz( frame, point( x + 1, y + 1 ), c_light_green, ">" );
            }
        }

        const ncmm_ui_tree_node_v1 &detail = nodes[selected];
        const int dx = divider_x + 2;
        ncmm_trim_and_print_literal( frame, point( dx, header_height ), detail_width - 3,
                        c_white, detail.title ? detail.title : "" );
        if( detail.subtitle != nullptr && detail.subtitle[0] != '\0' ) {
            ncmm_trim_and_print_literal( frame, point( dx, header_height + 1 ), detail_width - 3,
                            c_light_gray, detail.subtitle );
        }
        if( detail.badge != nullptr && detail.badge[0] != '\0' ) {
            ncmm_trim_and_print_literal( frame, point( dx, header_height + 2 ), detail_width - 3,
                            ( detail.flags & NCMM_UI_CARD_MAJOR ) ? c_yellow :
                            ( detail.flags & NCMM_UI_CARD_EFFECT ) ? c_magenta : c_cyan,
                            detail.badge );
        }
        if( detail.body != nullptr && detail.body[0] != '\0' ) {
            const std::vector<std::string> folded = foldstring( detail.body, detail_width - 3 );
            const int max_lines = std::max( 1, frame_height - header_height - footer_height - 4 );
            for( int line = 0; line < std::min<int>( max_lines, folded.size() ); ++line ) {
                ncmm_trim_and_print_literal( frame, point( dx, header_height + 4 + line ), detail_width - 3,
                                c_light_gray, folded[line] );
            }
        }

        std::string footer = tr_ui(
            "Arrows: navigate  Enter: details  Tab: cards  Esc: back  PgUp/PgDn: scroll",
            "Стрелки: навигация  Enter: детали  Tab: карточки  Esc: назад  PgUp/PgDn: прокрутка" );
        footer += "  " + std::to_string( selected + 1 ) + "/" + std::to_string( node_count );
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ), frame_width - 4,
                        c_dark_gray, footer );
        wnoutrefresh( frame );
    } );

    while( true ) {
        keep_visible();
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        if( action == "LEFT" ) {
            select_direction( 0, -1 );
        } else if( action == "RIGHT" ) {
            select_direction( 0, 1 );
        } else if( action == "UP" ) {
            select_direction( -1, 0 );
        } else if( action == "DOWN" ) {
            select_direction( 1, 0 );
        } else if( action == "PAGE_UP" ) {
            first_row = std::max( 0, first_row - visible_rows );
        } else if( action == "PAGE_DOWN" ) {
            first_row = std::min( std::max( 0, max_row - visible_rows + 1 ),
                                  first_row + visible_rows );
        } else if( action == "HOME" ) {
            selected = 0;
        } else if( action == "END" ) {
            selected = static_cast<int>( node_count ) - 1;
        } else if( action == "NEXT_TAB" ) {
            return NCMM_UI_TREE_SHOW_CARDS;
        } else if( action == "CONFIRM" ) {
            return selected;
        } else if( action == "QUIT" ) {
            return NCMM_UI_TREE_CANCEL;
        }
    }
}

void ui_message( const char *message )
{
    if( message != nullptr ) {
        popup( "%s", message );
    }
}

int character_modifier_set( const char *module_id, const char *modifier_id, double value )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        module_modifiers_quarantined( module_id ) ||
        modifier_id == nullptr || !std::isfinite( value ) ) {
        return 0;
    }

    const auto policy = character_modifier_limits.find( modifier_id );
    if( policy == character_modifier_limits.end() ||
        value < policy->second.first || value > policy->second.second ) {
        return 0;
    }

    character_modifier_values[module_id][modifier_id] = value;
    return 1;
}

int character_modifier_clear_module( const char *module_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ) {
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
    &character_modifier_clear_module,
    &get_api_version_major,
    &get_api_version_minor,
    &ui_tile_choose,
    &ui_card_choose,
    &ui_tree_choose,
    &gameplay_metric_get_i64
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

using manifest_contract = manifest_contract_v1;

manifest_contract read_manifest( const std::filesystem::path &directory,
                                 std::string *parse_reason = nullptr )
{
    manifest_contract result;
    const std::string text = read_text_file( directory / "mod.json" );
    std::string reason;
    if( !parse_manifest_contract_v1( text, result, reason ) ) {
        if( parse_reason != nullptr ) {
            *parse_reason = reason;
        }
        return {};
    }
    if( parse_reason != nullptr ) {
        parse_reason->clear();
    }
    return result;
}

int ui_hotkey_keycode( const std::string &value )
{
    if( !valid_ui_hotkey_v1( value ) || value.empty() ) {
        return 0;
    }
    int number = 0;
    for( std::size_t i = 1; i < value.size(); ++i ) {
        number = number * 10 + static_cast<int>( value[i] - '0' );
    }
    return keycode::f1 + number - 1;
}

std::string module_action_id( const loaded_mod &mod )
{
    if( mod.open_ui == nullptr || mod.descriptor == nullptr || mod.descriptor->id == nullptr ||
        mod.descriptor->id[0] == '\0' ) {
        return {};
    }
    return "ncmm.open." + std::string( mod.descriptor->id );
}

bool validate_manifest( const manifest_contract &manifest, std::string &reason )
{
    return validate_manifest_contract_v1( manifest, reason );
}

bool same_capabilities( const manifest_contract &manifest, const ncmm_mod_descriptor_v1 *desc )
{
    if( desc == nullptr || desc->required_capability_count > 32 ) {
        return false;
    }
    if( desc->required_capability_count != 0 && desc->required_capabilities == nullptr ) {
        return false;
    }
    if( desc->required_capability_count != manifest.required_capabilities.size() ) {
        return false;
    }

    std::set<std::string> manifest_caps( manifest.required_capabilities.begin(),
                                         manifest.required_capabilities.end() );
    std::set<std::string> descriptor_caps;
    for( size_t i = 0; i < desc->required_capability_count; ++i ) {
        if( desc->required_capabilities[i] == nullptr ) {
            return false;
        }
        const std::string capability( desc->required_capabilities[i] );
        if( !manifest_detail::safe_token( capability ) ||
            !descriptor_caps.insert( capability ).second ) {
            return false;
        }
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
    entry.lifecycle = state == "loaded" ? "active" :
                      ( state == "runtime_fault" || state == "suspended" ? "suspended" : "disabled" );
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
        << "  \"schema\": 3,\n"
        << "  \"host_version\": \"0.7.2\",\n"
        << "  \"loader_api\": " << NCMM_LOADER_API_VERSION << ",\n"
        << "  \"api_version\": {\"major\":" << NCMM_API_VERSION_MAJOR
        << ",\"minor\":" << NCMM_API_VERSION_MINOR << "},\n"
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
            << "\",\"lifecycle\":\"" << json_escape( state.lifecycle )
            << "\",\"reason\":\"" << json_escape( state.reason )
            << "\",\"default_hotkey\":\"" << json_escape( state.default_hotkey )
            << "\",\"directory\":\"" << json_escape( state.directory.filename().string() ) << "\"}";
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

loaded_mod *find_loaded_mutable( const std::filesystem::path &directory )
{
    const std::filesystem::path wanted = directory.lexically_normal();
    for( loaded_mod &mod : loaded ) {
        if( mod.directory.lexically_normal() == wanted ) {
            return &mod;
        }
    }
    return nullptr;
}

void quarantine_runtime_callback( loaded_mod &mod, runtime_callback_kind kind,
                                  const char *reason )
{
    const std::string module_id = mod.descriptor && mod.descriptor->id ?
                                  mod.descriptor->id : std::string();

    switch( kind ) {
        case runtime_callback_kind::turn:
            mod.on_turn = nullptr;
            break;
        case runtime_callback_kind::locale:
            mod.locale_changed = nullptr;
            break;
        case runtime_callback_kind::ui:
            mod.open_ui = nullptr;
            break;
    }

    if( !module_id.empty() ) {
        character_modifier_values.erase( module_id );
    }

    if( !mod.fault.quarantine( kind ) ) {
        return;
    }

    for( module_state &state : module_states ) {
        if( state.directory.lexically_normal() == mod.directory.lexically_normal() ) {
            state.state = "runtime_fault";
            state.lifecycle = "suspended";
            state.reason = reason ? reason : "runtime_exception";
            break;
        }
    }

    log_line( NCMM_LOG_WARN,
              ( "Runtime callback quarantined for module: " +
                ( module_id.empty() ? mod.directory.filename().string() : module_id ) +
                " -> " + ( reason ? reason : "runtime_exception" ) ).c_str() );
    write_modules_state();
}

bool suspend_state_migration( loaded_mod &mod, const char *reason )
{
    const std::string id = mod.descriptor && mod.descriptor->id ? mod.descriptor->id : std::string();
    if( !id.empty() ) {
        character_modifier_values.erase( id );
    }
    mod.migration_ready = false;
    mod.migration_suspended = true;
    for( module_state &state : module_states ) {
        if( state.directory.lexically_normal() == mod.directory.lexically_normal() ) {
            state.state = "suspended";
            state.lifecycle = "suspended";
            state.reason = reason ? reason : "state_migration_failed";
            break;
        }
    }
    log_line( NCMM_LOG_WARN,
              ( "State migration suspended module: " +
                ( id.empty() ? mod.directory.filename().string() : id ) + " -> " +
                ( reason ? reason : "state_migration_failed" ) ).c_str() );
    write_modules_state();
    return false;
}

bool ensure_state_migrated( loaded_mod &mod )
{
    if( mod.migrate_state == nullptr || mod.state_schema == 0 ) {
        return true;
    }
    if( !character_state_available() ) {
        mod.migration_ready = false;
        mod.migration_suspended = false;
        return true;
    }
    if( mod.migration_suspended ) {
        return false;
    }
    if( mod.migration_ready ) {
        return true;
    }

    const char *id = mod.descriptor && mod.descriptor->id ? mod.descriptor->id : nullptr;
    if( id == nullptr ) {
        return suspend_state_migration( mod, "state_migration_identity_missing" );
    }

    int64_t raw_schema = 0;
    {
        module_call_scope scope( id );
        raw_schema = character_state_get_i64( id, "schema", 0 );
    }
    if( raw_schema < 0 || raw_schema > 4294967295LL ) {
        return suspend_state_migration( mod, "state_schema_invalid" );
    }
    const uint32_t current = static_cast<uint32_t>( raw_schema );
    if( current == mod.state_schema ) {
        mod.migration_ready = true;
        return true;
    }
    if( current < mod.state_min_supported || current > mod.state_schema ) {
        return suspend_state_migration( mod, "state_schema_unsupported" );
    }

    bool ok = false;
    try {
        module_call_scope scope( id );
        ok = mod.migrate_state( &api, current, mod.state_schema ) != 0;
    } catch( ... ) {
        return suspend_state_migration( mod, "state_migration_exception" );
    }
    if( !ok ) {
        return suspend_state_migration( mod, "state_migration_failed" );
    }

    int64_t migrated = 0;
    {
        module_call_scope scope( id );
        migrated = character_state_get_i64( id, "schema", 0 );
    }
    if( migrated != static_cast<int64_t>( mod.state_schema ) ) {
        return suspend_state_migration( mod, "state_migration_uncommitted" );
    }

    mod.migration_ready = true;
    mod.migration_suspended = false;
    bool changed = false;
    for( module_state &state : module_states ) {
        if( state.directory.lexically_normal() == mod.directory.lexically_normal() ) {
            if( state.state == "suspended" ) {
                state.state = "loaded";
                state.lifecycle = "active";
                state.reason = "ok";
                changed = true;
            }
            break;
        }
    }
    if( changed ) {
        write_modules_state();
    }
    return true;
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
            const manifest_contract manifest = read_manifest( dir );
            entry.name = manifest.name;
            entry.version = manifest.version;
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
    std::string manifest_reason;
    const manifest_contract manifest = read_manifest( directory, &manifest_reason );

    if( !manifest_reason.empty() ) {
        record_module_state( directory, manifest, "rejected", manifest_reason );
        log_line( NCMM_LOG_WARN,
                  ( "Rejected module manifest parse/schema: " + directory.string() +
                    " -> " + manifest_reason ).c_str() );
        return;
    }
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
    if( manifest.api_contract_declared &&
        ( manifest.api_major != NCMM_API_VERSION_MAJOR ||
          manifest.api_min_minor > NCMM_API_VERSION_MINOR ) ) {
        record_module_state( directory, manifest, "rejected", "api_version_mismatch" );
        log_line( NCMM_LOG_WARN, ( "Rejected module due to semantic API mismatch: " + manifest.id ).c_str() );
        return;
    }
    for( const std::string &capability : manifest.required_capabilities ) {
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

    const ncmm_mod_descriptor_v1 *desc = nullptr;
    try {
        desc = get_descriptor();
    } catch( ... ) {
        record_module_state( directory, manifest, "failed", "descriptor_exception" );
        log_line( NCMM_LOG_WARN,
                  ( "Module descriptor callback threw; disabled: " + manifest.id ).c_str() );
        FreeLibrary( module );
        return;
    }

    if( desc == nullptr || desc->abi_version != NCMM_ABI_VERSION || desc->init == nullptr ||
        desc->id == nullptr || desc->name == nullptr || desc->version == nullptr ) {
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
    auto migrate_state = reinterpret_cast<ncmm_migrate_state_v1_fn>(
                             GetProcAddress( module, NCMM_MIGRATE_STATE_ENTRYPOINT ) );

    if( manifest.state_contract_declared && migrate_state == nullptr ) {
        record_module_state( directory, manifest, "rejected", "migration_entrypoint_missing" );
        log_line( NCMM_LOG_WARN,
                  ( std::string( "Rejected module state contract without migration callback: " ) + manifest.id ).c_str() );
        FreeLibrary( module );
        return;
    }

    if( !manifest.ui_hotkey.empty() && open_ui == nullptr ) {
        record_module_state( directory, manifest, "rejected", "ui_hotkey_without_ui" );
        log_line( NCMM_LOG_WARN,
                  ( std::string( "Rejected module hotkey without UI callback: " ) + manifest.id ).c_str() );
        FreeLibrary( module );
        return;
    }

    // Reserve identity only after the DLL descriptor/capability contract is fully validated.
    // Init-time host APIs depend on module_ids containing the active module.
    module_ids.insert( manifest.id );

    // A retry/reload must never inherit runtime effects from an older failed init.
    character_modifier_values.erase( manifest.id );
    bool init_ok = false;
    try {
        module_call_scope scope( manifest.id.c_str() );
        init_ok = desc->init( &api ) != 0;
    } catch( ... ) {
        character_modifier_values.erase( manifest.id );
        module_ids.erase( manifest.id );
        record_module_state( directory, manifest, "failed", "init_exception" );
        log_line( NCMM_LOG_WARN,
                  ( std::string( "Module init callback threw; disabled: " ) + desc->id ).c_str() );
        FreeLibrary( module );
        return;
    }
    if( !init_ok ) {
        character_modifier_values.erase( manifest.id );
        module_ids.erase( manifest.id );
        record_module_state( directory, manifest, "failed", "init_failed" );
        log_line( NCMM_LOG_WARN, ( std::string( "Module init failed; disabled: " ) + desc->id ).c_str() );
        FreeLibrary( module );
        return;
    }

    // Keep module identity and its preferred key as passive metadata.
    // Action IDs/default bindings are derived only when a gameplay input context exists.
    loaded.push_back( { module, desc, directory, locale_changed, on_turn, open_ui,
                        migrate_state,
                        manifest.state_contract_declared ? manifest.state_schema : 0u,
                        manifest.state_contract_declared ? manifest.state_min_supported : 0u,
                        false, false, manifest.ui_hotkey } );
    record_module_state( directory, manifest, "loaded", "ok" );
    log_line( NCMM_LOG_INFO, ( std::string( "Loaded module: " ) + desc->id + " " + desc->version ).c_str() );
}
#endif
} // namespace

double gameplay_modifier( const char *modifier_id )
{
    if( modifier_id == nullptr || character_modifier_limits.count( modifier_id ) == 0 ) {
        return 0.0;
    }

    double total = 0.0;
    for( const auto &module : character_modifier_values ) {
        const auto it = module.second.find( modifier_id );
        if( it != module.second.end() ) {
            total += it->second;
        }
    }

    // Aggregate clamp is intentionally wider than the per-module policy so several
    // independently validated code-mods can stack without one module bypassing bounds.
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

    for( loaded_mod &mod : loaded ) {
        if( mod.open_ui == nullptr || mod.descriptor == nullptr || mod.descriptor->id == nullptr ) {
            continue;
        }

        const std::string action_id = module_action_id( mod );
        if( action_id.empty() ) {
            continue;
        }

        std::string action_name = mod.descriptor->name ?
                                  mod.descriptor->name : action_id;
        action_name += tr_ui( " UI", " — интерфейс" );

        const int keycode_value = ui_hotkey_keycode( mod.default_hotkey );
        if( keycode_value != 0 ) {
            inp_mngr.ncmm_register_context_default_action(
                action_id,
                no_translation( action_name ),
                input_event( keycode_value, input_event_t::keyboard_code ),
                "DEFAULTMODE" );
            if( hotkey_registration_logged.insert( action_id ).second ) {
                log_line( NCMM_LOG_INFO,
                          ( "Module hotkey registered in gameplay context: " +
                            action_id + " -> " + mod.default_hotkey ).c_str() );
            }
        }
        ctxt.register_action( action_id, no_translation( action_name ) );
    }
}

bool handle_gameplay_action( const std::string &action )
{
    if( action == "ncmm.manager" ) {
        show_manager();
        return true;
    }

    for( loaded_mod &mod : loaded ) {
        if( mod.open_ui == nullptr || mod.descriptor == nullptr || mod.descriptor->id == nullptr ) {
            continue;
        }
        const std::string action_id = module_action_id( mod );
        if( action_id.empty() || action != action_id ) {
            continue;
        }
        if( !ensure_state_migrated( mod ) ) {
            popup( tr_ui( "Module state migration is suspended for this character. See NCMM diagnostics.",
                          "Миграция состояния модуля приостановлена для этого персонажа. См. диагностику NCMM." ) );
            return true;
        }
        try {
            module_call_scope scope( mod.descriptor->id );
            mod.open_ui( &api );
        } catch( ... ) {
            const std::string name = mod.descriptor->name ?
                                     mod.descriptor->name : action_id;
            quarantine_runtime_callback( mod, runtime_callback_kind::ui, "ui_exception" );
            log_line( NCMM_LOG_WARN, ( "Module UI callback failed: " + name ).c_str() );
            popup( tr_ui( "Module UI callback failed and was quarantined for this session.",
                          "Ошибка callback интерфейса мода; callback помещён в карантин до перезапуска." ) );
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
            } else if( entry.runtime_state == "runtime_fault" ) {
                state = tr_ui( "ON / quarantined", "ВКЛ / карантин" );
            } else if( entry.runtime_state == "suspended" ) {
                state = tr_ui( "ON / suspended", "ВКЛ / приостановлен" );
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
            if( !entry.reason.empty() && !entry.disabled &&
                ( !entry.loaded_now || entry.runtime_state == "runtime_fault" ) ) {
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

        loaded_mod *runtime = find_loaded_mutable( entry.directory );
        if( !entry.disabled && runtime != nullptr && runtime->open_ui != nullptr ) {
            uilist action;
            action.text = entry.name;
            action.addentry( 0, true, MENU_AUTOASSIGN, tr_ui( "Open module UI", "Открыть интерфейс мода" ) );
            action.addentry( 1, true, MENU_AUTOASSIGN, tr_ui( "Disable module", "Выключить модуль" ) );
            action.query();
            if( action.ret == 0 ) {
                if( !ensure_state_migrated( *runtime ) ) {
                    popup( tr_ui( "Module state migration is suspended for this character. See NCMM diagnostics.",
                                  "Миграция состояния модуля приостановлена для этого персонажа. См. диагностику NCMM." ) );
                    continue;
                }
                try {
                    module_call_scope scope( runtime->descriptor && runtime->descriptor->id ?
                                             runtime->descriptor->id : nullptr );
                    runtime->open_ui( &api );
                } catch( ... ) {
                    quarantine_runtime_callback( *runtime, runtime_callback_kind::ui, "ui_exception" );
                    log_line( NCMM_LOG_WARN, ( "Module UI callback failed: " + entry.name ).c_str() );
                    popup( tr_ui( "Module UI callback failed and was quarantined for this session.",
                                  "Ошибка callback интерфейса мода; callback помещён в карантин до перезапуска." ) );
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
        if( mod.on_turn && ensure_state_migrated( mod ) ) {
            try {
                module_call_scope scope( mod.descriptor && mod.descriptor->id ?
                                         mod.descriptor->id : nullptr );
                mod.on_turn( &api );
            } catch( ... ) {
                quarantine_runtime_callback( mod, runtime_callback_kind::turn, "turn_exception" );
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
                module_call_scope scope( mod.descriptor && mod.descriptor->id ?
                                         mod.descriptor->id : nullptr );
                mod.locale_changed( &api );
            } catch( ... ) {
                quarantine_runtime_callback( mod, runtime_callback_kind::locale, "locale_exception" );
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

    // Defensive re-entry: never abandon loaded DLLs or runtime modifier state.
    if( !loaded.empty() ) {
        shutdown();
    }
    active_module_id.clear();
    loaded.clear();
    module_states.clear();
    module_ids.clear();
    hotkey_registration_logged.clear();
    manifest_id_counts.clear();
    character_modifier_values.clear();
    gameplay_metric_values.clear();
    gameplay_avatar_id = character_id();
    gameplay_avatar_id_ready = false;
    log_line( NCMM_LOG_INFO, "NCMM 0.7.2 Host API 1.4 / gameplay.metrics.v1 initializing (subscription deferred)." );

    if( !shutdown_registered ) {
        std::atexit( &shutdown );
        shutdown_registered = true;
    }

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

        // Phase 1: count ACTIVE manifest IDs before any DLL is loaded. A disabled backup
        // must not block the one enabled copy; two enabled copies reject each other.
        for( const std::filesystem::path &directory : directories ) {
            if( std::filesystem::exists( directory / "disabled" ) ) {
                continue;
            }
            std::string parse_reason;
            const manifest_contract manifest = read_manifest( directory, &parse_reason );
            if( parse_reason.empty() && valid_module_id_v1( manifest.id ) ) {
                ++manifest_id_counts[manifest.id];
            }
        }

        // Phase 2: validate contracts and load only unambiguous modules.
        for( const std::filesystem::path &directory : directories ) {
            const auto lib = directory / "ncmm_mod.dll";
            const auto disabled = directory / "disabled";
            if( std::filesystem::exists( disabled ) ) {
                std::string disabled_parse_reason;
                const manifest_contract disabled_manifest = read_manifest( directory, &disabled_parse_reason );
                record_module_state( directory, disabled_manifest, "disabled",
                                     disabled_parse_reason.empty() ? "user_disabled" :
                                     "user_disabled:" + disabled_parse_reason );
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
    const std::filesystem::path directory = game_root() / "ncmm";
    const std::filesystem::path ready_path = directory / "boot.ready";
    const std::filesystem::path temp_path = directory / "boot.ready.tmp";
    const std::filesystem::path pending_path = directory / "boot.pending";

    std::filesystem::create_directories( directory );
    {
        std::ofstream out( temp_path, std::ios::trunc | std::ios::binary );
        if( !out ) {
            log_line( NCMM_LOG_ERROR, "Could not stage boot.ready; boot.pending preserved." );
            return;
        }
        out << "ready\n";
        out.flush();
        if( !out ) {
            log_line( NCMM_LOG_ERROR, "Could not flush staged boot.ready; boot.pending preserved." );
            out.close();
            std::error_code cleanup_ec;
            std::filesystem::remove( temp_path, cleanup_ec );
            return;
        }
    }

#ifdef _WIN32
    if( !MoveFileExW( temp_path.wstring().c_str(), ready_path.wstring().c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
        log_line( NCMM_LOG_ERROR, "Could not atomically publish boot.ready; boot.pending preserved." );
        std::error_code cleanup_ec;
        std::filesystem::remove( temp_path, cleanup_ec );
        return;
    }
#else
    std::error_code publish_ec;
    std::filesystem::remove( ready_path, publish_ec );
    publish_ec.clear();
    std::filesystem::rename( temp_path, ready_path, publish_ec );
    if( publish_ec ) {
        log_line( NCMM_LOG_ERROR, "Could not publish boot.ready; boot.pending preserved." );
        return;
    }
#endif

    std::error_code pending_ec;
    std::filesystem::remove( pending_path, pending_ec );
    if( pending_ec ) {
        log_line( NCMM_LOG_WARN, "boot.ready published but boot.pending could not be removed." );
    }
}

void shutdown()
{
#ifdef _WIN32
    for( auto it = loaded.rbegin(); it != loaded.rend(); ++it ) {
        const std::string module_id = it->descriptor && it->descriptor->id ?
                                      it->descriptor->id : std::string();
        if( it->descriptor && it->descriptor->shutdown ) {
            try {
                module_call_scope scope( module_id.empty() ? nullptr : module_id.c_str() );
                it->descriptor->shutdown();
            } catch( ... ) {
                log_line( NCMM_LOG_WARN,
                          ( "Module shutdown callback failed: " + module_id ).c_str() );
            }
        }
        if( !module_id.empty() ) {
            character_modifier_values.erase( module_id );
        }
        if( it->handle ) {
            FreeLibrary( it->handle );
        }
    }
#endif
    loaded.clear();
    module_states.clear();
    module_ids.clear();
    hotkey_registration_logged.clear();
    manifest_id_counts.clear();
    character_modifier_values.clear();
    active_module_id.clear();
}
} // namespace ncmm
