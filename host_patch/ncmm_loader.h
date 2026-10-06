class Creature;
class Character;
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class input_context;
class item;
class item_location;
class avatar;

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
bool handle_item_activation( const item_location &loc );
void show_manager();
std::string settings_menu_label();
std::string version_label();
std::string localized_text( const char *english, const char *russian );

/** Record one successfully completed craft for gameplay.metrics.v1. */
void gameplay_metric_record_completed_craft( const Character &who );

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
/** Action-specific gun selection. Never obtains or physically wields an item.
 * fire requires a ranged mode; controls also accept guns in a melee mode.
 * Empty guns remain candidates so aim can reload them in place.
 */
enum class ranged_weapon_action { fire, controls, reload };
bool ranged_weapon_capable( const item &weapon, ranged_weapon_action action );
/** Primary melee resolver for logical Mana Hands. Physical wielded items keep priority. */
item *primary_mana_hand_melee_weapon( Character &who );
/** Validate an already-selected Mana Hand firearm without inventory or slot reselection. */
bool ranged_weapon_binding_valid( const avatar &who, const item &weapon );
std::vector<item_location> ranged_weapon_candidates( avatar &who, ranged_weapon_action action );
item_location select_ranged_weapon( avatar &who, ranged_weapon_action action,
                                    const char *prompt_en, const char *prompt_ru );
std::string ranged_weapon_label( const item &weapon );
bool virtual_item_matches_slot( const item &candidate, const char *module_id,
                                const char *slot_id );
bool virtual_item_can_assign( const char *module_id, const char *slot_id,
                              const item_location &loc, uint32_t flags );
bool virtual_item_assign( const char *module_id, const char *slot_id,
                          const item_location &loc, uint32_t flags );
bool virtual_item_clear( const char *module_id, const char *slot_id );
bool mana_hand_inventory_action_visible( const item_location &loc );
bool mana_hand_inventory_action( item_location loc );
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
