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
