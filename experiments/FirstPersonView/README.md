# First Person View 0.12.3 — movement and visual fixes

Creatures use the complete original tileset artwork by default, including UDP
and Monster Art Pack silhouettes, colours and proportions. NCMM module settings
retain the experimental reconstructed volume option. Artwork is copied once per
appearance; an owned copy survives death presentation and resets with the world
or tileset. No new creature artwork is substituted.

Facade visibility tests the actual wall faces and height, including an upper
wall partly obscured by an overhang. Upper brick facades retain their brick
finish. Interpolated walking reuses visibility
at the authoritative avatar position; stairs retain the displayed camera height.
Derived structural flags are memoized. Roof columns are resolved on demand only
as far as their first roof; opaque slabs trace only their exposed surface.
Cells outside native visual range are rejected before ray tracing.
GPU geometry completes a changed snapshot without a 512-cell warmup/CPU detour.
GPU balanced/high caps are 1920×1080 / 2560×1440; CPU fallback uses
960×600 / 1280×800 to keep input responsive.

`Run-FirstPersonBenchmark.ps1 -GameRoot ... -OutputRoot ... -MovementBenchmark`
measures native steps and easing in street and multi-storey horde scenes at
640×480 and 1768×1084. Add `-TilesetRoot ...` for actual UDP Hybrid v3.
`-FocusedGpuBenchmark` retains the separate 96-frame smoke/one-creature profile.
Candidate verification and measured limitations are recorded in STABILIZATION.md.

Loaded Z-levels, floors, ceilings, stairs and shafts use the existing 0.12 renderer.
Version 0.12.1 adds bounded visual walking/stair interpolation after successful
presentation, safer tall-avatar headroom, player-attack timing fixes and fewer
repeated volume/creature lookups. Native turn costs and visibility are unchanged.

See [STABILIZATION.md](STABILIZATION.md) for qualification, timings and open manual
checks. The 30-minute session is automated; comfortable ordinary gameplay and
discrete-GPU performance are not yet qualified.

## Creature Appearance Reconstruction 0.11

See [APPEARANCE.md](APPEARANCE.md) for the optional reconstructed models. GPU and
CPU use the same artwork or volume selection and preserve normal depth/picking.

# First Person View — 0.10.0 GPU development

Version 0.10 uses the shared SDL GPU device for 3D geometry, depth, skinning, materials and atmosphere. The previous CPU renderer remains available. NCMM module settings offer Auto/GPU/CPU; F12 now cycles these modes. See [GPU.md](GPU.md) for architecture, qualification evidence and hardware limits.

Optional first-person renderer of the same live CDDA world through NCMM. This
preview bundles an exact experimental Windows x64 Host for 1040 or 1807 and uses
the normal installer's independent component selection. It remains outside the
shipping mod catalog; the certified Host feed is not promoted.

Clear weather no longer adds fog for CDDA's normal sight multiplier of 1.0.
Rain and actual fog/mist retain weather haze, while distant clear-air terrain
remains legible throughout the expanded snapshot.

## Installation and controls

Extract the complete matching archive and run `NCMM_Setup_FirstPerson_TEST.exe`.
Select the official CDDA 2026-10-01-1040 or 2026-10-06-1807 installation containing
`cataclysm-tiles.exe` and `VERSION.txt`. Keep First Person View selected and choose
Install / Repair. Both official Windows graphics packages, with and without
sounds, are supported, including CatLauncher paths with Cyrillic characters and
nested backup history longer than 260 characters. All executable and payload
identities are checked with SHA256. The shared installer preserves the verified
original game backup when upgrading ordinary NCMM, preview NCMM or legacy CML.

| Key | Action |
| --- | --- |
| F6 | Toggle first person / map, with a message-log confirmation |
| F7 / F8 | Rotate left / right |
| F9 / F10 | Look up / down, including the sky |
| F11 | Balanced / high quality |
| F12 | Auto → CPU → GPU → Auto renderer |
| Left mouse button over the viewport | Attack the visible hostile creature under the centre reticle |
| Hold right mouse button in the viewport | Free horizontal/vertical look |
| ↑ / ↓ in first person | Forward / backward relative to the camera |
| ← / → in first person | Strafe left / right relative to the camera |
| Other movement bindings; arrows in map/menus/vehicle control | Existing directions |

Camera controls preserve the avatar's position, moves, time and destination.
Version 0.8 expands the first-person environment radius from 16 to 48 map cells:
the Host supplies a 97×97 snapshot instead of 33×33. Only already-loaded cells
inside the current reality bubble are included; drawing does not load terrain or
extend simulation. Walls and opaque furniture still block the scene, and
creatures/items retain CDDA's normal visibility rules. A per-frame transparency
grid and allocation-free engine Bresenham traversal reduce repeated map lookups.
The legacy Graphics V1 path retains its original radius and table layout.
Ordinary commands, adjacent selection and inventory keep the perspective
backdrop. Coordinate look, targeting and the overmap use their normal map view
and restore perspective on return to gameplay. No command-driven temporary
view cancellation remains. Mouse capture releases on menus, focus loss, F6 and
module unload. Camera pitch affects the view; walking stays on the CDDA map grid,
with ordinary collision checks and movement cost.

Uncheck First Person View and install again to remove its managed DLL/manifest.
Use the normal installer to return to the certified Host, or Restore vanilla EXE
for vanilla play. This preview uses its bundled Host offline. It includes NCMM
Runtime 0.8.2 and the normal optional native modules; game data and external
tilesets are not bundled.

## Hybrid environment and lighting

The new optional Graphics V2 bridge supplies callback-scoped CPU sprites, visible
cell geometry, current weather and calendar astronomy. V1 and Core ABI contracts
remain unchanged. The renderer uses clipped perspective-correct triangles, a
per-pixel depth buffer, bilinear filtering and a ground mip level, with a balanced default up to 1280×800 and an F11 high-quality option up to
1920×1080. Covered scanline spans, conservative frustum culling, outward solid
faces, front-to-back cells and up to four workers reduce work. Triangles are
clipped/projected once; interleaved 16-row strips balance detailed floor and
view-model work across the workers, with one writer per pixel.
Small distant wheels, gauges and steering rings use fewer subdivisions based on
their projected pixel size; close cars keep their existing detail and higher
raster quality retains finer distant geometry.
Identical UI redraws reuse complete owned pixels for at most 250 ms. World state,
light and camera changes invalidate them immediately; held-item changes and
attack/weather animation compose over a separate owned world frame. Attack poses
sample at 40 Hz and presentation redraws never advance game time. The SDL streaming
texture is reused across frames and recreated on size/device changes. Material
detail levels are calculated per triangle and interpolated across pixels. Unlike the old scanline floor, ground fills every
covered pixel. Drawing at this resolution costs more CPU time than the map.

Doors retain jambs and a hinged panel; destroyed doors retain a frame. Windows
have upright sills, frames and translucent glass; broken openings and opaque
curtains/boards are separate cases. Fences/gates have open vertical mesh, posts
and hinges. Plants have an independent visible-ground substrate; trees use trunks
and foliage billboards. Tables, storage, seats, rocks, signs and posts receive
shape-specific geometry. Boulders use irregular faceted meshes, including opaque
impassable boulders. Semantic flags take priority over identifier/looks-like
fallbacks; the engine fixture audits all loaded terrain/furniture definitions.
Spilled field liquids and visible loose liquid items remain shallow irregular
patches; bottles, frozen items and liquid-container furniture keep their normal
representation. Water surfaces have continuous
wave shading, a visible shallow bed near land, and sea wash along shores.
Neighbour-aware shore distances continue across tile edges; hidden/missing cells
and bridges over water do not create false beaches. Puddle edges use subpixel
coverage. Fire, smoke and gas have depth-clipped volume rendering.

Visible vehicle mounts share actual yaw and one continuous anchor. Automatic
meshes combine chassis, body panels, roof, seats, windows, open doors, modeled
wheels/rims and lights. Version 0.4 added sloped windshields, beveled hood/roof
edges, wheel-arch cutouts, rounded 24-segment tyres, rims/spokes, bumpers, grilles,
wipers, door seams/handles and mirrors. Visible parts share a tileset-derived
paint palette and a mount lookup; hidden parts affect neither colors nor shape.
Version 0.5 joined roof spans and removed phantom seats. Version 0.6 makes adjacent
roof panels meet without interior bevel gaps, adds dashboards and steering wheels
at actual CONTROLS mounts, distinguishes glass from solid upper panels and reduces
distant wheel geometry. Forward car geometry is independent of the 2D visibility
mask so its own windshield does not erase the rest of its roof.
Creature dimensions combine actual volume, body plan and game size category.
Vehicle models are generated from parts, with no authored mesh for each prototype.

Near surfaces receive procedural material detail for asphalt, soil, grass,
wood, stone and tiles. Detail fades with the projected footprint to reduce
distance shimmer. Painted bodywork and metal use view-dependent highlights.
Sprite filtering and mips use premultiplied alpha to avoid dark/colored fringes;
odd texture dimensions retain their edge pixels. Upright sprites are cropped
to their opaque bounds and rooted at the surface, including transparent padding.

Surfaces use CDDA's actual visible-cell light intensity and RGB light cache,
normal-dependent face shading, sunlight and geometry-based directional shadows.
This includes colored terrain/furniture lights and the game's existing item,
field and vehicle light propagation. Sky gradients, clouds, stars, sun and phased
moon respond to game time. Sun direction uses CDDA astronomy. CDDA exposes lunar
phases but no lunar orbit: moon position is approximated with a phase-shifted
solar trajectory. It is a visual time/orientation aid, not a second clock.

Weather follows the current game descriptor: cloud cover, wind, fog, rain/drizzle,
snow/flurries, lightning and portal storms. Acid/sleet/dust identifiers receive
specialized effects when supplied by game data or mods. Outside particles are
occluded by geometry and do not spawn under roofs. Visual cloud/particle movement
uses real time without advancing a game turn.

Enchanted `*_plus_*` weapons use their declared `looks_like` physical silhouette
so 2D grade marks do not become floating view-model geometry. The snapshot still
identifies the real enchanted item; ordinary weapons keep their authored art.

Short items use their actual tileset silhouette in camera space with
an extruded contour. Guns use forward-facing model families with their mounted
accessories. Reach/two-handed long weapons generate a wood shaft and metal
head, with both hands attached to the same grip points. Wrists, palms, fingers
and weapon share one pose transform. Actual committed melee attacks start a
swing, polearm attacks a thrust, and actual shots trigger recoil. A 480 ms eased
timeline recovers to the exact idle pose, including an unarmed attack pose.
Presentation starts on the first rendered frame so a long simulation turn cannot
consume an unseen clip. Disabling/unloading the view cancels queued animation.
Rotated silhouettes fit a lower-right view-model region
that leaves the reticle clear; low-opacity sprite halos are excluded. An independent view-model depth buffer handles overlaps
and nearby walls. It clears when the item is unwielded and preserves world occlusion depth. Hands and the item receive
the camera cell's actual RGB light. Garments supply colors and material finishes,
and visible mutations/CBMs modify the hands. There are no bespoke models for
every weapon or reload animations yet.

## Visibility and lifecycle

In V2 first person, forward terrain/furniture/vehicle cells are supplied without
the 2D light/visibility mask. The camera cone excludes distant rear/side cells;
opaque terrain/furniture walls block the snapshot and renderer depth handles car
occlusion. Black unknown-border planes are removed. This deliberately permits
seeing forward environment geometry beyond ordinary 2D visibility; actual light
still shades it. Creatures and loose items retain normal game visibility checks.
Ground inference, vehicle neighbors and sun occluders use this same forward
snapshot. Map memory still uses the normal terrain pass. V1 preserves its original
visible-cell contract. Core, V1 and V2 table layouts remain unchanged; camera and
attack state use an optional Controls V1 extension.

Gas and source-light integration follows the extended snapshot rather than
stopping at 19 cells. Near sampling remains unchanged; farther intervals grow
with distance and correct optical transmittance, with a tighter step for smoke
so a single distant gas cell is not skipped. Weather fog still limits clarity.

The bridge caches copied atlas pixels with bounded memory, invalidates them on
tileset/renderer changes and restores SDL state around readback. Modules receive
no engine/GPU handles. Complete frames are validated and copied before drawing;
invalid output falls back to the map. Unload/quarantine clears callbacks before
the DLL can be freed. A V1 fallback remains for the previous preview contract.

## Remaining limits

Tile-center simulation, one visible z-level, generic surface dimensions and foliage
billboards. Creature models are deterministic anatomical reconstructions; unseen
surfaces and unusual mod anatomy remain approximations. Vehicle and held
item geometry is generated and needs further interactive refinement for unusual
modded shapes. Raster resolution improves perspective sampling but cannot add
art detail absent from the installed tileset. Gameplay and saves remain CDDA's;
first-person controls add relative arrows, mouse look and aimed clicks. Player testing of combat, targeting, save/reload and
character switching remains necessary.

## Exact experimental targets

| Role | Official tag | Source commit |
| --- | --- | --- |
| Preview installer and Host | `cdda-experimental-2026-10-06-1807` | `074aa98bd5be3de4c35f154082db32a0e63bb0f1` |
| Preview installer and Host | `cdda-experimental-2026-10-01-1040` | `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83` |

The preparation script rejects every other source identity, including retired
0546. These source checks do **not** add a build to the certified Host feed.

## Development build

Build and run the isolated DLL tests from the repository root:

```powershell
cmake -S experiments/FirstPersonView -B fp-build -A x64
cmake --build fp-build --config Release
ctest --test-dir fp-build -C Release --output-on-failure
python -m unittest discover -s experiments/graphics_bridge -p test_apply_graphics_bridge.py -v
```

Prepare a **fresh disposable upstream worktree** at one of the exact commits:

```powershell
./experiments/FirstPersonView/Prepare-FirstPersonHost.ps1 -SourceRoot C:/Dev/cdda-fp
```

This reuses the canonical NCMM patch stack, then applies the optional Graphics
Bridge to the experimental engine source only. Entry points cover Host
query/lifecycle, gameplay dispatch, terrain viewport, input-context suspension,
SDL mouse/presentation and committed melee/shot events. Preparation preflights all anchors before writing the
experimental additions; reapplication is rejected. It produces
`first-person-source.json` marked `experimental-not-certified`, and no feed,
certification record or installed-game change.

With the pinned vcpkg/toolchain from the exact upstream checkout, build the normal
MSVC graphics target. The dedicated `ncmm-first-person-prototype.yml` workflow does
this for both 1040 and 1807, alongside Windows/Linux module tests and the preview installers.
The test package remains explicitly experimental. A regular released Host does
not advertise the graphics capability and rejects this DLL safely.

## Validation

Native tests cover continuous floors, geometry/occlusion, windows and mesh gates,
plant ground, vehicle meshes, hidden-cell exclusion, sky/phase changes, RGB light,
weather/roof exclusion, held-item replacement, viewport bounds and V1/V2 lifecycle.
The 0.6 tests cover all eight relative movement sectors, bounded mouse angles,
optional control-table fallback, attack/recovery composition, retained world depth,
weather composition and opaque boulder classification. The 0.5 tests cover redraw-cache invalidation, reticle clearance, creature size
and independent world depth. The 0.4 tests additionally cover alpha-edge contamination, odd-sized mips,
grounded transparent margins, hidden vehicle neighbors and view-model occlusion.
ASan/UBSan and atomic source-transform failure cases accompany source syntax
checks against both pinned engines.

Dedicated preview CI compiles actual MSVC Hosts, checks installation/repair/
rollback/vanilla restore with Windows long paths disabled, then launches both
official archive variants through the actual bootstrap. Its SDL fixture injects
F6, exercises camera controls without spending turns, audits all loaded terrain
and furniture, changes real doors/map memory, tests actual colored light at
night, sun/phase data, game weather, wielded item swaps, vehicles and ceilings.
It also exercises SDL mouse motion, menu cursor release, relative arrow mapping,
streaming-texture reuse, forward wall occlusion and a real melee attack.
It captures engine-rendered BMPs for visual review. Mock renderer images and
native CPU timings are fixture results, not game-session FPS measurements.

Existing CDDA/tileset assets retain their licenses and attribution; see
`THIRD_PARTY_NOTICES.txt` and the installed tileset's provenance.

## Version 0.7

The optional presentation interface supplies real creature volume/body plan,
localized names through the Host, dimension identity, garment coverage, gunmods,
light sources and fluid/gas classifications. Core, Graphics V1 and V2 layouts are
unchanged. Pixel picking respects sprite alpha and world depth. A click is
revalidated against the avatar's position, camera, visible target and hostility;
firing/reach use CDDA's normal readiness, aiming, range and trajectory code.

The renderer includes joined bonnets, cockpit gauges, vents, a dimensional wheel
and roof headlining when boarded. Control mounts determine visual front using a
stable full-vehicle summary. Architecture now includes horizontal scrap bridges,
raised guardrails, thin scrap panels, full-height seamless glass, and the Mi-Go
three-leaf resin hatch. Door families use wood, metal, glass, cloth, stone and
resin materials; ceilings use the actual roof terrain and varied finishes.

Eight procedural sky profiles cover Earth/weather, highlands, abyss, scarlet,
grey cabins, the star-filled string dimension, radiosphere and fungal worlds.
They use dimension IDs and the referenced game descriptions, never translated
name matching. Unknown dimension mods retain their authored weather tint. Earth
sun/moon still follow the calendar; non-Earth void skies do not invent an Earth
sun. Fluids use continuous wave normals, sky reflection and specular highlights;
spills have small irregular footprints. Salt water, sewage, blood, acid, slime,
lava, oil and other fluids have distinct responses.

Firearms use forward-facing procedural models and attached optics, suppressors,
foregrips, bipods, stocks, simultaneous lights/lasers, bayonets and underbarrel weapons. These are model
families, not individually authored replicas of every gun. Visible sleeves and
gloves supply their tileset colors and materials per hand; supported external
fur/scales/chitin/claws/webbing/arm armor and hand CBMs affect exposed surfaces.
Internal CBMs do not turn the entire hand metallic. Hands have breathing, step
motion, mouse inertia and committed-attack clips. Held-item dimensions use game
lengths and creature dimensions use volume plus body plan. Highly abstract
creatures are bounded to keep the projection stable.

Gases use a smooth 3D density field, depth-clipped ray marching and propagated
RGB lighting. An adaptive scattering layer (at most 256 pixels wide) is reconstructed/blended by
SDL's GPU renderer when available. **Geometry and fluid shading still use CPU.**
The CPU path uses the same layer contract; a failed effects upload/composite
redraws a complete CPU-composited frame without discarding the world view.
F12 forces CPU composition; `NCMM_FP_CPU=1` disables the accelerated path from
startup. Volume lighting is prepared once per cell; rays skip empty space and
workers own separate output rows. The ordinary CPU compositor also uses disjoint
row workers and matches the Host recovery compositor byte for byte. This is
partial GPU acceleration, not a replacement GPU mesh engine.

Qualification adds presentation/occlusion/composition regressions, a full loaded
monster and gas-definition audit, and actual SDL captures of reported geometry,
localized labels, a roofed car interior, an equipped M27 and smoke. Performance
numbers from fixtures are not a measurement of a player's installation.

## Reproducing renderer timings

Configure with `-DFIRST_PERSON_BUILD_BENCHMARK=ON -DCMAKE_BUILD_TYPE=Release`,
build `first_person_benchmark`, then run `first_person_benchmark 24` on an idle
machine. It separates changing-camera frames from cached-world composition at
1280x800 for floor, walls, water, CPU smoke and GPU-layer smoke. Compare the same
compiler/build type and repeat the run; these synthetic timings exclude Host
snapshot generation, simulation and SDL presentation and are not game FPS.


## 0.8.1 performance and materials

The 48-cell radius remains. Geometry uses conservative camera-plane bounds,
including continuous vehicle anchors and actual large/elevated creatures.
Offscreen quads are rejected before projection. The full known context still
supplies sun occluders, connected vehicle layout, liquid banks and RGB light;
render culling never changes CDDA's enemies, AI, turns or saves. The Host rejects
rear/outside-cone cells before querying apparent light. Active look/attack
redraws are scheduled every 16 ms, with slower idle redraws retained.

Sky gradients/cloud rows run on disjoint CPU worker rows. Opaque geometry is
painted first, and sky pixels hidden behind it are skipped; glass composes
against the completed background. Atmosphere rays missing all gas/propagated
source-light support are skipped, without moving the retained sampling phase.
Noise is evaluated only when a gas coefficient actually contributes. Frame
invalidation hashes bounded unaligned words and computes its common scene key
once. No atlas pointer survives its callback.

Brick and standard drywall faces use the installed tile palette with continuous
filtered materials rather than stretching a top-down cap over the wall height.
Creature feet, tree trunks and wheels receive soft nearby contact shadows.

Native optimization cases compare every visible pixel, world depth, creature
pick and effect pixel with an unculled/full-sky/full-volume reference across
camera directions, steep pitch, oversized flying creatures, glass, rotated
vehicles, source light, smoke, fog and dimension skies. ABI/lifecycle tests link
the actual shipping DLL/SO, avoiding separate module.cpp builds. CI runs those
checks before either expensive Host job and runs ASan/UBSan on Linux.

`first_person_benchmark 12 distance benchmark.json` writes median/p95 timings
and geometry, sky and atmosphere work counters for both reference/optimized
policies. `cmake --build fp-build --config Release --target first_person_measure`
collects the same non-gating report in CI. Its reference intentionally disables
culling; it is not a historical 0.8.0 build. These are synthetic renderer timings,
not player FPS. Compiler-cache artifacts retain actual bypass reasons, including
MSVC PCH requests that were excluded from the previous headline cache-hit rate.


## 0.9.0 procedural creatures

The optional `graphics.creatures.v1` extension supplies visible, stable entity IDs,
resolved body plans, species, volume, mass, locomotion/material flags, NPC mutation
and clothing appearance, and observed combat events. Core/Graphics V1/V2 layouts
are unchanged. Older Hosts retain the legacy renderer. No engine or SDL handle
crosses the extension, and no presentation state is written to saves.

The automatic generator covers humanoids, quadrupeds, arachnids, insects,
centipedes, birds, reptiles, serpents, fish, amorphous organisms, robots and exotic
forms. Unrecognized anatomy receives a closed universal volume. Source sprites
supply separate regional materials; back surfaces use restrained regional colors.
NPC clothing/mutation layers are resolved by the installed tileset's own overlay
lookup. IDs identify cache entries; they are not an anatomical exception table.
Animated source tiles use a stable reference frame for generation, while ordinary
2D rendering retains its animation. Shared NPC clothing layers reuse atlas reads.

Each model has a hierarchy of joints, two meshes and eight generated depth/normal
impostor views in seven poses (including prone). LOD uses projected size, a triangle budget and 13%
hysteresis. Idle, walking, turning, attacking, hurt, death, flight and swimming
are presentation only. Movement uses observed absolute positions; teleports and
fresh visibility never invent a path through unknown terrain. Event clips start
at their first visible snapshot, so a long simulation turn cannot consume an
unseen animation. Death visuals expire after 550 ms of presentation, require
current cell visibility and cannot be attacked.

The module retains static opaque scenery and replays owned transparent surfaces
after dynamic creatures. This preserves glass and depth while avoiding a world
rebuild for every creature animation. One worker creates immutable assets from
owned source copies; no game/SDL access occurs on that thread. The asset cache
is 64 MiB, with at most 16 queued requests and four new requests per callback.
A small immediate volume covers generation latency. In-flight generation and
source copies add bounded working memory outside the retained asset budget.
World/tileset epochs cancel stale work; shutdown joins the worker before unload.
GPU geometry is not exposed by the current bridge, so meshes and impostors use
CPU depth/raster paths; existing GPU atmosphere composition remains optional.

Qualification artifacts include loaded-definition classification/fallback JSON,
a real SDL gallery with twelve creature definitions and eight views each, action,
LOD/light/occlusion captures, and 1/10/50/100-creature render-loop measurements.
CI builds the exact 0.8.1 DLL at `3f6a164e23a5d586156218ecb3ab0009e187d1ca`
and runs it against the same Host/fixture for the billboard comparison. Reports
record median/p95 SDL render time, process CPU, cold model readiness, generation
and cache memory. DXGI process-local video memory is recorded when available;
unavailable measurements are explicitly marked. These render-loop timings are
not player FPS and exclude interactive simulation/AI turn cost.

The native suites additionally exercise malformed/missing sprites, unknown mod
anatomy, joint topology, UV bounds, outward faces, LOD hysteresis, hidden entities,
transparent cache ownership, world resets and the actual shipping DLL lifecycle.
The real-engine qualification status and measured limitations belong in the
release report; a passing native test alone does not qualify an installer.
