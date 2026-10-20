# Incremental static neighbourhood signatures — experiment

The saved C63 Nsight profile contains actual module leaf samples in
`scene_cache::signatures`: 93 in the matched dense-city cold window and 26 in
forest. These are periodic sample observations, not CPU execution percentages.
An offline linker map was accepted only after executable code/RVAs and every
unwind range matched the profiled DLL byte for byte. Its runtime base has one
64-KiB-aligned solution covering all 1,393 observed module PCs. This is a derived
base and exact function-range map, not live module lookup or PDB/inline resolution.
Unresolved samples remain unresolved (92/424 city; 36/132 forest).

The existing cache already hashes record bytes in parallel word streams. This
patch keeps that hash and every normalization byte unchanged. It reuses the
existing final 27-neighbour signature when no input in that neighbourhood changed.
Added, removed and changed cells are merged by canonical absolute z/y/x values;
their neighbouring current cells are recomputed with the original ordered hash.
Frame indices and Native pointers are never persistent identity.

Small scenes and changes exceeding one quarter of the current cells use the
original full pass. The mask covers only the already validated loaded window
(at most 97x97x32 slots). Positions and actual mask capacity are accounted in producer cache bytes
and released by the existing world/resource reset. The mask is scratch state,
not a world representation; Native source facts and all visibility decisions keep
their existing owners.

No geometry, draw, quality, radius, Z, lighting, Native tile-pass, invalidation,
model, ABI or gameplay policy is changed. This changes signature work only.
All 33 local CTest cases passed, including the actual RTX 4090/D3D12 resource,
UI and full-scene checks. The differential signature test passed 4,262 checks
with MSVC and GCC 16.2: edits/removals/additions, holes, callback allocation
replacement, extreme coordinates/teleports, 1/3/21/32 levels, vehicle bands,
material/gas/ceiling metadata, dynamic light normalization and legacy-mode swaps.
For a synthetic 97x97 one-column shift, neighbour lookups fell from 254,043 to
7857 while every final hash matched the original full pass. This is work-count
evidence, not measured Native frame-time improvement.

An exact source check also verifies unchanged normalization, the hash primitive,
all geometry collectors, build validation, shading and draw code. Ordinary Native
timings and acceptance gates are pending. The df07611 snapshot-storage series
remains separate and unchanged. Earlier negative results do not become accepted
through this experiment.
