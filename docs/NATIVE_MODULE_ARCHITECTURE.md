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

## Generic build and publication (Phase 2)

`ci/Build-Runtime.ps1` now consumes `Get-NcmmNativeBuildModules` rather than
copying a CMake/smoke/copy/archive block for each module. The catalog supplies
build order, smoke exceptions, additional executables, required persisted data,
version and stable archive stem. The `NCMM_Full` release manifest and all module
archives are derived from the same validated rows.

`ci/Test-NativeReleaseLayout.ps1` verifies Full, Runtime and standalone ZIPs,
including manifest parity, DLLs and required data, before an artifact is published.
The runtime release uploader includes **all** catalog-declared native packages;
AWS and Survivor retain their existing dedicated release tags for compatibility.

The shared Host smoke and manifest-policy tests are now built once from
`tests/CMakeLists.txt` before any gameplay module. AWS is no longer a test-harness
dependency or required to be the first module in the build registry. Each module
still has its own smoke and fail-closed contract checks.

## Generic smoke profile for new modules

Each entry in `components/native-build.json` explicitly chooses a smoke profile:
`semantic` for the five existing deeply tested modules, or `generic` for new
modules whose module-specific semantic smoke has not yet been authored.
`generic` **requires** `missing_contract_smoke: true`; there is no `skip` mode.

The generic smoke harness checks Loader ABI, descriptor identity/version, non-empty
and unique required capabilities, supported Host capabilities, initialization and
shutdown. It also masks one of the module's actually declared capabilities
before calling init and rejects modules that initialize or register state despite
the missing requirement. The real runtime still owns authoritative manifest and
DLL descriptor verification. A generic smoke is a baseline, not gameplay proof.

`tests/fixtures/generic_native_fixture.cpp` uses an ID unknown to all existing
specialized tests, proving that a sixth module can pass generic smoke without
changing the test harness or any other game's module.

## SDK module starter (Phase 5)

The isolated generator at tools/New-NCMMNativeModule.ps1 creates an unregistered
CMake/C++ project plus a JSON registration blueprint. It never touches the
installed modules, Host, compatibility feed or published component catalog.
See sdk/CREATE_NATIVE_MODULE.md for invocation and integration steps.

Runtime CI compiles the generated native DLL against the real SDK header and
runs the generic success/fail-closed Host tests. Blocking Source/Package Audit
checks deterministic output, manifest parity, Unicode and rejection of unsafe
requests and overwrites.

A future architecture step will turn the flat Host patch stack into explicit
domains with dependency contracts. Loader ABI and gameplay remain unchanged.

## Shared C++ SDK Core guard (Phase 6)

The source-only header sdk/ncmm_sdk_core.hpp centralizes Loader/queried-Core
capability negotiation, API major/minor checks and struct-prefix validation.
Generated modules use it instead of repeating a fragile raw API pointer
sequence. tests/sdk_core_guard_test.cpp exercises failures against controlled
Host mocks; Runtime CI compiles and runs that harness before building gameplay
modules. Existing published mod binaries and Host ABI are untouched.

## Stage reviewed registration proposals (Phase 7)

\`tools/New-NCMMModuleRegistrationPreview.ps1\` can turn an unregistered SDK
starter into staged catalog/descriptor/build JSON outside the repository. It
checks identity, version, source path, group, capabilities and smoke policy,
and never edits or promotes the live NCMM component catalog, feeds or packages.
\`ci/Test-NCMMModuleRegistrationPreview.ps1\` exercises byte-stable output,
source immutability, and negative mutation scenarios.

Publication remains a separate reviewed change requiring registered module
sources, update/compatibility feeds, Runtime/Matrix tests and Host certification
where a new CDDA engine integration is needed.
