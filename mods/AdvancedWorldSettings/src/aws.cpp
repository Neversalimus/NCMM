#define NCMM_MOD_BUILD
#include "ncmm_api.h"

namespace
{
const char *required_caps[] = {
    "core.v1",
    "world_options.v1"
};

int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) {
        return 0;
    }
    if( !api->has_capability( "world_options.v1" ) ) {
        return 0;
    }

    struct option_desc {
        const char *id;
        const char *name;
        const char *tooltip;
    };

    const option_desc options[] = {
        { "SPAWN_DENSITY", "Monster spawn density",
          "Multiplier for monster spawn density. 1.00 is the vanilla default." },
        { "ITEM_SPAWNRATE", "Item spawn rate",
          "Multiplier for generated item quantity. 1.00 is the vanilla default." },
        { "MONSTER_SPEED", "Monster speed",
          "Global monster speed percentage. 100% is the vanilla default." },
        { "MONSTER_RESILIENCE", "Monster resilience",
          "Global monster health percentage. 100% is the vanilla default." },
        { "EVOLUTION_INVERSE_MULTIPLIER", "Monster evolution time multiplier",
          "Multiplier applied to monster evolution time. Higher values slow evolution; 0 disables upgrades where supported." }
    };

    for( const option_desc &opt : options ) {
        if( !api->can_expose_worldgen_option( opt.id ) ) {
            api->log( NCMM_LOG_ERROR, "AWS: preflight failed; no world options were exposed." );
            return 0;
        }
    }
    for( const option_desc &opt : options ) {
        if( !api->expose_worldgen_option( opt.id, opt.name, opt.tooltip ) ) {
            api->log( NCMM_LOG_ERROR, "AWS: unexpected commit failure after successful preflight." );
            return 0;
        }
    }

    api->log( NCMM_LOG_INFO, "Advanced World Settings 0.2 initialized." );
    return 1;
}

void shutdown()
{
}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    "advanced_world_settings",
    "Advanced World Settings",
    "0.2.0",
    required_caps,
    sizeof( required_caps ) / sizeof( required_caps[0] ),
    &init,
    &shutdown
};
} // namespace

extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1()
{
    return &descriptor;
}
