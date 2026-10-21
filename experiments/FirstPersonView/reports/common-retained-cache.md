# Common sparse signatures and completed-resource cleanup

The earlier Common candidate B19 retained 45 repeated timing regressions. This
change ports the separately measured Core169 runtime into Common while preserving
camera ownership, held meshes/transparency, target health, trace markers and all
Windows/Linux qualification gates. The original negative evidence remains valid
for B19; the new Common source is not yet accepted for Native performance or merge.

Owned sparse pages retain exact normalized cell contents by absolute XYZ. Complete
byte/flag/metadata comparisons reuse the original hash; geometry, neighbours,
visibility and rendering quality retain their previous rules. The cache has a
32 MiB capacity budget and 512 KiB per-call update/allocation bounds. Missing or
uncacheable entries use the original hash path. Producer capacities are now
exported separately from Host estimates, RSS and logical GPU bytes.

The existing guarded cleanup now runs after CPU batching/picking. One owned,
already-signalled fence remains until lifecycle reset or error, permitting the
same single Query-and-Wait callsite to reclaim other completed SDK submissions
without needing a newly completed visibility readback. No unfinished GPU wait,
new idle, prewarm, shader, SDK or public ABI change is introduced. Visibility
generation/serial validation and asynchronous readbacks remain independent.

Four source files have a byte-exact inverse merge proof preserving Common's
feature additions. The Native harness changes only three producer-stream lines;
its existing radius/Z/trace metadata remain without duplicate JSON members.
Focused sanitizers retain snapshot-cache coverage and add scene-signature
coverage. Only the total Windows job timeout grows from 270 to 360 minutes;
individual step deadlines, actual Linux gates and all existing checks remain.

All 41 registered local CTests pass, including actual SDL_GPU scenes/resource
ownership, queued poses, held assets/runtime, camera snapshots, target HUD and
eight library lifecycle cycles. The unchanged private SDK DLL was used. All
17 atomic-applier and nine qualification-policy tests also pass. These are local
functional checks and do not substitute for actual Common Native execution.

Core169's independent four ordinary runs passed the frozen gate: zero repeated
regressions and 128 repeated improvements across all 11 scenes and four groups,
with 2,816 first-render and 8,448 camera observations. Candidate first-render
medians were 9.4702–15.0035 ms; 1,277/1,408 samples were <=15 ms, with a 47.1147 ms
maximum. All 2,112 candidate warm samples made one cleanup call. Seventeen
single-direction negatives remain, including reverse street cold maximum
27.4492 ms versus Main17.5805 ms. These exact Core measurements do not measure
the new Common build or waive its fresh complete matrix and final hardware load.

The JSON manifest binds sources, local logs, the full Core result/report and
the earlier Common rejection by SHA256. Large raw arrays/captures remain in the
local evidence archive. Full CI, exact final installers, the unchanged-binary
1800-second GPU-debug/lifecycle load and final forward/reverse comparisons remain
required. Manual combat and physical high-DPI checks remain unverified.
