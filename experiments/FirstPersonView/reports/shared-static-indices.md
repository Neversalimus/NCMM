# Shared immutable index buffers — candidate, not performance accepted

The C63 comparison remains rejected: two repeated cold-rotation maximum
regressions, in dense city and forest, despite 151 repeated improvements.
This change does not override that result or the final component/combined gates.

## Measured reason for this experiment

A short live CPU/DX12 trace of C63 on RTX 4090 covered dense-city and forest
cold rotations. The trace contains 1,090 Present calls. Each 96-frame cold
sequence has a unique match to its Native timing sequence: 95 adjacent duration
pairs differ by at most 0.041 ms. The first pair is excluded because it includes
the existing BMP readback/write before cold rotation.

Observed Native stacks include `resource_cache::prepare` → `D3D12_CreateBuffer`
→ `D3D12_INTERNAL_CreateBufferContainer` → `D3D12_INTERNAL_CreateBuffer`.
These are new mesh vertex/index allocations. Stack observation counts are not
CPU-execution percentages. The original forest recording spike did not recur
under the profiler; its cause remains unproved.

The builder emits local triangle indices, and many independent cell meshes have
the same index contents. Previously every new mesh created its own vertex and
index buffer. This candidate keeps each vertex buffer and shares only exact,
canonicalized immutable index contents. Materials, ranges, picking data, shader
inputs and draw commands are unchanged. Dynamic meshes keep independent buffers.

## Ownership and accounting

The cache owns index contents and GPU buffers. Each resident immutable mesh holds
one lease; the last eviction releases its index buffer. Device abandonment keeps
the existing no-call-after-device-loss rule. GPU request bytes are charged once
per shared index buffer, and the budget is rechecked if reservation evicts an old
layout. These are logical allocation bytes, not measured physical VRAM.

Only a newly created index buffer is uploaded. New private Native diagnostics
report index-buffer creations, reused leases and resident index bytes. Existing
public ABI layouts and SDL/shader versions are unchanged.

## Validation and evidence limits

Local MSVC 19.44 qualification passed all 31 tests, including the real SDL_GPU
resource, GPU-scene and GPU/UI pixel tests. The resource test makes 597 checks:
256 independent vertex buffers share one index buffer; actual GPU readbacks
preserve bytes, different equal-length contents remain separate, material
canonicalization agrees, eviction keeps other leases valid, dynamic updates
remain isolated, and reservation safely reacquires an evicted layout.

This establishes correctness of those cases, not a frame-time improvement.
Both pinned Native builds, fresh hardware measurements and final integration
qualification remain required before acceptance.

The C63 trace and paired PDB remain local evidence. The Host/PDB GUID and age
match. Native PC lookup uses a documented, derived CRT relocation anchor rather
than a live module query. The raw trace also contains system scheduler metadata;
the analysis filters to Native PID 62480 and keeps raw captures out of the repo.
Nsight warns that some boundary events may be missing. Its standalone symbol
resolver ran out of memory; that failed output is not used as confirmation.

Capture: `0098adf337e695aa5a36e52628604ed29e169b00a740ec562791230a6469bb15`.
Original SQLite: `51cc1cc811d22612213a3b19516a949db7da36692a5259f3849c03ce6091b572`.
Native-only analysis SQLite: `a81a0d5383503a33cc0325bfe65725c767ec7d771c1d0254408045928253f31b`.
Local evidence root: `C:/workspace/fpv013-nsys-c63-bounded-r2`;
analysis receipts: `fpv-next-evidence/nsys-c63-bounded-r2` in the task workspace.
The bounded collection uses the documented
[Nsight interactive collection and deferred-report workflow](https://docs.nvidia.com/nsight-systems/UserGuide/index.html#example-interactive-cli-command-sequences).
