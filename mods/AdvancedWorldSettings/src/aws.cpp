#define NCMM_MOD_BUILD
#include "ncmm_api.h"

#include <string>

namespace
{
const char *required_caps[] = {
    "core.v1",
    "world_options.v1",
    "locale.v1",
    "module_contract.v1"
};

struct option_desc {
    const char *id;
    const char *name;
    const char *tooltip;
};

const option_desc options_en[] = {
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

const option_desc options_ru[] = {
    { "SPAWN_DENSITY", "Плотность появления монстров",
      "Множитель плотности появления монстров. 1,00 — стандартное значение игры." },
    { "ITEM_SPAWNRATE", "Количество генерируемых предметов",
      "Множитель количества генерируемых предметов. 1,00 — стандартное значение игры." },
    { "MONSTER_SPEED", "Скорость монстров",
      "Глобальная скорость монстров в процентах. 100% — стандартное значение игры." },
    { "MONSTER_RESILIENCE", "Живучесть монстров",
      "Глобальный запас здоровья монстров в процентах. 100% — стандартное значение игры." },
    { "EVOLUTION_INVERSE_MULTIPLIER", "Множитель времени эволюции монстров",
      "Множитель времени эволюции монстров. Большие значения замедляют эволюцию; 0 отключает улучшения там, где это поддерживается." }
};

const option_desc *localized_options( const ncmm_host_api_v1 *api )
{
    const char *raw = api && api->get_locale ? api->get_locale() : "en";
    const std::string locale = raw ? raw : "en";
    if( locale == "ru" || locale.rfind( "ru_", 0 ) == 0 ) {
        return options_ru;
    }
    return options_en;
}

int expose_all( const ncmm_host_api_v1 *api, bool log_errors )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) {
        return 0;
    }
    const option_desc *options = localized_options( api );
    const size_t count = sizeof( options_en ) / sizeof( options_en[0] );

    for( size_t i = 0; i < count; ++i ) {
        if( !api->can_expose_worldgen_option( options[i].id ) ) {
            if( log_errors ) {
                api->log( NCMM_LOG_ERROR, "AWS: preflight failed; no world options were exposed." );
            }
            return 0;
        }
    }
    for( size_t i = 0; i < count; ++i ) {
        if( !api->expose_worldgen_option( options[i].id, options[i].name, options[i].tooltip ) ) {
            if( log_errors ) {
                api->log( NCMM_LOG_ERROR, "AWS: unexpected commit failure after successful preflight." );
            }
            return 0;
        }
    }
    return 1;
}

int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) {
        return 0;
    }
    if( !api->has_capability( "world_options.v1" ) || !api->has_capability( "locale.v1" ) ) {
        return 0;
    }
    if( !expose_all( api, true ) ) {
        return 0;
    }

    api->log( NCMM_LOG_INFO, "Advanced World Settings 0.4 initialized." );
    return 1;
}

void shutdown()
{
}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    "advanced_world_settings",
    "Advanced World Settings",
    "0.4.0",
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

extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1( const ncmm_host_api_v1 *api )
{
    expose_all( api, false );
}
