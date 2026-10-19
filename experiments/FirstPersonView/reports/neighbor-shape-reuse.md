# Reuse draw-local terrain classification for physical joints

`graphics_structure_connections` reads the four loaded physical neighbours of
each oriented door, window, fence or related surface. It classified every
neighbour directly, repeating identifier/alias processing and Native trait
queries even when that immutable terrain definition had already been classified
in the same appearance callback.

The helper now receives the existing `terrain_shape` reader used by that callback.
Its existing bounded definition cache performs the identical classification on
a miss. The cache remains local to one `graphics_draw_hybrid` invocation; the
reader is invoked immediately and is never retained. There is no new cache,
Native pointer lifetime, cross-frame identity or authoritative world state.

All four loaded-neighbour checks, physical adjacency queries, connection bits,
fence policy, corner/T-junction behavior and orientation tie-breaking are
unchanged. Neighbours outside the sparse/FOV snapshot still contribute through
the same permitted physical geometry path. No Native tile pass, perception,
geometry, texture, radius, Z, resolution, ABI, GPU resource policy or SDK changes.

This candidate starts at PSX `d6a6543d1235251de170b2cb66d3eab8e38fc98d`.
It does not contain the CPU-cycle observer or the Common snapshot eligibility
experiment. The original D6 ordinary rejection remains valid. Unit/source
checks and CI establish correctness, not whole-game performance; a fresh
balanced ordinary Native comparison is required before acceptance.

Local differential validation compiled the actual old/new helper bodies with
the real classifier and bounded cache using strict GCC 16.2 C++17. A fake Native
map supplied loaded/unloaded neighbours, all surface kinds, aliases, changed
definitions between fresh draws, corners and orientation ties. All 688145
checks passed; unchanged Native map probes/reads were checked separately.
Trait reads in this helper fixture fell from 2802620 to 1712. These are unit
work counts, not measured production calls or a frame-time prediction.
All nine atomic Native source-transform tests also passed. Full actual Native
compilation, gameplay and ordinary hardware comparison remain pending.
