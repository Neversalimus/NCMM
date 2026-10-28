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
void callback_phases( bool nested = false, bool cpu = false )
{
    using b = turn_diag::callback_boundary;
    turn_diag::mark( b::native_cache_begin );
    turn_diag::mark( b::native_cache_end );
    if( nested ) {
        turn_diag::draw_scope inner( 200, 100 );
        callback_phases(); inner.complete( true, turn_diag::backend::cpu );
    }
    turn_diag::mark( b::visibility_begin );
    turn_diag::mark( b::visibility_end );
    turn_diag::mark( b::module_begin, 64, 1 );
    if( cpu ) turn_diag::observe_cpu( {640,360,4,777,8192,.000005,.000003,.000004,.000002} );
    turn_diag::mark( b::module_end, 640, 360 );
}
}
void test_native_turn_diagnostics()
{
    namespace d = turn_diag;
    const auto allocations = ncmm_test_allocation_calls();
    const auto ticks = turn_ticks;
    ncmm_test_allocation_enable( true );
    { d::draw_scope off( 1280, 720 ); callback_phases(); off.complete( true, d::backend::gpu ); }
    d::observe_cpu( {0,0,0,0,0,-1,-1,-1,-1} );
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
    d::turn_record detailed;
    ncmm_test_allocation_enable( true );
    { d::turn_capture capture( detailed, turn_now, true );
      { d::draw_scope draw( 1280, 720 ); callback_phases(); draw.complete( true, d::backend::gpu ); }
      capture.complete(); }
    ncmm_test_allocation_enable( false );
    check_turn( d::valid( detailed ) && allocations == ncmm_test_allocation_calls(), "detailed callback uses fixed storage without allocations" );
    check_turn( detailed.wall_ns == 90 && detailed.draw_wall_ns == 70 && detailed.callback_count == 1, "adjacent phases partition the outermost interval" );
    const auto &c = detailed.callbacks[0];
    check_turn( c.viewport_width == 1280 && c.viewport_height == 720 && c.image_width == 640 && c.image_height == 360,
                "submitted image dimensions stay distinct from Native viewport" );
    check_turn( c.count == 8 && c.returned && c.rendered && c.mode == d::backend::gpu, "complete world callback is identified" );
    check_turn( c.source_cells == 64 && c.source_slices == 1 && !c.cpu_reports, "source dimensions are captured without invented GPU CPU-raster metrics" );
    for( std::size_t i = 0; i < c.count; ++i )
        check_turn( std::size_t( c.tags[i] ) == i && c.offsets_ns[i] == std::int64_t( i * 10 ), "each adjacent boundary has its own raw timestamp" );
    d::turn_record detailed_nested;
    { d::turn_capture capture( detailed_nested, turn_now, true );
      { d::draw_scope draw( 1280, 720 ); callback_phases( true ); draw.complete( true, d::backend::gpu ); }
      capture.complete(); }
    check_turn( d::valid( detailed_nested ) && detailed_nested.draw_calls == 2 && detailed_nested.callback_count == 1 &&
                detailed_nested.draw_wall_ns == 70 && detailed_nested.callbacks[0].offsets_ns == c.offsets_ns,
                "nested markers neither overwrite outer partition nor add clocks" );
    for( int mutation = 0; mutation < 10; ++mutation ) {
        auto corrupt = detailed; auto &row = corrupt.callbacks[0];
        if( mutation == 0 ) row.count = 2;
        else if( mutation == 1 ) row.tags[2] = d::callback_boundary::module_end;
        else if( mutation == 2 ) row.offsets_ns[3] = row.offsets_ns[2] - 1;
        else if( mutation == 3 ) ++row.offsets_ns[7];
        else if( mutation == 4 ) row.image_width = 0;
        else if( mutation == 5 ) row.image_height = 8193;
        else if( mutation == 6 ) row.viewport_width = 0;
        else if( mutation == 7 ) row.viewport_height = 8193;
        else if( mutation == 8 ) row.returned = false;
        else row.ordinal = 1;
        check_turn( !d::valid( corrupt ), "partial, overlapping, mismatched or undimensioned callback rejected" );
    }
    d::turn_record partial;
    { d::turn_capture capture( partial, turn_now, true );
      { d::draw_scope draw( 1280, 720 ); d::mark( d::callback_boundary::native_cache_begin ); draw.complete( true, d::backend::cpu ); }
      capture.complete(); }
    check_turn( !d::valid( partial ), "a successful world image cannot qualify with missing phases" );
    d::turn_record wrong_order;
    { d::turn_capture capture( wrong_order, turn_now, true );
      { d::draw_scope draw( 1280, 720 ); d::mark( d::callback_boundary::visibility_begin ); draw.complete( false, d::backend::cpu ); }
      capture.complete(); }
    check_turn( !d::valid( wrong_order ) && ( wrong_order.errors & d::protocol ), "out-of-order marker faults the capture" );
    d::turn_record early_skip;
    { d::turn_capture capture( early_skip, turn_now, true );
      { d::draw_scope draw( 0, 0 ); draw.complete( false, d::backend::cpu ); }
      { d::draw_scope draw( 640, 480 ); draw.complete( true, d::backend::legacy ); }
      capture.complete(); }
    check_turn( d::valid( early_skip ) && early_skip.callback_count == 2, "honest early skip and legacy callback have no invented world phases" );
    d::turn_record detail_capacity;
    { d::turn_capture capture( detail_capacity, turn_now, true );
      for( std::size_t i = 0; i <= d::callback_capacity; ++i ) {
          d::draw_scope draw; draw.complete( false, d::backend::legacy );
      }
      capture.complete(); }
    check_turn( !d::valid( detail_capacity ) && ( detail_capacity.errors & d::overflow ) &&
                detail_capacity.callback_count == d::callback_capacity, "bounded callback capacity cannot silently drop intervals" );
    d::turn_record cpu_work;
    ncmm_test_allocation_enable( true );
    { d::turn_capture capture( cpu_work, turn_now, true );
      { d::draw_scope draw( 1280, 720 ); callback_phases( false, true ); draw.complete( true, d::backend::cpu ); }
      capture.complete(); }
    ncmm_test_allocation_enable( false );
    check_turn( d::valid( cpu_work ) && allocations == ncmm_test_allocation_calls(), "direct CPU report event allocates nothing" );
    const auto &reported = cpu_work.callbacks[0];
    check_turn( reported.cpu_reports == 1 && reported.cpu.raster_calls == 4 && reported.cpu.triangles == 777 &&
                reported.cpu.cache_bytes == 8192 && cpu_work.draw_wall_ns == 70, "fresh CPU counters do not add clocks or overlap wall partitions" );
    check_turn( reported.cpu.geometry_ms + reported.cpu.raster_ms + reported.cpu.effects_ms > reported.cpu.frame_ms,
                "inclusive component reports may overlap; no invented adjacent sum" );
    for( int mutation = 0; mutation < 8; ++mutation ) {
        auto corrupt = cpu_work; auto &row = corrupt.callbacks[0];
        if( mutation == 0 ) row.cpu_reports = 2;
        else if( mutation == 1 ) row.cpu.width = 123;
        else if( mutation == 2 ) row.cpu.frame_ms = std::numeric_limits<double>::quiet_NaN();
        else if( mutation == 3 ) row.cpu.raster_ms = -1;
        else if( mutation == 4 ) row.mode = d::backend::gpu;
        else if( mutation == 5 ) row.source_slices = 0;
        else if( mutation == 6 ) row.source_slices = 64;
        else row.cpu.frame_ms = 1;
        check_turn( !d::valid( corrupt ), "invalid, mismatched, stale-category CPU report or source slices rejected" );
    }
    d::turn_record late_cpu;
    { d::turn_capture capture( late_cpu, turn_now, true );
      { d::draw_scope draw( 1280, 720 ); callback_phases(); d::observe_cpu( reported.cpu ); draw.complete( true, d::backend::cpu ); }
      capture.complete(); }
    check_turn( !d::valid( late_cpu ) && ( late_cpu.errors & d::protocol ), "CPU report outside its module interval rejected" );
    d::turn_record legacy_cpu;
    { d::turn_capture capture( legacy_cpu, turn_now, true );
      { d::draw_scope draw( 640, 480 ); d::observe_cpu( reported.cpu ); draw.complete( true, d::backend::legacy ); }
      capture.complete(); }
    check_turn( d::valid( legacy_cpu ) && !legacy_cpu.callbacks[0].cpu_reports, "legacy module event cannot invent world CPU attribution" );
    std::cout << "Native turn render attribution: " << turn_checks << " checks passed; injected clocks are not measurements\n";
}
