#include "ncmm_api.h"

namespace
{
constexpr const char *capabilities[] = { "core.v1", "api.versioning.v1" };

int fixture_init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ||
        api->has_capability == nullptr ) {
        return 0;
    }
    for( const char *cap : capabilities ) {
        if( !api->has_capability( cap ) ) {
            return 0;
        }
    }
    return 1;
}

void fixture_shutdown() {}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    "ncmm_generic_fixture",
    "Generic smoke onboarding fixture",
    "0.0.1",
    capabilities,
    sizeof( capabilities ) / sizeof( capabilities[0] ),
    &fixture_init,
    &fixture_shutdown
};
}

extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1()
{
    return &descriptor;
}
