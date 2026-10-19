# Six ordinary core controls — diagnostic only

All three binaries were measured in forward and reverse order: Main, PSX, Common with camera/held-assets/target-HUD disabled, then the reverse order. All 11 full-size scenes remain present. 4224 first-render observations and 12672 warm/cold camera observations. No profiler, debugger or GPU debug.

Same RTX 4090 / D3D12, corrected private SDL stack, UDP inventory, actual VSync 1, viewport/internal resolution, R48, Z and quality settings. Each metric is **median / p95 / p99 / maximum in ms**. There are 64 first-render/action-to-ready and 96 warm/cold observations per scene and direction; small-sample p99 does not qualify rare stalls.

This is additional diagnosis, not a replacement acceptance test. The preceding D6 standalone rejection and its original 29 repeated regressions remain preserved. A common binary with disabled features still differs in compiled runtime/compiler layout; a difference does not identify one source change by itself. No merge or 15 ms acceptance.

## All repeated differences

| Reference | Candidate | Regressions | Improvements |
|---|---|---:|---:|
| main | psx | 3 | 65 |
| main | common_off | 7 | 25 |
| psx | common_off | 45 | 5 |

Counts require >5% AND >0.1 ms in both directions. They count metrics, not independent incidents. Single-direction outliers and the full observed repeat spread remain in result.json.

## first_render

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 20.067 / 27.515 / 31.500 / 31.569 | 20.495 / 31.768 / 34.923 / 35.144 |
| armed | psx | 17.329 / 23.492 / 25.649 / 26.998 | 17.396 / 23.441 / 28.062 / 30.402 |
| armed | common_off | 18.635 / 27.287 / 29.119 / 29.304 | 18.881 / 26.596 / 27.406 / 27.523 |
| city_blocks | main | 22.388 / 29.036 / 30.553 / 31.521 | 21.814 / 29.529 / 30.576 / 30.956 |
| city_blocks | psx | 19.334 / 27.790 / 31.560 / 32.912 | 20.249 / 26.907 / 28.840 / 29.587 |
| city_blocks | common_off | 18.253 / 26.015 / 30.449 / 32.851 | 20.985 / 27.344 / 28.508 / 28.985 |
| creatures_100 | main | 22.258 / 29.599 / 30.524 / 30.643 | 18.703 / 23.138 / 23.890 / 24.375 |
| creatures_100 | psx | 17.100 / 25.622 / 29.700 / 29.957 | 17.972 / 27.265 / 28.668 / 29.048 |
| creatures_100 | common_off | 19.306 / 26.612 / 27.836 / 28.203 | 20.958 / 26.999 / 31.808 / 33.305 |
| creatures_20 | main | 21.408 / 29.479 / 30.935 / 31.779 | 19.093 / 24.185 / 27.922 / 28.858 |
| creatures_20 | psx | 16.760 / 24.394 / 25.766 / 26.595 | 19.169 / 26.538 / 29.412 / 32.044 |
| creatures_20 | common_off | 19.584 / 27.840 / 31.163 / 35.219 | 19.354 / 27.084 / 28.934 / 30.409 |
| dense_city | main | 25.378 / 31.294 / 35.587 / 38.899 | 23.208 / 34.530 / 38.295 / 39.216 |
| dense_city | psx | 20.986 / 28.149 / 34.287 / 39.391 | 23.265 / 31.283 / 36.942 / 40.212 |
| dense_city | common_off | 23.038 / 33.620 / 37.254 / 42.606 | 25.444 / 34.146 / 36.751 / 38.381 |
| empty_attacks | main | 19.012 / 26.670 / 28.224 / 28.225 | 19.477 / 26.469 / 31.205 / 36.107 |
| empty_attacks | psx | 19.015 / 26.543 / 31.374 / 31.734 | 18.592 / 25.637 / 28.511 / 29.316 |
| empty_attacks | common_off | 18.540 / 25.784 / 27.533 / 27.598 | 17.749 / 27.073 / 28.217 / 28.846 |
| forest | main | 18.689 / 23.978 / 26.389 / 26.765 | 15.602 / 21.338 / 22.621 / 22.794 |
| forest | psx | 14.366 / 19.467 / 20.202 / 20.616 | 17.461 / 23.458 / 31.033 / 35.105 |
| forest | common_off | 15.442 / 21.174 / 22.517 / 22.710 | 17.805 / 23.173 / 25.143 / 25.309 |
| multi_level_horde | main | 21.048 / 28.698 / 32.979 / 33.300 | 17.983 / 23.018 / 26.500 / 29.656 |
| multi_level_horde | psx | 15.551 / 23.137 / 26.844 / 27.679 | 19.216 / 26.453 / 28.538 / 29.752 |
| multi_level_horde | common_off | 18.776 / 24.954 / 28.313 / 29.527 | 19.236 / 27.037 / 29.868 / 32.792 |
| smoke_rain | main | 19.129 / 25.957 / 28.297 / 29.802 | 17.217 / 21.619 / 27.440 / 28.012 |
| smoke_rain | psx | 15.639 / 23.185 / 23.834 / 24.284 | 16.027 / 20.840 / 22.871 / 24.047 |
| smoke_rain | common_off | 17.028 / 25.171 / 25.804 / 26.033 | 18.033 / 23.324 / 23.848 / 23.908 |
| street | main | 18.495 / 29.819 / 43.009 / 52.827 | 21.861 / 29.510 / 52.743 / 71.967 |
| street | psx | 16.561 / 22.443 / 38.798 / 61.783 | 18.728 / 26.111 / 42.489 / 64.464 |
| street | common_off | 17.538 / 25.856 / 38.259 / 56.330 | 19.985 / 28.969 / 47.172 / 75.370 |
| tool | main | 20.009 / 26.122 / 26.673 / 26.710 | 19.310 / 27.451 / 32.016 / 34.132 |
| tool | psx | 16.952 / 22.636 / 25.120 / 25.390 | 16.848 / 23.558 / 25.132 / 25.870 |
| tool | common_off | 17.511 / 24.243 / 26.343 / 26.370 | 20.869 / 29.021 / 31.640 / 33.152 |

## native_action_to_ready

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 41.520 / 61.980 / 64.058 / 65.734 | 48.317 / 66.971 / 75.342 / 82.192 |
| armed | psx | 39.666 / 55.447 / 59.692 / 60.190 | 37.436 / 58.127 / 63.997 / 69.571 |
| armed | common_off | 39.753 / 59.154 / 60.837 / 60.895 | 39.867 / 59.108 / 60.330 / 61.628 |
| city_blocks | main | 46.643 / 64.894 / 65.617 / 65.652 | 43.003 / 61.073 / 62.547 / 63.344 |
| city_blocks | psx | 41.125 / 59.194 / 68.838 / 69.049 | 42.912 / 60.737 / 66.215 / 66.729 |
| city_blocks | common_off | 40.122 / 59.372 / 63.595 / 66.776 | 42.802 / 61.860 / 64.186 / 66.734 |
| creatures_100 | main | 43.613 / 61.464 / 62.465 / 62.771 | 38.665 / 51.800 / 53.223 / 54.291 |
| creatures_100 | psx | 38.689 / 58.196 / 62.194 / 64.473 | 38.719 / 56.919 / 60.501 / 62.925 |
| creatures_100 | common_off | 41.147 / 59.327 / 62.732 / 63.257 | 41.611 / 61.966 / 67.139 / 73.021 |
| creatures_20 | main | 41.742 / 61.740 / 65.350 / 68.081 | 45.296 / 55.609 / 62.914 / 64.340 |
| creatures_20 | psx | 37.227 / 54.485 / 55.892 / 56.153 | 40.077 / 58.791 / 64.896 / 65.178 |
| creatures_20 | common_off | 40.200 / 61.705 / 66.884 / 69.213 | 41.829 / 60.818 / 63.473 / 65.342 |
| dense_city | main | 51.188 / 70.085 / 73.759 / 76.123 | 49.821 / 74.290 / 76.999 / 79.400 |
| dense_city | psx | 46.425 / 65.295 / 73.617 / 80.231 | 53.089 / 71.264 / 80.435 / 85.531 |
| dense_city | common_off | 50.518 / 69.686 / 84.919 / 100.633 | 54.270 / 73.166 / 75.952 / 76.767 |
| empty_attacks | main | 40.563 / 58.567 / 60.508 / 62.620 | 41.121 / 58.989 / 65.596 / 68.028 |
| empty_attacks | psx | 43.312 / 60.272 / 63.243 / 65.361 | 39.930 / 60.092 / 61.926 / 62.629 |
| empty_attacks | common_off | 40.535 / 57.856 / 61.347 / 63.894 | 39.838 / 58.166 / 61.402 / 63.344 |
| forest | main | 43.016 / 54.927 / 56.331 / 56.517 | 36.836 / 49.296 / 52.323 / 53.489 |
| forest | psx | 34.600 / 49.762 / 50.989 / 51.664 | 38.232 / 58.773 / 70.479 / 77.314 |
| forest | common_off | 35.632 / 50.373 / 53.625 / 53.705 | 39.368 / 54.903 / 55.863 / 56.970 |
| multi_level_horde | main | 42.562 / 63.119 / 65.822 / 68.675 | 39.881 / 55.260 / 61.571 / 65.137 |
| multi_level_horde | psx | 35.861 / 52.428 / 55.284 / 55.828 | 41.162 / 60.671 / 61.584 / 61.771 |
| multi_level_horde | common_off | 41.119 / 61.480 / 67.026 / 69.938 | 41.875 / 58.945 / 62.856 / 66.675 |
| smoke_rain | main | 38.568 / 57.287 / 61.916 / 65.276 | 39.003 / 55.758 / 59.083 / 60.609 |
| smoke_rain | psx | 35.542 / 53.714 / 55.670 / 55.737 | 36.931 / 51.287 / 53.664 / 54.105 |
| smoke_rain | common_off | 38.252 / 56.528 / 61.496 / 63.430 | 39.399 / 55.162 / 59.568 / 62.213 |
| street | main | 37.343 / 59.010 / 74.963 / 94.393 | 42.166 / 58.927 / 92.582 / 122.550 |
| street | psx | 39.941 / 52.520 / 77.387 / 109.804 | 38.488 / 57.189 / 78.573 / 112.451 |
| street | common_off | 36.869 / 54.170 / 72.263 / 95.999 | 40.285 / 57.822 / 87.442 / 126.345 |
| tool | main | 44.560 / 56.052 / 58.667 / 59.393 | 43.143 / 60.569 / 68.803 / 69.234 |
| tool | psx | 40.268 / 52.934 / 53.763 / 54.216 | 37.882 / 53.881 / 54.851 / 55.168 |
| tool | common_off | 38.736 / 55.976 / 56.513 / 56.616 | 48.357 / 64.185 / 73.172 / 84.889 |

## warm_rotation

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 8.795 / 10.477 / 11.032 / 11.065 | 9.229 / 11.253 / 11.438 / 11.622 |
| armed | psx | 8.474 / 10.361 / 11.391 / 12.037 | 8.398 / 12.423 / 13.541 / 16.685 |
| armed | common_off | 8.616 / 10.721 / 11.338 / 12.028 | 10.405 / 12.882 / 15.149 / 15.604 |
| city_blocks | main | 14.131 / 16.407 / 16.961 / 17.404 | 13.992 / 16.910 / 17.719 / 18.305 |
| city_blocks | psx | 10.511 / 14.129 / 14.870 / 15.405 | 12.207 / 17.230 / 18.296 / 19.329 |
| city_blocks | common_off | 11.353 / 16.971 / 17.991 / 19.150 | 12.566 / 15.346 / 15.882 / 16.478 |
| creatures_100 | main | 11.562 / 14.905 / 15.386 / 15.767 | 9.997 / 13.573 / 15.804 / 15.815 |
| creatures_100 | psx | 8.237 / 9.840 / 11.216 / 11.396 | 8.883 / 13.432 / 16.105 / 16.748 |
| creatures_100 | common_off | 8.845 / 11.620 / 11.993 / 13.279 | 10.583 / 13.938 / 17.477 / 18.057 |
| creatures_20 | main | 10.761 / 13.514 / 14.119 / 15.064 | 9.116 / 11.862 / 12.182 / 13.200 |
| creatures_20 | psx | 8.326 / 10.751 / 11.474 / 11.540 | 8.921 / 11.274 / 12.357 / 12.395 |
| creatures_20 | common_off | 8.146 / 9.842 / 10.107 / 10.175 | 9.039 / 12.603 / 13.367 / 14.101 |
| dense_city | main | 19.728 / 24.631 / 24.950 / 26.565 | 21.786 / 25.983 / 32.245 / 34.284 |
| dense_city | psx | 14.311 / 18.978 / 20.241 / 20.568 | 17.999 / 22.383 / 29.530 / 45.129 |
| dense_city | common_off | 16.460 / 22.676 / 25.586 / 27.170 | 19.044 / 25.330 / 29.262 / 32.071 |
| empty_attacks | main | 9.142 / 10.874 / 13.863 / 13.877 | 9.389 / 12.662 / 14.168 / 15.210 |
| empty_attacks | psx | 8.267 / 10.865 / 11.454 / 12.468 | 7.491 / 10.118 / 10.278 / 10.281 |
| empty_attacks | common_off | 7.949 / 9.629 / 10.314 / 10.548 | 8.694 / 10.453 / 11.360 / 12.016 |
| forest | main | 8.600 / 10.606 / 11.979 / 13.107 | 7.923 / 9.525 / 10.071 / 10.842 |
| forest | psx | 6.527 / 8.310 / 11.664 / 14.155 | 7.114 / 8.636 / 9.332 / 9.400 |
| forest | common_off | 6.836 / 9.259 / 10.190 / 10.704 | 7.129 / 9.447 / 10.239 / 11.283 |
| multi_level_horde | main | 9.729 / 12.219 / 13.605 / 13.785 | 9.268 / 10.557 / 13.082 / 13.189 |
| multi_level_horde | psx | 8.463 / 11.253 / 12.063 / 12.572 | 10.953 / 15.230 / 16.180 / 22.286 |
| multi_level_horde | common_off | 8.057 / 9.319 / 10.375 / 11.165 | 9.437 / 12.231 / 13.365 / 13.694 |
| smoke_rain | main | 9.896 / 11.906 / 12.598 / 12.723 | 9.292 / 10.835 / 11.126 / 11.548 |
| smoke_rain | psx | 8.604 / 11.050 / 11.449 / 11.558 | 8.862 / 12.216 / 15.141 / 15.283 |
| smoke_rain | common_off | 8.854 / 11.654 / 12.886 / 13.444 | 9.759 / 14.911 / 19.127 / 19.144 |
| street | main | 8.210 / 9.130 / 9.575 / 10.472 | 9.808 / 12.540 / 14.740 / 16.344 |
| street | psx | 7.661 / 9.593 / 10.797 / 10.989 | 7.909 / 10.522 / 11.086 / 11.226 |
| street | common_off | 7.151 / 9.040 / 9.404 / 10.289 | 8.413 / 11.589 / 11.955 / 12.440 |
| tool | main | 8.973 / 11.233 / 12.165 / 12.884 | 8.042 / 9.388 / 10.404 / 10.714 |
| tool | psx | 8.160 / 10.733 / 12.236 / 13.077 | 7.673 / 10.300 / 10.509 / 10.893 |
| tool | common_off | 8.523 / 11.233 / 12.448 / 14.003 | 9.629 / 14.096 / 18.334 / 19.851 |

## cold_rotation

| Scene | Binary | Forward | Reverse |
|---|---|---|---|
| armed | main | 8.946 / 10.507 / 11.051 / 11.554 | 10.484 / 14.191 / 15.521 / 16.953 |
| armed | psx | 7.924 / 8.717 / 9.573 / 10.328 | 8.227 / 10.403 / 10.867 / 11.759 |
| armed | common_off | 8.466 / 11.465 / 13.451 / 13.637 | 10.218 / 13.208 / 13.941 / 14.140 |
| city_blocks | main | 15.168 / 19.630 / 22.496 / 23.769 | 17.134 / 21.484 / 22.267 / 22.370 |
| city_blocks | psx | 13.002 / 16.703 / 17.318 / 20.126 | 13.527 / 17.115 / 19.961 / 20.555 |
| city_blocks | common_off | 11.876 / 17.876 / 19.926 / 22.115 | 16.190 / 21.605 / 26.322 / 37.185 |
| creatures_100 | main | 10.700 / 13.135 / 14.383 / 14.735 | 10.969 / 13.230 / 15.230 / 15.520 |
| creatures_100 | psx | 8.674 / 11.517 / 12.067 / 12.942 | 9.458 / 13.986 / 15.509 / 21.971 |
| creatures_100 | common_off | 9.310 / 12.799 / 13.346 / 15.492 | 10.488 / 13.616 / 14.864 / 15.748 |
| creatures_20 | main | 10.757 / 13.659 / 14.194 / 15.117 | 9.401 / 12.304 / 15.147 / 15.665 |
| creatures_20 | psx | 8.222 / 10.652 / 11.476 / 13.608 | 8.749 / 10.603 / 11.223 / 11.617 |
| creatures_20 | common_off | 8.407 / 10.430 / 12.846 / 13.262 | 8.478 / 10.916 / 11.781 / 12.106 |
| dense_city | main | 22.180 / 27.570 / 29.066 / 31.387 | 19.393 / 26.514 / 26.899 / 27.632 |
| dense_city | psx | 15.242 / 19.541 / 20.724 / 22.729 | 19.745 / 23.661 / 26.988 / 31.902 |
| dense_city | common_off | 16.750 / 22.484 / 25.691 / 26.506 | 19.761 / 24.635 / 25.707 / 26.277 |
| empty_attacks | main | 9.409 / 12.787 / 13.976 / 14.375 | 9.654 / 12.752 / 13.733 / 13.736 |
| empty_attacks | psx | 8.623 / 11.270 / 11.807 / 12.007 | 7.907 / 10.217 / 14.362 / 14.664 |
| empty_attacks | common_off | 7.873 / 9.786 / 10.611 / 11.505 | 9.594 / 12.669 / 14.202 / 16.102 |
| forest | main | 8.654 / 10.469 / 11.197 / 12.556 | 8.473 / 10.913 / 11.550 / 11.573 |
| forest | psx | 6.947 / 9.514 / 10.159 / 10.751 | 7.658 / 10.072 / 10.913 / 11.379 |
| forest | common_off | 7.314 / 10.665 / 11.263 / 12.106 | 7.732 / 9.822 / 11.010 / 11.253 |
| multi_level_horde | main | 10.142 / 12.844 / 13.237 / 14.204 | 9.363 / 11.558 / 12.001 / 12.020 |
| multi_level_horde | psx | 8.103 / 11.110 / 12.192 / 12.320 | 10.069 / 13.990 / 15.205 / 15.337 |
| multi_level_horde | common_off | 7.928 / 8.979 / 9.781 / 13.173 | 9.479 / 11.158 / 12.992 / 13.031 |
| smoke_rain | main | 10.987 / 14.617 / 15.134 / 15.740 | 9.366 / 10.749 / 11.807 / 13.377 |
| smoke_rain | psx | 8.100 / 11.173 / 12.702 / 13.495 | 7.770 / 10.874 / 11.572 / 11.861 |
| smoke_rain | common_off | 8.271 / 10.530 / 11.198 / 11.846 | 9.021 / 11.586 / 12.285 / 12.710 |
| street | main | 8.664 / 10.838 / 13.498 / 16.190 | 10.530 / 15.159 / 16.739 / 20.290 |
| street | psx | 8.559 / 11.410 / 13.462 / 13.590 | 8.452 / 11.424 / 12.793 / 13.181 |
| street | common_off | 9.358 / 12.395 / 14.449 / 20.992 | 9.245 / 12.377 / 13.901 / 15.162 |
| tool | main | 8.945 / 10.890 / 11.527 / 12.011 | 8.319 / 9.819 / 11.197 / 12.399 |
| tool | psx | 8.169 / 10.394 / 10.817 / 10.867 | 7.788 / 10.056 / 10.421 / 10.434 |
| tool | common_off | 8.696 / 12.480 / 13.053 / 16.310 | 9.226 / 12.045 / 12.702 / 13.145 |

## Resources and initial render

Values are forward / reverse; measured peaks do not establish absence of leaks.

| Scene | Binary | Initial render ms | Warm upload KiB | Warm static KiB | Warm draws median | GPU peak MiB | RSS peak MiB |
|---|---|---|---|---|---|---|---|
| armed | main | 32.019 / 32.952 | 69532.047 / 69532.047 | unavailable / unavailable | 35.000 / 35.000 | 55.728 / 55.728 | 1732.934 / 1733.594 |
| armed | psx | 25.742 / 24.688 | 50072.172 / 50072.172 | 0.000 / 0.000 | 40.000 / 40.000 | 56.964 / 56.964 | 1688.207 / 1684.070 |
| armed | common_off | 26.791 / 35.094 | 50072.172 / 50072.172 | 0.000 / 0.000 | 40.000 / 40.000 | 56.964 / 56.964 | 1685.445 / 1685.953 |
| city_blocks | main | 81.448 / 90.071 | 91597.156 / 91597.156 | unavailable / unavailable | 291.000 / 291.000 | 47.503 / 47.503 | 1714.254 / 1714.453 |
| city_blocks | psx | 73.810 / 78.505 | 71873.281 / 71873.281 | 0.000 / 0.000 | 296.000 / 296.000 | 48.736 / 48.736 | 1669.375 / 1668.988 |
| city_blocks | common_off | 71.467 / 80.127 | 71873.281 / 71873.281 | 0.000 / 0.000 | 296.000 / 296.000 | 48.736 / 48.736 | 1670.656 / 1670.844 |
| creatures_100 | main | 28.927 / 29.764 | 62359.938 / 62359.938 | unavailable / unavailable | 33.000 / 33.000 | 53.977 / 53.977 | 1725.789 / 1727.031 |
| creatures_100 | psx | 26.183 / 30.473 | 42636.062 / 42636.062 | 0.000 / 0.000 | 38.000 / 38.000 | 55.210 / 55.210 | 1685.066 / 1681.238 |
| creatures_100 | common_off | 26.016 / 29.189 | 42636.062 / 42636.062 | 0.000 / 0.000 | 38.000 / 38.000 | 55.210 / 55.210 | 1682.543 / 1682.484 |
| creatures_20 | main | 29.538 / 25.003 | 61555.672 / 61555.672 | unavailable / unavailable | 32.000 / 32.000 | 53.974 / 53.974 | 1726.898 / 1727.062 |
| creatures_20 | psx | 24.664 / 33.080 | 41831.797 / 41831.797 | 0.000 / 0.000 | 37.000 / 37.000 | 55.207 / 55.207 | 1685.113 / 1681.340 |
| creatures_20 | common_off | 26.536 / 28.149 | 41831.797 / 41831.797 | 0.000 / 0.000 | 37.000 / 37.000 | 55.207 / 55.207 | 1683.754 / 1682.668 |
| dense_city | main | 66.767 / 57.918 | 129078.453 / 129078.453 | unavailable / unavailable | 446.000 / 446.000 | 53.784 / 53.784 | 1742.145 / 1743.508 |
| dense_city | psx | 59.709 / 69.289 | 109354.578 / 109354.578 | 0.000 / 0.000 | 451.000 / 451.000 | 55.018 / 55.018 | 1690.746 / 1693.238 |
| dense_city | common_off | 58.273 / 64.159 | 109354.578 / 109354.578 | 0.000 / 0.000 | 451.000 / 451.000 | 55.018 / 55.018 | 1695.262 / 1694.914 |
| empty_attacks | main | 32.383 / 31.672 | 61409.547 / 61409.547 | unavailable / unavailable | 32.000 / 32.000 | 55.728 / 55.728 | 1730.770 / 1733.062 |
| empty_attacks | psx | 32.727 / 24.908 | 41685.672 / 41685.672 | 0.000 / 0.000 | 37.000 / 37.000 | 56.964 / 56.964 | 1688.121 / 1683.832 |
| empty_attacks | common_off | 30.070 / 28.010 | 41685.672 / 41685.672 | 0.000 / 0.000 | 37.000 / 37.000 | 56.964 / 56.964 | 1685.230 / 1685.875 |
| forest | main | 33.078 / 34.704 | 61525.719 / 61525.719 | unavailable / unavailable | 41.000 / 41.000 | 53.962 / 53.962 | 1727.652 / 1728.969 |
| forest | psx | 27.541 / 30.727 | 41801.844 / 41801.844 | 0.000 / 0.000 | 46.000 / 46.000 | 55.196 / 55.196 | 1684.129 / 1679.680 |
| forest | common_off | 36.905 / 32.199 | 41801.844 / 41801.844 | 0.000 / 0.000 | 46.000 / 46.000 | 55.196 / 55.196 | 1682.871 / 1681.723 |
| multi_level_horde | main | 31.355 / 26.525 | 64569.250 / 64569.250 | unavailable / unavailable | 59.500 / 59.500 | 53.972 / 53.972 | 1727.953 / 1729.371 |
| multi_level_horde | psx | 25.665 / 35.065 | 44845.375 / 44845.375 | 0.000 / 0.000 | 64.500 / 64.500 | 55.206 / 55.206 | 1684.004 / 1679.973 |
| multi_level_horde | common_off | 29.991 / 30.749 | 44845.375 / 44845.375 | 0.000 / 0.000 | 64.500 / 64.500 | 55.206 / 55.206 | 1684.211 / 1682.246 |
| smoke_rain | main | 32.036 / 35.253 | 68102.859 / 68102.859 | unavailable / unavailable | 27.000 / 27.000 | 55.640 / 55.640 | 1731.352 / 1732.184 |
| smoke_rain | psx | 32.989 / 32.806 | 48378.984 / 48378.984 | 0.000 / 0.000 | 32.000 / 32.000 | 56.873 / 56.873 | 1688.383 / 1684.641 |
| smoke_rain | common_off | 38.460 / 37.115 | 48378.984 / 48378.984 | 0.000 / 0.000 | 32.000 / 32.000 | 56.873 / 56.873 | 1685.801 / 1686.285 |
| street | main | 181.573 / 239.122 | 61409.984 / 61409.984 | unavailable / unavailable | 32.000 / 32.000 | 40.695 / 40.695 | 1845.887 / 1842.184 |
| street | psx | 216.433 / 205.272 | 41686.109 / 41686.109 | 0.000 / 0.000 | 37.000 / 37.000 | 41.928 / 41.928 | 1848.684 / 1846.613 |
| street | common_off | 212.710 / 236.403 | 41686.109 / 41686.109 | 0.000 / 0.000 | 37.000 / 37.000 | 41.928 / 41.928 | 1849.137 / 1848.055 |
| tool | main | 26.120 / 24.708 | 55210.797 / 55210.797 | unavailable / unavailable | 26.000 / 26.000 | 55.645 / 55.645 | 1732.094 / 1731.453 |
| tool | psx | 28.101 / 24.477 | 45352.422 / 45352.422 | 0.000 / 0.000 | 28.000 / 28.000 | 56.933 / 56.933 | 1688.398 / 1684.012 |
| tool | common_off | 27.821 / 32.417 | 45352.422 / 45352.422 | 0.000 / 0.000 | 28.000 / 28.000 | 56.933 / 56.933 | 1685.574 / 1685.949 |

## Every repeated regression

| Reference | Candidate | Scene | Group | Metric | Forward delta ms / % | Reverse delta ms / % |
|---|---|---|---|---|---|---|
| main | psx | armed | warm_rotation | maximum_ms | +0.973 / +8.79 | +5.063 / +43.57 |
| main | psx | city_blocks | native_action_to_ready | maximum_ms | +3.397 / +5.17 | +3.385 / +5.34 |
| main | psx | dense_city | native_action_to_ready | maximum_ms | +4.108 / +5.40 | +6.130 / +7.72 |
| main | common_off | armed | warm_rotation | maximum_ms | +0.964 / +8.71 | +3.983 / +34.27 |
| main | common_off | creatures_20 | first_render | maximum_ms | +3.440 / +10.83 | +1.550 / +5.37 |
| main | common_off | smoke_rain | warm_rotation | maximum_ms | +0.721 / +5.67 | +7.596 / +65.78 |
| main | common_off | tool | warm_rotation | maximum_ms | +1.119 / +8.69 | +9.137 / +85.28 |
| main | common_off | tool | cold_rotation | p95_ms | +1.590 / +14.60 | +2.226 / +22.67 |
| main | common_off | tool | cold_rotation | p99_ms | +1.526 / +13.24 | +1.505 / +13.45 |
| main | common_off | tool | cold_rotation | maximum_ms | +4.299 / +35.79 | +0.746 / +6.02 |
| psx | common_off | armed | first_render | median_ms | +1.306 / +7.54 | +1.484 / +8.53 |
| psx | common_off | armed | first_render | p95_ms | +3.795 / +16.15 | +3.155 / +13.46 |
| psx | common_off | armed | cold_rotation | median_ms | +0.542 / +6.84 | +1.991 / +24.20 |
| psx | common_off | armed | cold_rotation | p95_ms | +2.747 / +31.52 | +2.805 / +26.96 |
| psx | common_off | armed | cold_rotation | p99_ms | +3.878 / +40.51 | +3.074 / +28.28 |
| psx | common_off | armed | cold_rotation | maximum_ms | +3.310 / +32.05 | +2.381 / +20.25 |
| psx | common_off | city_blocks | cold_rotation | p95_ms | +1.173 / +7.02 | +4.490 / +26.24 |
| psx | common_off | city_blocks | cold_rotation | p99_ms | +2.608 / +15.06 | +6.361 / +31.87 |
| psx | common_off | city_blocks | cold_rotation | maximum_ms | +1.989 / +9.88 | +16.631 / +80.91 |
| psx | common_off | creatures_100 | first_render | median_ms | +2.206 / +12.90 | +2.986 / +16.61 |
| psx | common_off | creatures_100 | native_action_to_ready | median_ms | +2.458 / +6.35 | +2.892 / +7.47 |
| psx | common_off | creatures_100 | warm_rotation | median_ms | +0.608 / +7.38 | +1.700 / +19.14 |
| psx | common_off | creatures_100 | warm_rotation | p99_ms | +0.777 / +6.93 | +1.372 / +8.52 |
| psx | common_off | creatures_100 | warm_rotation | maximum_ms | +1.883 / +16.52 | +1.309 / +7.82 |
| psx | common_off | creatures_100 | cold_rotation | median_ms | +0.636 / +7.34 | +1.030 / +10.89 |
| psx | common_off | dense_city | first_render | median_ms | +2.052 / +9.78 | +2.180 / +9.37 |
| psx | common_off | dense_city | first_render | p95_ms | +5.472 / +19.44 | +2.863 / +9.15 |
| psx | common_off | dense_city | warm_rotation | median_ms | +2.149 / +15.01 | +1.045 / +5.81 |
| psx | common_off | dense_city | warm_rotation | p95_ms | +3.699 / +19.49 | +2.947 / +13.16 |
| psx | common_off | forest | warm_rotation | p95_ms | +0.950 / +11.43 | +0.812 / +9.40 |
| psx | common_off | multi_level_horde | first_render | maximum_ms | +1.849 / +6.68 | +3.040 / +10.22 |
| psx | common_off | multi_level_horde | native_action_to_ready | maximum_ms | +14.110 / +25.27 | +4.905 / +7.94 |
| psx | common_off | smoke_rain | first_render | median_ms | +1.388 / +8.88 | +2.006 / +12.52 |
| psx | common_off | smoke_rain | first_render | p95_ms | +1.986 / +8.57 | +2.484 / +11.92 |
| psx | common_off | smoke_rain | native_action_to_ready | median_ms | +2.709 / +7.62 | +2.467 / +6.68 |
| psx | common_off | smoke_rain | native_action_to_ready | p95_ms | +2.814 / +5.24 | +3.874 / +7.55 |
| psx | common_off | smoke_rain | native_action_to_ready | p99_ms | +5.827 / +10.47 | +5.904 / +11.00 |
| psx | common_off | smoke_rain | native_action_to_ready | maximum_ms | +7.693 / +13.80 | +8.107 / +14.98 |
| psx | common_off | smoke_rain | warm_rotation | p95_ms | +0.605 / +5.47 | +2.695 / +22.06 |
| psx | common_off | smoke_rain | warm_rotation | p99_ms | +1.437 / +12.55 | +3.986 / +26.33 |
| psx | common_off | smoke_rain | warm_rotation | maximum_ms | +1.886 / +16.32 | +3.860 / +25.26 |
| psx | common_off | street | first_render | median_ms | +0.977 / +5.90 | +1.257 / +6.71 |
| psx | common_off | street | first_render | p95_ms | +3.413 / +15.21 | +2.858 / +10.95 |
| psx | common_off | street | cold_rotation | median_ms | +0.798 / +9.33 | +0.793 / +9.38 |
| psx | common_off | street | cold_rotation | p95_ms | +0.985 / +8.63 | +0.953 / +8.34 |
| psx | common_off | street | cold_rotation | p99_ms | +0.987 / +7.33 | +1.108 / +8.66 |
| psx | common_off | street | cold_rotation | maximum_ms | +7.402 / +54.46 | +1.982 / +15.03 |
| psx | common_off | tool | first_render | p95_ms | +1.607 / +7.10 | +5.463 / +23.19 |
| psx | common_off | tool | native_action_to_ready | p95_ms | +3.043 / +5.75 | +10.304 / +19.12 |
| psx | common_off | tool | native_action_to_ready | p99_ms | +2.750 / +5.12 | +18.320 / +33.40 |
| psx | common_off | tool | warm_rotation | maximum_ms | +0.926 / +7.08 | +8.959 / +82.25 |
| psx | common_off | tool | cold_rotation | median_ms | +0.527 / +6.45 | +1.438 / +18.47 |
| psx | common_off | tool | cold_rotation | p95_ms | +2.086 / +20.06 | +1.989 / +19.78 |
| psx | common_off | tool | cold_rotation | p99_ms | +2.236 / +20.68 | +2.281 / +21.89 |
| psx | common_off | tool | cold_rotation | maximum_ms | +5.443 / +50.09 | +2.710 / +25.97 |

## Exact sources and evidence

- main: source `8c5c32fbd58e707e058343b9fb54e246f4e90d4d`, Host `486fcecd53ee70eff98b4fac1c9a2f176345151d76e08f48e9abab64af31e600`, module `4abb14111eab6bc2f36425f0339570c9183bc357262bdcd072e0b35d26d26666`, installer `c9fd1eb6d4be2cc00e472657c120c4985de9d9430f033f3e6b3af1c3c7b822db`.
- psx: source `d6a6543d1235251de170b2cb66d3eab8e38fc98d`, Host `c960d5d569c0ec1bda3748de4d7036b71b8e9c0246e42e41cf8f85257c8de243`, module `ed9ea615b5f6e3170a74dbd4b64914e439f3c481587be67e62b4b2bd8096abd8`, installer `4cf1139c15bbf9feaae318e591d0d822bce6e1e6d88a9725297a69f52151088f`.
- common_off: source `79d058d742aae71cf2baadd1ec4ca211b9c7ea06`, Host `0c8903a4a75789dbde05cba252c0432b2803e630922a38e3a48f3903731f7f34`, module `c5a66dff1e24aaea33a7a157a570cd16d26a13a4cc19f437958ff005b0e39b5e`, installer `c085213080ea5d1fef978827376d19e8d441d48a3e72bacdbf1e610137ca2e27`.
- Frozen plan SHA256: `13be4bad89ef623dc04f75c4f4f98f8213344030cd8d79c718d010adb288a2d7`.
- Result SHA256: `5772267542a1660ee9b88c03fa1c2b553302d77bf64e7242b2f7ac7f65fcdda6`.
- Original rejected result SHA256: `9420424c62474d187935bc888925aedad28d8f14a7f1aea4cf918f3cc8d936f8`.
- All six raw movement reports were rehashed before rendering. Original arrays and verdicts are unchanged.
- Whole-frame clocks include post-present metrics/readback retirement. CPU wall is not GPU execution.
- The established daylight fixture retains DEBUG_NIGHTVISION. Controlled actions omit ordinary AI turns; no normal-game FPS claim.
- Coarse Windows CPU ticks do not prove idle CPU. No manual playthrough was performed.
