# Verified retained pixels in the actual Native input loop

Source `29f12f84a0723822893b8ce328186222fd19872a`, CI 39177009280.
Both pinned engines passed the real `game::handle_action` / SDL event route.
The main thread did not use the benchmark fixture's artificial input lease.

| Pin | Mouse events | Rotation radians | Presents / GPU | CPU / nonperspective | Snapshot generations | Snapshot hits |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1040 | 466 | 13.980001 | 54 / 54 | 0 / 0 | 1 | 3,236,119 |
| 1807 | 471 | 14.130001 | 54 / 54 | 0 / 0 | 1 | 3,209,637 |

Native position, turn, moves, HP and attack serial were unchanged. Snapshot
storage was 74,190,676 bytes. No lease invalidation, cache clear, CPU fallback or
pixel-read failure occurred. Every present was checked. In each process 53
presents had a fresh GPU composition and one UI-only present retained its exact
previous perspective viewport. The separately downloaded actual/reference BMPs
were compared again: equal dimensions and identical complete RGBA bytes.

- 1040 artifact 12154997748: `d17b1be86020270fe212486ec29ca2e46da68dd9e25389ca4807563e2edcf5a3`
- 1807 artifact 12155676593: `b8170b683d28d4a761dc1dfb4adbba3c8f054b1612bdaadf8682fc0fd40ca958`

Whole artifact SHA-256 values were verified before inspection. Microsoft Basic
Render Driver / D3D12 is functional software-adapter evidence, not ordinary RTX
performance. The opt-in pixel readbacks are excluded from ordinary timings.
