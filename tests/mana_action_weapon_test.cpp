#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Minimal item/character doubles. The resolver implementations are extracted
// from production sources by Test-ManaActionWeaponContracts.ps1, not copied.
struct gun_mode {
    bool valid = true;
    bool melee_mode = false;
    explicit operator bool() const { return valid; }
    bool melee() const { return melee_mode; }
};
struct item {
    bool gun = false;
    bool gunmod = false;
    bool reloadable = true;
    bool melee = true;
    bool two_handed = false;
    bool primary = false;
    std::vector<item *> mods;
    gun_mode mode;
    bool is_gun() const { return gun; }
    bool is_gunmod() const { return gunmod; }
    bool is_reloadable() const { return reloadable; }
    bool is_melee() const { return melee; }
    template<class T> bool is_two_handed( const T & ) const { return two_handed; }
    std::vector<item *> gunmods() { return mods; }
    gun_mode gun_current_mode() const { return mode; }
};
struct avatar;
struct item_location {
    item *value = nullptr;
    std::vector<item *> parents;
    item_location() = default;
    item_location( avatar &, item *p ) : value(p) {}
    explicit operator bool() const { return value != nullptr; }
    item *get_item() const { return value; }
    item &operator*() const { return *value; }
    item *operator->() const { return value; }
};
struct avatar {
    item *physical = nullptr;
    bool explicit_locations = false;
    int location_reads = 0;
    std::vector<item_location> locations;
    std::vector<item_location> all_items_loc();
    bool is_avatar() const { return true; }
    bool is_mounted() const { return false; }
    item_location get_wielded_item() { return item_location(*this, physical); }
};
using Character = avatar;
avatar player;
avatar &get_avatar() { return player; }
int hand_count = 2;
int slot_reads = 0;
item *third = nullptr;
item *fourth = nullptr;
item *pair = nullptr;
std::vector<item_location> avatar::all_items_loc() {
    ++location_reads;
    if( explicit_locations ) { return locations; }
    std::vector<item_location> result;
    for( item *p : {third, fourth, pair} ) {
        if( p ) { result.emplace_back(*this, p); }
    }
    return result;
}
int ncmm_mana_hand_count_for_melee() { return hand_count; }
namespace ncmm {
constexpr const char *survivor_module_id = "survivor_progression";
constexpr const char *mana_hand_3_slot_id = "mana_hand_3";
constexpr const char *mana_hand_4_slot_id = "mana_hand_4";
constexpr const char *mana_hands_pair_slot_id = "mana_hands_34";
enum class ranged_weapon_action { fire, controls, reload };
enum class mana_hand_item_slot { none, hand3, hand4, paired };
enum class mana_hand_ranged_owner { none, single, paired };
int survivor_mana_hand_count() { return hand_count; }
item *virtual_item_for_slot( const char *, const char *slot ) {
    ++slot_reads;
    if( std::string(slot) == mana_hands_pair_slot_id ) { return pair; }
    return std::string(slot) == mana_hand_3_slot_id ? third : fourth;
}
bool virtual_item_primary_melee_enabled( const item &candidate ) { return candidate.primary; }
bool virtual_item_matches_slot( const item &candidate, const char *, const char *slot ) {
    if( std::string(slot) == mana_hands_pair_slot_id ) { return &candidate == pair; }
    if( std::string(slot) == mana_hand_3_slot_id ) { return &candidate == third; }
    return std::string(slot) == mana_hand_4_slot_id && &candidate == fourth;
}
// Production action resolvers.
#include "ranged_resolvers.inc"
#include "primary_melee_resolver.inc"
}

void check(bool ok, const char *scenario) {
    if(!ok) { std::cerr << "FAIL: " << scenario << '\n'; std::exit(1); }
}
int main() {
    using ncmm::ranged_weapon_action;
    const auto fire = ranged_weapon_action::fire;
    item sword;
    item pistol;
    pistol.gun = true;
    pistol.melee = false;
    item rifle = pistol;
    rifle.two_handed = true;
    item mana_melee;
    mana_melee.primary = true;
    player.physical = &sword;
    fourth = &pistol;
    auto guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 1 && guns[0].value == &pistol, "physical melee + Mana IV firearm");
    check(player.physical == &sword, "ranged resolution preserves physical melee");
    check(ncmm::primary_mana_hand_melee_weapon(player) == nullptr,
          "Mana firearm never intercepts physical melee");
    third = &mana_melee;
    check(ncmm::primary_mana_hand_melee_weapon(player) == nullptr,
          "physical melee remains primary with explicit Mana primary melee");
    fourth = nullptr;
    check(ncmm::ranged_weapon_candidates(player, fire).empty(), "Mana melee is not a gun");
    player.physical = nullptr;
    check(ncmm::primary_mana_hand_melee_weapon(player) == &mana_melee,
          "empty physical hand retains existing Mana primary melee");
    third = &pistol;
    auto active_items = ncmm::active_mana_hand_items(player);
    check(active_items.size() == 1 && active_items[0] == &pistol,
          "active Mana Hand items expose Mana III");
    check(ncmm::ranged_weapon_candidates(player, fire)[0].value == &pistol,
          "empty physical hands + Mana III firearm");
    check(ncmm::ranged_weapon_binding_valid(player, pistol),
          "selected Mana III firearm binding remains valid");
    check(ncmm::mana_hand_item_slot_of(player, pistol) ==
              ncmm::mana_hand_item_slot::hand3,
          "generic ownership identifies Mana Hand III");
    check(ncmm::mana_hand_ranged_item_owner(player, pistol) ==
              ncmm::mana_hand_ranged_owner::single,
          "single Mana Hand owns its bound firearm");
    check(ncmm::mana_hand_ranged_mode_owner(player, &pistol) ==
              ncmm::mana_hand_ranged_owner::single,
          "single Mana Hand owns its base firing mode");
    item pistol_mod;
    pistol.mods.push_back(&pistol_mod);
    check(ncmm::mana_hand_ranged_mode_owner(player, &pistol_mod) ==
              ncmm::mana_hand_ranged_owner::single,
          "single Mana Hand owns attached gunmod mode");
    player.physical = &sword;
    pair = &rifle;
    active_items = ncmm::active_mana_hand_items(player);
    check(active_items.size() == 1 && active_items[0] == &rifle,
          "paired Mana Hands take precedence over individual slots");
    guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 1 && guns[0].value == &rifle, "physical melee + paired III+IV firearm");
    check(ncmm::mana_hand_item_slot_of(player, rifle) ==
              ncmm::mana_hand_item_slot::paired,
          "generic ownership identifies paired Mana Hands");
    check(ncmm::mana_hand_ranged_item_owner(player, rifle) ==
              ncmm::mana_hand_ranged_owner::paired,
          "paired Mana Hands own their bound firearm");
    check(ncmm::mana_hand_ranged_mode_owner(player, &rifle) ==
              ncmm::mana_hand_ranged_owner::paired,
          "paired Mana Hands own two-handed firing mode");
    pair = nullptr;
    third = &mana_melee;
    fourth = &pistol;
    player.physical = &rifle;
    slot_reads = 0;
    guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 1 && guns[0].value == &rifle && slot_reads == 0,
          "physical firearm retains priority without scanning Mana slots");
    fourth = nullptr;
    check(ncmm::ranged_weapon_candidates(player, fire)[0].value == &rifle,
          "physical firearm + Mana melee remains physical firearm");
    player.physical = &sword;
    third = &pistol;
    fourth = &rifle;
    active_items = ncmm::active_mana_hand_items(player);
    check(active_items.size() == 2 && active_items[0] == &pistol && active_items[1] == &rifle,
          "active Mana Hand items retain III then IV order");
    guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 2 && guns[0].value == &pistol && guns[1].value == &rifle,
          "two independent Mana guns retain selector order");
    hand_count = 1;
    active_items = ncmm::active_mana_hand_items(player);
    check(active_items.size() == 1 && active_items[0] == &pistol,
          "one active Mana Hand hides Mana IV");
    hand_count = 2;
    check(ncmm::mana_hand_item_slot_of(player, rifle) ==
              ncmm::mana_hand_item_slot::hand4,
          "generic ownership remains action-neutral for a two-handed item in Mana IV");
    check(ncmm::mana_hand_ranged_item_owner(player, rifle) ==
              ncmm::mana_hand_ranged_owner::none,
          "two-handed firearm cannot masquerade as a single Mana Hand binding");
    pistol.mode.melee_mode = true;
    check(!ncmm::ranged_weapon_capable(pistol, fire), "melee gun mode cannot intercept fire");
    check(ncmm::ranged_weapon_capable(pistol, ranged_weapon_action::controls),
          "mode controls can switch gun out of melee mode");
    check(ncmm::ranged_weapon_capable(pistol, ranged_weapon_action::reload),
          "reload remains available in melee mode");
    pistol.reloadable = false;
    check(!ncmm::ranged_weapon_capable(pistol, ranged_weapon_action::reload),
          "reload checks reload capability");
    pistol.mode.melee_mode = false;
    pistol.mode.valid = false;
    check(!ncmm::ranged_weapon_capable(pistol, fire), "invalid gun mode is rejected");
    pistol.mode.valid = true;
    pistol.gunmod = true;
    check(!ncmm::ranged_weapon_capable(pistol, fire), "standalone gunmod is rejected");
    hand_count = 0;
    check(ncmm::ranged_weapon_candidates(player, fire).empty(), "unavailable Mana Hands cannot fire");
    check(ncmm::mana_hand_item_slot_of(player, pistol) ==
              ncmm::mana_hand_item_slot::none,
          "generic ownership follows active Mana Hand count");
    check(!ncmm::ranged_weapon_binding_valid(player, pistol),
          "aim binding invalidates when Mana Hands become unavailable");
    // Extracted production canonical lookup: nested ordinary containers,
    // hidden carrier, foreign/missing items, and physical fast path.
    item backpack;
    item pouch;
    item carrier;
    player.explicit_locations = true;
    player.location_reads = 0;
    player.physical = &pistol;
    auto physical_location = ncmm::virtual_item_location(player, pistol);
    check(physical_location.value == &pistol && physical_location.parents.empty() &&
          player.location_reads == 0, "physical canonical location does not scan inventory");
    player.physical = &sword;
    pistol.gunmod = false;
    pistol.reloadable = true;
    hand_count = 2;
    third = &pistol;
    fourth = nullptr;
    pair = nullptr;
    item_location nested(player, &pistol);
    nested.parents = {&pouch, &backpack};
    player.locations = {nested};
    auto canonical = ncmm::virtual_item_location(player, pistol);
    check(canonical.value == &pistol && canonical.parents == nested.parents,
          "canonical lookup preserves two ordinary container ancestors");
    guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 1 && guns[0].parents == nested.parents,
          "ranged action preserves complete selected location ancestry");
    for( auto action : {ranged_weapon_action::controls, ranged_weapon_action::reload} ) {
        guns = ncmm::ranged_weapon_candidates(player, action);
        check(guns.size() == 1 && guns[0].parents == nested.parents,
              "reload and mode controls preserve ancestry");
    }
    nested.parents = {&carrier};
    player.locations = {nested};
    canonical = ncmm::virtual_item_location(player, pistol);
    check(canonical.value == &pistol && canonical.parents == nested.parents,
          "canonical lookup preserves hidden carrier ancestor");
    player.locations.clear();
    check(!ncmm::virtual_item_location(player, pistol), "missing item has no fabricated owner");
    check(ncmm::ranged_weapon_candidates(player, fire).empty(),
          "ranged selection rejects stale slot not in actual inventory");
    avatar other;
    other.explicit_locations = true;
    check(!ncmm::virtual_item_location(other, pistol), "foreign character cannot claim item");
    player.explicit_locations = false;

    // These functions are extracted from the production Host, not reimplemented
    // in this test. Protect the three common action paths against accidentally
    // reintroducing repeated virtual-slot scans or pathological lookup cost.
    // This is a resolver microbenchmark, not a CDDA turn/TPS benchmark.
    constexpr int lookup_iterations = 40000;
    constexpr long long lookup_budget_ms = 2500;
    auto benchmark = [&]( const char *label, item *expected, int max_slot_reads ) {
        for( int n = 0; n < 1000; ++n ) {
            (void)ncmm::ranged_weapon_candidates( player, fire );
        }
        slot_reads = 0;
        const auto started = std::chrono::steady_clock::now();
        for( int n = 0; n < lookup_iterations; ++n ) {
            const auto results = ncmm::ranged_weapon_candidates( player, fire );
            check( results.size() == 1 && results[0].value == expected,
                   "ranged resolver benchmark changed weapon selection" );
        }
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - started ).count();
        std::cout << "Mana resolver hot path " << label << ": " << ms << "ms / "
                  << lookup_iterations << " selections, slot reads=" << slot_reads
                  << " (budget " << lookup_budget_ms << "ms, "
                  << max_slot_reads << " reads/selection)\n";
        check( slot_reads <= max_slot_reads * lookup_iterations,
               "ranged resolver repeated virtual slot lookup" );
        check( ms <= lookup_budget_ms, "ranged resolver exceeded microbenchmark budget" );
    };

    hand_count = 2;
    pistol.gunmod = false;
    pistol.mode.valid = true;
    pistol.mode.melee_mode = false;
    pair = nullptr;
    third = &rifle;
    fourth = nullptr;
    player.physical = &pistol;
    benchmark( "physical-firearm", &pistol, 0 );

    player.physical = &sword;
    third = &pistol;
    fourth = nullptr;
    benchmark( "single-Mana-Hand", &pistol, 3 );

    pair = &rifle;
    benchmark( "paired-Mana-Hands", &rifle, 1 );

    std::cout << "Mana action weapon resolver behavior: PASS\n";
}
