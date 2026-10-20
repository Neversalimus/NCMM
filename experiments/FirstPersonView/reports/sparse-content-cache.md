# Sparse normalized-content hash cache: experimental

The renderer compares every normalized cell/detail/metadata byte against owned data before reusing its previous content hash. It continues to normalize inputs and uses the original hash for every miss. The existing ordered neighbour pass, geometry collection/build, GPU commands, quality, Native source facts/visibility and public ABI remain unchanged.

The cache maps the actual sorted absolute z/y/x cells to one 128-record page pool. A linear merge retains matching keys and recycles departed slots. Two small position/index buffers replace full-record copies on viewport shifts. Storage, free slots and both mapping buffers fit a 32 MiB capacity bound. Record copies and new page allocations each have a separate 512 KiB per-call limit; deferred/unretained records use the complete original hash. Optional allocation failures fall back to hashing. World/tileset clear releases the added storage. Native pointers are never retained.

Capacity depends on actual sparse cells. The full loaded range is preserved: tests explicitly keep 97x97 with all 21 or 32 available levels while only the physical populated cells exist. A preceding dense-volume experiment would have disabled caching on actual Native input and was rejected before any Native launch. Its raw tests/CI remain separate, and its obsolete Full run was cancelled.

All 20,103,528 emitted original-oracle bytes match the unchanged 9c implementation on MSVC and GCC. The expanded suite has 9,138 checks, including Native-shaped empty storeys, both step directions, recycled slots without new allocation, extreme coordinates, metadata changes, memory bounds and clear. All 33 local CTest tests (including RTX 4090 pixels/lifecycle), 12 atomic source-transform and eight qualification-policy tests pass. Focused Linux ASan/UBSan now includes the signature test.

Native diagnostics additionally emit the existing `metrics.producer_cache_bytes` as `gpu_producer_cache_bytes`. It covers module-owned geometry/content cache capacity, which the old Host-only CPU counter omitted. The exact instrumentation delta is one vector, one per-frame append and one JSON field; the inclusive clock and rendering stay unchanged. Missing baseline producer data are unknown, never zero.

## Complete local function comparison

Order: unchanged9c, candidate, candidate, unchanged9c. Each timing cell is **cold / median / p95 / p99 / max**, in milliseconds, using the benchmark’s fixed sorted-sample convention. Counts exclude the separately shown first call. Every individual time is retained in `timings.tsv`; every full output signature is retained. These are function measurements, not Native frame times or acceptance.

| Repeat | Scenario (steady count) | Original | Candidate |
|---|---|---:|---:|
| Forward | stationary (96) | 2.5490 / 0.8315 / 0.9820 / 0.9937 / 1.0025 | 1.7154 / 0.4681 / 0.8848 / 0.9740 / 1.0122 |
| Forward | shift_forward (96) | 1.6423 / 0.9608 / 1.1316 / 1.1770 / 1.3393 | 1.8358 / 0.5395 / 0.9666 / 1.1520 / 1.1652 |
| Forward | shift_reverse (96) | 2.5512 / 0.9474 / 1.1121 / 1.1425 / 1.1787 | 2.1178 / 0.5410 / 0.9657 / 1.0786 / 1.0793 |
| Forward | three_storeys (64) | 5.6621 / 2.8906 / 3.3347 / 3.3588 / 3.3929 | 7.8632 / 2.5753 / 3.0479 / 3.3011 / 3.6226 |
| Forward | all_changed (64) | 2.6189 / 1.7572 / 2.5283 / 2.6779 / 2.7598 | 2.6833 / 1.8429 / 2.6684 / 2.6929 / 2.7213 |
| Forward | over_budget (16) | 10.0804 / 5.1095 / 5.2540 / 5.2540 / 5.4224 | 9.3030 / 4.7085 / 4.9637 / 4.9637 / 5.2732 |
| Forward | native_sparse_21 (96) | 2.5294 / 0.9799 / 1.1368 / 1.2927 / 1.3276 | 2.4731 / 0.8195 / 1.1876 / 1.3713 / 1.5941 |
| Forward | native_sparse_32 (64) | 5.9882 / 3.0649 / 3.3419 / 3.4792 / 3.6528 | 6.2001 / 2.6720 / 3.2481 / 3.2868 / 3.3353 |
| Reverse | stationary (96) | 1.7286 / 0.8293 / 0.9269 / 0.9956 / 1.0081 | 1.8800 / 0.4534 / 0.8793 / 1.0118 / 1.0521 |
| Reverse | shift_forward (96) | 1.8670 / 0.9348 / 1.0792 / 1.2748 / 1.4487 | 2.1829 / 0.5260 / 0.9862 / 1.0432 / 1.1330 |
| Reverse | shift_reverse (96) | 1.7102 / 0.9361 / 1.0703 / 1.1461 / 1.1607 | 2.0413 / 0.5305 / 1.0104 / 1.1514 / 1.2313 |
| Reverse | three_storeys (64) | 5.2065 / 2.8868 / 3.1852 / 3.3927 / 3.5335 | 5.9462 / 2.5981 / 3.2162 / 3.4870 / 3.5752 |
| Reverse | all_changed (64) | 2.6533 / 1.7447 / 2.3039 / 2.6707 / 2.6907 | 1.8615 / 1.8308 / 2.5896 / 2.7111 / 3.1566 |
| Reverse | over_budget (16) | 9.9324 / 4.8998 / 5.2548 / 5.2548 / 5.2561 | 9.3053 / 4.6792 / 5.2024 / 5.2024 / 5.2948 |
| Reverse | native_sparse_21 (96) | 1.9313 / 0.9606 / 1.1374 / 1.2546 / 1.7077 | 1.9716 / 0.8617 / 1.3393 / 1.5534 / 1.5702 |
| Reverse | native_sparse_32 (64) | 5.4722 / 3.0123 / 3.2500 / 3.5164 / 3.5317 | 8.9364 / 2.7142 / 3.3825 / 3.5805 / 3.6648 |

Cold calls and some tails remain worse despite lower steady medians. No sample is removed or treated as qualification. The complete four binary output streams match exactly; the JSON binds their size/hash and all raw evidence. Cache capacities and process RSS are distinct from measured VRAM. A fresh independent ordinary Native comparison and final four-mode/Main matrix must assess the complete frame, action-ready time, cold/tails, uploads and memory.

The existing B19 full-matrix rejection remains blocking. This report does not promote any public PR or waive final pinned Native Windows/Linux, sanitizer, GPU-debug and unchanged 30-minute lifecycle qualification. Large arrays, raw timings, drivers and rejected experiments remain in the local evidence directory.
