# Modular Vehicle Renderer 2.0

This integration reconstructs installed vehicle parts in the First Person View
module. It is under qualification in PR #250. Legacy remains the default until
the actual Native mutation tests, visual review, performance comparison and
installer gates are complete. A successful mesh unit test is not visual approval.

## Data and ownership

The optional `ncmm.host_api.v1.modular_vehicles` extension is discovered through
`graphics.vehicles.v1`. The published Core V1/V2, Graphics V1/V2, Presentation and
GPU/Mesh layouts are unchanged. Hosts/modules without this optional extension
continue through the existing Legacy geometry path.

The Native producer enumerates `vehicle::get_all_parts()`, retaining distinct
installed layers at the same mount. Removed and inactive fake parts are excluded.
Values include canonical part/variant IDs, mount, actual current pivot, rigid
orientation, health, confirmed open/enabled state, material and Native wheel
diameter/width. Wheel mount steering is inherited only from a real steerable
mount. Wheels are not manufactured from a vehicle category or name.

There are no Native pointers in the ABI. Three callback-scoped copy functions
publish owned bounded arrays: at most 256 vehicles, 4096 parts per vehicle and
32768 parts per frame. Unrepresentable or invalid constructions retain Legacy
as a whole; a partially prepared new car cannot suppress the old body.

Vehicle object identities belong to runtime lifetimes. Part identities move with
parts through vector relocation/erase and are newly allocated for copies. They
are never simulation IDs or serialized identities. Recreated objects after load
must receive fresh identities. The frame revision covers every copied payload
value; presentation seconds are separate. The module can reuse a validated owned
copy when that payload remains unchanged.

Native-permitted visible BODY cells emit their installed parts. Outside that
set, a published floor-top surface may expose the real roof and outward shell
faces proven by an adjacent Native-visible cell outside the structural frame.
These surface-only parts cannot be selected, and cannot expose hidden seats,
frames, cargo or equipment. All other parts are `TOPOLOGY_ONLY` with cell -1:
they describe construction adjacency but emit no geometry or labels. Container
contents, ammunition and hidden occupants are never copied into the extension.

## Geometry and state

The whole construction is analyzed before individual meshes are generated.
Mounts retain all installed layers; a four-neighbour graph identifies actual
frame components. Roofs, cabin span, boundary panels, Native variants, wheel
positions and dedicated cargo determine the shared profile and connections.
No complete-car asset or lookup by vehicle name is used.

The component library covers frame/floor, continuous roof, rounded boundary
panels, hinged door, transparent glazing, wheel, seat, controls, engine, cargo,
armor, solar panel, turret, light, rack, tank, curtain and unknown equipment.
Windshield cells share sloping glazing rather than independent upright boxes.
Corner panels wrap the actual frame boundary; nearby real wheels cut arches.
Armor at a bare frame becomes a plate, not an invented full-height cabin wall.
Unknown installed equipment has a bounded neutral fallback at its actual mount.

Body profiles are inferred from construction, not the prototype name. They
support narrow bikes, open frames, cars, SUVs, vans, trucks, buses, pickups,
armored constructions, trailers/platforms and boats. This is a visual heuristic;
the Native collision grid and vehicle simulation remain authoritative.

Structural nodes come from Native's `vpart_info.location == structure`, not a
nonexistent `STRUCTURE` flag. Wheel-mount flags provide axle/fork components;
flotation parts provide local hull lofts. Saddle and basket definitions keep
their own part identities and compact/wire geometry. Missing mounts are never
bridged into a new complete vehicle.

The copied colour remains the actual Native definition value. Standard generic
doors use the most frequent installed body-panel definition colour instead of
their cyan operational map symbol; custom part IDs retain their authored colour.
This chooses one canonical colour without averaging sprite pixels. Paint/material
changes invalidate the related topology and component recipe.

Native electronic controls with their own instrument light (`CTRL_ELECTRONIC`
and `DOME_LIGHT`) are instrument panels, including unknown mod definitions.
Electronically controlled heaters/coolers do not become dashboards. The
instrument dome-light flag does not put the display on the
roof or add another steering wheel. Narrow vehicles use a compact display at
the controls. Powered lenses/displays use a matching CPU/GPU emissive material;
Native still controls their power state and surrounding light propagation.

Confirmed Native state drives doors and shutters. A damaged part affects its
own surface; broken glass/door leaves and removed parts do not remain as intact
ghost geometry. Wheel angle comes from real absolute component displacement;
redraws, a stopped vehicle or a reality-bubble rebase cannot spin it. Teleports
reset rolling rather than creating fictitious speed.

Occupied camera XY follows the actual seat mount through the shared vehicle
transform. Eye height uses that seat/chassis, body scale and cabin headroom.
CPU, GPU, held effects and the reticle receive the same adjusted callback scene;
the original Native world snapshot is preserved. Exiting restores standing
camera behavior.

## Selection and Native activities

The displayed depth pass first supplies a Native cell. Ray intersections are
then restricted to submitted parts from that cell. The optional feedback API
returns the exact vehicle and part identity, which Native revalidates against
the current copied frame and perception permissions.

FPS interaction can route an actual openable part to the existing open/close
activities. Those actors keep their original time costs, theft checks,
inside/outside restrictions and closing obstruction checks. A precise door
selection uses the Native single-part action. At activity completion the Host
resolves the runtime identity again instead of trusting a stale vector index.
Pending exact-part activities cancel after save/restart because runtime IDs are
deliberately not restored. Native command mode retains ordinary tile selection.

## Assets and reproducibility

Only two independent wheels from Kenney Car Kit 3.1 are imported. The author
publishes the kit under CC0: [source](https://kenney.nl/assets/car-kit),
[author mirror](https://opengameart.org/content/car-kit). The selected archive is
SHA256 `fac7dacac5c7874348cf19729af3ef205f3d366493edaf0a827d93f4fdf3d0c4`.

`assets/vehicles/selection.json` pins the exact OBJ, MTL, palette and license
bytes. The source files retain the author's line endings. The bounded offline
importer converts a right-handed axis permutation, normalizes radial radius and
axle width, transforms normals correctly and bakes the palette. Runtime Native
dimensions take precedence. Bikes and low-detail wheels use procedural geometry.
There is no runtime OBJ loader or external asset download.

`tools/import_vehicle_components.py --check` reproduces the generated tables;
CMake verifies both original selection and generated hashes. The installer
includes the license, selection and import manifest with the compiled module.

## Cache and qualification

CPU and GPU consume the same immutable local component meshes. Geometry keys
exclude vehicle translation/rotation and door/wheel animation transforms.
Whole-construction analysis is retained by actual structure revision. Component
meshes have a 32 MiB bounded cache; copied arrays and topology/pose tables are
bounded by the ABI counts. GPU resources use the existing ownership/lifecycle
and resource budgets. Native animation feedback schedules ordinary bounded
redraws while a confirmed hinge/rolling interpolation remains active.

The Native producer caches only finalized definition feature bits, under owned
canonical IDs and the world lifetime. At most 4096 IDs of 127 bytes are retained;
longer mod IDs use the original resolver without truncation or storage. Client
teardown/world changes discard the cache. Health, availability, open/enabled
state, materials, powered light and installed steering remain live part values.

GPU component templates index exact duplicate corners, including all UV,
normal, material, joint, flag and pivot bits. Triangle order and hard/UV seams
remain intact; the geometry shader does not use the vertex index as an input.
An independent ordered byte map verifies the full expanded corner stream.
Immutable component recipes also retain an explicit absent-pass bit. An opaque
part no longer allocates indexing scratch or scans all triangles again for an
empty transparent pass. This is separate from an expired mesh owner, which must
rebuild after resource eviction. A changed glazing recipe checks both passes.
CPU vehicle triangles keep parallel scanline processing. A precomputed worker
mask skips only workers whose sixteen-row strips cannot intersect the triangle,
before those workers prepare material/derivative data. Source triangles, order,
participating visits and pixel arithmetic remain intact. Sequential streaming
of small projected batches was slower in actual Native measurements and is
disabled; it remains available only as an internal correctness reference.
For triangles spanning at most three rows, the exact original inclusive
scanline intersections can prove that no pixel sample exists before material
preparation. This is not an area/LOD cutoff: source geometry remains complete.
Per-corner detail bands are also omitted where the painter never reads them
(flat reflective, unshaded and contact materials). The original flat reflection,
detailed material bands and all pixel arithmetic remain intact.
The separate raster test compares complete 1280x800 colour, depth and picking
buffers bit for bit, with cold/cached, angled, clipped, inside and hidden cases.
New scheduling changes still require actual Native full-frame qualification.

Compact CPU vehicle projection records reference the immutable, callback-owned
source triangle until every painter worker joins. Only near-clipped vertex
triples need a separate owned pool, addressed by indices that survive growth.
The original full-field record remains a correctness reference. Compile-time
checks bound ordinary record storage to at most half its original size and
keep even clipped logical records smaller. This describes temporary records,
not total game RAM or a measured frame-rate improvement. Their complete pixel,
depth, picking and clipping-lifetime checks also run under ASan/UBSan in CI.

Healthy panel lofts can coalesce locally straight, coplanar spans while
retaining sampled arches, corners, dents and folded/degenerate sections.
An intact roof mount with real roof neighbours at both longitudinal ends has
no longitudinal variation in its height function; it retains every transverse
curvature sample while omitting redundant longitudinal planes. Eaves, damage,
apertures and missing neighbours keep the original grid. The generator's
internal reference subdivision switch is used only by pure surface tests;
the scene cache always uses the production geometry. Independent intersections,
materials, bounds and normals are checked against the original sampled surface.

On SSE2 platforms, eligible opaque smooth vehicle samples use four-lane
perspective/light/fog/packing arithmetic. Scan coverage and scalar weight
increments retain their original order; unsupported materials, clipped row
tails and other architectures keep the scalar path. Worker-local counters
confirm that CI's complete colour/depth/picking comparisons actually exercise
the vector path. Independent numeric controls retain scalar rounding and
depth-epsilon decisions. Both compact-scalar and full-field references remain.

Closed component shells can use the existing GPU material backface flag only
when the camera is outside their bounds, the entire shell is beyond the view
near plane, and every opaque triangle has an
unambiguous authored orientation. Reversed corner winding is normalized, with
all vertex attributes attached, in both GPU variants; triangle, material and
draw order remain unchanged. The unit geometric face plane is separate from smooth
lighting normals. Thin/glass parts, ambiguous geometry, fractional alpha and
inside cameras retain the original two-sided path. Opaque culled/unculled
templates have separate bounded weak owners. Forty-two CI image pairs compare
all pixels, opaque occlusion and picking through outside, angled, overhead,
near-plane and inside views, including smooth wheels and cached mode changes.
Public GPU structures, validators, shaders and pipeline state are unchanged.

Native vehicle reports retain the GPU backend's existing CPU-side geometry,
resource, upload, command-recording and submit timers with the most recent
submission serial. These timers
can overlap; cached images can retain an earlier submission. They are never
added together or presented as hardware execution time. Complete Native frame
comparisons remain the performance acceptance evidence.

Linux process-memory sampling omits malformed/missing terminal data only after
a bounded wait confirms child exit. A live child with incomplete data still
fails, the real exit code remains subject to the original process gate, and no
zero RSS/HWM sample is manufactured. Sampling periods, deadlines, the 900-second
soak and memory thresholds remain unchanged.

Straight panel lofts coalesce redundant coplanar strips only when every original
edge sample agrees with the same linear surface. Real wheel cut-outs, rounded
corners and local dents retain their sampled contour. World-space material
shading and the installed part identity are unchanged; the horizon and LOD
thresholds are unchanged. Complete Native frame comparisons still decide whether
this reduces the observed first CPU-frame cost.

The CPU path also skips back-facing triangles of closed frames, seats, intact
wheels, axles, tanks and bounded equipment when the eye is outside the exact
component bounds. Thin body panels, glass, open hulls and broken tire scraps
remain two-sided. Geometry-only ray comparisons from fourteen directions check
that the nearest actual surface is unchanged. This does not reduce the horizon,
LOD detail or the GPU component meshes, and still requires full Native frame
timing and image qualification.

CPU components use conservative transformed bounds with the renderer's existing
view planes before preparing individual triangles. Every original vertex is
checked against those bounds through rigid yaw/roll poses. Shared position and
normal bit patterns reuse only rigid transforms and per-cell lighting within
that component's current draw; triangle order, per-corner UVs, material and
emission remain unchanged. The immutable vertex index is counted in the same
32 MiB model budget. This is not cross-frame lighting reuse.

Native BODY/FLOOR permissions use one frame-local sorted array rather than two
node-allocated maps. An independent comparison retains the earliest source-cell
index for each permission, including duplicate coordinates and hidden cells.
The array is rebuilt from current Native visibility on every extraction, so it
does not introduce an invalidation cache or retain a previous world's access.

The opt-in `VehicleGallery` Native fixture creates fourteen real prototype
families on each pinned 1040/1807 Host. It records matched Legacy/Modular CPU/GPU
captures, Native identities and full presented-frame durations. Functional
cases include boarding, doors, damage, removal, installation, movement,
deletion and save/reload. Exact source-bound review sheets are a separate bounded
artifact; full captures remain in CI. The capture verifier checks provenance and
comparable inputs, and explicitly does not grant aesthetic acceptance.

Native randomly opens some doors even on undamaged spawns. The intact gallery
and benchmark close actual openable parts through Native before comparison;
explicit open/closed and damaged/removed cases follow separately. Production
spawn behavior is unchanged. Standard Native heavy-duty doors and hatches use
the same body-colour policy as ordinary steel doors; authored mod colours and
wood/cloth materials remain individual.

The mutation sequence takes its side from the actual selected Native door or
wheel mount. Separate intact, open, closed and damaged captures retain the same
camera, with independent checks for all four CPU/GPU and Legacy/Modular views.
Door health is restored before the independent wheel mutation case, so an
unrelated earlier broken door does not contaminate that comparison.

Every family with seats also supplies occupied views. Before/after interior
captures pin the same actual Native seat eye for both implementations; the
production Legacy camera is unchanged. Red/blue cases temporarily change actual
installed Native body/roof part-definition colours under a restoration guard, rather than tinting
the module's input independently. Full angle and interior review pages retain
the original full-resolution captures in the Native artifact.

The extended fixture records seven alternating Legacy/Modular pairs for empty,
one-car, multiple-car, parking and bus scenes at 1280x800, with seven warm frames
per pair. A cold sample means the first frame after a renderer switch and Native
cache refresh; device resources may remain resident. A separate 900-second real
Native loop changes poses and doors, alternates CPU/GPU, saves/reloads and checks
bounded owned memory. Its completion must be verified in the emitted record.

Before the full-size series, the fixture uses the existing quality action to
select actual 1280x800 CPU rendering for both Legacy and Modular, then verifies
those internal dimensions. It restores the prior setting on leaving the fixture.
Earlier full-size canvas reports without internal dimensions can include main's
inherited 960x600 CPU cap and cannot certify full-resolution CPU performance.

CPU records also retain the actual internal dimensions and the existing module
callback, geometry, raster and effect telemetry. Callback time is contained in
the full presented-frame wall interval; the internal debug timings are not
claimed to be mutually exclusive and must not be summed as independent work.
Reuse can legitimately report zero raster calls. GPU metrics describe the last
3D submission and can remain unchanged when a finished image is reused.

An occupied driving sequence starts a fueled real Native engine and calls the
same `pldrive()` command used by CDDA, followed by `map::vehmove()`. It verifies
actual movement and turning, the driver's Native seat position and stable
camera/part ownership across eight ticks, with matched CPU/GPU captures. It does
not inject desktop input, replace physics or constitute a manual playthrough.

Reload qualification follows Native's actual quickload lifecycle: set
`QUIT_NOSAVED`, run the ordinary quit cleanup through `do_turn()`, then call
`setup()` and `load(save)`. Directly calling `load(save)` on a live game is not
that lifecycle: Native appends saved global EOCs to the existing live queue,
while normal setup clears the queue first. The report retains the actual queue
size as well as process RAM; neither memory limit nor the required soak duration
is reduced. A changed test lifecycle still needs a complete fresh Native run.
As in the canonical non-interactive smoke's initial setup, a successful load
marks the character playable before rendering; the interactive first input turn
is not entered by this fixture. Production loading and availability guards are
unchanged.
The complete lifecycle uses a dedicated vehicle qualification gateway. The
pre-existing multi-level coordinate serialization gate retains main's original
in-place load gateway and deadlines; it is not a complete world restart test.
Native process/bootstrap restart and vehicle lifetime reload are separately
qualified. A vehicle fixture must not silently replace another test's scope.

Vehicle exteriors use separate outside-surface permissions at their actual
source cells. Roofs retain their own permission; boundary panels/doors/windows
carry a four-bit outward-face proof. No permission exposes the hidden interior.
Real boundary wheels and directional lamps use the same proof, so an upper
view does not lose visible outer hardware beneath an opaque vehicle cell.
Surface-only cells use Native RGB sampled above the real roof rather than the
unlit cabin below it. Outside shading follows Native's outside classification.
The terrain underneath stays present. Discovery enumerates actual loaded vehicles
and parts rather than treating a 2D sprite or `veh_at` display cache as the source
of a 3D vehicle's existence. Cached GPU cell keys include Legacy/Modular ownership;
rigid motion stays outside the static terrain key.

Dashboard equipment is distinct from a steering control. Directional light
orientation follows the Native part angle; dome/aisle/overhead lights use their
actual mounting family. Emission requires the Native enabled/available/power
state, including power-deficit suppression and self-powered light parts. Broken
wheels show rims/remnants, and broken roofs have local apertures. Neither gives
permission to reveal hidden occupants or container contents.

The steering rim, hub and column sit ahead of the actual seated eye, with the
instrument pod recessed behind them. Pure projection and independent ray tests
check that the forward view retains the rim and that the console cannot obscure
it. They supplement actual CPU/GPU cabin captures; they are not image tests or
a manual driving qualification. The Native seat position and camera remain the
source of the eye, and removing either real part still removes its own mesh.

Glazing joins the nearest actual roof edge on the same connected construction
and level. This covers Native layouts that put the roof over the seats and
windows one mount outside it, including triangular corner panes. The roof's
own footprint and part count stay unchanged; no opaque surface fills a missing
roof. Removing a roof invalidates the dependent glass recipe. A disconnected
nearby roof cannot pull another construction's window frame towards itself.

Exact shared position/normal indices are built in one bounded flat scratch
table, retaining first-seen vertex order and every original corner UV. This
avoids a heap node per distinct vertex during component preparation. It is a
construction-cost change, not a reduction of geometry, resolution or quality;
complete Native frame timings still determine its measured benefit.

CPU opaque vehicle sources borrow one callback-owned appearance for each
component/material, instead of copying that appearance into every triangle.
Owned vertices, source order, clipping, interpolation and picking are retained.
The material deque and source arrays outlive every raster worker; neither is
stored in the world cache or exposed through an ABI. Full-field and compact
material-copy references remain available to the complete-buffer CI comparison,
including distinct Native light/owner cells, near clipping and camera interiors.

The GPU may reject a proven one-sided range before fragment shading using the
same shaders and existing material-side condition. Eligibility requires exact
agreement between index winding and every published geometric unit normal,
fully opaque unchanged positions, a static identity bone, and proper rigid draw
and camera bases. Reflected/nonrigid/skinned/billboard/transparent/held or otherwise
unproved submissions keep the original rasterizer. A separate pure data/math
test checks this eligibility; the original frozen backend and all 414 ordinary
readbacks still determine pixel equivalence on real CI devices. Neither the
source-storage reduction nor this eligibility proof establishes a frame-time win.

Stationary CPU components can compose into the existing opaque world cache.
The content key includes every exact component pose, immutable model recipe,
part/cell identity and surface permission; the existing complete world, light,
camera, tileset and epoch keys remain in force. Native velocity, active hinges
or wheels, and fractional opaque alpha retain independent drawing. Cold frames
dispatch the original world and component sources in one ordered worker pass;
transparent vehicle glass still joins the original transparent ordering.
Complete color, depth, cell/creature ownership and crosshair comparisons cover
both cold composition and warm reuse, including invalidation after mutations.

Equivalent roof mounts reuse one local model when their exact transverse
profile, real roof neighbours, material, health and state agree. Each installed
part keeps its own pose, identity, Native cell and permission. Longitudinal
mount/cockpit bounds that do not affect generated roof data are absent from
that recipe. Pure controls compare every generated position, normal, UV,
material, shared index and bound bit, including damaged roofs and eaves.

An optional component fragment program excludes only branches disproved by
strict immutable material/corner and batch proofs: exact opacity, smooth
untextured surfaces, no coverage/creature/billboard flags, no liquid/contact
finish, an actual world cell, and the existing static-root/proper-basis checks.
Its selected ordinary detail, reflections, lighting, projection and explicit
fragment-depth expressions are retained. Proven flag bits, lower-light state
and opacity equal the specialized constants. Every other material/cell uses
the original program. Paint and metal additionally share one exact finish
specialization: both select the same original reflection expressions, with no
other arithmetic change. Other finishes keep their original component program.
All fourteen original program binaries and the general
vertex shader remain; sampler/storage/uniform layouts remain 3/2/2. Earlier
vertex-only and detailed-SIMD experiments lacked a complete-frame win and were
removed. Their measurements remain in the review evidence.

The frozen comparison keeps all 414 original readbacks and eight mixed-depth
near-plane/distant/coplanar cases. Four further source-validated readbacks cover
ordinary world lighting, the valid opaque sentinel cell, lower-slab lighting
and fractional opaque alpha. Three more compare paint, metal and the independent
non-reflective material path. These 429 complete readbacks and actual Native
frame/resource/image qualification are required; source or compiler instruction
counts never establish a game performance win.

The CPU path retains its original ordered scalar/SIMD painter. Additional
depth-owner passes did not improve complete Native frames and are not used.
Immutable models intern exact three-float normal identities and raw local face
directions. CPU lighting remains per draw, with its actual Native cell, pose,
environment and exterior permission; only identical input normals reuse a value.
Models without these optional tables retain the original calculations. The
existing 32 MiB model budget includes every auxiliary table. Wide frame beams
share a model only when their actual optional floor plates are identical.
CPU geometry telemetry includes modular source conversion before rasterization.
The joint CPU queue can shade proved flat-lit opaque world faces four pixels
at a time. Flat lighting has its own exact kernel; it never substitutes an
interpolated equal-corner value. Eligible opaque textures retain their original
per-pixel UV/filter/RGBA sampling and fallback-colour mix before SIMD light/fog.
Detailed, fractional, cutout, liquid, contact, creature, smooth textured and
overlay paths retain scalar arithmetic and all bands.
General GPU opaque ranges retain source-over blending. Only the four existing
strict alpha-one component/reflective variants use equivalent overwrite state;
their original fragment programs, explicit depth writes and bindings are intact.
Intact roof patches retain their original upper/inner samples and now seal the
35 mm rim along every actual patch boundary. This permits the existing proved
closed-shell culling path. Broken apertures retain two-sided rendering. Twelve
additional full-image pairs cover intact/broken roofs outside, below, inside
and near the camera, retaining all previous 429 readbacks (453 in total).

Native permissions are rebuilt from the current published cells every frame.
Compact coordinate bounds use an exact direct index of at most 2 MiB; sparse
or extreme coordinates keep the original ordered fallback. Duplicate body and
floor identities retain their independent original minima. No installed state,
visibility, Native pointer or gameplay object is cached by this lookup.

The long-run gate also validates source-bound process RAM samples, not just
module/GPU accounting. It rejects a late resident median more than 50 percent
(at least 256 MiB) above the warmed baseline, or a transient peak above twice
that baseline (at least 1 GiB headroom). Lifecycle cleanup drains SDL's pending
deletions after releasing device children, as well as draining queued draws
before release; ordinary frame submission does not wait for device idle.

Release qualification before enabling the new default or publishing a final
installer requires inspecting the actual Native images; completing custom-construction,
lighting/turning, tileset/Z/restart and long-run cases; comparing complete frame
samples and resource behavior; and passing both Windows installers and lifecycle gates.
Hosted software-adapter timings do not establish RTX 4090 FPS or the approximately
15 ms hardware target. No local game/input/device tests or manual playthrough
have been performed for this integration.

Ordinary translucent component glass has a separate proved fragment path.
It retains the existing vertex shader, source alpha, blend/depth state,
lighting, fog, explicit fragment depth and globally stable transparent order.
Only finish0, smooth untextured ordinary corners with a real cell and exact
static-root/rigid-pose proofs qualify. All other flags, material finishes,
alpha classes, textures, poses and lower-slab cases use the general program.
All32 original shader arrays remain; one SPIR-V/DXIL fragment pair is added.

The CPU keeps that same transparent order and uses compact projection and
four independent smooth-light/fog/blend samples for proved ordinary glass.
It reads each original scalar background only after its original depth test,
blends floating colour before byte packing, never writes opaque/held depth or
owners, and retains each exact crosshair focus minimum and the scalar tail.
The original scalar and all previous opaque kernels remain unchanged.
Independent numeric controls and complete1280x800 colour/depth/owner/focus
comparisons cover overlapping panes, opaque occlusion, clipping and caching.
This source change still requires full Native image and frame qualification.

The CPU can prove absent fine grain on stone, asphalt, rubber, cloth, skin and
plaster when its original fine bands are exactly zero at all three corners.
Those six finishes have no other medium-band term. The original bands and
every nonzero-grain, noisy, water, mortar and reflective expression remain;
this changes no sampling threshold, LOD, geometry or material colour. The
proof applies only to a valid Modular vehicle frame. Legacy retains its old
material decision. A disabled internal proof option supplies the full original
material reference, with complete buffer and positive-execution CI controls.

Proved opaque finish-zero components use an additional constant-finish
fragment. It retains the exact original component vertex/lighting/fog/depth,
alpha-one state, culling proof and draw order. Other finishes, fractional alpha,
lower light, animated roots, textures and unproved cells retain their original
programs. All34 previous shader arrays and463 readbacks remain; one fragment
SPIR-V/DXIL pair and ten full positive/fallback readbacks are added. Offline
compilation and pure checks do not establish a Native frame-time improvement.
