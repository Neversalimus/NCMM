# FPS action latency, recovery, liquids and projectiles

Development TEST changes for PR #223. No local Native game or input harness is
launched. Background games remain running; this is not a 15 ms frame-time claim.

The newer pinned Native SDL loop could wait for its complete 125 ms input
timeout after a presentation redraw. Held continuous movement therefore did not
receive another solver/render update until another event or that timeout.
The same existing wait now wakes at the next presentation deadline, at most
16 ms during walking, attacks and projectile flight. It does not manufacture a
Native timeout or spend an extra turn. The older pin retains its existing
frequent polling implementation.

Native projectile animation previously redrew and slept along the flight path.
FPS bypasses this blocking 2D path and its bullet/hit delay; original 2D and
command-mode behavior stays intact. This removes artificial delays, not the
real cost of Native ballistics, AI, visibility, rendering or saving.

Focus recovery remembers whether FPS owned capture before Alt/GUI released it.
Only that prior capture resumes. Physical held keys remain fenced and releases
consumed by a modal are reconciled. A new actual DEFAULTMODE poll renews a lease
revoked by a nested modal. A recursive input loop still cannot inherit it.
The Native matrix adds walking after Alt+Tab and after a modal teleport, across
both physical keyboard modes and CPU/GPU rendering: 180 controlled cases.

Puddles use an occupancy contour shared across compatible visible liquid cells.
The grid interior reduces to one quad, while outside and concave corners have
rounded banks. Unknown/hidden cells and solid walls cannot extend the puddle.
The CPU and GPU use the same mesh, replacing per-cell oval fragment cutouts.

The new optional projectile interface owns at most 32 paths of 64 coordinates.
Paths are copied only from confirmed avatar shots, bounded by the actual
traversed endpoint and visible cells. No Native pointer survives the callback.
Code-native bullet, pellet, arrow, bolt, laser, plasma, flame and thrown assets
share world depth; they do not draw through walls or alter hit/damage results.
Visual flight is deliberately slowed for readability. A captured muzzle stays
anchored while the avatar changes tiles. Empty/misfired shots create no path.

Validation includes exhaustive puddle shared-edge combinations, all 256 masks,
projectile lifetime and rebasing, renewed/revoked leases, held-key recovery and
presentation wait bounds. Render-only CPU and actual GPU tests cover connected
puddles, every projectile family and wall occlusion. Actual Native CI and fresh
installer receipts must bind this revision before a packaged result is claimed.
Manual Alt+Tab, a physical two-monitor session and ordinary full-frame timing
remain separate checks; the PR remains draft and the production feed unchanged.
