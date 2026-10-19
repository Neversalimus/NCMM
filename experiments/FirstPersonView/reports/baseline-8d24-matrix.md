# Comparable baseline matrix

This diagnostic branch starts at the qualified PSX source
`8d24cc2b8df4ccd6574e1f2f61108baf24f4bcf9`. Its only code changes are the
movement benchmark fixture and its process wrapper. The production renderer,
module, shaders, caches, ABI and game patch sources retain the original bytes.
The resulting Host has a new binary/source identity because it includes the
changed fixture; it is not labeled an unchanged 8d24 installer.

`-MovementBenchmark -Extended -FullSizeGpuMatrix` selects eleven GPU scenes at
1768x1084: street, city blocks, dense city, forest, multi-level horde, 20 and 100
creatures, smoke/rain, held crowbar, held SCAR and empty-hand attacks. The same
fixture is used by the new combination. The SCAR replaces the baseline's M27
only inside this explicitly selected matrix, giving the armed comparison the
same actual Native item. Ordinary and existing 80-case CI matrices are retained.

Each run supplies 704 actual Native steps and 2112 camera-only samples, plus
walking-easing frames and separate cold-start resource costs. Simulation turns
and normal AI are not advanced in this controlled movement fixture. Native
firing and ordinary combat are separate validations. The wrapper records real
SDL version, VSync state, physical window dimensions/display scale, viewport,
internal resolution, backend, software status and ordinary process provenance.
Full-size mode rejects GPU fallback or software-device results. It also rejects
GPU-debug mode; profiler tools must be detached for ordinary comparisons.

Forward/reverse repetitions, the original qualified 8d24 measurements and the
new four-mode component comparison remain required before performance acceptance.
This branch produces no additional PR or release publication.
