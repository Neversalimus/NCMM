# Survivor Progression 0.12.0

Survivor Progression is an optional NCMM native module that adds persistent character progression, perk trees, mechanical perk effects and conditional integrations with supported world mods.

It requires NCMM Host 0.8.1 and keeps persistent state schema **8**.

## Current system

- 30 normal Survivor levels with persistent XP and perk currencies.
- 369 perk nodes in the current source.
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

## Compatibility and migration

The module enters through Loader ABI v1, requires semantic Host API 1.9 capabilities and uses Host API 2.0 Core services for current generic events/settings/modifier/runtime-hook behavior.

State schemas 0–7 remain migration inputs supported by the current module contract. A save with an unsupported/newer schema is suspended rather than guessed or overwritten.

The 0.12.0 update preserves the 0.11.3 gameplay/perk baseline and adds manager-integrated balance controls. Existing perk nodes were not removed merely to equalize branch sizes; branch counts are intentionally allowed to differ.

CI treats the perk catalog as executable behavior, not just data. The fast semantic matrix walks all 369 perk definitions, checks direct modifiers, XP effects, amplifiers, conditional integrations, stateful momentum behavior, cleanup/respec, an all-perks-max stress state and deterministic mixed combinations. The real gameplay smoke then creates an actual CDDA avatar and exercises the whole catalog through the released DLL and Host, with representative primary-stat, speed, stamina, dodge, hit and movement checks against real Character methods.

## Development invariant

When extending Survivor, prefer a generic Host capability or hook that can serve multiple modules. A new Survivor-specific CDDA source patch should be treated as a design failure unless the engine truly lacks a reusable domain primitive.
