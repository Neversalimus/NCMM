#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fixture0113 {

enum class attitude { friendly, neutral, hostile };

struct riposte_context {
    bool avatar = true;
    bool mounted = false;
    bool source_alive = true;
    bool source_real = true;
    bool source_hostile = true;
    bool player_alive = true;
    bool enough_stamina = true;
    bool vanilla_counter_executed = false;
};

bool riposte_eligible( const riposte_context &c ) {
    return c.avatar && !c.mounted && c.source_alive && c.source_real && c.source_hostile &&
           c.player_alive && c.enough_stamina && !c.vanilla_counter_executed;
}

int isolated_refund( int pre_attack_base_cost, double pct ) {
    pct = std::clamp( pct, 0.0, 100.0 );
    return static_cast<int>( std::lround( std::max( 0, pre_attack_base_cost ) * pct / 100.0 ) );
}

int old_net_move_refund( int moves_before, int moves_after_nested_rewards, double pct ) {
    const int spent = std::max( 0, moves_before - moves_after_nested_rewards );
    return static_cast<int>( spent * pct / 100.0 );
}

bool hostile_reactive_target( attitude a, bool fleeing_hostile_monster ) {
    return a == attitude::hostile || fleeing_hostile_monster;
}

bool hostile_npc_kill_reward( bool direct_avatar_kill, bool hallucination, bool fake_npc,
                              bool guaranteed_hostile ) {
    return direct_avatar_kill && !hallucination && !fake_npc && guaranteed_hostile;
}

struct momentum_result {
    std::int64_t stacks;
    std::int64_t turns;
    bool recalc;
};

momentum_result on_kill_momentum( std::int64_t raw_stacks, int max_stacks, int duration ) {
    const std::int64_t current = std::min<std::int64_t>( max_stacks,
                                 std::max<std::int64_t>( 0, raw_stacks ) );
    const std::int64_t stacks = std::min<std::int64_t>( max_stacks, current + 1 );
    return { stacks, duration, stacks != raw_stacks };
}

momentum_result on_turn_momentum( std::int64_t raw_stacks, std::int64_t raw_turns,
                                  int stack_cap, int duration_cap ) {
    std::int64_t stacks = std::min<std::int64_t>( stack_cap, std::max<std::int64_t>( 0, raw_stacks ) );
    std::int64_t remaining = std::min<std::int64_t>( duration_cap, std::max<std::int64_t>( 0, raw_turns ) );
    bool changed = stacks != raw_stacks;
    if( remaining <= 0 ) {
        changed = changed || stacks != 0;
        stacks = 0;
        return { stacks, 0, changed };
    }
    --remaining;
    if( remaining == 0 && stacks != 0 ) {
        return { 0, 0, true };
    }
    return { stacks, remaining, changed };
}

int monotonic_failure_point( int item_counter, int rerolled_failure_point ) {
    if( rerolled_failure_point <= item_counter ) {
        return std::min( 10000000, item_counter + 1 );
    }
    return rerolled_failure_point;
}

float catastrophic_ui_chance( float vanilla_chance, double failure_save_pct ) {
    const double save = std::clamp( failure_save_pct, 0.0, 35.0 );
    const float result = vanilla_chance * static_cast<float>( std::max( 0.0, 1.0 - save / 100.0 ) );
    return std::clamp( result, 0.0f, 1.0f );
}

} // namespace fixture0113

int main() {
    using namespace fixture0113;

    // Nested crit/kill +moves no longer alter the refund basis.
    assert( isolated_refund( 100, 50.0 ) == 50 );
    assert( isolated_refund( 100, 50.0 ) == isolated_refund( 100, 50.0 ) );
    assert( old_net_move_refund( 100, 30, 50.0 ) == 35 ); // 100-cost attack +30 nested reward -> under-refund.

    riposte_context r;
    assert( riposte_eligible( r ) );
    r.mounted = true;
    assert( !riposte_eligible( r ) );
    r.mounted = false;
    r.vanilla_counter_executed = true;
    assert( !riposte_eligible( r ) );
    r.vanilla_counter_executed = false; // selected-but-failed vanilla attack leaves fallback eligible.
    assert( riposte_eligible( r ) );

    assert( hostile_reactive_target( attitude::hostile, false ) );
    assert( hostile_reactive_target( attitude::neutral, true ) ); // fleeing hostile monster.
    assert( !hostile_reactive_target( attitude::friendly, false ) );
    assert( !hostile_reactive_target( attitude::neutral, false ) );

    assert( hostile_npc_kill_reward( true, false, false, true ) );
    assert( !hostile_npc_kill_reward( true, true, false, true ) );
    assert( !hostile_npc_kill_reward( true, false, true, true ) );
    assert( !hostile_npc_kill_reward( true, false, false, false ) );
    assert( !hostile_npc_kill_reward( false, false, false, true ) );

    // Saturate before +1: INT64_MAX cannot overflow.
    const auto huge = on_kill_momentum( std::numeric_limits<std::int64_t>::max(), 7, 20 );
    assert( huge.stacks == 7 && huge.turns == 20 && huge.recalc );
    const auto capped = on_kill_momentum( 7, 7, 20 );
    assert( capped.stacks == 7 && !capped.recalc ); // timer refresh only, no modifier recompute.
    const auto negative = on_kill_momentum( -99, 5, 12 );
    assert( negative.stacks == 1 && negative.recalc );

    const auto healed = on_turn_momentum( 999, 999, 5, 20 );
    assert( healed.stacks == 5 && healed.turns == 19 && healed.recalc );
    const auto expired = on_turn_momentum( 5, -100, 5, 20 );
    assert( expired.stacks == 0 && expired.turns == 0 && expired.recalc );

    assert( monotonic_failure_point( 5000000, 5000000 ) == 5000001 );
    assert( monotonic_failure_point( 5000000, 4999999 ) == 5000001 );
    assert( monotonic_failure_point( 5000000, 7000000 ) == 7000000 );
    assert( monotonic_failure_point( 9999999, 0 ) == 10000000 );

    assert( std::fabs( catastrophic_ui_chance( 0.40f, 10.0 ) - 0.36f ) < 0.0001f );
    assert( std::fabs( catastrophic_ui_chance( 0.40f, 100.0 ) - 0.26f ) < 0.0001f ); // save is clamped to 35%.
    assert( catastrophic_ui_chance( 2.0f, 0.0 ) == 1.0f );

    return 0;
}
