#include "ncmm_api.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
constexpr const char *module_id = "ballistic_hit_chance";
constexpr const char *module_version = "0.1.0";

const ncmm_host_api_v2_core *host2 = nullptr;

const char *required_caps[] = {
    "core.v1",
    "locale.v1",
    "api.versioning.v1",
    "host_api.v2.core",
    "settings.typed.v2",
    "runtime_settings.bindings.v2"
};

bool russian( const ncmm_host_api_v1 *api )
{
    const char *raw = api && api->get_locale ? api->get_locale() : "en";
    const std::string locale = raw ? raw : "en";
    return locale == "ru" || locale.rfind( "ru_", 0 ) == 0;
}

const char *tr( bool ru, const char *en, const char *ru_text )
{
    return ru ? ru_text : en;
}

bool runtime_binding_tail_available( const ncmm_host_api_v2_core *core )
{
    if( core == nullptr ) {
        return false;
    }
    const std::size_t required_size =
        offsetof( ncmm_host_api_v2_core, runtime_hook_f64 ) +
        sizeof( core->runtime_hook_f64 );
    return core->struct_size >= required_size &&
           core->runtime_hook_bind_setting != nullptr &&
           core->runtime_hook_bool != nullptr;
}

bool register_settings( const ncmm_host_api_v1 *api )
{
    if( host2 == nullptr || !host2->world_setting_register_bool ) {
        return false;
    }
    const bool ru = russian( api );

    if( !host2->world_setting_register_bool(
            module_id, "NCMM_BHC_ENABLED",
            tr( ru, "Show exact hit chance", "Показывать точный шанс попадания" ),
            tr( ru,
                "Shows deterministic probability from the same dispersion model used by firearm projectiles. "
                "The value is conditional on the shot being fired; jams and weapon failures are not included.",
                "Показывает детерминированную вероятность из той же модели разброса, которую используют "
                "огнестрельные снаряды. Значение считается при условии, что выстрел произошёл; осечки и "
                "неисправности оружия не учитываются." ),
            1, NCMM_WORLD_SETTING_LIVE ) ) {
        return false;
    }

    if( !host2->world_setting_register_bool(
            module_id, "NCMM_BHC_DECIMAL",
            tr( ru, "One decimal place", "Один знак после запятой" ),
            tr( ru,
                "Off: show whole percentages (87%). On: show one decimal place (87.4%).",
                "Выкл.: показывать целые проценты (87%). Вкл.: показывать один знак после запятой (87,4%)." ),
            0, NCMM_WORLD_SETTING_LIVE ) ) {
        return false;
    }

    return true;
}

bool bind_runtime_settings()
{
    if( !runtime_binding_tail_available( host2 ) ) {
        return false;
    }
    return host2->runtime_hook_bind_setting(
               module_id, "targeting.hit_probability.enabled",
               "NCMM_BHC_ENABLED", NCMM_SETTING_BOOL_V2 ) != 0 &&
           host2->runtime_hook_bind_setting(
               module_id, "targeting.hit_probability.decimal",
               "NCMM_BHC_DECIMAL", NCMM_SETTING_BOOL_V2 ) != 0;
}

int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ||
        api->query_interface == nullptr || api->has_capability == nullptr ) {
        return 0;
    }
    for( const char *capability : required_caps ) {
        if( !api->has_capability( capability ) ) {
            return 0;
        }
    }
    if( api->get_api_version_major && api->get_api_version_minor ) {
        if( api->get_api_version_major() != 1u || api->get_api_version_minor() < 9u ) {
            return 0;
        }
    }

    host2 = static_cast<const ncmm_host_api_v2_core *>(
                api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 0u ) );
    if( host2 == nullptr || host2->abi_version != NCMM_HOST_API_V2_CORE_ABI ||
        host2->api_major != 2u || !runtime_binding_tail_available( host2 ) ) {
        host2 = nullptr;
        return 0;
    }

    if( !register_settings( api ) || !bind_runtime_settings() ) {
        host2 = nullptr;
        return 0;
    }

    if( api->log ) {
        api->log( NCMM_LOG_INFO,
                  "Ballistic Hit Chance 0.1.0 initialized: exact dispersion probability display enabled." );
    }
    return 1;
}

void shutdown()
{
    host2 = nullptr;
}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    module_id,
    "Ballistic Hit Chance",
    module_version,
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
    if( host2 != nullptr ) {
        register_settings( api );
    }
}
