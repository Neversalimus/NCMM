# Legacy Character Points 0.1.0

Independent NCMM native module for classic point-based creation in the modern CDDA creator.
Requires a Host advertising `character_creation.points.v1`; it does not require Survivor Progression.
This module does not add items, mutate loaded saves, or change NPC generation.

## Using the module
Select **Legacy Character Points** in `NCMM_Setup.exe`, then use its settings in the NCMM manager or the world's options. The default policy is **Any**; the initial chooser highlights **Multiple pools**. A world may instead require Multiple pools, Single pool, or Survivor (current freeform). F6 (remappable) and the button above the tabs change the mode only in an Any world. Changing a setting affects the next character, not a creator already open.

The budget remains visible with General Info collapsed. Prices appear next to scenarios, professions, backgrounds, traits and skill levels. Negative raw subtotals are permitted when higher pools can fund them. The total and the specific error explain whether the final character is legal. Editing can temporarily exceed the budget, but finishing cannot. Unspent points require confirmation.

## Classic rules
The source reference is official CDDA 0.H `src/newcharacter.cpp`, `src/player_difficulty.h`, and `src/game_constants.h`. Defaults: 6 stat points above four base stats of 8, 0 trait points, 2 skill points, and 12 points each of advantages/disadvantages. Budgets and cap can be configured from 0 to 1000. Stat range in point modes is 4..14; every stat point above 12 costs double. Skill cumulative prices are 0,1,1,2,4,6,9,12,16,20,25 for levels 0..10; the usual +/- action goes 0->2 and 2->0. Direct numeric entry and old templates may still contain level 1, priced correctly.

Multiple pools use the original equations. Stat points may fund traits and skills; trait points may fund skills; reverse borrowing is forbidden. Single pool shares the entire budget. Scenario, profession and background costs belong to skills. Mandatory scenario/profession/background traits are not charged separately. Skill bonuses from professions/backgrounds are applied later by CDDA and are not charged twice.

Intentional clarification of old inconsistent behavior: **all mandatory traits are exempt from both the price and the positive/negative cap**, including backgrounds. Old 0.H's manual trait tab exempted scenarios/professions from the cap but not backgrounds, while the randomizer had another rule. A fixed modern background must not make an otherwise valid character impossible to finish. Voluntary traits always count, including traits introduced through dependency chains.

Native ratings remain in freeform. In point modes the point budget replaces, rather than stacks with, the 1040 rating hard limit. Current JSON costs are used; restoring the accounting cannot restore balancing prices that an unrelated mod has removed or changed. No speculative rebalancing is added.

## Templates and saves
Mode IDs remain freeform=0, single=1, multi=2, transfer=3. Any honors valid modes saved in regular templates; a fixed world policy wins over the template. Invalid/unknown regular modes normalize to Multiple pools. Transfers retain the native early-return path and are never treated as new point-buy characters. Named templates and Last Character preserve the current mode. Every non-transfer start is revalidated before saving the last template or initializing equipment/skills. A random start that cannot meet a configured budget opens the editor instead of silently bypassing it.

Disabling/uninstalling this module restores normal chargen behavior; created characters need no permanent module data. No changes to Survivor's progression currency, Prime rules, or existing gameplay modifiers are made.

## Validation
Pure production accounting is exhaustively checked on 226,981 three-pool combinations, plus numeric bounds, skill prices, caps, fixed-policy templates and invalid input. The actual module is tested for every registration failure and missing/truncated Host capability. The source transform is checked on exact official 0546 and 1040 sources, LF/CRLF, repeated application, incomplete edits, and corruption of every required block. The installation matrix tests standalone install/disable/re-enable and all six modules. The real Host gameplay smoke calls the actual chargen cost functions and compares stat, skill and loaded-trait costs with the retained upstream helpers.

MSVC Host compilation and headless integration are necessary but not a substitute for a visual in-game check of mouse/keyboard layout and Russian labels. See the PR checks for the exact tested source revision; do not treat an older Host or a generic smoke as proof of this feature's readiness.
