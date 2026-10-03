class Creature;
class Character;
#pragma once
#include <cstdint>
#include <string>

class input_context;
class item;
class item_location;

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
std::string localized_text( const char *english, const char *russian );


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
// Internal CDDA UI helpers; not part of the module API/ABI.
bool inventory_symbols_enabled( bool vanilla_symbols );
std::string inventory_item_symbol( const item &it );

/* Engine-side bridge for Host API 2.1 logical item slots. */
item *virtual_item_for_slot( const char *module_id, const char *slot_id );
bool virtual_item_can_assign( const char *module_id, const char *slot_id,
                              const item_location &loc, uint32_t flags );
bool virtual_item_assign( const char *module_id, const char *slot_id,
                          const item_location &loc, uint32_t flags );
bool virtual_item_clear( const char *module_id, const char *slot_id );
bool release_virtual_item( item &it );
bool is_virtual_item( const item &it );
bool virtual_melee_context_begin( Character &who, item &weapon,
                                 bool suppress_martial_arts = true );
void virtual_melee_context_end( Character &who );
bool virtual_melee_context_active( const Character &who );
bool virtual_melee_context_suppresses_martial_arts( const Character &who );
item *virtual_melee_context_item( const Character &who );
bool virtual_melee_context_is_wielding( const Character &who, const item &it );
bool virtual_item_secondary_melee_enabled( const item &it );
bool virtual_item_primary_melee_enabled( const item &it );
bool virtual_item_set_primary_melee( item &it, bool enabled );
bool virtual_item_set_secondary_melee( item &it, bool enabled );
} // namespace ncmm
