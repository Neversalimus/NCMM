# First Person diagnostics

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
The outer rotation timer includes native cache refresh; the nested frame phase
stream excludes that refresh. First-ready time includes the actual move,
refresh, rendered frame, presentation and instrumentation. Medians of nested
phases must not be added to form an invented frame time.

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

Before each native renderer reset, the host drains the shared SDL GPU device even if module disable already cleared the private backend. This covers queued SDL presentation copies and swapchain release. It is a lifecycle boundary, not a new per-frame fence. The retained 0.12.7 WARP stacks and matching-image metadata are preserved in qualification/0.12.8-warp-stacks.json.gz.
