# Autonomous optimization, 27 October 2026

Baseline: `128453707e6c4e2712237d046dfa07315eac3ca5`, clean
`codex/fpv-0131-fps-controls-20261024`, draft PR #223. Engine pins remain
1040 `3f7fb352bf492ba521bd9408a0c9f6ce239e8d83` and
1807 `074aa98bd5be3de4c35f154082db32a0e63bb0f1`.

Local Native game/input harnesses, mouse capture/warp/focus changes and elevated
profilers remain prohibited. Background games remain running. Available checks
are Release builds, pure/model/shared-module tests and the inventoried hidden
render-only GPU suite. Their timings are not ordinary game frames, GPU duration
or action-to-first-present latency. The 15 ms target remains unqualified.
Previous Native run 40407823699 was still running its long gameplay/Linux gates
at the start; its completed 180-case-per-pin FPS tests do not qualify new bytes.
Do not merge or release before the user's separate finalization request.

## Completed checkpoints

1. **Unique texture ownership.** `mesh_builder::texture()` retained one
   `shared_ptr` per textured triangle; `packet::mesh()` revisited every owner
   even though the GPU texture upload was already cached. The new pure
   regression failed on production baseline: 4096 owners for one texture.
   Meshes now retain each immutable texture once, also when registered through
   a material's color/normal/depth leases. No resource keys or geometry changed.

   Same 4096-triangle fixture: vertex/index/material bytes, mesh digest
   `5bb968231abc8f92` and packet submission match. Owner references 4096 -> 1;
   pointer-vector capacity 86320 -> 16 bytes; growth events 22 -> 1; visits over
   512 packet builds 2097152 -> 512. A paired isolated run measured 1.947 ->
   0.052 ms **total for 512 packet builds**, not per game frame. The initial
   failing run is retained separately; timings under background load are not
   a hardware qualification. The owner/lifetime regression (11 checks) and
   complete GPU geometry/cache pure suite (324193 checks) pass.

## Working queue

1. Remove CPU distance sorting from GPU cell collection only, after an exact
   output comparison and bounded first-update preparation measurements.
2. Reuse the unchanged sparse-world dense XYZ index without retaining a Host
   scene pointer; prove ordered-position invalidation and lifecycle bounds.
3. Repair confirmed chain-link wire direction changes at straight half-panel
   seams. First reproduce with actual emitted geometry, then verify negative
   coordinates, origin shifts, turns and gates. Current boundary-only tests
   cannot establish continuity.
4. Distant radio towers are still classified as generic BUILDING horizon
   patches and become wide boxes. Near towers already use lattice geometry.
   A separate compatible skyline representation is needed; do not silently
   repurpose the existing kind or hide the object as a performance change.

Other historical symptoms have existing causal fixes and regressions. This
audit has not established their recurrence in a current game session.

## Validation still required for this continuation

Run the affected checks after each edit, then the autonomous local inventory,
hidden GPU pixels/resource checks, source/package checks and fresh remote
Native qualification for the resulting exact revision. Keep first changed
frames separate from warm frames. No new installer or full Native result is
claimed by this journal.
