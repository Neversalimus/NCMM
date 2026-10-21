# Curated PSX frame-time runtime

This branch selects the measured net runtime from Core169 onto the verified
merge of PR218 with current Main. Main's two feed updates remain byte-identical.
Forty-one runtime/build/test/SDK files are copied by exact SHA256; the complete
remaining code and asset tree matches the selected source. Nineteen experimental
reports and their commit history are excluded. This curation creates new binary
identities, so the prior Native packages are evidence, not final qualification.

The selected runtime bounds Native definition-cache collisions, preserves
submap/transparent-field invalidation, reuses snapshot vector capacity, shares
exact immutable index data and retains normalized static cell signatures by
absolute XYZ. Sparse records use a 32 MiB budget and 512 KiB per-call limits;
uncached cases use the original hash/geometry rules. One owned already-completed
fence lets the existing guarded SDK cleanup reclaim other completed submissions
after CPU batching/picking. Lifecycle/error release and visibility serial checks
remain. No ordinary unfinished GPU wait, idle, prewarm, shader/ABI or quality
reduction is introduced. The complete Native tile pass remains.

The original production private SDL descriptor correction, its pinned source,
license, exact manifest and boundary/pixel regression test are included. This is
the same SDK used by both ordinary controls; diagnostic SDK observers and the
rejected cache bypass are excluded. Full CI retains all checks and individual
step deadlines, with only the total Windows job budget extended to 360 minutes.

The identical selected source passed all 33 local CTests, including real SDL_GPU
fence/resource ownership and queued-pose checks. Those exact-source local results
are explicitly reused; no fresh local run is claimed for this file curation.
Full CI will rebuild and rerun the exact curated source.

Core169's separate four ordinary runs passed the frozen standalone gate with
zero repeated regressions and 128 repeated improvements, across all 11 scenes,
four groups, 2,816 first-render and 8,448 camera observations. Candidate first-render
medians were 9.4702–15.0035 ms; 1277/1408 samples were <=15 ms, with a 47.1147 ms maximum.
All 2112 candidate warm frames recorded one cleanup. Seventeen single-direction
negatives remain, including reverse street cold maximum 27.4492 ms versus
Main 17.5805 ms. All earlier negative results and complete samples remain retained;
the fresh final matrix must investigate recurrence rather than discard tails.

These are controlled stress render/action measurements, with ordinary AI and OS
input dispatch omitted. They establish no all-frame15ms guarantee or normal-game
FPS. CPU wall, GPU execution and Present remain distinct. Fresh exact curated
Native source/field checks, final Main/component comparisons, Full CI and an
unchanged-installer GPU-debug/lifecycle load of at least 1800 seconds remain
mandatory. Final Common additionally requires actual Linux Native on 1040/1807.
Manual combat and physical high-DPI remain unverified. No merge is accepted yet.
