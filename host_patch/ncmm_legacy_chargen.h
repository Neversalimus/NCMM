#pragma once
// Engine-only bridge. Included by newcharacter.cpp; never touches a loaded save.
#include "ncmm_character_points.hpp"
#include "ncmm_loader.h"
#include "avatar.h"
#include "cata_imgui.h"
#include "imgui/imgui.h"
#include "input.h"
#include "input_context.h"
#include "input_enums.h"
#include "mutation.h"
#include "options.h"
#include "output.h"
#include "player_difficulty.h"
#include "profession.h"
#include "scenario.h"
#include "skill.h"
#include "system_locale.h"
#include "translations.h"
#include "uilist.h"

namespace ncmm::legacy_chargen {
namespace points = ncmm::character_points;
inline const char *tr(const char *en, const char *ru) {
    std::string lang = get_option<std::string>("USE_LANG");
    if(lang.empty()) lang = SystemLocale::Language().value_or("en");
    return lang == "ru" || lang.rfind("ru_",0)==0 ? ru : en;
}
struct session;
inline session *current = nullptr;
struct session {
    session *previous = current;
    bool enabled = false;
    bool random_avatar = false;
    std::string policy = "any";
    points::pool mode = points::pool::freeform;
    points::config cfg;
    session() {
        enabled = ncmm::runtime_setting_hook_bool("chargen.points.enabled",0) != 0;
        if(enabled) {
            policy = get_option<std::string>("NCMM_LCP_MODE");
            auto value=[](const char *hook,int fallback) {
                return static_cast<int>(std::clamp<std::int64_t>(
                    ncmm::runtime_setting_hook_i64(hook,fallback),0,1000));
            };
            cfg.stats=value("chargen.points.stats",6);
            cfg.traits=value("chargen.points.traits",0);
            cfg.skills=value("chargen.points.skills",2);
            cfg.trait_cap=value("chargen.points.trait_cap",12);
            mode=points::select_mode(policy,0,false);
        }
        current=this;
    }
    session(const session&)=delete;
    session& operator=(const session&)=delete;
    ~session() {current=previous;}
};
inline bool active() {return current && current->enabled;}
inline bool limited() {return active() && points::limited(current->mode);}
inline pool_type saved_pool() {
    return static_cast<pool_type>(active() ? static_cast<int>(current->mode) : 0);
}
inline const char *mode_name(points::pool p) {
    if(p==points::pool::multi) return tr("Legacy: Multiple pools","Классика: раздельные очки");
    if(p==points::pool::single) return tr("Legacy: Single pool","Классика: единые очки");
    return tr("Survivor (freeform)","Выживший (без очков)");
}
inline bool choose_mode() {
    if(!active() || current->policy!="any") return true;
    uilist menu;
    menu.text=tr("Character point pools\nMultiple: Stats -> Traits -> Skills, never the reverse.\nSingle: one shared budget. Survivor: current freeform creation.",
                 "Система очков персонажа\nРаздельные: характеристики -> черты -> навыки, не наоборот.\nЕдиные: общий бюджет. Выживший: нынешнее свободное создание.");
    menu.addentry(2,true,'m',mode_name(points::pool::multi));
    menu.addentry(1,true,'s',mode_name(points::pool::single));
    menu.addentry(0,true,'f',mode_name(points::pool::freeform));
    menu.selected=current->mode==points::pool::multi ? 0 : current->mode==points::pool::single ? 1 : 2;
    menu.query();
    if(!points::selectable(menu.ret)) return false;
    current->mode=static_cast<points::pool>(menu.ret);
    return true;
}
inline bool begin(pool_type loaded, bool from_template, bool interactive) {
    if(!active()) return true;
    current->mode=points::select_mode(current->policy,static_cast<int>(loaded),from_template);
    // Templates retain their mode in Any worlds. A fixed world policy always wins.
    return !interactive || from_template || choose_mode();
}
inline void register_input(input_context &ctxt) {
    if(!active() || current->policy!="any") return;
    // Registered only in chargen input contexts; existing user bindings are preserved.
    inp_mngr.ncmm_register_default_action("ncmm.chargen.points",
        no_translation(tr("Character point pools","Система очков персонажа")),
        input_event(keycode::f6,input_event_t::keyboard_code));
    ctxt.register_action("ncmm.chargen.points",no_translation(tr("Character point pools","Система очков персонажа")));
}
inline bool locked(const avatar &u, const trait_id &t) {
    if(get_scenario()->is_locked_trait(t) || u.prof->is_locked_trait(t)) return true;
    for(const profession *h : u.hobbies) if(h->is_locked_trait(t)) return true;
    return false;
}
inline points::costs calculate(const avatar &u) {
    points::costs c;
    for(int s : {u.get_str_base(),u.get_dex_base(),u.get_int_base(),u.get_per_base()}) c.stat(s);
    for(const trait_id &t : u.get_mutations(true)) c.trait(t->points,locked(u,t));
    c.add(c.skills,get_scenario()->point_cost());
    c.add(c.skills,u.prof->point_cost());
    for(const profession *h : u.hobbies) c.add(c.skills,h->point_cost());
    // Profession/background skill bonuses are applied later by initialize(), not charged twice.
    for(const Skill &s : Skill::skills) c.skill(u.get_skill_level(s.ident()));
    return c;
}
inline const char *error_text(points::error e) {
    switch(e) {
        case points::error::invalid_data: return tr("Invalid character point data. Correct the character before starting.","Некорректные данные очков. Исправьте персонажа перед началом игры.");
        case points::error::stat_limit: return tr("Classic point modes allow base stats from 4 to 14.","В классических режимах базовые характеристики должны быть от 4 до 14.");
        case points::error::trait_limit: return tr("Too many points of advantages or disadvantages. Reduce voluntary traits.","Превышен лимит очков достоинств или недостатков. Уберите часть выбранных черт.");
        case points::error::skill_budget: return tr("Too many points allocated. Reduce traits, skills, stats or starting background costs.","Распределено слишком много очков. Уменьшите характеристики, черты, навыки или стоимость предыстории.");
        case points::error::trait_budget: return tr("Too many trait points allocated. Reduce traits or lower stats; skill points cannot pay for traits.","Не хватает очков черт. Уберите черты или снизьте характеристики; очки навыков не оплачивают черты.");
        case points::error::stat_budget: return tr("Too many stat points allocated. Lower base stats; other pools cannot pay for them.","Не хватает очков характеристик. Снизьте базовые характеристики; другие пулы их не оплачивают.");
        default: return "";
    }
}
inline bool valid(const avatar &u, bool show_error=false) {
    if(!limited()) return true;
    auto b=points::evaluate(current->mode,current->cfg,calculate(u));
    if(!b.valid() && show_error) popup("%s",error_text(b.problem));
    return b.valid();
}
inline bool confirm_finish(const avatar &u) {
    if(!valid(u,true)) return false;
    auto b=points::evaluate(current->mode,current->cfg,calculate(u));
    if(b.total_left>0 && !query_yn(tr("Remaining points will be discarded, are you sure you want to proceed?",
                                      "Оставшиеся очки будут потеряны. Продолжить?"))) return false;
    return query_yn(u.name.empty() ? tr("Are you finished? Your name will be randomly generated.","Завершить создание? Имя будет выбрано случайно.") :
                                      tr("Are you finished creating your character?","Завершить создание персонажа?"));
}
inline bool can_take(const avatar &u,const trait_id &t) {
    if(!limited() || locked(u,t)) return true;
    auto c=calculate(u);
    c.trait(t->points,false);
    if(c.invalid || c.advantages>current->cfg.trait_cap || c.disadvantages>current->cfg.trait_cap) {
        popup("%s",error_text(points::error::trait_limit));
        return false;
    }
    return true;
}
inline int stat_maximum(int vanilla) {return limited() ? points::stat_max : vanilla;}
inline int skill_delta(int level,int delta) {
    if(!limited()) return delta;
    if(level < 0 || level > 10) return 0; // malformed templates remain invalid at the final gate
    return points::skill_step(level,delta,true)-level;
}
inline std::string price(const std::string &label,int cost) {
    if(!limited()) return label;
    return label+" ["+(cost>0?"+":"")+std::to_string(cost)+"]";
}
inline std::string trait_price(const avatar &u,const trait_id &t) {
    return price(t->name(),locked(u,t)?0:t->points);
}
inline bool draw(const avatar &u, const input_context &ctxt) {
    if(!active()) return false;
    bool requested=false;
    // Outside the collapsible info panel: budgets cannot disappear when General Info is closed.
    cataimgui::draw_colored_text(mode_name(current->mode),c_white,ImGui::GetContentRegionAvail().x);
    if(current->policy=="any") {
        const std::string label=std::string(tr("Change point pools", "Сменить систему очков"))+" ("+ctxt.get_desc("ncmm.chargen.points")+")";
        if(ImGui::Button(label.c_str())) {
            // Input is handled after redraw, never open a modal recursively inside ImGui rendering.
            // Uses the same deferred action channel as the native creator's buttons.
            requested=true;
        }
    }
    if(!limited()) return requested;
    const auto c=calculate(u);
    const auto b=points::evaluate(current->mode,current->cfg,c);
    std::string text;
    if(current->mode==points::pool::single) {
        text=std::string(tr("Points left: ","Осталось очков: "))+std::to_string(b.total_left);
    } else {
        text=std::string(tr("Pools left (raw): Stats ","Остаток пулов: хар. "))+std::to_string(b.pure_stats)+
            tr(" | Traits "," | черты ")+std::to_string(b.pure_traits)+tr(" | Skills "," | навыки ")+std::to_string(b.pure_skills)+
            tr(" | Total "," | всего ")+std::to_string(b.total_left);
    }
    cataimgui::draw_colored_text(text,b.valid()?c_light_green:c_light_red,ImGui::GetContentRegionAvail().x);
    text=std::string(tr("Advantages: ","Достоинства: "))+std::to_string(c.advantages)+"/"+std::to_string(current->cfg.trait_cap)+
        tr(" | Disadvantages: "," | недостатки: ")+std::to_string(c.disadvantages)+"/"+std::to_string(current->cfg.trait_cap);
    cataimgui::draw_colored_text(text,c_light_gray,ImGui::GetContentRegionAvail().x);
    if(!b.valid()) cataimgui::draw_colored_text(error_text(b.problem),c_light_red,ImGui::GetContentRegionAvail().x);
    return requested;
}
// Current CDDA also uses its old point helpers for NPC randomization. Restrict overrides to avatars.
struct randomizer_scope {
    bool previous=false;
    explicit randomizer_scope(bool avatar) {
        if(current) {previous=current->random_avatar; current->random_avatar=avatar;}
    }
    randomizer_scope(const randomizer_scope&)=delete;
    randomizer_scope& operator=(const randomizer_scope&)=delete;
    ~randomizer_scope() {if(current) current->random_avatar=previous;}
};
inline int random_budget(int which,int fallback) {
    if(!active() || !current->random_avatar) return fallback;
    switch(which) {case 0:return current->cfg.stats; case 1:return current->cfg.traits;
                  case 2:return current->cfg.skills; default:return current->cfg.trait_cap;}
}
} // namespace ncmm::legacy_chargen
