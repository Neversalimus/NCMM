# First Person View 0.12.x stabilization

Baseline: `c5a0a0a5e12991df06ba000f80dd359f274b7f50` (merged PR #205).
Engine pins remain 1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83`
and 1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.
Scope: gameplay audit, measured performance, camera/interaction stability and
selective visual polish. No new graphics architecture or gameplay rules.

## 0.12.5 facade continuity candidate

Runtime qualification is pending. Engine pins and public API table layouts are
unchanged. This follows user screenshots of detached upper floors and large
facade gaps.

The geometry regression constructs a four-level building with covered windows,
closed doors and a fenced roof. It compares actual rendered depth after the
source visibility policy against the complete unculled building from ten
camera/pitch combinations. It reproduced sky cracks around closed panels;
matching panels to their complete openings removes them. Full-depth surrounds
and recessed frames use neighbouring facade materials. Visible architecture
carries its actual slab; wall sides no longer stop 8 cm below the storey plane.
Upper-level tree billboards retain their original world elevation.

Facade visibility now records which physical faces are exposed. Only visible
faces with non-opaque, loaded neighbours can supply facade light or its native
distance limit. Native body perception stays separate. This addresses shaded
facades below overhanging floors without publishing room contents or actual
unseen terrain identifiers. Face witnesses are cached with the same eye/world
revision as other physical rays, using a bounded 16-bit mask.

Terrain/atmosphere collection and native appearance extraction use a bounded
draw-local XYZ index. The former GPU neighbour index delegates to the same
implementation. Invalid indexed spans stay closed; holes, negative coordinates
and distant absolute origins retain the binary lookup's results.
Shared wall faces are removed only beside another complete rendered wall;
missing snapshot neighbours and vertical transitions keep their faces.

The native movement runner additionally verifies all 18 front-wall cells of a
roofed building at 30 cells with ordinary vision, in CPU and GPU modes, and
exports captures and per-cell light/permission diagnostics. Older multi-Z
fixtures used DEBUG_NIGHTVISION and could miss the reported condition.
Diagnostic F6 dispatch tolerates bounded pending input events; the PowerShell
runner preserves native failure reasons if a locale-dependent smoke JSON number
cannot be parsed.

Local and CI checks, exact installer hashes and hardware results will be recorded
after qualification. No general speedup or ordinary gameplay-FPS claim is made
from the implementation alone.

## 0.12.4 camera and movement performance follow-up

Qualified runtime: `c1ad43add41c2d5b583e885d5a702cebc4b4e81b`.
Test merge `0ec75b84d2aa49607975a6069377aea9ba50b507` has the identical runtime
tree. [Native integration](https://github.com/Neversalimus/NCMM/actions/runs/38800352387),
[GPU/UI](https://github.com/Neversalimus/NCMM/actions/runs/38800266254),
[Source/Package Audit](https://github.com/Neversalimus/NCMM/actions/runs/38800266300)
and [targeted ASan/UBSan](https://github.com/Neversalimus/NCMM/actions/runs/38801084536)
passed. The sanitizer branch adds only its workflow to the qualified runtime.

Native sight-range results are memoized for exact float arguments within one
draw; every frame starts fresh. Candidate enumeration retains both native
body/lower-slab ranges. Same-storey rays use the equivalent supercover path,
including corner ties and the general traversal near storey boundaries.
Absolute-coordinate derived geometry and roof facts retain their overlap after
a viewport shift. Native geometry notifications, loaded-Z bounds and realm
changes still invalidate dependent facts. A plane is considered sealed only
when every source-window tile has a full opaque slab; any opening or glass
defeats the proof. The nearest upper slab underside and avatar storey remain.

GPU neighbour signatures use a bounded frame-local index of the validated sparse
XYZ scene. Auto selects CPU for software adapters; explicit GPU remains available
for qualification. It is substantially slower on the CI software adapter.

Cold-step profiling then identified up to 112908 source-fact rebuilds on the
12-step route, with seven geometry revisions. Definition flags and shape
classification now use a bounded draw-local terrain/furniture memo. Both pinned
engines implement map::has_flag as terrain OR furniture after bounds checking.
The no-Z/bottom floor policy is part of the key. Fields, furniture presence,
vehicles and roofs are evaluated live outside this memo. Recorded frames have
at most 40 definition evaluations and up to 112905 memo hits. Collision tests
recompute exact results; no definition identity survives a draw.

Both pinned Hosts passed graphics/sounds installation, repair, rollback, Restore
Vanilla, repeated bootstrap launches and native multi-Z save/load. Native gameplay
passed all ten action groups, six AI turns with 1/5/20 enemies, official and
actual UDP CPU/GPU scenes, a 60-second automated soak, focused profiles and all
12 movement records per pin. Windows/Linux each passed 24 module tests and eight
atomic source-transform tests. Three focused Linux ASan/UBSan tests passed on
the final cache/traversal/index code. This integration does not repeat the
historical full gallery, paired 0.12 image matrix or 30-minute soak.

### Recorded CI movement

These are CPU wall milliseconds through SDL presentation at a requested
1768×1084 viewport. The CPU renderer uses its existing resolution cap. First-ready
includes the native move/cache update and first presentation; easing and rotation
do not advance simulation turns. Each first-ready row has 12 samples.
Before is checkpoint `a85824f3`; After is the qualified definition-memo runtime.
These are independent CI runs, not a paired same-machine experiment or a
general player-FPS guarantee.

| Pin | Scene | First median before | First median after | First p95 after | Warm rotation median | Easing median |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1040 | street | 99.36 | 67.92 | 100.70 | 34.69 | 29.92 |
| 1040 | multi_level_horde | 102.62 | 74.79 | 92.77 | 35.52 | 31.23 |
| 1040 | city_blocks | 110.82 | 82.77 | 98.78 | 39.92 | 33.59 |
| 1807 | street | 206.68 | 176.31 | 207.69 | 57.37 | 47.02 |
| 1807 | multi_level_horde | 209.02 | 170.55 | 205.45 | 60.39 | 49.03 |
| 1807 | city_blocks | 213.48 | 181.73 | 223.00 | 69.52 | 53.87 |

Explicit GPU cases identify Microsoft Basic Render Driver, a software adapter.
Their full-size warm-rotation medians remain 91–314 ms across the scenes/pins.
Auto selects CPU on that adapter. These CI measurements do not establish discrete-GPU
performance. Separate exact-installer 1040 hardware measurements follow below;
ordinary manual gameplay and 1807 hardware performance remain unqualified.

### Exact 1040 installer on RTX 4090 (2026-10-16)

The exact delivered 1040 Host and DLL completed the packaged movement runner on
NVIDIA GeForce RTX 4090, Direct3D12, actual UDP Hybrid v3, in clear weather and a
native rain-descriptor fixture. All 32 payload hashes and Setup were verified
before extraction; the tested clone's Host/DLL hashes match the delivery hashes
below. [Raw samples and provenance](qualification/0.12.4-rtx4090-movement.json)
include the initial run and the unchanged-installer retry.

Each run contains all 12 scene/viewport/CPU-or-GPU records. Each record has 48
cold and 48 warm continuous-rotation samples, 12 real native steps and the
following easing frames. All 1020 requested GPU frames in the successful clear run and
955 in rain used the hardware GPU, including the first changed-world frames.
The two runs use the same 107 UDP asset files, with matching asset manifests.

Times below are CPU wall milliseconds through SDL presentation at a requested
1768×1084 viewport, using the existing renderer resolution cap. Cold/warm and
first/easing describe separate operations, not additive stages. Each first-ready
row has only 12 samples. These controlled routes advance no ordinary AI turns
and do not establish player FPS, manual driving or prolonged-session comfort.
1807 hardware performance and ordinary manual gameplay remain unqualified.

| Weather | Scene | Cold median | Warm median | Warm p95 | First median | First p95 | Easing median |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| clear | street | 15.67 | 14.06 | 15.19 | 95.13 | 134.61 | 17.63 |
| clear | multi_level_horde | 14.46 | 13.51 | 16.81 | 97.32 | 116.90 | 15.10 |
| clear | city_blocks | 11.40 | 9.27 | 10.15 | 89.03 | 118.60 | 12.23 |
| rain fixture | street | 40.34 | 37.08 | 41.36 | 134.06 | 170.71 | 39.06 |
| rain fixture | multi_level_horde | 32.27 | 29.81 | 39.99 | 125.21 | 146.08 | 22.90 |
| rain fixture | city_blocks | 18.18 | 14.77 | 17.31 | 123.93 | 147.92 | 17.07 |

Rain uses the engine's native rain descriptor under the sunny ID forced by the
smoke fixture, in an owned game-data clone. The fixture changes no executable or
module bytes; its source/fixture SHA256 values are retained with the samples.
This exercises real rain rendering, not the normal weather-selection simulation.
Clear/rain captures were inspected: upper brick facades remain continuous and
creatures retain the complete native tileset artwork; the rain capture contains
precipitation and weather haze.

The initial clear run completed all 12 performance records, then failed the
final automatic F6 dispatch. One repeat with identical binaries and the rain
run passed the entire runner, including F6 restoration. The initial failure's
cause remains unconfirmed; this is not evidence of a production hang fix.
The failed smoke result also contains a locale-dependent decimal comma, causing
a secondary PowerShell parse error; the movement JSON is valid and its exported
percentiles were verified from the raw samples.

### Verified delivery

[Artifact review](https://github.com/Neversalimus/NCMM/actions/runs/38807413895)
verified each original qualified ZIP digest, Setup, all 32 payload hashes, engine
pins, runtime source tree, 42 native input hashes and the 12-record movement
evidence. It stages the qualified files as direct-download ZIPs; Setup and payload
bytes are unchanged. The GitHub artifact wrapper is recompressed, so its digest
differs from the original inner installer ZIP.

- [1040 installer](https://github.com/Neversalimus/NCMM/actions/runs/38807413895/artifacts/12006203659): delivery SHA256 `29d6163d90bdeb88b473155caa96da40f2ce96a2e1aefded2327f2a43b118824`; original qualified ZIP `33a32a6dc39589a12b84e1027f204338e35c8bacfba5c14b8cb3b45aefeeeeda`.
- [1807 installer](https://github.com/Neversalimus/NCMM/actions/runs/38807413895/artifacts/12005959053): delivery SHA256 `77e3a7929199fe9f0ba041720e17e6c062f92dd03b073b16283f36c51b45e1d3`; original qualified ZIP `20c1e6eba256edac1dfeeac90aef0b7d317bf70eb97929521719b28fbd041b07`.

1040 Host SHA256: `6191091e4288b97e9e6a10040ba7a783133376e9a0a5fe9bd3e2fa1ed9117bee`; module SHA256:
`61f0845fad051edc6b45e0ffac98b3dbe583d0c7264383b7d7b8c33f03e11c72`.

1807 Host SHA256: `34a6448aeb8b70ddbd7158565b17172a7c1ab440da20d226cf5b1992876b03d8`; module SHA256:
`31c3191b2135e91bbf83d3c8464f84c5ac55b14625f4509f6fe40d6099f1cf94`.

## 0.12.3 movement and visual follow-up

The reported screenshot shows fragmented upper facades and heavily distorted
creature reconstruction. The new default keeps the full native tileset sprite,
its transparent outline and cropped aspect ratio; reconstructed models remain
an explicit NCMM setting. The bounded 32 MiB owned art cache shares appearances,
retains death clips and invalidates on world/tileset changes.

A facade regression reproduces an upper wall whose centre ray is blocked by a
foreground slab while its upper face remains visible. Face/height probes grant
only the existing structure permission. They never grant room contents or
creature visibility. A solid foreground wall still blocks all probes.
Coarse upper brick walls retain the canonical brick finish instead of turning
into plain grey plaster; the actual unseen terrain ID stays unpublished.

Derived world flags are cached instead of recalculating neighbours and roof
masks at every DDA step. Visibility uses the native avatar eye; visual walking
offsets affect projection without retracing the world on every easing frame.
Vertical transitions retain the displayed eye so the surrounding floor cannot
disappear before the camera passes through the stair opening. The native stair
audit checks this agreement on the first displayed frame.
Roof masks are now lazy per column and stop at the first blocking surface;
geometry invalidation uses explicit native notifications instead of invalidating
rays again when an unchanged source fact is first read. Opaque slabs skip rays
to their hidden backs. Glass, stair openings and slab edges keep both paths.
Randomized 1/3/21/32-level masks match the exhaustive reference, and CPU frames
above, below and beside slabs preserve all pixels, depth and ownership.
The same native current/lower visibility-range conditions are checked before
tracing; cells that cannot receive any existing permission are rejected early.
Day/night limits are shared across the frame and native tile perception is
queried once for the body/top pair. Private occluders still participate in rays.
The GPU producer completes all dirty cells in the validated, resource-bounded
snapshot, avoiding its previous 512-cell warmup and full CPU fallback.

GPU resolution caps become 1920×1080 balanced and 2560×1440 high. The CPU fallback
caps become 960×600 / 1280×800. CPU/GPU depth, visibility and native action costs
retain their existing ownership.

An isolated local **0.12.2 baseline** ran on NVIDIA GeForce RTX 4090, D3D12.
The native action audit took 2087.40 ms for four steps including easing. Across
its 57 action frames, visibility had median 98.34 ms and maximum 187.33 ms;
stair first-presentation median was 186.08 ms. These are action-audit wall times,
not a claim about ordinary-game FPS. The audit completed its action/scene JSON
but the following save/load process exited with 0xC0000409; the entire local
audit is not a pass. The separate four-case focused baseline passed.

New coverage: partial upper facades, native art proportions/depth/death cache,
and a 1940-cell GPU snapshot ready in its first frame. The native movement
benchmark measures 12 real steps per scene/viewport/renderer, including all
easing frames, at 640×480 and 1768×1084. Requested GPU frames must all stay on
GPU.

Qualified runtime head: `40e5c24e3003de7066b2cd2109c500eee2477bde`
(test merge `dda4fbdb014ffdca0bbb045e6ab962c1e58104e4`, identical runtime tree).
[Integration run](https://github.com/Neversalimus/NCMM/actions/runs/38768486226),
[GPU/UI](https://github.com/Neversalimus/NCMM/actions/runs/38768483804) and
[Source/Package Audit](https://github.com/Neversalimus/NCMM/actions/runs/38768483723)
passed. Both pinned Hosts passed graphics/sounds installation, repair, rollback,
Restore Vanilla, repeated bootstrap launches and native multi-Z save/load.
The native audit passed all ten action groups, six AI turns with 1/5/20 enemies,
official and actual UDP CPU/GPU scenes, a 60-second automated soak and saves.
Focused profiling and all eight movement records passed on each pin.
24 module tests and eight source-transform tests passed on Windows and Linux;
24 fresh local MSVC tests also passed. This integration checkpoint does not
repeat the historical full gallery, paired 0.12 benchmark or 30-minute soak.

The delivered installer ZIPs retain the qualified CI bytes unchanged.
SHA256: 1040 `395520bc32e1bd67a589bd64a68c2a4580edebb7e918d3ded22874c7e3894abd`;
1807 `88250c4adec60cd261f4adde1c13e5d4e456f1b59383b98226c478de46c052da`.

### Hardware movement measurement

The exact delivered 1040 installer Host and module passed the isolated movement
runner on NVIDIA GeForce RTX 4090, Direct3D12, actual UDP Hybrid v3. Host SHA256:
`83c37c9213d60756450ae80d86a8c75ea28c2f931eb08225c90912dd798b864c`; module SHA256:
`87d777c5d1a4a588e56ad3f4abbe56c08cc55bb695abe7b95d7898b345e4ec0d`.
All eight CPU/GPU records completed, with 12 native steps each and no native turn
advancement. All 296 requested GPU frames, including each first changed-world
frame, used GPU; no software fallback occurred. The final horde GPU capture
shows continuous brick upper facades and full native creature artwork.

Times are CPU wall milliseconds through actual SDL presentation. Viewports are
requested window viewports, with the documented renderer resolution caps.
First-ready includes the native step and first presentation; easing is the
following visual-only frames. Each first-ready row contains only 12 samples,
so p95/p99 describe this route and are not a general player-latency guarantee.

| Scene | Viewport | First median | First p95 | First p99 | Easing median | Easing p95 | Easing p99 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| street | 640×480 | 119.45 | 153.01 | 156.94 | 15.58 | 18.36 | 19.05 |
| street | 1768×1084 | 118.99 | 157.32 | 170.03 | 17.46 | 19.92 | 20.28 |
| multi_level_horde | 640×480 | 112.25 | 139.13 | 143.07 | 11.29 | 14.24 | 14.76 |
| multi_level_horde | 1768×1084 | 109.63 | 132.49 | 135.89 | 12.38 | 15.57 | 16.08 |

These controlled routes are not a full ordinary-game FPS measurement. They do
not include ordinary AI turn cost, manual driving or a prolonged manual session.
The older action-audit baseline above uses a different route and is not a paired
before/after walking experiment. Reconstructed volumes remain an explicit option.

## 0.12.2 follow-up candidate

This candidate moves the existing widest conservative frustum rejection before
map/bubble conversion and loading probes. The bound and native perception rules
are unchanged. A speedup is not established until native measurements complete.

The previous 1807 single-creature 48-frame GPU result had a slower median but
better p95/p99: graphics 44.97 -> 53.08 ms median, 86.98 -> 71.09 ms p95 and
93.63 -> 74.45 ms p99. Geometry stayed 4.51 -> 4.56 ms and extraction improved
24.28 -> 20.49 ms. Sounds show the same mixed direction. This does not identify
the cause; previously unmeasured window presentation/native tiles may contribute.

The GPU benchmark now acknowledges successful presentation just like ordinary
gameplay, and exports exclusive native-tiles, perspective, actual-present and
unattributed wall-time samples with actual CPU/GPU tags. CPU recording timings
remain distinct from GPU execution time. The isolated runner accepts
`-FocusedGpuBenchmark` for smoke-gas and one creature at actual 640x480, 96 frames
per renderer/case. It does not perform the gameplay soak or verify UDP; those
switches cannot be combined. Default full matrices and gameplay audit stay intact.

24 local Windows native tests and PowerShell parsing passed. Both pinned Hosts
compiled in run #128 and passed preview installation, Vanilla restore, bootstrap
launches and the early native multi-Z save/load gate. The native gameplay audit
then stopped at `native_combat_20_turn_2` on both pins: its progress journal stayed
at 101 rendered frames until the 180-second watchdog fired. This checkpoint is
before `do_turn()` and the following CPU/GPU rendering; it does not yet identify
which operation blocked. The independent focused profile was skipped by the
initial workflow's default failure condition, so no new speedup is established.

Diagnostic follow-up `966542ae` retains process CPU/RSS, native runtime/error
logs and a bounded CI-only stalled-window image in the job log. The focused
phase reproduction runs after a successful installer gate even when the native
audit fails; that failure still makes the job red.

[Repeat integration #131](https://github.com/Neversalimus/NCMM/actions/runs/38744742106)
passed on both 1040 and 1807 at runtime head `966542ae5219793bc61f58aa37b0ed7f828faded`.
All ten native action groups, six AI turns with 1/5/20 enemies, official and
actual UDP Hybrid v3 CPU/GPU scene coverage, the 60-second automated soak and
native multi-Z saves passed. The separate focused profile passed four cases
per pin, each with 96 measured frames at actual 640x480. Per-frame exclusive
phase accounting, renderer identity and exported percentiles were verified.
[GPU/UI #67](https://github.com/Neversalimus/NCMM/actions/runs/38744674106) and
[Source/Package Audit #951](https://github.com/Neversalimus/NCMM/actions/runs/38744673744)
also passed, as did 24 module tests and eight source-transform tests on each OS.

The earlier timeout did not reproduce. Its cause remains unconfirmed, so this
does not claim a production hang fix. The retry changed the runner/CI only,
including hidden process launch; it did not change Host or module source.
The watchdog and native diagnostics remain enabled for a future occurrence.

Focused software-D3D12 results below are CPU wall-time medians in milliseconds.
Both GPU cases identify `Microsoft Basic Render Driver`, not a hardware GPU.
Stage medians need not sum to the full-frame median; per-frame samples include
the separately exported unattributed remainder and reconcile individually.

| Pin | Scene | CPU frame | GPU frame | Native tiles (GPU) | Perspective (GPU) | Actual present (GPU) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1040 | Smoke/gas | 43.42 | 123.23 | 3.59 | 14.07 | 105.43 |
| 1040 | One creature | 16.14 | 27.53 | 4.38 | 23.13 | 0.26 |
| 1807 | Smoke/gas | 75.48 | 220.94 | 1.20 | 20.87 | 197.76 |
| 1807 | One creature | 26.85 | 57.24 | 1.49 | 31.39 | 0.43 |

Smoke's largest measured GPU wall phase is actual presentation on both pins.
One-creature geometry/resource preparation medians are 2.82/0.58 ms (1040)
and 4.39/0.65 ms (1807); these alone do not explain the full frame. The runs
are not a paired 0.12.1/candidate comparison on the same adapter/session.
They locate measured work, but establish neither a general speedup nor a
discrete-GPU regression. The 0.12.1 full gallery, paired baseline and 30-minute
soak were not repeated for this small follow-up.
The 0.12.1 installers below remain the last fully qualified release until that
candidate is qualified. Manual acceptance and discrete-GPU qualification remain open.

## Final automated qualification of 0.12.1 — 15 October 2026

Qualified runtime head: `ccb11df48b582bde00c9958bdd5b8b507d573556` (test merge
`b51eca9877d4cd81ae0d1af600aa75c8d0ed4a12`).
[Full run #123](https://github.com/Neversalimus/NCMM/actions/runs/38685586242),
[GPU/UI #63](https://github.com/Neversalimus/NCMM/actions/runs/38680995835) and
[Source/Package Audit #947](https://github.com/Neversalimus/NCMM/actions/runs/38680995867) passed.
24 Windows + 24 Linux tests, 19 sanitizer tests and 8 source-transform tests
on each platform passed. Both Hosts and graphics/sounds installation matrices,
bootstrap launches, repair/rollback/Restore Vanilla and native Z -1/0/+1 saves
passed. The retained initial-checkpoint sections below describe findings before
these final results; their pending automated statuses are superseded here.

CAM-01/02/03/04 and PERF-01/02 are implemented and exercised by those gates.
CI-01 is implemented. QA-01 has CPU submission accounting; GPU execution timestamps
remain unavailable. QA-02 has automated native actions, not manual acceptance.
QA-03 passed with pinned actual `UndeadPeople_0J_Hybrid_v3` assets from CDDA-Mods
`59123092c33244d516fa9d5ca69eaafb8d487f37`; this does not qualify every third-party tile.
No production P0 failure was confirmed by these runs.

The 1807 automated soak completed 1800.13 s and 8931 rendered frames: 139 F6
cycles, 93 renderer switches, 34 map loads and 17 saves. Working set
initial/final/peak: 901.5/437.0/958.5 MiB. Mixed-renderer full-frame
median/p95/p99/max: 138.13/298.28/460.25/981.36 ms on software D3D12.
This bounded observation does not prove absence of leaks or comfortable hardware play.

The separate smoke-gas benchmark uses **48 frames**, actual internal 640x480:

| Pin/package | Median before -> after (ms) | p95 (ms) | p99 (ms) |
| --- | --- | --- | --- |
| 1040 graphics | 250.97 -> 200.03 | 279.85 -> 214.26 | 285.97 -> 220.01 |
| 1040 sounds | 251.45 -> 201.15 | 273.09 -> 220.88 | 286.98 -> 223.30 |
| 1807 graphics | 257.87 -> 205.44 | 292.76 -> 221.87 | 299.68 -> 230.17 |
| 1807 sounds | 260.08 -> 206.03 | 280.14 -> 224.65 | 296.17 -> 226.79 |

These are frame wall times on **Microsoft Basic Render Driver**, not discrete-GPU
execution times or a player FPS promise. General improvements are modest and
not universal: among 112 load cases per renderer (24 samples each), the median
paired after/before ratio for 1040 graphics is CPU 0.988 / GPU 0.966 and for
1807 graphics CPU 0.970 / GPU 0.995. The 1807 single-creature GPU benchmark
regressed by 18% (graphics) / 11% (sounds) in this software environment.
The three-sample stair galleries also fluctuate; their quantiles are exploratory.
Causality and hardware impact of these isolated deltas remain unconfirmed.

Actual before/after building, stairwell and smoke images, UDP creature CPU/GPU
views, a melee frame and the vehicle interior were inspected. Static comparisons
preserve their appearance; this is not a new model/material overhaul. Coarse
faces, broad creature color patches, simple walls and vegetation remain visible
style limitations. The images and raw JSON remain in the linked run artifacts.

**Manual acceptance remains open:** a normal new-character session, complete
inventory and aiming/reload UI flows, driving physics and a prolonged ordinary
playthrough were not performed. The automation uses controlled constructed
fixtures, debug-protected combat, inventory cancellation, reload completion and
controlled vehicle displacement. Use the bundled isolated benchmark on a real GPU
and perform scenarios A-E from the specification before claiming production quality.

The final documentation correction replaces the obsolete installer claim of
"one visible z-level" and "tile-center movement". Delivered packages preserve
every qualified file byte-for-byte except README.txt; no EXE, DLL, payload hash
receipt or installer logic is rebuilt.

## Evidence and limitations (initial checkpoint)

The starting evidence is prototype run `38599750082`, GPU run `38599750041`
and source audit `38599750047`, on branch head
`9c3c69397bbb5ae9d3ec16f8ef21732e15e08b34` and Actions test merge
`74315d034bc28f96133f4af9a71ed44a0d077f7e`. All renderer/Host source blobs
equal the merged baseline; only `feed/index.json` differs.

These are automated real Windows/CDDA/SDL fixtures and software Vulkan/D3D12
adapters. They are not a manual playthrough or discrete-GPU qualification.
The current development environment has no display, Windows runtime, GPU
device, or UDP Hybrid v3 assets. Manual gameplay, hardware FPS and UDP
compatibility must remain explicitly unverified until evidence is supplied.

The baseline contains 21 scenes, 126 CPU/GPU views and 224 load records per
1040/1807 graphics/sounds package. It checks native save/load and restores
absolute XYZ/moves/calendar/map-Z. It does not establish that inventory,
reload, furniture interactions and a prolonged ordinary game are comfortable.

## Defect and verification register

| ID | Priority | Evidence | Status / next verification |
| --- | --- | --- | --- |
| PERF-01 | P1 | 0.12 dense smoke at 640x480 on software D3D12: median 251.046 ms (1040 graphics), 272.111 ms (1040 sounds), 276.457 ms (1807 graphics). Volume shader performs four sparse binary searches per ray sample, even while remaining in the same neighbour quartet. | Confirmed slow scene; test sample-local lookup reuse with identical images and actual SDL timing. Hardware attribution remains unmeasured. |
| PERF-02 | P1 | Stable world publication still visits 197589 candidate cells across 21 loaded levels; visibility stage is approximately 13–15 ms. Source-cache misses are zero. | Profile/cull candidate work while retaining native perception, neighbouring-floor structure, airborne items and creatures. |
| CAM-01 | P1 | World callback supplies only a Z offset; XY camera offsets remain zero despite both renderers supporting them. Adjacent native walking steps therefore jump visually between tile centres. | Add bounded visual-only interpolation for safe adjacent walking, with cancellation for teleports, opaque paths and vehicles. Native position and move cost remain authoritative. |
| CAM-02 | P1 | Vertical easing starts before the first expensive render. Recorded native upward transition first-ready is 370.814 ms, longer than the 180 ms interpolation target. A slow first render can consume the entire easing interval. | Start the interpolation clock after the first successful presentation; test interrupted moves and slow first frames. |
| CAM-03 | P1 | Player attack presentation is committed after command submission; a slow GPU presentation can consume its opening keyframes. Native attack notifications already observe real melee/shooting pathways. | Commit the player attack clock after successful SDL window presentation, sharing the camera acknowledgement. Native action costs and animation clips are unchanged. |
| CAM-04 | P1 | Accumulated tall-avatar stair easing plus headroom correction can exceed the existing 5 m scene bound although each offset is valid separately. | Cancel excessive visual lag before publication; add rapid native stairs with a tall avatar. |
| QA-01 | Verification gap | Existing GPU metrics combine command submission and do not isolate world/creature/volume/composition work or measure GPU execution. | Add diagnostic phase accounting and a reproducible local benchmark; label CPU submission, present wall time and GPU execution separately. |
| QA-02 | Verification gap | Existing real SDL fixtures cover geometry and state guards rather than an ordinary 30-minute manual session. | Add bounded automated gameplay/soak scenarios; report exactly which native actions were exercised. Manual session remains separate. |
| QA-03 | Verification gap | Actual UDP Hybrid v3 resources are available at `Neversalimus/CDDA-Mods@59123092c33244d516fa9d5ca69eaafb8d487f37`, `mods/undeadpeople/content`, identity `UndeadPeople_0J_Hybrid_v3` (2026-09-23). UltimateCataclysm was the previous runtime asset source. | Run a separate native SDL audit with these exact assets. Runtime compatibility remains unverified until that run passes. |
| CI-01 | Infrastructure | Every relevant PR update currently starts the expensive Host matrix. | Keep quick checks automatic; run costly integration/final qualification at deliberate checkpoints, without removing release assertions. |

No new P0 defect has been confirmed at this checkpoint. P0 inspection must
cover crash/save safety, native visibility, target actions, renderer fallback
and vertical transition state. Passing the baseline does not close these new
gameplay-audit questions.

## QA schedule

1. Quick tests: directly affected camera, geometry/cache, action and shader
   controls after each change.
2. Integration: actual SDL audit and paired performance after a coherent group
   of fixes, with isolated owned temporary worlds and deterministic seeds.
3. Final candidate: native/ASan/UBSan, source transforms, GPU controls, both
   pinned Hosts, installation/repair/rollback/Restore vanilla, graphics/sounds,
   paired world/appearance evidence, native saves and resource lifecycle.

All timing comparisons for this phase use exact **0.12 main** as Before.
The earlier 0.11 comparison is historical evidence only. Report actual render
dimensions, adapter identity, p95/p99, cold frames and remaining limitations.

## Implemented candidate and checkpoints

- Camera policy tests cover a slow first render, interruption/reversal, diagonal
  bounds, cancellation, invalid input and clock rollback. Native walking, stairs,
  basement, ceiling and vehicle checks are added to the SDL integration audit.
- Shader lookup reuse compiles to all 14 SPIR-V/DXIL programs. GPU qualification
  run `38645746009` passed on Windows and Linux; 54 paired captures are identical
  to 0.12 pixel-for-pixel. This establishes those image controls, not a speedup.
- The audit uses real `avatar_action::move`, door methods, pickup, inventory
  cancellation, item activation, melee/fire, reload completion, vertical moves,
  vehicle control/displacement and native turn processing. Reload's interactive
  activity UI and driving physics remain separate from these automation checks.
- Private optional GPU profiling records CPU command work for world, creatures,
  held items, shadows, volume and composition. Full-frame wall time includes real
  SDL presentation. GPU execution timestamps remain unavailable.

Run quick checks on each PR update. To request one expensive checkpoint, add
`<!-- fp-qa:integration@FULL_HEAD_SHA -->` or
`<!-- fp-qa:full@FULL_HEAD_SHA -->` to its description. The marker matches only
that exact head, so a later small push cannot start another Host matrix.
Full qualification retains both pins, graphics/sounds, installer transactions,
all existing world/appearance checks and the exact old **Host plus DLL**.

## Local GPU and prolonged automation

The TEST installer includes `Run-FirstPersonBenchmark.ps1` and its small data
fixture. After extracting it, use Windows PowerShell:

```powershell
.\Run-FirstPersonBenchmark.ps1 -GameRoot 'D:\Games\CDDA-1807' `
  -OutputRoot 'D:\FP-Benchmarks\candidate-0121' `
  -TilesetRoot 'D:\Tilesets\UndeadPeople_0J_Hybrid_v3' -SoakSeconds 1800
```

The output folder must be new and outside the game/installer folders. The script
copies installation assets and the verified bundled Host/modules into it, keeps
player saves/configuration out of the clone, and records native JSON, BMPs,
adapter identity, asset hashes and memory samples. It never rewrites the source
installation. `TilesetRoot` is optional; omitting it does not verify UDP.
Keep software adapters labelled as such. The 30-minute loop is automated;
comfort, full inventory/reload interactions and a manual driving session still
require a player. Inspect RSS across warmed cycles separately from bounded
renderer cache counters and from native map loading.

The native combat audit now runs six actual AI turns with 1, 5 and 20 enemies,
including player attacks and CPU/GPU render-state guards at the same simulation
state. Its disposable avatar uses CDDA's existing debug protection; damage to
enemy targets and enemy AI use native pathways. This is a controlled stress
trial, not evidence for ordinary combat balance or manual play. Full soaking
also exports frame/loop samples and actual renderer tags for drift analysis.

The first Host compile found private UI entry points and an enum argument in
the new audit. Explicit nonvirtual diagnostic gateways call the original game
methods; the vehicle fixture uses the same UNDAMAGED enum as the established
smoke test. Module API tables and class data layouts are not extended.

The first native audit stopped inside the pickup/inventory/item-use group on
both pins. Menu cancellation now feeds Escape through SDL repeatedly while the
native UI owns input, rather than relying on one key queued before UI redraw.
Fine-grained progress is flushed to a separate journal and the runner fails
after 180 seconds without progress, preserving the last action. This is an
automation correction; it is not classified as a confirmed gameplay defect.
Soak day/night cycling alternates two fixed instants and restores the actual
post-action simulation time before the final native save/load checks.

At c928cfea the inventory returned normally after 233 ms, but the checkpoint
after it was too broad to identify the following stop. The a988dc0d owned-window
capture identifies the actual cause: `ammo_set` raised CDDA's modal diagnostic
"Tried to set invalid ammo of battery for flashlight". Rendering had resumed.
The fixture now charges a real medium battery using its default ammo and
inserts it into the flashlight's magazine well, matching CDDA's own battery
tests. Native debug messages within action trials are captured and fail the
trial instead of blocking at a modal prompt. This corrects test setup, not a
confirmed production renderer defect. The runner retains native error logs,
stdout/stderr and a stalled-window capture for further failures.
The local runner also passes an explicit owned `--userdir`, keeping configuration,
worlds and native debug logs inside its new clone even on home-directory builds.

At d420e1ba, native walking, inventory/item use, combat/reload and both stair
trials passed on 1040. The vehicle fixture then exposed an omitted prerequisite
of `map::displace_vehicle`: its passenger calculation consumes next-step mount
coordinates. The diagnostic now uses the same `precalc_mounts` and pivot-offset
preparation as `map::move_vehicle`, and checks native passenger displacement
separately from visual camera lag. It still does not qualify driving physics.

At b427d915 the vehicle trial passed; the first native AI turn hit SIGSEGV.
The canonical API smoke calls game::setup and directly constructs an avatar,
sets new_game=false and never calls game::start_game. Its game-mode pointer is
therefore unset, whereas do_turn immediately invokes gamemode->per_turn. The
opt-in audit now creates the same default special_game as start_game through a
nonvirtual diagnostic gateway, and retains the main UI adaptor as run_game_loop
does. The ordinary gameplay startup and production turn implementation are
unchanged. The runner also preserves config/crash.log.

At 657f1361 all ten native action groups and the fixed-scene profiling cases
passed on 1807. The rotation loop then changed the viewpoint after warming and
incorrectly required every cold view to be an already-ready GPU frame. The
existing producer deliberately generates at most 512 cells per frame and uses
safe CPU fallback while warming. The audit now records continuous rotation's
actual renderer tags, frame times and recovery latency separately from 24
warmed-pose CPU/GPU samples. Stable GPU frames and bounded recovery remain
mandatory; warmup frames are excluded from steady stage percentiles. Cold-view
fallback remains an explicitly measured performance limitation, not a claimed
speedup or a discrete-GPU result.
