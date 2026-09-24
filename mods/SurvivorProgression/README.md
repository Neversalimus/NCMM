# Survivor Progression v0.1.0

First NCMM vertical-slice gameplay mod.

Current slice:
- per-character persistent level / XP / perk points stored through `character_state.v1`;
- survival XP from `events.turn.v1` (1 XP per in-game minute);
- level-up loop up to level 30;
- one working perk: Fast Learner (cost 1 point, doubles survival XP rate);
- level-up opens the basic NCMM choice UI;
- optional module UI export for later integration with richer in-game UI.

This is intentionally a technical vertical slice, not the final balance model.
Future versions will replace survival-time-only XP with real gameplay event sources and add the full perk trees.
