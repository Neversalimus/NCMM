# Current-target health

The existing perspective picker and native name-label layer draw one name and
health bar for the current visible living hostile. Neutral and friendly names
retain their previous label. The live world setting **Target health bar /
Здоровье цели** disables the added block. It does not change targeting or combat.

The Host reads `Creature::get_hp()` / `get_hp_max()` from the current native
creature on the main thread. Monsters use their native scalar HP; characters use
Creature's existing native aggregate. The bridge does not calculate a separate
limb total. Missing or invalid maximum HP produces a grey unknown bar and `[?]`,
never a full bar. Health is absent from static world geometry and mesh keys.

Every native Creature receives a non-serialized presentation lifetime ID. Copies
and moves receive new IDs; assignment preserves the destination's ID. IDs are
never derived from addresses or reused after destruction. The experimental Host
is rebuilt as a whole; existing public NCMM and graphics ABI layouts are unchanged.
The owned creature event records now use these IDs and retain neither Creature
pointers nor weak native references. Atlas caches use an SDL texture property
with a monotonic numeric lifetime ID, rather than keeping atlas addresses.

Selection requires matching current native ID, world generation, source cell and
absolute XYZ in the prepared creature snapshot. Native perception, physical body
visibility and map line-of-sight are checked again. The selected reference also
contains a Host frame serial; clicks and confirmed aim revalidate the same
reference. The GPU module accepts a pick only when metrics are active and their
producer frame serial equals the submitted scene. The existing GPU pick is
synchronous geometry picking; asynchronous pixel-count completion is not used
as target identity. World changes, death, lost aim, replacement and 2D do not
inherit another entity's name or HP.

The block uses CDDA's configured font, Unicode fallback and existing glyph batch.
It limits the name to 28 display columns, truncates at Unicode boundaries, and
preserves the unknown marker. Font metrics size the block at different DPI; name
and bar occupy separate rows. The complete block is clamped inside the viewport
above the projected head. Tiny viewports hide it safely. Native SDL draw colour,
blend and clip state are restored.

`target_hud_test` checks object-address reuse, copy/move/assignment identities,
concurrent native construction, stale frame/world/XYZ, HP and viewport/DPI bounds.
The actual shipping-module GPU test rejects stale/future/inactive metrics.
The opt-in native gameplay audit captures full/damaged HP, long Cyrillic names,
disabled HUD, replacement in the same cell, death, wall/glass, friendly names,
lost aim, night, native NPC summary/damage and explicit F6 off, on CPU and GPU. Its separate verifier rejects
missing or corrupt captures, hidden fallback, wrong HP and reused target IDs.

Native Windows/Linux, both pinned builds, actual pixels, performance and final
lifecycle qualification remain required. Unit tests and capture metadata alone
are not a claim that these native checks passed or that a manual session ran.
