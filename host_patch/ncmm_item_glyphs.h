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
    if( it.is_dangerous() ) return u8"\u26A0";
    if( it.is_corpse() ) return u8"\u2620";
    if( it.is_money() ) return u8"\u00A4";
    if( it.is_battery() || it.is_vehicle_battery() || it.is_fuel() ) return u8"\u26A1";
    if( it.is_gun() ) return u8"\u2316";
    if( it.is_ammo() || it.is_magazine() ) return u8"\u25C9";
    if( it.is_medication() || it.is_medical_tool() ) return u8"\u271A";
    if( it.is_armor() || it.is_pet_armor() ) return u8"\u26E8";
    const auto &food = it.get_comestible();
    if( food && food->comesttype == "DRINK" ) return u8"\u224B";
    if( food && food->comesttype == "FOOD" ) return u8"\u2668";
    if( it.is_book() || it.is_map() || it.is_software() || it.is_estorage() ) return u8"\u270E";
    if( it.is_melee() && it.get_category_shallow().get_id().str() == "weapons" ) return u8"\u2694";
    if( it.is_tool() ) return u8"\u2692";
    if( it.is_gunmod() || it.is_toolmod() || it.is_bionic() || it.is_engine() || it.is_wheel() ) {
        return u8"\u2699";
    }
    if( it.is_container() ) return u8"\u25A3";
    return nullptr;
}

template<typename Item>
std::string symbol( const Item &it, bool semantic_enabled )
{
    if( semantic_enabled ) {
        if( const char *glyph = classify( it ) ) return glyph;
    }
    return it.symbol();
}
} // namespace item_glyphs
} // namespace ncmm
