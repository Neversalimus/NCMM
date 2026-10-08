# NCMM native module extension architecture

## Purpose

Adding a new native module should not require copy/pasting an existing mod's build,
package, smoke, installer or release-manifest logic. The **Host ABI stays fixed**:
modules continue to use `ncmm_get_descriptor_v1` and capability-gated queried APIs.

Three registries have different responsibilities:

- `components/index.json`: authoritative published component IDs, versions,
  dependencies and atomic-group membership.
- `mods/<Folder>/mod.json`: runtime Loader ABI, module identity, capabilities
  and failure policy. The installer discovers packaged modules by this file.
- `components/native-build.json`: **build-only** onboarding recipe: module
  source/install folder, stable archive stem, isolated build directory and
  smoke/payload checks. Does not change the installed package format or Host ABI.

`ci/NativeModuleCatalog.ps1` validates all source-built native modules against
the published component catalog and real source manifests. It rejects unknown,
missing or duplicate registrations, unsafe paths, archive collisions, unregistered
module folders, absent required assets and incorrect manifest version/identity.
`ci/Test-NativeModuleCatalog.ps1` runs mutation fixtures, including path
traversal and missing required files.

## New module checklist

1. Create `mods/<Folder>/CMakeLists.txt`, `mod.json` and C++ implementation
   against `sdk/ncmm_api.h`; do **not** patch CDDA directly from a module.
2. Register its ID/version/dependencies and an independent atomic group in
   `components/index.json`; add its `components/<id>.json` descriptor.
3. Add a build recipe to `components/native-build.json` with an archive stem
   and unique build directory. Explicitly declare fail-closed/missing-contract
   smoke policy, extra test executables and persistent required files.
4. Add the component to the update feed / compatibility manifest only when its
   contract and release policy have been reviewed.
5. Run `ci/Test-NativeModuleCatalog.ps1`, `ci/Test-ComponentCatalog.ps1`,
   full Runtime CI and Real Installation Matrix before distributing it.
6. Add any genuinely new CDDA integration to the **generic** Host patch stack
   (`ci/host-patch-stack.json`) with independent source/behavior smoke tests.
   Prefer existing Core domains; do not enlarge the ABI for a one-off mod.

The source-first build is still gated by MSVC compile, synthetic installation
matrix and gameplay smoke. A successful registry check is necessary, not
sufficient, for release.

## Compatibility boundaries

- Never move module-specific gameplay rules into the Host just to centralize code.
- Never load a module merely because a folder or DLL exists. The Loader must
  validate its runtime manifest, identity, capabilities and crash policy.
- Do not make optional modules dependencies of one another without an explicit
  component dependency and a tested install/update/remove lifecycle.
- Preserve module-owned state and user files during selective upgrades.
- A new `native-build.json` recipe must not change existing standalone archive
  names, installed `code_mods/<Folder>` paths or a running game's configuration.

## Planned consolidation

After the registry proves stable, move repetitive CMake/smoke/copy/archive loops
from `ci/Build-Runtime.ps1` into one recipe-driven build function, preserve
specialized smoke executables, and derive `release-manifest.json` from the same
validated rows. Then provide a small SDK template/scaffolder that registers a
new module without modifying any existing module's sources.

This sequence deliberately **does not** change Survivor Legacy UI, existing
gameplay mechanics, C ABI or certified Host revision.
