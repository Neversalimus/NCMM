# Autonomous First Person View optimization — current journal

## Current checkpoint

Updated28 October2026, through stage66. Baseline `1284537`; working branch
`codex/fpv-0131-fps-controls-20261024`; draft [PR223](https://github.com/Neversalimus/NCMM/pull/223).
Supported Native pins remain1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83`
and1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.

**15ms ordinary game frames and action-to-first-correct-present remain
unqualified.** Isolated CPU/model timings, WARP CI and render-only RTX tests
are separate evidence. Warm results do not erase first-use regressions.

No local Native/game/input harness, mouse capture/warp/focus, elevated profiler,
NVIDIA/power/priority changes or subagents. Background games remain running.
Use the existing checkout/build, read-only synced sources, pure/shared-module
tests and the inventoried hidden render-only suite. No main merge, release,
feed or installer publication before user finalization/review.

Detailed stages1–63, failures, provenance and raw measurement references remain
[in the immutable prior journal](https://github.com/Neversalimus/NCMM/blob/616d718810e665892b1e7eaba33030cf8a00c7b4/experiments/FirstPersonView/reports/night-optimization-20261027.md).
Full stage65 detail is also retained in local Git revision `1c5ce96`.
Private evidence/checkpoints are in `../fpv-0131-work/`; do not overwrite
measured summaries or derive new data from old truncated reports.

## Retained changes

| Area | Concrete change and bounded evidence |
| --- | --- |
| GPU texture ownership | One immutable texture owner replaces4096 duplicates; pointer capacity86320→16 bytes. Exact geometry/material/packet digest retained. |
| GPU scene preparation | CPU-only transparent sorting is skipped during GPU collection. Early opaque-floor templates retain exact packets.97x97 cold CPU producer examples18.11→6.88ms and21.20→9.51ms; no GPU/present timing. |
| Coordinates and snapshots | Owned sparse XYZ lookup avoids warm dense-index allocation. Host detail storage removes warm3048555-byte allocation at9409 cells. Native pointers remain callback-local; exact coordinate/epoch/window invalidation remains. |
| Map boundaries | Shift-axis border invalidation preserves perpendicular hits and cold entering strips. Reuse overlapping slab facts; axial97/diagonal193 entering reads versus9409 cold reads. No visibility/range reduction. |
| Native submap work | Retain exact turn/phase dependencies, skip redundant actualization and reuse owned support nodes. Node-allocation model8193→1; source/trace/atomic two-pin controls pass. Full details and source SHA guards are in the prior journal. |
| Native visibility | Reuse existing integer range results. Exact standard float distance uses a bounded root lookup for qualifying callbacks; unusual/independent cases retain original Native behavior. Public formula, rounding and APIs remain unchanged. |
| Floor cache | Uniform solid upper floors are resolved independently of mixed lower storage. Uniform upper gaps keep every ordered mixed-lower roof check.859 frozen-oracle cases/22663405 checks; upper terrain reads17424→121 in qualifying models. Dirty/invalidation rules remain. |
| Static architecture | Terrain-only ordinary stairs can render as static structure within physical sight while BODY/contents/discovery remain protected. Offset flights retain cut supports. Causal Native1807 night fixture confirms the earlier missing geometry;1040 control was inconclusive. |
| World visuals | Continuous absolute chain-link phase; standard radio skyline uses17.5m open lattice rather than a box. Exact IDs/resident disclosure required; legacy/modded layouts retain their path. V1 and old/new Host compatibility controls pass. |
| FPS controls | Correct held tap/chord and recovery state without manufacturing extra Native actions. Current controlled184-case matrices cover added W+D/tap cases. Physical input during ordinary play is not qualified by these fixtures. |
| CPU volumetric light, stage65 | Prepare up to12 source positions, camera depths and attenuations once per frame on the stack. Same source order, propagated RGB, range, occlusion and ray sampling; no persistent cache/ABI/settings change. |

Historical local producer measurements are not a universal speedup: the
stationary-boundary first sample worsened2.5233→2.9792ms despite a lower
median3.3933→2.9931; local edits did not improve. See the preserved raw data.

## Latest measured performance

### Isolated actual Native visibility

Source `12a7abcd1bd4f058bc71f233a4df63946bcc067b`, run40600292716.
Source-bound selected program JSONs, paired same-process caster calls,
protected float/Native bytes exact. Warm street/horde/city medians(ms):

| Pin | Before | Retained distance lookup |
| --- | --- | --- |
|1040|4.6542 /5.4176 /6.4740|3.9230 /4.5155 /5.2764|
|1807|5.7838 /6.4826 /8.2441|4.7483 /5.2877 /6.7237|

This is about15–19% less warmed isolated caster wall time, not whole-game
FPS or GPU execution. Individual pairs regress; first1807 horde call worsens
9.46→14.77ms. Raw: `night-sight-12a7abc/`, independently verified summary
SHA256 `392c053c4419ab2fcdbe13a50408c69d33567ab32f0e6a74c75d95c9e660c13a`.

### Current PR walking, first-ready boundary

Source `cdf6f796ad9764a94d98a576c789822d01bc80bd`, runtime merge `717ee22`,
exact equal tree `2b43614f26a368176204b296c75774e9e1345cd7`.
288 accepted steps/144 cache-child records; clean processes and exact source
receipts.640x480, WARP software adapter, VSync1; crossing medians(ms):

| Pin/route | Accepted action | First-ready | Main cache | Seen-cache child |
| --- | ---: | ---: | ---: | ---: |
|1040 street|24.544|56.219|12.377|5.476|
|1040 horde|30.207|75.326|17.103|7.719|
|1040 city|34.333|103.121|20.327|10.179|
|1807 street|37.889|97.969|16.266|7.220|
|1807 horde|30.546|84.701|16.812|7.800|
|1807 city|34.967|104.097|20.877|10.560|

Independent medians must not be added. First-ready is a controlled fixture
boundary, not display scanout. Independent revisions/runners are not causal
A/B. No-shift action.357–.400ms excludes refresh/render. Raw:
`night-movement-cdf6f79/strict-current-movement59.json`, SHA256
`0e25f56b109f48e50e3b6100c7e89c3fcc4986c1b8eafef1ffe458f96ed8a133`.

### Actual callback attribution

Source `2d9401b`, run40619494293, schema2: both26-turn reports verify five
exact program JSONs,106 copied shared inputs, engine/Host/module/privateSDL
identity and clean exit.25 callbacks, at most one per turn; no nested/legacy
reuse/skips. Viewport672x704; GPU image672x704, CPU image572x600. WARP.

| Empty-scene8 turns, median ms |1040|1807|
| --- | ---: | ---: |
| Whole turn |78.3942|136.5442|
| Outermost perspective draw |65.2547|114.7093|
| Entry Native cache, already warm |.01865|.01725|
| Visibility |4.96345|10.84335|
| Post-visibility preparation |2.3044|4.2984|
| Module call |55.39065|96.2005|

Warm entry-cache cost does not contradict crossing cache12–21ms above.
Module wall includes its Native API/preparation/raster/submission work, not
pure GPU time. The fixture alternates672x704 Native and640x480 manual frames;
viewport cache thrashing is a hypothesis, not ordinary-game evidence.
The separate110 manual-render aggregate cannot attribute the root callbacks.
Raw `night-turn-2d9401b/strict-early-turn-{1040,1807}.json`; summary SHA256
`9ac027a31986c0583822f2d3afcf29b47efe542255b40c69b72444043f113ac6` /
`d5781572e26e7e934339b915a6be2ded45b46d703aeec51793286d4158b21146`.

Schema3 source `616d718` directly observes the existing validated CPU-metrics
event inside the current module call, adding source cell/slice counts and
fresh image/frame/geometry/raster/effect/counter fields. Missing data remains
unavailable; stale/duplicate/wrong-image reports are rejected. Components may
overlap and must not be summed. No extra render/poll/wait/clock/allocation.
318 Python,96 atomic two-pin tests,61 fake-clock checks and10 compiled bad
controls pass; both actual schema3 reports from run40629160097 are verified below.

Schema3 actual1040 now verifies artifact12792943027 (38865 ZIP bytes), all
five program JSONs/106 copied inputs/privateSDL/source/Host/module and exit0.
Summary SHA256 `966a1546c71c1e6f7d542f6c25077119a637c5313f9e90fb4678979ff2c1bebf`.
25 callbacks/26 turns;9811 source cells and21 slices per callback. The eight
empty CPU callbacks have module80.9638ms/frame80.9578ms, geometry42.29395ms,
raster30.49035ms, effects.09745ms medians, three raster calls/22886 triangles,
23078240 cache bytes. Components may overlap. This directs the next CPU work
toward scene preparation/raster, not tiny effects. It remains a WARP controlled
fixture with the viewport interference below;1807 and full qualification are
pending. No ordinary/RTX/15ms conclusion or causal revision comparison.
Raw `night-turn-616d718/strict-early-turn-1040.json`;104341/8388608 transfer bytes
reserved so far. Texture analysis already shares identical sources within each
callback; inspect terrain-feather profile repetition instead of duplicating it.

Schema3 actual1807 independently verifies artifact12793781179 (39098 ZIP
bytes), the same106 inputs/privateSDL/clean process/source identities and
25 callbacks/26 turns. Summary SHA256
`1d82f1419d26423e488a61ad277c90ed0dbd28dd0fe51fddf9513301fc2349db`.
12522 cells/21 slices; emptyCPU frame98.6679ms, geometry52.27765ms,
raster34.9698ms, effects.1424ms; three raster calls/27724 triangles and
23078240 bytes. Both artifacts reserve143379/8388608 bytes. The priority is
consistent across pins, not a causal or equal-scene performance comparison.
Whole integration/full qualification and new volume-source Native CI remain
pending. The next bounded experiment targets repeated terrain-feather
calculations; no new renderer edit has yet been made.

### Stage69: isolate diagnostic viewport interference

A pure exact-shared-module experiment at `fdbba45` alternates manual640x480
and root672x704 frames versus matching both672x704. Both sequences advance
the same render timestamps and hand composition; rootCPU572x600 pixels agree
exactly at each timestamp. Seven alternating pairs, six root samples/lane:
root medians13.687 versus2.0028ms, geometry.8601 versus.50445ms; raster calls
12 versus6 per six root frames. The unchanged static world can be retained
only in the matching sequence. This demonstrates fixture-induced work, not
a production optimization, actual Native attribution, ordinary movement gain
or15ms qualification. Do not subtract these numbers from prior Native data.
The first constant-timestamp variant is preserved separately; the advancing
timestamp result above is the useful control. Evidence
`night-viewport-alternation-model69-r2/summary.json`, module SHA256
`b533507f7c2c088fdb9e3a357ea097199ccf4ed9907d92e870cc458f9d0b56c9`.

### Stage65 CPU light preparation

Frozen full-renderer oracle:168 frames,2268 assertions,16329600 exact RGBA
pixels, identical depth/identity/effect buffers and7729476 integration samples.
Covers three Z levels, camera offsets, ties,16 candidates,0/1 thresholds,
smoke/fire/fog, source occlusion and three image sizes. Fixture linkage and
invalid-metadata failures were corrected before measurement.

Seven alternating pairs per eight workloads,64 warm renders/lane:
12-light640x480 effect medians4.29994→3.98633ms; full CPU render5.81375→5.49528.
Median paired ratios.92746/.93979.320x240 fire/occluded effect ratios
.89956/.86904;572x600 ratio.95933. One-light1.00835 is unchanged within noise;
inactive code does not execute the new preparation. Background load remains.
No ordinary Native/RTX GPU/present/15ms qualification follows.
Evidence `night-volume-sources-model65-r4/` and `night-volume-sources-timing65/`.

Full Release, six affected CTests(3.07s),171 exact indexed frozen sources and
the rebuilt hidden render-only RTX4090 suite pass. Control retains45 grass
draws, zero warm static uploads/CPU world raster, multi-Z21108 open/0 closed
pixels;21 fallbackADS,12 authoredADS,34 objects, two muzzle composites and
subway dark0.00 pass. This is correctness evidence under background games,
not a fresh game benchmark. No Native game/input/mouse/focus is used.

Stage70 closes the remaining cold-world/offscreen risk for stage65. Seven
alternating pairs per four workloads,32 fresh frame-vector/no-world-cache
renders per lane, retain exact pixels/depth/IDs/effects/integration counts.
640x480 twelve-light fire full-frame medians13.42206→12.64019ms (paired.94569),
effects4.67569→4.17788ms (.86809); lights behind the camera5.53738→5.39847ms
(.95295). Inactive/one-light full-frame ratios1.00827/1.00257 are unchanged
within background noise. No broad first-use regression is established.
This means cold static-world construction in the owned CPU renderer, not
cold process/driver/Native launch or ordinary15ms game qualification.
Evidence: `night-volume-cold70/summary.json`. No new source edits or reruns of
the already passing unrelated suites were needed.

## Rejected experiments

| Experiment | Reason not retained |
| --- | --- |
| Native exponent MRU | Actual paired common street/horde regressions; removed from production and replaced by exact distance work. |
| Denominator/intensity reuse, stage51 | Three broad models did not justify introducing dependencies/rounding risk. |
| Lazy mixed-lower floor reuse, stage60 | Three designs each preserve940 cases/24723071 checks, but broader seven-scene inline/noinline timings regress common paths. Best target-only gains are insufficient; production floor unchanged. |
| Alpha-blend fast paths, stage64 |3,670,016 exact blend cases and270 frames/3,203,001 painted samples pass, but seven paired workloads show mixed-alpha blend1.207x and mixed flash1.121x. Reject; no robust smoke gain. Private `night-weapon-blend-model64-r3/` and `night-weapon-blend-timing64/`. |

Stage67 also rejects CPU retained-coordinate integration. The private full
renderer preserves224 frames/3,143 assertions,12,364,800 pixels and4,356,630
volume samples across turns, camera offsets, replacement source buffers,
light/epoch/origin/grid/Z-window changes and release. Seven alternating pairs
per six cold/warm workloads give a.75169 warm sparse ratio, but cold dense
1.08448 (1.89129→2.03434ms) and cold small1.03766. A warm gain does not justify
a first-use regression or extra832944–1354896 retained bytes. The first large
timing fixture correctly failed the65536-cell admission limit; its partial
measurements are excluded. Production CPU/index/module bytes remain unchanged.
Evidence: `night-cpu-coordinate-model67/` and `night-cpu-coordinate-timing67-r2/`.

## Remote qualification and next steps

- Full `eae8d02`, run40580475214: all8 jobs pass. Source-bound selected
  reports and CodeReview tails were saved before PR advancement. It does not
  qualify later distance/floor/volume source bytes.
- Full `12a7abcd`, [run40600292716](https://github.com/Neversalimus/NCMM/actions/runs/40600292716):
  all8 jobs, including the required gate122619592124, now pass. Exact run/head
  and job conclusions are saved in `night-stage66-12a7-full-status.json`.
  Historical logs are not queried against the later PR head.
- Current PR `cdf6f79`, full40615847078: both module/Linux Native jobs pass;
  both Windows gameplay/focused checks pass, galleries/paired benchmark remain
  in progress. Independent GPU40615847005/source40615847050/runtime40615846995
  pass. No full current-CDF qualification yet.
- Diagnostic `2d9401b`, integration40619494293: seven executable jobs pass,
  required gate fails at the integration tier; do not label the full run green.
- Fresh schema3 `616d718`, integration40629160097: pending. Read only the five
  selected early program JSONs with source/run identity, bounded HTTP ranges
  and independent exact-source verification. No artifacts from another source
  may qualify the changed volume renderer in local `1c5ce96`.

Next: target the measured geometry/raster cost. The private terrain-feather
profile experiment is underway; its Native impact remains unqualified. Investigate viewport alternation only with a separate controlled
fixture; preserve old measurements. Keep current full PR run intact. Push
tested work to an available existing diagnostic branch when appropriate;
retain draft/review and no-release boundaries. CI logs exclusively CodeReview
at current PR head; metadata/program JSON are not log fallbacks. Native source
transfer ledger has161994 bytes remaining; no new upstream body was fetched
during stages57–66. Continue solo until user finalization or a real limit.
