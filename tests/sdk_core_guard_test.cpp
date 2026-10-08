#include "ncmm_sdk_core.hpp"
#include <cstring>
#include <iostream>

namespace {
bool deny_capability = false;
const char *denied_capability = "";
bool deny_query = false;
bool deny_setting = false;
std::uint32_t api_major = NCMM_API_VERSION_MAJOR;
std::uint32_t api_minor = NCMM_API_VERSION_MINOR;
int registered = 0;
ncmm_host_api_v2_core host_core{};

int has_capability( const char *value )
{
    if( value == nullptr ) return 0;
    const char *caps[] = {
        "core.v1", "api.versioning.v1",
        "host_api.v2.core", "settings.typed.v2"
    };
    for( const char *cap : caps ) {
        if( std::strcmp( cap, value ) == 0 ) {
            return deny_capability && std::strcmp( value, denied_capability ) == 0 ? 0 : 1;
        }
    }
    return 0;
}
std::uint32_t major_version() { return api_major; }
std::uint32_t minor_version() { return api_minor; }
const void *query_interface( const char *id, std::uint32_t major,
                             std::uint32_t minor )
{
    if( deny_query || id == nullptr ||
        std::strcmp( id, NCMM_HOST_API_V2_CORE_ID ) != 0 ||
        major != NCMM_HOST_API_V2_CORE_MAJOR ||
        minor > NCMM_HOST_API_V2_CORE_MINOR ) return nullptr;
    return &host_core;
}
int register_bool( const char *module_id, const char *setting_id,
                   const char *label, const char *tooltip,
                   int default_value, std::uint32_t scope )
{
    if( module_id == nullptr || setting_id == nullptr ||
        label == nullptr || tooltip == nullptr ||
        std::strcmp( module_id, "example_mod" ) != 0 ||
        std::strcmp( setting_id, "NCMM_EXAMPLE_MOD_ENABLED" ) != 0 ||
        default_value != 1 || scope != NCMM_WORLD_SETTING_LIVE ) return 0;
    ++registered;
    return deny_setting ? 0 : 1;
}
bool check( bool passed, const char *description )
{
    if( !passed ) std::cerr << "SDK Core guard FAIL: " << description << '\n';
    return passed;
}
}

int main()
{
    using namespace ncmm::sdk;
    int failed = 0;
    int checks = 0;
    auto expect = [&]( bool ok, const char *label ) {
        ++checks;
        if( !check( ok, label ) ) ++failed;
    };

    const char *caps[] = { "core.v1", "settings.typed.v2" };
    ncmm_host_api_v1 legacy{};
    legacy.abi_version = NCMM_ABI_VERSION;
    legacy.has_capability = &has_capability;
    legacy.get_api_version_major = &major_version;
    legacy.get_api_version_minor = &minor_version;
    legacy.query_interface = &query_interface;
    host_core.struct_size = sizeof( host_core );
    host_core.abi_version = NCMM_HOST_API_V2_CORE_ABI;
    host_core.api_major = NCMM_HOST_API_V2_CORE_MAJOR;
    host_core.api_minor = NCMM_HOST_API_V2_CORE_MINOR;
    host_core.world_setting_register_bool = &register_bool;

    constexpr std::size_t needed = NCMM_SDK_CORE_FIELD_END( world_setting_register_bool );
    auto access = [&]() {
        return require_core( &legacy, caps, 2u, needed );
    };
    expect( bool( access() ), "complete supported Host accepted" );
    expect( register_live_bool( access(), "example_mod", "NCMM_EXAMPLE_MOD_ENABLED",
                                "Enable", "Example" ) && registered == 1,
            "LIVE bool registration with expected ownership" );
    expect( !require_core( nullptr, caps, 2u, needed ), "null API rejected" );
    legacy.abi_version = 9;
    expect( access().error == core_error::legacy_abi, "wrong Loader ABI rejected" );
    legacy.abi_version = NCMM_ABI_VERSION;
    legacy.has_capability = nullptr;
    expect( access().error == core_error::legacy_abi, "missing has_capability rejected" );
    legacy.has_capability = &has_capability;
    deny_capability = true;
    denied_capability = "settings.typed.v2";
    expect( access().error == core_error::missing_capability, "missing requested cap rejected" );
    denied_capability = "host_api.v2.core";
    expect( access().error == core_error::missing_capability, "missing Core cap rejected" );
    denied_capability = "api.versioning.v1";
    expect( access().error == core_error::missing_capability, "missing version cap rejected" );
    deny_capability = false;
    expect( require_core( &legacy, nullptr, 2u, needed ).error ==
            core_error::missing_capability, "invalid cap array rejected" );
    const char *empty_caps[] = { "" };
    expect( require_core( &legacy, empty_caps, 1u, needed ).error ==
            core_error::missing_capability, "empty cap rejected" );
    legacy.get_api_version_major = nullptr;
    expect( access().error == core_error::missing_version_api, "missing major callback rejected" );
    legacy.get_api_version_major = &major_version;
    legacy.get_api_version_minor = nullptr;
    expect( access().error == core_error::missing_version_api, "missing minor callback rejected" );
    legacy.get_api_version_minor = &minor_version;
    api_major = 2;
    expect( access().error == core_error::incompatible_version, "future v1 major rejected" );
    api_major = NCMM_API_VERSION_MAJOR;
    api_minor = NCMM_API_VERSION_MINOR - 1u;
    expect( access().error == core_error::incompatible_version, "old v1 minor rejected" );
    api_minor = NCMM_API_VERSION_MINOR;
    legacy.query_interface = nullptr;
    expect( access().error == core_error::missing_query_api, "missing query API rejected" );
    legacy.query_interface = &query_interface;
    deny_query = true;
    expect( access().error == core_error::unavailable_core, "no Core domain rejected" );
    deny_query = false;

    host_core.struct_size = 2;
    expect( access().error == core_error::truncated_core, "tiny Core rejected before header reads" );
    host_core.struct_size = needed - 1u;
    expect( access().error == core_error::truncated_core, "Core field boundary enforced" );
    host_core.struct_size = needed;
    expect( bool( access() ), "compatible older Core prefix accepted" );
    expect( require_core( &legacy, caps, 2u, sizeof( host_core ) + 1u ).error ==
            core_error::truncated_core, "future field request rejected" );
    expect( require_core( &legacy, caps, 2u, 1u ).error ==
            core_error::truncated_core, "invalid minimum size rejected" );
    host_core.abi_version = 5;
    expect( access().error == core_error::incompatible_core, "wrong Core ABI rejected" );
    host_core.abi_version = NCMM_HOST_API_V2_CORE_ABI;
    host_core.api_major = 4;
    expect( access().error == core_error::incompatible_core, "wrong Core major rejected" );
    host_core.api_major = NCMM_HOST_API_V2_CORE_MAJOR;
    host_core.api_minor = 1;
    expect( require_core( &legacy, caps, 2u, needed, 2u ).error ==
            core_error::incompatible_core, "Core minor floor enforced" );
    host_core.api_minor = NCMM_HOST_API_V2_CORE_MINOR;
    host_core.world_setting_register_bool = nullptr;
    expect( !register_live_bool( access(), "example_mod", "NCMM_EXAMPLE_MOD_ENABLED",
                                 "Enable", "Example" ), "missing typed setter rejected" );
    host_core.world_setting_register_bool = &register_bool;
    deny_setting = true;
    expect( !register_live_bool( access(), "example_mod", "NCMM_EXAMPLE_MOD_ENABLED",
                                 "Enable", "Example" ), "failed typed registration rejected" );
    deny_setting = false;
    expect( !register_live_bool( access(), "", "NCMM_EXAMPLE_MOD_ENABLED",
                                 "Enable", "Example" ), "empty module namespace rejected" );
    expect( !register_live_bool( access(), "example_mod", "",
                                 "Enable", "Example" ), "empty setting id rejected" );
    expect( registered == 2, "no extra successful registrations" );

    if( failed ) return 1;
    std::cout << "NCMM SDK Core guard: PASS (" << checks
              << " ABI/capability/struct/setting contracts).\n";
    return 0;
}
