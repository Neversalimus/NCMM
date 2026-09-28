# NCMM

NCMM is a native code-mod/runtime platform for Cataclysm: Dark Days Ahead.

This repository is intentionally **not a fork of Cataclysm: DDA**. NCMM keeps its own SDK,
runtime, source contracts, host patch, diagnostics, tests and bundled code-mods. Certified
hosts are built in CI from exact `CleverRaven/Cataclysm-DDA` releases only after the relevant
source contracts pass.

Current source stack:
- NCMM Infrastructure: **0.8.3.1**
- NCMM Host: **0.8.0**
- Host ABI / Loader API: **v1**
- Semantic Host API: **1.9**
- Queried Host API: **2.0 Core**
- Survivor Progression: **0.11.3**
- Advanced World Settings: **0.6.2**

Canonical repository: `Neversalimus/NCMM`.

The standalone repository is the only active development path. The legacy `Neversalimus/Cataclysm` fork is not part of the current NCMM source or package pipeline. The source tree has advanced beyond the last public 0.7.1 certified-host feed; runtime 0.8.0 publication remains gated until matching certified hosts are promoted.

Documentation:
- `ARCHITECTURE.md` - platform architecture and compatibility model.
- `README_RU.md` - Russian project documentation.
- `compat/` - source contracts for supported CDDA builds.
- `mods/` - bundled NCMM code-mods, including Survivor Progression.
