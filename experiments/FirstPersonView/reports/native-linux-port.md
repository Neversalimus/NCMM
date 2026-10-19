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
