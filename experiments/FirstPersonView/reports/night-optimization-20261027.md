# Autonomous optimization, 27 October 2026

Baseline `128453707e6c4e2712237d046dfa07315eac3ca5`; branch
`codex/fpv-0131-fps-controls-20261024`, draft PR #223. Engine pins remain
1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83` and
1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.

**The 15 ms target and first-correct-game-frame latency remain unqualified.**
Local Native game/input harnesses, mouse capture/warp/focus changes and elevated
profilers are prohibited. Background games stay running. Local checks use
Release pure/model/shared-module tests and an inventoried SDL_WINDOW_HIDDEN
render-only GPU suite. No full game copy or new checkout was made. No main
merge, release or feed publication before the user's finalization request.

This journal replaces repetitive checkpoint detail; the complete stage 1-15
record remains [in its prior revision](https://github.com/Neversalimus/NCMM/blob/f6f706bf986503c25b89835e4aa425282a25e9ff/experiments/FirstPersonView/reports/night-optimization-20261027.md).
Private raw checks/selected receipts are under `../fpv-0131-work/`, using the
prefixes below. CI logs are read exclusively through Code Review; artifact
reads select program JSON/BMP/provenance, never CI stdout or log substitutes.

## Completed changes and measured limits

1. **Unique immutable texture owners (`08bf0e2`).** The failing baseline retained
   4096 shared owners for one texture. After: 1 owner, pointer capacity
   86320 -> 16 bytes, growths 22 -> 1; geometry/material/packet bytes and digest
   `5bb968231abc8f92` match. Across 512 packet builds: owner visits 2097152 ->
   512 and isolated elapsed 1.947 -> 0.052 ms **total**, not per frame.

2. **Skip CPU-only depth sorting in GPU collection (`18a3ef6`).** CPU sorting
   remains. Exact four-state resource/command digests, regenerated-cell counts
   and atlas reads match. Same 97x97 mixed fixture, MSVC Release, 1280x800,
   16 samples/state, no GPU/present: cold median 20.2533 -> 18.6074 ms;
   old origin-shift boundary stress median 4.76405 -> 4.31435 ms. Boundary p95
   worsened 5.09173 -> 5.2219; local-edit median worsened 2.075 -> 2.42435.
   Background load was uncontrolled. Raw: `night-gpu-preparation-20261027.json`.

3. **Retained owned sparse XYZ index (`3ae466f`).** Warm 97x97x21 dense-index
   allocation 790395 bytes -> 0. Owned integer coordinates/slots are keyed by
   exact ordered XYZ, epoch, grid/origin and loaded Z; Native pointers remain
   callback-local. New buffers/flags/light can reuse the mapping; changed
   coordinates/holes/order/window rebuild it. Across 512 index+lookup calls:
   sparse 57.5496 -> 0.0456 ms total; dense 59.769 -> 20.0672. Both remove
   512 allocations/404682240 bytes with equal lookup digest. Existing origin-Z
   float roundoff (7.45058e-8) was independently reproduced before this change.

4. **Continuous chain-link phase (`da8f991`).** Collinear panels use absolute
   world phase. CPU regression and five actual CPU raster captures pass;
   joints, corners and gate forms retain their geometry.

5. **Overlapping slab facts (`30630ce`).** Same 97x97 fixture: cold source reads
   9409, axial entry 97, diagonal 193; closed-plane+rectangle reads
   14114 -> 9409. Full source coverage and cold-oracle results remain exact.
   These callback counts are not Native frame timings.

6. **Allocator instrumentation repair (`5c244d4`).** Matching nothrow allocation
   hooks fix a test-only Linux ASan allocation-family mismatch. Actual Linux
   sanitizer CI passes; Windows' zero-mismatch probe was not a Linux repro.

7. **Early resolved opaque-floor templates (`710d070`).** Shared CPU producer
   fixtures preserve exact packets, lifecycle and negative guards. Same 97x97
   cold medians: legacy 18.1089 -> 6.8751 ms; Native-like TOP
   21.1995 -> 9.5144, p95 24.696 -> 11.685. No device/presentation; no game
   frame claim. Raw: `night-floor-preparation-20261027.json`.

8. **Callback-scoped Host detail storage (`fc2ada7`).** Warm detail allocation
   1 -> 0; 9409-cell allocation 3048555 -> 0 bytes, 65536-cell allocation
   21233703 -> 0. Capacity is bounded/released; no borrowed Native pointer is
   retained. Shared lifetime/poison/reentry controls pass, not a Native timing.

9. **Stationary-world boundary benchmark (`d7f5415`).** Separates ordinary
   absolute-coordinate overlap from old origin-only invalidation stress. Five
   before/after digests match; coordinate discrepancy <=9.53674e-7 is existing
   float behavior, all other fields exact. **First sample worsened
   2.5233 -> 2.9792 ms**; median improved 3.3933 -> 2.9931, p95
   3.9305 -> 3.8159. Local mutation did not improve. Background uncontrolled;
   raw `night-static-boundary-preparation-20261027.json`. No first-frame win.

10. **Typed standard radio skyline (`e7a358f`).** Optional minor-2 representation
    replaces the standard site's 18x18x5 m box with authored 5.81x5.81x17.5 m
    open lattice (16 triangles, two cutout masks). Exact standard IDs, complete
    disclosure and already-resident overmaps are required. Generic/modded
    layouts retain legacy behavior; V1 bytes/kinds are unchanged. Actual binary
    old-Host/new-module, new-Host/old-module, new/new controls pass 37/55/86
    checks. Hidden GPU: 268 radio/176 high pixels; CPU: 402/266. Gaps/depth and
    foreground wall pass; camera turn causes zero static upload/mesh generation.
    Six actual readbacks inspected. Thin-wire minification and nominal rather
    than destruction-aware profiles remain limits; no Native hardware claim.

11. **Candidate Native integer-distance reuse (`ddfa802`).** Actual extracted
    Host range helper plus forwarding controls reduce floor/corner target
    distance calls 2/4 -> 1/1; lower XYZ stays independent at 1/1. All 13440
    paired cases preserve ordered non-distance reads and decisions. Native
    distance function/formula/rounding and visual/discovery policy are unchanged.

12. **Causal static-stair recorder (`25e0e27`, API fix `2ce404d`).** Exclusive
    remote CPU fixture uses fixed-camera normal/debug/normal at day/dusk/night.
    Full 97x97xloaded-Z physical/memory and 31x31xloaded-Z resident-OMT ledgers
    protect terrain, furniture, fields, inventory, creatures, vehicles, light,
    moves, HP, clock, weather, camera and discovery. A furniture-only GOES_UP
    control and occluded contents/focus are separate. Missing-BODY/missing-NAV
    cases use an owned-snapshot 2x2 factorial at fixed **normal** published
    light; only target BODY/navigation/direction may vary. Matrix00 must equal
    real normal, all protected/native-context digests remain identical. Fixed
    thresholds: interaction >=24 triangles and >=8 ROI pixels with signed RGB
    interaction >=4/255. CPU-cache expiry uses 260 ms wait, not invented epochs.
    This is diagnostic, not a production visibility fix or performance test.

    Initial `21b3878` Native compilation caught protected `map::get_field`,
    unqualified GOES_UP and GCC misleading-indentation errors missed by local
    shared tests. `2ce404d` removes unnecessary field restoration, qualifies
    enums and braces the loop. Both Windows/Linux pinned Hosts now compile.
    The recorder has 34 synthetic negative tests; they are not Native evidence.
    Actual results and their limits are recorded below.

13. **Adjacent Native step/refresh wall scopes (`bb8deb1`).** Same Native calls,
    order, route/cost assertions and 12x12 matrix; no production game loop/API
    change. Eleven stack timestamps split move, checks, view center, reset light,
    build map cache, invalidate/update visibility, refresh return, inclusive
    render and tail. XYZ/cost receipt storage follows the first-ready boundary.
    Strict verifier checks adjacent monotonic scopes, source frame identity,
    accepted route/cost, separate render bookkeeping and unclamped signed
    residual, without double-counting inner rendering. Six fixed fractional
    digits give 1 ns millisecond-field quantum; budgets 0.1 us identity/3 us sum.
    All 12 old real reports are rejected as lacking these phases; nothing is
    reconstructed as a measurement. 23 movement/payload tests pass. The release
    entrypoint is 12-step; 64-step support only has shared unit coverage.

14. **Shifted-axis geometry-border invalidation (`21b3878`).** Extracted original
    eight invalidations into a shared helper. Baseline passed 26 causal controls
    then failed the protected perpendicular-border hit check. Fix invalidates
    horizontal old/new borders only for Y shift, vertical only for X; diagonals
    keep both, entering strips remain cold. All stairs/corners/ambiguity/OMT,
    negative coordinates, multi-cell steps, teleports, Z/revision/realm controls
    match the bounded cold oracle. After, each of four 97x97x3 axial fixtures:
    zero callbacks for two protected cells, 873 total derived computations,
    291 entering reads; cold coverage 28227. No measured old full-count or
    Native millisecond gain is claimed. Evidence `night-border-*`.

15. **Native apparent-light distance reuse (`f6f706b`).** Both pinned primary
    functions share the exact 5280-byte source block, SHA256
    `7e570456c43c6dcf6b02d29bd4ef8a25a63a0889006855f28483870dd6a70a14`.
    An unchanged extraction first established outer/helper/clairvoyance callback
    counts 2/1/1. File-local forwarding then gives 1/1/1; public helper fallback
    and every independent neighbor XY distance remain. Across 3180 cases,
    five enums, float bits, non-distance read order and complete 2D traces match,
    including 256 neighbor masks at four edges. No formula, threshold, public
    declaration or policy changed; upstream provenance/CC-BY-SA attribution
    is retained. Both pin adaptations have atomic digest/uniqueness guards.
    Full Release, 1401746 source/range/light/border checks, 240 Python, 30 atomic
    bridge tests, 50 inventoried autonomous CTests (14.51 s) pass. Evidence
    `night-apparent-light-{baseline,after}*`. Actual new Native qualification
    and millisecond impact remain pending.

16. **Earlier Native movement receipts.** Moved the single existing UDP checkout
    and unchanged movement step after stair upload, before camera/galleries and
    the 95/25-minute gameplay/focused steps. The 12x12 matrix, command, timeout,
    conditions and final strict verifier are unchanged; no duplicate game run.
    Early always-upload publishes program JSON/provenance and selected BMPs
    directly from `fp-movement`, including incomplete failures. It is diagnostic,
    not installer qualification. Failures still fail the job and may skip later
    default steps; final gates were not weakened. Exact structural comparison
    preserves all 87 existing steps and adds one upload. This is not a full YAML
    parser or a same-condition A/B: earlier ordering changes resource warmup.

## Actual remote evidence and next priorities

**Stair evidence at exact `2ce404d`, run 40441923163.** Packaged runtime merge
`0fa01998b3a7f037306f058902d7f4ab203c6d89` and PR head have identical tree
`12f5d42b002d30694e079d52d958aa868c52cec8`; engine pins match. Both processes
exit 0, no dumps/wrapper/debugger errors. Private receipts/readbacks:
`night-static-stair-2ce404/{12707711000,12708020013}/`.

- 1040: all day/dusk/night cases **inconclusive: normal_body_already_visible**.
  Target flags 16689 in every N/D/N; native sees=true. No factorial performed.
  Three real normal-after BMPs inspected; staircase is visible at each light.
- 1807: day/dusk have the same inconclusive reason. **Night confirmed**: raw
  terrain `t_stairs_up`, furniture `f_null`; N/D/N flags 161/16689/161, native
  sees=false/true/false, published terrain floor/stairs/floor. Raw ambient 3.7
  and raw RGB 0/0/0 stay exact. Debug published light differs, so factorial
  fixes normal RGB .365478. All four inputs retain identical protected/native
  digests and full ledgers: 197589 physical/memory cells, 20181 OMT; hidden
  contents/focus and furniture provenance remain protected. Triangles
  23138/23032/23032/23032 => interaction 106; CI pixel interaction 29741.
  Three actual night BMPs inspected; matrix11 has staircase, matrix00 and real
  normal-after do not. The latter two files are identical, SHA256
  `309eb0fff9f5ac34474f45631285e2153758402d54765dd2a502fb7e1e43f35a`;
  matrix11 SHA256 `0624642393df2401a029f2bf7a57968f56c53a3765a940492ed414ddb6238e1a`.
  Independently downloaded only 3 images, not all 4 factorial images; full pixel
  verification is CI's. Combined reserved transfer <658 KB of an 8 MiB cap.

This establishes one terrain-only render defect on a controlled 1807 night
scene, not a general visibility fix. Next: retain ordinary static terrain stairs
without granting BODY or publishing furniture/items/creatures/fields/focus or
changing discovery. Require a separate post-fix pixel/ledger test. Do not infer
absence of other stair defects from 1040's already-visible case.

**Earlier timing priority, not new-byte performance.** Selected program JSON
from `fc2ada7`'s exact-tree package used 5133467 bytes instead of full multi-GB
artifacts; all 102 frozen Git sources/27 shared Native inputs match. Win1040,
640x480, VSync 1, **software D3D12 Microsoft Basic Render Driver**: street/horde/
city first-render medians 17.49/20.27/47.50 ms; per-step paired first-ready minus
first-render medians 43.17/61.61/76.25. Gap includes actions, refresh, assertions
and bookkeeping, not one measured function. Snapshot medians 5.05/6.04/11.81,
geometry 2.42/3.13/6.83. Separate gameplay Native-cache 31.03 ms (Linux
17.34/16.49) includes forced map-cache invalidation and cannot fill that gap.
Older Win1807 JSON was truncated and excluded. Private
`night-fc2-native-component-profile/profile-summary.json` SHA256
`15c558ac119a97c90c2c1f75725ce6333f328134d543fa02eb1d1748bdc3629d`.

Use the new adjacent receipt to choose the next bottleneck. The old median
1419 Host derived misses is not GPU mesh generation. Enlarging the light-range
cache is deprioritized: recorded frames already show only 2-15 range evaluations
for 1400-3000 hits. Other historical symptoms have causal fixes/regressions;
recurrence in a current ordinary session is not established.

## Qualification boundary

At `2ce404d`, [main run](https://github.com/Neversalimus/NCMM/actions/runs/40441923163)
has passing Windows/Linux module jobs and both pinned Windows/Linux Host
compilations; actual Native qualification remains running.
[GPU Windows/Linux/Vulkan/sanitizers](https://github.com/Neversalimus/NCMM/actions/runs/40441923119),
[source/package](https://github.com/Neversalimus/NCMM/actions/runs/40441923114),
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40441923116) pass.
These results do not qualify subsequent stage15 Host bytes. The failed
`21b3878` Native compile and its repair are retained above; older ddfa/fc2
passes also do not qualify new bytes.

Hidden render-only RTX 4090 controls at unchanged renderer/module bytes retain
45 grass draws, zero CPU world raster, zero warm static uploads and 21108 open
multi-Z pixels on both rasterizers. Existing ADS/objects/muzzle/subway brightness
checks pass. Repeat affected checks after code edits; do not rerun unchanged
suites just to accumulate passes. No nightly installer, full Native qualification,
ordinary RTX 4090 frame time or action-to-first-present result is claimed.
