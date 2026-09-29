# NCMM Host API 2.0 Core contract

NCMM Host 0.8.1 keeps **Loader ABI v1**. Existing compatible modules still enter through `ncmm_get_descriptor_v1` and the stable `ncmm_host_api_v1` prefix.

Host API 2.0 Core is an additive queried interface, not a replacement ABI. A module that requires capability `host_api.v2.core` obtains it through the v1 tail:

```cpp
const auto *core = static_cast<const ncmm_host_api_v2_core *>(
    api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2, 0 ) );
if( !core || core->abi_version != 2 || core->api_major != 2 ) {
    return 0;
}
```

A consumer must respect `struct_size` before using future additive tail fields.

## Current Core domains

- `events.core.v2` — generic Host/world/turn/locale event subscriptions.
- `settings.typed.v2` — typed bool/int/float/enum settings and reads.
- `active_mods.registry.v2` — active world-mod enumeration.
- `module.lifecycle.query.v2` — loaded/version/state queries.
- `character.modifiers.v2` — built-in and module-owned dynamic numeric modifiers.
- `runtime_hooks.registry.v2` — generic hook + selector + modifier rules.
- `worldgen.bindings.v2` — generic world-generation hook to typed-setting bindings.

The stable v1 API remains responsible for the legacy binary entry path, module manifest contract, ownership scope, persistent character state and compatible additive services.

## Current consumers

**Survivor Progression 0.12.0** uses Host API 2.0 events, typed settings, character modifiers and generic runtime-hook infrastructure while keeping its perk IDs and integration registry inside the module.

**Advanced World Settings 0.6.2** uses typed settings and generic world-generation bindings. Its `NCMM_AWS_*` setting IDs remain module-owned; CDDA-facing Host hooks use generic geography domains.

Both modules therefore use Host API 2.0 through the Loader ABI v1 compatibility path. Module-specific gameplay concepts are not promoted into the Host API unless they represent a genuinely reusable engine domain.

## Settings scope contract

Typed world settings carry a scope used by the Host to decide where they belong:

- LIVE / RELOAD — NCMM module manager.
- NEW_MAP / NEW_WORLD — CDDA world-options UI, including world creation.

The Host owns persistence/presentation mechanics. The module owns the setting ID, text, default/range and gameplay meaning.

## Compatibility rule

A module must declare required capabilities in `mod.json`. Host preflight validates those requirements before normal initialization and cross-checks the loaded DLL descriptor against the manifest. Unsupported API/capability combinations fail closed for that module.

The public C header remains `sdk/ncmm_api.h`; this document describes the contract but is not itself part of the binary ABI.
