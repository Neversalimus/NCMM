# Private current-thread cycle diagnosis

The ordinary six-run core comparison remains diagnostic, with 4224 first-render and 12672 camera observations. PSX versus Main has three repeated maximum regressions; Common with all three new features disabled versus PSX has 45 repeated metric regressions. The original D6 standalone rejection (29 repeated regressions) remains intact. These counts refer to metrics, not independent incidents; no performance acceptance or merge follows from this report.

The identical Common binary changed by >5% and >0.1 ms in 31 of 44 group medians between its two processes. This establishes repeat variability, not its cause. The next private build measures CPU work without changing resource retirement, rendering, scene quality, public ABI or SDL dependencies.

During the disposable movement benchmark only, a scoped collector records raw current-thread cycles around Native 2D, perspective, Present plus post-present metrics, world extraction, visibility, the module callback, the whole measured frame, and the native move-to-ready interval. The original whole-frame clocks still include fence/readback polling. Nested phase counts are not added together. Outside the benchmark frame, the collector is inactive.

The [Microsoft API contract](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime) forbids conversion of these cycles to elapsed time. The report preserves raw cycles and explicit availability. Unsupported or backward readings remain unavailable; zero is not used as evidence of idle CPU. This is not a GPU execution timer, a scheduler trace or a replacement acceptance run. Instrumented diagnostic timings cannot qualify a final unchanged release binary.

Local MSVC 19.38 tests exercise unavailable/backward/zero/unsigned-boundary readings, disabled collection, actual Windows current-thread access, single phase retirement and frame reset. Atomic source-transform tests retain their existing coverage. Actual new Native binaries and their CI results are still pending at this commit.

The generated companion `ordinary-core-79d058d.md` keeps every median/p95/p99/maximum, resource count and repeated regression from all six ordinary controls. Raw reports remain in the user's local `C:/workspace/fpv013-core-79d-*` evidence directories; they are not claimed to be downloadable CI artifacts. Original installations and raw files are preserved.

- Ordinary plan SHA256: `13be4bad89ef623dc04f75c4f4f98f8213344030cd8d79c718d010adb288a2d7`.
- Ordinary result SHA256: `5772267542a1660ee9b88c03fa1c2b553302d77bf64e7242b2f7ac7f65fcdda6`.
- Original local generated report SHA256: `fbba899f95540e8745b10524e69144e1b5ab93fa7b023095f0643c85ef03c1a6`.
- This branch is private diagnostic work. Existing Draft PR heads and certified feeds are unchanged.
- Repository companion report SHA256 (canonical LF): `b69f125e615372888faab55082cf1bb32c18b3c8accdf0ea83b1cf70126fcbf5`.
