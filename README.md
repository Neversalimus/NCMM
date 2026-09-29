# NCMM

NCMM (Neversalimus Code Mod Manager) is a native code-mod/runtime platform for Cataclysm: Dark Days Ahead. It is deliberately maintained as a standalone project rather than a CDDA fork: NCMM keeps its own bootstrap, certified Host, stable module API, source contracts, diagnostics, CI and optional native modules.

## Current stack

| Component | Version |
| --- | --- |
| NCMM Infrastructure | 0.8.3.1 |
| NCMM Runtime / Host | 0.8.1 |
| Loader ABI | 1 |
| Legacy semantic Host API | 1.9 |
| Queried Host API | 2.0 Core |
| Survivor Progression | 0.12.0 |
| Advanced World Settings | 0.6.2 |

Survivor Progression keeps state schema 8. The current source contains 369 perk nodes and conditionally exposes mod-specific progression for supported active world mods.

## Install

For players, use the **Full** release package.

1. Download `NCMM_Full_v0.8.1.zip` from the `ncmm-runtime-v0.8.1` release.
2. Extract it anywhere outside the CDDA game directory.
3. Run `NCMM_Setup.exe`.
4. Select the CDDA installation and the optional modules you want.
5. Choose **Install / Repair selected**.
6. Launch CDDA normally from CatLauncher, Catapult or your usual shortcut.

No compiler, Git, CMake or MSYS2 is required on the player's PC. If no exact certified Host exists for the installed CDDA executable, NCMM fails closed and launches vanilla CDDA.

The repository checkout also contains `NCMM.cmd`, the maintainer/development entry point for install, verify, update, diagnostics, adapter and self-test workflows. Its build cache is intentionally reusable.

## Player-facing features

- **NCMM Mod Configuration** is integrated into CDDA's settings menu. The current manager uses a two-pane layout: modules on the left, version/status/description/hotkey/settings on the right.
- **Survivor Progression** opens with F1 by default and remains remappable through CDDA. Version 0.12.0 adds Host-managed live controls for experience gain and direct stat-perk strength without changing the 0.11.3 perk-state schema.
- **Advanced World Settings** exposes NCMM-owned world-generation controls through CDDA's world-options UI. NEW_MAP / NEW_WORLD settings are available during world creation; LIVE / RELOAD settings stay in the NCMM manager.
- Current main-source UI polish uses CDDA's native `menu_move` sound for NCMM perk navigation and displays a compact `NCMM 0.8.1` label in the main menu.
- Module failures are isolated where possible and reported through machine-readable runtime/module state instead of silently loading incompatible code.

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
