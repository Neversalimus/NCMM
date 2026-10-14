# Multi-level world contracts

The optional `graphics.multilevel.v1` interface preserves Core, Graphics V1/V2,
creature and GPU/Mesh V1 binary layouts. It supplies callback-scoped, immutable
appearance arrays plus explicit canonical absolute XYZ coordinates and sorted
sparse slice ranges. A frame index identifies a submitted cell; it never implies
XY or Z. Physical coordinates subtract the snapshot's absolute origin using
64-bit arithmetic before converting to floats. One CDDA Z level is 2.5 visual
metres, matching the existing wall/ceiling scale. World identity is independent
of Z; separate dimensions must have separate world epochs.

## Pinned engine semantics

The source audit covers `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83` (1040)
and `074aa98bd5be3de4c35f154082db32a0e63bb0f1` (1807).

- `map::has_floor` treats out-of-bounds/unavailable levels conservatively and
  excludes both `NO_FLOOR` and `NO_FLOOR_WATER`. Physical support and visual
  transparency are different contracts.
- `map::build_floor_cache` also opens sight at `GOES_DOWN` and
  `TRANSPARENT_FLOOR`; furniture below with `SUN_ROOF_ABOVE` overrides the gap.
  An ordinary staircase is a walkable landing with a visual vertical opening,
  not an ordinary full floor or a wholly empty cell.
- `TRANSPARENT_FLOOR` is still a physical surface. `map::is_roofed` explicitly
  treats it, water surfaces, canopies and vehicle roofs as precipitation blockers.
  A glass/grated floor must not be replaced with a hole or admit rain below it.
- `Z_TRANSPARENT` exists in both flag registries, but neither pinned floor-cache
  implementation uses it alone to remove a slab. Unknown contradictory data
  must not invent a vertical passage.
- `GOES_UP`, `GOES_DOWN`, `RAMP`, `RAMP_UP`, `RAMP_DOWN`, `RAMP_END` and
  `DIFFICULT_Z` determine transition semantics. Runtime geometry uses flags and
  actual neighboring levels, without an ID-to-building/model asset list.
- Both pins use `constexpr int fov_3d_z_range = 10` in `game_constants.h`.
  `FOV_3D_Z_RANGE` is a removed/migrated option and `FOV_3D` is not a configurable
  switch in either options implementation. Qualification must document this,
  rather than inventing an enabled/disabled setting matrix.
- `Creature::sees` and `map::sees` retain CDDA's ten-level/range/perception rules.
  Cross-level `map::sees` currently has a transparent-floor support TODO; visual
  transmission through glass cannot grant unsupported gameplay interactions.

The engine-independent world policy distinguishes physical slabs, transparent
slabs, transition landings, water and unknown cells. Its bounded supercover DDA
tests floor crossings in physical Z and checks tied boundary cells, preventing
diagonal/corner leaks. Geometry permission and CDDA entity perception remain
separate: the renderer does not grant visibility, targetability or movement.

Snapshot cells are bounded at 65,536 and slices at 32; only already-loaded map
data may be read. The interface contains no loading, simulation or map mutation
function. Legacy adapters remain available for older Hosts.

## Snapshot and rendering path

`map::ncmm_graphics_loaded` is a read-only submap probe. The world adapter reads
already loaded levels within the existing 48-square horizontal radius. It builds
ordinary transient map/light caches at the avatar's current Z and never calls
`update_visibility_cache` to explore an extra level. A main-thread cache stores
canonical XYZ flag facts, per-level revisions and ray results; an absolute-column
roof mask avoids rescanning upper floors when the camera turns. No engine object,
terrain/item ID, sprite address or entity pointer survives in this private cache.

Publication combines physical floor/opaque-voxel ray checks with the game's own
perception. Items and living creatures require native CDDA visibility. A slab
seen from below carries a structural underside and lower-room RGB, with no upper
room furniture/items or navigation facts. Death clips require an already observed
entity and a currently visible body cell. Actual triangle depth then handles the
visible models and cutout/translucent surfaces in both rendering paths.

CPU and GPU share the same world geometry collector. Slabs have top and lower
faces separated by 0.08 visual metres; the lower face has its own structural
finish and RGB. Stairs, ramps, ramp ends and ladders use transition flags and
actual neighbor data. The old interior bit cannot create a world ceiling. Mesh
chunks use absolute XYZ keys and a world epoch independent of the avatar's Z.

The GPU extension wraps the existing scene without changing its ABI. The Host
checks callback ownership and exact source-coordinate identity, then packs an
owned 112-byte std430 cell from the existing 80-byte cell plus origin/lower RGB.
The existing SDL GPU device and shared depth remain in use. Visibility readback
fences carry a producer generation, so a restarted frame serial or changed world
cannot reuse an old creature-pixel count. Failure falls back to the complete
multi-Z CPU scene. Submission timing is not GPU execution timing.

Vertical camera interpolation follows actual native position changes through a
known passage. It lasts 180 ms, keeps yaw/pitch and spends no movement points.
The camera is clamped below an actual opaque ceiling. Native targeting retains
CDDA's reach, perception and trajectory rules; an XY melee move never becomes a
cross-level attack.

## Qualification evidence

The native suite covers policy, coordinate overflow, corner/floor occlusion,
canonical cache reuse, per-Z invalidation, geometry, CPU/GPU collection and the
actual shared-module lifecycle. Existing Core/V1/V2/GPU tests remain enabled.
The source transformer validates all anchors before any source write.

The real-engine fixture defines 21 scenes and 224 load records per measured
engine/package: 1/3/5/9 constructed levels, seven environments, four viewport
sizes and CPU/GPU. It records available/submitted levels separately; it does not
truncate the native loaded vertical range to the requested construction count.
The main 0.11 reference DLL runs on the same pinned experimental Host and game
fixture. Actual render dimensions are recorded alongside viewport dimensions.
Native stair moves, visual continuity, unchanged render-time game state and
exploration, F6 cleanup, giant-avatar headroom and native saves at Z -1/0/+1 are
also checked. Full qualification is complete only after current-head CI and
manual review of the paired SDL captures pass.
