# Retain source facts during Native cache resets

The ordinary D6 controls recorded about 25–27 thousand median source-cache
misses in the first frame after a step in the sampled street/city scenes,
with maxima above 50 thousand. Warm rotation had zero source misses. These are
Native work counters from the preserved six-run controls, not GPU timings or
proof that every miss explains a particular wall-time spike.

Both pinned Native sources contain two broad cache-reset paths: `map::loadn`
marks Native lighting/visibility/path caches dirty for each incoming submap,
and `game::update_map` invalidates Native caches after rebasing the bubble.
The former graphics hook treated each such flag reset as a change to all
physical source facts on that level. This defeats absolute-coordinate overlap
reuse even when the retained terrain has not changed. The observed repeated
revision delta of 714 is consistent with these paths; individual live
notifications were not separately traced.

This candidate starts at `b74b25cfc12af66ab81f99f8c753e289a3b4eeef` and makes
the following private changes:

- A synchronous thread-local scope identifies only the exact Native cache-flag
  reset blocks. Every original Native cache call, rebuild, map generation,
  actualization, vehicle/NPC update, perception and terrain pass remains.
- Native submap loads invalidate the clipped absolute rectangle on that Z.
  Actual changed terrain/furniture stores also notify their cell, including
  identifier/alias changes with equal Native lighting flags. Equal-ID early
  returns are unchanged.
- Public world/level notifications remain monotonic. Separate private source
  validity allows cache-only work to retain physical facts. Real generic
  notifications still invalidate private levels, conservatively including
  adjacent roof/canopy dependencies. Unknown global changes invalidate all.
- Existing global geometry/ray generations still advance. Derived geometry,
  roof columns, sealed-plane proofs and visibility cannot reuse an old
  dependency generation after a changed source region.

No new world representation, simulation, resource policy, SDK, public ABI,
texture, radius, Z, viewport or quality change. Borrowed map addresses exist
only during synchronous scopes and are compared, never dereferenced or
retained after them. The extra private revision array is fixed at 257 entries.

The existing Native source reader was factored without changing its body;
the ordinary source-cache lookup body is also unchanged. Functional scene
checks compare reusable cached facts against the fresh Native reader.
The short walking diagnostic additionally checks after settled real moves.
Extended ordinary performance runs do **not** run this oracle and report
`source_facts_oracle_checked=false`, `source_facts_oracle_cells=0`.

Local validation passed 787129 cache/scope checks and 7123 tests of the actual
old/new notification bodies on GCC 16.2 and MSVC 19.38. Coverage includes shifted
ring offsets, clipped regions, changed Z, overflow bounds, invalid sizes,
fresh Native oracles, nested/exception/thread scope restoration, unchanged
public notifications and conservative real invalidation. All 11 atomic source
transform tests passed. Actual reset and changed-store anchors were checked
against both complete pinned primary `map.cpp`/`game.cpp` files.

The source-reader body SHA256 is
`c65cfdf0549798a4227ea045de4252bef3d25bb717aa89d84c14d4c54ceaef80`;
the unchanged ordinary lookup body SHA256 is
`98009dfd4bdf07430f0e54763e0cf4c007e1a5607d679c71aa91e3a1a04ee894`.
MSVC used the project's `/W4` warning level; intentional unreachable paths in
negative-test lambdas remain visible in the test log. GCC used strict warnings.
The pre-existing sealed-plane indentation warning was fixed without behavior
change, and the outer test counter was renamed to avoid shadowing.

These tests use a fake map for notification-policy checks and do not claim
an actual Native engine run. Full Windows 1040/1807 builds, Native source
oracles and fresh balanced RTX ordinary comparisons are still required.
This parent workflow checks Linux modules/sanitizers; final integration must
also run the actual pinned Linux Native qualification. Original D6, Core,
CPU-diagnostic and inactive-snapshot negative evidence remains preserved.
No standalone performance acceptance, 15 ms claim, final combined acceptance
or merge is implied.
