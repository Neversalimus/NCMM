# NCMM native SDK: safe queried Core access

Use the source-only C++17 header ncmm_sdk_core.hpp when building a new NCMM
native module. It supplements the existing ncmm_api.h and does not change any
published Host or Loader ABI.

## How it works

Use the canonical required capability array in mod.json and the descriptor.
Pass the same capabilities to ncmm::sdk::require_core(), together with
NCMM_SDK_CORE_FIELD_END(last_core_field_used). A successful result contains
a borrowed const ncmm_host_api_v2_core*; it must not be retained after module
shutdown or an unloading/replacement lifecycle.

The function fails closed before dereferencing capability-gated legacy-v1
tail fields. It validates:

- non-null Loader API pointer, ABI v1 and has_capability function;
- api.versioning.v1, host_api.v2.core and all requested capabilities;
- non-null version callbacks and API major 1 / minor at least 9;
- non-null query_interface and the returned Core pointer;
- minimum safe Core header size and caller-requested field boundary;
- Core ABI major 2 and requested Core API minor floor.

No allocation, exception, CDDA patch, process-global state or gameplay hook
is added. An explicit core_error value is available for diagnostics, but
modules should normally fail initialization rather than guessing a fallback.

The helper ncmm::sdk::register_live_bool() also checks the optional Core
function pointer, namespace, key, label and description. It always creates a
LIVE-scoped bool setting, not a gameplay hook.

## Version compatibility

Do NOT use sizeof(ncmm_host_api_v2_core) as the minimum supported Core size:
a compatible older Host can expose only the fields your module needs.
Use NCMM_SDK_CORE_FIELD_END to request a valid prefix.

Do NOT use v1 tail fields before confirming their associated capability.
Do NOT reinterpret or copy Host-owned structs into a different ABI layout.
Prefer capability negotiation over assuming a particular CDDA revision.

New projects created by tools/New-NCMMNativeModule.ps1 now use this header.
The standalone MSVC harness tests null/missing capability, old API versions,
short Core prefixes, invalid major/ABI, missing query function, missing
typed-setting callback and successful LIVE setting registration.

The five existing game mods are not silently rewritten to use this helper;
migrating each requires a separately reviewed compatibility and smoke pass.
