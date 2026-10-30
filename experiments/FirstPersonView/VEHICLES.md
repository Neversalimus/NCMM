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

Only Native-permitted visible BODY cells can emit geometry. A real installed
part outside that permission set is explicitly `TOPOLOGY_ONLY` with cell -1:
it may describe construction adjacency but cannot emit geometry or a label, or
be selected. Cargo contents, fuel contents, ammunition and hidden occupants are
not copied into the extension.

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

The opt-in `VehicleGallery` Native fixture creates fourteen real prototype
families on each pinned 1040/1807 Host. It records matched Legacy/Modular CPU/GPU
captures, Native identities and full presented-frame durations. Functional
cases include boarding, doors, damage, removal, installation, movement,
deletion and save/reload. Exact source-bound review sheets are a separate bounded
artifact; full captures remain in CI. The capture verifier checks provenance and
comparable inputs, and explicitly does not grant aesthetic acceptance.

The extended fixture records seven alternating Legacy/Modular pairs for empty,
one-car, multiple-car, parking and bus scenes at 1280x800, with seven warm frames
per pair. A cold sample means the first frame after a renderer switch and Native
cache refresh; device resources may remain resident. A separate 900-second real
Native loop changes poses and doors, alternates CPU/GPU, saves/reloads and checks
bounded owned memory. Its completion must be verified in the emitted record.

Vehicle roofs use a separate surface permission where Native exposes their
architectural upper-storey floor. That permission emits only the installed roof,
without granting visibility to the hidden interior. The corresponding synthetic
slab is replaced only for an accepted Modular component. Cached GPU cell keys
include this replacement and Legacy/Modular ownership; rigid motion stays outside
the static terrain key.

Remaining qualification before enabling the new default or publishing a final
installer: inspect the actual Native images; complete custom-construction,
lighting/turning, tileset/Z/restart and long-run cases; compare complete frame
samples and resource behavior; pass both Windows installers and lifecycle gates.
Hosted software-adapter timings do not establish RTX 4090 FPS or the approximately
15 ms hardware target. No local game/input/device tests or manual playthrough
have been performed for this integration.
