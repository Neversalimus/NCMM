# First Person View architecture and reliability audit

## Stage A: baseline and scope

The starting main is `631a82f54d7c86b2d07ef100dfc181ecd453403a` (0.12.7).
PR #214 is a reviewed dependency, not part of that main: its qualified 0.12.8
runtime and diagnostics must be integrated without losing the original evidence.
No public Core/Graphics ABI or CDDA simulation rule is to change.

The source inventory counts FPV `src/` and `graphics_bridge/` implementation,
excluding tests, documentation, SDK and generated shader bytes: **67 files,
10,458 physical lines, 861,374 bytes**. Dense one-line C++ makes line counts a
poor complexity proxy. Largest units are `hybrid_renderer.hpp` (1,278 lines),
Host smoke (597), hybrid extraction (579), world extraction (435), bridge (346),
appearance (332), movement diagnostics (291), and module lifecycle (280).
Generated shader arrays are required release assets, not dead code.

The unchanged main passes 25 native tests on MSVC 19.38 in 5.34 seconds locally.
A MinGW attempt could not load its runtime DLLs (0xC0000135); this is an
environment failure and is not counted as renderer test coverage. MSVC is the
local comparison compiler. CI Windows/Linux and actual Host qualification are
separate evidence. Baseline raw inventories and timings are retained with the
task before runtime edits.

Historical hardware evidence is retained in STABILIZATION.md. The exact 0.12.7
1040 paired clear/rain baseline has first-frame medians 19.44–24.78 ms on
RTX 4090 / D3D12, viewport 1768×1084. PR #214 reduces those to 15.49–20.02 ms.
The existing 0.12.6 18.87–21.66 ms range comes from different runs; it must not
replace a paired same-input baseline. Full-frame p99 and memory comparisons
must be computed from actual raw samples, not inferred from p95.

## Responsibilities and dependencies

| Boundary | Implementation and responsibility | Ownership, concurrency and change cost |
| --- | --- | --- |
| Host extraction | `ncmm_graphics_world.inc`, `ncmm_graphics_hybrid.inc`, presentation/creature includes | Main CDDA thread owns snapshots; module borrows them only during callbacks. High cost: engine types and patch anchors. |
| Visibility | `ncmm_world_policy.hpp`, `ncmm_world_cache.hpp`; Host native range/perception | Structural surfaces and perceived contents have intentionally different rules. Pure policy tests can run without CDDA. |
| Shared geometry | `hybrid_renderer.hpp`, architecture/multilevel/creature/liquid includes | Existing Geometry Sink already shares world dimensions and material choices. Do not add a parallel scene graph. |
| Materials | `image`, `material`, creature appearance/skin and GPU packet ownership | Host atlas pixels are borrowed; worker sources and submitted packets own copies. Sampling differs intentionally by backend. |
| GPU producer | `gpu_scene.hpp`, `gpu_packet.hpp` | Module owns immutable chunks/meshes/textures; packet owns exposed pointers through submit. No SDL access from its worker. |
| GPU Host | `ncmm_gpu_backend.hpp`, resources/device/programs/validation | Shared SDL renderer owns the device/swapchain. Host owns allocations/readback fences, bounded independently of producer memory. |
| CPU fallback | `renderer::run`, projection/raster/volumes/sky | Full raster, depth, picking and atmosphere remain supported. Pixel/depth tests compare reference and optimized geometry. |
| Creatures | appearance → mesh → animation → LOD; cache/presentation → `creatures.inc` | One bounded worker generates owned immutable assets. Main thread owns instances and native art. Epoch cancellation and joining before unload are required. |
| Module | `module.cpp` | Registers optional Host APIs, chooses CPU/GPU, composes overlays and metrics. Repeated metrics construction and mixed cache/lifecycle logic are debt. |
| Integration | `apply_graphics_bridge.py`, canonical Host patch stack | Exact pinned engine identities and transactional preflight guards are necessary. Unknown anchors must fail before writes. |
| Packaging | installer bridge, NCMM transaction/runtime IO | Managed long-path transactions are distinct from native CDDA save IO. Existing repair/rollback/vanilla recovery must remain tested. |
| Diagnostics | native smoke/gameplay/movement/appearance includes; PS runner; verifiers/workflows | Explicit isolated processes, worlds and files. Failed children or incomplete evidence cannot qualify a release. |

The normal pipeline is already CDDA → owned callback snapshot → common
visibility/geometry → CPU raster or GPU packet → Host. CPU/GPU rasterization,
transparency ordering and asynchronous readbacks are genuinely different
responsibilities; merging those implementations would increase coupling.

## Cache inventory and invalidation contracts

This inventory describes owners rather than pretending every map member is an
independent cache. Main-thread structures must not be shared with the worker.

| Owner/cache | Key and data | Invalidation / lifetime / memory |
| --- | --- | --- |
| Host source facts/rays | Absolute XYZ, world, level revision; native topology and exact visibility bits | World/load reset; source revisions and eye/ray revision are separate. Bounded loaded window. |
| Host derived geometry | Absolute XYZ and dependency revision | Preserve overlap on walking; clear on world and loaded Z-window changes. |
| Host roof columns | XY and geometry revision | Roof facts have a different dependency span from individual XYZ source facts. Keep distinct storage. |
| Host sealed planes | World, window and topology revision; opening prefix sums | Lazy per-plane proofs; never assume every plane is sealed from one witness. |
| Draw-local semantics/range | Definition identities and context; exact light bits | Callback lifetime only. Fields, vehicles, entities and SDL pointers are never retained. |
| Host CPU sprite/composite | Renderer atlas identity/rectangle and creature composition | Tileset/renderer generation, 48 MiB guard and hybrid reset. Own CPU pixels. |
| Host horizon | Known resident overmap snapshot/revision | Reset on world identity; preserve across camera turns. No map generation or discovery. |
| Renderer images/vehicle lookup | Sprite identity/layout and vehicle mount/group | One collect/raster callback; borrowed pointers cannot escape. |
| CPU frame/world | Camera, world facts, environment and presentation digests | Module disable/mode changes, content key, periodic 250 ms refresh. Own pixels/depth and copied transparency. |
| Creature generated assets | Appearance/generator/tileset identity | World/tileset reset cancels queued generations; worker mutex; 64 MiB retained, 16 pending, 4 requests/frame. External immutable owners remain valid. |
| Creature native art/instances | Appearance identity; entity identity for poses | Epoch reset, disappearance; main thread; native art 32 MiB. |
| GPU producer chunks/signatures | Absolute XYZ, static normalized facts and neighbours | World/tileset epoch; light and animation excluded; moving window evicts departed cells. |
| GPU producer skins/impostors | Model identity, LOD, targeting and atlas frame | Weak ownership; dead entries pruned each frame. Content resources own immutable arrays. |
| GPU producer content | Content hashes for meshes/textures | Referenced entries cannot be evicted; separate 128/64 MiB budgets. |
| GPU Host resources | Content keys, current device and usage | Device/renderer reset, resource epoch, bounded LRU; full scene must be ready or use complete CPU fallback. |
| GPU readback ring | Fence, producer serial and readback generation | Old-world completions ignored; release before device destruction. Three bounded 64-byte slots. |

These caches do not all have interchangeable keys or invalidation. Unifying
source facts with display visibility, or producer CPU ownership with SDL
allocation ownership, would create correctness hazards. Prefer sharing their
window/index mechanics and removing repeated work rather than a global cache.

## Confirmed risks and selected work

1. Main lacks the qualified GPU teardown/strict-contract correction in PR #214.
   Preserve the matched-symbol evidence; the exact copied resource and the
   separate effect of each fix remain unisolated. The older 1040 SIGSEGV has no
   original stack and must remain an unconfirmed historical fault.
2. Certified Hosts run `39037975570` fails before compilation on exact upstream
   tags 2026-10-16-1900 and 2026-10-17-0119. `newcharacter.cpp` renamed scenario,
   profession and hobby locals. The pricing semantics still exist. The dedicated
   negative test only fetches 1040; extend it to both pins and exact new forms.
3. Native save IO has a path-length failure independently reproduced with old
   runtime bytes. Test infrastructure should reject an unsafe output path before
   copying a game, and use short isolated roots. Do not move player saves or
   rewrite upstream persistence.
4. `creature_cache.hpp` pulls the complete filesystem/bitmap/JSON debug exporter
   into every renderer translation unit. Separate that implementation and keep
   a small declaration so diagnostic IO no longer controls renderer rebuilds.
5. GPU producer and shared renderer each construct a dense XYZ index during a
   single collect. Reuse one callback-owned index if exact geometry/signature
   tests demonstrate equivalence and bounded ownership.
6. Module duplicates creature metrics marshalling across CPU/GPU. Keep one
   concrete translation helper with backend-specific visible-pixel values.
7. CI already has quick/integration/full opt-ins, but expensive test invocations
   and merge evidence need auditing. A cheaper tier must not satisfy full release
   qualification. Keep shader identity and source guards in Quick.

Stages B–G will record fixes, tests, measured cost, rejected risky changes and
remaining limits in separate logical commits. No new rendering feature or
public ABI is required by the selected work.

## Stage B: reliability changes

Integrated the exact qualified PR #214 dependency in a separate merge commit.
Its runtime, matched-symbol WARP diagnostics and 16 clean native focused exits
remain attributed to the original runtime and packaged source identifiers.
No causal claim is added for the old 1040 SIGSEGV without its missing stack.

Certified Host failure root cause: upstream changed the scenario/profession/
background UI from range loops using `scen`/`prof_id` to indexed loops using
`current_scenario`/`current_profession`/`current_hobby`. Both exact new sources
(`9da8cce18f4b112a2b43339913ba6385a6017195` and
`d29db9cb3106381ea5efc47dba3d1834cd452dd7`) have identical chargen inputs.
The patch now selects exactly one complete known replacement group per
contract, preserving name and point-cost expressions. Missing, ambiguous or
partial groups still reject before changing any file. The dedicated test now
covers both supported pins and both failed sources: 126 cases pass on each of
PowerShell 5.1 and 7.6, including LF/CRLF, idempotence and every altered active
postcondition. This fixes the proven transform failure; it does not certify
new upstream Hosts or promote the certified feed.

The isolated benchmark runner rejects output roots longer than 80 characters
before copying game assets. This conservative test policy avoids the known
291-character memory-archive failure for qualified fixtures. It is not an
exact universal upstream save-path limit and does not rewrite persistence.
Nine actual preflight checks pass on PS 5.1 and 7.6, covering the boundary,
source-folder containment and valid sibling paths. Existing 72 environment
and five evidence-validation checks remain passing. No player save was moved,
edited, copied into the fixture, or deleted.

## Stage C: one lookup for shared scene preparation

Removed the GPU-only `coordinate_index` wrapper. A single callback-owned
`ncmm_world::coordinates` now serves static signatures, GPU collection and the
existing Geometry Sink generator. CPU raster still constructs its own index
when called independently. The renderer cannot be copied with a reference to
another object's owned lookup. No new persistent cache, epoch or public ABI
was introduced.

Previously the GPU path built two dense lookup arrays per collection (their
lifetimes overlap during generation); now it builds one. At the maximum
97×97×32 window this removes one allocation and 1,204,352 transient bytes.
This is a calculated lookup-storage reduction, not a measured process-RSS or
GPU-memory claim. Static resource budgets are unchanged.

All 25 MSVC suites pass after this step. Differential geometry checks compare
the owned and shared lookup paths at 36 camera/Z combinations, preserving
ordered triangle counts, material ownership, exact positions, UV and light.
Existing sparse holes/extreme coordinates, lighting-only invalidation,
adjacency and warm-cache reuse checks also pass.

## Stage D: separate diagnostic IO and metrics translation

Moved the existing appearance exporter implementation into one compiled source
unit with a small forward-declaration header. Bitmap/JSON output, owned asset
inputs and worker behavior are unchanged. Filesystem, stream formatting and
bitmap-writing implementation no longer enter every renderer translation unit.
One static diagnostic library uses the same build/runtime/sanitizer settings
and is position independent for the Linux shared module. This is a concrete
dependency boundary, rather than splitting renderer methods into include files.

An MSVC preprocessor probe including `hybrid_renderer.hpp` changes from
3,832,834 to 3,215,248 bytes (same flags/toolchain; before is PR #214). This
measures compiler input, not runtime memory or a percentage of technical debt.
All 25 native suites still pass, including shared-library lifecycle and CPU/
GPU creature rendering. Full native appearance export remains a release gate.

CPU and GPU now call one creature-metrics translator in `module.cpp`, with
their own visible-pixel count. The repeated cache/anatomy/failure/timing field
mapping has been removed without changing the Host metrics ABI.

## Stages E/F: evidence and qualification cost

The comparison now retains p99 for first frames, warm rotation and exclusive
phases, alongside median/p95 and raw samples. Baseline source is explicit and
can be changed only by a caller providing its exact identity. A skewed-sample
test protects the tail calculation. A dozen first-step samples do not establish
a stable population p99; sample counts and maximum remain visible.

Version metadata now comes from mod.json for the module, benchmark and package
builder, removing independent hardcoded descriptor/benchmark versions. The
release verifier still demands an explicit expected version (default 0.12.9).
The old one-visible-Z README claim is corrected. Documentation now distinguishes
unchanged CPU Balanced 960×600 / High 1280×800 from GPU Balanced 1920×1080 /
High 2560×1440. Older 0.12.8 quality text mistakenly called 1280×800 the CPU
Balanced cap; retained historical evidence is not rewritten.

CI selects the maximum required tier across changed files and includes rename
origins. Source comparison truncation fails closed. Quick keeps renderer,
contract, source-transform, shader-identity and focused sanitizer checks;
Integration adds both native Hosts and their ordinary installer/save/gameplay/
movement gates; Full keeps exhaustive sanitizers, appearance and legacy paired
comparisons, both official package variants and the long automated soak.
Explicitly selecting Quick cannot qualify an ABI/device/installer change.
The aggregate check requires module success and both Host successes whenever
native qualification is selected. Final merge must also verify the independent
GPU and canonical NCMM engineering workflows.

Documentation/evidence-only edits no longer trigger the two standalone GPU
device jobs, and editing a PR description no longer rebuilds native Hosts.
Expensive release scenarios are retained where they cover distinct behavior;
no claimed CI speed percentage is extrapolated from reduced trigger counts.
Current full qualification is required before this branch can be merged.

One confirmed duplicate production Host build is removed from automatic PR
triggers. The Equipment Body Map workflow and PR engineering workflow both
check the same body-map/canonical/integrity contracts and invoke
`Build-HostPackage.ps1` for exact 1040. Engineering additionally tests real
installation, module lifecycle, gameplay and restore. Its trigger covers every
former Body Map trigger, including edits to the standalone workflow itself.
The standalone workflow remains manually runnable for diagnosis. This removes
one production Host compilation on overlapping payload/patch PRs while keeping
the complete superset of checks; experimental FPV 1040/1807 builds remain.

Hardware-baseline recovery exercised the stalled-window diagnostic branch and
found that PowerShell 7's .NET drawing types need explicit references beyond
the System.Drawing facade. The runner now includes the drawing assemblies
present in its own PowerShell runtime. A test compiles the actual capture
statements on PS 5.1 and PS 7.6 without capturing a window; both pass. This
changes diagnostics only. The timed-out old 0.12.7 process is retained as a
failed attempt, not counted as successful performance evidence.

The stalled native capture then identified `Stale game data detected` for the
injected multi-level fixture. The runner had copied derived `data/cache`
flexbuffers from a previously played installation before updating its JSON
fixture. Data copying now omits only that generated directory and preserves
all original source assets/cache. Five real copy-isolation checks pass on both
shells, including nested files and a bracketed path. A fresh exact 0.12.7
hardware control then completed all four 96-frame CPU/GPU profiles with exit
zero, no dumps and RTX 4090 GPU records. The earlier two timeouts are preserved;
CDDA's stale-data validation was not bypassed.

## Stage G: qualification findings

The first Full 1040 run rejected a stationary GPU frame because its total upload
exceeded an old 2 MiB threshold. A bounded native diagnostic reproduction showed
15,336 source cells, zero regenerated cells and signature reuse on that frame,
with 2,757,444 bytes of legitimate frame data. The gate predated the current
multi-level scene and conflated dynamic instances/cells/bones with static data.
The resource cache now records immutable mesh/texture upload bytes privately;
the native gate requires exactly zero static uploads and zero regenerated cells.
No public metrics ABI, resource budget, rendering or synchronization changes.
Device regression cases cover a valid dynamic transfer above 2 MiB, dynamic mesh
updates, and a real static reload after eviction. The original Full failure is
retained and the final source must pass Full again.

Two additional local controls using older exact binaries (20d gameplay and
cf6 movement) failed at synthetic F6 dispatch, with controlled exit 191 and no
access violation. The helper queued one key before eight input reads. CDDA's
pre-poll redraw/recovery can pump and discard that key, leaving subsequent reads
without an event; the existing menu-cancellation diagnostic already documents
this boundary. Each of the same eight bounded attempts now queues its own key
and logs unexpected actions. The expected action, actual module toggle, cache
clearing and final disabled-state checks remain required. Normal input is unchanged.

Every native smoke process deliberately drains its first synthetic key through
the real `input_manager::pump_events` before reading input. Qualification requires
that this regression was exercised and that a fresh attempt successfully
dispatches F6. Counters are included in the smoke JSON. This tests the lost-key
case in real CDDA; it does not claim the exact unrecorded event in either older
failure was identified. Both older failures are retained, and this final change
requires another Full and exact-package hardware qualification.

The next Full reached the procedural gallery and rejected its LOD1 case while
observing only LOD0. The module's existing default is `tileset`; the gallery
labelled itself procedural but never selected `volume`. Native sprites correctly
bypass procedural LOD selection and report bucket zero. The fixture now explicitly
selects the existing volume mode for procedural CPU/GPU galleries, records that
choice, and restores the previous setting on exit. The product default, LOD
thresholds and the requirement to observe all four procedural buckets are unchanged.

The additional RTX4090 long audit passed 12,384 frames before the same one-shot
delivery defect occurred at F12 (exit 191, no dump). F6/F12 diagnostics now share
one bounded key dispatcher. Both require their exact native action and handler;
the eight 250ms reads preserve the two-second nominal input budget. A gameplay
audit must exercise a deliberate real pre-poll drain and successful requeue for
each key. Normal gameplay hotkey handling remains outside this opt-in fixture.
