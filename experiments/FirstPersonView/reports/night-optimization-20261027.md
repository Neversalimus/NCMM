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

## Actual remote evidence and next priorities

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
