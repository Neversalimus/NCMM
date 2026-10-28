#include "ncmm_graphics_turn_diagnostics.hpp"
#include <cstddef>
#include <iostream>
#include <stdexcept>

std::size_t ncmm_test_allocation_calls() noexcept;
bool ncmm_test_allocation_enable( bool ) noexcept;
void ncmm_turn_diagnostics_cross_tu();
namespace {
namespace turn_diag = ncmm_graphics_turn_diagnostics;
std::int64_t turn_ticks = 0;
std::size_t turn_checks = 0;
turn_diag::clock::time_point turn_now() noexcept
{
    return turn_diag::clock::time_point( std::chrono::nanoseconds( turn_ticks += 10 ) );
}
void check_turn( bool condition, const char *message )
{
    ++turn_checks;
    if( !condition ) throw std::runtime_error( message );
}
}
void test_native_turn_diagnostics()
{
    namespace d = turn_diag;
    const auto allocations = ncmm_test_allocation_calls();
    const auto ticks = turn_ticks;
    ncmm_test_allocation_enable( true );
    { d::draw_scope off; off.complete( true, d::backend::gpu ); }
    ncmm_turn_diagnostics_cross_tu();
    ncmm_test_allocation_enable( false );
    check_turn( turn_ticks == ticks && allocations == ncmm_test_allocation_calls(), "disabled turn capture does not clock or allocate" );
    d::turn_record record;
    ncmm_test_allocation_enable( true );
    { d::turn_capture capture( record, turn_now );
      { d::draw_scope outer;
        { d::draw_scope nested; nested.complete( true, d::backend::cpu ); }
        outer.complete( true, d::backend::gpu ); }
      { d::draw_scope skipped; skipped.complete( false, d::backend::legacy ); }
      capture.complete(); }
    ncmm_test_allocation_enable( false );
    check_turn( allocations == ncmm_test_allocation_calls(), "enabled turn capture uses no allocations" );
    check_turn( d::valid( record ) && !d::active_capture, "complete turn restores inactive TLS" );
    check_turn( record.draw_calls == 3 && record.outer_draws == 2 && record.max_depth == 2, "nested draw ancestry counted" );
    check_turn( record.cpu_draws == 1 && record.gpu_draws == 1 && record.skipped_draws == 1 && !record.legacy_draws, "backend classified only after successful draw" );
    check_turn( record.wall_ns == 50 && record.draw_wall_ns == 20, "outermost union avoids nested double counting" );
    d::turn_record no_draw;
    { d::turn_capture capture( no_draw, turn_now ); capture.complete(); }
    check_turn( d::valid( no_draw ) && no_draw.wall_ns == 10 && !no_draw.draw_wall_ns, "a real turn can legitimately render no perspective callback" );
    d::turn_record cross;
    { d::turn_capture capture( cross, turn_now ); ncmm_turn_diagnostics_cross_tu(); capture.complete(); }
    check_turn( d::valid( cross ) && cross.gpu_draws == 1 && cross.outer_draws == 1, "turn TLS capture crosses translation units" );
    d::turn_record legacy;
    { d::turn_capture capture( legacy, turn_now ); { d::draw_scope draw; draw.complete( true, d::backend::legacy ); } capture.complete(); }
    check_turn( d::valid( legacy ) && legacy.legacy_draws == 1, "legacy success is not invented as CPU world draw" );
    auto bad = record; ++bad.cpu_draws;
    check_turn( !d::valid( bad ), "corrupted backend sum rejected" );
    bad = record; bad.draw_wall_ns = bad.wall_ns + 1;
    check_turn( !d::valid( bad ), "draw time cannot exceed its parent turn" );
    d::turn_record incomplete;
    { d::turn_capture capture( incomplete, turn_now ); }
    check_turn( !d::valid( incomplete ) && ( incomplete.errors & d::protocol ), "missing turn return rejected" );
    d::turn_record incomplete_draw;
    { d::turn_capture capture( incomplete_draw, turn_now ); { d::draw_scope draw; } capture.complete(); }
    check_turn( !d::valid( incomplete_draw ), "missing draw completion rejected" );
    d::turn_record double_draw;
    { d::turn_capture capture( double_draw, turn_now ); { d::draw_scope draw; draw.complete( true,d::backend::cpu ); draw.complete( true,d::backend::gpu ); } capture.complete(); }
    check_turn( !d::valid( double_draw ) && double_draw.cpu_draws == 1 && !double_draw.gpu_draws, "double draw completion rejected without double count" );
    d::turn_record backwards;
    { d::turn_capture capture( backwards, turn_now ); turn_ticks -= 100; { d::draw_scope draw; draw.complete( true,d::backend::cpu ); } capture.complete(); }
    check_turn( !d::valid( backwards ) && ( backwards.errors & d::backwards ), "backwards clock rejected" );
    d::turn_record exceptional;
    try { d::turn_capture capture( exceptional, turn_now ); d::draw_scope draw; throw std::runtime_error( "injected" ); }
    catch( const std::runtime_error & ) {}
    check_turn( !d::valid( exceptional ) && ( exceptional.errors & d::exceptional ) && !d::active_capture, "exception restores TLS and invalidates partial time" );
    d::turn_record first, second;
    { d::turn_capture outer( first,turn_now );
      { d::turn_capture inner( second,turn_now ); inner.complete(); }
      check_turn( d::active_capture == &outer, "nested capture restores outer owner" ); outer.complete(); }
    check_turn( !d::valid( first ) && !d::valid( second ) && ( first.errors & d::nested ) && ( second.errors & d::nested ), "overlapping turn captures cannot qualify" );
    { d::turn_capture reuse( record,turn_now ); reuse.complete(); }
    check_turn( !d::valid( record ) && ( record.errors & d::reused ), "reused receipt rejected" );
    d::turn_record saturated;
    { d::turn_capture capture( saturated,turn_now ); saturated.draw_calls = UINT32_MAX;
      { d::draw_scope draw; draw.complete( false,d::backend::cpu ); } capture.complete(); }
    check_turn( !d::valid( saturated ) && ( saturated.errors & d::overflow ) && saturated.draw_calls == UINT32_MAX, "counter saturates without wraparound" );
    std::cout << "Native turn render attribution: " << turn_checks << " checks passed; injected clocks are not measurements\n";
}
