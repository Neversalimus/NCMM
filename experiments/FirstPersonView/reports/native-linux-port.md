# Qualify the actual pinned Linux Native Host

The private experimental Host now loads the same module ABI through `dlopen`,
resolves entry points and unloads in reverse order. Windows retains its native
wide-path loader. Persistent data and activation remain scoped by the original
Host. The canonical payload and main are unchanged.

Full qualification requires both real Linux CDDA pins, their ELF module builds,
eight library load/symbol/unload cycles, actual SDL controls, the real camera
input loop and the compiled gameplay features. The complete integration also
requires all 64 held-model and 42 target-HUD cases. Logical PRs without those
features test their own controls, camera and firing paths. Present features
without their verifiers fail; they are never silently skipped.

The Linux runner deliberately uses Mesa software Vulkan under Xvfb. Its results
are functional evidence, with actual process exits, source and binary hashes,
serialized configuration, pixel captures and failures retained. They establish
no RTX frame-time result. Windows and Linux release gates remain mandatory.

Strict GCC compilation exposed internal linkage/formatting mistakes and the
lost deferred option restoration described in `deferred-options-native.md`.
After those fixes, runs 39179348167 and 39179873769 reached Native
`vehicle_use.cpp`. GCC 13 diagnoses an optional-backed iterator copied inside
`std::any_of`. The exact experimental transform uses the same predicate and
short-circuit order in a direct range loop, without suppressing a diagnostic or
changing vehicle behavior. This translation unit is compiled early so any
remaining problem is reported before the full engine build.

Local atomic source-transform tests, diagnostic environment isolation, module
compilation and the actual Windows private library lifecycle pass. Actual
Linux runtime qualification of these final bytes is still required.

The independent camera branch also needs its world-visibility forward
declaration in the presentation include. The complete integration already has
that declaration for its target HUD. Reusing the integration's redundant-decl
cleanup verbatim had removed the camera-only branch's sole declaration; actual
Native CI 39183206827 caught the missing declaration on both compilers/pins.
The independent branch now declares it once before the presentation caller.

Both engines in run 39187622247 compiled and linked, loaded every ELF module
through the eight-cycle library test and exposed Mesa software Vulkan. Their
first game process exited before SDL because the Linux build's default data
prefix was `/usr/local/share/cataclysm-dda/`. The isolated fixture now passes
the upstream-supported empty `--basepath` argument to select its existing
portable `data/` and `gfx/` layout. It records the complete argument vector and
working directory and prints both output streams on failure. No production
path or validation gate changes; actual successful gameplay remains required.

The subsequent 1807 process in run 39190123525 loaded all seven ELF modules,
then timed out before writing a smoke result. Its log reports `C.UTF-8` with
an unset game language. The pinned `SystemLocale::Language` does not match
that locale, so `select_initial_language()` opens an interactive picker before
the smoke dispatcher. Every fresh owned Linux fixture now explicitly selects
`USE_LANG=en` and preserves its initial options bytes as evidence. This retains
the four serialized typed-value checks. Original user configuration is never
read or overwritten; no gameplay dialog is automatically confirmed. Actual
runtime completion after this fixture correction is still required.

Run 39197367747 reached 1,938 real SDL frames and passed all eight deferred
option checks on 1807, then exited 191 with `appearance_definition_missing`.
The Linux harness omitted `creature_appearance_fixture.json`, which the Windows
controls qualification copies before testing the unknown-mod anatomy case.
Both fixtures are now copied with the same relative names as Windows, and
their initial SHA-256 inventory is preserved per mode. Missing source files or
existing destination fixtures fail before any copy. Six configuration/data
checks pass. The large failed artifact is retained; selected members were
verified by CRC/SHA only, not represented as a verified whole archive.
Actual complete Linux runtime qualification remains required.
