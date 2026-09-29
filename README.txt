Survivor Progression 0.12.0 + NCMM Host 0.8.1 — Mod Manager & Live Balance Settings
---------------------------------------------------------------------
- NCMM Mod Configuration is now a two-pane manager: installed mods on the left, selected module details and actions on the right.
- The details panel shows module version, runtime status, id, hotkey, localized description and Host-managed LIVE/RELOAD settings.
- Survivor Progression exposes two live balance controls through the existing typed-settings persistence layer:
  * Experience gain: 25%..300% in 25% steps, applied after branch anti-farm adjustments.
  * Stat perk strength: 25%..300% in 25% steps, scaling direct stat-perk effects only; mechanical perks are unchanged.
- Survivor Progression is 0.12.0. Its state schema remains 8 and the 0.11.3 gameplay/perk baseline is preserved.
- NCMM Host is 0.8.1. Loader ABI 1, legacy semantic API 1.9 and Host API 2.0 Core remain compatible.
- Advanced World Settings remains 0.6.2; its NEW_MAP settings stay in the Experimental world-settings page rather than the live module-settings list.
- Narrow terminals keep a compact fallback menu.
- Module descriptions are packaged as optional about.en.txt / about.ru.txt sidecars; older Hosts ignore them safely.

Survivor Progression 0.11.3 — Combinatorial Edge Polish
---------------------------------------------------------------------
- Builds on the complete 0.10.0 Mechanical Perks pass; no existing perk node is removed.
- Adds 25 new nodes with deliberately unequal branch counts: Combat +6, Survival +2, Mobility +4, Crafting +5, Scavenging +5, Mastery +3.
- Reactive combat: guarded dodge ripostes, post-dodge/crit/kill move and stamina effects, low-HP execution window, and kill-driven Momentum stacks.
- Crafting technical mechanics modify the real failure pipeline: success roll, complete failure cancellation, component preservation, and reduced progress loss.
- Scavenging fieldcraft modifies real trap/lock systems: trap detection, lockpick roll/time, tool preservation, and alarm bypass.
- Host remains generic: concrete Survivor perk IDs stay inside the module; CDDA sees generic Host API 2.0 hook names and one generic player-kill event.
- State schema remains 8. Existing 0.10.0/0.9.x purchases are preserved; the 25 new nodes start unowned.
- 0.11.1 preserves vanilla martial-arts dodge counters: the Survivor riposte is a stamina-gated fallback rather than a competing first strike.
- 0.11.2 hardens reactive targeting: automatic riposte never targets friendly/neutral/hallucination sources, damaging-critical rewards ignore zero-damage/hallucination hits, and kill/Momentum rewards require an XP-awarding hostile/fleeing combat target.
- 0.11.3 isolates riposte refunds from nested crit/kill move rewards, only treats a vanilla martial-arts counter as consumed when that attack actually executes, and excludes mounted automatic ripostes.
- Hostile human NPC kills now feed the same generic player-kill/Momentum path as hostile monster kills; friendly, neutral, hallucination and fake NPCs remain excluded.
- Momentum kill increments are saturation-safe even with corrupted INT64 state, self-heal persisted values to owned caps, and avoid needless modifier recalculation when already capped.
- Craft-failure cancellation now advances the next failure point monotonically; the catastrophic-failure UI estimate includes the same cancellation chance used by runtime.
- Monster kill-reactive perks ignore zero-XP kills (including revived-target farming); hostile direct-avatar NPC kills use the parallel generic kill path. Outgoing/execute hooks no longer amplify self-inflicted damage.
- Crafting UI success estimates now reflect the same NCMM bonus as the real failure-point roll.
- Quick Entry respects vanilla timing floors: normal lockpicks never go below 30 seconds; perfect lockpicks retain their 5-second floor.
- Momentum transient state is bounded to owned perk caps, is cleared when its owner perk disappears, and is immediately reset by full respec.
- Lockpick timing is null-safe in the source hook, retaining the normal 30-second and perfect 5-second floors.
- Exact CDDA target: cdda_experimental_2026_09_23_0546 / e262adb299a7613b4aedc5f12c08fe0413c56a84.
- This 0.11.3 package is a candidate until it passes a fresh Windows/MSVC install plus first-launch runtime verification.

Entrypoint: NCMM.cmd
Do NOT delete C:\NCMMBuild; the installer intentionally reuses source/vcpkg/MSBuild caches and invalidates only changed fingerprints.

Hotfix 17 — Host API 2.0 public-hook transform order
- Fixes the real cause of the eight Host API 2.0 unresolved symbols.
- The old transform inserted generic public hook definitions before gameplay_modifier(), then legacy contextual_metaphysics cleanup deleted that entire range.
- Legacy contextual cleanup now runs first; all eight public runtime/worldgen definitions are inserted afterwards and audited 1/1.
- Host build fingerprint is bumped to hotfix17; Hotfix 16 explicit ncmm_loader.cpp project integration remains enabled.

Hotfix 14 — MSVC option-template order + strict host build gate
- Moves ncmm_get_option_bool/int/float_or implementations from options.h to options.cpp after cOpt::value_as<T> explicit specializations.
- Fixes MSVC C2908/C2910/C2672 seen in options.cpp on exact CDDA 0546.
- Host MSBuild now fails on compiler/linker error markers even if the process wrapper reports exit code 0.
- A cache miss deletes the old build output first; no recursive stale-exe fallback is allowed.
- Host build cache key changed so the bad Hotfix 13 host cache cannot be reused.

Launcher Hotfix 2
- Use NCMM.cmd (menu) or NCMM.cmd install.
- Launcher no longer embeds PowerShell code through -Command.
- Any error stays visible and is appended to logs\launcher.log.

NCMM Host API 2.0 Core — on Infrastructure 0.8.3.1

This package keeps the 0.8.3.1 staged installer and Host API 2.0 baseline, runs the preserved 0.10.0 Mechanical Perks transform, then advances Survivor Progression through 0.11.0 Reactive Mechanics + Technical Mastery, applies the 0.11.1 semantic polish pass, and finishes with the 0.11.2 edge-case hardening pass and 0.11.3 combinatorial interaction pass.

Compatibility model
- NCMM Host: 0.8.1
- Loader ABI: 1 (unchanged)
- Legacy semantic API bridge: 1.9 (existing v1 modules remain compatible)
- Host API 2.0 Core: queried through api->query_interface("ncmm.host_api.v2.core", 2, 0)
- Survivor Progression 0.12.0 keeps every prior node and state schema 8; it preserves the 0.11.3 gameplay baseline and adds Host-managed live XP/stat-perk balance controls.
- Advanced World Settings 0.6.2 is migrated to Host API 2.0 typed settings/worldgen bindings. CDDA geography source now sees only generic geography.* Host hooks; NCMM_AWS_* IDs remain inside the AWS module.

Core 2.0 domains
- events.core.v2: host-ready, world-loaded/unloaded, turn, locale-changed subscriptions
- settings.typed.v2: bool/int/float/enum world-setting registration and typed reads
- active_mods.registry.v2: active world-mod enumeration
- module.lifecycle.query.v2: loaded/version/state queries
- character.modifiers.v2: built-in modifier writes + module-owned dynamic modifier definitions
- runtime_hooks.registry.v2: generic named hook + selector -> modifier rules
- worldgen.bindings.v2: generic named worldgen hook -> typed setting bindings

Boundary
The Host owns CDDA integration points. Future modules should register data/rules through Host API domains. If a genuinely new engine domain is needed, add one generic Host hook rather than a module-specific source patch.

Entrypoint
  NCMM.cmd

Do NOT delete C:\NCMMBuild. Existing source/vcpkg/MSBuild caches are intentionally reused.

First live validation
  NCMM.cmd install
Then:
  NCMM.cmd selftest

NCMM Infrastructure 0.8.3.1 — Infrastructure Cleanup

ONE USER ENTRYPOINT:
  NCMM.cmd

Run without arguments for menu. Command-line examples:
  NCMM.cmd install
  NCMM.cmd check
  NCMM.cmd update
  NCMM.cmd probe
  NCMM.cmd deepprobe
  NCMM.cmd adapter
  NCMM.cmd selftest
  NCMM.cmd diagnostics
  NCMM.cmd recover
  NCMM.cmd package C:\path\NCMM_update.zip

What changed from 0.8.2:
- fixed Windows PowerShell 5.1 Join-Path System.Object[] failure in static test;
- reduced user-facing CMD files from 16 to 1;
- removed legacy 0.8.0/0.8.1 redirect entrypoints;
- removed thin wrapper scripts and merged their routing into NCMM.ps1;
- merged update/dependency regression into one Infrastructure 0.8.3.1 static test;
- every PS1 is parsed by the real Windows PowerShell parser before static test passes;
- local package updater now uses NCMM.ps1 instead of version-specific entrypoint names.

Historical note: the original Infrastructure 0.8.3 cleanup left the game payload unchanged.
This package intentionally extends that payload with the already-live-verified Host API 2.0 stack and Survivor Progression 0.11.3 Combinatorial Edge Polish candidate.


NCMM Infrastructure 0.8.3.1 staged verification model
---------------------------------------------------
Install transaction gate: offline verification only (hashes, binding, host validity, installed module packages).
Runtime module verification is intentionally deferred until the first normal Host launch.
After installation: start Cataclysm normally once, then run `NCMM.cmd selftest`.
Direct offline re-check: `NCMM.cmd verify`.
Installer orchestration is split under internal\stages; the historical v8.7.6.8 payload is isolated as the compatibility/build engine and now emits granular pipeline checkpoints.

Infrastructure 0.8.3.1 persistence correction
---------------------------------------------
0.8.3.1 fixes runtime persistence for native NCMM world settings. NCMM_* values encountered in global
options.json before their owning module initializes are deferred until the module registers the real typed
setting. The dedicated ncmm_experimental page is also restored from world options when existing worlds load.
This prevents Advanced World Settings 0.6.2 from failing init after its settings have previously been saved.
