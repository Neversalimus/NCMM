#pragma once
#include <string>

// Host-internal policy. Instantiated with CDDA item in ncmm_loader.cpp and with
// a predicate fixture in tests; there is only one ordered classifier.
namespace ncmm
{
namespace item_glyphs
{
inline bool symbol_slot_enabled( bool semantic_enabled, bool vanilla_symbols )
{
    return semantic_enabled || vanilla_symbols;
}

template<typename Item>
const char *classify( const Item &it )
{
    const std::string category = it.get_category_shallow().get_id().str();

    if( it.is_dangerous() ) return u8"\u26A0";
    if( it.is_corpse() || category == "corpses" ) return u8"\u2620";
    if( it.is_money() || it.is_cash_card() || category == "currency" ) return u8"\u00A4";
    if( it.is_battery() || it.is_vehicle_battery() || it.is_fuel() || category == "fuel" ) {
        return u8"\u26A1";
    }
    if( it.is_gun() || category == "guns" ) return u8"\u2316";
    if( it.is_ammo() || it.is_magazine() || category == "ammo" ||
        category == "magazines" || category == "tool_magazine" ) {
        return u8"\u25C9";
    }
    if( it.is_medication() || it.is_medical_tool() ) return u8"\u271A";
    if( it.is_armor() || it.is_pet_armor() || category == "armor" ||
        category == "clothing" || category == "exosuit" || category == "ITEMS_WORN" ) {
        return u8"\u26E8";
    }
    if( it.is_seed() || category == "seeds" ) return u8"\u2663";

    const auto &food = it.get_comestible();
    if( food && food->comesttype == "DRINK" ) return u8"\u224B";
    if( food && food->comesttype == "FOOD" ) return u8"\u2668";
    if( category == "food" ) return u8"\u2668";

    if( it.is_book() || it.is_map() || it.is_software() || it.is_estorage() ||
        category == "manuals" || category == "books" || category == "maps" ||
        category == "ma_manuals" || category == "e_files" || category == "software" ) {
        return u8"\u270E";
    }
    if( it.is_relic() || category == "artifacts" ) return u8"\u25C8";
    if( it.is_deployable() || category == "traps" ) return u8"\u25B3";
    if( it.is_melee() && category == "weapons" ) return u8"\u2694";
    if( category == "weapons" || category == "WEAPON_HELD" ) return u8"\u2694";
    if( it.is_tool() || category == "tools" ) return u8"\u2692";
    if( it.is_gunmod() || it.is_toolmod() || it.is_bionic() || it.is_engine() || it.is_wheel() ||
        category == "mods" || category == "bionics" || category == "veh_parts" ||
        category == "INTEGRATED" || category == "BIONIC_FUEL_SOURCE" ) {
        return u8"\u2699";
    }
    if( it.is_container() || category == "container" ) return u8"\u25A3";
    if( category == "drugs" || category == "mutagen" || category == "chems" ) {
        return u8"\u2697";
    }
    if( category == "keys" ) return u8"\u2311";
    if( category == "spare_parts" ) return u8"\u25C7";

    // Semantic mode is intentionally total: custom/modded categories that do not
    // match a known role still get a stable neutral glyph instead of leaking the
    // arbitrary vanilla item symbol back into a semantic list.
    return u8"\u00B7";
}

template<typename Item>
std::string symbol( const Item &it, bool semantic_enabled )
{
    if( semantic_enabled ) return classify( it );
    return it.symbol();
}
} // namespace item_glyphs
} // namespace ncmm
