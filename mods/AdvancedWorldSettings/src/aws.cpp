#define NCMM_MOD_BUILD
#include "ncmm_api.h"

#include <cstddef>
#include <string>

namespace
{
constexpr const char *module_id = "advanced_world_settings";
const ncmm_host_api_v2_core *host2 = nullptr;
const char *required_caps[] = {
    "core.v1", "world_options.v1", "world_options.layout.v1", "world_settings.v2",
    "world_options.experimental.v1", "locale.v1", "module_contract.v1", "api.versioning.v1",
    "host_api.v2.core", "settings.typed.v2", "worldgen.bindings.v2"
};

struct option_desc {
    const char *id;
    const char *name;
    const char *tooltip;
};

const option_desc options_en[] = {
    { "SPAWN_DENSITY", "Monster spawn density", "Multiplier for monster spawn density. 1.00 is the default." },
    { "ITEM_SPAWNRATE", "Item spawn rate", "Multiplier for generated item quantity. 1.00 is the default." },
    { "MONSTER_SPEED", "Monster speed", "Global monster speed percentage. 100% is the default." },
    { "MONSTER_RESILIENCE", "Monster resilience", "Global monster health percentage. 100% is the default." },
    { "EVOLUTION_INVERSE_MULTIPLIER", "Monster evolution time multiplier", "Higher values slow evolution; 0 disables upgrades where supported." },
    { "SEASON_LENGTH", "Season length (days)", "Requires world reload. Number of days in each season. CDDA default is 91." },
    { "CONSTRUCTION_SCALING", "Construction time scaling", "Percentage of base construction time. 100% is normal; 50% is twice as fast." },
    { "ETERNAL_SEASON", "Eternal season", "Requires world reload. Stops normal season progression." },
    { "ETERNAL_TIME_OF_DAY", "Fixed time of day", "Requires world reload. Normal time flow, permanent day, or permanent night." }
};
const option_desc options_ru[] = {
    { "SPAWN_DENSITY", "Плотность монстров", "Множитель плотности появления монстров. 1,00 — стандарт." },
    { "ITEM_SPAWNRATE", "Количество предметов", "Множитель количества генерируемых предметов. 1,00 — стандарт." },
    { "MONSTER_SPEED", "Скорость монстров", "Глобальная скорость монстров в процентах. 100% — стандарт." },
    { "MONSTER_RESILIENCE", "Живучесть монстров", "Глобальный запас здоровья монстров в процентах. 100% — стандарт." },
    { "EVOLUTION_INVERSE_MULTIPLIER", "Время эволюции монстров", "Большие значения замедляют эволюцию; 0 отключает улучшения там, где это поддерживается." },
    { "SEASON_LENGTH", "Длина сезона (дни)", "Нужна перезагрузка мира. Количество дней в сезоне. Стандарт CDDA — 91." },
    { "CONSTRUCTION_SCALING", "Время строительства", "Процент от базового времени. 100% — стандарт; 50% — вдвое быстрее." },
    { "ETERNAL_SEASON", "Вечный сезон", "Нужна перезагрузка мира. Останавливает обычную смену сезонов." },
    { "ETERNAL_TIME_OF_DAY", "Фиксированное время суток", "Нужна перезагрузка мира. Обычный цикл, постоянный день или постоянная ночь." }
};

bool russian( const ncmm_host_api_v1 *api ) {
    const char *raw = api && api->get_locale ? api->get_locale() : "en";
    const std::string locale = raw ? raw : "en";
    return locale == "ru" || locale.rfind( "ru_", 0 ) == 0;
}
const char *tr( bool ru, const char *en, const char *ru_text ) { return ru ? ru_text : en; }

bool expose_range( const ncmm_host_api_v1 *api, const option_desc *options, std::size_t begin,
                   std::size_t end, const char *group_id, const char *group_name,
                   const char *group_tooltip ) {
    if( !api->worldgen_group_begin( group_id, group_name, group_tooltip ) ) return false;
    bool ok = true;
    for( std::size_t i = begin; i < end; ++i ) {
        if( !api->expose_worldgen_option( options[i].id, options[i].name, options[i].tooltip ) ) {
            ok = false; break;
        }
    }
    api->worldgen_group_end();
    return ok;
}

bool reg_bool( const ncmm_host_api_v1 *api, bool ru, const char *id, const char *en,
               const char *ru_name, const char *en_tip, const char *ru_tip, bool def ) {
    return host2->world_setting_register_bool( module_id, id, tr( ru, en, ru_name ),
            tr( ru, en_tip, ru_tip ), def ? 1 : 0, NCMM_WORLD_SETTING_NEW_MAP ) != 0;
}
bool reg_int( const ncmm_host_api_v1 *api, bool ru, const char *id, const char *en,
              const char *ru_name, const char *en_tip, const char *ru_tip,
              int minv, int maxv, int def ) {
    return host2->world_setting_register_int( module_id, id, tr( ru, en, ru_name ),
            tr( ru, en_tip, ru_tip ), minv, maxv, def, NCMM_WORLD_SETTING_NEW_MAP ) != 0;
}
bool reg_float( const ncmm_host_api_v1 *api, bool ru, const char *id, const char *en,
                const char *ru_name, const char *en_tip, const char *ru_tip,
                double minv, double maxv, double def, double step ) {
    return host2->world_setting_register_float( module_id, id, tr( ru, en, ru_name ),
            tr( ru, en_tip, ru_tip ), minv, maxv, def, step, NCMM_WORLD_SETTING_NEW_MAP ) != 0;
}

bool geography( const ncmm_host_api_v1 *api, bool ru ) {
    bool ok = true;
    auto begin = [&]( const char *id, const char *en, const char *ru_name, const char *en_tip, const char *ru_tip ) {
        return api->worldgen_experimental_group_begin( id, tr( ru, en, ru_name ),
                tr( ru, en_tip, ru_tip ) ) != 0;
    };
    auto end = [&]() { api->worldgen_group_end(); };

    if( !begin( "aws_geo_city", "Cities and infrastructure", "Города и инфраструктура",
                "Affects only areas generated after this change.",
                "Влияет только на новые области, созданные после изменения." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_CUSTOM_GEOGRAPHY","Use custom geography","Использовать свою географию","Leave this off to use the world's normal geography. Turn it on to customize newly generated areas. This overrides default-region geography values, including changes from region-overlay mods.","Оставьте выключенным для обычной географии мира. Включите, чтобы настраивать новые области. При этом значения географии региона default, включая изменения region-overlay модов, переопределяются.",false );
    ok &= reg_int( api,ru,"NCMM_AWS_CITY_SIZE","Base city size","Базовый размер города","0 disables random cities; default 8.","0 отключает случайные города; стандарт 8.",0,32,8 );
    ok &= reg_int( api,ru,"NCMM_AWS_CITY_SPACING","City spacing","Расстояние между городами","Higher values produce fewer cities; default 4.","Чем выше значение, тем реже города; стандарт 4.",0,8,4 );
    ok &= reg_int( api,ru,"NCMM_AWS_MAX_URBANITY","Maximum city growth","Максимальный рост городов","Limits how strongly regional generation can enlarge cities; default 8.","Ограничивает, насколько сильно региональные настройки могут увеличивать города; стандарт 8.",1,16,8 );
    ok &= reg_bool( api,ru,"NCMM_AWS_MEGACITY","Megacity generation","Мегаполис","Generates new areas as a dense megacity. This can noticeably increase generation time.","Новые области генерируются как плотный мегаполис. Это может заметно увеличить время генерации.",false );
    ok &= reg_int( api,ru,"NCMM_AWS_SHOP_RADIUS","Shop radius","Радиус магазинов","Controls how far from the city center shops may appear. Larger values spread shops farther out; 0 prevents shops from being placed by this rule. CDDA 0546 default is 30.","Определяет, насколько далеко от центра города могут появляться магазины. Чем выше значение, тем дальше они распространяются; 0 запрещает размещение магазинов по этому правилу. Стандарт CDDA 0546 — 30.",0,200,30 );
    ok &= reg_int( api,ru,"NCMM_AWS_SHOP_SIGMA","Shop spread","Разброс магазинов","Controls how widely shops are scattered around the city center. CDDA 0546 default is 50.","Определяет, насколько широко магазины распределяются вокруг центра города. Стандарт CDDA 0546 — 50.",0,200,50 );
    ok &= reg_int( api,ru,"NCMM_AWS_PARK_RADIUS","Park radius","Радиус парков","Controls how far from the city center parks may appear. Larger values spread parks farther out; 0 prevents parks from being placed by this rule. CDDA 0546 default is 20.","Определяет, насколько далеко от центра города могут появляться парки. Чем выше значение, тем дальше они распространяются; 0 запрещает размещение парков по этому правилу. Стандарт CDDA 0546 — 20.",0,200,20 );
    ok &= reg_int( api,ru,"NCMM_AWS_PARK_SIGMA","Park spread","Разброс парков","Controls how widely parks are scattered around the city center. CDDA 0546 default is 80.","Определяет, насколько широко парки распределяются вокруг центра города. Стандарт CDDA 0546 — 80.",0,200,80 );
    ok &= reg_bool( api,ru,"NCMM_AWS_PLACE_ROADS","Generate roads","Генерировать дороги","Disables new inter-city roads when off.","Отключает новые межгородские дороги.",true );
    ok &= reg_bool( api,ru,"NCMM_AWS_PLACE_RAILROADS","Generate railroads","Генерировать железные дороги","Controls railroad generation while custom geography is active. Vanilla CDDA 0546 default is off.","Управляет генерацией железных дорог при включённой своей географии. В стандартной CDDA 0546 по умолчанию выключено.",false );
    ok &= reg_bool( api,ru,"NCMM_AWS_PLACE_SPECIALS","Generate special locations","Генерировать особые локации","Controls placement of new special locations.","Управляет размещением новых особых локаций.",true );
    ok &= reg_bool( api,ru,"NCMM_AWS_NEIGHBOR_CONNECTIONS","Connect neighboring map regions","Связывать соседние области карты","Keeps roads, rail lines and rivers continuous across map-region borders.","Сохраняет непрерывность дорог, железных дорог и рек между областями карты.",true );
    end(); if( !ok ) return false;

    if( !begin( "aws_geo_forest", "Forests, swamps and trails", "Леса, болота и тропы",
                "Lower threshold values generate more of the selected terrain in new areas.",
                "Чем ниже порог, тем больше соответствующего ландшафта появится в новых областях." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_FORESTS","Generate forests","Генерировать леса","Turns forest generation on or off in new areas.","Включает или отключает леса в новых областях.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_FOREST_THRESHOLD","Forest threshold","Порог леса","Lower = more forest. CDDA 0546 default is 0.20.","Ниже = больше леса. Стандарт CDDA 0546 — 0,20.",0.0,1.0,0.20,0.01 );
    ok &= reg_float( api,ru,"NCMM_AWS_FOREST_THICK_THRESHOLD","Dense forest threshold","Порог густого леса","Lower = more dense forest. CDDA 0546 default is 0.25.","Ниже = больше густого леса. Стандарт CDDA 0546 — 0,25.",0.0,1.0,0.25,0.01 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_SWAMPS","Generate swamps","Генерировать болота","Turns swamp generation on or off in new areas.","Включает или отключает болота в новых областях.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_SWAMP_ADJ_THRESHOLD","Floodplain swamp threshold","Порог пойменных болот","Lower = more river-adjacent swamp. Default 0.30.","Ниже = больше болот у рек. Стандарт 0,30.",0.0,1.0,0.30,0.01 );
    ok &= reg_float( api,ru,"NCMM_AWS_SWAMP_ISOLATED_THRESHOLD","Isolated swamp threshold","Порог изолированных болот","Lower = more isolated swamp. Default 0.60.","Ниже = больше отдельных болот. Стандарт 0,60.",0.0,1.0,0.60,0.01 );
    ok &= reg_int( api,ru,"NCMM_AWS_FLOODPLAIN_MIN","Floodplain radius minimum","Минимальный радиус поймы","Minimum river floodplain buffer. Default 3.","Минимальный буфер поймы реки. Стандарт 3.",0,30,3 );
    ok &= reg_int( api,ru,"NCMM_AWS_FLOODPLAIN_MAX","Floodplain radius maximum","Максимальный радиус поймы","Maximum river floodplain buffer. Default 15.","Максимальный буфер поймы реки. Стандарт 15.",0,60,15 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_TRAILS","Generate forest trails","Генерировать лесные тропы","Turns forest trails and trailheads on or off in new areas.","Включает или отключает лесные тропы и выходы к дорогам в новых областях.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAIL_CHANCE","Forest trail chance (1 in X)","Шанс лесной тропы (1 из X)","1 means every qualifying forest; CDDA 0546 default is 2.","1 означает каждый подходящий лес; стандарт CDDA 0546 — 2.",1,32,2 );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAIL_MIN_FOREST","Minimum forest size for trails","Минимальный лес для троп","Minimum contiguous forest tiles; CDDA 0546 default is 100.","Минимальный размер связного леса; стандарт CDDA 0546 — 100.",1,1000,100 );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAILHEAD_CHANCE","Trailhead chance (1 in X)","Шанс входа на тропу (1 из X)","1 means every eligible trail end; default 1.","1 означает каждый подходящий конец тропы; стандарт 1.",1,32,1 );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAILHEAD_ROAD_DISTANCE","Trailhead road distance","Дистанция тропы до дороги","Maximum road-search radius for a trailhead; default 6.","Радиус поиска дороги для входа на тропу; стандарт 6.",1,30,6 );
    end(); if( !ok ) return false;

    if( !begin( "aws_geo_water", "Rivers, lakes and oceans", "Реки, озёра и океаны",
                "Controls rivers, lakes and oceans in newly generated areas.",
                "Настройки рек, озёр и океанов в новых областях." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_RIVERS","Generate rivers","Генерировать реки","Turns river generation on or off in new areas.","Включает или отключает реки в новых областях.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_RIVER_SCALE","River width scale","Масштаб ширины рек","0 disables rivers; default region value is 1.","0 отключает реки; стандарт региона 1.",0,5,1 );
    ok &= reg_float( api,ru,"NCMM_AWS_RIVER_FREQUENCY","River frequency","Частота рек","Higher = fewer new major rivers. Default 1.5.","Выше = меньше новых крупных рек. Стандарт 1,5.",1.0,8.0,1.5,0.1 );
    ok &= reg_int( api,ru,"NCMM_AWS_RIVER_BRANCH_CHANCE","River branch chance (1 in X)","Ветвление рек (1 из X)","Lower = more branches. Default 64.","Ниже = больше ответвлений. Стандарт 64.",1,256,64 );
    ok &= reg_int( api,ru,"NCMM_AWS_RIVER_REMERGE_CHANCE","Branch merging chance (1 in X)","Слияние рукавов (1 из X)","Lower = river branches merge back more often. Default 2.","Ниже = рукава рек чаще сливаются обратно. Стандарт 2.",1,64,2 );
    ok &= reg_float( api,ru,"NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE","Branch narrowing","Сужение рукавов","How much narrower each new river branch becomes. Default 1.0.","Насколько уже становится каждый новый рукав. Стандарт 1,0.",0.0,5.0,1.0,0.25 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_LAKES","Generate lakes","Генерировать озёра","Turns lake generation on or off in new areas.","Включает или отключает озёра в новых областях.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_LAKE_THRESHOLD","Lake threshold","Порог озёр","Lower = more lake terrain. Default 0.25.","Ниже = больше озёр. Стандарт 0,25.",0.0,1.0,0.25,0.01 );
    ok &= reg_int( api,ru,"NCMM_AWS_LAKE_MIN_SIZE","Minimum lake size","Минимальный размер озера","Lakes smaller than this are not generated. Default 20.","Озёра меньше этого размера не генерируются. Стандарт 20.",1,1000,20 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_OCEANS","Generate oceans","Генерировать океаны","Turns ocean generation on or off where the current region supports it.","Включает или отключает океаны там, где текущий регион их поддерживает.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_OCEAN_THRESHOLD","Ocean threshold","Порог океана","Lower = ocean expands more easily where coastline generation is active. Default 0.25.","Ниже = океан легче расширяется там, где активна береговая генерация. Стандарт 0,25.",0.0,1.0,0.25,0.01 );
    ok &= reg_int( api,ru,"NCMM_AWS_OCEAN_MIN_SIZE","Minimum ocean body size","Минимальный размер океана","Ocean areas smaller than this are not generated. Default 100.","Океанские области меньше этого размера не генерируются. Стандарт 100.",1,5000,100 );
    end(); if( !ok ) return false;

    if( !begin( "aws_geo_transport", "Highways and ravines", "Шоссе и овраги",
                "Controls highways and ravines in newly generated areas.",
                "Настройки шоссе и оврагов в новых областях." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_HIGHWAYS","Generate highways","Генерировать шоссе","Turns highway generation on or off in new areas.","Включает или отключает шоссе в новых областях.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_HIGHWAY_GRID_ROW","Highway row separation","Расстояние между горизонтальными шоссе","Distance between highway rows, measured in map regions. Default 8.","Расстояние между рядами шоссе в областях карты. Стандарт 8.",2,32,8 );
    ok &= reg_int( api,ru,"NCMM_AWS_HIGHWAY_GRID_COLUMN","Highway column separation","Расстояние между вертикальными шоссе","Distance between highway columns, measured in map regions. Default 10.","Расстояние между колоннами шоссе в областях карты. Стандарт 10.",2,32,10 );
    ok &= reg_int( api,ru,"NCMM_AWS_HIGHWAY_GRID_VARIANCE","Highway alignment variation","Разброс линий шоссе","How far highway intersections may shift from the grid. For safety the effective value is clamped to at most one quarter of the tighter grid spacing. Default 2.","Насколько перекрёстки могут смещаться относительно сетки. Для безопасности фактическое значение ограничивается четвертью меньшего шага сетки. Стандарт 2.",0,7,2 );
    ok &= reg_float( api,ru,"NCMM_AWS_HIGHWAY_STRAIGHTNESS","Highway endpoint randomness","Разброс концов шоссе","Higher = more random endpoint placement; lower = straighter alignment. CDDA 0546 underlying default is 0.60.","Выше = более случайное размещение концов шоссе; ниже = более прямое выравнивание. Базовое значение CDDA 0546 — 0,60.",0.0,1.0,0.60,0.05 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_RAVINES","Generate ravines","Генерировать овраги","Turns ravines on or off where the current region supports them.","Включает или отключает овраги там, где текущий регион их поддерживает.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_COUNT","Ravines per map region","Оврагов на область карты","0 disables ravines. Default region value is 0.","0 отключает овраги. В стандартном регионе по умолчанию 0.",0,16,0 );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_RANGE","Ravine length range","Длина оврага","Path displacement range. Default 45.","Диапазон смещения пути. Стандарт 45.",1,120,45 );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_WIDTH","Ravine width","Ширина оврага","Ravine width control. CDDA 0546 default is 3.","Управление шириной оврага. Стандарт CDDA 0546 — 3.",1,10,3 );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_DEPTH","Ravine depth Z-level","Глубина оврага по Z","Negative Z-level for ravine floor. Current supported CDDA hosts have 10 overmap levels below ground, so the safe range is -10 to -1. Default -3.","Отрицательный Z-уровень дна оврага. В текущих поддерживаемых версиях CDDA есть 10 уровней овермапа вниз, поэтому безопасный диапазон — от -10 до -1. Стандарт -3.",-10,-1,-3 );
    end();
    return ok;
}

struct geography_binding_v2 {
    const char *hook;
    const char *setting;
    uint32_t type;
};

bool bind_geography_hooks_v2()
{
    if( host2 == nullptr || !host2->worldgen_hook_bind_setting ) return false;
    const geography_binding_v2 bindings[] = {
        { "geography.custom.enabled", "NCMM_AWS_CUSTOM_GEOGRAPHY", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.city.size", "NCMM_AWS_CITY_SIZE", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.spacing", "NCMM_AWS_CITY_SPACING", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.max_urbanity", "NCMM_AWS_MAX_URBANITY", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.megacity", "NCMM_AWS_MEGACITY", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.city.shop_radius", "NCMM_AWS_SHOP_RADIUS", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.shop_sigma", "NCMM_AWS_SHOP_SIGMA", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.park_radius", "NCMM_AWS_PARK_RADIUS", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.park_sigma", "NCMM_AWS_PARK_SIGMA", NCMM_WORLDGEN_INT_V2 },
        { "geography.roads.enabled", "NCMM_AWS_PLACE_ROADS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.railroads.enabled", "NCMM_AWS_PLACE_RAILROADS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.specials.enabled", "NCMM_AWS_PLACE_SPECIALS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.neighbor_connections.enabled", "NCMM_AWS_NEIGHBOR_CONNECTIONS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.forests.enabled", "NCMM_AWS_ENABLE_FORESTS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.forests.threshold", "NCMM_AWS_FOREST_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.forests.thick_threshold", "NCMM_AWS_FOREST_THICK_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.swamps.enabled", "NCMM_AWS_ENABLE_SWAMPS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.swamps.adjacent_threshold", "NCMM_AWS_SWAMP_ADJ_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.swamps.isolated_threshold", "NCMM_AWS_SWAMP_ISOLATED_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.swamps.floodplain_min", "NCMM_AWS_FLOODPLAIN_MIN", NCMM_WORLDGEN_INT_V2 },
        { "geography.swamps.floodplain_max", "NCMM_AWS_FLOODPLAIN_MAX", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.enabled", "NCMM_AWS_ENABLE_TRAILS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.trails.chance", "NCMM_AWS_TRAIL_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.min_forest", "NCMM_AWS_TRAIL_MIN_FOREST", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.trailhead_chance", "NCMM_AWS_TRAILHEAD_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.road_distance", "NCMM_AWS_TRAILHEAD_ROAD_DISTANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.enabled", "NCMM_AWS_ENABLE_RIVERS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.rivers.scale", "NCMM_AWS_RIVER_SCALE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.frequency", "NCMM_AWS_RIVER_FREQUENCY", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.rivers.branch_chance", "NCMM_AWS_RIVER_BRANCH_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.remerge_chance", "NCMM_AWS_RIVER_REMERGE_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.branch_scale_decrease", "NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.lakes.enabled", "NCMM_AWS_ENABLE_LAKES", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.lakes.threshold", "NCMM_AWS_LAKE_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.lakes.min_size", "NCMM_AWS_LAKE_MIN_SIZE", NCMM_WORLDGEN_INT_V2 },
        { "geography.oceans.enabled", "NCMM_AWS_ENABLE_OCEANS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.oceans.threshold", "NCMM_AWS_OCEAN_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.oceans.min_size", "NCMM_AWS_OCEAN_MIN_SIZE", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.enabled", "NCMM_AWS_ENABLE_HIGHWAYS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.highways.grid_row", "NCMM_AWS_HIGHWAY_GRID_ROW", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.grid_column", "NCMM_AWS_HIGHWAY_GRID_COLUMN", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.grid_variance", "NCMM_AWS_HIGHWAY_GRID_VARIANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.straightness", "NCMM_AWS_HIGHWAY_STRAIGHTNESS", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.ravines.enabled", "NCMM_AWS_ENABLE_RAVINES", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.ravines.count", "NCMM_AWS_RAVINE_COUNT", NCMM_WORLDGEN_INT_V2 },
        { "geography.ravines.range", "NCMM_AWS_RAVINE_RANGE", NCMM_WORLDGEN_INT_V2 },
        { "geography.ravines.width", "NCMM_AWS_RAVINE_WIDTH", NCMM_WORLDGEN_INT_V2 },
        { "geography.ravines.depth", "NCMM_AWS_RAVINE_DEPTH", NCMM_WORLDGEN_INT_V2 },
    };
    for( const geography_binding_v2 &binding : bindings ) {
        if( !host2->worldgen_hook_bind_setting( module_id, binding.hook, binding.setting, binding.type ) ) return false;
    }
    return true;
}
int expose_all( const ncmm_host_api_v1 *api, bool log_errors ) {
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || !api->has_capability ||
        !api->has_capability( "world_settings.v2" ) ||
        !api->has_capability( "world_options.experimental.v1" ) ||
        !api->can_expose_worldgen_option || !api->expose_worldgen_option ||
        !api->worldgen_group_begin || !api->worldgen_experimental_group_begin ||
        !api->worldgen_group_end || !api->worldgen_set_string_choices ||
        !api->world_setting_register_bool ||
        !api->world_setting_register_int || !api->world_setting_register_float ) return 0;
    const option_desc *options = russian( api ) ? options_ru : options_en;
    const std::size_t count = sizeof( options_en ) / sizeof( options_en[0] );
    for( std::size_t i = 0; i < count; ++i ) {
        if( !api->can_expose_worldgen_option( options[i].id ) ) {
            if( log_errors ) api->log( NCMM_LOG_ERROR, "AWS: vanilla option preflight failed." );
            return 0;
        }
    }
    const char *time_ids[] = { "normal", "day", "night" };
    const char *time_en[] = { "Normal", "Day", "Night" };
    const char *time_ru[] = { "Обычное", "День", "Ночь" };
    if( !api->worldgen_set_string_choices( "ETERNAL_TIME_OF_DAY", time_ids,
            russian( api ) ? time_ru : time_en, 3 ) ) return 0;
    const bool ru = russian( api );
    if( !expose_range( api, options, 0, 5, "aws_difficulty",
            tr(ru,"Difficulty and population","Сложность и население"),
            tr(ru,"Independent controls that replace the single coarse difficulty preset.","Независимые настройки вместо одного грубого пресета сложности.") ) ) return 0;
    if( !expose_range( api, options, 5, count, "aws_time",
            tr(ru,"Time and calendar","Время и календарь"),
            tr(ru,"Calendar controls. Some require a world reload.","Настройки календаря. Некоторые требуют перезагрузки мира.") ) ) return 0;
    if( !geography( api, ru ) ) {
        if( log_errors ) api->log( NCMM_LOG_ERROR, "AWS: geography registration failed." );
        return 0;
    }
    return 1;
}

int init( const ncmm_host_api_v1 *api ) {
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || api->query_interface == nullptr ) return 0;
    host2 = static_cast<const ncmm_host_api_v2_core *>(
                api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 0u ) );
    if( host2 == nullptr || host2->api_major != 2u || !host2->worldgen_hook_bind_setting ) return 0;
    for( const char *capability : required_caps ) {
        if( !api->has_capability || !api->has_capability( capability ) ) return 0;
    }
    if( api->get_api_version_major && api->get_api_version_minor ) {
        if( api->get_api_version_major() != 1 || api->get_api_version_minor() < 9 ) return 0;
    }
    if( !expose_all( api, true ) || !bind_geography_hooks_v2() ) return 0;
    api->log( NCMM_LOG_INFO, "Advanced World Settings 0.6.3 initialized: Host API 2.0 typed settings + generic geography hooks active." );
    return 1;
}
void shutdown() { host2 = nullptr; }

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION, module_id, "Advanced World Settings", "0.6.3",
    required_caps, sizeof( required_caps ) / sizeof( required_caps[0] ), &init, &shutdown
};
}

extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1() { return &descriptor; }
extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1( const ncmm_host_api_v1 *api ) { expose_all( api, false ); }