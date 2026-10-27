#include "ncmm_graphics_move_diagnostics.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

std::size_t ncmm_test_allocation_calls() noexcept;
bool ncmm_test_allocation_enable( bool ) noexcept;
void ncmm_cache_diagnostics_cross_tu();
namespace native_cache_diagnostics_test {
namespace diag = ncmm_graphics_move_diagnostics;
std::uint64_t ticks = 0;
diag::clock::time_point now() noexcept { return diag::clock::time_point( std::chrono::nanoseconds( ++ticks ) ); }
void complete( diag::cache_scope &scope ) {
    scope.bounds( -10, 10 );
    for( std::size_t i = 2; i < diag::cache_boundary_capacity - 1; ++i )
        scope.mark( static_cast<diag::cache_boundary>( i ) );
}
std::size_t checks = 0;
void require( bool ok, const char *message ) {
    ++checks;
    if( !ok ) throw std::runtime_error( message );
}
struct cleanup {
    diag::clock::time_point &point;
    ~cleanup() { point = now(); }
};
}
void test_native_cache_diagnostics() {
    using namespace native_cache_diagnostics_test;
    const auto old_ticks = ticks;
    const auto allocations = ncmm_test_allocation_calls();
    ncmm_test_allocation_enable( true );
    { diag::cache_scope off( 0, false, true ); complete( off ); }
    ncmm_test_allocation_enable( false );
    require( ticks == old_ticks && ncmm_test_allocation_calls() == allocations,
             "disabled cache probes have no clocks or allocations" );
    diag::action_record record;
    diag::clock::time_point cleaned;
    ncmm_test_allocation_enable( true );
    { diag::action_capture capture( record, now );
      { diag::cache_scope scope( 2, true, true ); cleanup local{ cleaned }; complete( scope ); } }
    ncmm_test_allocation_enable( false );
    require( ncmm_test_allocation_calls() == allocations, "complete cache capture uses fixed storage" );
    require( record.cache_invocations == 1 && record.cache_recorded == 1 && record.cache_dropped == 0 &&
             record.cache_max_depth == 1 && record.protocol_errors == 0, "complete cache counters" );
    const auto &event = record.caches[0];
    require( event.complete && !event.exceptional && event.bounds_known && event.zlev == 2 && event.skip_lightmap &&
             event.zlevels && event.min_z == -10 && event.max_z == 10 && event.parent_ordinal == -1 &&
             event.parent_update_ordinal == -1 && event.parent_shift_ordinal == -1, "cache context and standalone ancestry" );
    require( event.count == 12 && event.points[10] < cleaned && cleaned < event.points[11],
             "cache exit includes destruction of later locals" );
    for( std::size_t i = 0; i < event.count; ++i ) {
        require( static_cast<std::size_t>( event.tags[i] ) == i, "all adjacent cache boundaries retain order" );
        if( i ) require( event.points[i-1] < event.points[i], "injected cache clock order" );
    }
    diag::action_record parent;
    { diag::action_capture capture( parent, now ); int x = 77, y = 65;
      { diag::update_scope update( x, y, false ); x = 65; update.route( 1, 0 );
        { diag::shift_scope shift( 1, 0 ); shift.accept();
          { diag::cache_scope inner( 0, false, true ); complete( inner ); }
          for( std::size_t i = 1; i < diag::shift_boundary_capacity - 1; ++i )
              shift.mark( static_cast<diag::shift_boundary>( i ) ); }
        update.mark( diag::boundary::shift ); update.mark( diag::boundary::rebase_load_npcs ); update.mark( diag::boundary::invalidate );
        { diag::cache_scope direct( 0, false, true ); complete( direct ); }
        update.mark( diag::boundary::build ); update.mark( diag::boundary::spawn ); update.mark( diag::boundary::overmap ); } }
    require( parent.protocol_errors == 0 && parent.cache_recorded == 2 && parent.events[0].complete && parent.shifts[0].complete,
             "cache calls preserve complete outer Native protocols" );
    require( parent.caches[0].parent_update_ordinal == 0 && parent.caches[0].parent_shift_ordinal == 0 &&
             parent.caches[1].parent_update_ordinal == 0 && parent.caches[1].parent_shift_ordinal == -1,
             "cache calls bind to actual active update and shift owners" );
    diag::action_record nested;
    { diag::action_capture capture( nested, now ); diag::cache_scope outer( 0, false, true );
      { diag::cache_scope inner( 1, false, true ); complete( inner ); } complete( outer ); }
    require( nested.cache_recorded == 2 && nested.cache_max_depth == 2 && nested.caches[1].parent_ordinal == 0 &&
             nested.caches[1].depth == 2 && nested.protocol_errors == 0, "nested cache ancestry is preserved" );
    diag::action_record exceptional;
    try { diag::action_capture capture( exceptional, now ); diag::cache_scope scope( 0, false, true );
          scope.bounds( -10, 10 ); throw std::runtime_error( "owned cache exception" ); }
    catch( const std::runtime_error & ) {}
    require( exceptional.cache_exceptional_invocations == 1 && exceptional.caches[0].exceptional &&
             !exceptional.caches[0].complete && exceptional.caches[0].count == 3 && exceptional.protocol_errors == 0 &&
             diag::active_capture == nullptr, "exceptions restore capture without invented cache phases" );
    diag::action_record missing;
    { diag::action_capture capture( missing, now ); diag::cache_scope scope( 0, false, true ); scope.bounds( -10, 10 ); }
    require( !missing.caches[0].complete && missing.protocol_errors != 0, "missing cache markers cannot qualify" );
    diag::action_record overflow;
    { diag::action_capture capture( overflow, now );
      for( std::size_t i = 0; i < diag::cache_event_capacity + 5; ++i ) { diag::cache_scope scope( 0, false, true ); complete( scope ); } }
    require( overflow.cache_invocations == 37 && overflow.cache_recorded == 32 && overflow.cache_dropped == 5 &&
             overflow.cache_max_depth == 1 && overflow.protocol_errors == 0, "cache storage is bounded and drops are counted" );
    diag::action_record saturated;
    saturated.cache_invocations = std::numeric_limits<std::uint32_t>::max();
    { diag::action_capture capture( saturated, now ); diag::cache_scope scope( 0, false, true ); complete( scope ); }
    require( saturated.counter_overflow && saturated.cache_invocations == std::numeric_limits<std::uint32_t>::max(),
             "cache counters saturate honestly" );
    diag::action_record cross;
    { diag::action_capture capture( cross, now ); ncmm_cache_diagnostics_cross_tu(); }
    require( cross.cache_recorded == 1 && cross.caches[0].complete && cross.protocol_errors == 0,
             "cache capture shares the external inline TLS owner across translation units" );
    std::cout << "Native cache diagnostic primitives: " << checks << " checks passed; injected clocks are not timings\n";
}
