NCMM 0.8.2 / Infrastructure 0.8.3.1
========================================

Current bundled modules:
  Survivor Progression 0.12.0
  Advanced World Settings 0.6.3
  Ballistic Hit Chance 0.1.0

PLAYER PACKAGE
--------------
Use NCMM_Full_v0.8.2.zip from the ncmm-runtime-v0.8.2 release.

1. Extract the archive outside the CDDA game directory.
2. Run NCMM_Setup.exe.
3. Select the CDDA installation.
4. Select Advanced World Settings, Survivor Progression and/or Ballistic Hit Chance.
5. Click Install / Repair selected.
6. Launch CDDA normally.

No compiler, Git, CMake or MSYS2 is required for normal installation.
If the installed CDDA executable has no exact certified Host, NCMM fails closed
and launches vanilla CDDA.

CURRENT UI
----------
NCMM Mod Configuration uses a two-pane module manager.
Survivor Progression opens on F1 by default and exposes live XP/stat-perk
balance settings through the manager.
Advanced World Settings exposes NEW_MAP / NEW_WORLD controls through CDDA's
world-options UI, including world creation.

Current main-source polish also uses CDDA's native menu_move sound for NCMM
perk navigation and displays NCMM 0.8.2 in the main menu.

DEVELOPER / SOURCE CHECKOUT
---------------------------
NCMM.cmd is the single maintainer entry point. Run it without arguments for the
menu or use commands such as install, check, update, probe, deepprobe, adapter,
selftest, diagnostics, recover and package.

C:\NCMMBuild is an intentional reusable build cache. Do not delete it merely
to update NCMM; the pipeline invalidates changed fingerprints itself.

ARCHITECTURE
------------
NCMM is not a CDDA fork. The certified Host owns CDDA source integration.
Native modules consume NCMM APIs and declare capabilities through mod.json.

Loader ABI: 1
Legacy semantic Host API: 1.9
Queried Host API: 2.0 Core
Survivor state schema: 8

See README.md / README_RU.md for the current project overview and
ARCHITECTURE.md for trust boundaries and compatibility.
