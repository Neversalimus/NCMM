# First Person 0.7 work checkpoint

Baseline: d8d22bd (0.6), PR 200. Target: CDDA 1040 and 1807 Windows.

Requested work is grouped into independently committed changes:
1. Semantic geometry and callback-scoped presentation data.
2. Sprite aim/name display and first-person click attacks.
3. Vehicle shell, hood, cockpit and interior ceilings.
4. Materials, liquids, dimension skies, volumes and source lighting.
5. Physical view models, attachments, grips and avatar appearance.
6. GPU atmosphere composition, CPU fallback, audit and installer qualification.

Checks must include native render regressions, actual game data taxonomy,
atomic source transforms, both pinned Host syntax checks and real Windows SDL
installer smoke. New code is not a delivered build until those checks pass.

Dimension references: pinned CDDA data/json/region_settings/region_settings/dimensions,
data/json/effects_on_condition/nether_eocs/reverberations.json (grey sky and
misty grasslands), string_dimension.json (star field), and
scarlet_storm_effect_on_condition.json (scarlet sky). Unknown mod dimensions
retain their actual weather tint instead of guessing from translated names.

Implementation checkpoints 1–6 are committed. Pinned 1040/1807 Host syntax,
404 presentation checks, 303 hybrid checks, the 496657-check legacy renderer
suite, 34 module lifecycle checks and five atomic-transform cases pass locally.
Release gates: real-SDL cases, Windows packaging and captured-image review.
The qualification workflow and each archive's release-manifest.json identify
the exact tested source revision. Do not ship before those gates succeed.

Additional performance checkpoint: prepare gas/light coefficients once per cell,
clip ray intervals, bound the scattering layer and split volume/compositor rows
between up to four workers. Random-alpha and odd-size outputs match the Host
reference compositor exactly. In the local 1280x800 cached-world smoke fixture,
composition dropped from 219 to 39 ms on CPU and GPU-layer preparation from
155 to 37 ms. These are synthetic scene timings, not player FPS.
The optimization and resin-seam repair change only the module; all 26 pinned
Host input hashes remain identical for each game build, allowing exact cached
Hosts to be reused for the final installer qualification.

Final accessory audit covers all nine supported attachment types, simultaneous
light/laser mounts and canonical revolver IDs, including enchanted variants.

## 0.7.1 continuation

The floor-sample SDL fixture now drives the DLL camera through real actions,
asserts its downward angle and restores it, instead of changing only the Host echo.
Raster preparation clips/projects each triangle once. Interleaved 16-row strips
spread lower-screen floor and view-model work across the workers, preserving
submission order and disjoint writes. Thirty reference frames at 640x480 and
1280x800 match the previous implementation byte for byte across color, world/held
depth, creature picking and ownership, including glass, doors, near clipping,
water and firearms. Initial isolated camera benchmarks at 1280x800 improve clear
floor from 50 to 29 ms, walls from 45 to 34 ms and water from 52 to 31 ms; timing
varies with host load and these numbers do not represent in-game player FPS.

Water now shades a shallow bed along actual known land, adds narrow sea wash,
and blends subpixel puddle edges. Cardinal/diagonal bank distances meet at tile
boundaries and unknown/no-floor cells do not invent beaches. Presentation tests
now contain 461 checks, including rendered shoreline pixels and edge continuity.
`FIRST_PERSON_BUILD_BENCHMARK` builds a manual reproducible timing tool without
adding a machine-dependent performance gate. Version advanced to 0.7.1-dev;
Windows installer qualification is pending for this new revision.
