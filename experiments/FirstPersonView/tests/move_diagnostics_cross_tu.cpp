#include "ncmm_graphics_move_diagnostics.hpp"
#include "ncmm_graphics_turn_diagnostics.hpp"
#include <initializer_list>

void ncmm_turn_diagnostics_cross_tu()
{
    ncmm_graphics_turn_diagnostics::draw_scope scope;
    scope.complete( true, ncmm_graphics_turn_diagnostics::backend::gpu );
}

void ncmm_cache_diagnostics_cross_tu()
{
    namespace diag = ncmm_graphics_move_diagnostics;
    diag::cache_scope scope( 0, false, true );
    if( scope ) {
        scope.bounds( -10, 10 );
        for( std::size_t i = 2; i < diag::cache_boundary_capacity - 1; ++i )
            scope.mark( static_cast<diag::cache_boundary>( i ) );
    }
}

void ncmm_move_diagnostics_cross_tu()
{
    namespace diag = ncmm_graphics_move_diagnostics;
    int x = 65, y = 65;
    diag::update_scope scope( x, y, false );
    if( scope ) {
        scope.route( 0, 0 );
        scope.mark( diag::boundary::zero_position );
        scope.mark( diag::boundary::zero_overmap );
    }
}

void ncmm_shift_diagnostics_cross_tu()
{
    namespace diag = ncmm_graphics_move_diagnostics;
    int x = 72, y = 60;
    diag::update_scope parent( x, y, false );
    if( parent ) parent.route( 1, 0 );
    {
        diag::shift_scope child( 1, 0 );
        if( child ) {
            child.accept();
            for( auto tag : {diag::shift_boundary::setup, diag::shift_boundary::unload,
                             diag::shift_boundary::origin_traps_remote, diag::shift_boundary::vehicle_zones,
                             diag::shift_boundary::cache_rebase, diag::shift_boundary::grid_copy_load,
                             diag::shift_boundary::vehicle_levels_remote, diag::shift_boundary::support_rebase,
                             diag::shift_boundary::actualize, diag::shift_boundary::reconcile} ) child.mark( tag );
        }
    }
    x = 60;
    if( parent ) for( auto tag : {diag::boundary::shift, diag::boundary::rebase_load_npcs, diag::boundary::invalidate,
                                 diag::boundary::build, diag::boundary::spawn, diag::boundary::overmap} ) parent.mark( tag );
}
