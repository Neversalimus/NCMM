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
The six 4b980fe same-binary Native runs completed: 4,224 first frames and 12,672
camera observations. Fresh transfer objects had 41 repeated regressed metrics;
`early_cleanup` had five (armed first median/max, city cold p99/max, dense-city
cold p95). Neither is accepted. Early city warm medians were 7.917/8.410 ms
versus retained 15.463/14.987; smoke warm 6.933/6.912 versus 13.578/13.858.
All samples and gate verdicts are retained outside the diff.

There is also a timer caveat: `metrics()` polls fences, so legacy early cleanup
can retire resources after the rendered-frame clock stopped. The outer rotation
and action-to-ready clocks already include that call; the first-render clock
did not. This does not invalidate all outer-clock gains, and it does prevent
accepting the first-render budget from those measurements.

The new `submit_cleanup` experiment holds at most one owned already-signalled
fence while readback metrics advance. Only the next submit retires completed
resources, once, before allocations. Clear releases that fence; a destroyed
borrowed device abandons its stale address. It never waits for unfinished GPU
work. The Native harness now collects its post-present metrics before stopping
frame/CPU clocks, and records that collection and private retirement call/time
streams. These nested timers are not GPU execution and must not be added.
Actual GPU tests require asynchronous metrics to advance without hidden
retirement, then exactly one retirement at the next submit. A new same-binary
Native forward/reverse comparison is required; this remains a private experiment.
