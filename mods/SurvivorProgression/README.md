# Survivor Progression v0.8.1 — Full Progression System Pass

This replaces the 0.1 technical vertical slice with the first complete playable progression system.

## Core
- 30 Survivor levels.
- 1 normal perk point per level gained.
- 1 Major Point at levels 5, 10, 15, 20, 25 and 30.
- Persistent per-character state through `character_state.v1`.
- F1 opens the progression interface; the binding remains remappable through CDDA.
- Level-up no longer forcibly opens a menu during sleep/wait/activity; it posts a notification instead.
- Full respec with exact refund of spent normal and Major Points.
- 0.1.x Fast Learner ownership is migrated into the new Mastery tree.

## Six branches / 60 perks
Each branch contains 8 normal perks, 1 Major perk and 1 capstone:
- Combat
- Survival
- Mobility
- Crafting
- Scavenging
- Mastery

Each branch has two prerequisite lanes which merge into its Major perk, then a level-30 capstone.
The full character can earn 29 normal perk points and 6 Major Points, so the system is intentionally
choice-driven rather than allowing every perk in one run.

## Real gameplay effects
Perks use NCMM `character.modifiers.v1`, not fake UI-only bonuses. Supported effects in this pass include:
- STR / DEX / PER / INT
- movement speed and movement cost
- maximum stamina
- carrying capacity
- dodge and melee accuracy
- natural healing
- reading speed
- crafting speed
- Survivor XP rate

## Balance state
This is feature-complete enough for live play, but remains v0.8 rather than 1.0 until the 60-perk balance
and long-save migration have been tested in real CDDA sessions.

## 0.8.1 safe polish
- Overview now shows purchased normal/Major counts and aggregated active gameplay effects.
- Locked perk labels distinguish level gates from missing prerequisites.
- Respec confirmation shows the exact refund before applying it.
- Respec smoke coverage now verifies point refund and modifier cleanup.
- No perk values or progression thresholds were rebalanced in this pass.
