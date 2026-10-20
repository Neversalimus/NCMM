# Actual Native input lease and retained viewport

Source `46e51b019650e6330cb74d31b91af760fb19b64e`, CI run
39172831134, both pinned Native Windows engines. The adapter was Microsoft Basic
Render Driver / D3D12: functional evidence only, not hardware performance.

| Native pin | SDL mouse events | Rotation radians | Snapshot generations | Hits | Fresh GPU presents | UI-only presents |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1040 | 469 | 14.070001 | 1 | 5,255,722 | 84 | 1 |
| 1807 | 476 | 14.280001 | 1 | 5,162,962 | 83 | 1 |

Native position, turn, moves, torso HP and attack serial remained unchanged.
No input lease invalidation occurred during either actual Native input wait.
The preceding candidate changed its lease 41/42 times on 125 ms TIMEOUT polls.

The strict probe failed because a UI-only present had no new terrain submission.
Both saved actual window captures were inspected: the 3D door, wall, floor and
PSX arms remained present. Artifact digests:

- 1040, 12153259143: `880b04f4070286482a6e0c695856263d043c4e0de5704e6b71e4e35de03a6ca9`
- 1807, 12153842199: `2fc51ef8d59d8cf986aaaab887dd2973a9b05b99bd470f677c1962d499e05f84`

The next diagnostic reads the actual viewport before every present. A UI-only
present counts as retained GPU composition only when every RGBA pixel and the
viewport bounds match the last real GPU composition. Read failures, CPU output,
changed/unclassified pixels or missing references fail the run. Actual and
reference BMP pairs are retained for the first eight UI-only presents. This
readback is confined to the explicit camera input diagnostic and is excluded
from ordinary timing.

An additional full-size hardware-only matrix has eleven scenes, including a
held tool: 704 actual post-move frames and 2112 cold/warm camera samples per run.
It retains radius 48 and loaded Z, and rejects profiler markers, GPU debug,
software devices and cache differential verification. No ordinary performance
acceptance is established by this report.
