# Require the current authored item after a Native equip change

Full run 39178210153, Native 1040, completed the 64-case CPU/GPU held gallery,
192-attempt gameplay audit and eight focused GPU runs. Its later appearance
process failed `wielded_item_sprite_stale` when switching from the rifle to
`knife_combat`.

That old assertion required a tileset sprite request. The new catalog correctly
draws its authored knife, which has no reason to request a held billboard.
The assertion now requires the exact current Native item ID, the selected
`knife` model, an actual match with no fallback and at least one submitted
authored draw. The earlier unsupported `naginata` assertion still requires its
tileset fallback. Both paths therefore retain a concrete rendering check.

The gallery and gameplay results do not qualify the failed enclosing run.
The updated appearance process must still pass on both actual Native pins.
