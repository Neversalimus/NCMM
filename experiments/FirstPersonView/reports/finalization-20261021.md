# First Person View 0.13 — final integration, 2026-10-21

The owner requested finalization and merging of the best result available before background workloads begin. This closes the current optimization effort with known frame-time limitations; it does not relabel incomplete performance gates as passed.

The final tree combines the fully qualified feature source `8f404d9eff1d80ec66526ee122ea3d08f83f8d0b` with the selected measured Core changes from `68c0f2c1877ff0cf820ed11803c2ffd16f621e0c`: exact immutable vertex sharing with allocation rollback, private Direct3D 12 scratch reuse, and callback-local const Native tile reads. The later unmeasured entity-occupancy experiment `5b98220` is excluded.

## Gameplay

Native firing and targeting preserve the user's first-person preference and camera, while native menus, ballistics, ammo, recoil and safe fallback retain their behavior. Camera redraws use owned XYZ facts with explicit invalidation and a Native input lease; the 125 ms interval is input polling, not a cache TTL. The integrated CC0 held library contains 30 models (26 families and four accessories), nine source packs, 55 source files, three textures and 32 generated tables. Native item identity and properties choose compatible resources or the existing fallback. Target names and health use guarded native identity and visibility.

The Common transparent held-material pass and every existing NVTX range remain present. Shader bytes, public ABI, Native 2D, R48/Z visibility, quality, resolution and final SDL-visible output cycling are unchanged. Non-Direct3D-12 drivers retain the original scratch cycle policy.

## Evidence and limits

The feature parent passed all eight full CI jobs, including real Native 1040/1807 on both Windows and Linux: [full feature qualification](https://github.com/Neversalimus/NCMM/actions/runs/39455408174). That run qualifies the parent, not this newly combined source.

Fresh integrated local validation passed all 44 registered C++ tests, 144 Python tests and 19 atomic Native-transform tests. The RTX 4090 Direct3D 12 fixture compared all 135 complete normalized images (165,889,620 bytes) against the Common reference after vertex sharing and before scratch-policy changes: every byte matched. Allocation-failure and shared-lease lifetime tests passed. These are correctness tests, not Native game timing or a manual playthrough.

Four ordinary standalone Core runs contain 2,816 first-frame and 8,448 camera observations with the original thresholds. Typical first-render medians were:

| Scene | Main forward / reverse, ms | Selected Core forward / reverse, ms |
|---|---|---|
| Street | 18.345 / 19.877 | 13.760 / 13.275 |
| City blocks | 20.689 / 19.405 | 14.539 / 12.793 |
| Forest | 16.222 / 16.083 | 11.540 / 10.321 |
| Dense city | 23.697 / 22.654 | 17.408 / 15.143 |

The original two-order comparison passed with zero repeated regressions and 164 repeated improvements. The stricter retained single-order review still failed three maximum-time keys: city action-to-ready forward 62.921 → 66.346 ms; forest cold rotation forward 10.607 → 11.296 ms; empty-attacks cold rotation reverse 10.134 → 10.655 ms. Street first-frame maxima remain 36.926 / 39.040 ms. **A universal 15 ms frame budget is not achieved.**

The measured Core runs disable camera snapshot reuse, imported held assets and target HUD, use controlled daylight with DEBUG_NIGHTVISION, and omit ordinary AI turns. Their medians are not final combined-feature or normal-game FPS claims. The newly combined tree has not received a fresh complete Native performance matrix, long soak, manual combat or high-DPI playthrough. CPU profiler sample counts are not durations; separately captured CPU profiles do not prove a causal speedup.

The selected Core Native correctness run checked 15,857,638 tile reads, 15,857,566 cached source facts, 72 field mutations and 52 exact atlas differential cases. Ordinary timing runs performed zero tile-oracle work. This validates the selected implementation, without claiming the new combined executable was the measured binary.

The adjacent JSON binds current source bytes, local binary, logs and image-comparison evidence. `selected-core-68c0f2c-four-run.md` is an immutable historical measurement report: its old hold/merge flags describe the earlier acceptance policy. This document records the owner's later best-effort finalization decision; historical failures remain visible. The existing full Native and cross-driver CI policies remain enabled for the integrated commit.
