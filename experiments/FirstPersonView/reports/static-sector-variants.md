# Bounded geometry variants for revisited sparse sectors

The unchanged `bbb8575` Native 1807 / RTX4090 full-size diagnostic produced no
warm static GPU uploads, but its 96 warm camera samples still regenerated
20,533 static cells in street, 44,231 in city, 61,533 in dense city and 32,426 in
smoke/rain. Each frame's sparse sector changes its 27-neighbour content signature
at the sector boundary. Keeping only the last signature makes repeated turns
rebuild previously observed geometry despite identical native contents.

The existing per-cell geometry cache now retains four weak alternatives, keyed
by the exact original content signature. No signature input is removed. The
existing resource cache owns geometry and may evict old alternatives under its
unchanged budgets; expired weak references force the normal build. Empty passes
are represented separately from evicted meshes. Source arrays, Native pointers,
GPU objects and a second world representation are not retained by this change.
Resetting the world/device clears all variants as before. Variant bookkeeping is
included in the existing producer memory measurement.

The regression test alternates full and clipped sparse sectors through several
camera angles, compares every static cell/pass mesh identity with an independent
fresh collector, then changes a real floor permission and resets the world.
The previous implementation fails on the first revisited sector. The candidate
passes without regenerating static cells for the warmed repeated sectors and
still rebuilds the changed topology. This establishes correctness for the
fixture; actual Native timing, memory, full-turn images and regression thresholds
remain required before accepting the optimization.
