#define NCMM_MOD_BUILD
#include "ncmm_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char *const module_id = "survivor_progression";
constexpr int state_schema = 4;

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
    "module_hotkeys.context.v1",
    "module_hotkeys.v1",
    "api.versioning.v1",
    "state.migration.v1",
    "module.lifecycle.v1"
};

const ncmm_host_api_v1 *host = nullptr;
int turn_accumulator = 0;
bool last_character_available = false;
bool effects_dirty = true;
int current_xp_bonus_pct = 0;

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
    { "c_footwork", branch_id::combat, 1, 1, currency_id::perk, "", "", "Combat Footwork", "Боевая работа ног", "+0.5 dodge", "+0,5 уклонения", {{ { "dodge_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_precision", branch_id::combat, 2, 5, currency_id::perk, "c_power", "", "Precision", "Точность", "+0.5 melee hit", "+0,5 точности в ближнем бою", {{ { "melee_hit_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_reflexes", branch_id::combat, 2, 5, currency_id::perk, "c_footwork", "", "Reflex Drills", "Тренировка рефлексов", "+1 Dexterity", "+1 к ловкости", {{ { "dex_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_conditioning", branch_id::combat, 3, 10, currency_id::perk, "c_precision", "", "Combat Conditioning", "Боевая выносливость", "+8% maximum stamina", "+8% к максимуму выносливости", {{ { "stamina_max_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_tempo", branch_id::combat, 3, 10, currency_id::perk, "c_reflexes", "", "Battle Tempo", "Темп боя", "+3% speed", "+3% к скорости", {{ { "speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "c_bruiser", branch_id::combat, 4, 15, currency_id::perk, "c_conditioning", "", "Bruiser", "Громила", "+1 Strength, +0.5 melee hit", "+1 сила, +0,5 точности", {{ { "str_flat", 1 }, { "melee_hit_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "c_evasion", branch_id::combat, 4, 15, currency_id::perk, "c_tempo", "", "Evasive Fighter", "Уклончивый боец", "+0.75 dodge, -3% move cost", "+0,75 уклонения, -3% стоимости движения", {{ { "dodge_flat", 0.75 }, { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "c_veteran", branch_id::combat, 5, 20, currency_id::major, "c_bruiser", "c_evasion", "Veteran Fighter", "Ветеран", "+1 STR, +1 DEX, +0.5 melee hit", "+1 сила, +1 ловкость, +0,5 точности", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { "melee_hit_flat", 0.5 }, { nullptr, 0.0 } }}, 3, 0 },
    { "c_apex", branch_id::combat, 6, 30, currency_id::major, "c_veteran", "", "Apex Combatant", "Вершина боя", "+5% speed, +1 dodge, +10% stamina, +0.5 hit", "+5% скорость, +1 уклонение, +10% выносливость, +0,5 точность", {{ { "speed_pct", 5 }, { "dodge_flat", 1 }, { "stamina_max_pct", 10 }, { "melee_hit_flat", 0.5 } }}, 4, 0 },
    { "s_hardy", branch_id::survival, 1, 1, currency_id::perk, "", "", "Hardy", "Закалённый", "+6% maximum stamina", "+6% к максимуму выносливости", {{ { "stamina_max_pct", 6 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_field", branch_id::survival, 1, 1, currency_id::perk, "", "", "Field Medicine", "Полевая медицина", "+10% natural healing", "+10% естественного лечения", {{ { "healing_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_pack", branch_id::survival, 2, 5, currency_id::perk, "s_hardy", "", "Pack Discipline", "Грамотная укладка", "+10% carrying capacity", "+10% грузоподъёмности", {{ { "carry_weight_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_resilient", branch_id::survival, 2, 5, currency_id::perk, "s_field", "", "Resilient Body", "Живучий организм", "+15% natural healing", "+15% естественного лечения", {{ { "healing_pct", 15 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_endurance", branch_id::survival, 3, 10, currency_id::perk, "s_pack", "", "Long Haul", "Долгий путь", "+10% maximum stamina", "+10% к максимуму выносливости", {{ { "stamina_max_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_instinct", branch_id::survival, 3, 10, currency_id::perk, "s_resilient", "", "Survival Instinct", "Инстинкт выживания", "+1 Perception", "+1 к восприятию", {{ { "per_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "s_ironback", branch_id::survival, 4, 15, currency_id::perk, "s_endurance", "", "Iron Back", "Железная спина", "+15% carry, +1 Strength", "+15% грузоподъёмности, +1 сила", {{ { "carry_weight_pct", 15 }, { "str_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "s_recovery", branch_id::survival, 4, 15, currency_id::perk, "s_instinct", "", "Rapid Recovery", "Быстрое восстановление", "+20% healing, +5% maximum stamina", "+20% лечение, +5% максимум выносливости", {{ { "healing_pct", 20 }, { "stamina_max_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "s_survivor", branch_id::survival, 5, 20, currency_id::major, "s_ironback", "s_recovery", "True Survivor", "Настоящий выживший", "+15% stamina, +10% carry, +15% healing", "+15% выносливость, +10% грузоподъёмность, +15% лечение", {{ { "stamina_max_pct", 15 }, { "carry_weight_pct", 10 }, { "healing_pct", 15 }, { nullptr, 0.0 } }}, 3, 0 },
    { "s_unbreakable", branch_id::survival, 6, 30, currency_id::major, "s_survivor", "", "Unbreakable", "Несломленный", "+15% stamina, +20% healing, +1 STR, +10% carry", "+15% выносливость, +20% лечение, +1 сила, +10% грузоподъёмность", {{ { "stamina_max_pct", 15 }, { "healing_pct", 20 }, { "str_flat", 1 }, { "carry_weight_pct", 10 } }}, 4, 0 },
    { "m_light", branch_id::mobility, 1, 1, currency_id::perk, "", "", "Light Step", "Лёгкий шаг", "-3% move cost", "-3% стоимости движения", {{ { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_cardio", branch_id::mobility, 1, 1, currency_id::perk, "", "", "Cardio Base", "Кардиобаза", "+6% maximum stamina", "+6% к максимуму выносливости", {{ { "stamina_max_pct", 6 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_stride", branch_id::mobility, 2, 5, currency_id::perk, "m_light", "", "Efficient Stride", "Эффективный шаг", "+3% speed", "+3% к скорости", {{ { "speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_breath", branch_id::mobility, 2, 5, currency_id::perk, "m_cardio", "", "Deep Reserve", "Глубокий резерв", "+8% maximum stamina", "+8% к максимуму выносливости", {{ { "stamina_max_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_parkour", branch_id::mobility, 3, 10, currency_id::perk, "m_stride", "", "Parkour Habit", "Привычка к паркуру", "-5% move cost, +1 Dexterity", "-5% стоимости движения, +1 ловкость", {{ { "move_cost_pct", -5 }, { "dex_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "m_quick", branch_id::mobility, 3, 10, currency_id::perk, "m_breath", "", "Quick Recovery", "Второе дыхание", "+4% speed", "+4% к скорости", {{ { "speed_pct", 4 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "m_runner", branch_id::mobility, 4, 15, currency_id::perk, "m_parkour", "", "Runner", "Бегун", "+5% speed, -3% move cost", "+5% скорость, -3% стоимость движения", {{ { "speed_pct", 5 }, { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "m_marathon", branch_id::mobility, 4, 15, currency_id::perk, "m_quick", "", "Marathoner", "Марафонец", "+12% maximum stamina, -2% move cost", "+12% выносливость, -2% стоимость движения", {{ { "stamina_max_pct", 12 }, { "move_cost_pct", -2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "m_flow", branch_id::mobility, 5, 20, currency_id::major, "m_runner", "m_marathon", "Flow State", "Состояние потока", "+5% speed, +1 DEX, -5% move cost", "+5% скорость, +1 ловкость, -5% стоимость движения", {{ { "speed_pct", 5 }, { "dex_flat", 1 }, { "move_cost_pct", -5 }, { nullptr, 0.0 } }}, 3, 0 },
    { "m_untouchable", branch_id::mobility, 6, 30, currency_id::major, "m_flow", "", "Untouchable", "Неуловимый", "+7% speed, +1 dodge, -5% move cost, +8% stamina", "+7% скорость, +1 уклонение, -5% движение, +8% выносливость", {{ { "speed_pct", 7 }, { "dodge_flat", 1 }, { "move_cost_pct", -5 }, { "stamina_max_pct", 8 } }}, 4, 0 },
    { "f_hands", branch_id::crafting, 1, 1, currency_id::perk, "", "", "Practiced Hands", "Набитая рука", "+5% crafting speed", "+5% скорости крафта", {{ { "craft_speed_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_reader", branch_id::crafting, 1, 1, currency_id::perk, "", "", "Focused Reading", "Сосредоточенное чтение", "+10% reading speed", "+10% скорости чтения", {{ { "read_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_efficiency", branch_id::crafting, 2, 5, currency_id::perk, "f_hands", "", "Workshop Rhythm", "Ритм мастерской", "+8% crafting speed", "+8% скорости крафта", {{ { "craft_speed_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_study", branch_id::crafting, 2, 5, currency_id::perk, "f_reader", "", "Study Habit", "Привычка учиться", "+1 Intelligence", "+1 к интеллекту", {{ { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_workflow", branch_id::crafting, 3, 10, currency_id::perk, "f_efficiency", "", "Efficient Workflow", "Эффективный процесс", "+10% crafting speed", "+10% скорости крафта", {{ { "craft_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_quickstudy", branch_id::crafting, 3, 10, currency_id::perk, "f_study", "", "Quick Study", "Быстрое обучение", "+15% reading speed", "+15% скорости чтения", {{ { "read_speed_pct", 15 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "f_engineer", branch_id::crafting, 4, 15, currency_id::perk, "f_workflow", "", "Engineer", "Инженер", "+10% crafting speed, +1 INT", "+10% крафт, +1 интеллект", {{ { "craft_speed_pct", 10 }, { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "f_scholar", branch_id::crafting, 4, 15, currency_id::perk, "f_quickstudy", "", "Scholar", "Учёный", "+20% reading speed, +1 INT", "+20% чтение, +1 интеллект", {{ { "read_speed_pct", 20 }, { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "f_master", branch_id::crafting, 5, 20, currency_id::major, "f_engineer", "f_scholar", "Master Artisan", "Мастер", "+15% craft, +10% read, +1 INT", "+15% крафт, +10% чтение, +1 интеллект", {{ { "craft_speed_pct", 15 }, { "read_speed_pct", 10 }, { "int_flat", 1 }, { nullptr, 0.0 } }}, 3, 0 },
    { "f_genius", branch_id::crafting, 6, 30, currency_id::major, "f_master", "", "Technical Genius", "Технический гений", "+20% craft, +20% read, +1 INT, +10% carry", "+20% крафт, +20% чтение, +1 интеллект, +10% грузоподъёмность", {{ { "craft_speed_pct", 20 }, { "read_speed_pct", 20 }, { "int_flat", 1 }, { "carry_weight_pct", 10 } }}, 4, 0 },
    { "g_observer", branch_id::scavenging, 1, 1, currency_id::perk, "", "", "Observer", "Наблюдатель", "+1 Perception", "+1 к восприятию", {{ { "per_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_hauler", branch_id::scavenging, 1, 1, currency_id::perk, "", "", "Hauler", "Носильщик", "+10% carrying capacity", "+10% грузоподъёмности", {{ { "carry_weight_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_route", branch_id::scavenging, 2, 5, currency_id::perk, "g_observer", "", "Route Sense", "Чувство маршрута", "-3% move cost", "-3% стоимости движения", {{ { "move_cost_pct", -3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_pack", branch_id::scavenging, 2, 5, currency_id::perk, "g_hauler", "", "Pack Expert", "Эксперт по укладке", "+10% carrying capacity", "+10% грузоподъёмности", {{ { "carry_weight_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_awareness", branch_id::scavenging, 3, 10, currency_id::perk, "g_route", "", "Situational Awareness", "Ситуационная осведомлённость", "+1 PER, +2% speed", "+1 восприятие, +2% скорость", {{ { "per_flat", 1 }, { "speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "g_endurance", branch_id::scavenging, 3, 10, currency_id::perk, "g_pack", "", "Loaded March", "Марш с грузом", "+7% maximum stamina", "+7% к максимуму выносливости", {{ { "stamina_max_pct", 7 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "g_pathfinder", branch_id::scavenging, 4, 15, currency_id::perk, "g_awareness", "", "Pathfinder", "Следопыт", "-5% move cost, +1 PER", "-5% движение, +1 восприятие", {{ { "move_cost_pct", -5 }, { "per_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "g_mule", branch_id::scavenging, 4, 15, currency_id::perk, "g_endurance", "", "Human Mule", "Вьючный человек", "+15% carry, +1 STR", "+15% грузоподъёмность, +1 сила", {{ { "carry_weight_pct", 15 }, { "str_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "g_raider", branch_id::scavenging, 5, 20, currency_id::major, "g_pathfinder", "g_mule", "Veteran Scavenger", "Опытный добытчик", "+1 PER, +15% carry, +3% speed", "+1 восприятие, +15% грузоподъёмность, +3% скорость", {{ { "per_flat", 1 }, { "carry_weight_pct", 15 }, { "speed_pct", 3 }, { nullptr, 0.0 } }}, 3, 0 },
    { "g_legend", branch_id::scavenging, 6, 30, currency_id::major, "g_raider", "", "Wasteland Scavenger", "Легенда пустошей", "+1 PER, +20% carry, -5% move cost, +3% speed", "+1 восприятие, +20% грузоподъёмность, -5% движение, +3% скорость", {{ { "per_flat", 1 }, { "carry_weight_pct", 20 }, { "move_cost_pct", -5 }, { "speed_pct", 3 } }}, 4, 0 },
    { "a_fast", branch_id::mastery, 1, 1, currency_id::perk, "", "", "Fast Learner", "Быстрый ученик", "+100% Survivor XP", "+100% опыта Survivor", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 100 },
    { "a_focus", branch_id::mastery, 1, 1, currency_id::perk, "", "", "Focused Mind", "Собранный ум", "+1 Intelligence", "+1 к интеллекту", {{ { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0 },
    { "a_adapt", branch_id::mastery, 2, 5, currency_id::perk, "a_fast", "", "Adaptive Learning", "Адаптивное обучение", "+25% Survivor XP", "+25% опыта Survivor", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 25 },
    { "a_balance", branch_id::mastery, 2, 5, currency_id::perk, "a_focus", "", "Balanced Growth", "Сбалансированное развитие", "+1 STR, +1 DEX", "+1 сила, +1 ловкость", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "a_learning", branch_id::mastery, 3, 10, currency_id::perk, "a_adapt", "", "Learning Loop", "Цикл обучения", "+25% Survivor XP, +5% crafting", "+25% опыта Survivor, +5% крафта", {{ { "craft_speed_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 25 },
    { "a_insight", branch_id::mastery, 3, 10, currency_id::perk, "a_balance", "", "Insight", "Проницательность", "+1 PER, +1 INT", "+1 восприятие, +1 интеллект", {{ { "per_flat", 1 }, { "int_flat", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "a_growth", branch_id::mastery, 4, 15, currency_id::perk, "a_learning", "", "Accelerated Growth", "Ускоренный рост", "+25% Survivor XP, +5% stamina", "+25% опыта Survivor, +5% выносливость", {{ { "stamina_max_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 25 },
    { "a_polymath", branch_id::mastery, 4, 15, currency_id::perk, "a_insight", "", "Polymath", "Универсал", "+10% craft, +10% reading", "+10% крафт, +10% чтение", {{ { "craft_speed_pct", 10 }, { "read_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0 },
    { "a_paragon", branch_id::mastery, 5, 20, currency_id::major, "a_growth", "a_polymath", "Paragon", "Образец", "+1 STR, +1 DEX, +1 PER, +1 INT", "+1 ко всем основным характеристикам", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { "per_flat", 1 }, { "int_flat", 1 } }}, 4, 0 },
    { "a_transcendent", branch_id::mastery, 6, 30, currency_id::major, "a_paragon", "", "Transcendent Survivor", "Совершенный выживший", "+50% XP, +3% speed, +10% stamina, +10% healing", "+50% опыта, +3% скорость, +10% выносливость, +10% лечение", {{ { "speed_pct", 3 }, { "stamina_max_pct", 10 }, { "healing_pct", 10 }, { nullptr, 0.0 } }}, 3, 50 }
,
    { "ce_rhythm", branch_id::combat, 1, 3, currency_id::perk, "", "", "Combat Rhythm", "Боевой ритм", "Combat stat perks are 5% stronger.", "Статовые боевые перки на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "ce_drills", branch_id::combat, 1, 6, currency_id::perk, "ce_rhythm", "", "Drilled Reflexes", "Отработанные рефлексы", "+0.25 dodge and +0.25 melee hit.", "+0,25 уклонения и +0,25 точности ближнего боя.", {{ { "dodge_flat", 0.25 }, { "melee_hit_flat", 0.25 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ce_reserve", branch_id::combat, 2, 9, currency_id::perk, "ce_drills", "", "Reserve Under Fire", "Резерв под огнём", "+2% max stamina per active Survivor branch.", "+2% максимума выносливости за каждую активную ветку Survivor.", {{ { "stamina_max_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ce_lessons", branch_id::combat, 2, 12, currency_id::perk, "ce_reserve", "", "Lessons of Violence", "Уроки боя", "+4% Survivor XP per owned major perk.", "+4% опыта Survivor за каждый купленный большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 4, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ce_tactics", branch_id::combat, 3, 15, currency_id::major, "ce_lessons", "", "Tactical Integration", "Тактическая интеграция", "Combat stat perks are another 10% stronger.", "Статовые боевые перки ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "ce_pressure", branch_id::combat, 3, 18, currency_id::perk, "ce_tactics", "", "Relentless Pressure", "Непрерывный натиск", "+1% speed per active Survivor branch.", "+1% скорости за каждую активную ветку Survivor.", {{ { "speed_pct", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ce_memory", branch_id::combat, 4, 22, currency_id::perk, "ce_pressure", "", "Battle Memory", "Боевая память", "+3% Survivor XP per active Survivor branch.", "+3% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ce_refined", branch_id::combat, 4, 26, currency_id::perk, "ce_memory", "", "Refined Drills", "Отточенная подготовка", "+0.5 melee hit and +0.5 dodge.", "+0,5 точности ближнего боя и +0,5 уклонения.", {{ { "melee_hit_flat", 0.5 }, { "dodge_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ce_veteran_reflex", branch_id::combat, 5, 32, currency_id::perk, "ce_refined", "", "Veteran Reflex", "Рефлекс ветерана", "+3% speed, +5% max stamina, +0.25 dodge.", "+3% скорости, +5% выносливости, +0,25 уклонения.", {{ { "speed_pct", 3 }, { "stamina_max_pct", 5 }, { "dodge_flat", 0.25 }, { nullptr, 0.0 } }}, 3, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ce_warmaster", branch_id::combat, 6, 40, currency_id::major, "ce_veteran_reflex", "", "Warmaster", "Воевода", "All stat perks are 5% stronger.", "Все статовые перки на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "se_lessons", branch_id::survival, 1, 3, currency_id::perk, "", "", "Hard Lessons", "Тяжёлые уроки", "Survival stat perks are 5% stronger.", "Статовые перки выживания на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "se_routine", branch_id::survival, 1, 6, currency_id::perk, "se_lessons", "", "Survival Routine", "Режим выживания", "+5% healing per active Survivor branch.", "+5% лечения за каждую активную ветку Survivor.", {{ { "healing_pct", 5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_reserves", branch_id::survival, 2, 9, currency_id::perk, "se_routine", "", "Deep Reserves", "Глубокие резервы", "+2% max stamina per active Survivor branch.", "+2% выносливости за каждую активную ветку Survivor.", {{ { "stamina_max_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_adaptive", branch_id::survival, 2, 12, currency_id::perk, "se_reserves", "", "Adaptive Survivor", "Адаптивный выживший", "+4% Survivor XP per owned major perk.", "+4% опыта Survivor за каждый большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 4, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "se_anchor", branch_id::survival, 3, 15, currency_id::major, "se_adaptive", "", "Anchor Point", "Точка опоры", "Survival stat perks are another 10% stronger.", "Статовые перки выживания ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "se_memory", branch_id::survival, 3, 18, currency_id::perk, "se_anchor", "", "Long Memory", "Долгая память", "+3% carry capacity per active Survivor branch.", "+3% грузоподъёмности за каждую активную ветку.", {{ { "carry_weight_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_hardened", branch_id::survival, 4, 22, currency_id::perk, "se_memory", "", "Hardened Practice", "Закалённая практика", "+4% healing per owned major perk.", "+4% лечения за каждый купленный большой перк.", {{ { "healing_pct", 4 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "se_grit", branch_id::survival, 4, 26, currency_id::perk, "se_hardened", "", "Grit", "Стойкость", "+10% healing and +8% max stamina.", "+10% лечения и +8% максимума выносливости.", {{ { "healing_pct", 10 }, { "stamina_max_pct", 8 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "se_carried", branch_id::survival, 5, 32, currency_id::perk, "se_grit", "", "Lessons Carried", "Накопленный опыт", "+3% Survivor XP per active Survivor branch.", "+3% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "se_indomitable", branch_id::survival, 6, 40, currency_id::major, "se_carried", "", "Indomitable", "Несгибаемый", "All stat perks are 5% stronger.", "Все статовые перки на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "me_economy", branch_id::mobility, 1, 3, currency_id::perk, "", "", "Motion Economy", "Экономия движения", "Mobility stat perks are 5% stronger.", "Статовые перки мобильности на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "me_practice", branch_id::mobility, 1, 6, currency_id::perk, "me_economy", "", "Kinetic Practice", "Кинетическая практика", "-1% move cost per active Survivor branch.", "-1% стоимости движения за каждую активную ветку.", {{ { "move_cost_pct", -1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_breath", branch_id::mobility, 2, 9, currency_id::perk, "me_practice", "", "Breath Cycle", "Цикл дыхания", "+2% max stamina per active Survivor branch.", "+2% выносливости за каждую активную ветку.", {{ { "stamina_max_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_road", branch_id::mobility, 2, 12, currency_id::perk, "me_breath", "", "Road Sense", "Чувство дороги", "+3% Survivor XP per active Survivor branch.", "+3% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_flow", branch_id::mobility, 3, 15, currency_id::major, "me_road", "", "Flow Control", "Контроль потока", "Mobility stat perks are another 10% stronger.", "Статовые перки мобильности ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "me_stride", branch_id::mobility, 3, 18, currency_id::perk, "me_flow", "", "Long Stride", "Длинный шаг", "+1% speed per active Survivor branch.", "+1% скорости за каждую активную ветку.", {{ { "speed_pct", 1 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_mastery", branch_id::mobility, 4, 22, currency_id::perk, "me_stride", "", "Kinetic Mastery", "Мастерство движения", "-0.5% move cost per owned major perk.", "-0,5% стоимости движения за каждый большой перк.", {{ { "move_cost_pct", -0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "me_feather", branch_id::mobility, 4, 26, currency_id::perk, "me_mastery", "", "Featherstep", "Невесомый шаг", "+0.5 dodge and +2% speed.", "+0,5 уклонения и +2% скорости.", {{ { "dodge_flat", 0.5 }, { "speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "me_endless", branch_id::mobility, 5, 32, currency_id::perk, "me_feather", "", "Endless Road", "Бесконечная дорога", "+3% Survivor XP per active Survivor branch.", "+3% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "me_horizon", branch_id::mobility, 6, 40, currency_id::major, "me_endless", "", "Horizon Runner", "Бегущий к горизонту", "All stat perks are 5% stronger.", "Все статовые перки на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "fe_iterate", branch_id::crafting, 1, 3, currency_id::perk, "", "", "Iterative Practice", "Практика итераций", "Crafting stat perks are 5% stronger.", "Статовые перки крафта на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "fe_method", branch_id::crafting, 1, 6, currency_id::perk, "fe_iterate", "", "Methodical Work", "Методичная работа", "+3% crafting speed per active Survivor branch.", "+3% скорости крафта за каждую активную ветку.", {{ { "craft_speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "fe_notes", branch_id::crafting, 2, 9, currency_id::perk, "fe_method", "", "Living Notes", "Живые заметки", "+3% reading speed per active Survivor branch.", "+3% скорости чтения за каждую активную ветку.", {{ { "read_speed_pct", 3 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "fe_learning", branch_id::crafting, 2, 12, currency_id::perk, "fe_notes", "", "Learning by Making", "Учёба делом", "+4% Survivor XP per owned major perk.", "+4% опыта Survivor за каждый большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 4, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "fe_breakthrough", branch_id::crafting, 3, 15, currency_id::major, "fe_learning", "", "Breakthrough", "Прорыв", "Crafting stat perks are another 10% stronger.", "Статовые перки крафта ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "fe_standard", branch_id::crafting, 3, 18, currency_id::perk, "fe_breakthrough", "", "Standardized Process", "Стандартизация", "+2% crafting speed per owned major perk.", "+2% скорости крафта за каждый большой перк.", {{ { "craft_speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "fe_systems", branch_id::crafting, 4, 22, currency_id::perk, "fe_standard", "", "Systems Thinking", "Системное мышление", "+2% reading speed per owned major perk.", "+2% скорости чтения за каждый большой перк.", {{ { "read_speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "fe_theory", branch_id::crafting, 4, 26, currency_id::perk, "fe_systems", "", "Theory Into Practice", "Теория в практике", "+10% crafting and +10% reading speed.", "+10% скорости крафта и +10% скорости чтения.", {{ { "craft_speed_pct", 10 }, { "read_speed_pct", 10 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "fe_tuning", branch_id::crafting, 5, 32, currency_id::perk, "fe_theory", "", "Fine Tuning", "Тонкая настройка", "+3% Survivor XP per active Survivor branch.", "+3% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "fe_architect", branch_id::crafting, 6, 40, currency_id::major, "fe_tuning", "", "Architect Mind", "Разум архитектора", "All stat perks are 5% stronger.", "Все статовые перки на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ge_eye", branch_id::scavenging, 1, 3, currency_id::perk, "", "", "Sharp Eye", "Острый глаз", "Scavenging stat perks are 5% stronger.", "Статовые перки добычи на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 5, 0 },
    { "ge_routes", branch_id::scavenging, 1, 6, currency_id::perk, "ge_eye", "", "Route Discipline", "Дисциплина маршрута", "-0.75% move cost per active Survivor branch.", "-0,75% стоимости движения за каждую активную ветку.", {{ { "move_cost_pct", -0.75 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_load", branch_id::scavenging, 2, 9, currency_id::perk, "ge_routes", "", "Load Planning", "Планирование груза", "+4% carry capacity per active Survivor branch.", "+4% грузоподъёмности за каждую активную ветку.", {{ { "carry_weight_pct", 4 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_field", branch_id::scavenging, 2, 12, currency_id::perk, "ge_load", "", "Field Experience", "Полевой опыт", "+3% Survivor XP per active Survivor branch.", "+3% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_opportunist", branch_id::scavenging, 3, 15, currency_id::major, "ge_field", "", "Opportunist", "Оппортунист", "Scavenging stat perks are another 10% stronger.", "Статовые перки добычи ещё на 10% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 10, 0 },
    { "ge_cache", branch_id::scavenging, 3, 18, currency_id::perk, "ge_opportunist", "", "Cache Logic", "Логика тайников", "+0.25 PER per active Survivor branch.", "+0,25 восприятия за каждую активную ветку.", {{ { "per_flat", 0.25 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ge_network", branch_id::scavenging, 4, 22, currency_id::perk, "ge_cache", "", "Networked Routes", "Сеть маршрутов", "+2% speed per owned major perk.", "+2% скорости за каждый большой перк.", {{ { "speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 1, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ge_instinct", branch_id::scavenging, 4, 26, currency_id::perk, "ge_network", "", "Scavenger Instinct", "Инстинкт добытчика", "+10% carry capacity and +0.5 PER.", "+10% грузоподъёмности и +0,5 восприятия.", {{ { "carry_weight_pct", 10 }, { "per_flat", 0.5 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::fixed, 0, 0 },
    { "ge_wisdom", branch_id::scavenging, 5, 32, currency_id::perk, "ge_instinct", "", "Long Haul Wisdom", "Мудрость дальних рейдов", "+3% Survivor XP per owned major perk.", "+3% опыта Survivor за каждый большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ge_nomad", branch_id::scavenging, 6, 40, currency_id::major, "ge_wisdom", "", "Nomad Legend", "Легенда кочевника", "All stat perks are 5% stronger.", "Все статовые перки на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ae_reflect", branch_id::mastery, 1, 3, currency_id::perk, "", "", "Reflection", "Рефлексия", "All stat perks are 2% stronger.", "Все статовые перки на 2% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 2 },
    { "ae_cross", branch_id::mastery, 1, 6, currency_id::perk, "ae_reflect", "", "Cross Training", "Перекрёстная подготовка", "+5% Survivor XP per active Survivor branch.", "+5% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 5, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_foundation", branch_id::mastery, 2, 9, currency_id::perk, "ae_cross", "", "Strong Foundation", "Прочный фундамент", "+0.15 STR/DEX/PER/INT per active branch.", "+0,15 СИЛ/ЛОВ/ВОС/ИНТ за каждую активную ветку.", {{ { "str_flat", 0.15 }, { "dex_flat", 0.15 }, { "per_flat", 0.15 }, { "int_flat", 0.15 } }}, 4, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_pattern", branch_id::mastery, 2, 12, currency_id::perk, "ae_foundation", "", "Pattern Recognition", "Распознавание закономерностей", "+3% Survivor XP per owned major perk.", "+3% опыта Survivor за каждый большой перк.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 3, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ae_milestone", branch_id::mastery, 3, 15, currency_id::major, "ae_pattern", "", "Milestone Discipline", "Дисциплина рубежей", "All stat perks are another 5% stronger.", "Все статовые перки ещё на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ae_integrate", branch_id::mastery, 3, 18, currency_id::perk, "ae_milestone", "", "Integration", "Интеграция", "+2% crafting and reading speed per active branch.", "+2% крафта и чтения за каждую активную ветку.", {{ { "craft_speed_pct", 2 }, { "read_speed_pct", 2 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 2, 0, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_compound", branch_id::mastery, 4, 22, currency_id::perk, "ae_integrate", "", "Compounding Practice", "Накопительная практика", "All stat perks are another 5% stronger.", "Все статовые перки ещё на 5% сильнее.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 0, perk_kind::effect, perk_scaling::fixed, 0, 5 },
    { "ae_longgame", branch_id::mastery, 4, 26, currency_id::perk, "ae_compound", "", "Long Game", "Долгая игра", "+6% Survivor XP per active Survivor branch.", "+6% опыта Survivor за каждую активную ветку.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 6, perk_kind::effect, perk_scaling::per_active_branch, 0, 0 },
    { "ae_legacy", branch_id::mastery, 5, 32, currency_id::perk, "ae_longgame", "", "Legacy Mindset", "Мышление наследия", "+0.05 STR/DEX/PER/INT per owned major perk.", "+0,05 СИЛ/ЛОВ/ВОС/ИНТ за каждый большой перк.", {{ { "str_flat", 0.05 }, { "dex_flat", 0.05 }, { "per_flat", 0.05 }, { "int_flat", 0.05 } }}, 4, 0, perk_kind::effect, perk_scaling::per_owned_major, 0, 0 },
    { "ae_ascendant", branch_id::mastery, 6, 40, currency_id::major, "ae_legacy", "", "Ascendant", "Восхождение", "All stat perks are 10% stronger and Survivor XP +50%.", "Все статовые перки на 10% сильнее, опыт Survivor +50%.", {{ { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }}, 0, 50, perk_kind::effect, perk_scaling::fixed, 0, 10 },
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

std::string perk_key( const perk_def &perk )
{
    return std::string( "p_" ) + perk.id;
}

bool owned( const perk_def &perk )
{
    return get_state( perk_key( perk ), 0 ) != 0;
}

const perk_def *find_perk( const char *id )
{
    if( id == nullptr || *id == '\0' ) {
        return nullptr;
    }
    for( const perk_def &perk : perks ) {
        if( std::string( perk.id ) == id ) {
            return &perk;
        }
    }
    return nullptr;
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
            return tr( "XP / synergy / global growth", "Опыт / синергия / общий рост" );
    }
    return {};
}

int branch_owned_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}

int branch_total_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch ) {
            ++result;
        }
    }
    return result;
}

int owned_count( currency_id currency )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.currency == currency && owned( perk ) ) {
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
    return id;
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
    const std::array<branch_id, 6> branches = {
        branch_id::combat, branch_id::survival, branch_id::mobility,
        branch_id::crafting, branch_id::scavenging, branch_id::mastery
    };

    for( branch_id branch : branches ) {
        if( branch_owned_count( branch ) > 0 ) {
            ++result.active_branches;
        }
    }
    result.major_owned = owned_count( currency_id::major );

    for( const perk_def &perk : perks ) {
        if( !owned( perk ) || effective_kind( perk ) != perk_kind::effect ) {
            continue;
        }
        result.branch_amp[branch_index( perk.branch )] += perk.branch_amp_pct / 100.0;
        result.global_amp += perk.global_amp_pct / 100.0;
    }

    for( const perk_def &perk : perks ) {
        if( !owned( perk ) ) {
            continue;
        }

        double scale = 1.0;
        if( perk.scaling == perk_scaling::per_active_branch ) {
            scale = static_cast<double>( result.active_branches );
        } else if( perk.scaling == perk_scaling::per_owned_major ) {
            scale = static_cast<double>( result.major_owned );
        }

        if( effective_kind( perk ) == perk_kind::stat ) {
            scale *= result.global_amp * result.branch_amp[branch_index( perk.branch )];
        }

        result.xp_bonus_pct += static_cast<int>( std::llround( perk.xp_bonus_pct * scale ) );
        for( int i = 0; i < perk.effect_count; ++i ) {
            if( perk.effects[i].id != nullptr ) {
                result.modifiers[perk.effects[i].id] += perk.effects[i].value * scale;
            }
        }
    }

    result.xp_bonus_pct = std::max( 0, std::min( 5000, result.xp_bonus_pct ) );
    return result;
}

std::map<std::string, double> owned_effect_totals()
{
    return calculate_owned_effects().modifiers;
}

bool prerequisites_met( const perk_def &perk )
{
    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) {
            continue;
        }
        const perk_def *required = find_perk( id );
        if( required == nullptr || !owned( *required ) ) {
            return false;
        }
    }
    return true;
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

    set_state( "schema", state_schema );
    effects_dirty = true;
}

std::string status_prefix( const perk_def &perk, int64_t level, int64_t perk_points, int64_t major_points )
{
    if( owned( perk ) ) {
        return "[✓] ";
    }
    if( level < perk.required_level ) {
        return std::string( russian() ? "[УР " : "[L" ) +
               std::to_string( perk.required_level ) + ( russian() ? "] " : "] " );
    }
    if( !prerequisites_met( perk ) ) {
        return russian() ? "[ТРЕБ.] " : "[REQ] ";
    }
    const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;
    if( !enough ) {
        return russian() ? "[НЕТ ОЧКОВ] " : "[NO POINTS] ";
    }
    return perk.currency == currency_id::perk ? "[1P] " : "[1M] ";
}

std::string cost_text( const perk_def &perk )
{
    return perk.currency == currency_id::perk ?
           tr( "1 perk point", "1 очко перка" ) :
           tr( "1 major point", "1 большое очко" );
}

bool purchase_perk( const perk_def &perk )
{
    int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );

    if( owned( perk ) ) {
        message( tr( "This perk is already owned.", "Этот перк уже куплен." ) );
        return false;
    }
    if( level < perk.required_level ) {
        message( tr( "Your Survivor level is too low.", "Недостаточный уровень Survivor." ) );
        return false;
    }
    if( !prerequisites_met( perk ) ) {
        message( tr( "Prerequisites are not met.", "Не выполнены требования предыдущих перков." ) );
        return false;
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

    set_state( perk_key( perk ), 1 );
    effects_dirty = true;
    recalculate_effects();

    message( tr( "Perk purchased: ", "Куплен перк: " ) +
             ( russian() ? perk.name_ru : perk.name_en ) );
    return true;
}

void show_perk_detail( const perk_def &perk )
{
    while( true ) {
        const int level = static_cast<int>( get_state( "level", 1 ) );
        const bool is_owned = owned( perk );
        const bool unlocked = level >= perk.required_level && prerequisites_met( perk );

        std::string title = ( russian() ? perk.name_ru : perk.name_en );
        title += "\n" + std::string( russian() ? perk.desc_ru : perk.desc_en );
        title += "\n" + tr( "Tier ", "Тир " ) + std::to_string( perk.tier );
        title += " | " + tr( "Requires level ", "Нужен уровень " ) + std::to_string( perk.required_level );
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost: ", "Стоимость: " ) + cost_text( perk );

        std::string buy = is_owned ? tr( "[Owned]", "[Куплено]" ) :
                          unlocked ? tr( "Purchase", "Купить" ) :
                          tr( "Locked", "Закрыто" );
        std::string back = tr( "Back", "Назад" );
        const char *entries[] = { buy.c_str(), back.c_str() };
        const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
        if( choice != 0 ) {
            return;
        }
        if( is_owned ) {
            return;
        }
        if( !unlocked ) {
            message( tr( "This perk is locked.", "Этот перк пока закрыт." ) );
            continue;
        }
        purchase_perk( perk );
        return;
    }
}

int branch_unlocked_count( branch_id branch, int64_t level )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !owned( perk ) &&
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

std::pair<int, int> survival_tree_position( const std::string &id )
{
    static const std::map<std::string, std::pair<int, int>> positions = {
        { "s_hardy", { 0, 0 } },
        { "s_field", { 0, 2 } },
        { "s_pack", { 1, 0 } },
        { "s_resilient", { 1, 2 } },
        { "s_endurance", { 2, 0 } },
        { "s_instinct", { 2, 2 } },
        { "s_ironback", { 3, 0 } },
        { "s_recovery", { 3, 2 } },
        { "s_survivor", { 4, 1 } },
        { "s_unbreakable", { 5, 1 } },

        { "se_lessons", { 0, 3 } },
        { "se_routine", { 1, 3 } },
        { "se_reserves", { 2, 3 } },
        { "se_adaptive", { 3, 3 } },
        { "se_anchor", { 4, 3 } },
        { "se_memory", { 5, 3 } },
        { "se_hardened", { 6, 3 } },
        { "se_grit", { 7, 3 } },
        { "se_carried", { 8, 3 } },
        { "se_indomitable", { 9, 3 } }
    };
    const auto it = positions.find( id );
    return it == positions.end() ? std::make_pair( 0, 0 ) : it->second;
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
    if( perk.currency == currency_id::major ) {
        return tr( "KEYSTONE", "КЛЮЧЕВОЙ" );
    }
    return effective_kind( perk ) == perk_kind::effect ?
           tr( "EFFECT", "ЭФФЕКТ" ) : tr( "STAT", "СТАТ" );
}

std::string branch_icon_key( branch_id branch )
{
    return std::string( "survivor/branch/" ) + branch_name_en( branch );
}

void show_branch( branch_id branch )
{
    bool tree_mode = branch == branch_id::survival;

    while( true ) {
        const int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        std::vector<const perk_def *> branch_perks;
        branch_perks.reserve( 24 );
        std::vector<card_text> texts;
        texts.reserve( 24 );

        for( const perk_def &perk : perks ) {
            if( perk.branch != branch ) {
                continue;
            }
            branch_perks.push_back( &perk );

            const bool is_owned = owned( perk );
            const bool unlocked = level >= perk.required_level && prerequisites_met( perk );
            const bool enough = perk.currency == currency_id::perk ?
                                perk_points > 0 : major_points > 0;

            card_text card;
            card.id = perk.id;
            card.title = russian() ? perk.name_ru : perk.name_en;
            card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                            tr( "Lv ", "Ур " ) + std::to_string( perk.required_level ) +
                            " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
            card.body = russian() ? perk.desc_ru : perk.desc_en;

            card.badge = perk_kind_label( perk ) + " | ";
            if( is_owned ) {
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

        std::string progress_label =
            branch_name( branch ) + "  " + std::to_string( owned_now ) + "/" +
            std::to_string( total_now );
        ncmm_ui_progress_v1 progress{
            progress_label.c_str(),
            owned_now,
            std::max( 1, total_now )
        };

        if( branch == branch_id::survival && tree_mode && host->ui_tree_choose ) {
            std::vector<tree_node_text> tree_texts;
            tree_texts.reserve( branch_perks.size() );
            std::map<std::string, size_t> index_by_id;

            for( size_t i = 0; i < branch_perks.size(); ++i ) {
                const perk_def &perk = *branch_perks[i];
                tree_node_text node;
                node.card = texts[i];
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
                const std::pair<int, int> position = survival_tree_position( perk.id );
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
                summary + tr( " | TREE PROTOTYPE | Tab: cards",
                              " | ДЕРЕВО-ПРОТОТИП | Tab: карточки" );
            const int choice = host->ui_tree_choose(
                                   title.c_str(), tree_summary.c_str(), &progress,
                                   nodes.data(), nodes.size(), edges.data(), edges.size() );
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
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 2 ) :
                           -1;
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

    std::string out = "Survivor Progression v0.9.2\n";
    out += tr( "Level ", "Уровень " ) + std::to_string( level );
    out += " | XP " + std::to_string( xp ) + "/" + std::to_string( xp_to_next( level ) );
    out += "\nP " + std::to_string( perk_points ) + " | M " + std::to_string( major_points );
    out += "\n" + tr( "Purchased: ", "Куплено: " ) +
           std::to_string( normal_owned ) + "P / " + std::to_string( major_owned ) + "M";
    out += "\n" + tr( "Major points: every 5 levels, with no level cap.",
                       "Большие очки: каждые 5 уровней, без ограничения уровня." );

    for( branch_id branch : { branch_id::combat, branch_id::survival, branch_id::mobility,
                              branch_id::crafting, branch_id::scavenging, branch_id::mastery } ) {
        out += "\n" + branch_name( branch ) + ": " +
               std::to_string( branch_owned_count( branch ) ) + "/" +
               std::to_string( branch_total_count( branch ) );
    }

    out += "\n\n" + tr( "Active effects:", "Активные эффекты:" );
    const std::map<std::string, double> totals = owned_effect_totals();
    if( totals.empty() && current_xp_bonus_pct == 0 ) {
        out += "\n" + tr( "none", "нет" );
    } else {
        for( const auto &entry : totals ) {
            const std::string sign = entry.second > 0.0 ? "+" : "";
            out += "\n" + effect_label( entry.first ) + ": " + sign + format_number( entry.second );
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
        if( !owned( perk ) ) {
            continue;
        }
        if( perk.currency == currency_id::perk ) {
            ++refund_perk;
        } else {
            ++refund_major;
        }
    }

    if( refund_perk == 0 && refund_major == 0 ) {
        message( tr( "No Survivor perks to reset.", "Нет перков Survivor для сброса." ) );
        return;
    }

    std::string title = tr(
        "Respec all Survivor perks?\nRefund: ",
        "Сбросить все перки Survivor?\nВозврат: " );
    title += std::to_string( refund_perk ) + "P / " + std::to_string( refund_major ) + "M";
    title += tr( "\nAll active gameplay modifiers from Survivor Progression will be removed.",
                 "\nВсе активные игровые модификаторы Survivor Progression будут сняты." );

    std::string yes = tr( "Respec", "Сбросить" );
    std::string no = tr( "Cancel", "Отмена" );
    const char *entries[] = { yes.c_str(), no.c_str() };
    const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
    if( choice != 0 ) {
        return;
    }

    for( const perk_def &perk : perks ) {
        if( owned( perk ) ) {
            set_state( perk_key( perk ), 0 );
        }
    }

    set_state( "perk_points", get_state( "perk_points", 0 ) + refund_perk );
    set_state( "major_points", get_state( "major_points", 0 ) + refund_major );
    set_state( "fast_learner", 0 );
    effects_dirty = true;
    recalculate_effects();

    message( tr( "Survivor perks reset. Refunded: ", "Перки Survivor сброшены. Возвращено: " ) +
             std::to_string( refund_perk ) + "P / " + std::to_string( refund_major ) + "M" );
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
        texts.reserve( 9 );
        for( branch_id branch : branches ) {
            card_text card;
            card.id = branch_name_en( branch );
            card.title = branch_name( branch );
            card.subtitle =
                std::to_string( branch_owned_count( branch ) ) + "/" +
                std::to_string( branch_total_count( branch ) ) +
                tr( " purchased | ", " куплено | " ) +
                std::to_string( branch_unlocked_count( branch, level ) ) +
                tr( " available", " доступно" );
            card.body = branch_focus( branch );
            card.badge = tr( "BRANCH", "ВЕТКА" );
            card.icon_key = branch_icon_key( branch );
            card.flags = NCMM_UI_CARD_ACCENT;
            texts.push_back( std::move( card ) );
        }

        card_text overview;
        overview.id = "overview";
        overview.title = tr( "Overview", "Обзор" );
        overview.subtitle = tr( "Level / points / active effects", "Уровень / очки / активные эффекты" );
        overview.body = tr( "Inspect the complete Survivor state.",
                            "Полное состояние прогрессии Survivor." );
        overview.badge = tr( "INFO", "ИНФО" );
        overview.icon_key = "survivor/action/overview";
        texts.push_back( std::move( overview ) );

        card_text reset;
        reset.id = "respec";
        reset.title = tr( "Respec", "Сброс перков" );
        reset.subtitle = tr( "Refund every purchase", "Вернуть все покупки" );
        reset.body = tr( "Refund perk and major points and clear Survivor modifiers.",
                         "Вернуть очки и снять модификаторы Survivor." );
        reset.badge = tr( "ACTION", "ДЕЙСТВИЕ" );
        reset.icon_key = "survivor/action/respec";
        texts.push_back( std::move( reset ) );

        card_text close;
        close.id = "close";
        close.title = tr( "Close", "Закрыть" );
        close.subtitle = tr( "Return to game", "Вернуться в игру" );
        close.body = tr( "Keep your build and continue playing.",
                         "Сохранить билд и вернуться в игру." );
        close.badge = tr( "ACTION", "ДЕЙСТВИЕ" );
        close.icon_key = "survivor/action/close";
        texts.push_back( std::move( close ) );

        const int total_owned = owned_count( currency_id::perk ) + owned_count( currency_id::major );
        const int total_perks = static_cast<int>( sizeof( perks ) / sizeof( perks[0] ) );

        std::string title = "Survivor Progression v0.9.4";
        std::string summary =
            tr( "Level ", "Уровень " ) + std::to_string( level ) +
            " | P " + std::to_string( perk_points ) +
            " | M " + std::to_string( major_points ) +
            tr( " | purchased ", " | куплено " ) +
            std::to_string( total_owned ) + "/" + std::to_string( total_perks );

        const int64_t xp_needed = xp_to_next( level );
        std::string progress_label =
            "XP " + std::to_string( xp ) + "/" + std::to_string( xp_needed ) +
            tr( " -> Level ", " -> Уровень " ) + std::to_string( level + 1 );
        ncmm_ui_progress_v1 progress{ progress_label.c_str(), xp, xp_needed };

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 3 ) :
                           -1;

        switch( choice ) {
            case 0: show_branch( branch_id::combat ); break;
            case 1: show_branch( branch_id::survival ); break;
            case 2: show_branch( branch_id::mobility ); break;
            case 3: show_branch( branch_id::crafting ); break;
            case 4: show_branch( branch_id::scavenging ); break;
            case 5: show_branch( branch_id::mastery ); break;
            case 6: show_overview(); break;
            case 7: respec(); break;
            default: return;
        }
    }
}
void award_minute_xp()
{
    if( !character_available() ) {
        return;
    }

    int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );

    int64_t fraction = get_state( "xp_fraction", 0 );
    fraction += 100 + current_xp_bonus_pct;
    int64_t gained = fraction / 100;
    fraction %= 100;
    set_state( "xp_fraction", fraction );

    if( gained <= 0 ) {
        return;
    }

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
        text += tr( " Open Survivor Progression to spend them.",
                    " Откройте Survivor Progression, чтобы потратить их." );
        message( text );
    }
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
    award_minute_xp();
}

int init( const ncmm_host_api_v1 *api )
{
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
        !api->ui_message ) {
        return 0;
    }

    host = api;
    api->log( NCMM_LOG_INFO,
              "Survivor Progression 0.9.4 initialized: unbounded levels / 120 perks / 6 branches." );
    return 1;
}

void shutdown()
{
    clear_runtime_modifiers();
    host = nullptr;
    turn_accumulator = 0;
    last_character_available = false;
    effects_dirty = true;
    current_xp_bonus_pct = 0;
}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    module_id,
    "Survivor Progression",
    "0.9.4",
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
