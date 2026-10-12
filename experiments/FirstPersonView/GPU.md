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
