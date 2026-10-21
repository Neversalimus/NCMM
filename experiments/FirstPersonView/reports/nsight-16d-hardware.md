# Live Native CPU/GPU traces on RTX 4090

All seven approved scenes now have a live Nsight Graphics GPU Trace and 16
validated Nsight Systems CPU/NVTX ranges (112 logical ranges total). These are
diagnostic captures, not an ordinary frame-time acceptance result. Source 16d
predates the new 30-model held catalog and target HP bar.

Exact source/binary/tool hashes, viewport, internal resolution, static input
inventory identity and compact measurements are in `nsight-16d-hardware.json`.
The Host uses static SDL 3.4.10, D3D12, RTX 4090 and NVIDIA driver 610.47.
Quality, distance 48 and Z policy were retained. No global NVIDIA or security
setting was changed. Graphics used enforced VSync off and unaltered GPU clocks;
Systems did not override the Native setting or collect compositor/scheduler
traces, so its VSync/compositor attribution remains unqualified.

| Scene | CPU logical range median, ms | Native 2D median, ms | Snapshot median, ms | Offscreen GPU root median, ms |
|---|---:|---:|---:|---:|
| quiet | 8.893 | 4.840 | 1.633 | 0.114 |
| rotate | 10.513 | 4.984 | 1.773 | 0.141 |
| step | 9.657 | 4.929 | 2.088 | 0.135 |
| shot | 10.225 | 4.830 | 1.833 | 0.144 |
| hands | 9.878 | 4.815 | 1.722 | 0.130 |
| smoke | 8.000 | 2.964 | 2.062 | 0.501 |
| city | 8.932 | 2.395 | 2.845 | 0.194 |

These columns are different measurements and must not be added. CPU phases are
inclusive wall intervals. The Graphics offscreen root excludes Native SDL/UI
composition. Its exported GPU-frame span includes gaps and is not GPU busy time.
`D3DPERF_EVENTS.xls` mixes genuine GPU debug groups with NVTX queue projections;
the first FP_SCENE root / Present projection is not CPU wall or GPU execution.
The separately validated second root contains the actual offscreen GPU markers.

The first actual step contains one present: 12.581 ms Native Update followed by
approximately 14.850 ms rendering inside a 27.431 ms logical range. This supports
investigating CPU extraction, visibility and cold resource preparation before
GPU upload redesign. It does not establish the ordinary 15 ms goal. The first
shot spans 13 actual presents and 231.269 ms including the Native bullet
animation; it is not a single 231 ms render frame, and that animation is retained.

All requested CPU phases were found; Creatures is conditional on an actual
creature draw list. GPU groups identify Upload, World, Hands/Items, Effects and
Visibility plus independent resource uploads. CPU HUD cost is separately marked.
No sampled CPU execution, context-switch or compositor timeline was collected.

Systems only exported DX12_WORKLOAD for the first range of six scenes and no
workload table for smoke. Missing GPU values stay unknown. Its missing-event
warnings are preserved; the captures cannot prove a complete queue, barrier or
GPU-idle attribution. Graphics supplies the primary live GPU execution evidence.
The initial Systems hands attempt lost its agent; its failed evidence is retained.
Only hands/smoke/city were retried, capturing 16 ranges then allowing a normal
Native exit. All 8,599 immutable static source files still match their hashes.

Raw evidence stays outside Git in `C:/workspace/fpv013-nsight-16d`,
`C:/workspace/fpv013-nsys-16d` and `C:/workspace/fpv013-nsys-16d-r2`, with manifests,
capture hashes, Native journals and ready BMPs. Private validators and full
derived results are under the chat's `fpv-next-evidence` directory. Final ordinary
measurements, combined-source profiling and unchanged-binary qualification remain
required. Prior noisy city first-frame regressions remain acceptance blockers.
