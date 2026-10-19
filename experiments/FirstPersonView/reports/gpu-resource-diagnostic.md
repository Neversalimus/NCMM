# Private resource-retirement experiment

This branch is diagnostic and is not an accepted performance change. Its default
path retains the f157 staging behavior. The benchmark can select `fresh` transfer
objects or retirement through an already-signalled fence (`early_cleanup`). The
wrapper isolates/restores the environment and records the selection. There is no
per-frame GPU-idle call, quality reduction or public ABI change.

The ten predeclared f157 RTX runs passed structural checks but failed performance:
7,040 first frames and 21,120 camera frames, 21 repeated component metrics and 67
net metrics against main. Combined city warm medians were 15.33975/15.40005 ms
versus PSX baseline 12.7929/13.2027 ms. The dependency also failed its main gate.
No merge or 15 ms acceptance follows from functional CI.

Sixteen exact-PDB city-warm point stacks caught internal D3D12 buffer/texture
allocations, including cycling, uniform buffers and CPU synchronization. They do
not measure execution percentages or prove a Present/DWM cause. Three private
six-run synthetic experiments did not reproduce the Native stall; their full
results, images, source/binary snapshots and hashes remain outside the diff.
The two variants require a same-binary Native comparison before any proposed fix.
