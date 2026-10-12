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
