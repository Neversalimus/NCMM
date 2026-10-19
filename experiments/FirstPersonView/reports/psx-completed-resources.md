# Completed GPU resource retirement — qualification candidate

The same-binary f0a7c94 RTX experiment found no repeated >5% and >0.1 ms
regressions for submit-only retirement in its 4,224 first frames and 12,672 camera
observations. Legacy retirement from metrics queries still had eleven. Smoke
warm medians changed from 15.723/15.840 to 7.205/6.947 ms. City retained repeats
were noisy (14.970/9.087 versus 8.576/8.269); all tails and failed modes remain
preserved. This is evidence for a candidate, not standalone PSX acceptance.

Apply only that resource policy to the original PSX renderer with the same
already-required SDL descriptor correctness patch. An asynchronous readback
keeps at most one owned already-signalled fence; the next submit uses it to
retire completed resources before allocations, then releases it. Metrics queries
still advance counters, without performing retirement outside the frame. Clear
and destroyed-device abandonment handle the retained fence with other children.
No per-frame GPU-idle call, shader/quality change or public ABI change is added.

The frame timer includes its post-present metrics collection. New private
recording-interval counters are separate from GPU execution. GPU scene tests
require metrics to advance with zero initial retirements, then one at the next
submit, alongside queued-pose/cell pixels and lifecycle checks.

This private branch must pass an independent forward/reverse Main comparison
with the same corrected frame-clock boundary and SDK before changing PR #218.
The original failed PSX/Main matrix remains rejected; 15 ms is not established.
