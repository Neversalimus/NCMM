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
