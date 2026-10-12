# First Person View development checkpoints

Baseline: d8d22bd (0.6), PR 200. Target: CDDA 1040 and 1807 Windows.

Requested work is grouped into independently committed changes:
1. Semantic geometry and callback-scoped presentation data.
2. Sprite aim/name display and first-person click attacks.
3. Vehicle shell, hood, cockpit and interior ceilings.
4. Materials, liquids, dimension skies, volumes and source lighting.
5. Physical view models, attachments, grips and avatar appearance.
6. GPU atmosphere composition, CPU fallback, audit and installer qualification.

Checks must include native render regressions, actual game data taxonomy,
atomic source transforms, both pinned Host syntax checks and real Windows SDL
installer smoke. New code is not a delivered build until those checks pass.

Dimension references: pinned CDDA data/json/region_settings/region_settings/dimensions,
data/json/effects_on_condition/nether_eocs/reverberations.json (grey sky and
misty grasslands), string_dimension.json (star field), and
scarlet_storm_effect_on_condition.json (scarlet sky). Unknown mod dimensions
retain their actual weather tint instead of guessing from translated names.

Implementation checkpoints 1–6 are committed. Pinned 1040/1807 Host syntax,
404 presentation checks, 303 hybrid checks, the 496657-check legacy renderer
suite, 34 module lifecycle checks and five atomic-transform cases pass locally.
Release gates: real-SDL cases, Windows packaging and captured-image review.
The qualification workflow and each archive's release-manifest.json identify
the exact tested source revision. Do not ship before those gates succeed.

Additional performance checkpoint: prepare gas/light coefficients once per cell,
clip ray intervals, bound the scattering layer and split volume/compositor rows
between up to four workers. Random-alpha and odd-size outputs match the Host
reference compositor exactly. In the local 1280x800 cached-world smoke fixture,
composition dropped from 219 to 39 ms on CPU and GPU-layer preparation from
155 to 37 ms. These are synthetic scene timings, not player FPS.
The optimization and resin-seam repair change only the module; all 26 pinned
Host input hashes remain identical for each game build, allowing exact cached
Hosts to be reused for the final installer qualification.

Final accessory audit covers all nine supported attachment types, simultaneous
light/laser mounts and canonical revolver IDs, including enchanted variants.

## 0.7.1 continuation

The floor-sample SDL fixture now drives the DLL camera through real actions,
asserts its downward angle and restores it, instead of changing only the Host echo.
Raster preparation clips/projects each triangle once. Interleaved 16-row strips
spread lower-screen floor and view-model work across the workers, preserving
submission order and disjoint writes. Thirty reference frames at 640x480 and
1280x800 match the previous implementation byte for byte across color, world/held
depth, creature picking and ownership, including glass, doors, near clipping,
water and firearms. Initial isolated camera benchmarks at 1280x800 improve clear
floor from 50 to 29 ms, walls from 45 to 34 ms and water from 52 to 31 ms; timing
varies with host load and these numbers do not represent in-game player FPS.

Water now shades a shallow bed along actual known land, adds narrow sea wash,
and blends subpixel puddle edges. Cardinal/diagonal bank distances meet at tile
boundaries and unknown/no-floor cells do not invent beaches. Presentation tests
now contain 461 checks, including rendered shoreline pixels and edge continuity.
`FIRST_PERSON_BUILD_BENCHMARK` builds a manual reproducible timing tool without
adding a machine-dependent performance gate. Version advanced to 0.7.1-dev;
Windows installer qualification is pending for this new revision.

Visual review found a pre-existing artificial shadow at the snapshot boundary:
unknown cells applied a 0.7 sunlight multiplier to otherwise open ground. Only
known occluders now add directional shadows; CDDA's actual cell lighting remains
the source of ambient/local light. Regression cases cover empty ground, hidden
walls and a visible wall. This is a module-only follow-up and does not change the
26 prepared Host inputs or the engine cache key.

## 0.8.0 extended view distance

The V2 environment radius increases from 16 to 48 cells (33×33 to 97×97), while
Graphics V1 remains unchanged. Unloaded cells remain absent, with no generation
or simulation-radius changes. A fresh transparency byte grid is rebuilt for
each draw; the engine's original Bresenham path stops at the first blocker
without allocating per-target point vectors or repeatedly querying terrain.
Opaque target surfaces remain visible and vehicles retain their 3D occlusion.
Small wheels/gauges/steering rings use fewer subdivisions by projected pixel
size, preserving close-up detail and increasing detail with raster quality.

The renderer validates the larger bounded scene, and smoke/source-light
integration extends past the previous 19-cell limit with transmittance-correct
distant intervals. Gas intervals remain short enough for single-cell plumes.
Native range cases cover distant geometry and creature picking, a blocker
beyond the previous edge, missing cells, all camera directions, gas forty
cells ahead and rejection of oversized storage. The real SDL street fixture
includes distant buildings and compares cached rays with the original engine
line over multiple slopes. `first_person_benchmark 12 distance` compares radii
16 and 48 on open ground, forest, vehicles and smoke at 1280×800, separating
changing-camera and cached-world composition costs.

Release gates: native/transform/syntax checks, both Windows Host builds,
installer transactions, real SDL distance capture and image review. The 0.7.1
installer remains the last qualified package until all 0.8.0 gates pass.

The first real capture exposed a pre-existing weather-unit error: CDDA uses
1.0 for normal sight, which previously added 0.025 fog even in sunny weather.
Weather haze now uses only the excess above 1.0; actual fog/mist and rain still
contribute. The distance fixture captures an unobstructed street before its
near car is created and asserts the live clear-weather fog density.


## Qualified 0.8.0 baseline

Workflow 38180230357 and source/package audit 38180230372 pass at f3e7f736.
The final 1040 installer archive was downloaded and verified (104 local archive
checks, all 32 payload hashes, final merge b191a277). Both official-package SDL
receipts pass 4578 checks with radius 48, 1566 distant cells and clear-weather
fog 0.0. Final distance, car-interior and smoke captures were inspected.

## 0.8.1 continuation

Implemented conservative model/quad bounds, deferred hidden-sky shading,
parallel sky/cloud rows, active-volume ray rejection, lazy gas noise, dense
callback-local sprite references and word-wise single-pass scene invalidation.
Host apparent-light queries now occur after camera/terrain visibility rejection.
Active redraw scheduling increases from 24 to 16 ms without advancing game time.
Continuous brick/drywall surfaces keep tileset palettes; soft contact shadows
anchor feet, trunks and tyres. The 48-cell radius and ABI layouts remain.

Build options are consolidated; lifecycle/contract tests link the shipping
shared module. CI produces median/p95/work-counter reports, adds Linux sanitizer
coverage, waits for native checks before Host jobs and records actual compiler
cache bypasses. Local frame-equivalence and native suites pass; final expanded
regressions, pinned Host syntax, Windows installers and SDL capture review are
being qualified. The 0.8.0 installer remains the last qualified package until
all 0.8.1 gates pass.


## 0.9.0 procedural-creature development checkpoint

Work is isolated in `codex/first-person-procedural-creatures` / PR #201 on top
of the qualified 0.8.1 PR #200 head. All seven implementation stages are persisted:
semantic contract/classifier, anatomical meshes/materials, joint animation,
eight-angle depth impostors/LOD, bounded worker/cache, and live Host integration.
The new dynamic pass reuses opaque world pixels and owns deferred transparency.

Thirteen native suites pass locally, including the shipped shared library.
Fresh canonical preparations for both pinned CDDA revisions accepted the event
hooks; GCC syntax checks passed against both sets of real headers. The extended
NPC overlay, diagnostic and SDL gallery work is being qualified next. MSVC/SDL
and installer qualification for this feature are still pending; no claim of
release completion or real performance improvement is made at this checkpoint.

Audit fixes cover live NPC posture (including distant prone impostors), animated
pose bounds, spider body continuity/leg attachment, speed-sensitive gait,
fractional creature masses, stable 2D source frames and reused NPC overlay
readbacks. The latest local 13-suite run passes; real-header checks include the
modified tileset lookup and combat pathways. The paired SDL fixture retains
runtime anatomy/fallback/cache-pressure counters alongside timing data.
