# NCMM

NCMM is a native code-mod/runtime platform for Cataclysm: Dark Days Ahead.

This repository is intentionally **not a fork of Cataclysm: DDA**. NCMM keeps its own SDK,
runtime, source contracts, host patch, diagnostics, tests and bundled code-mods. Certified
hosts are built in CI from exact `CleverRaven/Cataclysm-DDA` releases only after the relevant
source contracts pass.

Current platform line: **NCMM 0.7.1**
Current Host ABI / Loader API: **v1**
Current semantic Host API: **1.1**

Canonical repository: `Neversalimus/NCMM`.

NCMM 0.7.1 is the first post-migration runtime line published only from the standalone repository. The legacy `Neversalimus/Cataclysm` fork is not part of the active NCMM release/feed path.

Documentation:
- `ARCHITECTURE.md` - platform architecture and compatibility model.
- `README_RU.md` - Russian project documentation.
- `compat/` - source contracts for supported CDDA builds.
- `mods/` - bundled NCMM code-mods, including Survivor Progression.
