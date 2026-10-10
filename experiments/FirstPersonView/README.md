# First Person View — 0.1.0 development prototype

Optional first-person renderer for the **same live CDDA world** through NCMM.
This is the first engineering milestone, not a published/installer-ready module.
The renderer lives outside `mods/`, so the shipping component catalog, installer,
certified feeds, save schemas and other native modules remain unchanged.

## Implemented

- A separately compiled native DLL, using the existing Loader ABI v1 and a new,
  explicitly queried experimental `graphics.viewport.v1` capability.
- F6 toggles the view; F7/F8 rotate the camera by 15 degrees. Defaults are scoped
  to `DEFAULTMODE` and remappable through CDDA's normal keybinding screen.
- Perspective floor, one-unit walls and closed doors, plus depth-clipped furniture,
  item and monster billboards using the active tileset's actual textures.
- Original CDDA movement/turns/combat. Movement keys retain their normal world
  directions; camera rotation does not rotate movement controls or spend a turn.
- A 70-degree horizontal field of view, a bounded 33×33 read-only snapshot, a
  240-column internal render grid and a hard 50,000-command budget.
- The Host only supplies cells with `visibility_type::CLEAR`. Unknown cells stop
  rays and expose no terrain/entity identities. Monsters also require avatar LOS.
- Camera state is volatile. New/load-character events reset to tiles. Menu,
  targeting and other non-gameplay input contexts temporarily use normal tiles.
- Toggle/rotation preserve the avatar's destination, position, moves and save data.
- Commands are validated/staged before drawing. Failed render callbacks, invalid
  output or missing contracts fall back to tiles; quarantined/unloaded modules
  lose their registered callbacks before the DLL can be freed.

## Current boundary

The camera is at tile centers, with one visible z-level and simple wall geometry.
Ceilings, windows/transparency, NPC appearance, vehicles, terrain heights, fields,
traps, animation interpolation and mouse look are subsequent work. A visible
vehicle, displaced look cursor, unsupported avatar cell or invalid renderer output
uses the normal terrain view. UDP art is reused as-is; surface-specific materials,
sprite proportions and lighting need visual refinement in the real game.

A software QA image made from the renderer's command stream is a **test fixture**,
not evidence of a running CDDA session. CPU renderer timing is not a game/GPU FPS
measurement. Real in-game toggle/turn/menu/door/lifecycle QA remains required before
providing a player test installer.

## Exact experimental targets

| Role | Official tag | Source commit |
| --- | --- | --- |
| Primary prototype | `cdda-experimental-2026-10-06-1807` | `074aa98bd5be3de4c35f154082db32a0e63bb0f1` |
| Current NCMM baseline contract | `cdda-experimental-2026-10-01-1040` | `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83` |

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
this for 1807, alongside Windows/Linux module tests. CI artifacts are development
evidence, not a certified Host package or an installer. A regular released Host
does not advertise the graphics capability and rejects this DLL safely.

## Validation and next milestones

Local validation on 2026-10-10:

- C++17 native module compilation with GCC warnings treated as errors.
- Semantic fixture tests: closed/open doors, behind-wall entities, unknown cells,
  72 camera angles, wide/tall/minimum/maximum viewport sizes, ABI negotiation,
  missing-capability rejection, toggle/shutdown, command bounds and CPU budget.
- AddressSanitizer/UBSan run passed. LeakSanitizer cannot run in the local traced
  container; leak detection was disabled for that local run only.
- Atomic-transform tests reject unknown sources, a changed late anchor and
  reapplication without partial experimental writes.
- Canonical NCMM + graphics source preparation on both exact source commits.
- Syntax compilation of the Host loader, SDL terrain hook and input-context hook
  on both exact sources. Windows/MSVC full link and actual gameplay remain CI/QA
  gates; no compatibility claim is made from source checks alone.

Next milestones:

1. Complete Windows Host link and run an actual room in a separate game copy:
   F6 round trip, rotation without turns, opening a door, walking/bumping combat,
   targeting/inventory, save/reload and switching characters.
2. Improve floor UV sampling, wall/door materials, ceiling and billboard aspect
   ratios; record real frame timings and compare with vanilla.
3. Add camera-relative controls as an explicit mode, mouse look, NPCs and fields;
   extend geometry for vehicles and z-levels with the same visibility rules.
4. Register/publish the optional module only after semantic and gameplay gates,
   exact Host certification and installer rollback/removal checks pass.

No external tileset is bundled here. Existing CDDA/UDP assets retain their original
licenses and attribution; see the repository's `THIRD_PARTY_NOTICES.txt` and the
source tileset's provenance when distributing a future test package.
