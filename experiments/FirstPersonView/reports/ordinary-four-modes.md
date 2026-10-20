# Ordinary Native comparison protocol

`tests/Run-FullsizeModes.ps1` executes the eight predeclared runs sequentially:
baseline, assets/HUD, optimization, combined, then the reverse order. Each run
uses a fresh isolated Native game and the exact packaged benchmark wrapper.
The plan fixes both candidate source identities, bundle roots, the official
game and actual UDP source. It is hashed before each launch and cannot be
changed between runs without invalidating their comparison.

The controller restores the process VSync hint after the series. Ordinary runs
use the existing default and require actual `SDL_GetRenderVSync == 1`; they
attach neither a profiler nor debugger and do not enable GPU debug. The new
component switches are diagnostic controls, not additional player settings.

`tests/compare_fullsize_modes.py` requires the same RTX 4090/D3D12 backend,
actual UDP file hashes, SDL version, actual Native window pixels/display scale,
1768x1084 perspective canvas and 1761x1080 internal render resolution. The canvas
is reported separately from the actual Native window; it is not described as a
fullscreen performance result. Loaded Z bounds must agree between all new
component modes. The legacy baseline has no loaded-Z telemetry, so its source
equivalence to the unchanged production renderer is checked separately.

Each run covers eleven scenes with 64 true first frames after Native movement,
96 cold-sector and 96 warm camera frames per scene: 5,632 first frames and
16,896 camera frames across the eight runs. Controlled movement does not run
ordinary AI turns and is distinct from the actual firing audit. Initial render,
action-to-ready, median/p95/p99/max, phase accounting, uploads, draw calls and
memory remain separate. CPU submit time never becomes GPU execution duration.

The verifier retains both repeats and observed baseline spread. A full-frame
metric that regresses more than 5% and 0.1 ms in both directions blocks the
affected mode. One bad repeat is retained for investigation, not discarded.
Its p99/max estimates have limited precision at 64 first-frame samples per
scene. A passing comparison gate alone does not prove a speedup or certify
15 ms everywhere. Actual final Native runs and lifecycle qualification remain
required; this protocol contains no fabricated timing result.
# Shared SDK correctness prerequisite

The original renderer baseline now uses the same pinned SDL 3.4.10 graphics
descriptor correction as the component modes. Its immutable SDK recipe is
`078ee4f86d1231683b3d05a2dd2254f76db44d3e`, derived from diagnostic harness
`2bb3ec5a10ce2e22b11b537ec2fd5782518ee9f2`. The validator first rejects all
runtime changes outside the original three diagnostic files and this exact
SDK recipe, then compares every SDK/build/packaging file byte-for-byte against
that reference. It also requires matching source/patch/vendor identities in
all eight actual candidate receipts. The original uncorrected SDK cannot stand
in for this baseline. Fetch the baseline SDK branch before reproducing the Git
equivalence proof; a missing reference is an error, never an exemption.

The combined manual Full exceeded GitHub's 300-file compare list. Classification
now reads the complete Git trees of the exact merge base and requested commit,
retains additions/deletions/modes/gitlinks and both names of a rename, and still
rejects truncated trees. Its output matched the actual 309-file Git diff. Seven
collector checks, nine qualification-policy tests and twelve four-mode checks
pass. This changes evidence collection, not the required release tier or gates.
