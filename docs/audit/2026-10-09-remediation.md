# Engineering audit remediation — 2026-10-09

Baseline: `4a83414a1bf4147860b0e3dd49a9aded0c5581d0`.
This change set addresses A01–A15 and the implementation recommendations in R01–R10.
It is not a claim that all possible defects have been eliminated.

| Audit IDs | Implementation / verification |
|---|---|
| A01–A02 | One migration and runtime-fault gate for v1/v2 callbacks. Extracted production callback bodies exercise failure, recovery, suspension, and complete effect cleanup. |
| A03 | Complete snapshot inventory and digest verification before any rollback replacement; replacements staged, originals displaced into the transaction. Damaged-snapshot installation test. |
| A04 | Refuse ambiguous bootstrap identity, preserve original executable and vanilla backup. Lost-receipt upgrade regression. |
| A05 | Data-only disabled module keeps its manifest and persistent definitions. Only the installer's own disabled marker is removed on re-enable. |
| A06 | Shared no-reparse path/tree guard before managed writes. Junction fixture checks rejection and preservation of outside files. |
| A07 | Retire an old directory's executable when the same module ID moves. Explicit rename regression. |
| A08–A09 | Shared exact/prefix source identity rules; actual file hashing, not timestamp-based cache trust. Actual Bootstrap validation test changes bytes without changing size/timestamp. |
| A10 | Checked XP arithmetic, saturated counters, bounded normalization/work. Actual release module boundary tests + semantic catalog + UBSan. |
| A11–A13 | Effective selected/installed dependency resolution, atomic dependency closure, immutable future state rejection. Legacy partial apply is rejected BEFORE mutation, not silently broadened. Native Setup remains the selected-component executor. |
| A14 | Allocate numeric columns first, shorten names rather than values. Extracted actual readout tested in RU/EN across widths 33–100 and signed int limits. |
| A15 / R05 | Piecewise adaptive integration with endpoint atoms and a hard operation budget. Unsupported/ill-conditioned distributions display `--`, never fabricated percentages. |
| R01 | Same-item assignment is idempotent; selected item re-resolved by stable UID after carrier mutation. Real engine/UI lifecycle still requires the dedicated gameplay scenario. |
| R02–R03 | Shared physical per-installation lock, durable EXE+binding journal, recovery for every phase. Hard-abort/first-install/reinstall/corrupt-snapshot fixtures. |
| R04 | Core struct-size/ABI guard before AWS/Survivor field access; actual module-init rejection tests. |
| R06 | Required external ZIP digest, local validator before package execution, no downloaded-validator dot-source. |
| R07 | Refuse vanilla fallback/restore with persistent native item definitions. No migration or deletion of player items is attempted. |
| R08 | Publish requires exact source SHA's real installation run, with successful launch/gameplay/restore steps, not merely a green feed. |
| R09–R10 | Recipe includes workflow/toolchain lock; provenance records source and runner. Immutable build-scoped runtime releases, no asset clobber, official pinned artifact attestation for new publications. |

## Operational behavior

An active game, Setup, Host update, or recovery holds `.ncmm-install.lock`.
A second cooperating operation refuses without modifying the installation.
Do not delete this lock file; its presence is normal. The operating system releases
its open handle on process exit, so the file itself is not a stale-lock marker.

Host updates retain `.ncmm-host-tx-*` recovery archives. A pending transaction is
recovered before a Host is accepted. A corrupted recovery snapshot causes a clear
refusal and preserves live files; it is not silently deleted. Old setup snapshots
without a verifiable inventory are also refused rather than guessed at.

With save-critical native definitions installed, vanilla fallback is blocked even
if a DLL is disabled or missing. Disable optional modules through Setup; do not
use vanilla restore as a way to load and re-save an NCMM world. A real save round
trip with populated Mana Hands/Dimensional Pouch remains a release-validation item.

## Validation status

Local: extracted production lifecycle/readout/probability tests; real Survivor/AWS
module smokes; integer boundary cases and UBSan. Windows C# crash/installation,
PowerShell regression, and genuine CDDA Host compilation must pass on this exact
change set before release. Do not substitute the baseline audit run for these results.

Existing unrelated technical debt (large cumulative payload and large integration
files) is not magically removed by a bug-fix pass. Canonical overrides are explicitly
tracked and run after cumulative transforms, and clean-package/source identities
are checked on every gate.
