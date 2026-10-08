# CDDA / NCMM performance regression runbook

**Scope:** reproducible performance evidence. The fast DLL semantic smoke is not a proxy for gameplay TPS.

## Automated release gate

`tests/smoke_host.cpp` exercises `ncmm_test_recalculate_v1` over the full Survivor perk catalog, 64 successive recalculations. A 5,000 ms wall-clock upper bound is a **coarse regression guard** on the CI machine, not a player-facing frame-time target. The test emits the observed duration in Runtime CI. Do not compare durations from unlike runners.

The existing Real Installation Matrix runs an exact certified Host with AWS worldgen and Survivor gameplay and restores vanilla. It verifies functionality, not sustained FPS/TPS.

## Comparative in-game measurements

Capture the exact CDDA commit, NCMM commit + Host patch revision, active modules and content mods, CPU and platform, save copy and scenario. Use the same save snapshot and isolated userdirs for both variants. Each scenario should be run in vanilla (with compatible content where possible), NCMM Host with no native module, and full NCMM; document anything impossible to run without a mod.

Recommended measurements: (1) 500-1000 movement turns in an unchanged area, recording median/p95 turn latency and stutters; (2) inventory opening and 100 selection changes with a large inventory and active Mana Hands; (3) 100 aiming updates and 20 burst fires; (4) 25 successful crafts and activity cancellations; (5) 10 new submap generations, reporting p95 and longest pause; (6) Survivor tree open / Prime purchase / respec timing. Warm up before collecting data; repeat each measurement at least three times and report median. Compare with a separate base-game run of the same scenario.

## Release policy

- Treat repeatable crashes, save corruption, missing equipment or inventory loss as release blockers regardless of measured speed.
- For gameplay latency, record percentage changes relative to the **same** CDDA baseline. Flag a >10% repeatable median slowdown or >20% p95 slowdown for investigation rather than silently accepting it. The thresholds are engineering review triggers, not proven user-visible limits.
- Identify and log unexpected spikes when traversing new map chunks or querying virtual items. Never claim a TPS improvement without a benchmark.
- Preserve the Legacy Survivor UI when optimizing the existing tree; benchmark data first, then optimize proven hotspots.

## Canonical source and release reproducibility

The cumulative Survivor payload preserves canonical copies of the bootstrap, setup core and smoke Host. To refresh them after source changes, run:

```powershell
.\ci\Sync-CanonicalPayload.ps1 -RepositoryRoot .
.\ci\Regenerate-PackageIntegrity.ps1 -PackageRoot .
.\ci\Sync-CanonicalPayload.ps1 -RepositoryRoot . -Check
.\ci\Regenerate-PackageIntegrity.ps1 -PackageRoot . -Check
```

Run Source/Package Audit, Runtime semantic smoke and Real Installation Matrix before publishing. The updater should never regenerate canonical snapshots implicitly while verifying them.
