# Boundary continuity and reticle flight

Development TEST continuation for PR #223. The earlier input-wait fix did not
remove all pauses at a Native tile boundary. This change addresses additional
preparation work and the first camera frame after an acknowledged move.

Pure Native cache-flag resets still invalidate visibility, lighting and the
blocked-thread snapshot lease. They no longer discard unchanged derived
geometry and roof columns. Actual regional terrain, furniture and field
notifications invalidate the affected 3x3 neighbourhood on adjacent levels and
the affected roof columns. Broad/unknown mutations retain full invalidation.
Both old/new source-window borders are invalidated after shifts, including
large same-world moves, so newly available neighbours cannot retain old flags.

An acknowledged crossing retains its bounded integration clock through the
next real Native input scope. Its pre-poll redraw can sample held walking state
under that explicit safe lease. It cannot dispatch another action, accrue
unbounded movement debt, read a mutating simulation or inherit a nested modal's
ownership. The controlled Native matrix records first safe post-boundary
presentations and their maximum observed dispatch-to-present gap. It rejects a
stationary first safe frame while walking remains held. This is a functional
continuity check, not a hardware frame-time qualification.

Interaction focus includes nearby floors, furniture and the avatar tile; it
must not select the ballistic destination. A visible creature still supplies
an explicit target. Otherwise a loaded-only physical reticle ray selects the
first wall/floor or the furthest loaded point, including pitch and fractional
avatar position. A deliberately wrong floor focus is exercised in Native CI.

The additive projectile v2 interface owns the firing ray alongside the original
confirmed path. Original v1 clients/layouts remain supported. Presentation uses
one continuous segment from the actual posed muzzle to the confirmed endpoint,
instead of joining the muzzle to the first grid centre behind a long barrel.
Free horizontal shots retain eye height; turning/walking cannot drag a stored
muzzle or impact. Point-blank impacts behind the visible barrel never generate
a backwards flight. Native damage, dispersion and collision remain in charge.

The muzzle is computed directly in the small local render window. A regression
was first reproduced in the actual shared module by translating the complete
scene by twenty million world units: adding/subtracting absolute float origins
rounded away barrel offsets. The corrected scene has identical projectile
pixels after translation. Free-fire ray samples also reuse unchanged voxel
facts, avoiding eight repeated Native queries per travelled metre.
The first visible streak also retains its muzzle end during the clock's 10 ms
presentation lead; otherwise a fast bullet could first appear a metre ahead of
the barrel despite a correct stored origin. Every projectile family is tested
at short and long distances for this initial contact.

Ground transitions have a wider, less uniform shared feather. A world-random
texture variant was rejected by the actual GPU draw-budget check: it produced
73 grass-field draws instead of 45. The shared profile preserves batching and
the original two opaque triangles, without an extra transparent pass.

Local checks use pure/model tests and the inventoried hidden render-only GPU
suite. No Native game/input harness, mouse capture/warp, UAC profiler or global
driver/power changes are permitted. Background games remain running. Fresh
Native CI and exact TEST archive identities are required before packaging is
claimed; ordinary gameplay's 15 ms target and manual physical play remain
unqualified.
