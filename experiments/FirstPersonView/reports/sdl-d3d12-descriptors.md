# D3D12 descriptor boundary: hardware blocker and narrow SDK correction

The exact current firing installer completed 192 real Native input/UI attempts,
132 confirmed shots and 2,915 lossless PNG integrity checks on RTX 4090/D3D12.
All 2,910 fixed-policy presented frames passed the spatial pixel verifier.
The full process subsequently failed while drawing the CPU viewmodel gallery
after loading UDP Hybrid v3. This is a failed complete audit, not qualification.

The debug layer reported error 646, `INVALID_DESCRIPTOR_HANDLE`, followed by
exception `0x87D`. Exact PDBs matched the tested Host SHA. The stack enters
`D3D12_INTERNAL_WriteGPUDescriptors` through graphics resource binding and the
Native tile renderer. The dump shows sampler capacity and current index both
equal to 2,048: a descriptor batch crossed the end of the heap. A stale texture
lifetime was an initial hypothesis; this evidence establishes a capacity bug.

The pinned SDL 3.4.10 backend checks only whether a descriptor batch starts in
a full heap. The same problem is described in the primary upstream
[issue 16369](https://github.com/libsdl-org/SDL/issues/16369). Upgrading to a
different SDL release is not part of this correction.

An isolated real SDL renderer reproduces the same error immediately on the
same RTX 4090. It alternates real linear-color texture identities so SDL records
thousands of advanced-shader draws in one command buffer. The same test
executable, with only its SDL library replaced, then completes 65,536 draws
and checks all 1,048,576 output pixels across 16 frames. Debugger errors,
validation errors and dumps are zero. The future CI test additionally verifies
the compiled vendor marker and passes the same hardware pixel coverage.

The private port preserves SDL 3.4.10 and the original vcpkg source SHA-512.
Before graphics binding, it reserves space for every table used by that draw.
If either heap must change, all graphics root descriptor tables are rebound
together. No per-frame GPU idle wait, reduced detail, different rendering
resolution or discarded Native pass is introduced. Compute and non-D3D12
backends are unchanged; this does not claim to repair every upstream compute
descriptor edge case.

Preparation verifies every original/derived port file, rejects unknown Native
manifests or existing overlays, and records all overlay hashes in the Host
receipt. Both pins use this same private port. vcpkg and Host cache identities
include the correction. The installer retains the SDL license and altered-source
notice. Global SDL/vcpkg installations, the canonical payload and public ABI
remain unchanged. Sixteen preparation/package contract tests pass.

`sdl-d3d12-descriptors.json` records the exact Native Host/module, patch and
runtime hashes, the failed audit and raw evidence hashes. Large captures,
PDBs and dumps stay outside Git. One earlier marker-only attempt exited before
rendering because CMake rewrites `SDL_REVISION`; the supported `SDL_VENDOR_INFO`
field now identifies the correction and is checked at runtime.

This short SDK regression is not a Native gameplay performance measurement or
a manual playthrough. Fresh complete Native runs, both actual pins, Linux,
ordinary four-mode comparisons and the final unchanged 30-minute hardware
debug/lifecycle load remain mandatory. All four ordinary modes must share this
SDK correctness fix so the comparison isolates the intended feature changes.
