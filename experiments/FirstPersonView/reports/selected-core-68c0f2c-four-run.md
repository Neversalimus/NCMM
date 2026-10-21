# Native tile-read candidate: complete ordinary measurements

Original four-run gate: **PASS**.
Full CI and unchanged-binary visual/lifecycle qualification remain separate required gates.

# Four ordinary Native runs — new standalone PSX candidate

Frozen verdict: **accepted for this comparison**. 0 repeated regressions and 164 repeated improvements. A difference must exceed both 5% and 0.1 ms in both directions. These count metrics, not independent incidents.

All 11 full-size scenes and all four groups remain present: 2816 first-render and 8448 camera observations. Each timing cell is **median / p95 / p99 / maximum in ms**, with 64 first/action and 96 warm/cold observations per scene/process. Small-sample p99/max does not qualify rare stalls.

Same verified SDK, UDP inventory, actual VSync 1, viewport/internal resolution, R48, Z and quality. Camera snapshots, imported held assets and target HUD are disabled. PSX arms remain part of their actual candidate. No profiler, debugger, CPU-cycle observer or GPU debug. Native tile pass is retained. Whole-frame clocks include post-present metrics/readback service. CPU wall is not GPU execution.

This compares the new exact source against unchanged Main rendering with the inclusive diagnostic clock. The original D6 rejection remains preserved. Final combined comparison, unchanged-binary hardware validation and merge remain separate. No manual playthrough. The established daylight fixture uses DEBUG_NIGHTVISION and controlled actions omit ordinary AI turns; no normal-game FPS claim.

## first_render

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 18.048 / 25.067 / 25.968 / 27.037 | 17.770 / 24.969 / 28.894 / 34.096 |
| armed | psx | 13.202 / 16.622 / 19.124 / 21.968 | 11.352 / 13.907 / 15.182 / 16.042 |
| city_blocks | main | 20.689 / 26.076 / 28.665 / 29.503 | 19.405 / 25.611 / 26.808 / 27.315 |
| city_blocks | psx | 14.539 / 18.067 / 21.696 / 25.052 | 12.793 / 17.864 / 18.911 / 19.611 |
| creatures_100 | main | 18.486 / 25.450 / 26.848 / 27.559 | 20.758 / 27.071 / 27.799 / 28.053 |
| creatures_100 | psx | 14.047 / 17.219 / 21.318 / 27.619 | 12.410 / 15.398 / 19.594 / 22.575 |
| creatures_20 | main | 19.167 / 25.671 / 26.324 / 26.444 | 19.555 / 26.261 / 27.677 / 28.145 |
| creatures_20 | psx | 13.032 / 16.539 / 19.192 / 19.349 | 12.026 / 15.773 / 16.858 / 17.473 |
| dense_city | main | 23.697 / 32.378 / 35.332 / 36.047 | 22.654 / 31.455 / 34.122 / 37.245 |
| dense_city | psx | 17.408 / 22.157 / 22.736 / 22.781 | 15.143 / 19.244 / 20.462 / 20.676 |
| empty_attacks | main | 17.718 / 24.986 / 26.250 / 27.396 | 16.199 / 21.304 / 24.842 / 26.307 |
| empty_attacks | psx | 12.752 / 15.554 / 17.034 / 17.958 | 10.788 / 13.770 / 15.216 / 15.905 |
| forest | main | 16.222 / 20.972 / 23.837 / 26.292 | 16.083 / 21.550 / 23.672 / 25.745 |
| forest | psx | 11.540 / 14.133 / 15.256 / 16.438 | 10.321 / 11.338 / 12.522 / 12.536 |
| multi_level_horde | main | 19.257 / 25.146 / 27.252 / 29.239 | 18.059 / 25.874 / 27.839 / 29.600 |
| multi_level_horde | psx | 13.674 / 16.688 / 17.662 / 17.679 | 11.819 / 14.318 / 15.007 / 15.083 |
| smoke_rain | main | 17.665 / 22.930 / 23.584 / 23.820 | 18.352 / 23.860 / 25.562 / 26.156 |
| smoke_rain | psx | 11.192 / 13.681 / 14.226 / 14.476 | 10.402 / 12.628 / 14.243 / 15.143 |
| street | main | 18.345 / 24.727 / 41.289 / 67.964 | 19.877 / 28.139 / 41.439 / 63.519 |
| street | psx | 13.760 / 17.583 / 26.923 / 36.926 | 13.275 / 16.495 / 26.436 / 39.040 |
| tool | main | 19.127 / 25.810 / 27.008 / 27.323 | 18.686 / 25.182 / 27.887 / 29.296 |
| tool | psx | 13.181 / 16.395 / 17.204 / 17.268 | 11.867 / 14.464 / 16.890 / 18.771 |

## native_action_to_ready

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 38.958 / 57.087 / 59.042 / 60.190 | 37.285 / 55.048 / 67.360 / 82.353 |
| armed | psx | 34.823 / 46.480 / 51.119 / 57.765 | 32.431 / 40.274 / 43.366 / 46.114 |
| city_blocks | main | 42.971 / 59.014 / 61.447 / 62.921 | 42.637 / 57.739 / 59.742 / 60.977 |
| city_blocks | psx | 38.529 / 51.059 / 59.766 / 66.346 | 34.720 / 48.386 / 51.768 / 53.533 |
| creatures_100 | main | 39.809 / 57.688 / 61.040 / 61.570 | 42.286 / 59.841 / 61.953 / 62.512 |
| creatures_100 | psx | 35.197 / 48.498 / 57.790 / 62.457 | 32.535 / 46.671 / 49.928 / 51.934 |
| creatures_20 | main | 40.070 / 55.105 / 56.792 / 57.775 | 39.565 / 56.989 / 63.000 / 64.333 |
| creatures_20 | psx | 35.568 / 49.231 / 50.962 / 52.683 | 33.764 / 43.196 / 46.215 / 48.179 |
| dense_city | main | 54.948 / 67.700 / 73.767 / 74.161 | 51.299 / 67.469 / 75.013 / 80.485 |
| dense_city | psx | 46.108 / 58.093 / 63.753 / 67.081 | 39.790 / 52.476 / 54.809 / 55.580 |
| empty_attacks | main | 38.781 / 56.305 / 58.796 / 60.488 | 36.208 / 50.489 / 56.169 / 60.503 |
| empty_attacks | psx | 33.872 / 47.799 / 50.345 / 53.616 | 31.844 / 40.248 / 42.391 / 42.895 |
| forest | main | 35.981 / 52.360 / 57.246 / 59.465 | 36.342 / 51.270 / 61.152 / 66.214 |
| forest | psx | 33.470 / 43.358 / 47.388 / 49.329 | 28.360 / 38.079 / 40.121 / 40.208 |
| multi_level_horde | main | 39.984 / 57.511 / 58.659 / 59.124 | 39.505 / 58.256 / 58.621 / 58.640 |
| multi_level_horde | psx | 36.557 / 49.424 / 50.331 / 50.771 | 31.868 / 42.184 / 43.461 / 43.738 |
| smoke_rain | main | 38.791 / 54.738 / 65.161 / 81.061 | 39.465 / 55.718 / 56.676 / 57.602 |
| smoke_rain | psx | 32.523 / 44.236 / 45.859 / 46.896 | 30.996 / 42.389 / 44.200 / 46.629 |
| street | main | 38.525 / 53.805 / 77.636 / 116.270 | 39.702 / 58.873 / 78.828 / 104.663 |
| street | psx | 35.694 / 46.331 / 62.460 / 81.353 | 33.137 / 46.770 / 69.527 / 90.689 |
| tool | main | 42.915 / 56.853 / 58.703 / 59.973 | 41.754 / 58.509 / 69.034 / 73.550 |
| tool | psx | 37.521 / 49.071 / 52.247 / 52.613 | 34.377 / 42.177 / 43.435 / 43.730 |

## warm_rotation

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 8.767 / 11.409 / 12.181 / 12.506 | 9.259 / 11.460 / 11.943 / 12.266 |
| armed | psx | 7.606 / 9.311 / 9.946 / 11.010 | 6.951 / 9.182 / 9.520 / 11.131 |
| city_blocks | main | 13.225 / 16.704 / 17.658 / 17.670 | 13.655 / 16.623 / 17.045 / 17.274 |
| city_blocks | psx | 10.569 / 13.727 / 14.648 / 15.103 | 9.130 / 10.676 / 11.152 / 11.579 |
| creatures_100 | main | 9.327 / 11.464 / 12.827 / 12.918 | 9.958 / 12.264 / 13.472 / 13.513 |
| creatures_100 | psx | 7.616 / 9.850 / 10.282 / 10.726 | 7.184 / 9.082 / 9.252 / 9.702 |
| creatures_20 | main | 8.991 / 10.930 / 12.211 / 12.342 | 9.875 / 12.767 / 13.648 / 14.510 |
| creatures_20 | psx | 7.419 / 9.342 / 10.895 / 11.371 | 6.714 / 7.986 / 9.197 / 9.713 |
| dense_city | main | 17.287 / 20.872 / 21.824 / 23.916 | 17.114 / 22.482 / 23.813 / 24.492 |
| dense_city | psx | 15.149 / 19.688 / 21.026 / 23.922 | 12.383 / 14.312 / 15.026 / 15.432 |
| empty_attacks | main | 9.197 / 11.779 / 12.872 / 13.014 | 8.416 / 10.138 / 10.944 / 11.409 |
| empty_attacks | psx | 6.838 / 9.469 / 10.034 / 10.043 | 6.467 / 8.389 / 9.809 / 10.825 |
| forest | main | 7.771 / 9.856 / 10.505 / 10.606 | 8.115 / 9.676 / 10.407 / 10.681 |
| forest | psx | 6.957 / 8.154 / 8.802 / 9.207 | 6.957 / 7.308 / 7.973 / 8.684 |
| multi_level_horde | main | 9.284 / 11.496 / 12.238 / 12.273 | 9.483 / 13.220 / 17.820 / 20.411 |
| multi_level_horde | psx | 7.745 / 10.650 / 11.230 / 11.532 | 7.364 / 8.569 / 8.967 / 9.250 |
| smoke_rain | main | 9.709 / 12.982 / 13.622 / 13.935 | 9.911 / 11.473 / 12.174 / 12.240 |
| smoke_rain | psx | 7.267 / 10.164 / 10.515 / 10.596 | 7.679 / 10.013 / 10.408 / 11.084 |
| street | main | 8.526 / 10.881 / 12.247 / 12.449 | 8.871 / 11.525 / 11.923 / 11.971 |
| street | psx | 6.958 / 9.609 / 10.162 / 10.541 | 7.268 / 9.399 / 9.644 / 9.810 |
| tool | main | 8.471 / 10.608 / 11.236 / 11.258 | 8.702 / 11.397 / 11.754 / 12.005 |
| tool | psx | 6.969 / 9.046 / 9.694 / 10.661 | 6.984 / 8.688 / 9.027 / 9.186 |

## cold_rotation

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 8.885 / 10.960 / 12.128 / 12.425 | 9.403 / 11.921 / 12.233 / 12.829 |
| armed | psx | 7.582 / 9.045 / 10.503 / 10.832 | 6.764 / 7.575 / 8.217 / 8.812 |
| city_blocks | main | 14.216 / 17.617 / 19.210 / 20.517 | 13.840 / 16.881 / 18.430 / 19.055 |
| city_blocks | psx | 11.380 / 14.648 / 19.825 / 19.873 | 10.353 / 12.433 / 13.819 / 14.555 |
| creatures_100 | main | 9.675 / 12.781 / 13.175 / 14.269 | 10.097 / 12.883 / 13.926 / 13.953 |
| creatures_100 | psx | 7.703 / 9.838 / 11.081 / 11.947 | 7.234 / 9.158 / 9.699 / 10.054 |
| creatures_20 | main | 9.132 / 11.548 / 12.574 / 12.597 | 9.940 / 12.823 / 13.234 / 13.345 |
| creatures_20 | psx | 7.711 / 10.202 / 10.457 / 10.480 | 6.978 / 8.909 / 9.614 / 9.678 |
| dense_city | main | 18.078 / 21.591 / 22.595 / 23.215 | 17.624 / 21.873 / 23.872 / 25.047 |
| dense_city | psx | 15.220 / 18.758 / 20.649 / 20.905 | 13.489 / 16.577 / 18.516 / 18.806 |
| empty_attacks | main | 8.941 / 10.743 / 11.369 / 12.159 | 8.520 / 9.743 / 10.052 / 10.134 |
| empty_attacks | psx | 6.951 / 9.020 / 10.198 / 11.463 | 6.914 / 8.751 / 9.703 / 10.655 |
| forest | main | 7.897 / 10.258 / 10.592 / 10.607 | 8.198 / 9.871 / 10.709 / 11.742 |
| forest | psx | 6.525 / 8.815 / 9.713 / 11.296 | 6.889 / 7.549 / 10.092 / 10.326 |
| multi_level_horde | main | 9.418 / 12.331 / 13.068 / 13.549 | 9.765 / 12.090 / 13.157 / 13.444 |
| multi_level_horde | psx | 7.931 / 10.754 / 11.810 / 11.849 | 7.551 / 9.645 / 10.183 / 10.324 |
| smoke_rain | main | 9.487 / 12.186 / 12.819 / 13.123 | 9.904 / 12.820 / 13.547 / 13.793 |
| smoke_rain | psx | 7.707 / 10.198 / 10.775 / 11.206 | 7.029 / 8.405 / 9.398 / 9.742 |
| street | main | 9.080 / 12.061 / 14.805 / 16.811 | 9.147 / 12.038 / 13.405 / 16.317 |
| street | psx | 7.117 / 9.324 / 9.951 / 11.635 | 7.926 / 10.189 / 11.672 / 12.354 |
| tool | main | 8.472 / 11.000 / 11.664 / 11.923 | 8.403 / 11.356 / 11.786 / 11.964 |
| tool | psx | 7.080 / 9.459 / 10.559 / 10.592 | 7.070 / 8.557 / 8.844 / 9.027 |

## First rendered frames at or below 15 ms

This is a count of the recorded first-frame samples, not an acceptance override.

| Scene | Binary | Forward | Reverse |
|---|---|---:|---:|
| armed | main | 26/64 | 23/64 |
| armed | psx | 52/64 | 63/64 |
| city_blocks | main | 5/64 | 10/64 |
| city_blocks | psx | 38/64 | 55/64 |
| creatures_100 | main | 25/64 | 8/64 |
| creatures_100 | psx | 45/64 | 59/64 |
| creatures_20 | main | 19/64 | 19/64 |
| creatures_20 | psx | 49/64 | 58/64 |
| dense_city | main | 0/64 | 0/64 |
| dense_city | psx | 1/64 | 31/64 |
| empty_attacks | main | 29/64 | 31/64 |
| empty_attacks | psx | 53/64 | 63/64 |
| forest | main | 30/64 | 26/64 |
| forest | psx | 63/64 | 64/64 |
| multi_level_horde | main | 21/64 | 19/64 |
| multi_level_horde | psx | 49/64 | 63/64 |
| smoke_rain | main | 29/64 | 28/64 |
| smoke_rain | psx | 64/64 | 63/64 |
| street | main | 19/64 | 15/64 |
| street | psx | 49/64 | 50/64 |
| tool | main | 25/64 | 28/64 |
| tool | psx | 46/64 | 61/64 |

## Resources

Forward / reverse observed values. Finite peaks do not establish absence of leaks.

| Scene | Binary | Initial render ms | Warm upload KiB | Warm static KiB | Warm draws median | GPU peak MiB | RSS peak MiB |
|---|---|---|---|---|---|---|---|
| armed | main | 29.249 / 28.215 | 69532.047 / 69532.047 | unavailable / unavailable | 35.000 / 35.000 | 55.728 / 55.728 | 1733.770 / 1727.914 |
| armed | psx | 25.470 / 22.381 | 50072.172 / 50072.172 | 0.000 / 0.000 | 40.000 / 40.000 | 54.493 / 54.493 | 1742.273 / 1745.867 |
| city_blocks | main | 79.150 / 69.095 | 91597.156 / 91597.156 | unavailable / unavailable | 291.000 / 291.000 | 47.503 / 47.503 | 1717.371 / 1709.844 |
| city_blocks | psx | 55.194 / 51.701 | 71873.281 / 71873.281 | 0.000 / 0.000 | 296.000 / 296.000 | 48.219 / 48.219 | 1727.215 / 1729.117 |
| creatures_100 | main | 27.645 / 32.197 | 62359.938 / 62359.938 | unavailable / unavailable | 33.000 / 33.000 | 53.977 / 53.977 | 1727.836 / 1719.301 |
| creatures_100 | psx | 26.092 / 20.075 | 42636.062 / 42636.062 | 0.000 / 0.000 | 38.000 / 38.000 | 54.402 / 54.402 | 1741.734 / 1745.832 |
| creatures_20 | main | 29.609 / 30.790 | 61555.672 / 61555.672 | unavailable / unavailable | 32.000 / 32.000 | 53.974 / 53.974 | 1728.543 / 1719.949 |
| creatures_20 | psx | 23.670 / 21.267 | 41831.797 / 41831.797 | 0.000 / 0.000 | 37.000 / 37.000 | 54.402 / 54.402 | 1740.223 / 1744.109 |
| dense_city | main | 60.078 / 65.077 | 129078.453 / 129078.453 | unavailable / unavailable | 446.000 / 446.000 | 53.784 / 53.784 | 1746.887 / 1737.660 |
| dense_city | psx | 50.981 / 50.711 | 109354.578 / 109354.578 | 0.000 / 0.000 | 451.000 / 451.000 | 54.223 / 54.223 | 1753.543 / 1755.102 |
| empty_attacks | main | 25.797 / 24.911 | 61409.547 / 61409.547 | unavailable / unavailable | 32.000 / 32.000 | 55.728 / 55.728 | 1732.762 / 1728.031 |
| empty_attacks | psx | 22.906 / 18.555 | 41685.672 / 41685.672 | 0.000 / 0.000 | 37.000 / 37.000 | 54.493 / 54.493 | 1742.527 / 1745.844 |
| forest | main | 30.110 / 26.847 | 61525.719 / 61525.719 | unavailable / unavailable | 41.000 / 41.000 | 53.962 / 53.962 | 1730.387 / 1721.301 |
| forest | psx | 25.392 / 20.916 | 41801.844 / 41801.844 | 0.000 / 0.000 | 46.000 / 46.000 | 54.393 / 54.393 | 1740.105 / 1745.695 |
| multi_level_horde | main | 30.964 / 30.894 | 64569.250 / 64569.250 | unavailable / unavailable | 59.500 / 59.500 | 53.972 / 53.972 | 1729.008 / 1721.285 |
| multi_level_horde | psx | 27.278 / 21.991 | 44845.375 / 44845.375 | 0.000 / 0.000 | 64.500 / 64.500 | 54.401 / 54.401 | 1741.449 / 1745.629 |
| smoke_rain | main | 31.684 / 41.645 | 68102.859 / 68102.859 | unavailable / unavailable | 27.000 / 27.000 | 55.640 / 55.640 | 1732.609 / 1725.703 |
| smoke_rain | psx | 22.273 / 26.241 | 48378.984 / 48378.984 | 0.000 / 0.000 | 32.000 / 32.000 | 54.402 / 54.402 | 1742.738 / 1747.227 |
| street | main | 206.479 / 211.685 | 61409.984 / 61409.984 | unavailable / unavailable | 32.000 / 32.000 | 40.695 / 40.695 | 1847.449 / 1843.184 |
| street | psx | 199.666 / 202.487 | 41686.109 / 41686.109 | 0.000 / 0.000 | 37.000 / 37.000 | 41.748 / 41.748 | 1893.875 / 1893.613 |
| tool | main | 23.898 / 25.653 | 55210.797 / 55210.797 | unavailable / unavailable | 26.000 / 26.000 | 55.645 / 55.645 | 1732.367 / 1725.750 |
| tool | psx | 20.750 / 21.640 | 45352.422 / 45352.422 | 0.000 / 0.000 | 28.000 / 28.000 | 54.462 / 54.462 | 1742.176 / 1746.059 |

## Every repeated regression

| Scene | Group | Metric | Forward delta ms / % | Reverse delta ms / % |
|---|---|---|---|---|

## Every repeated improvement

| Scene | Group | Metric | Forward delta ms / % | Reverse delta ms / % |
|---|---|---|---|---|
| armed | first_render | median_ms | -4.846 / -26.85 | -6.417 / -36.11 |
| armed | first_render | p95_ms | -8.445 / -33.69 | -11.062 / -44.30 |
| armed | first_render | p99_ms | -6.844 / -26.35 | -13.712 / -47.45 |
| armed | first_render | maximum_ms | -5.069 / -18.75 | -18.054 / -52.95 |
| armed | native_action_to_ready | median_ms | -4.135 / -10.61 | -4.854 / -13.02 |
| armed | native_action_to_ready | p95_ms | -10.607 / -18.58 | -14.774 / -26.84 |
| armed | native_action_to_ready | p99_ms | -7.923 / -13.42 | -23.994 / -35.62 |
| armed | warm_rotation | median_ms | -1.161 / -13.24 | -2.308 / -24.93 |
| armed | warm_rotation | p95_ms | -2.099 / -18.39 | -2.278 / -19.88 |
| armed | warm_rotation | p99_ms | -2.235 / -18.35 | -2.423 / -20.29 |
| armed | warm_rotation | maximum_ms | -1.496 / -11.96 | -1.135 / -9.25 |
| armed | cold_rotation | median_ms | -1.304 / -14.67 | -2.639 / -28.07 |
| armed | cold_rotation | p95_ms | -1.915 / -17.47 | -4.346 / -36.45 |
| armed | cold_rotation | p99_ms | -1.624 / -13.39 | -4.016 / -32.83 |
| armed | cold_rotation | maximum_ms | -1.593 / -12.82 | -4.017 / -31.31 |
| city_blocks | first_render | median_ms | -6.150 / -29.73 | -6.612 / -34.07 |
| city_blocks | first_render | p95_ms | -8.009 / -30.71 | -7.747 / -30.25 |
| city_blocks | first_render | p99_ms | -6.968 / -24.31 | -7.897 / -29.46 |
| city_blocks | first_render | maximum_ms | -4.451 / -15.09 | -7.703 / -28.20 |
| city_blocks | native_action_to_ready | median_ms | -4.442 / -10.34 | -7.916 / -18.57 |
| city_blocks | native_action_to_ready | p95_ms | -7.955 / -13.48 | -9.353 / -16.20 |
| city_blocks | warm_rotation | median_ms | -2.656 / -20.09 | -4.525 / -33.14 |
| city_blocks | warm_rotation | p95_ms | -2.977 / -17.82 | -5.948 / -35.78 |
| city_blocks | warm_rotation | p99_ms | -3.010 / -17.04 | -5.893 / -34.57 |
| city_blocks | warm_rotation | maximum_ms | -2.567 / -14.53 | -5.695 / -32.97 |
| city_blocks | cold_rotation | median_ms | -2.836 / -19.95 | -3.487 / -25.20 |
| city_blocks | cold_rotation | p95_ms | -2.969 / -16.86 | -4.449 / -26.35 |
| creatures_100 | first_render | median_ms | -4.439 / -24.01 | -8.348 / -40.22 |
| creatures_100 | first_render | p95_ms | -8.232 / -32.34 | -11.673 / -43.12 |
| creatures_100 | first_render | p99_ms | -5.530 / -20.60 | -8.204 / -29.51 |
| creatures_100 | native_action_to_ready | median_ms | -4.612 / -11.59 | -9.751 / -23.06 |
| creatures_100 | native_action_to_ready | p95_ms | -9.190 / -15.93 | -13.170 / -22.01 |
| creatures_100 | native_action_to_ready | p99_ms | -3.250 / -5.32 | -12.025 / -19.41 |
| creatures_100 | warm_rotation | median_ms | -1.711 / -18.34 | -2.774 / -27.86 |
| creatures_100 | warm_rotation | p95_ms | -1.615 / -14.08 | -3.182 / -25.94 |
| creatures_100 | warm_rotation | p99_ms | -2.544 / -19.84 | -4.220 / -31.32 |
| creatures_100 | warm_rotation | maximum_ms | -2.192 / -16.97 | -3.811 / -28.20 |
| creatures_100 | cold_rotation | median_ms | -1.972 / -20.38 | -2.863 / -28.35 |
| creatures_100 | cold_rotation | p95_ms | -2.943 / -23.02 | -3.726 / -28.92 |
| creatures_100 | cold_rotation | p99_ms | -2.095 / -15.90 | -4.228 / -30.36 |
| creatures_100 | cold_rotation | maximum_ms | -2.321 / -16.27 | -3.899 / -27.94 |
| creatures_20 | first_render | median_ms | -6.135 / -32.01 | -7.529 / -38.50 |
| creatures_20 | first_render | p95_ms | -9.132 / -35.57 | -10.488 / -39.94 |
| creatures_20 | first_render | p99_ms | -7.132 / -27.09 | -10.819 / -39.09 |
| creatures_20 | first_render | maximum_ms | -7.094 / -26.83 | -10.672 / -37.92 |
| creatures_20 | native_action_to_ready | median_ms | -4.501 / -11.23 | -5.801 / -14.66 |
| creatures_20 | native_action_to_ready | p95_ms | -5.874 / -10.66 | -13.793 / -24.20 |
| creatures_20 | native_action_to_ready | p99_ms | -5.830 / -10.26 | -16.785 / -26.64 |
| creatures_20 | native_action_to_ready | maximum_ms | -5.092 / -8.81 | -16.154 / -25.11 |
| creatures_20 | warm_rotation | median_ms | -1.571 / -17.47 | -3.162 / -32.01 |
| creatures_20 | warm_rotation | p95_ms | -1.588 / -14.53 | -4.781 / -37.45 |
| creatures_20 | warm_rotation | p99_ms | -1.316 / -10.78 | -4.450 / -32.61 |
| creatures_20 | warm_rotation | maximum_ms | -0.971 / -7.87 | -4.798 / -33.06 |
| creatures_20 | cold_rotation | median_ms | -1.421 / -15.56 | -2.962 / -29.80 |
| creatures_20 | cold_rotation | p95_ms | -1.346 / -11.66 | -3.914 / -30.52 |
| creatures_20 | cold_rotation | p99_ms | -2.117 / -16.84 | -3.620 / -27.35 |
| creatures_20 | cold_rotation | maximum_ms | -2.116 / -16.80 | -3.667 / -27.48 |
| dense_city | first_render | median_ms | -6.288 / -26.54 | -7.510 / -33.15 |
| dense_city | first_render | p95_ms | -10.221 / -31.57 | -12.211 / -38.82 |
| dense_city | first_render | p99_ms | -12.596 / -35.65 | -13.660 / -40.03 |
| dense_city | first_render | maximum_ms | -13.266 / -36.80 | -16.569 / -44.49 |
| dense_city | native_action_to_ready | median_ms | -8.840 / -16.09 | -11.509 / -22.44 |
| dense_city | native_action_to_ready | p95_ms | -9.607 / -14.19 | -14.993 / -22.22 |
| dense_city | native_action_to_ready | p99_ms | -10.014 / -13.58 | -20.204 / -26.93 |
| dense_city | native_action_to_ready | maximum_ms | -7.080 / -9.55 | -24.906 / -30.94 |
| dense_city | warm_rotation | median_ms | -2.139 / -12.37 | -4.731 / -27.65 |
| dense_city | warm_rotation | p95_ms | -1.184 / -5.67 | -8.170 / -36.34 |
| dense_city | cold_rotation | median_ms | -2.858 / -15.81 | -4.134 / -23.46 |
| dense_city | cold_rotation | p95_ms | -2.833 / -13.12 | -5.296 / -24.21 |
| dense_city | cold_rotation | p99_ms | -1.946 / -8.61 | -5.356 / -22.44 |
| dense_city | cold_rotation | maximum_ms | -2.310 / -9.95 | -6.240 / -24.91 |
| empty_attacks | first_render | median_ms | -4.966 / -28.03 | -5.411 / -33.40 |
| empty_attacks | first_render | p95_ms | -9.431 / -37.75 | -7.535 / -35.37 |
| empty_attacks | first_render | p99_ms | -9.216 / -35.11 | -9.626 / -38.75 |
| empty_attacks | first_render | maximum_ms | -9.438 / -34.45 | -10.402 / -39.54 |
| empty_attacks | native_action_to_ready | median_ms | -4.909 / -12.66 | -4.364 / -12.05 |
| empty_attacks | native_action_to_ready | p95_ms | -8.506 / -15.11 | -10.241 / -20.28 |
| empty_attacks | native_action_to_ready | p99_ms | -8.451 / -14.37 | -13.778 / -24.53 |
| empty_attacks | native_action_to_ready | maximum_ms | -6.872 / -11.36 | -17.609 / -29.10 |
| empty_attacks | warm_rotation | median_ms | -2.359 / -25.65 | -1.949 / -23.16 |
| empty_attacks | warm_rotation | p95_ms | -2.310 / -19.61 | -1.749 / -17.25 |
| empty_attacks | warm_rotation | p99_ms | -2.838 / -22.05 | -1.135 / -10.37 |
| empty_attacks | warm_rotation | maximum_ms | -2.971 / -22.83 | -0.584 / -5.12 |
| empty_attacks | cold_rotation | median_ms | -1.989 / -22.25 | -1.605 / -18.84 |
| empty_attacks | cold_rotation | p95_ms | -1.723 / -16.04 | -0.992 / -10.18 |
| forest | first_render | median_ms | -4.682 / -28.86 | -5.762 / -35.83 |
| forest | first_render | p95_ms | -6.840 / -32.61 | -10.212 / -47.39 |
| forest | first_render | p99_ms | -8.581 / -36.00 | -11.150 / -47.10 |
| forest | first_render | maximum_ms | -9.854 / -37.48 | -13.210 / -51.31 |
| forest | native_action_to_ready | median_ms | -2.511 / -6.98 | -7.982 / -21.96 |
| forest | native_action_to_ready | p95_ms | -9.002 / -17.19 | -13.191 / -25.73 |
| forest | native_action_to_ready | p99_ms | -9.858 / -17.22 | -21.031 / -34.39 |
| forest | native_action_to_ready | maximum_ms | -10.136 / -17.04 | -26.006 / -39.28 |
| forest | warm_rotation | median_ms | -0.815 / -10.48 | -1.158 / -14.27 |
| forest | warm_rotation | p95_ms | -1.702 / -17.27 | -2.368 / -24.48 |
| forest | warm_rotation | p99_ms | -1.704 / -16.22 | -2.434 / -23.39 |
| forest | warm_rotation | maximum_ms | -1.399 / -13.19 | -1.997 / -18.70 |
| forest | cold_rotation | median_ms | -1.373 / -17.38 | -1.309 / -15.97 |
| forest | cold_rotation | p95_ms | -1.443 / -14.07 | -2.322 / -23.52 |
| forest | cold_rotation | p99_ms | -0.878 / -8.29 | -0.616 / -5.76 |
| multi_level_horde | first_render | median_ms | -5.584 / -29.00 | -6.240 / -34.55 |
| multi_level_horde | first_render | p95_ms | -8.458 / -33.63 | -11.556 / -44.66 |
| multi_level_horde | first_render | p99_ms | -9.590 / -35.19 | -12.832 / -46.09 |
| multi_level_horde | first_render | maximum_ms | -11.559 / -39.53 | -14.517 / -49.04 |
| multi_level_horde | native_action_to_ready | median_ms | -3.427 / -8.57 | -7.637 / -19.33 |
| multi_level_horde | native_action_to_ready | p95_ms | -8.087 / -14.06 | -16.073 / -27.59 |
| multi_level_horde | native_action_to_ready | p99_ms | -8.328 / -14.20 | -15.160 / -25.86 |
| multi_level_horde | native_action_to_ready | maximum_ms | -8.354 / -14.13 | -14.902 / -25.41 |
| multi_level_horde | warm_rotation | median_ms | -1.539 / -16.57 | -2.119 / -22.35 |
| multi_level_horde | warm_rotation | p95_ms | -0.846 / -7.36 | -4.651 / -35.18 |
| multi_level_horde | warm_rotation | p99_ms | -1.008 / -8.23 | -8.853 / -49.68 |
| multi_level_horde | warm_rotation | maximum_ms | -0.741 / -6.04 | -11.161 / -54.68 |
| multi_level_horde | cold_rotation | median_ms | -1.488 / -15.80 | -2.214 / -22.67 |
| multi_level_horde | cold_rotation | p95_ms | -1.577 / -12.79 | -2.445 / -20.22 |
| multi_level_horde | cold_rotation | p99_ms | -1.257 / -9.62 | -2.973 / -22.60 |
| multi_level_horde | cold_rotation | maximum_ms | -1.700 / -12.55 | -3.119 / -23.20 |
| smoke_rain | first_render | median_ms | -6.474 / -36.65 | -7.950 / -43.32 |
| smoke_rain | first_render | p95_ms | -9.248 / -40.33 | -11.232 / -47.07 |
| smoke_rain | first_render | p99_ms | -9.358 / -39.68 | -11.319 / -44.28 |
| smoke_rain | first_render | maximum_ms | -9.345 / -39.23 | -11.013 / -42.11 |
| smoke_rain | native_action_to_ready | median_ms | -6.268 / -16.16 | -8.469 / -21.46 |
| smoke_rain | native_action_to_ready | p95_ms | -10.502 / -19.19 | -13.329 / -23.92 |
| smoke_rain | native_action_to_ready | p99_ms | -19.302 / -29.62 | -12.477 / -22.01 |
| smoke_rain | native_action_to_ready | maximum_ms | -34.165 / -42.15 | -10.974 / -19.05 |
| smoke_rain | warm_rotation | median_ms | -2.442 / -25.15 | -2.232 / -22.52 |
| smoke_rain | warm_rotation | p95_ms | -2.817 / -21.70 | -1.460 / -12.73 |
| smoke_rain | warm_rotation | p99_ms | -3.107 / -22.81 | -1.766 / -14.51 |
| smoke_rain | warm_rotation | maximum_ms | -3.339 / -23.96 | -1.156 / -9.45 |
| smoke_rain | cold_rotation | median_ms | -1.780 / -18.76 | -2.875 / -29.03 |
| smoke_rain | cold_rotation | p95_ms | -1.988 / -16.32 | -4.415 / -34.44 |
| smoke_rain | cold_rotation | p99_ms | -2.044 / -15.95 | -4.149 / -30.63 |
| smoke_rain | cold_rotation | maximum_ms | -1.917 / -14.61 | -4.050 / -29.36 |
| street | first_render | median_ms | -4.585 / -24.99 | -6.602 / -33.21 |
| street | first_render | p95_ms | -7.144 / -28.89 | -11.644 / -41.38 |
| street | first_render | p99_ms | -14.366 / -34.79 | -15.003 / -36.21 |
| street | first_render | maximum_ms | -31.038 / -45.67 | -24.479 / -38.54 |
| street | native_action_to_ready | median_ms | -2.830 / -7.35 | -6.565 / -16.54 |
| street | native_action_to_ready | p95_ms | -7.475 / -13.89 | -12.104 / -20.56 |
| street | native_action_to_ready | p99_ms | -15.176 / -19.55 | -9.301 / -11.80 |
| street | native_action_to_ready | maximum_ms | -34.917 / -30.03 | -13.974 / -13.35 |
| street | warm_rotation | median_ms | -1.568 / -18.39 | -1.603 / -18.07 |
| street | warm_rotation | p95_ms | -1.272 / -11.69 | -2.125 / -18.44 |
| street | warm_rotation | p99_ms | -2.085 / -17.02 | -2.279 / -19.11 |
| street | warm_rotation | maximum_ms | -1.908 / -15.32 | -2.161 / -18.05 |
| street | cold_rotation | median_ms | -1.963 / -21.62 | -1.222 / -13.36 |
| street | cold_rotation | p95_ms | -2.737 / -22.69 | -1.849 / -15.36 |
| street | cold_rotation | p99_ms | -4.854 / -32.79 | -1.733 / -12.93 |
| street | cold_rotation | maximum_ms | -5.176 / -30.79 | -3.963 / -24.29 |
| tool | first_render | median_ms | -5.946 / -31.09 | -6.820 / -36.50 |
| tool | first_render | p95_ms | -9.415 / -36.48 | -10.718 / -42.56 |
| tool | first_render | p99_ms | -9.804 / -36.30 | -10.997 / -39.43 |
| tool | first_render | maximum_ms | -10.055 / -36.80 | -10.525 / -35.93 |
| tool | native_action_to_ready | median_ms | -5.394 / -12.57 | -7.377 / -17.67 |
| tool | native_action_to_ready | p95_ms | -7.781 / -13.69 | -16.332 / -27.91 |
| tool | native_action_to_ready | p99_ms | -6.456 / -11.00 | -25.599 / -37.08 |
| tool | native_action_to_ready | maximum_ms | -7.360 / -12.27 | -29.820 / -40.54 |
| tool | warm_rotation | median_ms | -1.502 / -17.73 | -1.718 / -19.74 |
| tool | warm_rotation | p95_ms | -1.562 / -14.73 | -2.709 / -23.77 |
| tool | warm_rotation | p99_ms | -1.541 / -13.72 | -2.727 / -23.20 |
| tool | warm_rotation | maximum_ms | -0.597 / -5.30 | -2.819 / -23.48 |
| tool | cold_rotation | median_ms | -1.392 / -16.43 | -1.333 / -15.86 |
| tool | cold_rotation | p95_ms | -1.540 / -14.00 | -2.799 / -24.65 |
| tool | cold_rotation | p99_ms | -1.105 / -9.47 | -2.942 / -24.96 |
| tool | cold_rotation | maximum_ms | -1.331 / -11.16 | -2.937 / -24.55 |

## Exact evidence

- main: source `8c5c32fbd58e707e058343b9fb54e246f4e90d4d`, Host `486fcecd53ee70eff98b4fac1c9a2f176345151d76e08f48e9abab64af31e600`, module `4abb14111eab6bc2f36425f0339570c9183bc357262bdcd072e0b35d26d26666`, installer `c9fd1eb6d4be2cc00e472657c120c4985de9d9430f033f3e6b3af1c3c7b822db`.
- psx: source `68c0f2c1877ff0cf820ed11803c2ffd16f621e0c`, Host `643435eda90b5521e5e57119b918c1850693126474222f5427c16982bf47d4fe`, module `029e44f279bf6c1f60de2da929352a5c80f88e3f0a4bb30e83e7392e4907af15`, installer `0ed9bb4ca93ed57a9d66260c7717c2d59b02add9a2e6f0b29bb5478ce57b4ad5`.
- Frozen plan SHA256: `6c1b8e9cc6e569d74e5f6f9dc3735bca64573fc88775b0fad46bea39f1b0a94f`.
- Result SHA256: `e3492e136440fad26db135b22b3c17d78eba207bc80430296cd5beeb64b8af32`.
- Original rejected D6 result SHA256: `4f224f256352413258afabb0f21b7ab3f2bbd507ffdeed9f2611bd8ba711322b`.
- Every raw report was rehashed. All original arrays, failed matrices and repeat spreads remain intact.
- main/forward: raw report `c2f2ba3baacfa11d8d5d5d92fe2a31fb0104ea694652b458d1850dd6a848ba03`, root `C:\workspace\fpv013-standalone-68c0f2c-main-forward`.
- psx/forward: raw report `cfaf3e9e58ae70338da18e27d0a51fb6a08b8ab9d9083b4da6a71f5437d7f7f6`, root `C:\workspace\fpv013-standalone-68c0f2c-psx-forward`.
- psx/reverse: raw report `a177545f828e8d7d1a578334712946e14a8a572c11e2881ed0885936b2a7987c`, root `C:\workspace\fpv013-standalone-68c0f2c-psx-reverse`.
- main/reverse: raw report `7e85bdac7dff815619b5fc2d743437485b702ad4f73a4c1dd900a24bb2d493a6`, root `C:\workspace\fpv013-standalone-68c0f2c-main-reverse`.


## Extra CPU source storage and encoding cost

The original resolved-sprite/composite payload cap remains 64 MiB. Compressed decoded atlas sources have an additional 64 MiB cap: up to 128 MiB combined payload. These are retained CPU bytes, not total RSS, GPU residency or allocation capacity. Main has no source-cache telemetry; its unavailable values are not reported as zero. Encoding time is cumulative measured CPU wall time spent retaining atlas sources since loading, not active CPU execution or a complete startup/load-time measurement. Copy counts and encoding totals are cumulative and must not be summed across scenes.

| Role / direction | Scene | Compressed source MiB min / max | Resolved + source peak MiB | Encoding cumulative ms | CPU crop count cumulative |
|---|---|---:|---:|---:|---:|
| psx/baseline/forward | armed | 40.280 / 40.280 | 40.610 | 1224.907 | 53 |
| psx/baseline/forward | city_blocks | 40.280 / 40.280 | 40.544 | 1224.907 | 40 |
| psx/baseline/forward | creatures_100 | 40.280 / 40.280 | 40.563 | 1224.907 | 41 |
| psx/baseline/forward | creatures_20 | 40.280 / 40.280 | 40.563 | 1224.907 | 41 |
| psx/baseline/forward | dense_city | 40.280 / 40.280 | 40.544 | 1224.907 | 40 |
| psx/baseline/forward | empty_attacks | 40.280 / 40.280 | 40.610 | 1224.907 | 53 |
| psx/baseline/forward | forest | 40.280 / 40.280 | 40.563 | 1224.907 | 41 |
| psx/baseline/forward | multi_level_horde | 40.280 / 40.280 | 40.563 | 1224.907 | 41 |
| psx/baseline/forward | smoke_rain | 40.280 / 40.280 | 40.602 | 1224.907 | 51 |
| psx/baseline/forward | street | 40.280 / 40.280 | 40.524 | 1224.907 | 35 |
| psx/baseline/forward | tool | 40.280 / 40.280 | 40.606 | 1224.907 | 52 |
| psx/baseline/reverse | armed | 40.280 / 40.280 | 40.610 | 1233.457 | 53 |
| psx/baseline/reverse | city_blocks | 40.280 / 40.280 | 40.544 | 1233.457 | 40 |
| psx/baseline/reverse | creatures_100 | 40.280 / 40.280 | 40.563 | 1233.457 | 41 |
| psx/baseline/reverse | creatures_20 | 40.280 / 40.280 | 40.563 | 1233.457 | 41 |
| psx/baseline/reverse | dense_city | 40.280 / 40.280 | 40.544 | 1233.457 | 40 |
| psx/baseline/reverse | empty_attacks | 40.280 / 40.280 | 40.610 | 1233.457 | 53 |
| psx/baseline/reverse | forest | 40.280 / 40.280 | 40.563 | 1233.457 | 41 |
| psx/baseline/reverse | multi_level_horde | 40.280 / 40.280 | 40.563 | 1233.457 | 41 |
| psx/baseline/reverse | smoke_rain | 40.280 / 40.280 | 40.602 | 1233.457 | 51 |
| psx/baseline/reverse | street | 40.280 / 40.280 | 40.524 | 1233.457 | 35 |
| psx/baseline/reverse | tool | 40.280 / 40.280 | 40.606 | 1233.457 | 52 |

## Exact strict warm maxima and completed cleanup

Each maximum uses the original 96 strict warm samples and its exact raw index. Record/retire CPU intervals and cleanup counts do not measure GPU execution, global GPU idleness or causality.

| Role / direction | Scene | Actual loaded Z | CPU cache peak MiB | Warm maximum ms | Raw index | Record / retire CPU ms | Cleanup calls at maximum |
|---|---|---|---:|---:|---:|---:|---:|
| psx/baseline/forward | armed | [-10, 10, 21] | 7.780 | 11.010 | 120 | 0.068 / 0.139 | 1 |
| psx/baseline/forward | city_blocks | [-10, 10, 21] | 17.798 | 15.103 | 121 | 0.165 / 0.037 | 1 |
| psx/baseline/forward | creatures_100 | [-10, 10, 21] | 7.805 | 10.726 | 101 | 0.107 / 0.032 | 1 |
| psx/baseline/forward | creatures_20 | [-10, 10, 21] | 7.802 | 11.371 | 159 | 0.082 / 0.026 | 1 |
| psx/baseline/forward | dense_city | [-10, 10, 21] | 24.708 | 23.922 | 150 | 0.290 / 0.068 | 1 |
| psx/baseline/forward | empty_attacks | [-10, 10, 21] | 7.785 | 10.043 | 136 | 0.132 / 0.026 | 1 |
| psx/baseline/forward | forest | [-10, 10, 21] | 7.795 | 9.207 | 190 | 0.079 / 0.024 | 1 |
| psx/baseline/forward | multi_level_horde | [-10, 10, 21] | 8.908 | 11.532 | 185 | 0.122 / 0.068 | 1 |
| psx/baseline/forward | smoke_rain | [-10, 10, 21] | 8.372 | 10.596 | 189 | 0.087 / 0.024 | 1 |
| psx/baseline/forward | street | [-10, 10, 21] | 8.979 | 10.541 | 170 | 0.099 / 0.028 | 1 |
| psx/baseline/forward | tool | [-10, 10, 21] | 7.714 | 10.661 | 186 | 0.064 / 0.103 | 1 |
| psx/baseline/reverse | armed | [-10, 10, 21] | 7.780 | 11.131 | 105 | 0.116 / 0.166 | 1 |
| psx/baseline/reverse | city_blocks | [-10, 10, 21] | 17.798 | 11.579 | 100 | 0.149 / 0.028 | 1 |
| psx/baseline/reverse | creatures_100 | [-10, 10, 21] | 7.805 | 9.702 | 191 | 0.079 / 0.025 | 1 |
| psx/baseline/reverse | creatures_20 | [-10, 10, 21] | 7.802 | 9.713 | 158 | 0.069 / 0.022 | 1 |
| psx/baseline/reverse | dense_city | [-10, 10, 21] | 24.708 | 15.432 | 144 | 0.164 / 0.037 | 1 |
| psx/baseline/reverse | empty_attacks | [-10, 10, 21] | 7.785 | 10.825 | 151 | 0.131 / 0.038 | 1 |
| psx/baseline/reverse | forest | [-10, 10, 21] | 7.795 | 8.684 | 100 | 0.079 / 0.051 | 1 |
| psx/baseline/reverse | multi_level_horde | [-10, 10, 21] | 8.908 | 9.250 | 177 | 0.076 / 0.020 | 1 |
| psx/baseline/reverse | smoke_rain | [-10, 10, 21] | 8.372 | 11.084 | 103 | 0.108 / 0.038 | 1 |
| psx/baseline/reverse | street | [-10, 10, 21] | 8.979 | 9.810 | 168 | 0.096 / 0.008 | 1 |
| psx/baseline/reverse | tool | [-10, 10, 21] | 7.714 | 9.186 | 144 | 0.084 / 0.146 | 1 |

## Immutable vertex allocations over every raw frame

Creation/reuse columns sum per-frame events. Pool and complete GPU resource columns are peaks, not time sums. Initial render counters are recorded separately. Main has no vertex-pool telemetry; its missing values are unavailable. Exact byte sharing does not merge materials, instances, draw ranges or writable geometry. CPU wall intervals do not measure GPU execution. All-zero Windows thread/process CPU streams do not establish active CPU execution. This ordinary series has no new hardware GPU execution trace.

| Role / direction | Scene | Raw frames | Initial create / reuse | Frame creates / reuses | Vertex pool peak MiB | Full GPU resource peak MiB | Static upload total MiB |
|---|---|---:|---:|---:|---:|---:|---:|
| psx/forward | armed | 640 | 0 / 0 | 0 / 0 | 13.808 | 54.493 | 0.000 |
| psx/forward | city_blocks | 599 | 85 / 5 | 255 / 4 | 8.651 | 48.219 | 5.194 |
| psx/forward | creatures_100 | 637 | 0 / 0 | 0 / 3 | 13.808 | 54.402 | 0.000 |
| psx/forward | creatures_20 | 640 | 0 / 0 | 1 / 0 | 13.808 | 54.402 | 0.001 |
| psx/forward | dense_city | 576 | 29 / 0 | 136 / 0 | 13.654 | 54.223 | 4.150 |
| psx/forward | empty_attacks | 640 | 0 / 0 | 0 / 0 | 13.808 | 54.493 | 0.000 |
| psx/forward | forest | 640 | 5 / 0 | 36 / 0 | 13.798 | 54.393 | 0.131 |
| psx/forward | multi_level_horde | 635 | 2 / 0 | 2 / 1 | 13.807 | 54.401 | 0.005 |
| psx/forward | smoke_rain | 640 | 0 / 1 | 0 / 3 | 13.808 | 54.402 | 0.000 |
| psx/forward | street | 639 | 0 / 0 | 31 / 62 | 2.471 | 41.748 | 2.255 |
| psx/forward | tool | 639 | 0 / 0 | 0 / 0 | 13.808 | 54.462 | 0.000 |
| psx/reverse | armed | 640 | 0 / 0 | 0 / 0 | 13.808 | 54.493 | 0.000 |
| psx/reverse | city_blocks | 631 | 85 / 5 | 255 / 4 | 8.651 | 48.219 | 5.194 |
| psx/reverse | creatures_100 | 639 | 0 / 0 | 0 / 3 | 13.808 | 54.402 | 0.000 |
| psx/reverse | creatures_20 | 640 | 0 / 0 | 1 / 0 | 13.808 | 54.402 | 0.001 |
| psx/reverse | dense_city | 596 | 29 / 0 | 136 / 0 | 13.654 | 54.223 | 4.150 |
| psx/reverse | empty_attacks | 649 | 0 / 0 | 0 / 0 | 13.808 | 54.493 | 0.000 |
| psx/reverse | forest | 671 | 5 / 0 | 36 / 0 | 13.798 | 54.393 | 0.131 |
| psx/reverse | multi_level_horde | 640 | 2 / 0 | 2 / 1 | 13.807 | 54.401 | 0.005 |
| psx/reverse | smoke_rain | 640 | 0 / 1 | 0 / 3 | 13.808 | 54.402 | 0.000 |
| psx/reverse | street | 640 | 0 / 0 | 31 / 62 | 2.471 | 41.748 | 2.255 |
| psx/reverse | tool | 641 | 0 / 0 | 0 / 0 | 13.808 | 54.462 | 0.000 |

The previous Core650 result remains rejected for all four repeated tail metrics (result SHA256 `6e6df03622cd11877a249fe58c3985065721136fcf17c90cb11a1bb67fc357e1`). This new comparison does not waive that history or establish acceptance for the combined feature stack. Full first-frame and action-to-ready quantiles, maxima and both run orders appear above; a median below 15 ms does not establish a 15 ms tail budget.


## Qualification limits and preserved evidence

Original source, pixel and normal-vision correctness oracles precede ordinary timing; the ordinary runs explicitly disable pixel differential work. The normal Native 2D pass, quality, R48, loaded Z, viewport/internal resolution and inclusive post-Present frame clock remain. The same previously recorded Intel background service is retained by the frozen background policy. No new profiler/UAC capture was used for this series.

Earlier rejected/partial matrices and raw reports remain retained, including Core650 4 and 4ea 53 repeated regressions, B19 45, DAE 1, sparse512 cross-attempt failures and the old 169 single-direction tails. No successful helper/sanitizer test certifies whole-game performance. Actual Windows/Linux Native 1040/1807, installer/repair/rollback/save checks, unchanged-binary GPU lifecycle/30-minute qualification and manual battle/physical high-DPI status require their own evidence.

Result SHA256 `e3492e136440fad26db135b22b3c17d78eba207bc80430296cd5beeb64b8af32`; plan SHA256 `6c1b8e9cc6e569d74e5f6f9dc3735bca64573fc88775b0fad46bea39f1b0a94f`; unabridged mechanical report SHA256 `06df41f607fffaec83c2029def579c11b79db297a98d93a77a06e280f05241c2`.


## All retained Core problem metrics in both new orders

The previous Core7baa passed the mechanical four-run gate but failed the additional retained-tail review. All 35 retained observations (17 from169, four from650, two from06, twelve from7baa) remain as27 distinct issue keys. Any recurrence above >5% AND >0.1ms in either fresh direction blocks the additional Core gate. All older combined feature matrix negatives remain for a fresh final matrix.

| Scene | Group / metric | Prior origins | Forward before / after ms | Reverse before / after ms |
|---|---|---|---:|---:|
| armed | warm_rotation/maximum_ms | 169:forward; 7baa:forward | 12.506 / 11.010 | 12.266 / 11.131 |
| city_blocks | cold_rotation/maximum_ms | 7baa:reverse | 20.517 / 19.873 | 19.055 / 14.555 |
| city_blocks | native_action_to_ready/maximum_ms | 169:reverse | 62.921 / 66.346 FAIL | 60.977 / 53.533 |
| city_blocks | native_action_to_ready/p99_ms | 169:reverse | 61.447 / 59.766 | 59.742 / 51.768 |
| city_blocks | warm_rotation/maximum_ms | 169:forward | 17.670 / 15.103 | 17.274 / 11.579 |
| creatures_100 | native_action_to_ready/maximum_ms | 169:reverse | 61.570 / 62.457 | 62.512 / 51.934 |
| creatures_100 | native_action_to_ready/p99_ms | 169:reverse | 61.040 / 57.790 | 61.953 / 49.928 |
| creatures_100 | warm_rotation/maximum_ms | 169:reverse | 12.918 / 10.726 | 13.513 / 9.702 |
| creatures_20 | cold_rotation/maximum_ms | 169:reverse | 12.597 / 10.480 | 13.345 / 9.678 |
| creatures_20 | cold_rotation/p95_ms | 169:reverse | 11.548 / 10.202 | 12.823 / 8.909 |
| creatures_20 | cold_rotation/p99_ms | 169:reverse | 12.574 / 10.457 | 13.234 / 9.614 |
| empty_attacks | cold_rotation/maximum_ms | 650:both-directions | 12.159 / 11.463 | 10.134 / 10.655 FAIL |
| empty_attacks | cold_rotation/p95_ms | 169:reverse | 10.743 / 9.020 | 9.743 / 8.751 |
| empty_attacks | cold_rotation/p99_ms | 650:both-directions | 11.369 / 10.198 | 10.052 / 9.703 |
| empty_attacks | warm_rotation/p95_ms | 169:reverse | 11.779 / 9.469 | 10.138 / 8.389 |
| forest | cold_rotation/maximum_ms | 650:both-directions; 06:reverse; 7baa:forward | 10.607 / 11.296 FAIL | 11.742 / 10.326 |
| forest | cold_rotation/p95_ms | 7baa:forward | 10.258 / 8.815 | 9.871 / 7.549 |
| forest | cold_rotation/p99_ms | 169:reverse; 06:reverse; 7baa:forward | 10.592 / 9.713 | 10.709 / 10.092 |
| multi_level_horde | warm_rotation/maximum_ms | 7baa:forward | 12.273 / 11.532 | 20.411 / 9.250 |
| street | cold_rotation/maximum_ms | 169:reverse; 7baa:forward | 16.811 / 11.635 | 16.317 / 12.354 |
| street | cold_rotation/p95_ms | 169:reverse; 7baa:forward | 12.061 / 9.324 | 12.038 / 10.189 |
| street | cold_rotation/p99_ms | 169:reverse; 7baa:forward | 14.805 / 9.951 | 13.405 / 11.672 |
| street | warm_rotation/maximum_ms | 7baa:forward | 12.449 / 10.541 | 11.971 / 9.810 |
| street | warm_rotation/p95_ms | 7baa:forward | 10.881 / 9.609 | 11.525 / 9.399 |
| street | warm_rotation/p99_ms | 7baa:forward | 12.247 / 10.162 | 11.923 / 9.644 |
| tool | cold_rotation/p99_ms | 169:reverse | 11.664 / 10.559 | 11.786 / 8.844 |
| tool | warm_rotation/maximum_ms | 650:both-directions | 11.258 / 10.661 | 12.005 / 9.186 |

Mechanical four-run gate: True; additional Core consistency gate: False. Neither status establishes combined feature, full Native, hardware or merge acceptance.
Retained review SHA256 `2b8599dca0829d2546283730fbee55da25db8f8547fe75464cd1695c2d45dae1`.


## Native tile reads and diagnostic isolation

The exact fresh Native functional route checked 15,857,638 loaded tile reads against canonical terrain, furniture, field and item accessors. It also retained 15,857,566 source-fact checks, 72 field mutations and all 52 exact atlas differential checks. This validates data/visibility correctness; its timings are not ordinary performance evidence. Both new ordinary candidate runs report exactly zero tile-oracle work in every one of their11scenes. The existing atlas diagnostic-isolation checks remain independently mandatory.

Earlier CPU-only profiles locate Native submap/item/furniture work but are separated by hours and have no frame boundaries. Sample counts are not saved milliseconds and do not qualify a direct profiled speed comparison.

Fresh functional receipt SHA256 `a7dde0bf2026f3f5e1fbb1660cbccf04f7814e1da725c5b6234722a9a4f49c53`.
