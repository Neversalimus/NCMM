#define NCMM_MOD_BUILD
#include "ncmm_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
const char *const module_id = "survivor_progression";
constexpr int state_schema = 8;
constexpr int64_t mobility_steps_per_xp = 300;

const char *required_caps[] = {
    "core.v1",
    "locale.v1",
    "module_contract.v1",
    "events.turn.v1",
    "character_state.v1",
    "character.modifiers.v1",
    "ui.basic.v1",
    "ui.tiles.v1",
    "ui.cards.v1",
    "ui.tree.v1",
    "gameplay.metrics.v1",
    "active_mods.v1",
    "ui.theme.v1",
    "active_mods.registry.v2",
    "host_api.v2.core",
    "character.virtual_items.v1",
    "settings.typed.v2",
    "events.core.v2",
    "character.modifiers.v2",
    "runtime_hooks.registry.v2",
    "ui.layout.v1",
    "module_hotkeys.context.v1",
    "module_hotkeys.v1",
    "api.versioning.v1",
    "state.migration.v1",
    "module.lifecycle.v1"
};

const ncmm_host_api_v1 *host = nullptr;
const ncmm_host_api_v2_core *host2 = nullptr;
int turn_accumulator = 0;
bool last_character_available = false;
bool effects_dirty = true;
int current_xp_bonus_pct = 0;
int last_stat_power_pct = -1;

enum class branch_id {
    combat,
    survival,
    mobility,
    crafting,
    scavenging,
    mastery
};

enum class currency_id {
    perk,
    major
};

enum class perk_kind {
    stat,
    effect
};

enum class perk_scaling {
    fixed,
    per_active_branch,
    per_owned_major
};

struct modifier_effect {
    const char *id;
    double value;
};

struct perk_def {
    const char *id;
    branch_id branch;
    int tier;
    int required_level;
    currency_id currency;
    const char *prereq1;
    const char *prereq2;
    const char *name_en;
    const char *name_ru;
    const char *desc_en;
    const char *desc_ru;
    std::array<modifier_effect, 4> effects;
    int effect_count;
    int xp_bonus_pct;
    perk_kind kind = perk_kind::stat;
    perk_scaling scaling = perk_scaling::fixed;
    double branch_amp_pct = 0.0;
    double global_amp_pct = 0.0;
};

perk_kind effective_kind( const perk_def &perk )
{
    return perk.kind == perk_kind::effect || perk.xp_bonus_pct != 0 ?
           perk_kind::effect : perk_kind::stat;
}

const perk_def perks[] = {
    { "c_power", branch_id::combat, 1, 1, currency_id::perk, "", "", "Power Training", "Силовая подготовка", "+1 Strength", "+1 к силе", {{ { "str_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_footwork", branch_id::combat, 1, 1, currency_id::perk, "", "", "Combat Footwork", "Боевая работа ног", "+0.5 dodge", "+0,5 к уклонению", {{ { "dodge_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_precision", branch_id::combat, 2, 5, currency_id::perk, "c_power", "", "Precision", "Точность", "+0.5 melee hit", "+0,5 точности в ближнем бою", {{ { "melee_hit_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_reflexes", branch_id::combat, 2, 5, currency_id::perk, "c_footwork", "", "Reflex Drills", "Тренировка рефлексов", "+1 Dexterity", "+1 к ловкости", {{ { "dex_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_conditioning", branch_id::combat, 3, 10, currency_id::perk, "c_precision", "", "Combat Conditioning", "Боевая выносливость", "+8% maximum stamina", "+8% к максимуму выносливости", {{ { "stamina_max_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_tempo", branch_id::combat, 3, 10, currency_id::perk, "c_reflexes", "", "Battle Tempo", "Темп боя", "+3% speed", "+3% к скорости", {{ { "speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_bruiser", branch_id::combat, 4, 15, currency_id::perk, "c_conditioning", "", "Bruiser", "Громила", "+1 Strength, +0.5 melee hit", "+1 к силе, +0,5 точности", {{ { "str_flat", 1 }, { "melee_hit_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "c_evasion", branch_id::combat, 4, 15, currency_id::perk, "c_tempo", "", "Evasive Fighter", "Уклончивый боец", "+0.75 dodge, -3% move cost", "+0,75 к уклонению, -3% стоимости движения", {{ { "dodge_flat", 0.75 }, { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "c_veteran", branch_id::combat, 5, 20, currency_id::major, "c_bruiser", "c_evasion", "Veteran Fighter", "Ветеран", "+1 STR, +1 DEX, +0.5 melee hit", "+1 к силе, +1 к ловкости, +0,5 точности", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { "melee_hit_flat", 0.5 }, { nullptr, 0.0 } }}, 3, 0 },
    { "c_apex", branch_id::combat, 6, 30, currency_id::major, "c_veteran", "", "Elite Combatant", "Элитный боец", "+5% speed, +1 dodge, +10% stamina, +0.5 hit", "+5% скорости, +1 к уклонению, +10% выносливости, +0,5 к точности", {{ { "speed_pct", 5 }, { "dodge_flat", 1 }, { "stamina_max_pct", 10 }, { "melee_hit_flat", 0.5 } }}, 4, 0 },
    { "s_hardy", branch_id::survival, 1, 1, currency_id::perk, "", "", "Hardy", "Закалённый", "+6% maximum stamina", "+6% к максимуму выносливости", {{ { "stamina_max_pct", 6 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_field", branch_id::survival, 1, 1, currency_id::perk, "", "", "Field Medicine", "Полевая медицина", "+10% natural healing", "+10% естественного лечения", {{ { "healing_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_pack", branch_id::survival, 2, 5, currency_id::perk, "s_hardy", "", "Efficient Packing", "Грамотная укладка", "+10% carrying capacity", "+10% грузоподъёмности", {{ { "carry_weight_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_resilient", branch_id::survival, 2, 5, currency_id::perk, "s_field", "", "Resilient Body", "Живучий организм", "+15% natural healing", "+15% естественного лечения", {{ { "healing_pct", 15 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_endurance", branch_id::survival, 3, 10, currency_id::perk, "s_pack", "", "Long Haul", "Долгий путь", "+10% maximum stamina", "+10% к максимуму выносливости", {{ { "stamina_max_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_instinct", branch_id::survival, 3, 10, currency_id::perk, "s_resilient", "", "Survival Instinct", "Инстинкт выживания", "+1 Perception", "+1 к восприятию", {{ { "per_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_ironback", branch_id::survival, 4, 15, currency_id::perk, "s_endurance", "", "Iron Back", "Железная спина", "+15% carry, +1 Strength", "+15% грузоподъёмности, +1 к силе", {{ { "carry_weight_pct", 15 }, { "str_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "s_recovery", branch_id::survival, 4, 15, currency_id::perk, "s_instinct", "", "Rapid Recovery", "Быстрое восстановление", "+20% healing, +5% maximum stamina", "+20% лечения, +5% максимальной выносливости", {{ { "healing_pct", 20 }, { "stamina_max_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "s_survivor", branch_id::survival, 5, 20, currency_id::major, "s_ironback", "s_recovery", "True Survivor", "Настоящий выживший", "+15% stamina, +10% carry, +15% healing", "+15% выносливости, +10% грузоподъёмности, +15% лечения", {{ { "stamina_max_pct", 15 }, { "carry_weight_pct", 10 }, { "healing_pct", 15 }, { nullptr, 0.0 } }}, 3, 0 },
    { "s_unbreakable", branch_id::survival, 6, 30, currency_id::major, "s_survivor", "", "Unbreakable", "Несломленный", "+15% stamina, +20% healing, +1 STR, +10% carry", "+15% выносливости, +20% лечения, +1 к силе, +10% грузоподъёмности", {{ { "stamina_max_pct", 15 }, { "healing_pct", 20 }, { "str_flat", 1 }, { "carry_weight_pct", 10 } }}, 4, 0 },
    { "m_light", branch_id::mobility, 1, 1, currency_id::perk, "", "", "Light Step", "Лёгкий шаг", "-3% move cost", "-3% стоимости движения", {{ { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_cardio", branch_id::mobility, 1, 1, currency_id::perk, "", "", "Cardio Base", "Кардиобаза", "+6% maximum stamina", "+6% к максимуму выносливости", {{ { "stamina_max_pct", 6 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_stride", branch_id::mobility, 2, 5, currency_id::perk, "m_light", "", "Efficient Stride", "Эффективный шаг", "+3% speed", "+3% к скорости", {{ { "speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_breath", branch_id::mobility, 2, 5, currency_id::perk, "m_cardio", "", "Deep Reserve", "Глубокий резерв", "+8% maximum stamina", "+8% к максимуму выносливости", {{ { "stamina_max_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_parkour", branch_id::mobility, 3, 10, currency_id::perk, "m_stride", "", "Parkour Habit", "Привычка к паркуру", "-5% move cost, +1 Dexterity", "-5% стоимости движения, +1 к ловкости", {{ { "move_cost_pct", -5 }, { "dex_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "m_quick", branch_id::mobility, 3, 10, currency_id::perk, "m_breath", "", "Quick Recovery", "Второе дыхание", "+4% speed", "+4% к скорости", {{ { "speed_pct", 4 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_runner", branch_id::mobility, 4, 15, currency_id::perk, "m_parkour", "", "Runner", "Бегун", "+5% speed, -3% move cost", "+5% скорости, -3% стоимости движения", {{ { "speed_pct", 5 }, { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "m_marathon", branch_id::mobility, 4, 15, currency_id::perk, "m_quick", "", "Marathoner", "Марафонец", "+12% maximum stamina, -2% move cost", "+12% выносливости, -2% стоимости движения", {{ { "stamina_max_pct", 12 }, { "move_cost_pct", -2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "m_flow", branch_id::mobility, 5, 20, currency_id::major, "m_runner", "m_marathon", "Flow State", "Состояние потока", "+5% speed, +1 DEX, -5% move cost", "+5% скорости, +1 к ловкости, -5% стоимости движения", {{ { "speed_pct", 5 }, { "dex_flat", 1 }, { "move_cost_pct", -5 }, { nullptr, 0.0 } }}, 3, 0 },
    { "m_untouchable", branch_id::mobility, 6, 30, currency_id::major, "m_flow", "", "Untouchable", "Неуловимый", "+7% speed, +1 dodge, -5% move cost, +8% stamina", "+7% скорости, +1 к уклонению, -5% стоимости движения, +8% выносливости", {{ { "speed_pct", 7 }, { "dodge_flat", 1 }, { "move_cost_pct", -5 }, { "stamina_max_pct", 8 } }}, 4, 0 },
    { "f_hands", branch_id::crafting, 1, 1, currency_id::perk, "", "", "Practiced Hands", "Набитая рука", "+5% crafting speed", "+5% скорости крафта", {{ { "craft_speed_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_reader", branch_id::crafting, 1, 1, currency_id::perk, "", "", "Focused Reading", "Сосредоточенное чтение", "+10% reading speed", "+10% скорости чтения", {{ { "read_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_efficiency", branch_id::crafting, 2, 5, currency_id::perk, "f_hands", "", "Workshop Rhythm", "Ритм мастерской", "+8% crafting speed", "+8% скорости крафта", {{ { "craft_speed_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_study", branch_id::crafting, 2, 5, currency_id::perk, "f_reader", "", "Study Habit", "Привычка учиться", "+1 Intelligence", "+1 к интеллекту", {{ { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_workflow", branch_id::crafting, 3, 10, currency_id::perk, "f_efficiency", "", "Workshop Routine", "Отлаженная работа", "+10% crafting speed", "+10% скорости крафта", {{ { "craft_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_quickstudy", branch_id::crafting, 3, 10, currency_id::perk, "f_study", "", "Quick Study", "Быстрое обучение", "+15% reading speed", "+15% скорости чтения", {{ { "read_speed_pct", 15 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_engineer", branch_id::crafting, 4, 15, currency_id::perk, "f_workflow", "", "Engineer", "Инженер", "+10% crafting speed, +1 INT", "+10% скорости крафта, +1 к интеллекту", {{ { "craft_speed_pct", 10 }, { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "f_scholar", branch_id::crafting, 4, 15, currency_id::perk, "f_quickstudy", "", "Scholar", "Учёный", "+20% reading speed, +1 INT", "+20% скорости чтения, +1 к интеллекту", {{ { "read_speed_pct", 20 }, { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "f_master", branch_id::crafting, 5, 20, currency_id::major, "f_engineer", "f_scholar", "Master Artisan", "Мастер", "+15% craft, +10% read, +1 INT", "+15% скорости крафта, +10% скорости чтения, +1 к интеллекту", {{ { "craft_speed_pct", 15 }, { "read_speed_pct", 10 }, { "int_flat", 1 }, { nullptr, 0.0 } }}, 3, 0 },
    { "f_genius", branch_id::crafting, 6, 30, currency_id::major, "f_master", "", "Technical Genius", "Технический гений", "+20% craft, +20% read, +1 INT, +10% carry", "+20% скорости крафта, +20% скорости чтения, +1 к интеллекту, +10% грузоподъёмности", {{ { "craft_speed_pct", 20 }, { "read_speed_pct", 20 }, { "int_flat", 1 }, { "carry_weight_pct", 10 } }}, 4, 0 },
    { "g_observer", branch_id::scavenging, 1, 1, currency_id::perk, "", "", "Observer", "Наблюдатель", "+1 Perception", "+1 к восприятию", {{ { "per_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_hauler", branch_id::scavenging, 1, 1, currency_id::perk, "", "", "Hauler", "Носильщик", "+10% carrying capacity", "+10% грузоподъёмности", {{ { "carry_weight_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_route", branch_id::scavenging, 2, 5, currency_id::perk, "g_observer", "", "Route Sense", "Чувство маршрута", "-3% move cost", "-3% стоимости движения", {{ { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_pack", branch_id::scavenging, 2, 5, currency_id::perk, "g_hauler", "", "Pack Expert", "Эксперт по укладке", "+10% carrying capacity", "+10% грузоподъёмности", {{ { "carry_weight_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_awareness", branch_id::scavenging, 3, 10, currency_id::perk, "g_route", "", "Situational Awareness", "Ситуационная осведомлённость", "+1 PER, +2% speed", "+1 к восприятию, +2% скорости", {{ { "per_flat", 1 }, { "speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "g_endurance", branch_id::scavenging, 3, 10, currency_id::perk, "g_pack", "", "Loaded March", "Марш с грузом", "+7% maximum stamina", "+7% к максимуму выносливости", {{ { "stamina_max_pct", 7 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_pathfinder", branch_id::scavenging, 4, 15, currency_id::perk, "g_awareness", "", "Pathfinder", "Следопыт", "-5% move cost, +1 PER", "-5% стоимости движения, +1 к восприятию", {{ { "move_cost_pct", -5 }, { "per_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "g_mule", branch_id::scavenging, 4, 15, currency_id::perk, "g_endurance", "", "Human Mule", "Вьючный человек", "+15% carry, +1 STR", "+15% грузоподъёмности, +1 к силе", {{ { "carry_weight_pct", 15 }, { "str_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "g_raider", branch_id::scavenging, 5, 20, currency_id::major, "g_pathfinder", "g_mule", "Veteran Scavenger", "Опытный добытчик", "+1 PER, +15% carry, +3% speed", "+1 к восприятию, +15% грузоподъёмности, +3% скорости", {{ { "per_flat", 1 }, { "carry_weight_pct", 15 }, { "speed_pct", 3 }, { nullptr, 0.0 } }}, 3, 0 },
    { "g_legend", branch_id::scavenging, 6, 30, currency_id::major, "g_raider", "", "Wasteland Scavenger", "Легенда пустошей", "+1 PER, +20% carry, -5% move cost, +3% speed", "+1 к восприятию, +20% грузоподъёмности, -5% стоимости движения, +3% скорости", {{ { "per_flat", 1 }, { "carry_weight_pct", 20 }, { "move_cost_pct", -5 }, { "speed_pct", 3 } }}, 4, 0 },
    { "a_fast", branch_id::mastery, 1, 1, currency_id::perk, "", "", "Fast Learner", "Быстрый ученик", "+50% Survivor XP", "+50% опыта Survivor", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 50 },
    { "a_focus", branch_id::mastery, 1, 1, currency_id::perk, "", "", "Focused Mind", "Собранный ум", "+1 Intelligence", "+1 к интеллекту", {{ { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "a_adapt", branch_id::mastery, 2, 5, currency_id::perk, "a_fast", "", "Adaptive Learning", "Адаптивное обучение", "+25% Survivor XP", "+25% опыта Survivor", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 25 },
    { "a_balance", branch_id::mastery, 2, 5, currency_id::perk, "a_focus", "", "Balanced Growth", "Сбалансированное развитие", "+1 STR, +1 DEX", "+1 к силе, +1 к ловкости", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "a_learning", branch_id::mastery, 3, 10, currency_id::perk, "a_adapt", "", "Practice Pays Off", "Учёба на практике", "+25% Survivor XP, +5% crafting", "+25% опыта Survivor, +5% крафта", {{ { "craft_speed_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 25 },
    { "a_insight", branch_id::mastery, 3, 10, currency_id::perk, "a_balance", "", "Insight", "Проницательность", "+1 PER, +1 INT", "+1 к восприятию, +1 к интеллекту", {{ { "per_flat", 1 }, { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "a_growth", branch_id::mastery, 4, 15, currency_id::perk, "a_learning", "", "Accelerated Growth", "Ускоренный рост", "+25% Survivor XP, +5% stamina", "+25% опыта Survivor, +5% выносливости", {{ { "stamina_max_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 25 },
    { "a_polymath", branch_id::mastery, 4, 15, currency_id::perk, "a_insight", "", "Polymath", "Универсал", "+10% craft, +10% reading", "+10% скорости крафта, +10% скорости чтения", {{ { "craft_speed_pct", 10 }, { "read_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "a_paragon", branch_id::mastery, 5, 20, currency_id::major, "a_growth", "a_polymath", "Paragon", "Образец", "+1 STR, +1 DEX, +1 PER, +1 INT", "+1 ко всем основным характеристикам", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { "per_flat", 1 }, { "int_flat", 1 } }}, 4, 0 },
    { "a_transcendent", branch_id::mastery, 6, 30, currency_id::major, "a_paragon", "", "Transcendent Survivor", "Совершенный выживший", "+50% XP, +3% speed, +10% stamina, +10% healing", "+50% опыта, +3% скорости, +10% выносливости, +10% лечения", {{ { "speed_pct", 3 }, { "stamina_max_pct", 10 }, { "healing_pct", 10 }, { nullptr, 0.0 } }}, 3, 50 }
,
    { "ce_rhythm", branch_id::combat, 1, 3, currency_id::perk, "c_power", "", "Combat Rhythm", "Боевой ритм", "Direct bonuses from Combat perks are 5% stronger.", "Прямые бонусы боевых перков на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "ce_drills", branch_id::combat, 1, 6, currency_id::perk, "c_footwork", "", "Drilled Reflexes", "Отработанные рефлексы", "+0.25 dodge and +0.25 melee hit.", "+0,25 к уклонению и +0,25 точности ближнего боя.", {{ { "dodge_flat", 0.25 }, { "melee_hit_flat", 0.25 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ce_reserve", branch_id::combat, 2, 9, currency_id::perk, "c_precision", "ce_rhythm", "Reserve Under Fire", "Резерв под огнём", "+2% max stamina for each Survivor branch you've invested in.", "+2% максимума выносливости за каждую ветку Survivor с купленными перками.", {{ { "stamina_max_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ce_lessons", branch_id::combat, 2, 12, currency_id::perk, "c_reflexes", "ce_drills", "Lessons of Violence", "Уроки боя", "+4% Survivor XP for each major perk you own.", "+4% опыта Survivor за каждый купленный большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 4, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ce_tactics", branch_id::combat, 3, 15, currency_id::major, "c_conditioning", "ce_reserve", "Combat Doctrine", "Боевая доктрина", "Direct bonuses from Combat perks are another 10% stronger.", "Прямые бонусы боевых перков ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "ce_pressure", branch_id::combat, 3, 18, currency_id::perk, "c_tempo", "ce_lessons", "Relentless Pressure", "Непрерывный натиск", "+1% speed for each Survivor branch you've invested in.", "+1% скорости за каждую ветку Survivor с купленными перками.", {{ { "speed_pct", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ce_memory", branch_id::combat, 4, 22, currency_id::perk, "c_bruiser", "ce_tactics", "Battle Memory", "Боевая память", "+3% Survivor XP for each Survivor branch you've invested in.", "+3% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ce_refined", branch_id::combat, 4, 26, currency_id::perk, "c_evasion", "ce_pressure", "Refined Drills", "Отточенная подготовка", "+0.5 melee hit and +0.5 dodge.", "+0,5 точности ближнего боя и +0,5 к уклонению.", {{ { "melee_hit_flat", 0.5 }, { "dodge_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ce_veteran_reflex", branch_id::combat, 5, 32, currency_id::perk, "ce_memory", "ce_refined", "Veteran Reflex", "Рефлекс ветерана", "+3% speed, +5% max stamina, +0.25 dodge.", "+3% скорости, +5% выносливости, +0,25 к уклонению.", {{ { "speed_pct", 3 }, { "stamina_max_pct", 5 }, { "dodge_flat", 0.25 }, { nullptr, 0.0 } }}, 3, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ce_warmaster", branch_id::combat, 6, 40, currency_id::major, "c_apex", "ce_veteran_reflex", "Warmaster", "Воевода", "Direct bonuses from all Survivor perks are 5% stronger.", "Прямые бонусы всех перков Survivor на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "se_lessons", branch_id::survival, 1, 3, currency_id::perk, "s_hardy", "", "Hard Lessons", "Тяжёлые уроки", "Direct bonuses from Survival perks are 5% stronger.", "Прямые бонусы перков выживания на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "se_routine", branch_id::survival, 1, 6, currency_id::perk, "s_field", "", "Survival Routine", "Режим выживания", "+5% healing for each Survivor branch you've invested in.", "+5% лечения за каждую ветку Survivor с купленными перками.", {{ { "healing_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_reserves", branch_id::survival, 2, 9, currency_id::perk, "s_pack", "se_lessons", "Deep Reserves", "Глубокие резервы", "+2% max stamina for each Survivor branch you've invested in.", "+2% выносливости за каждую ветку Survivor с купленными перками.", {{ { "stamina_max_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_adaptive", branch_id::survival, 2, 12, currency_id::perk, "s_resilient", "se_routine", "Adaptive Survivor", "Адаптивный выживший", "+4% Survivor XP for each major perk you own.", "+4% опыта Survivor за каждый купленный большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 4, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "se_anchor", branch_id::survival, 3, 15, currency_id::major, "s_endurance", "se_reserves", "Anchor Point", "Точка опоры", "Direct bonuses from Survival perks are another 10% stronger.", "Прямые бонусы перков выживания ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "se_memory", branch_id::survival, 3, 18, currency_id::perk, "s_instinct", "se_adaptive", "Long Memory", "Долгая память", "+3% carry capacity for each Survivor branch you've invested in.", "+3% грузоподъёмности за каждую ветку Survivor с купленными перками.", {{ { "carry_weight_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_hardened", branch_id::survival, 4, 22, currency_id::perk, "s_ironback", "se_anchor", "Hardened Practice", "Закалённая практика", "+4% healing for each major perk you own.", "+4% лечения за каждый купленный большой перк.", {{ { "healing_pct", 4 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "se_grit", branch_id::survival, 4, 26, currency_id::perk, "s_recovery", "se_memory", "Grit", "Стойкость", "+10% healing and +8% max stamina.", "+10% лечения и +8% максимума выносливости.", {{ { "healing_pct", 10 }, { "stamina_max_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "se_carried", branch_id::survival, 5, 32, currency_id::perk, "se_hardened", "se_grit", "Lessons Carried", "Накопленный опыт", "+3% Survivor XP for each Survivor branch you've invested in.", "+3% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_indomitable", branch_id::survival, 6, 40, currency_id::major, "s_unbreakable", "se_carried", "Indomitable", "Несгибаемый", "Direct bonuses from all Survivor perks are 5% stronger.", "Прямые бонусы всех перков Survivor на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "me_economy", branch_id::mobility, 1, 3, currency_id::perk, "m_light", "", "Motion Economy", "Экономия движения", "Direct bonuses from Mobility perks are 5% stronger.", "Прямые бонусы перков мобильности на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "me_practice", branch_id::mobility, 1, 6, currency_id::perk, "m_cardio", "", "Kinetic Practice", "Кинетическая практика", "-1% move cost for each Survivor branch you've invested in.", "-1% стоимости движения за каждую ветку Survivor с купленными перками.", {{ { "move_cost_pct", -1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_breath", branch_id::mobility, 2, 9, currency_id::perk, "m_stride", "me_economy", "Breath Cycle", "Цикл дыхания", "+2% max stamina for each Survivor branch you've invested in.", "+2% выносливости за каждую ветку Survivor с купленными перками.", {{ { "stamina_max_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_road", branch_id::mobility, 2, 12, currency_id::perk, "m_breath", "me_practice", "Road Sense", "Чувство дороги", "+3% Survivor XP for each Survivor branch you've invested in.", "+3% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_flow", branch_id::mobility, 3, 15, currency_id::major, "m_parkour", "me_breath", "Motion Discipline", "Дисциплина движения", "Direct bonuses from Mobility perks are another 10% stronger.", "Прямые бонусы перков мобильности ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "me_stride", branch_id::mobility, 3, 18, currency_id::perk, "m_quick", "me_road", "Long Stride", "Длинный шаг", "+1% speed for each Survivor branch you've invested in.", "+1% скорости за каждую ветку Survivor с купленными перками.", {{ { "speed_pct", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_mastery", branch_id::mobility, 4, 22, currency_id::perk, "m_runner", "me_flow", "Kinetic Rhythm", "Ритм движения", "-0.5% move cost for each major perk you own.", "-0,5% стоимости движения за каждый купленный большой перк.", {{ { "move_cost_pct", -0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "me_feather", branch_id::mobility, 4, 26, currency_id::perk, "m_marathon", "me_stride", "Featherstep", "Невесомый шаг", "+0.5 dodge and +2% speed.", "+0,5 к уклонению и +2% скорости.", {{ { "dodge_flat", 0.5 }, { "speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "me_endless", branch_id::mobility, 5, 32, currency_id::perk, "me_mastery", "me_feather", "Endless Road", "Бесконечная дорога", "+3% Survivor XP for each Survivor branch you've invested in.", "+3% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_horizon", branch_id::mobility, 6, 40, currency_id::major, "m_untouchable", "me_endless", "Horizon Runner", "Бегущий к горизонту", "Direct bonuses from all Survivor perks are 5% stronger.", "Прямые бонусы всех перков Survivor на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "fe_iterate", branch_id::crafting, 1, 3, currency_id::perk, "f_hands", "", "Iterative Practice", "Практика итераций", "Direct bonuses from Crafting perks are 5% stronger.", "Прямые бонусы перков крафта на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "fe_method", branch_id::crafting, 1, 6, currency_id::perk, "f_reader", "", "Methodical Work", "Методичная работа", "+3% crafting speed for each Survivor branch you've invested in.", "+3% скорости крафта за каждую ветку Survivor с купленными перками.", {{ { "craft_speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "fe_notes", branch_id::crafting, 2, 9, currency_id::perk, "f_efficiency", "fe_iterate", "Living Notes", "Живые заметки", "+3% reading speed for each Survivor branch you've invested in.", "+3% скорости чтения за каждую ветку Survivor с купленными перками.", {{ { "read_speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "fe_learning", branch_id::crafting, 2, 12, currency_id::perk, "f_study", "fe_method", "Learning by Making", "Учёба делом", "+4% Survivor XP for each major perk you own.", "+4% опыта Survivor за каждый купленный большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 4, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "fe_breakthrough", branch_id::crafting, 3, 15, currency_id::major, "f_workflow", "fe_notes", "Breakthrough", "Прорыв", "Direct bonuses from Crafting perks are another 10% stronger.", "Прямые бонусы перков крафта ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "fe_standard", branch_id::crafting, 3, 18, currency_id::perk, "f_quickstudy", "fe_learning", "Workshop Standards", "Стандарты мастерской", "+2% crafting speed for each major perk you own.", "+2% скорости крафта за каждый купленный большой перк.", {{ { "craft_speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "fe_systems", branch_id::crafting, 4, 22, currency_id::perk, "f_engineer", "fe_breakthrough", "Technical Insight", "Техническое чутьё", "+2% reading speed for each major perk you own.", "+2% скорости чтения за каждый купленный большой перк.", {{ { "read_speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "fe_theory", branch_id::crafting, 4, 26, currency_id::perk, "f_scholar", "fe_standard", "Theory Into Practice", "Теория в практике", "+10% crafting and +10% reading speed.", "+10% скорости крафта и +10% скорости чтения.", {{ { "craft_speed_pct", 10 }, { "read_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "fe_tuning", branch_id::crafting, 5, 32, currency_id::perk, "fe_systems", "fe_theory", "Fine Tuning", "Тонкая настройка", "+3% Survivor XP for each Survivor branch you've invested in.", "+3% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "fe_architect", branch_id::crafting, 6, 40, currency_id::major, "f_genius", "fe_tuning", "Architect Mind", "Разум архитектора", "Direct bonuses from all Survivor perks are 5% stronger.", "Прямые бонусы всех перков Survivor на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ge_eye", branch_id::scavenging, 1, 3, currency_id::perk, "g_observer", "", "Sharp Eye", "Острый глаз", "Direct bonuses from Scavenging perks are 5% stronger.", "Прямые бонусы перков добычи на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "ge_routes", branch_id::scavenging, 1, 6, currency_id::perk, "g_hauler", "", "Efficient Routes", "Экономные маршруты", "-0.75% move cost for each Survivor branch you've invested in.", "-0,75% стоимости движения за каждую ветку Survivor с купленными перками.", {{ { "move_cost_pct", -0.75 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_load", branch_id::scavenging, 2, 9, currency_id::perk, "g_route", "ge_eye", "Load Planning", "Планирование груза", "+4% carry capacity for each Survivor branch you've invested in.", "+4% грузоподъёмности за каждую ветку Survivor с купленными перками.", {{ { "carry_weight_pct", 4 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_field", branch_id::scavenging, 2, 12, currency_id::perk, "g_pack", "ge_routes", "Field Experience", "Полевой опыт", "+3% Survivor XP for each Survivor branch you've invested in.", "+3% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_opportunist", branch_id::scavenging, 3, 15, currency_id::major, "g_awareness", "ge_load", "Opportunist", "Оппортунист", "Direct bonuses from Scavenging perks are another 10% stronger.", "Прямые бонусы перков добычи ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "ge_cache", branch_id::scavenging, 3, 18, currency_id::perk, "g_endurance", "ge_field", "Cache Logic", "Логика тайников", "+0.25 PER for each Survivor branch you've invested in.", "+0,25 восприятия за каждую ветку Survivor с купленными перками.", {{ { "per_flat", 0.25 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_network", branch_id::scavenging, 4, 22, currency_id::perk, "g_pathfinder", "ge_opportunist", "Networked Routes", "Сеть маршрутов", "+2% speed for each major perk you own.", "+2% скорости за каждый купленный большой перк.", {{ { "speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ge_instinct", branch_id::scavenging, 4, 26, currency_id::perk, "g_mule", "ge_cache", "Scavenger Instinct", "Инстинкт добытчика", "+10% carry capacity and +0.5 PER.", "+10% грузоподъёмности и +0,5 восприятия.", {{ { "carry_weight_pct", 10 }, { "per_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ge_wisdom", branch_id::scavenging, 5, 32, currency_id::perk, "ge_network", "ge_instinct", "Long Haul Wisdom", "Мудрость дальних рейдов", "+3% Survivor XP for each major perk you own.", "+3% опыта Survivor за каждый купленный большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ge_nomad", branch_id::scavenging, 6, 40, currency_id::major, "g_legend", "ge_wisdom", "Nomad Legend", "Легенда кочевника", "Direct bonuses from all Survivor perks are 5% stronger.", "Прямые бонусы всех перков Survivor на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ae_reflect", branch_id::mastery, 1, 3, currency_id::perk, "a_fast", "", "Reflection", "Рефлексия", "Direct bonuses from all Survivor perks are 2% stronger.", "Прямые бонусы всех перков Survivor на 2% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 2 },
    { "ae_cross", branch_id::mastery, 1, 6, currency_id::perk, "a_focus", "", "Cross Training", "Перекрёстная подготовка", "+5% Survivor XP for each Survivor branch you've invested in.", "+5% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 5, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_foundation", branch_id::mastery, 2, 9, currency_id::perk, "a_adapt", "ae_reflect", "Strong Foundation", "Прочный фундамент", "+0.15 STR/DEX/PER/INT for each Survivor branch you've invested in.", "+0,15 СИЛ/ЛОВ/ВОС/ИНТ за каждую ветку Survivor с купленными перками.", {{ { "str_flat", 0.15 }, { "dex_flat", 0.15 }, { "per_flat", 0.15 }, { "int_flat", 0.15 } }}, 4, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_pattern", branch_id::mastery, 2, 12, currency_id::perk, "a_balance", "ae_cross", "Pattern Recognition", "Распознавание закономерностей", "+3% Survivor XP for each major perk you own.", "+3% опыта Survivor за каждый купленный большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ae_milestone", branch_id::mastery, 3, 15, currency_id::major, "a_learning", "ae_foundation", "Milestone Focus", "Ориентир на результат", "Direct bonuses from all Survivor perks are another 5% stronger.", "Прямые бонусы всех перков Survivor ещё на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ae_integrate", branch_id::mastery, 3, 18, currency_id::perk, "a_insight", "ae_pattern", "Cross-Training", "Разносторонняя подготовка", "+2% crafting and reading speed for each Survivor branch you've invested in.", "+2% крафта и чтения за каждую ветку Survivor с купленными перками.", {{ { "craft_speed_pct", 2 }, { "read_speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_compound", branch_id::mastery, 4, 22, currency_id::perk, "a_growth", "ae_milestone", "Lessons Learned", "Усвоенные уроки", "Direct bonuses from all Survivor perks are another 5% stronger.", "Прямые бонусы всех перков Survivor ещё на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ae_longgame", branch_id::mastery, 4, 26, currency_id::perk, "a_polymath", "ae_integrate", "Long Game", "Долгая игра", "+6% Survivor XP for each Survivor branch you've invested in.", "+6% опыта Survivor за каждую ветку Survivor с купленными перками.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 6, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_legacy", branch_id::mastery, 5, 32, currency_id::perk, "ae_compound", "ae_longgame", "Legacy Mindset", "Мышление наследия", "+0.05 STR/DEX/PER/INT for each major perk you own.", "+0,05 СИЛ/ЛОВ/ВОС/ИНТ за каждый купленный большой перк.", {{ { "str_flat", 0.05 }, { "dex_flat", 0.05 }, { "per_flat", 0.05 }, { "int_flat", 0.05 } }}, 4, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ae_ascendant", branch_id::mastery, 6, 40, currency_id::major, "a_transcendent", "ae_legacy", "Ascendant", "Восхождение", "Direct bonuses from all Survivor perks are 10% stronger; Survivor XP +50%.", "Прямые бонусы всех перков Survivor на 10% сильнее; опыт Survivor +50%.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 50, perk_kind::effect, perk_scaling::fixed, 0, 10 },
    { "spc_c_juggernaut", branch_id::combat, 4, 15, currency_id::perk, "c_conditioning", "", "Prime Juggernaut", "Прайм: Штурмовик", "+2 STR, +25% stamina, +20% carry; -12% speed.", "+2 СИЛ, +25% выносливости, +20% грузоподъёмности; -12% скорости.", {{ { "str_flat", 2 }, { "stamina_max_pct", 25 }, { "carry_weight_pct", 20 }, { "speed_pct", -12 } }}, 4, 0, perk_kind::effect },
    { "spc_c_duelist", branch_id::combat, 4, 15, currency_id::perk, "c_tempo", "", "Prime Duelist", "Прайм: Дуэлянт", "+10% speed, +2 dodge, -10% move cost; -25% carry.", "+10% скорости, +2 к уклонению, -10% стоимости движения; -25% грузоподъёмности.", {{ { "speed_pct", 10 }, { "dodge_flat", 2 }, { "move_cost_pct", -10 }, { "carry_weight_pct", -25 } }}, 4, 0, perk_kind::effect },
    { "spc_c_tactician", branch_id::combat, 4, 15, currency_id::perk, "c_precision", "c_reflexes", "Prime Tactician", "Прайм: Тактик", "+2 PER, +1.5 melee hit, +5% speed; -20% stamina.", "+2 ВОС, +1,5 точности ближнего боя, +5% скорости; -20% выносливости.", {{ { "per_flat", 2 }, { "melee_hit_flat", 1.5 }, { "speed_pct", 5 }, { "stamina_max_pct", -20 } }}, 4, 0, perk_kind::effect },
    { "spc_c_juggernaut_cap", branch_id::combat, 5, 25, currency_id::major, "spc_c_juggernaut", "", "Iron Advance", "Железный натиск", "+1 STR, +12% stamina, +5% carry.", "+1 СИЛ, +12% выносливости, +5% грузоподъёмности.", {{ { "str_flat", 1 }, { "stamina_max_pct", 12 }, { "carry_weight_pct", 5 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_c_duelist_cap", branch_id::combat, 5, 25, currency_id::major, "spc_c_duelist", "", "Perfect Tempo", "Идеальный темп", "+5% speed, +1 dodge, -3% move cost.", "+5% скорости, +1 к уклонению, -3% стоимости движения.", {{ { "speed_pct", 5 }, { "dodge_flat", 1 }, { "move_cost_pct", -3 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_c_tactician_cap", branch_id::combat, 5, 25, currency_id::major, "spc_c_tactician", "", "Battlefield Control", "Контроль поля боя", "+1 PER, +0.75 melee hit, +3% speed.", "+1 ВОС, +0,75 точности, +3% скорости.", {{ { "per_flat", 1 }, { "melee_hit_flat", 0.75 }, { "speed_pct", 3 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_s_nomad", branch_id::survival, 4, 15, currency_id::perk, "s_endurance", "", "Prime Nomad", "Прайм: Кочевник", "+30% stamina, +30% carry; -12% speed.", "+30% выносливости, +30% грузоподъёмности; -12% скорости.", {{ { "stamina_max_pct", 30 }, { "carry_weight_pct", 30 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_s_medic", branch_id::survival, 4, 15, currency_id::perk, "s_field", "s_resilient", "Prime Field Medic", "Прайм: Полевой медик", "+60% healing, +15% stamina; -25% carry.", "+60% лечения, +15% выносливости; -25% грузоподъёмности.", {{ { "healing_pct", 60 }, { "stamina_max_pct", 15 }, { "carry_weight_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_s_quartermaster", branch_id::survival, 4, 15, currency_id::perk, "s_pack", "", "Prime Quartermaster", "Прайм: Интендант", "+40% carry, +25% crafting speed; -12% speed.", "+40% грузоподъёмности, +25% скорости крафта; -12% скорости.", {{ { "carry_weight_pct", 40 }, { "craft_speed_pct", 25 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_s_nomad_cap", branch_id::survival, 5, 25, currency_id::major, "spc_s_nomad", "", "Long Road", "Долгая дорога", "+15% stamina, +15% carry, -3% move cost.", "+15% выносливости, +15% грузоподъёмности, -3% стоимости движения.", {{ { "stamina_max_pct", 15 }, { "carry_weight_pct", 15 }, { "move_cost_pct", -3 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_s_medic_cap", branch_id::survival, 5, 25, currency_id::major, "spc_s_medic", "", "Trauma Veteran", "Ветеран травм", "+30% healing and +8% stamina.", "+30% лечения и +8% выносливости.", {{ { "healing_pct", 30 }, { "stamina_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_s_quartermaster_cap", branch_id::survival, 5, 25, currency_id::major, "spc_s_quartermaster", "", "Prepared for Anything", "Готов ко всему", "+25% carry and +8% crafting speed.", "+25% грузоподъёмности и +8% скорости крафта.", {{ { "carry_weight_pct", 25 }, { "craft_speed_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_sprinter", branch_id::mobility, 4, 15, currency_id::perk, "m_cardio", "", "Prime Sprinter", "Прайм: Спринтер", "+12% speed, -10% move cost; -35% carry.", "+12% скорости, -10% стоимости движения; -35% грузоподъёмности.", {{ { "speed_pct", 12 }, { "move_cost_pct", -10 }, { "carry_weight_pct", -35 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_m_ghost", branch_id::mobility, 4, 15, currency_id::perk, "m_light", "m_parkour", "Prime Ghost", "Прайм: Призрак", "-15% move cost, +2 dodge; -25% stamina.", "-15% стоимости движения, +2 к уклонению; -25% выносливости.", {{ { "move_cost_pct", -15 }, { "dodge_flat", 2 }, { "stamina_max_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_m_pathfinder", branch_id::mobility, 4, 15, currency_id::perk, "m_stride", "", "Prime Pathfinder", "Прайм: Путепроходец", "-12% move cost, +25% stamina, +20% carry; -25% healing.", "-12% стоимости движения, +25% выносливости, +20% грузоподъёмности; -25% лечения.", {{ { "move_cost_pct", -12 }, { "stamina_max_pct", 25 }, { "carry_weight_pct", 20 }, { "healing_pct", -25 } }}, 4, 0, perk_kind::effect },
    { "spc_m_sprinter_cap", branch_id::mobility, 5, 25, currency_id::major, "spc_m_sprinter", "", "Burst Engine", "Двигатель рывка", "+6% speed and +10% stamina.", "+6% скорости и +10% выносливости.", {{ { "speed_pct", 6 }, { "stamina_max_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_ghost_cap", branch_id::mobility, 5, 25, currency_id::major, "spc_m_ghost", "", "Vanishing Step", "Исчезающий шаг", "-7% move cost and +1 dodge.", "-7% стоимости движения и +1 к уклонению.", {{ { "move_cost_pct", -7 }, { "dodge_flat", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_pathfinder_cap", branch_id::mobility, 5, 25, currency_id::major, "spc_m_pathfinder", "", "Always a Route", "Путь всегда есть", "-5% move cost, +10% carry, +10% stamina.", "-5% стоимости движения, +10% грузоподъёмности, +10% выносливости.", {{ { "move_cost_pct", -5 }, { "carry_weight_pct", 10 }, { "stamina_max_pct", 10 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_f_systems", branch_id::crafting, 4, 15, currency_id::perk, "f_engineer", "", "Prime Systems Engineer", "Прайм: Системный инженер", "+30% crafting speed, +2 INT; -12% speed.", "+30% скорости крафта, +2 ИНТ; -12% скорости.", {{ { "craft_speed_pct", 30 }, { "int_flat", 2 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_f_improviser", branch_id::crafting, 4, 15, currency_id::perk, "f_hands", "f_workflow", "Prime Improviser", "Прайм: Импровизатор", "+25% crafting speed, +25% carry; -30% reading speed.", "+25% скорости крафта, +25% грузоподъёмности; -30% скорости чтения.", {{ { "craft_speed_pct", 25 }, { "carry_weight_pct", 25 }, { "read_speed_pct", -30 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_f_researcher", branch_id::crafting, 4, 15, currency_id::perk, "f_reader", "f_scholar", "Prime Researcher", "Прайм: Исследователь", "+35% reading speed, +2 INT; -20% crafting speed.", "+35% скорости чтения, +2 ИНТ; -20% скорости крафта.", {{ { "read_speed_pct", 35 }, { "int_flat", 2 }, { "craft_speed_pct", -20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_f_systems_cap", branch_id::crafting, 5, 25, currency_id::major, "spc_f_systems", "", "Chief Engineer", "Главный инженер", "+18% crafting speed and +1 INT.", "+18% скорости крафта и +1 ИНТ.", {{ { "craft_speed_pct", 18 }, { "int_flat", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_improviser_cap", branch_id::crafting, 5, 25, currency_id::major, "spc_f_improviser", "", "Make It Work", "Заставить работать", "+15% crafting speed and +10% carry.", "+15% скорости крафта и +10% грузоподъёмности.", {{ { "craft_speed_pct", 15 }, { "carry_weight_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_researcher_cap", branch_id::crafting, 5, 25, currency_id::major, "spc_f_researcher", "", "Applied Theory", "Прикладная теория", "+20% reading, +1 INT, +5% crafting speed.", "+20% чтения, +1 ИНТ, +5% скорости крафта.", {{ { "read_speed_pct", 20 }, { "int_flat", 1 }, { "craft_speed_pct", 5 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_g_prospector", branch_id::scavenging, 4, 15, currency_id::perk, "g_observer", "", "Prime Prospector", "Прайм: Искатель", "+2 PER, +25% carry; -12% speed.", "+2 ВОС, +25% грузоподъёмности; -12% скорости.", {{ { "per_flat", 2 }, { "carry_weight_pct", 25 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_g_courier", branch_id::scavenging, 4, 15, currency_id::perk, "g_pack", "g_endurance", "Prime Courier", "Прайм: Курьер", "+40% carry, -12% move cost; -1.5 PER.", "+40% грузоподъёмности, -12% стоимости движения; -1,5 ВОС.", {{ { "carry_weight_pct", 40 }, { "move_cost_pct", -12 }, { "per_flat", -1.5 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_g_investigator", branch_id::scavenging, 4, 15, currency_id::perk, "g_awareness", "", "Prime Investigator", "Прайм: Исследователь руин", "+2 PER, +30% reading speed; -25% carry.", "+2 ВОС, +30% скорости чтения; -25% грузоподъёмности.", {{ { "per_flat", 2 }, { "read_speed_pct", 30 }, { "carry_weight_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "spc_g_prospector_cap", branch_id::scavenging, 5, 25, currency_id::major, "spc_g_prospector", "", "Nothing Wasted", "Ничего не пропадает", "+1 PER and +10% carry.", "+1 ВОС и +10% грузоподъёмности.", {{ { "per_flat", 1 }, { "carry_weight_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_g_courier_cap", branch_id::scavenging, 5, 25, currency_id::major, "spc_g_courier", "", "Heavy Route", "Тяжёлый маршрут", "+25% carry, -4% move cost, +8% stamina.", "+25% грузоподъёмности, -4% стоимости движения, +8% выносливости.", {{ { "carry_weight_pct", 25 }, { "move_cost_pct", -4 }, { "stamina_max_pct", 8 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_g_investigator_cap", branch_id::scavenging, 5, 25, currency_id::major, "spc_g_investigator", "", "Read the Ruins", "Читать руины", "+1.5 PER, +10% reading, -2% move cost.", "+1,5 ВОС, +10% чтения, -2% стоимости движения.", {{ { "per_flat", 1.5 }, { "read_speed_pct", 10 }, { "move_cost_pct", -2 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_a_specialist", branch_id::mastery, 4, 15, currency_id::perk, "a_focus", "a_growth", "Prime Specialist", "Прайм: Специалист", "+30% Survivor XP and +1 INT; -1 STR, -1 DEX.", "+30% опыта Survivor и +1 ИНТ; -1 СИЛ, -1 ЛОВ.", {{ { "int_flat", 1 }, { "str_flat", -1 }, { "dex_flat", -1 }, { nullptr, 0 } }}, 3, 30, perk_kind::effect },
    { "spc_a_polymath", branch_id::mastery, 4, 15, currency_id::perk, "a_balance", "a_polymath", "Prime Polymath", "Прайм: Универсал", "+1 STR/DEX/PER/INT; -20% Survivor XP.", "+1 СИЛ/ЛОВ/ВОС/ИНТ; -20% опыта Survivor.", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { "per_flat", 1 }, { "int_flat", 1 } }}, 4, -20, perk_kind::effect },
    { "spc_a_selfteacher", branch_id::mastery, 4, 15, currency_id::perk, "a_adapt", "", "Prime Self-Teacher", "Прайм: Самоучка", "+25% reading, +25% crafting, +15% Survivor XP; -15% speed.", "+25% чтения, +25% крафта, +15% опыта Survivor; -15% скорости.", {{ { "read_speed_pct", 25 }, { "craft_speed_pct", 25 }, { "speed_pct", -15 }, { nullptr, 0 } }}, 3, 15, perk_kind::effect },
    { "spc_a_specialist_cap", branch_id::mastery, 5, 25, currency_id::major, "spc_a_specialist", "", "Dedicated Study", "Углублённое обучение", "+20% Survivor XP.", "+20% опыта Survivor.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 20 },
    { "spc_a_polymath_cap", branch_id::mastery, 5, 25, currency_id::major, "spc_a_polymath", "", "Cross Discipline", "Перекрёстная дисциплина", "+0.5 to all primary stats.", "+0,5 ко всем основным характеристикам.", {{ { "str_flat", 0.5 }, { "dex_flat", 0.5 }, { "per_flat", 0.5 }, { "int_flat", 0.5 } }}, 4, 0 },
    { "spc_a_selfteacher_cap", branch_id::mastery, 5, 25, currency_id::major, "spc_a_selfteacher", "", "Self-Taught Expertise", "Опыт самоучки", "+10% reading, +10% crafting, +10% Survivor XP.", "+10% чтения, +10% крафта, +10% опыта Survivor.", {{ { "read_speed_pct", 10 }, { "craft_speed_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 10 },
    { "mg_arcane_focus", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Arcane Focus", "Магический фокус", "Magiclysm: +0.5 Spellcraft for spell checks.", "Magiclysm: +0,5 Spellcraft для проверок заклинаний.", {{ { "mg_spellcraft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_mana_sensitivity", branch_id::mastery, 2, 5, currency_id::perk, "mg_arcane_focus", "", "Mana Sensitivity", "Чувствительность к мане", "Magiclysm: +8% maximum mana.", "Magiclysm: +8% к максимуму маны.", {{ { "mg_mana_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_battlemage", branch_id::mastery, 2, 5, currency_id::perk, "mg_arcane_focus", "", "Invocation Drills", "Тренировка заклинаний", "Magiclysm: spell casting time -5%.", "Magiclysm: время сотворения заклинаний -5%.", {{ { "mg_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_wayfarer", branch_id::mastery, 2, 5, currency_id::perk, "mg_arcane_focus", "", "Arcane Reach", "Магическая дальность", "Magiclysm: spell range +5%.", "Magiclysm: дальность заклинаний +5%.", {{ { "mg_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_mana_regeneration", branch_id::mastery, 3, 9, currency_id::perk, "mg_mana_sensitivity", "", "Mana Regeneration", "Регенерация маны", "Magiclysm: mana regeneration +10%.", "Magiclysm: восстановление маны +10%.", {{ { "mg_mana_regen_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_stable_formula", branch_id::mastery, 3, 9, currency_id::perk, "mg_battlemage", "", "Stable Formula", "Стабильная формула", "Magiclysm: spell failure chance is reduced by 7% multiplicatively.", "Magiclysm: шанс провала заклинаний снижается на 7% мультипликативно.", {{ { "mg_fail_pct", -7 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_shaped_evocation", branch_id::mastery, 3, 9, currency_id::perk, "mg_wayfarer", "", "Shaped Evocation", "Формованная эвокация", "Magiclysm: spell power +6% to damage and healing.", "Magiclysm: сила заклинаний +6% к урону и лечению.", {{ { "mg_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_efficient_channels", branch_id::mastery, 4, 14, currency_id::perk, "mg_mana_regeneration", "", "Efficient Channels", "Эффективные каналы", "Magiclysm: spell mana cost -6%.", "Magiclysm: стоимость заклинаний в мане -6%.", {{ { "mg_spell_cost_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_spellcraft_drills", branch_id::mastery, 4, 14, currency_id::perk, "mg_stable_formula", "", "Spellcraft Drills", "Практика Spellcraft", "Magiclysm: +0.5 Spellcraft for spell checks.", "Magiclysm: +0,5 Spellcraft для проверок заклинаний.", {{ { "mg_spellcraft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_sustained_weave", branch_id::mastery, 4, 14, currency_id::perk, "mg_shaped_evocation", "", "Sustained Weave", "Удержание плетения", "Magiclysm: spell duration +8%.", "Magiclysm: длительность заклинаний +8%.", {{ { "mg_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_deep_reservoir", branch_id::mastery, 5, 20, currency_id::perk, "mg_efficient_channels", "", "Deep Reservoir", "Глубокий резерв", "Magiclysm: +12% maximum mana and +8% mana regeneration.", "Magiclysm: +12% максимум маны и +8% восстановления маны.", {{ { "mg_mana_max_pct", 12 }, { "mg_mana_regen_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_quick_invocation", branch_id::mastery, 5, 20, currency_id::perk, "mg_spellcraft_drills", "", "Quick Invocation", "Быстрое сотворение", "Magiclysm: casting time -8% and failure chance -5%.", "Magiclysm: время сотворения -8%, шанс провала -5%.", {{ { "mg_cast_time_pct", -8 }, { "mg_fail_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_arcane_geometry", branch_id::mastery, 5, 20, currency_id::perk, "mg_sustained_weave", "", "Arcane Geometry", "Магическая геометрия", "Magiclysm: area of effect +6% and range +5%.", "Magiclysm: площадь действия +6%, дальность +5%.", {{ { "mg_aoe_pct", 6 }, { "mg_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_mana_mastery", branch_id::mastery, 6, 25, currency_id::major, "mg_deep_reservoir", "", "Mana Mastery", "Мастерство маны", "Spell cost -10%, mana regeneration +15%.", "Стоимость заклинаний -10%, регенерация маны +15%.", {{ { "mg_spell_cost_pct", -10 }, { "mg_mana_regen_pct", 15 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_ritual_craft", branch_id::mastery, 6, 25, currency_id::major, "mg_quick_invocation", "", "Ritual Mastery", "Мастерство ритуалов", "+0.75 Spellcraft and +12% spell XP.", "+0,75 Spellcraft и +12% опыта заклинаний.", {{ { "mg_spellcraft_flat", 0.75 }, { "mg_spell_xp_pct", 12 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_high_thaumaturgy", branch_id::mastery, 6, 25, currency_id::major, "mg_arcane_geometry", "", "High Thaumaturgy", "Высшая тауматургия", "Spell potency +10%, duration +10%.", "Мощность +10%, длительность +10%.", {{ { "mg_spell_power_pct", 10 }, { "mg_duration_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_efficient_theory", branch_id::mastery, 7, 30, currency_id::perk, "mg_mana_mastery", "mg_ritual_craft", "Efficient Theory", "Эффективная теория", "Spell cost -5%, spell XP +8%.", "Стоимость -5%, опыт заклинаний +8%.", {{ { "mg_spell_cost_pct", -5 }, { "mg_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_combat_weave", branch_id::mastery, 7, 30, currency_id::perk, "mg_ritual_craft", "mg_high_thaumaturgy", "Combat Weave", "Боевое плетение", "Casting time -5%, spell potency +8%.", "Время сотворения -5%, мощность +8%.", {{ { "mg_cast_time_pct", -5 }, { "mg_spell_power_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_resonant_reserve", branch_id::mastery, 7, 30, currency_id::perk, "mg_mana_mastery", "mg_high_thaumaturgy", "Resonant Reserve", "Резонансный резерв", "Maximum mana +10%, spell duration +8%.", "Максимум маны +10%, длительность +8%.", {{ { "mg_mana_max_pct", 10 }, { "mg_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_archmage", branch_id::mastery, 8, 40, currency_id::major, "mg_efficient_theory", "mg_combat_weave", "Archmage", "Архимаг", "+0.75 Spellcraft, -5% failure, +8% potency, +8% spell XP.", "+0,75 Spellcraft, -5% провала, +8% мощности, +8% опыта заклинаний.", {{ { "mg_spellcraft_flat", 0.75 }, { "mg_fail_pct", -5 }, { "mg_spell_power_pct", 8 }, { "mg_spell_xp_pct", 8 } }}, 4, 0, perk_kind::effect },
    { "mg_mana_vampirism", branch_id::mastery, 9, 40, currency_id::perk, "mg_archmage", "", "Mana Vampirism", "Вампиризм маны", "Magiclysm: restore mana equal to 1% of actual melee damage dealt per rank (1-5%).", "Magiclysm: восстанавливает ману в размере 1% от фактически нанесённого урона в ближнем бою за ранг (1-5%).", {{ { "mg_melee_mana_vamp_pct", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_mana_hand_3", branch_id::mastery, 9, 42, currency_id::perk, "mg_archmage", "", "Third Mana Hand", "Третья рука маны", "Magiclysm: manifest one unencumbered virtual hand. It can hold one real carried item without moving or duplicating it; shields can block and a magic focus counts as held. An empty mana hand can perform somatic casting while physical hands are occupied.", "Magiclysm: создаёт одну свободную от стеснения виртуальную руку. Она может удерживать один реальный предмет персонажа без перемещения и копирования; щит может блокировать, а магический фокус считается удерживаемым. Пустая рука маны может выполнять соматику, когда физические руки заняты.", {{ { "mg_virtual_hand_count", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_mana_hand_4", branch_id::mastery, 10, 48, currency_id::perk, "mg_mana_hand_3", "", "Fourth Mana Hand", "Четвёртая рука маны", "Magiclysm: manifest a second unencumbered virtual hand with its own logical item slot. Together Mana Hands III+IV can also hold one real two-handed item as a paired grip, including firearms. Occupied hands are not free for somatic casting unless they hold a magic focus.", "Magiclysm: создаёт вторую свободную от стеснения виртуальную руку со своим логическим слотом. Вместе руки маны III+IV также могут удерживать один реальный двуручный предмет парным хватом, включая огнестрельное оружие. Занятые руки не считаются свободными для соматики, если только не удерживают магический фокус.", {{ { "mg_virtual_hand_count", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },

    { "mom_mental_focus", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Psionic Focus", "Псионический фокус", "Mind Over Matter powers: +0.5 effective Metaphysics while channeling.", "Силы Mind Over Matter: +0,5 к эффективной Metaphysics при ченнелинге.", {{ { "mom_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_still_mind", branch_id::mastery, 2, 5, currency_id::perk, "mom_mental_focus", "", "Still Mind", "Спокойный разум", "Mind Over Matter: power failure chance -6%.", "Mind Over Matter: шанс провала псионических сил -6%.", {{ { "mom_fail_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_neural_reserve", branch_id::mastery, 2, 5, currency_id::perk, "mom_mental_focus", "", "Neural Reserve", "Нейронный резерв", "Mind Over Matter: psionic stamina cost -5%.", "Mind Over Matter: затраты выносливости на псионику -5%.", {{ { "mom_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_kinetic_control", branch_id::mastery, 2, 5, currency_id::perk, "mom_mental_focus", "", "Kinetic Control", "Кинетический контроль", "Mind Over Matter: power range +5%.", "Mind Over Matter: дальность псионических сил +5%.", {{ { "mom_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_channel_discipline", branch_id::mastery, 3, 9, currency_id::perk, "mom_still_mind", "", "Channel Discipline", "Дисциплина канала", "Mind Over Matter: activation/casting time -5%.", "Mind Over Matter: время активации/применения -5%.", {{ { "mom_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_efficient_channel", branch_id::mastery, 3, 9, currency_id::perk, "mom_neural_reserve", "", "Efficient Channel", "Эффективный канал", "Mind Over Matter: psionic stamina cost -6%.", "Mind Over Matter: затраты выносливости на псионику -6%.", {{ { "mom_spell_cost_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_psionic_pressure", branch_id::mastery, 3, 9, currency_id::perk, "mom_kinetic_control", "", "Psionic Pressure", "Псионическое давление", "Mind Over Matter: psionic power potency +6%.", "Mind Over Matter: мощность псионических сил +6%.", {{ { "mom_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_metaphysical_method", branch_id::mastery, 4, 14, currency_id::perk, "mom_channel_discipline", "", "Metaphysical Method", "Метод метафизики", "Mind Over Matter powers: +0.5 effective Metaphysics while channeling.", "Силы Mind Over Matter: +0,5 к эффективной Metaphysics при ченнелинге.", {{ { "mom_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_controlled_exposure", branch_id::mastery, 4, 14, currency_id::perk, "mom_efficient_channel", "", "Controlled Exposure", "Контролируемое воздействие", "Mind Over Matter: power-use XP +8%. Nether Attunement itself is not rewritten.", "Mind Over Matter: опыт за применение сил +8%. Сам Nether Attunement не переписывается.", {{ { "mom_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_extended_pattern", branch_id::mastery, 4, 14, currency_id::perk, "mom_psionic_pressure", "", "Extended Pattern", "Продлённый паттерн", "Mind Over Matter: power duration +8%.", "Mind Over Matter: длительность псионических сил +8%.", {{ { "mom_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_mental_lattice", branch_id::mastery, 5, 20, currency_id::perk, "mom_metaphysical_method", "", "Mental Lattice", "Ментальная решётка", "Mind Over Matter: failure chance -7%, activation time -5%.", "Mind Over Matter: шанс провала -7%, время активации -5%.", {{ { "mom_fail_pct", -7 }, { "mom_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_recovery_cycle", branch_id::mastery, 5, 20, currency_id::perk, "mom_controlled_exposure", "", "Recovery Cycle", "Цикл восстановления", "Mind Over Matter: stamina cost -7%, power-use XP +8%.", "Mind Over Matter: стоимость по выносливости -7%, опыт сил +8%.", {{ { "mom_spell_cost_pct", -7 }, { "mom_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_field_shaping", branch_id::mastery, 5, 20, currency_id::perk, "mom_extended_pattern", "", "Field Shaping", "Формирование поля", "Mind Over Matter: area of effect +6%, range +5%.", "Mind Over Matter: площадь действия +6%, дальность +5%.", {{ { "mom_aoe_pct", 6 }, { "mom_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_combat_focus", branch_id::mastery, 6, 25, currency_id::major, "mom_mental_lattice", "", "Noetic Control", "Ноэтический контроль", "+0.75 effective Metaphysics while channeling, failure chance -8%.", "+0,75 эффективной Metaphysics при ченнелинге, шанс провала -8%.", {{ { "mom_metaphysics_flat", 0.75 }, { "mom_fail_pct", -8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_nether_discipline", branch_id::mastery, 6, 25, currency_id::major, "mom_recovery_cycle", "", "Nether Discipline", "Дисциплина Низины", "Stamina cost -10%, power-use XP +12%.", "Стоимость по выносливости -10%, опыт сил +12%.", {{ { "mom_spell_cost_pct", -10 }, { "mom_spell_xp_pct", 12 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_noetic_projection", branch_id::mastery, 6, 25, currency_id::major, "mom_field_shaping", "", "Noetic Projection", "Ноэтическая проекция", "Potency +10%, duration +10%.", "Мощность +10%, длительность +10%.", {{ { "mom_spell_power_pct", 10 }, { "mom_duration_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_stable_channel", branch_id::mastery, 7, 30, currency_id::perk, "mom_combat_focus", "mom_nether_discipline", "Stable Channel", "Стабильный канал", "Failure chance -5%, stamina cost -5%.", "Шанс провала -5%, стоимость по выносливости -5%.", {{ { "mom_fail_pct", -5 }, { "mom_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_precise_manifestation", branch_id::mastery, 7, 30, currency_id::perk, "mom_combat_focus", "mom_noetic_projection", "Precise Manifestation", "Точная манифестация", "Activation time -5%, range +6%.", "Время активации -5%, дальность +6%.", {{ { "mom_cast_time_pct", -5 }, { "mom_range_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_efficient_force", branch_id::mastery, 7, 30, currency_id::perk, "mom_nether_discipline", "mom_noetic_projection", "Efficient Force", "Эффективная сила", "Stamina cost -5%, potency +8%.", "Стоимость по выносливости -5%, мощность +8%.", {{ { "mom_spell_cost_pct", -5 }, { "mom_spell_power_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_transcendent_focus", branch_id::mastery, 8, 40, currency_id::major, "mom_stable_channel", "mom_efficient_force", "Transcendent Focus", "Трансцендентный фокус", "+0.75 effective Metaphysics while channeling, failure -5%, potency +8%, power XP +8%.", "+0,75 эффективной Metaphysics при ченнелинге, шанс провала -5%, мощность +8%, опыт сил +8%.", {{ { "mom_metaphysics_flat", 0.75 }, { "mom_fail_pct", -5 }, { "mom_spell_power_pct", 8 }, { "mom_spell_xp_pct", 8 } }}, 4, 0, perk_kind::effect },

    { "xe_anomaly_method", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Anomaly Method", "Метод аномалий", "Xedra Evolved: +0.5 effective Deduction.", "Xedra Evolved: +0,5 к эффективной Deduction.", {{ { "xe_deduction_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_field_agent", branch_id::mastery, 2, 5, currency_id::perk, "xe_anomaly_method", "", "XEDRA Field Analysis", "Полевой анализ XEDRA", "Xedra Evolved: +0.5 effective Deduction.", "Xedra Evolved: +0,5 к эффективной Deduction.", {{ { "xe_deduction_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dross_resonance", branch_id::mastery, 2, 5, currency_id::perk, "xe_anomaly_method", "", "Dreamdross Resonance", "Резонанс дримдросса", "Xedra Evolved: +8% maximum mana.", "Xedra Evolved: +8% к максимуму маны.", {{ { "xe_mana_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dimensional_hunter", branch_id::mastery, 2, 5, currency_id::perk, "xe_anomaly_method", "", "Liminal Reach", "Пограничная дальность", "Xedra Evolved: spell/power range +5%.", "Xedra Evolved: дальность заклинаний/сил +5%.", {{ { "xe_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_gramarye_studies", branch_id::mastery, 3, 9, currency_id::perk, "xe_field_agent", "", "Gramarye Studies", "Изучение Gramarye", "Xedra Evolved: +0.5 effective Gramarye for fae magicks.", "Xedra Evolved: +0,5 к эффективной Gramarye для магии фей.", {{ { "xe_gramarye_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dream_metabolism", branch_id::mastery, 3, 9, currency_id::perk, "xe_dross_resonance", "", "Dream Metabolism", "Метаболизм сновидений", "Xedra Evolved: mana regeneration +10%.", "Xedra Evolved: восстановление маны +10%.", {{ { "xe_mana_regen_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_fast_manifestation", branch_id::mastery, 3, 9, currency_id::perk, "xe_dimensional_hunter", "", "Fast Manifestation", "Быстрая манифестация", "Xedra Evolved: casting/activation time -5%.", "Xedra Evolved: время сотворения/активации -5%.", {{ { "xe_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dimensional_model", branch_id::mastery, 4, 14, currency_id::perk, "xe_gramarye_studies", "", "Dimensional Model", "Модель измерений", "Xedra Evolved: spell failure chance -6%.", "Xedra Evolved: шанс провала заклинаний -6%.", {{ { "xe_fail_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_efficient_oneiromancy", branch_id::mastery, 4, 14, currency_id::perk, "xe_dream_metabolism", "", "Efficient Oneiromancy", "Эффективная онейромантия", "Xedra Evolved: mana cost -6% for Xedra spells.", "Xedra Evolved: стоимость маны заклинаний Xedra -6%.", {{ { "xe_spell_cost_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_oneiric_force", branch_id::mastery, 4, 14, currency_id::perk, "xe_fast_manifestation", "", "Oneiric Force", "Онейрическая сила", "Xedra Evolved: spell/power potency +6%.", "Xedra Evolved: мощность заклинаний/сил +6%.", {{ { "xe_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_pattern_archive", branch_id::mastery, 5, 20, currency_id::perk, "xe_dimensional_model", "", "Anomaly Archive", "Архив аномалий", "Xedra Evolved: +0.5 Deduction and +8% spell XP.", "Xedra Evolved: +0,5 Deduction и +8% опыта заклинаний.", {{ { "xe_deduction_flat", 0.5 }, { "xe_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_dream_reservoir", branch_id::mastery, 5, 20, currency_id::perk, "xe_efficient_oneiromancy", "", "Dream Reservoir", "Резерв сновидений", "Xedra Evolved: +12% maximum mana and +8% mana regeneration.", "Xedra Evolved: +12% максимум маны и +8% восстановления маны.", {{ { "xe_mana_max_pct", 12 }, { "xe_mana_regen_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_liminal_persistence", branch_id::mastery, 5, 20, currency_id::perk, "xe_oneiric_force", "", "Liminal Persistence", "Пограничная устойчивость", "Xedra Evolved: duration +8%, area of effect +6%.", "Xedra Evolved: длительность +8%, площадь действия +6%.", {{ { "xe_duration_pct", 8 }, { "xe_aoe_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_occult_engineer", branch_id::mastery, 6, 25, currency_id::major, "xe_pattern_archive", "", "Occult Engineer", "Оккультный инженер", "+0.75 Deduction and +0.75 Gramarye.", "+0,75 Deduction и +0,75 Gramarye.", {{ { "xe_deduction_flat", 0.75 }, { "xe_gramarye_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_dream_economy", branch_id::mastery, 6, 25, currency_id::major, "xe_dream_reservoir", "", "Dream Economy", "Экономия сновидений", "Mana cost -10%, mana regeneration +15%.", "Стоимость маны -10%, регенерация +15%.", {{ { "xe_spell_cost_pct", -10 }, { "xe_mana_regen_pct", 15 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_reality_shaper", branch_id::mastery, 6, 25, currency_id::major, "xe_liminal_persistence", "", "Reality Shaper", "Формирователь реальности", "Potency +10%, range +8%.", "Мощность +10%, дальность +8%.", {{ { "xe_spell_power_pct", 10 }, { "xe_range_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_dream_theorist", branch_id::mastery, 7, 30, currency_id::perk, "xe_occult_engineer", "xe_dream_economy", "Dream Theorist", "Теоретик сновидений", "Spell XP +8%, mana cost -5%.", "Опыт заклинаний +8%, стоимость маны -5%.", {{ { "xe_spell_xp_pct", 8 }, { "xe_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_liminal_engineer", branch_id::mastery, 7, 30, currency_id::perk, "xe_occult_engineer", "xe_reality_shaper", "Liminal Engineer", "Пограничный инженер", "Failure chance -5%, cast time -5%.", "Шанс провала -5%, время сотворения -5%.", {{ { "xe_fail_pct", -5 }, { "xe_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_oneiric_architect", branch_id::mastery, 7, 30, currency_id::perk, "xe_dream_economy", "xe_reality_shaper", "Oneiric Architect", "Онейрический архитектор", "Potency +8%, duration +8%.", "Мощность +8%, длительность +8%.", {{ { "xe_spell_power_pct", 8 }, { "xe_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_boundary_master", branch_id::mastery, 8, 40, currency_id::major, "xe_dream_theorist", "xe_oneiric_architect", "Boundary Master", "Мастер границы", "+0.75 Deduction, +0.5 Gramarye, failure -5%, potency +8%.", "+0,75 Deduction, +0,5 Gramarye, шанс провала -5%, мощность +8%.", {{ { "xe_deduction_flat", 0.75 }, { "xe_gramarye_flat", 0.5 }, { "xe_fail_pct", -5 }, { "xe_spell_power_pct", 8 } }}, 4, 0, perk_kind::effect },

    { "af_systems_operator", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Systems Operator", "Оператор систем", "Aftershock Exoplanet: +0.25 effective Smartgun and +0.25 effective Metaphysics while channeling esper powers.", "Aftershock Exoplanet: +0,25 к Smartgun и +0,25 к эффективной Metaphysics при ченнелинге эспер-сил.", {{ { "af_smartgun_flat", 0.25 }, { "af_metaphysics_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_targeting_link", branch_id::mastery, 2, 5, currency_id::perk, "af_systems_operator", "", "Targeting Link", "Связь с прицелом", "Aftershock Exoplanet: +0.25 effective Smartgun.", "Aftershock Exoplanet: +0,25 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_conditioning", branch_id::mastery, 2, 5, currency_id::perk, "af_systems_operator", "", "Psi Endurance", "Пси-выносливость", "Aftershock Exoplanet esper powers: stamina cost -5%.", "Псионика Aftershock Exoplanet: затраты выносливости -5%.", {{ { "af_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_expedition_logistics", branch_id::mastery, 2, 5, currency_id::perk, "af_systems_operator", "", "Vector Projection", "Векторная проекция", "Aftershock Exoplanet esper powers: range +5%.", "Псионика Aftershock Exoplanet: дальность +5%.", {{ { "af_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_predictive_fire", branch_id::mastery, 3, 9, currency_id::perk, "af_targeting_link", "", "Predictive Fire", "Предиктивный огонь", "Aftershock Exoplanet: +0.25 effective Smartgun.", "Aftershock Exoplanet: +0,25 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_metaphysical_training", branch_id::mastery, 3, 9, currency_id::perk, "af_conditioning", "", "Metaphysical Training", "Тренировка метафизики", "Aftershock Exoplanet esper powers: +0.5 effective Metaphysics while channeling.", "Эспер-силы Aftershock Exoplanet: +0,5 к эффективной Metaphysics при ченнелинге.", {{ { "af_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_telekinetic_geometry", branch_id::mastery, 3, 9, currency_id::perk, "af_expedition_logistics", "", "Esper Geometry", "Геометрия эспера", "Aftershock Exoplanet esper powers: area of effect +6%.", "Псионика Aftershock Exoplanet: площадь действия +6%.", {{ { "af_aoe_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_sensor_fusion", branch_id::mastery, 4, 14, currency_id::perk, "af_predictive_fire", "", "Sensor Fusion", "Слияние сенсоров", "Aftershock Exoplanet: +0.25 effective Smartgun.", "Aftershock Exoplanet: +0,25 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_stable_esper", branch_id::mastery, 4, 14, currency_id::perk, "af_metaphysical_training", "", "Stable Esper", "Стабильный эспер", "Aftershock Exoplanet esper powers: failure chance -7%.", "Псионика Aftershock Exoplanet: шанс провала -7%.", {{ { "af_fail_pct", -7 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_esper_force", branch_id::mastery, 4, 14, currency_id::perk, "af_telekinetic_geometry", "", "Esper Force", "Сила эспера", "Aftershock Exoplanet esper powers: potency +6%.", "Псионика Aftershock Exoplanet: мощность +6%.", {{ { "af_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_combat_technician", branch_id::mastery, 5, 20, currency_id::perk, "af_sensor_fusion", "", "Combat Technician", "Боевой техник", "Aftershock Exoplanet: +0.5 effective Smartgun.", "Aftershock Exoplanet: +0,5 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_efficient_esper", branch_id::mastery, 5, 20, currency_id::perk, "af_stable_esper", "", "Efficient Esper", "Эффективный эспер", "Aftershock Exoplanet esper powers: stamina cost -7%, power XP +8%.", "Псионика Aftershock Exoplanet: стоимость по выносливости -7%, опыт сил +8%.", {{ { "af_spell_cost_pct", -7 }, { "af_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_sustained_phenomena", branch_id::mastery, 5, 20, currency_id::perk, "af_esper_force", "", "Sustained Phenomena", "Устойчивые феномены", "Aftershock Exoplanet esper powers: duration +8%, range +5%.", "Псионика Aftershock Exoplanet: длительность +8%, дальность +5%.", {{ { "af_duration_pct", 8 }, { "af_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_smartgun_mastery", branch_id::mastery, 6, 25, currency_id::major, "af_combat_technician", "", "Smartgun Mastery", "Мастерство Smartgun", "+0.75 effective Smartgun.", "+0,75 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_esper_mastery", branch_id::mastery, 6, 25, currency_id::major, "af_efficient_esper", "", "Esper Mastery", "Мастерство эспера", "+0.75 effective Metaphysics while channeling, failure chance -10%.", "+0,75 эффективной Metaphysics при ченнелинге, шанс провала -10%.", {{ { "af_metaphysics_flat", 0.75 }, { "af_fail_pct", -10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_noetic_artillery", branch_id::mastery, 6, 25, currency_id::major, "af_sustained_phenomena", "", "Noetic Artillery", "Ноэтическая артиллерия", "Esper potency +10%, range +8%.", "Мощность эспера +10%, дальность +8%.", {{ { "af_spell_power_pct", 10 }, { "af_range_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_neural_targeting", branch_id::mastery, 7, 30, currency_id::perk, "af_smartgun_mastery", "af_esper_mastery", "Neural Targeting", "Нейронное наведение", "+0.25 Smartgun and +0.5 effective Metaphysics while channeling.", "+0,25 Smartgun и +0,5 эффективной Metaphysics при ченнелинге.", {{ { "af_smartgun_flat", 0.25 }, { "af_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_psionic_firecontrol", branch_id::mastery, 7, 30, currency_id::perk, "af_smartgun_mastery", "af_noetic_artillery", "Psionic Fire Control", "Псионическое управление огнём", "+0.25 Smartgun, esper potency +6%.", "+0,25 Smartgun, мощность эспера +6%.", {{ { "af_smartgun_flat", 0.25 }, { "af_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_stable_projection", branch_id::mastery, 7, 30, currency_id::perk, "af_esper_mastery", "af_noetic_artillery", "Stable Projection", "Стабильная проекция", "Stamina cost -5%, failure chance -5%.", "Стоимость по выносливости -5%, шанс провала -5%.", {{ { "af_spell_cost_pct", -5 }, { "af_fail_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_posthuman_operator", branch_id::mastery, 8, 40, currency_id::major, "af_neural_targeting", "af_stable_projection", "Posthuman Operator", "Постчеловеческий оператор", "+0.5 Smartgun, +0.75 effective Metaphysics while channeling, stamina cost -5%, esper potency +8%.", "+0,5 Smartgun, +0,75 эффективной Metaphysics при ченнелинге, стоимость выносливости -5%, мощность эспера +8%.", {{ { "af_smartgun_flat", 0.5 }, { "af_metaphysics_flat", 0.75 }, { "af_spell_cost_pct", -5 }, { "af_spell_power_pct", 8 } }}, 4, 0, perk_kind::effect },
    { "afp_prime_operator", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Prime Operator", "Оператор Prime", "Aftershock Prime: +0.25 Smartgun for related checks and +3% XP for Prime abilities.", "Aftershock Prime: +0,25 Smartgun для связанных проверок и +3% опыта способностей Prime.", {{ { "afp_smartgun_flat", 0.25 }, { "afp_spell_xp_pct", 3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_smartgun_interface", branch_id::mastery, 2, 5, currency_id::perk, "afp_prime_operator", "", "Smartgun Interface", "Интерфейс Smartgun", "Prime smart weapons: +0.25 effective Smartgun.", "Умное оружие Prime: +0,25 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_systems_theory", branch_id::mastery, 2, 5, currency_id::perk, "afp_prime_operator", "", "Systems Theory", "Теория систем", "Prime abilities: energy cost -4%, XP +4%.", "Способности Prime: стоимость энергии -4%, опыт +4%.", {{ { "afp_spell_cost_pct", -4 }, { "afp_spell_xp_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_translocation_calculus", branch_id::mastery, 2, 5, currency_id::perk, "afp_prime_operator", "", "Translocation Calculus", "Расчёт транслокации", "Prime spatial abilities: range +5%, duration +4%.", "Пространственные способности Prime: дальность +5%, длительность +4%.", {{ { "afp_range_pct", 5 }, { "afp_duration_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_predictive_targeting", branch_id::mastery, 3, 9, currency_id::perk, "afp_smartgun_interface", "", "Predictive Targeting", "Предиктивное наведение", "Prime smart weapons: +0.25 effective Smartgun.", "Умное оружие Prime: +0,25 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_power_budget", branch_id::mastery, 3, 9, currency_id::perk, "afp_systems_theory", "", "Power Budget", "Энергобюджет", "Prime abilities: energy cost -4%, activation time -3%.", "Способности Prime: стоимость энергии -4%, время активации -3%.", {{ { "afp_spell_cost_pct", -4 }, { "afp_cast_time_pct", -3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_spatial_solution", branch_id::mastery, 3, 9, currency_id::perk, "afp_translocation_calculus", "", "Spatial Solution", "Пространственное решение", "Prime spatial abilities: range +5%, area +5%.", "Пространственные способности Prime: дальность +5%, площадь +5%.", {{ { "afp_range_pct", 5 }, { "afp_aoe_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_sensor_fusion", branch_id::mastery, 4, 14, currency_id::perk, "afp_predictive_targeting", "", "Sensor Fusion", "Слияние сенсоров", "Prime smart weapons: +0.25 effective Smartgun.", "Умное оружие Prime: +0,25 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_utility_protocols", branch_id::mastery, 4, 14, currency_id::perk, "afp_power_budget", "", "Utility Suite", "Набор утилит", "Prime abilities: XP +6%, failure chance -4%.", "Способности Prime: опыт +6%, шанс провала -4%.", {{ { "afp_spell_xp_pct", 6 }, { "afp_fail_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_stable_translation", branch_id::mastery, 4, 14, currency_id::perk, "afp_spatial_solution", "", "Stable Translocation", "Стабильная транслокация", "Prime spatial abilities: duration +6%, failure chance -4%.", "Пространственные способности Prime: длительность +6%, шанс провала -4%.", {{ { "afp_duration_pct", 6 }, { "afp_fail_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_combat_technician", branch_id::mastery, 5, 20, currency_id::perk, "afp_sensor_fusion", "", "Prime Combat Technician", "Боевой техник Prime", "Prime smart weapons: +0.5 effective Smartgun.", "Умное оружие Prime: +0,5 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_systems_automation", branch_id::mastery, 5, 20, currency_id::perk, "afp_utility_protocols", "", "Systems Automation", "Автоматизация систем", "Prime abilities: activation time -5%, XP +7%.", "Способности Prime: время активации -5%, опыт +7%.", {{ { "afp_cast_time_pct", -5 }, { "afp_spell_xp_pct", 7 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_field_projection", branch_id::mastery, 5, 20, currency_id::perk, "afp_stable_translation", "", "Field Projection", "Полевая проекция", "Prime abilities: potency +6%, range +5%.", "Способности Prime: мощность +6%, дальность +5%.", {{ { "afp_spell_power_pct", 6 }, { "afp_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_smartgun_mastery", branch_id::mastery, 6, 25, currency_id::major, "afp_combat_technician", "", "Prime Smartgun Mastery", "Мастерство Smartgun Prime", "+0.75 effective Smartgun.", "+0,75 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_prime_systems_mastery", branch_id::mastery, 6, 25, currency_id::major, "afp_systems_automation", "", "Prime Systems Mastery", "Мастерство систем Prime", "Energy cost -7%, activation time -6%, XP +8%.", "Стоимость энергии -7%, время активации -6%, опыт +8%.", {{ { "afp_spell_cost_pct", -7 }, { "afp_cast_time_pct", -6 }, { "afp_spell_xp_pct", 8 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "afp_translocation_mastery", branch_id::mastery, 6, 25, currency_id::major, "afp_field_projection", "", "Translocation Mastery", "Мастерство транслокации", "Potency +8%, range +8%, duration +8%.", "Мощность +8%, дальность +8%, длительность +8%.", {{ { "afp_spell_power_pct", 8 }, { "afp_range_pct", 8 }, { "afp_duration_pct", 8 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "afp_integrated_firecontrol", branch_id::mastery, 7, 30, currency_id::perk, "afp_smartgun_mastery", "afp_prime_systems_mastery", "Integrated Fire Control", "Интегрированное управление огнём", "+0.25 Smartgun, activation time -4%.", "+0,25 Smartgun, время активации -4%.", {{ { "afp_smartgun_flat", 0.25 }, { "afp_cast_time_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_remote_geometry", branch_id::mastery, 7, 30, currency_id::perk, "afp_prime_systems_mastery", "afp_translocation_mastery", "Remote Geometry", "Удалённая геометрия", "Area +5%, duration +5%.", "Площадь +5%, длительность +5%.", {{ { "afp_aoe_pct", 5 }, { "afp_duration_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_mobile_platform", branch_id::mastery, 7, 30, currency_id::perk, "afp_smartgun_mastery", "afp_translocation_mastery", "Mobile Platform", "Мобильная платформа", "+0.25 Smartgun, range +5%.", "+0,25 Smartgun, дальность +5%.", {{ { "afp_smartgun_flat", 0.25 }, { "afp_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_prime_integrator", branch_id::mastery, 8, 40, currency_id::major, "afp_integrated_firecontrol", "afp_remote_geometry", "Prime Integrator", "Интегратор Prime", "+0.5 Smartgun, energy cost -5%, potency +6%, duration +6%.", "+0,5 Smartgun, стоимость энергии -5%, мощность +6%, длительность +6%.", {{ { "afp_smartgun_flat", 0.5 }, { "afp_spell_cost_pct", -5 }, { "afp_spell_power_pct", 6 }, { "afp_duration_pct", 6 } }}, 4, 0, perk_kind::effect },

    { "sec_field_researcher", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Secronom Field Researcher", "Полевой исследователь Secronom", "Against Secronom creatures: damage +2%, incoming damage -2%.", "Против существ Secronom: урон +2%, входящий урон -2%.", {{ { "sec_damage_pct", 2 }, { "sec_resist_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_hunter_drills", branch_id::mastery, 2, 5, currency_id::perk, "sec_field_researcher", "", "Hunter Drills", "Тренировки охотника", "Against Secronom creatures: damage +3%.", "Против существ Secronom: урон +3%.", {{ { "sec_damage_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_pathogen_hardening", branch_id::mastery, 2, 5, currency_id::perk, "sec_field_researcher", "", "Pathogen Hardening", "Закалка против патогенов", "Against Secronom creatures: incoming damage -3%.", "Против существ Secronom: входящий урон -3%.", {{ { "sec_resist_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_crimson_anatomy", branch_id::mastery, 2, 5, currency_id::perk, "sec_field_researcher", "", "Crimson Anatomy", "Анатомия Crimson", "Against Crimson Horror species: extra damage +3%.", "Против видов Crimson Horror: дополнительный урон +3%.", {{ { "sec_crimson_damage_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_vital_targets", branch_id::mastery, 3, 9, currency_id::perk, "sec_hunter_drills", "", "Vital Targets", "Уязвимые точки", "Against Secronom creatures: damage +4%.", "Против существ Secronom: урон +4%.", {{ { "sec_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_toxicology", branch_id::mastery, 3, 9, currency_id::perk, "sec_pathogen_hardening", "", "Toxicology", "Токсикология", "Against Secronom creatures: incoming damage -4%.", "Против существ Secronom: входящий урон -4%.", {{ { "sec_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_flesh_patterning", branch_id::mastery, 3, 9, currency_id::perk, "sec_crimson_anatomy", "", "Flesh Patterning", "Структура плоти", "Against Crimson Horror species: extra damage +4%.", "Против видов Crimson Horror: дополнительный урон +4%.", {{ { "sec_crimson_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_elite_tracking", branch_id::mastery, 4, 14, currency_id::perk, "sec_vital_targets", "", "Elite Tracking", "Выслеживание элиты", "Against elite/catastrophic Secronom species: extra damage +4%.", "Против элитных/катастрофических видов Secronom: дополнительный урон +4%.", {{ { "sec_elite_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_adaptive_response", branch_id::mastery, 4, 14, currency_id::perk, "sec_toxicology", "", "Adaptive Response", "Адаптивная реакция", "Against elite/catastrophic Secronom species: incoming damage -4% extra.", "Против элитных/катастрофических видов Secronom: дополнительное снижение урона -4%.", {{ { "sec_elite_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_crimson_countermeasures", branch_id::mastery, 4, 14, currency_id::perk, "sec_flesh_patterning", "", "Crimson Countermeasures", "Контрмеры Crimson", "Against Crimson Horror species: incoming damage -4% extra.", "Против видов Crimson Horror: дополнительное снижение урона -4%.", {{ { "sec_crimson_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_execution_protocol", branch_id::mastery, 5, 20, currency_id::perk, "sec_elite_tracking", "", "Elite Hunter", "Охотник на элиту", "Against elite/catastrophic Secronom species: extra damage +5%.", "Против элитных/катастрофических видов Secronom: дополнительный урон +5%.", {{ { "sec_elite_damage_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_hardened_survivor", branch_id::mastery, 5, 20, currency_id::perk, "sec_adaptive_response", "", "Hardened Survivor", "Закалённый выживший", "Against elite/catastrophic Secronom species: incoming damage -5% extra.", "Против элитных/катастрофических видов Secronom: дополнительное снижение урона -5%.", {{ { "sec_elite_resist_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_fleshbreaker", branch_id::mastery, 5, 20, currency_id::perk, "sec_crimson_countermeasures", "", "Fleshbreaker", "Разрушитель плоти", "Against Crimson Horror species: extra damage +5%, incoming damage -3% extra.", "Против видов Crimson Horror: дополнительный урон +5%, дополнительное снижение урона -3%.", {{ { "sec_crimson_damage_pct", 5 }, { "sec_crimson_resist_pct", 3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_hunter_mastery", branch_id::mastery, 6, 25, currency_id::major, "sec_execution_protocol", "", "Veteran Secronom Hunter", "Опытный охотник Secronom", "Damage +6% to all Secronom and +4% extra to elites.", "Урон +6% по всем Secronom и ещё +4% по элите.", {{ { "sec_damage_pct", 6 }, { "sec_elite_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_survival_mastery", branch_id::mastery, 6, 25, currency_id::major, "sec_hardened_survivor", "", "Secronom Survivor", "Выживший против Secronom", "Incoming damage -6% from all Secronom and -4% extra from elites.", "Входящий урон -6% от всех Secronom и ещё -4% от элиты.", {{ { "sec_resist_pct", 6 }, { "sec_elite_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_crimson_mastery", branch_id::mastery, 6, 25, currency_id::major, "sec_fleshbreaker", "", "Crimson Veteran", "Ветеран Crimson Horror", "Extra damage +7%, incoming damage -5% extra.", "Дополнительный урон +7%, дополнительное снижение урона -5%.", {{ { "sec_crimson_damage_pct", 7 }, { "sec_crimson_resist_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_apex_hunter", branch_id::mastery, 7, 30, currency_id::perk, "sec_hunter_mastery", "sec_survival_mastery", "Nightmare Hunter", "Охотник на кошмары", "Damage +4%, incoming damage -4%.", "Урон +4%, входящий урон -4%.", {{ { "sec_damage_pct", 4 }, { "sec_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_ultimate_protocol", branch_id::mastery, 7, 30, currency_id::perk, "sec_survival_mastery", "sec_crimson_mastery", "Crimson Doctrine", "Багровая доктрина", "Elite resistance +5%, Crimson damage +4%.", "Защита от элиты +5%, урон по Crimson +4%.", {{ { "sec_elite_resist_pct", 5 }, { "sec_crimson_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_red_harvest", branch_id::mastery, 7, 30, currency_id::perk, "sec_hunter_mastery", "sec_crimson_mastery", "Red Harvest", "Красная жатва", "Elite damage +5%, Crimson damage +5%.", "Урон по элите +5%, урон по Crimson +5%.", {{ { "sec_elite_damage_pct", 5 }, { "sec_crimson_damage_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_nightmare_specialist", branch_id::mastery, 8, 40, currency_id::major, "sec_apex_hunter", "sec_red_harvest", "Nightmare Specialist", "Специалист по кошмарам", "Damage +5%, resistance +5%, elite damage +5%, Crimson damage +5%.", "Урон +5%, защита +5%, урон по элите +5%, урон по Crimson +5%.", {{ { "sec_damage_pct", 5 }, { "sec_resist_pct", 5 }, { "sec_elite_damage_pct", 5 }, { "sec_crimson_damage_pct", 5 } }}, 4, 0, perk_kind::effect },

    { "secx_flesh_initiate", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Flesh Initiate", "Посвящённый плоти", "Secronom+: +0.25 effective Flesh Weaving and +0.25 Bio-organic Weapons.", "Secronom+: +0,25 к Flesh Weaving и +0,25 к Bio-organic Weapons.", {{ { "secx_flesh_craft_flat", 0.25 }, { "secx_flesh_combat_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_flesh_weaving", branch_id::mastery, 2, 5, currency_id::perk, "secx_flesh_initiate", "", "Flesh Weaving Practice", "Практика Flesh Weaving", "Secronom+: +0.5 effective Flesh Weaving.", "Secronom+: +0,5 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_biomorph_training", branch_id::mastery, 2, 5, currency_id::perk, "secx_flesh_initiate", "", "Biomorph Training", "Тренировка Biomorph", "Secronom+: +0.5 effective Bio-organic Weapons.", "Secronom+: +0,5 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_neural_link", branch_id::mastery, 2, 5, currency_id::perk, "secx_flesh_initiate", "", "Flesh Vessel Neural Link", "Нейросвязь Flesh Vessel", "Secronom+ abilities: energy cost -4%, XP +4%.", "Способности Secronom+: стоимость энергии -4%, опыт +4%.", {{ { "secx_spell_cost_pct", -4 }, { "secx_spell_xp_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_resource_shaping", branch_id::mastery, 3, 9, currency_id::perk, "secx_flesh_weaving", "", "Resource Shaping", "Формирование ресурсов", "Secronom+: +0.5 effective Flesh Weaving.", "Secronom+: +0,5 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_armament_drills", branch_id::mastery, 3, 9, currency_id::perk, "secx_biomorph_training", "", "Armament Drills", "Тренировки вооружения", "Secronom+: +0.5 effective Bio-organic Weapons.", "Secronom+: +0,5 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_morph_control", branch_id::mastery, 3, 9, currency_id::perk, "secx_neural_link", "", "Morph Control", "Контроль морфинга", "Secronom+ abilities: activation time -4%, failure chance -4%.", "Способности Secronom+: время активации -4%, шанс провала -4%.", {{ { "secx_cast_time_pct", -4 }, { "secx_fail_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_advanced_weaving", branch_id::mastery, 4, 14, currency_id::perk, "secx_resource_shaping", "", "Advanced Flesh Weaving", "Продвинутое Flesh Weaving", "Secronom+: +0.5 effective Flesh Weaving.", "Secronom+: +0,5 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_combat_instinct", branch_id::mastery, 4, 14, currency_id::perk, "secx_armament_drills", "", "Bio-organic Combat Instinct", "Биоорганический боевой инстинкт", "Secronom+: +0.5 effective Bio-organic Weapons.", "Secronom+: +0,5 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_flesh_channel", branch_id::mastery, 4, 14, currency_id::perk, "secx_morph_control", "", "Flesh Channel", "Канал плоти", "Secronom+ abilities: duration +5%, potency +5%.", "Способности Secronom+: длительность +5%, мощность +5%.", {{ { "secx_duration_pct", 5 }, { "secx_spell_power_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_material_mastery", branch_id::mastery, 5, 20, currency_id::perk, "secx_advanced_weaving", "", "Living Material Mastery", "Мастерство живого материала", "Secronom+: +0.75 effective Flesh Weaving.", "Secronom+: +0,75 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_bioorganic_mastery", branch_id::mastery, 5, 20, currency_id::perk, "secx_combat_instinct", "", "Bio-organic Armament Mastery", "Мастерство биооружия", "Secronom+: +0.75 effective Bio-organic Weapons.", "Secronom+: +0,75 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_vessel_resonance", branch_id::mastery, 5, 20, currency_id::perk, "secx_flesh_channel", "", "Flesh Vessel Resonance", "Резонанс Flesh Vessel", "Secronom+ abilities: energy cost -5%, duration +6%.", "Способности Secronom+: стоимость энергии -5%, длительность +6%.", {{ { "secx_spell_cost_pct", -5 }, { "secx_duration_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_fleshcraft_mastery", branch_id::mastery, 6, 25, currency_id::major, "secx_material_mastery", "", "Fleshcraft Mastery", "Мастерство Fleshcraft", "+1.0 effective Flesh Weaving.", "+1,0 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 1.0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_biomorph_mastery", branch_id::mastery, 6, 25, currency_id::major, "secx_bioorganic_mastery", "", "Biomorph Mastery", "Мастерство Biomorph", "+1.0 effective Bio-organic Weapons.", "+1,0 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 1.0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_flesh_vessel_mastery", branch_id::mastery, 6, 25, currency_id::major, "secx_vessel_resonance", "", "Flesh Vessel Mastery", "Мастерство Flesh Vessel", "Potency +8%, energy cost -6%, duration +8%.", "Мощность +8%, стоимость энергии -6%, длительность +8%.", {{ { "secx_spell_power_pct", 8 }, { "secx_spell_cost_pct", -6 }, { "secx_duration_pct", 8 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "secx_living_arsenal", branch_id::mastery, 7, 30, currency_id::perk, "secx_fleshcraft_mastery", "secx_biomorph_mastery", "Living Arsenal", "Живой арсенал", "+0.5 Flesh Weaving and +0.5 Bio-organic Weapons.", "+0,5 Flesh Weaving и +0,5 Bio-organic Weapons.", {{ { "secx_flesh_craft_flat", 0.5 }, { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_adaptive_morph", branch_id::mastery, 7, 30, currency_id::perk, "secx_biomorph_mastery", "secx_flesh_vessel_mastery", "Adaptive Morph", "Адаптивный морф", "+0.5 Bio-organic Weapons, activation time -4%.", "+0,5 Bio-organic Weapons, время активации -4%.", {{ { "secx_flesh_combat_flat", 0.5 }, { "secx_cast_time_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_artificial_ecology", branch_id::mastery, 7, 30, currency_id::perk, "secx_fleshcraft_mastery", "secx_flesh_vessel_mastery", "Artificial Ecology", "Искусственная экология", "+0.5 Flesh Weaving, ability XP +6%.", "+0,5 Flesh Weaving, опыт способностей +6%.", {{ { "secx_flesh_craft_flat", 0.5 }, { "secx_spell_xp_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_flesh_architect", branch_id::mastery, 8, 40, currency_id::major, "secx_living_arsenal", "secx_adaptive_morph", "Flesh Architect", "Архитектор плоти", "+0.75 Flesh Weaving, +0.75 Bio-organic Weapons, potency +6%, energy cost -5%.", "+0,75 Flesh Weaving, +0,75 Bio-organic Weapons, мощность +6%, стоимость энергии -5%.", {{ { "secx_flesh_craft_flat", 0.75 }, { "secx_flesh_combat_flat", 0.75 }, { "secx_spell_power_pct", 6 }, { "secx_spell_cost_pct", -5 } }}, 4, 0, perk_kind::effect },
    { "mg_prime_arcanist", branch_id::mastery, 9, 45, currency_id::major, "mg_archmage", "", "Prime Arcanist", "Прайм: Арканист", "+2 Spellcraft, +20% potency, +15% range; mana cost +30%.", "+2 Spellcraft, +20% мощности, +15% дальности; стоимость маны +30%.", {{ { "mg_spellcraft_flat", 2 }, { "mg_spell_power_pct", 20 }, { "mg_range_pct", 15 }, { "mg_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mg_prime_channeler", branch_id::mastery, 9, 45, currency_id::major, "mg_archmage", "", "Prime Channeler", "Прайм: Проводник", "+40% mana, +30% mana regen, spell cost -20%; casting time +30%.", "+40% маны, +30% восстановления маны, стоимость заклинаний -20%; время сотворения +30%.", {{ { "mg_mana_max_pct", 40 }, { "mg_mana_regen_pct", 30 }, { "mg_spell_cost_pct", -20 }, { "mg_cast_time_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mg_prime_warcaster", branch_id::mastery, 9, 45, currency_id::major, "mg_archmage", "", "Prime Warcaster", "Прайм: Боевой маг", "Casting time -25%, failure chance -20%, potency +20%; spell XP -35%.", "Время сотворения -25%, шанс провала -20%, мощность +20%; опыт заклинаний -35%.", {{ { "mg_cast_time_pct", -25 }, { "mg_fail_pct", -20 }, { "mg_spell_power_pct", 20 }, { "mg_spell_xp_pct", -35 } }}, 4, 0, perk_kind::effect },

    { "mom_prime_kinetic", branch_id::mastery, 9, 45, currency_id::major, "mom_transcendent_focus", "", "Prime Kinetic Savant", "Прайм: Кинетик", "Potency +25%, range +20%, area +20%; psionic cost +30%.", "Мощность +25%, дальность +20%, площадь +20%; стоимость псионики +30%.", {{ { "mom_spell_power_pct", 25 }, { "mom_range_pct", 20 }, { "mom_aoe_pct", 20 }, { "mom_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mom_prime_overclock", branch_id::mastery, 9, 45, currency_id::major, "mom_transcendent_focus", "", "Prime Neural Overclock", "Прайм: Нейроразгон", "+1.5 Metaphysics, activation -25%, power XP +20%; failure +30%.", "+1,5 Metaphysics, время активации -25%, опыт сил +20%; шанс провала +30%.", {{ { "mom_metaphysics_flat", 1.5 }, { "mom_cast_time_pct", -25 }, { "mom_spell_xp_pct", 20 }, { "mom_fail_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mom_prime_ascetic", branch_id::mastery, 9, 45, currency_id::major, "mom_transcendent_focus", "", "Prime Deep Focus", "Прайм: Глубокий фокус", "Failure chance -25%, cost -20%, duration +30%; potency -20%.", "Шанс провала -25%, стоимость -20%, длительность +30%; мощность -20%.", {{ { "mom_fail_pct", -25 }, { "mom_spell_cost_pct", -20 }, { "mom_duration_pct", 30 }, { "mom_spell_power_pct", -20 } }}, 4, 0, perk_kind::effect },

    { "xe_prime_analyst", branch_id::mastery, 9, 45, currency_id::major, "xe_boundary_master", "", "Prime Anomaly Analyst", "Прайм: Аналитик аномалий", "+1.5 Deduction, +1.5 Gramarye, potency +20%; failure +30%.", "+1,5 Deduction, +1,5 Gramarye, мощность +20%; шанс провала +30%.", {{ { "xe_deduction_flat", 1.5 }, { "xe_gramarye_flat", 1.5 }, { "xe_spell_power_pct", 20 }, { "xe_fail_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "xe_prime_resonant", branch_id::mastery, 9, 45, currency_id::major, "xe_boundary_master", "", "Prime Resonance Vessel", "Прайм: Резонансный сосуд", "+40% mana, +30% mana regen, cost -20%; range -20%.", "+40% маны, +30% восстановления маны, стоимость -20%; дальность -20%.", {{ { "xe_mana_max_pct", 40 }, { "xe_mana_regen_pct", 30 }, { "xe_spell_cost_pct", -20 }, { "xe_range_pct", -20 } }}, 4, 0, perk_kind::effect },
    { "xe_prime_riftwalker", branch_id::mastery, 9, 45, currency_id::major, "xe_boundary_master", "", "Prime Riftwalker", "Прайм: Странник разломов", "Range +30%, area +25%, casting time -20%; duration -30%.", "Дальность +30%, площадь +25%, время сотворения -20%; длительность -30%.", {{ { "xe_range_pct", 30 }, { "xe_aoe_pct", 25 }, { "xe_cast_time_pct", -20 }, { "xe_duration_pct", -30 } }}, 4, 0, perk_kind::effect },

    { "af_prime_smartgun", branch_id::mastery, 9, 45, currency_id::major, "af_posthuman_operator", "", "Prime Smartgun Ace", "Прайм: Ас Smartgun", "+2 Smartgun, potency +20%, range +15%; energy cost +30%.", "+2 Smartgun, мощность +20%, дальность +15%; стоимость энергии +30%.", {{ { "af_smartgun_flat", 2 }, { "af_spell_power_pct", 20 }, { "af_range_pct", 15 }, { "af_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "af_prime_systems", branch_id::mastery, 9, 45, currency_id::major, "af_posthuman_operator", "", "Prime Systems Specialist", "Прайм: Специалист по системам", "+1.5 Metaphysics, cost -20%, activation -20%; ability XP -30%.", "+1,5 Metaphysics, стоимость -20%, время активации -20%; опыт способностей -30%.", {{ { "af_metaphysics_flat", 1.5 }, { "af_spell_cost_pct", -20 }, { "af_cast_time_pct", -20 }, { "af_spell_xp_pct", -30 } }}, 4, 0, perk_kind::effect },
    { "af_prime_phase", branch_id::mastery, 9, 45, currency_id::major, "af_posthuman_operator", "", "Prime Phase Engineer", "Прайм: Фазовый инженер", "Range +30%, area +25%, duration +25%; failure chance +30%.", "Дальность +30%, площадь +25%, длительность +25%; шанс провала +30%.", {{ { "af_range_pct", 30 }, { "af_aoe_pct", 25 }, { "af_duration_pct", 25 }, { "af_fail_pct", 30 } }}, 4, 0, perk_kind::effect },

    { "afp_prime_gunslinger", branch_id::mastery, 9, 45, currency_id::major, "afp_prime_integrator", "", "Prime Gunslinger", "Прайм: Стрелок Prime", "+2 Smartgun, potency +20%, range +15%; energy cost +30%.", "+2 Smartgun, мощность +20%, дальность +15%; стоимость энергии +30%.", {{ { "afp_smartgun_flat", 2 }, { "afp_spell_power_pct", 20 }, { "afp_range_pct", 15 }, { "afp_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "afp_prime_systems_specialist", branch_id::mastery, 9, 45, currency_id::major, "afp_prime_integrator", "", "Prime Systems Controller", "Прайм: Системный оператор", "Energy cost -25%, activation -20%, duration +25%; ability XP -30%.", "Стоимость энергии -25%, время активации -20%, длительность +25%; опыт способностей -30%.", {{ { "afp_spell_cost_pct", -25 }, { "afp_cast_time_pct", -20 }, { "afp_duration_pct", 25 }, { "afp_spell_xp_pct", -30 } }}, 4, 0, perk_kind::effect },
    { "afp_prime_translocator", branch_id::mastery, 9, 45, currency_id::major, "afp_prime_integrator", "", "Prime Translocator", "Прайм: Транслокатор", "Range +35%, area +25%, activation time -20%; failure chance +30%.", "Дальность +35%, площадь +25%, время активации -20%; шанс провала +30%.", {{ { "afp_range_pct", 35 }, { "afp_aoe_pct", 25 }, { "afp_cast_time_pct", -20 }, { "afp_fail_pct", 30 } }}, 4, 0, perk_kind::effect },

    { "sec_prime_hunter", branch_id::mastery, 9, 45, currency_id::major, "sec_nightmare_specialist", "", "Prime Hunter", "Прайм: Охотник", "+20% damage vs Secronom, +15% elite damage; -20% Secronom resistance.", "+20% урона по Secronom, +15% урона по элите; -20% защиты от Secronom.", {{ { "sec_damage_pct", 20 }, { "sec_elite_damage_pct", 15 }, { "sec_resist_pct", -20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "sec_prime_bulwark", branch_id::mastery, 9, 45, currency_id::major, "sec_nightmare_specialist", "", "Prime Bulwark", "Прайм: Бастион", "+25% Secronom resistance, +20% elite resistance; -20% damage vs Secronom.", "+25% защиты от Secronom, +20% защиты от элиты; -20% урона по Secronom.", {{ { "sec_resist_pct", 25 }, { "sec_elite_resist_pct", 20 }, { "sec_damage_pct", -20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "sec_prime_crimson", branch_id::mastery, 9, 45, currency_id::major, "sec_nightmare_specialist", "", "Prime Crimson Reaper", "Прайм: Багровый жнец", "+30% Crimson damage, +20% elite damage; -25% Crimson resistance.", "+30% урона по Crimson, +20% урона по элите; -25% защиты от Crimson.", {{ { "sec_crimson_damage_pct", 30 }, { "sec_elite_damage_pct", 20 }, { "sec_crimson_resist_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "secx_prime_architect", branch_id::mastery, 9, 45, currency_id::major, "secx_flesh_architect", "", "Prime Flesh Architect", "Прайм: Архитектор плоти", "+2 Flesh Weaving, potency +20%, ability XP +20%; energy cost +30%.", "+2 Flesh Weaving, мощность +20%, опыт способностей +20%; стоимость энергии +30%.", {{ { "secx_flesh_craft_flat", 2 }, { "secx_spell_power_pct", 20 }, { "secx_spell_xp_pct", 20 }, { "secx_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "secx_prime_predator", branch_id::mastery, 9, 45, currency_id::major, "secx_flesh_architect", "", "Prime Biomorph Predator", "Прайм: Биоморф-хищник", "+2 Bio-organic Weapons, potency +20%, range +15%; failure +30%.", "+2 Bio-organic Weapons, мощность +20%, дальность +15%; шанс провала +30%.", {{ { "secx_flesh_combat_flat", 2 }, { "secx_spell_power_pct", 20 }, { "secx_range_pct", 15 }, { "secx_fail_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "secx_prime_vessel", branch_id::mastery, 9, 45, currency_id::major, "secx_flesh_architect", "", "Prime Flesh Vessel", "Прайм: Сосуд плоти", "Energy cost -25%, failure chance -20%, duration +30%; Flesh Weaving -1.5.", "Стоимость энергии -25%, шанс провала -20%, длительность +30%; Flesh Weaving -1,5.", {{ { "secx_spell_cost_pct", -25 }, { "secx_fail_pct", -20 }, { "secx_duration_pct", 30 }, { "secx_flesh_craft_flat", -1.5 } }}, 4, 0, perk_kind::effect },
    { "cm_critical_eye", branch_id::combat, 3, 12, currency_id::perk, "c_precision", "ce_lessons", "Critical Eye", "Критический глаз", "+2 percentage points to melee critical-hit chance.", "+2 процентных пункта к шансу критического удара в ближнем бою.", {{ { "sp_melee_crit_chance_pct", 2 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_vital_strike", branch_id::combat, 4, 17, currency_id::perk, "cm_critical_eye", "c_bruiser", "Vital Strike", "Смертельный удар", "Melee critical damage +10%.", "Урон критических ударов в ближнем бою +10%.", {{ { "sp_melee_crit_damage_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_execution_window", branch_id::combat, 5, 23, currency_id::major, "cm_vital_strike", "c_veteran", "Execution Window", "Окно для добивания", "Melee critical chance +3 points and critical damage +15%.", "+3 пункта к шансу критического удара и +15% к критическому урону в ближнем бою.", {{ { "sp_melee_crit_chance_pct", 3 }, { "sp_melee_crit_damage_pct", 15 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cm_guard_reserve", branch_id::combat, 3, 14, currency_id::perk, "c_conditioning", "ce_reserve", "Guard Reserve", "Резерв защиты", "+1 block attempt whenever defensive attempts refresh.", "+1 попытка блока при каждом обновлении защитных попыток.", {{ { "sp_block_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_second_reaction", branch_id::combat, 4, 18, currency_id::perk, "c_reflexes", "ce_drills", "Second Reaction", "Вторая реакция", "Gain one extra dodge before dodge attempts refresh.", "Одно дополнительное уклонение до следующего восстановления попыток.", {{ { "sp_dodge_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_ballistic_weakpoints", branch_id::combat, 4, 18, currency_id::perk, "c_precision", "ce_tactics", "Ballistic Weakpoints", "Баллистические уязвимости", "Projectile critical multiplier +12%.", "Множитель критического урона снарядов +12%.", {{ { "sp_ranged_crit_damage_pct", 12 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_lethal_mastery", branch_id::combat, 6, 32, currency_id::major, "cm_execution_window", "cm_ballistic_weakpoints", "Killing Edge", "Смертельная грань", "Melee crit chance +2 points; melee and ranged critical damage +15%.", "+2 пункта к шансу крита в ближнем бою; критический урон ближнего и дальнего боя +15%.", {{ { "sp_melee_crit_chance_pct", 2 }, { "sp_melee_crit_damage_pct", 15 }, { "sp_ranged_crit_damage_pct", 15 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "sm_damage_control", branch_id::survival, 3, 12, currency_id::perk, "s_hardy", "se_lessons", "Damage Control", "Контроль повреждений", "Incoming damage -3%.", "Входящий урон -3%.", {{ { "sp_damage_taken_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sm_hard_to_kill", branch_id::survival, 4, 18, currency_id::perk, "sm_damage_control", "s_resilient", "Hard to Kill", "Живучий", "Incoming damage -4%.", "Входящий урон -4%.", {{ { "sp_damage_taken_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sm_defy_fate", branch_id::survival, 4, 16, currency_id::perk, "s_instinct", "se_adaptive", "Defy Fate", "Обмануть судьбу", "Each rank adds a 1% chance to completely avoid incoming damage, up to 5% at rank V.", "Каждый ранг даёт 1% шанс полностью избежать входящего урона, до 5% на V ранге.", {{ { "sp_damage_avoid_pct", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sm_brace_for_impact", branch_id::survival, 5, 23, currency_id::perk, "sm_hard_to_kill", "s_survivor", "Brace for Impact", "Принять удар", "+1 block attempt and +5% maximum stamina.", "+1 попытка блока и +5% максимума выносливости.", {{ { "sp_block_attempts_bonus", 1 }, { "stamina_max_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sm_indomitable_body", branch_id::survival, 6, 34, currency_id::major, "sm_brace_for_impact", "sm_defy_fate", "Indomitable Body", "Несокрушимое тело", "Another 5% incoming damage reduction and +10% healing.", "Ещё -5% входящего урона и +10% лечения.", {{ { "sp_damage_taken_pct", 5 }, { "healing_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "mm_second_dodge", branch_id::mobility, 3, 12, currency_id::perk, "m_parkour", "me_breath", "Second Dodge", "Второе уклонение", "Gain one extra dodge before dodge attempts refresh.", "Одно дополнительное уклонение до следующего восстановления попыток.", {{ { "sp_dodge_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mm_efficient_evasion", branch_id::mobility, 4, 17, currency_id::perk, "mm_second_dodge", "m_quick", "Efficient Evasion", "Экономное уклонение", "One dodge attempt per refresh costs no stamina.", "Одно уклонение до следующего восстановления попыток не тратит выносливость.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mm_afterimage", branch_id::mobility, 4, 19, currency_id::perk, "m_runner", "me_stride", "Afterimage", "Послеобраз", "1% chance to completely avoid incoming damage.", "1% шанс полностью избежать входящего урона.", {{ { "sp_damage_avoid_pct", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mm_perfect_step", branch_id::mobility, 5, 25, currency_id::major, "mm_efficient_evasion", "mm_afterimage", "Perfect Step", "Идеальный шаг", "+2% chance to completely avoid incoming damage and -3% move cost.", "+2% шанс полностью избежать входящего урона и -3% стоимости движения.", {{ { "sp_damage_avoid_pct", 2 }, { "move_cost_pct", -3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mm_combat_flow", branch_id::mobility, 6, 34, currency_id::major, "mm_perfect_step", "m_untouchable", "Combat Flow", "Боевой поток", "One dodge per refresh costs no stamina; +2% speed and +1 point melee critical chance.", "Одно уклонение до восстановления попыток не тратит выносливость; +2% скорости и +1 пункт шанса крита в ближнем бою.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { "speed_pct", 2 }, { "sp_melee_crit_chance_pct", 1 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "fm_precision_assembly", branch_id::crafting, 4, 18, currency_id::perk, "f_engineer", "fe_standard", "Precision Assembly", "Точная сборка", "+8% crafting speed and +0.5 INT.", "+8% скорости крафта и +0,5 ИНТ.", {{ { "craft_speed_pct", 8 }, { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "fm_field_maintenance", branch_id::crafting, 5, 24, currency_id::perk, "fm_precision_assembly", "f_master", "Field Maintenance", "Полевая эксплуатация", "+8% crafting speed and +10% carrying capacity.", "+8% скорости крафта и +10% грузоподъёмности.", {{ { "craft_speed_pct", 8 }, { "carry_weight_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "fm_masterwork_discipline", branch_id::crafting, 6, 34, currency_id::major, "fm_field_maintenance", "f_genius", "Masterful Worksmanship", "Высшее мастерство", "+12% crafting, +10% reading and +1 INT.", "+12% крафта, +10% чтения и +1 ИНТ.", {{ { "craft_speed_pct", 12 }, { "read_speed_pct", 10 }, { "int_flat", 1 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "gm_weakpoint_eye", branch_id::scavenging, 4, 18, currency_id::perk, "g_awareness", "ge_cache", "Weakpoint Eye", "Глаз на уязвимости", "+1 point melee critical chance and +5% projectile critical multiplier.", "+1 пункт шанса крита в ближнем бою и +5% множителя критического урона снарядов.", {{ { "sp_melee_crit_chance_pct", 1 }, { "sp_ranged_crit_damage_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "gm_scrap_armor_instinct", branch_id::scavenging, 5, 26, currency_id::perk, "g_mule", "ge_instinct", "Scrap Armor Instinct", "Инстинкт бронесборщика", "Incoming damage reduction +2% and carrying capacity +5%.", "-2% входящего урона и +5% грузоподъёмности.", {{ { "sp_damage_taken_pct", 2 }, { "carry_weight_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "gm_escape_route", branch_id::scavenging, 6, 33, currency_id::major, "gm_weakpoint_eye", "gm_scrap_armor_instinct", "Escape Route", "Маршрут отхода", "+1 free dodge, +3% speed and -2% move cost.", "+1 бесплатное уклонение, +3% скорости и -2% стоимости движения.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { "speed_pct", 3 }, { "move_cost_pct", -2 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "am_combat_synthesis", branch_id::mastery, 4, 19, currency_id::perk, "a_insight", "ae_integrate", "Battle Sense", "Боевое чутьё", "+1 point melee critical chance; melee and ranged critical damage +5%.", "+1 пункт шанса крита; критический урон ближнего и дальнего боя +5%.", {{ { "sp_melee_crit_chance_pct", 1 }, { "sp_melee_crit_damage_pct", 5 }, { "sp_ranged_crit_damage_pct", 5 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "am_survival_synthesis", branch_id::mastery, 5, 25, currency_id::major, "a_paragon", "am_combat_synthesis", "Hardened Reflexes", "Закалённые рефлексы", "Incoming damage -2% and +1% chance to completely avoid incoming damage.", "-2% входящего урона и +1% шанс полностью избежать входящего урона.", {{ { "sp_damage_taken_pct", 2 }, { "sp_damage_avoid_pct", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "am_reflex_memory", branch_id::mastery, 5, 27, currency_id::perk, "a_polymath", "ae_longgame", "Reflex Memory", "Память рефлексов", "One dodge per refresh costs no stamina; +2% speed.", "Одно уклонение до восстановления попыток не тратит выносливость; +2% скорости.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { "speed_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "am_apex_adaptation", branch_id::mastery, 6, 40, currency_id::major, "a_transcendent", "am_survival_synthesis", "Total Adaptation", "Полная адаптация", "Incoming damage -2%, +1 point melee crit chance, +5% melee crit damage and +5% ranged crit damage.", "-2% входящего урона, +1 пункт шанса крита, +5% критического урона ближнего и дальнего боя.", {{ { "sp_damage_taken_pct", 2 }, { "sp_melee_crit_chance_pct", 1 }, { "sp_melee_crit_damage_pct", 5 }, { "sp_ranged_crit_damage_pct", 5 } }}, 4, 0, perk_kind::effect },
    { "cr_riposte", branch_id::combat, 4, 20, currency_id::perk, "cm_second_reaction", "ce_tactics", "Riposte", "Рипост", "Reactive: after a successful dodge on foot, if no martial-arts counter fires and you have combat stamina, gain a 20% chance to counter the adjacent hostile attacker. Friendly, neutral, hallucination and mounted cases are never auto-targeted.", "Реакция: после успешного уклонения пешком, если не сработала контратака боевого искусства и хватает выносливости, есть 20% шанс контратаковать соседнего враждебного атакующего. Дружественные, нейтральные, иллюзорные и ситуации верхом исключены.", {{ { "sp_riposte_chance_pct", 20 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cr_counterflow", branch_id::combat, 5, 25, currency_id::perk, "cr_riposte", "c_veteran", "Counterflow", "Поток контратаки", "Ripostes refund 50% of their base move cost; successful dodges also return 5 moves.", "Рипост возвращает 50% базовой стоимости атаки; успешное уклонение также возвращает 5 ед. хода.", {{ { "sp_riposte_refund_pct", 50 }, { "sp_on_dodge_moves", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cr_critical_surge", branch_id::combat, 4, 21, currency_id::perk, "cm_critical_eye", "cm_vital_strike", "Critical Surge", "Критический импульс", "A damaging melee critical against a hostile target returns 10 moves and restores 2% maximum stamina. Fleeing enemies still count.", "Критический удар в ближнем бою по враждебной цели возвращает 10 ед. хода и восстанавливает 2% максимальной выносливости. Убегающие враги тоже учитываются.", {{ { "sp_on_crit_moves", 10 }, { "sp_on_crit_stamina_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cr_execution_protocol", branch_id::combat, 5, 27, currency_id::major, "cm_execution_window", "cm_vital_strike", "Finisher", "Добивание", "Deal +60% damage to hostile targets at 18% health or less. Friendly and neutral targets are unaffected.", "По враждебным целям с 18% здоровья или меньше урон увеличивается на 60%. Дружественные и нейтральные цели не затрагиваются.", {{ { "sp_execute_threshold_pct", 18 }, { "sp_execute_damage_pct", 60 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cr_predator_momentum", branch_id::combat, 5, 28, currency_id::perk, "c_veteran", "ce_tactics", "Predator Momentum", "Импульс хищника", "Killing a hostile monster that grants XP builds Momentum, up to 3 stacks for 12 turns. Each stack grants +3% damage and +1% speed. Allies, neutral creatures and revived enemies that grant no XP do not add stacks.", "Убийство враждебного монстра, за которое начисляется опыт, даёт заряд Импульса: до 3 зарядов на 12 ходов. Каждый заряд даёт +3% урона и +1% скорости. Союзники, нейтральные существа и возрождённые враги без опыта зарядов не дают.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 0, perk_kind::effect },
    { "cr_relentless_momentum", branch_id::combat, 6, 38, currency_id::major, "cr_predator_momentum", "cm_lethal_mastery", "Relentless Momentum", "Неудержимый импульс", "Momentum can build to 5 stacks and lasts 20 turns. Killing a hostile monster that grants XP also returns 15 moves and 3% maximum stamina.", "Импульс накапливается до 5 зарядов и длится 20 ходов. Убийство враждебного монстра, за которое начисляется опыт, также возвращает 15 ед. хода и 3% максимальной выносливости.", {{ { "sp_on_kill_moves", 15 }, { "sp_on_kill_stamina_pct", 3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "sr_adrenal_recovery", branch_id::survival, 4, 20, currency_id::perk, "sm_hard_to_kill", "s_recovery", "Adrenal Recovery", "Адреналиновое восстановление", "A hostile monster kill credited to you that grants XP restores 4% maximum stamina.", "Убийство враждебного монстра, засчитанное вам и дающее опыт, восстанавливает 4% максимальной выносливости.", {{ { "sp_on_kill_stamina_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sr_battle_breath", branch_id::survival, 5, 26, currency_id::perk, "sr_adrenal_recovery", "s_survivor", "Battle Breath", "Боевое дыхание", "Melee criticals restore 2% maximum stamina; hostile monster kills that grant XP return 5 moves.", "Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства враждебных монстров, за которые начисляется опыт, возвращают 5 ед. хода.", {{ { "sp_on_crit_stamina_pct", 2 }, { "sp_on_kill_moves", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "mr_slipstream", branch_id::mobility, 4, 19, currency_id::perk, "mm_second_dodge", "me_stride", "Slipstream", "Скольжение", "Every successful dodge immediately returns 12 moves.", "Каждое успешное уклонение немедленно возвращает 12 ед. хода.", {{ { "sp_on_dodge_moves", 12 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mr_breath_return", branch_id::mobility, 4, 21, currency_id::perk, "mm_efficient_evasion", "m_marathon", "Breath Return", "Возврат дыхания", "Every successful dodge restores 3% maximum stamina.", "Каждое успешное уклонение восстанавливает 3% максимальной выносливости.", {{ { "sp_on_dodge_stamina_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mr_reactive_step", branch_id::mobility, 5, 27, currency_id::perk, "mr_slipstream", "mm_perfect_step", "Reactive Step", "Ответный шаг", "Successful dodges gain +10 percentage points of riposte chance; ripostes refund 25% of their move cost.", "Успешные уклонения получают +10 процентных пунктов шанса рипоста; рипосты возвращают 25% стоимости хода.", {{ { "sp_riposte_chance_pct", 10 }, { "sp_riposte_refund_pct", 25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mr_kinetic_chain", branch_id::mobility, 6, 36, currency_id::major, "mr_breath_return", "mm_combat_flow", "Kinetic Chain", "Кинетическая цепь", "Criticals return 5 moves; hostile monster kills that grant XP return 10 moves.", "Криты возвращают 5 ед. хода; убийства враждебных монстров, за которые начисляется опыт, — 10 ед. хода.", {{ { "sp_on_crit_moves", 5 }, { "sp_on_kill_moves", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "fr_quality_control", branch_id::crafting, 4, 26, currency_id::perk, "fm_precision_assembly", "fe_theory", "Quality Control", "Контроль качества", "+0.25 to crafting success checks; the displayed success chance uses the same bonus.", "+0,25 к проверкам успеха крафта; отображаемый шанс успеха учитывает тот же бонус.", {{ { "sp_craft_success_roll_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_second_measure", branch_id::crafting, 5, 24, currency_id::perk, "fr_quality_control", "f_master", "Measure Twice", "Семь раз отмерь", "10% chance to prevent a crafting failure before it causes defects, destroys components or removes progress. The next failure check still advances normally.", "10% шанс предотвратить ошибку крафта до появления дефекта, потери компонентов или прогресса. Следующая проверка ошибки всё равно сдвигается вперёд.", {{ { "sp_craft_failure_save_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_material_discipline", branch_id::crafting, 5, 26, currency_id::perk, "fm_field_maintenance", "fr_quality_control", "Careful Handling", "Бережная работа", "Each component threatened by a crafting failure has a 25% chance to survive.", "Каждый компонент, которому грозит уничтожение при ошибке крафта, имеет 25% шанс сохраниться.", {{ { "sp_craft_component_loss_reduction_pct", 25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_failure_analysis", branch_id::crafting, 5, 28, currency_id::perk, "fr_second_measure", "fr_material_discipline", "Failure Analysis", "Анализ ошибок", "Lose 35% less progress when crafting fails.", "При ошибке крафта теряется на 35% меньше прогресса.", {{ { "sp_craft_progress_loss_reduction_pct", 35 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_zero_defect", branch_id::crafting, 6, 38, currency_id::major, "fr_failure_analysis", "fm_masterwork_discipline", "Flawless Work", "Безупречная работа", "+10% chance to prevent a crafting failure, +15% component protection and 20% less progress loss.", "+10% шанс предотвратить ошибку крафта, +15% защиты компонентов и на 20% меньше потери прогресса.", {{ { "sp_craft_failure_save_pct", 10 }, { "sp_craft_component_loss_reduction_pct", 15 }, { "sp_craft_progress_loss_reduction_pct", 20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "gr_trap_reader", branch_id::scavenging, 3, 14, currency_id::perk, "g_awareness", "ge_field", "Trap Reader", "Чтение ловушек", "+2 to trap detection checks.", "+2 к проверкам обнаружения ловушек.", {{ { "sp_trap_detection_flat", 2 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_lock_whisperer", branch_id::scavenging, 4, 19, currency_id::perk, "gr_trap_reader", "g_pathfinder", "Lock Whisperer", "Шёпот замков", "+2 to lockpicking checks.", "+2 к проверкам взлома.", {{ { "sp_lockpick_roll_flat", 2 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_quick_entry", branch_id::scavenging, 4, 22, currency_id::perk, "gr_lock_whisperer", "ge_network", "Quick Entry", "Быстрый вход", "Lockpicking is 20% faster, but cannot go below 30 seconds with normal picks or 5 seconds with perfect picks.", "Взлом на 20% быстрее, но не может занять меньше 30 секунд обычной отмычкой или 5 секунд идеальной.", {{ { "sp_lockpick_time_reduction_pct", 20 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_gentle_tools", branch_id::scavenging, 5, 27, currency_id::perk, "gr_lock_whisperer", "gm_scrap_armor_instinct", "Gentle Tools", "Бережный инструмент", "50% chance to keep your lockpick from being damaged or destroyed after a severe failure.", "50% шанс сохранить отмычку от повреждения или уничтожения после тяжёлой неудачи.", {{ { "sp_lockpick_tool_protection_pct", 50 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_alarm_bypass", branch_id::scavenging, 6, 35, currency_id::major, "gr_quick_entry", "gr_gentle_tools", "Alarm Bypass", "Обход сигнализации", "25% chance to prevent an alarm from triggering after attempting an alarmed lock.", "25% шанс не дать сигнализации сработать после попытки взлома защищённого замка.", {{ { "sp_lockpick_alarm_avoid_pct", 25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },

    { "ar_reactive_synthesis", branch_id::mastery, 5, 28, currency_id::perk, "am_combat_synthesis", "ae_integrate", "Reflex Chain", "Цепная реакция", "Dodges, melee criticals and hostile monster kills that grant XP return 5 moves.", "Уклонения, критические удары в ближнем бою и убийства враждебных монстров, за которые начисляется опыт, возвращают 5 ед. хода.", {{ { "sp_on_dodge_moves", 5 }, { "sp_on_crit_moves", 5 }, { "sp_on_kill_moves", 5 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "ar_momentum_engine", branch_id::mastery, 6, 40, currency_id::major, "ar_reactive_synthesis", "am_apex_adaptation", "Unbroken Momentum", "Непрерывный импульс", "With Predator Momentum, each stack gains another +1% damage and +1% speed, and the maximum increases by 2 stacks.", "С Импульсом хищника каждый заряд даёт ещё +1% урона и +1% скорости, а максимум увеличивается на 2 заряда.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 0, perk_kind::effect },
    { "ar_perfect_process", branch_id::mastery, 6, 40, currency_id::major, "ar_reactive_synthesis", "ae_ascendant", "Masterful Work", "Работа мастера", "+5% chance to prevent a crafting failure, +10% component protection and 15% less progress loss.", "+5% шанс предотвратить ошибку крафта, +10% защиты компонентов и на 15% меньше потери прогресса.", {{ { "sp_craft_failure_save_pct", 5 }, { "sp_craft_component_loss_reduction_pct", 10 }, { "sp_craft_progress_loss_reduction_pct", 15 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect }
};

bool russian()
{
    const char *raw = host && host->get_locale ? host->get_locale() : "en";
    const std::string locale = raw ? raw : "en";
    return locale == "ru" || locale.rfind( "ru_", 0 ) == 0;
}

std::string tr( const char *en, const char *ru )
{
    return russian() ? ru : en;
}

constexpr const char *xp_rate_setting = "NCMM_SP_XP_RATE";
constexpr const char *stat_power_setting = "NCMM_SP_STAT_POWER";

int progression_percent_setting( const char *setting_id, int fallback )
{
    if( host2 == nullptr || host2->world_setting_get_string == nullptr ) {
        return fallback;
    }
    const char *raw = host2->world_setting_get_string( setting_id, "" );
    if( raw == nullptr || raw[0] == '\0' ) {
        return fallback;
    }
    char *end = nullptr;
    const long value = std::strtol( raw, &end, 10 );
    if( end == raw || ( end != nullptr && *end != '\0' ) ) {
        return fallback;
    }
    return static_cast<int>( std::max<long>( 25, std::min<long>( 300, value ) ) );
}

int progression_xp_rate_pct()
{
    return progression_percent_setting( xp_rate_setting, 100 );
}

int progression_stat_power_pct()
{
    return progression_percent_setting( stat_power_setting, 100 );
}

bool configure_progression_settings()
{
    if( host2 == nullptr || host2->world_setting_register_enum == nullptr ||
        host2->world_setting_get_string == nullptr ) {
        return false;
    }
    static const char *values[] = {
        "25", "50", "75", "100", "125", "150", "175", "200", "225", "250", "275", "300"
    };
    static const char *labels[] = {
        "25%", "50%", "75%", "100%", "125%", "150%", "175%", "200%", "225%", "250%", "275%", "300%"
    };
    const size_t count = sizeof( values ) / sizeof( values[0] );
    if( !host2->world_setting_register_enum(
            module_id, xp_rate_setting,
            russian() ? "Получение опыта" : "Experience gain",
            russian() ? "Множитель опыта веток и общего уровня Survivor после антифарма. 100% сохраняет стандартный баланс." :
                        "Multiplier for branch XP and the global Survivor level after anti-farm adjustments. 100% keeps the default balance.",
            values, labels, count, "100", NCMM_WORLD_SETTING_LIVE ) ) {
        return false;
    }
    if( !host2->world_setting_register_enum(
            module_id, stat_power_setting,
            russian() ? "Сила стат-перков" : "Stat perk strength",
            russian() ? "Масштабирует прямые бонусы перков к характеристикам и пассивным параметрам. Механические перки не затрагиваются." :
                        "Scales direct bonuses from perks that grant attributes and passive stats. Mechanical perks are not affected.",
            values, labels, count, "100", NCMM_WORLD_SETTING_LIVE ) ) {
        return false;
    }
    return true;
}

bool active_world_mod( const char *mod_id )
{
    return host != nullptr && host->world_mod_active != nullptr &&
           host->world_mod_active( mod_id ) != 0;
}
int64_t get_state( const std::string &key, int64_t fallback )
{
    if( !host || !host->character_state_get_i64 ) {
        return fallback;
    }
    return host->character_state_get_i64( module_id, key.c_str(), fallback );
}

void set_state( const std::string &key, int64_t value )
{
    if( host && host->character_state_set_i64 ) {
        host->character_state_set_i64( module_id, key.c_str(), value );
    }
}

void message( const std::string &text )
{
    if( host && host->ui_message ) {
        host->ui_message( text.c_str() );
    }
}

bool character_available()
{
    return host && host->character_state_available && host->character_state_available();
}

const std::string &perk_key( const perk_def &perk )
{
    static const std::map<std::string_view, std::string> keys = []() {
        std::map<std::string_view, std::string> result;
        for( const perk_def &entry : perks ) {
            result.emplace( std::string_view( entry.id ), std::string( "p_" ) + entry.id );
        }
        return result;
    }();
    const auto it = keys.find( std::string_view( perk.id ) );
    if( it != keys.end() ) {
        return it->second;
    }
    static const std::string invalid_key = "p_invalid";
    return invalid_key;
}

struct ranked_perk_rule {
    const char *id;
    int max_rank;
    double extra_scale;
};

const ranked_perk_rule *ranked_perk_rule_for( const perk_def &perk )
{
    static const ranked_perk_rule rules[] = {
        { "c_conditioning", 5, 0.125 }, { "s_field", 3, 0.50 },
        { "sm_defy_fate", 5, 1.0 },
        { "m_light", 5, 1.0 / 6.0 }, { "f_hands", 5, 0.40 },
        { "g_route", 3, 1.0 / 3.0 }, { "a_adapt", 5, 0.20 },

        { "mg_arcane_focus", 5, 0.125 }, { "mg_spellcraft_drills", 5, 0.125 },
        { "mg_mana_sensitivity", 3, 0.25 }, { "mg_mana_regeneration", 3, 0.25 },
        { "mg_mana_vampirism", 5, 1.0 },
        { "mom_mental_focus", 5, 0.125 }, { "mom_metaphysical_method", 5, 0.125 },
        { "mom_neural_reserve", 3, 0.25 }, { "mom_channel_discipline", 3, 0.25 },
        { "xe_anomaly_method", 5, 0.125 }, { "xe_gramarye_studies", 5, 0.125 },
        { "xe_field_agent", 3, 0.25 }, { "xe_dimensional_model", 3, 0.25 },
        { "af_systems_operator", 5, 0.125 }, { "af_targeting_link", 5, 0.125 },
        { "af_metaphysical_training", 5, 0.125 }, { "af_conditioning", 3, 0.25 },

        { "afp_prime_operator", 5, 0.125 }, { "afp_smartgun_interface", 5, 0.125 },
        { "afp_systems_theory", 3, 0.25 }, { "afp_translocation_calculus", 3, 0.25 },
        { "sec_field_researcher", 5, 0.125 }, { "sec_hunter_drills", 5, 0.125 },
        { "sec_pathogen_hardening", 3, 0.25 }, { "sec_crimson_anatomy", 3, 0.25 },
        { "secx_flesh_initiate", 5, 0.125 }, { "secx_flesh_weaving", 5, 0.125 },
        { "secx_biomorph_training", 5, 0.125 }, { "secx_neural_link", 3, 0.25 }
    };
    const std::string id = perk.id ? perk.id : "";
    for( const ranked_perk_rule &rule : rules ) {
        if( id == rule.id ) return &rule;
    }
    return nullptr;
}

bool ranked_perk_id( const perk_def &perk )
{
    return ranked_perk_rule_for( perk ) != nullptr;
}

int perk_max_rank( const perk_def &perk )
{
    const ranked_perk_rule *rule = ranked_perk_rule_for( perk );
    return rule ? rule->max_rank : 1;
}

double perk_extra_rank_scale( const perk_def &perk )
{
    const ranked_perk_rule *rule = ranked_perk_rule_for( perk );
    return rule ? rule->extra_scale : 0.0;
}

int perk_rank( const perk_def &perk )
{
    return static_cast<int>( std::max<int64_t>(
        0, std::min<int64_t>( perk_max_rank( perk ), get_state( perk_key( perk ), 0 ) ) ) );
}

bool perk_maxed( const perk_def &perk )
{
    return perk_rank( perk ) >= perk_max_rank( perk );
}

double perk_rank_multiplier_for( const perk_def &perk, int rank )
{
    if( rank <= 0 ) {
        return 0.0;
    }
    rank = std::min( rank, perk_max_rank( perk ) );
    return 1.0 + static_cast<double>( rank - 1 ) * perk_extra_rank_scale( perk );
}

double perk_rank_multiplier( const perk_def &perk )
{
    return perk_rank_multiplier_for( perk, perk_rank( perk ) );
}

std::string rank_roman( int rank )
{
    switch( rank ) {
        case 1: return "I";
        case 2: return "II";
        case 3: return "III";
        case 4: return "IV";
        case 5: return "V";
        default: return std::to_string( rank );
    }
}

std::string rank_chevrons( const perk_def &perk )
{
    const int max_rank = perk_max_rank( perk );
    if( max_rank <= 1 ) return {};
    const int rank = perk_rank( perk );
    std::string result;
    for( int i = 0; i < max_rank; ++i ) {
        result += i < rank ? "▲" : "△";
    }
    return result;
}

std::string perk_display_name( const perk_def &perk )
{
    std::string result = russian() ? perk.name_ru : perk.name_en;
    const int rank = perk_rank( perk );
    if( perk_max_rank( perk ) > 1 && rank > 0 ) {
        result += " " + rank_roman( rank );
    }
    return result;
}

enum class integration_id {
    none,
    magiclysm,
    mindovermatter,
    xedra_evolved,
    aftershock_exoplanet,
    aftershock_prime,
    secronom,
    secronom_plus
};

integration_id perk_integration( const perk_def &perk )
{
    static const std::map<std::string_view, integration_id> registry = {
        { "mg_arcane_focus", integration_id::magiclysm },
        { "mg_mana_sensitivity", integration_id::magiclysm },
        { "mg_battlemage", integration_id::magiclysm },
        { "mg_wayfarer", integration_id::magiclysm },
        { "mg_mana_regeneration", integration_id::magiclysm },
        { "mg_stable_formula", integration_id::magiclysm },
        { "mg_shaped_evocation", integration_id::magiclysm },
        { "mg_efficient_channels", integration_id::magiclysm },
        { "mg_spellcraft_drills", integration_id::magiclysm },
        { "mg_sustained_weave", integration_id::magiclysm },
        { "mg_deep_reservoir", integration_id::magiclysm },
        { "mg_quick_invocation", integration_id::magiclysm },
        { "mg_arcane_geometry", integration_id::magiclysm },
        { "mg_mana_mastery", integration_id::magiclysm },
        { "mg_ritual_craft", integration_id::magiclysm },
        { "mg_high_thaumaturgy", integration_id::magiclysm },
        { "mg_efficient_theory", integration_id::magiclysm },
        { "mg_combat_weave", integration_id::magiclysm },
        { "mg_resonant_reserve", integration_id::magiclysm },
        { "mg_archmage", integration_id::magiclysm },
        { "mg_mana_vampirism", integration_id::magiclysm },
        { "mg_mana_hand_3", integration_id::magiclysm },
        { "mg_mana_hand_4", integration_id::magiclysm },
        { "mom_mental_focus", integration_id::mindovermatter },
        { "mom_still_mind", integration_id::mindovermatter },
        { "mom_neural_reserve", integration_id::mindovermatter },
        { "mom_kinetic_control", integration_id::mindovermatter },
        { "mom_channel_discipline", integration_id::mindovermatter },
        { "mom_efficient_channel", integration_id::mindovermatter },
        { "mom_psionic_pressure", integration_id::mindovermatter },
        { "mom_metaphysical_method", integration_id::mindovermatter },
        { "mom_controlled_exposure", integration_id::mindovermatter },
        { "mom_extended_pattern", integration_id::mindovermatter },
        { "mom_mental_lattice", integration_id::mindovermatter },
        { "mom_recovery_cycle", integration_id::mindovermatter },
        { "mom_field_shaping", integration_id::mindovermatter },
        { "mom_combat_focus", integration_id::mindovermatter },
        { "mom_nether_discipline", integration_id::mindovermatter },
        { "mom_noetic_projection", integration_id::mindovermatter },
        { "mom_stable_channel", integration_id::mindovermatter },
        { "mom_precise_manifestation", integration_id::mindovermatter },
        { "mom_efficient_force", integration_id::mindovermatter },
        { "mom_transcendent_focus", integration_id::mindovermatter },
        { "xe_anomaly_method", integration_id::xedra_evolved },
        { "xe_field_agent", integration_id::xedra_evolved },
        { "xe_dross_resonance", integration_id::xedra_evolved },
        { "xe_dimensional_hunter", integration_id::xedra_evolved },
        { "xe_gramarye_studies", integration_id::xedra_evolved },
        { "xe_dream_metabolism", integration_id::xedra_evolved },
        { "xe_fast_manifestation", integration_id::xedra_evolved },
        { "xe_dimensional_model", integration_id::xedra_evolved },
        { "xe_efficient_oneiromancy", integration_id::xedra_evolved },
        { "xe_oneiric_force", integration_id::xedra_evolved },
        { "xe_pattern_archive", integration_id::xedra_evolved },
        { "xe_dream_reservoir", integration_id::xedra_evolved },
        { "xe_liminal_persistence", integration_id::xedra_evolved },
        { "xe_occult_engineer", integration_id::xedra_evolved },
        { "xe_dream_economy", integration_id::xedra_evolved },
        { "xe_reality_shaper", integration_id::xedra_evolved },
        { "xe_dream_theorist", integration_id::xedra_evolved },
        { "xe_liminal_engineer", integration_id::xedra_evolved },
        { "xe_oneiric_architect", integration_id::xedra_evolved },
        { "xe_boundary_master", integration_id::xedra_evolved },
        { "af_systems_operator", integration_id::aftershock_exoplanet },
        { "af_targeting_link", integration_id::aftershock_exoplanet },
        { "af_conditioning", integration_id::aftershock_exoplanet },
        { "af_expedition_logistics", integration_id::aftershock_exoplanet },
        { "af_predictive_fire", integration_id::aftershock_exoplanet },
        { "af_metaphysical_training", integration_id::aftershock_exoplanet },
        { "af_telekinetic_geometry", integration_id::aftershock_exoplanet },
        { "af_sensor_fusion", integration_id::aftershock_exoplanet },
        { "af_stable_esper", integration_id::aftershock_exoplanet },
        { "af_esper_force", integration_id::aftershock_exoplanet },
        { "af_combat_technician", integration_id::aftershock_exoplanet },
        { "af_efficient_esper", integration_id::aftershock_exoplanet },
        { "af_sustained_phenomena", integration_id::aftershock_exoplanet },
        { "af_smartgun_mastery", integration_id::aftershock_exoplanet },
        { "af_esper_mastery", integration_id::aftershock_exoplanet },
        { "af_noetic_artillery", integration_id::aftershock_exoplanet },
        { "af_neural_targeting", integration_id::aftershock_exoplanet },
        { "af_psionic_firecontrol", integration_id::aftershock_exoplanet },
        { "af_stable_projection", integration_id::aftershock_exoplanet },
        { "af_posthuman_operator", integration_id::aftershock_exoplanet },
        { "afp_prime_operator", integration_id::aftershock_prime },
        { "afp_smartgun_interface", integration_id::aftershock_prime },
        { "afp_systems_theory", integration_id::aftershock_prime },
        { "afp_translocation_calculus", integration_id::aftershock_prime },
        { "afp_predictive_targeting", integration_id::aftershock_prime },
        { "afp_power_budget", integration_id::aftershock_prime },
        { "afp_spatial_solution", integration_id::aftershock_prime },
        { "afp_sensor_fusion", integration_id::aftershock_prime },
        { "afp_utility_protocols", integration_id::aftershock_prime },
        { "afp_stable_translation", integration_id::aftershock_prime },
        { "afp_combat_technician", integration_id::aftershock_prime },
        { "afp_systems_automation", integration_id::aftershock_prime },
        { "afp_field_projection", integration_id::aftershock_prime },
        { "afp_smartgun_mastery", integration_id::aftershock_prime },
        { "afp_prime_systems_mastery", integration_id::aftershock_prime },
        { "afp_translocation_mastery", integration_id::aftershock_prime },
        { "afp_integrated_firecontrol", integration_id::aftershock_prime },
        { "afp_remote_geometry", integration_id::aftershock_prime },
        { "afp_mobile_platform", integration_id::aftershock_prime },
        { "afp_prime_integrator", integration_id::aftershock_prime },
        { "sec_field_researcher", integration_id::secronom },
        { "sec_hunter_drills", integration_id::secronom },
        { "sec_pathogen_hardening", integration_id::secronom },
        { "sec_crimson_anatomy", integration_id::secronom },
        { "sec_vital_targets", integration_id::secronom },
        { "sec_toxicology", integration_id::secronom },
        { "sec_flesh_patterning", integration_id::secronom },
        { "sec_elite_tracking", integration_id::secronom },
        { "sec_adaptive_response", integration_id::secronom },
        { "sec_crimson_countermeasures", integration_id::secronom },
        { "sec_execution_protocol", integration_id::secronom },
        { "sec_hardened_survivor", integration_id::secronom },
        { "sec_fleshbreaker", integration_id::secronom },
        { "sec_hunter_mastery", integration_id::secronom },
        { "sec_survival_mastery", integration_id::secronom },
        { "sec_crimson_mastery", integration_id::secronom },
        { "sec_apex_hunter", integration_id::secronom },
        { "sec_ultimate_protocol", integration_id::secronom },
        { "sec_red_harvest", integration_id::secronom },
        { "sec_nightmare_specialist", integration_id::secronom },
        { "secx_flesh_initiate", integration_id::secronom_plus },
        { "secx_flesh_weaving", integration_id::secronom_plus },
        { "secx_biomorph_training", integration_id::secronom_plus },
        { "secx_neural_link", integration_id::secronom_plus },
        { "secx_resource_shaping", integration_id::secronom_plus },
        { "secx_armament_drills", integration_id::secronom_plus },
        { "secx_morph_control", integration_id::secronom_plus },
        { "secx_advanced_weaving", integration_id::secronom_plus },
        { "secx_combat_instinct", integration_id::secronom_plus },
        { "secx_flesh_channel", integration_id::secronom_plus },
        { "secx_material_mastery", integration_id::secronom_plus },
        { "secx_bioorganic_mastery", integration_id::secronom_plus },
        { "secx_vessel_resonance", integration_id::secronom_plus },
        { "secx_fleshcraft_mastery", integration_id::secronom_plus },
        { "secx_biomorph_mastery", integration_id::secronom_plus },
        { "secx_flesh_vessel_mastery", integration_id::secronom_plus },
        { "secx_living_arsenal", integration_id::secronom_plus },
        { "secx_adaptive_morph", integration_id::secronom_plus },
        { "secx_artificial_ecology", integration_id::secronom_plus },
        { "secx_flesh_architect", integration_id::secronom_plus },
        { "mg_prime_arcanist", integration_id::magiclysm },
        { "mg_prime_channeler", integration_id::magiclysm },
        { "mg_prime_warcaster", integration_id::magiclysm },
        { "mom_prime_kinetic", integration_id::mindovermatter },
        { "mom_prime_overclock", integration_id::mindovermatter },
        { "mom_prime_ascetic", integration_id::mindovermatter },
        { "xe_prime_analyst", integration_id::xedra_evolved },
        { "xe_prime_resonant", integration_id::xedra_evolved },
        { "xe_prime_riftwalker", integration_id::xedra_evolved },
        { "af_prime_smartgun", integration_id::aftershock_exoplanet },
        { "af_prime_systems", integration_id::aftershock_exoplanet },
        { "af_prime_phase", integration_id::aftershock_exoplanet },
        { "afp_prime_gunslinger", integration_id::aftershock_prime },
        { "afp_prime_systems_specialist", integration_id::aftershock_prime },
        { "afp_prime_translocator", integration_id::aftershock_prime },
        { "sec_prime_hunter", integration_id::secronom },
        { "sec_prime_bulwark", integration_id::secronom },
        { "sec_prime_crimson", integration_id::secronom },
        { "secx_prime_architect", integration_id::secronom_plus },
        { "secx_prime_predator", integration_id::secronom_plus },
        { "secx_prime_vessel", integration_id::secronom_plus },
    };
    const auto it = registry.find( perk.id ? std::string_view( perk.id ) : std::string_view() );
    return it == registry.end() ? integration_id::none : it->second;
}

bool integration_perk( const perk_def &perk )
{
    return perk_integration( perk ) != integration_id::none;
}

const char *integration_mod_id( integration_id integration )
{
    switch( integration ) {
        case integration_id::magiclysm: return "magiclysm";
        case integration_id::mindovermatter: return "mindovermatter";
        case integration_id::xedra_evolved: return "xedra_evolved";
        case integration_id::aftershock_exoplanet: return "aftershock_exoplanet";
        case integration_id::aftershock_prime: return "aftershock_prime";
        case integration_id::secronom: return "secronom";
        case integration_id::secronom_plus: return "secronom_lore_expansion";
        case integration_id::none: break;
    }
    return "";
}

const char *integration_mod_id( const perk_def &perk )
{
    return integration_mod_id( perk_integration( perk ) );
}

std::string integration_mod_name( const perk_def &perk )
{
    switch( perk_integration( perk ) ) {
        case integration_id::magiclysm: return "Magiclysm";
        case integration_id::mindovermatter: return "Mind Over Matter";
        case integration_id::xedra_evolved: return "Xedra Evolved";
        case integration_id::aftershock_exoplanet: return "Aftershock Exoplanet";
        case integration_id::aftershock_prime: return "Aftershock Prime";
        case integration_id::secronom: return "Secronom";
        case integration_id::secronom_plus: return "Secronom+";
        case integration_id::none: break;
    }
    return {};
}

uint32_t branch_theme_color( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return NCMM_UI_COLOR_RED;
        case branch_id::survival: return NCMM_UI_COLOR_GREEN;
        case branch_id::mobility: return NCMM_UI_COLOR_CYAN;
        case branch_id::crafting: return NCMM_UI_COLOR_YELLOW;
        case branch_id::scavenging: return NCMM_UI_COLOR_BLUE;
        case branch_id::mastery: return NCMM_UI_COLOR_MAGENTA;
    }
    return NCMM_UI_COLOR_DEFAULT;
}

uint32_t integration_theme_color( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "mindovermatter" ) return NCMM_UI_COLOR_CYAN;
    if( mod_id == "xedra_evolved" ) return NCMM_UI_COLOR_GREEN;
    if( mod_id == "aftershock_exoplanet" ) return NCMM_UI_COLOR_BLUE;
    if( mod_id == "aftershock_prime" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "secronom" ) return NCMM_UI_COLOR_RED;
    if( mod_id == "secronom_lore_expansion" ) return NCMM_UI_COLOR_YELLOW;
    return NCMM_UI_COLOR_DEFAULT;
}

int mod_prime_root_slot( const char *raw_id );

bool prime_visual_perk( const perk_def &perk )
{
    const std::string id = perk.id ? perk.id : "";
    return id.rfind( "spc_", 0 ) == 0 || mod_prime_root_slot( perk.id ) > 0;
}

uint32_t rpg_border_style_for( const perk_def &perk )
{
    if( prime_visual_perk( perk ) ) {
        return NCMM_UI_BORDER_PRIME;
    }
    if( perk.currency == currency_id::major ) {
        return NCMM_UI_BORDER_MAJOR;
    }
    return NCMM_UI_BORDER_NORMAL;
}

std::string rpg_detail_body( const perk_def &perk, const std::string &body,
                             const std::string &requires_text )
{
    std::string bonus = body;
    std::string drawback;
    if( prime_visual_perk( perk ) ) {
        const size_t colon = bonus.find( ':' );
        if( colon != std::string::npos ) {
            bonus = bonus.substr( colon + 1 );
        }
        const size_t semicolon = bonus.find( ';' );
        if( semicolon != std::string::npos ) {
            drawback = bonus.substr( semicolon + 1 );
            bonus = bonus.substr( 0, semicolon );
        }
        while( !bonus.empty() && bonus.front() == ' ' ) bonus.erase( bonus.begin() );
        while( !drawback.empty() && drawback.front() == ' ' ) drawback.erase( drawback.begin() );
    }

    std::string result;
    result += tr( "BONUS:", "БОНУС:" );
    result += "\n" + bonus;
    if( prime_visual_perk( perk ) && !drawback.empty() ) {
        result += "\n\n";
        result += tr( "DRAWBACK:", "ШТРАФ:" );
        result += "\n" + drawback;
    }
    result += "\n\n";
    result += tr( "REQUIRES:", "ТРЕБУЕТ:" );
    result += "\n" + requires_text;
    return result;
}

ncmm_ui_theme_v1 branch_ui_theme( branch_id branch )
{
    return { branch_theme_color( branch ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
             NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
             26, 42, nullptr, 0 };
}

ncmm_ui_theme_v1 integration_ui_theme( const std::string &mod_id )
{
    return { integration_theme_color( mod_id ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
             NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
             26, 42, nullptr, 0 };
}

std::string compact_tree_badge( const perk_def &perk, bool unlocked,
                                int64_t perk_points, int64_t major_points,
                                bool mod_branch )
{
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );
    const bool maxed = rank >= max_rank;
    const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;

    std::string result;
    if( mod_branch ) {
        result = "MOD";
    }
    const std::string chevrons = rank_chevrons( perk );
    if( !chevrons.empty() ) {
        if( !result.empty() ) result += " | ";
        result += chevrons;
    }
    if( !result.empty() ) result += " | ";
    if( maxed ) {
        result += max_rank > 1 ? tr( "MAX ", "МАКС " ) + std::to_string( rank ) + "/" +
                  std::to_string( max_rank ) : tr( "OWN", "КУП" );
    } else if( rank > 0 ) {
        result += "R" + std::to_string( rank ) + "/" + std::to_string( max_rank );
    } else if( !unlocked ) {
        result += tr( "LOCK", "ЗАКР" );
    } else if( !enough ) {
        result += tr( "NO PTS", "НЕТ ОЧК" );
    } else {
        result += tr( "READY", "ГОТОВ" );
    }
    return result;
}
bool perk_world_available( const perk_def &perk )
{
    const integration_id integration = perk_integration( perk );
    if( integration == integration_id::none ) {
        return true;
    }
    return active_world_mod( integration_mod_id( integration ) );
}

int specialization_root_slot( const char *raw_id )
{
    const std::string_view id = raw_id ? std::string_view( raw_id ) : std::string_view();
    if( id == "spc_c_juggernaut" || id == "spc_s_nomad" || id == "spc_m_sprinter" ||
        id == "spc_f_systems" || id == "spc_g_prospector" || id == "spc_a_specialist" ) return 1;
    if( id == "spc_c_duelist" || id == "spc_s_medic" || id == "spc_m_ghost" ||
        id == "spc_f_improviser" || id == "spc_g_courier" || id == "spc_a_polymath" ) return 2;
    if( id == "spc_c_tactician" || id == "spc_s_quartermaster" || id == "spc_m_pathfinder" ||
        id == "spc_f_researcher" || id == "spc_g_investigator" || id == "spc_a_selfteacher" ) return 3;
    return 0;
}

int specialization_slot( const perk_def &perk )
{
    const int direct = specialization_root_slot( perk.id );
    if( direct > 0 ) {
        return direct;
    }
    return specialization_root_slot( perk.prereq1 );
}

bool specialization_perk( const perk_def &perk )
{
    return specialization_slot( perk ) > 0;
}

bool specialization_root( const perk_def &perk )
{
    return specialization_root_slot( perk.id ) > 0;
}

std::string specialization_state_key( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return "spec_combat";
        case branch_id::survival: return "spec_survival";
        case branch_id::mobility: return "spec_mobility";
        case branch_id::crafting: return "spec_crafting";
        case branch_id::scavenging: return "spec_scavenging";
        case branch_id::mastery: return "spec_mastery";
    }
    return "spec_unknown";
}

bool specialization_allowed( const perk_def &perk )
{
    if( !specialization_perk( perk ) ) {
        return true;
    }
    const int slot = specialization_slot( perk );
    const int64_t selected_slot = get_state( specialization_state_key( perk.branch ), 0 );
    if( specialization_root( perk ) ) {
        return selected_slot == 0 || selected_slot == slot;
    }
    return selected_slot == slot;
}

int mod_prime_root_slot( const char *raw_id )
{
    const std::string_view id = raw_id ? std::string_view( raw_id ) : std::string_view();
    if( id == "mg_prime_arcanist" || id == "mom_prime_kinetic" || id == "xe_prime_analyst" ||
        id == "af_prime_smartgun" || id == "afp_prime_gunslinger" || id == "sec_prime_hunter" ||
        id == "secx_prime_architect" ) return 1;
    if( id == "mg_prime_channeler" || id == "mom_prime_overclock" || id == "xe_prime_resonant" ||
        id == "af_prime_systems" || id == "afp_prime_systems_specialist" || id == "sec_prime_bulwark" ||
        id == "secx_prime_predator" ) return 2;
    if( id == "mg_prime_warcaster" || id == "mom_prime_ascetic" || id == "xe_prime_riftwalker" ||
        id == "af_prime_phase" || id == "afp_prime_translocator" || id == "sec_prime_crimson" ||
        id == "secx_prime_vessel" ) return 3;
    return 0;
}

bool mod_prime_specialization_root( const perk_def &perk )
{
    return mod_prime_root_slot( perk.id ) > 0;
}

std::string mod_prime_state_key( const perk_def &perk )
{
    switch( perk_integration( perk ) ) {
        case integration_id::magiclysm: return "prime_magiclysm";
        case integration_id::mindovermatter: return "prime_mindovermatter";
        case integration_id::xedra_evolved: return "prime_xedra_evolved";
        case integration_id::aftershock_exoplanet: return "prime_aftershock_exoplanet";
        case integration_id::aftershock_prime: return "prime_aftershock_prime";
        case integration_id::secronom: return "prime_secronom";
        case integration_id::secronom_plus: return "prime_secronom_plus";
        default: return "prime_unknown";
    }
}

bool mod_prime_specialization_allowed( const perk_def &perk )
{
    const int slot = mod_prime_root_slot( perk.id );
    if( slot <= 0 ) return true;
    const int64_t selected = get_state( mod_prime_state_key( perk ), 0 );
    return selected == 0 || selected == slot;
}

bool exclusive_specialization_perk( const perk_def &perk )
{
    return specialization_perk( perk ) || mod_prime_specialization_root( perk );
}

bool exclusive_specialization_root( const perk_def &perk )
{
    return specialization_root( perk ) || mod_prime_specialization_root( perk );
}

int exclusive_specialization_slot( const perk_def &perk )
{
    const int mod_slot = mod_prime_root_slot( perk.id );
    return mod_slot > 0 ? mod_slot : specialization_slot( perk );
}

std::string exclusive_specialization_state_key( const perk_def &perk )
{
    return mod_prime_specialization_root( perk ) ?
           mod_prime_state_key( perk ) : specialization_state_key( perk.branch );
}

bool exclusive_specialization_allowed( const perk_def &perk )
{
    if( mod_prime_specialization_root( perk ) ) return mod_prime_specialization_allowed( perk );
    return specialization_allowed( perk );
}
bool owned( const perk_def &perk )
{
    return perk_world_available( perk ) && perk_rank( perk ) > 0;
}

const perk_def *find_perk( const char *id )
{
    if( id == nullptr || *id == '\0' ) {
        return nullptr;
    }
    static const std::map<std::string_view, const perk_def *> index = []() {
        std::map<std::string_view, const perk_def *> result;
        for( const perk_def &perk : perks ) {
            result.emplace( std::string_view( perk.id ), &perk );
        }
        return result;
    }();
    const auto it = index.find( std::string_view( id ) );
    if( it == index.end() || !perk_world_available( *it->second ) ) {
        return nullptr;
    }
    return it->second;
}

int64_t xp_to_next( int64_t level )
{
    level = std::max<int64_t>( 1, level );
    if( level <= 30 ) {
        return 30 + ( level - 1 ) * 15;
    }

    // Keep the proven early curve intact, then transition to a slow quadratic tail.
    // There is no gameplay level cap; the numeric saturation only prevents int64 overflow.
    const long double d = static_cast<long double>( level - 30 );
    const long double required = 465.0L + 12.0L * d + 0.20L * d * d;
    const long double safe_max = static_cast<long double>(
                                     std::numeric_limits<int64_t>::max() / 4 );
    if( required >= safe_max ) {
        return std::numeric_limits<int64_t>::max() / 4;
    }
    return std::max<int64_t>( 1, static_cast<int64_t>( std::llround( required ) ) );
}

const char *branch_name_en( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return "Combat";
        case branch_id::survival: return "Survival";
        case branch_id::mobility: return "Mobility";
        case branch_id::crafting: return "Crafting";
        case branch_id::scavenging: return "Scavenging";
        case branch_id::mastery: return "Mastery";
    }
    return "Unknown";
}

const char *branch_name_ru( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return "Бой";
        case branch_id::survival: return "Выживание";
        case branch_id::mobility: return "Мобильность";
        case branch_id::crafting: return "Крафт";
        case branch_id::scavenging: return "Добыча";
        case branch_id::mastery: return "Мастерство";
    }
    return "Неизвестно";
}

std::string branch_name( branch_id branch )
{
    return russian() ? branch_name_ru( branch ) : branch_name_en( branch );
}

std::string branch_focus( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat:
            return tr( "Power / accuracy / battle tempo", "Сила / точность / темп боя" );
        case branch_id::survival:
            return tr( "Stamina / healing / carrying", "Выносливость / лечение / груз" );
        case branch_id::mobility:
            return tr( "Speed / movement / endurance", "Скорость / движение / резерв" );
        case branch_id::crafting:
            return tr( "Crafting / study / intelligence", "Крафт / обучение / интеллект" );
        case branch_id::scavenging:
            return tr( "Perception / load / long routes", "Восприятие / груз / маршруты" );
        case branch_id::mastery:
            return tr( "XP / attributes / cross-training", "Опыт / характеристики / разносторонность" );
    }
    return {};
}
const std::array<branch_id, 6> all_branches = {
    branch_id::combat, branch_id::survival, branch_id::mobility,
    branch_id::crafting, branch_id::scavenging, branch_id::mastery
};

const char *branch_tag( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return "combat";
        case branch_id::survival: return "survival";
        case branch_id::mobility: return "mobility";
        case branch_id::crafting: return "crafting";
        case branch_id::scavenging: return "scavenging";
        case branch_id::mastery: return "mastery";
    }
    return "unknown";
}

std::string branch_state_key( branch_id branch, const char *suffix )
{
    return std::string( "b_" ) + branch_tag( branch ) + "_" + suffix;
}

int64_t branch_xp_to_next( int64_t level )
{
    level = std::max<int64_t>( 1, level );
    const long double required = 20.0L + static_cast<long double>( level - 1 ) * 10.0L;
    return required >= 1000000.0L ? 1000000 : static_cast<int64_t>( required );
}

int64_t branch_level( branch_id branch )
{
    return std::max<int64_t>( 1, get_state( branch_state_key( branch, "level" ), 1 ) );
}

int64_t branch_xp( branch_id branch )
{
    return std::max<int64_t>( 0, get_state( branch_state_key( branch, "xp" ), 0 ) );
}

std::string branch_xp_source( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat:
            return tr( "Kills; dangerous targets are worth more.",
                       "Убийства; опасные цели дают больше опыта." );
        case branch_id::survival:
            return tr( "Real damage healed.",
                       "Фактически восстановленное здоровье." );
        case branch_id::mobility:
            return tr( "Active movement over the map.",
                       "Активное перемещение по карте." );
        case branch_id::crafting:
            return tr( "Successfully completed crafting activities.",
                       "Успешно завершённый крафт." );
        case branch_id::scavenging:
            return tr( "Entering overmap tiles while exploring.",
                       "Переходы между клетками глобальной карты." );
        case branch_id::mastery:
            return tr( "Skill level-ups plus 10% of other branch XP.",
                       "Рост навыков плюс 10% опыта остальных веток." );
    }
    return {};
}
int64_t branch_fatigue( branch_id branch )
{
    return std::max<int64_t>( 0,
                              std::min<int64_t>( 1000,
                                  get_state( branch_state_key( branch, "fatigue" ), 0 ) ) );
}

int branch_xp_efficiency_pct( branch_id branch )
{
    const int64_t fatigue = branch_fatigue( branch );
    if( fatigue < 70 ) {
        return 100;
    }
    if( fatigue < 160 ) {
        return 80;
    }
    if( fatigue < 280 ) {
        return 60;
    }
    if( fatigue < 430 ) {
        return 40;
    }
    if( fatigue < 620 ) {
        return 20;
    }
    return 10;
}

std::string branch_efficiency_text( branch_id branch )
{
    return tr( "Experience gain ", "Получение опыта " ) +
           std::to_string( branch_xp_efficiency_pct( branch ) ) + "%";
}

void decay_branch_fatigue()
{
    for( branch_id branch : all_branches ) {
        const int64_t fatigue = branch_fatigue( branch );
        if( fatigue <= 0 ) {
            continue;
        }
        const int64_t decay = branch == branch_id::mastery ? 20 : 14;
        set_state( branch_state_key( branch, "fatigue" ),
                   std::max<int64_t>( 0, fatigue - decay ) );
    }
}

int64_t scale_configured_xp( branch_id branch, int64_t adjusted )
{
    if( adjusted <= 0 ) {
        return 0;
    }
    const int64_t rate = progression_xp_rate_pct();
    const std::string key = branch_state_key( branch, "rate_fraction" );
    int64_t fraction = std::max<int64_t>( 0, get_state( key, 0 ) ) % 100;
    if( adjusted > ( std::numeric_limits<int64_t>::max() - fraction ) /
        std::max<int64_t>( 1, rate ) ) {
        adjusted = ( std::numeric_limits<int64_t>::max() - fraction ) /
                   std::max<int64_t>( 1, rate );
    }
    const int64_t scaled = adjusted * rate + fraction;
    set_state( key, scaled % 100 );
    return scaled / 100;
}

int64_t anti_farm_adjust( branch_id branch, int64_t raw )
{
    if( raw <= 0 ) {
        const std::string streak_key = branch_state_key( branch, "streak" );
        if( get_state( streak_key, 0 ) != 0 ) {
            set_state( streak_key, 0 );
        }
        return 0;
    }

    int64_t streak = std::max<int64_t>( 0,
        get_state( branch_state_key( branch, "streak" ), 0 ) );
    streak = std::min<int64_t>( 12, streak + 1 );
    set_state( branch_state_key( branch, "streak" ), streak );

    const int efficiency = branch_xp_efficiency_pct( branch );
    int64_t adjusted = raw * efficiency / 100;
    if( adjusted == 0 && raw >= 5 && efficiency >= 20 ) {
        adjusted = 1;
    }

    int64_t fatigue_gain = branch == branch_id::mastery ?
                           raw * 3 : raw * 8;
    fatigue_gain += std::max<int64_t>( 0, streak - 2 ) * 6;
    fatigue_gain = std::min<int64_t>( 180, fatigue_gain );

    set_state( branch_state_key( branch, "fatigue" ),
               std::min<int64_t>( 1000, branch_fatigue( branch ) + fatigue_gain ) );
    return scale_configured_xp( branch, adjusted );
}

int branch_xp_balance_pct( branch_id branch )
{
    switch( branch ) {
        case branch_id::survival: return 200;
        case branch_id::mobility: return 115;
        case branch_id::scavenging: return 80;
        default: return 100;
    }
}

int64_t apply_branch_xp_balance( branch_id branch, int64_t raw )
{
    if( raw <= 0 ) {
        return 0;
    }
    const int64_t rate = branch_xp_balance_pct( branch );
    if( rate == 100 ) {
        return raw;
    }
    const std::string key = branch_state_key( branch, "balance_fraction" );
    int64_t fraction = std::max<int64_t>( 0, get_state( key, 0 ) ) % 100;
    if( raw > ( std::numeric_limits<int64_t>::max() - fraction ) /
        std::max<int64_t>( 1, rate ) ) {
        raw = ( std::numeric_limits<int64_t>::max() - fraction ) /
              std::max<int64_t>( 1, rate );
    }
    const int64_t scaled = raw * rate + fraction;
    set_state( key, scaled % 100 );
    return scaled / 100;
}

int branch_owned_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !integration_perk( perk ) &&
            perk_world_available( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}

int branch_total_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !integration_perk( perk ) &&
            perk_world_available( perk ) ) {
            ++result;
        }
    }
    return result;
}

int visible_perk_count()
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk_world_available( perk ) ) {
            ++result;
        }
    }
    return result;
}

int owned_count( currency_id currency )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.currency == currency && perk_world_available( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}

std::string format_number( double value )
{
    std::ostringstream out;
    const double rounded = std::round( value );
    if( std::abs( value - rounded ) < 0.0001 ) {
        out << static_cast<long long>( rounded );
    } else {
        out << std::fixed << std::setprecision( 2 ) << value;
    }
    return out.str();
}

std::string effect_label( const std::string &id )
{
    if( id == "str_flat" ) return tr( "STR", "СИЛ" );
    if( id == "dex_flat" ) return tr( "DEX", "ЛОВ" );
    if( id == "per_flat" ) return tr( "PER", "ВОС" );
    if( id == "int_flat" ) return tr( "INT", "ИНТ" );
    if( id == "speed_pct" ) return tr( "Speed %", "Скорость %" );
    if( id == "move_cost_pct" ) return tr( "Move cost %", "Стоимость движения %" );
    if( id == "stamina_max_pct" ) return tr( "Max stamina %", "Макс. выносливость %" );
    if( id == "carry_weight_pct" ) return tr( "Carry %", "Грузоподъёмность %" );
    if( id == "dodge_flat" ) return tr( "Dodge", "Уклонение" );
    if( id == "melee_hit_flat" ) return tr( "Melee hit", "Точность ближнего боя" );
    if( id == "healing_pct" ) return tr( "Healing %", "Лечение %" );
    if( id == "read_speed_pct" ) return tr( "Reading %", "Чтение %" );
    if( id == "craft_speed_pct" ) return tr( "Crafting %", "Крафт %" );
    if( id == "mg_spellcraft_flat" ) return "Magiclysm Spellcraft";
    if( id == "mg_mana_max_pct" ) return tr( "Maximum mana %", "Максимум маны %" );
    if( id == "mg_mana_regen_pct" ) return tr( "Mana regeneration %", "Регенерация маны %" );
    if( id == "mg_melee_mana_vamp_pct" ) return tr( "Melee mana vampirism %", "Вампиризм маны в ближнем бою %" );
    if( id == "mg_virtual_hand_count" ) return tr( "Virtual mana hands", "Виртуальные руки маны" );
    if( id == "mom_metaphysics_flat" ) return "MoM channeling Metaphysics";
    if( id == "xe_deduction_flat" ) return "Xedra Deduction";
    if( id == "xe_mana_max_pct" ) return tr( "Maximum mana %", "Максимум маны %" );
    if( id == "xe_mana_regen_pct" ) return tr( "Mana regeneration %", "Регенерация маны %" );
    if( id == "xe_gramarye_flat" ) return "Xedra Gramarye";
    if( id == "af_smartgun_flat" ) return "Aftershock Smartgun";
    if( id == "af_metaphysics_flat" ) return "Aftershock Exoplanet channeling Metaphysics";
    if( id == "afp_smartgun_flat" ) return "Aftershock Prime Smartgun";
    if( id == "secx_flesh_craft_flat" ) return "Secronom+ Flesh Weaving";
    if( id == "secx_flesh_combat_flat" ) return "Secronom+ Bio-organic Weapons";
    if( id == "sec_damage_pct" ) return tr( "Damage vs Secronom %", "Урон по Secronom %" );
    if( id == "sec_resist_pct" ) return tr( "Resistance vs Secronom %", "Защита от Secronom %" );
    if( id == "sec_elite_damage_pct" ) return tr( "Damage vs Secronom elites %", "Урон по элите Secronom %" );
    if( id == "sec_elite_resist_pct" ) return tr( "Resistance vs Secronom elites %", "Защита от элиты Secronom %" );
    if( id == "sec_crimson_damage_pct" ) return tr( "Damage vs Crimson Horrors %", "Урон по Crimson Horrors %" );
    if( id == "sec_crimson_resist_pct" ) return tr( "Resistance vs Crimson Horrors %", "Защита от Crimson Horrors %" );
    if( id.find( "spell_cost_pct" ) != std::string::npos ) return tr( "Power cost %", "Стоимость силы %" );
    if( id.find( "cast_time_pct" ) != std::string::npos ) return tr( "Cast time %", "Время применения %" );
    if( id.find( "fail_pct" ) != std::string::npos ) return tr( "Failure %", "Провал %" );
    if( id.find( "spell_xp_pct" ) != std::string::npos ) return tr( "Spell/power XP %", "Опыт сил %" );
    if( id.find( "spell_power_pct" ) != std::string::npos ) return tr( "Spell/power potency %", "Мощность сил %" );
    if( id.find( "range_pct" ) != std::string::npos ) return tr( "Range %", "Дальность %" );
    if( id.find( "aoe_pct" ) != std::string::npos ) return tr( "Area %", "Площадь %" );
    if( id.find( "duration_pct" ) != std::string::npos ) return tr( "Duration %", "Длительность %" );
    if( id == "sp_melee_crit_chance_pct" ) return tr( "Melee critical chance", "Шанс крита в ближнем бою" );
    if( id == "sp_melee_crit_damage_pct" ) return tr( "Melee critical damage %", "Критический урон ближнего боя %" );
    if( id == "sp_ranged_crit_damage_pct" ) return tr( "Projectile critical damage %", "Критический урон снарядов %" );
    if( id == "sp_damage_avoid_pct" ) return tr( "Full damage avoidance %", "Полное избегание урона %" );
    if( id == "sp_damage_taken_pct" ) return tr( "Incoming damage reduction %", "Снижение входящего урона %" );
    if( id == "sp_dodge_attempts_bonus" ) return tr( "Dodge attempts", "Попытки уклонения" );
    if( id == "sp_free_dodge_attempts_bonus" ) return tr( "Free dodge attempts", "Бесплатные уклонения" );
    if( id == "sp_block_attempts_bonus" ) return tr( "Block attempts", "Попытки блока" );    if( id == "sp_riposte_chance_pct" ) return tr( "Riposte chance %", "Шанс рипоста %" );
    if( id == "sp_riposte_refund_pct" ) return tr( "Riposte move refund %", "Возврат стоимости рипоста %" );
    if( id == "sp_on_dodge_moves" ) return tr( "Moves restored after dodge", "Возврат ед. хода после уклонения" );
    if( id == "sp_on_dodge_stamina_pct" ) return tr( "Stamina restored after dodge %", "Восстановление выносливости после уклонения %" );
    if( id == "sp_on_crit_moves" ) return tr( "Moves restored after melee critical", "Возврат ед. хода после критического удара" );
    if( id == "sp_on_crit_stamina_pct" ) return tr( "Stamina restored after melee critical %", "Восстановление выносливости после критического удара %" );
    if( id == "sp_execute_threshold_pct" ) return tr( "Finisher health threshold %", "Порог здоровья для добивания %" );
    if( id == "sp_execute_damage_pct" ) return tr( "Finisher damage %", "Урон при добивании %" );
    if( id == "sp_damage_dealt_pct" ) return tr( "Outgoing damage %", "Исходящий урон %" );
    if( id == "sp_on_kill_moves" ) return tr( "Moves restored after kill", "Возврат ед. хода после убийства" );
    if( id == "sp_on_kill_stamina_pct" ) return tr( "Stamina restored after kill %", "Восстановление выносливости после убийства %" );
    if( id == "sp_craft_success_roll_flat" ) return tr( "Crafting success bonus", "Бонус к успеху крафта" );
    if( id == "sp_craft_failure_save_pct" ) return tr( "Craft failure prevention %", "Предотвращение ошибки крафта %" );
    if( id == "sp_craft_component_loss_reduction_pct" ) return tr( "Craft component protection %", "Защита компонентов крафта %" );
    if( id == "sp_craft_progress_loss_reduction_pct" ) return tr( "Reduced crafting progress loss %", "Снижение потери прогресса при крафте %" );
    if( id == "sp_lockpick_roll_flat" ) return tr( "Lockpicking bonus", "Бонус к взлому" );
    if( id == "sp_lockpick_time_reduction_pct" ) return tr( "Lockpicking time reduction %", "Сокращение времени взлома %" );
    if( id == "sp_lockpick_tool_protection_pct" ) return tr( "Lockpick protection %", "Сохранность отмычки %" );
    if( id == "sp_lockpick_alarm_avoid_pct" ) return tr( "Alarm bypass %", "Обход сигнализации %" );
    if( id == "sp_trap_detection_flat" ) return tr( "Trap detection bonus", "Бонус к обнаружению ловушек" );    return id;
}

bool effect_is_percentage( const std::string &id )
{
    return id.size() >= 4 && id.compare( id.size() - 4, 4, "_pct" ) == 0;
}

std::string effect_display_label( const std::string &id )
{
    std::string label = effect_label( id );
    if( effect_is_percentage( id ) && label.size() >= 2 &&
        label.compare( label.size() - 2, 2, " %" ) == 0 ) {
        label.resize( label.size() - 2 );
    }
    return label;
}

std::string effect_value_text( const std::string &id, double value )
{
    const std::string sign = value > 0.0 ? "+" : "";
    return sign + format_number( value ) + ( effect_is_percentage( id ) ? "%" : "" );
}

std::string effect_delta_text( const std::string &id, double delta )
{
    const std::string sign = delta > 0.0 ? "+" : "";
    std::string result = sign + format_number( delta );
    if( effect_is_percentage( id ) ) {
        result += tr( " pp", " п.п." );
    }
    return result;
}

double ranked_effect_multiplier( const perk_def &perk, int rank )
{
    double multiplier = perk_rank_multiplier_for( perk, rank );
    if( effective_kind( perk ) == perk_kind::stat ) {
        multiplier *= static_cast<double>( progression_stat_power_pct() ) / 100.0;
    }
    return multiplier;
}

std::string ranked_effect_summary( const perk_def &perk, int rank )
{
    if( rank <= 0 ) {
        return {};
    }

    const double multiplier = ranked_effect_multiplier( perk, rank );
    std::vector<std::string> parts;
    for( int i = 0; i < perk.effect_count; ++i ) {
        if( perk.effects[i].id == nullptr ) {
            continue;
        }
        const std::string id = perk.effects[i].id;
        const double value = perk.effects[i].value * multiplier;
        parts.push_back( effect_display_label( id ) + ": " + effect_value_text( id, value ) );
    }
    if( perk.xp_bonus_pct != 0 ) {
        const int value = static_cast<int>(
                              std::llround( static_cast<double>( perk.xp_bonus_pct ) * multiplier ) );
        const std::string sign = value > 0 ? "+" : "";
        parts.push_back( tr( "Survivor XP: ", "Опыт Survivor: " ) + sign +
                         std::to_string( value ) + "%" );
    }

    std::string result;
    for( size_t i = 0; i < parts.size(); ++i ) {
        if( i != 0 ) {
            result += ", ";
        }
        result += parts[i];
    }
    return result;
}

std::string ranked_effect_next_summary( const perk_def &perk, int current_rank, int next_rank )
{
    const double current_multiplier = ranked_effect_multiplier( perk, current_rank );
    const double next_multiplier = ranked_effect_multiplier( perk, next_rank );
    std::vector<std::string> parts;
    for( int i = 0; i < perk.effect_count; ++i ) {
        if( perk.effects[i].id == nullptr ) {
            continue;
        }
        const std::string id = perk.effects[i].id;
        const double current_value = perk.effects[i].value * current_multiplier;
        const double next_value = perk.effects[i].value * next_multiplier;
        const double delta = next_value - current_value;
        parts.push_back( effect_display_label( id ) + ": " + effect_value_text( id, next_value ) +
                         " (" + effect_delta_text( id, delta ) + ")" );
    }
    if( perk.xp_bonus_pct != 0 ) {
        const int current_value = static_cast<int>( std::llround(
                                      static_cast<double>( perk.xp_bonus_pct ) * current_multiplier ) );
        const int next_value = static_cast<int>( std::llround(
                                   static_cast<double>( perk.xp_bonus_pct ) * next_multiplier ) );
        const int delta = next_value - current_value;
        const std::string value_sign = next_value > 0 ? "+" : "";
        const std::string delta_sign = delta > 0 ? "+" : "";
        parts.push_back( tr( "Survivor XP: ", "Опыт Survivor: " ) + value_sign +
                         std::to_string( next_value ) + "% (" + delta_sign +
                         std::to_string( delta ) + tr( " pp)", " п.п.)" ) );
    }

    std::string result;
    for( size_t i = 0; i < parts.size(); ++i ) {
        if( i != 0 ) {
            result += ", ";
        }
        result += parts[i];
    }
    return result;
}

std::string perk_description( const perk_def &perk )
{
    std::string result = russian() ? perk.desc_ru : perk.desc_en;
    const int max_rank = perk_max_rank( perk );
    if( max_rank <= 1 ) {
        return result;
    }

    const int rank = perk_rank( perk );
    result += "\n" + tr( "Rank ", "Ранг " ) + std::to_string( rank ) + "/" +
              std::to_string( max_rank );

    if( rank > 0 ) {
        result += "\n" + tr( "Current: ", "Сейчас: " ) +
                  ranked_effect_summary( perk, rank );
    }
    if( rank < max_rank ) {
        result += "\n" + tr( "Next rank ", "Следующий ранг " ) + rank_roman( rank + 1 ) + ": " +
                  ranked_effect_next_summary( perk, rank, rank + 1 );
    } else {
        result += "\n" + tr( "Maximum rank reached.", "Максимальный ранг." );
    }
    return result;
}

struct calculated_effects {
    std::map<std::string, double> modifiers;
    int xp_bonus_pct = 0;
    int active_branches = 0;
    int major_owned = 0;
    std::array<double, 6> branch_amp = {{ 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 }};
    double global_amp = 1.0;
};

int branch_index( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return 0;
        case branch_id::survival: return 1;
        case branch_id::mobility: return 2;
        case branch_id::crafting: return 3;
        case branch_id::scavenging: return 4;
        case branch_id::mastery: return 5;
    }
    return 0;
}

calculated_effects calculate_owned_effects()
{
    calculated_effects result;
    std::array<bool, 6> branch_active = {{ false, false, false, false, false, false }};

    // Pass 1: one ownership lookup per perk.  The old implementation scanned the
    // full catalog once per branch, then again for majors and amplifier effects.
    for( const perk_def &perk : perks ) {
        if( !perk_world_available( perk ) ) {
            continue;
        }
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }
        if( !integration_perk( perk ) ) {
            branch_active[branch_index( perk.branch )] = true;
        }
        if( perk.currency == currency_id::major ) {
            ++result.major_owned;
        }
        if( effective_kind( perk ) == perk_kind::effect ) {
            const double rank_scale = perk_rank_multiplier_for( perk, rank );
            result.branch_amp[branch_index( perk.branch )] +=
                perk.branch_amp_pct * rank_scale / 100.0;
            result.global_amp += perk.global_amp_pct * rank_scale / 100.0;
        }
    }
    for( bool active : branch_active ) {
        if( active ) {
            ++result.active_branches;
        }
    }

    // Pass 2: resolve scaling after active-branch and major totals are known.
    for( const perk_def &perk : perks ) {
        if( !perk_world_available( perk ) ) {
            continue;
        }
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }

        double scale = 1.0;
        if( perk.scaling == perk_scaling::per_active_branch ) {
            scale = static_cast<double>( result.active_branches );
        } else if( perk.scaling == perk_scaling::per_owned_major ) {
            // Long-running characters keep progressing, but major-scaling perks
            // deliberately stop at twelve owned major perks.
            scale = static_cast<double>( std::min( result.major_owned, 12 ) );
        }

        scale *= perk_rank_multiplier_for( perk, rank );

        if( effective_kind( perk ) == perk_kind::stat ) {
            scale *= result.global_amp * result.branch_amp[branch_index( perk.branch )];
            scale *= static_cast<double>( progression_stat_power_pct() ) / 100.0;
        }

        result.xp_bonus_pct += static_cast<int>( std::llround( perk.xp_bonus_pct * scale ) );
        for( int i = 0; i < perk.effect_count; ++i ) {
            if( perk.effects[i].id != nullptr ) {
                result.modifiers[perk.effects[i].id] += perk.effects[i].value * scale;
            }
        }
    }

    auto owns_id = []( const char *id ) {
        const perk_def *perk = find_perk( id );
        return perk != nullptr && owned( *perk );
    };
    int64_t momentum_stack_cap = owns_id( "cr_relentless_momentum" ) ? 5 : 3;
    if( owns_id( "ar_momentum_engine" ) ) momentum_stack_cap += 2;
    const int64_t momentum_turn_cap = owns_id( "cr_relentless_momentum" ) ? 20 : 12;
    const int64_t momentum_stacks = std::min<int64_t>( momentum_stack_cap,
                                      std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) ) );
    const int64_t momentum_turns = std::min<int64_t>( momentum_turn_cap,
                                     std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) ) );
    if( momentum_stacks > 0 && momentum_turns > 0 && owns_id( "cr_predator_momentum" ) ) {
        double damage_per_stack = 3.0;
        double speed_per_stack = 1.0;
        if( owns_id( "ar_momentum_engine" ) ) {
            damage_per_stack += 1.0;
            speed_per_stack += 1.0;
        }
        result.modifiers["sp_damage_dealt_pct"] += momentum_stacks * damage_per_stack;
        result.modifiers["speed_pct"] += momentum_stacks * speed_per_stack;
    }    // Prime drawbacks may intentionally reduce Survivor XP.  Keep the
    // effective multiplier non-negative while preserving declared penalties.
    result.xp_bonus_pct = std::max( -100, std::min( 5000, result.xp_bonus_pct ) );
    return result;
}

std::map<std::string, double> owned_effect_totals()
{
    return calculate_owned_effects().modifiers;
}

int64_t perk_progression_level( const perk_def &perk )
{
    if( integration_perk( perk ) ) {
        return std::max<int64_t>( 1, get_state( "level", 1 ) );
    }
    return branch_level( perk.branch );
}
bool prerequisites_met( const perk_def &perk )
{
    if( !perk_world_available( perk ) || !exclusive_specialization_allowed( perk ) ) {
        return false;
    }
    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) continue;
        const perk_def *required = find_perk( id );
        if( required == nullptr || !owned( *required ) ) return false;
    }
    return true;
}

std::string perk_lock_reason( const perk_def &perk )
{
    const int64_t level = perk_progression_level( perk );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( !perk_world_available( perk ) ) {
        const std::string mod_name = integration_mod_name( perk );
        return mod_name.empty() ?
               tr( "Required mod is not active.", "Требуемый мод не активен." ) :
               tr( "Requires mod: ", "Нужен мод: " ) + mod_name;
    }
    if( rank >= max_rank ) {
        return tr( "Maximum rank reached.", "Достигнут максимальный ранг." );
    }

    std::vector<std::string> reasons;
    if( level < perk.required_level ) {
        reasons.push_back(
            ( integration_perk( perk ) ? tr( "Survivor level ", "Уровень Survivor " ) :
                                        tr( "Branch level ", "Уровень ветки " ) ) +
            std::to_string( level ) + "/" + std::to_string( perk.required_level ) );
    }

    if( exclusive_specialization_perk( perk ) && !exclusive_specialization_allowed( perk ) ) {
        reasons.push_back( tr( "Another Prime path is already selected; full respec is required.",
                               "Уже выбран другой Прайм-путь; для смены нужен полный сброс." ) );
    }

    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) {
            continue;
        }
        const perk_def *required = find_perk( id );
        if( required == nullptr ) {
            reasons.push_back( tr( "Required perk is unavailable in this world.",
                                   "Требуемый перк недоступен в этом мире." ) );
        } else if( !owned( *required ) ) {
            reasons.push_back( tr( "Requires: ", "Нужен перк: " ) + perk_display_name( *required ) );
        }
    }

    if( reasons.empty() ) {
        const int64_t points = perk.currency == currency_id::perk ?
                               get_state( "perk_points", 0 ) : get_state( "major_points", 0 );
        if( points <= 0 ) {
            reasons.push_back( perk.currency == currency_id::perk ?
                               tr( "Need 1 perk point; available: 0.", "Нужно 1 очко перка; доступно: 0." ) :
                               tr( "Need 1 major point; available: 0.", "Нужно 1 большое очко; доступно: 0." ) );
        }
    }

    if( reasons.empty() ) {
        return rank > 0 ? tr( "Ready to upgrade.", "Можно улучшить." ) :
                          tr( "Ready to purchase.", "Можно купить." );
    }

    std::string result;
    for( size_t i = 0; i < reasons.size(); ++i ) {
        if( i > 0 ) result += "\n";
        result += "- " + reasons[i];
    }
    return result;
}

std::string branch_next_unlock_text( branch_id branch )
{
    const int64_t current_level = branch_level( branch );
    const perk_def *best = nullptr;
    int best_level = 1000000;
    for( const perk_def &perk : perks ) {
        if( perk.branch != branch || integration_perk( perk ) || !perk_world_available( perk ) ||
            perk_maxed( perk ) || perk.required_level <= current_level ) {
            continue;
        }
        if( perk.required_level < best_level ) {
            best_level = perk.required_level;
            best = &perk;
        }
    }
    if( best == nullptr ) {
        return tr( "No later level-gated perk.", "Нет следующего перка по уровню." );
    }
    return perk_display_name( *best ) + " — " +
           tr( "branch level ", "уровень ветки " ) + std::to_string( best_level );
}

int integration_ready_count( const std::string &mod_id )
{
    const int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( !integration_perk( perk ) || !perk_world_available( perk ) ||
            mod_id != integration_mod_id( perk ) || perk_maxed( perk ) ) {
            continue;
        }
        if( level >= perk.required_level && prerequisites_met( perk ) ) {
            ++result;
        }
    }
    return result;
}

std::string integration_next_unlock_text( const std::string &mod_id )
{
    const int64_t current_level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    const perk_def *best = nullptr;
    int best_level = 1000000;
    for( const perk_def &perk : perks ) {
        if( !integration_perk( perk ) || !perk_world_available( perk ) ||
            mod_id != integration_mod_id( perk ) || perk_maxed( perk ) ||
            perk.required_level <= current_level ) {
            continue;
        }
        if( perk.required_level < best_level ) {
            best_level = perk.required_level;
            best = &perk;
        }
    }
    if( best == nullptr ) {
        return tr( "No later Survivor-level unlock.", "Нет следующего открытия по уровню Survivor." );
    }
    return perk_display_name( *best ) + " — " +
           tr( "Survivor level ", "уровень Survivor " ) + std::to_string( best_level );
}
std::string prereq_text( const perk_def &perk )
{
    std::vector<std::string> names;
    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) {
            continue;
        }
        const perk_def *required = find_perk( id );
        if( required != nullptr ) {
            names.emplace_back( russian() ? required->name_ru : required->name_en );
        }
    }
    if( names.empty() ) {
        return tr( "none", "нет" );
    }
    if( names.size() == 1 ) {
        return names[0];
    }
    return names[0] + " + " + names[1];
}

void clear_runtime_modifiers()
{
    if( host && host->character_modifier_clear_module ) {
        host->character_modifier_clear_module( module_id );
    }
    current_xp_bonus_pct = 0;
}

void recalculate_effects()
{
    if( !character_available() || !host || !host->character_modifier_set ||
        !host->character_modifier_clear_module ) {
        clear_runtime_modifiers();
        effects_dirty = true;
        return;
    }

    const calculated_effects calculated = calculate_owned_effects();

    int64_t purchased_ranks = 0;
    for( const perk_def &perk : perks ) {
        purchased_ranks += std::max( 0, perk_rank( perk ) );
    }
    set_state( "respec_available", purchased_ranks > 0 ? 1 : 0 );

    host->character_modifier_clear_module( module_id );
    for( const auto &entry : calculated.modifiers ) {
        if( entry.second != 0.0 &&
            !host->character_modifier_set( module_id, entry.first.c_str(), entry.second ) ) {
            if( host->log ) {
                const std::string msg = "Survivor Progression: host rejected modifier " + entry.first;
                host->log( NCMM_LOG_WARN, msg.c_str() );
            }
        }
    }

    current_xp_bonus_pct = calculated.xp_bonus_pct;
    effects_dirty = false;
}

void migrate_state()
{
    if( !character_available() ) {
        return;
    }

    const int64_t schema = get_state( "schema", 0 );
    if( schema >= state_schema ) {
        return;
    }

    int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    set_state( "level", level );

    int64_t xp = std::max<int64_t>( 0, get_state( "xp", 0 ) );
    int64_t fraction = std::max<int64_t>( 0, get_state( "xp_fraction", 0 ) );
    if( fraction >= 100 ) {
        xp += fraction / 100;
        fraction %= 100;
    }
    set_state( "xp", xp );
    set_state( "xp_fraction", fraction );
    set_state( "perk_points", std::max<int64_t>( 0, get_state( "perk_points", 0 ) ) );
    set_state( "major_points", std::max<int64_t>( 0, get_state( "major_points", 0 ) ) );

    // Preserve the old 0.1.x Fast Learner purchase.
    if( get_state( "fast_learner", 0 ) != 0 ) {
        const perk_def *legacy = find_perk( "a_fast" );
        if( legacy != nullptr && !owned( *legacy ) ) {
            set_state( perk_key( *legacy ), 1 );
        }
    }

    // Base major points continue every five levels forever.
    const int64_t expected_major_awards = level / 5;
    int64_t major_awarded = std::max<int64_t>( 0, get_state( "major_awarded", 0 ) );
    int64_t major_points = get_state( "major_points", 0 );
    if( major_awarded < expected_major_awards ) {
        major_points += expected_major_awards - major_awarded;
        major_awarded = expected_major_awards;
        set_state( "major_points", major_points );
    }
    if( major_awarded > expected_major_awards ) {
        major_awarded = expected_major_awards;
    }
    set_state( "major_awarded", major_awarded );

    for( branch_id branch : all_branches ) {
        set_state( branch_state_key( branch, "level" ),
                   std::max<int64_t>( 1, get_state( branch_state_key( branch, "level" ), 1 ) ) );
        set_state( branch_state_key( branch, "xp" ),
                   std::max<int64_t>( 0, get_state( branch_state_key( branch, "xp" ), 0 ) ) );
        const int64_t selected_spec = get_state( specialization_state_key( branch ), 0 );
        set_state( specialization_state_key( branch ),
                   selected_spec >= 1 && selected_spec <= 3 ? selected_spec : 0 );
    }
    for( const char *key : {
             "metric_combat_kills", "metric_combat_kill_xp", "metric_survival_healing",
             "metric_mobility_steps", "metric_crafting_completed", "metric_scavenging_omt",
             "metric_mastery_skill_levels"
         } ) {
        if( get_state( key, std::numeric_limits<int64_t>::min() ) ==
            std::numeric_limits<int64_t>::min() ) {
            set_state( key, -1 );
        }
    }
    set_state( "survival_heal_remainder",
               std::max<int64_t>( 0, get_state( "survival_heal_remainder", 0 ) ) );
    set_state( "mobility_step_remainder",
               std::max<int64_t>( 0, get_state( "mobility_step_remainder", 0 ) ) );
    set_state( "mastery_share_fraction",
               std::max<int64_t>( 0, get_state( "mastery_share_fraction", 0 ) ) % 100 );

    for( branch_id branch : all_branches ) {
        set_state( branch_state_key( branch, "fatigue" ),
                   std::max<int64_t>( 0,
                       std::min<int64_t>( 1000,
                           get_state( branch_state_key( branch, "fatigue" ), 0 ) ) ) );
        set_state( branch_state_key( branch, "streak" ),
                   std::max<int64_t>( 0,
                       std::min<int64_t>( 12,
                           get_state( branch_state_key( branch, "streak" ), 0 ) ) ) );
    }

    for( const char *key : {
             "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
             "prime_aftershock_exoplanet", "prime_aftershock_prime",
             "prime_secronom", "prime_secronom_plus"
         } ) {
        const int64_t selected = get_state( key, 0 );
        set_state( key, selected >= 1 && selected <= 3 ? selected : 0 );
    }
    set_state( "schema", state_schema );
    effects_dirty = true;
}

std::string status_prefix( const perk_def &perk, int64_t level, int64_t perk_points, int64_t major_points )
{
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );
    const std::string chevrons = rank_chevrons( perk );
    const std::string rank_prefix = chevrons.empty() ? std::string() : "[" + chevrons + "] ";
    if( rank >= max_rank ) {
        return rank_prefix + "[✓] ";
    }
    if( level < perk.required_level ) {
        return rank_prefix + std::string( russian() ? "[УР " : "[L" ) +
               std::to_string( perk.required_level ) + "] ";
    }
    if( !prerequisites_met( perk ) ) {
        return rank_prefix + ( russian() ? "[ТРЕБ.] " : "[REQ] " );
    }
    const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;
    if( !enough ) {
        return rank_prefix + ( russian() ? "[НЕТ ОЧКОВ] " : "[NO POINTS] " );
    }
    return rank_prefix + ( perk.currency == currency_id::perk ? "[1P] " : "[1M] " );
}

std::string cost_text( const perk_def &perk )
{
    return perk.currency == currency_id::perk ?
           tr( "1 perk point", "1 очко перка" ) :
           tr( "1 major point", "1 большое очко" );
}

bool purchase_perk( const perk_def &perk )
{
    const int64_t level = perk_progression_level( perk );
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( !perk_world_available( perk ) ) {
        message( tr( "The required mod is not active.", "Требуемый мод не активен." ) );
        return false;
    }
    if( rank >= max_rank ) {
        message( tr( "This perk is already at maximum rank.", "Этот перк уже максимального ранга." ) );
        return false;
    }
    if( level < perk.required_level ) {
        message( integration_perk( perk ) ?
                 tr( "Your Survivor level is too low for this perk.", "Недостаточный уровень Survivor для этого перка." ) :
                 tr( "Your branch level is too low.", "Недостаточный уровень этой ветки." ) );
        return false;
    }
    if( !prerequisites_met( perk ) ) {
        if( exclusive_specialization_perk( perk ) && !exclusive_specialization_allowed( perk ) ) {
            message( tr( "Another Prime specialization is already committed here. Full respec is required to change it.",
                         "Здесь уже выбрана другая Прайм-специализация. Для смены нужен полный сброс." ) );
        } else {
            message( tr( "Prerequisites are not met.", "Не выполнены требования предыдущих перков." ) );
        }
        return false;
    }

    int chosen_slot = 0;
    std::string chosen_state;
    if( exclusive_specialization_root( perk ) ) {
        chosen_slot = exclusive_specialization_slot( perk );
        chosen_state = exclusive_specialization_state_key( perk );
        const int64_t existing = get_state( chosen_state, 0 );
        if( existing == 0 ) {
            std::string prompt = tr(
                "Choose this Prime specialization? Its bonus and drawback remain until a full respec, and the other two choices will be locked.\n",
                "Выбрать эту Прайм-специализацию? Её бонус и штраф останутся до полного сброса, а два других варианта будут закрыты.\n" );
            prompt += perk_display_name( perk ) + "\n" + perk_description( perk );
            std::string yes = tr( "Choose Prime", "Выбрать" );
            std::string no = tr( "Cancel", "Отмена" );
            const char *entries[] = { yes.c_str(), no.c_str() };
            const int choice = host->ui_choose ? host->ui_choose( prompt.c_str(), entries, 2 ) : -1;
            if( choice != 0 ) return false;
        }
    }

    if( perk.currency == currency_id::perk ) {
        if( perk_points <= 0 ) {
            message( tr( "Not enough perk points.", "Недостаточно очков перков." ) );
            return false;
        }
        set_state( "perk_points", perk_points - 1 );
    } else {
        if( major_points <= 0 ) {
            message( tr( "Not enough major points.", "Недостаточно больших очков." ) );
            return false;
        }
        set_state( "major_points", major_points - 1 );
    }

    if( chosen_slot > 0 ) set_state( chosen_state, chosen_slot );
    set_state( perk_key( perk ), rank + 1 );
    effects_dirty = true;
    recalculate_effects();

    std::string result = rank == 0 ? tr( "Perk purchased: ", "Куплен перк: " ) :
                                     tr( "Perk upgraded: ", "Перк улучшен: " );
    result += russian() ? perk.name_ru : perk.name_en;
    if( max_rank > 1 ) result += " " + std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
    message( result );
    return true;
}

constexpr const char *mana_hand_pair_slot_id = "mana_hands_34";

const char *mana_hand_slot_id( const perk_def &perk )
{
    if( std::string_view( perk.id ) == "mg_mana_hand_3" ) return "mana_hand_3";
    if( std::string_view( perk.id ) == "mg_mana_hand_4" ) return "mana_hand_4";
    return nullptr;
}

bool mana_hand_perk( const perk_def &perk )
{
    return mana_hand_slot_id( perk ) != nullptr;
}

std::string virtual_item_name_for_slot( const char *slot_id )
{
    if( slot_id == nullptr || host2 == nullptr || host2->virtual_item_name == nullptr ) {
        return {};
    }
    const char *raw = host2->virtual_item_name( module_id, slot_id );
    return raw != nullptr ? std::string( raw ) : std::string();
}

bool virtual_item_slot_occupied( const char *slot_id )
{
    return slot_id != nullptr && host2 != nullptr && host2->virtual_item_uid != nullptr &&
           host2->virtual_item_uid( module_id, slot_id ) > 0;
}

std::string mana_hand_item_name( const perk_def &perk )
{
    return virtual_item_name_for_slot( mana_hand_slot_id( perk ) );
}

std::string paired_mana_hand_item_name()
{
    return virtual_item_name_for_slot( mana_hand_pair_slot_id );
}

bool virtual_item_secondary_controls_available()
{
    return host2 != nullptr && host2->api_minor >= 2u &&
           host2->struct_size >= NCMM_HOST_API_V2_CORE_SIZE_2_2 &&
           host2->virtual_item_secondary_melee_enabled != nullptr &&
           host2->virtual_item_set_secondary_melee != nullptr;
}

bool virtual_item_secondary_enabled_for_slot( const char *slot_id )
{
    return virtual_item_secondary_controls_available() && slot_id != nullptr &&
           host2->virtual_item_secondary_melee_enabled( module_id, slot_id ) != 0;
}

bool virtual_item_set_secondary_for_slot( const char *slot_id, bool enabled )
{
    return virtual_item_secondary_controls_available() && slot_id != nullptr &&
           host2->virtual_item_set_secondary_melee(
               module_id, slot_id, enabled ? 1 : 0 ) != 0;
}

bool virtual_item_primary_controls_available()
{
    return host2 != nullptr && host2->api_minor >= 3u &&
           host2->struct_size >= NCMM_HOST_API_V2_CORE_SIZE_2_3 &&
           host2->virtual_item_primary_melee_enabled != nullptr &&
           host2->virtual_item_set_primary_melee != nullptr;
}

bool virtual_item_primary_enabled_for_slot( const char *slot_id )
{
    return virtual_item_primary_controls_available() && slot_id != nullptr &&
           host2->virtual_item_primary_melee_enabled( module_id, slot_id ) != 0;
}

bool virtual_item_set_primary_for_slot( const char *slot_id, bool enabled )
{
    return virtual_item_primary_controls_available() && slot_id != nullptr &&
           host2->virtual_item_set_primary_melee(
               module_id, slot_id, enabled ? 1 : 0 ) != 0;
}

void clear_mana_hand_slots()
{
    if( host2 == nullptr || host2->virtual_item_clear == nullptr ) {
        return;
    }
    host2->virtual_item_clear( module_id, "mana_hand_3" );
    host2->virtual_item_clear( module_id, "mana_hand_4" );
    host2->virtual_item_clear( module_id, mana_hand_pair_slot_id );
}

void show_perk_detail( const perk_def &perk )
{
    while( true ) {
        const int level = static_cast<int>( perk_progression_level( perk ) );
        const int rank = perk_rank( perk );
        const int max_rank = perk_max_rank( perk );
        const bool maxed = rank >= max_rank;
        const bool unlocked = level >= perk.required_level && prerequisites_met( perk );

        std::string title = perk_display_name( perk );
        title += "\n" + perk_description( perk );
        title += "\n" + tr( "Tier ", "Тир " ) + std::to_string( perk.tier );
        if( integration_perk( perk ) ) {
            title += " | " + tr( "Requires Survivor level ", "Нужен уровень Survivor " ) + std::to_string( perk.required_level );
        } else {
            title += " | " + tr( "Requires branch level ", "Нужен уровень ветки " ) + std::to_string( perk.required_level );
        }
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );
        title += "\n" + tr( "Status: ", "Статус: " ) + perk_lock_reason( perk );
        if( exclusive_specialization_root( perk ) ) {
            title += "\n" + tr( "Prime specialization: choosing it locks the other two choices until a full respec.",
                                  "Прайм-специализация: её выбор закрывает два других варианта до полного сброса." );
        } else if( specialization_perk( perk ) ) {
            title += "\n" + tr( "Part of the chosen Prime specialization.", "Часть выбранной Прайм-специализации." );
        }
        if( integration_perk( perk ) ) {
            title += "\n" + tr( "Requires mod: ", "Нужен мод: " ) + integration_mod_name( perk );
            title += "\n" + tr( "This perk improves abilities or mechanics from that mod.",
                                  "Этот перк усиливает способности или механику этого мода." );
        }

        std::string buy;
        if( maxed ) buy = tr( "[Maximum rank]", "[Максимальный ранг]" );
        else if( !unlocked ) buy = tr( "Locked", "Закрыто" );
        else if( rank > 0 ) buy = tr( "Upgrade to rank ", "Улучшить до ранга " ) +
                                  std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
        else buy = tr( "Purchase", "Купить" );

        std::string back = tr( "Back", "Назад" );
        if( maxed && mana_hand_perk( perk ) ) {
            const std::string paired = paired_mana_hand_item_name();
            if( !paired.empty() ) {
                title += "\n" + tr( "Paired virtual grip III+IV: ", "Парный виртуальный хват III+IV: " ) + paired;
                title += "\n" + tr(
                             "Both Mana Hands are occupied by one real two-handed item. The item remains in its normal CDDA location.",
                             "Обе руки маны заняты одним реальным двуручным предметом. Предмет остаётся в своём обычном месте CDDA." );
                const bool primary_controls = virtual_item_primary_controls_available();
                const bool primary_enabled =
                    primary_controls && virtual_item_primary_enabled_for_slot( mana_hand_pair_slot_id );
                const bool secondary_controls = virtual_item_secondary_controls_available();
                const bool secondary_enabled =
                    secondary_controls && virtual_item_secondary_enabled_for_slot( mana_hand_pair_slot_id );
                if( primary_controls ) {
                    title += "\n" + tr( "Primary melee: ", "Основное оружие: " ) +
                             ( primary_enabled ? tr( "ON", "ВКЛ" ) : tr( "OFF", "ВЫКЛ" ) );
                }
                if( secondary_controls ) {
                    title += "\n" + tr( "Secondary strike: ", "Дополнительный удар: " ) +
                             ( secondary_enabled ? tr( "ON", "ВКЛ" ) : tr( "OFF", "ВЫКЛ" ) );
                }

                const std::string toggle_pair_primary = primary_enabled ?
                    tr( "Disable primary Mana Hand melee", "Отключить основное оружие руки маны" ) :
                    tr( "Enable primary Mana Hand melee", "Включить основное оружие руки маны" );
                const std::string toggle_pair_secondary = secondary_enabled ?
                    tr( "Disable secondary strike", "Отключить дополнительный удар" ) :
                    tr( "Enable secondary strike", "Включить дополнительный удар" );
                const std::string release_pair =
                    tr( "Release paired virtual item", "Освободить парный виртуальный предмет" );

                std::vector<std::string> pair_labels;
                std::vector<int> pair_actions;
                if( primary_controls ) {
                    pair_labels.push_back( toggle_pair_primary );
                    pair_actions.push_back( 1 );
                }
                if( secondary_controls ) {
                    pair_labels.push_back( toggle_pair_secondary );
                    pair_actions.push_back( 2 );
                }
                pair_labels.push_back( release_pair );
                pair_actions.push_back( 3 );
                pair_labels.push_back( back );
                pair_actions.push_back( 4 );

                std::vector<const char *> pair_entries;
                pair_entries.reserve( pair_labels.size() );
                for( const std::string &label : pair_labels ) {
                    pair_entries.push_back( label.c_str() );
                }

                const int pair_choice = host->ui_choose ?
                    host->ui_choose( title.c_str(), pair_entries.data(), pair_entries.size() ) : -1;
                if( pair_choice < 0 || static_cast<size_t>( pair_choice ) >= pair_actions.size() ) {
                    return;
                }
                switch( pair_actions[static_cast<size_t>( pair_choice )] ) {
                    case 1:
                        if( !virtual_item_set_primary_for_slot(
                                mana_hand_pair_slot_id, !primary_enabled ) ) {
                            message( tr(
                                "This item cannot be used as the primary Mana Hand melee weapon.",
                                "Этот предмет нельзя использовать как основное оружие руки маны." ) );
                        }
                        continue;
                    case 2:
                        if( !virtual_item_set_secondary_for_slot(
                                mana_hand_pair_slot_id, !secondary_enabled ) ) {
                            message( tr(
                                "This item cannot be used for a Mana Hand secondary strike.",
                                "Этот предмет нельзя использовать для дополнительного удара рукой маны." ) );
                        }
                        continue;
                    case 3:
                        if( host2->virtual_item_clear ) {
                            host2->virtual_item_clear( module_id, mana_hand_pair_slot_id );
                            continue;
                        }
                        return;
                    default:
                        return;
                }
                return;
            }

            const std::string held = mana_hand_item_name( perk );
            title += "\n" + tr( "Virtual slot: ", "Виртуальный слот: " ) +
                     ( held.empty() ? tr( "empty", "пусто" ) : held );
            title += "\n" + tr(
                         "The item remains in its real CDDA location. Replacing or releasing the slot never creates a copy.",
                         "Предмет остаётся в своём реальном месте CDDA. Замена или освобождение слота не создаёт копию." );

            std::string equip = held.empty() ?
                                tr( "Equip carried item", "Экипировать предмет" ) :
                                tr( "Replace held item", "Заменить предмет" );
            std::string release = tr( "Release virtual item", "Освободить предмет" );
            const char *slot_id = mana_hand_slot_id( perk );
            if( held.empty() ) {
                const bool fourth_hand = std::string_view( perk.id ) == "mg_mana_hand_4";
                const bool both_single_slots_empty =
                    !virtual_item_slot_occupied( "mana_hand_3" ) &&
                    !virtual_item_slot_occupied( "mana_hand_4" );
                std::string equip_pair = tr(
                    "Equip two-handed item with Mana Hands III+IV",
                    "Экипировать двуручный предмет руками маны III+IV" );

                int choice = -1;
                if( fourth_hand && both_single_slots_empty ) {
                    const char *entries[] = { equip.c_str(), equip_pair.c_str(), back.c_str() };
                    choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 3 ) : -1;
                    if( choice == 2 || choice < 0 ) return;
                } else {
                    const char *entries[] = { equip.c_str(), back.c_str() };
                    choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
                    if( choice != 0 ) return;
                }

                if( choice == 1 && fourth_hand && both_single_slots_empty ) {
                    const std::string picker_title =
                        tr( "Choose two-handed item for Mana Hands III+IV",
                            "Выберите двуручный предмет для рук маны III+IV" );
                    if( host2->virtual_item_choose ) {
                        host2->virtual_item_choose(
                            module_id, mana_hand_pair_slot_id, picker_title.c_str(),
                            NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2 |
                            NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2 |
                            NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2 |
                            NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2 );
                    }
                    continue;
                }

                const std::string picker_title =
                    tr( "Choose item for ", "Выберите предмет для " ) + perk_display_name( perk );
                if( host2->virtual_item_choose ) {
                    host2->virtual_item_choose(
                        module_id, slot_id, picker_title.c_str(),
                        NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2 |
                        NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2 );
                }
                continue;
            }

            const bool primary_controls = virtual_item_primary_controls_available();
            const bool primary_enabled =
                primary_controls && virtual_item_primary_enabled_for_slot( slot_id );
            const bool secondary_controls = virtual_item_secondary_controls_available();
            const bool secondary_enabled =
                secondary_controls && virtual_item_secondary_enabled_for_slot( slot_id );
            if( primary_controls ) {
                title += "\n" + tr( "Primary melee: ", "Основное оружие: " ) +
                         ( primary_enabled ? tr( "ON", "ВКЛ" ) : tr( "OFF", "ВЫКЛ" ) );
            }
            if( secondary_controls ) {
                title += "\n" + tr( "Secondary strike: ", "Дополнительный удар: " ) +
                         ( secondary_enabled ? tr( "ON", "ВКЛ" ) : tr( "OFF", "ВЫКЛ" ) );
            }

            const std::string toggle_primary = primary_enabled ?
                tr( "Disable primary Mana Hand melee", "Отключить основное оружие руки маны" ) :
                tr( "Enable primary Mana Hand melee", "Включить основное оружие руки маны" );
            const std::string toggle_secondary = secondary_enabled ?
                tr( "Disable secondary strike", "Отключить дополнительный удар" ) :
                tr( "Enable secondary strike", "Включить дополнительный удар" );

            std::vector<std::string> labels;
            std::vector<int> actions;
            labels.push_back( equip );
            actions.push_back( 1 );
            if( primary_controls ) {
                labels.push_back( toggle_primary );
                actions.push_back( 2 );
            }
            if( secondary_controls ) {
                labels.push_back( toggle_secondary );
                actions.push_back( 3 );
            }
            labels.push_back( release );
            actions.push_back( 4 );
            labels.push_back( back );
            actions.push_back( 5 );

            std::vector<const char *> entries;
            entries.reserve( labels.size() );
            for( const std::string &label : labels ) {
                entries.push_back( label.c_str() );
            }

            const int choice = host->ui_choose ?
                host->ui_choose( title.c_str(), entries.data(), entries.size() ) : -1;
            if( choice < 0 || static_cast<size_t>( choice ) >= actions.size() ) {
                return;
            }
            switch( actions[static_cast<size_t>( choice )] ) {
                case 1: {
                    const std::string picker_title =
                        tr( "Choose item for ", "Выберите предмет для " ) + perk_display_name( perk );
                    if( host2->virtual_item_choose ) {
                        host2->virtual_item_choose(
                            module_id, slot_id, picker_title.c_str(),
                            NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2 |
                            NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2 );
                    }
                    continue;
                }
                case 2:
                    if( !virtual_item_set_primary_for_slot( slot_id, !primary_enabled ) ) {
                        message( tr(
                            "This item cannot be used as the primary Mana Hand melee weapon.",
                            "Этот предмет нельзя использовать как основное оружие руки маны." ) );
                    }
                    continue;
                case 3:
                    if( !virtual_item_set_secondary_for_slot( slot_id, !secondary_enabled ) ) {
                        message( tr(
                            "This item cannot be used for a Mana Hand secondary strike.",
                            "Этот предмет нельзя использовать для дополнительного удара рукой маны." ) );
                    }
                    continue;
                case 4:
                    if( host2->virtual_item_clear ) {
                        host2->virtual_item_clear( module_id, slot_id );
                        continue;
                    }
                    return;
                default:
                    return;
            }
            return;
        }

        const char *entries[] = { buy.c_str(), back.c_str() };
        const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
        if( choice != 0 || maxed ) return;
        if( !unlocked ) {
            message( tr( "This perk is locked.", "Этот перк пока закрыт." ) );
            continue;
        }
        purchase_perk( perk );
        return;
    }
}

int branch_unlocked_count( branch_id branch, int64_t )
{
    const int64_t level = branch_level( branch );
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !integration_perk( perk ) &&
            perk_world_available( perk ) && !perk_maxed( perk ) &&
            level >= perk.required_level && prerequisites_met( perk ) ) {
            ++result;
        }
    }
    return result;
}

struct card_text {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string body;
    std::string badge;
    std::string icon_key;
    uint32_t flags = NCMM_UI_CARD_NONE;
};

std::vector<ncmm_ui_card_v1> bind_cards( std::vector<card_text> &texts )
{
    std::vector<ncmm_ui_card_v1> result;
    result.reserve( texts.size() );
    for( card_text &text : texts ) {
        result.push_back( {
            text.id.c_str(),
            text.title.c_str(),
            text.subtitle.c_str(),
            text.body.c_str(),
            text.badge.c_str(),
            text.icon_key.c_str(),
            text.flags
        } );
    }
    return result;
}

struct tree_node_text {
    card_text card;
    int row = 0;
    int column = 0;
};

std::pair<int, int> branch_tree_position( branch_id branch, size_t branch_index )
{
    // Base 20-node topology is preserved exactly for save/UI familiarity.
    static const std::array<std::pair<int, int>, 20> standard = {{
        { 0, 0 }, { 0, 4 }, { 2, 0 }, { 2, 4 }, { 4, 0 }, { 4, 4 },
        { 6, 0 }, { 6, 4 }, { 8, 2 }, { 10, 2 },
        { 1, 1 }, { 1, 3 }, { 3, 1 }, { 3, 3 }, { 5, 1 }, { 5, 3 },
        { 7, 1 }, { 7, 3 }, { 9, 2 }, { 11, 2 }
    }};
    static const std::array<std::pair<int, int>, 20> mobility = {{
        { 0, 0 }, { 0, 4 }, { 2, 1 }, { 2, 3 }, { 4, 0 }, { 4, 4 },
        { 6, 1 }, { 6, 3 }, { 8, 2 }, { 10, 2 },
        { 1, 1 }, { 1, 3 }, { 3, 0 }, { 3, 4 }, { 5, 1 }, { 5, 3 },
        { 7, 0 }, { 7, 4 }, { 9, 2 }, { 11, 2 }
    }};
    static const std::array<std::pair<int, int>, 20> mastery = {{
        { 0, 1 }, { 0, 3 }, { 2, 0 }, { 2, 4 }, { 4, 1 }, { 4, 3 },
        { 6, 0 }, { 6, 4 }, { 8, 2 }, { 10, 2 },
        { 1, 2 }, { 1, 4 }, { 3, 1 }, { 3, 3 }, { 5, 2 }, { 5, 4 },
        { 7, 1 }, { 7, 3 }, { 9, 2 }, { 11, 2 }
    }};

    if( branch_index < 20 ) {
        if( branch == branch_id::mobility || branch == branch_id::scavenging ) {
            return mobility[branch_index];
        }
        if( branch == branch_id::mastery ) {
            return mastery[branch_index];
        }
        return standard[branch_index];
    }

    // 20..22 are exclusive Prime roots; 23..25 their capstones.
    // Compact 0/1/2 lanes keep all three choices visible; the host preserves Prime lanes.
    if( branch_index < 23 ) {
        return { 12, static_cast<int>( branch_index - 20 ) };
    }
    if( branch_index < 26 ) {
        return { 14, static_cast<int>( branch_index - 23 ) };
    }

    // Conditional mod-integration nodes live below specializations in a compact grid.
    const size_t integration_index = branch_index - 26;
    return { 16 + static_cast<int>( integration_index / 3 ) * 2,
             static_cast<int>( integration_index % 3 ) * 2 };
}

std::vector<ncmm_ui_tree_node_v1> bind_tree_nodes( std::vector<tree_node_text> &texts )
{
    std::vector<ncmm_ui_tree_node_v1> result;
    result.reserve( texts.size() );
    for( tree_node_text &text : texts ) {
        result.push_back( {
            text.card.id.c_str(),
            text.card.title.c_str(),
            text.card.subtitle.c_str(),
            text.card.body.c_str(),
            text.card.badge.c_str(),
            text.card.icon_key.c_str(),
            text.card.flags,
            text.row,
            text.column
        } );
    }
    return result;
}

std::string perk_kind_label( const perk_def &perk )
{
    if( mod_prime_specialization_root( perk ) || specialization_root( perk ) ) {
        return tr( "PRIME", "ПРАЙМ" );
    }
    if( specialization_perk( perk ) ) {
        return tr( "PRIME PERK", "ПРАЙМ-ПЕРК" );
    }
    if( integration_perk( perk ) ) return tr( "MOD PERK", "ПЕРК МОДА" );
    if( perk.currency == currency_id::major ) return tr( "MAJOR PERK", "БОЛЬШОЙ ПЕРК" );
    return tr( "PERK", "ПЕРК" );
}

std::string branch_icon_key( branch_id branch )
{
    return std::string( "survivor/branch/" ) + branch_name_en( branch );
}

void show_branch( branch_id branch )
{
    bool tree_mode = true;

    while( true ) {
        const int64_t level = branch_level( branch );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        std::vector<const perk_def *> branch_perks;
        branch_perks.reserve( 24 );
        std::vector<card_text> texts;
        texts.reserve( 24 );

        for( const perk_def &perk : perks ) {
            if( perk.branch != branch || integration_perk( perk ) || !perk_world_available( perk ) ) {
                continue;
            }
            branch_perks.push_back( &perk );

            const int rank = perk_rank( perk );
            const int max_rank = perk_max_rank( perk );
            const bool maxed = rank >= max_rank;
            const bool unlocked = level >= perk.required_level && prerequisites_met( perk );
            const bool enough = perk.currency == currency_id::perk ?
                                perk_points > 0 : major_points > 0;

            card_text card;
            card.id = perk.id;
            card.title = perk_display_name( perk );
            card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                            tr( "Lv ", "Ур " ) + std::to_string( perk.required_level ) +
                            " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
            if( max_rank > 1 ) {
                card.subtitle += " | " + tr( "R ", "Р " ) +
                                 std::to_string( rank ) + "/" + std::to_string( max_rank );
            }
            card.body = perk_description( perk );

            card.badge = perk_kind_label( perk );
            const std::string chevrons = rank_chevrons( perk );
            if( !chevrons.empty() ) card.badge += " | " + chevrons;
            card.badge += " | ";
            if( maxed ) {
                card.badge += max_rank > 1 ?
                              tr( "MAX ", "МАКС " ) + std::to_string( rank ) + "/" +
                              std::to_string( max_rank ) :
                              tr( "OWNED", "КУПЛЕНО" );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( rank > 0 ) {
                card.badge += tr( "RANK ", "РАНГ " ) + std::to_string( rank ) + "/" +
                              std::to_string( max_rank );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( !unlocked ) {
                card.badge += tr( "LOCKED", "ЗАКРЫТО" );
                card.flags |= NCMM_UI_CARD_LOCKED;
            } else if( !enough ) {
                card.badge += tr( "NO POINTS", "НЕТ ОЧКОВ" );
            } else {
                card.badge += tr( "AVAILABLE", "ДОСТУПНО" );
            }

            if( perk.currency == currency_id::major ) {
                card.flags |= NCMM_UI_CARD_MAJOR;
            }
            if( effective_kind( perk ) == perk_kind::effect ) {
                card.flags |= NCMM_UI_CARD_EFFECT;
            }
            card.icon_key = std::string( "survivor/perk/" ) + perk.id;
            texts.push_back( std::move( card ) );
        }

        const int owned_now = branch_owned_count( branch );
        const int total_now = branch_total_count( branch );
        const int unlocked_now = branch_unlocked_count( branch, level );

        std::string title = "Survivor Progression > " + branch_name( branch );
        std::string summary =
            tr( "Purchased ", "Куплено " ) + std::to_string( owned_now ) + "/" +
            std::to_string( total_now ) +
            tr( " | available ", " | доступно " ) + std::to_string( unlocked_now ) +
            " | P " + std::to_string( perk_points ) +
            " | M " + std::to_string( major_points );

        const int64_t blevel = branch_level( branch );
        const int64_t bxp = branch_xp( branch );
        const int64_t bnext = branch_xp_to_next( blevel );
        summary += tr( " | Lv ", " | ур. " ) + std::to_string( blevel ) +
                   " XP " + std::to_string( bxp ) + "/" + std::to_string( bnext ) +
                   " | " + branch_efficiency_text( branch );

        std::string progress_label =
            branch_name( branch ) + tr( " XP -> L", " XP -> ур." ) +
            std::to_string( blevel + 1 );
        ncmm_ui_progress_v1 progress{
            progress_label.c_str(),
            bxp,
            bnext
        };

        if( tree_mode && host->ui_tree_choose ) {
            std::vector<tree_node_text> tree_texts;
            tree_texts.reserve( branch_perks.size() );
            std::map<std::string, size_t> index_by_id;

            for( size_t i = 0; i < branch_perks.size(); ++i ) {
                const perk_def &perk = *branch_perks[i];
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( perk );
                const int tree_max_rank = perk_max_rank( perk );
                const bool tree_unlocked = level >= perk.required_level && prerequisites_met( perk );
                node.card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                                     tr( "Lv ", "ур. " ) + std::to_string( perk.required_level ) +
                                     " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                if( prime_visual_perk( perk ) ) {
                    node.card.title = "★ " + node.card.title;
                }
                node.card.badge = compact_tree_badge( perk, tree_unlocked,
                                                       perk_points, major_points, false );
                node.card.body = rpg_detail_body( perk, node.card.body, prereq_text( perk ) ) +
                                 "\n\n" + tr( "STATUS:", "СТАТУС:" ) + "\n" + perk_lock_reason( perk );
                const std::pair<int, int> position = branch_tree_position( branch, i );
                node.row = position.first;
                node.column = position.second;
                index_by_id[perk.id] = i;
                tree_texts.push_back( std::move( node ) );
            }

            std::vector<ncmm_ui_tree_edge_v1> edges;
            auto add_edge = [&]( const char *prereq, size_t to ) {
                if( prereq == nullptr || prereq[0] == '\0' ) {
                    return;
                }
                const auto it = index_by_id.find( prereq );
                if( it != index_by_id.end() ) {
                    edges.push_back( { it->second, to } );
                }
            };
            for( size_t i = 0; i < branch_perks.size(); ++i ) {
                add_edge( branch_perks[i]->prereq1, i );
                add_edge( branch_perks[i]->prereq2, i );
            }

            std::vector<ncmm_ui_tree_node_v1> nodes = bind_tree_nodes( tree_texts );
            const std::string tree_summary =
                summary + tr( " | Tree view | Tab: cards",
                              " | Дерево | Tab: карточки" );
            std::vector<uint32_t> rpg_item_accents;
            std::vector<uint32_t> rpg_item_borders;
            rpg_item_accents.reserve( branch_perks.size() );
            rpg_item_borders.reserve( branch_perks.size() );
            for( const perk_def *visual_perk : branch_perks ) {
                rpg_item_accents.push_back( branch_theme_color( branch ) );
                rpg_item_borders.push_back( visual_perk ? rpg_border_style_for( *visual_perk ) : NCMM_UI_BORDER_NORMAL );
            }
            const ncmm_ui_theme_ex_v1 theme{
                branch_theme_color( branch ),
                NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
                26, 42, rpg_item_accents.data(), rpg_item_accents.size(),
                rpg_item_borders.data(), rpg_item_borders.size()
            };
            const int choice = host->ui_tree_choose_rpg ?
                                   host->ui_tree_choose_rpg( title.c_str(), tree_summary.c_str(), &progress,
                                                             nodes.data(), nodes.size(), edges.data(), edges.size(), &theme ) :
                                   host->ui_tree_choose_themed( title.c_str(), tree_summary.c_str(), &progress,
                                                                nodes.data(), nodes.size(), edges.data(), edges.size(),
                                                                reinterpret_cast<const ncmm_ui_theme_v1 *>( &theme ) );
            if( choice == NCMM_UI_TREE_SHOW_CARDS ) {
                tree_mode = false;
                continue;
            }
            if( choice < 0 || static_cast<size_t>( choice ) >= branch_perks.size() ) {
                return;
            }
            show_perk_detail( *branch_perks[choice] );
            continue;
        }

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const std::string card_summary =
            summary + tr( " | Cards | Tab: tree",
                          " | Карточки | Tab: дерево" );
        const ncmm_ui_theme_v1 theme = branch_ui_theme( branch );
        const int choice = host->ui_card_choose_themed ?
                           host->ui_card_choose_themed( title.c_str(), card_summary.c_str(), &progress,
                                                        cards.data(), cards.size(), 2, &theme ) :
                           -1;
        if( choice == NCMM_UI_CARD_SHOW_TREE ) {
            tree_mode = true;
            continue;
        }
        if( choice < 0 || static_cast<size_t>( choice ) >= branch_perks.size() ) {
            return;
        }
        show_perk_detail( *branch_perks[choice] );
    }
}

void show_overview()
{
    const int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    const int64_t xp = std::max<int64_t>( 0, get_state( "xp", 0 ) );
    const int64_t perk_points = get_state( "perk_points", 0 );
    const int64_t major_points = get_state( "major_points", 0 );
    const int normal_owned = owned_count( currency_id::perk );
    const int major_owned = owned_count( currency_id::major );

    std::string out = "Survivor Progression v0.14.0\n";
    out += tr( "Level ", "Уровень " ) + std::to_string( level );
    out += " | XP " + std::to_string( xp ) + "/" + std::to_string( xp_to_next( level ) );
    out += "\nP " + std::to_string( perk_points ) + " | M " + std::to_string( major_points );
    out += "\n" + tr( "Purchased: ", "Куплено: " ) +
           std::to_string( normal_owned ) + "P / " + std::to_string( major_owned ) + "M";
    out += "\n" + tr( "Major points: every 5 levels, with no level cap.",
                       "Большие очки: каждые 5 уровней, без ограничения уровня." );
    out += "\n" + tr( "Mod perks available for: ", "Перки модов доступны для: " );
    std::vector<std::string> integration_names;
    if( active_world_mod( "magiclysm" ) ) integration_names.push_back( "Magiclysm" );
    if( active_world_mod( "mindovermatter" ) ) integration_names.push_back( "Mind Over Matter" );
    if( active_world_mod( "xedra_evolved" ) ) integration_names.push_back( "Xedra Evolved" );
    if( active_world_mod( "aftershock_exoplanet" ) ) integration_names.push_back( "Aftershock Exoplanet" );
    if( active_world_mod( "aftershock_prime" ) ) integration_names.push_back( "Aftershock Prime" );
    if( active_world_mod( "secronom" ) ) integration_names.push_back( "Secronom" );
    if( active_world_mod( "secronom_lore_expansion" ) ) integration_names.push_back( "Secronom+" );
    if( integration_names.empty() ) {
        out += tr( "none", "нет" );
    } else {
        for( size_t i = 0; i < integration_names.size(); ++i ) {
            if( i != 0 ) out += ", ";
            out += integration_names[i];
        }
    }

    for( branch_id branch : { branch_id::combat, branch_id::survival, branch_id::mobility,
                              branch_id::crafting, branch_id::scavenging, branch_id::mastery } ) {
        const int64_t blevel = branch_level( branch );
        out += "\n" + branch_name( branch ) + ": L" +
               std::to_string( blevel ) + " XP " +
               std::to_string( branch_xp( branch ) ) + "/" +
               std::to_string( branch_xp_to_next( blevel ) ) + " | perks " +
               std::to_string( branch_owned_count( branch ) ) + "/" +
               std::to_string( branch_total_count( branch ) ) + " | " +
               branch_efficiency_text( branch );
    }

    out += "\n\n" + tr( "Active effects:", "Активные эффекты:" );
    const std::map<std::string, double> totals = owned_effect_totals();
    if( totals.empty() && current_xp_bonus_pct == 0 ) {
        out += "\n" + tr( "none", "нет" );
    } else {
        for( const auto &entry : totals ) {
            out += "\n" + effect_display_label( entry.first ) + ": " +
                   effect_value_text( entry.first, entry.second );
        }
        if( current_xp_bonus_pct != 0 ) {
            out += "\n" + tr( "Survivor XP: +", "Опыт Survivor: +" ) +
                   std::to_string( current_xp_bonus_pct ) + "%";
        }
    }
    message( out );
}

void respec()
{
    int64_t refund_perk = 0;
    int64_t refund_major = 0;
    for( const perk_def &perk : perks ) {
        const int rank = perk_rank( perk );
        if( rank <= 0 ) continue;
        if( perk.currency == currency_id::perk ) refund_perk += rank;
        else refund_major += rank;
    }

    set_state( "respec_request", 0 );
    if( refund_perk == 0 && refund_major == 0 ) {
        set_state( "respec_available", 0 );
        message( tr( "No Survivor perks to reset.", "Нет перков Survivor для сброса." ) );
        return;
    }

    clear_mana_hand_slots();
    for( const perk_def &perk : perks ) {
        if( perk_rank( perk ) > 0 ) set_state( perk_key( perk ), 0 );
    }
    set_state( "perk_points", get_state( "perk_points", 0 ) + refund_perk );
    set_state( "major_points", get_state( "major_points", 0 ) + refund_major );
    set_state( "fast_learner", 0 );
    for( branch_id branch : all_branches ) set_state( specialization_state_key( branch ), 0 );
    for( const char *key : {
             "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
             "prime_aftershock_exoplanet", "prime_aftershock_prime",
             "prime_secronom", "prime_secronom_plus"
         } ) {
        set_state( key, 0 );
    }
    set_state( "momentum_stacks", 0 );
    set_state( "momentum_turns", 0 );
    set_state( "respec_available", 0 );
    effects_dirty = true;
    recalculate_effects();
    message( tr( "Survivor recalibration complete. Refunded: ",
                 "Рекалибровка Survivor завершена. Возвращено: " ) +
             std::to_string( refund_perk ) + "P / " + std::to_string( refund_major ) + "M" );
}

std::vector<std::string> active_supported_integration_mods()
{
    static const std::array<const char *, 7> supported = {{
        "magiclysm", "mindovermatter", "xedra_evolved", "aftershock_exoplanet",
        "aftershock_prime", "secronom", "secronom_lore_expansion"
    }};
    std::set<std::string> active;
    if( host != nullptr && host->world_mod_count != nullptr && host->world_mod_id != nullptr ) {
        const size_t count = std::min<size_t>( host->world_mod_count(), 1024 );
        for( size_t i = 0; i < count; ++i ) {
            const char *id = host->world_mod_id( i );
            if( id != nullptr && id[0] != '\0' ) active.emplace( id );
        }
    }
    std::vector<std::string> result;
    result.reserve( supported.size() );
    for( const char *id : supported ) {
        if( active.count( id ) != 0 || active_world_mod( id ) ) result.emplace_back( id );
    }
    return result;
}
const char *integration_anchor_id( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return "mg_arcane_focus";
    if( mod_id == "mindovermatter" ) return "mom_mental_focus";
    if( mod_id == "xedra_evolved" ) return "xe_anomaly_method";
    if( mod_id == "aftershock_exoplanet" ) return "af_systems_operator";
    if( mod_id == "aftershock_prime" ) return "afp_prime_operator";
    if( mod_id == "secronom" ) return "sec_field_researcher";
    if( mod_id == "secronom_lore_expansion" ) return "secx_flesh_initiate";
    return "";
}

std::string integration_mod_display_name( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return "Magiclysm";
    if( mod_id == "mindovermatter" ) return "Mind Over Matter";
    if( mod_id == "xedra_evolved" ) return "Xedra Evolved";
    if( mod_id == "aftershock_exoplanet" ) return "Aftershock Exoplanet";
    if( mod_id == "aftershock_prime" ) return "Aftershock Prime";
    if( mod_id == "secronom" ) return "Secronom";
    if( mod_id == "secronom_lore_expansion" ) return "Secronom+";
    return mod_id;
}

std::string integration_mod_focus( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return tr(
        "Magiclysm: better Spellcraft, safer casting and stronger spells.",
        "Magiclysm: выше Spellcraft, надёжнее сотворение и сильнее заклинания." );
    if( mod_id == "mindovermatter" ) return tr(
        "Mind Over Matter: stronger powers, steadier channeling and lower stamina cost.",
        "Mind Over Matter: сильнее способности, стабильнее концентрация и ниже расход выносливости." );
    if( mod_id == "xedra_evolved" ) return tr(
        "Xedra Evolved: Deduction, Gramarye and stronger anomaly abilities.",
        "Xedra Evolved: Deduction, Gramarye и усиление аномальных способностей." );
    if( mod_id == "aftershock_exoplanet" ) return tr(
        "Aftershock Exoplanet: Smartgun expertise and stronger, steadier esper powers.",
        "Aftershock Exoplanet: навык Smartgun и более сильные, стабильные эспер-способности." );
    if( mod_id == "aftershock_prime" ) return tr(
        "Aftershock Prime: Smartgun expertise, translocation and more efficient abilities.",
        "Aftershock Prime: навык Smartgun, транслокация и более эффективные способности." );
    if( mod_id == "secronom" ) return tr(
        "Secronom: more damage and resistance against its creatures, elites and Crimson Horrors.",
        "Secronom: больше урона и защиты против его существ, элиты и Crimson Horrors." );
    if( mod_id == "secronom_lore_expansion" ) return tr(
        "Secronom+: Flesh Weaving, living weapons and Flesh Vessel abilities.",
        "Secronom+: Flesh Weaving, живое оружие и способности Flesh Vessel." );
    return {};
}

int integration_owned_count( const std::string &mod_id )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( integration_perk( perk ) && perk_world_available( perk ) &&
            mod_id == integration_mod_id( perk ) && owned( perk ) ) ++result;
    }
    return result;
}

int integration_total_count( const std::string &mod_id )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( integration_perk( perk ) && perk_world_available( perk ) &&
            mod_id == integration_mod_id( perk ) ) ++result;
    }
    return result;
}

std::pair<int, int> integration_tree_position( size_t i )
{
    static const std::array<std::pair<int, int>, 20> layout = {{
        { 0, 2 },
        { 2, 0 }, { 2, 2 }, { 2, 4 },
        { 4, 0 }, { 4, 2 }, { 4, 4 },
        { 6, 0 }, { 6, 2 }, { 6, 4 },
        { 8, 0 }, { 8, 2 }, { 8, 4 },
        { 10, 0 }, { 10, 2 }, { 10, 4 },
        { 12, 0 }, { 12, 2 }, { 12, 4 },
        { 14, 2 }
    }};
    if( i < layout.size() ) return layout[i];
    if( i < 23 ) return { 16, static_cast<int>( i - 20 ) };
    return { 18 + static_cast<int>( ( i - 23 ) / 3 ) * 2,
             static_cast<int>( ( i - 23 ) % 3 ) };
}

void show_integration_branch( const std::string &mod_id )
{
    if( !active_world_mod( mod_id.c_str() ) ) {
        message( tr( "This mod is not active in the current world.",
                     "Этот мод не активен в текущем мире." ) );
        return;
    }

    bool tree_mode = true;
    while( true ) {
        const int64_t survivor_level = std::max<int64_t>( 1, get_state( "level", 1 ) );
        const int64_t survivor_xp = std::max<int64_t>( 0, get_state( "xp", 0 ) );
        const int64_t survivor_next = xp_to_next( survivor_level );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        std::vector<const perk_def *> mod_perks;
        mod_perks.reserve( 24 );
        const char *anchor_id = integration_anchor_id( mod_id );
        const perk_def *anchor = find_perk( anchor_id );
        if( anchor != nullptr && integration_perk( *anchor ) && mod_id == integration_mod_id( *anchor ) ) {
            mod_perks.push_back( anchor );
        }
        for( const perk_def &perk : perks ) {
            if( !integration_perk( perk ) || !perk_world_available( perk ) ||
                mod_id != integration_mod_id( perk ) || &perk == anchor ) continue;
            mod_perks.push_back( &perk );
        }
        if( mod_perks.empty() ) {
            message( tr( "No Survivor perks are available for this mod.",
                         "Для этого мода нет доступных перков Survivor." ) );
            return;
        }

        std::vector<card_text> texts;
        texts.reserve( mod_perks.size() );
        for( const perk_def *perk_ptr : mod_perks ) {
            const perk_def &perk = *perk_ptr;
            const int rank = perk_rank( perk );
            const int max_rank = perk_max_rank( perk );
            const bool maxed = rank >= max_rank;
            const bool unlocked = survivor_level >= perk.required_level && prerequisites_met( perk );
            const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;

            card_text card;
            card.id = perk.id;
            card.title = perk_display_name( perk );
            card.subtitle = tr( "Survivor L", "Survivor ур." ) + std::to_string( perk.required_level ) +
                            " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
            card.body = perk_description( perk );
            card.badge = perk_kind_label( perk );
            const std::string chevrons = rank_chevrons( perk );
            if( !chevrons.empty() ) card.badge += " | " + chevrons;
            card.badge += " | ";
            if( maxed ) {
                card.badge += tr( "OWNED", "КУПЛЕНО" );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( !unlocked ) {
                card.badge += tr( "LOCKED", "ЗАКРЫТО" );
                card.flags |= NCMM_UI_CARD_LOCKED;
            } else if( !enough ) {
                card.badge += tr( "NO POINTS", "НЕТ ОЧКОВ" );
            } else {
                card.badge += tr( "AVAILABLE", "ДОСТУПНО" );
            }
            if( perk.currency == currency_id::major ) card.flags |= NCMM_UI_CARD_MAJOR;
            card.flags |= NCMM_UI_CARD_EFFECT;
            card.icon_key = std::string( "survivor/mod/" ) + mod_id + "/" + perk.id;
            texts.push_back( std::move( card ) );
        }

        const std::string mod_name = integration_mod_display_name( mod_id );
        std::string title = "Survivor Progression > " + mod_name;
        std::string summary = tr( "Mod perks", "Перки мода" ) +
                              tr( " | purchased ", " | куплено " ) +
                              std::to_string( integration_owned_count( mod_id ) ) + "/" +
                              std::to_string( integration_total_count( mod_id ) ) +
                              " | P " + std::to_string( perk_points ) + " | M " + std::to_string( major_points ) +
                              tr( " | requires Survivor level", " | нужен уровень Survivor" );
        std::string progress_label = "Survivor XP " + std::to_string( survivor_xp ) + "/" +
                                     std::to_string( survivor_next ) + tr( " -> L", " -> ур." ) +
                                     std::to_string( survivor_level + 1 );
        ncmm_ui_progress_v1 progress{ progress_label.c_str(), survivor_xp, survivor_next };

        if( tree_mode && host->ui_tree_choose ) {
            std::vector<tree_node_text> tree_texts;
            tree_texts.reserve( mod_perks.size() );
            std::map<std::string, size_t> index_by_id;
            for( size_t i = 0; i < mod_perks.size(); ++i ) {
                const perk_def &tree_perk = *mod_perks[i];
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( tree_perk );
                const int tree_max_rank = perk_max_rank( tree_perk );
                const bool tree_unlocked = survivor_level >= tree_perk.required_level &&
                                           prerequisites_met( tree_perk );
                node.card.subtitle = tr( "Lv ", "ур. " ) + std::to_string( tree_perk.required_level ) +
                                     " | " + ( tree_perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                if( prime_visual_perk( tree_perk ) ) {
                    node.card.title = "★ " + node.card.title;
                }
                node.card.badge = compact_tree_badge( tree_perk, tree_unlocked,
                                                       perk_points, major_points, true );
                node.card.body = rpg_detail_body( tree_perk, node.card.body, prereq_text( tree_perk ) ) +
                                 "\n\n" + tr( "STATUS:", "СТАТУС:" ) + "\n" + perk_lock_reason( tree_perk );
                const std::pair<int, int> pos = integration_tree_position( i );
                node.row = pos.first;
                node.column = pos.second;
                index_by_id[mod_perks[i]->id] = i;
                tree_texts.push_back( std::move( node ) );
            }
            std::vector<ncmm_ui_tree_edge_v1> edges;
            auto add_edge = [&]( const char *prereq, size_t to ) {
                if( prereq == nullptr || prereq[0] == '\0' ) return;
                const auto it = index_by_id.find( prereq );
                if( it != index_by_id.end() ) edges.push_back( { it->second, to } );
            };
            for( size_t i = 0; i < mod_perks.size(); ++i ) {
                add_edge( mod_perks[i]->prereq1, i );
                add_edge( mod_perks[i]->prereq2, i );
            }
            std::vector<ncmm_ui_tree_node_v1> nodes = bind_tree_nodes( tree_texts );
            std::vector<uint32_t> rpg_item_accents;
            std::vector<uint32_t> rpg_item_borders;
            rpg_item_accents.reserve( mod_perks.size() );
            rpg_item_borders.reserve( mod_perks.size() );
            for( const perk_def *visual_perk : mod_perks ) {
                rpg_item_accents.push_back( integration_theme_color( mod_id ) );
                rpg_item_borders.push_back( visual_perk ? rpg_border_style_for( *visual_perk ) : NCMM_UI_BORDER_NORMAL );
            }
            const ncmm_ui_theme_ex_v1 theme{
                integration_theme_color( mod_id ),
                NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
                26, 42, rpg_item_accents.data(), rpg_item_accents.size(),
                rpg_item_borders.data(), rpg_item_borders.size()
            };
            const int choice = host->ui_tree_choose_rpg ?
                               host->ui_tree_choose_rpg( title.c_str(), summary.c_str(), &progress,
                                                         nodes.data(), nodes.size(), edges.data(), edges.size(), &theme ) :
                               host->ui_tree_choose_themed( title.c_str(), summary.c_str(), &progress,
                                                            nodes.data(), nodes.size(), edges.data(), edges.size(),
                                                            reinterpret_cast<const ncmm_ui_theme_v1 *>( &theme ) );
            if( choice == NCMM_UI_TREE_SHOW_CARDS ) { tree_mode = false; continue; }
            if( choice < 0 || static_cast<size_t>( choice ) >= mod_perks.size() ) return;
            show_perk_detail( *mod_perks[choice] );
            continue;
        }

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const ncmm_ui_theme_v1 theme = integration_ui_theme( mod_id );
        const int choice = host->ui_card_choose_themed ?
                           host->ui_card_choose_themed( title.c_str(), summary.c_str(), &progress,
                                                        cards.data(), cards.size(), 2, &theme ) : -1;
        if( choice == NCMM_UI_CARD_SHOW_TREE ) { tree_mode = true; continue; }
        if( choice < 0 || static_cast<size_t>( choice ) >= mod_perks.size() ) return;
        show_perk_detail( *mod_perks[choice] );
    }
}

void open_progression()
{
    if( !character_available() ) {
        message( tr( "Survivor Progression: load a character first.",
                     "Survivor Progression: сначала загрузите персонажа." ) );
        return;
    }

    migrate_state();
    if( effects_dirty ) {
        recalculate_effects();
    }

    while( true ) {
        const int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
        const int64_t xp = std::max<int64_t>( 0, get_state( "xp", 0 ) );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        const std::array<branch_id, 6> branches = {
            branch_id::combat, branch_id::survival, branch_id::mobility,
            branch_id::crafting, branch_id::scavenging, branch_id::mastery
        };

        std::vector<card_text> texts;
        texts.reserve( 16 );
        for( branch_id branch : branches ) {
            card_text card;
            card.id = branch_name_en( branch );
            card.title = branch_name( branch );
            const int64_t blevel = branch_level( branch );
            const int64_t bxp = branch_xp( branch );
            const int64_t bnext = branch_xp_to_next( blevel );
            card.subtitle =
                tr( "Lv ", "Ур. " ) + std::to_string( blevel ) +
                " | XP " + std::to_string( bxp ) + "/" + std::to_string( bnext ) +
                " | " + std::to_string( branch_owned_count( branch ) ) + "/" +
                std::to_string( branch_total_count( branch ) );
            card.body = branch_focus( branch ) + "\n\n" +
                        tr( "HOW TO GAIN XP:", "КАК КАЧАТЬ:" ) + "\n" +
                        branch_xp_source( branch ) + "\n\n" +
                        tr( "PROGRESSION:", "ПРОГРЕСС:" ) + "\n" +
                        tr( "Branch level ", "Уровень ветки " ) + std::to_string( blevel ) +
                        " | XP " + std::to_string( bxp ) + "/" + std::to_string( bnext ) + "\n" +
                        tr( "Available now: ", "Доступно сейчас: " ) +
                        std::to_string( branch_unlocked_count( branch, blevel ) ) + "\n" +
                        tr( "Next unlock: ", "Следующее открытие: " ) + branch_next_unlock_text( branch ) + "\n\n" +
                        tr( "EFFICIENCY:", "ЭФФЕКТИВНОСТЬ:" ) + "\n" +
                        branch_efficiency_text( branch );
            card.badge = tr( "BRANCH", "ВЕТКА" );
            card.icon_key = branch_icon_key( branch );
            card.flags = NCMM_UI_CARD_ACCENT;
            texts.push_back( std::move( card ) );
        }

        std::vector<std::string> mod_branches;
        auto add_mod_branch = [&]( const char *mod_id ) {
            if( !active_world_mod( mod_id ) ) {
                return;
            }
            const std::string id = mod_id;
            mod_branches.push_back( id );

            card_text card;
            card.id = std::string( "mod_" ) + id;
            card.title = integration_mod_display_name( id );
            card.subtitle =
                tr( "Mod perks | ", "Перки мода | " ) +
                std::to_string( integration_owned_count( id ) ) + "/" +
                std::to_string( integration_total_count( id ) );
            card.body = integration_mod_focus( id ) + "\n\n" +
                        tr( "PROGRESSION:", "ПРОГРЕСС:" ) + "\n" +
                        tr( "These perks use your overall Survivor level instead of separate branch XP.",
                            "Эти перки используют общий уровень Survivor вместо отдельного опыта ветки." ) + "\n" +
                        tr( "Survivor level: ", "Уровень Survivor: " ) + std::to_string( level ) + "\n" +
                        tr( "Available now: ", "Доступно сейчас: " ) +
                        std::to_string( integration_ready_count( id ) ) + "\n" +
                        tr( "Next unlock: ", "Следующее открытие: " ) + integration_next_unlock_text( id ) + "\n\n" +
                        tr( "Available only in worlds where this mod is active.",
                            "Доступно только в мирах, где активен этот мод." );
            card.badge = tr( "MOD PERKS", "ПЕРКИ МОДА" );
            card.icon_key = std::string( "survivor/mod/" ) + id;
            card.flags = NCMM_UI_CARD_ACCENT;
            texts.push_back( std::move( card ) );
        };
        for( const std::string &mod_id : active_supported_integration_mods() ) {
            add_mod_branch( mod_id.c_str() );
        }

        const int overview_index = static_cast<int>( texts.size() );
        card_text overview;
        overview.id = "overview";
        overview.title = tr( "Overview", "Обзор" );
        overview.subtitle = tr( "Level / points / active effects", "Уровень / очки / активные эффекты" );
        overview.body = tr( "View your level, points, branch progress and active bonuses.",
                            "Уровень, очки, прогресс веток и действующие бонусы." );
        overview.badge = tr( "INFO", "ИНФО" );
        overview.icon_key = "survivor/action/overview";
        texts.push_back( std::move( overview ) );

        const int close_index = static_cast<int>( texts.size() );
        card_text close;
        close.id = "close";
        close.title = tr( "Close", "Закрыть" );
        close.subtitle = tr( "Return to game", "Вернуться в игру" );
        close.body = tr( "Keep your build and continue playing.",
                         "Сохранить билд и вернуться в игру." );
        close.badge = tr( "ACTION", "ДЕЙСТВИЕ" );
        close.icon_key = "survivor/action/close";
        texts.push_back( std::move( close ) );

        const int total_owned = owned_count( currency_id::perk ) +
                                owned_count( currency_id::major );
        const int total_perks = visible_perk_count();

        std::string title = "Survivor Progression v0.14.0";
        std::string summary =
            tr( "Level ", "Уровень " ) + std::to_string( level ) +
            " | P " + std::to_string( perk_points ) +
            " | M " + std::to_string( major_points ) +
            tr( " | purchased ", " | куплено " ) +
            std::to_string( total_owned ) + "/" + std::to_string( total_perks );
        if( !mod_branches.empty() ) {
            summary += tr( " | mods ", " | модов " ) +
                       std::to_string( mod_branches.size() );
        }

        const int64_t xp_needed = xp_to_next( level );
        std::string progress_label =
            "XP " + std::to_string( xp ) + "/" + std::to_string( xp_needed ) +
            tr( " -> Level ", " -> Уровень " ) + std::to_string( level + 1 );
        ncmm_ui_progress_v1 progress{ progress_label.c_str(), xp, xp_needed };

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        std::vector<uint32_t> item_accents;
        item_accents.reserve( cards.size() );
        for( branch_id branch : branches ) {
            item_accents.push_back( branch_theme_color( branch ) );
        }
        for( const std::string &mod_id : mod_branches ) {
            item_accents.push_back( integration_theme_color( mod_id ) );
        }
        while( item_accents.size() < cards.size() ) {
            item_accents.push_back( NCMM_UI_COLOR_DEFAULT );
        }
        const ncmm_ui_theme_ex_v1 overview_theme{
            NCMM_UI_COLOR_DEFAULT,
            NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_SECTIONED_DETAIL,
            0, 42, item_accents.data(), item_accents.size(), nullptr, 0
        };
        const int choice = host->ui_card_choose_rpg ?
                           host->ui_card_choose_rpg( title.c_str(), summary.c_str(), &progress,
                                                     cards.data(), cards.size(), 3, &overview_theme ) :
                           host->ui_card_choose_themed ?
                           host->ui_card_choose_themed( title.c_str(), summary.c_str(), &progress,
                                                        cards.data(), cards.size(), 3,
                                                        reinterpret_cast<const ncmm_ui_theme_v1 *>( &overview_theme ) ) :
                           -1;

        if( choice >= 0 && choice < static_cast<int>( branches.size() ) ) {
            show_branch( branches[static_cast<size_t>( choice )] );
            continue;
        }

        const int mod_offset = static_cast<int>( branches.size() );
        const int mod_end = mod_offset + static_cast<int>( mod_branches.size() );
        if( choice >= mod_offset && choice < mod_end ) {
            show_integration_branch( mod_branches[static_cast<size_t>( choice - mod_offset )] );
            continue;
        }
        if( choice == overview_index ) {
            show_overview();
            continue;
        }

        if( choice == close_index || choice < 0 ) {
            return;
        }
    }
}

void award_global_xp( int64_t raw_gained )
{
    if( raw_gained <= 0 || !character_available() ) {
        return;
    }

    int64_t fraction = get_state( "xp_fraction", 0 );
    const int64_t multiplier = std::max<int64_t>( 0, 100 + current_xp_bonus_pct );
    if( raw_gained > ( std::numeric_limits<int64_t>::max() - fraction ) /
        std::max<int64_t>( 1, multiplier ) ) {
        raw_gained = ( std::numeric_limits<int64_t>::max() - fraction ) /
                     std::max<int64_t>( 1, multiplier );
    }
    fraction += raw_gained * multiplier;
    int64_t gained = fraction / 100;
    fraction %= 100;
    set_state( "xp_fraction", fraction );
    if( gained <= 0 ) {
        return;
    }

    int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    int64_t xp = std::max<int64_t>( 0, get_state( "xp", 0 ) ) + gained;
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    int64_t major_awarded = get_state( "major_awarded", 0 );
    int64_t levels_gained = 0;
    int64_t majors_gained = 0;

    while( xp >= xp_to_next( level ) && level < std::numeric_limits<int64_t>::max() ) {
        xp -= xp_to_next( level );
        ++level;
        ++perk_points;
        ++levels_gained;
        if( level % 5 == 0 ) {
            ++major_points;
            ++major_awarded;
            ++majors_gained;
        }
    }

    set_state( "level", level );
    set_state( "xp", xp );
    set_state( "perk_points", perk_points );
    set_state( "major_points", major_points );
    set_state( "major_awarded", major_awarded );

    if( levels_gained > 0 ) {
        std::string text = tr( "Survivor level up! +", "Новый уровень Survivor! +" ) +
                           std::to_string( levels_gained ) +
                           tr( " perk point(s).", " очк. перков." );
        if( majors_gained > 0 ) {
            text += tr( " +", " +" ) + std::to_string( majors_gained ) +
                    tr( " major point(s).", " больших очк." );
        }
        message( text );
    }
}

int64_t award_branch_xp( branch_id branch, int64_t raw_gained )
{
    const int64_t adjusted = anti_farm_adjust( branch, raw_gained );
    const int64_t gained = apply_branch_xp_balance( branch, adjusted );
    if( gained <= 0 ) {
        return 0;
    }

    const int64_t old_level = branch_level( branch );
    int64_t level = old_level;
    int64_t xp = branch_xp( branch ) + gained;
    while( xp >= branch_xp_to_next( level ) &&
           level < std::numeric_limits<int64_t>::max() ) {
        xp -= branch_xp_to_next( level );
        ++level;
    }
    set_state( branch_state_key( branch, "level" ), level );
    set_state( branch_state_key( branch, "xp" ), xp );

    // Only post-anti-farm branch XP reaches the global Survivor level.
    award_global_xp( gained );

    if( level > old_level ) {
        message( branch_name( branch ) + tr( " branch level up: ", " — новый уровень ветки: " ) +
                 std::to_string( level ) );
    }
    return gained;
}

int64_t metric_now( const char *metric )
{
    return host && host->gameplay_metric_get_i64 ?
           std::max<int64_t>( 0, host->gameplay_metric_get_i64( metric ) ) : 0;
}

void prime_metric_baselines()
{
    const std::array<std::pair<const char *, const char *>, 7> metrics = {{
        { "combat.kills", "metric_combat_kills" },
        { "combat.kill_xp", "metric_combat_kill_xp" },
        { "survival.healing", "metric_survival_healing" },
        { "mobility.steps", "metric_mobility_steps" },
        { "crafting.completed", "metric_crafting_completed" },
        { "scavenging.omt", "metric_scavenging_omt" },
        { "mastery.skill_levels", "metric_mastery_skill_levels" }
    }};
    for( const auto &entry : metrics ) {
        set_state( entry.second, metric_now( entry.first ) );
    }
}

int64_t metric_delta( const char *metric, const char *baseline_key )
{
    const int64_t now = metric_now( metric );
    const int64_t before = get_state( baseline_key, -1 );
    if( before != now ) {
        set_state( baseline_key, now );
    }
    if( before < 0 || now < before ) {
        return 0;
    }
    return now - before;
}

int activity_diversity_bonus_pct( int active_branches )
{
    if( active_branches >= 4 ) return 15;
    if( active_branches == 3 ) return 10;
    if( active_branches == 2 ) return 5;
    return 0;
}

int64_t apply_activity_diversity_bonus( branch_id branch, int64_t raw, int bonus_pct )
{
    if( raw <= 0 || bonus_pct <= 0 ) {
        return raw;
    }
    const std::string key = branch_state_key( branch, "diversity_fraction" );
    int64_t scaled = raw * ( 100 + bonus_pct ) +
                     std::max<int64_t>( 0, get_state( key, 0 ) );
    const int64_t result = scaled / 100;
    set_state( key, scaled % 100 );
    return result;
}

int integration_branch_xp_bonus_pct( branch_id branch )
{
    int bonus = 0;
    if( active_world_mod( "magiclysm" ) &&
        ( branch == branch_id::crafting || branch == branch_id::mastery ) ) bonus += 10;
    if( active_world_mod( "mindovermatter" ) &&
        ( branch == branch_id::mobility || branch == branch_id::mastery ) ) bonus += 10;
    if( active_world_mod( "xedra_evolved" ) &&
        ( branch == branch_id::scavenging || branch == branch_id::mastery ) ) bonus += 10;
    if( active_world_mod( "aftershock_exoplanet" ) &&
        ( branch == branch_id::crafting || branch == branch_id::scavenging ) ) bonus += 10;
    return std::min( bonus, 20 );
}

int64_t scale_activity_xp( branch_id branch, int64_t raw, int diversity_bonus_pct )
{
    if( raw <= 0 ) return 0;
    const int total_bonus = diversity_bonus_pct + integration_branch_xp_bonus_pct( branch );
    if( total_bonus <= 0 ) return raw;
    const std::string key = branch_state_key( branch, "activity_bonus_fraction" );
    int64_t scaled = raw * ( 100 + total_bonus ) +
                     std::max<int64_t>( 0, get_state( key, 0 ) );
    const int64_t result = scaled / 100;
    set_state( key, scaled % 100 );
    return result;
}

void poll_branch_xp()
{
    decay_branch_fatigue();

    const int64_t kills = metric_delta( "combat.kills", "metric_combat_kills" );
    const int64_t kill_xp = metric_delta( "combat.kill_xp", "metric_combat_kill_xp" );
    int64_t combat_gain = std::min<int64_t>( std::max<int64_t>( kills, ( kill_xp + 49 ) / 50 ), 25 );

    int64_t healing = metric_delta( "survival.healing", "metric_survival_healing" );
    const int64_t old_heal_remainder = get_state( "survival_heal_remainder", 0 );
    healing += old_heal_remainder;
    int64_t survival_gain = std::min<int64_t>( healing / 10, 8 );
    const int64_t new_heal_remainder = healing % 10;
    if( new_heal_remainder != old_heal_remainder ) {
        set_state( "survival_heal_remainder", new_heal_remainder );
    }

    int64_t steps = metric_delta( "mobility.steps", "metric_mobility_steps" );
    const int64_t old_step_remainder = get_state( "mobility_step_remainder", 0 );
    steps += old_step_remainder;
    int64_t mobility_gain = std::min<int64_t>( steps / mobility_steps_per_xp, 3 );
    const int64_t new_step_remainder = steps % mobility_steps_per_xp;
    if( new_step_remainder != old_step_remainder ) {
        set_state( "mobility_step_remainder", new_step_remainder );
    }

    const int64_t crafts = metric_delta( "crafting.completed", "metric_crafting_completed" );
    int64_t crafting_gain = std::min<int64_t>( crafts * 3, 12 );
    const int64_t omts = metric_delta( "scavenging.omt", "metric_scavenging_omt" );
    int64_t scavenging_gain = std::min<int64_t>( omts * 4, 8 );
    const int64_t skill_levels = metric_delta( "mastery.skill_levels", "metric_mastery_skill_levels" );
    int64_t mastery_gain = std::min<int64_t>( skill_levels * 6, 18 );

    int active = 0;
    active += combat_gain > 0 ? 1 : 0;
    active += survival_gain > 0 ? 1 : 0;
    active += mobility_gain > 0 ? 1 : 0;
    active += crafting_gain > 0 ? 1 : 0;
    active += scavenging_gain > 0 ? 1 : 0;
    const int diversity_bonus = activity_diversity_bonus_pct( active );

    combat_gain = scale_activity_xp( branch_id::combat, combat_gain, diversity_bonus );
    survival_gain = scale_activity_xp( branch_id::survival, survival_gain, diversity_bonus );
    mobility_gain = scale_activity_xp( branch_id::mobility, mobility_gain, diversity_bonus );
    crafting_gain = scale_activity_xp( branch_id::crafting, crafting_gain, diversity_bonus );
    scavenging_gain = scale_activity_xp( branch_id::scavenging, scavenging_gain, diversity_bonus );
    mastery_gain = scale_activity_xp( branch_id::mastery, mastery_gain, 0 );

    int64_t activity_total = 0;
    activity_total += award_branch_xp( branch_id::combat, combat_gain );
    activity_total += award_branch_xp( branch_id::survival, survival_gain );
    activity_total += award_branch_xp( branch_id::mobility, mobility_gain );
    activity_total += award_branch_xp( branch_id::crafting, crafting_gain );
    activity_total += award_branch_xp( branch_id::scavenging, scavenging_gain );

    const int64_t old_mastery_fraction = get_state( "mastery_share_fraction", 0 );
    int64_t mastery_fraction = old_mastery_fraction + activity_total * 10;
    mastery_gain += mastery_fraction / 100;
    mastery_fraction %= 100;
    if( mastery_fraction != old_mastery_fraction ) {
        set_state( "mastery_share_fraction", mastery_fraction );
    }
    award_branch_xp( branch_id::mastery, mastery_gain );
}

void tick()
{
    const bool available = character_available();
    if( !available ) {
        if( last_character_available ) {
            clear_runtime_modifiers();
            effects_dirty = true;
        }
        last_character_available = false;
        turn_accumulator = 0;
        return;
    }

    if( !last_character_available ) {
        last_character_available = true;
        migrate_state();
        prime_metric_baselines();
        effects_dirty = true;
    }
    if( get_state( "respec_request", 0 ) > 0 ) {
        respec();
    }
    const int stat_power = progression_stat_power_pct();
    if( stat_power != last_stat_power_pct ) {
        last_stat_power_pct = stat_power;
        effects_dirty = true;
    }
    if( effects_dirty ) {
        recalculate_effects();
    }

    ++turn_accumulator;
    if( turn_accumulator < 60 ) {
        return;
    }
    turn_accumulator -= 60;
    poll_branch_xp();
}

bool survivor_has_perk_id( const char *id )
{
    const perk_def *perk = find_perk( id );
    return perk != nullptr && owned( *perk );
}

void survivor_reactive_event_v2( uint32_t event_id, void * )
{
    if( !character_available() ) return;
    if( event_id == NCMM_EVENT_WORLD_LOADED_V2 ) {
        set_state( "momentum_stacks", 0 );
        set_state( "momentum_turns", 0 );
        effects_dirty = true;
        recalculate_effects();
        return;
    }
    if( event_id == NCMM_EVENT_PLAYER_KILL_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) return;
        int max_stacks = survivor_has_perk_id( "cr_relentless_momentum" ) ? 5 : 3;
        if( survivor_has_perk_id( "ar_momentum_engine" ) ) max_stacks += 2;
        const int64_t raw_stacks = get_state( "momentum_stacks", 0 );
        const int64_t current_stacks = std::min<int64_t>( max_stacks,
                                       std::max<int64_t>( 0, raw_stacks ) );
        const int64_t stacks = std::min<int64_t>( max_stacks, current_stacks + 1 );
        set_state( "momentum_stacks", stacks );
        set_state( "momentum_turns", survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12 );
        if( stacks != raw_stacks ) {
            effects_dirty = true;
            recalculate_effects();
        }
        return;
    }
    if( event_id == NCMM_EVENT_TURN_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) {
            if( get_state( "momentum_stacks", 0 ) != 0 || get_state( "momentum_turns", 0 ) != 0 ) {
                set_state( "momentum_stacks", 0 );
                set_state( "momentum_turns", 0 );
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        int64_t stack_cap = survivor_has_perk_id( "cr_relentless_momentum" ) ? 5 : 3;
        if( survivor_has_perk_id( "ar_momentum_engine" ) ) stack_cap += 2;
        const int64_t duration_cap = survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12;
        const int64_t raw_stacks = get_state( "momentum_stacks", 0 );
        const int64_t raw_turns = get_state( "momentum_turns", 0 );
        int64_t stacks = std::min<int64_t>( stack_cap, std::max<int64_t>( 0, raw_stacks ) );
        int64_t remaining = std::min<int64_t>( duration_cap, std::max<int64_t>( 0, raw_turns ) );
        bool modifier_changed = stacks != raw_stacks;
        if( remaining <= 0 ) {
            modifier_changed = modifier_changed || stacks != 0;
            stacks = 0;
        }
        if( stacks != raw_stacks ) set_state( "momentum_stacks", stacks );
        if( remaining != raw_turns ) set_state( "momentum_turns", remaining );
        if( remaining <= 0 ) {
            if( modifier_changed ) {
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        --remaining;
        set_state( "momentum_turns", remaining );
        if( remaining == 0 && stacks != 0 ) {
            set_state( "momentum_stacks", 0 );
            effects_dirty = true;
            recalculate_effects();
        } else if( modifier_changed ) {
            effects_dirty = true;
            recalculate_effects();
        }
    }
}
bool configure_host_api2_runtime_hooks()
{
    if( host2 == nullptr || !host2->modifier_define || !host2->runtime_hook_bind_modifier ) return false;
    const char *dynamic_modifiers[] = {
        "mg_spell_cost_pct","mg_cast_time_pct","mg_fail_pct","mg_spell_xp_pct","mg_spell_power_pct","mg_range_pct","mg_aoe_pct","mg_duration_pct","mg_mana_max_pct","mg_mana_regen_pct","mg_spellcraft_flat","mg_melee_mana_vamp_pct","mg_virtual_hand_count",
        "mom_spell_cost_pct","mom_cast_time_pct","mom_fail_pct","mom_spell_xp_pct","mom_spell_power_pct","mom_range_pct","mom_aoe_pct","mom_duration_pct","mom_metaphysics_flat",
        "xe_spell_cost_pct","xe_cast_time_pct","xe_fail_pct","xe_spell_xp_pct","xe_spell_power_pct","xe_range_pct","xe_aoe_pct","xe_duration_pct","xe_mana_max_pct","xe_mana_regen_pct","xe_deduction_flat","xe_gramarye_flat",
        "af_spell_cost_pct","af_cast_time_pct","af_fail_pct","af_spell_xp_pct","af_spell_power_pct","af_range_pct","af_aoe_pct","af_duration_pct","af_metaphysics_flat","af_smartgun_flat",
        "afp_spell_cost_pct","afp_cast_time_pct","afp_fail_pct","afp_spell_xp_pct","afp_spell_power_pct","afp_range_pct","afp_aoe_pct","afp_duration_pct","afp_smartgun_flat",
        "secx_spell_cost_pct","secx_cast_time_pct","secx_fail_pct","secx_spell_xp_pct","secx_spell_power_pct","secx_range_pct","secx_duration_pct","secx_flesh_craft_flat","secx_flesh_combat_flat",
        "sec_damage_pct","sec_resist_pct","sec_elite_damage_pct","sec_elite_resist_pct","sec_crimson_damage_pct","sec_crimson_resist_pct"
    };
    for( const char *id : dynamic_modifiers ) if( !host2->modifier_define( module_id, id, -1000.0, 1000.0 ) ) return false;
    const char *mechanical_modifiers[] = {
        "sp_melee_crit_chance_pct", "sp_melee_crit_damage_pct", "sp_ranged_crit_damage_pct",
        "sp_damage_avoid_pct", "sp_damage_taken_pct", "sp_dodge_attempts_bonus",
        "sp_free_dodge_attempts_bonus", "sp_block_attempts_bonus",
        "sp_riposte_chance_pct", "sp_riposte_refund_pct", "sp_on_dodge_moves", "sp_on_dodge_stamina_pct",
        "sp_on_crit_moves", "sp_on_crit_stamina_pct", "sp_execute_threshold_pct", "sp_execute_damage_pct",
        "sp_damage_dealt_pct", "sp_on_kill_moves", "sp_on_kill_stamina_pct",
        "sp_craft_success_roll_flat", "sp_craft_failure_save_pct", "sp_craft_component_loss_reduction_pct",
        "sp_craft_progress_loss_reduction_pct", "sp_lockpick_roll_flat", "sp_lockpick_time_reduction_pct",
        "sp_lockpick_tool_protection_pct", "sp_lockpick_alarm_avoid_pct", "sp_trap_detection_flat"
    };
    for( const char *id : mechanical_modifiers ) if( !host2->modifier_define( module_id, id, -100.0, 100.0 ) ) return false;
    auto bind = [&]( const char *hook, uint32_t kind, const char *selector, const char *modifier ) {
        return host2->runtime_hook_bind_modifier( module_id, hook, kind, selector, modifier ) != 0;
    };
    if( !bind("combat.melee_crit_chance_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_melee_crit_chance_pct") ||
        !bind("combat.melee_crit_damage_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_melee_crit_damage_pct") ||
        !bind("combat.melee_mana_vamp_pct",NCMM_SELECTOR_ANY_V2,nullptr,"mg_melee_mana_vamp_pct") ||
        !bind("combat.ranged_crit_damage_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_ranged_crit_damage_pct") ||
        !bind("combat.damage_avoid_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_damage_avoid_pct") ||
        !bind("combat.damage_taken_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_damage_taken_pct") ||
        !bind("combat.dodge_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_dodge_attempts_bonus") ||
        !bind("combat.free_dodge_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_free_dodge_attempts_bonus") ||
        !bind("combat.block_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_block_attempts_bonus") ||
        !bind("combat.riposte_chance_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_riposte_chance_pct") ||
        !bind("combat.riposte_refund_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_riposte_refund_pct") ||
        !bind("combat.on_dodge_moves",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_dodge_moves") ||
        !bind("combat.on_dodge_stamina_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_dodge_stamina_pct") ||
        !bind("combat.on_crit_moves",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_crit_moves") ||
        !bind("combat.on_crit_stamina_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_crit_stamina_pct") ||
        !bind("combat.execute_threshold_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_execute_threshold_pct") ||
        !bind("combat.execute_damage_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_execute_damage_pct") ||
        !bind("combat.damage_dealt_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_damage_dealt_pct") ||
        !bind("combat.on_kill_moves",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_kill_moves") ||
        !bind("combat.on_kill_stamina_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_kill_stamina_pct") ||
        !bind("crafting.success_roll_flat",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_success_roll_flat") ||
        !bind("crafting.failure_save_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_failure_save_pct") ||
        !bind("crafting.component_loss_reduction_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_component_loss_reduction_pct") ||
        !bind("crafting.progress_loss_reduction_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_progress_loss_reduction_pct") ||
        !bind("scavenging.lockpick_roll_flat",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_roll_flat") ||
        !bind("scavenging.lockpick_time_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_time_reduction_pct") ||
        !bind("scavenging.lockpick_tool_protection_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_tool_protection_pct") ||
        !bind("scavenging.lockpick_alarm_avoid_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_alarm_avoid_pct") ||
        !bind("scavenging.trap_detection_flat",NCMM_SELECTOR_ANY_V2,nullptr,"sp_trap_detection_flat") ) return false;
    struct spell_source { const char *mod; const char *prefix; bool area; };
    const spell_source spell_sources[] = {
        {"magiclysm","mg",true},{"mindovermatter","mom",true},{"xedra_evolved","xe",true},
        {"aftershock_exoplanet","af",true},{"aftershock_prime","afp",true},{"secronom_lore_expansion","secx",false}
    };
    for( const spell_source &src : spell_sources ) {
        const std::string p = src.prefix;
        if( !bind("spell.cost_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_spell_cost_pct").c_str()) ||
            !bind("spell.cast_time_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_cast_time_pct").c_str()) ||
            !bind("spell.failure_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_fail_pct").c_str()) ||
            !bind("spell.experience_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_spell_xp_pct").c_str()) ||
            !bind("spell.power_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_spell_power_pct").c_str()) ||
            !bind("spell.range_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_range_pct").c_str()) ||
            !bind("spell.duration_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_duration_pct").c_str()) ) return false;
        if( src.area && !bind("spell.area_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_aoe_pct").c_str()) ) return false;
    }
    if( !bind("magic.virtual_hand_count",NCMM_SELECTOR_SOURCE_MOD_V2,"magiclysm","mg_virtual_hand_count") ||
        !bind("magic.mana.max_pct",NCMM_SELECTOR_ANY_V2,nullptr,"mg_mana_max_pct") ||
        !bind("magic.mana.max_pct",NCMM_SELECTOR_ANY_V2,nullptr,"xe_mana_max_pct") ||
        !bind("magic.mana.regen_pct",NCMM_SELECTOR_ANY_V2,nullptr,"mg_mana_regen_pct") ||
        !bind("magic.mana.regen_pct",NCMM_SELECTOR_ANY_V2,nullptr,"xe_mana_regen_pct") ||
        !bind("skill.spellcraft.flat",NCMM_SELECTOR_ANY_V2,nullptr,"mg_spellcraft_flat") ||
        !bind("skill.deduction.flat",NCMM_SELECTOR_ANY_V2,nullptr,"xe_deduction_flat") ||
        !bind("skill.gramarye.flat",NCMM_SELECTOR_ANY_V2,nullptr,"xe_gramarye_flat") ||
        !bind("skill.smartgun.flat",NCMM_SELECTOR_ANY_V2,nullptr,"af_smartgun_flat") ||
        !bind("skill.smartgun.flat",NCMM_SELECTOR_ANY_V2,nullptr,"afp_smartgun_flat") ||
        !bind("skill.secro_flesh_craft.flat",NCMM_SELECTOR_ANY_V2,nullptr,"secx_flesh_craft_flat") ||
        !bind("skill.secro_flesh_combat.flat",NCMM_SELECTOR_ANY_V2,nullptr,"secx_flesh_combat_flat") ||
        !bind("skill.metaphysics.flat",NCMM_SELECTOR_SOURCE_MOD_V2,"mindovermatter","mom_metaphysics_flat") ||
        !bind("skill.metaphysics.flat",NCMM_SELECTOR_SOURCE_MOD_V2,"aftershock_exoplanet","af_metaphysics_flat") ) return false;
    const char *base_species[] = {"SECROZED_1","SECROZED_2","SECROZED_3","SECROZED_ULTIMATE","SECROSPEC","SECROWORM","SECRODRAG","SECROSWARMER","SECROSWARMER_ALPHA","SFLESH","SFLESH_FLESHLING","SFLESH_FLESHLING_EX","SSADDLER"};
    const char *elite_species[] = {"SECROZED_2","SECROZED_3","SECROZED_ULTIMATE","SECRODRAG","SECROSWARMER_ALPHA"};
    const char *crimson_species[] = {"SFLESH","SFLESH_FLESHLING","SFLESH_FLESHLING_EX"};
    for( const char *sp : base_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_resist_pct") ) return false;
    for( const char *sp : elite_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_elite_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_elite_resist_pct") ) return false;
    for( const char *sp : crimson_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_crimson_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_crimson_resist_pct") ) return false;
    if( !host2->event_available || !host2->event_subscribe ) return false;
    if( !host2->event_available( NCMM_EVENT_TURN_V2 ) ||
        !host2->event_available( NCMM_EVENT_WORLD_LOADED_V2 ) ||
        !host2->event_available( NCMM_EVENT_PLAYER_KILL_V2 ) ) return false;
    if( !host2->event_subscribe( module_id, NCMM_EVENT_TURN_V2, &survivor_reactive_event_v2, nullptr ) ||
        !host2->event_subscribe( module_id, NCMM_EVENT_WORLD_LOADED_V2, &survivor_reactive_event_v2, nullptr ) ||
        !host2->event_subscribe( module_id, NCMM_EVENT_PLAYER_KILL_V2, &survivor_reactive_event_v2, nullptr ) ) return false;    return true;
}
int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || api->query_interface == nullptr ) return 0;
    host2 = static_cast<const ncmm_host_api_v2_core *>(
                api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 1u ) );
    if( host2 == nullptr || host2->api_major != 2u ||
        host2->api_minor < 1u || host2->virtual_item_choose == nullptr ||
        host2->virtual_item_clear == nullptr || host2->virtual_item_name == nullptr ||
        host2->virtual_item_uid == nullptr || !configure_host_api2_runtime_hooks() ) return 0;
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) {
        return 0;
    }
    if( !api->get_api_version_major || !api->get_api_version_minor ||
        api->get_api_version_major() != NCMM_API_VERSION_MAJOR ||
        api->get_api_version_minor() < NCMM_API_VERSION_MINOR ) {
        return 0;
    }
    for( const char *capability : required_caps ) {
        if( !api->has_capability || !api->has_capability( capability ) ) {
            return 0;
        }
    }
    if( !api->character_state_available || !api->character_state_get_i64 ||
        !api->character_state_set_i64 || !api->character_modifier_set ||
        !api->character_modifier_clear_module || !api->ui_choose || !api->ui_tile_choose ||
        !api->ui_card_choose || !api->ui_tree_choose ||
        !api->gameplay_metric_get_i64 || !api->world_mod_active || !api->world_mod_count || !api->world_mod_id || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_tree_choose_rpg || !api->ui_message ) {
        return 0;
    }

    host = api;
    if( !configure_progression_settings() ) {
        return 0;
    }
    last_stat_power_pct = progression_stat_power_pct();
    api->log( NCMM_LOG_INFO,
              "Survivor Progression 0.14.0 initialized: branch bars / exclusive specializations / conditional deep mod integrations." );
    return 1;
}

void shutdown()
{
    clear_runtime_modifiers();
    if( host2 != nullptr && host2->event_unsubscribe_all ) host2->event_unsubscribe_all( module_id );
    host = nullptr;
    host2 = nullptr;
    turn_accumulator = 0;
    last_character_available = false;
    effects_dirty = true;
    current_xp_bonus_pct = 0;
    last_stat_power_pct = -1;
}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    module_id,
    "Survivor Progression",
    "0.14.0",
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

extern "C" NCMM_EXPORT int ncmm_migrate_state_v1( const ncmm_host_api_v1 *api,
        uint32_t from_schema, uint32_t to_schema )
{
    if( api == nullptr || to_schema != static_cast<uint32_t>( state_schema ) ||
        from_schema > to_schema ) {
        return 0;
    }
    host = api;
    migrate_state();
    return get_state( "schema", 0 ) == state_schema ? 1 : 0;
}

extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1( const ncmm_host_api_v1 *api )
{
    if( api != nullptr ) {
        host = api;
    }
    configure_progression_settings();
}
extern "C" NCMM_EXPORT void ncmm_on_turn_v1( const ncmm_host_api_v1 *api )
{
    if( api != nullptr ) {
        host = api;
    }
    tick();
}

extern "C" NCMM_EXPORT void ncmm_open_ui_v1( const ncmm_host_api_v1 *api )
{
    if( api != nullptr ) {
        host = api;
    }
    open_progression();
}


// Diagnostic-only semantic test surface.  These exports are intentionally outside
// the NCMM runtime ABI: production Host never resolves them.  CI loads the exact
// release DLL and uses them to prove every perk definition reaches the normal
// state/recalculation path instead of testing a duplicated model.
extern "C" NCMM_EXPORT size_t ncmm_test_perk_count_v1()
{
    return sizeof( perks ) / sizeof( perks[0] );
}

extern "C" NCMM_EXPORT const char *ncmm_test_perk_id_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? perks[index].id : nullptr;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_branch_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? static_cast<int>( perks[index].branch ) : -1;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_currency_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? static_cast<int>( perks[index].currency ) : -1;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_kind_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ?
           static_cast<int>( effective_kind( perks[index] ) ) : -1;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_scaling_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ?
           static_cast<int>( perks[index].scaling ) : -1;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_integration_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() && integration_perk( perks[index] ) ? 1 : 0;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_max_rank_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? perk_max_rank( perks[index] ) : 0;
}

extern "C" NCMM_EXPORT double ncmm_test_perk_rank_multiplier_v1( size_t index, int rank )
{
    return index < ncmm_test_perk_count_v1() ?
           perk_rank_multiplier_for( perks[index], rank ) : 0.0;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_effect_count_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? perks[index].effect_count : 0;
}

extern "C" NCMM_EXPORT const char *ncmm_test_perk_effect_id_v1( size_t index, int effect_index )
{
    if( index >= ncmm_test_perk_count_v1() || effect_index < 0 ||
        effect_index >= perks[index].effect_count ) {
        return nullptr;
    }
    return perks[index].effects[effect_index].id;
}

extern "C" NCMM_EXPORT double ncmm_test_perk_effect_value_v1( size_t index, int effect_index )
{
    if( index >= ncmm_test_perk_count_v1() || effect_index < 0 ||
        effect_index >= perks[index].effect_count ) {
        return 0.0;
    }
    return perks[index].effects[effect_index].value;
}

extern "C" NCMM_EXPORT int ncmm_test_perk_xp_bonus_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? perks[index].xp_bonus_pct : 0;
}

extern "C" NCMM_EXPORT double ncmm_test_perk_branch_amp_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? perks[index].branch_amp_pct : 0.0;
}

extern "C" NCMM_EXPORT double ncmm_test_perk_global_amp_v1( size_t index )
{
    return index < ncmm_test_perk_count_v1() ? perks[index].global_amp_pct : 0.0;
}

extern "C" NCMM_EXPORT int ncmm_test_reset_all_perks_v1()
{
    if( !character_available() ) {
        return 0;
    }
    clear_mana_hand_slots();
    for( const perk_def &perk : perks ) {
        set_state( perk_key( perk ), 0 );
    }
    const char *state_keys[] = {
        "spec_combat", "spec_survival", "spec_mobility", "spec_crafting",
        "spec_scavenging", "spec_mastery",
        "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
        "prime_aftershock_exoplanet", "prime_aftershock_prime",
        "prime_secronom", "prime_secronom_plus",
        "momentum_stacks", "momentum_turns"
    };
    for( const char *key : state_keys ) {
        set_state( key, 0 );
    }
    effects_dirty = true;
    recalculate_effects();
    return 1;
}

extern "C" NCMM_EXPORT int ncmm_test_set_perk_rank_v1( size_t index, int rank )
{
    if( index >= ncmm_test_perk_count_v1() || !character_available() ) {
        return 0;
    }
    const perk_def &perk = perks[index];
    const int bounded = std::max( 0, std::min( perk_max_rank( perk ), rank ) );
    set_state( perk_key( perk ), bounded );
    effects_dirty = true;
    recalculate_effects();
    return perk_rank( perk ) == bounded ? 1 : 0;
}

extern "C" NCMM_EXPORT int ncmm_test_recalculate_v1()
{
    if( !character_available() ) {
        return 0;
    }
    effects_dirty = true;
    recalculate_effects();
    return effects_dirty ? 0 : 1;
}

extern "C" NCMM_EXPORT int ncmm_test_current_xp_bonus_v1()
{
    return current_xp_bonus_pct;
}

extern "C" NCMM_EXPORT void ncmm_test_dispatch_event_v1( uint32_t event_id )
{
    survivor_reactive_event_v2( event_id, nullptr );
}
