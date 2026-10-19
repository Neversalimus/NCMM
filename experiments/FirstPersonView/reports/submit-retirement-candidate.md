# Completed-resource retirement in the combined candidate

The private f0a7c940f942f0a28d58d48c439bb1fd47be9dfe experiment measured six
ordinary RTX 4090 / D3D12 Native runs, actual VSync 1, with the complete frame
clock including post-present metrics. Submit-only retirement had no repeated
regressions and 48 improvements across first render, action-to-ready, warm and
cold rotation, and median/p95/p99/max. Legacy retirement from metrics queries
had eleven repeated regressions. The threshold is >5% AND >0.1 ms in both
directions; these are metric counts, not independent incidents.

Smoke warm medians changed from 15.723/15.840 to 7.205/6.947 ms. City retained
repeats were noisy (14.970/9.087 versus 8.576/8.269); a uniform gain is
unsupported. Dense-city first-frame medians remain about 20–22 ms. The result
SHA256 is 37e63eb37e38922cfa2b6cab3c07d9e72f357c0a786e31bb95455867d21e737b.
Raw arrays, tails, all failed policies and the earlier rejected matrices remain
preserved outside the repository. This experiment selects a candidate policy;
it does not establish standalone PSX or final combined acceptance or 15 ms.

Apply the same policy as standalone PSX candidate
f22a8e7c076f2db6941a84d9fd40080dc8358eb7: keep at most one owned already-signalled
readback fence, then retire completed resources before the next submission's
allocations and release the fence. Metrics queries advance asynchronous counters
without retirement. Clear releases the retained fence; destroyed-device
abandonment drops its address with the other borrowed-device children. The
query must still confirm completion. There is no wait for unfinished GPU work,
per-frame GPU-idle call, experimental selector, shader/quality change or public
ABI change. Existing profiling markers and all four component implementations
remain intact.

The Native benchmark includes post-present metrics before stopping frame,
thread and process CPU clocks. Private retirement duration/call and metrics
duration streams describe recording intervals, never GPU execution. Real GPU
tests require zero retirement while repeatedly reading metrics, then one at
the next submission, alongside queued pose/cell pixels and lifecycle checks.
The exact source-owned installer payload contract replaces the obsolete fixed
32-file gate; unexpected, missing and undeclared archive members still fail.

This branch is an isolated build candidate. The independent four-run comparison
of unchanged production Main (inclusive-clock harness 8c5c32f) and standalone
PSX f22a8e7 is pending. Public Main, PR #218 and the four component PR heads are
unchanged. Standalone acceptance, the new combined/component/Main matrix, final
CI, both Native pins and the required hardware lifecycle checks remain gates.
