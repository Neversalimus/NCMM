# NCMM

NCMM (Neversalimus Code Mod Manager) is a native code-mod/runtime platform for Cataclysm: Dark Days Ahead. It is deliberately maintained as a standalone project rather than a CDDA fork: NCMM keeps its own bootstrap, certified Host, stable module API, source contracts, diagnostics, CI and optional native modules.

## Current stack

| Component | Version |
| --- | --- |
| NCMM Infrastructure | 0.8.3.1 |
| NCMM Runtime / Host | 0.8.2 |
| Loader ABI | 1 |
| Legacy semantic Host API | 1.9 |
| Queried Host API | 2.0 Core |
| Survivor Progression | 0.14.0 |
| Advanced World Settings | 0.6.4 |
| Ballistic Hit Chance | 0.1.0 |
| Equipment Body Map | 0.1.0 |
| Item Glyphs | 0.1.0 |

Survivor Progression keeps state schema 8. The current source contains 372 perk nodes and conditionally exposes mod-specific progression for supported active world mods.

## Install

For players, use the **Full** release package.

1. Download `NCMM_Full_v0.8.2.zip` from the `ncmm-runtime-v0.8.2` release.
2. Extract it anywhere outside the CDDA game directory.
3. Run `NCMM_Setup.exe`.
4. Select the CDDA installation and the optional modules you want.
5. Choose **Install / Repair selected**.
6. Launch CDDA normally from CatLauncher, Catapult or your usual shortcut.

For an existing NCMM installation, use the latest Full package and run **Install / Repair selected** again; a separate uninstall is not required, and Survivor's persistent state remains on schema 8 with migration support for older supported schemas.

No compiler, Git, CMake or MSYS2 is required on the player's PC. If no exact certified Host exists for the installed CDDA executable, NCMM fails closed and launches vanilla CDDA.

The repository checkout also contains `NCMM.cmd`, the maintainer/development entry point for install, verify, update, diagnostics, adapter and self-test workflows. Its build cache is intentionally reusable.

## Player-facing features

- **NCMM Mod Configuration** is integrated into CDDA's settings menu. The current manager uses a two-pane layout: modules on the left, version/status/description/hotkey/settings on the right.
- **Survivor Progression 0.14.0** opens with F1 by default and remains remappable through CDDA. It keeps state schema 8, includes Host-managed live XP/stat-perk controls, and adds Magiclysm Mana Hand III/IV virtual item slots with melee, ranged and utility integrations that keep real items in CDDA's normal item graph.
- **Advanced World Settings 0.6.4** exposes NCMM-owned world-generation controls through CDDA's world-options UI. NEW_MAP / NEW_WORLD settings are available during world creation; LIVE / RELOAD settings stay in the NCMM manager.
- **Ballistic Hit Chance 0.1.0** adds live ballistic hit probabilities to firearm targeting: current-shot probability, per-aim-mode probability, and compact burst prediction using the active fire mode and recoil growth.
- **Equipment Body Map 0.1.0** adds a live body map to the normal inventory, reflecting worn-item coverage and highlighting the selected item's coverage.
- **Item Glyphs 0.1.0** adds semantic item glyphs to Inventory, Pickup, Trade and Advanced Inventory; unknown items retain their normal symbol and the feature can be disabled.
- Current main-source UI polish uses CDDA's native `menu_move` sound for NCMM perk navigation and displays a compact `NCMM 0.8.2` label in the main menu.
- Module failures are isolated where possible and reported through machine-readable runtime/module state instead of silently loading incompatible code.

## Automated installation lifecycle

Every Runtime build now runs the production `SetupCore` against isolated CDDA-shaped installations before packages are published. The installation matrix currently covers clean installs with no modules / Survivor / AWS / both, module removal and re-enable, idempotent reinstall, previous-runtime update, corrupted DLL repair, corrupt-manifest fail-closed behavior, invalid payload/selection, rollback at multiple install phases, and recovery after a hard interrupted process.

Bootstrap has a separate lifecycle harness covering certified-host selection, incompatible bindings, first and second healthy launches, host crash/auto-disable, reset recovery and fail-closed marker failures. A failed matrix blocks the Runtime release. A separate nightly/manual **Real Installation Matrix** downloads an official Windows CDDA release and runs the same production SetupCore plus Survivor/AWS selection changes against the real extracted game tree before restoring the original vanilla executable. Module QA is semantic rather than load-only: AWS validates all 48 typed geography settings/bindings, min/max boundaries and deterministic randomized cases; Survivor validates all 372 perks, modifier consumers, cleanup/respec behavior, conditional integrations, full-catalog max-rank aggregation and deterministic perk combinations. The real gameplay smoke also creates, saves and reloads a randomized AWS world, generates an overmap, and applies Survivor effects to a real CDDA avatar before checking cleanup.

## Compatibility model

NCMM does not trust a folder name or launcher version. Runtime validates the vanilla executable SHA-256, CDDA source commit, Loader API, NCMM version and Host patch revision against the certified feed. A Host is published only after source-contract preflight and Windows/MSVC certification for the exact upstream CDDA release.

The bootstrap preserves the original executable as `cataclysm-tiles.vanilla.exe`. Invalid, missing or uncertified Host state falls back to vanilla execution. Runtime state is written to `ncmm/runtime.state.json`; Host/module state is written to `ncmm/modules.state.json`.

## Repository map

- `runtime/` — bootstrap and graphical setup application.
- `host_patch/` — source integration layer used to build a certified Host.
- `sdk/` — stable public module API.
- `mods/` — bundled native modules.
- `compat/` — source contracts, compatibility metadata and package integrity.
- `components/` — installable component catalog.
- `ci/` and `.github/workflows/` — build, certification, feed and regression pipelines.
- `adapters/` — exact/inherited CDDA build adapters.
- `checkpoints/` — intentionally historical package snapshots retained for reproducibility.

For the current trust boundaries and API layout, see `ARCHITECTURE.md`. Russian documentation is in `README_RU.md`.

## Project status

`Neversalimus/NCMM` is the only canonical NCMM repository. The earlier `Neversalimus/Cataclysm` tree is historical and is not used by the current source, package, feed or release pipeline.

The project intentionally keeps release history and machine-audit artifacts separate from the user documentation. Root audit/changelog files that are still present are package provenance or regression evidence, not additional setup guides.
