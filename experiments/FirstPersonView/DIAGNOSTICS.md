# First Person diagnostics

## Native firing UI context

The gameplay audit now dispatches the ordinary game command through SDL input,
then processes the native aim activity and target UI. Its private qualification
gateway calls `game::handle_action`; it does not fabricate an attack notification.
One legacy-policy cancel trial must reproduce the old native-map presentation.
The fixed policy then checks 126 attempts on CPU/GPU, including confirmed single
and automatic shots, cancellation, an empty weapon, no target, a window, a blocked
wall and a vertical cancellation. Each successful native window presentation
has a lossless PNG, mode/backend state and a contiguous trial/frame receipt.
`verify_fire_audit.py` checks image integrity and rejects any intermediate map
frame or silent CPU fallback. These instrumented runs are not FPS measurements.

The initial LMB fixture holds RMB look and uses the real native picking/click
path. Pointer-only LMB, a successful vertical shot, fault handling and vehicle
fire require additional cases. The controlled series advances native move/ammo
costs but not ordinary enemy AI. The existing combat/turn audit remains separate.
Successful compilation or context unit tests do not qualify this native series;
its completed process, captures and verifier must pass on both pinned Hosts.

For 0.13 development, the existing runner also accepts `-MovementBenchmark -Extended`.
This opt-in matrix records 10 scenes at 640x480, 1280x800, 1768x1084 and 1920x1080,
on CPU and GPU, with 64 actual native steps and 96 cold/warm turns per record.
It retains the existing raw exclusive frame phases and move-to-first-ready scope,
and adds actual internal dimensions, process RSS, static/dynamic geometry uploads,
draw calls, palette matrices and Host/GPU cache bytes. These counters measure CPU
preparation/command submission; they are not GPU execution timestamps. A 64-step
p99 still cannot establish the frequency of very rare stalls.

`-ViewmodelGallery` belongs to the gameplay audit and captures 27 native states
on both renderers and both tilesets, at a 1280x800 viewport. Garments, mutations,
CBMs, weapon geometry, boarding and attacks come from the disposable avatar's
real CDDA state. Attacks must advance the confirmed native serial; no fabricated
reload/use event is injected. Pair this mode with the exact same instrumented
Host and game data when comparing an old and a new DLL. Record any mixed source
components explicitly: this development comparison does not qualify a release.
For a quick visual pass use `-SoakSeconds 0`; that is deliberately not a long-soak
qualification and the ordinary long-audit verifier continues to reject it.

The new modes are opt-in only. The standard qualification matrix and normal
gameplay remain unchanged. All diagnostic environment flags are isolated and
restored by the existing runner.

The diagnostic runner uses a fresh copy of installation assets. It does not copy player saves or configuration. Game data and the installer Host must have the same supported engine pin. The output directory must be new and outside both source directories. The runner rejects OutputRoot paths longer than 80 characters before copying a game. This conservative harness budget leaves room for encoded avatar names and nested memory archives; it is not an upstream native-save long-path fix. Use a short path such as `C:\ncmm-test\run-1`.

Windows PowerShell 5.1 and PowerShell 7 are supported. Before launch, competing diagnostic environment flags are removed rather than assigned an empty value: native `getenv` treats an empty flag as enabled. The caller's absent, empty or populated environment is restored even when the child fails. `tests/Test-BenchmarkEnvironment.ps1` checks the actual isolation and restoration code without starting the game.

The new diagnostic copy omits CDDA's derived `data/cache` flexbuffers so its injected JSON fixture cannot conflict with an old installation cache. Source assets, saves and source cache remain unchanged. `tests/Test-BenchmarkDataCopy.ps1` verifies that boundary. Window-capture diagnostics compile against the active PowerShell drawing assemblies; `tests/Test-BenchmarkCapture.ps1` checks compilation without capturing a window.

## Reproduce a GPU failure

From the extracted TEST installer, run:

~~~powershell
.\Run-FirstPersonBenchmark.ps1 -GameRoot 'C:\Games\CDDA' -OutputRoot 'C:\fpv-qa-new' -FocusedGpuBenchmark -CaptureCrashDump -GpuDebug
~~~

This renders the smoke/gas and one-creature fixtures on CPU and GPU, with 96 measured frames per case. It uses the official tileset. The full gameplay and UDP audit is a separate mode.

CaptureCrashDump attaches a diagnostic debugger only to the new child that this invocation launches. It records first-chance access violations, module addresses, small thread/stack dumps and the real process exit. GpuDebug enables SDL GPU validation. SDL assertions abort the diagnostic child instead of leaving an unattended modal dialog. The debugger leaves ordinary exception handling unchanged.

Keep the complete evidence directory, especially:

- first-person-process-result.json: actual exit code, final progress, binary hashes and debugger status, including failed runs.
- first-person-debug-events.log: module addresses, exceptions and bounded debug-layer messages; critical errors remain visible after the warning limit.
- first-person-crash-*.dmp and *.stack.txt: postmortem thread stacks. The packaged analyzer reads dumps; it does not attach to another process.
- first-person-native-stderr.log and first-person-gpu-benchmark.json: assertions and completed measured records.

An earlier bootstrap report with reason=ok does not establish that the focused matrix, teardown or process exit succeeded. A valid JSON profile is insufficient when the child exits with an error.

## Measure walking

~~~powershell
.\Run-FirstPersonBenchmark.ps1 -GameRoot 'C:\Games\CDDA' -OutputRoot 'C:\fpv-walk-new' -MovementBenchmark -TilesetRoot 'C:\Tilesets\UndeadPeople_0J_Hybrid_v3'
~~~

The movement matrix records two viewport sizes, CPU/GPU modes, three scenes, two 48-angle rotations and twelve native steps. It advances no ordinary AI turns. Gameplay, combat, save/load and soak coverage run separately.

first_frame_after_step_samples_ms measures the first rendered/presented frame after each step; it excludes the preceding native move and refresh. first_ready_samples_ms includes those operations. first_frame_indices identifies the twelve frames in the full stream.

Native tiles, perspective, present and unattributed phase arrays are exclusive and sum to each frame's wall time. Extraction, callback and GPU preparation arrays are nested diagnostics and must not be added to those exclusive phases. GPU preparation arrays are meaningful only for frames tagged as GPU; older diagnostics can retain prior-frame metrics. Current native CPU frames zero the inactive GPU metrics, and the comparison tool omits GPU arrays from CPU summaries. They are CPU wall times, not GPU execution timestamps or player FPS. Preserve resolution, internal rendering limits, tileset, weather, device and binary hashes when comparing versions. Fresh Balanced uses a 1920x1080 cap on GPU and 960x600 on CPU. CPU High uses 1280x800; historical summaries misstated that value as the Balanced cap. These runtime caps are unchanged. The movement report records the viewport, not internal raster dimensions.

## Qualification requirements

### Extended 0.13 diagnostics

Add `-Extended` to `-MovementBenchmark` for the strict 80-record matrix:
street, city blocks, dense city, forest, multi-level horde, 20/100 creatures,
smoke/rain, armed and empty-hand attacks; 640x480, 1280x800, 1768x1084 and
1920x1080 viewports; CPU and GPU. Each record has 64 actual native steps and
96 cold plus 96 warm camera samples. Empty attacks require sixteen confirmed
native melee serials per record. Stamina starts identically for each entry.

Extended records include actual internal dimensions, raw upload/draw/bone
counts, resource/cache bytes and process RSS. They use the same frame path as
ordinary gameplay. Resource and stage counters add diagnostic overhead; report
that scope and keep instrumentation identical in each paired run. RSS is a
whole-process measurement and must not be described as viewmodel allocation.
Each record also retains its initial rendered/presented frame and initial
geometry/resource/upload costs before timed rotations and walking. This is the
first frame of that record, not a claim that all process/device caches are cold.
The outer rotation timer includes native cache refresh; the nested frame phase
stream excludes that refresh. First-ready time includes the actual move,
refresh, rendered frame, presentation and instrumentation. Medians of nested
phases must not be added to form an invented frame time.

Extended records also retain main-thread and whole-process CPU time from the
native tile draw through SDL presentation, alongside wall time. Windows CPU
accounting ticks can be coarser than a frame: zero samples do not mean free work;
compare aggregate intervals. Process CPU includes workers and can exceed wall
time. Unsupported thread accounting is marked unavailable. These exclude the
outer move/cache refresh and subsequent resource/RSS collection. They are not
GPU execution timestamps. Opt-in sprite readback attempts/time and retained
sprite bytes expose SDL atlas readback waits; the timer includes readback setup
and restoration and counts failed attempts too. GPU generated-cell counts
distinguish real chunk rebuilds from resource deduplication; CPU-mode GPU fields
are inactive. Ordinary gameplay does not enable these diagnostic timers.
`gpu_frame_upload_capacity_bytes` reports retained SDL upload-staging capacity
separately from logical GPU targets/assets. It is not measured VRAM or the sum
of SDL's internally cycled backing allocations. `gpu_frame_upload_creations`
counts staging-object creation/growth within the current device lifecycle;
repeated poses should retain the same object. The combined logical target,
resource and retained-staging budget remains bounded.

`tests/compare_movement_runs.py --extended` explicitly selects this larger matrix
and verifies its complete sample streams and recorded resolution. The default
12-record release gate remains unchanged. Component ablations may explicitly
supply `--baseline-host-source` and `--candidate-host-source` with full expected
commits. They require matching `diagnostic_mixed_components` receipts and all
payload hashes, and are labeled diagnostic snapshots, never qualified installers.
Extended scenes select their own native sunny/rain states and record their IDs.
`--single-extended-weather clear` compares one complete 80-record run per version
against the pinned clear input file; the smoke/rain scene still uses real rain.
This avoids duplicating the entire extended matrix. The legacy release route
still requires both weather fixtures and rejects this option.

Add `-ViewmodelGallery -SoakSeconds 0` to the gameplay audit for 27 states in
CPU/GPU for each of the official and UDP tilesets, at 1280x800. The gallery uses
real worn garments, item IDs, gunmods, mutations, CBMs, boarding and confirmed
native melee/shot actions. It retains 108 actual SDL BMP captures when both
tilesets are present. Zero soak is an inspection run, not long-session coverage.
Do not mix `-ViewmodelGallery` with movement or focused GPU modes.

The existing optional `first_person_benchmark` also accepts `4096 viewmodel
pose.json`. This measures source TRS sampling, two IK solves and weighted
palette matrices in isolation, with 32 warmup iterations. It retains raw samples
in their original order. It excludes material preparation, packet construction,
CPU rasterization, GPU execution, world extraction and presentation; it must not
be reported as the complete cost of a native frame or viewmodel draw.
The separate `first_prepare_ms` field retains each state's first CPU preparation
before the 32 warmups. Only the empty state includes the process's first asset
validation; held/firearm reuse those mapped assets. It excludes executable/DLL
mapping, material preparation and GPU resource creation. Repeating fresh processes
can measure its distribution, without claiming a cold OS file cache or GPU.

### Required release gates

The live heartbeat reader tolerates legacy locale-specific number separators,
including malformed UTF-8. The 180-second stall limit still requires changing
native progress; child exit status and complete evidence remain mandatory.

The renderer-created device disables clip distance, depth clamping, indirect first-instance and anisotropy features. Standalone GPU tests deliberately use the same feature profile with validation enabled. They include 32 backend replacements with queued work, alpha/depth comparisons, animated bounds and multi-Z picking.

The native release gate requires eight independent focused children per engine pin, each with strict validation, no crash dump and exit zero. One failed process fails the gate. Evidence is uploaded even on failure. Host PDBs are retained with the matching build. Resource teardown flushes SDL composition and waits for the shared device only at resource-reset boundaries.

The separate old-binary reproduction workflow is opt-in through workflow_dispatch or the exact-head PR marker fp-crash-baseline@SHA. It uses pinned 0.12.7 archives and verified official data; its expected baseline failures are diagnostic evidence, not qualification of a new build.

## Compare the exact hardware packages

The comparison tool rejects mismatched Host/DLL bytes, engine pins, weather, UDP assets, software rendering, incomplete phase streams and abnormal process exits. Use the two extracted package directories and the four fresh output directories:

~~~powershell
python tests/compare_movement_runs.py --baseline-bundle C:\fp127 --candidate-bundle C:\fp128 --baseline-clear C:\baseline-clear --baseline-rain C:\baseline-rain --candidate-clear C:\candidate-clear --candidate-rain C:\candidate-rain --expected-candidate-source QUALIFIED_COMMIT --output comparison.json
~~~

This paired route requires the exact 1040 engine and RTX 4090 / Direct3D 12. It counts all 72 full-size GPU first-step samples against 15 ms and reports regressions as well as improvements. A successful comparison means the evidence is consistent; it does not mean the latency target was met. Clear and controlled-rain descriptors are pinned by hash. The rain fixture is not a substitute for broader weather coverage.

The optional tests/cache_shift_benchmark.cpp in the source repository measures only source/geometry cache-window shifts. Build instructions are in its first lines. Its timings are not native frame latency or game FPS.

Extended movement records optionally include `gpu_cell_prepare_samples_ms`,
`gpu_cell_upload_bytes` and `gpu_cell_cache_bytes`. Preparation compares complete
owned cell records on the CPU; it is nested in the callback, outside the upload
timer. Upload bytes report actual cell transfers, while cache bytes report the
retained CPU snapshot. The Host CPU cache total includes this snapshot. GPU
targets/assets, staging capacity and the cell snapshot have separate counters;
their combined budget is bounded. Older missing fields mean unavailable.

Before each native renderer reset, the host drains the shared SDL GPU device even if module disable already cleared the private backend. This covers queued SDL presentation copies and swapchain release. It is a lifecycle boundary, not a new per-frame fence. The retained 0.12.7 WARP stacks and matching-image metadata are preserved in qualification/0.12.8-warp-stacks.json.gz.

## Camera snapshot experiment and live tracing

The Host may reuse owned, camera-independent cell data only while CDDA's main
thread is blocked in the ordinary input loop. Camera actions preserve this
lease. Unknown/native actions, nested UI, other module callbacks and native map
changes revoke its generation. Position/eye, source revisions, loaded Z range,
viewport, renderer texture generation and tileset independently guard reuse.
This is not a turn-number cache. Actors, death clips and vehicles continue on
their live path. The complete native tile pass still runs on every redraw.

The existing sparse snapshot cache acquires a new side/back sector lazily.
Frustum rejections never become cached native occlusion. Its owned cell,
appearance and physical-light payload is capped at 96 MiB including capacity;
no item, Creature or atlas pointer is added to it. Substrate matching and camera
culling still run against the current submitted cells. Existing GPU mesh/resource
caches remain authoritative for their own geometry and upload lifetimes.

`Run-FirstPersonBenchmark.ps1 -MovementBenchmark -CameraSnapshotVerify` repeats
native extraction and compares permissions, IDs, lights, shape/detail and
presentation against the owned data. Each record must contain at least 100
actual comparisons and zero mismatches. This diagnostic is intentionally slow.
`-DisableCameraSnapshot` supplies the same-binary control. Both switches are
temporary diagnostics and their environment is restored, including on failure.
Movement records identify their controlled immutable-fixture input lease and
retain per-frame hit/miss, comparison, generation and byte counters. This is
automated qualification, not a manual combat session.

For profiling only, `NCMM_FP_TRACE_MARKERS=1` enables CPU NVTX ranges and SDL GPU
debug groups. The unmodified NVTX core is header-only and pinned under
`graphics_bridge/third_party/nvtx3`; no installed SDK or additional profiler DLL
is required to build or run the game. Profiler-off measurements leave markers
disabled. Native Update covers resolved action dispatch; Input Wait identifies
blocking menus/targeting separately. Native 2D, Snapshot, Visibility, Geometry,
Resource Preparation, Upload, World, Creatures, Hands/Items, Effects, Target HUD
and Present identify the remaining stages. Nested ranges are not additive.
These annotations do not convert CPU submit/record timers into GPU execution
time. Their actual presence in a live hardware capture must still be checked.

Nsight's CLI must receive `--platform="Windows (x86_64)"` as one argument.
Passing `--platform` and its value as separate arguments lets Qt consume it as
its own QPA plugin selection. Omit `--hostname` for a local launch. Capture
settings (including Nsight's enforced VSync-off default and clock mode) must be
recorded separately from ordinary profiler-off timings. A startup capture is
only connection validation and does not qualify a first-person scene.
