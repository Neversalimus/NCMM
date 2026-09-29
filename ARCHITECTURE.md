# NCMM architecture

Current baseline: Infrastructure 0.8.3.1, Runtime / Host 0.8.1, Loader ABI 1, semantic Host API 1.9, queried Host API 2.0 Core, Survivor Progression 0.12.0 and Advanced World Settings 0.6.2.

The important design rule is separation: CDDA-facing source integration belongs to the certified Host; gameplay modules consume NCMM APIs and do not patch CDDA independently.

```text
Launcher / shortcut
        |
        v
cataclysm-tiles.exe
 [NCMM bootstrap]
        |
        | exact vanilla exe SHA + VERSION.txt source commit
        v
 local binding valid? ---- no ----> certified feed
        |                         exact executable match?
       yes                              |
        |                              yes
        |                               |
        +----------------------- verified Host
                                        |
                                        v
                         cataclysm-tiles.ncmm.exe
                              [NCMM Host]
                                        |
                        Loader ABI v1 + Host API 1.9
                        + queried Host API 2.0 Core
                                        |
                    manifest / capability / state preflight
                                        |
                            code_mods / native modules
                              /                   \
                             v                     v
             Advanced World Settings      Survivor Progression
```

## Trust boundaries

1. **Bootstrap** selects vanilla or the certified Host. It validates executable identity and does not need to understand gameplay internals.
2. **Certified Host** is built from an exact upstream CDDA release after source-contract preflight. Feed entries bind a Host to exact source/executable identity and patch revision.
3. **Host API** is a narrow C ABI. Modules do not receive STL objects or arbitrary CDDA pointers.
4. **Module Contract** validates `mod.json`, Loader/API requirements, capability requirements, module identity and the DLL descriptor before normal gameplay callbacks are allowed.
5. **Module state** is namespaced and migration-aware. A broken or incompatible module can be disabled/suspended without redefining the whole runtime contract.
6. **Runtime publication** uses explicit state files and crash-loop markers. Failure to prove a safe Host launch falls back to vanilla.
7. Native DLLs remain trusted native code. NCMM contains and validates known failure paths, but it is not a security sandbox for arbitrary machine code.

## API layers

### Loader ABI v1

`ncmm_host_api_v1` remains the stable binary prefix. Existing correctly written v1 modules stay binary-compatible. Additive v1-tail fields expose Host identity, capabilities, world settings, UI, active-world mod discovery, state migration and related services.

### Semantic Host API 1.9

The semantic v1 surface is capability-gated. Modules declare their minimum API/capability requirements in `mod.json`; the Host rejects unsupported combinations before normal initialization.

### Host API 2.0 Core

Host API 2.0 is obtained additively through:

```cpp
api->query_interface( "ncmm.host_api.v2.core", 2, 0 )
```

Current Core domains are:

- `events.core.v2` — generic lifecycle/gameplay event subscriptions;
- `settings.typed.v2` — typed bool/int/float/enum settings;
- `active_mods.registry.v2` — active world-mod enumeration;
- `module.lifecycle.query.v2` — module state/version queries;
- `character.modifiers.v2` — built-in and module-owned numeric modifiers;
- `runtime_hooks.registry.v2` — generic runtime hook + selector + modifier rules;
- `worldgen.bindings.v2` — generic world-generation hook to typed-setting bindings.

Survivor Progression and Advanced World Settings both use the Host API 2.0 substrate while retaining the stable Loader ABI v1 entry path.

## Settings model

NCMM typed settings have an explicit scope. The Host owns persistence and presentation rules; modules own names, defaults, ranges and semantics.

- **LIVE / RELOAD** settings are presented in the NCMM module manager.
- **NEW_MAP / NEW_WORLD** settings are surfaced in CDDA's world-options UI, including world creation.
- World-generation settings use CDDA's existing `WORLD_OPTIONS` persistence rather than a parallel NCMM save file.

This separation prevents a module's balance controls from polluting world creation while keeping true world-generation controls available where the player expects them.

## Input and UI

NCMM registers namespaced actions through CDDA's normal input system. Default keys such as F1/F2 are defaults only; player remaps remain owned by CDDA.

The Host owns common UI primitives (choice, tiles, cards, tree layouts, theme/layout extensions and the two-pane module manager). Modules provide data and callbacks. UI sounds use CDDA's existing SFX mechanism rather than a separate NCMM audio layer.

## Module lifecycle and persistence

Before loading a module, the Host checks its manifest and required capabilities. After loading, the DLL descriptor is cross-checked against the manifest.

Persistent per-character module state uses the Host state API and explicit schema versions. `state.migration.v1` runs migrations inside the owning module scope. Unsupported newer/older state, migration failure or quarantined runtime callbacks suspend only the affected module and clear its runtime modifiers where required.

Survivor Progression currently uses state schema 8.

## Certification and compatibility

NCMM compatibility is identity-based, not launcher-based. A Host is usable only when the relevant runtime/loader contract, CDDA source commit, vanilla executable SHA and patch revision all match the certified metadata.

The CI pipeline therefore:

1. selects an exact upstream CDDA release;
2. validates source contracts before mutation;
3. applies the Host patch;
4. builds on Windows/MSVC;
5. runs certification/regression checks;
6. binds the resulting Host to official vanilla executable SHA values;
7. publishes immutable Host assets and updates the feed only after integrity checks pass.

A new CDDA experimental can therefore be accepted automatically when contracts still match, or rejected safely without teaching the bootstrap to guess.

## Runtime state

`ncmm/runtime.state.json` records bootstrap/runtime identity and launch state. `ncmm/modules.state.json` records Host capabilities and per-module state/reasons. Diagnostics correlate those files with the current executable, binding and installed module set.

Crash-loop markers use a two-phase model: `boot.pending` means a Host launch began; `boot.ready` proves Host/module initialization reached the ready point. Ambiguous or invalid state fails closed.

## Repository lineage

NCMM originally lived under `ncmm-platform/` in `Neversalimus/Cataclysm`. The standalone `Neversalimus/NCMM` repository replayed the meaningful NCMM history and became the sole canonical source/feed/release repository at the 0.7.1 cutover. The legacy Cataclysm repository is no longer part of the active build pipeline.

Historical package snapshots that still matter for reproducibility live under `checkpoints/`; current documentation is intentionally kept in this file and the two root README files instead of duplicating version-by-version architecture narratives.
