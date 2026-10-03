# Survivor Progression 0.14.0

Survivor Progression is an optional NCMM native module that adds persistent character progression, perk trees, mechanical perk effects and conditional integrations with supported world mods.

It requires NCMM Host 0.8.2 and keeps persistent state schema **8**.

## Current system

- 30 normal Survivor levels with persistent XP and perk currencies.
- 372 perk nodes in the current source.
- Core branches: Combat, Survival, Mobility, Crafting, Scavenging and Mastery.
- Additional mod-specific progression is shown only when the matching world mod is active.
- F1 opens the progression UI by default; the action is remappable through CDDA.
- Full respec preserves the module's explicit refund rules and clears transient perk state when required.
- Level-up feedback does not forcibly interrupt sleep/wait/activity.
- UI navigation is Host-owned; current main-source Host uses CDDA's native `menu_move` SFX while moving between perk nodes/cards.

## Supported conditional integrations

The current integration registry contains dedicated progression for:

- Magiclysm
- Mind Over Matter
- Xedra Evolved
- Aftershock Exoplanet
- Aftershock Prime
- Secronom
- Secronom+

These nodes stay unavailable/hidden when their corresponding world mod is not active. Survivor uses the Host active-mod registry rather than hard-wiring world-mod assumptions into CDDA itself.

## Gameplay effects

Survivor uses NCMM Host APIs and generic Host hooks for real gameplay effects rather than UI-only bonuses. The current stack covers direct attributes and movement/stamina/carry/healing/reading/crafting effects, plus the later reactive/mechanical systems introduced in 0.10–0.11: combat reactions, momentum/kill effects, crafting failure handling, trap/lock interactions and mod-specific spell/ability mechanics.

Module-specific perk IDs remain inside Survivor. CDDA-facing source hooks are generic Host API 2.0 bindings.

## 0.12.0 live balance controls

NCMM Mod Configuration exposes two Host-managed settings:

| Setting | Range | Meaning |
| --- | ---: | --- |
| Experience gain | 25%–300%, step 25% | Scales Survivor XP after branch anti-farm adjustments. |
| Stat perk strength | 25%–300%, step 25% | Scales direct stat-perk effects only. Mechanical perk behavior is unchanged. |

The controls use the existing typed-settings persistence layer and do not change state schema 8.

### Mobility XP balance hotfix

Movement now grants 1 raw Mobility XP per 300 movement events instead of 150, a 50% reduction to passive travel XP. This changes only the Mobility movement source; combat, healing, crafting, scavenging and skill-level XP are unchanged. Existing XP-rate settings and XP perks still scale the resulting award normally.

## Compatibility and migration

The module enters through Loader ABI v1, requires semantic Host API 1.9 capabilities and uses additive Host API 2.x Core services for generic events/settings/modifiers/runtime hooks and virtual-item behavior. Host API 2.2 exposes secondary-melee controls; Host API 2.3 additionally exposes primary-melee controls.

State schemas 0–7 remain migration inputs supported by the current module contract. A save with an unsupported/newer schema is suspended rather than guessed or overwritten.

The 0.12.0 update preserves the 0.11.3 gameplay/perk baseline and adds manager-integrated balance controls. Existing perk nodes were not removed merely to equalize branch sizes; branch counts are intentionally allowed to differ.

## Development invariant

When extending Survivor, prefer a generic Host capability or hook that can serve multiple modules. A new Survivor-specific CDDA source patch should be treated as a design failure unless the engine truly lacks a reusable domain primitive.


## 0.14.0 Mana Hand virtual slots

Magiclysm's Third and Fourth Mana Hand perks can bind real carried items to logical virtual slots through the vanilla CDDA inventory selector. The item never leaves the vanilla item graph and is never duplicated: the Host stores a namespaced marker on the item plus its persistent UID and reconciles stale or copied identities safely.

An occupied Mana Hand is no longer a free somatic hand. A bound MAGIC_FOCUS still satisfies focus casting, and a supported blocking item participates in the normal shield-selection and wear path. Releasing the slot or resetting the perks removes only the logical binding; the real item stays where CDDA already stores it.

The reusable capability is `character.virtual_items.v1` in the additive Host API 2.1 tail, so later modules can reuse the same logical-slot primitive without adding another synthetic `item_location` type.

## Mana Hand utility items

A real item bound to Mana Hand III or IV can satisfy CDDA's item-local `need_wielding` requirement while the corresponding perk is actually active. This covers transform actions, item-cast spells such as Magiclysm wands, and effect-on-condition activations without making the item globally wielded.

The utility bridge deliberately does **not** modify `Character::is_wielding`, weapon categories, gun handling, holsters, gunmods, or secondary melee attacks. Those remain separate integration layers.


## Mana Hand secondary melee

A one-handed melee item held by Mana Hand III or IV can be explicitly opted into a secondary strike from the item context menu. The feature is off by default so shields, focuses, and utility items are never consumed or worn down as weapons unless the player enables it.

Each enabled Mana Hand performs its own vanilla melee attack after the primary manual attack if the target is still alive and adjacent. The secondary attack pays its normal move and stamina cost and additionally spends mana equal to `ceil(attack_speed / 10)`, clamped to 5–50 mana. Secondary attacks use vanilla hit, crit, armor, damage, item wear, enchantments, combat events, and skill training, but do not select weapon techniques, use miss-recovery techniques, fire martial-art on-miss/on-crit/on-kill/on-attack chains, or trigger another Mana Hand attack.

Guns and items that CDDA considers two-handed are excluded. Releasing or reassigning the item clears the secondary-strike opt-in.


## Mana Hand primary melee

A bound non-gun melee item can be explicitly selected as the primary Mana Hand weapon from the item context menu or directly from the Survivor Mana Hand detail UI when Host API 2.3 controls are available. It is used for normal manual melee attacks only when the character is not physically wielding an item and the selected martial-art style is not forcing unarmed attacks. A physical wielded item always keeps vanilla priority.

Primary Mana Hand attacks use the normal vanilla melee pipeline, including weapon techniques and martial-art on-miss/on-crit/on-kill/on-attack chains. They pay the weapon's normal move and stamina costs plus the same mana cost used by a secondary strike: `ceil(attack_speed / 10)`, clamped to 5–50 mana. If there is not enough mana, the attack is cancelled instead of silently falling back to an unarmed strike.

Weapon-specific martial arts resolve the selected primary virtual weapon for technique requirements, defensive technique lookup, weapon attack vectors, and style-use validation. This keeps a Mana Hand sword/spear compatible with styles that require that weapon without globally pretending that the item is physically wielded.

Primary and secondary modes are mutually exclusive for the same virtual item. Other eligible Mana Hands may still perform their enabled secondary strikes after a successful primary Mana Hand attack.


Primary Mana Hand melee also participates in CDDA reach attacks. A spear, whip, polearm, or other reach-capable primary virtual weapon uses the normal reach targeting UI, obstacle/thin-fence handling, weapon wear, stamina/move cost, techniques, and the same 5–50 mana primary-attack cost. Physical wielded reach weapons and force-unarmed martial-art styles keep vanilla priority.

The normal smash/bash action also uses the selected Primary Mana Hand melee weapon when the physical wield slot is empty. Bash damage, weapon wear, move/stamina costs and enchantment-aware melee damage stay on the vanilla path; the action costs the same 5–50 mana scale when a virtual primary weapon actually smashes something. If a fragile virtual weapon shatters, its real item is removed from its original `item_location` and the magical hand absorbs the break instead of injuring a physical hand.

## Mana Hand paired grip

With Fourth Mana Hand unlocked, Mana Hands III+IV can jointly hold one real two-handed item through the dedicated `mana_hands_34` logical slot, including a firearm. The paired item has one marker and one persistent UID; it is not duplicated across the two single-hand slots and never leaves the vanilla CDDA item graph.

A paired item consumes both virtual hands for somatic casting. MAGIC_FOCUS, SPELLCASTING_AID, blocking, item-local utility activation, opt-in secondary melee, and paired ranged firing all recognize the same real item once. The pair can only be assigned while Mana Hand III and IV are both otherwise empty, and single-hand assignment is blocked until the pair is released.

## Mana Hand ranged firearms

A real one-handed firearm bound to Mana Hand III or IV, or a real two-handed firearm bound to paired Mana Hands III+IV, can be fired without moving or copying the gun into the physical wield slot. The item context action remains available, and the normal FIRE command now uses a bound Mana Hand gun whenever no physically wielded ranged weapon has priority; if III and IV each hold a gun, FIRE asks which one to use. The normal CDDA aim UI, recoil, ammunition, gun modes, faults, UPS/bionic power checks, projectile events and reload activity remain in control.

Single-hand virtual guns still reject two-handed modes and `FIRE_TWOHAND`; the paired III+IV grip satisfies those handling requirements. `RELOAD_AND_SHOOT` weapons are supported through the same real `item_location`: the aim activity loads/unloads the bound gun normally, and ammo switching resolves the activity weapon instead of assuming `Character::get_wielded_item()`. Reload requests target the same real virtual gun. Firing through Mana Hands costs 5 mana per projectile actually fired, capped at 100 mana per burst.


## Mana Hand standard firearm controls

When no physically wielded firearm has priority, the normal reload-weapon, reload-wielded, burst-fire, select-fire-mode, and select-default-ammo actions can target a real firearm held by Mana Hand III, Mana Hand IV, or the paired III+IV grip. If two single-hand guns are available, the action asks which one to use.

These actions keep the gun in its original vanilla `item_location`; they do not obtain, copy, or temporarily move it into `Character::weapon`. A physically wielded gun keeps vanilla priority.

