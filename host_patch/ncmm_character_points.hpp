#pragma once
// CDDA 0.H point accounting, isolated from the engine and shared by actual tests.
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace ncmm::character_points {
enum class pool : int { freeform = 0, single = 1, multi = 2, transfer = 3 };
constexpr int stat_min = 4;
constexpr int stat_max = 14;
constexpr int high_stat = 12;
constexpr std::array<int, 11> skill_costs = {0, 1, 1, 2, 4, 6, 9, 12, 16, 20, 25};
struct config { int stats = 6; int traits = 0; int skills = 2; int trait_cap = 12; };
struct costs {
    std::int64_t stats = 0, traits = 0, skills = 0, advantages = 0, disadvantages = 0;
    bool invalid = false, stats_out_of_range = false;
    void add( std::int64_t &target, std::int64_t value ) {
        if( (value > 0 && target > std::numeric_limits<std::int64_t>::max() - value) ||
            (value < 0 && target < std::numeric_limits<std::int64_t>::min() - value) ) {
            invalid = true;
            return;
        }
        target += value;
    }
    void stat( int value ) {
        stats_out_of_range = stats_out_of_range || value < stat_min || value > stat_max;
        const std::int64_t v = value;
        add( stats, v + std::max<std::int64_t>( 0, v - high_stat ) );
    }
    void trait( int value, bool locked ) {
        if( locked ) return; // Scenario/profession/background already pays for mandatory traits.
        add( traits, value );
        if( value > 0 ) add( advantages, value );
        else add( disadvantages, -static_cast<std::int64_t>( value ) );
    }
    void skill( int value ) {
        if( value < 0 || value >= static_cast<int>(skill_costs.size()) ) { invalid = true; return; }
        add( skills, skill_costs[static_cast<std::size_t>(value)] );
    }
};
enum class error { none, invalid_data, stat_limit, trait_limit, skill_budget, trait_budget, stat_budget };
struct balance {
    std::int64_t pure_stats = 0, pure_traits = 0, pure_skills = 0;
    std::int64_t stats_left = 0, traits_left = 0, total_left = 0;
    error problem = error::none;
    bool valid() const { return problem == error::none; }
};
inline bool limited( pool mode ) { return mode == pool::single || mode == pool::multi; }
inline bool selectable( int mode ) { return mode >= 0 && mode <= 2; }
inline pool select_mode( const std::string &policy, int template_mode, bool has_template ) {
    if( policy == "freeform" || policy == "story_teller" ) return pool::freeform;
    if( policy == "one_pool" ) return pool::single;
    if( policy == "multi_pool" ) return pool::multi;
    if( policy == "any" && has_template && selectable(template_mode) )
        return static_cast<pool>(template_mode);
    return pool::multi;
}
inline int skill_step( int level, int direction, bool classic ) {
    level = std::clamp(level, 0, 10);
    if(direction == 0) return level;
    const int delta = classic && ((level == 0 && direction > 0) ||
                                  (level == 2 && direction < 0)) ? 2 : 1;
    return std::clamp(level + (direction > 0 ? delta : -delta), 0, 10);
}
inline balance evaluate( pool mode, const config &cfg, const costs &c ) {
    balance b;
    if( mode == pool::freeform || mode == pool::transfer ) return b;
    // A conservative representability bound keeps every subsequent sum/subtraction defined.
    constexpr std::int64_t bound = std::numeric_limits<std::int64_t>::max() / 8;
    if( !limited(mode) || c.invalid || c.stats < -bound || c.stats > bound ||
        c.traits < -bound || c.traits > bound || c.skills < -bound || c.skills > bound ||
        c.advantages < 0 || c.disadvantages < 0 || cfg.stats < 0 || cfg.stats > 1000 ||
        cfg.traits < 0 || cfg.traits > 1000 || cfg.skills < 0 || cfg.skills > 1000 ||
        cfg.trait_cap < 0 || cfg.trait_cap > 1000 ) {
        b.problem = error::invalid_data; return b;
    }
    b.pure_stats = 32LL + cfg.stats - c.stats;
    b.pure_traits = static_cast<std::int64_t>(cfg.traits) - c.traits;
    b.pure_skills = static_cast<std::int64_t>(cfg.skills) - c.skills;
    b.stats_left = b.pure_stats + std::min<std::int64_t>(0, b.pure_traits +
                   std::min<std::int64_t>(0, b.pure_skills));
    b.traits_left = b.pure_stats + b.pure_traits + std::min<std::int64_t>(0, b.pure_skills);
    b.total_left = b.pure_stats + b.pure_traits + b.pure_skills;
    if( c.stats_out_of_range ) b.problem = error::stat_limit;
    else if( c.advantages > cfg.trait_cap || c.disadvantages > cfg.trait_cap ) b.problem = error::trait_limit;
    else if( b.total_left < 0 ) b.problem = error::skill_budget;
    else if( mode == pool::multi && b.traits_left < 0 ) b.problem = error::trait_budget;
    else if( mode == pool::multi && b.stats_left < 0 ) b.problem = error::stat_budget;
    return b;
}
} // namespace ncmm::character_points
