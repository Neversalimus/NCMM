# NCMM licensing audit — 2026-10-10

## Scope and evidence

Review of the canonical `Neversalimus/NCMM` repository, its production certified-Host, Runtime, Full and standalone-module packaging recipes, plus the upstream CDDA `LICENSE.txt` and the [CC BY-SA 3.0 Unported legal code](https://creativecommons.org/licenses/by-sa/3.0/legalcode.en).

Relevant sources: `ci/Build-HostPackage.ps1`, `ci/Build-Runtime.ps1`,
`ci/Test-NativeReleaseLayout.ps1`, `.github/workflows/ncmm-host.yml`,
`.github/workflows/ncmm-runtime.yml`, `ci/Build-UpdateRelease.ps1`,
`compat/package-files.txt`, `host_patch/Apply-NCMMHostPatch.ps1`,
`host_patch/ncmm_loader.cpp`, `ARCHITECTURE.md`, and `README.md`.

This is a repository-level licensing and release-path audit, **not** legal advice,
a binary-level software composition analysis, or a legal opinion on the
derivative-work status of individually authored native modules.

## Findings

| ID | Severity | Finding | Disposition |
| --- | --- | --- | --- |
| L01 | High | Certified Host ZIP was packaged only with `cataclysm-tiles.ncmm.exe` and `host.json`, without upstream copyright/license notices. | Remediated in this PR: copy exact upstream `LICENSE.txt` as `CDDA_LICENSE.txt` plus NCMM attribution notice into the Host ZIP; fail closed on absent/invalid upstream license. |
| L02 | High | The GitHub Host feed also distributes a **raw EXE** directly, separately from the ZIP. | Remediated in this PR: publish both notice files as individually accessible assets of the same immutable Host release and describe the licensed upstream work in release notes. This does not retroactively update historical releases. |
| L03 | Medium | Runtime, Full and standalone native-module packages had no consistent third-party notice; archive tests did not check notices. | Remediated in this PR: include `THIRD_PARTY_NOTICES.txt` in each package and verify actual ZIP contents. |
| L04 | Medium | The repository has no project-wide explicit `LICENSE`/`COPYING` setting clear terms for **original** NCMM code. | Open policy decision. Do not apply CDDA's ShareAlike license to every independently authored file without determining ownership and intended publication terms. |
| L05 | Medium | No verified binary-level dependency/notice inventory for the MSVC/vcpkg-built Host and any redistributable resources. | Open. Generate a dependency SBOM from real output and reconcile with exact upstream `LICENSE.txt` and dependency licenses; verify any required runtime redistribution notices. |
| L06 | Medium | Existing historical downloads may lack bundled notices; immutable releases prevent silently replacing existing binaries. | Open release-maintenance work: publish supplemental notices or add a prominent link to each affected historical release without mutating existing asset hashes. |
| L07 | Low | No explicit note distinguished our modifications from official CDDA or explained non-endorsement. | Remediated in `THIRD_PARTY_NOTICES.txt`. |

## License analysis and boundaries

- CDDA's primary `LICENSE.txt` states Creative Commons Attribution-ShareAlike 3.0 Unported; its other included components can carry different licenses.
- The **modified CDDA executable** is a CDDA adaptation. Its distribution must satisfy the applicable attribution, modification-indication, ShareAlike, license-URI and notice-preservation conditions.
- CC BY-SA 3.0 is perpetual **subject to compliance**. Section 7 provides automatic termination upon a violation; it does not allow upstream maintainers to revoke rights arbitrarily merely because a project uses LLMs.
- NCMM's GitHub organization, repository administration and release signing/publishing remain independent of CDDA's Code of Conduct. Upstream maintainers can choose their own contribution policy but cannot take NCMM repository permissions through that policy.
- Native modules that only use a separately published ABI are **not automatically proven** to be CDDA derivative works; neither is their independent legal status guaranteed by ABI separation alone. Examine copied implementation, linked code, bundled assets and actual distribution per module.
- Adding a new blanket project license without express authorization from the rightsholder(s) could waive or grant rights in original NCMM code unintentionally. This audit intentionally does not do so.
- The official upstream `LICENSE.txt` may change across tags. The certified Host packaging must use the file from the **exact source commit** used to produce each binary.

## Evidence limits and follow-ups

1. Rebuild and inspect a representative certified Host ZIP on Windows, proving that `CDDA_LICENSE.txt` matches `<upstream-tag>/LICENSE.txt` byte-for-byte, and verify that the two notice assets accompany the standalone EXE release.
2. Run Runtime CI and `ci/Test-NativeReleaseLayout.ps1` against real newly generated ZIPs.
3. Audit actual executable dependencies (imports, static linking, fonts/data, DLLs and redistributable runtime) and create a source/release SBOM. Merely copying upstream `LICENSE.txt` is not proof that every dependency obligation is satisfied.
4. Review each third-party asset in bundled modules for provenance, especially graphics and any upstream-derived code. This audit checked the declared packaging paths, **not** each asset's origin.
5. Decide whether and how to license original NCMM sources separately; involve qualified counsel if a formal legal assurance is needed.
6. Consider a separate historical-release notice remediation plan that preserves hashes and provenance of published binaries.

## CI change impact

`ci/Build-HostPackage.ps1` and `.github/workflows/ncmm-host.yml` are in the Host patch-revision fingerprint. Modifying the distribution recipe therefore legitimately requires **fresh exact-build certification** and publication under a new immutable revision. This PR does not overwrite existing certified Host assets and should not be merged or released as green until CI confirms the new pipeline.
