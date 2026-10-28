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
The user's subsequent instruction disables all subagents until reauthorized;
remaining investigation, implementation and validation are performed solo.

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

17. **Keep ordinary terrain stairs as static architecture.** The actual 1807
    night evidence below motivated a narrow exception: BODY=0, physical sight
    radius, loaded bounded terrain with genuine ordinary UP/DOWN. Furniture,
    gas and vehicles cannot prove connectors, direction or opacity. No new
    permission is given to ramps/ladders/unknown terrain. BODY and noneligible
    paths retain their prior rules; physics, sight/discovery and public ABI are
    unchanged. Rare proofs perform at most 36 local terrain callbacks, not a
    new whole-window pass or persistent cache. Fresh STRUCTURE RGB uses the
    existing Native color sample. Review caught and corrected combined hidden
    furniture/gas OPAQUE being retained by the newly granted STRUCTURE mask;
    effective-permission controls now require terrain's own opaque fact.

    Shared CPU/GPU rendering draws these ordinary flights in the static branch
    while contents remain behind BODY. An offset stair without either BODY or
    STRUCTURE cannot cut a neighboring permitted support wall. The first new
    pure rendering regression failed before this renderer change. After: four
    facings and aligned/offset flights preserve exact geometry/UV/RGB, CPU
    pixels/depth and foreground-wall occlusion. Supplied hidden furniture,
    loot, creatures and fields do not change the image or create a target.

    Full Release, 1412064 source/cache checks, 182644 multilevel-render checks,
    257 Python, 30 atomic bridge tests and all 50 inventoried CTests (15.90 s)
    pass. Hidden render-only D3D12/RTX4090: 43781 pixels differ from the
    navigation-removed control; restored navigation and supplied hidden-content
    variants match the original RGBA pixels exactly. Three actual captures
    inspected; CPU world raster remains zero. Grass remains 45 draws, radio
    268/176 pixels, open multi-Z 21108 on both rasterizers; ADS/objects/muzzle
    and subway brightness controls pass. The first readback attempt stopped on
    the CLI's Unicode output-prefix encoding; repeating with an ASCII prefix
    passed. Evidence: `night-static-terrain-stairs-*`.

    Additive diagnostic schema2 now separately tests a real normal BODY=0
    snapshot against removal of only UP/CONNECTED_UP and direction. Untouched
    owned normal must match actual normal pixels/digest; fixed normal RGB,
    protected contents and complete physical/discovery ledgers remain equal.
    Separate retained thresholds are 24 removed triangles/8 changed ROI pixels/
    RGB difference 4, with status `retained_geometry_verified`. Legacy schema1
    factorial verification remains strict and separate; 17 new negative tests
    plus the prior 34 pass. Runner AST, isolated schema1/2 acceptance and 0/3
    rejection, and 162 environment-isolation checks pass without a game launch.
    **Actual post-fix Native receipt is recorded below; ordinary performance
    remains unqualified.**

18. **Avoid unused static-detail Native reads.** Non-BODY details always read
    terrain, furniture and top RGB, even for a lower-only slab or an already
    resolved facade RGB. First extracted the unchanged production block into
    a callback-scoped helper; an independent frozen 1759-byte baseline (SHA256
    `f4013a7f84a0163079fb9fd73881459707da474e82587b732ede4f8440a0317d`)
    established all three reads per static cell. The lazy-read regression then
    failed before optimization. After, terrain/furniture/RGB calls are:
    lower-only 0/0/0, top-only 0/0/1, facade 1/0/0, structure without facade
    1/0/1, instead of 1/1/1 in each case. Across 256 static cases, totals
    256/256/256 -> 128/0/128. BODY/world-null extraction stays unchanged.

    All 1024 paired cases preserve every detail byte, float bit (including
    signed zero/subnormal/NaN payload forwarding) and the order of all required
    shape/connection/neighbor/copy reads. The Native RGB formula is unchanged.
    Explicit source copying/receipts and both-pin atomic rejection tests cover
    the new include. Full Release, 1416420 source/cache checks, all 50 inventoried
    CTests (13.97 s), 34 atomic bridge tests pass. The module SHA256 remains
    `83cabfb09bc1553f4eb11def5e00209e4953e6949348aabaf695b350c9465bc1`, so the
    preceding real GPU readbacks still apply; no redundant GPU run. Fresh pure
    binary old-Host/new-module, new-Host/old-module and new/new controls pass
    37/55/86 checks. Evidence `night-static-detail-*` and
    `night-static-stair-abi-receipt.json`. These counts are not Native timings;
    actual new Host compilation/performance remains required. Removing lower
    range queries is separate: their direct-mapped cache can shift later hits,
    so it requires sequence/collision controls before any production change.

19. **Skip unconsumed lower range queries.** The loaded candidate block now
    keeps the current Native query first and omits the distinct lower-XYZ range
    query only when `static_surface` is true. That implies structure range, so
    both later lower-result consumers short-circuit. Structure-range-only air
    candidates still perform the query; the mandatory lower RGB sample remains.
    The exact pinned sight/threshold/ambient query bodies were inspected: no RNG
    or action writes, and required Native level caches already exist at this
    point. No persistent cache or range/formula/permission change was added.

    The actual extracted block and real range helper/cache are compared with an
    independent frozen 839-byte pre-change block, SHA256
    `9f5d494805a4f67005db9636c92ee80cbfe677a6ad492a9497349738936b37b6`.
    Baseline and meaningful failing-before runs were saved. Across 1728
    candidates / 704 guard and collision sequences: lower calls 1728 -> 840,
    total cache misses 4104 -> 3385; no sequence increases misses. Deleting keys
    cannot increase key runs in each of the actual 128 single-way slots, though
    individual later hits/misses can move. All used decisions/permission bits,
    target/surface/below XYZ and retained non-sight read order match; canonical
    sight results remain exact despite shifted miss sites. Controls include
    loaded/Z bounds, blindness, zero distance, signed zero and raw-bit collisions.
    Full Release, 1440145 pure source/cache checks, all 50 inventoried CTests
    (14.17 s) and 38 atomic bridge tests pass. Logs `night-lower-range-*`.
    These callback counts do not establish a Native millisecond gain.

20. **Align the multilevel test observer with proven terrain stairs.** The
    old diagnostic accepted only structural shape enums, contradicting stage17's
    permitted non-BODY ordinary terrain stairs with FLAT shape. A pure actual-
    predicate regression builds but fails before the correction. Its replacement
    requires the bounded terrain proof's exact navigation/direction/opacity,
    physical terrain ID and empty hidden contents/field/vehicle/shapes. Existing
    architectural acceptance and subsequent privacy guards remain unchanged.
    A shared terrain sampler is a mechanical extraction of the original 1577-
    byte body (SHA256 `7afcd0f9d8edb77431a91883846355752fe6b677f6bbce89029e79d80192521c`);
    520 frozen-oracle value/ordered-read controls cover limits, loaded cells,
    both floor regimes, excluded navigation, opacity and partial supports.
    Test-only inspection errors now reach the outer report with their precise
    reason. Exceptions from the actual module provider still reach the unchanged
    production quarantine. No production permission or renderer rule changes.

    After: 183637 pure multilevel-render checks, full Release, the same 50 CTests
    (13.99 s) and 42 atomic bridge tests pass. Evidence
    `night-observer-contract-{red,after,full-build,final}*`. This repairs a proven
    test-contract contradiction; whether it explains the observed Linux exit191
    still requires the new exact-source remote Native run.

    First remote module build at `866fe99` exposed a test-only portability
    error: GCC requires the explicit `<utility>` declaration for `std::as_const`
    in this fixture. Added that header globally; no helper or game-body change.
    Diagnostics were obtained through Code Review, job122109104755.

21. **Adjacent submap-shift diagnostics, opt-in only.** Earlier receipts mix
    fast in-submap actions with 40-66 ms crossings. The exact 3205-byte Native
    `game::update_map` body, identical at both pins (SHA256
    `bfb01cf4e07c99f1b41ebd460c81531fb8ff33e0c9800e58cf13e30201253a4b`),
    now has bounded stack-owned wall-clock markers around its unchanged route,
    shift, actor/NPC rebase, all-Z invalidation, cache build, spawn and overmap
    calls. Removing only the markers and existing stage14 reset-scope line
    restores the entire primary body exactly. In ordinary calls the capture
    stays disabled, with no clocks, allocations or retained Native references.
    The benchmark activates it only around the existing move call; original
    eleven outer boundaries and first-ready storage/serialization order remain.

    Fail-before actual-source control: all 60 Native oracle pairs agreed, but
    zero internal invocations were recorded. After: 60/60 and 513 pure checks,
    including actual two-translation-unit TLS sharing, disabled probes, call
    order/arguments/NPCs/positions/cost/return, exceptions, reentry and overflow.
    Strict report verification rejects partial, reused, nested, dropped,
    exceptional, backward or out-of-action scopes and inconsistent rebases.
    A second fail-before control demonstrated that individually valid events
    could be assigned to different actions; consecutive rebased coordinates
    are now tied to the existing absolute route. Historical absence stays
    explicitly unavailable; new qualification requires the complete receipt.

    Full Release, 50 inventoried autonomous CTests (16.14 s), 46 atomic bridge
    tests and 263 Python verifier tests pass. Evidence prefixes
    `night-submap-profile-*`. Injected test clocks are not performance data;
    actual Native inner timings and compilation required the remote run;
    the resulting exact-`7f0ea25` receipts are recorded below.
    Recorded timings include diagnostic probe overhead. No gameplay call,
    renderer, visibility rule or game-input harness was changed by this stage.

22. **Resolve uniform Native floor predicates once.** Exact primary
    `map::build_floor_cache` at both pins is 2349 bytes, SHA256
    `80c7cfd987d5550275fd2eabf182d2027591c99d22fa8ca58024da46c2495eb6`.
    After its original two submap lookups and null/debug guards, fully uniform
    current/lower blocks resolve the same terrain-hole OR and actual shared
    `f_null` canopy once. All mixed blocks retain the entire old loop. Invalid
    integer IDs and absent null-string registration also fall back, preserving
    every original diagnostic. Uncovered gaps fill exactly twelve Y-ranges,
    respecting the full cache stride; dirty/no-floor-gaps/return stay exact.
    No cache, visibility, actualization or gameplay call is skipped.

    An unchanged extraction first passed 452 independent primary-oracle cases
    and then failed only the uniform read-count assertion. After: 453 cases /
    8829965 checks pass, including every full-grid bit, ordered nonuniform
    reads, diagnostics, modded null canopy, missing submaps, lowest Z, no-Z
    aliasing and bounds guards. In the all-uniform 11x11 case, terrain and
    furniture getters each decrease 17424 -> 121. Removing only the fast block
    restores the full primary body. Both exact-pin adaptations are guarded
    atomically, copied and included in source receipts.

    Full Release, the same 50 inventoried autonomous CTests (14.73 s), 51 atomic
    bridge tests and 263 Python tests pass. Evidence `night-floor-*`.
    Independent read-only source review found no residual defect. Native
    compilation/runtime qualification of these new bytes is pending. The
    component call reduction is not a measured Native/frame-time improvement;
    the observed 17-32 ms cache cost below covers all of `build_map_cache`.

23. **Skip provably empty uniform Native item walks.** Both complete primary
    `map::reconcile_loaded_items` bodies are preserved: 1040 is 4665 bytes,
    SHA256 `43e9d5dff43780bc726edd5ce0798ecd7161dc6a8978072349f3d4b4088c2f82`;
    1807 is 4660 bytes,
    `ac9217d4344b3f4363009e3549e542a37fff80e894e0eb3df910b910cc6cc4d9`.
    The only upstream difference is the cursor constructor; a private macro
    expands to each pin's exact expression and is immediately undefined.
    SHA/uniqueness and macro-collision guards reject every other adaptation.

    After the original null check, a uniform submap's actual **mutable** item
    sentinel is checked once. If empty, all original 144 tile iterations have
    no item/craft/wakeup callback. A nonempty sentinel keeps the complete
    original loop and pays one additional pure getter; nonuniform reads remain
    exact. The guard is reevaluated for each submap after earlier callbacks.
    Vehicles, every cargo part, recursive children, avatar/NPC inventories,
    craft reservations, seen-token cleanup and full/additive policy retain
    their original order and state. Actualization is unchanged.

    Unchanged extraction: 72 semantic pairs plus four whole-bubble oracle
    comparisons pass, then the final repeated-getter assertion fails. After:
    76 pairs / 1080060 checks pass. Per pin, fully empty uniform no-Z getters
    17424 -> 121; all 21 Z-levels 365904 -> 2541. Separate private compiled
    negative controls using the wrong const sentinel or uniform-only skipping
    both fail on callback/state equivalence, before the cardinality gate.
    They are pure owned std::list-backed models, not plf performance models.

    Full Release, the same 50 inventoried autonomous CTests (14.29 s), 56 atomic
    bridge tests pass; unchanged Python verifier coverage remains the stage22
    263-test pass. Evidence `night-items-*`. An independent read-only review
    found no residual defect. Actual Native compilation/runtime and elapsed
    gain remain pending; the compiler's possible existing loop hoisting is not
    measured. Getter counts alone establish no Native or first-frame speedup.

24. **Resolve empty uniform vision-transparency predicates once.** Both pinned
    `map::build_vision_transparency_cache` primary bodies are identical: 3826
    bytes, SHA256
    `5dec4f06915166dad4638e9656dbd9f672acb20bc82a8b74ebe84a8bea229d5d`.
    After the original null/debug and dirty-bit guards, a valid uniform terrain
    without TRANSLUCENT proves that its old 12x12 loop has no writes. Only that
    loop is skipped. Invalid IDs and translucent/mixed submaps keep the old
    path. The initial full memcpy, posture/coverage handling, accumulated dirty,
    player-cell reopening, dirty-bit reset and return remain unchanged.

    Red: 447 semantic cases match before the getter-count assertion fails.
    Green: 447 cases / 18617133 checks, with every float cache bit and protected
    state retained, including signed zeros, NaN payloads, infinities and
    subnormals. All/none/partial dirty masks, missing submaps, invalid IDs,
    player/Z/coverage/posture combinations and ordered required reads are
    covered. Uniform non-translucent getter/obj/flag calls each 17424 -> 121;
    translucent fallback pays one additional pure read. Range responses and
    light sentinels are explicitly injected model values, not reconstructed
    Native iterator implementations or numeric constants.

    Full Release and the same 50 inventoried autonomous CTests pass (17.77 s).
    Initial atomic testing exposed an old whole-lightmap expectation that
    omitted the second extraction; it now checks both independent frozen
    replacements and still compares the entire surrounding source. After:
    all 61 atomic bridge tests pass. Unchanged verifier coverage remains the
    stage22 263-test pass. Evidence `night-vision-*`. Independent read-only
    review found no residual defect. New Native compilation/runtime and any
    elapsed/frame-time improvement remain pending.

    The retained primary generic_factory.h (both pins, blob
    `65ae31e67f8e5632ea67c2944329f98f247a45f4`) also confirms stage22's quiet
    invalid-f_null eligibility check: negative CID caching makes later find_id
    return false without another lookup, but convert still emits its warning
    after every false result. The check does not consume fallback diagnostics.

25. **Batch identical uniform outside-cache stamps.** Complete Native
    `map::build_outside_cache` is identical at both pins: 2217 bytes, SHA256
    `99181cb92ff370313fa5870684f513ee0dad010dcc0798f6aa9587b085a66abc`.
    After the unchanged null guards, valid uniform terrain/furniture resolve
    the original INDOORS OR once. Terrain remains first: an indoor terrain
    never queries furniture, even when f_null is missing. Mixed, invalid or
    missing-null definitions retain every old loop and diagnostic. Outdoor
    blocks have no stamps; indoor blocks fill the exact 14x14 union of all 144
    original 3x3 stamps as fourteen rows with padded stride 134, retaining the
    one-cell halo, adjacent/corner interactions and final active copy extents.
    Underground keeps its original early return and dirty-flag behavior.

    Red: all 164 semantic pairs and four full-grid OR combinations agree, then
    the getter-count gate fails. Green: 168 pairs / 13289378 checks pass,
    including complete bool caches, untouched tails/guards, both dirty states,
    widths 1/3/11, negative Z, null blocks, modded f_null, invalid integer IDs
    and missing null-string conversion diagnostics. At 11x11, terrain getters
    17424 -> 121 in all four OR cases; furniture 17424 -> 121 only when terrain
    is outdoor, otherwise 0 -> 0. Red/green counts are component evidence, not
    Native execution timings.

    Full Release, the same 50 inventoried autonomous CTests (15.24 s), all 66
    atomic bridge tests and independent read-only source review pass. Unchanged
    verifier coverage remains the stage22 263-test pass. Evidence
    `night-outside-*`. All original function bytes outside the bounded fast
    block remain exact. New Native compilation/runtime and any first-frame
    gain still require remote qualification.

26. **Repair the pinned terrain-ID link contract.** Actual `191d881` CI
    rejects both Windows Hosts with LNK2019/LNK2001 and Linux1807 with undefined
    references to `int_id<ter_t>::is_valid()`. The specialization is inline in
    Native mapdata.cpp without an externally usable definition; the earlier
    local fake supplied a body and hid this integration defect. All three
    uniform terrain gates now use `static_cast<std::size_t>(id.to_i()) <
    ter_t::count()`, the exact quiet bounds predicate from generic_factory.
    Retained primary blobs `65ae31e67f8e5632ea67c2944329f98f247a45f4`
    (generic_factory.h, lines519-520) and
    `415150870af4a2954eba5c40879ea4681605f301` (mapdata.cpp, lines1150-1153)
    were independently hashed: count() returns that same factory's size.
    Furniture validity remains its separately linkable Native specialization.

    Red: leave only the terrain validity declaration in all three fixtures;
    the unchanged includes reproduce three unresolved terrain symbols locally.
    Green: full Release and the same 50 autonomous CTests pass (14.90 s), with
    all 66 atomic bridge tests. Each fixture adds INT_MIN/-1/upper-bound/INT_MAX
    invalid-ID pairs at widths1/3. Complete frozen bodies and ordered diagnostics
    remain exact: floor461 cases / 8981133 checks, vision455 / 18919245,
    outside176 pairs / 13615282; eligible uniform getter counts stay17424 ->121.
    Evidence `night-terrain-link-*`. Unchanged Python verifier coverage remains
    the stage22 263-test pass. New Native linkage and gameplay require remote
    qualification; no new latency gain is inferred from this repair.

27. **Adjacent Native map::shift child scopes.** The measured remaining
    crossing cost justifies separating setup, unload, origin/traps/remote,
    vehicle-zone writes, cache rebasing, interleaved grid copy/load, vehicle
    levels/remote restore, support rebasing, actualization, item reconciliation
    and local cleanup. A bounded 32-child capture shares the existing global
    inline TLS and clock; ordinary disabled calls perform no clock/allocation,
    and enabled probe storage also allocates nothing. Native calls and order
    stay exact. Complete primary bodies are independently guarded at 4916/4968
    bytes; the later pin's sole extra memory-sweep flag is locally bound and
    immediately undefined. Declaring the scope first includes loaded-vector
    destruction in the final interval. Early returns, exceptions, saturation,
    nesting and dropped children remain explicit, never invented as complete.

    Serialization stays after first-ready. Fresh qualification checks all 12
    child boundaries, the actual clamped sequence for multiple shifts, parent
    ordinal, nonoverlap/containment and signed parent gap; 3 ns child tolerance
    matches the 1 ns serialized quantum. Inclusive parent and child times are
    never added together. Historical absence remains unavailable; it cannot
    qualify the new contract.

    Full Release, the same 50 autonomous CTests (14.43 s), 274 Python verifier
    tests and 72 atomic bridge tests pass. Complete-source owned-state models
    cover 228 paired scenarios / 3051719 checks, all eight directions,
    widths1/3/11, missing caches/submaps, remote/support state, every loaded-Z
    callback, exceptions, multi-call and nested shifts, dropped/saturated
    counters and real cross-TU TLS. Four compiled negative controls reject
    absent hooks on each pin after all 96 semantic pairs, an exit before local
    cleanup, and a lost 1807 memory flag. Initial atomic testing caught CRLF
    versus LF in the newly generated include; canonical LF restores exact
    source-copy receipts. Evidence `night-map-shift-*`. These injected clocks
    are not timings; actual Native compilation/captures remain pending.

28. **Transfer support-cache nodes during coordinate rebasing.** Actual child
    scopes below identify 1.2–2.8 ms median support rebasing in the diagnostic
    route. Native moves its std::set aside, allocates a replacement node for
    every translated position and destroys all old nodes. A private helper now
    extracts each old C++17 node, translates its detached value and inserts it
    into the cleared destination. Only default-allocator sets are accepted;
    both containers' allocators are compatible. The separate old set is drained
    once, so inserted values cannot be processed again. End hints preserve
    correctness even for coordinate orders that change under translation.
    Empty behavior, complete XYZ/outside-bubble positions, ordering and all
    Native calls around the block remain unchanged; no actualization is skipped.

    Red: 90 semantic cases pass, then 8192 positions still allocate 8193 times
    and retain 0 value-node addresses. Green: 8193 -> 1 allocations (MSVC's
    constant move sentinel), all 8192 value-node addresses retained, with the
    exact same coordinates. The empty set allocates 0. Independent normal and
    parity-first orders, sets through 4096 values, all nine XY offsets and
    negative/outside coordinates cover 90 cases / 76739 checks. The complete
    frozen Native map::shift oracles still pass 228 scenarios / 3051719 checks
    on both pins. Source guards restore the one replaced support block before
    comparing every original function byte. Full Release, the same 50 autonomous
    CTests (16.56 s) and all 72 atomic bridge tests pass. Unchanged verifier
    coverage remains stage27's 274-test pass. Evidence `night-support-nodes-*`.
    New Native timing and first-frame gains remain pending; allocation counts
    are not measured game-frame improvements.

29. **Proven no-op tile loops in underground actualization.** Actual child
    captures attribute 7–11 ms median to inclusive actualization, so this targets
    repeated tile work rather than the already-small bitset rebase. Both pinned
    primary actualize bodies are identical: 2401 bytes, SHA256
    `469b917a6c8b6cc465780354731c07c5fbd8b2eda0645c80f643a58cb3d40502`.
    Every vehicle overflow/refresh, item callback and explosion runs first.
    The shortcut then requires a still-matching loaded submap, Z-level support,
    valid underground grid coordinates and uniform storage. Quiet registry
    checks protect invalid terrain/furniture and missing f_null diagnostics.
    Eligibility further excludes both emission lists, built-in traps, plants,
    harvested terrain and tapped maples. Uniform getters supply no explicit
    traps/radiation; funnels are already disabled underground. The mutable
    shared field object must actually be empty. Only then can every remaining
    helper return before effects/RNG, allowing the 144-cell loop to be omitted
    while retaining the exact last_touched update. Zero elapsed time alone
    never qualifies. Above-ground, mixed, aliased/non-Z, active or invalid cases
    retain the complete original loop. New eligibility facts and definition
    references are created after callbacks and are not retained across one.

    The bridge guards the full primary body and five independent early-return
    prefixes, preserves every byte outside the bounded shortcut, and copies/
    receipts the include atomically for both pins. Read-only original Git
    objects were used instead of the modified probe checkout's working files;
    one missing 1040 source was fetched within the existing shared 10 MiB cap,
    verified by full SHA256/Git-blob identity, then only small functions saved.

    Red: the frozen original passes 686 state comparisons, then fails the work
    gate with 144 -> 144 explicit-trap reads. Green: 144 -> 0 in the eligible
    model, 691 paired cases / 1384 checks pass, including five callback exception
    boundaries. Owned active-helper models cover flags, shared fields, invalid
    IDs/conversions, all grid guards, mapping replacement/materialization,
    callback ordering, time rewind/zero/positive elapsed and last_touched. They
    exercise predicates and observable active paths, not complete CDDA helper
    gameplay. Both pins also share the verified field.cpp blob whose begin/end
    methods only return the underlying iterators. Three compiled controls reject ignoring the
    shared field, using a stale submap and gating before item callbacks.
    Full Release builds; the affected autonomous cache CTest passes in .81 s;
    78 atomic bridge tests pass, with the final six affected tests rerun after
    the last adjustment. Unchanged 274 verifier / 50 CTest coverage remains
    stage28 evidence, not a fresh all-suite claim. Evidence `night-actualize-*`.
    Actual new Native compilation and elapsed impact remain pending; this is
    a work-count result, not a measured first-frame or 15 ms improvement.

30. **Proven empty terrain phase scans during loading.** Inclusive grid copy/
    load remains 7–9 ms in the latest child captures. Its Native loadn tail calls
    temperature-based terrain processing 144 times on every loaded level, even
    for uniform definitions without phase rules. Recorded original terrain is
    a separate private map and can exist while storage is uniform; it must not
    be inferred empty. A read-only inline accessor tests that actual map. It
    adds no data or virtual methods and changes no public module ABI.

    After the unchanged loading/vehicle/tree callbacks and weather/origin setup,
    the shortcut revalidates mapping, XYZ bounds and Z-level semantics. Only
    uniform terrain with valid integer/string registry definitions, empty
    phase_method and no recorded original terrain can omit the loop. Real
    restoration, freezing, weather seeds and invalid-definition diagnostics
    retain the original calls. The original full loadn body (5042 bytes),
    temperature helper (3269 bytes), metadata query (129 bytes) and394-byte
    header anchor have independent source guards for both pins. The existing
    pure cache-reset scope still wraps exactly the same flag writes. Every
    other Native loadn/header byte is preserved; both new includes are copied
    and receipted atomically. Tiny bounded GitHub metadata establishes that both
    pins share the same mapgen.cpp Git blob; source was read from existing local
    Git objects, without a new remote330KB source or checkout.

    Red:230 paired state cases pass before the work gate fails at144 ->144 cell
    calls. Green:144 ->0 per eligible submap,3024 ->0 over21 eligible levels;
    230 cases /443 checks pass. Exact Native loop/metadata/early-return code is
    compiled against owned data; active freezing/weather is an observable
    model, not a complete Native weather simulation. Cases protect mixed and
    unknown phases, all recorded-history positions including outside the tile
    domain, invalid IDs/string conversions, aliases/bounds,21 Z levels and
    callback changes. Four compiled negative controls reject ignoring original
    history, mixed terrain, stale mapping and gating before callbacks. Full
    Release and affected cache CTest (.86 s) pass; all84 atomic bridge tests
    pass. The Native terrain string-validity and integer-to-string functions
    have external definitions; the unavailable integer validity specialization
    remains deliberately undefined in the model. Evidence `night-phase-*`.
    Actual new Native compilation, elapsed impact and first-frame benefit are
    pending. No unchanged renderer/hardware suite was rerun for these Host edits.

31. **Attribute Native cache construction before changing its invalidation.**
    Current actual crossing receipts below still attribute13–24 ms median to
    update_map's inclusive build_map_cache call. The separate benchmark refresh
    costs4–8 ms and is not a substitute for that action measurement. Add opt-in
    adjacent scopes for structural caches, vehicles, vision transparency, skew
    cleanup, moncam state, seen/moncam caches, character lights, lightmap and
    local cleanup. Every Native call remains in place. Both pinned complete
    builder bodies are identical4288 bytes, SHA256
    `1f8d5580024513cbd0f684eb32957832ba194ba41ba7e3c4d3ccc5919c1fda21`;
    removing only the probe statements restores every original byte. The scope
    is the first local so its exit includes destruction of later temporaries.

    Disabled capture reads no clock and allocates nothing; enabled capture uses
    bounded fixed storage and records actual update/shift ancestry, Z bounds,
    lightmap branch, missing markers, exceptions, nesting, drops and saturation.
    JSON construction remains outside the first-ready boundary. Fresh strict
    qualification requires the new receipts, exact labels, monotonic contained
    children and exactly one direct main builder on a crossing. Historical
    reports remain explicitly unavailable. Signed parent gaps are retained,
    including tiny rounding residuals; inclusive parent/child times are never
    added. No light, actor, discovery or gameplay invalidation is removed.

    Full Release,283 Python tests (18.444 s),89 atomic bridge tests and the two
    affected autonomous trace CTests (.34 s) pass. Five affected atomic tests
    pass again after the final source-placement assertions. Thirty-six compiled
    primitive checks cover clocks, allocations, cleanup, owners, cross-TU TLS
    and failure protocols. Three compiled negative controls reject absent
    hooks, misassigned labels and a scope ending before local destruction.
    The complete Native builder is checked for source preservation/placement,
    not compiled against a locally invented engine model. The exact043 remote
    compilation and child timings are recorded below. This diagnostic
    stage makes no performance-gain claim. Evidence `night-map-cache-*`.

32. **Reuse exact sight attenuation denominators within each Native segment.**
    The new actual cache receipts identify the seen-update block as the main
    child (6.75–13.19 ms median in the recorded software scenes). That block
    includes other work; it is not an isolated shadowcaster timing. The pinned
    sight formula repeatedly computes `numerator / exp(transparency*distance)`.
    Keep multiplication and division in their original order, and reuse only
    the last exact exponent's denominator. The key uses all float bits,
    including signed zero. A 12-byte stack object lives for one horizontal or
    vertical segment; there is no heap, TLS, persistent warm state, map pointer,
    approximation, changed ray, reduced radius or deferred scene publication.
    Only the exact Native `float/sight_calc` specialization is cached; fragment
    and custom callbacks retain their original calls and side effects.

    Every exponent outside[-80,80], including nonfinite values, still invokes
    the original evaluator every time. Non-normal results are never cached.
    The pinned pure sight/check/accumulate path does not change rounding mode.
    Both complete Native files are identical at both pins and guarded before
    patching: cpp32649 bytes, SHA256
    `fbf4e65de6c85af60267e5b73a0d2d4d933a0dd24fe4752a9685281d32fa59c9`,
    header5106 bytes, SHA256
    `9f5eea643f64c436fcbb202e8b966d53bc54a9ce45392447ceebf17cc1ad0af8`.
    Removing one private include, two segment locals and two call substitutions
    restores every cpp byte. The Native header remains unchanged.

    Full pinned algorithm bodies, all24 transforms, real pinned zlib PLF list
    and the Native float math run against owned coordinate/grid/distance and
    fragment-helper context. **498 paired cases /13073 checks** pass: every
    output float bit on21 levels, complete input/floor identity, check/accumulate
    order, both distance modes, all directions, Z/XY edges, walls, floor holes,
    haze, deterministic mixed fields, prefilled outputs and zero/negative
    radii. Full60-tile cases cover air/solid/structured walls; fine transparency
    permutations use radius24 to bound model complexity. An initial radius60
    fine-permutation Cartesian matrix was stopped after excessive model cost;
    it yielded no timing or pass claim. Production radius remains60.

    The uncached control passes the same semantic matrix and then fails work
    acceptance: **10702148 ->10702148** exponent evaluations. The cache passes
    with **10702148 ->5080030 (52.53% fewer)** and identical distance requests.
    Four separately compiled controls reject disabled reuse, a wrong cached
    denominator, caching unsafe exponents and merging signed-zero keys. Edge
    checks cover numerator changes, invalidation, exceptions, non-normal
    evaluator results, errno and independent cache scopes.

    Full local Release,283 Python tests (18.578 s),94 atomic bridge tests
    (47.102 s) and the new source-model CTest (3.01 s) pass. The CTest has a60 s
    limit and is added to existing Linux ASan/UBSan coverage. New real Native
    compilation/gameplay/paired timings remain pending. These deterministic
    work counts and owned-model timings do not establish ordinary RTX4090
    frame time or the15 ms target. Evidence `night-sight-*` and source receipts
    `night-visibility-source-audit/`; no local game/input or subagent was used.

33. **Keep queued movement chords independent of event-processing latency.**
    Exact043 program reports from Windows1040 FPS and Linux1807 gameplay both
    fail `fps_audit_chord_not_one_native_diagonal` (exit191, no timeout/dump).
    Linux1807's other five processes pass. These are real failures, despite
    earlier passing1b two-pin Linux and Windows functional/appearance gates.
    Original CI logs remain unavailable through Code Review while PR head is
    still1b; only selected application JSON/provenance was read, never stdout.

    A pure router/solver composition reproduces a concrete timing defect:
    advancing between queued W and D turns handler CPU delay into one-axis
    travel. Atomic SDL queue insertion cannot prevent that. Integrate preceding
    held state at Native's existing pre-poll hook instead of between keyboard
    events. The existing post-drain pending/take path samples the full chord;
    Native keys/modal ownership and action/collision rules remain unchanged.
    Fourteen delay/order cases (0–100 ms, both key orders) retain one diagonal
    request; a separately compiled old-order control fails that assertion.
    The actual cause of both remote failures remains unconfirmed until a new
    Native run passes. No local SDL/input/game reproduction is claimed.

    Both affected pure CTests pass (.06 s): locomotion338 checks and router3560.
    The25 FPS-report verifier tests pass. On the same unchanged remote diagonal
    assertion's failure path, retain a `success:false` program JSON containing
    actual displacement, action count/cost, yaw, fractional position and last
    action. Incomplete evidence still fails; there is no retry or relaxed gate.
    Evidence `night-chord-*`, `night-fps-043fb4d/12756303634/` (JSON hashes
    `8b9cd0169e737df994aba32d6a9770d9e4f743474fd5fb8c3d5482bc8a8a3f0f`,
    `1b86bfc27fc1d67fdab334e3e02457e358d69e3cac5a71661278263a80f33a2b`)
    and `night-linux-043fb4d/12757734002/`; the process-list hash is
    `393c21ecbd46bb0efe7902668d08d6ebd8ce575ea4eee51da93b068bf56ceee5`.

34. **Reject the Z-loop clamp; independently time the unobserved sight math.**
    A proposed valid-origin float-only horizontal extent clamp preserved498
    complete algorithm pairs /14928 checks and every valid grid read, with
    rows14178044 ->12985288 and unchanged exponent work. Invalid inputs and
    fragment paths retained their full loops. However, an isolated comparison
    against the already cached algorithm worsened six of eight paired median
    ratios (up to24.8%). The initial long benchmark exceeded its60 s bound and
    produced no timing result; a shorter alternating-order7-pair/2-cast sample
    established no reliable benefit. **All candidate changes were removed**;
    the saved stage32/33 implementation and its3D model were rebuilt and pass
    (CTest2.75 s). No branch contains the rejected extent helper/clamp.
    Private summary SHA256
    `70599a6abdb3c45189f94bbd8c481705d9ce3ee4213185230062c76ffb3d15b5`,
    `night-shadow-bounds-*`. Fewer operations alone are insufficient acceptance.

    Also compare stage32 against the full original with **the actual Native
    std::exp formula and no exp observer/evaluator wrapper in either path**.
    Both use pinned algorithm/PLF and owned coordinates/grids/distances, seven
    alternating-order pairs, two casts/sample, with complete output-bit and
    unchanged-input checks. MSVC/O2 model medians ms (original ->cache):

    | Distance | Air | Walls/floors | Haze | Mixed |
    |---|---:|---:|---:|---:|
    | Square |6.114 ->4.527|.1668 ->.1410|10.109 ->9.759|37.230 ->37.012|
    | Circular |3.683 ->3.621|.1366 ->.1327|9.598 ->9.113|36.710 ->36.816|

    Individual paired-ratio medians range.746–.970. **The independent circular
    mixed median worsens**, as do both first mixed samples and the first circular
    wall sample. These small noisy owned-model captures support the cache as a
    candidate, not a guaranteed per-scene/first-frame benefit. Background load
    is uncontrolled; they are not ordinary Native, RTX4090 or first-present
    timings. All56 comparisons retain exact output bits. Summary SHA256
    `b548908ca40689e90fe084c8066b820cdcba237df5027fa2ee301efc223c0a75`,
    `night-sight-unobserved-paired-model/`.

    Both module jobs including focused Linux ASan/UBSan pass for1c8bfdc and
    eb816c5. The two actual Windows Hosts compile at eb816c5 in diagnostic
    run40569273849; actual input/gameplay and Linux Host checks remain pending.
    Main PR1b's full comparison run40534567570 is preserved. Integration-tier
    dispatches intentionally do not establish the required full qualification.

35. **Retain released taps until a real fractional physics step.** The pure
    router/solver reproduces lost input when a press and release are drained
    inside one SDL millisecond: walking consumed the saved edge before advance
    could integrate any time. Add an explicit non-consuming peek; the Host
    consumes only when the solver can advance, after collision constraints may
    have reset its clock. Existing one-argument behavior remains unchanged.
    Cold clocks, zero/backward time, braking, no fabricated Native action and
    modal/focus cancellation pass. No new queue, class field, ABI/config change
    or elapsed-time catch-up is introduced.

    The uncured shared implementation fails the new assertion; two separately
    compiled controls reject consuming a peek or consuming at zero elapsed time.
    Full Release and three affected pure CTests pass (.12 s): router3560,
    locomotion364 and camera36 checks. All288 Python tests pass (20.002 s),
    including30 FPS verifier tests; all94 atomic bridge checks also pass on
    this source (50.678 s), and the packaged PowerShell runner parses.
    Evidence `night-short-tap-*`; no local game/SDL/input execution is used.

    Prepare a new actual Native same-batch press/release case for all four
    keyboard/renderer combinations. It must present a released fractional
    position twice while retaining physical tile, calendar, moves and action
    count. This isolated probe declares the valid F5 maximum6 to exceed the
    existing presentation threshold even at1 ms; the held sub-tile probe keeps
    its declared minimum1, and ordinary fixture speed is restored. Fresh reports
    require184 cases, both controlled speeds, release metadata and zero tap move
    cost/actions. This new184-case Native check is **pending**; earlier180-case
    evidence must be checked with its exact historical verifier.

    At exact eb816c5, both Windows actual180-case FPS steps finish successfully
    (1807 19:52:08 UTC,1040 19:52:46) in run40569273849, including the earlier
    failing W+D assertion. Linux/remaining Native phases still run. These results
    qualify that bounded stage33 matrix, not this new tap scenario or full
    current-source qualification. PR1b's long full run is still preserved.

36. **Close the preserved full qualification and retain mixed Native timings.**
    Exact1b run40534567570 now **passes all eight jobs**, including both full
    Windows Hosts, both actual Linux Native jobs and required qualification.
    Code Review confirms head1b and Windows jobs122299656609/122299656571;
    log views are truncated, so job conclusions, not unseen log details, support
    this claim. Existing GPU/source/runtime passes remain at that same source.
    This full pass does not qualify subsequent stage31–35 Host bytes.

    Windows1040 qualification artifact12766611444 verifies38 payload files and
    eight clean focused exits. Runtimee564's tree matches1b. Only seven selected
    program/provenance JSON files plus the archive directory were read, using
    the existing movement1b budget (771865/8388608 bytes total). Baseline main
    `5258e5313ad5deae23f967a4a7fc91c7554377f4` differs from its accepted source
    `9e3d6230f7adbd7462de80a665fb69e939cd81f1` only in the two feed JSON files,
    independently checked with offline Git. Both Host and module were replaced.
    All112 before/after benchmark keys match, but geometry differs in corrected
    scenes; the reports are not evidence of an identical-output global speedup.
    Software D3D12/640x480 GPU city median77.719 ->78.894 ms,
    p95 81.947 ->93.607 ms; mode-first-ready62.743 ->54.687 ms.
    Open first-ready59.908 ->60.175 ms worsens. These controlled mode timings
    include Native2D and forced asset invalidation, not player FPS, on-core CPU,
    GPU execution, ordinary action latency or the15 ms target.

    Exact eb816c5 dispatch40569273849 has strict new movement receipts for both
    pins:288 actions /144 cache children, all exit0 without wrapper/debugger
    errors/dumps. Four selected JSON files per artifact12767421535/12767381566
    reserve362165/8388608 bytes, explicitly reconciled against both range counts.
    Source/runtime are the same commit/tree0f93248a64ca78ff954cc3da4d7edd8afb85ccda.
    Summary hashes1040 `92e737d90b87fa37301510f94aca50f0f63b4e466ba21a31df0215db671803c3`,
    1807 `92fa4a74d3217d1780788dcb487324b156f6399b90dff63414726f8ab1a84a52`.
    GPU640/VSync1, seen-update medians043 ->eb ms:1040 street6.754 ->7.133,
    horde8.529 ->10.222, city12.578 ->10.889;1807 street8.796 ->8.944,
    horde9.687 ->9.261, city13.190 ->11.479. Main cache medians likewise mix
    gains/regressions (1040 horde17.909 ->22.038 ms). Different CI runs/VM load
    prevent a causal cache-gain claim; independent phase medians cannot be added.
    The cache/probe/patch code is byte-identical between1c andeb. Verified local
    Native1807 lightmap blob0852aeec calls3D cast_zlight unconditionally from
    build_seen_cache;2D castLightAll is also used for vehicle mirrors.

    Both Windows eb FPS reports independently pass the exact historical180-case
    verifier and fail the current184-case gate as intended. Four W+D cases per
    pin pass. Boundary frozen frames are zero, but measured software gaps are
    165–443 ms, not15 ms. Strict FPS summary SHA256
    `e511b256e50fe313adc69ddaa21aad0b0c03c35f2572405f999638232281876b`.
    Private evidence `night-fps-eb816c5/`, `night-movement-eb816c5/`,
    `night-stage30-windows*-tool-receipt.json`. Next: qualify the new tap code,
    then use a same-machine Native comparison before promoting timing claims.

37. **Reject larger sight-history caches.** Private two-entry MRU and16-entry
    hashed alternatives preserve all480 float-grid pairs each,25504 combined
    checks, exact input/floor bytes and callback values/order. Relative to the
    retained one-entry cache's5080030 exponent evaluations, counts fall to
    4205268 /1821294, but stack storage rises12 ->20 /144 bytes. No Native or
    production source is changed by this private comparison.

    The observer-free timing paths use the actual standard exponent and PLF
    allocator: seven alternating pairs, two casts per sample, eight combinations
    of scene and distance metric. Both alternatives worsen four of eight paired
    median ratios; two-entry worst1.0412, sixteen-entry worst1.0767. First-sample
    regressions also occur. Uncontrolled background load and the owned numeric
    context prevent ordinary-game/RTX/frame claims. Fewer exponent calls do not
    justify either replacement. Raw/summary/source hashes are retained privately
    under `night-sight-history-model/` (summary SHA256
    `922d7960f8ad68e0a152cf4313bee5e15c57a9ad057baf43583abb7b70130589`);
    the existing one-entry implementation and
    all153 frozen sources remain intact. Next: count Native span-list allocation
    and test synchronous storage reuse only if the baseline supports it.

38. **Reject cast-local span-list reuse on current evidence.** A private candidate
    reuses one real PLF list only across the synchronous cast's24 segments,
    clearing and recreating the initial span each time. Both float and owned
    fragment-helper matrices retain all768 paired output grids,18432 checks,
    inputs/floors and callback values/order. Counting allocators show29044 ->2750
    allocations across that matrix, with every allocation freed before return.
    Representative open/wall/haze/mixed cases fall48/60/224/259 ->2/4/14/32;
    mixed allocated bytes3403424 ->1058848, but peak live storage1057792 ->1057824
    increases32 bytes. There is no persistent pool or cross-action warm state.

    A separately compiled build restores the original standard allocator and
    exponent in both timing paths. Seven alternating pairs/two casts per sample
    worsen five of eight paired median ratios, up to1.0427. First open sample
    4.123 ->4.439 ms also worsens. The large allocation-count reduction does not
    establish a latency benefit, so no production/container/Native patch is kept.
    Private raw counts, exact candidate and timing summary are preserved under
    `night-sight-workspace-model/` (summary SHA256
    `f4a6405d345402cfc8a038677d039c39f5957bfe4c4208031280eec1e8fc413e`).
    These source-model measurements with ongoing
    background work are not Native gameplay, RTX4090 or first-present results.
    Next: obtain a same-process comparison on actual Native cache inputs before
    altering the retained caster further; preserve the current full CI run.

39. **Prepare same-process Native sight comparison.** An explicit isolated short
    movement option compares the retained caster with a second instantiation of
    the same fully guarded Native body. A distinct sight callback selects the
    generic forwarding path and executes the original formula every time.
    Ordinary Native visibility does not call the new diagnostic entrypoint.
    No cache capacity, traversal, range, math formula, rendering or public ABI
    is changed. Missing/existing companion files fail before any Native write.

    Street/horde/city each collect nine alternating pairs of eight casts, on
    the same actual Native transparency/floor pointers, origin, numerator and
    real no-floor-gap direction. Outputs are owned. Reset/allocation, hashing,
    validation and JSON IO are outside the timed scope; original caster-owned
    PLF allocation stays inside. All365904 output floats are compared bitwise
    after each pair. Exact byte snapshots protect every transparency/floor/
    seen/camera grid across21Z (4756752 bytes), plus avatar origin/moves/calendar.
    Reports state warmed Native-caster wall time only, not whole seen-cache,
    cold load, GPU execution, ordinary FPS or first-correct-presentation timing.
    Current ordinary movement timings carry the diagnostic-enabled flag; this
    extra pre-matrix workload must not be silently equated with old warmup.

    Full local Release build passes; frozen coverage now includes158 exact
    indexed sources. The math target passes498 ordinary pairs /13265 checks,
    including eight added original-reference pairs. A separately compiled
    negative control which accidentally caches the reference fails the exact
    exponent-count requirement. All95 atomic bridge tests (55.563s),299 Python
    tests (19.860s), including11 strict receipt tests, and affected CTest (3.19s)
    pass; PowerShell is parsed without execution. The verifier accepts measured
    regressions, rejects missing/nonfinite/fabricated-scope data and does not
    impose a favorable timing threshold.

    The actual comparison include also compiles/runs with owned map/avatar/
    JSON stand-ins: three output contracts and corrupted-output, changed-Native-
    bytes and failed-IO controls pass. Those model reports explicitly emit
    actual_cdda_sdl=false and fail the actual-Native receipt gate. The first
    full-radius finely varying haze model hit its45s bound and was stopped;
    its partial files are retained. A new output directory uses full-radius
    open/solid/structured owned inputs; production radius, input and collector
    are unchanged. Evidence `night-sight-comparison-model-probe-short/`.
    Actual Native compilation/comparison of these new diagnostic bytes remains
    pending; keep the existing eae full run alive and use a separate diagnostic
    branch, without a local game/input run or new checkout.

40. **Qualify the current short-tap fix on both actual Windows Hosts.** Exact
    eae8d02 run40580475214 emits184-case reports on both1040 and1807. Each
    passes its exact eae Git-object verifier, including all eight new released
    taps and eight W+D cases across keychar/keycode and CPU/GPU. Tap fractional
    distances .137616–.223348 retain zero Native actions and move cost, actual
    presentations and released keys at the declared controlled speed6. Existing
    speed1 held-step controls remain required. Both child processes exit0 with
    no wrapper/debugger error or crash dump; this is an isolated diagnostic,
    not a physical/manual play session.

    Sourceeae8d02 and runtime merge9e8a85a8592cc4219dd31f91f30987d447345a3e
    share treee866c81e3a2acea6cbc30377b6f253c8b4305899, checked against local
    commit objects and remote Git metadata. Only three selected program JSON
    members per artifact12771495265/12769584568 were read,71583/8388608 reserved
    bytes total. Strict summary SHA256
    `51e87dcab9ec8fbeeb85389321936758c430aa10896925473607fd11d8ca4193`.
    CI diagnostics use Code Review only; ongoing job logs were unavailable,
    so exact selected program receipts and passing completed FPS steps support
    this bounded conclusion. Boundary frozen-frame counters are zero, but
    software-CI gaps171–412ms still do not establish15ms or RTX4090 play.
    Full current-source qualification and stage39 Native comparison remain
    pending. GPU/source/runtime and both module jobs at eae pass.

41. **Actual Native comparison revises the exponent-cache decision.** Exact
    f6ac259 diagnostic run40586668131 finishes both short Windows movement
    programs without wrapper/debugger failure or crash dump. Seven selected
    program JSON files per artifact12772249842/12773163479 consume369942 of
    the shared8388608-byte range budget; no archive or CI log is downloaded.
    Exact source/process/Host/module identities and the measured Git-object
    verifier pass. All54 alternating pairs preserve365904 output floats
    bitwise,4756752 protected Native bytes, avatar position/moves/calendar.
    The accompanying movement matrices validate288 actions/144 cache children.

    Cached/original per-cast medians (ms),1040 street/horde/city:
    5.978/5.693,6.551/6.490,7.370/8.169;1807:
    5.779/5.258,6.177/5.856,7.191/7.904. Paired ratio medians respectively
    1.0474/1.0076/.9037 and1.0966/1.0536/.9265. Street worsens in all18
    pairs; horde in16/18, city in2/18. First horde1807 also worsens
    9.584 ->13.733. Stage32's fewer exponent calls therefore do not establish
    a general Native latency improvement. Revisit this retained cache before
    final integration; no favorable-only scene selection or FPS claim.
    Strict summary SHA256
    `1a48df692c4022f6d766ea9f840f9cc895fdd41362d05e94d4c4a0d294315520`.
    These are warmed isolated caster wall times; Microsoft software rendering,
    uncontrolled scheduling and diagnostic pre-matrix work remain limits.
    They do not qualify the complete seen cache, ordinary frames or15ms.

    Both actual Linux jobs at current PR headeae8d02 now pass, confirmed
    through Code Review jobs122458321498/122458321440. Its truncated logs
    contain successful controls/FPS exits; complete job conclusions cover
    the remaining functional sequence. Windows full qualification continues.
    Main PR remains eae; f6ac is only on the existing diagnostic branch.

42. **Investigate exact square-distance dispatch, private candidate only.** At
    the two guarded3D-caster sites one positive delta axis equals the row
    distance, and the other axes lie within it. For the square metric, Native
    `rl_dist` therefore equals the row integer. Circular and generic callbacks
    must retain the original distance call, including callbacks which can
    change the metric. Both pins' complete line/coordinate Git objects match
    (metadata only, no source-body transfer);174285 bytes remain in the
    existing10MiB source budget.

    A private cast-entry dispatch, restricted to all three exact standard
    sight callbacks, passes498 paired grids/466358 checks and226981 exhaustive
    Native numerical deltas. Native distance calls halve10702148 ->5351074;
    exponent calls remain5080030. Plain-bool model fidelity matches Native's
    metric declaration. Seven alternating timing pairs/two casts improve all
    four square-metric medians (ratios.6340/.8399/.9044/.9198), while two
    circular medians worsen (1.0155/1.0239) and circular first mixed worsens
    40.388 ->42.255ms. Earlier per-cell dispatch and reference-alias variants
    are preserved, not rerun until favorable. This model includes owned grid,
    coordinate and fragment context and cannot establish Native gameplay or
    first-frame performance. No production edit is retained at this checkpoint.
    Summary SHA256
    `a96285e38f58f0c057ea246a3f7a33626830446bcb3ff6cfb6641a30848b58ba`.
    Next: assess the exact row shortcut without the cache that regressed
    ordinary Native street/horde, then qualify the selected bounded change.

43. **Replace regressing exponent cache with an exact square-row candidate.**
    Stage41 establishes an actual Native street/horde regression. The source
    adapter now restores the original exponent formula on every request,
    removes the cache from copied Native inputs, and selects the exact integer
    row distance once per cast only for the complete standard sight callback
    triple with square distance enabled. Circular and unknown callbacks retain
    the original live Native distance; public signature, explicit instantiations,
    traversal, floors, range and Native state remain unchanged. The old cache
    helper stays only for historical source-model controls, not the Native copy.

    The complete source adapter still guards both full pinned caster/header
    digests. Reversing every dispatch edit restores the original bytes. The new
    compiled model is checked against the actual emitted full body, with just
    two distance-value observers and owned coordinate/grid/fragment context.
    All501 pairs/466428 checks and226981 exhaustive numerical deltas pass,
    including three custom callbacks which change the metric during traversal.
    Distance helper calls10702148 ->5351074, exact row values5351074; exponent
    requests10702148 ->10702148. All output/input bits, distance values/order
    and generic callback side effects agree. Five separately compiled controls
    (wrong row, forced circular shortcut, missing each of three callback guards)
    all fail the exact output comparison. Both pure math CTests pass in5.99s.

    A separately compiled observer-free private comparison against the original
    formula has square-metric paired median ratios.5297/.7610/.7892/.9045
    for open/wall/haze/mixed. Two circular ratios worsen1.0517/1.0096;
    first circular open4.809 ->5.758 and mixed39.118 ->40.149ms also worsen.
    Background work is uncontrolled and the context is owned: these are not
    Native, ordinary-game or first-presentation performance results. Private
    summary SHA256
    `bb4b9669a7ee5b5489ab2bcfbf0cc038d441926d86a0307aee2fb59ad12fe98b`.

    Release build,300 Python tests(20.215s),96 atomic preparation tests(58.454s)
    and PowerShell syntax pass. Schema2 names the new distance candidate and
    rejects old exponent-cache reports; twelve receipt tests pass. The actual
    collector compiles with the new candidate in an owned model, validates
    three scene contracts and rejects changed output, changed Native bytes and
    failed IO. Model reports remain actual_cdda_sdl=false and fail the actual
    Native gate. Evidence prefixes `night-sight-distance-negative-controls/`
    (summary3dbde1d9689b52cf0a005e5762ce3da84ffc808477d15b5b834c93b7dd9bbc91)
    and `night-sight-distance-collector-model/`
    (summaryf790df444b4ba90cfe5b51f00fc4b6dc9c724d09c9c56503fc0d091b869c887f).
    The new model also joins the existing Linux ASan/UBSan gate. Actual Native
    compilation and same-process comparison remain pending; do not integrate
    a latency claim until those results establish the selected change's scope.
    Keep full PR eae and prior diagnostic f6ac running; reuse the completed
    older sight-diagnostic branch for this bounded next comparison.

44. **Defer blind removal of the Native2D pass.** The exact cached1807
    cata_tiles.cpp blobc4b85c1cab06e23f5633f847a2d1d042969ff361 is verified
    against pinned Git metadata. Its1056-line draw function mutates draw and
    overlay caches, clears temporary terrain/furniture/etc overrides, changes
    map-memory decoration and consumes one-shot hit/line/weather/cursor queues.
    Existing fallback must not consume those queues twice. This is not a
    side-effect-free raster call which can simply be removed for a timing win.
    Metadata proves cata_tiles.cpp,its header and sdltiles.cpp all differ at1040;
    complete1040 CPP files exceed the remaining source budget. Only479 bytes
    of metadata are returned, with12291 conservatively reserved;161994 bytes
    remain in the existing10MiB ledger. No new Native source body is fetched.
    Evidence `night-terrain-raster-source-audit/`. No production edit or gain
    is claimed; separating Native facts/queue consumption from raster submission
    remains a larger follow-up requiring exact two-pin source and fallback proof.

45. **Strengthen Linux qualification and investigate circular distance.** At
    exacteae/merge9e8a85a both actual Linux Native jobs complete all six modes
    (controls/camera/FPS/held/target/gameplay), twelve exit0 processes without
    timeout. Every process row matches the Code Review-only log receipt despite
    truncation of the complete logs. Each source receipt independently matches
    101 copied inputs against the exact eae Git tree. Patched Native translation
    units are not independently reconstructed by that copied-input check.
    Four actual184-case matrices (FPS and gameplay at each pin) pass their exact
    eae verifier, including released taps and diagonal chords. Both artifacts
    12773991938/12773662355 are read through selected program JSON ranges only,
    2202642/8388608 reserved bytes. Strict summary SHA256
    `f2179f3ddd0729b625474e42a276bc8f6338c72f598fcb1f8ecec2d8e0cf90f9`.
    Software Vulkan/functional results do not qualify RTX/15ms or stage43 bytes.

    Private negative-offset testing preserves the first full-radius varying-haze
    probe's20s timeout. A second bounded open/solid/wall model retains the same
    four negative offsets and passes24 pairs/528 checks, both metrics, exact
    outputs/input bytes/distance order. It is not a performance result.
    Summary692d88fa6ff2c973fd0c2a8034629bb7325aba68732cba8fb04c635cacf0ad15.

    A separate private circular candidate uses an immutable10801-byte table of
    exact integer distances, with original generic/out-of-range fallback.
    All498 pairs/693339 checks and226981 exhaustive actual-Native numerical
    deltas agree, including Native's double-to-float-to-int rounding. Standard
    helper calls10702148 ->0; exponent calls remain10702148. Seven alternating
    observer-free timing pairs improve all eight paired median ratios: square
    .6049/.8529/.8784/.9160; circular.7245/.8252/.8301/.8884. All eight first
    samples improve in this model. The table compiles under default constexpr
    limits and adds no first-use runtime initialization. Raw/summary/source
    hashes stay private in `night-sight-circle-table-model/`, summary SHA256
    `6847ea3365bc99f9d08d4c076798d5ac2921ce9d69cec4ca84a5d4bf3dc8d48d`.
    Owned coordinate/grid/fragment context, background scheduling and lack of
    Native execution prevent a game-performance claim. No table production
    edit is retained at this checkpoint. The current Native comparison does
    not record which distance metric was active; add explicit metric identity
    and preservation before attributing any shortcut's measured effect.

46. **Prepare exact circular distance for the standard sight path.** Extend
    stage43's compile-time dispatch with an immutable10801-byte table, restricted
    to the exact standard callback triple and nonnegative bounded offsets.
    Exhaustive Native numerical comparison covers all226981 supported XYZ
    deltas, including double-to-float-to-int rounding. No first-use allocation,
    initialization or Native pointer is retained. Unknown callbacks and circular
    offsets outside the table domain keep original live Native distance calls.
    The public signature, traversal, exponent formula and output remain intact.

    The actual emitted-body model passes525 pairs/693961 checks. Standard
    helper calls10702148 ->0, split5351074 exact rows/5351074 table results;
    exponent requests remain10702148. Twenty-four negative-offset cases and
    live-metric generic callbacks protect fallback. Seven separately compiled
    bad variants (old five, wrong table and missing offset guard) all fail.
    Control summary6773d4271003c4302892aac752897f4c77826fc34c1b013eac5b88b2725cad1b.
    This is exact-value source-model evidence, not Native performance.

    Schema3 records the actual distance metric, selected standard shortcut and
    metric preservation at each pair boundary. Fourteen receipt tests reject
    old square-only/exponent-cache reports and inconsistent metric identity.
    The collector never writes the metric. An owned model tests both metrics
    across six scene contracts plus changed output, Native bytes, failed IO and
    persistent metric mutation. The first mutation control toggled eight times,
    restoring its state at the observed pair boundary; that failed-control
    output is retained. Its corrected persistent-change control is rejected
    even with identical outputs/Native bytes. Model actual_cdda_sdl=false is
    still rejected by the Native gate. Collector summary
    f52228ba65578046d75ef552672e3be6270988d9b5655669ab6040af569efa1f.

    Full Release,302 Python tests(19.154s),the new math CTest(3.16s),PowerShell
    syntax and96 atomic tests pass. The first atomic attempt has61 temporary-
    write ENOSPC errors; the physical C: volume subsequently reports67GiB free.
    Repeating with temporary files on the existing G: build drive passes all96
    tests in36.370s; final include-order validation repeats all96 in36.478s.
    No user files or measured evidence are deleted. Hardware
    rendering is unchanged and its previous hidden render-only checks are not
    rerun. Actual Native compilation and schema3 timing remain pending.

47. **Stage43 actual Native timings do not establish a useful gain.** Both
    exact7dd6365 short Windows programs exit0 without wrapper/debugger error or
    dump. Seven selected JSON members per artifact12776488290/12777387677 use
    367196/8388608 reserved bytes. The measured7dd verifier is loaded from its
    Git object and independently matched to that revision's frozen source hash,
    not the now-changed working verifier. All54 pairs retain exact output and
    protected Native bytes; accompanying matrices validate288 actions/144 cache
    children. Summaryda8526b3334b4e5c84f87be0bbe85c8e7985a6348a94f720bbe1e62a5e1bbd55.

    Candidate/original medians(ms),1040 street/horde/city:
    5.734/5.691,6.490/6.476,8.220/8.235;1807:
    5.731/5.780,6.403/6.450,8.164/8.259. Paired ratios respectively
    1.0057/1.0030/1.0029 and.9914/.9943/.9854. First city1040 worsens
    14.668 ->24.320ms. These near-unity warmed-caster results neither establish
    a material benefit nor meet the first-frame goal. Schema2 lacks the active
    metric; shortcut selection and component attribution cannot be inferred.
    Stage46 addresses that diagnostic gap and adds the circular candidate.
    Independent software action times remain mixed and are not a causal frame
    comparison. Keep the full eae and7dd runs; new table bytes need their own
    Native compilation/comparison before any integration/performance claim.
    The earlier f6ac integration run40586668131 now finishes all seven actual
    Host/module/planning jobs successfully; its final required gate fails
    solely because the selected integration tier is insufficient for full
    qualification. That completed diagnostic branch can be reused without
    cancelling its functional work.

48. **Audit complete controlled Native turns before blaming AI.** Exact f6ac
    gameplay/qualification artifacts12775734174/12777336904 supply only three
    selected program JSON files each,238637/8388608 reserved bytes. Audit,
    gameplay provenance and release receipts share the exact source/Host/module
    identities already checked by the Native sight comparison. Each pin really
    invokes26 do_turn calls: six rounds each with1/5/20 enemies and eight empty
    turns. All actions succeed and every combat group observes enemy attacks.
    Avatar protection, stamina/state resets and isolated fixture exclude normal
    gameplay balance. Strict summary SHA256
    ae85b8e1b5e2976a6ac1c122269cfc258896b4f9820c6c66d648a7feb1a2952a.

    Empty/combat medians(ms) are114.361/230.598 at1040 and143.819/251.292 at1807;
    combat p95433.604/277.034. These inclusive wall scopes can include rendering,
    UI, waits and scheduling; they do not measure pure AI. Code selectsCPU before
    the empty-turn block; combat merges all three group sizes. Existing reports
    lack per-turn renderer/callback counts and separate group distributions.
    The unconditional legacy controlled_without_ai_turn_loop flag cannot prove
    absence of controlled AI turns. Ordinary default remains Auto: hardwareGPU
    on a supported device, CPU for software adapters. Legacy projectile drawing
    and artificial animation delays are already bypassed in FPS presentation;
    no new animation-delay fix is justified by this audit. Next priority: bound
    perspective rendering inside each actual do_turn without changing simulation.

49. **Retain a measured Native circular-caster improvement, with first limits.**
    Exact12a7 run40600292716 produces successful short Windows programs on both
    pins. Schema3 confirms circular distance and the exact-table path. Both
    processes exit0 without wrapper/debugger error or dump;54 alternating pairs
    preserve all365904 output floats,4756752 Native bytes, position/moves/time
    and metric at each pair boundary. Accompanying movement matrices validate
    288 actions/144 cache children. Seven selected JSON members per artifact
    12779685982/12779801403 use366765/8388608 reserved bytes. The exact12a7
    verifier matches its frozen Git source identity. Strict summary SHA256
    392c053c4419ab2fcdbe13a50408c69d33567ab32f0e6a74c75d95c9e660c13a.

    Original -> candidate per-cast medians(ms),1040 street/horde/city:
    4.6542 ->3.9230,5.4176 ->4.5155,6.4740 ->5.2764;1807:
    5.7838 ->4.7483,6.4826 ->5.2877,8.2441 ->6.7237. Paired ratio medians
    .8454/.8327/.8123 and.8199/.8201/.8187 support about15–19% improvement
    **only in this warmed Native3D-caster wall scope**. Four of54 individual
    pairs regress. First1807 horde worsens9.462 ->14.767 and city16.157 ->16.272;
    first-sample latency and the15ms complete-frame target remain unqualified.
    Each first number averages eight casts and is not a first game action.
    SoftwareD3D12/WARP and scheduling remain limits; independent whole-action
    timings are mixed and cannot be used as a causal ordinary-frame comparison.
    This directly supports the circular candidate; full12a7 required qualification
    continues, while the existing exact-eae full run is preserved.

**50. Attribute perspective callbacks inside real controlled Native turns.**
Stage48's inclusive `do_turn` samples cannot distinguish simulation from UI,
rendering or waits. A stack-owned opt-in capture now surrounds the same18
combat and8 empty Native turns. It records raw nanoseconds, requested renderer,
actual successful CPU/GPU/legacy callbacks, skipped attempts and ancestry per
turn. Only outermost callback intervals enter the draw union; the exact
remainder is labelled other Native/UI/wait/scheduling work, never pure AI.
The ordinary draw path has no active capture, clock or recording allocation.
The original11301-byte draw implementation remains byte-for-byte unchanged
(SHA256 `02efcabefa2fc999f0facac8fa1bdeec1557f54228f1dd0e94aebdd3542bc4c9`).
No Native turn, warm-up, renderer setting or simulation call is added/removed.

The old unconditional provenance field `controlled_without_ai_turn_loop` was
misleading: real controlled AI turns already ran. Replace it with the explicit
controlled-fixture descriptor and reported Native-turn sample count. A strict
verifier rejects missing/duplicate turns, backend/count inconsistencies,
double-counted partitions, partial captures and non-finite/invalid values.
Local checks: full Release build;310 Python tests(19.406s);96 atomic source
tests for both pins(35.028s), including global TLS/header receipts and atomic
failure paths;2 affected CTests(0.32s),20 new clock/ownership/allocation checks.
Three independently compiled bad controls (nested double count, accepted
backwards clock, per-translation-unit TLS) are rejected; retained code passes.
PowerShell was parsed only. No local game, SDL/input harness or user input ran.
Actual Native timing for this new diagnostic remains pending isolated CI.
The first dispatch of `d6098b4` was rejected with HTTP422 because the newly
added workflow step had an extra indentation level. Correct its step/run
indentation before retrying. This did not launch Native jobs or affect the
separate full runs for `eae8d02` and `12a7abc`; source checks above remain valid.

## Actual remote evidence and next priorities

**Actual cache children at exact `043fb4d`, diagnostic run40547452414.** Both
early Windows movement processes exit0 without dumps or wrapper/debugger errors.
Dispatch runtime equals source`043fb4d349799da87cc1d5afddd7c30d94aa704a`,
tree`e9b091409810816cc6454ae2fd8843350f67fbbc`. All288 action receipts and144
cache-builder children pass the new strict validator, including exact ancestry,
Z/lightmap flags,12 boundaries, containment and signed parent gaps. Four JSON
members per artifact12757812571/12757553539 reserve361879 bytes /8 MiB. Private
receipts: `night-movement-043fb4d/`; cache-summary SHA256 values1040
`b6f66e5c9e35aa4c29e2fc894bbe683d8c9c466d79b154f8ad7a563053799e3f`,
1807 `133dad18ccb0e2b3b5a9066cd221c4efbbb8c966dc513a311934370a7dfb21cd`.

640x480, VSync1, software D3D12 **Microsoft Basic Render Driver**, six crossings
per row; independent medians in milliseconds do not necessarily sum:

| Pin / scene | Main cache parent | Structure | Vision transparency | Seen update | Lightmap |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1040 street | 16.260 | 5.481 | .688 | 6.754 | 1.974 |
| 1040 horde | 17.909 | 5.887 | .765 | 8.529 | 3.123 |
| 1040 city | 23.265 | 5.760 | .752 | 12.578 | 3.923 |
| 1807 street | 17.547 | 5.380 | .623 | 8.796 | 2.611 |
| 1807 horde | 18.982 | 5.405 | .617 | 9.687 | 3.385 |
| 1807 city | 23.488 | 5.445 | .648 | 13.190 | 4.086 |

Structure combines outside/transparency/floor builders. Seen update includes
build_seen_cache, the unchanged position/range publication and tile draw-cache
invalidation; it is not a separately timed shadowcaster. Vehicles cost.0015–
.0026 ms, character-light state.0053–.0094 ms: neither is the next target.
Main parent/child median gaps remain.00075–.00155 ms. The next investigation is
the seen-update block and its 3D shadowcasting, preserving all ranges and outputs.
All four new actual Hosts compile and both module jobs pass; full functional
diagnostic jobs remain pending at this checkpoint. The deliberately selected
integration tier cannot qualify the required full tier. These diagnostic scopes
are not paired gains, isolated on-core/GPU execution, ordinary RTX4090 frames
or action-to-first-correct-present;15 ms remains unqualified.

**Actual phase-scan revision `1bdb0cd`, run40534567570.** Both early Windows
programs exit0 without wrapper/debugger error or dumps. Runtime
`e5641e79f4e9d398468a5cb8e014eade4560a138` shares saved source tree
`9c1cab08a53c2740cf3801098f512b482d4f2473`. Four selected program JSON files
per artifact12749968461/12751251676 reserve351322 bytes /8 MiB. All288 action
scopes and144 actual shift children pass strict validation. Private receipts
and independently hashed summaries: `night-movement-1bdb0cd/`; 1040 summary
`eacbc97f6b056c9641629930b0321b661cef3f665ecb6d52d38cf0c3d2570468`,
1807 `e5403c15babd5248739602c8de7e9c1693ced868564a15892dea90898dda68e7`.

640x480, VSync1, **software D3D12 Microsoft Basic Render Driver**; six
crossings per row. Independent medians in milliseconds do not necessarily sum:

| Pin / scene | Action | map::shift | Copy/load | Actualize | Main cache build |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1040 street | 26.084 | 11.288 | 3.277 | 5.508 | 13.454 |
| 1040 horde | 48.411 | 18.014 | 3.572 | 5.816 | 19.563 |
| 1040 city | 46.848 | 12.806 | 3.648 | 5.790 | 23.625 |
| 1807 street | 30.628 | 11.693 | 3.076 | 5.604 | 17.482 |
| 1807 horde | 31.064 | 11.710 | 2.970 | 5.539 | 17.764 |
| 1807 city | 37.018 | 13.125 | 3.066 | 5.605 | 21.837 |

Copy/load medians are lower than the preceding independent capture's7–9 ms,
but this is not a paired causal timing experiment. 1040 horde/city whole-action
medians worsen;1040 city actualize reaches72.586 ms and1807 city main cache
86.196 ms. Spawn also has a19.468 ms horde outlier. The main cache call remains
a justified next profiling target; forced-refresh cache medians4.375/6.693/
7.849 ms (1040) and5.408/5.708/7.745 ms (1807) must remain separate. Current
module, GPU Windows/Linux/Vulkan/sanitizer, source/package and runtime jobs
pass, and all four pinned Hosts compile. Full Native gameplay/appearance jobs
are still running at this checkpoint. No new full Native pass, ordinary
RTX4090 frame time, first-correct-present or15 ms result is claimed.

**Current stair regression readbacks at the same `1bdb0cd` tree.** Artifacts
12750617022/12749498670 contain successful exact-runtime CPU-raster diagnostic
processes. Three program JSON members per pin and only four selected BMPs
across both pins reserve328974 bytes /8 MiB, including metadata. Full native
physical/memory/overmap ledgers, hidden furniture/contents/focus and repeated
normal input/pixels remain unchanged in every day/dusk/night record.

1807 night again verifies retained geometry without BODY: normal_static equals
normal_after pixel-for-pixel; removing only navigation semantics changes106
triangles and29741 ROI pixels. The normal flight/rails and their removal control
were visually inspected. Both normal night BMP hashes remain exactly the
previous `1a951e1` hashes:1040 `84b026e7bf4ce9c433ce4272661fda78971a7fcd8e5278d73047a01299505872`,
1807 `25177b5c18781cb28d7cee679a6aa310e6793de45efd1b11767fb0e9adcd782a`.
1040 remains causally inconclusive because BODY is already visible; this is
not a general visibility or performance qualification. Private receipts and
lossless readback previews: `night-static-stair-1bdb0cd/`; independent summary
SHA256 `c7aefa84844a9547aecd01d8d0d8ae6c903424cd5735ee59f431ad5144cedf79`.
No local Native game was launched and no renderer suite was rerun.

**Completed Linux qualification at exact `1bdb0cd`.** Both actual Native jobs
[1040/122299656818](https://github.com/Neversalimus/NCMM/actions/runs/40534567570/job/122299656818)
and [1807/122299656905](https://github.com/Neversalimus/NCMM/actions/runs/40534567570/job/122299656905)
pass in full. Controls, camera, FPS, held, target and gameplay each exit0
without timeout: all12 reported process rows were read through Code Review,
whose120000-character job views are truncated. The successful full job
conclusions are separate from that bounded log coverage.

Four selected program JSON files per artifact12753581519/12752868249 reserve
2197611 bytes /8 MiB;740/781 MB archives were not downloaded in full. Both
completion summaries name runtime`e5641e79f4e9d398468a5cb8e014eade4560a138`
and the correct engine pin; their six process records match the Code Review
rows exactly. Both share the already-proved source tree. All99 copied shared
inputs per pin, including actualize/phase/support/cache wrappers, match exact
`1bdb0cd` Git objects after the bridge's universal-newline handling. Patched
Native translation units were not independently reconstructed by this check.
Private completion/source receipts: `night-linux-1bdb0cd/`; summary SHA256
`727d5b08ec1c897b4e9628d13b0fd90e87c966cc3011ded2cd56cc90d708962f`.
This is software-Vulkan functional qualification through stage30, not a
hardware timing or new-stage31 pass. Windows1040's longer gameplay/UDP step
also reports success; full Windows/main qualification is still pending.

Stage31 diagnostics and the following stair-evidence note are separately
committed as`493c175`/`043fb4d`. An isolated integration dispatch
[40547452414](https://github.com/Neversalimus/NCMM/actions/runs/40547452414)
at exact`043fb4d` runs on branch`codex/fpv-night-cache-attribution-20261027`
to obtain cache child receipts while the full PR run remains undisturbed.
The requested integration tier is insufficient for final full qualification;
its diagnostic output must not be represented as a full required-tier pass.
Its new Native compilation/measurements remain pending at this checkpoint.

**Actual underground-loop revision `aff7dd7`, run40525583050.** Both early
Windows programs exit0 without wrapper/debugger error or dumps. Runtime
`ce3d5328b61c3906ffd15e04306a934f08eb72bc` shares the saved source tree
`d70642062c20a8fb9e1b85bdbcf781f43ddd101c`. Four selected JSON members per
artifact12747006628/12746137910 reserve350518 bytes /8 MiB. All288 action scopes
and144 actual shift children pass the unchanged strict verifier. Private
`night-movement-aff7dd7/` summaries SHA256:
1040 `eb62d4f27ee3e502d44a3645d0693743dd886e8a6c1f2e28419f4d38bbfc2557`,
1807 `01aefbc9b84bbf2bc04d894836084ea5176a287b0163927ed057260c51041312`.

Software D3D12 Microsoft Basic Render Driver,640x480,VSync1; median ms,
six crossings per row. Independent phase medians MUST NOT be summed:

| Pin / scene | Crossing action | Inclusive shift | Grid copy/load | Support rebase | Actualize | Reconcile |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1040 street | 32.058 | 15.736 | 7.361 | .858 | 5.272 | 1.363 |
| 1040 horde | 36.374 | 15.677 | 7.368 | .927 | 5.741 | 1.534 |
| 1040 city | 45.627 | 16.528 | 7.525 | 1.601 | 5.750 | 1.490 |
| 1807 street | 42.512 | 19.072 | 8.742 | .991 | 7.211 | 2.029 |
| 1807 horde | 43.948 | 19.152 | 8.592 | 1.086 | 7.313 | 2.011 |
| 1807 city | 53.714 | 20.596 | 8.699 | 1.957 | 7.406 | 2.124 |

Observed actualization medians are5.27–7.41 ms, compared with7.26–10.84 ms in
the preceding independent capture. The work-count model is causal; these
different CI runs are not a paired Native gain. Copy/load becomes slower, and
all1040 crossing-action medians worsen despite lower actualize medians. 1807
city actualize p95 remains30.531 ms. Both source trees are independently proved;
their equality to each runtime does not make the two workloads/machines a paired
experiment. Stage30 still requires its own Native compilation and timings;
ordinary RTX4090 FPS, scanout and15 ms first-correct-game frames are unqualified.

**Actual node-transfer revision `2b62a69`, run 40517335063.** Both early
Windows programs exit0 without wrapper/debugger error or dumps. The source and
runtime `57fa820f6f05407fc7e2c1dd7b15b99689673b5f` share Git tree
`804bd3aec63294d262e074dc65c72b0995d1cd47`. Four program JSON members from
each artifact 12741853595/12742108555 reserve 351232 bytes / 8 MiB; all 288
action scopes and 144 shift children pass the unchanged strict verification.
Private `night-movement-2b62a69/` summary SHA256:
1040 `3a7267a7080a61f7151378d7fd5237442e8f5162a85db41eb04dfe283731673a`,
1807 `b498b85f646dcc58011e83af10959529a766824841bfa4301b692cf72abe1915`.

Software D3D12 Microsoft Basic Render Driver, 640x480, VSync1; median ms,
six crossings per row. Independent medians MUST NOT be summed:

| Pin / scene | Crossing action | Inclusive shift | Grid copy/load | Support rebase | Actualize | Reconcile |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1040 street | 28.477 | 15.658 | 5.895 | .808 | 7.256 | 1.484 |
| 1040 horde | 33.999 | 16.992 | 5.965 | .876 | 8.163 | 1.833 |
| 1040 city | 44.031 | 18.030 | 5.989 | 1.502 | 8.471 | 1.854 |
| 1807 street | 53.169 | 19.924 | 7.119 | .870 | 10.176 | 1.652 |
| 1807 horde | 42.785 | 20.494 | 7.530 | .944 | 10.220 | 1.674 |
| 1807 city | 58.567 | 23.692 | 7.618 | 1.611 | 10.837 | 1.699 |

Support medians are .81–1.61 ms in this capture, compared with 1.22–2.79 ms
in the previous independent capture. The allocation control is causal; these
separate CI machines/runs are not a paired timing experiment. In particular,
1807 street/horde crossing action medians worsen despite lower support times.
1807 city actualize p95 remains65.582 ms, copy/load p95 20.010 ms. Ordinary
support-queue cardinality remains unmeasured. Both module jobs, GPU Windows/
Linux/Vulkan sanitizers, source/package and runtime pass at exact2b62a69;
all four complete Native jobs were still running before the next source push.
This does not qualify stage29, ordinary hardware FPS or first-correct-present.
Next: validate stage29 in the actual engine; investigate copy/load's terrain
phase-change scan without omitting original-terrain restoration or freezing.

**Actual child intervals at exact `7c0a8b9`, run 40508748878.** Both early
Windows programs exit 0 with successful provenance and no wrapper/debugger
error or dump. Runtime `72e71bc46271c77d642f5c3b5b1a848ec4b3c565` and source
share tree `091aa10ea921b4801ec3fea623514164c0df6f79`. Four selected program
JSON members per artifact 12737019895/12737399271 reserve 350923 bytes / 8 MiB.
All 288 action scopes and 144 actual child calls pass the strict parent, clamped
direction, phase, containment and conservation checks. Private summaries under
`night-movement-7c0a8b9/` have SHA256
1040 `a4a3d17f457b33fe6d5072deeb6ebf05704f851fee90dcbea8cf4ed6bcb9793e`,
1807 `ce59102875d797bfbe10aea78daace29ca54ac782f88d19e4100c566d7974db6`.

Software D3D12, Microsoft Basic Render Driver, 640x480, VSync 1: independent
median milliseconds, six crossings per row. These medians MUST NOT be summed:

| Pin / scene | Crossing action | Inclusive shift | Grid copy/load | Support rebase | Actualize | Reconcile |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1040 street | 35.470 | 18.974 | 6.518 | 1.285 | 9.171 | 1.295 |
| 1040 horde | 41.980 | 20.848 | 7.451 | 1.542 | 10.150 | 1.615 |
| 1040 city | 47.804 | 22.256 | 7.478 | 2.722 | 10.227 | 1.568 |
| 1807 street | 33.843 | 17.237 | 5.912 | 1.218 | 8.296 | 1.797 |
| 1807 horde | 35.344 | 17.573 | 5.850 | 1.379 | 8.470 | 1.779 |
| 1807 city | 83.179 | 50.631 | 7.841 | 2.795 | 8.546 | 1.814 |

1807 city peaks hit different phases on different crossings: actualize p95
69.272 ms, copy/load 35.678 ms, support 10.385 ms and reconcile 7.156 ms. Per-action
child sums DO conserve the inclusive parent; median signed parent gaps are
.0009–.0015 ms. Bitset/vehicle cache rebasing is only .023–.035 ms median here,
so source-only speculation about that loop is deprioritized. The Native
transparency builder already bulk-resolves uniform submaps; no duplicate
optimization was added. Actualization includes gameplay callbacks and is not
skipped for uniform storage or zero elapsed time.

The support queue's ordinary-game cardinality is not measured; this no-turn
fixture can retain construction work in that queue. New 1040 crossing times
are worse than 311d717's separate run despite no new production optimization
between those sources. These results guide attribution, not a paired gain,
ordinary RTX 4090 FPS or a 15 ms claim. At that checkpoint stage28 still needed
the subsequent capture recorded above.

**Fresh cache-stage evidence at exact `311d717`, run 40501372289.** Both pinned
Windows Hosts compile, and their early movement programs exit 0 with successful
provenance and no wrapper/debugger errors or dumps. Runtime merge
`352f056b208e05e8ae98a53bc9b57c55fd610e24` and the saved PR commit share tree
`408ac1edceabadcc3ac48db816d6d7cf96c6f7c5`; this comparison uses Git objects,
not the subsequent diagnostic worktree. All 24 records / 288 actions conserve
their strict adjacent update_map scopes and rebases. Actual new child scopes
do not exist in this revision; their absence remains explicitly unavailable.
Four program JSON members per artifact were read from 12733459198/12734059117,
with 340348 bytes reserved against the shared 8 MiB cap. Private:
`night-movement-311d717/`. Independent submap summaries have SHA256
1040 `7dd34f941b80ffbe84454f62f601e2355137eff9863bc3bb934d45dd079f8904`,
1807 `e6d680e13e39160a0209a38e004364eb03abce90e14718bdcac1f5cbcc82e189`;
combined scene summary
`168bef63fd72d60907af62213187f34188db6afdcac67282f58a89c1f96dd106`.

640x480 GPU mode, VSync1, **software D3D12 Microsoft Basic Render Driver**;
medians in ms, six crossings per row. Independent medians need not sum:

| Pin / scene | No shift action | Crossing action | map::shift | build_map_cache | Crossing first-ready (p95) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1040 street | .339 | 22.055 | 11.901 | 8.928 | 42.088 (54.697) |
| 1040 horde | .345 | 27.828 | 13.265 | 13.155 | 52.043 (68.506) |
| 1040 city | .369 | 31.202 | 14.992 | 14.496 | 110.667 (152.497) |
| 1807 street | .385 | 40.005 | 20.142 | 18.043 | 86.560 (108.580) |
| 1807 horde | .386 | 41.902 | 20.478 | 19.773 | 114.312 (123.211) |
| 1807 city | .395 | 47.454 | 22.160 | 23.384 | 134.682 (180.299) |

The older 1040 crossing medians below were 40–55 ms; these are separate CI runs,
not a controlled paired A/B gain. 1807 city still has crossing-action p95
109.201 ms and build_map_cache p95 82.158 ms. The unchanged diagnostic refresh
explicitly invalidates visibility; the benchmark does not advance simulation turns.
First-ready includes actions, forced refresh and synchronous render; it is not
scanout/present latency or ordinary RTX 4090 play. No 15 ms claim follows.
Next: use the bounded child capture to distinguish actualization/reconciliation,
copy/load and cache rebasing within the still-expensive shift; retain the
separate cache-build and real-render costs.

**Submap attribution at exact `7f0ea25`, run 40481073067.** Both Windows
movement programs exit0 with successful provenance, no wrapper/debugger error
or dump. Runtime merge `d55d606928b7d99b49742f70e17ffebc3e84400d` and PR head
have identical tree `0150cff20dde6bd06426e69aab92b2769f7318e9`; both engine
pins match. Only four program JSON members per artifact were read, with 338681
bytes reserved against an 8 MiB cap, including metadata reservation. Private
receipts: `night-movement-7f0ea25/{12724516435,12724581265}/`.

All 24 records / 288 steps have strict complete inner receipts: exactly one
`update_map` invocation per accepted action, exact rebases and cross-action
coordinates, no nesting/reuse/drop/exception/protocol error, and all adjacent
outer durations conserved (maximum recorded residual0). Inner durations are
contained in the measured move, not added to it. Historical `1a951e1` is
correctly rejected as lacking these inner receipts.

640x480 GPU mode, VSync1, **software D3D12 Microsoft Basic Render Driver**:
independent medians in milliseconds (they need not sum):

| Pin / scene | No shift action | Crossing action | Inside update_map | map::shift | build_map_cache |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1040 street | .397 | 40.572 | 40.209 | 21.958 | 17.137 |
| 1040 horde | .410 | 50.488 | 50.098 | 22.654 | 25.619 |
| 1040 city | .404 | 55.360 | 54.938 | 23.667 | 29.107 |
| 1807 street | .399 | 50.976 | 50.576 | 21.876 | 27.629 |
| 1807 horde | .380 | 50.652 | 50.263 | 22.184 | 27.258 |
| 1807 city | .378 | 60.947 | 60.557 | 23.440 | 32.493 |

Rebase/NPC medians are .18-.21 ms; invalidation .003-.006 ms; spawn .67-.83 ms;
overmap .12-.15 ms; action time outside update_map .36-.42 ms. City cache
tails remain large: 1040 p95/max89.424/90.138 ms, 1807 p95/max98.158/117.101.
1807 city spawn has a 36.282 ms maximum despite its .822 ms median. This
selects submap shift and cache construction for further investigation. It
does not justify skipping actualization, item/craft reconciliation, spawn or
discovery. It is diagnostic CI evidence, not ordinary RTX4090 play, paired
before/after improvement or a measured floor-cache duration.

Independent strict summary SHA256: 1040
`5277cbe869ab4a47cd810d0cb15fe2555e8845a29788ea8746da139cdbf76317`;
1807 `ff95351752ed01be215de9d97592142d2544a438a2309d512de5b80d2a4b7453`.

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
scene. Do not infer absence of other stair defects from 1040's already-visible
case.

**Actual post-fix Native evidence at `1a951e1`.** Runtime
`d42ff81c8653f7e8c112014e0ba8f77bf82135b2` has the same tree
`35dff0b96f281bf19c3f358457f4866f072b6aff` as that PR head. Both engine pins
and process receipts match (exit0, no wrapper/debugger errors or dumps).
Bounded selected JSON/BMP reads: `night-static-stair-1a951e1/`, independent
summary SHA256 `33419a005f410a51c74e5e6f30ea10702a1062f14a371f9ffe93bfbb0b9a017b`.

- 1807 night: schema2 **retained_geometry_verified**. Normal flags 278945 carry
  BODY=0, STRUCTURE, UP and CONNECTED_UP. Removing only UP/CONNECTED_UP and
  direction changes 23138 -> 23032 triangles and 29741 ROI pixels; real normal
  and untouched owned static controls match byte-for-byte. All N/D/N and pair
  physical/memory/OMT ledgers and protected/source digests match. Normal RGB
  .365478 stays fixed; debug .390478 is separately recorded. Actual normal,
  static and NAV-removed images were inspected. Normal/static BMP SHA256
  `25177b5c18781cb28d7cee679a6aa310e6793de45efd1b11767fb0e9adcd782a`.
- 1040 remains honestly inconclusive (BODY already visible at all three light
  levels); its actual night BMP matches the preceding 1040 image. Selected
  readbacks total four images across both pins, not all diagnostic images.
  Shared reserved transfer is 460049 bytes / 8 MiB. This proves the controlled
  stair correction, not a general visibility qualification or frame-time gain.

**New adjacent movement phases at the same exact tree.** Early artifacts
12709744679 / 12711130391 contain 12 records each, 12 accepted steps per record
(288 total), matching engine/Host/module/process provenance. Every adjacent
phase receipt conserves its original first-ready time with zero reported
accounting residual. Private summaries are under `night-movement-1a951e1/`;
their SHA256 values are `1d13d5c23b5c604826ec4e847b3b2b8732082c6557323d20b71736fc7b170772`
and `67fec8f0307e1706d71952a0b91a1b4847c4c93398cfa8cf18cfed91f6964d2a`.
Only selected program JSON was fetched (323755 reserved bytes / 8 MiB).

GPU640 medians in milliseconds, software D3D12 **Microsoft Basic Render Driver**,
VSync1, same 12-step isolated route:

| Pin / scene | Action | Build map cache | Update visibility | First render | First ready (p95) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1040 street | 19.20 | 5.32 | 12.56 | 17.37 | 62.09 (84.28) |
| 1040 horde | 25.69 | 6.76 | 20.46 | 21.73 | 82.34 (119.90) |
| 1040 city | 26.20 | 8.11 | 19.36 | 41.13 | 110.83 (164.76) |
| 1807 street | 24.60 | 5.86 | 19.50 | 28.97 | 87.46 (119.72) |
| 1807 horde | 25.97 | 6.48 | 19.68 | 42.45 | 108.47 (141.14) |
| 1807 city | 28.34 | 8.21 | 26.11 | 52.39 | 136.77 (242.76) |

These are synchronous diagnostic wall scopes, not on-core CPU, GPU execution,
ordinary play or paired before/after gains. The benchmark's existing refresh
explicitly invalidates visibility; its cost cannot be assigned wholesale to
ordinary movement. Render-call contains the first render; bookkeeping medians
are .0026-.0031 ms in these records, not the earlier unmeasured large gap.
Independent phase medians need not sum to the median total. Actual actions and
Native visibility are justified next investigation points; 15 ms is unproved.

**Boundary attribution from these receipts.** All 24 records have two action
populations. GPU640 even/no-shift vs odd/crossing medians: 1040 street
.372/40.489 ms, horde .391/55.780, city .374/57.032; 1807 street .369/55.867,
horde .364/66.219, city .379/59.392. The 19-28 ms pooled medians above lie
between those populations. Exact fixture origin/route and Native `[60,72)`
central bubble bounds imply an alternating submap shift at Y=59/60; internal
shift timings have not yet been captured. AUTOPEEK is disabled in this fixture.
The action contains shift, actor rebasing, all-Z invalidation, cache build,
spawn and overmap discovery. Private analysis of 288 steps:
`movement-native-parity-analysis.json`, SHA256
`d129581615146fa2128bd02fa15378b91136c7f89c35bc28acd9ec8c645f3ff3`.
Next: adjacent scopes around those unchanged calls, then choose an optimization.
Do not skip actualization, item reconciliation, spawn or discovery from source
inspection alone; they have Native gameplay side effects.

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

At preceding `2ce404d`, [main run](https://github.com/Neversalimus/NCMM/actions/runs/40441923163)
passed Windows/Linux module jobs, both pinned Windows/Linux Host compilations
and both actual Linux Native functional jobs. Windows qualification was
superseded/cancelled, so no full Native pass is claimed.
[GPU Windows/Linux/Vulkan/sanitizers](https://github.com/Neversalimus/NCMM/actions/runs/40441923119),
[source/package](https://github.com/Neversalimus/NCMM/actions/runs/40441923114),
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40441923116) pass.
These results do not qualify subsequent stage15/17/18 Host or renderer bytes. The failed
`21b3878` Native compile and its repair are retained above; older ddfa/fc2
passes also do not qualify new bytes.

Pushed `1a951e156562478ae1632857c207f5768ca5084e` has passing
[GPU Windows/Linux/Vulkan/sanitizers](https://github.com/Neversalimus/NCMM/actions/runs/40448928683),
[source/package](https://github.com/Neversalimus/NCMM/actions/runs/40448928654),
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40448928727).
Its [main run](https://github.com/Neversalimus/NCMM/actions/runs/40448928678)
compiled all four actual Hosts. Both Windows early actions/stairs/movement
steps passed, but both longer Creature/appearance/gameplay smoke jobs later
failed (exit191), so the main run and final required qualification failed.
Both Linux controls
failed (exit191, no timeout): their program smoke reports
`multilevel_real_sdl_render_failed`, after 12 complete multilevel records through
stairs CPU/GPU and before stairs_down. Selected program JSON receipts are in
`night-linux-1a951e1/{12710699779,12711496525}/`; CI logs were read only through
Code Review. The 1040 source receipt independently matches 84 copied shared
inputs at exact `1a951e1` with the bridge's universal-newline handling; patched
Native bodies were not independently reconstructed by that comparison.
Stage20 fixes the demonstrated observer contradiction and adds precise failure
forwarding; its connection to those exits remains a hypothesis until rerun.
No full Native pass is claimed for that older run.

At exact `7f0ea25`, [main run40481073067](https://github.com/Neversalimus/NCMM/actions/runs/40481073067)
compiles all four pinned Hosts. Both module jobs and Windows early
actions/stairs/movement gates pass. **Actual Linux Native1040 job122111496632
and Native1807 job122111496600 pass in full**: controls, camera, FPS, held,
target and gameplay each exit0
without timeout, including the previously failing multilevel control. CI
diagnostics were read only through Code Review; the log view is truncated,
so complete job conclusions and twelve reported process results establish
this bounded claim. Both Windows pins also pass longer gameplay/UDP and eight
focused clean-teardown checks. Windows appearance and subsequent full
qualification were still running at that checkpoint and were subsequently
cancelled when stages22-25 were pushed. No full main-run pass is claimed. The independent
[GPU Windows/Linux/Vulkan/sanitizers](https://github.com/Neversalimus/NCMM/actions/runs/40481073086),
[source/package](https://github.com/Neversalimus/NCMM/actions/runs/40481073024)
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40481072968) pass.
Subsequent stages22-25 Host bytes require their own remote qualification.

At `191d881`, [main run40497497118](https://github.com/Neversalimus/NCMM/actions/runs/40497497118)
passes both module jobs but fails Native linkage on both Windows pins and
Linux1807 as described in stage26; Linux1040 was still running when the repair
was prepared. Windows job IDs122168779629/122168779487 and Linux1807
job122168779729 were read through Code Review only. No new game run, movement
receipt or full Native pass exists for this failed source revision.

At `311d717`, [main run40501372289](https://github.com/Neversalimus/NCMM/actions/runs/40501372289)
passes both module jobs and both actual Windows Host compilation/early movement
programs. Windows galleries and full Linux qualification were still running at
the checkpoint above. The independent
[GPU Windows/Linux/Vulkan/sanitizers](https://github.com/Neversalimus/NCMM/actions/runs/40501372156)
pass, as do [source/package](https://github.com/Neversalimus/NCMM/actions/runs/40501372230)
and [runtime](https://github.com/Neversalimus/NCMM/actions/runs/40501372183).
These facts do not establish a full Native pass or qualify stage27's new
diagnostic Host bytes.

The freshly rebuilt hidden render-only RTX 4090 controls retain
45 grass draws, zero CPU world raster, zero warm static uploads and 21108 open
multi-Z pixels on both rasterizers. Existing ADS/objects/muzzle/subway brightness
checks pass. Repeat affected checks after code edits; do not rerun unchanged
suites just to accumulate passes. No nightly installer, full Native qualification,
ordinary RTX 4090 frame time or action-to-first-present result is claimed.
