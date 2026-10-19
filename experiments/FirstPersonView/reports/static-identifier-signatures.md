# Semantic identifier hashing: not accepted

Candidate module `09d3dc42099f8affae0486ca8a12cfebbe609bef` changes only bounded static identifier hashing. The same older Native Host and all actual UDP inputs were used for four explicitly mixed-component diagnostic runs; these are not installers or final qualification.

Conditions: RTX 4090, D3D12, Native 1807, SDL 3.4.10, actual VSync 1, perspective canvas 1768x1084, internal render 1761x1080. The actual Native window was 2560x1369 at display scale 1.0. No profiler, debugger or GPU debug was active. Each run contains 640 first frames and 1,920 camera frames.

The original byte-padding regression fails before the change and passes afterward with cached/fresh mesh equality and real-ID/floor invalidation. Those correctness results do not prove a frame-time gain.

| Scene | First median before/after, forward ms | First median before/after, reverse ms | Warm median forward ms | Warm median reverse ms |
|---|---:|---:|---:|---:|
| street | 17.013/16.752 | 16.584/16.166 | 6.950/6.944 | 6.939/15.495 |
| city_blocks | 16.582/17.386 | 17.496/17.002 | 9.367/8.521 | 10.461/15.461 |
| dense_city | 20.443/19.293 | 19.702/22.410 | 13.405/11.854 | 12.217/11.944 |
| forest | 14.276/13.563 | 14.087/15.011 | 6.949/6.949 | 6.930/6.948 |
| multi_level_horde | 16.571/16.894 | 17.151/17.108 | 6.920/7.076 | 6.926/6.919 |
| creatures_20 | 16.882/16.375 | 16.752/16.537 | 6.915/6.953 | 6.939/6.955 |
| creatures_100 | 17.267/16.832 | 16.301/17.038 | 6.588/6.940 | 6.928/6.851 |
| smoke_rain | 16.283/14.707 | 14.962/16.637 | 14.033/13.507 | 14.130/14.144 |
| armed | 16.335/15.756 | 15.456/16.355 | 6.949/6.943 | 6.951/15.406 |
| empty_attacks | 16.427/14.703 | 15.057/15.666 | 6.957/6.950 | 6.951/6.937 |

The reverse run invalidates the initial improvement claim; street and armed warm medians more than doubled in that repetition. The experiment is isolated from the final branch. No API, shader, quality, world radius or retained geometry variant was changed. The observed stalls have no proven compositor/driver cause.

Repeated >5% and >0.1 ms complete-frame/tail regressions found: 4.
- street warm maximum: +8.14% (+0.669 ms), +230.90% (+18.995 ms)
- dense_city ready maximum: +12.55% (+8.232 ms), +17.98% (+12.103 ms)
- smoke_rain first p99: +9.62% (+2.633 ms), +40.44% (+11.418 ms)
- smoke_rain first maximum: +21.67% (+7.098 ms), +47.34% (+18.259 ms)

Raw reports, phase arrays, resource counts, all process/binary receipts and actual images remain under `G:/ncmm-fpv-next-20261018/ids-{before,after}{1,2}`. Decision and complete metric comparisons are in the external `identifier-decision.json`; this branch must not be merged into the implementation PRs.
