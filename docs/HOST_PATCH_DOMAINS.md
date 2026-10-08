# Host patch stack: domain contract

The canonical execution recipe is ci/host-patch-stack.json. It contains 40 ordered
PowerShell transformation functions. This order is important: different transforms
sometimes edit the same CDDA source function and the next transform consumes the
output of a previous one.

ci/host-patch-domains.json adds a *read-only* architectural map of that stack.
It partitions all 40 existing layers into nine domains, with explicit 'after'
dependencies and short descriptions:

1. worldgen-settings: settings, data bridge and AWS geography.
2. runtime-hooks: generic gameplay runtime hooks and reactive mechanics.
3. virtual-item-foundation: Mana Hands identity, logical slots and item context.
4. vehicle-xp-bridge: vehicle/crafting XP prerequisite.
5. virtual-item-utilities: spellcasting, lifecycle, utility, secondary melee.
6. virtual-item-actions: paired grip, ranged/primary melee and action integrations.
7. progression-completion: completion telemetry, reconciled count and direct UI.
8. source-invariants: post-transform source assertions.
9. diagnostics-runtime: recipe profiler and final infrastructure.

ci/HostPatchDomains.ps1 checks exact one-to-one coverage and ordering against
the canonical stack. It rejects duplicate/missing/unknown layers, duplicate IDs,
cyclic or forward dependencies and untracked domain members.
ci/Test-HostPatchDomains.ps1 locks the current inventory and runs 12 negative
mutation fixtures on Windows PowerShell 5.1. The Source/Package Audit and Runtime
CI both require it.

## Rules for future Host extensions

- Reuse the existing queried Core API or registered hook/settings domain first.
- If a genuinely new CDDA-facing source transform is necessary, assign it to a
  named domain and explicitly list prerequisites; add source and behavior smoke.
- Append or insert the layer in ci/host-patch-stack.json only after checking
  interactions with all previous transforms.
- Update ci/host-patch-domains.json in the same PR; CI must reject unclassified
  changes before any release.
- A real modification to the canonical stack changes the certified Host patch
  revision. Re-certify and run Real Installation Matrix before publishing.

This contract does NOT yet change how transforms execute: Build-HostPackage.ps1
continues using the exact existing flat canonical list. Grouping is a structural
guard, not runtime feature gating, and does not alter Loader ABI, gameplay or
the current certified patch revision.
