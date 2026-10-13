# Creature Appearance Reconstruction — First Person View 0.11

This revision builds on the merged 0.10 SDL_GPU engine. CPU and GPU consume the
same immutable anatomical models, rest-space UV charts and source-derived
materials. It adds no Monster-ID model/texture overrides, runtime AI requests,
extra window/device, mandatory assets or changes to the existing Host ABI.

## Confirmed causes in the previous implementation

The audit started at main `7f8a48a714e082daf67176e5e4a9806f878eef42`, including
PRs #201/#202, and the actual 1040 SDL captures from run `38203247598`.

| Previous behavior | Consequence | Current behavior |
| --- | --- | --- |
| Native sprite reduced to at most 64 × 64 | A 128 × 128 source retained only 25% of its pixel samples | Exact, stride-aware copy up to the existing validated 512 × 512 limit |
| Every region forced into 16 × 16 | Crops discarded large-source features and expanded small ones without information | Native-resolution crops; original sprite stored separately |
| Rear surface = 88% mean + 12% sampled color | Clothing, hair and animal markings became nearly uniform | Observed regional edge patterns and row palettes continue onto unobserved surfaces |
| UV horizontal coordinate derived from the normal | Nonuniform projection, folded stripes and repeated details on segments | Position-based rest-space charts with shared regional physical bounds |
| Hands used the head material | Faces/head colors could repeat on palms | Separate hand and foot regions |
| Front/back flags differed inside a triangle | A single face could interpolate across incompatible charts | Vertices split at chart boundaries; one chart/material per triangle |
| Side charts joined the wrong front/rear edges | Colors jumped at humanoid sides; profile tails could receive muzzle colors | Edges account for reversed front/rear orientation; profile end caps retain their own observed edge |
| All impostors used 48 × 64 | Broad/wide bodies received too few horizontal samples | Source-aware height 48–112; physical-aspect width 32–160 |
| LOD3 replaced source colors with a mean | Distant NPC clothing and species color composition disappeared | All four levels retain source colors |
| Encoded signed-depth textures allowed hardware mip selection | Averaging encoded bytes could change sign/carry | Normal/depth data use nearest sampling of the base level |
| Flying impostors baked every pose at time zero | Distant wings froze while nearby wings animated | Existing two motion frames bake opposite wing phases and use the same flight clock as geometric LODs |
| Animated bodies received world-space material grain | Detail moved through the skin rather than with the body | Source markings stay attached to the skeleton; metallic response remains |

## Source, projections and materials

The Host reads the foreground sprite rectangle at native size with texture
color/alpha mods disabled and nearest sampling. Its existing bounded sprite
readback/cache API is retained. The module copies only callback-owned pixels;
neither SDL/game pointers nor providers reach the background worker.

The original RGBA, including transparent pixels, is kept unchanged. Opaque
body charts extend the closest observed color into transparent crop areas;
transparent RGB never enters the body palette. The silhouette and anatomical
priors retain translucent bodies even when a few opaque eyes/highlights are
present; a tiny opaque core does not replace the whole silhouette. Anatomical
priors define separate head, torso, lower body, limb, hand, foot, wing, tail,
appendage and detail crops. Only regions used by the anatomy allocate native
textures: an unknown 512 × 512 source must not allocate ten redundant copies.
Humanoid heads follow the upper connected silhouette and its neck/shoulder or
persistent colour boundary. This keeps an offset head separate from raised
arms and excludes shirt colours from the face when the boundary is visible.
Ambiguous silhouettes retain a bounded anatomical prior.
Raised hands can lie above the face and cross the image centre. The head
locator follows a narrow central run attached to the trunk, then stops at the
gap to overhead hands. Its horizontal extent excludes neighbouring arms;
joined shoulders do not prematurely cut off the lower facial pixels.
Hands sample a lateral upper-body extremity instead of the trouser region.
Humanoid arms use a separate existing chart with shoulder-to-wrist bounds;
source glove colours remain observations. A sustained central colour boundary
selects the shirt/trouser split when visible. The pelvis and both leg segments
share one lower-body projection, instead of reusing shirt/arm crops on thighs.
Stationary mechanical anatomy uses a braced stand, housing and directional
working barrel. This shared semantic construction replaces its full-height
capsule without adding a per-ID turret model.

A part's local frame generates geometry and normals. Its rest-space position
selects UVs within shared anatomical bounds. Both geometric LODs use those
same bounds; adjacent segments do not restart a whole head/clothing texture.
All vertices in a triangle have the same chart. Animation transforms joints
and normals without regenerating UVs or textures.

Material classifications include flesh, cloth, fur, scales, chitin, feathers,
slime, metal, synthetic and fantastic surfaces. Source pixels provide their
individual markings. This is a stylized source-preserving pipeline, without
an additional PBR stack or a generic random-noise overlay.

An upright sprite uses a canonical front chart; profile anatomy uses lateral
projection. A single sprite does **not** establish an authoritative camera
direction or reveal its hidden sides. Rear heads use row palette continuation
without copying eye/face details. Other hidden surfaces continue observed
regional edges. These are conservative reconstructions, not recovered views.
An unknown body preserves its observed image on one canonical front chart;
its rear uses row palettes rather than duplicating a possible face.

## Filtering and LOD

Close source details use native texel sampling. CPU sampling estimates the
perspective-correct UV footprint and transitions toward bilinear/trilinear
minification. Odd-size mip generation includes edge pixels, and filtering
weights colors by alpha to prevent colored/black silhouette halos. CPU
impostors use the same color-pyramid policy. GPU color sampling uses nearest
magnification and linear/trilinear minification on the existing SDL device.

The pipeline retains the existing CDDA-style display-space color convention.
It does not introduce a separate linear-light/PBR exposure model. Original
sRGB byte colors are unchanged at native sampling; fractional color filtering
follows the existing GPU UNORM convention. Gamma/exposure is not presented as
a new physically based rendering feature.
CPU impostors select the same fractional mip level once per triangle. Empty
filter neighbourhoods skip colour arithmetic; view rotation also stays outside
the pixel loop. Bilinear/trilinear weights and perspective-correct UVs remain.

Normal and signed-depth atlases remain data. Color minification must not apply
to their encoded bytes. Mesh/atlas materials share original charts, and
four-level distance hysteresis remains. A two-degree direction hysteresis
prevents repeated angle flips near an impostor-sector boundary.

The impostor resolution describes projected geometry, silhouette, normals and
poses; it does not manufacture source texture detail. Subpixel creatures and
details smaller than an output pixel cannot remain fully recognizable.
The resolution budget follows the occupied source silhouette rather than its
transparent canvas, so padded tiles do not multiply atlas baking and storage.

## Bounded generation and lifetime

The existing single worker, four requests per frame, 16-job queue and 64 MiB
retained model/atlas budget remain. Accounting includes original sprites,
dynamic skin planes, mip vectors and adaptive color/normal/depth atlases.
Sources are copied before queueing. A captured epoch is rechecked after the
provider returns, so resetting a world/tileset during copying cannot enqueue
old pixels into the new epoch. Appearance generator revision 4 invalidates CPU
and GPU resources independently of the unchanged shared anatomical policy.

Gameplay never waits for generation or writes diagnostic files. It continues
to draw the existing closed-volume placeholder until resources are ready.
First use copies the native source without the generic billboard palette/PCA
and mip preprocessing; anatomical analysis runs on the worker. Generic
textures prepare their data lazily when actually needed for drawing.
The Host's existing first-use sprite readback remains synchronous and bounded;
this change does not claim to remove that Host API limitation.

## Reproducible qualification

`creature_appearance_test.cpp` verifies native source/stride/alpha preservation,
background exclusion, high-contrast head detail, hidden-face suppression,
chart boundary continuity, mip/halo handling, UV/material consistency,
adaptive atlas aspect, material classification and the 512-pixel unknown-body
memory case. Existing mesh, animation, cache, rendering and shipping-module
recovery tests remain. The module workflow includes Linux ASan/UBSan and atomic
Host-transform checks.

`ncmm_graphics_appearance_smoke.inc` creates actual SDL frames for zombie,
hulk, dog, spider, ant, centipede, crow, fish, snake, Mi-Go, turret, blob, a
deterministically clothed NPC, and an isolated JSON creature with unknown
anatomy. It sweeps eight directions, three distances and day/blue-night
lighting, plus applicable movement and NPC prone states. Small creatures use
the closest supported game-cell distance; large far views stay within the
actual sight/snapshot range. Actual distance and selected LOD are recorded.
The isolated observer has fixed perception 64 as well as debug night vision:
CDDA's fish water camouflage otherwise hides distant fish according to the
random quickstart avatar's perception. Creature flags and game visibility
rules are retained. Combat debris is cleared before paired captures.

Both official 1040/1807 graphics/sounds packages are exercised. The comparison
uses fixed seed `ncmm-appearance-011`, presentation clocks and entity identity,
and an exact, unmodified main 0.10 CPU DLL built at the above commit. Reports
retain camera, pose, source tileset, lighting and output pixel information.
CDDA normally selects monster sprite variants from the object's memory address.
Only this qualification environment selects the first foreground variant,
so paired processes really share the same original pixels; ordinary game
variants remain unchanged. Native source hashes, including composed NPC
clothes, are checked alongside creature dimensions, heading and flags.
The disposable NPC clears randomly selected clothing variants; all directions,
distances and modes must have one identical source-pixel hash per subject.
The existing actual game benchmark covers groups of 1/10/50/100 creatures,
four viewports, current CPU/GPU and the paired main CPU reference, with
median/p95/p99, generation, upload, draw and memory counters.

Set `NCMM_FP_APPEARANCE_DEBUG_DIR` only in qualification to export:

- exact RGBA bytes and checkerboard source preview;
- anatomical crop overlay, native texture planes and UV wire layouts;
- untextured/textured geometry views;
- all eight directions × seven pose color/normal/depth atlases;
- a provenance manifest with source dimensions, region observations and bytes.

Debug export runs on the worker after pure generation timing. Measured warmed
render loops do not contain export I/O; diagnostic first-ready wall time can
include it and must not be advertised as ordinary gameplay generation cost.
Actual hardware, source commits, verified installer hashes, visual findings
and performance limits belong in the qualification report for the tested
commit. Synthetic/native timings are never player FPS. Software Vulkan/D3D12
qualification does not establish discrete-GPU performance.
