#pragma once
// Header-only helpers for *new* NCMM native modules. No new Host/Loader ABI.
#include "ncmm_api.h"
#include <cstddef>
#include <cstdint>

// Guard all optional Core fields with an offsetof-based struct_size check.
// Do not take sizeof(the entire Core): old compatible Hosts may have a shorter
// valid prefix even when a new SDK knows additional fields.
#define NCMM_SDK_CORE_FIELD_END(field) \
    ( offsetof(ncmm_host_api_v2_core, field) + \
      sizeof(((ncmm_host_api_v2_core *)nullptr)->field) )

namespace ncmm {
namespace sdk {

enum class core_error {
    none,
    legacy_abi,
    missing_capability,
    missing_version_api,
    incompatible_version,
    missing_query_api,
    unavailable_core,
    truncated_core,
    incompatible_core
};

struct core_access {
    const ncmm_host_api_v2_core *core = nullptr;
    core_error error = core_error::unavailable_core;

    explicit operator bool() const noexcept {
        return core != nullptr && error == core_error::none;
    }
};

// Fail closed before accessing any capability-guarded tail member of api.
// Passing required_size = NCMM_SDK_CORE_FIELD_END(last_member_used) lets a
// caller use only the supported Core prefix. All pointers remain Host-owned.
inline core_access require_core(
    const ncmm_host_api_v1 *api,
    const char *const *required_capabilities,
    std::size_t required_count,
    std::size_t required_size,
    std::uint32_t min_core_minor = 0u ) noexcept
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ||
        api->has_capability == nullptr ) {
        return { nullptr, core_error::legacy_abi };
    }
    if( !api->has_capability( "api.versioning.v1" ) ||
        !api->has_capability( "host_api.v2.core" ) ||
        ( required_count != 0 && required_capabilities == nullptr ) ) {
        return { nullptr, core_error::missing_capability };
    }
    for( std::size_t i = 0; i < required_count; ++i ) {
        const char *cap = required_capabilities[i];
        if( cap == nullptr || !*cap || !api->has_capability( cap ) ) {
            return { nullptr, core_error::missing_capability };
        }
    }
    // These v1 tail fields may only be read when the capability above exists.
    if( api->get_api_version_major == nullptr ||
        api->get_api_version_minor == nullptr ) {
        return { nullptr, core_error::missing_version_api };
    }
    if( api->get_api_version_major() != NCMM_API_VERSION_MAJOR ||
        api->get_api_version_minor() < NCMM_API_VERSION_MINOR ) {
        return { nullptr, core_error::incompatible_version };
    }
    if( api->query_interface == nullptr ) {
        return { nullptr, core_error::missing_query_api };
    }
    const auto *core = static_cast<const ncmm_host_api_v2_core *>(
        api->query_interface( NCMM_HOST_API_V2_CORE_ID,
                              NCMM_HOST_API_V2_CORE_MAJOR, min_core_minor ) );
    if( core == nullptr ) {
        return { nullptr, core_error::unavailable_core };
    }
    // Read no additional fields until a complete v2 header is known to exist.
    constexpr std::size_t v2_header_size =
        NCMM_SDK_CORE_FIELD_END( api_minor );
    if( core->struct_size < v2_header_size ||
        required_size < v2_header_size ||
        required_size > sizeof( ncmm_host_api_v2_core ) ||
        core->struct_size < required_size ) {
        return { nullptr, core_error::truncated_core };
    }
    if( core->abi_version != NCMM_HOST_API_V2_CORE_ABI ||
        core->api_major != NCMM_HOST_API_V2_CORE_MAJOR ||
        core->api_minor < min_core_minor ) {
        return { nullptr, core_error::incompatible_core };
    }
    return { core, core_error::none };
}

inline bool register_live_bool(
    core_access checked,
    const char *module_id,
    const char *setting_id,
    const char *label,
    const char *description,
    bool initial = true ) noexcept
{
    if( !checked || checked.core->struct_size <
            NCMM_SDK_CORE_FIELD_END( world_setting_register_bool ) ||
        checked.core->world_setting_register_bool == nullptr ||
        module_id == nullptr || !*module_id ||
        setting_id == nullptr || !*setting_id ||
        label == nullptr || !*label ||
        description == nullptr || !*description ) {
        return false;
    }
    return checked.core->world_setting_register_bool(
        module_id, setting_id, label, description,
        initial ? 1 : 0, NCMM_WORLD_SETTING_LIVE ) != 0;
}

} // namespace sdk
} // namespace ncmm
