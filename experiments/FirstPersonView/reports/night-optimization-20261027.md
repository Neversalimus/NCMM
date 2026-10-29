# Autonomous First Person View optimization — current journal

## Current checkpoint

Updated29 October2026, through stage111. Baseline `1284537`; working branch
`codex/fpv-stair-frame-stages-20261029`; latest delivered release [PR231](https://github.com/Neversalimus/NCMM/pull/231).
Supported Native pins remain1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83`
and1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.

**15ms ordinary game frames and action-to-first-correct-present remain
unqualified.** Isolated CPU/model timings, WARP CI and render-only RTX tests
are separate evidence. Warm results do not erase first-use regressions.

No local Native/game/input harness, mouse capture/warp/focus, elevated profiler,
NVIDIA/power/priority changes or subagents. Background games remain running.
Use the existing checkout/build, read-only synced sources, pure/shared-module
tests and the inventoried hidden render-only suite. The user has now requested
finalization, main integration and installer delivery, followed by continued work.
ReleaseA9 passed all8 jobs of full40674207425 and merged into main as
`cd9ba08d47583482f67374d59f5bf0cc871416a9`. Both exact0.13.1TEST installers
are [published](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-v0.13.1-test-a9f3ef7480f0),
with checksums/source qualification and verified anonymousHEAD200 downloads.
Stage90/CDB passed all8 jobs of full40692590155 and merged [PR225](https://github.com/Neversalimus/NCMM/pull/225)
as main `b3b6e24297be9fedad4816ec56f3970fe06858cb`, exact tree `e0e8052bbd14e07209d71080f731d9320804019d`.
Its two checked TEST installers are [published separately](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-v0.13.1-test-cdb929b3453a).
All38 payloads/106 copied Native inputs/171 frozen sources and104 resource
sources match. Anonymous installerHEAD200/length checks pass. The prior A9
release remains available. Drafts226–231 retain exact source heads and their
ongoing full captures. Neither the new growth candidate nor those drafts is
included in the CDB archives.

Detailed stages1–63, failures, provenance and raw measurement references remain
[in the immutable prior journal](https://github.com/Neversalimus/NCMM/blob/616d718810e665892b1e7eaba33030cf8a00c7b4/experiments/FirstPersonView/reports/night-optimization-20261027.md).
Full stage65 detail is also retained in local Git revision `1c5ce96`.
Private evidence/checkpoints are in `../fpv-0131-work/`; do not overwrite
measured summaries or derive new data from old truncated reports.

## Delivery through stage109

PR230/59f snapshot capacity and PR231/6da visible-substrate repair have each
passed8full jobs,3GPU jobs and source audit. Exact package/report/resource
proofs match38payloads,106Native inputs,171frozen sources and104resource
source/license files. Latest mainbf622288 retains the independent2feedJSONs;
every other byte equals qualified6da. Both [6daTEST installers](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-v0.13.1-test-6da17a35142a)
are public, with anonymous HEAD200/length verification. Prior releases remain.
Later232-236and stages110/111are not in that release. User explicitly requests
continued overnight work until stopped; same-chat heartbeat repeats20min.
No subagents or local game/input/device/mouse/focus use is authorized.

## Stage101 delivery checkpoint

B9F/CPU93 passed all8jobs of full40718418978, all3GPU jobs and source audit.
PR227 merged as main2fb1169 while preserving the independently updated two
feed JSONs; every other path equals qualified B9F. The two exact TEST packages
and qualification/checksums are [published](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-v0.13.1-test-b9f4ee984746).
All38payloads/106copied Native inputs/171frozen sources/104resource files and
licenses agree. This delivery does not include later PR228-232 or stage103.
Those exact heads remain fixed. At this source capture PR228 full8has passed;
its separate package qualification is in progress. No ordinary15ms claim.

## Stage104 delivery status

B81/GPU94 passed all8jobs of full40725731505, all3GPU jobs and source audit.
PR228 merged as main69a121e while retaining the independently updated feed
JSONs; every other Git path and byte matches qualified B81. Both exact checked
TEST installers, source proof and checksums are [published](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-v0.13.1-test-b81e22d6ab8b).
This delivered version does not include later229-233 or stage104. Their heads
and earlier qualification receipts remain fixed. PR229 full8has now passed;
its exact primary package/resource qualification is still pending.

## F102 delivery checkpoint

PR229/F102 snapshot-entry reuse passed all8jobs of full40731702421, all3GPU
jobs and source audit. Both primary release JSONs match the exact packages;
all38payloads/106Native inputs/171frozen sources/104resources and licenses
were verified. Main0e59906preserves the2feedJSONs and every other path equals
qualified F102. The [two TEST installers](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-v0.13.1-test-f102c628188f), source proof and checksums are published;
anonymous HEAD200and exact lengths pass without body downloads. Prior A9/CDB/
B9F/B81 remain available. Later230-234and stages104-106are not in this release.

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
| Terrain feather, stage71 | Compute the existing edge-width profile once per texture row/column; skip finite-color additions with exactly zero weight. Preserve every texture pixel, floor triangle, material, range and source call. |
| CPU ordering/projection, stages73/79 | Compute selected-cell distance once for stable ordering and reuse exact uncut view transforms. Complete frame and near-plane scalar proofs; broad isolated CPU gains about1.5–5%, not Native/RTX latency qualification. |
| Ground templates, stage76 | Reuse identical owned blends within one callback after all original provider reads. Mixed32 CPU8.819→5.039ms; CPU preparation for GPU8.805→.939ms in controlled models. No state survives the callback. |
| Texture reference storage, stage80 | Allocate only requested layers.65536 cells/three layers use1.5MiB rather than6MiB pointer storage. Large CPU models improve about7%; small long control costs about1%. |

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

### Stage71: exact terrain-feather profile reuse

Fresh Native geometry timings motivated this bounded shared CPU/GPU producer
change. The blend previously evaluated the same sine profile inside every
pixel/edge and sampled neighbours even for zero contribution. Two bounded
128-float stack arrays retain the same formula, inputs and evaluation order
once per row/column. A zero weight leaves finite packed colors unchanged.
No extra Native call, persistent cache, shader, API, LOD, quality or setting.

Frozen original full-renderer comparison passes96 cases/1344 assertions,
7372800 exact frame pixels and1765276 complete raw ground-texture values.
All16 edge masks, source widths1/32/33/64/128/512, odd/padded heights/pitches
and mixed zero/partial/full alpha are covered. Depth, identities, effect
buffers, geometry/raster workload and offscreen texture samples agree.

Seven alternating same-process pairs per five scenes and two paths,24 fresh
builds/lane: mixed surfaces cold CPU12.0045→8.95642ms (paired.72616), producer
collection14.0465→7.65063ms (.52904); straight road5.65508→5.54408ms (.97262),
collection1.10082→.80696ms (.73305). Checker stress CPU12.89854→8.34842ms
(.63597), collection15.27517→6.87663ms (.46889). Odd33 textures also improve.
No-edge paired ratios1.0141/1.0256 are within the uncontrolled background
noise of the unchanged path. Collection timings are CPU work, not GPU execution.
These are owned component scenes, not actual Native/ordinary/first-present
or15ms qualification; do not apply the ratios to earlier Native numbers.

Full Release, six affected CTests(3.13s), all171 exact indexed source hashes
and the rebuilt hidden render-only RTX4090 scene pass.21 fallbackADS,
12 authoredADS,34 objects, two muzzle composites,45 grass draws, unchanged
static uploads and dark subway controls remain intact. No Native game/input/
mouse/focus/subagent is used locally. Evidence: `night-terrain-feather-model71`,
`night-terrain-feather-timing71`, `night-stage71-{release-build,affected-tests,
hidden-render}.txt` and `night-stage71-index-identity.json`.

## Stage73 — exact CPU cell ordering (local; Native qualification pending)

CPU world sorting recomputed each cell's squared distance for many comparisons.
The retained variant ranks only selected cells once, keeps the stable equal-key
order and restores the same index traversal. GPU collection and persistent
caches are unchanged. Scratch ranks cost8 bytes per selected cell, plus the
larger stable-sort temporary buffer; at the65536-cell limit the incremental
scratch bound is approximately640KiB. Empty/single-cell orders allocate nothing.

The private full renderer preserves168 frames/2,268 assertions/16,329,600
pixels/7,729,476 volume samples, depth and owner/creature IDs and work counters.
Seven alternating pairs per six fresh-build workloads give paired ratios:
small legacy.96681, small multilevel.98498, five-level dense.94880,
constructed21-slice.95004, hidden dense.99590, five-level city.97322.
Dense medians50.753→48.48325ms; the constructed21-slice scene has12,789
cells and17.10975→16.451625ms. Hidden independent medians2.03524→2.10468ms
diverge from the paired ratio: no reliable hidden-scene speedup is claimed.
These are CPU models under background load, not captured Native/GPU/Present
timings. Ordinary15ms and any RTX4090 gain remain unqualified.

The initial all-source-cell distance table is rejected: hidden dense1.03918x
and unnecessary allocation despite sparse/empty output. Both experiment variants
and their timings are preserved. The retained source passes the Release build
and7 affected CTests (3.06s). Keep this commit local while the exact48d8c23
Native terrain comparison is running; do not change that proof's PR head.
Evidence: `night-world-order-model73-r2/`, `night-world-order-timing73-r2/`,
`night-stage73-release-build.txt`, `night-stage73-affected-tests.txt`.

## Stage75 — durable owned-child receipts (local; rerun pending)

The new same-Host1807 comparison reaches its first reference child but times
out after600s. CodeReview job122667781069 confirms the timeout; the source-bound
program receipt has no completed trials, and both original module and fixture
are restored. There is no paired Native timing or image qualification yet.
The internal phase is still unknown; do not label this a game crash or extend
the deadline without evidence. The ordinary automatic48d8c23 qualification
continues separately; keep its exact PR head until diagnostics are retained.

The local runner now saves typed child states before launch, while running and
on timeout/launch error/exit, including PID, bounded wall interval and exit code.
It logs bounded Native phase names into the normal CI log every30s for later
CodeReview diagnosis; raw stdout/progress text is not repackaged into program
JSON. The original600s limit, owned-child cleanup, unchanged-Host checks, full
capture equality and module restoration remain. Final pair verification records
its own failure stage. Twelve mock/receipt/negative tests pass; no real local
child, window or input is launched. This improves diagnostic completeness;
it does not yet fix the Native comparison's internal timeout cause.
Evidence: `night-stage75-pair1807-final-review.json`,
`night-terrain-48d8c23/12797852135/`, `night-stage75-child-receipt-tests-r2.txt`.

## Stage76 — callback-local ground blend templates (local; Native pending)

Identical mixed-ground textures were baked and analysed again for each cell.
The renderer now reuses an owned bake within one callback, keyed by the edge
mask, five immutable image identities and exact fallbackRGB bits. All neighbor
and Native provider reads still occur before lookup. The profile, source pixels,
material finishing, cell IDs, range, lighting and geometry remain unchanged.
The104-byte key can add at most about6.25MiB of key payload for65536 distinct
blends; repeated inputs reduce the pixel/mip owners. No state crosses callbacks.

Exact models preserve96 frames/7,372,800 pixels/1,765,276 complete sampled floor
values, plus60 frames/4,608,000 pixels and255,928,400 complete emitted GPU values
(positions, materials and texture bytes). Masks0–15, source widths1/32/33/64/
128/512, padded pitch, mixed alpha, legacy and three-level layouts are covered.

Seven alternating pairs×24 fresh builds/lane: mixed32 coldCPU8.81921→5.03863ms
(paired.56912), CPU GPU-geometry collection8.80458→.93930ms (.10118).
Straight road collection.91191→.27377ms (.31090), checker6.74171→.67403ms
(.09840), odd33mixed8.47138→.98475ms (.11294). No-edge controls remain
approximately unchanged (.98948/.99329); no speedup is claimed for them.

The initial168-byte key and short16-build unique-source guards showed variable
3–7% regressions. A compact key keeps every exact identity field; a predetermined
longer128-build/lane guard gives unique32 CPU/collection ratios.98432/.98380
and unique64.99691/1.00296. All short and long raw results are retained; no
reliable unique-source gain is claimed. These are same-process CPU models under
background load, not cold-process, Native, GPU execution, Present or15ms proof.

Release and7 affected CTests pass (3.10s). The newly built render-only hidden
RTX4090 suite passes: fullGPU path, zero warm static uploads,34 objects, ADS,
stairs, multiZ, grass, radio and subway lighting. It uses no Native game/input;
it verifies images/behavior and does not supply ordinary hardware FPS timing.
Evidence: `night-ground-template-model76-r2/`, `night-ground-template-timing76-r2/`,
`night-ground-unique-confirm76-r2/`, `night-stage76-{release-build,affected-tests,hidden-render}.txt`.
Keep local while protected48d8c23 full/82b diagnostic source logs are retained.

## Stage77 — coarse-clock mock guard (local)

A mocked child may start and finish within one UTC clock tick. A local fixed-clock
probe reproduces the overly strict `ended > started` test assertion; the mock
now accepts equality, with a dedicated fixed-clock regression test. The actual
Native verifier still requires a positive bounded interval, clean exit, matching
source/Host/module and exact captures. Thirteen receipt/mock/negative tests pass.
The82b diagnostic Windows module job fails at this test step; its exact CI
message remains unconfirmed until CodeReview can read that source as current PR
head. Do not present the local reproduction as a confirmed CI root cause.
Evidence: `night-stage77-coarse-clock-tests.txt`. No runtime or local Native change.

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

## Stage72 — same-Host terrain ablation (Native result pending)

The previous Native smoke/creature profile resets pavement and cannot isolate
mixed-ground feather work. An explicit CI dispatch now selects grass control,
straight road and mixed pavement/gravel/dirt/grass in a fixed97×97 physical grid.
It records warmup phases separately from96 steady frames; CPU geometry/recording
remains distinct from GPU execution, which is unavailable.

A separate CMake target compiles the current module with one frozen pre-71
terrain include (SHA256 `4b9a36a6…0d86ec`). Default builds use the production
include without a runtime branch. The CI runner reuses the existing FPS fixture
and module build, preserves the same Host, alternates seven reference/current
pairs, restores the production DLL and clears the temporary CMake option.
It refuses local execution. No local Native, SDL window, focus or input was used.

The pure shared-module gate passes24 cases,177 checks and1,924,596 exact pixels.
The initial gate test exposed Windows short/long-path normalization; the corrected
runner passes9 positive/negative receipt tests, including partial pairs, stale
source/module, failed exit, changed image, wrong phases and local-launch refusal.
The ordinary two-scene profile gate remains unchanged. The existing module and
GPU-geometry CTests and96 atomic source-transform tests pass. The benchmark
environment isolation passes174 checks.
Native compilation, exact captures and paired timings still require CI; this
stage is diagnostic infrastructure, not another measured game speedup.
Private checkpoints: `night-terrain-dll72/`, `night-stage72-verifier-tests-r2.txt`.

Stage74 rejects deferred CPU analyses for simple GPU ground. The full model
preserves60 CPU frames/901 assertions/4,608,000 pixels and255,928,400 complete
emitted GPU values, including texture bytes, but32px pavement collection has
1.12444 paired ratio (+.0375ms).128px gains of about.15ms are not a robust
reason for another preparation path. No production74 renderer change.
The first fixture used an incorrect held/item override and failed compilation;
it has no measurement and is excluded. Evidence: `night-ground-prepare-model74-r2/`
and `night-ground-prepare-timing74/` (seven pairs per16 CPU/collection workloads).

## Stage79 — reuse CPU vertex view transforms (local; Native pending)

CPU triangle projection transformed each input vertex repeatedly for clipping
and projection. It now computes three input view transforms once. Unclipped
triangles reuse them; partial near-plane clipping still transforms the exact
same world-space interpolated vertices. GPU collection, clipping thresholds,
rounding, material finishing, range and persistent state are unchanged.

The complete renderer model preserves168 frames/2,268 assertions/16,329,600
pixels/7,729,476 volume samples, including depth and IDs. A separate near-plane
guard preserves23,328 cases/1,189,296 projected scalar values bit for bit across
yaw/pitch, six boundary depths and12 shape/material variants. Its initial
cross-namespace fixture type error failed compilation and supplied no data.

Seven alternating pairs per six fresh CPU workloads give paired ratios:
small legacy.96520, small multilevel.98429, dense five-level.98473,
constructed21-slice.96917 and city five-level.98295. Dense frame medians are
49.01025→48.08013ms (geometry27.80475→26.71550); city52.43913→50.50875ms
(geometry30.13963→28.76438). Components overlap and must not be summed.
The hidden control has diverging independent medians1.94303→2.01410ms and
paired ratio.97893; no hidden-scene gain is claimed. These same-process CPU
models under background load do not qualify Native, GPU execution, Present,
ordinary FPS or15ms. Release and7 affected CTests pass (3.03s).

Evidence: `night-project-view-model79/{summary,near-summary-r2}.json`,
`night-project-view-timing79/summary.json`, `night-stage79-affected-tests.txt`.

## Stage80 — allocate texture references by requested layer (local; Native pending)

The callback used one12-layer pointer table for every cell, including unused
layers. It now allocates each requested layer separately, preserving all12
layers, per-cell/held memoization, borrowed source lifetime and provider calls.
For65536 cells, one/three active layers use.5/1.5MiB rather than6MiB of pointer
storage. The renderer object adds264 bytes; requesting all12 layers keeps the
same heap payload and uses up to12 allocations instead of one. Nothing survives
the callback, and no range, source, geometry or quality setting changes.

Exact models preserve168 frames/16,329,600 pixels and60 frames/255,928,400
complete emitted GPU values. Another15,345 assertions cover all cell/held
layers, invalid sizes/pitch/providers, prepare transitions, memoization and
exact provider call order at1/225/12789/47045/65536 cells. First two auxiliary
launches failed CMD path parsing and supplied no measurements; normalized
Windows paths pass. Their failure records remain retained.

Seven alternating fresh-frame pairs: dense47045-cell CPU50.93063→48.14113ms,
paired ratio.92926; city50.09900→47.07638ms, ratio.92340. The constructed
21-slice12789-cell model is15.26100→15.19088ms (.99040); it is not Native.
The first small legacy guard regresses1.89%. A predetermined128-build/lane
confirmation still has small225-cell ratio1.01132 (independent medians
2.49275→2.57417ms); small675-cell multilevel.98643. Retain the bounded
small-scene cost for the large-scene CPU and memory benefit; do not present
it as a universal speedup. Background applications remain running.

Release/7 affected CTests pass (3.07s), as does the affected hidden render-only
RTX4090 suite: full GPU path, zero warm static uploads, ADS,34 objects, stairs,
grass, multiZ and subway lighting. No local Native/input/focus or mouse use.
These models and correctness checks do not qualify ordinary GPU FPS, Present,
action-to-first-correct-game-frame or15ms.
Evidence: `night-image-layers-model80-r3/`, `night-image-layers-timing80/`,
`night-image-layers-small-confirm80/`, `night-stage80-*-tests.txt` and hidden-render.

Stage81 rejects per-triangle fog-density hoisting. The exact168-frame model
passes, as do112 timed complete-frame comparisons across16 clear/fog/smoke/wall,
legacy/mixed-roofed multilevel,320×240/572×600 workloads. Seven alternating
pairs×8 fresh frames show fog-multilevel ratios1.05038/1.04407 and legacy wall
1.04425/1.02502; some clear/smoke lanes improve, but the broader regressions
do not justify retention. No production raster/fog change. Evidence:
`night-fog-triangle-model81/` and `night-fog-triangle-timing81/`.

## Stage82 — identify Native terrain setup stall (CI follow-up pending)

A temporary stacked [draft224](https://github.com/Neversalimus/NCMM/pull/224)
exposes the completed82b diagnostic checks to CodeReview at their exact head
while protected PR223 full48 continues. No historical-head log fallback,
local Native/input run, merge or release is used. All three relevant logs
are saved; both Native excerpts are120000 characters and explicitly truncated.

CodeReview confirms the Windows mock-clock failures already addressed by77:
instant fake launches have equal start/end UTC ticks. Its local fixed-clock
13-test guard passes; strict positive real Native intervals remain unchanged.

Both Windows first reference children reach `gpu_benchmark_scene_16` within
60–90s and stay there through570s, then time out at600s.1807 previously reports
`terrain-road_gpu_frame_87` at60s. Thus the observed stall is before mixed-scene
CPU/GPU mode timing, not a measured slow raster sample. Reset, material setup,
cache refresh, digest and canvas creation still need distinguishing. Original
module/fixture restoration is confirmed by typed program receipts. There are
zero completed terrain A/B trials; no speedup or root cause is claimed.

Add scenario-labelled primary CI progress for entity/player/tile/graphics reset,
terrain setup, light/map/visibility refresh, layout digest and canvas creation.
New I/O is outside measured frame loops, no-op in non-focused runs. Keep the
same physical scenes,96 steady frames,14 children and600s child deadline.
Existing96 atomic source-transform tests pass (45.397s); actual Host compilation
and the finer-phase rerun remain CI work. The newly triggered draft224 old82b
automatic run is still compiling Native, with no Native capture steps started;
advancing that diagnostic branch may supersede it. PR223 full48 is preserved.

Evidence: `night-stage82-diagnostic-review-{122700193431,122700193403,122700193436}.json`,
`night-terrain-82b21e6/12802481666/`, `night-stage82-atomic-transform-tests.txt`.

## Stages83–84 — reject or hold speculative CPU shortcuts

Stage83 skips unused material-footprint calculations in a private CPU model.
All168 frame comparisons remain exact, but seven alternating fresh-frame pairs
regress common small/dense/city workloads by1.6–2.5%. No production change.
Evidence: `night-unused-footprint-{model,timing}83/`.

Stage84 attributes repeated texture analysis in constructed47045-cell CPU
scenes, then tests an owned, mutation-checked cross-callback cache privately.
The initial deep-copy variant regresses cleanup-inclusive cold frames by11.2%
and is rejected. Shared ownership preserves168 frames/16,329,600 pixels;
8,717 direct assertions cover pitched rows, mutation, ownership, eviction,
live memory limits and allocation failure. One intermediate variant fails
compilation and supplies no measurements. All variants/raw records remain.

Seven alternating pairs show useful warm512-source64px reuse:83.2695→52.3138ms,
paired ratio.61103. True first-call data do not establish a robust cold benefit;
cleanup-inclusive cold costs remain recorded separately. Changing all512
sources each rebuilt frame regresses80.2478→85.5982ms (paired1.05567), with
zero hits. The cache is NOT integrated or enabled. Actual Native hit rate and
first-action evidence are needed before considering that memory/latency tradeoff.
Evidence: `night-analysis-cache-model84-r4/`, `night-analysis-cache-timing84-r3/`,
`night-analysis-cache-first-use84-r3/`, `night-analysis-cache-churn84-r4/`.

## Stage85 — repair the invalid controlled terrain fixture (CI pending)

Both exact-head ec1 Native CodeReview excerpts remain in
`gpu_benchmark_terrain_setup_16` from60s through570s before the600s timeout.
Read-only audits of both existing, SHA-verified1040/1807 distribution archives
confirm `t_gravel` is undefined; actual gravel is `t_railroad_rubble`. Use valid
`t_concrete` for the non-earth stone control without introducing rail geometry.
This matches the failing first mixed-material setup; the corrected Native
rerun is still needed to close the causal diagnosis. No local Native run.

Validate all four core material definitions in Python before any reference
build or child, and again in Native with `ter_str_id::is_valid()` before
conversion. Invalid IDs now fail the diagnostic gate rather than reaching
CDDA's blocking invalid-ID path. Bind material names and defining-file hashes
to the immutable fixture receipt. Version the changed fixture explicitly:
profile schema3, pair receipt schema2. Old/different fixtures are rejected.
Default non-terrain profile schema and ordinary game data remain unchanged.

All16 receipt/material guards and96 atomic source-transform tests pass.
Both installed core definitions match the corresponding verified ZIP bytes.
No upstream source/network body was fetched. Same97×97 grid,96 steady frames,
three scenes,14 alternating children and600s child deadline; no weaker gate.
Actual Host compilation and corrected Native captures remain CI work.
Evidence: `night-stage85-{terrain-guard-tests,atomic-transform-final}.txt`,
`night-stage85-pinned-{native-preflight,archive-definitions}.json` and
`night-stage85-ec1-fine-review-{122735136124,122735136065}.json`.

## Stages86–88 — additional cold-frame candidates not retained

All candidates remain private; production texture analysis and lookup are
unchanged. Same-process full CPU timings include fresh analysis and previous
frame cleanup under background applications. They do not qualify Native,
GPU execution, Present or15ms. Raw short/failed and successful records remain.

| Candidate | Correctness and reason not to retain |
| --- | --- |
|86: hoist2×2 mip row addresses |168 exact frames; seven pairs×6 fresh frames across7 source sets.64px/512-source ratio.97930, but small control1.07108 and32px common1.00503. Initial auxiliary link response-file creation fails and supplies no data; corrected environment passes. `night-image-mip-{model86-r2,timing86}/`. |
|87: alpha0/255 shortcuts |168 exact frames plus262144 padded source pixels/all256 channel-alpha combinations,2572 assertions. Per-pixel branch improves opaque64px/512 ratio.94431 but fractional32px regresses1.03644. Once-per-image selection preserves the fractional loop, yet13-workload guards remain mixed: r2 odd33 ratio1.01822; r3 common32/64-source1.05461, fractional64px1.02279. No robust broad basis. `night-image-alpha-{model,timing}87{-r2,-r3}/`. |
|88: callback-local hashed image map |168 exact CPU frames. Seven pairs×6 frames/13 source sets: fractional64px ratio.94440, cutout32px1.03284,128px1.02069, small fractional1.02477. Not retained; no direct rehash/GPU proof or hardware rerun is claimed. `night-image-map-{model,timing}88/`. |

Both currenta9 module jobs pass, including Windows47 CTests and Linux
sanitizer controls. Source/GPU/runtime checks report success in metadata;
two runtime-log reads return providerHTTP502, with no CLI/log fallback.
Current full40674207425 and corrected terrain40674244460 Native jobs remain
running at fixeda9. Earlier ec1 full40664250771 has both actual Linux jobs
and both module jobs passing; Windows full captures remain running. Preserve
both PR heads and meaningful captures.

## Stages89–90 — finalization and bounded CPU raster dispatch

The user explicitly requested main integration, installers and continued work.
ReleasePR223 stays at `a9f3ef7`, exact runtime `0167645`/tree
`48bf32bb587aa32b91ecf54ed64781992766a7ca`. Both current22MB packages have
38 exact payload hashes,106 copied Native inputs,171 frozen Git sources,
104 original resource files and matching Setup/Host/DLL/privateSDL/CC0 records.
Both actual Linux Hosts pass; both full Windows jobs remain active at this
checkpoint. Do not substitute older qualified48 for the pending A9 gate.

Both fresh A9 early callback reports independently verify all five program
JSONs,106 copied inputs,26 controlled turns/25 callbacks and clean exits.
EmptyCPU frames are70.45235ms(1040) and50.48325ms(1807), with geometry
29.68060/20.83685 and raster31.07560/22.40250ms medians. These are separate
WARP CI runners, with the known672x704/640x480 diagnostic viewport alternation,
not an equal-scene revision comparison, ordinaryRTX or15ms qualification.
Verified-summary SHAs are `fc1eda1a43d1fab7ba67824a11e3103ed0449be507454dfce5743771881e6167`
and `e67cd8b5a0662ad3c6f99df7a58b75b83fc6ae042af26b45d749086fbe39e6b8`.

The corrected1040 optional terrain ablation reaches all14 child reports and
restores the module/fixture. Its final equality guard rejects the last CPU
road submission counter: pair5 reference8120/current4144; pair7 reverses that.
All14 individual program-profile contracts pass locally; the receipt declares
one identical hash per capture across every child. The250ms module cache
expiry is a plausible cause of last-frame work variation, not yet proven by
a matched-cache diagnostic. Keep the guard; no paired Native speedup is claimed.
This failure is separate from ordinary full installer qualification.

Stage90 uses the existing disjoint16-row raster bands for substantial CPU
world frames between250000 and500000 pixels, with at least1024 queued
triangles. It permits up to two workers, joined before return. Existing large
frames/dense grass retain up to four workers; small queues and medium held
overlays retain their prior dispatch. Geometry, submitted triangles, provider
calls, contents/range/quality and the GPU producer are unchanged. The private
per-strip work counter may rise at most2x; public submitted geometry does not.

Frozen original versus candidate:256 frames/4696 assertions/45,212,800 exact
RGBA pixels and18,679,296 volume samples; raw float depth/shadow bits, identity
buffers, targeting, world/creature/effect counters and submitted geometry agree.
72 medium material cases execute the added path;66 have transparent geometry.
Opaque/zero/partial alpha, odd/padded/mip sources, windows, water and smoke
are covered. All3,319,896 provider calls remain on the callback thread.
The first auxiliary build used a mistyped test liquid enum and supplied no
measurements; its diagnostics are preserved. The corrected model passes.

Seven predetermined alternating pairs with complete fresh frame timing,
including cleanup and per-frame worker creation/join:

| CPU model | Before → after, ms | Median paired ratio |
| --- | ---: | ---: |
| City572x600 |63.64618 →58.49975|.91062|
| Constructed21 slices572x600 |27.93800 →23.69258|.84804|
| Small legacy control |2.47471 →2.48427|.97208|
| Unchanged medium legacy |9.61010 →9.38896|.98198|
| Unchanged small city |50.82334 →50.15860|1.01598|
| Existing four-worker city |61.35698 →60.95737|.97704|

Independent medians and paired ratios need not move together. The small-city
guard's1.6% paired regression is retained explicitly; no universal speedup.
For constructed21, first-of-lane median27.4018→23.3734ms and frame p99
35.40847→28.47906ms; city first61.4795→56.9425 and p9978.82929→69.93155.
These are model frame lanes, not cold processes, Native, GPU or Present.

The complete local Release build,51 non-device CTests(26.79s) and the rebuilt
inventoried hidden RTX4090 scene suite pass. It retains45 grass draws, zero
CPU world raster/warm static uploads,21 fallbackADS/12 authoredADS/34 objects,
multiZ and subway lighting. No Native game, mouse, focus or system-setting
change was used. Current Native CI applies to releaseA9; stage90 still needs
its own remote qualification. Public numeric evidence is
`night-raster-dispatch-20261028.json`; raw private models/tails remain preserved.

## Stage91 — compare identical CPU world work

The A9 comparison failure is reproduced in the exact shared module: after a
275ms cache expiry, unchanged geometry/time yields identical complete pixels
but submitted work rises1176→2686 triangles and one→two raster calls in all
five controls. This is a causal module model, not an independent diagnosis of
every Native frame or a performance gain. Raw: `night-terrain-cache-counter-model91/`.

Both excluded comparison DLLs now share an opt-in forced CPU world rebuild.
The normal DLL has no runtime diagnostic branch; the setting is compiled into
the two private targets only. The current/reference DLLs still differ only
in the pinned pre71 terrain include. Normal source hashing remains in the
diagnostic path. The original shipping DLL is saved/restored separately and
its bundle hash is enforced; all three module identities must be distinct.
The runner still refuses local/non-CI execution before accessing arguments.
No additional checkout or game copy is created.

Terrain profile4/receipt3 record and compare all96 per-frame submission,
draw-call and CPU-raster counters, retaining scalar, source, asset, process,
layout and exact-capture guards. Missing/false cold policy, incomplete/noninteger
work arrays, normal-cache rows, wrong final counts and an earlier workload
change with identical last counts/captures are rejected. No equality guard
was waived and old profile3 receipts cannot qualify this protocol.

Six compiled shared-module configurations x5 trials pass: old/normal modules,
ordinary module with diagnostic environment present, both cold targets, and
the private current target without its environment. Ordinary caching remains
active with or without that setting; both cold targets execute equal work.
Complete RGBA bytes agree, SHA256
`7314d3961fedddc4cfd998e3b6d78ca5a3b01d66fec52031e1130b1ea9ad0f14`.
This is neither a Native launch nor a new timing result. Raw evidence and
compiled/source identities: `night-terrain-cold-policy-model91/summary.json`.

Full local Release/51 non-device CTests(22.05s),18 terrain guards(4.967s) and
96 atomic two-pin preparation tests(51.087s) pass. An initial wrong test
selector ran zero cases and failed; it is preserved and not counted. The
reference CMake setting is cleared after both private targets build. Header
geometry is unchanged from90, so no repeated device run is needed. No local
Native/SDL/input/window, focus/mouse, profiler or global system change is used.
The changed Native benchmark include has source-transform coverage, not a
new local linked Native/game test. A remote rerun is still required.

A9 full Windows jobs are now in paired main0.13 comparisons. PR225's source/
GPU checks and module suites pass while its own Native capture continues.
Keep both remote heads fixed; stage91 is not pushed over either active run.
Release draft426662059 contains the exact verified A9 installer bytes but
remains unpublished until A9's complete required gate and main integration.

## Stage92 — row setup/iteration candidate rejected

Early skip of unowned row bands and division-free row stepping preserve all
256frames/45,212,800pixels, raw depth/IDs/metadata/submission/strip counters,
3,319,896 callback-thread provider calls and187690 ordered-row controls.
Seven predetermined pairs across ten complete-frame lanes give mixed results:
city572 ratio1.00938, city6401.00849 and small multilevel1.00435. The row
candidate is not retained. All raw lanes and `night-stage92-rejection.json`
remain preserved; no source change or ordinary-game gain comes from it.

## Stage93 — bounded reserve for CPU world geometry

Growing the fresh CPU opaque queue repeatedly copied existing200-byte
triangles. Count only selected cells with a floor, furniture, vehicle or
non-flat terrain. At1024 qualifying cells, reserve a four-triangle hint,
bounded to131072 initial entries(25MiB). Small/sparse/empty-air scenes and
GPU collection acquire no CPU triangle reserve. Geometry order, quantities,
providers, cache lifetime, worker counts, pixels and picking are unchanged.

The first variant measured faster heavy frames but reserved13,107,200 unused
bytes in visible air and grew the dense queue to147456 entries, versus138255
before. It is not retained. The surface-counted variant leaves air capacity0;
dense/city capacity falls138255→113284, saving4,994,200bytes in these models.
All selected-cell/triangle counts agree; sparse capacity1066 remains unchanged.

Full exact models pass256frames/4696 assertions/45,212,800RGBApixels and raw
depth/shadow/ID/metadata/workload equality, including66 transparent frames
and all3,319,896 provider calls on the callback thread.

Seven predetermined alternating pairs of complete fresh CPU frames:

| Model | Before → after ms | Median paired ratio |
| --- | ---: | ---: |
| Dense572 |59.10275 →53.03399|.89888|
| City572 |63.64641 →55.56675|.87767|
| Constructed21 layers |24.54204 →22.48326|.90711|
| City640 |57.19450 →50.33567|.88174|
| Small city |49.09815 →42.61098|.88275|
| Large city |59.90895 →52.92776|.88977|
| Fog city |58.96208 →53.27316|.89273|
| Actual animated-grass model guard |66.54418 →58.50555|.88522|

Focused hidden/air/sparse and longer unchanged small/medium/multilevel guards
retain all raw samples: paired ratios.97066/1.00221/1.02847 and
.99820/1.02508/1.00026 respectively. The sparse and unchanged-medium guards
regress about2.8%/2.5%; no universal speedup is claimed. City572 first-of-lane
65.1412→58.8276ms and p9974.10874→67.81633; constructed first25.9192→22.4233
and p9929.38634→26.74666. Dense572 and small-city p99 tails regress; those
records remain explicit. These are fresh CPU model lanes with cleanup,
allocation, thread creation/join included, not cold Native processes, GPU
execution, ordinaryRTX FPS, 15ms or action-to-first-correct-present proof.

Full local Release and51 inventoried non-device CTests pass(23.57s). The
shipping header is exactly model-equivalent except a comment/local name.
The reference diagnostic CMake setting stays empty; there is no new Native,
SDL/device window, mouse/focus, system-setting change, subagent or game copy.
Public numeric evidence: `night-world-queue-reserve-20261028.json`.

Stage91 now has both module suites and all3 GPU/device/sanitizer jobs passing;
full Native and strict terrain-pair captures continue at its fixed source.
Stage90's new1807 early program evidence independently verifies all five JSONs,
106 copied inputs,26 controlled turns and a clean exit at runtime2338d9b/tree
e0e8052bbd14e07209d71080f731d9320804019d. Summary SHA
`0c2a90c928a9da142b44d6a023d01f3aa721912b2eb64d9f880e56aa1875946e`.
It is separate WARP evidence, not a paired revision speedup or full-tier result.

## Stage94 — reuse owned GPU chunk lookups inside one callback

The GPU collector looked up every visible static cell during both the dirty
scan and final packet construction. Reuse callback-local pointers to owned
map entries in scenes with at least1024 source cells; inserts may rehash the
map but preserve element references. A fully hidden world and smaller scenes
allocate no pointer array. No Host address or pointer is retained after the
call. Geometry, order, source signatures, resources, visibility, camera, LOD,
ABI and persistent cache accounting are unchanged. Extra x64 scratch is at
most512KiB of pointer elements plus allocator overhead, bounded by the existing
65536-cell validation; this is not a whole-game RAM reduction.

The first measured variant is rejected: hidden/small paired ratios1.03822 and
1.06934 exceed its3% guard. The refined variant passes270 complete packet
comparisons/201648 assertions,24,584,487 vertex/index/material values and
70,779,510 texture pixels. Draws, transforms, bones, cell light/gas data, source
signatures, template counts and owned-cache bytes agree, including later
callbacks with replaced input allocations and packets retained by a caller.

Predetermined9 alternating pairs of the full CPU GPU-scene producer plus
packet.finish(), with longer small-scene lanes and no concurrent benchmark:

| Owned input model | Before → after ms | Median paired ratio |
| --- | ---: | ---: |
| Warm dense97 |1.69414 →1.59376|.95001|
| Warm city97 |1.96184 →1.81272|.93183|
| Warm grass97 |1.74627 →1.64395|.94758|
| Warm sparse49x3 |1.30486 →1.24021|.95223|
| Hidden97 |.86624 →.85870|1.00489|
| Visible air97 |1.43644 →1.36486|.94325|
| Shifting city97 |4.51216 →4.36713|.97701|
| Cold-cache city49 |10.54105 →9.99340|.95064|
| Small17 |.06737 →.06709|.99689|
| One visible cell in97 |.85605 →.84916|.99196|
| Small30 |.16112 →.15681|.97325|
| Threshold32 |.17483 →.18182|.99399|

Independent medians and paired ratios are distinct; the last row has about4%
higher independent median despite a slightly better paired ratio. All108
raw lanes and both variants remain preserved. This is about0.10–0.15ms saved
in substantial warm producer models, not5–7% total game FPS. Shift-city paired
gain is2.3%; no universal frame-tail or ordinary Native stutter fix is claimed.

The complete local Release build,51 inventoried non-device CTests(22.51s),
and the inventoried hidden RTX4090 render-only scene(3.50s) pass. It retains
45 grass draws, zero CPU world raster and warm static uploads, multi-Z/stairs,
subway brightness and21 fallbackADS/12 authoredADS/34 object checks. No local
Native game, foreground/input test, mouse/focus or system-setting change,
subagent or new checkout/game copy is used. The171 frozen source bindings
change only this header. Public numeric proof:
`night-gpu-chunk-links-20261028.json`. This source needs independent remote
qualification and is not in the publishedA9 installers. Active PR225/226/227
heads and meaningful captures remain unchanged.

Previous CPU queue93 now has both module suites and all three independent GPU
jobs passing, including the Vulkan sanitizer lane. Full Native checks continue.
Both actual Linux Hosts of diagnostic91 pass. Its optional1040 strict terrain
pair step now completes successfully; allowlisted numerical evidence is being
independently checked without fetching images, logs or entire artifacts.
No Native upstream source body is fetched; the ledger still has161994 bytes.

## Stage95 — recycle owned snapshot entry objects after invalidation

A new camera snapshot cleared the logical entry vector and subsequently
zero-initialized every1120-byte entry before copying fresh payloads into it.
Keep owned POD objects and track the active count separately. New snapshot
keys still clear every slot; each reused entry copies current world/facade
facts and revokes all four base/detail/presentation/source-light readiness
flags. Cold objects remain value-initialized; every published payload is
overwritten before its ready flag is set. No uninitialized object/move trick,
Native pointer, changed permission, lease, key, rejected slot, bound or source
invalidation is introduced. clear() still releases all retained heap storage.
All existing consumers were checked: they read payloads only behind their
corresponding ready flag; world/facade facts are initialized before store().

The differential model passes37,703,209 assertions,2,146,401 store attempts,
5,942,385 entry observations and3,800,460,292 compared ready-payload bytes.
All logical sizes, heap accounting, original counters,32-level/extreme XYZ,
growth/shrink, rejection, suspend/resume, invalid ranges and clear agree.
Heap bounds stay96MiB/65536cells. The private cache object adds one size_t;
entry layout and existing heap capacities remain unchanged.

Nine predetermined alternating pairs mirror the Native cache publication
order with separate acquisition/base/detail/presentation passes. Final raw
initialized payload hashes agree in every measured lane:

| Cache model | Before → after ms | Median paired ratio |
| --- | ---: | ---: |
| New9409 base-only |.35685 →.18521|.52232|
| New9409 all payloads |.53068 →.33205|.63167|
| New28227 all payloads |1.61269 →1.06489|.65993|
| New65536 all payloads |4.78534 →3.51545|.72203|
| Small49 |.00276 →.00172|.63203|
| Small1024 |.05841 →.03743|.66527|
| Unchanged9409 |.20003 →.18639|.94133|
| Cold-cache9409 |3.94276 →3.70757|.93578|
| Alternating9409/1700 |.30462 →.20269|.63693|
| Empty32 loaded levels |.00762 →.00758|.97719|
| Presentation-only9409 |.32366 →.13249|.41398|
| Detail-only9409 |.33102 →.15569|.45940|

These are helper/cache costs, not full Native extraction, game frames, GPU
execution, cold processes or action latency. At9409 all-payload entries the
observed saving is about.20ms; the large65536 model saves about1.27ms. All108
raw lanes remain preserved. No universal ordinary-game or frame-tail gain.

Typed regressions publish old actor/item/field/gas/hostility/source-light
payloads, invalidate the lifetime, and independently republish each new field.
Both frozen original and candidate pass5608 checks. Four deliberately omitted
readiness resets each fail, covering base, detail, presentation and light.
The complete local Release,51 non-device CTests(23.18s) and96 atomic source
transform tests(36.186s) pass. A wrapper footer-position assertion was corrected
from existing evidence only: exit0,96 individual ok lines and Ran96/OK are
present; deferred fixture stdout followed OK. No test was relaxed or rerun.

Only this private Native helper and its regression are operational changes;
both bindings are updated among the171 frozen sources. The GPU source is
unchanged from stage94 and its already-passed hidden render-only suite is not
repeated. Stage94 now has Windows module plus all three GPU jobs passing,
including Vulkan ASan/UBSan, at its fixed PR228 source. Full Native captures
remain fixed across PR225/226/227/228. No local Native game, input test, window,
mouse/focus/system change, subagent or new checkout/game copy is used.
Numeric model proof: `night-snapshot-entry-reuse-20261028.json`.

The previous strict terrain include A/B is now independently checked on both
1040 and1807:28 clean Native children,168 records,96 samples per record, all
per-frame work vectors equal and all declared six image digests identical
across each14 children. Actual images/physical fixture-file hashes remain the
CI strict verifier result, not a claimed local BMP verification. Mixed CPU
model medians105.9854→38.27815ms and111.8972→43.7704ms confirm the earlier
terrain include work; grass controls are essentially unchanged and GPU steady
medians show no material gain. First-mode-ready improves for mixed materials,
but begins after Native setup. This is WARP/forced-rebuild diagnostic evidence
for that earlier include only, not hardware FPS or candidate95 qualification.
Public summary: `native-terrain-ablation-20261028.json`. Bounded selected JSON
reads reserve605766 of8MiB; no logs, images or whole archives are read. A failed
second-pin read resumes only13 missing JSONs; all selected hashes now verify.
No upstream Native source body is fetched;161994 bytes remain in that ledger.

## Stage96 — array append construction rejected

The broad append variant changed base-vector capacity growth and stopped at
the accounting guard despite identical output bytes. The narrower detail-only
variant passes18,385,440 assertions and3,103,192,192 compared bytes, but its
9409-cell cache-miss and verification controls regress6.25% and4.44% by paired
median. Do not retain either initialization shortcut. Raw private evidence and
the initial capacity diagnosis remain intact; no Native source body is fetched.

## Stage97 — keep final base-snapshot growth within its retention allowance

A base snapshot near the65536-cell limit can grow geometrically beyond that
existing retention allowance. Its loan then discards the allocation, making
nearby alternating large counts allocate repeatedly. Preserve value-initializing
resize, but reserve the existing65536-element bound for a final qualifying
growth. The first count guard keeps smaller arrays on their original path.
The sole Native call-site change selects this helper; all source reads,
candidate order, permissions, cache readiness, fields, ABI and reset rules stay.

The pure complete-array comparison passes18,385,600 assertions over160 changing
frames and3,103,192,192 output bytes. Actual allocator/lifetime tests pass5291
checks; deliberately removing the final-growth bound fails after9. Nine
predetermined alternating pairs for each of14 cases retain final byte hashes:

| Complete owned-array construction | Before → after, independent medians(ms) | Paired ratio |
| --- | --- | --- |
|65535/65536 alternating cells|7.63921→4.79740|.63141|
|45000/45001 alternating cells|5.15930→2.53197|.48921|
|Unchanged9409-cell warm control|.36739→.37955|1.01831|
|Unchanged28227-cell warm control|1.32283→1.32738|1.02039|
|Growing9409-cell control|.35145→.35924|1.01448|

All substantial controls meet the predeclared3% paired guard. The warm9409
independent medians regress3.31%; individual pairs vary. The empty control is
20.19ns,4.49% paired slower, below its predeclared50ns absolute ceiling. The
first growth variant was rejected for its23.45%49-cell paired regression;
the refined run uses an early count guard and longer small controls. Raw lanes
and both outcomes remain. A test-only duplicate-probe self-write was repaired;
the original failed test is retained and no production assertion was removed.

Sequence peak output-array payload capacity is72,350,704→55,312,384bytes,
excluding allocator overhead. This is not a universal RAM reduction. Actual
MSVC32769→32770 base capacity rises25,559,560→34,078,720bytes under the same
existing allowance; shrinking to one cell after the largest growth keeps that
reusable capacity. Oversized transient requests retain their previous behavior.
No new persistent pool, GPU memory or Native pointer is introduced.

The full local Release build,51 non-device CTests(24.12s) and96 atomic two-pin
source-transform tests(37.706s) pass. Renderer code is unchanged; no repeat
device run or local Native/input test is needed. Full current-source Native
qualification is pending. Model cache-miss controls use reused array storage;
they are not cold processes, whole extraction/game frames, GPU execution,
ordinary movement/shooting stalls or15ms evidence.

Numeric proof: `night-snapshot-growth-cap-20261028.json`.

## Stage98 — restore visible substrate neighbours and remove a duplicate copy

The Host assembled sparse XYZ coordinates before attaching `world.appearance`.
An indexed lookup created in that interval read a zero-width grid and rejected
the scene. Consequently the vertical-terrain substrate pass could never select
a visible neighbouring floor. Move that existing pass into the already copied
static-detail include and use its canonical sparse coordinate list directly.
Only snapshot-visible cells on the same Z supply a texture; no Native map
query, FOV rule, candidate budget, world ordering or ABI changes.

The helper regression independently builds its expected result from an
absolute-XYZ map:163623 complete-detail/priority checks across384 sparse and
legacy scenes plus8 explicit visibility/order cases. Appearance-unbound and
bound results agree. Reintroducing the indexed lookup, accepting an invisible
neighbour, using another Z or reversing priority each fails. The first private
build omitted six unrelated test fixture translation units and failed at link;
its output is retained. The focused helper runner and full linked target pass.

Reuse the selected owned string for its final copy. Against the **already
corrected pass**,9 alternating pairs x12 workloads show city9409 .41447→.31760ms,
trees9409 .45554→.40649ms, city28227 1.67089→1.33079ms and trees65536
7.06198→5.66968ms; median paired ratios.78841/.85645/.82274/.80285.
Long-ID temporary allocations halve:4518→2259 for9409 city and41932→20966
for65536 trees. Full detail bytes agree for every benchmark output.
All-flat9409 regresses1.42% paired; all prescribed controls pass. These are
transient allocations and a CPU helper model, not RSS/VRAM or game-frame gains.
The prior broken pass skipped valid work and produced different output, so its
lower skipped-work cost cannot be an equivalent optimization baseline.

Release build,51 non-device CTests(23.94s),96 atomic two-pin source-transform
tests and the final four negative controls pass. Renderer/assets/device code
is unchanged. No local Native/input/device test, mouse capture, background-game
change, subagent or new game copy. Full exact-source Native CI is pending;
published A9/CDB installers remain unchanged.

Numeric proof: `night-visible-substrate-20261028.json`.

## Stage99 — resume same-direction sparse neighbour searches

Stage98 restored a real substrate pass. Its existing binary search still starts
from the full sparse list for every neighbour. Canonical Z/Y/X producer order
is unchanged, including the explicit re-sort after distance capping. A fixed
cardinal offset preserves that order, so four independent insertion cursors
can resume forward. Initial queries use full lower_bound; exponential
bracketing handles large gaps without a linear scan of unrelated cells.
Legacy/small frames retain their original lookup. A pure prefix scan skips
cells needing no work; search code remains outside the common traversal.

The source-bound complete-pass model uses9alternating pairs x18workloads,
with fourfold iterations prescribed before the final run. City9409
.32990→.22317ms, trees9409 .42329→.27702ms, city28227 1.37722→.74855ms and
trees65536 6.66355→3.14606ms; paired ratios.66658/.65683/.55229/.47213.
All complete detail bytes, temporary allocations and allocated-byte counts
agree. The new object has sizeof24bytes onx64(16integer bytes); it is callback
local and introduces no persistent heap cache, Native pointer ownership or
extra Native read. sizeof is not measured stack reservation/RSS/VRAM.

All substantial controls meet the original3%paired guard. Empty37.35%,
small494.78% and single-cell7.79%paired regressions stay under the prescribed
50ns absolute allowance; raw paired deltas are retained. Small49 independent
medians improve while its paired ratio regresses. Long-gap independent
medians regress .074us while paired timing improves3.54%. No tail guarantee.
Three prior variants are rejected: first legacy+8.74%; separate fallback
all-flat/legacy/lone-first+14.21/21.58/18.75%; isolated search hidden+19.46%
and long-gap+4.20%. A private duplicate include marker stopped an intermediate
preparation before compilation/timing; its source is retained separately.

Real helper/class guards pass1099623checks: prior163623independent surface
checks,935972every-query comparisons against full binary lookup and28owned
range/immutability/size checks. Cover12sparse layouts,32levels,65536cells,
missing neighbours, skipped-query subsequences, end positions and coordinate
overflow. Four deliberately broken variants fail. Release build,51non-device
CTests and96atomic two-pin source-transform tests pass. Device/renderer code
is unchanged; no local Native/input/device test, mouse capture or subagent.

These are CPU helper costs against the corrected6da17a3 source, not full Native
frames, ordinary movement/shooting latency, GPU time, cold-process startup or
15ms evidence. Full exact-source Native CI remains required. Main/CDB and
published installers are unchanged. Numeric proof:
`night-substrate-forward-search-20261028.json`.

## Stages100/102 — rejected allocation experiments

Stage100 tested four borrowed-string variants with identical output and no
temporary allocations. They failed the unchanged3%control guard (+3.58–6.55%)
and none was promoted. Stage102 reused a projected-triangle scratch vector
inside a renderer. All271complete frames matched, but city572/constructed21/
small-multilevel paired costs regressed3.06/3.51/3.82%; only1of17workloads met
the2%gain criterion. Its retained buffer also raised peak requested bytes by
up to2476040. Allocation savings did not override the failed timing guard.
Original per-paint vectors remain in production.

## Stage103 — bypass unnecessary near-plane polygon construction

Most visible triangles lie wholly in front of the near plane. After the
existing three transforms they now enter the original shared projection body
directly. Only intersecting triangles build/interpolate the original clipping
polygon. Vertex ordering, floating-point operation order, the original first
vertex flat light, per-paint vectors and synchronous worker joins are retained.
All source outside these two CPU projection methods is unchanged. GPU collect
does not call this CPU projection path; renderer fields and sizeof14080 agree.

Complete source-bound comparison matches271frames/50360800RGBA pixels,
19711886volume samples and3319896callback provider calls, including9cached
replays and9multiple-paint frames. A separate near-plane guard matches102816
cases/40290projected pieces with3890078field/bit checks: all8visibility masks,
exact.08/nextafter boundaries,7yaw/pitch/multi-Z poses,17materials, both
windings, ordinary/offscreen/degenerate/sliver/giant geometry. Two deliberately
wrong view/interpolation variants fail. No raw comparison of struct padding.

Seven fixed alternating pairs x17complete-frame workloads retain the original
3%regression/2gains-of2%guard. Small multilevel11.8768→11.65584ms has paired
ratio.97505; cached grass11.50328→11.61141ms has paired ratio.95629. The latter
independent median regresses despite its paired gain. Sparse-floor+2.79%,
city-small+2.32%, hidden+2.14%and fog-city+2.08%paired regressions are retained,
as are every raw lane/first frame/p95/p99/max. This is a modest CPU candidate,
not a broad or guaranteed tail improvement. All17allocation counts, requested
bytes, peaks and ending live bytes agree in a separate ordinary-new probe;
this is not RSS/VRAM/allocator-overhead evidence. The timers are uninstrumented
and include full build_frame, prior cleanup, renderer destruction and joins.

Release build,51non-device CTests and96atomic source-transform tests pass.
No local Native/input/device run, mouse capture, background-game interruption
or subagent. Full current-head Native qualification is pending. Ordinary
shooting/movement freeze reduction, RTX FPS and15ms first-correct-present are
unqualified. Numeric output/timings/memory and rejected102evidence:
`night-cpu-projection-fastpath-20261029.json`.

## Stage104 — storey rays and stair-delay attribution

The reported ascent freeze is not yet proven fixed. Existing B9F Native CI
has four combined vertical_move+first-render samples per pin:1040median
142.3187ms and1807median152.5411ms, both software-GPU controlled fixtures.
They do not separate the engine move from Native2D tiles, perspective work
and SDL present, and are not ordinary RTX4090 gameplay measurements.

The new opt-in audit captures each of the four real vertical_move calls and
its first successful explicit render/present separately, with fresh frame
cache/tile/callback/present stages. Setup and later easing settle are excluded.
Existing camera continuity, displayed-eye visibility and Native cost/state
guards remain. The existing26turn schema3 output is retained. The validator
requires ordered adjacent floors, complete callback partitions and fresh
finite contained stages.18Python tests include12negative stair cases. No
extra diagnostic clocks or captures run during ordinary play.

General multi-storey visibility now uses scalar state for the three fixed
axes. All arithmetic/query/corner/slab ordering and horizontal fast path
agree:327680initial rays and1376256expanded rays/42layouts, with13466276
identical ordered geometry reads in the expanded guard. Two deliberately
missing slab/corner guards fail. Seven fixed alternating pairs x10helper
workloads retain the predeclared3%regression/two2%gains guard. Downstairs
paired cost improves9.30%, but upstairs regresses0.86%; XYZ-tie independent
medians also regress. These are ray-helper costs, not stair/whole-frame gains.
Both fewer-query alternatives failed the unchanged timing guard and were
rejected. Every raw lane and control is retained in the numeric report.

Release build,51non-device CTests,96atomic source-transform and18attribution
tests pass. Native-only audit compilation/capture requires current-source CI.
No repeated local device run is justified for this CPU/opt-in audit change.

Parent233compiled but its1040full job stopped at410expired original0.13
baseline Actions artifact. With explicit user authorization, the two original
35payload0.13ZIPs were [archived](https://github.com/Neversalimus/NCMM/releases/tag/ncmm-fpv-ci-baseline-v0.13-9e3d6230f7ad).
Their exact old installer SHA256s/source/engine pins are unchanged. The new
workflow downloads those immutable bytes, validates the original hashes and
all payloads, and retains old outer Actions hashes as historical metadata.
No release binary was rebuilt and this archive is not a new game version.
Existing full qualification gates are unchanged. Numeric/source evidence:
`night-stair-ray-attribution-20261029.json`.

## Stage105 — integer geometry-slot experiment held

The bounded dense slot fits32*97*97=301088, so a private variant kept the
intermediates as int before converting the final index to size_t. All8550973
checks,327680rays/8480344queries, query order, cache counters/bytes and104byte
sizeof agree. Seven fixed alternating pairs x12cache/ray cases pass the
original3%control/two2%gains helper guard. Upstairs/downstairs paired costs
improve2.53/8.01%, but cold grid regresses2.60%paired and5.9681→6.6265ms
(11.03%)independent medians. XYZ-tie paired and up-glass independent medians
also regress. For the first-updated-frame objective this candidate is held,
not promoted. This is a conservative decision after a passing helper gate,
not a claim that its declared gate failed. No rerun-until-pass, module build,
CTest or new Native CI for unchanged production. Raw168lanes are retained.

## Stage106 — real stairs in both CPU and GPU modes

Source inspection confirms that the earlier four stairs, including104split
captures, run after warm(cpu). Newly verified F102 primary reports contain
CPU-only combined stair medians142.24455ms(1040)and90.5162ms(1807), four
samples each. Later combat callbacks use GPU, but those are not stair frames.
The two runs are software-CI context, not a causal before/after or RTX result.

The opt-in audit now resets the same fixture and warms each backend outside
measurement. Four actual Native moves plus first frames run on CPU, then four
on GPU. Schema2requires both groups in order, correct adjacent floors, true
requested first-frame backend (GPU fallback cannot qualify), complete wall
partitions and five fresh contained host stages. Native movement cost/state,
camera continuity and displayed-eye visibility guards remain. The existing
26turn schema3is unchanged. After the probe CPU is restored for later fixtures.

19attribution tests with18negative stair cases and96atomic transform tests
pass. The ordinary module/renderer/ray/cache/asset source and ABI are unchanged;
the existing104Release build/51non-device CTests are not repeated. Native-only
audit compilation and actual8transition capture require full current-source
CI on a separate head;230-234heads remain fixed. No local game/input/device,
mouse/focus or subagent. Stair-freeze elimination and ordinary15ms are still
unqualified. Numeric/source evidence including held105:
`night-stair-cpu-gpu-attribution-20261029.json`.

## Stage107 — opening witnesses held

Three bounded owned sealed-plane variants retained exact1,317,326checks and
75,272post-proof ray cases each. Central stairs halve source callbacks; shifted
stairs also avoid the complete prefix. However each of the three fixed7pair
series violates its original3%control guard. The final existing-vector witness
removes the extra64byte object layout, but corner/unknown/water/warm controls
still regress. All are held; production cache source is byte-unchanged. No
unchanged reruns, relaxed thresholds or ordinary ascent-gain claim. Full raw
lanes and the repaired pre-compilation Windows launch failures are preserved
in `night-sealed-opening-held-20261029.json`.

## Stage108 — exact nearest selection above the snapshot cap

The over65536candidate branch fully sorted every admitted candidate by distance
before trimming, then canonically sorted the retained set. Replace the first
sort with nth_element ordered by finite distance then canonical Z/Y/X. Because
the input traversal is already unique canonical XYZ, the same stable-tie set
and final order remain. Under/equal-cap behavior, all source reads/permissions,
radius, payloads and ABI stay unchanged; no new cache or persistent state.

The private exact original-fragment model preserves7,375,656selected payloads;
the repository helper matrix preserves5,531,742more, including signed zero,
cap0/49/65536/above input, boundaries and32*97*97cells. Reversed tie selection
and missing canonical output are both rejected. The first all-equal negative
fixture was vacuous on MSVC and was repaired;50initial passing tests remain,
only the changed failed test was rebuilt/rerun. Release+51non-device+96atomic
tests pass. The ordinary ray/visibility/device/SDL/asset code is unchanged.

Seven fixed alternating pairs x8cases x3selections pass all original guards;
paired ratios span.1318–.5139. Per-selection helper medians, derived from the
three-selection raw lanes: barely-over10.0517→4.0118ms, radial-city8.6695→
4.0315ms, ordered maximum12.7498→1.7186ms, seven bands18.0786→8.5863ms.
The80byte model removes one temporary allocation and up to11.49MiB per
selection. Input cloning is outside timing; input candidates still exist.
These are isolated Host selection costs, not whole-frame/RTX gains or proof
that ordinary staircase freezes are fixed. Report/source/raw proofs:
`night-world-budget-selection-20261029.json`. Full exact-source CI and Native
stair samples remain necessary; keep234/235capture heads untouched.

## Stage109 — verified first-stair action/frame attribution

Read only5exact primary numerical JSONs from2341040and2351040, after source/run
artifact inventory and under separate shared8MiB labels. Reconstruct106copied
inputs, privateSDL identity, source/runtime equal Git trees, clean process exit,
26turn partitions and each source's exact stair verifier. FourCPU234records and
eightCPU/GPU235records pass. Reserved bytes106595and108420; no log members or
whole report archives. Existing exact capture heads remain unchanged.

On2351040Microsoft software GPU, the four first GPU stair frames take204.55-
354.47ms. Their module callback takes84.48-168.23ms; one Present219.51ms and
one post-visibility90.25ms. CPU first frames59.20-117.27ms. These are controlled
CI samples, not a causal comparison or RTX FPS. In particular the fixture's
render refresh() invalidates Native caches deliberately; these costs do not
establish ordinary-play cache misses. Split already available backend phases
before selecting another ascent fix. See `night-stair-primary-phases-20261029.json`.

## Stage110 — equal-Z volume neighbour batching held

Both private variants preserve73,361checks,204fullCPUframes/17,020,800pixels,
8,012,878integration samples and all effect/depth/identity/geometry/targeting
outputs. Binary/owned/retained lookup also agrees with an independent map near
signed coordinate limits. A new21level fixture initially had zero slice ID at
z=-2; its fixture-only identity was repaired before any timing, preserving the
initial evidence. Production candidate code did not change for that repair.

R1improves3/21level volume effects but regresses legacy controls. FinalR2keeps
the legacy scalar lookup and improves21level smoke/fire effects; however the
portrait-fire total paired1.03868violates the original1.03control. Both7pair x
12case x32frames/lane series are held. No unchanged repeat, third variant or
threshold relaxation. Production volume/coordinate source remains unchanged.
See `night-volume-quad-held-20261029.json` for all raw/source hash references.

## Stage111 — fresh backend stages for each first stair frame

The opt-in fixture already appends CPU geometry/raster/effects and GPU geometry,
resource/update/upload/command/submit and recording subcomponent diagnostics.
Copy these existing fresh values into each stair row:11stages forCPU,20forGPU.
Every selected vector must grow exactly1inside that first-frame render.
Schema3requires the exact backend set, finite contained values and honest
overlap scope. Recording subcomponents can overlap their containers; they
are not additive or GPU execution timestamps. No new stage timer, renderer,
ordinary action, geometry/cache policy, asset or ABI change.

21attribution tests, including190new malformed component/scope checks and an
explicit overlapping-positive fixture, plus96atomic source transform tests
pass. Existing108Release+51non-device tests are unchanged and not repeated.
Actual Native compilation and eight real transition captures need full exact
source CI. No claim of fixed stair freeze or ordinary15ms. Evidence:
`night-stair-backend-stages-20261029.json`.

## Earlier qualification receipts retained from stage99

- CDB `cdb929b`, [full40692590155](https://github.com/Neversalimus/NCMM/actions/runs/40692590155):
  all8 jobs pass; required122987662168 confirms required/selected `full`.
  Both final Windows CodeReview excerpts are retained, explicitly truncated.
  Source/package and all3 GPU jobs pass. PR225 merged without bypass; exact
  checked source is in main and the new TEST release. The independent Runtime
  workflow did not run for this CPU-only source difference; its runtime source
  remains unchanged from qualified A9. No current-head Runtime pass is claimed.

- Full `eae8d02`, run40580475214: all8 jobs pass; exact earlier receipts retained.
- Full `12a7abcd`, [run40600292716](https://github.com/Neversalimus/NCMM/actions/runs/40600292716):
  all8 jobs pass, including required122619592124.
- Previously qualified PR source `cdf6f79`, [full40615847078](https://github.com/Neversalimus/NCMM/actions/runs/40615847078):
  all8 jobs now pass, including Windows1040/1807, both actual Linux jobs and
  required122663447792 (`REQUIRED_TIER=full`, `SELECTED_TIER=full`). Windows
  CodeReview excerpts show both21-scene/224-load paired gates passing.
  Independent GPU40615847005/source40615847050/runtime40615846995 also pass.
  This qualifies CDF, not later volume/terrain sources or ordinary15ms frames.
- `616d718`, integration40629160097, and volume65 `fdbba45`,
  integration40633375838: all7 planning/module/actual Native jobs pass; their
  required gates fail because the selected integration tier is insufficient
  for full qualification. The schema3 baseline CPU profiles are independently
  verified above. Cross-run timings are not a paired causal comparison.
- Terrain71 `1407dd4`, integration40637761432: all7 planning/module/actual
  Native jobs pass; required122673470673 fails at the integration tier.
- `48d8c23`, [full40643681993](https://github.com/Neversalimus/NCMM/actions/runs/40643681993):
  all8 jobs pass, including both Windows Native jobs and required122760580661.
  Final1040/1807 CodeReview excerpts each show21-scene/224-load paired gates
  passing; excerpts are120000 characters and explicitly truncated. Final logs
  and source identities are retained before advancing PR223. Independent GPU
  40643681893/runtime40643681914/source40643681897 also pass. This qualifies
  functional full48, not later CPU changes, causal A/B gains or ordinary15ms.
- Failed optional pair40643770328: both Windows first reference children hit
  600s; both Linux/module jobs pass. No Native A/B measurements qualify.
- Repair82b integration40653873199 completed: both Linux/module-Linux jobs pass;
  Windows module mock-clock tests and both optional Native pairs fail. Exact
  CodeReview logs are retained through draft224; stage82 describes the confirmed
  setup phase and local77 test fix. New diagnostics/CPU changes use that draft;
  protected PR223 full48 remains unchanged until its final logs are retained.


## Remote qualification and next steps

Preserve232-236exact capture heads. Latest main/installers are qualified6da.
2351040eight stairs are strict;1807still captures.236both module suites,
3GPU jobs and source audit pass; four actual Native jobs run. Raw236plan logs
returned CodeReview502four times; annotations contain only provider notices,
so no numerical plan is claimed. No CLI/log-artifact fallback.

111diagnostics use a new head and full exact-source CI. Read fresh backend
stages to select an actual ascent bottleneck. Primary numerical JSON remains
inventory-first, exact allowlist/shared8MiB and sequential per label. Native
body budget remains161994bytes, with0new source-body transfers.105/107/110are
held; no unchanged timing repeats. Continue solo overnight until user stops;
disable same-chat heartbeat ncmm-first-person-view on an explicit stop.
