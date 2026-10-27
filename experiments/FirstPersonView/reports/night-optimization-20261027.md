# Autonomous optimization, 27 October 2026

Baseline: `128453707e6c4e2712237d046dfa07315eac3ca5`, clean
`codex/fpv-0131-fps-controls-20261024`, draft PR #223. Engine pins remain
1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83` and
1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.

Local Native game/input harnesses, mouse capture/warp/focus changes and elevated
profilers remain prohibited. Background games remain running. Available checks
are Release builds, pure/model/shared-module tests and the inventoried hidden
render-only GPU suite. Their timings are not ordinary game frames, GPU duration
or action-to-first-present latency. The 15 ms target remains unqualified.
Previous Native run 40407823699 was still running its long gameplay/Linux gates
at the start; its completed 180-case-per-pin FPS tests do not qualify new bytes.
Do not merge or release before the user's separate finalization request.

## Completed checkpoints

1. **Unique texture ownership.** `mesh_builder::texture()` retained one
   `shared_ptr` per textured triangle; `packet::mesh()` revisited every owner
   even though the GPU texture upload was already cached. The new pure
   regression failed on production baseline: 4096 owners for one texture.
   Meshes now retain each immutable texture once, also when registered through
   a material's color/normal/depth leases. No resource keys or geometry changed.

   Same 4096-triangle fixture: vertex/index/material bytes, mesh digest
   `5bb968231abc8f92` and packet submission match. Owner references 4096 -> 1;
   pointer-vector capacity 86320 -> 16 bytes; growth events 22 -> 1; visits over
   512 packet builds 2097152 -> 512. A paired isolated run measured 1.947 ->
   0.052 ms **total for 512 packet builds**, not per game frame. The initial
   failing run is retained separately; timings under background load are not
   a hardware qualification. The owner/lifetime regression (11 checks) and
   complete GPU geometry/cache pure suite (324193 checks) pass.

2. **GPU traversal avoids CPU depth sorting.** Dirty GPU cells were sorted by
   distance before cell-local collection, although the collector independently
   emits the final cell/pass order. CPU front-to-back sorting is retained.
   The pure producer benchmark now separates cold preparation, camera changes,
   first updates at a tile boundary and local edits, with raw samples, p95/p99
   and exact resource/command digests. All four digests, regenerated-cell counts
   and atlas read counts match the original sorted path. The CPU pixel/depth/
   picking equivalence suite passes 3416 checks.

   Same 97x97 mixed floor/wall/table fixture, 1280x800, MSVC Release, 16 samples
   per state: cold median 20.2533 -> 18.6074 ms, boundary median 4.76405 ->
   4.31435 ms. These are producer CPU times with **no device or presentation**.
   Boundary p95 5.09173 -> 5.2219 ms and local-edit median 2.075 -> 2.42435 ms
   did not improve. Background workload was not controlled; no overall game
   frame-time or latency claim follows. Full conditions and raw samples are
   in `night-gpu-preparation-20261027.json`.

3. **Owned sparse-world index.** Each GPU callback allocated and filled the
   entire loaded-Z dense lookup, even when its ordered XYZ set was unchanged.
   The baseline regression measured one 790395-byte allocation both cold and
   warm for a 97x97 / 21-level window. The retained path makes zero warm dense
   allocations. Persistent data contains only integer slots and owned XYZ
   triples; the adapter's Host scene pointer remains callback-local.

   The key includes epoch, grid/origin XY, loaded Z bounds, count and the exact
   ordered coordinates. New source allocations, flags, light and camera changes
   reuse the same mapping. Holes, reordered indices, window/epoch changes and
   malformed input are checked against the original binary-search oracle.
   Retained capacity is counted and released by `clear()`. 146015 checks pass.

   Isolated index construction plus one lookup, 512 calls (no scene render):
   20 sparse cells, 57.5496 -> 0.0456 ms total; 9409 visible floor cells,
   59.769 -> 20.0672 ms total. Both remove 512 dense allocations / 404682240
   allocated bytes and preserve their lookup digest. These are component
   timings under background load. A changed coordinate set still rebuilds the
   mapping; this is not a claim about a whole camera turn or a Native step.

   The expanded test also reproduced an existing local slab-Z roundoff with
   the old index: changing physical origin Z produces a 7.45058e-8 difference
   from a cold rebuild. Exact topology, materials, current transforms and
   integer lookup are preserved. The origin-Z comparison allows only this
   bounded local vertex-Z roundoff; every mapping transition retains exact
   packet-digest equality. No expensive global mesh reset was added merely to
   force bit-identical arithmetic, and no visual repair is claimed for it.

4. **Continuous chain-link phase.** Each reflected half-panel previously
   restarted its own diagonal wire pattern. Actual emitted triangles reproduced
   wires changing direction at a straight seam; matching boundary extents did
   not detect this defect. Cardinal panels now use one absolute lattice phase.
   Integer remainder arithmetic preserves the phase at negative/large world
   coordinates and across snapshot-origin shifts. Hinged open gates retain
   their local frame. Wire pitch/thickness, height, posts and rails are unchanged.

   The original collinearity regression failed before the fix. Afterward the
   multilevel render suite passes 112369 checks, including straight half/cell
   seams in all four directions, corners, gates and origin shifts. A test-only
   endpoint cutoff was moved between physical wire heights to avoid a float
   boundary at exactly 0.20 m; the old wrong slopes still fail the criterion.
   Five optional actual CPU-renderer captures (front/oblique/vertical straight,
   corner, open gate) were generated and visually inspected. They are isolated
   render fixtures, not a Native game session. GPU qualification remains pending.

5. **Retain overlapping slab facts.** A tile shift discarded the entire closed
   plane proof/prefix and sampled all slab flags again. The new tri-state ring
   retains only owned absolute-cell UNKNOWN/FLOOR/GLASS facts across an unchanged
   source revision and overlapping XY window. Whole-plane state and rectangle
   prefixes still reset on every shift. Epoch/revision/Z/layout changes and
   non-overlapping moves invalidate facts; no Native notification was weakened.

   The same immutable 97x97 fixture failed before the fix and now reports:
   cold 9409 -> 9409 flag callbacks, axial step 9409 -> 97, diagonal step
   9409 -> 193; a negative whole-plane proof followed by a rectangle proof
   14114 -> 9409. These are accesses to geometry-cache flags, **not Native
   map reads or a 97x frame-time improvement**. No millisecond benefit has been
   established. All 850221 source-cache checks pass, including 320 shifts,
   entering/exiting holes/glass/water, exact rectangle oracles, revisions,
   loaded bounds, extreme coordinates and bounded/released memory. Two old
   tests changed their reader's geometry without changing its revision during
   a shift; those mutations now explicitly advance the revision.

6. **Repair the allocation measurement harness.** Linux module CI passed its
   ordinary 45 tests, then ASan found an allocation-family mismatch in the new
   test hook: nothrow array-new still used the runtime, while the replacement
   array-delete used `free`. Matching nothrow allocation/deallocation forms now
   use the same unaligned allocation family. Aligned forms stay with the runtime.
   Smoke checks cover scalar/array, ordinary/nothrow, constructor exceptions,
   partial-array cleanup, deterministic allocation failure and aligned pairs.
   The affected Release suite passes 146029 checks. An extracted standalone
   MSVC ASan smoke also passes with mismatch detection enabled; the old hook
   did not reproduce the Linux failure on MSVC. Fresh Linux CI is still required.
   Production allocation and all sanitizer settings remain unchanged.

7. **Reuse empty opaque floor recipes before expansion.** GPU content dedup
   already produced one shared mesh, but first expanded and hashed the same
   floor separately for every cell. A bounded 512-key weak template cache now
   shares the fixed TOP quad by the complete resolved GPU material. Individual
   transforms, cells, light, world-space finishing and grass remain unchanged.
   The shortcut rejects existing procedural geometry, objects, liquids, rails,
   glass, ceilings and transitions. Multi-Z also requires TOP-only permission
   and four actual floor neighbours proving that no slab rim is needed.

   All 18 cold packet identities and 20 negative fixtures match the original
   generator exactly: geometry, materials, resources, commands and atlas reads.
   Paired opt-out checks, epochs, lights, negative/distant coordinates, failure
   recovery, budgets and the 512-key limit pass. Full geometry has 324390 checks;
   the complete local build, 50 autonomous CTests and hidden render-only RTX
   4090 suite pass. Grass keeps 45 draws and zero warm static resource uploads.

   Same 97x97 floor, nine fresh producer caches, MSVC Release:

   | CPU producer fixture | Median before -> after | p95 before -> after | Ordinary allocations |
   | --- | --- | --- | --- |
   | Legacy flat | 18.1089 -> 6.8751 ms | 20.3940 -> 7.6142 ms | 122877 -> 9979 |
   | Native-like TOP / 21 loaded Z | 21.1995 -> 9.5144 ms | 24.6963 -> 11.6851 ms | 125093 -> 16803 |

   Allocated bytes fall from 23690857 to 9352428 and from 25404626 to 11657557,
   respectively. A second pair in the same compiled producer, with only the
   floor shortcut ablated, checks a mixed floor/wall/table scene over 16 updates:
   cold median 17.6249 -> 5.5900 ms; first tile-boundary update 3.7677 ->
   3.3831 ms, boundary median 3.8603 -> 2.8540 ms and p95 5.0214 -> 3.6516 ms.
   All four action digests, generated-cell arrays and atlas reads match.
   Local-mutation median/p95 and camera-turn p99 did not improve. Background
   workload was uncontrolled. These are CPU preparation timings without Native
   gameplay/extraction, device execution or presentation, not proof of 15 ms
   game frames or resolved player-visible freezes. Raw pairs and conditions are
   in `night-floor-preparation-20261027.json`.

8. **Retain the Host details array between callbacks.** The current
   `graphics_draw_hybrid` still constructed a fresh `vector<cell_v2>`; the
   saved Native city stack independently records this same allocation path.
   Details now borrow a separate existing-style `vector_storage` loan, bounded
   to 65536 retained cells (20.25 MiB logical capacity). The array remains owned
   for the complete callback, is fully value-initialized again, and cannot be
   invalidated by a nested callback. A world reset releases retained capacity
   and retires active loans. Active/retained details now count once in snapshot
   memory and growth/reuse metrics; the existing 80 MiB snapshot gate is intact.

   The pure storage-pattern comparison uses actual allocation hooks and full
   byte equality against fresh vectors, not a Native Host invocation or timing:
   cold/warm allocation counts 1/1 -> 1/0; at 9409 cells, repeated allocated
   bytes 3048555 -> 0, and at 65536 cells 21233703 -> 0 (MSVC includes 39 bytes
   of alignment overhead). All 5201 storage checks pass, including shrink/grow,
   stale/omitted fields and IDs, invisible cells, nested callbacks, exceptions,
   reset and bounds. All 195 Python and 24 atomic bridge tests pass. Full
   initialization work remains; this is not proof that Native pauses are gone.
   No local Native compile tree was available; both pinned Host builds and
   actual callback/snapshot memory qualification remain pending in remote CI.

9. **Separate a stationary-world boundary from origin-only stress.** The old
   `tile_boundary` producer case moved the snapshot origin but kept content at
   fixed array indices, moving physical objects too. Its earlier paired numbers
   are valid for that wider invalidation workload, not an ordinary static-world
   step. The new case reindexes the same absolute-world contents: 148992 exact
   overlapping-cell comparisons over 16 steps, each regenerating 291 cells,
   compared with 1220 in the original stress. Every first changed packet is
   checked against a fresh original generator outside the timed interval.

   The initial exact cold hash failed with floor templates already disabled:
   world-to-local float subtraction changes a few local vertex bits across XY
   shifts. The cold oracle permits only <=1e-6 local XYZ difference, observed
   9.53674e-7, and corresponding derived mesh keys; all other vertex fields,
   topology, materials, transforms, cells/light, poses, camera and textures are
   exact. No production arithmetic or global cache reset was changed. All five
   full packet digests, regenerated counts and atlas reads match exactly between
   the floor-template off/on paths.

   Same compiled producer, 16 consecutive stationary-world updates: first
   2.5233 -> 2.9792 ms (worse), median 3.3933 -> 2.9931 ms, p95 3.9305 ->
   3.8159 ms. Background load remains uncontrolled. This does not establish a
   first-frame speedup or ordinary gameplay latency. Raw scope and samples are
   in `night-static-boundary-preparation-20261027.json`. Only the manual CPU
   benchmark changed in this stage; its Release build and paired modes pass.

10. **Typed standard radio skyline.** The pinned standard `radio_tower` site
    was published as a generic BUILDING and drawn as an 18x18x5 m box. Its
    near lattice/mapgen instead has four vertical corner stacks across six
    2.5 m levels and a narrow seventh level: a 5.81x5.81x17.5 m envelope.
    Optional minor-2 skyline data now replaces that box with eight fixed
    cutout face cards (16 radio triangles), with a real open centre and no
    invented taper. Existing V1 kinds 0-5, patch bytes and revision remain
    unchanged. Old Hosts/modules retain V1 geography; unavailable, malformed
    or truncated new data falls back, while a valid disabled skyline remains
    authoritative. The actual Host dispatcher accepts minor 2 and rejects 3.

    Only an exact standard ID on a fully disclosed, already resident surface
    overmap can select the authored profile. Unseen/vague/outline/detail
    discovery, generic/mod/quest tower types and clipped/near sites do not
    reveal a radio silhouette. This is the nominal standard layout, not an
    inspection of destruction or modded geometry. All four rotations and
    negative origins, callback ownership, disabled/underground reset and
    unchanged legacy data pass the actual Host LOD source with mocked Native
    interfaces. Independent static review found no actionable defect.

    Initial checks caught a 640-pixel mask exceeding the unchanged V1 512
    texture limit and a fixture passing a copied scene instead of the required
    world-owned scene address. Both were fixed without relaxing pixel,
    validation or budget assertions. Final masks are 256x512 and 128x256;
    measured owned masks use 1,474,912 bytes. Masks plus the one-site retained
    metadata use 1,474,992 bytes; the tested Host geography arrays retain
    100,280 bytes and release them on clear. All 961 possible radio sites
    generate 3,944,160 bytes of horizon mesh, within the existing budget.

    Real old binary controls pass: old Host-prefix executable/new DLL 37
    checks; new executable/actual old DLL 55; new/new 86. The saved old DLL
    SHA256 is `f2cc7f84c854f6073edcdaa7eef8638aeb9790d8fc2e624fad70ba5712ba87f2`.
    The pure render suite passes 112716 checks. In the hidden D3D12 fixture,
    actual radio coverage is 268 GPU / 402 CPU pixels, including 176 / 266
    pixels above the former box's height. Ground-only, lattice, old-box,
    restored, turned and foreground-wall readbacks were saved and inspected;
    restored and initial lattice pixels are identical. Camera turns upload
    no static resources and generate no mesh. Near-wall depth/focus wins.
    Subpixel wire minification/aliasing remains visible at this distance;
    this is not a wire-perfect or ordinary Native-game visual qualification.
    Evidence: `night-skyline-*-final.log`, `night-skyline-gpu-scene-r2.log`
    and unchanged-pixel conversions in `night-skyline-captures` under the
    existing private work directory. No frame-time speedup is claimed.

    Primary sources: pinned [1040 mapgen](https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/data/json/mapgen/radio_tower.json),
    [terrain palette](https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/data/json/mapgen_palettes/radio_tower_palette.json)
    and [seven-level special](https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/data/json/overmap/overmap_special/specials.json).
    Local installed data on both pins have identical Git blobs respectively
    `f205898bda8b32dcf3e247b0a67baa13a4ac0101`,
    `0b1dc3f2f134990139cbd1c50ee376ebafea9a75` and
    `5870b1fb6a56b031e3756a0fa6ac2b328ba9e895`. Metre dimensions additionally
    come from the existing near renderer's .81 m cell lattice width and
    2.5 m storey height, rather than being dimensions asserted by JSON.

11. **Reuse a candidate's Native integer distance.** The current range check,
    structural range and lit facade checks independently called `rl_dist`
    for the same observer/target. The candidate now computes that exact
    Native integer once, after the existing loaded/frustum/occlusion/local-air
    gates. Current, structure and facade queries reuse it; the lower XYZ
    retains its own call. There is no cross-cell/frame cache, distance formula
    replacement, floating approximation, changed range or additional memory
    table. Fallback remains conditional, including a supplied zero distance.

    First extracted the unchanged actual Host helper into a copied bridge
    include: its old body matched 1247 characters after newline normalization.
    Compiling this production include with injected map/avatar dependencies
    established a passing before baseline (877135 total checks): floor target
    distance callbacks 2, corner 4, lower 1/1. After reuse they are 1, 1 and
    1/1. The paired helper matrix preserves every boolean decision and ordered
    non-distance read trace across 13440 combinations of light, artificial
    sources, no-sight, range thresholds, separate surface-light Z and observer
    relocation. Known-zero/fallback, loaded/Z rejection before light reads,
    Native sees short-circuit, exact callback arguments and a lower XYZ with
    a different integer/decision are checked. Representative injected square
    and circular integers are dependency controls, not an oracle for Native
    distance formula, Z scaling or frame elapsed time.

    Affected Release build, 904041-check cache/range suite, 195 Python and
    25 atomic bridge tests pass. Evidence: `night-range-baseline*` and
    `night-range-after*` logs in the existing private work directory. No
    millisecond gain or ordinary-game first-frame improvement is claimed;
    actual pinned Host compilation and qualification must follow.
    The same 50 inventoried autonomous CTests also pass after this stage
    (15.48 s). Renderer/module bytes are unchanged by the Host-only range edit;
    the skyline hidden GPU result above remains its applicable device check.

12. **Static-stair causal recorder, pending Native evidence.** Added a separate
    exclusive `-StaticStairProbe` for remote Native qualification. It never
    changes production visibility. Day/dusk/night record normal -> debug night
    vision -> normal at one declared camera, with exactly two preparation
    frames before observation. A terrain stair three cells away, furniture-only
    GOES_UP and hidden chair/item/creature controls distinguish navigation from
    content permission. Full 97x97 loaded-Z physical ledgers, map-memory getters
    and resident-only overmap disclosure detect scene/discovery changes.

    Only repeatable, light-stable normal snapshots can enter a CPU-only 2x2
    BODY/navigation factorial. All four reuse the same owned normal appearance;
    only the target's BODY and measured missing terrain-navigation bits vary.
    Matrix00 must match the untouched normal source and actual pixels. Fixed
    ROI/thresholds require signed interaction >=24 triangles and >=8 pixels at
    >=4/255 RGB; changed debug illumination alone cannot confirm the cause.
    Existing CPU raster expiry is respected without fabricated source epochs,
    and every recorded frame must actually rasterize. The API's normal GPU
    pointer/identity validation is retained. An inconclusive result is valid;
    a successful recorder is not a production fix or timing qualification.

    Local checks: 34 deliberately contrived verifier tests, 229 total Python
    tests, 25 atomic bridge tests, PowerShell parsing and 162 real environment
    isolation/restoration checks pass. Both pins' required Native APIs were
    checked against primary source. No local Native executable was launched;
    actual compilation, ledgers and 9-21 BMP readbacks remain a remote CI gate.
    No renderer/module bytes changed, so the preceding local GPU check is the
    applicable device result. Logs: `night-stair-*` and
    `night-static-stair-verifier.log` in the existing private work directory.

    Native follow-up: actual 21b3878 Windows/Linux compilation caught a protected
    `map::get_field` restoration call, missing scoped flag names and GCC's
    misleading-indentation Werror. Removed the unnecessary field copy/assignment
    under the existing enforced empty-field seed guard; restore uses public
    clear_fields. Qualified GOES_UP and braced the scalar loop. These are
    recorder integration errors, not stair evidence. The real compiler failure
    is preserved in the Code Review tool jobs 121981509837/121981510076, with a
    compact private `night-stair-21b-compile-errors.json` receipt. The corrected
    bridge's 25 atomic tests pass; fresh Native compilation remains required.

13. **Measure the Native action/refresh gap, without changing it.** The existing
    movement benchmark now records eleven adjacent steady-clock boundaries per
    accepted step: move, assertions, view center, light reset, map-cache build,
    visibility invalidation/update, refresh return, inclusive render and tail.
    The original Native statements, route, cost/position assertions, 12-case
    matrix, rotations and source oracle remain. Fixed-size timestamp storage
    is reserved outside timing; XYZ/cost metadata and sample storage follow the
    original first-ready boundary. No production game loop/API changed.

    Additive receipts retain first-ready/frame samples, frame indices, render
    bookkeeping and signed residual. The verifier checks monotonic/adjacent
    scopes, original stream identity, accepted route/cost and conserved totals;
    inner first-frame is never added twice to its inclusive render call.
    Both pinned primary JsonOut constructors use fixed output, and a fresh
    ofstream retains six fractional digits: 1 ns in millisecond fields. The
    predeclared accounting budgets are 0.1 microsecond for identities and
    3 microseconds for the complete sum; residuals are reported without clamp.

    Local 23 movement/payload verifier tests (11 new), 240 total Python tests
    and 25 atomic bridge tests pass. The 12 actual historical Win1040 movement
    records still pass their frame checks, explicitly report no action split
    and are all rejected by the new strict receipt gate. Their absent phases
    were not reconstructed as measurements. Evidence: `night-native-step-*`.
    Native compilation and real per-function timings remain pending. These
    synchronous wall scopes include clock-probe overhead and possible waits;
    they do not measure on-core CPU or GPU execution or ordinary game latency.
    An independent read-only review found no scope/index/double-count defect.
    The release entrypoint qualifies the short 12-step route; 64-step support
    currently has shared verifier unit coverage, not a new Extended receipt gate.

14. **Invalidate geometry borders only on the shifted axis.** A pure XY step
    invalidated all four old/new perimeters, including middle cells whose full
    real/UNKNOWN neighbourhood was unchanged. Cached derive_flags reads the
    same XYZ and vertical neighbours; only ordinary-stair partner lookup adds
    radius-one XY dependencies. Directions are computed afresh, including their
    radius-two probes. Entering strips were already invalidated by prepare().

    Extracted the original eight invalidations unchanged into a shared Host
    helper. Its new regression first passed 26 causal/oracle cases, then failed
    the protected perpendicular-border hit assertion on the old rule. The fix
    keeps old/new horizontal invalidations for Y changes, vertical ones for X
    changes and both for diagonals. No source/visibility/ray/range/scene-window
    policy changed. Corner UP/DOWN partner gain/loss, ambiguous diagonal pairs,
    OMT rejection, negative coordinates, multi-cell shifts, teleports and all
    relative aliases still match a cold bounded source oracle. Existing cache
    revision, Z-window and realm controls remain in the same suite.

    After fix: all four axial 97x97x3 fixtures perform zero protected-cell
    callbacks, 873 total derived computations and 291 entering source reads;
    cold coverage remains all 28227 cells. These are exact component counts,
    not predicted Native misses or frame milliseconds. Full Release build,
    1381157-check source/range/border suite, 25 atomic bridge tests and all 50
    inventoried autonomous CTests pass (15.65 s). Before/after evidence:
    `night-border-baseline*`, `night-border-after*`, `night-border-bridge.log`.
    No local Native game or input test ran; pinned Host qualification and the
    actual first-frame timing gain remain pending.

15. **Reuse the exact Native apparent-light distance.** Primary lightmap.cpp
    on both supported pins computes observer-to-target XYZ distance in
    apparent_light_at, then computes it again in apparent_light_helper. Both
    observer getters return the same avatar; intervening clairvoyance/field/cache
    reads do not change its position or distance policy. The separate neighbour
    XY distances describe other targets and remain unchanged.

    First extracted both exact functions (5280 ASCII bytes, shared SHA256
    `7e570456c43c6dcf6b02d29bd4ef8a25a63a0889006855f28483870dd6a70a14`)
    without a behavior change. The actual extracted code compiled against
    injected Native dependencies and passed 3180 paired primary-source cases,
    establishing measured callback counts outer 2 / direct helper 1 / early
    clairvoyance 1. Then introduced a file-local implementation receiving the
    same avatar reference and already-converted integer. The public two-argument
    static helper still performs its original getter/distance fallback for
    pl_sees and other callers. The outer query reuses its own exact integer;
    no distance formula, vision threshold, discovery policy, float operation
    order or public declaration changed.

    After: outer 1 / direct 1 / clairvoyance 1. All five light enums, bitwise
    float outputs, ordered non-distance reads and complete ordered neighbour
    2D traces match the frozen primary baseline, including zero distance,
    Z/observer/target arguments, camera visibility, opacity, recoil/cant-see,
    clairvoyance and 256 neighbour masks at four map edges. Injected integer
    policies are forwarding controls, not a replacement Native-distance oracle.
    Original block provenance, upstream blobs and CC-BY-SA attribution remain
    in the include. The apply script accepts only the two exact pinned blocks;
    lightmap.cpp/include join its existing atomic updates and source receipt.
    Corrupted, duplicated, missing and already-patched input tests fail without
    partial writes or receipt replacement on either pin.

    Full Release build, 1401746-check source/range/light/border suite, 240 Python,
    30 atomic bridge tests and all 50 inventoried autonomous CTests pass
    (14.51 s). Before/after logs: `night-apparent-light-baseline*` and
    `night-apparent-light-after*`. No Native millisecond improvement or hardware
    frame-time result is inferred. Actual pinned Host compilation and visibility
    qualification remain required for these subsequent bytes.

## Fresh profile evidence and working queue

Bounded HTTP-range reads of selected program JSON from run 40425289251 used
5,133,467 bytes, rather than downloading its multi-gigabyte artifacts. Packaged
merge `54d8d5f6796a9c3e9c84d118a0afb53bf48d10e9` has the exact tree of parent
`fc2ada738ca9cf0d0ca2a33582461cba562658cf`; all 102 frozen Git sources and the
27 shared Native source-receipt inputs match. Private selected receipts and
raw JSON are in `night-fc2-native-component-profile/`; `profile-summary.json`
SHA256 `15c558ac119a97c90c2c1f75725ce6333f328134d543fa02eb1d1748bdc3629d`.
These are controlled Native fixtures, not ordinary gameplay or stage11 results.

Win1040/D3D12 **software Microsoft Basic Render Driver**, VSync 1, 640x480,
12 accepted steps per scene: first render-frame medians are street 17.49 ms,
horde 20.27 ms and city 47.50 ms. Per-step paired outer `first_ready` minus
inner `first_frame` medians are 43.17/61.61/76.25 ms respectively. This gap
includes avatar movement, refresh, assertions and render bookkeeping; it is
not the duration of any one Native function. Inclusive snapshot medians are
5.05/6.04/11.81 ms; geometry preparation 2.42/3.13/6.83 ms. The separate
gameplay fixture's Native-cache median 31.03 ms (Linux 17.34/16.49 ms) also
includes forced map-cache invalidation, so it cannot fill the movement gap.
Windows1807's older gameplay JSON is truncated and excluded.

1. Run the stage13 adjacent Native action/refresh receipt on the same matrix;
   keep render bookkeeping separate and use measured phases to select the next
   production bottleneck. The axis-specific border change passes a cold source
   oracle; qualify its actual Host effect rather than treating the old median
   1419 Host derived-cache misses as GPU mesh regeneration. Repeated Host
   candidate and Native apparent-light distances are already removed
   and callback counts verified. Enlarging the existing light-range cache remains
   rejected as a priority: recorded frames already make only 2-15 sight-range
   evaluations for 1400-3000 cache hits.
2. The typed radio skyline now passes local CPU/GPU/module/Host-source
   controls. Qualify the resulting exact revision in the real pinned Hosts;
   assess distant wire minification in that setting before further changes.
3. Run the stage12 remote normal/debug/normal and factorial diagnostic. Normal
   static stairs being withheld with BODY remains a hypothesis, not a confirmed
   current game defect. Before changing the hidden-navigation filter, distinguish
   a missing target, already-visible stair, light drift and insufficient effect.
   Any future fix must
   distinguish terrain from furniture and leave hidden contents/focus/exploration
   protected.

Other historical symptoms have existing causal fixes and regressions. This
audit has not established their recurrence in a current game session.

## Validation still required for this continuation

After the skyline stage, a fresh full Release build, all 50 inventoried
autonomous CTests (16.90 s), 195 Python tests, 25 atomic bridge tests and the
separate hidden render-only RTX 4090 scene pass. Grass remains 45 draws,
zero CPU world raster and zero warm static uploads; open multi-Z coverage
remains 21108 pixels on both rasterizers. Existing ADS/objects/muzzle/subway
brightness checks pass. Real old/new binary pairs also pass as recorded above.
These local checks do not execute the real Native extraction/input/game loop.

Pushed checkpoint `21b3878c3612c04f071cc87db8e3cb65a65192da` has passing
[GPU Windows/Linux/Clang ASan/UBSan/Vulkan](https://github.com/Neversalimus/NCMM/actions/runs/40440858188),
[source/package](https://github.com/Neversalimus/NCMM/actions/runs/40440858196)
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40440858342) runs.
Its [actual Native builds](https://github.com/Neversalimus/NCMM/actions/runs/40440858328)
failed on the recorder integration errors recorded under stage12; no Native
experiment or new timing ran. Corrected checkpoint
`2ce404deb8b0581ae92d0cea7e22b07d02ce69e9` has passing
[GPU](https://github.com/Neversalimus/NCMM/actions/runs/40441923119),
[source/package](https://github.com/Neversalimus/NCMM/actions/runs/40441923114)
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40441923116).
In its [main run](https://github.com/Neversalimus/NCMM/actions/runs/40441923163),
both pinned Windows Hosts now compile, Windows module tests pass and Native
qualification is proceeding; Linux compilation/module checks remain pending.
These passes do not qualify the subsequent local stage15 adaptation. Earlier ddfa802
Windows/Linux module jobs and both pins' Host compilations passed, then their
long Native qualification was superseded. The older fc2 Win1040 gameplay/profile
step passed, and its Linux module sanitizer pass resolved the earlier
allocation-family mismatch; neither result qualifies subsequent Host bytes.

Run the affected checks after each edit, then the autonomous local inventory,
hidden GPU pixels/resource checks, source/package checks and fresh remote
Native qualification for the resulting exact revision. Keep first changed
frames separate from warm frames. No new installer or full Native result is
claimed by this journal.
