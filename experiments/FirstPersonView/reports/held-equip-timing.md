# Record the actual first render and pose capture time per held model

The Native CPU/GPU gallery records a complete first render/presentation after
each actual equip change, its initial static build/reuse and GPU upload/resource
counters, and two subsequent settled render samples. The 280 ms equip settling
delay is reported separately and is never presented as renderer execution time.
The capture records its actual presentation clock and elapsed time after equip.
No attack, reload or Native simulation event is synthesized for the capture.

These are controlled gallery diagnostics. Some item bodies are already resident
when testing accessories; initial static-build counters distinguish those from
new GPU resources. This does not claim every equip begins in an empty process,
or replace the separate ordinary four-mode full-frame measurements. The strict
current verifier requires finite complete timing, counters and the declared
settling interval. Historical evidence may be inspected explicitly without
timing, but reports that timing was not verified. Release CI uses the strict
default.
