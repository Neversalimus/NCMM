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
Local results: **48/48 explicitly inventoried autonomous CTest cases, 182 Python
tests and 22 atomic bridge tests pass**. The GPU visual gate includes 21 fallback
ADS frames, 12 authored ADS frames, 32 object views and two actual SDL/GPU muzzle
composites. No Native game process was launched locally.
Native CPU/GPU input/action and game gallery checks run remotely for both pins;
they must be bound to the resulting commit and installer bytes before delivery.

## Grip audit added after the visual reports

The previous one-axis constraint left palm roll undefined. Its fixed elbow
pole produced reversed support wrists, and its two-link trigger solver forced
the final phalanx to world-forward. Matching an anchor alone did not detect
these errors. The new solver uses the knuckle row and palm direction, the
measured centre of the closed source grab, and the actual handle radius. Arms
follow the wrist while preserving both source bone lengths and hiding the
shoulder below the transformed camera edge. Longitudinal handles also choose
an incoming forearm direction from below. Unarmed finger bases are bounded.

All six independently weighted authored triggers are calibrated to their real
triangle surfaces by `tools/calibrate_grips.py`. The three-phalanx index solve
preserves bone lengths and limits curl and extension. The unrigged revolver
retains its declared mechanical contact. Thicker handles select a more open
original grab pose from an immutable bounded cache. Original skin weights,
textures and animation resources remain intact.

Primary grips sit below the trigger finger; support contacts are on the actual
fore-end, with longer rifles/shotguns supported nearer its rear. The gas can
contact moves from its handle opening into the solid upper bar. Shovel and
pickaxe handles were toy-proportioned at Native's real length: declared offline
radial sections thin their shafts, preserve the head, topology, UVs and materials,
and transform normals with the deformation. Original source OBJ files remain
byte-identical. The shovel's primary contact is its crossbar. Body-size scaling
is uniform in XYZ, including the muzzle effect origin, so a small character no
longer squashes the gun or pushes the forward hand out of reach. Bow support/draw
handedness and all procedural handle axes are corrected.

The independent grip test examines **5,760 renderer frames / 11,496 submitted
hands** across all 26 body models, all supported authored accessories, seven
procedural firearm/bow families, poles, generic/missing sprites and unarmed
poses. It includes idle, equip, confirmed-attack animation, ADS, portrait/4:3/wide
viewports and body scales 0.7/1/1.25. It checks submitted joints and palettes,
bone lengths, finger-base angles, hinge direction and silent fallback. Maximum
wrist deviation is **29.651 degrees** inside the 35-degree pose envelope;
maximum finger-joint flexion is **112.287 degrees** inside the configured
middle/distal limits. These are practical rendering constraints, not anatomical
measurements or a manual playthrough. All 26 independent CPU previews were
visually inspected; the exact Native CPU/GPU gallery is a separate remote gate.

The first remote Native gallery exposed streamed optic-riser triangles on a
warm frame. The riser now uses one retained immutable unit mesh and ordinary
draw transforms; the regression test requires zero warm mesh/texture uploads.
The Native gate is retained, rather than allowing extra dynamic geometry.

The Linux appearance sweep also exposed a fixture obstruction: its solid
RGB control lamp at (1,-1) stood on a diagonal camera ray and completely hid
the low snake in its blue-night case. The preserved Native image confirms
the obstruction. The lamp now stands at (3,-1), outside the eight camera rays
and the subject's half-step. Visibility, depth tests and the nonzero-creature
pixel requirement remain enabled. Completed Native held galleries are also
published immediately, while the remaining qualification runs continue.

The protected GPU backend, resource lifetime policy, geometry ABI and shipping
shaders are unchanged. Strict readback comparisons and sanitizer gates remain
enabled, including the earlier intermittent llvmpipe comparison limitation;
this work does not claim to diagnose that earlier issue.

## Furniture facing, partitions and repeated-room preparation

The terrain `orientation` field describes a structural axis, not a saved
furniture front. Using it for every prop left rows pointing the same way and
could only produce two rotations. The new layout policy keeps four cardinal
facings. Explicit directional identifiers win; otherwise chairs face adjacent
work surfaces, desk working edges face their chairs, and cabinets/appliances
put their backs at visible walls or cubicle partitions. Storage rows face the
aisle. Ties are deterministic, independent of camera/player position. Only the
four visible same-level neighbours are sampled, within the existing cached-cell
invalidation footprint. No hidden room furniture is inspected.

Imported furniture uses a common -Y front. Composed objects rotate their local
offsets too, so monitors stay on the correct side of their desks. The separate
3TD cabinet had the opposite source axis and is calibrated offline by 180
degrees. Room dividers now form thin framed panels, 1.87 m tall, with connected
straight, corner, T and cross layouts. They no longer use the 0.72 m generic
sprite-footprint extrusion.

The filing cabinet is the original **388-triangle** visual node from Ron
Kapaun / 3TD Studios' [Office Starter Pack](https://opengameart.org/content/3td-office-starter-pack).
Its CC0 dedication is retained as the original pack PDF and a UTF-8 license
transcription. Two pinned diffuse images are sampled offline into face colours;
source lamps, exporter collision geometry and environment reflection are
excluded. The catalog now contains 44 models: 43 original Kenney selections
and this cabinet. Installer manifests and notices include both sources, with
all 49 source/license files hash checked. There is no runtime model importer.

Repeated authored furniture now shares a complete opaque **floor plus prop**
template. Each new combination is expanded and hashed once, while individual
cells retain their own draw transform, lighting and current index. A maximum
of eight props per assembly and 512 weak template keys bounds this additional
cache. Existing resource budgets and failure handling remain enforced. There
is no extra floor/prop draw split; transparent models retain their old path.
World/resource resets release the template owners and accounting.

A local pure preparation ablation uses a 33x33 room with 496 objects, including
384 authored props. Its comparison copy returns false at the existing `prop`
callback, selecting the exact ordinary triangle path, with no other source
change. Nine cold samples and 54 warm camera samples per variant gave:

| Same updated scene | Ordinary triangle expansion | Shared cell templates |
| --- | ---: | ---: |
| Cold preparation median | 29.990 ms | 7.123 ms |
| Warm preparation median | 0.196 ms | 0.201 ms |
| Draw commands | 1095 | 1095 |
| Unique meshes / indices | 32 / 13536 | 32 / 13536 |
| Template builds / reused prop cells | 0 / 0 | 5 / 379 |

The material-resolved triangle multiset, transforms, cells and draw passes
match (canonical geometry digest `3523857513230377072`). Opaque triangle and
material table ordering may differ. These are CPU scene-preparation numbers,
with background games still running, not ordinary game frame times or a 15 ms
qualification. The warm timings do not establish an improvement.

Validation: 49 explicitly inventoried autonomous C++/render-only tests pass,
including a new furniture layout/cache test, and 187 Python tests pass. The
unchanged strict GPU device suite passes on RTX 4090 with 64 additional
orientation images (16 families x four walls), 34 ordinary object views,
zero CPU world raster and zero warm static uploads. Those orientation captures
were visually inspected; the cabinet fronts, appliance doors, chair seats,
desk drawer fronts and monitors face the aisle. Native gameplay and installer
checks must still bind to the resulting source revision before delivery.
No local Native process, input injection, mouse capture or focus change is used.

The Linux import replay found platform-libm double tails in the cabinet's
manifest dimensions. Those bounds now use the generated float table's eight
significant digits. The compiled geometry remains byte-identical. A regression
perturbs both source rotation trigonometric results by one double ULP in both
directions and requires every generated byte, including the manifest, to match.
The exact source-replay gate is retained.

## Continuous walking and frame-packet preparation

WASD now moves a fractional camera position inside the authoritative Native
tile. Its direction follows the full camera angle, with normalized diagonals,
short acceleration and braking. Releasing a key leaves the camera at the
fractional position. Crossing a tile boundary requests exactly one adjacent
Native movement action; acknowledgement rebases the offset without moving the
displayed world position. Native processing time never becomes catch-up motion.
Unexpected displacement, teleportation and level changes reset the fraction;
stairs keep their existing vertical transition.

A 0.18 m circle checks an owned 3x3 collision stencil, sampled on the Native
main thread during the existing input lease. Walls, closed doors, creatures,
vehicles and unsupported floor edges block motion, while substeps permit wall
sliding without diagonal corner tunnelling. A newly changed obstacle constrains
an old offset toward the current tile centre. A refused Native action fences
the held key and retracts the crossing. Menus, focus changes, released capture
and vehicle mode suspend continuous walking. This is presentation movement:
Native combat distance, time and world simulation remain tile based. It does
not introduce continuous physics or simulate additional turns within a tile.

F5 exposes a live walking-speed setting, 1--6 m/s, default 3. Running, crouching,
prone movement and terrain cost modify that speed. Camera footfall is 9 mm;
hand gait is driven by actual displayed distance rather than an idle timer,
with 6 mm sway and an 80% reduction while aiming. Origin shifts cannot reset
every step's gait. Fractional views reuse authoritative visibility and static
world resources; eight actual GPU captures require different world pixels,
correct camera coordinates and zero warm static uploads or cell regeneration.
The CPU path separately verifies changed fractional-view pixels and reuse of
an unchanged fractional view.

The module reuses CPU frame-packet vector storage between GPU submissions.
Each reset releases immutable resource owners and clears all frame metadata,
draws and descriptors. Retained vector storage is capped at 16 MiB, included
in producer-memory accounting, and released on renderer/lifecycle resets.
GPU backend allocation, resource lifetime policy, ABI and shaders are unchanged.
An isolated 200-frame, 8000-draw producer comparison gave:

| Same packet geometry | Fresh packet | Reused packet |
| --- | ---: | ---: |
| Preparation median | 0.6541 ms | 0.2093 ms |
| Heap allocations | 6000 | 225 |
| Total allocated bytes | 633377800 | 3177369 |
| Retained vector storage | 1481680 bytes | 1481680 bytes |

Both paths have geometry digest `13728559659972321324`. These are CPU packet
preparation measurements with background games running, not full game frame
times, GPU timings or a 15 ms qualification.

Local validation passes 49 explicitly inventoried autonomous C++ tests plus
the separate hidden render-only GPU scene on RTX 4090, 192 Python tests and
22 atomic Native-patch tests. No local Native game, gameplay/input harness,
mouse capture/warp or focus change is used. Remote Native qualification now
requires 172 FPS cases per pin, including presented sub-tile motion with zero
Native action/time cost at the declared F5 minimum of 1 tile/s, zero-cost wall blocking and three costed wall-slide
crossings. That fresh Native result and the exact installer identities must
be checked before this new TEST build is delivered.
The sub-tile probe records positions from the frames actually submitted for
presentation, rather than a newer input state observed during rendering.
Ordinary movement cases retain their configured speed.
