# Draw-local definition-cache collision candidate

The independent Main/PSX comparison completed four ordinary RTX 4090 / D3D12
runs, actual VSync 1, with the corrected complete-frame clock. All 2,816 first
frames and 8,448 camera observations passed structural, binary, SDK and quality
checks. PSX f22a8e7 was **not accepted**: thirteen repeated >5% AND >0.1 ms
regressions remain (eight first-render tails, four action-to-ready tails and one
dense-city cold maximum), despite forty-two improvements. Source/result identity:

- Main diagnostic harness: 8c5c32fbd58e707e058343b9fb54e246f4e90d4d, production renderer unchanged.
- PSX: f22a8e7c076f2db6941a84d9fd40080dc8358eb7.
- Frozen plan SHA256: 72accf1b06baca104123b92a5b9b6ed8fd2a2f0087fcfbfe4a689ee42dd5099f.
- Result SHA256: a282df9468e99f0b029759394fbeb784f87f0c0d01f1e2bfa2f07b9531354153.

The forward PSX visibility wall increased while the reverse repeat returned
near Main. The Native world-extraction implementation is unchanged between
Main and original PSX; ray/source/geometry counts also match. Existing material
counters isolate one concrete source of variation. For the same 1,776,970
lookups in the 64 first frames of the armed scene:

| Run | Definition evaluations | Cache hits |
|---|---:|---:|
| Main forward | 896 | 1,776,074 |
| PSX forward | 599,888 | 1,177,082 |
| PSX reverse | 928 | 1,776,042 |
| Main reverse | 1,072 | 1,775,898 |

Street and smoke show the same 599,888 forward PSX evaluations versus roughly
900–1,100 in the other processes, with matching total lookup counts. The cache
hash uses borrowed definition addresses. A single direct-mapped slot permits
different allocation/ASLR placements to make common definitions continuously
evict each other. This explains the extra definition recomputation; it does
not establish that all thirteen timing regressions have this sole cause.

Keep both definition caches local to the same main-thread extraction, with the
same 256 entries, keys and computed values. Search at most eight neighbouring
slots before bounded replacement, allowing small collision clusters to coexist.
No allocation, pointer dereference/lifetime extension, cross-frame retention,
Native visibility approximation, quality change or public ABI change is added.
Fields, vehicles, items and actors remain outside the immutable-definition cache.
The light-range cache and all existing invalidation rules are unchanged.

The new adversarial test alternates eight fixed identities that share the old
single-slot hash. It reproduces repeated recomputation on the unchanged cache;
the corrected cache evaluates eight times over 40,000 requests. A ninth alias,
large overflowing key streams, distinct floor policies and a fresh draw still
return the exact requested results with bounded storage/work. The local MSVC
19.38 cache test passes 736,510 checks; shipping CI uses 19.44. This is functional
evidence, not Native performance acceptance. A fresh balanced Main/PSX comparison,
then the combined matrix and final qualification, remain required. No 15 ms or
merge acceptance is claimed.
