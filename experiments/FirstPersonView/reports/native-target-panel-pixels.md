# Native aiming panel and the firing pixel fixture

The actual Linux 1040 run [39201623140](https://github.com/Neversalimus/NCMM/actions/runs/39201623140) at source `25c21a4d3b66179a084b0a4ac1f7f55bba2ea35f` completed controls, camera, held models, target HUD and gameplay with exit code zero. Its subsequent firing-pixel verifier failed at frame 967 with a 0.625 reference match. The full run remains failed; this recheck does not replace CI qualification.

The preserved 640 × 384 PNG shows the first-person world through a foreground window. The native aiming panel begins at x=203 and covers some of the old grid's selected points. The corresponding pre-input frame 964 and the deliberately reproduced legacy map frame were also inspected. This was a comparison of world pixels with legitimate native UI pixels.

The fixture now uses a fixed, denser grid inside the left 30% of the image. Its minimum 12 discriminators, color distances, 75% match requirement, per-trial reference, structural checks and requirement to examine every presented frame are unchanged. No point is selected from the failing frame, no frame is skipped, and no production renderer, input, camera, model, shader or SDK code changes.

The complete artifact `12166200659` was downloaded and its whole SHA256 verified as `64418b05eeb1f5ca3617e65a917dfe26dac4a343f0f2bc0ab821bf660e5ed4cf`. Every firing PNG's member CRC, PNG structure and SHA256 was checked. Rechecking all 192 actual native attempts and 132 confirmed shots examined 2,957 PNGs, including all 2,952 non-control frames. Their minimum world reference match was 1.0 with the corrected grid. Raw data and the detailed member manifest remain outside the repository.

The 27 pixel and structural unit tests pass, including three added cases: the native aiming panel over a foreground window is accepted, while an actual map or blank world beneath the same panel is rejected. These synthetic tests are distinct from the actual Linux captures. A fresh CI run with this verifier is still required. Linux uses Mesa software Vulkan and establishes functional behavior only; it provides no RTX frame-time or manual-play claim.
