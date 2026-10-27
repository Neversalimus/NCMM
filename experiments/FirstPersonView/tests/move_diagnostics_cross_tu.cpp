#include "ncmm_graphics_move_diagnostics.hpp"

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
