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
Empty idle contacts follow the viewport's lower edge, keeping both hands visible
at wide aspect ratios. Empty strikes use the source clip without a second
primitive-hand swing. The shoulder constraint includes the actual held transform,
so item swing, equip movement and body scaling cannot expose the upper-arm roots.

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

The complete native tile draw remains in both gameplay and diagnostics,
including perception, light, layers, animation, map memory and stack heights.
An evaluated shortcut for hidden 2D submissions was removed: its small native
phase savings did not establish a stable full-frame win, and city rotation
regressed in the measured runs. Existing native pixels remain available when
perspective fails, without consuming one-shot native animation queues twice.

Internal scene/mesh cache fingerprints process longer records in four independent
word streams instead of one serialized multiply chain. Every byte, order and
unaligned tail still participates. These are transient invalidation keys, not
integrity hashes or save data; the public key representation and API layouts are unchanged.
The isolated hashing experiment is separate from native frame measurements.

The native backend retains one frame-upload transfer object with bounded,
growing capacity. SDL's existing cycling mechanism preserves storage still
referenced by queued copies. The established lifecycle drain precedes its release.
Retained staging capacity shares the resource budget and is reported separately
from GPU targets/assets; it is not measured VRAM. Creation/growth counters and
queued distinct-pose pixel tests verify reuse and copy lifetime. Native timing
must establish whether this removes the observed upload/recording delays.
The neutral fallback texel is initialized once per target lifetime, rather than
uploaded and cycled every frame. Successful queue submission marks it ready;
target recreation and device cleanup reset that state. Pixel and upload checks
cover warm animation, queued poses and viewport recreation.
An owned cell snapshot compares the complete prepared GPU cell data, including
multi-level origins and lower lighting. Identical cells keep their existing GPU
buffer; changed cells use the existing cycling upload. No source pointer survives
the callback. Comparison/preparation CPU time, actual cell uploads and retained
CPU capacity are reported separately. The snapshot shares the bounded resource
allowance and is released with the dynamic buffers. Shadows, lighting evaluation,
world drawing and presentation continue on every frame.

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

## Compiled CC0 held resources

`assets/held_items/selection.json` pins 30 separate author models: 26 main items
and four installed firearm accessories. It records the publishing authors,
versions, direct archive or individual author downloads, original file hashes
and CC0 evidence. `generated/manifest.json` pins every compiled output. Author
GLB/OBJ/MTL/PNG inputs remain unchanged; the offline importer accepts only this
bounded selection and rejects unsupported influences or animation data.
No selected source contains an authored animation clip. All retained mechanism
nodes, hierarchy, exact joint weights, normals, UVs and material groups use the
same bounded rig evaluator as PSX arms. The arms retain their stricter existing
asset/clip/texture validation. Red-dot transmission is explicitly adapted to
tinted alpha blending, without a claim of physical refraction.

The optional `graphics.held.v1` interface copies owned canonical IDs, a bounded
cycle-checked `looks_like` chain, actual gun flags/skill, qualities, dimensions
and installed mods on the native thread. No item/Creature/atlas pointer is
retained. Selection is canonical ID, compatible appearance ancestry, conservative
native property family, then the established renderer. Unknown mechanisms,
visible attachments, incompatible sizes and unreachable hands report a specific
fallback reason. Localized item names are never used by this selection path.
Canonical item assignments select compatible authored families; they do not
claim an exact branded replica of every CDDA firearm. The Native identity,
mechanism, dimensions and installed modifications still decide compatibility.
The assault family includes the author's weighted extended Stock geometry;
SCAR's exact native `high_end_folding_stock` default is represented by that body.
A removed, folded or unsupported replacement stock selects the existing fallback
instead of leaving a phantom stock or silently ignoring a visible attachment.

One calibrated source frame defines the mesh and both grip contacts. The original
PSX weighted hands orient around those grip axes and retain source bone lengths;
the trigger finger uses the same posed trigger point. Only confirmed native
shot serials animate supported trigger/automatic-action nodes. No reload, use,
shot, movement cost or simulation event is fabricated. The GPU collector keys
immutable meshes by the offline model identity and uploads poses thereafter;
detachable magazines and the actual presence of ammunition select existing parts
without rebuilding static vertices. Empty weapons do not display an authored round.
The imported path does not request an unused tileset held sprite.

The separate Pichuliru accessories are authored in GLB world coordinates; the
gun sockets inherit the rig's exported bone coordinate frame. Their explicit
offline rigid mating rotations join those frames before the uniform Native
dimension scale. Socket animation and original mesh proportions are retained.
Numerical checks require the front grip and support hand below the bottom rail
and the suppressor ahead of the muzzle; mounted orientation also requires review
in actual Native captures. Cancelling mesh normalization alone is insufficient.

The installer compiles geometry/textures into the DLL and installs the exact
resource manifest and license evidence. It requires no Blender, Python, network
or runtime model importer. Its existing transaction tests also check these files
and repair a damaged installed receipt. `-HeldAssetGallery` runs 64 native SDL
captures (26 base models, four actual installed accessories and loaded/empty
pump states, CPU and GPU),
recording chosen model/accessory identities, fallback, matrices and upload/cache
counters. `-DisableHeldAssets` is a process-isolated diagnostic control for
paired measurements; it is not a new gameplay setting. The extended armed scene
uses canonical `scar_l` so the resource path participates in those comparisons.
The gallery verifier requires all 64 cases, the exact 30 compiled identities,
complete lossless pixel storage and pose-only warm GPU reuse. CI preserves
gallery diagnostics on failure. These checks do not claim visual quality or
ordinary frame-time acceptance.

Local numerical/rendering tests are development evidence. Native selection,
screenshots, both engine pins, final performance and long GPU-debug qualification
remain separate required checks; this change does not declare them passed.

The 0.13.1 grip correction uses a complete palm frame, a measured closed-grab
centre and per-contact handle radii. A wrist-driven arm chain retains source
lengths, limits wrist deviation, and keeps the shoulder outside the camera.
Source grab samples adapt finger closure; unarmed finger bases and all three
trigger phalanges have explicit pose bounds. `tools/calibrate_grips.py --check`
requires the six rigged trigger contacts on their actual source surfaces.
Declared radial sections adapt the two oversized static tool shafts offline;
their originals remain unchanged. Uniform XYZ body scaling is shared with the
muzzle origin. Optic risers use one immutable GPU mesh with pose-only warm reuse.
`first_person_grip_alignment` independently audits all renderer grip paths,
including fallback items and unarmed clips. The counts, limits and Native
validation boundary are recorded in `reports/world-visuals-20261026.md`.
