# Extended actual Native target gallery

Source `a389fead4e87129e3c9ab890d37602baabec9795`, CI 39173579369.
Both pins reached `target_hud_gallery_complete`: 21 scenarios in each of CPU
and D3D12 modes, including actual blindness, restored sight, hidden lower floor,
huge close creature, flying eye, wide viewport, Native NPC health and Cyrillic.
The corrected flying fixture aims at the opaque iris; production picking and
Native visibility checks remain unchanged.

The complete 1040 gallery artifact 12154585903 was downloaded and its archive
SHA-256 verified:
`c294eb9aebfbb8f643be956e9a05ba4d827e8722ddcf7478fc10b575dcd5e168`.
The strict target verifier passed all 42 captures. The adapter was Microsoft
Basic Render Driver, explicitly software; this is not RTX performance evidence.

The enclosing isolated-gallery process failed afterward because the common
smoke tail required F12 event-drain counters produced only by the firing audit.
The fix applies that requirement to the full gameplay audit while excluding
the explicitly separate HUD-only gallery. Full gameplay still requires F6/F12
regressions and actual firing. The fixed enclosing process needs a new run.
