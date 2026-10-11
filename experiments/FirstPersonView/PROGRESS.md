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
