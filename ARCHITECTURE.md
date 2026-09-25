# NCMM v0.7.1 architecture

```text
Any launcher / manual shortcut
             |
             v
    cataclysm-tiles.exe
       [NCMM Bootstrap]
             |
   SHA256(vanilla exe) + VERSION.txt commit
             |
       local binding valid?
       /              \
     yes               no
      |                 |
      |          HTTPS certified feed
      |                 |
      |            exact SHA entry?
      |             /         \
      |           yes          no/error
      |            |              |
      |        download host       |
      |        verify SHA256       |
      |            |              |
      +------------+              |
             |                    |
             v                    v
  cataclysm-tiles.ncmm.exe   vanilla executable
       [NCMM Host]
             |
      Host API v1/capabilities
             |
   Module Contract v1 preflight
   mod.json <-> DLL descriptor
             |
         code_mods/*
             |
     AdvancedWorldSettings

Runtime writes ncmm/runtime.state.json.
Host writes ncmm/modules.state.json.
```

## Trust boundaries

1. **Bootstrap** owns executable selection and fallback. It does not inspect CDDA internals.
2. **Certified host** is built from the exact upstream source tag and is bound to exact official vanilla executable SHA values.
3. **Host API** is a narrow C ABI. Mods do not receive STL types or raw CDDA object pointers.
4. **Code mods** are manifest-preflighted before `LoadLibrary`, then descriptor-cross-checked and independently disabled on contract failure.
5. **Runtime diagnostics** persist machine-readable bootstrap/host/module state without weakening fail-closed behavior.
6. **State publication** uses staged/replace semantics; crash-loop markers distinguish a host that actually started from a `Process.Start` failure.
7. **Manifest hardening** rejects oversized/incomplete manifests, duplicate IDs, duplicate requirements and unsupported failure policy before module initialization.
8. Native DLLs are trusted code. A bug after successful initialization can still crash the process; NCMM cannot sandbox arbitrary native code. Script/WASM sandboxing is a future layer.

## Host API v1

Capabilities currently exposed:

- `core.v1`
- `world_options.v1`
- `locale.v1`
- `module_contract.v1`
- `host_info.v1`

`module_contract.v1` means the host validates `mod.json` before loading native code and cross-checks the manifest against the DLL descriptor after load.

`host_info.v1` exposes the host version, loader API version and enumerable capability registry through a binary-compatible tail extension of `ncmm_host_api_v1`.

`world_options.v1` currently exposes only the minimum primitive needed by AWS:

- preflight: can an existing permanently-hidden world option be exposed?
- commit: expose it only in world-generation options and assign display name/tooltip.

## Compatibility rule

NCMM does not guess by folder name or launcher version.

A host is usable only when:

- runtime loader API matches;
- `VERSION.txt` source commit matches feed metadata;
- vanilla executable SHA is present in the certified feed;
- downloaded host SHA matches feed metadata;
- host reaches the ready marker after module initialization.

Failure of any check results in vanilla execution.

## v0.5 compatibility boundary

The source-contract registry is checked before source mutation. A certified host therefore carries
an explicit set of source contracts that passed for its exact upstream tag. Code-mods consume only
NCMM capabilities; the first gameplay consumer, Survivor Progression, never includes CDDA headers.

Character persistence is implemented behind `character_state.v1`; the module sees only namespaced
integer keys while the host adapts that contract to CDDA's serialized character values.
The turn source hook is isolated behind `events.turn.v1`.

## 0.5.1 gameplay input bridge

NCMM registers namespaced gameplay actions instead of polling raw keys. The host supplies stable defaults
(`F2` for the NCMM manager and a manifest `ui_hotkey` for module UI), while CDDA's own keybinding system
owns user overrides and persistence. `handle_action.cpp` dispatches an NCMM action before conversion to
the native `action_id`, so opening NCMM UI consumes no game turn.

`COPT_WORLDGEN_ONLY` remains hidden from global/default options, but becomes visible in the active
world's Current World tab. CDDA's existing `options_manager::show(true)` path remains responsible for
saving `WORLD_OPTIONS` and applying option changes.

The compatibility registry now includes `gameplay_input.source.v1`; host certification therefore fails
closed before touching input integration points that no longer match the reviewed contract.

## 0.5.2 dual-mode input + world-options layout

NCMM keyboard defaults now mirror CDDA `keyboard_any`: the same logical default is registered for
both `keyboard_code` and `keyboard_char`. User overrides remain owned by CDDA's normal keybinding
manager. This is necessary because DEFAULTMODE may fall back from keycode to keychar at runtime.

`world_options.layout.v1` adds generic host-owned layout primitives for existing hidden world options:
collapsible groups and a string-choice adapter. Modules still do not own CDDA world storage; the
underlying option IDs and WORLD_OPTIONS serialization remain native CDDA state.

## 0.6.0 character.modifiers.v1

Gameplay modifiers are host-owned runtime state. Modules submit a namespaced value through the stable ABI;
the host validates the modifier id against a fixed allowlist and aggregates values across loaded modules.
CDDA source hooks only query the aggregate and only alter the avatar path, leaving NPC simulation untouched.

Persistence remains the module's responsibility through `character_state.v1`. This deliberately avoids
serializing host modifier internals and makes module disable/uninstall behavior clean on restart.

## 0.6.1 modifier hardening

`character.modifiers.v1` remains the same ABI capability. The host now applies a per-modifier input policy,
requires a registered module id, and clears a module namespace around failed initialization/shutdown.
These are containment rules only; no new CDDA object pointers or module-visible internals are exposed.

Positive healing bonuses are applied only to positive healing rates, so a perk cannot amplify an unrelated
negative degeneration rate. Survivor 0.8.1 adds UI/diagnostic polish without changing perk balance.

## 0.6.2 callback-scoped ownership and ready publication

State/modifier namespace ownership is enforced by the host callback boundary rather than trusting the
module-supplied module_id string. During init, turn, locale, UI and shutdown callbacks the loader binds
a thread-local active module identity; namespaced state/modifier operations reject any different id and
reject calls made outside a host callback scope.

Descriptor/init callbacks are exception-contained. `boot.ready` is staged and atomically published before
`boot.pending` is removed, so a readiness-publication failure remains fail-closed for the next bootstrap.
The ABI and existing capability names remain unchanged.

## 0.6.3 certification and crash-loop invariants

Certification identity now spans host source patch inputs and host packaging logic. Published host assets use
a patch-revision-qualified release tag; the publisher verifies GitHub's asset digest before committing a feed
entry. A current-revision rejection removes older feed entries for the same upstream tag.

Runtime host bindings are versioned with `ncmm_version`, `loader_api`, and `patch_revision`. Bootstrap accepts
offline local hosts only when the binding matches the current runtime/loader contract, and online feed entries
must match the feed's current patch revision.

Crash-loop markers form a two-phase state: `boot.pending` means launch in progress, while `boot.ready` proves
module initialization completed. `pending + ready` is treated as cleanup failure rather than a crash; ambiguous
marker cleanup fails closed to vanilla for that run.

## 0.6.3.1 text-encoding invariant

Repository text consumed by GitHub Actions, PowerShell, C#, JSON and native builds is guarded as UTF-8.
Workflow YAML is stricter and must be BOM-free because workflow parsing occurs before any CI step can run.
Other NCMM text files may retain one UTF-8 BOM for Windows PowerShell compatibility, but multiple BOMs,
UTF-16/UTF-32 and embedded U+FEFF are rejected.

The invariant is enforced in three layers: EditorConfig at edit time, package/pre-commit validation, and
both runtime/host build entry points. This hotfix intentionally does not change the 0.6.3 runtime/host protocol.

## 0.6.4 executable failure harness

Bootstrap safety is now tested as an executable state machine. CI compiles the production bootstrap and launches
it inside isolated temporary game roots containing deterministic synthetic vanilla/host child executables.
The harness asserts process selection, exit propagation, runtime.state.json and crash-loop marker transitions.

The suite is offline by construction and does not depend on GitHub/network availability. Its purpose is to catch
regressions in fail-closed behavior before runtime packaging: invalid bindings must select vanilla, host crashes
must become auto-disable on the next launch, successful ready publication must not be treated as a crash, and
filesystem failures while persisting/cleaning recovery markers must never cause an unsafe host launch.

## 0.6.5 feed integrity and runtime callback quarantine

Certified-host publication is now guarded by a feed-integrity auditor. Feed entries must be revision-coherent,
use immutable patch-revision-qualified GitHub release URLs, and match GitHub's published asset digest. The host
publisher audits the staged feed before committing it, while a separate scheduled/push workflow re-audits the
published feed online.

Runtime callback failures are isolated per callback. A first C++ exception in turn, locale, or UI execution
quarantines that callback for the remainder of the process, clears all gameplay modifiers registered by the
module, and blocks subsequent modifier writes from the quarantined module. The native DLL stays loaded so the
host never unloads code that may still have live function/static state. `modules.state.json` is atomically
updated to `runtime_fault` with a machine-readable reason. The pure quarantine state machine lives in
`ncmm_fault_policy.h` and is exercised by the runtime smoke executable.

## 0.6.6 manifest identity and Diagnostics 2.0

Module Contract v1 manifest parsing is no longer substring-based. `ncmm_manifest_policy.h` owns a small,
bounded schema parser for the exact v1 manifest fields. Duplicate JSON keys, malformed strings/types,
overflow, missing required fields and trailing content fail before `LoadLibrary`. Descriptor capability
lists are independently normalized and duplicate-checked before equality with the manifest contract.

Duplicate identity is defined across enabled modules only. Disabled module directories remain visible in
state/diagnostics but do not reserve or conflict with an active module ID. Two enabled directories with the
same ID reject symmetrically. Runtime identity reservation happens only after descriptor/capability validation
and is released if init throws or returns failure.

`modules.state.json` schema 2 adds the module directory basename. Diagnostics 2.0 correlates executable SHA,
binding identity, bootstrap runtime state, host module state and a fresh `code_mods` scan. It exports a
bounded text snapshot to `ncmm/diagnostics-latest.txt`; custom feed URLs are stripped of query/fragment and
raw log contents are not embedded. The diagnostics harness runs against synthetic installations in Runtime CI.

## 0.7.0 API stabilization and state migration

NCMM 0.7 keeps `NCMM_ABI_VERSION=1` and `NCMM_LOADER_API_VERSION=1`. The existing
`ncmm_host_api_v1` prefix is unchanged; API 1.1 is a capability-gated tail extension, so correctly
written older v1 code-mods remain binary compatible.

`api.versioning.v1` exposes semantic API major/minor values independently from the NCMM runtime
release number. A 0.7-aware manifest may declare `api_major` and `api_min_minor`; an incompatible
major or unavailable minimum minor is rejected before `LoadLibrary` side effects.

`state.migration.v1` standardizes persistent module-state upgrades. A module declares `state_schema`
and `state_min_supported` and exports `ncmm_migrate_state_v1`. Before gameplay/UI callbacks touch an
available character, the host checks the namespaced `schema`. Supported older state is migrated inside
the normal module ownership scope. Newer/too-old state, callback failure, exception, or failure to commit
the target schema suspends only that module and clears its runtime modifiers.

`module.lifecycle.v1` adds an explicit `lifecycle` value to `modules.state.json` schema 3. Legacy
`state` is retained for diagnostics compatibility. Normal modules are `active`; migration/runtime-fault
modules are `suspended`; disabled/rejected modules are `disabled`.

Survivor Progression 0.9.0 is the first production consumer: persistent schema 3, migration from schemas
0–2, semantic API 1.1 requirement, and remap-safe level-up messaging. Perk balance is unchanged.
