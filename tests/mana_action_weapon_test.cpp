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
    item_location() = default;
    item_location( avatar &, item *p ) : value(p) {}
    explicit operator bool() const { return value != nullptr; }
    item &operator*() const { return *value; }
    item *operator->() const { return value; }
};
struct avatar {
    item *physical = nullptr;
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
int ncmm_mana_hand_count_for_melee() { return hand_count; }
namespace ncmm {
constexpr const char *survivor_module_id = "survivor_progression";
constexpr const char *mana_hand_3_slot_id = "mana_hand_3";
constexpr const char *mana_hand_4_slot_id = "mana_hand_4";
constexpr const char *mana_hands_pair_slot_id = "mana_hands_34";
enum class ranged_weapon_action { fire, controls, reload };
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
    check(ncmm::ranged_weapon_candidates(player, fire)[0].value == &pistol,
          "empty physical hands + Mana III firearm");
    check(ncmm::ranged_weapon_binding_valid(player, pistol),
          "selected Mana III firearm binding remains valid");
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
    guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 1 && guns[0].value == &rifle, "physical melee + paired III+IV firearm");
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
    guns = ncmm::ranged_weapon_candidates(player, fire);
    check(guns.size() == 2 && guns[0].value == &pistol && guns[1].value == &rifle,
          "two independent Mana guns retain selector order");
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
    check(!ncmm::ranged_weapon_binding_valid(player, pistol),
          "aim binding invalidates when Mana Hands become unavailable");
    std::cout << "Mana action weapon resolver behavior: PASS\n";
}
