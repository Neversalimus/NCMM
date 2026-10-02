#include "ncmm_loader.h"
#include "ncmm_item_glyphs.h"
#include "item.h"
#include "item_category.h"
#include "item_location.h"
#include "character.h"
#include "flag.h"
#include "game_inventory.h"
#include "itype.h"
#include "ncmm_api.h"
#include "ncmm_fault_policy.h"
#include "ncmm_manifest_policy.h"
#include "avatar.h"
#include "creature.h"
#include "game.h"
#include "mod_manager.h"
#include "event_bus.h"
#include "event_subscriber.h"
#include "type_id.h"
#include "input.h"
#include "input_context.h"
#include "init.h"
#include "options.h"
#include "output.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "path_info.h"
#include "sounds.h"
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
std::map<std::string, std::string> world_setting_owners;
std::map<std::string, uint32_t> world_setting_scopes;
std::string world_setting_string_cache;

struct module_setting_meta {
    std::string module_id;
    std::string setting_id;
    std::string name;
    std::string tooltip;
    std::string type;
    uint32_t scope = NCMM_WORLD_SETTING_LIVE;
    double min_value = 0.0;
    double max_value = 0.0;
    double step = 1.0;
    std::string default_value;
    std::vector<std::pair<std::string, std::string>> choices;
};
std::vector<module_setting_meta> module_settings;

std::map<std::string, size_t> manifest_id_counts;
std::map<std::string, std::map<std::string, double>> character_modifier_values;
std::map<std::string, double, std::less<>> character_modifier_totals;
std::map<std::string, std::string, std::less<>> modifier_owners_v2;

struct ncmm_event_subscription_v2_internal {
    std::string module_id;
    uint32_t event_id = 0u;
    ncmm_event_callback_v2 callback = nullptr;
    void *user_data = nullptr;
};
struct ncmm_runtime_hook_rule_v2_internal {
    std::string module_id;
    std::string hook_id;
    uint32_t selector_kind = NCMM_SELECTOR_ANY_V2;
    std::string selector_value;
    std::string modifier_id;
};
struct ncmm_worldgen_binding_v2_internal {
    std::string module_id;
    std::string setting_id;
    uint32_t value_type = 0u;
};
std::vector<ncmm_event_subscription_v2_internal> event_subscriptions_v2;
std::vector<ncmm_runtime_hook_rule_v2_internal> runtime_hook_rules_v2;
std::map<std::string, ncmm_worldgen_binding_v2_internal, std::less<>> worldgen_bindings_v2;
std::map<std::string, ncmm_worldgen_binding_v2_internal, std::less<>> runtime_setting_bindings_v2;
thread_local std::string api_v2_string_cache;
thread_local std::string runtime_source_mod_context_v2;
bool api_v2_world_announced = false;

void erase_module_modifiers( const std::string &module_id )
{
    const auto module_it = character_modifier_values.find( module_id );
    if( module_it == character_modifier_values.end() ) {
        return;
    }
    for( const auto &entry : module_it->second ) {
        const auto total_it = character_modifier_totals.find( entry.first );
        if( total_it == character_modifier_totals.end() ) {
            continue;
        }
        total_it->second -= entry.second;
        if( std::abs( total_it->second ) < 1.0e-12 ) {
            character_modifier_totals.erase( total_it );
        }
    }
    character_modifier_values.erase( module_it );
}
std::map<std::string, int64_t> gameplay_metric_values;
character_id gameplay_avatar_id;
bool gameplay_avatar_id_ready = false;
thread_local std::string active_module_id;
thread_local ncmm_ui_theme_v1 active_ui_theme = {
    NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0
};
thread_local const uint32_t *active_ui_border_styles = nullptr;
thread_local size_t active_ui_border_style_count = 0;
thread_local uint32_t active_ui_layout_flags = NCMM_UI_THEME_NONE;

uint32_t ncmm_ui_theme_accent_id( size_t item_index = static_cast<size_t>( -1 ) )
{
    if( item_index != static_cast<size_t>( -1 ) && active_ui_theme.item_accents != nullptr &&
        item_index < active_ui_theme.item_accent_count ) {
        const uint32_t item = active_ui_theme.item_accents[item_index];
        if( item <= NCMM_UI_COLOR_MAGENTA ) {
            return item;
        }
    }
    return active_ui_theme.accent;
}

uint32_t ncmm_ui_border_style_id( size_t item_index = static_cast<size_t>( -1 ) )
{
    if( item_index != static_cast<size_t>( -1 ) && active_ui_border_styles != nullptr &&
        item_index < active_ui_border_style_count ) {
        const uint32_t item = active_ui_border_styles[item_index];
        if( item <= NCMM_UI_BORDER_EXCLUDED ) {
            return item;
        }
    }
    return NCMM_UI_BORDER_AUTO;
}

bool ncmm_ui_theme_enabled( size_t item_index = static_cast<size_t>( -1 ) )
{
    return ncmm_ui_theme_accent_id( item_index ) != NCMM_UI_COLOR_DEFAULT;
}

bool ncmm_ui_horizontal_viewport()
{
    return ( active_ui_layout_flags & NCMM_UI_THEME_HORIZONTAL_VIEWPORT ) != 0u;
}

bool ncmm_ui_sectioned_detail()
{
    return ( active_ui_layout_flags & NCMM_UI_THEME_SECTIONED_DETAIL ) != 0u;
}

nc_color ncmm_ui_theme_accent( size_t item_index = static_cast<size_t>( -1 ) )
{
    switch( ncmm_ui_theme_accent_id( item_index ) ) {
        case NCMM_UI_COLOR_RED: return c_light_red;
        case NCMM_UI_COLOR_GREEN: return c_light_green;
        case NCMM_UI_COLOR_CYAN: return c_light_cyan;
        case NCMM_UI_COLOR_YELLOW: return c_yellow;
        case NCMM_UI_COLOR_BLUE: return c_light_blue;
        case NCMM_UI_COLOR_MAGENTA: return c_pink;
        default: return c_light_gray;
    }
}

class ncmm_ui_theme_scope
{
    public:
        explicit ncmm_ui_theme_scope( const ncmm_ui_theme_v1 *theme ) : previous_( active_ui_theme ),
            previous_border_( active_ui_border_styles ), previous_border_count_( active_ui_border_style_count ),
            previous_layout_flags_( active_ui_layout_flags )
        {
            active_ui_theme = { NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0 };
            active_ui_border_styles = nullptr;
            active_ui_border_style_count = 0;
            active_ui_layout_flags = NCMM_UI_THEME_NONE;
            if( theme != nullptr ) {
                active_ui_theme = *theme;
                if( active_ui_theme.accent > NCMM_UI_COLOR_MAGENTA ) {
                    active_ui_theme.accent = NCMM_UI_COLOR_DEFAULT;
                }
                active_ui_theme.flags &= NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                                         NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL;
                active_ui_layout_flags = active_ui_theme.flags;
            }
        }

        ncmm_ui_theme_scope( const ncmm_ui_theme_scope & ) = delete;
        ncmm_ui_theme_scope &operator=( const ncmm_ui_theme_scope & ) = delete;

        ~ncmm_ui_theme_scope()
        {
            active_ui_theme = previous_;
            active_ui_border_styles = previous_border_;
            active_ui_border_style_count = previous_border_count_;
            active_ui_layout_flags = previous_layout_flags_;
        }

    private:
        ncmm_ui_theme_v1 previous_;
        const uint32_t *previous_border_;
        size_t previous_border_count_;
        uint32_t previous_layout_flags_;
};

class ncmm_ui_rpg_theme_scope
{
    public:
        explicit ncmm_ui_rpg_theme_scope( const ncmm_ui_theme_ex_v1 *theme ) : previous_( active_ui_theme ),
            previous_border_( active_ui_border_styles ), previous_border_count_( active_ui_border_style_count ),
            previous_layout_flags_( active_ui_layout_flags )
        {
            active_ui_theme = { NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0 };
            active_ui_border_styles = nullptr;
            active_ui_border_style_count = 0;
            active_ui_layout_flags = NCMM_UI_THEME_NONE;
            if( theme != nullptr ) {
                active_ui_theme = { theme->accent, theme->flags, theme->preferred_node_width,
                                    theme->preferred_detail_width, theme->item_accents,
                                    theme->item_accent_count };
                if( active_ui_theme.accent > NCMM_UI_COLOR_MAGENTA ) {
                    active_ui_theme.accent = NCMM_UI_COLOR_DEFAULT;
                }
                active_ui_theme.flags &= NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                                         NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL;
                active_ui_layout_flags = active_ui_theme.flags;
                active_ui_border_styles = theme->item_border_styles;
                active_ui_border_style_count = theme->item_border_style_count;
            }
        }

        ncmm_ui_rpg_theme_scope( const ncmm_ui_rpg_theme_scope & ) = delete;
        ncmm_ui_rpg_theme_scope &operator=( const ncmm_ui_rpg_theme_scope & ) = delete;

        ~ncmm_ui_rpg_theme_scope()
        {
            active_ui_theme = previous_;
            active_ui_border_styles = previous_border_;
            active_ui_border_style_count = previous_border_count_;
            active_ui_layout_flags = previous_layout_flags_;
        }

    private:
        ncmm_ui_theme_v1 previous_;
        const uint32_t *previous_border_;
        size_t previous_border_count_;
        uint32_t previous_layout_flags_;
};
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

std::map<std::string, std::pair<double, double>, std::less<>> character_modifier_limits = {
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
    { "craft_speed_pct", { -90.0, 500.0 } },
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
    "active_mods.v1",
    "active_mods.registry.v2",
    "world_settings.v2",
    "world_options.experimental.v1",
    "ui.theme.v1",
    "ui.layout.v1",
    "module_hotkeys.context.v1",
    "module_hotkeys.v1",
    "ingame_manager.v1",
    "world_options.layout.v1",
    "character.modifiers.v1",
    "api.versioning.v1",
    "module.lifecycle.query.v2",
    "worldgen.bindings.v2",
    "runtime_settings.bindings.v2",
    "character.virtual_items.v1",
    "runtime_hooks.registry.v2",
    "character.modifiers.v2",
    "settings.typed.v2",
    "events.core.v2",
    "host_api.v2.core",
    "state.migration.v1",
    "module.lifecycle.v1"
};

std::filesystem::path game_root()
{
    return std::filesystem::current_path();
}

void ncmm_trim_and_print_literal( const catacurses::window &w, const point &begin,
                                  int width, const nc_color &base_color,
                                  const std::string &text )
{
    // The std::string mvwprintz overload is literal-safe. Do not pre-escape '%':
    // doing so visibly produced "100%%" in Survivor 0.9.7.
    const std::string clipped = trim_by_length( text, width );
    mvwprintz( w, begin, base_color, clipped );
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
    return "0.8.2";
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

int world_mod_active( const char *requested_mod_id )
{
    if( requested_mod_id == nullptr || requested_mod_id[0] == '\0' ||
        world_generator == nullptr || world_generator->active_world == nullptr ) {
        return 0;
    }

    // Compare mod_id values directly. type_id.h only forward-declares
    // MOD_INFORMATION; dereferencing mod_id here is unnecessary.
    const mod_id wanted( requested_mod_id );
    for( const mod_id &mod : world_generator->active_world->active_mod_order ) {
        if( mod == wanted ) {
            return 1;
        }
    }
    return 0;
}
// Defined later in the loader; World Settings v2 is injected before that definition.
bool active_module_matches( const char *module_id );

bool safe_world_setting_id( const char *value )
{
    if( value == nullptr ) {
        return false;
    }
    const std::string text( value );
    if( text.size() < 6 || text.size() > 80 || text.rfind( "NCMM_", 0 ) != 0 ) {
        return false;
    }
    for( unsigned char c : text ) {
        if( !( std::isupper( c ) || std::isdigit( c ) || c == '_' ) ) {
            return false;
        }
    }
    return true;
}

bool claim_world_setting( const char *module_id, const char *setting_id, uint32_t scope )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !safe_world_setting_id( setting_id ) || scope > NCMM_WORLD_SETTING_NEW_WORLD ) {
        return false;
    }
    const std::string id( setting_id );
    const auto owner = world_setting_owners.find( id );
    if( owner != world_setting_owners.end() && owner->second != module_id ) {
        return false;
    }
    world_setting_owners[id] = module_id;
    world_setting_scopes[id] = scope;
    return true;
}

void remember_module_setting( const module_setting_meta &meta )
{
    auto existing = std::find_if( module_settings.begin(), module_settings.end(),
    [&]( const module_setting_meta &entry ) {
        return entry.module_id == meta.module_id && entry.setting_id == meta.setting_id;
    } );
    if( existing != module_settings.end() ) {
        *existing = meta;
    } else {
        module_settings.push_back( meta );
    }
}

bool manager_visible_setting_scope( uint32_t scope )
{
    return scope == NCMM_WORLD_SETTING_LIVE || scope == NCMM_WORLD_SETTING_RELOAD;
}

int world_setting_register_bool( const char *module_id, const char *setting_id,
                                 const char *display_name, const char *tooltip,
                                 int default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    const int registered = get_options().ncmm_register_world_bool(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), default_value != 0,
                               scope >= NCMM_WORLD_SETTING_NEW_MAP ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "bool";
        meta.scope = scope;
        meta.default_value = default_value != 0 ? "true" : "false";
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_register_int( const char *module_id, const char *setting_id,
                                const char *display_name, const char *tooltip,
                                int min_value, int max_value, int default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    const int registered = get_options().ncmm_register_world_int(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), min_value, max_value, default_value,
                               scope >= NCMM_WORLD_SETTING_NEW_MAP ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "int";
        meta.scope = scope;
        meta.min_value = min_value;
        meta.max_value = max_value;
        meta.step = 1.0;
        meta.default_value = std::to_string( default_value );
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_register_float( const char *module_id, const char *setting_id,
                                  const char *display_name, const char *tooltip,
                                  double min_value, double max_value, double default_value,
                                  double step, uint32_t scope )
{
    if( !display_name || !tooltip || !std::isfinite( min_value ) || !std::isfinite( max_value ) ||
        !std::isfinite( default_value ) || !std::isfinite( step ) ||
        !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    const int registered = get_options().ncmm_register_world_float(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), static_cast<float>( min_value ),
                               static_cast<float>( max_value ), static_cast<float>( default_value ),
                               static_cast<float>( step ),
                               scope >= NCMM_WORLD_SETTING_NEW_MAP ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "float";
        meta.scope = scope;
        meta.min_value = min_value;
        meta.max_value = max_value;
        meta.step = step;
        std::ostringstream default_text;
        default_text << default_value;
        meta.default_value = default_text.str();
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_register_enum( const char *module_id, const char *setting_id,
                                 const char *display_name, const char *tooltip,
                                 const char *const *value_ids, const char *const *display_names,
                                 size_t count, const char *default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !value_ids || !display_names || !default_value ||
        count == 0 || count > 64 || !claim_world_setting( module_id, setting_id, scope ) ) {
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
    const int registered = get_options().ncmm_register_world_enum(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), items, default_value,
                               scope >= NCMM_WORLD_SETTING_NEW_MAP ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "enum";
        meta.scope = scope;
        meta.default_value = default_value;
        for( size_t i = 0; i < count; ++i ) {
            meta.choices.emplace_back( value_ids[i], display_names[i] );
        }
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_get_bool( const char *setting_id, int fallback )
{
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return fallback;
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "bool" ) {
        return fallback;
    }
    return opt.value_as<bool>() ? 1 : 0;
}

int64_t world_setting_get_i64( const char *setting_id, int64_t fallback )
{
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return fallback;
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "int" && opt.getType() != "int_map" ) {
        return fallback;
    }
    return static_cast<int64_t>( opt.value_as<int>() );
}

double world_setting_get_f64( const char *setting_id, double fallback )
{
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return fallback;
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "float" ) {
        return fallback;
    }
    return static_cast<double>( opt.value_as<float>() );
}

const char *world_setting_get_string( const char *setting_id, const char *fallback )
{
    world_setting_string_cache = fallback ? fallback : "";
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return world_setting_string_cache.c_str();
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "string_select" && opt.getType() != "string_input" && opt.getType() != "string" ) {
        return world_setting_string_cache.c_str();
    }
    world_setting_string_cache = opt.value_as<std::string>();
    return world_setting_string_cache.c_str();
}
size_t world_mod_count()
{
    if( world_generator == nullptr || world_generator->active_world == nullptr ) return 0;
    return world_generator->active_world->active_mod_order.size();
}

const char *world_mod_id( size_t index )
{
    static thread_local std::string id_cache;
    id_cache.clear();
    if( world_generator == nullptr || world_generator->active_world == nullptr ) return nullptr;
    const auto &mods = world_generator->active_world->active_mod_order;
    if( index >= mods.size() ) return nullptr;
    id_cache = mods[index].str();
    return id_cache.c_str();
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

int worldgen_experimental_group_begin( const char *group_id, const char *display_name,
                                       const char *tooltip )
{
    if( !group_id || !display_name || !tooltip ) {
        return 0;
    }
    return get_options().ncmm_begin_experimental_group(
               group_id, to_translation( display_name ), to_translation( tooltip ) ) ? 1 : 0;
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
    return g != nullptr && !g->new_game && world_generator != nullptr &&
           world_generator->active_world != nullptr ? 1 : 0;
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

constexpr const char *virtual_item_marker_key = "ncmm_virtual_slot";

bool safe_virtual_slot_id( const char *slot_id )
{
    return safe_state_token( slot_id ) && std::string( slot_id ).size() <= 48;
}

std::string virtual_item_marker( const char *module_id, const char *slot_id )
{
    return std::string( module_id ) + ":" + slot_id;
}

std::string virtual_item_state_key( const char *slot_id )
{
    return "vslot_" + std::string( slot_id );
}

int64_t virtual_item_state_uid_internal( const char *module_id, const char *slot_id )
{
    if( !character_state_available() || !safe_state_token( module_id ) ||
        !safe_virtual_slot_id( slot_id ) ) {
        return 0;
    }
    const auto &values = get_avatar().get_values();
    const auto it = values.find( character_state_key( module_id,
                                 virtual_item_state_key( slot_id ).c_str() ) );
    if( it == values.end() || !it->second.is_str() ) {
        return 0;
    }
    try {
        std::size_t consumed = 0;
        const std::string &raw = it->second.str();
        const long long parsed = std::stoll( raw, &consumed, 10 );
        return consumed == raw.size() ? std::max<int64_t>( 0, parsed ) : 0;
    } catch( ... ) {
        return 0;
    }
}

void virtual_item_state_set_uid_internal( const char *module_id, const char *slot_id, int64_t uid )
{
    if( !character_state_available() || !safe_state_token( module_id ) ||
        !safe_virtual_slot_id( slot_id ) ) {
        return;
    }
    get_avatar().get_values()[character_state_key(
        module_id, virtual_item_state_key( slot_id ).c_str() )] =
            diag_value( std::to_string( std::max<int64_t>( 0, uid ) ) );
}

item *virtual_item_for_slot_internal( const char *module_id, const char *slot_id )
{
    if( !character_state_available() || !safe_state_token( module_id ) ||
        !safe_virtual_slot_id( slot_id ) || module_ids.count( module_id ) == 0 ) {
        return nullptr;
    }

    const std::string wanted_marker = virtual_item_marker( module_id, slot_id );
    const int64_t wanted_uid = virtual_item_state_uid_internal( module_id, slot_id );
    item *uid_match = nullptr;
    std::vector<item *> marker_matches;

    for( item_location loc : get_avatar().all_items_loc() ) {
        item *candidate = loc.get_item();
        if( candidate == nullptr || candidate->is_null() ) {
            continue;
        }
        const std::string marker = candidate->get_var( virtual_item_marker_key, "" );
        if( wanted_uid > 0 && candidate->uid().get_value() == wanted_uid ) {
            uid_match = candidate;
        }
        if( marker == wanted_marker ) {
            marker_matches.push_back( candidate );
        }
    }

    if( uid_match != nullptr ) {
        const std::string marker = uid_match->get_var( virtual_item_marker_key, "" );
        if( marker.empty() ) {
            uid_match->set_var( virtual_item_marker_key, wanted_marker );
        } else if( marker != wanted_marker ) {
            virtual_item_state_set_uid_internal( module_id, slot_id, 0 );
            return nullptr;
        }
        for( item *duplicate : marker_matches ) {
            if( duplicate != uid_match ) {
                duplicate->erase_var( virtual_item_marker_key );
            }
        }
        return uid_match;
    }

    if( marker_matches.size() == 1 ) {
        item *resolved = marker_matches.front();
        virtual_item_state_set_uid_internal( module_id, slot_id, resolved->uid().get_value() );
        return resolved;
    }

    if( marker_matches.size() > 1 ) {
        for( item *duplicate : marker_matches ) {
            duplicate->erase_var( virtual_item_marker_key );
        }
    }
    if( wanted_uid != 0 || !marker_matches.empty() ) {
        virtual_item_state_set_uid_internal( module_id, slot_id, 0 );
    }
    return nullptr;
}

void virtual_item_clear_internal( const char *module_id, const char *slot_id )
{
    if( !character_state_available() || !safe_state_token( module_id ) ||
        !safe_virtual_slot_id( slot_id ) ) {
        return;
    }
    const std::string wanted_marker = virtual_item_marker( module_id, slot_id );
    for( item_location loc : get_avatar().all_items_loc() ) {
        item *candidate = loc.get_item();
        if( candidate != nullptr &&
            candidate->get_var( virtual_item_marker_key, "" ) == wanted_marker ) {
            candidate->erase_var( virtual_item_marker_key );
        }
    }
    virtual_item_state_set_uid_internal( module_id, slot_id, 0 );
}

int virtual_item_choose_v2( const char *module_id, const char *slot_id,
                            const char *title, uint32_t flags )
{
    if( !active_module_matches( module_id ) || !safe_virtual_slot_id( slot_id ) ||
        title == nullptr || !character_state_available() ) {
        return 0;
    }

    avatar &you = get_avatar();
    item_location chosen = game_menus::inv::titled_filter_menu(
        [&]( const item_location & loc ) {
            if( !loc || !loc.held_by( you ) ) {
                return false;
            }
            const item &candidate = *loc;
            if( candidate.is_null() || candidate.has_flag( flag_INTEGRATED ) ||
                candidate.has_flag( flag_PSEUDO ) ) {
                return false;
            }
            if( ( flags & NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2 ) != 0u &&
                candidate.count_by_charges() ) {
                return false;
            }
            if( ( flags & NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2 ) != 0u &&
                ( candidate.made_of( phase_id::LIQUID ) ||
                  candidate.made_of( phase_id::GAS ) ) ) {
                return false;
            }
            return true;
        }, you, title, -1,
        tr_ui( "No eligible carried item is available.",
               "Нет подходящего предмета у персонажа." ) );

    if( !chosen ) {
        return 0;
    }

    item *selected = chosen.get_item();
    if( selected == nullptr ) {
        return 0;
    }

    const std::string new_marker = virtual_item_marker( module_id, slot_id );
    const std::string previous_marker = selected->get_var( virtual_item_marker_key, "" );
    const std::string module_prefix = std::string( module_id ) + ":";
    if( previous_marker.rfind( module_prefix, 0 ) == 0 && previous_marker != new_marker ) {
        const std::string old_slot = previous_marker.substr( module_prefix.size() );
        if( safe_virtual_slot_id( old_slot.c_str() ) ) {
            virtual_item_state_set_uid_internal( module_id, old_slot.c_str(), 0 );
        }
    }

    virtual_item_clear_internal( module_id, slot_id );
    selected->set_var( virtual_item_marker_key, new_marker );
    virtual_item_state_set_uid_internal( module_id, slot_id, selected->uid().get_value() );
    return 1;
}

int virtual_item_clear_v2( const char *module_id, const char *slot_id )
{
    if( !active_module_matches( module_id ) || !safe_virtual_slot_id( slot_id ) ) {
        return 0;
    }
    virtual_item_clear_internal( module_id, slot_id );
    return 1;
}

const char *virtual_item_name_v2( const char *module_id, const char *slot_id )
{
    if( !active_module_matches( module_id ) || !safe_virtual_slot_id( slot_id ) ) {
        return "";
    }
    item *bound = virtual_item_for_slot_internal( module_id, slot_id );
    api_v2_string_cache = bound != nullptr ? bound->display_name() : "";
    return api_v2_string_cache.c_str();
}

int64_t virtual_item_uid_v2( const char *module_id, const char *slot_id )
{
    if( !active_module_matches( module_id ) || !safe_virtual_slot_id( slot_id ) ) {
        return 0;
    }
    item *bound = virtual_item_for_slot_internal( module_id, slot_id );
    return bound != nullptr ? bound->uid().get_value() : 0;
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
        const int previous_selected = selected;
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
        if( selected != previous_selected ) {
            sfx::play_variant_sound( "menu_move", "default", 100 );
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
    constexpr int card_height = 8;
    constexpr int header_height = 6;
    constexpr int footer_height = 2;
    const bool detail_panel = ncmm_ui_sectioned_detail() && TERMX >= 108;
    const int requested_card_detail_width = active_ui_theme.preferred_detail_width > 0 ?
                                            active_ui_theme.preferred_detail_width : 40;
    const int card_detail_width = detail_panel ?
                                  std::clamp( requested_card_detail_width, 34, 48 ) : 0;
    const int card_detail_reserve = detail_panel ? card_detail_width + 1 : 0;

    while( columns > 1 ) {
        const int candidate = ( TERMX - 4 - card_detail_reserve - gap * ( columns - 1 ) ) / columns;
        if( candidate >= 28 ) {
            break;
        }
        --columns;
    }

    const int card_width = std::max( 28, std::min( 46,
                           ( TERMX - 4 - card_detail_reserve - gap * ( columns - 1 ) ) / columns ) );
    const int frame_width = columns * card_width + gap * ( columns - 1 ) + 2 + card_detail_reserve;
    const int total_rows = ( static_cast<int>( count ) + columns - 1 ) / columns;
    const int max_frame_height = std::max( header_height + card_height + footer_height,
                                          TERMY - 2 );
    const int visible_rows = std::max( 1, std::min( total_rows,
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
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "MOUSE_MOVE" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );
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
        first_row = std::max( 0, std::min( first_row,
                         std::max( 0, total_rows - visible_rows ) ) );
    };

    auto card_at = [&]( const point &p ) -> int {
        if( p.y < header_height || p.y >= header_height + visible_rows * card_height ||
            p.x < 1 ) {
            return -1;
        }
        const int local_x = p.x - 1;
        const int slot_span = card_width + gap;
        const int col = local_x / slot_span;
        const int inside_x = local_x % slot_span;
        const int row = ( p.y - header_height ) / card_height;
        if( col < 0 || col >= columns || row < 0 || row >= visible_rows ||
            inside_x < 0 || inside_x >= card_width ) {
            return -1;
        }
        const int index = ( first_row + row ) * columns + col;
        return index >= 0 && index < static_cast<int>( count ) ? index : -1;
    };

    ui_adaptor ui;
    ui.position_from_window( frame );
    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( frame );
        draw_border( frame, BORDER_COLOR );
        ncmm_trim_and_print_literal( frame, point( 2, 1 ), frame_width - 4,
                                    ncmm_ui_theme_enabled() ? ncmm_ui_theme_accent() : c_white, title );

        if( summary != nullptr && summary[0] != '\0' ) {
            const std::vector<std::string> lines = foldstring( summary, frame_width - 4 );
            for( size_t i = 0; i < std::min<size_t>( 2, lines.size() ); ++i ) {
                ncmm_trim_and_print_literal( frame, point( 2, 2 + static_cast<int>( i ) ),
                                            frame_width - 4, c_light_gray, lines[i] );
            }
        }

        if( progress != nullptr && progress->maximum > 0 ) {
            const int64_t maximum = std::max<int64_t>( 1, progress->maximum );
            const int64_t current = std::max<int64_t>( 0, std::min( progress->current, maximum ) );
            const int percent = static_cast<int>( std::llround(
                static_cast<long double>( current ) * 100.0L /
                static_cast<long double>( maximum ) ) );
            std::string line = progress->label ? progress->label : "";
            if( !line.empty() ) {
                line += "  ";
            }
            const int bar_width = 18;
            const int filled = std::max( 0, std::min( bar_width,
                               static_cast<int>( std::llround( percent * bar_width / 100.0 ) ) ) );
            line += "[" + std::string( static_cast<size_t>( filled ), '#' ) +
                    std::string( static_cast<size_t>( bar_width - filled ), '-' ) + "] " +
                    std::to_string( percent ) + "%";
            ncmm_trim_and_print_literal( frame, point( 2, 4 ), frame_width - 4,
                                        ncmm_ui_theme_enabled() ? ncmm_ui_theme_accent() : c_light_green, line );
        }

        std::string footer = tr_ui(
            "Mouse: hover/click/wheel  Tab: tree  Enter: open  Esc: back",
            "Мышь: наведение/клик/колесо  Tab: дерево  Enter: открыть  Esc: назад" );
        footer += "  " + std::to_string( selected + 1 ) + "/" + std::to_string( count );
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ),
                                    frame_width - 4, c_dark_gray, footer );

        if( detail_panel ) {
            const int divider_x = 1 + columns * card_width + gap * ( columns - 1 );
            for( int y = header_height - 1; y < frame_height - footer_height; ++y ) {
                mvwaddch( frame, point( divider_x, y ), LINE_XOXO );
            }
            ncmm_trim_and_print_literal( frame, point( divider_x + 2, header_height - 1 ),
                                        card_detail_width - 3, c_dark_gray,
                                        tr_ui( "DETAIL", "ДЕТАЛИ" ) );

            const ncmm_ui_card_v1 &detail = cards[selected];
            const int dx = divider_x + 2;
            int dy = header_height;
            const nc_color detail_accent = ncmm_ui_theme_enabled( static_cast<size_t>( selected ) ) ?
                                           ncmm_ui_theme_accent( static_cast<size_t>( selected ) ) : c_white;
            const std::vector<std::string> detail_title =
                foldstring( detail.title ? detail.title : "", card_detail_width - 3 );
            for( size_t line = 0; line < std::min<size_t>( 2, detail_title.size() ); ++line ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy++ ), card_detail_width - 3,
                                            detail_accent, detail_title[line] );
            }
            if( detail.subtitle && detail.subtitle[0] != '\0' ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy++ ), card_detail_width - 3,
                                            c_light_gray, detail.subtitle );
            }
            if( detail.badge && detail.badge[0] != '\0' ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy++ ), card_detail_width - 3,
                                            detail_accent, detail.badge );
            }
            ++dy;
            if( detail.body && detail.body[0] != '\0' ) {
                const std::vector<std::string> folded = foldstring( detail.body, card_detail_width - 3 );
                const int max_lines = std::max( 1, frame_height - footer_height - dy - 1 );
                for( int line = 0; line < std::min<int>( max_lines, folded.size() ); ++line ) {
                    nc_color detail_color = c_light_gray;
                    if( folded[line] == "HOW TO GAIN XP:" || folded[line] == "КАК КАЧАТЬ:" ||
                        folded[line] == "PROGRESSION:" || folded[line] == "ПРОГРЕСС:" ) {
                        detail_color = c_light_green;
                    } else if( folded[line] == "EFFICIENCY:" || folded[line] == "ЭФФЕКТИВНОСТЬ:" ) {
                        detail_color = c_yellow;
                    }
                    ncmm_trim_and_print_literal( frame, point( dx, dy + line ), card_detail_width - 3,
                                                detail_color, folded[line] );
                }
            }
        }

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

            const bool themed = ncmm_ui_theme_enabled( static_cast<size_t>( index ) );
            const uint32_t border_style = ncmm_ui_border_style_id( static_cast<size_t>( index ) );
            const nc_color theme_accent = ncmm_ui_theme_accent( static_cast<size_t>( index ) );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                    border_style == NCMM_UI_BORDER_MAJOR ? c_yellow :
                                    border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    accent ? c_light_blue :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color title_color = locked || border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                          active ? c_white :
                                          border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                          themed ? theme_accent : c_light_gray;
            draw_border( card_win, border );

            ncmm_trim_and_print_literal( card_win, point( 2, 1 ), card_width - 4,
                                        title_color, card.title ? card.title : "" );
            if( card.subtitle != nullptr && card.subtitle[0] != '\0' ) {
                ncmm_trim_and_print_literal( card_win, point( 2, 2 ), card_width - 4,
                                            locked ? c_dark_gray : c_light_gray, card.subtitle );
            }
            if( card.body != nullptr && card.body[0] != '\0' ) {
                const std::vector<std::string> folded = foldstring( card.body, card_width - 4 );
                const size_t card_body_lines = detail_panel ? 2 : 3;
                for( size_t line = 0; line < std::min<size_t>( card_body_lines, folded.size() ); ++line ) {
                    ncmm_trim_and_print_literal( card_win,
                                                point( 2, 3 + static_cast<int>( line ) ),
                                                card_width - 4,
                                                locked ? c_dark_gray :
                                                active ? c_cyan : c_light_gray,
                                                folded[line] );
                }
            }
            if( card.badge != nullptr && card.badge[0] != '\0' ) {
                ncmm_trim_and_print_literal( card_win, point( 2, card_height - 2 ),
                                            card_width - 4,
                                            owned ? c_cyan :
                                            major ? c_yellow :
                                            effect ? c_magenta :
                                            locked ? c_dark_gray : c_green,
                                            card.badge );
            }
            const char *marker = active ? ">" :
                                 border_style == NCMM_UI_BORDER_PRIME ? "*" :
                                 border_style == NCMM_UI_BORDER_MAJOR ? "+" :
                                 border_style == NCMM_UI_BORDER_EXCLUDED ? "x" :
                                 ( themed && ( active_ui_layout_flags & NCMM_UI_THEME_STRONG_BORDER ) ? "*" : nullptr );
            if( marker != nullptr ) {
                mvwprintz( card_win, point( 1, 1 ),
                           border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                           ( themed ? theme_accent : c_light_green ), marker );
            }
            wnoutrefresh( card_win );
        }
    } );

    while( true ) {
        keep_visible();
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        const int previous_selected = selected;

        if( action == "MOUSE_MOVE" || action == "SELECT" ) {
            const std::optional<point> mouse = ctxt.get_coordinates_text( frame );
            if( mouse ) {
                const int hit = card_at( *mouse );
                if( hit >= 0 ) {
                    selected = hit;
                    if( selected != previous_selected ) {
                        sfx::play_variant_sound( "menu_move", "default", 100 );
                    }
                    if( action == "SELECT" ) {
                        return selected;
                    }
                }
            }
            continue;
        }

        const int col = selected % columns;
        const int row = selected / columns;

        if( action == "LEFT" ) {
            if( col > 0 ) --selected;
        } else if( action == "RIGHT" ) {
            if( col + 1 < columns && selected + 1 < static_cast<int>( count ) ) ++selected;
        } else if( action == "UP" || action == "SCROLL_UP" ) {
            if( row > 0 ) selected = std::max( 0, selected - columns );
        } else if( action == "DOWN" || action == "SCROLL_DOWN" ) {
            const int next = selected + columns;
            if( next < static_cast<int>( count ) ) selected = next;
        } else if( action == "PAGE_UP" ) {
            selected = std::max( 0, selected - visible_rows * columns );
        } else if( action == "PAGE_DOWN" ) {
            selected = std::min( static_cast<int>( count ) - 1,
                                 selected + visible_rows * columns );
        } else if( action == "HOME" ) {
            selected = 0;
        } else if( action == "END" ) {
            selected = static_cast<int>( count ) - 1;
        } else if( action == "NEXT_TAB" ) {
            return NCMM_UI_CARD_SHOW_TREE;
        } else if( action == "CONFIRM" ) {
            return selected;
        } else if( action == "QUIT" ) {
            return -1;
        }
        if( selected != previous_selected ) {
            sfx::play_variant_sound( "menu_move", "default", 100 );
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
    for( size_t i = 0; i < node_count; ++i ) {
        if( nodes[i].title == nullptr || nodes[i].row < 0 || nodes[i].column < 0 ||
            nodes[i].row > 31 || nodes[i].column > 7 ) {
            return NCMM_UI_TREE_CANCEL;
        }
        max_row = std::max( max_row, nodes[i].row );
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
            if( ncmm_ui_border_style_id( i ) == NCMM_UI_BORDER_PRIME ) {
                continue;
            }
            int parent_count = 0;
            int parent_min = 1000000;
            int parent_max = -1000000;
            size_t single_parent = 0;
            for( size_t e = 0; e < edge_count; ++e ) {
                if( edges[e].to_index != i ) continue;
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

    // NCMM HOTFIX13: keep every multi-node row physically non-overlapping after routing.
    // Singleton rows retain parent-centering; siblings use declared lanes with a one-lane minimum gap.
    std::map<int, std::vector<size_t>> ncmm_row_nodes;
    for( size_t i = 0; i < node_count; ++i ) {
        ncmm_row_nodes[nodes[i].row].push_back( i );
    }
    for( auto &row_entry : ncmm_row_nodes ) {
        std::vector<size_t> &row_nodes = row_entry.second;
        if( row_nodes.size() < 2 ) {
            continue;
        }
        std::stable_sort( row_nodes.begin(), row_nodes.end(), [&]( size_t lhs, size_t rhs ) {
            if( nodes[lhs].column != nodes[rhs].column ) {
                return nodes[lhs].column < nodes[rhs].column;
            }
            return lhs < rhs;
        } );
        int next_x2 = nodes[row_nodes.front()].column * 2;
        for( size_t index : row_nodes ) {
            const int declared_x2 = nodes[index].column * 2;
            next_x2 = std::max( next_x2, declared_x2 );
            layout_x2[index] = next_x2;
            next_x2 += 2;
        }
    }

    int max_layout_x2 = 0;
    for( int x2 : layout_x2 ) max_layout_x2 = std::max( max_layout_x2, x2 );

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

    const int requested_node_width = active_ui_theme.preferred_node_width > 0 ?
                                     active_ui_theme.preferred_node_width :
                                     ( ( active_ui_theme.flags & NCMM_UI_THEME_WIDE_NODES ) ? 30 : 24 );
    const int requested_detail_width = active_ui_theme.preferred_detail_width > 0 ?
                                       active_ui_theme.preferred_detail_width : 42;
    int node_width = std::clamp( requested_node_width, 24, 34 );
    constexpr int node_height = 6;
    constexpr int hgap = 2;
    constexpr int vgap = 1;
    constexpr int header_height = 6;
    constexpr int footer_height = 2;
    int detail_width = std::clamp( requested_detail_width, 38, 54 );

    // Preserve every logical tree column before spending horizontal space on
    // cosmetic width.  Large displays get the requested 30-char Survivor
    // nodes; narrower terminals automatically step down to the proven 24-char
    // geometry instead of silently clipping the right-hand branch.
    const auto required_tree_width = [&]( int candidate_node, int candidate_detail ) {
        const int candidate_lane = candidate_node + hgap;
        const int candidate_tree = candidate_node +
                                   ( max_layout_x2 * candidate_lane + 1 ) / 2;
        return candidate_tree + candidate_detail + 7;
    };
    while( detail_width > 38 && required_tree_width( node_width, detail_width ) > TERMX - 2 ) {
        --detail_width;
    }
    while( node_width > 24 && required_tree_width( node_width, detail_width ) > TERMX - 2 ) {
        --node_width;
    }

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
    ctxt.register_action( "MOUSE_MOVE" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    int selected = 0;
    int first_row = 0;
    int first_x = 0;
    const int max_first_x = std::max( 0, logical_tree_width - canvas_width + 1 );

    auto logical_node_x = [&]( size_t i ) {
        return 2 + ( layout_x2[i] * lane_step + 1 ) / 2;
    };

    auto keep_visible = [&]() {
        const int row = nodes[selected].row;
        if( row < first_row ) first_row = row;
        else if( row >= first_row + visible_rows ) first_row = row - visible_rows + 1;
        first_row = std::max( 0, std::min( first_row,
                         std::max( 0, max_row - visible_rows + 1 ) ) );

        if( ncmm_ui_horizontal_viewport() ) {
            int row_left = 1000000;
            int row_right = -1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( nodes[i].row != row ) continue;
                const int candidate_left = logical_node_x( i );
                row_left = std::min( row_left, candidate_left );
                row_right = std::max( row_right, candidate_left + node_width - 1 );
            }

            // If the entire logical row fits, show it as a group.  This is crucial
            // for the three-way Prime choice: all alternatives remain visible at once.
            if( row_left <= row_right && row_right - row_left + 1 < canvas_width ) {
                first_x = row_left - 2;
            } else {
                const int left = logical_node_x( static_cast<size_t>( selected ) );
                const int right = left + node_width - 1;
                if( left - first_x < 2 ) {
                    first_x = left - 2;
                } else if( right - first_x >= canvas_width + 2 ) {
                    first_x = right - canvas_width;
                }
            }
            first_x = std::max( 0, std::min( first_x, max_first_x ) );
        } else {
            first_x = 0;
        }
    };

    auto node_x = [&]( size_t i ) {
        return logical_node_x( i ) - first_x;
    };
    auto node_y = [&]( size_t i ) {
        return header_height + ( nodes[i].row - first_row ) * ( node_height + vgap );
    };
    auto visible = [&]( size_t i ) {
        const int x = node_x( i );
        return nodes[i].row >= first_row && nodes[i].row < first_row + visible_rows &&
               x >= 2 && x + node_width < canvas_width + 2;
    };
    auto node_at = [&]( const point &p ) -> int {
        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) continue;
            const int x = node_x( i );
            const int y = node_y( i );
            if( p.x >= x && p.x < x + node_width &&
                p.y >= y && p.y < y + node_height ) {
                return static_cast<int>( i );
            }
        }
        return -1;
    };

    // v7: logical navigation follows the declared tree grid instead of the
    // post-routing layout_x2.  This prevents multi-row jumps and makes every
    // tile on an occupied row reachable by arrows.
    auto select_direction = [&]( int row_sign, int col_sign ) {
        const int sr = nodes[selected].row;
        const int sc = nodes[selected].column;
        int best = -1;

        if( row_sign != 0 ) {
            int nearest_row_delta = 1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( static_cast<int>( i ) == selected ) {
                    continue;
                }
                const int dr = nodes[i].row - sr;
                if( ( row_sign < 0 && dr >= 0 ) || ( row_sign > 0 && dr <= 0 ) ) {
                    continue;
                }
                nearest_row_delta = std::min( nearest_row_delta, std::abs( dr ) );
            }
            if( nearest_row_delta == 1000000 ) {
                return;
            }

            int best_col_delta = 1000000;
            int best_declared_col = 1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( static_cast<int>( i ) == selected ) {
                    continue;
                }
                const int dr = nodes[i].row - sr;
                if( ( row_sign < 0 && dr >= 0 ) || ( row_sign > 0 && dr <= 0 ) ||
                    std::abs( dr ) != nearest_row_delta ) {
                    continue;
                }
                const int col_delta = std::abs( nodes[i].column - sc );
                if( col_delta < best_col_delta ||
                    ( col_delta == best_col_delta && nodes[i].column < best_declared_col ) ) {
                    best_col_delta = col_delta;
                    best_declared_col = nodes[i].column;
                    best = static_cast<int>( i );
                }
            }
        } else if( col_sign != 0 ) {
            int nearest_col_delta = 1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( static_cast<int>( i ) == selected || nodes[i].row != sr ) {
                    continue;
                }
                const int dc = nodes[i].column - sc;
                if( ( col_sign < 0 && dc >= 0 ) || ( col_sign > 0 && dc <= 0 ) ) {
                    continue;
                }
                const int col_delta = std::abs( dc );
                if( col_delta < nearest_col_delta ) {
                    nearest_col_delta = col_delta;
                    best = static_cast<int>( i );
                }
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
        ncmm_trim_and_print_literal( frame, point( 2, 1 ), frame_width - 4,
                                    ncmm_ui_theme_enabled() ? ncmm_ui_theme_accent() : c_white, title );

        if( summary != nullptr && summary[0] != '\0' ) {
            const std::vector<std::string> lines = foldstring( summary, frame_width - 4 );
            for( size_t i = 0; i < std::min<size_t>( 2, lines.size() ); ++i ) {
                ncmm_trim_and_print_literal( frame, point( 2, 2 + static_cast<int>( i ) ),
                                            frame_width - 4, c_light_gray, lines[i] );
            }
        }

        if( progress != nullptr && progress->maximum > 0 ) {
            const int64_t maximum = std::max<int64_t>( 1, progress->maximum );
            const int64_t current = std::max<int64_t>( 0, std::min( progress->current, maximum ) );
            const int percent = static_cast<int>( std::llround(
                static_cast<long double>( current ) * 100.0L /
                static_cast<long double>( maximum ) ) );
            std::string line = progress->label ? progress->label : "";
            if( !line.empty() ) line += "  ";
            const int bar_width = 18;
            const int filled = std::max( 0, std::min( bar_width,
                               static_cast<int>( std::llround( percent * bar_width / 100.0 ) ) ) );
            line += "[" + std::string( static_cast<size_t>( filled ), '#' ) +
                    std::string( static_cast<size_t>( bar_width - filled ), '-' ) + "] " +
                    std::to_string( percent ) + "%";
            ncmm_trim_and_print_literal( frame, point( 2, 4 ), frame_width - 4,
                                        ncmm_ui_theme_enabled() ? ncmm_ui_theme_accent() : c_light_green, line );
        }

        const int divider_x = frame_width - detail_width - 2;
        for( int x = 1; x < divider_x; ++x ) {
            mvwaddch( frame, point( x, header_height - 1 ), LINE_OXOX );
        }
        for( int y = header_height - 1; y < frame_height - footer_height; ++y ) {
            mvwaddch( frame, point( divider_x, y ), LINE_XOXO );
        }
        ncmm_trim_and_print_literal( frame, point( divider_x + 2, header_height - 1 ),
                                    detail_width - 3, c_dark_gray, tr_ui( "DETAIL", "ДЕТАЛИ" ) );

        // 0.9.11: obstacle-safe dependency graph.
        // Connectors no longer encode perk type with milestone/effect colors.
        // Nodes carry semantic color; edges stay neutral, with only the edge
        // touching the current selection highlighted.  Every committed edge is
        // a complete BFS path, so a blocked horizontal leg can never appear as
        // a visually broken half-connector.
        constexpr int edge_n = 1;
        constexpr int edge_e = 2;
        constexpr int edge_s = 4;
        constexpr int edge_w = 8;
        const int grid_size = frame_width * frame_height;
        std::vector<int> edge_mask( static_cast<size_t>( grid_size ), 0 );
        std::vector<int> edge_style( static_cast<size_t>( grid_size ), 0 );
        std::vector<std::array<int, 4>> blocked_rects;
        blocked_rects.reserve( node_count );

        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) {
                continue;
            }
            const int bx = node_x( i );
            const int by = node_y( i );
            blocked_rects.push_back( {{ bx, by, bx + node_width - 1, by + node_height - 1 }} );
        }

        auto grid_index = [&]( int x, int y ) {
            return y * frame_width + x;
        };
        auto is_blocked = [&]( int x, int y ) {
            for( const std::array<int, 4> &r : blocked_rects ) {
                if( x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3] ) {
                    return true;
                }
            }
            return false;
        };
        auto mark_dir = [&]( int x, int y, int dir, int style ) {
            if( x <= 0 || x >= divider_x || y < header_height ||
                y >= frame_height - footer_height || is_blocked( x, y ) ) {
                return;
            }
            const int index = grid_index( x, y );
            edge_mask[index] |= dir;
            edge_style[index] = std::max( edge_style[index], style );
        };
        auto connect_cells = [&]( int ax, int ay, int bx, int by, int style ) {
            if( ax == bx && by == ay + 1 ) {
                mark_dir( ax, ay, edge_s, style );
                mark_dir( bx, by, edge_n, style );
            } else if( ax == bx && by == ay - 1 ) {
                mark_dir( ax, ay, edge_n, style );
                mark_dir( bx, by, edge_s, style );
            } else if( ay == by && bx == ax + 1 ) {
                mark_dir( ax, ay, edge_e, style );
                mark_dir( bx, by, edge_w, style );
            } else if( ay == by && bx == ax - 1 ) {
                mark_dir( ax, ay, edge_w, style );
                mark_dir( bx, by, edge_e, style );
            }
        };
        auto color_for_style = [&]( int style ) {
            if( style >= 2 ) return c_light_green;
            if( style == 1 ) return c_light_gray;
            return c_dark_gray;
        };
        auto glyph_for_mask = [&]( int mask ) -> int {
            const bool n = ( mask & edge_n ) != 0;
            const bool e = ( mask & edge_e ) != 0;
            const bool s = ( mask & edge_s ) != 0;
            const bool w = ( mask & edge_w ) != 0;
            if( n && e && s && w ) return LINE_XXXX;
            if( n && e && s ) return LINE_XXXO;
            if( n && e && w ) return LINE_XXOX;
            if( n && s && w ) return LINE_XOXX;
            if( e && s && w ) return LINE_OXXX;
            if( n && e ) return LINE_XXOO;
            if( e && s ) return LINE_OXXO;
            if( s && w ) return LINE_OOXX;
            if( n && w ) return LINE_XOOX;
            if( n || s ) return LINE_XOXO;
            return LINE_OXOX;
        };

        std::vector<bool> has_incoming( node_count, false );
        std::vector<bool> has_outgoing( node_count, false );

        // v8.7.6.1: focus the selected node's actual dependency chain.  Ancestors
        // and descendants are propagated separately so sibling branches do not
        // become highlighted merely because they share a common parent.
        std::vector<bool> dependency_ancestor( node_count, false );
        std::vector<bool> dependency_descendant( node_count, false );
        std::vector<bool> dependency_related( node_count, false );
        dependency_ancestor[static_cast<size_t>( selected )] = true;
        dependency_descendant[static_cast<size_t>( selected )] = true;
        for( size_t pass = 0; pass < node_count; ++pass ) {
            bool changed = false;
            for( size_t e = 0; e < edge_count; ++e ) {
                const size_t from = edges[e].from_index;
                const size_t to = edges[e].to_index;
                if( dependency_ancestor[to] && !dependency_ancestor[from] ) {
                    dependency_ancestor[from] = true;
                    changed = true;
                }
                if( dependency_descendant[from] && !dependency_descendant[to] ) {
                    dependency_descendant[to] = true;
                    changed = true;
                }
            }
            if( !changed ) break;
        }
        for( size_t i = 0; i < node_count; ++i ) {
            dependency_related[i] = dependency_ancestor[i] || dependency_descendant[i];
        }

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
            if( y2 < y1 || x1 <= 0 || x1 >= divider_x ||
                x2 <= 0 || x2 >= divider_x ) {
                continue;
            }

            const bool related_to_selection =
                ( dependency_ancestor[from] && dependency_ancestor[to] ) ||
                ( dependency_descendant[from] && dependency_descendant[to] );
            const int style = related_to_selection ? 2 : 0;

            const int start_index = grid_index( x1, y1 );
            const int goal_index = grid_index( x2, y2 );
            std::vector<int> previous( static_cast<size_t>( grid_size ), -1 );
            std::vector<int> frontier;
            frontier.reserve( static_cast<size_t>( grid_size ) );
            previous[start_index] = start_index;
            frontier.push_back( start_index );

            size_t head = 0;
            while( head < frontier.size() && previous[goal_index] < 0 ) {
                const int current = frontier[head++];
                const int cx = current % frame_width;
                const int cy = current / frame_width;

                // Prefer downward movement first: dependency trees remain easy to read,
                // while BFS still guarantees a complete route around every tile.
                const std::array<std::pair<int, int>, 4> directions = {{
                    { 0, 1 }, { x2 >= cx ? 1 : -1, 0 },
                    { x2 >= cx ? -1 : 1, 0 }, { 0, -1 }
                }};
                for( const auto &dir : directions ) {
                    const int nx = cx + dir.first;
                    const int ny = cy + dir.second;
                    if( nx <= 0 || nx >= divider_x || ny < header_height ||
                        ny >= frame_height - footer_height ) {
                        continue;
                    }
                    if( is_blocked( nx, ny ) && !( nx == x2 && ny == y2 ) ) {
                        continue;
                    }
                    const int next = grid_index( nx, ny );
                    if( previous[next] >= 0 ) {
                        continue;
                    }
                    previous[next] = current;
                    frontier.push_back( next );
                }
            }

            // Never draw a partial connector.  If no complete path exists in the
            // visible canvas, omit this edge for the current scroll window.
            if( previous[goal_index] < 0 ) {
                continue;
            }

            // Only expose the border tees after a COMPLETE connector exists.
            // This prevents the apparent one-cell "broken line" stubs that
            // v1 could leave when routing failed in the current scroll window.
            has_outgoing[from] = true;
            has_incoming[to] = true;

            std::vector<int> path;
            for( int at = goal_index; ; at = previous[at] ) {
                path.push_back( at );
                if( at == start_index ) {
                    break;
                }
            }
            std::reverse( path.begin(), path.end() );
            for( size_t p = 1; p < path.size(); ++p ) {
                const int a = path[p - 1];
                const int b = path[p];
                connect_cells( a % frame_width, a / frame_width,
                               b % frame_width, b / frame_width, style );
            }
        }

        for( int y = header_height; y < frame_height - footer_height; ++y ) {
            for( int x = 1; x < divider_x; ++x ) {
                const int index = grid_index( x, y );
                if( edge_mask[index] == 0 ) {
                    continue;
                }
                const nc_color color = color_for_style( edge_style[index] );
                wattron( frame, color );
                mvwaddch( frame, point( x, y ), glyph_for_mask( edge_mask[index] ) );
                wattroff( frame, color );
            }
        }

        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) continue;
            const int x = node_x( i );
            const int y = node_y( i );
            const bool active = static_cast<int>( i ) == selected;
            const bool owned = ( nodes[i].flags & NCMM_UI_CARD_OWNED ) != 0;
            const bool locked = ( nodes[i].flags & NCMM_UI_CARD_LOCKED ) != 0;
            const bool major = ( nodes[i].flags & NCMM_UI_CARD_MAJOR ) != 0;
            const bool effect = ( nodes[i].flags & NCMM_UI_CARD_EFFECT ) != 0;
            const bool themed = ncmm_ui_theme_enabled( i );
            const bool dependency_focus = dependency_related[i];
            const uint32_t border_style = ncmm_ui_border_style_id( i );
            const nc_color theme_accent = ncmm_ui_theme_accent( i );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    !dependency_focus ? c_dark_gray :
                                    border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                    border_style == NCMM_UI_BORDER_MAJOR ? c_yellow :
                                    border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = active ? c_white :
                                        !dependency_focus ? c_dark_gray :
                                        locked || border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                        border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                        themed ? theme_accent : c_light_gray;

            const bool prime_border = border_style == NCMM_UI_BORDER_PRIME;
            const int horizontal_glyph = prime_border ? '=' : LINE_OXOX;
            const int vertical_glyph = prime_border ? '|' : LINE_XOXO;
            const int top_left_glyph = prime_border ? '+' : LINE_OXXO;
            const int top_right_glyph = prime_border ? '+' : LINE_OOXX;
            const int bottom_left_glyph = prime_border ? '+' : LINE_XXOO;
            const int bottom_right_glyph = prime_border ? '+' : LINE_XOOX;

            wattron( frame, border );
            mvwhline( frame, point( x + 1, y ), horizontal_glyph, node_width - 2 );
            mvwhline( frame, point( x + 1, y + node_height - 1 ), horizontal_glyph, node_width - 2 );
            mvwvline( frame, point( x, y + 1 ), vertical_glyph, node_height - 2 );
            mvwvline( frame, point( x + node_width - 1, y + 1 ), vertical_glyph, node_height - 2 );
            mvwaddch( frame, point( x, y ), top_left_glyph );
            mvwaddch( frame, point( x + node_width - 1, y ), top_right_glyph );
            mvwaddch( frame, point( x, y + node_height - 1 ), bottom_left_glyph );
            mvwaddch( frame, point( x + node_width - 1, y + node_height - 1 ), bottom_right_glyph );

            const int center_x = x + node_width / 2;
            if( has_incoming[i] ) {
                mvwaddch( frame, point( center_x, y ), prime_border ? '+' : LINE_XXOX );
            }
            if( has_outgoing[i] ) {
                mvwaddch( frame, point( center_x, y + node_height - 1 ), prime_border ? '+' : LINE_OXXX );
            }
            wattroff( frame, border );

            const std::vector<std::string> title_lines =
                foldstring( nodes[i].title ? nodes[i].title : "", node_width - 4 );
            for( size_t line = 0; line < std::min<size_t>( 2, title_lines.size() ); ++line ) {
                ncmm_trim_and_print_literal( frame, point( x + 2, y + 1 + static_cast<int>( line ) ),
                                            node_width - 4, text_color, title_lines[line] );
            }
            ncmm_trim_and_print_literal( frame, point( x + 2, y + 3 ), node_width - 4,
                                        locked ? c_dark_gray : c_light_gray,
                                        nodes[i].subtitle ? nodes[i].subtitle : "" );
            ncmm_trim_and_print_literal( frame, point( x + 2, y + 4 ), node_width - 4,
                                        major ? c_yellow : effect ? c_magenta :
                                        owned ? c_cyan : locked ? c_dark_gray : c_green,
                                        nodes[i].badge ? nodes[i].badge : "" );
            const char *marker = active ? ">" :
                                 !dependency_focus ? nullptr :
                                 border_style == NCMM_UI_BORDER_PRIME ? "*" :
                                 border_style == NCMM_UI_BORDER_MAJOR ? "+" :
                                 border_style == NCMM_UI_BORDER_EXCLUDED ? "x" :
                                 ( themed && ( active_ui_layout_flags & NCMM_UI_THEME_STRONG_BORDER ) ? "*" : nullptr );
            if( marker != nullptr ) {
                mvwprintz( frame, point( x + 1, y + 1 ),
                           border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                           ( themed ? theme_accent : c_light_green ), marker );
            }
        }

        const ncmm_ui_tree_node_v1 &detail = nodes[selected];
        const int dx = divider_x + 2;
        const std::vector<std::string> detail_title =
            foldstring( detail.title ? detail.title : "", detail_width - 3 );
        int dy = header_height;
        for( size_t line = 0; line < std::min<size_t>( 2, detail_title.size() ); ++line ) {
            ncmm_trim_and_print_literal( frame, point( dx, dy++ ), detail_width - 3,
                                        ncmm_ui_theme_enabled( static_cast<size_t>( selected ) ) ?
                                        ncmm_ui_theme_accent( static_cast<size_t>( selected ) ) : c_white,
                                        detail_title[line] );
        }
        if( detail.subtitle && detail.subtitle[0] != '\0' ) {
            ncmm_trim_and_print_literal( frame, point( dx, dy++ ), detail_width - 3,
                                        c_light_gray, detail.subtitle );
        }
        if( detail.badge && detail.badge[0] != '\0' ) {
            ncmm_trim_and_print_literal( frame, point( dx, dy++ ), detail_width - 3,
                                        ( detail.flags & NCMM_UI_CARD_MAJOR ) ? c_yellow :
                                        ( detail.flags & NCMM_UI_CARD_EFFECT ) ? c_magenta : c_cyan,
                                        detail.badge );
        }
        ++dy;
        if( detail.body && detail.body[0] != '\0' ) {
            const std::vector<std::string> folded = foldstring( detail.body, detail_width - 3 );
            const int max_lines = std::max( 1, frame_height - footer_height - dy - 1 );
            for( int line = 0; line < std::min<int>( max_lines, folded.size() ); ++line ) {
                nc_color detail_color = c_light_gray;
                if( ncmm_ui_sectioned_detail() ) {
                    if( folded[line] == "BONUS:" || folded[line] == "БОНУС:" ) {
                        detail_color = c_light_green;
                    } else if( folded[line] == "DRAWBACK:" || folded[line] == "ШТРАФ:" ) {
                        detail_color = c_light_red;
                    } else if( folded[line] == "REQUIRES:" || folded[line] == "ТРЕБУЕТ:" ) {
                        detail_color = c_yellow;
                    } else if( folded[line] == "STATUS:" || folded[line] == "СТАТУС:" ) {
                        detail_color = c_cyan;
                    }
                }
                ncmm_trim_and_print_literal( frame, point( dx, dy + line ), detail_width - 3,
                                            detail_color, folded[line] );
            }
        }

        int hidden_left = 0;
        int hidden_right = 0;
        for( size_t i = 0; i < node_count; ++i ) {
            if( nodes[i].row < first_row || nodes[i].row >= first_row + visible_rows ) {
                continue;
            }
            const int x = node_x( i );
            if( x < 2 ) {
                ++hidden_left;
            } else if( x + node_width >= canvas_width + 2 ) {
                ++hidden_right;
            }
        }

        std::string footer = tr_ui(
            "Arrows: move  PgUp/PgDn: jump  Home/End: ends  Tab: cards  Enter: details  Esc: back",
            "Стрелки: ход  PgUp/PgDn: прыжок  Home/End: края  Tab: карточки  Enter: детали  Esc: назад" );
        if( hidden_left > 0 ) {
            footer = "< " + std::to_string( hidden_left ) + "  " + footer;
        }
        if( hidden_right > 0 ) {
            footer += "  " + std::to_string( hidden_right ) + " >";
        }
        footer += "  " + std::to_string( selected + 1 ) + "/" + std::to_string( node_count );
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ),
                                    frame_width - 4, c_dark_gray, footer );
        wnoutrefresh( frame );
    } );

    while( true ) {
        keep_visible();
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        const int previous_selected = selected;

        if( action == "MOUSE_MOVE" || action == "SELECT" ) {
            const std::optional<point> mouse = ctxt.get_coordinates_text( frame );
            if( mouse ) {
                const int hit = node_at( *mouse );
                if( hit >= 0 ) {
                    selected = hit;
                    if( selected != previous_selected ) {
                        sfx::play_variant_sound( "menu_move", "default", 100 );
                    }
                    if( action == "SELECT" ) return selected;
                }
            }
            continue;
        }

        if( action == "LEFT" ) select_direction( 0, -1 );
        else if( action == "RIGHT" ) select_direction( 0, 1 );
        else if( action == "UP" || action == "SCROLL_UP" ) select_direction( -1, 0 );
        else if( action == "DOWN" || action == "SCROLL_DOWN" ) select_direction( 1, 0 );
        else if( action == "PAGE_UP" ) {
            for( int i = 0; i < visible_rows; ++i ) select_direction( -1, 0 );
        } else if( action == "PAGE_DOWN" ) {
            for( int i = 0; i < visible_rows; ++i ) select_direction( 1, 0 );
        } else if( action == "HOME" ) selected = 0;
        else if( action == "END" ) selected = static_cast<int>( node_count ) - 1;
        else if( action == "NEXT_TAB" ) return NCMM_UI_TREE_SHOW_CARDS;
        else if( action == "CONFIRM" ) return selected;
        else if( action == "QUIT" ) return NCMM_UI_TREE_CANCEL;

        if( selected != previous_selected ) {
            sfx::play_variant_sound( "menu_move", "default", 100 );
        }
    }
}

int ui_card_choose_themed( const char *title, const char *summary,
                           const ncmm_ui_progress_v1 *progress,
                           const ncmm_ui_card_v1 *cards, size_t count, size_t columns,
                           const ncmm_ui_theme_v1 *theme )
{
    ncmm_ui_theme_v1 safe_theme = theme != nullptr ? *theme :
                                  ncmm_ui_theme_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                    0, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, count );
    }
    ncmm_ui_theme_scope scope( &safe_theme );
    return ui_card_choose( title, summary, progress, cards, count, columns );
}

int ui_tree_choose_themed( const char *title, const char *summary,
                           const ncmm_ui_progress_v1 *progress,
                           const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                           const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                           const ncmm_ui_theme_v1 *theme )
{
    ncmm_ui_theme_v1 safe_theme = theme != nullptr ? *theme :
                                  ncmm_ui_theme_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                    0, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, node_count );
    }
    ncmm_ui_theme_scope scope( &safe_theme );
    return ui_tree_choose( title, summary, progress, nodes, node_count, edges, edge_count );
}
int ui_card_choose_rpg( const char *title, const char *summary,
                        const ncmm_ui_progress_v1 *progress,
                        const ncmm_ui_card_v1 *cards, size_t count, size_t columns,
                        const ncmm_ui_theme_ex_v1 *theme )
{
    ncmm_ui_theme_ex_v1 safe_theme = theme != nullptr ? *theme :
                                     ncmm_ui_theme_ex_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                          0, 0, nullptr, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, count );
    }
    if( safe_theme.item_border_styles == nullptr ) {
        safe_theme.item_border_style_count = 0;
    } else {
        safe_theme.item_border_style_count = std::min( safe_theme.item_border_style_count, count );
    }
    ncmm_ui_rpg_theme_scope scope( &safe_theme );
    return ui_card_choose( title, summary, progress, cards, count, columns );
}

int ui_tree_choose_rpg( const char *title, const char *summary,
                        const ncmm_ui_progress_v1 *progress,
                        const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                        const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                        const ncmm_ui_theme_ex_v1 *theme )
{
    ncmm_ui_theme_ex_v1 safe_theme = theme != nullptr ? *theme :
                                     ncmm_ui_theme_ex_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                          0, 0, nullptr, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, node_count );
    }
    if( safe_theme.item_border_styles == nullptr ) {
        safe_theme.item_border_style_count = 0;
    } else {
        safe_theme.item_border_style_count = std::min( safe_theme.item_border_style_count, node_count );
    }
    ncmm_ui_rpg_theme_scope scope( &safe_theme );
    return ui_tree_choose( title, summary, progress, nodes, node_count, edges, edge_count );
}
void ui_message( const char *message )
{
    if( message != nullptr ) {
        popup( "%s", message );
    }
}

bool api_v2_token_safe( const char *value )
{
    if( value == nullptr ) return false;
    const std::string text( value );
    if( text.empty() || text.size() > 128 ) return false;
    for( unsigned char c : text ) {
        if( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' || c == ':' ) ) return false;
    }
    return true;
}

void clear_module_runtime_v2( const std::string &module_id )
{
    erase_module_modifiers( module_id );
    event_subscriptions_v2.erase( std::remove_if( event_subscriptions_v2.begin(), event_subscriptions_v2.end(),
    [&]( const ncmm_event_subscription_v2_internal &s ) { return s.module_id == module_id; } ), event_subscriptions_v2.end() );
    runtime_hook_rules_v2.erase( std::remove_if( runtime_hook_rules_v2.begin(), runtime_hook_rules_v2.end(),
    [&]( const ncmm_runtime_hook_rule_v2_internal &r ) { return r.module_id == module_id; } ), runtime_hook_rules_v2.end() );
    for( auto it = modifier_owners_v2.begin(); it != modifier_owners_v2.end(); ) {
        if( it->second == module_id ) {
            character_modifier_limits.erase( it->first );
            it = modifier_owners_v2.erase( it );
        } else {
            ++it;
        }
    }
    for( auto it = worldgen_bindings_v2.begin(); it != worldgen_bindings_v2.end(); ) {
        if( it->second.module_id == module_id ) it = worldgen_bindings_v2.erase( it ); else ++it;
    }
    for( auto it = runtime_setting_bindings_v2.begin(); it != runtime_setting_bindings_v2.end(); ) {
        if( it->second.module_id == module_id ) it = runtime_setting_bindings_v2.erase( it ); else ++it;
    }
}

const char *current_module_id_v2()
{
    return active_module_id.empty() ? nullptr : active_module_id.c_str();
}

int event_available_v2( uint32_t event_id )
{
    switch( event_id ) {
        case NCMM_EVENT_HOST_READY_V2:
        case NCMM_EVENT_WORLD_LOADED_V2:
        case NCMM_EVENT_WORLD_UNLOADED_V2:
        case NCMM_EVENT_TURN_V2:
        case NCMM_EVENT_LOCALE_CHANGED_V2:
        case NCMM_EVENT_PLAYER_KILL_V2:
            return 1;
        default:
            return 0;
    }
}

int event_subscribe_v2( const char *module_id, uint32_t event_id,
                        ncmm_event_callback_v2 callback, void *user_data )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !event_available_v2( event_id ) || callback == nullptr ) return 0;
    for( const auto &s : event_subscriptions_v2 ) {
        if( s.module_id == module_id && s.event_id == event_id &&
            s.callback == callback && s.user_data == user_data ) return 1;
    }
    event_subscriptions_v2.push_back( { module_id, event_id, callback, user_data } );
    return 1;
}

int event_unsubscribe_all_v2( const char *module_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ) return 0;
    event_subscriptions_v2.erase( std::remove_if( event_subscriptions_v2.begin(), event_subscriptions_v2.end(),
    [&]( const ncmm_event_subscription_v2_internal &s ) { return s.module_id == module_id; } ), event_subscriptions_v2.end() );
    return 1;
}

void dispatch_event_v2( uint32_t event_id )
{
    if( !event_available_v2( event_id ) || event_subscriptions_v2.empty() ) return;
    const auto snapshot = event_subscriptions_v2;
    std::set<std::string> failed;
    for( const auto &s : snapshot ) {
        if( s.callback == nullptr || module_ids.count( s.module_id ) == 0 ) continue;
        try {
            module_call_scope scope( s.module_id.c_str() );
            s.callback( event_id, s.user_data );
        } catch( ... ) {
            failed.insert( s.module_id );
            log_line( NCMM_LOG_WARN, ( "Host API 2.0 event callback failed: " + s.module_id ).c_str() );
        }
    }
    if( !failed.empty() ) {
        event_subscriptions_v2.erase( std::remove_if( event_subscriptions_v2.begin(), event_subscriptions_v2.end(),
        [&]( const ncmm_event_subscription_v2_internal &s ) { return failed.count( s.module_id ) != 0; } ), event_subscriptions_v2.end() );
    }
}

int module_is_loaded_v2( const char *module_id )
{
    return module_id && module_ids.count( module_id ) != 0 ? 1 : 0;
}

const char *module_version_v2( const char *module_id )
{
    api_v2_string_cache.clear();
    if( module_id == nullptr ) return nullptr;
    for( const loaded_mod &mod : loaded ) {
        if( mod.descriptor && mod.descriptor->id && std::string( mod.descriptor->id ) == module_id ) {
            api_v2_string_cache = mod.descriptor->version ? mod.descriptor->version : "";
            return api_v2_string_cache.c_str();
        }
    }
    for( const module_state &state : module_states ) {
        if( state.id == module_id ) {
            api_v2_string_cache = state.version;
            return api_v2_string_cache.c_str();
        }
    }
    return nullptr;
}

const char *module_state_v2( const char *module_id )
{
    api_v2_string_cache.clear();
    if( module_id == nullptr ) return nullptr;
    for( const module_state &state : module_states ) {
        if( state.id == module_id ) {
            api_v2_string_cache = state.state;
            if( !state.lifecycle.empty() ) api_v2_string_cache += "/" + state.lifecycle;
            return api_v2_string_cache.c_str();
        }
    }
    return nullptr;
}

int modifier_define_v2( const char *module_id, const char *modifier_id,
                        double min_value, double max_value )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( modifier_id ) || !std::isfinite( min_value ) ||
        !std::isfinite( max_value ) || min_value > max_value ||
        min_value < -100000.0 || max_value > 100000.0 ) return 0;
    const auto owner = modifier_owners_v2.find( modifier_id );
    const auto policy = character_modifier_limits.find( modifier_id );
    if( policy != character_modifier_limits.end() ) {
        if( owner == modifier_owners_v2.end() || owner->second != module_id ) return 0;
        return std::abs( policy->second.first - min_value ) < 1.0e-12 &&
               std::abs( policy->second.second - max_value ) < 1.0e-12 ? 1 : 0;
    }
    character_modifier_limits.emplace( modifier_id, std::make_pair( min_value, max_value ) );
    modifier_owners_v2[modifier_id] = module_id;
    return 1;
}

int runtime_hook_bind_modifier_v2( const char *module_id, const char *hook_id,
                                   uint32_t selector_kind, const char *selector_value,
                                   const char *modifier_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( hook_id ) || !api_v2_token_safe( modifier_id ) ||
        selector_kind > NCMM_SELECTOR_TARGET_SPECIES_V2 ) return 0;
    if( selector_kind != NCMM_SELECTOR_ANY_V2 && !api_v2_token_safe( selector_value ) ) return 0;
    if( character_modifier_limits.find( modifier_id ) == character_modifier_limits.end() ) return 0;
    const auto owner = modifier_owners_v2.find( modifier_id );
    if( owner != modifier_owners_v2.end() && owner->second != module_id ) return 0;
    const std::string selector = selector_kind == NCMM_SELECTOR_ANY_V2 ? "" : selector_value;
    for( const auto &r : runtime_hook_rules_v2 ) {
        if( r.module_id == module_id && r.hook_id == hook_id &&
            r.selector_kind == selector_kind && r.selector_value == selector &&
            r.modifier_id == modifier_id ) return 1;
    }
    runtime_hook_rules_v2.push_back( { module_id, hook_id, selector_kind, selector, modifier_id } );
    return 1;
}

bool runtime_rule_matches_v2( const ncmm_runtime_hook_rule_v2_internal &r,
                              const char *subject_id, const char *source_mod_id,
                              const char *source_species_id, const char *target_species_id )
{
    switch( r.selector_kind ) {
        case NCMM_SELECTOR_ANY_V2: return true;
        case NCMM_SELECTOR_SUBJECT_ID_V2: return subject_id && r.selector_value == subject_id;
        case NCMM_SELECTOR_SOURCE_MOD_V2: return source_mod_id && r.selector_value == source_mod_id;
        case NCMM_SELECTOR_SOURCE_SPECIES_V2: return source_species_id && r.selector_value == source_species_id;
        case NCMM_SELECTOR_TARGET_SPECIES_V2: return target_species_id && r.selector_value == target_species_id;
        default: return false;
    }
}

double runtime_hook_value_v2( const char *hook_id, const char *subject_id,
                              const char *source_mod_id, const char *source_species_id,
                              const char *target_species_id )
{
    // HOTFIX13: mod character-creation EOCs may query spell/skill formulas before the
    // avatar is fully established. Runtime gameplay modifiers stay neutral until the
    // first real turn announces the world.
    if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;
    double total = 0.0;
    std::set<std::string> counted;
    for( const auto &r : runtime_hook_rules_v2 ) {
        if( r.hook_id != hook_id || !runtime_rule_matches_v2( r, subject_id, source_mod_id,
                source_species_id, target_species_id ) ) continue;
        const auto module_it = character_modifier_values.find( r.module_id );
        if( module_it == character_modifier_values.end() ) continue;
        const auto value_it = module_it->second.find( r.modifier_id );
        if( value_it == module_it->second.end() ) continue;
        const std::string key = r.module_id + "\n" + r.modifier_id;
        if( counted.insert( key ).second ) total += value_it->second;
    }
    return std::max( -100000.0, std::min( 100000.0, total ) );
}

int worldgen_hook_bind_setting_v2( const char *module_id, const char *hook_id,
                                   const char *setting_id, uint32_t value_type )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( hook_id ) || !api_v2_token_safe( setting_id ) ||
        value_type < NCMM_WORLDGEN_BOOL_V2 || value_type > NCMM_WORLDGEN_FLOAT_V2 ) return 0;
    const auto existing = worldgen_bindings_v2.find( hook_id );
    if( existing != worldgen_bindings_v2.end() ) {
        return existing->second.module_id == module_id && existing->second.setting_id == setting_id &&
               existing->second.value_type == value_type ? 1 : 0;
    }
    worldgen_bindings_v2.emplace( hook_id, ncmm_worldgen_binding_v2_internal{ module_id, setting_id, value_type } );
    return 1;
}

int worldgen_hook_bool_v2( const char *hook_id, int fallback )
{
    const auto it = hook_id ? worldgen_bindings_v2.find( hook_id ) : worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() || it->second.value_type != NCMM_WORLDGEN_BOOL_V2 ) return fallback;
    return world_setting_get_bool( it->second.setting_id.c_str(), fallback );
}
int64_t worldgen_hook_i64_v2( const char *hook_id, int64_t fallback )
{
    const auto it = hook_id ? worldgen_bindings_v2.find( hook_id ) : worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() || it->second.value_type != NCMM_WORLDGEN_INT_V2 ) return fallback;
    return world_setting_get_i64( it->second.setting_id.c_str(), fallback );
}
double worldgen_hook_f64_v2( const char *hook_id, double fallback )
{
    const auto it = hook_id ? worldgen_bindings_v2.find( hook_id ) : worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() || it->second.value_type != NCMM_WORLDGEN_FLOAT_V2 ) return fallback;
    return world_setting_get_f64( it->second.setting_id.c_str(), fallback );
}

int runtime_hook_bind_setting_v2( const char *module_id, const char *hook_id,
                                  const char *setting_id, uint32_t value_type )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( hook_id ) || !safe_world_setting_id( setting_id ) ||
        value_type < NCMM_SETTING_BOOL_V2 || value_type > NCMM_SETTING_FLOAT_V2 ) return 0;
    const auto owner = world_setting_owners.find( setting_id );
    if( owner == world_setting_owners.end() || owner->second != module_id ) return 0;
    const auto existing = runtime_setting_bindings_v2.find( hook_id );
    if( existing != runtime_setting_bindings_v2.end() ) {
        return existing->second.module_id == module_id &&
               existing->second.setting_id == setting_id &&
               existing->second.value_type == value_type ? 1 : 0;
    }
    runtime_setting_bindings_v2.emplace(
        hook_id, ncmm_worldgen_binding_v2_internal{ module_id, setting_id, value_type } );
    return 1;
}

int runtime_hook_bool_v2( const char *hook_id, int fallback )
{
    const auto it = hook_id ? runtime_setting_bindings_v2.find( hook_id ) :
                    runtime_setting_bindings_v2.end();
    if( it == runtime_setting_bindings_v2.end() ||
        it->second.value_type != NCMM_SETTING_BOOL_V2 ) return fallback;
    return world_setting_get_bool( it->second.setting_id.c_str(), fallback );
}

int64_t runtime_hook_i64_v2( const char *hook_id, int64_t fallback )
{
    const auto it = hook_id ? runtime_setting_bindings_v2.find( hook_id ) :
                    runtime_setting_bindings_v2.end();
    if( it == runtime_setting_bindings_v2.end() ||
        it->second.value_type != NCMM_SETTING_INT_V2 ) return fallback;
    return world_setting_get_i64( it->second.setting_id.c_str(), fallback );
}

double runtime_hook_f64_v2( const char *hook_id, double fallback )
{
    const auto it = hook_id ? runtime_setting_bindings_v2.find( hook_id ) :
                    runtime_setting_bindings_v2.end();
    if( it == runtime_setting_bindings_v2.end() ||
        it->second.value_type != NCMM_SETTING_FLOAT_V2 ) return fallback;
    return world_setting_get_f64( it->second.setting_id.c_str(), fallback );
}

double modifier_get_total_v2( const char *modifier_id )
{
    return gameplay_modifier( modifier_id );
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
    const auto dynamic_owner = modifier_owners_v2.find( modifier_id );
    if( dynamic_owner != modifier_owners_v2.end() && dynamic_owner->second != module_id ) {
        return 0;
    }

    auto &module_values = character_modifier_values[module_id];
    double &module_value = module_values[modifier_id];
    const double previous_value = module_value;
    module_value = value;

    double &aggregate = character_modifier_totals[modifier_id];
    aggregate += value - previous_value;
    if( std::abs( aggregate ) < 1.0e-12 ) {
        character_modifier_totals.erase( modifier_id );
    }
    return 1;
}

int character_modifier_clear_module( const char *module_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ) {
        return 0;
    }
    erase_module_modifiers( module_id );
    return 1;
}

const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor );

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
    &gameplay_metric_get_i64,
    &world_mod_active,
    &world_setting_register_bool,
    &world_setting_register_int,
    &world_setting_register_float,
    &world_setting_register_enum,
    &world_setting_get_bool,
    &world_setting_get_i64,
    &world_setting_get_f64,
    &world_setting_get_string,
    &worldgen_experimental_group_begin,
    &ui_card_choose_themed,
    &ui_tree_choose_themed,
    &world_mod_count,
    &world_mod_id,
    &ui_card_choose_rpg,
    &ui_tree_choose_rpg,
    &query_interface_v2
};
const ncmm_host_api_v2_core api_v2_core = {
    sizeof( ncmm_host_api_v2_core ),
    NCMM_HOST_API_V2_CORE_ABI,
    NCMM_HOST_API_V2_CORE_MAJOR,
    NCMM_HOST_API_V2_CORE_MINOR,
    &api,
    &log_line,
    &has_capability,
    &get_host_version,
    &current_module_id_v2,
    &event_available_v2,
    &event_subscribe_v2,
    &event_unsubscribe_all_v2,
    &world_setting_register_bool,
    &world_setting_register_int,
    &world_setting_register_float,
    &world_setting_register_enum,
    &world_setting_get_bool,
    &world_setting_get_i64,
    &world_setting_get_f64,
    &world_setting_get_string,
    &world_mod_count,
    &world_mod_id,
    &world_mod_active,
    &character_state_available,
    &character_state_get_i64,
    &character_state_set_i64,
    &module_is_loaded_v2,
    &module_version_v2,
    &module_state_v2,
    &modifier_define_v2,
    &character_modifier_set,
    &character_modifier_clear_module,
    &modifier_get_total_v2,
    &runtime_hook_bind_modifier_v2,
    &runtime_hook_value_v2,
    &worldgen_hook_bind_setting_v2,
    &worldgen_hook_bool_v2,
    &worldgen_hook_i64_v2,
    &worldgen_hook_f64_v2,
    &runtime_hook_bind_setting_v2,
    &runtime_hook_bool_v2,
    &runtime_hook_i64_v2,
    &runtime_hook_f64_v2,
    &virtual_item_choose_v2,
    &virtual_item_clear_v2,
    &virtual_item_name_v2,
    &virtual_item_uid_v2
};

const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )
{
    if( interface_id == nullptr || std::string( interface_id ) != NCMM_HOST_API_V2_CORE_ID ) return nullptr;
    if( min_major > NCMM_HOST_API_V2_CORE_MAJOR ) return nullptr;
    if( min_major == NCMM_HOST_API_V2_CORE_MAJOR && min_minor > NCMM_HOST_API_V2_CORE_MINOR ) return nullptr;
    return &api_v2_core;
}

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
        << "  \"host_version\": \"0.8.2\",\n"
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

loaded_mod *find_loaded_by_id( const char *module_id )
{
    if( module_id == nullptr ) {
        return nullptr;
    }
    for( loaded_mod &mod : loaded ) {
        if( mod.descriptor != nullptr && mod.descriptor->id != nullptr &&
            std::string( mod.descriptor->id ) == module_id ) {
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
        erase_module_modifiers( module_id );
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
        erase_module_modifiers( id );
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
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string default_hotkey;
    std::string runtime_state;
    std::string reason;
    bool disabled = false;
    bool loaded_now = false;
};

std::string manager_description( const std::filesystem::path &directory )
{
    const std::filesystem::path localized = directory /
        ( russian_ui() ? "about.ru.txt" : "about.en.txt" );
    std::string result = read_text_file( localized );
    if( result.empty() && russian_ui() ) {
        result = read_text_file( directory / "about.en.txt" );
    }
    while( !result.empty() && ( result.back() == '\n' || result.back() == '\r' ) ) {
        result.pop_back();
    }
    return result;
}

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
            entry.id = runtime->descriptor->id ? runtime->descriptor->id : "";
            entry.name = runtime->descriptor->name ? runtime->descriptor->name : dir.filename().string();
            entry.version = runtime->descriptor->version ? runtime->descriptor->version : "";
            entry.default_hotkey = runtime->default_hotkey;
        } else if( state ) {
            entry.id = state->id;
            entry.name = state->name;
            entry.version = state->version;
            entry.default_hotkey = state->default_hotkey;
        } else {
            const manifest_contract manifest = read_manifest( dir );
            entry.id = manifest.id;
            entry.name = manifest.name;
            entry.version = manifest.version;
            entry.default_hotkey = manifest.ui_hotkey;
            if( entry.name.empty() ) {
                entry.name = dir.filename().string();
            }
        }
        entry.description = manager_description( dir );
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
    clear_module_runtime_v2( manifest.id );
    bool init_ok = false;
    try {
        module_call_scope scope( manifest.id.c_str() );
        init_ok = desc->init( &api ) != 0;
    } catch( ... ) {
        clear_module_runtime_v2( manifest.id );
        module_ids.erase( manifest.id );
        record_module_state( directory, manifest, "failed", "init_exception" );
        log_line( NCMM_LOG_WARN,
                  ( std::string( "Module init callback threw; disabled: " ) + desc->id ).c_str() );
        FreeLibrary( module );
        return;
    }
    if( !init_ok ) {
        clear_module_runtime_v2( manifest.id );
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

item *virtual_item_for_slot( const char *module_id, const char *slot_id )
{
    return virtual_item_for_slot_internal( module_id, slot_id );
}

bool is_virtual_item( const item &it )
{
    return !it.get_var( virtual_item_marker_key, "" ).empty();
}

double runtime_hook_modifier( const char *hook_id, const char *subject_id,
                              const char *source_mod_id, const char *source_species_id,
                              const char *target_species_id )
{
    return runtime_hook_value_v2( hook_id, subject_id, source_mod_id,
                                  source_species_id, target_species_id );
}

void runtime_event_notify( uint32_t event_id )
{
    dispatch_event_v2( event_id );
}

std::string runtime_source_mod_swap( const std::string &source_mod_id )
{
    std::string previous = runtime_source_mod_context_v2;
    runtime_source_mod_context_v2 = source_mod_id;
    return previous;
}

const std::string &runtime_source_mod()
{
    return runtime_source_mod_context_v2;
}

double runtime_hook_modifier_for_creatures( const char *hook_id,
        const Creature *source, const Creature *target )
{
    // HOTFIX13: never expose combat/runtime modifier state during chargen or pre-world load.
    if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;
    double total = 0.0;
    std::set<std::string> counted;
    for( const auto &r : runtime_hook_rules_v2 ) {
        if( r.hook_id != hook_id ) continue;
        bool match = false;
        switch( r.selector_kind ) {
            case NCMM_SELECTOR_ANY_V2: match = true; break;
            case NCMM_SELECTOR_SOURCE_MOD_V2:
                match = !runtime_source_mod_context_v2.empty() &&
                        r.selector_value == runtime_source_mod_context_v2;
                break;
            case NCMM_SELECTOR_SOURCE_SPECIES_V2:
                match = source != nullptr && source->in_species( species_id( r.selector_value ) );
                break;
            case NCMM_SELECTOR_TARGET_SPECIES_V2:
                match = target != nullptr && target->in_species( species_id( r.selector_value ) );
                break;
            default: break;
        }
        if( !match ) continue;
        const auto module_it = character_modifier_values.find( r.module_id );
        if( module_it == character_modifier_values.end() ) continue;
        const auto value_it = module_it->second.find( r.modifier_id );
        if( value_it == module_it->second.end() ) continue;
        const std::string key = r.module_id + "\n" + r.modifier_id;
        if( counted.insert( key ).second ) total += value_it->second;
    }
    return std::max( -100000.0, std::min( 100000.0, total ) );
}

bool worldgen_scope_hook_enabled( const char *scope_hook_id )
{
    const auto it = scope_hook_id ? worldgen_bindings_v2.find( scope_hook_id ) :
                    worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() ||
        it->second.value_type != NCMM_WORLDGEN_BOOL_V2 ) {
        // Backward compatibility: older AWS builds did not publish scope hooks.
        return true;
    }
    return world_setting_get_bool( it->second.setting_id.c_str(), 1 ) != 0;
}

bool worldgen_hook_scope_enabled( const char *hook_id )
{
    if( hook_id == nullptr ) {
        return false;
    }

    const std::string id( hook_id );
    // The master switch and the scope switches themselves must always remain
    // queryable. Scope filtering applies only to concrete geography hooks.
    if( id == "geography.custom.enabled" || id.rfind( "geography.scope.", 0 ) == 0 ) {
        return true;
    }

    const char *scope_hook = nullptr;
    if( id.rfind( "geography.city.", 0 ) == 0 ||
        id.rfind( "geography.roads.", 0 ) == 0 ||
        id.rfind( "geography.railroads.", 0 ) == 0 ) {
        scope_hook = "geography.scope.cities.enabled";
    } else if( id.rfind( "geography.forests.", 0 ) == 0 ||
               id.rfind( "geography.swamps.", 0 ) == 0 ||
               id.rfind( "geography.trails.", 0 ) == 0 ) {
        scope_hook = "geography.scope.ecology.enabled";
    } else if( id.rfind( "geography.rivers.", 0 ) == 0 ||
               id.rfind( "geography.lakes.", 0 ) == 0 ||
               id.rfind( "geography.oceans.", 0 ) == 0 ) {
        scope_hook = "geography.scope.water.enabled";
    } else if( id.rfind( "geography.highways.", 0 ) == 0 ||
               id.rfind( "geography.ravines.", 0 ) == 0 ) {
        scope_hook = "geography.scope.transport.enabled";
    }

    return scope_hook == nullptr || worldgen_scope_hook_enabled( scope_hook );
}

bool worldgen_hook_bound( const char *hook_id )
{
    return hook_id != nullptr &&
           worldgen_bindings_v2.find( hook_id ) != worldgen_bindings_v2.end() &&
           worldgen_hook_scope_enabled( hook_id );
}
int worldgen_hook_bool( const char *hook_id, int fallback )
{
    return worldgen_hook_bool_v2( hook_id, fallback );
}
int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback )
{
    return worldgen_hook_i64_v2( hook_id, fallback );
}
double worldgen_hook_f64( const char *hook_id, double fallback )
{
    return worldgen_hook_f64_v2( hook_id, fallback );
}
namespace
{
bool semantic_item_glyphs_enabled()
{
    return runtime_setting_hook_bound( "inventory.item_glyphs.enabled" ) &&
           runtime_setting_hook_bool( "inventory.item_glyphs.enabled", 0 ) != 0;
}
}

bool inventory_symbols_enabled( bool vanilla_symbols )
{
    return item_glyphs::symbol_slot_enabled( semantic_item_glyphs_enabled(), vanilla_symbols );
}

std::string inventory_item_symbol( const item &it )
{
    return item_glyphs::symbol( it, semantic_item_glyphs_enabled() );
}

bool runtime_setting_hook_bound( const char *hook_id )
{
    return hook_id != nullptr &&
           runtime_setting_bindings_v2.find( hook_id ) != runtime_setting_bindings_v2.end();
}
int runtime_setting_hook_bool( const char *hook_id, int fallback )
{
    return runtime_hook_bool_v2( hook_id, fallback );
}
int64_t runtime_setting_hook_i64( const char *hook_id, int64_t fallback )
{
    return runtime_hook_i64_v2( hook_id, fallback );
}
double runtime_setting_hook_f64( const char *hook_id, double fallback )
{
    return runtime_hook_f64_v2( hook_id, fallback );
}
double gameplay_modifier( const char *modifier_id )
{
    if( modifier_id == nullptr || character_modifier_limits.find( modifier_id ) == character_modifier_limits.end() ) {
        return 0.0;
    }
    const auto it = character_modifier_totals.find( modifier_id );
    if( it == character_modifier_totals.end() ) {
        return 0.0;
    }
    return std::max( -500.0, std::min( 500.0, it->second ) );
}

std::string settings_menu_label()
{
    return tr_ui( "<N|n>CMM / Mod Configuration", "<N|n>CMM / Настройка модов" );
}

std::string version_label()
{
    return std::string( "NCMM " ) + get_host_version();
}

std::string localized_text( const char *english, const char *russian )
{
    return tr_ui( english ? english : "", russian ? russian : "" );
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
            popup( tr_ui( "This mod could not load its saved data safely. Open NCMM diagnostics for details.",
                          "Не удалось безопасно загрузить сохранённые данные этого мода. Подробности — в диагностике NCMM." ) );
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
            popup( tr_ui( "This mod's interface failed to open and has been disabled for this session.",
                          "Интерфейс мода не открылся и отключён до перезапуска игры." ) );
        }
        return true;
    }

    return false;
}

std::string manager_state_label( const manager_entry &entry )
{
    if( entry.disabled && entry.loaded_now ) {
        return tr_ui( "OFF / restart required", "ВЫКЛ / нужен перезапуск" );
    }
    if( entry.disabled ) return tr_ui( "OFF", "ВЫКЛ" );
    if( entry.runtime_state == "runtime_fault" || entry.runtime_state == "failed" ) {
        return tr_ui( "ON / error", "ВКЛ / ошибка" );
    }
    if( entry.runtime_state == "suspended" ) {
        return tr_ui( "ON / needs attention", "ВКЛ / требует внимания" );
    }
    if( entry.loaded_now ) return tr_ui( "ON / loaded", "ВКЛ / загружен" );
    if( entry.runtime_state == "rejected" ) {
        return tr_ui( "ON / incompatible", "ВКЛ / несовместим" );
    }
    return tr_ui( "ON / restart required", "ВКЛ / нужен перезапуск" );
}

std::string manager_state_marker( const manager_entry &entry )
{
    if( entry.disabled ) return "[ ]";
    if( entry.runtime_state == "runtime_fault" || entry.runtime_state == "failed" ) return "[!]";
    if( entry.runtime_state == "suspended" ) return "[?]";
    if( entry.runtime_state == "rejected" ) return "[x]";
    if( entry.loaded_now ) return "[+]";
    return "[*]";
}

nc_color manager_state_color( const manager_entry &entry )
{
    if( entry.disabled ) return c_dark_gray;
    if( entry.runtime_state == "runtime_fault" || entry.runtime_state == "failed" ||
        entry.runtime_state == "rejected" ) return c_light_red;
    if( entry.runtime_state == "suspended" || !entry.loaded_now ) return c_yellow;
    return c_light_green;
}

std::vector<const module_setting_meta *> manager_settings_for( const std::string &module_id )
{
    std::vector<const module_setting_meta *> result;
    for( const module_setting_meta &setting : module_settings ) {
        if( setting.module_id == module_id ) result.push_back( &setting );
    }
    return result;
}

std::string manager_setting_value( const module_setting_meta &setting )
{
    if( !get_options().has_option( setting.setting_id ) ) {
        return tr_ui( "unavailable", "недоступно" );
    }
    if( setting.type == "bool" ) {
        return world_setting_get_bool( setting.setting_id.c_str(), 0 ) ?
               tr_ui( "On", "Вкл" ) : tr_ui( "Off", "Выкл" );
    }
    if( setting.type == "int" ) {
        return std::to_string( world_setting_get_i64( setting.setting_id.c_str(), 0 ) );
    }
    if( setting.type == "float" ) {
        std::ostringstream out;
        out << world_setting_get_f64( setting.setting_id.c_str(), 0.0 );
        return out.str();
    }
    const std::string current = world_setting_get_string( setting.setting_id.c_str(), "" );
    for( const auto &choice : setting.choices ) {
        if( choice.first == current ) return choice.second;
    }
    return current;
}

std::string manager_setting_default_value( const module_setting_meta &setting )
{
    if( setting.type == "bool" ) {
        return setting.default_value == "true" ? tr_ui( "On", "Вкл" ) :
               tr_ui( "Off", "Выкл" );
    }
    if( setting.type == "enum" ) {
        for( const auto &choice : setting.choices ) {
            if( choice.first == setting.default_value ) return choice.second;
        }
    }
    return setting.default_value;
}

std::string manager_setting_scope_label( uint32_t scope )
{
    if( scope == NCMM_WORLD_SETTING_LIVE ) {
        return tr_ui( "applies immediately", "применяется сразу" );
    }
    if( scope == NCMM_WORLD_SETTING_RELOAD ) {
        return tr_ui( "after world reload", "после перезагрузки мира" );
    }
    if( scope == NCMM_WORLD_SETTING_NEW_MAP ) {
        return tr_ui( "newly generated areas", "для новых областей мира" );
    }
    return tr_ui( "new world only", "только для нового мира" );
}

bool manager_setting_is_default( const module_setting_meta &setting )
{
    if( !get_options().has_option( setting.setting_id ) ) return true;
    return get_options().get_option( setting.setting_id ).getValue() == setting.default_value;
}

bool manager_reset_setting( const module_setting_meta &setting )
{
    if( setting.default_value.empty() || !get_options().has_option( setting.setting_id ) ) {
        return false;
    }
    options_manager::cOpt &opt = get_options().get_option( setting.setting_id );
    const std::string before = opt.getValue();
    opt.setValue( setting.default_value );
    return opt.getValue() != before;
}

bool manager_reset_module_settings( const std::string &module_id )
{
    bool changed = false;
    for( const module_setting_meta &setting : module_settings ) {
        if( setting.module_id == module_id ) {
            changed = manager_reset_setting( setting ) || changed;
        }
    }
    return changed;
}

bool manager_has_world_settings( const std::string &module_id )
{
    for( const auto &owner : world_setting_owners ) {
        if( owner.second != module_id ) continue;
        const auto scope = world_setting_scopes.find( owner.first );
        if( scope != world_setting_scopes.end() &&
            scope->second >= NCMM_WORLD_SETTING_NEW_MAP ) return true;
    }
    return false;
}

bool manager_confirm_reset( const manager_entry &entry )
{
    uilist confirm;
    confirm.text = tr_ui( "Reset all inline settings for ", "Сбросить все настройки мода " ) +
                   entry.name + "?";
    confirm.addentry( 0, true, MENU_AUTOASSIGN, tr_ui( "Reset to defaults", "Сбросить" ) );
    confirm.addentry( 1, true, MENU_AUTOASSIGN, tr_ui( "Cancel", "Отмена" ) );
    confirm.query();
    return confirm.ret == 0;
}

bool manager_adjust_setting( const module_setting_meta &setting, int direction )
{
    if( direction == 0 || !get_options().has_option( setting.setting_id ) ) return false;
    options_manager::cOpt &opt = get_options().get_option( setting.setting_id );
    if( setting.type == "bool" ) {
        const bool current = world_setting_get_bool( setting.setting_id.c_str(), 0 ) != 0;
        opt.setValue( current ? "false" : "true" );
        return true;
    }
    if( setting.type == "int" ) {
        const int64_t current = world_setting_get_i64( setting.setting_id.c_str(), 0 );
        const int64_t next = std::max<int64_t>( static_cast<int64_t>( setting.min_value ),
                             std::min<int64_t>( static_cast<int64_t>( setting.max_value ),
                                               current + direction * static_cast<int64_t>( setting.step ) ) );
        opt.setValue( std::to_string( next ) );
        return next != current;
    }
    if( setting.type == "float" ) {
        const double current = world_setting_get_f64( setting.setting_id.c_str(), 0.0 );
        const double next = std::max( setting.min_value,
                                     std::min( setting.max_value, current + direction * setting.step ) );
        std::ostringstream value;
        value << next;
        opt.setValue( value.str() );
        return std::abs( next - current ) > 0.000001;
    }
    if( setting.type == "enum" && !setting.choices.empty() ) {
        const std::string current = world_setting_get_string(
                                        setting.setting_id.c_str(),
                                        setting.choices.front().first.c_str() );
        size_t index = 0;
        for( size_t i = 0; i < setting.choices.size(); ++i ) {
            if( setting.choices[i].first == current ) { index = i; break; }
        }
        if( direction < 0 && index > 0 ) --index;
        if( direction > 0 && index + 1 < setting.choices.size() ) ++index;
        opt.setValue( setting.choices[index].first );
        return setting.choices[index].first != current;
    }
    return false;
}

bool manager_persist_settings()
{
    if( world_generator != nullptr && world_generator->active_world != nullptr ) {
        return world_generator->active_world->save();
    }
    return get_options().save();
}

bool manager_open_world_settings()
{
    const bool ingame = world_generator != nullptr && world_generator->active_world != nullptr;
    get_options().show( ingame, true, false );
    return true;
}

bool manager_open_module_ui( const manager_entry &entry )
{
    loaded_mod *runtime = find_loaded_mutable( entry.directory );
    if( runtime == nullptr || runtime->open_ui == nullptr ) return false;
    if( !ensure_state_migrated( *runtime ) ) {
        popup( tr_ui( "This mod could not load its saved data safely. Open NCMM diagnostics for details.",
                      "Не удалось безопасно загрузить сохранённые данные этого мода. Подробности — в диагностике NCMM." ) );
        return true;
    }
    try {
        module_call_scope scope( runtime->descriptor && runtime->descriptor->id ?
                                 runtime->descriptor->id : nullptr );
        runtime->open_ui( &api );
    } catch( ... ) {
        quarantine_runtime_callback( *runtime, runtime_callback_kind::ui, "ui_exception" );
        log_line( NCMM_LOG_WARN, ( "Module UI callback failed: " + entry.name ).c_str() );
        popup( tr_ui( "This mod's interface failed to open and has been disabled for this session.",
                      "Интерфейс мода не открылся и отключён до перезапуска игры." ) );
    }
    return true;
}

void manager_toggle_module( const manager_entry &entry )
{
    const std::filesystem::path marker = entry.directory / "disabled";
    std::error_code ec;
    if( entry.disabled ) {
        std::filesystem::remove( marker, ec );
        if( ec ) popup( tr_ui( "Could not enable the mod.", "Не удалось включить мод." ) );
        else popup( tr_ui( "Mod enabled. Restart CDDA to apply.",
                           "Мод включён. Перезапустите CDDA для применения." ) );
        return;
    }
    std::ofstream out( marker, std::ios::trunc );
    if( !out ) {
        popup( tr_ui( "Could not disable the mod.", "Не удалось выключить мод." ) );
        return;
    }
    out << "Disabled by NCMM Mod Configuration. Restart required.\n";
    out.close();
    popup( tr_ui( "Mod disabled. Restart CDDA to apply.",
                  "Мод выключен. Перезапустите CDDA для применения." ) );
}

void show_manager()
{
    write_diagnostics_summary();
    bool settings_dirty = false;
    std::string selected_module_id;
    const auto persist_settings = [&]() {
        if( !settings_dirty ) return true;
        if( manager_persist_settings() ) {
            settings_dirty = false;
            return true;
        }
        popup( tr_ui( "Could not save NCMM settings.",
                      "Не удалось сохранить настройки NCMM." ) );
        return false;
    };

    while( true ) {
        const std::vector<manager_entry> entries = manager_entries();
        if( entries.empty() ) {
            persist_settings();
            popup( tr_ui( "No NCMM mods are installed.", "Моды NCMM не установлены." ) );
            return;
        }

        int preferred_module = 0;
        if( !selected_module_id.empty() ) {
            for( int i = 0; i < static_cast<int>( entries.size() ); ++i ) {
                if( entries[static_cast<size_t>( i )].id == selected_module_id ) {
                    preferred_module = i;
                    break;
                }
            }
        }

        if( TERMX < 78 || TERMY < 24 ) {
            uilist menu;
            menu.text = tr_ui( "NCMM — Mod Configuration", "NCMM — Настройка модов" );
            for( int i = 0; i < static_cast<int>( entries.size() ); ++i ) {
                menu.addentry( i, true, MENU_AUTOASSIGN,
                               manager_state_marker( entries[i] ) + " " +
                               entries[i].name + "  " + entries[i].version );
            }
            menu.selected = preferred_module;
            menu.query();
            if( menu.ret < 0 || menu.ret >= static_cast<int>( entries.size() ) ) {
                if( persist_settings() ) return;
                continue;
            }

            const manager_entry &entry = entries[static_cast<size_t>( menu.ret )];
            selected_module_id = entry.id;
            const std::vector<const module_setting_meta *> settings = manager_settings_for( entry.id );
            loaded_mod *runtime = find_loaded_mutable( entry.directory );
            const bool has_open = runtime != nullptr && runtime->open_ui != nullptr;
            const bool has_world = manager_has_world_settings( entry.id );

            uilist action;
            action.text = entry.name + "  " + entry.version + "\n" + manager_state_label( entry );
            int index = 0, open_index = -1, world_index = -1, reset_index = -1;
            if( has_open ) {
                open_index = index++;
                action.addentry( open_index, true, MENU_AUTOASSIGN,
                                 tr_ui( "Open mod interface", "Открыть интерфейс мода" ) );
            }
            if( has_world ) {
                world_index = index++;
                action.addentry( world_index, true, MENU_AUTOASSIGN,
                                 tr_ui( "Open world settings", "Открыть настройки мира" ) );
            }
            if( !settings.empty() ) {
                reset_index = index++;
                action.addentry( reset_index, true, MENU_AUTOASSIGN,
                                 tr_ui( "Reset module settings", "Сбросить настройки мода" ) );
            }
            const int toggle_index = index;
            action.addentry( toggle_index, true, MENU_AUTOASSIGN,
                             entry.disabled ? tr_ui( "Enable mod", "Включить мод" ) :
                             tr_ui( "Disable mod", "Выключить мод" ) );
            action.query();

            if( open_index >= 0 && action.ret == open_index ) manager_open_module_ui( entry );
            else if( world_index >= 0 && action.ret == world_index ) {
                if( persist_settings() ) manager_open_world_settings();
            } else if( reset_index >= 0 && action.ret == reset_index ) {
                if( manager_confirm_reset( entry ) ) {
                    settings_dirty = manager_reset_module_settings( entry.id ) || settings_dirty;
                }
            } else if( action.ret == toggle_index ) manager_toggle_module( entry );
            continue;
        }

        const int frame_width = std::min( TERMX - 2, 118 );
        const int frame_height = std::min( TERMY - 2, 32 );
        const int left_width = std::max( 26, std::min( 36, frame_width / 3 ) );
        const int divider_x = left_width + 1;
        const int right_x = divider_x + 2;
        const int right_width = frame_width - right_x - 2;
        const int list_top = 3;
        const int list_bottom = frame_height - 3;
        const int visible_modules = std::max( 1, list_bottom - list_top + 1 );
        const point origin( ( TERMX - frame_width ) / 2, ( TERMY - frame_height ) / 2 );
        catacurses::window frame = catacurses::newwin( frame_height, frame_width, origin );

        input_context ctxt( "NCMM_MANAGER", keyboard_mode::keychar );
        ctxt.register_cardinal();
        ctxt.register_action( "NEXT_TAB" );
        ctxt.register_action( "CONFIRM" );
        ctxt.register_action( "QUIT" );
        ctxt.register_action( "HELP_KEYBINDINGS" );

        int selected_module = preferred_module;
        int first_module = 0;
        int focus = 0;
        int selected_detail = 0;
        int first_setting = 0;

        auto keep_module_visible = [&]() {
            if( selected_module < first_module ) first_module = selected_module;
            if( selected_module >= first_module + visible_modules ) {
                first_module = selected_module - visible_modules + 1;
            }
            first_module = std::max( 0, std::min( first_module,
                            std::max( 0, static_cast<int>( entries.size() ) - visible_modules ) ) );
        };
        keep_module_visible();

        while( true ) {
            const manager_entry &entry = entries[static_cast<size_t>( selected_module )];
            selected_module_id = entry.id;
            const std::vector<const module_setting_meta *> settings = manager_settings_for( entry.id );
            loaded_mod *runtime = find_loaded_mutable( entry.directory );
            const bool has_open = runtime != nullptr && runtime->open_ui != nullptr;
            const bool has_world = manager_has_world_settings( entry.id );
            const bool has_reset = !settings.empty();

            const int open_index = has_open ? static_cast<int>( settings.size() ) : -1;
            const int world_index = has_world ?
                                    static_cast<int>( settings.size() ) + ( has_open ? 1 : 0 ) : -1;
            const int reset_index = has_reset ?
                                    static_cast<int>( settings.size() ) + ( has_open ? 1 : 0 ) +
                                    ( has_world ? 1 : 0 ) : -1;
            const int toggle_index = static_cast<int>( settings.size() ) +
                                     ( has_open ? 1 : 0 ) + ( has_world ? 1 : 0 ) +
                                     ( has_reset ? 1 : 0 );
            const int detail_count = toggle_index + 1;
            selected_detail = std::max( 0, std::min( selected_detail, detail_count - 1 ) );

            ui_adaptor ui;
            ui.position_from_window( frame );
            ui.on_redraw( [&]( const ui_adaptor & ) {
                werase( frame );
                draw_border( frame, BORDER_COLOR );
                ncmm_trim_and_print_literal( frame, point( 2, 1 ), left_width - 2,
                                            focus == 0 ? c_light_green : c_white,
                                            tr_ui( "NCMM MODS", "МОДЫ NCMM" ) );
                std::string details_title = tr_ui( "MODULE DETAILS", "СВЕДЕНИЯ О МОДЕ" );
                if( settings_dirty ) details_title += " *";
                ncmm_trim_and_print_literal( frame, point( right_x, 1 ), right_width,
                                            focus == 1 ? c_light_green : c_white,
                                            details_title );

                for( int y = 1; y < frame_height - 1; ++y ) {
                    mvwprintz( frame, point( divider_x, y ), BORDER_COLOR, "|" );
                }

                for( int row = 0; row < visible_modules; ++row ) {
                    const int index = first_module + row;
                    if( index >= static_cast<int>( entries.size() ) ) break;
                    const manager_entry &candidate = entries[static_cast<size_t>( index )];
                    const bool active = index == selected_module;
                    std::string label = ( active ? "> " : "  " ) +
                                        manager_state_marker( candidate ) + " " +
                                        candidate.name;
                    if( !candidate.version.empty() ) label += " " + candidate.version;
                    ncmm_trim_and_print_literal( frame, point( 2, list_top + row ),
                                                left_width - 3,
                                                active ? ( focus == 0 ? c_light_green : c_cyan ) :
                                                manager_state_color( candidate ), label );
                }

                int y = 3;
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            c_white, entry.name );
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            c_light_gray,
                                            tr_ui( "Version: ", "Версия: " ) + entry.version );
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            manager_state_color( entry ),
                                            tr_ui( "Status: ", "Статус: " ) +
                                            manager_state_label( entry ) );
                if( !entry.default_hotkey.empty() ) {
                    ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                c_dark_gray,
                                                tr_ui( "Hotkey: ", "Горячая клавиша: " ) +
                                                entry.default_hotkey );
                }
                if( !entry.reason.empty() && entry.reason != "ok" ) {
                    ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                c_light_red, manager_reason_text( entry.reason ) );
                }
                if( !entry.description.empty() ) {
                    const std::vector<std::string> desc = foldstring( entry.description, right_width );
                    for( size_t i = 0; i < std::min<size_t>( 2, desc.size() ) &&
                         y < frame_height - 9; ++i ) {
                        ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                    c_light_gray, desc[i] );
                    }
                }
                ++y;

                const int action_count = ( has_open ? 1 : 0 ) + ( has_world ? 1 : 0 ) +
                                         ( has_reset ? 1 : 0 ) + 1;
                const int footer_y = frame_height - 2;
                const int tooltip_rows = 3;
                const int action_start = footer_y - tooltip_rows - action_count;
                const int settings_top = y + 1;
                const int visible_settings = std::max( 0, action_start - settings_top );

                if( selected_detail < static_cast<int>( settings.size() ) && visible_settings > 0 ) {
                    if( selected_detail < first_setting ) first_setting = selected_detail;
                    if( selected_detail >= first_setting + visible_settings ) {
                        first_setting = selected_detail - visible_settings + 1;
                    }
                } else if( static_cast<int>( settings.size() ) > visible_settings ) {
                    first_setting = std::max( 0, static_cast<int>( settings.size() ) - visible_settings );
                }
                first_setting = std::max( 0, std::min( first_setting,
                                std::max( 0, static_cast<int>( settings.size() ) - visible_settings ) ) );

                std::string settings_title = tr_ui( "SETTINGS", "НАСТРОЙКИ" );
                if( !settings.empty() && static_cast<int>( settings.size() ) > visible_settings &&
                    visible_settings > 0 ) {
                    settings_title += " " + std::to_string( first_setting + 1 ) + "-" +
                                      std::to_string( std::min<int>(
                                          static_cast<int>( settings.size() ),
                                          first_setting + visible_settings ) ) +
                                      "/" + std::to_string( settings.size() );
                }
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            c_white, settings_title );

                if( settings.empty() && y < action_start ) {
                    ncmm_trim_and_print_literal(
                        frame, point( right_x, y++ ), right_width, c_dark_gray,
                        has_world ?
                        tr_ui( "World-generation settings are available through the action below.",
                               "Настройки генерации мира доступны через действие ниже." ) :
                        tr_ui( "No inline settings.", "Нет встроенных настроек." ) );
                } else if( visible_settings > 0 ) {
                    const int end_setting = std::min<int>(
                                                static_cast<int>( settings.size() ),
                                                first_setting + visible_settings );
                    for( int index = first_setting; index < end_setting; ++index ) {
                        const module_setting_meta *setting = settings[static_cast<size_t>( index )];
                        const bool active = focus == 1 && index == selected_detail;
                        std::string row = ( active ? "> " : "  " ) + setting->name +
                                          "  < " + manager_setting_value( *setting ) + " >";
                        if( !manager_setting_is_default( *setting ) ) row += " *";
                        ncmm_trim_and_print_literal(
                            frame, point( right_x, settings_top + index - first_setting ),
                            right_width, active ? c_light_green : c_light_gray, row );
                    }
                }

                int action_y = action_start;
                if( has_open ) {
                    const bool active = focus == 1 && selected_detail == open_index;
                    ncmm_trim_and_print_literal( frame, point( right_x, action_y++ ), right_width,
                                                active ? c_light_green : c_cyan,
                                                ( active ? "> " : "  " ) +
                                                tr_ui( "[Open mod interface]", "[Открыть интерфейс мода]" ) );
                }
                if( has_world ) {
                    const bool active = focus == 1 && selected_detail == world_index;
                    ncmm_trim_and_print_literal( frame, point( right_x, action_y++ ), right_width,
                                                active ? c_light_green : c_cyan,
                                                ( active ? "> " : "  " ) +
                                                tr_ui( "[Open world settings]", "[Открыть настройки мира]" ) );
                }
                if( has_reset ) {
                    const bool active = focus == 1 && selected_detail == reset_index;
                    ncmm_trim_and_print_literal( frame, point( right_x, action_y++ ), right_width,
                                                active ? c_light_green : c_yellow,
                                                ( active ? "> " : "  " ) +
                                                tr_ui( "[Reset module settings]", "[Сбросить настройки мода]" ) );
                }
                {
                    const bool active = focus == 1 && selected_detail == toggle_index;
                    ncmm_trim_and_print_literal(
                        frame, point( right_x, action_y++ ), right_width,
                        active ? c_light_green : c_yellow,
                        ( active ? "> " : "  " ) +
                        ( entry.disabled ? tr_ui( "[Enable mod]", "[Включить мод]" ) :
                          tr_ui( "[Disable mod]", "[Выключить мод]" ) ) );
                }

                const int tooltip_top = action_start + action_count;
                if( selected_detail < static_cast<int>( settings.size() ) ) {
                    const module_setting_meta &setting =
                        *settings[static_cast<size_t>( selected_detail )];
                    const std::string meta =
                        tr_ui( "Applies: ", "Применение: " ) +
                        manager_setting_scope_label( setting.scope ) +
                        tr_ui( " | Default: ", " | По умолчанию: " ) +
                        manager_setting_default_value( setting );
                    ncmm_trim_and_print_literal( frame, point( right_x, tooltip_top ),
                                                right_width, c_dark_gray, meta );
                    const std::vector<std::string> tips = foldstring( setting.tooltip, right_width );
                    for( size_t i = 0; i < std::min<size_t>( 2, tips.size() ); ++i ) {
                        ncmm_trim_and_print_literal(
                            frame, point( right_x, tooltip_top + 1 + static_cast<int>( i ) ),
                            right_width, c_light_gray, tips[i] );
                    }
                } else {
                    std::string hint;
                    if( selected_detail == open_index ) {
                        hint = tr_ui( "Open this module's own interface.",
                                      "Открыть собственный интерфейс этого мода." );
                    } else if( selected_detail == world_index ) {
                        hint = tr_ui( "Open current/default CDDA world settings, including this module's world-generation controls.",
                                      "Открыть настройки мира CDDA, включая параметры генерации этого мода." );
                    } else if( selected_detail == reset_index ) {
                        hint = tr_ui( "Restore all inline settings for this module to their defaults.",
                                      "Вернуть встроенные настройки этого мода к значениям по умолчанию." );
                    } else if( selected_detail == toggle_index ) {
                        hint = tr_ui( "Module enable/disable changes take effect after restarting CDDA.",
                                      "Включение и выключение мода применяется после перезапуска CDDA." );
                    }
                    const std::vector<std::string> hints = foldstring( hint, right_width );
                    for( size_t i = 0; i < std::min<size_t>( 2, hints.size() ); ++i ) {
                        ncmm_trim_and_print_literal(
                            frame, point( right_x, tooltip_top + static_cast<int>( i ) ),
                            right_width, c_dark_gray, hints[i] );
                    }
                }

                std::string footer = tr_ui(
                    "Up/Down: select  Tab: panel  Left/Right: change  Enter: action  Esc: close",
                    "Вверх/вниз: выбор  Tab: панель  Влево/вправо: изменить  Enter: действие  Esc: выход" );
                if( settings_dirty ) footer += tr_ui( "  * unsaved", "  * не сохранено" );
                ncmm_trim_and_print_literal( frame, point( 2, footer_y ),
                                            frame_width - 4, c_dark_gray, footer );
                wnoutrefresh( frame );
            } );

            ui_manager::redraw();
            const std::string action = ctxt.handle_input();

            if( action == "QUIT" ) {
                if( persist_settings() ) return;
                continue;
            }
            if( action == "NEXT_TAB" || ( focus == 0 && action == "RIGHT" ) ||
                ( focus == 1 && action == "LEFT" && settings.empty() ) ) {
                focus = 1 - focus;
                continue;
            }
            if( focus == 0 ) {
                if( action == "UP" && selected_module > 0 ) {
                    --selected_module;
                    selected_module_id = entries[static_cast<size_t>( selected_module )].id;
                    selected_detail = 0;
                    first_setting = 0;
                    keep_module_visible();
                } else if( action == "DOWN" &&
                           selected_module + 1 < static_cast<int>( entries.size() ) ) {
                    ++selected_module;
                    selected_module_id = entries[static_cast<size_t>( selected_module )].id;
                    selected_detail = 0;
                    first_setting = 0;
                    keep_module_visible();
                } else if( action == "CONFIRM" ) {
                    focus = 1;
                }
                continue;
            }

            if( action == "UP" && selected_detail > 0 ) { --selected_detail; continue; }
            if( action == "DOWN" && selected_detail + 1 < detail_count ) {
                ++selected_detail;
                continue;
            }

            if( selected_detail < static_cast<int>( settings.size() ) ) {
                if( action == "LEFT" ) {
                    settings_dirty = manager_adjust_setting(
                                         *settings[static_cast<size_t>( selected_detail )], -1 ) ||
                                     settings_dirty;
                } else if( action == "RIGHT" || action == "CONFIRM" ) {
                    settings_dirty = manager_adjust_setting(
                                         *settings[static_cast<size_t>( selected_detail )], 1 ) ||
                                     settings_dirty;
                }
                continue;
            }

            if( selected_detail == open_index && action == "CONFIRM" ) {
                manager_open_module_ui( entry );
                continue;
            }
            if( selected_detail == world_index && action == "CONFIRM" ) {
                if( persist_settings() ) manager_open_world_settings();
                continue;
            }
            if( selected_detail == reset_index && action == "CONFIRM" ) {
                if( manager_confirm_reset( entry ) ) {
                    settings_dirty = manager_reset_module_settings( entry.id ) || settings_dirty;
                }
                continue;
            }
            if( selected_detail == toggle_index && action == "CONFIRM" ) {
                selected_module_id = entry.id;
                manager_toggle_module( entry );
                break;
            }
            if( action == "LEFT" ) focus = 0;
        }
    }
}

void on_turn()
{
    if( !api_v2_world_announced && character_state_available() ) {
        api_v2_world_announced = true;
        dispatch_event_v2( NCMM_EVENT_WORLD_LOADED_V2 );
    }
    dispatch_event_v2( NCMM_EVENT_TURN_V2 );
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
    dispatch_event_v2( NCMM_EVENT_LOCALE_CHANGED_V2 );
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

bool command_line_flag_present( const char *flag )
{
#ifdef _WIN32
    if( flag == nullptr || *flag == '\0' ) {
        return false;
    }
    const char *raw = GetCommandLineA();
    if( raw == nullptr ) {
        return false;
    }
    const std::string command_line( raw );
    const std::string needle( flag );
    size_t pos = command_line.find( needle );
    while( pos != std::string::npos ) {
        const size_t end = pos + needle.size();
        const bool left_boundary = pos == 0 ||
                                   std::isspace( static_cast<unsigned char>( command_line[pos - 1] ) ) ||
                                   command_line[pos - 1] == '"';
        const bool right_boundary = end == command_line.size() ||
                                    std::isspace( static_cast<unsigned char>( command_line[end] ) ) ||
                                    command_line[end] == '"';
        if( left_boundary && right_boundary ) {
            return true;
        }
        pos = command_line.find( needle, end );
    }
#endif
    return false;
}

bool runtime_smoke_requested()
{
    return command_line_flag_present( "--ncmm-runtime-smoke" );
}

bool gameplay_smoke_requested()
{
    return command_line_flag_present( "--ncmm-gameplay-smoke" );
}

uint32_t gameplay_smoke_rng_next( uint32_t &state )
{
    state = state * 1664525u + 1013904223u;
    return state;
}

void write_gameplay_smoke_result( bool success, const std::string &reason,
                                  size_t aws_settings, size_t aws_hooks,
                                  size_t survivor_perks )
{
    std::filesystem::create_directories( game_root() / "ncmm" );
    std::ofstream out( game_root() / "ncmm" / "gameplay-smoke.json",
                       std::ios::binary | std::ios::trunc );
    if( !out ) {
        return;
    }
    out << "{\n"
        << "  \"schema\": 1,\n"
        << "  \"success\": " << ( success ? "true" : "false" ) << ",\n"
        << "  \"reason\": \"" << reason << "\",\n"
        << "  \"aws_settings\": " << aws_settings << ",\n"
        << "  \"aws_hooks\": " << aws_hooks << ",\n"
        << "  \"survivor_perks\": " << survivor_perks << "\n"
        << "}\n";
}

int run_gameplay_smoke()
{
#ifndef _WIN32
    write_gameplay_smoke_result( false, "windows_only", 0, 0, 0 );
    return 96;
#else
    constexpr const char *aws_id = "advanced_world_settings";
    constexpr const char *survivor_id = "survivor_progression";
    constexpr const char *world_name = "NCMM Gameplay Smoke";

    size_t aws_setting_count = 0;
    size_t aws_hook_count = 0;
    size_t survivor_perk_count = 0;

    try {
        loaded_mod *aws = find_loaded_by_id( aws_id );
        loaded_mod *survivor = find_loaded_by_id( survivor_id );
        if( aws == nullptr || survivor == nullptr || survivor->handle == nullptr ) {
            write_gameplay_smoke_result( false, "required_module_missing", 0, 0, 0 );
            return 97;
        }

        // Randomize every AWS NEW_MAP option through the real CDDA cOpt object.
        // setNext() guarantees values remain legal for bool/int/float/select options.
        std::map<std::string, std::string> expected_world_values;
        uint32_t rng = 0xA75EED42u;
        for( const auto &owner : world_setting_owners ) {
            if( owner.second != aws_id ) {
                continue;
            }
            const auto scope_it = world_setting_scopes.find( owner.first );
            if( scope_it == world_setting_scopes.end() ||
                scope_it->second < NCMM_WORLD_SETTING_NEW_MAP ||
                !get_options().has_option( owner.first ) ) {
                continue;
            }

            options_manager::cOpt &opt = get_options().get_option( owner.first );
            const uint32_t advances = 1u + gameplay_smoke_rng_next( rng ) % 11u;
            for( uint32_t i = 0; i < advances; ++i ) {
                opt.setNext();
            }
            ++aws_setting_count;
        }

        if( aws_setting_count != 50 || !get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) ) {
            write_gameplay_smoke_result( false, "aws_registration_count", aws_setting_count, 0, 0 );
            return 98;
        }

        // Real-Host regression coverage for selective geography scopes. Turning
        // a scope off must make its concrete hooks appear unbound so patched
        // worldgen falls back to active region_settings / region-overlay values.
        struct aws_scope_probe {
            const char *setting_id;
            const char *scope_hook;
            const char *concrete_hook;
        };
        const aws_scope_probe scope_probes[] = {
            { "NCMM_AWS_SCOPE_CITIES", "geography.scope.cities.enabled", "geography.city.size" },
            { "NCMM_AWS_SCOPE_ECOLOGY", "geography.scope.ecology.enabled", "geography.forests.threshold" },
            { "NCMM_AWS_SCOPE_WATER", "geography.scope.water.enabled", "geography.lakes.threshold" },
            { "NCMM_AWS_SCOPE_TRANSPORT", "geography.scope.transport.enabled", "geography.highways.grid_row" }
        };
        for( const aws_scope_probe &probe : scope_probes ) {
            if( !get_options().has_option( probe.setting_id ) ) {
                write_gameplay_smoke_result( false, "aws_scope_setting_missing",
                                             aws_setting_count, 0, 0 );
                return 123;
            }
            options_manager::cOpt &scope_opt = get_options().get_option( probe.setting_id );
            const std::string original_value = scope_opt.getValue();

            scope_opt.setValue( "false" );
            const bool scope_queryable = worldgen_hook_bound( probe.scope_hook );
            const bool concrete_bound_when_off = worldgen_hook_bound( probe.concrete_hook );

            scope_opt.setValue( "true" );
            const bool concrete_bound_when_on = worldgen_hook_bound( probe.concrete_hook );

            scope_opt.setValue( original_value );
            if( !scope_queryable || concrete_bound_when_off || !concrete_bound_when_on ) {
                write_gameplay_smoke_result( false, "aws_scope_fallback_mismatch",
                                             aws_setting_count, 0, 0 );
                return 124;
            }
        }

        if( worldgen_hook_bound( "geography.specials.enabled" ) ||
            worldgen_hook_bound( "geography.neighbor_connections.enabled" ) ) {
            write_gameplay_smoke_result( false, "aws_protected_hook_exposed",
                                         aws_setting_count, 0, 0 );
            return 125;
        }

        log_line( NCMM_LOG_INFO,
                  "NCMM gameplay smoke checkpoint: AWS selective scope fallback + protected hooks PASS." );

        get_options().get_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ).setValue( "true" );

        // Keep the only explicit min/max pair valid after independent randomization.
        if( get_options().has_option( "NCMM_AWS_FLOODPLAIN_MIN" ) &&
            get_options().has_option( "NCMM_AWS_FLOODPLAIN_MAX" ) ) {
            options_manager::cOpt &min_opt = get_options().get_option( "NCMM_AWS_FLOODPLAIN_MIN" );
            options_manager::cOpt &max_opt = get_options().get_option( "NCMM_AWS_FLOODPLAIN_MAX" );
            if( min_opt.value_as<int>() > max_opt.value_as<int>() ) {
                max_opt.setValue( min_opt.value_as<int>() );
            }
        }

        // The semantic matrix owns min/max stress.  The real lifecycle should
        // exercise a representative randomized world instead of accidentally
        // turning one overmap into a worst-case ravine benchmark.
        if( get_options().has_option( "NCMM_AWS_RAVINE_COUNT" ) &&
            get_options().get_option( "NCMM_AWS_RAVINE_COUNT" ).value_as<int>() > 1 ) {
            get_options().get_option( "NCMM_AWS_RAVINE_COUNT" ).setValue( 1 );
        }
        if( get_options().has_option( "NCMM_AWS_RAVINE_RANGE" ) &&
            get_options().get_option( "NCMM_AWS_RAVINE_RANGE" ).value_as<int>() > 45 ) {
            get_options().get_option( "NCMM_AWS_RAVINE_RANGE" ).setValue( 45 );
        }
        if( get_options().has_option( "NCMM_AWS_RAVINE_WIDTH" ) &&
            get_options().get_option( "NCMM_AWS_RAVINE_WIDTH" ).value_as<int>() > 3 ) {
            get_options().get_option( "NCMM_AWS_RAVINE_WIDTH" ).setValue( 3 );
        }
        if( get_options().has_option( "NCMM_AWS_RAVINE_DEPTH" ) &&
            get_options().get_option( "NCMM_AWS_RAVINE_DEPTH" ).value_as<int>() < -3 ) {
            get_options().get_option( "NCMM_AWS_RAVINE_DEPTH" ).setValue( -3 );
        }
        log_line( NCMM_LOG_INFO,
                  "NCMM gameplay smoke: randomized AWS profile normalized for representative-cost ravine generation." );

        for( const auto &owner : world_setting_owners ) {
            if( owner.second == aws_id && get_options().has_option( owner.first ) ) {
                expected_world_values[owner.first] = get_options().get_option( owner.first ).getValue();
            }
        }

        const std::vector<mod_id> mods = world_generator->get_mod_manager().get_default_mods();
        WORLD *world = world_generator->make_new_world( world_name, mods );
        if( world == nullptr ) {
            write_gameplay_smoke_result( false, "world_create_failed", aws_setting_count, 0, 0 );
            return 99;
        }

        for( const auto &expected : expected_world_values ) {
            const auto it = world->WORLD_OPTIONS.find( expected.first );
            if( it == world->WORLD_OPTIONS.end() || it->second.getValue() != expected.second ) {
                write_gameplay_smoke_result( false, "aws_world_copy_mismatch",
                                             aws_setting_count, 0, 0 );
                return 100;
            }
        }

        if( !world->save() ) {
            write_gameplay_smoke_result( false, "world_save_failed", aws_setting_count, 0, 0 );
            return 101;
        }

        // Prove persistence by reloading the world through the real worldfactory.
        world_generator->set_active_world( nullptr );
        world_generator->init();
        WORLD *reloaded = world_generator->get_world( world_name );
        if( reloaded == nullptr ) {
            write_gameplay_smoke_result( false, "world_reload_failed", aws_setting_count, 0, 0 );
            return 102;
        }
        for( const auto &expected : expected_world_values ) {
            const auto it = reloaded->WORLD_OPTIONS.find( expected.first );
            if( it == reloaded->WORLD_OPTIONS.end() || it->second.getValue() != expected.second ) {
                write_gameplay_smoke_result( false, "aws_world_reload_mismatch",
                                             aws_setting_count, 0, 0 );
                return 103;
            }
        }
        world_generator->set_active_world( reloaded );

        // Verify every real Host worldgen binding now reads the reloaded WORLD_OPTIONS.
        for( const auto &binding : worldgen_bindings_v2 ) {
            if( binding.second.module_id != aws_id ) {
                continue;
            }
            const auto opt_it = reloaded->WORLD_OPTIONS.find( binding.second.setting_id );
            if( opt_it == reloaded->WORLD_OPTIONS.end() ) {
                write_gameplay_smoke_result( false, "aws_binding_option_missing",
                                             aws_setting_count, aws_hook_count, 0 );
                return 104;
            }
            const options_manager::cOpt &opt = opt_it->second;
            bool matches = false;
            if( binding.second.value_type == NCMM_WORLDGEN_BOOL_V2 ) {
                matches = worldgen_hook_bool_v2( binding.first.c_str(), -7 ) ==
                          ( opt.value_as<bool>() ? 1 : 0 );
            } else if( binding.second.value_type == NCMM_WORLDGEN_INT_V2 ) {
                matches = worldgen_hook_i64_v2( binding.first.c_str(), -777777 ) ==
                          static_cast<int64_t>( opt.value_as<int>() );
            } else if( binding.second.value_type == NCMM_WORLDGEN_FLOAT_V2 ) {
                matches = std::abs( worldgen_hook_f64_v2( binding.first.c_str(), -777777.0 ) -
                                    static_cast<double>( opt.value_as<float>() ) ) < 0.00001;
            }
            if( !matches ) {
                write_gameplay_smoke_result( false, "aws_binding_value_mismatch",
                                             aws_setting_count, aws_hook_count, 0 );
                return 105;
            }
            ++aws_hook_count;
        }
        if( aws_hook_count != 50 ) {
            write_gameplay_smoke_result( false, "aws_binding_count",
                                         aws_setting_count, aws_hook_count, 0 );
            return 106;
        }

        // Enter the same world initialization path used for a real new character.
        // game::setup() owns the ordering constraints around calendar state, mod
        // validation, core/mod loading, and DynamicDataLoader finalization.
        log_line( NCMM_LOG_INFO,
                  "NCMM gameplay smoke checkpoint: AWS save/reload + 50 bindings PASS; running game setup." );
        g->setup();
        log_line( NCMM_LOG_INFO, "NCMM gameplay smoke checkpoint: game setup complete." );
        overmap_buffer.init_region_layout();
        log_line( NCMM_LOG_INFO, "NCMM gameplay smoke checkpoint: region layout initialized; generating overmap." );
        overmap_special_batch empty_specials( point_abs_om{} );
        overmap_buffer.create_custom_overmap( point_abs_om{}, empty_specials );
        log_line( NCMM_LOG_INFO, "NCMM gameplay smoke checkpoint: overmap generation complete." );

        get_avatar() = avatar();
        get_avatar().create( character_type::NOW );
        get_avatar().setID( g->assign_npc_id(), false );
        g->new_game = false;
        on_turn();
        log_line( NCMM_LOG_INFO, "NCMM gameplay smoke checkpoint: real avatar initialized." );

        using perk_count_fn = size_t ( * )();
        using perk_id_fn = const char *( * )( size_t );
        using perk_max_rank_fn = int ( * )( size_t );
        using perk_reset_fn = int ( * )();
        using perk_set_rank_fn = int ( * )( size_t, int );
        using perk_recalc_fn = int ( * )();

        const auto perk_count = reinterpret_cast<perk_count_fn>(
                                    GetProcAddress( survivor->handle, "ncmm_test_perk_count_v1" ) );
        const auto perk_id = reinterpret_cast<perk_id_fn>(
                                 GetProcAddress( survivor->handle, "ncmm_test_perk_id_v1" ) );
        const auto perk_max_rank = reinterpret_cast<perk_max_rank_fn>(
                                       GetProcAddress( survivor->handle, "ncmm_test_perk_max_rank_v1" ) );
        const auto perk_reset = reinterpret_cast<perk_reset_fn>(
                                    GetProcAddress( survivor->handle, "ncmm_test_reset_all_perks_v1" ) );
        const auto perk_set_rank = reinterpret_cast<perk_set_rank_fn>(
                                       GetProcAddress( survivor->handle, "ncmm_test_set_perk_rank_v1" ) );
        const auto perk_recalc = reinterpret_cast<perk_recalc_fn>(
                                     GetProcAddress( survivor->handle, "ncmm_test_recalculate_v1" ) );
        if( !perk_count || !perk_id || !perk_max_rank || !perk_reset ||
            !perk_set_rank || !perk_recalc ) {
            write_gameplay_smoke_result( false, "survivor_test_surface_missing",
                                         aws_setting_count, aws_hook_count, 0 );
            return 107;
        }

        module_call_scope survivor_scope( survivor_id );
        survivor_perk_count = perk_count();
        log_line( NCMM_LOG_INFO, "NCMM gameplay smoke checkpoint: Survivor test surface resolved." );
        if( survivor_perk_count != 369 || !perk_reset() || !perk_recalc() ) {
            write_gameplay_smoke_result( false, "survivor_catalog_or_reset",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 108;
        }

        const auto find_perk_index = [&]( const char *wanted ) {
            for( size_t i = 0; i < survivor_perk_count; ++i ) {
                const char *id = perk_id( i );
                if( id != nullptr && std::string( id ) == wanted ) {
                    return i;
                }
            }
            return survivor_perk_count;
        };

        // First prove that the exact release DLL can hold all 369 perks at max rank
        // simultaneously and recompute its aggregate state without crashing or
        // dropping the Host modifier channel.  Gameplay assertions below are then
        // isolated per consumer to avoid false failures from CDDA's stat caps.
        for( size_t i = 0; i < survivor_perk_count; ++i ) {
            const int rank = perk_max_rank( i );
            if( rank < 1 || !perk_set_rank( i, rank ) ) {
                write_gameplay_smoke_result( false, "survivor_grant_all_failed",
                                             aws_setting_count, aws_hook_count, survivor_perk_count );
                return 109;
            }
        }
        if( !perk_recalc() || character_modifier_values.find( survivor_id ) ==
            character_modifier_values.end() ) {
            write_gameplay_smoke_result( false, "survivor_recalculate_failed",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 110;
        }
        log_line( NCMM_LOG_INFO, "NCMM gameplay smoke checkpoint: Survivor 369-perk aggregate recompute PASS." );
        if( !perk_reset() || !perk_recalc() ) {
            write_gameplay_smoke_result( false, "survivor_post_aggregate_reset_failed",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 111;
        }

        const size_t c_power = find_perk_index( "c_power" );
        const size_t c_reflexes = find_perk_index( "c_reflexes" );
        const size_t m_light = find_perk_index( "m_light" );
        const size_t g_observer = find_perk_index( "g_observer" );
        const size_t a_focus = find_perk_index( "a_focus" );
        const size_t m_stride = find_perk_index( "m_stride" );
        const size_t m_cardio = find_perk_index( "m_cardio" );
        const size_t ce_drills = find_perk_index( "ce_drills" );
        const size_t g_hauler = find_perk_index( "g_hauler" );
        const size_t required_indices[] = {
            c_power, c_reflexes, m_light, g_observer, a_focus,
            m_stride, m_cardio, ce_drills, g_hauler
        };
        for( size_t index : required_indices ) {
            if( index >= survivor_perk_count ) {
                write_gameplay_smoke_result( false, "survivor_representative_perk_missing",
                                             aws_setting_count, aws_hook_count, survivor_perk_count );
                return 112;
            }
        }

        const auto prepare_single = [&]( size_t index ) {
            return perk_reset() && perk_recalc() && perk_set_rank( index, 1 ) && perk_recalc();
        };

        // Primary Character stats.
        if( !perk_reset() || !perk_recalc() ) return 113;
        const int base_str = get_avatar().get_str();
        if( !prepare_single( c_power ) ||
            get_avatar().get_str() != base_str +
            static_cast<int>( std::lround( gameplay_modifier( "str_flat" ) ) ) ) {
            write_gameplay_smoke_result( false, "survivor_real_strength_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 113;
        }

        if( !perk_reset() || !perk_recalc() ) return 114;
        const int base_dex = get_avatar().get_dex();
        if( !prepare_single( c_reflexes ) ||
            get_avatar().get_dex() != base_dex +
            static_cast<int>( std::lround( gameplay_modifier( "dex_flat" ) ) ) ) {
            write_gameplay_smoke_result( false, "survivor_real_dexterity_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 114;
        }

        // Test move cost independently from DEX. Character::run_cost performs its
        // internal calculation in float and exposes only a truncated int, so the
        // exact pre-modifier fractional part is intentionally hidden from this
        // black-box smoke. Derive the tight integer interval that can result from
        // any hidden fraction in [base, base + 1) instead of assuming it was zero.
        if( !perk_reset() || !perk_recalc() ) return 115;
        const int base_run_cost = get_avatar().run_cost( 100, false );
        if( !prepare_single( m_light ) ) return 115;
        const double move_multiplier = std::max(
                                           0.25, 1.0 + gameplay_modifier( "move_cost_pct" ) / 100.0 );
        const int expected_run_cost_min = std::max(
                                              1, static_cast<int>( base_run_cost * move_multiplier ) );
        const int expected_run_cost_max = std::max(
                                              expected_run_cost_min,
                                              static_cast<int>( std::ceil(
                                                      ( base_run_cost + 1.0 ) * move_multiplier ) ) - 1 );
        const int actual_run_cost = get_avatar().run_cost( 100, false );
        if( actual_run_cost < expected_run_cost_min || actual_run_cost > expected_run_cost_max ) {
            log_line( NCMM_LOG_WARN,
                      ( "Survivor move-cost smoke: base=" + std::to_string( base_run_cost ) +
                        " modifier=" + std::to_string( gameplay_modifier( "move_cost_pct" ) ) +
                        " expected=[" + std::to_string( expected_run_cost_min ) + "," +
                        std::to_string( expected_run_cost_max ) + "] actual=" +
                        std::to_string( actual_run_cost ) ).c_str() );
            write_gameplay_smoke_result( false, "survivor_real_move_cost_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 115;
        }

        if( !perk_reset() || !perk_recalc() ) return 116;
        const int base_per = get_avatar().get_per();
        if( !prepare_single( g_observer ) ||
            get_avatar().get_per() != base_per +
            static_cast<int>( std::lround( gameplay_modifier( "per_flat" ) ) ) ) {
            write_gameplay_smoke_result( false, "survivor_real_perception_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 116;
        }

        if( !perk_reset() || !perk_recalc() ) return 117;
        const int base_int = get_avatar().get_int();
        if( !prepare_single( a_focus ) ||
            get_avatar().get_int() != base_int +
            static_cast<int>( std::lround( gameplay_modifier( "int_flat" ) ) ) ) {
            write_gameplay_smoke_result( false, "survivor_real_intelligence_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 117;
        }

        // Speed and stamina multipliers.
        if( !perk_reset() || !perk_recalc() ) return 118;
        const int base_speed = get_avatar().get_speed();
        if( !prepare_single( m_stride ) ) return 118;
        const int expected_speed = std::max(
                                       1, static_cast<int>( std::lround(
                                               base_speed * std::max(
                                                   0.1, 1.0 + gameplay_modifier( "speed_pct" ) / 100.0 ) ) ) );
        if( get_avatar().get_speed() != expected_speed ) {
            write_gameplay_smoke_result( false, "survivor_real_speed_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 118;
        }

        if( !perk_reset() || !perk_recalc() ) return 119;
        const int base_stamina = get_avatar().get_stamina_max();
        if( !prepare_single( m_cardio ) ) return 119;
        const int expected_stamina = std::max(
                                         1, static_cast<int>( std::lround(
                                                 base_stamina * std::max(
                                                     0.1, 1.0 + gameplay_modifier( "stamina_max_pct" ) / 100.0 ) ) ) );
        if( get_avatar().get_stamina_max() != expected_stamina ) {
            write_gameplay_smoke_result( false, "survivor_real_stamina_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 119;
        }

        // Melee hit reaches the actual Character::get_hit_base consumer.
        if( !perk_reset() || !perk_recalc() ) return 120;
        const float base_hit = get_avatar().get_hit_base();
        if( !prepare_single( ce_drills ) ) return 120;
        const float expected_hit = base_hit +
                                   static_cast<float>( gameplay_modifier( "melee_hit_flat" ) );
        if( std::abs( get_avatar().get_hit_base() - expected_hit ) > 0.0001f ) {
            write_gameplay_smoke_result( false, "survivor_real_melee_hit_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 120;
        }

        // Carry capacity reaches Character::weight_capacity and units::mass.
        if( !perk_reset() || !perk_recalc() ) return 121;
        const auto base_capacity = get_avatar().weight_capacity();
        if( !prepare_single( g_hauler ) ) return 121;
        const double carry_multiplier = std::max(
                                            0.0, 1.0 + gameplay_modifier( "carry_weight_pct" ) / 100.0 );
        const auto expected_capacity_value = static_cast<decltype( base_capacity.value() )>(
                std::llround( static_cast<double>( base_capacity.value() ) * carry_multiplier ) );
        if( std::llabs( static_cast<long long>( get_avatar().weight_capacity().value() ) -
                        static_cast<long long>( expected_capacity_value ) ) > 1 ) {
            write_gameplay_smoke_result( false, "survivor_real_carry_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 121;
        }

        // Final reset proves the real Character consumers return to their vanilla baseline.
        if( !perk_reset() || !perk_recalc() ||
            std::abs( gameplay_modifier( "str_flat" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "dex_flat" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "per_flat" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "int_flat" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "speed_pct" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "stamina_max_pct" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "move_cost_pct" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "carry_weight_pct" ) ) > 0.000001 ||
            std::abs( gameplay_modifier( "melee_hit_flat" ) ) > 0.000001 ) {
            write_gameplay_smoke_result( false, "survivor_real_cleanup_mismatch",
                                         aws_setting_count, aws_hook_count, survivor_perk_count );
            return 122;
        }

        write_gameplay_smoke_result( true, "ok", aws_setting_count,
                                     aws_hook_count, survivor_perk_count );
        log_line( NCMM_LOG_INFO,
                  "NCMM gameplay smoke PASS: real AWS world save/reload/overmap + Survivor 369-perk aggregate plus isolated Character consumers." );
        return 0;
    } catch( const std::exception &err ) {
        log_line( NCMM_LOG_ERROR, ( std::string( "NCMM gameplay smoke exception: " ) + err.what() ).c_str() );
        write_gameplay_smoke_result( false, "exception", aws_setting_count,
                                     aws_hook_count, survivor_perk_count );
        return 116;
    } catch( ... ) {
        log_line( NCMM_LOG_ERROR, "NCMM gameplay smoke unknown exception." );
        write_gameplay_smoke_result( false, "unknown_exception", aws_setting_count,
                                     aws_hook_count, survivor_perk_count );
        return 117;
    }
#endif
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
    character_modifier_totals.clear();
    for( const auto &owned : modifier_owners_v2 ) {
        character_modifier_limits.erase( owned.first );
    }
    modifier_owners_v2.clear();
    event_subscriptions_v2.clear();
    runtime_hook_rules_v2.clear();
    worldgen_bindings_v2.clear();
    runtime_source_mod_context_v2.clear();
    api_v2_world_announced = false;
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

    dispatch_event_v2( NCMM_EVENT_HOST_READY_V2 );
    write_modules_state();
    mark_ready();

    if( runtime_smoke_requested() ) {
        log_line( NCMM_LOG_INFO,
                  "NCMM runtime smoke reached Host ready state; exiting before main menu." );
        std::exit( 0 );
    }
}

void load_module_data()
{
    DynamicDataLoader &loader = DynamicDataLoader::get_instance();
    for( const loaded_mod &runtime : loaded ) {
        if( runtime.descriptor == nullptr || runtime.descriptor->id == nullptr ) continue;
        const std::filesystem::path data_dir = runtime.directory / "data";
        if( !std::filesystem::exists( data_dir ) || !std::filesystem::is_directory( data_dir ) ) continue;
        const std::string source = "ncmm:" + std::string( runtime.descriptor->id );
        log_line( NCMM_LOG_INFO, ( "Loading module data: " + source + " -> " + data_dir.string() ).c_str() );
        loader.load_data_from_path( cata_path{ cata_path::root_path::unknown, data_dir }, source );
    }
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
    if( api_v2_world_announced ) {
        dispatch_event_v2( NCMM_EVENT_WORLD_UNLOADED_V2 );
        api_v2_world_announced = false;
    }
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
            erase_module_modifiers( module_id );
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
    character_modifier_totals.clear();
    for( const auto &owned : modifier_owners_v2 ) {
        character_modifier_limits.erase( owned.first );
    }
    modifier_owners_v2.clear();
    event_subscriptions_v2.clear();
    runtime_hook_rules_v2.clear();
    worldgen_bindings_v2.clear();
    runtime_source_mod_context_v2.clear();
    api_v2_world_announced = false;
    active_module_id.clear();
}
} // namespace ncmm
