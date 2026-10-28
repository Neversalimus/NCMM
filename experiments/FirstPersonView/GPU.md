# GPU renderer integration

Development branch based on merged First Person View 0.9.0, commit
`a434a820aaa5b0bec7611474f8e01ff9b2b77432`. This document records decisions and
qualification evidence; it does not certify an unfinished backend.

## Device and UI ownership

Both pinned upstreams use vcpkg baseline
`f6672d8e480ccdecddfad3fd1b838ba369ffe6cd`: SDL **3.4.10** and
SDL_shadercross **3.0.0-preview2**, shadercross source
`1ca46e0ef7a9e50c706e7be6ef73ce467bac3b2e`. The upstream renderer already
prefers `gpu` and supports native GPU sprite shaders for the 2D UI.

The Host borrows `SDL_GetGPURendererDevice(renderer)`. An offscreen 3D color
target and hardware depth attachment live on that same device. SDL samples the
color target using its existing renderer, then draws the ordinary UI. Only SDL
claims the window and presents its swapchain. No second context or swapchain is
created, and no full-frame GPU-to-CPU-to-GPU transfer is part of this design.

Modules submit callback-scoped mesh/material/pose data through the optional
`ncmm.host_api.v1.graphics_gpu` interface. They receive no SDL/GPU handles.
The Host validates bounds, copies or owns input resources, manages GPU memory
and releases resources on mode/device/tileset/module changes. V1/V2 and Core
tables retain their existing layouts. The current CPU renderer remains the
fallback for missing interfaces, unsupported devices and rejected GPU work.

Shader programs are compiled **during development/build** with the existing
glslang and shadercross tools to SPIR-V and DXIL. The generated header embeds
owned bytecode into the Host; the game does not spawn a shader compiler or make
external requests. An uncompiled source header cannot pass GPU qualification.

## Initial pipeline qualification

`gpu_probe_test.cpp` requires actual SDL_GPU draw commands from vertex/index
buffers, perspective vertex transformation and D32 depth testing. A near red
triangle is drawn before a farther green triangle; the capture must remain red
at the reticle. A normal SDL blue UI strip is drawn over the GPU image and must
remain intact. The test fails if a GPU device is unavailable; there is no CPU
fallback in this qualification case. Readback is restricted to test evidence.

Windows runs this probe in an actual SDL window. Linux uses an offscreen SDL
renderer and Mesa software Vulkan. Device/backend names and software-adapter
status are saved with the original capture. Software Vulkan/D3D12 testing
validates the API pipeline, not performance on a discrete graphics card.

The pinned public SDL_GPU API does not expose timestamp queries. GPU execution
time remains explicitly unavailable unless a trustworthy supported query path
is added; CPU command recording time is not GPU execution time.

## Work still in progress

The shared-device pass also passed inside actual CDDA **1040 and 1807** in
run https://github.com/Neversalimus/NCMM/actions/runs/38230504249,
head `e96a9916c8a6f6d3000c008a12e67653aa3e7546`. Original full SDL captures were
inspected. D3D12 used Microsoft Basic Render Driver; 645120 existing pixels
outside the probe overlay remained identical. Both graphics/sounds installers
completed their runtime smoke. This gate used the old CPU game scene plus a
small genuine GPU pass; it did not qualify the complete GPU engine.

The complete shader/pipeline/resource backend passed native device tests in
run https://github.com/Neversalimus/NCMM/actions/runs/38236790804,
head `547301dfe22a6379ab201ea7e27f8351c3ec9647`: Windows D3D12 / Microsoft Basic
Render Driver and Linux Vulkan / llvmpipe (LLVM 20.1.2, 256 bits). Captures prove
hardware depth semantics and a vertex-shader bone transformation that exposes
the farther surface. Animation uploaded **356 bytes**, with no geometry
regeneration or CPU pixel raster. These are native synthetic API tests;
full-game visuals, benchmarks and recovery qualification remain outstanding.

## Controls during GPU integration

The optional live NCMM setting **First Person renderer** offers Auto, GPU and
CPU. Auto attempts the shared SDL GPU path; a rejected or unavailable path uses
the existing CPU scene and records its exact reason. GPU mode also reports an
actual fallback; it never labels a CPU frame as GPU. F6 retains its map toggle.

F12 now cycles **Auto → CPU → GPU → Auto**. This is the explicit migration from
the former atmosphere-composition toggle. Atmospheric effects remain available
in each renderer. F11 retains the established balanced/high resolution limits.

Windows initial probe passed in workflow run
https://github.com/Neversalimus/NCMM/actions/runs/38226642367,
head `fa77798f1cd27830da5c53a2aa602a8b7b1881cc`, using SDL_GPU `direct3d12`
on **Microsoft Basic Render Driver (software adapter)**. The original 640x480
capture was inspected: red foreground depth result and intact blue SDL UI.
This establishes the shared-device API path, not discrete-GPU performance or
completed integration in CDDA. Linux's first configuration was blocked by SDL's
desktop-window dependency check; the offscreen test explicitly opts into the
documented console build configuration.

The initial shared-device probe is a technical gate. World geometry,
procedural creatures/LOD, materials, lighting/atmosphere, resource caching,
fallback settings and complete game/installer qualification must all be
completed before this branch can be considered a full GPU renderer or merged.

Primary references:
- https://wiki.libsdl.org/SDL3/SDL_GetGPURendererDevice
- https://wiki.libsdl.org/SDL3/SDL_GetTextureProperties
- https://wiki.libsdl.org/SDL3/SDL_FlushRenderer
- https://github.com/libsdl-org/SDL/tree/release-3.4.10
- https://github.com/microsoft/vcpkg/tree/f6672d8e480ccdecddfad3fd1b838ba369ffe6cd/ports/sdl3

## Full in-game gate and current qualification checkpoint

Actual CDDA 1040/1807, both official graphics/sounds packages, passed the full
GPU room gate in run https://github.com/Neversalimus/NCMM/actions/runs/38247876983
(head `6520fcd9e3ec274117eac1fbfd91063728af5885`). Both MSVC Hosts compiled,
install/upgrade/rollback tests and repeated bootstrap launches passed.
Original SDL captures were inspected: room walls, door, floor and held hands
retain geometry and palette. Room RGB mean absolute error was 0.00194–0.00209
against the same Host's CPU frame. The GPU frame submitted 1590 triangles with
20 explicit 3D draws, zero CPU raster calls and no CPU full-frame upload.
The adapter was Microsoft Basic Render Driver; this is software D3D12 evidence.
The initial report used locale-sensitive integer formatting; it is corrected
to the game's JSON writer in the following checkpoint.

Native updated SPIR-V/DXIL programs and dynamic mesh/frustum re-entry tests
passed Windows and Linux in run
https://github.com/Neversalimus/NCMM/actions/runs/38263669211
(head `9efede5414b54b6d37f3b9740b64e2fbc7ee1ad8`). Embedded source SHA256 is
`232f9fb7e99f8266cc32749a60ff4cf343ad2016391cda2f158c7331d05a9702`.

The expanded qualification records paired real SDL frames for 14 scenes at
four viewport sizes, plus the exact stable-main 0.9.0 DLL built from
`9df95cdc8766f8d80f4da7cf08f241f053a40281`. CPU-stage timers are unavailable
for that unmodified reference binary; its total frame/process CPU measurements
are real. At the 2560×1080 viewport, the established high-quality cap makes the
internal 3D image 1920×810; the report records both sizes. These fixtures use
48 measured frames after bounded warming. Median/p95/p99 and observed maximum
are reported without claiming player FPS or a precise long-run p99 estimate.
The new gallery/matrix must pass and be visually reviewed before final merge.

## Continuation audit: blocked procedural gallery and recovery fixes

The expanded qualification at `a018d5c`, run
https://github.com/Neversalimus/NCMM/actions/runs/38264955229, exposed a real
integration defect in CDDA 1807: the first GPU creature was rejected with
`triangle references multiple materials`; the gallery consequently failed
`procedural_creature_generation_did_not_settle`. The successful room/device
gate did not prove that reconstructed anatomical meshes worked.

The GPU collector now preserves the CPU renderer's one-material-per-face
choice, including triangles crossing front/back regional seams. Native GPU
geometry coverage collects thirteen procedural anatomies at eight headings and
checks every submitted triangle, rather than exercising only synthetic test
triangles. Failed producer allocations also invalidate incomplete chunk
signatures before retry; a small-budget case verifies repeated failure and
complete recovery when a scene fits again.

Live CPU selection now invalidates Host GPU resources just like F12. The
shipping module reports deduplicated, specific fallback diagnostics for missing
GPU interfaces, incompatible SDL renderers, pending geometry and rejected
pipelines. A test linked to that actual module checks Auto/GPU/CPU, live-setting
changes, F12, explicit-GPU fallback on older Hosts and recovery after injected
pipeline rejection.

These fixes require renewed Windows Host/game/installer qualification. Do not
merge based on the earlier room result or mark a fallback frame as GPU.

Local continuation checks also exercised the actual geometry collector on SDL
3.4.10 Vulkan/llvmpipe (LLVM 20.1.2, 256 bits): all thirteen anatomy categories,
eight headings each, real indexed draw commands, GPU-visible creature counts
and picking against the unchanged CPU reference. Maximum whole-frame RGB MAE
was **0.004393**; captures were inspected. This synthetic API/device evidence
does not replace the real CDDA gallery or discrete-GPU performance testing.
The shipping-module recovery and geometry/cache suites passed ASan/UBSan locally
(LeakSanitizer disabled for the traced container; CI retains its normal checks).

Host preparation now rejects stale, missing or uncompiled embedded shaders:
both SPIR-V and DXIL must be present for all fourteen source programs and match
the current shader-source SHA256. Tampered source identity and invalid bytecode
were rejected in local negative checks. Shader compilation remains a build-time
operation, with no compiler invocation in normal gameplay.

The next actual-game gate at `3fa1012`, run
https://github.com/Neversalimus/NCMM/actions/runs/38271617904, passed all 96 GPU
anatomy/angle captures, NPC appearance/posture, movement/combat/death and LOD0–2
on both pinned Hosts. It then stopped while waiting for a tiny LOD3 crow to
cover more than two pixels. The fixture now requires at least one real GPU
sample for LOD3, retaining the three-sample rule for larger representations and
failing if the GPU backend or creature is absent. Timeout evidence includes
active backend, completed/frame serials, LOD counters and an original SDL frame.

A native same-source regression uses the actual-sized 0.6 L, 18.4 cm × 23.5 cm
crow at 34 cells, plus eight subpixel camera offsets. CPU and Vulkan each draw
exactly **two or three pixels** at those offsets; requiring three pixels in all
cases is not a valid hardware-raster visibility criterion. Larger and smaller
bird fixtures additionally exercise skeletal/impostor/silhouette transitions.
The real Windows/game gate must be repeated; no erased creature or CPU fallback
is accepted by this adjustment.

Dense grass uses four immutable tile-local patterns, three distance bands and
three shared colour shades. Each Native grass tile submits an instance with its
own cell ID, lighting, height and world-coordinate wind phase. The backend can
batch those instances instead of recording a separate material command for
each randomly coloured blade. LOD selection does not enter the static world
signature, so crossing 5/12/22 m never rebuilds floors or their neighbours.
There are 128/32/8 broad, tapered blades per tile, with one non-degenerate
triangle per blade and fixed roots/two-bone wind. Native dry, white, alien and
short/grazed variants retain their height/colour distinction. Distant coverage
returns to the Native floor. No external asset, shader or public ABI changed.

The CPU renderer reuses the same patterns, culls whole patches, visits opaque
patches near first and uses a flat opaque blade path. Dense grass uses at most
four existing row workers, including in compact windows. Each pixel still has
one writer, and the ordinary lighting, depth, fog and centre picking are kept.

The [paired RTX 4090 measurements](reports/grass-instancing-4090.json) compare
the old `56766ea` grass with this implementation on an identical 41x41 field at
640x480. Two alternating pairs, each with 80 measured warm GPU frames, produced:

| Measurement | Old grass | Dense instanced grass |
| --- | ---: | ---: |
| Maximum actual GPU draw calls | 5,802 | 45 |
| Maximum submitted triangles | 14,314 | 8,370 |
| Median CPU packet preparation | 0.58–0.74 ms | 0.35–0.36 ms |
| Median CPU backend submission | 11.07–12.35 ms | 1.07–1.13 ms |
| Median cached software frame | 7.00–7.45 ms | 4.61–4.63 ms |
| Warm static uploads / generated cells | 0 / 0 | 0 / 0 |

The ordinary GPU test also checks actual blade coverage above the hands
(69,218 of 108,800 pixels), visible wind, at most 48 field draw calls, and zero
warm mesh/texture regeneration. Unit tests reject non-grass/furniture/hidden
tiles, verify fixed roots and non-degenerate broad blades, retain per-instance
Native cell IDs and bound hundreds of tiles to twelve shared models. All 46
local C++ and 168 Python checks passed. The frozen source gate binds all 17
reviewed inputs; the protected backend/resource implementation and shader
sources remain unchanged.

These are controlled field measurements of CPU work, with identical backend
profiling enabled in both revisions. GPU waits/readbacks are outside the timed
loop. Other games stayed running; no GPU execution timestamps, ordinary Native
frame-time qualification or stable 15 ms guarantee are claimed. Installer and
exact-package Native results belong to the corresponding artifact receipts.
