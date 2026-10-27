// The public Native signature and explicit instantiations stay unchanged.
// Only the complete standard sight callback triple is pure with respect to
// trigdist. Generic callbacks must observe its live value at every cell.
template<typename T, T( *calc )( const T &, const T &, const int & ),
         bool( *is_transparent )( const T &, const T & ),
         T( *accumulate )( const T &, const T &, const int & )>
void cast_zlight(
    const array_of_grids_of<T> &output_caches,
    const array_of_grids_of<const T> &input_arrays,
    const array_of_grids_of<const bool> &floor_caches,
    const tripoint_bub_ms &origin, const int offset_distance, const T numerator,
    vertical_direction dir )
{
    if constexpr( std::is_same_v<T, float> ) {
        if constexpr( calc == ::sight_calc && is_transparent == ::sight_check &&
                      accumulate == ::accumulate_transparency ) {
            if( !trigdist ) {
                ncmm_cast_zlight_implementation<true, T, calc, is_transparent, accumulate>(
                    output_caches, input_arrays, floor_caches, origin, offset_distance, numerator, dir );
                return;
            }
        }
    }
    ncmm_cast_zlight_implementation<false, T, calc, is_transparent, accumulate>(
        output_caches, input_arrays, floor_caches, origin, offset_distance, numerator, dir );
}
