# Matched Native gallery on the unchanged 0.13 production renderer

This diagnostic branch adds an isolated Native fixture for the same 26 body
families, four installed accessories and empty/loaded pump states as the new
authored gallery. Both CPU and GPU captures use a 1280x800 canvas, the same Native
world clearing/camera angle and a declared 280 ms settling interval. Actual
item IDs, ammo, backend, device and capture clocks are recorded.

The module, shaders, geometry, hand animation, public SDK and production Host
bridge behavior remain the `8d24cc2` baseline. This fixture never loads its old
module into the new Host's private interfaces. Each Native Host and module are
compiled together. Captures are diagnostic evidence and cannot qualify a new
installer, prove performance or replace the separate baseline matrix at
`2bb3ec5`.

The additional source files only prepare and verify the Before gallery. This
branch has no pull request and must not be merged into the implementation PRs.
Actual Native captures and visual comparison remain pending.
