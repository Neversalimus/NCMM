# NCMM Host API 2.0 Core contract

Host `0.8.0` keeps Loader ABI v1. Existing modules continue to initialize through `ncmm_get_descriptor_v1`.

A module that wants Core 2.0 requires capability `host_api.v2.core`, then during its v1 init obtains the v2 table through the additive legacy tail:

```cpp
const auto *core = static_cast<const ncmm_host_api_v2_core *>(
    api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2, 0 ) );
if( !core || core->abi_version != 2 || core->api_major != 2 ) { return 0; }
```

The table exposes events, typed world settings, active-world mods, character state, module lifecycle queries, modifier ownership, runtime-hook rules and worldgen bindings. `struct_size` must be checked before using future additive tail fields.

Current Survivor/AWS binaries are deliberately still legacy users. The Core API is the substrate for their next migration; this package does not claim that their current source hooks have already been removed.
