class Creature;
#pragma once
#include <cstdint>
#include <string>

class input_context;

namespace ncmm
{
void initialize();
void load_module_data();
void shutdown();
void mark_ready();
bool gameplay_smoke_requested();
int run_gameplay_smoke();
void on_turn();
void on_language_changed();
void register_gameplay_actions( input_context &ctxt );
bool handle_gameplay_action( const std::string &action );
void show_manager();
std::string settings_menu_label();
std::string version_label();


/** Aggregate runtime gameplay modifier registered by loaded NCMM modules. */
double gameplay_modifier( const char *modifier_id );

/** Host-owned generic integration points. Individual modules register rules/bindings through API 2.0. */
double runtime_hook_modifier( const char *hook_id, const char *subject_id = nullptr,
                              const char *source_mod_id = nullptr,
                              const char *source_species_id = nullptr,
                              const char *target_species_id = nullptr );
void runtime_event_notify( uint32_t event_id );
std::string runtime_source_mod_swap( const std::string &source_mod_id );
const std::string &runtime_source_mod();
double runtime_hook_modifier_for_creatures( const char *hook_id,
        const Creature *source, const Creature *target );
bool worldgen_hook_bound( const char *hook_id );
int worldgen_hook_bool( const char *hook_id, int fallback );
int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback );
double worldgen_hook_f64( const char *hook_id, double fallback );

/** Generic LIVE/RELOAD typed-setting hooks consumed by engine UI/runtime code. */
bool runtime_setting_hook_bound( const char *hook_id );
int runtime_setting_hook_bool( const char *hook_id, int fallback );
int64_t runtime_setting_hook_i64( const char *hook_id, int64_t fallback );
double runtime_setting_hook_f64( const char *hook_id, double fallback );
} // namespace ncmm
