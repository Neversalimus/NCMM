# Bounded normalized-content cache: experimental CPU change

The renderer previously recomputed each normalized cell content hash before reusing its neighbour signature. The new path compares every normalized byte against owned storage, then reuses only an exactly matching hash. This removes hash work; normalization and comparison still run.

Absolute z/y/x identities use slots modulo the validated viewport dimensions. Each slot also checks its complete absolute identity. Cells entering a shifted viewport replace only their old slot. Legacy dense-grid mode stays separate. Coordinates, world flags/direction, vehicle detail bands, metadata presence, occupied vehicle and every existing normalized source byte retain their original significance. World/tileset clear frees all added storage; no Native pointers are retained.

Storage is allocated in 128-record pages with an explicit 32 MiB total bound, including page pointers and the validity mask. Each call allocates at most 512 KiB of pages and copies at most 512 KiB of records. Deferred records are hashed normally. Scenes below 256 cells or above the capacity use the original complete hashing path and release optional storage. The existing CPU-cache byte accounting includes the added allocation.

The unchanged 9c source is an independent oracle: all 20,103,528 emitted bytes match on MSVC and GCC across the original 4,262 differential checks. Expanded checks cover lazy filling, changing content, both shift directions, extreme coordinates, metadata presence, memory bounds, over-budget fallback and clear. All 33 local CTest tests, including RTX 4090 GPU pixel/lifecycle tests, pass. Twelve atomic source-transform and eight qualification-policy tests pass. The focused Linux ASan/UBSan job now includes this test explicitly.

## Complete balanced local function measurements

Order: unchanged9c, candidate, candidate, unchanged9c. Each cell below is **cold / median / p95 / max**, in milliseconds. Cold is the first call and is not hidden inside steady samples. Each repeat has 96 steady observations per one-storey scenario, 64 for three storeys and changing content, and 16 over-budget observations. These are function timings, not Native frame times. All four complete 47,876,496-byte signature streams match exactly.

| Repeat | Scenario | Original | Candidate |
|---|---|---:|---:|
| Forward | stationary | 1.6769 / 0.8224 / 0.9551 / 1.0618 | 2.3342 / 0.5136 / 0.8680 / 0.9557 |
| Forward | shift_forward | 1.6312 / 0.9248 / 1.1297 / 1.1628 | 2.2595 / 0.5756 / 0.9478 / 1.3029 |
| Forward | shift_reverse | 2.1955 / 0.9279 / 1.1034 / 1.5869 | 1.8006 / 0.6303 / 0.9866 / 1.1617 |
| Forward | three_storeys | 5.2616 / 2.8706 / 3.4064 / 3.6778 | 5.5543 / 2.6747 / 3.3153 / 3.5462 |
| Forward | all_changed | 2.5466 / 1.7725 / 2.4677 / 2.6183 | 3.0286 / 1.7640 / 2.5900 / 2.8162 |
| Forward | over_budget | 8.7550 / 4.8387 / 5.1642 / 5.2773 | 9.3558 / 5.3220 / 5.8624 / 5.9757 |
| Reverse | stationary | 1.8118 / 0.8500 / 1.0103 / 1.1369 | 1.9124 / 0.5147 / 0.9248 / 1.0720 |
| Reverse | shift_forward | 1.6597 / 0.9619 / 1.1338 / 1.1818 | 1.9596 / 0.7061 / 1.1410 / 1.2913 |
| Reverse | shift_reverse | 2.3021 / 0.9401 / 1.0919 / 1.1352 | 2.1932 / 0.5753 / 0.9537 / 1.0762 |
| Reverse | three_storeys | 5.5018 / 2.9483 / 3.2968 / 3.4017 | 7.9326 / 2.8468 / 3.5703 / 3.7554 |
| Reverse | all_changed | 2.0150 / 1.7573 / 2.4973 / 2.6658 | 1.7733 / 1.7953 / 2.8182 / 2.8918 |
| Reverse | over_budget | 8.7685 / 4.8632 / 5.3787 / 5.4997 | 8.7202 / 4.8056 / 5.3435 / 5.7349 |

Steady medians improve for unchanged/shifted scenes, but this candidate is **not accepted**: several cold calls and some tails remain worse. The three-storey cold call changes from 5.2616/5.5018 ms to 5.5543/7.9326 ms. This remains visible and must be assessed against complete Native frame measurements. One-storey peak owned CPU cache rises from about 0.26 MiB to 10.24 MiB; three-storey measured peak rises from about 0.78 MiB to 24.12 MiB. These are CPU-cache capacities, not measured VRAM. No GPU upload reduction is inferred.

The earlier local two-array version increased cold allocation and copy tails and was discarded. Its binaries, results and source digest remain in local evidence. The later B19 full-matrix rejection remains authoritative; the historical positive standalone run is not a waiver. No public PR or final hardware acceptance follows from these function measurements.

The adjacent JSON records exact source/evidence identities. Raw signature streams, benchmark/oracle drivers, failed exploratory measurements and build logs are in `fpv-next-evidence/content-cache-*` beside this checkout. Large arrays are not included in this diff. Fresh independent ordinary Native comparison, final mode matrix, both pinned Hosts/platforms, sanitizer CI and unchanged final hardware/lifecycle qualification remain required.
