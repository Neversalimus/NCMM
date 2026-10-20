# Callback-scoped snapshot storage experiment

The complete f26 R2 ordinary comparison was rejected: armed cold rotation p95
and p99, and 100-creature warm rotation maximum, worsened in both directions
above the unchanged 5% and 0.1 ms gates. All four runs and the earlier incomplete
protocol attempt remain preserved. No combined acceptance or merge follows.

The largest forward warm spike included 14.0274 ms Snapshot and 11.1106 ms of
its nested Visibility phase. There were no source/derived/roof misses or static
index buffer operations in that frame. Those timings do not establish an
allocation or scheduling cause. CPU sample/context-event and scheduler
limitations in the Nsight evidence remain applicable.

This experiment removes repeated allocation of six snapshot arrays: candidates,
base cells, positions, world coordinates, slices and facade light. Each callback
owns its arrays until it returns. Elements are destroyed at return; the next
callback constructs fresh values, including full value-initialization of every
base cell and its ID slots. Only empty vector capacity persists. Native pointers
are not stored in these arrays. Nested callbacks receive separate arrays; world
reset releases retained capacity and prevents active old loans from returning it.

At most 65,536 elements per array and 32 slices can be retained. A larger
temporary candidate vector follows the original traversal/budget/order logic and
is freed at return. There is no speculative maximum-sized preallocation. The
Native tile pass, source invalidation, perception, rays, material data, sort and
cell budget remain unchanged. GPU resources, draws, shaders and public ABI are
unchanged by this patch.

Private benchmark counters record actual vector capacity growth events, reused
buffers, retained capacity bytes and candidate count. The snapshot byte estimate
now includes the previously omitted candidate and facade arrays. It is logical
CPU capacity, not physical VRAM; RSS remains a separate measurement.

Local validation: all 32 CTest cases passed across the initial 29 successful tests
and a repeat of the three GPU tests after adding the exact SDL DLL directory to
the test environment. Those three initial processes did not start (0xc0000135);
both logs are retained. GPU resource/UI/full-scene tests used the RTX 4090/D3D12
with no CPU raster fallback. The new snapshot test passed 5,120 checks with MSVC
and separately GCC 16.2 with `-Wall -Wextra -Werror`. It checks stale ID bytes,
actual allocator calls, nested callbacks, reset during a loan, bounds, exception
cleanup and canonical slices. Twelve atomic source-transform tests also passed,
including exact copying/inclusion of the new header.

Actual pinned Native validation and ordinary full-frame comparison are pending.
Allocation removal is not proof of frame-time improvement. Earlier negative
experiments are not accepted by this patch or by functional CI.
