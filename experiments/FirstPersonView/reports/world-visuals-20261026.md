# Terrain, weapon sights, muzzle effects and world props — 2026-10-26

This is an experimental **0.13.1-dev TEST** change in draft PR #223. It does
not qualify an ordinary 15 ms frame, a manual playthrough, or a certified release.
Local Native gameplay/input harnesses remain disabled by the user's instruction:
no mouse capture, warping, focus changes or injected gameplay events.

## Result and source scope

Natural grass, dirt, gravel and pavement meet through a continuous material
feather baked into the existing opaque floor texture. Each floor still has its
two triangles. There is no transparent strip pass or overlapping depth surface.
Unknown/unseen neighbours, water, stairs, building floors and open shafts cannot
supply a transition. Visibility and same-level Native floor permissions remain
authoritative. Near grass tapers at the edge; canonical rotated fringes and
reduced distant palettes keep the existing draw budget.

The real GPU 17×17 meadow/road fixture uses **45 draw commands with the unchanged
48-command limit**. It retains 68,025 changed grass pixels in the fixture's
108,800-pixel coverage region above the hands. Wind changes 51,426 pixels while
uploading no static mesh/texture data. These are correctness/work counters from
a render-only RTX 4090 fixture, **not GPU timestamps or ordinary game FPS**.
Background games remain running; no exclusive performance session is claimed.

ADS covers all seven authored firearm families and all seven procedural kinds,
including the M27 fallback shown in the report. Procedural optics have a real
open bore. Authored iron sight contacts are calibrated from the actual visible
mesh aperture/post, rather than the scope mounting socket. The red-dot contact
is the source glass centre. Gun scope sockets have inconsistent exported axes;
optics retain their actual mount position in the normalized barrel frame, with
a visible bounded riser where an iron hood/receiver otherwise covers the bore.
Hands, gun, attachments and muzzle effects share the same camera transform.
The original CC0 weapon meshes and source skeletons remain intact.

Confirmed Native combustion shots produce a brief warm flash followed by soft
world-anchored gas/smoke. Suppression reduces flash and uses the real exit.
Bows, pneumatic/energy ammunition and refused attacks do not invent combustion.
The timeline retains at most eight shots for 0.95 seconds; the optional GPU
effect image is bounded to 640×480 and composed through the existing SDL GPU
renderer. CPU effects leave the cached world intact. An effect upload failure
preserves the already rendered GPU world instead of claiming a CPU world fallback.
These are stylized alpha effects, not measured weapon photometry or fluid simulation.

There are **43 original static meshes / 10,460 catalog triangles total** from
[Kenney Furniture Kit](https://kenney.nl/assets/furniture-kit), published by
the author under CC0. The largest selected mesh has 830 triangles. Source GLBs,
the author's License.txt, archive/source hashes and deterministic generated
tables are retained in `assets/world_props`. Furniture includes seating, beds,
tables, storage, kitchen/bathroom appliances, electronics and potted plants.
Round pipes follow visible neighbouring pipe cells and have hollow bores/flanges.
Trees, shrubs, stems, poles, tanks, pumps and common street objects gain volumes.
Unmapped vertical furniture/objects use a bounded extrusion of their visible
source footprint. This is broad common-family coverage, not a separately
authored mesh for every Native/mod identifier. Original inventory icons and
Native creature silhouettes are separate existing presentation paths.

Identical imported furniture uses exact cell-local GPU geometry, avoiding float
rounding differences that previously prevent instancing. Props have a 35 m
coarse LOD, round pipes reduce segments past 18 m, and trees retain their far
impostors past 14 m. Only relevant distance changes invalidate static chunks.
The package carries both world asset license/provenance and effect references;
installation and repair verify them as ordinary hashed payload files.

## References and validation boundaries

The flash/gas presentation uses qualitative references from the manufacturer's
[SureFire Flash Test](https://www.surefire.com/videos/) and
[SilencerCo's explanation of suppressor gas expansion](https://silencerco.com/blog/how-do-suppressors-work).
Reference links do not redistribute footage, photographs or audio. The exact
reference inventory and effect budgets are in `assets/fire_effects/references.json`.

Autonomous tests cover all 512 attachment masks × three lengths × seven fallback
kinds, 63 CPU fallback ADS views across portrait/4:3/wide viewports, authored hand
contacts at the same aspect ratios, real GPU gun/optic images, 32 world-object
fixtures, actual GPU flash/smoke composition, world anchoring, emission gating,
all 43 source bounds, deterministic conversion and hostile input rejection.
Additional checks enforce hidden/open-floor isolation, rotated grass-edge parity,
shared furniture meshes, distance LOD invalidation and zero warm static uploads.
Local results: **47/47 explicitly inventoried autonomous CTest cases, 178 Python
tests and 22 atomic bridge tests pass**. The GPU visual gate includes 21 fallback
ADS frames, 12 authored ADS frames, 32 object views and two actual SDL/GPU muzzle
composites. No Native game process was launched locally.
Native CPU/GPU input/action and game gallery checks run remotely for both pins;
they must be bound to the resulting commit and installer bytes before delivery.

The protected GPU backend, resource lifetime policy, geometry ABI and shipping
shaders are unchanged. Strict readback comparisons and sanitizer gates remain
enabled, including the earlier intermittent llvmpipe comparison limitation;
this work does not claim to diagnose that earlier issue.
