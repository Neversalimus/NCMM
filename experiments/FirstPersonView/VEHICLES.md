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

The opt-in `VehicleGallery` Native fixture creates fourteen real prototype
families on each pinned 1040/1807 Host. It records matched Legacy/Modular CPU/GPU
captures, Native identities and full presented-frame durations. Functional
cases include boarding, doors, damage, removal, installation, movement,
deletion and save/reload. Exact source-bound review sheets are a separate bounded
artifact; full captures remain in CI. The capture verifier checks provenance and
comparable inputs, and explicitly does not grant aesthetic acceptance.

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

The long-run gate also validates source-bound process RAM samples, not just
module/GPU accounting. It rejects a late resident median more than 50 percent
(at least 256 MiB) above the warmed baseline, or a transient peak above twice
that baseline (at least 1 GiB headroom). Lifecycle cleanup drains SDL's pending
deletions after releasing device children, as well as draining queued draws
before release; ordinary frame submission does not wait for device idle.

Remaining qualification before enabling the new default or publishing a final
installer: inspect the actual Native images; complete custom-construction,
lighting/turning, tileset/Z/restart and long-run cases; compare complete frame
samples and resource behavior; pass both Windows installers and lifecycle gates.
Hosted software-adapter timings do not establish RTX 4090 FPS or the approximately
15 ms hardware target. No local game/input/device tests or manual playthrough
have been performed for this integration.
