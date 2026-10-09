#include "ncmm_sdk_core.hpp"
#include <string>
#include <stdexcept>

namespace {
constexpr const char *id = "legacy_character_points";
constexpr const char *version = "0.1.0";
const ncmm_host_api_v2_core *host = nullptr;
const char *caps[] = {"core.v1", "locale.v1", "api.versioning.v1", "host_api.v2.core",
    "settings.typed.v2", "runtime_settings.bindings.v2", "character_creation.points.v1"};
bool ru(const ncmm_host_api_v1 *api) {
    const char *s = api && api->get_locale ? api->get_locale() : nullptr;
    return s && std::string(s).rfind("ru", 0) == 0;
}
const char *tr(bool r, const char *en, const char *rus) { return r ? rus : en; }
bool settings(const ncmm_host_api_v1 *api) {
    if(!host || !host->world_setting_register_bool || !host->world_setting_register_enum ||
       !host->world_setting_register_int || !host->runtime_hook_bind_setting) return false;
    const bool r = ru(api);
    const char *ids[] = {"any", "multi_pool", "one_pool", "freeform"};
    const char *names[] = {tr(r,"Any (choose at creation)","Любая (выбор при создании)"),
        tr(r,"Legacy: Multiple pools","Классика: раздельные очки"),
        tr(r,"Legacy: Single pool","Классика: единые очки"),
        tr(r,"Survivor (freeform)","Выживший (без очков)")};
    if(!host->world_setting_register_bool(id,"NCMM_LCP_ENABLED",
        tr(r,"Legacy Character Points","Классические очки персонажа"),
        tr(r,"Restore classic character creation points. Applies only to new characters, not existing saves.",
             "Возвращает классические очки создания персонажа. Действует только на новых персонажей, без изменения сохранений."),
        1,NCMM_WORLD_SETTING_LIVE)) return false;
    if(!host->world_setting_register_enum(id,"NCMM_LCP_MODE",
        tr(r,"Character point pools","Система очков персонажа"),
        tr(r,"Any permits choosing a mode at creation. Multiple pools permits stat points to fund traits/skills and trait points to fund skills, never the reverse. Point modes replace rating-based limits.",
             "Любая — выбор режима при создании. Раздельные очки: характеристики оплачивают черты и навыки, черты оплачивают навыки, но не наоборот. В поинтовых режимах рейтинговые ограничения заменяются бюджетом."),
        ids,names,4,"any",NCMM_WORLD_SETTING_LIVE)) return false;
    struct integer_setting {const char *key; const char *hook; const char *en; const char *rus; int value;};
    const integer_setting integers[] = {
        {"NCMM_LCP_STAT_POINTS","chargen.points.stats","Initial stat points","Начальные очки характеристик",6},
        {"NCMM_LCP_TRAIT_POINTS","chargen.points.traits","Initial trait points","Начальные очки черт",0},
        {"NCMM_LCP_SKILL_POINTS","chargen.points.skills","Initial skill points","Начальные очки навыков",2},
        {"NCMM_LCP_TRAIT_CAP","chargen.points.trait_cap","Maximum trait points (each sign)","Лимит очков черт (для каждого знака)",12}
    };
    for(const auto &s : integers) {
        if(!host->world_setting_register_int(id,s.key,tr(r,s.en,s.rus),
            tr(r,"Classic budget for the next new character. Mandatory scenario/profession/background traits are not charged twice. Existing characters are unchanged.",
                 "Классический бюджет для следующего нового персонажа. Обязательные черты сценария, профессии и предыстории повторно не оплачиваются. Существующие персонажи не меняются."),
            0,1000,s.value,NCMM_WORLD_SETTING_LIVE)) return false;
        if(!host->runtime_hook_bind_setting(id,s.hook,s.key,NCMM_SETTING_INT_V2)) return false;
    }
    // Publish the enable binding last: a partially initialized module is never active.
    return host->runtime_hook_bind_setting(id,"chargen.points.enabled","NCMM_LCP_ENABLED",NCMM_SETTING_BOOL_V2)!=0;
}
int init(const ncmm_host_api_v1 *api) {
    host = nullptr;
    auto checked = ncmm::sdk::require_core(api,caps,sizeof(caps)/sizeof(caps[0]),
                   NCMM_SDK_CORE_FIELD_END(runtime_hook_i64));
    if(!checked || !checked.core->runtime_hook_i64 || !checked.core->runtime_hook_bool) return 0;
    host = checked.core;
    if(!settings(api)) {host=nullptr; return 0;}
    if(api->log) api->log(NCMM_LOG_INFO,"Legacy Character Points 0.1.0: classic chargen pools registered.");
    return 1;
}
void shutdown() { host=nullptr; }
const ncmm_mod_descriptor_v1 descriptor = {NCMM_ABI_VERSION,id,"Legacy Character Points",version,
    caps,sizeof(caps)/sizeof(caps[0]),&init,&shutdown};
}
extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1() {return &descriptor;}
extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1(const ncmm_host_api_v1 *api) {
    if(host && !settings(api)) throw std::runtime_error("Legacy Character Points setting refresh failed");
}
