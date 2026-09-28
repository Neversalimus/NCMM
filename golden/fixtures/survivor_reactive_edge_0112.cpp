#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace fixture {

enum class attitude { friendly, neutral, hostile };
enum class monster_mood { friend_, passive, flee, follow, ignore, attack };

bool riposte_eligible( bool vanilla_counter_fired, bool in_progress, bool source_present,
                       bool adjacent, bool source_dead, bool source_hallucination,
                       attitude source_to_player, bool player_dead,
                       int stamina, int stamina_max, double chance )
{
    return !vanilla_counter_fired && !in_progress && source_present && adjacent && !source_dead &&
           !source_hallucination && source_to_player == attitude::hostile && !player_dead &&
           stamina >= stamina_max / 3 && chance > 0.0;
}

bool crit_reward_eligible( bool avatar, int dealt_damage, bool hallucination )
{
    return avatar && dealt_damage > 0 && !hallucination;
}

bool kill_reward_eligible( bool avatar, int kill_xp, monster_mood mood )
{
    return avatar && kill_xp > 0 && ( mood == monster_mood::attack || mood == monster_mood::flee );
}

struct momentum_state {
    std::int64_t stacks = 0;
    std::int64_t turns = 0;
};

momentum_state clamp_momentum( momentum_state s, bool predator, bool relentless, bool engine )
{
    if( !predator ) {
        return {};
    }
    std::int64_t stack_cap = relentless ? 5 : 3;
    if( engine ) {
        stack_cap += 2;
    }
    const std::int64_t turn_cap = relentless ? 20 : 12;
    s.stacks = std::min<std::int64_t>( stack_cap, std::max<std::int64_t>( 0, s.stacks ) );
    s.turns = std::min<std::int64_t>( turn_cap, std::max<std::int64_t>( 0, s.turns ) );
    if( s.turns == 0 ) {
        s.stacks = 0;
    }
    return s;
}

void full_respec( momentum_state &s )
{
    s.stacks = 0;
    s.turns = 0;
}

int lockpick_moves( const bool *perfect_flag, int vanilla_moves, double reduction_pct )
{
    const bool perfect = perfect_flag != nullptr && *perfect_flag;
    const int floor_moves = perfect ? 500 : 3000; // 5 s / 30 s at 100 moves/s in the fixture.
    const double bounded = std::max( 0.0, std::min( 60.0, reduction_pct ) );
    return std::max( floor_moves, static_cast<int>( vanilla_moves * ( 1.0 - bounded / 100.0 ) ) );
}

} // namespace fixture

int main()
{
    using namespace fixture;

    // Riposte must remain a fallback and never auto-target unsafe sources.
    assert( riposte_eligible( false, false, true, true, false, false, attitude::hostile,
                              false, 400, 900, 20.0 ) );
    assert( !riposte_eligible( true, false, true, true, false, false, attitude::hostile,
                               false, 400, 900, 20.0 ) );
    assert( !riposte_eligible( false, false, true, true, false, true, attitude::hostile,
                               false, 400, 900, 20.0 ) );
    assert( !riposte_eligible( false, false, true, true, false, false, attitude::friendly,
                               false, 400, 900, 20.0 ) );
    assert( !riposte_eligible( false, false, true, true, false, false, attitude::neutral,
                               false, 400, 900, 20.0 ) );
    assert( !riposte_eligible( false, false, true, true, false, false, attitude::hostile,
                               false, 299, 900, 20.0 ) );

    // Critical rewards require actual damage against a non-hallucination target.
    assert( crit_reward_eligible( true, 1, false ) );
    assert( !crit_reward_eligible( true, 0, false ) );
    assert( !crit_reward_eligible( true, 99, true ) );

    // Kill rewards reject zero-XP and non-combat dispositions, while fleeing hostile combatants count.
    assert( kill_reward_eligible( true, 10, monster_mood::attack ) );
    assert( kill_reward_eligible( true, 10, monster_mood::flee ) );
    assert( !kill_reward_eligible( true, 0, monster_mood::attack ) );
    assert( !kill_reward_eligible( true, 10, monster_mood::friend_ ) );
    assert( !kill_reward_eligible( true, 10, monster_mood::passive ) );
    assert( !kill_reward_eligible( true, 10, monster_mood::follow ) );
    assert( !kill_reward_eligible( true, 10, monster_mood::ignore ) );

    // Corrupt/stale transient state is bounded to currently owned perk caps.
    auto m = clamp_momentum( { 999, 999 }, true, false, false );
    assert( m.stacks == 3 && m.turns == 12 );
    m = clamp_momentum( { 999, 999 }, true, true, true );
    assert( m.stacks == 7 && m.turns == 20 );
    m = clamp_momentum( { -5, -1 }, true, true, true );
    assert( m.stacks == 0 && m.turns == 0 );
    m = clamp_momentum( { 5, 10 }, false, true, true );
    assert( m.stacks == 0 && m.turns == 0 );
    m = { 5, 20 };
    full_respec( m );
    assert( m.stacks == 0 && m.turns == 0 );

    // Null lockpick pointers are safe and all timing floors survive percentage reductions.
    const bool perfect = true;
    const bool ordinary = false;
    assert( lockpick_moves( nullptr, 6000, 60.0 ) == 3000 );
    assert( lockpick_moves( &ordinary, 10000, 20.0 ) == 8000 );
    assert( lockpick_moves( &ordinary, 3100, 60.0 ) == 3000 );
    assert( lockpick_moves( &perfect, 500, 60.0 ) == 500 );

    std::cout << "survivor_reactive_edge_0112: PASS\n";
    return 0;
}
