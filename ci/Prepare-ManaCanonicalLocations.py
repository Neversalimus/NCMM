from pathlib import Path
import sys
root = Path(sys.argv[1]).resolve()

def edit(path, changes):
    p = root / path
    text = p.read_bytes().decode('utf-8')
    for old, new in changes:
        n = text.count(old)
        if n != 1:
            raise RuntimeError(f'{path}: expected one anchor, found {n}: {old[:100]!r}')
        text = text.replace(old, new)
    p.write_bytes(text.encode('utf-8'))

helper = '''item_location virtual_item_location( Character &who, item &candidate )
{
    // Preserve the engine's actual parent chain, including forbidden carrier
    // pockets and ordinary nested containers. Never invent a person location
    // for an item that is missing from this character's current inventory.
    item_location physical = who.get_wielded_item();
    if( physical && physical.get_item() == &candidate ) {
        return physical;
    }
    for( item_location loc : who.all_items_loc() ) {
        if( loc.get_item() == &candidate ) {
            return loc;
        }
    }
    return item_location();
}

'''
edit('host_patch/ncmm_loader.h', [('std::vector<item_location> ranged_weapon_candidates( avatar &who, ranged_weapon_action action );', '''/** Resolve a carried item without moving it, preserving the complete parent chain.
 * Call only when selecting an explicit action; do not rescan on an aim tick.
 * Missing/foreign items return an empty location instead of fabricated ownership.
 */
item_location virtual_item_location( Character &who, item &candidate );
std::vector<item_location> ranged_weapon_candidates( avatar &who, ranged_weapon_action action );''')])
edit('host_patch/ncmm_loader.cpp', [
('std::vector<item_location> ranged_weapon_candidates( avatar &who, ranged_weapon_action action )\n{', helper + 'std::vector<item_location> ranged_weapon_candidates( avatar &who, ranged_weapon_action action )\n{'),
('''        if( ranged_weapon_capable( *candidate, action ) ) {
            result.emplace_back( who, candidate );
        }''', '''        if( ranged_weapon_capable( *candidate, action ) ) {
            item_location loc = virtual_item_location( who, *candidate );
            if( loc ) {
                result.push_back( loc );
            }
        }'''),
('''        const int64_t hatchet_uid = hatchet->uid().get_value();''', '''        // The real ranged selector must retain the backpack parent; a raw
        // item_location( avatar, gun ) loses the obtain/serialization ancestry.
        const auto gun_candidates = ranged_weapon_candidates(
                                        get_avatar(), ranged_weapon_action::fire );
        if( !gun.has_parent() || gun_candidates.size() != 1 ||
            gun_candidates.front().get_item() != gun.get_item() ||
            !gun_candidates.front().has_parent() ||
            gun_candidates.front().parent_item().get_item() != gun.parent_item().get_item() ||
            gun_candidates.front().obtain_cost( get_avatar() ) < 0 ||
            virtual_item_for_slot_internal( survivor_id, "mana_hand_4" ) != gun.get_item() ) {
            return mana_hands_fail( "mana_hands_ranged_canonical_location", 153 );
        }
        ++mana_hands_check_count;

        const int64_t hatchet_uid = hatchet->uid().get_value();'''),
('''        virtual_item_clear_internal( survivor_id, "mana_hands_34" );
        if( !get_avatar().is_armed() ||''', '''        const item_location canonical_carrier_item = virtual_item_location(
                    get_avatar(), *wield_transfer_bound );
        if( !canonical_carrier_item || !canonical_carrier_item.has_parent() ||
            canonical_carrier_item.get_item() != wield_transfer_bound ||
            canonical_carrier_item.parent_item().get_item() !=
            wield_transfer_bound_loc.parent_item().get_item() ||
            canonical_carrier_item.obtain_cost( get_avatar() ) < 0 ) {
            return mana_hands_fail( "mana_hands_carrier_canonical_location", 154 );
        }
        ++mana_hands_check_count;

        virtual_item_clear_internal( survivor_id, "mana_hands_34" );
        if( !get_avatar().is_armed() ||'''),
('''                    "/16 real binding/state checks PASS." ).c_str() );''', '''                    "/19 real binding/state checks PASS." ).c_str() );'''),
('''        log_line( NCMM_LOG_INFO,
                  ( "NCMM gameplay smoke checkpoint: Mana Hands " +''', '''        item detached_location_probe( itype_id( "hatchet" ) );
        if( virtual_item_location( get_avatar(), detached_location_probe ) ) {
            return mana_hands_fail( "mana_hands_canonical_location_rejects_foreign", 155 );
        }
        ++mana_hands_check_count;

        log_line( NCMM_LOG_INFO,
                  ( "NCMM gameplay smoke checkpoint: Mana Hands " +''')
])
edit('payload/SURVIVOR_0911_0915_v8.7.6.8.ps1', [('''        item_location loc( you, candidate );''', '''        item_location loc = ncmm::virtual_item_location( you, *candidate );''')])
edit('ci/Test-HostPayloadContracts.ps1', [('''    '/16 real binding/state checks PASS.',''', '''    '/19 real binding/state checks PASS.',
    'mana_hands_ranged_canonical_location',
    'mana_hands_carrier_canonical_location',
    'mana_hands_canonical_location_rejects_foreign',''')])
edit('ci/Test-ManaActionWeaponContracts.ps1', [
("$resolver=Get-CppFunction $hostSource 'std::vector<item_location> ranged_weapon_candidates('", "$canonicalLocation=Get-CppFunction $hostSource 'item_location virtual_item_location('\n$resolver=Get-CppFunction $hostSource 'std::vector<item_location> ranged_weapon_candidates('"),
("Require $resolver 'ranged_weapon_capable( *candidate, action )'", """Require $resolver 'ranged_weapon_capable( *candidate, action )'
Require $resolver 'virtual_item_location( who, *candidate )'
Require $canonicalLocation 'who.get_wielded_item()'
Require $canonicalLocation 'for( item_location loc : who.all_items_loc() )'
Require $canonicalLocation 'loc.get_item() == &candidate'
Require $canonicalLocation 'return item_location();'
if($resolver.Contains('result.emplace_back( who, candidate )')){throw 'Ranged selection flattened a nested item location.'}
$throwStart=$payload.IndexOf('item_location ncmm_select_mana_hand_throw_item( avatar &you )')
if($throwStart -lt 0){throw 'Mana throw selector missing.'}
$throwSelector=Get-CppFunction $payload 'item_location ncmm_select_mana_hand_throw_item( avatar &you )'
Require $throwSelector 'ncmm::virtual_item_location( you, *candidate )'
if($throwSelector.Contains('item_location loc( you, candidate )')){throw 'Throw selection flattened a nested item location.'}"""),
("$capable+\"`n\"+$activeItems", "$canonicalLocation+\"`n\"+$capable+\"`n\"+$activeItems")
])
edit('tests/mana_action_weapon_test.cpp', [
('''    item *value = nullptr;
    item_location() = default;''', '''    item *value = nullptr;
    std::vector<item *> parents;
    item_location() = default;'''),
('''    item &operator*() const { return *value; }''', '''    item *get_item() const { return value; }
    item &operator*() const { return *value; }'''),
('''    item *physical = nullptr;
    bool is_avatar() const''', '''    item *physical = nullptr;
    bool explicit_locations = false;
    int location_reads = 0;
    std::vector<item_location> locations;
    std::vector<item_location> all_items_loc();
    bool is_avatar() const'''),
('''item *pair = nullptr;
int ncmm_mana_hand_count_for_melee()''', '''item *pair = nullptr;
std::vector<item_location> avatar::all_items_loc() {
    ++location_reads;
    if( explicit_locations ) { return locations; }
    std::vector<item_location> result;
    for( item *p : {third, fourth, pair} ) {
        if( p ) { result.emplace_back(*this, p); }
    }
    return result;
}
int ncmm_mana_hand_count_for_melee()'''),
('''    // These functions are extracted from the production Host, not reimplemented''', '''    // Extracted production canonical lookup: nested ordinary containers,
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

    // These functions are extracted from the production Host, not reimplemented''')
])
print('Prepared canonical-location port: ranged/controls/reload and throw only; no bootstrap cache or physical-wield transaction changes.')
