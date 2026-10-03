# NCMM Host API 2.x Core contract

NCMM Host 0.8.2 keeps **Loader ABI v1**. Existing compatible modules still enter through `ncmm_get_descriptor_v1` and the stable `ncmm_host_api_v1` prefix.

Host API 2.x Core is an additive queried interface, not a replacement ABI. The current minor is 2.3. A module that requires capability `host_api.v2.core` obtains it through the v1 tail:

```cpp
const auto *core = static_cast<const ncmm_host_api_v2_core *>(
    api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2, 0 ) );
if( !core || core->abi_version != 2 || core->api_major != 2 ) {
    return 0;
}
```

A consumer must respect `struct_size` before using additive tail fields. `NCMM_HOST_API_V2_CORE_SIZE_2_1`, `_2_2`, and `_2_3` define the exact safe prefix sizes for each virtual-item tail.

## Current Core domains

- `events.core.v2` — generic Host/world/turn/locale event subscriptions.
- `settings.typed.v2` — typed bool/int/float/enum settings and reads.
- `active_mods.registry.v2` — active world-mod enumeration.
- `module.lifecycle.query.v2` — loaded/version/state queries.
- `character.modifiers.v2` — built-in and module-owned dynamic numeric modifiers.
- `runtime_hooks.registry.v2` — generic hook + selector + modifier rules.
- `worldgen.bindings.v2` — generic world-generation hook to typed-setting bindings.
- `runtime_settings.bindings.v2` — generic LIVE/RELOAD typed-setting bindings consumed by runtime/UI hooks.
- `character.virtual_items.v1` — logical item slots backed by real CDDA items. Host API 2.2 adds secondary-melee behavior controls; 2.3 adds primary-melee behavior controls.

The stable v1 API remains responsible for the legacy binary entry path, module manifest contract, ownership scope, persistent character state and compatible additive services.

## Current consumers

**Survivor Progression 0.14.0** uses Host API 2.x events, typed settings, character modifiers, generic runtime hooks and logical virtual-item slots. The 2.2/2.3 tails expose secondary/primary melee behavior toggles without exposing engine item pointers to the module.

**Advanced World Settings 0.6.3** uses typed settings and generic world-generation bindings. Its `NCMM_AWS_*` setting IDs remain module-owned; CDDA-facing Host hooks use generic geography domains.

**Ballistic Hit Chance 0.1.0** uses generic runtime-setting bindings. Its `NCMM_BHC_*` setting IDs remain module-owned; patched CDDA only sees generic `targeting.hit_probability.*` hooks.

Modules continue to use Host API 2.x through the Loader ABI v1 compatibility path. Module-specific gameplay concepts are not promoted into the Host API unless they represent a genuinely reusable engine domain.

## Settings scope contract

Typed world settings carry a scope used by the Host to decide where they belong:

- LIVE / RELOAD — NCMM module manager.
- NEW_MAP / NEW_WORLD — CDDA world-options UI, including world creation.

The Host owns persistence/presentation mechanics. The module owns the setting ID, text, default/range and gameplay meaning.

## Compatibility rule

A module must declare required capabilities in `mod.json`. Host preflight validates those requirements before normal initialization and cross-checks the loaded DLL descriptor against the manifest. Unsupported API/capability combinations fail closed for that module.

The public C header remains `sdk/ncmm_api.h`; this document describes the contract but is not itself part of the binary ABI.
