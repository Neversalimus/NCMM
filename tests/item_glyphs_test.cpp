#include "ncmm_item_glyphs.h"
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#ifdef NCMM_TEST_CDDA_WIDTH
#include "wcwidth.h"
#endif

namespace
{
struct predicate_item {
    std::set<std::string> flags;
    struct food_data { std::string comesttype; } food;
    struct category_id {
        std::string value;
        const std::string &str() const { return value; }
    };
    struct category_data {
        category_id id;
        const category_id &get_id() const { return id; }
    } category;
    const category_data &get_category_shallow() const { return category; }
    const food_data *get_comestible() const { return food.comesttype.empty() ? nullptr : &food; }
    std::string symbol() const { return "?"; }
    bool is_dangerous() const { return flags.count( "dangerous" ) != 0; }
    bool is_corpse() const { return flags.count( "corpse" ) != 0; }
    bool is_money() const { return flags.count( "money" ) != 0; }
    bool is_battery() const { return flags.count( "battery" ) != 0; }
    bool is_vehicle_battery() const { return flags.count( "vehicle_battery" ) != 0; }
    bool is_fuel() const { return flags.count( "fuel" ) != 0; }
    bool is_gun() const { return flags.count( "gun" ) != 0; }
    bool is_ammo() const { return flags.count( "ammo" ) != 0; }
    bool is_magazine() const { return flags.count( "magazine" ) != 0; }
    bool is_medication() const { return flags.count( "medication" ) != 0; }
    bool is_medical_tool() const { return flags.count( "medical_tool" ) != 0; }
    bool is_armor() const { return flags.count( "armor" ) != 0; }
    bool is_pet_armor() const { return flags.count( "pet_armor" ) != 0; }
    bool is_book() const { return flags.count( "book" ) != 0; }
    bool is_map() const { return flags.count( "map" ) != 0; }
    bool is_software() const { return flags.count( "software" ) != 0; }
    bool is_estorage() const { return flags.count( "estorage" ) != 0; }
    bool is_melee() const { return flags.count( "melee" ) != 0; }
    bool is_tool() const { return flags.count( "tool" ) != 0; }
    bool is_gunmod() const { return flags.count( "gunmod" ) != 0; }
    bool is_toolmod() const { return flags.count( "toolmod" ) != 0; }
    bool is_bionic() const { return flags.count( "bionic" ) != 0; }
    bool is_engine() const { return flags.count( "engine" ) != 0; }
    bool is_wheel() const { return flags.count( "wheel" ) != 0; }
    bool is_container() const { return flags.count( "container" ) != 0; }
};

bool expect( bool ok, const std::string &name )
{
    if( !ok ) std::cerr << "FAIL Item Glyphs: " << name << '\n';
    return ok;
}
}

int main()
{
    struct test_case {
        const char *name;
        std::set<std::string> flags;
        const char *food;
        const char *category;
        const char *expected;
        uint32_t codepoint;
    };
    const test_case cases[] = {
        { "dangerous", { "dangerous" }, "", "", u8"\u26A0", 0x26A0 },
        { "corpse", { "corpse" }, "", "", u8"\u2620", 0x2620 },
        { "money", { "money" }, "", "", u8"\u00A4", 0x00A4 },
        { "battery", { "battery" }, "", "", u8"\u26A1", 0x26A1 },
        { "vehicle_battery", { "vehicle_battery" }, "", "", u8"\u26A1", 0x26A1 },
        { "fuel", { "fuel" }, "", "", u8"\u26A1", 0x26A1 },
        { "gun", { "gun" }, "", "", u8"\u2316", 0x2316 },
        { "ammo", { "ammo" }, "", "", u8"\u25C9", 0x25C9 },
        { "magazine", { "magazine" }, "", "", u8"\u25C9", 0x25C9 },
        { "medication", { "medication" }, "", "", u8"\u271A", 0x271A },
        { "medical_tool", { "medical_tool" }, "", "", u8"\u271A", 0x271A },
        { "armor", { "armor" }, "", "", u8"\u26E8", 0x26E8 },
        { "pet_armor", { "pet_armor" }, "", "", u8"\u26E8", 0x26E8 },
        { "DRINK", {}, "DRINK", "", u8"\u224B", 0x224B },
        { "FOOD", {}, "FOOD", "", u8"\u2668", 0x2668 },
        { "book", { "book" }, "", "", u8"\u270E", 0x270E },
        { "map", { "map" }, "", "", u8"\u270E", 0x270E },
        { "software", { "software" }, "", "", u8"\u270E", 0x270E },
        { "estorage", { "estorage" }, "", "", u8"\u270E", 0x270E },
        { "melee", { "melee" }, "", "weapons", u8"\u2694", 0x2694 },
        { "tool", { "tool" }, "", "", u8"\u2692", 0x2692 },
        { "gunmod", { "gunmod" }, "", "", u8"\u2699", 0x2699 },
        { "toolmod", { "toolmod" }, "", "", u8"\u2699", 0x2699 },
        { "bionic", { "bionic" }, "", "", u8"\u2699", 0x2699 },
        { "engine", { "engine" }, "", "", u8"\u2699", 0x2699 },
        { "wheel", { "wheel" }, "", "", u8"\u2699", 0x2699 },
        { "container", { "container" }, "", "", u8"\u25A3", 0x25A3 },
        { "dangerous priority", { "dangerous", "corpse", "money", "battery", "gun", "ammo",
          "medical_tool", "armor", "book", "melee", "tool", "gunmod", "container" },
          "DRINK", "weapons", u8"\u26A0", 0x26A0 },
        { "battery > magazine", { "battery", "magazine" }, "", "", u8"\u26A1", 0x26A1 },
        { "medical tool > tool", { "medical_tool", "tool" }, "", "", u8"\u271A", 0x271A },
        { "gun > melee", { "gun", "melee" }, "", "weapons", u8"\u2316", 0x2316 },
        { "melee outside weapons", { "melee" }, "", "tools", "?", 0 },
        { "weapons category alone", {}, "", "weapons", "?", 0 },
        { "unknown", {}, "", "", "?", 0 }
    };
    bool ok = true;
    std::set<uint32_t> glyphs;
    for( const auto &test : cases ) {
        predicate_item it{ test.flags, { test.food }, { { test.category } } };
        ok &= expect( ncmm::item_glyphs::symbol( it, true ) == test.expected, test.name );
        if( test.codepoint ) {
            glyphs.insert( test.codepoint );
            // Verify actual classifier output is exactly one BMP codepoint, no VS/emoji sequence.
            const std::string glyph = ncmm::item_glyphs::symbol( it, true );
            uint32_t cp = 0;
            if( glyph.size() == 2 ) {
                cp = ( static_cast<unsigned char>( glyph[0] ) & 0x1f ) << 6 |
                     ( static_cast<unsigned char>( glyph[1] ) & 0x3f );
            } else if( glyph.size() == 3 ) {
                cp = ( static_cast<unsigned char>( glyph[0] ) & 0x0f ) << 12 |
                     ( static_cast<unsigned char>( glyph[1] ) & 0x3f ) << 6 |
                     ( static_cast<unsigned char>( glyph[2] ) & 0x3f );
            }
            ok &= expect( cp == test.codepoint && cp <= 0xffff, test.name );
#ifdef NCMM_TEST_CDDA_WIDTH
            ok &= expect( mk_wcwidth( cp ) == 1, std::string( test.name ) + " CDDA width == 1" );
#endif
        } else {
            ok &= expect( ncmm::item_glyphs::classify( it ) == nullptr, test.name );
        }
        // The same policy drives the existing inventory indent/draw and AIM prefix.
        for( bool enabled : { false, true } ) {
            for( bool vanilla : { false, true } ) {
                const bool slot = ncmm::item_glyphs::symbol_slot_enabled( enabled, vanilla );
                const std::string drawn = slot ? ncmm::item_glyphs::symbol( it, enabled ) : "";
                const std::string expected = enabled ? test.expected : vanilla ? "?" : "";
                ok &= expect( drawn == expected && slot == ( enabled || vanilla ),
                              std::string( test.name ) + " UI enable matrix" );
            }
        }
    }
    ok &= expect( glyphs.size() == 15, "15 distinct glyphs" );
    if( !ok ) return 1;
    std::cout << "Item Glyphs classifier + symbol-slot matrix: PASS\n";
#ifdef NCMM_TEST_CDDA_WIDTH
    std::cout << "CDDA 0546 wcwidth: PASS (15/15 glyphs width 1)\n";
#else
    std::cout << "CDDA wcwidth not linked; set NCMM_CDDA_SOURCE for width validation.\n";
#endif
    return 0;
}
