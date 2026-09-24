# NCMM v0.5.0 architecture

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
