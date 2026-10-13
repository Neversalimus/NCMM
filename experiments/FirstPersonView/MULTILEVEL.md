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
