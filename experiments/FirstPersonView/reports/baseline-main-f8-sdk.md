# Main dependency control with the shared SDK

This diagnostic branch starts at exact main
`f8affe5570b632178eb3c9ad9d103c40701e0eb8` (First Person View 0.12.9-dev).
Production module, renderer, shaders, assets, public SDK and canonical Host patch
remain unchanged. It adds the same private SDL 3.4.10 graphics descriptor fix
used by all component modes, including its strict boundary/pixel test and source
provenance. This prerequisite does not accept the PSX performance dependency.

The existing Native movement harness and packaged wrapper are based on
`2bb3ec5a10ce2e22b11b537ec2fd5782518ee9f2` to expose the same eleven full-size GPU
scenes, sample counts, actual window/display scale, backend and VSync telemetry.
The 0.12.9 adaptation collects its existing total uploads, draws, memory,
process CPU and exclusive frame phases. Later PSX-specific thread/readback,
staging/cell, bone and static/dynamic upload counters are explicitly unavailable;
they are omitted and listed, never fabricated as zero. The eleven scenes,
Native actions, sample counts, resolution, weather, camera, actual window,
VSync and complete-frame timers are preserved. The wrapper is byte-identical.
These are diagnostic-only changes. The original renderer is not replaced by a
newer DLL; the package builds its matched Host/module from this branch.

Two additional ordinary main controls will bracket the eight component-mode
runs. Their complete-frame results must be retained separately from the
four-mode comparison against the PSX base. They determine the net cost of the
dependency plus the new combination. Main, PR #218 and published feeds are not
modified, and this reference branch has no PR.

No measurements or final qualification are declared here. Exact package
identities, source equivalence, shared SDK and all hardware conditions must be
verified before using the controls.
