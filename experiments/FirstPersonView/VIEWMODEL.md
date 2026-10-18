# PSX viewmodel implementation and qualification

This document describes the 0.13 development implementation. Native hardware
qualification and installer results must be reported separately from the
implementation and numerical equivalence checks.

## Source asset

Drillimpact's [PSX First Person Arms](https://drillimpact.itch.io/psx-first-person-arms-free),
version 1.1.0, is distributed by its author under CC0. The exact official ZIP
has SHA256 `13689348acce167599db2b7190214aa4dbe914f5547f74e1877ff61361fc8cb8`.
The repository notice and `assets/psx_arms/manifest.json` retain attribution,
input/output hashes, converter identity and the author's license declaration.
The converter uses the root `arms_rig.glb`; it does not execute Blender or FBX.

`tools/import_psx_arms.py` validates the pinned archive, paths, accessors, node
hierarchy, bind transforms, channels, textures and all source weights. It emits
immutable typed tables. Runtime has no glTF parser, decoder, Python dependency
or network request. CMake checks generated resource hashes and watches the
asset manifest and module version when configuring an incremental build.

The model has 758 source vertices, 1,176 triangles, 52 skin joints and 18 clips.
Hand/forearm material seams produce 790 prepared vertices without altering
positions, UVs or weights. Both 512x512 textures keep their original dimensions;
the source PNGs' 16-bit channels are converted to the existing RGBA8 texture API.
No source influence is discarded. Constant animation tracks are compacted
without resampling moving tracks.

## Existing rendering pipeline

`viewmodel_animation` samples source TRS tracks and evaluates the source hierarchy
and inverse bind transforms. For each distinct original weight tuple, it forms
the weighted affine skin matrix. A vertex refers to that matrix using the
existing single-joint GPU field. This evaluates the original linear blend
skinning while preserving the public GPU vertex ABI.

The original 494 weight tuples exceed the existing limit of 128 matrices per
draw. Immutable mesh partitions respect that limit. An offline opaque-triangle
ordering groups the same oriented triangles by side, material region and bone
locality: six partitions use 554 uploaded palette matrices in total, plus the
existing identity transform. The sorting key never replaces the weights.
Transparent source material is rejected rather than reordered unsafely.

`viewmodel::frame` is shared by the GPU attempt and CPU fallback in one render
callback. Two-bone IK preserves authored arm lengths and places the palm on the
same physical grip used by the procedural item. Fingers preserve source clips;
the firearm index finger also uses the modeled trigger point. Shoulder roots
remain outside the lower view plane. Unsafe or non-finite grips reject the
perspective callback and preserve the ordinary fallback.

GPU resources use the existing mesh cache and `NCMM_GPU_HELD` pass, with its
existing depth rules and shared SDL device. Static mesh, normals, UVs, hashes
and textures are prepared on cache creation; animation updates matrices.
CPU rasterization uses the same weighted geometry, pose and textured materials.
It retains texture owners while rasterizing, including after an appearance
cache entry has been evicted.

## Contacts and appearance

Canonical item properties and Graphics Controls/Presentation fields choose
empty, single, two-hand, pole, bow and firearm grips. Existing firearm geometry
defines primary/secondary contacts, trigger, item pivot and sight locations.
Foregrips change both geometry and the support-hand contact. Revolvers use
canonical IDs, not translated names. Unknown items keep the existing semantic
fallback and bounded physical length.

Bare hands select the author's skin texture. Actual worn gloves select the
glove texture only on the hand; sleeves affect the forearm region. A bounded
eight-entry texture cache applies garment/mutation color while retaining the
author's painted luminance detail. Mutations and CBMs use existing presentation
flags and material finishes. Claws and webbing are small cached overlays bound
to the source finger joints. Fur, scales and chitin retain this human arm
topology; they do not claim distinct anatomical meshes or bespoke fur geometry.
CDDA's canonical `integrated_*` mutation armor can also occupy the glove/sleeve
metadata slots. It keeps the body feature materials and skeletal overlays;
ordinary worn garments retain their separate hand/forearm coverage.

No runtime pointer to a scene, item, atlas or GPU resource is retained by the
animator. Appearance pixel owners and GPU cache entries follow their existing
lifecycle rules. F6, module initialization and shutdown reset the timeline and
pose snapshot. World/clock changes suppress old confirmed attack serials.

## Events and animation

| Presentation state | Source clip or adaptation | Event boundary |
| --- | --- | --- |
| Empty idle | `relax` | No wielded item |
| Generic held grip | `knife_idle`, source grip fingers | Actual item/grip metadata |
| Firearm grip | `finger_gun_idle` wrist; closed source fingers | Native firearm metadata |
| Walking and camera motion | Existing sway layered on the source pose | Actual movement and look metadata |
| Item change | `knife_draw` / `guard_draw`, 0.24 s transition | Changed canonical held identity |
| Empty strike | Alternating `jab.R` / `jab.L` | New confirmed native melee serial |
| Held strike | `knife_hit_01` / `knife_hit_02` plus existing item motion | New confirmed native melee serial |
| Shot recoil | `finger_gun_fire`, wrist/grip constrained to the real gun | New confirmed native shot serial |
| Rapid repeated attacks | 0.08 s smooth blend from one local-pose snapshot | New confirmed serial during an active clip |
| Reload, magazine, bolt or distinct use action | No invented source clip | Current Controls API has no confirmed event for these actions |

All clip times use the Host's absolute presentation clock. Successful native
actions and the existing first-visible-presentation boundary control attack
starts. UI redraws cannot restart an action. Repeated interruptions use one
fixed-size pose snapshot rather than a growing clip history. Changing items,
returning to 2D, changing worlds or unloading cannot replay an old attack.
The viewmodel never changes damage, accuracy, native moves or action duration.

## First-frame work

The candidate native optimization retains CDDA's ordinary tile traversal,
perception, light, layers, animation, map memory and exact stack-height updates.
Only hidden 2D sprite preparation/submission is skipped while preparing the
opaque perspective pass. A scoped flag is restored on return, nesting and
exceptions. Perspective failure redraws the original 2D path. The same route
runs in ordinary gameplay and diagnostics; it is not a benchmark-only shortcut.
Pending native animations retain the complete ordinary tile draw, so a failed
perspective callback can keep those pixels without consuming the native queue
twice. Their cost remains in the measured frame.

World distance, internal resolution caps, Z levels, creature detail, native
perception, simulation and the established horizon LOD are preserved. Whether
this reduces native frame latency must be established by matched hardware runs.

## Verification and remaining release work

Asset tests compare all imported weights, source skinning, UVs and animation
samples. GPU packet tests reconstruct the weighted CPU positions, enforce the
128-matrix limit, preserve the complete oriented triangle multiset and reject
static mesh/texture retransmission during bone changes. Rig tests cover finite
contacts, source lengths, hidden roots, 30/60 Hz cadence, resets and repeated
interruptions. These checks establish numerical contracts, not visual quality
or gameplay FPS.

Use Quick/Integration during development; run Full only on the final candidate.
The expanded diagnostic matrix and 27-state native SDL gallery are opt-in;
default release gates keep their existing strict matrix. Both pinned Hosts,
install/repair/rollback, native debug-device lifecycle and the final hardware
comparison must qualify before merging. See [DIAGNOSTICS.md](DIAGNOSTICS.md).
