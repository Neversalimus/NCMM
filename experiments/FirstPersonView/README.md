# First Person View — 0.3.0 development prototype

Optional first-person renderer of the same live CDDA world through NCMM. This
preview bundles an exact experimental Windows x64 Host for 1040 or 1807 and uses
the normal installer's independent component selection. It remains outside the
shipping mod catalog; the certified Host feed is not promoted.

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
| Existing movement keys | Existing world directions |

Camera controls preserve the avatar's position, moves, time and destination.
Ordinary commands, adjacent selection and inventory keep the perspective
backdrop. Coordinate look, targeting and the overmap use their normal map view
and restore perspective on return to gameplay. No command-driven temporary
view cancellation remains.

Uncheck First Person View and install again to remove its managed DLL/manifest.
Use the normal installer to return to the certified Host, or Restore vanilla EXE
for vanilla play. This preview uses its bundled Host offline. It includes NCMM
Runtime 0.8.2 and the normal optional native modules; game data and external
tilesets are not bundled.

## Hybrid environment and lighting

The new optional Graphics V2 bridge supplies callback-scoped CPU sprites, visible
cell geometry, current weather and calendar astronomy. V1 and Core ABI contracts
remain unchanged. The renderer uses clipped perspective-correct triangles, a
per-pixel depth buffer, bilinear filtering and a ground mip level, at viewport
resolution up to 1920×1080. Unlike the old scanline floor, ground fills every
covered pixel. Drawing at this resolution costs more CPU time than the map.

Doors retain jambs and a hinged panel; destroyed doors retain a frame. Windows
have upright sills, frames and translucent glass; broken openings and opaque
curtains/boards are separate cases. Fences/gates have open vertical mesh, posts
and hinges. Plants have an independent visible-ground substrate; trees use trunks
and foliage billboards. Tables, storage, seats, rocks, signs and posts receive
shape-specific geometry. Semantic flags take priority over identifier/looks-like
fallbacks; the engine fixture audits all loaded terrain/furniture definitions.
Flat blood/liquid fields remain ground decals; fire, smoke and gas use billboards.

Visible vehicle mounts share actual yaw and one continuous anchor. Automatic
meshes combine chassis, body panels, roof, seats, windows, open doors, modeled
wheels/rims and lights. The current tileset supplies paint/material colors,
rather than a top-down car part stretched into a wall. Dimensions are coarse and
generic; this is not a separate authored model for every vehicle prototype.

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

The wielded item's actual tileset silhouette is generated in camera space with
shaded thickness, forearms and hands. It updates when the player changes or
unwields the item. This is the first automatic held-item pass, not a collection
of bespoke 3D weapon models or attack/reload animations.

## Visibility and lifecycle

Only cells/entities currently visible to the avatar submit identities, geometry
or sprite requests. Ground inference, vehicle neighbors and sun occluders use
that same snapshot. Unknown borders carry a generic dark surface. Map memory is
updated by the normal terrain pass before the perspective overlay. Darkness
keeps the renderer active without revealing hidden identities.

The bridge caches copied atlas pixels with bounded memory, invalidates them on
tileset/renderer changes and restores SDL state around readback. Modules receive
no engine/GPU handles. Complete frames are validated and copied before drawing;
invalid output falls back to the map. Unload/quarantine clears callbacks before
the DLL can be freed. A V1 fallback remains for the previous preview contract.

## Remaining limits

Tile-center movement, one visible z-level, generic surface dimensions, foliage
and creature billboards, no mouse look or NPC appearance yet. Vehicle and held
item geometry is generated and needs further interactive refinement for unusual
modded shapes. Raster resolution improves perspective sampling but cannot add
art detail absent from the installed tileset. Gameplay, saves and world-direction
movement remain CDDA's. Player testing of combat, targeting, save/reload and
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
Bridge to the experimental engine source only. The four engine entry points are
Host query/lifecycle, gameplay action dispatch, terrain viewport drawing, and
input-context suspension. Preparation preflights all anchors before writing the
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
ASan/UBSan and atomic source-transform failure cases accompany source syntax
checks against both pinned engines.

Dedicated preview CI compiles actual MSVC Hosts, checks installation/repair/
rollback/vanilla restore with Windows long paths disabled, then launches both
official archive variants through the actual bootstrap. Its SDL fixture injects
F6, exercises camera controls without spending turns, audits all loaded terrain
and furniture, changes real doors/map memory, tests actual colored light at
night, sun/phase data, game weather, wielded item swaps, vehicles and ceilings.
It captures engine-rendered BMPs for visual review. Mock renderer images and
native CPU timings are fixture results, not game-session FPS measurements.

Existing CDDA/tileset assets retain their licenses and attribution; see
`THIRD_PARTY_NOTICES.txt` and the installed tileset's provenance.
