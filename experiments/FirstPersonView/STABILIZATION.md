# First Person View 0.12.x stabilization

Baseline: `c5a0a0a5e12991df06ba000f80dd359f274b7f50` (merged PR #205).
Engine pins remain 1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83`
and 1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.
Scope: gameplay audit, measured performance, camera/interaction stability and
selective visual polish. No new graphics architecture or gameplay rules.

## Evidence and limitations

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
