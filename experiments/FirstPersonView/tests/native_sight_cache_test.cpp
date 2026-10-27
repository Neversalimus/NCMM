// Complete pinned Native 3D shadowcasting and PLF container, with owned numeric
// coordinate/grid/distance and fragment helper context. This is a source model,
// not an SDL/game/input test or an ordinary-game frame-time measurement.
#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "third_party/native_plf_list.h"

constexpr int MAPSIZE_X = 132, MAPSIZE_Y = 132;
constexpr int OVERMAP_DEPTH = 10, OVERMAP_HEIGHT = 10, OVERMAP_LAYERS = 21;
constexpr int MAX_VIEW_DISTANCE = 60;
constexpr float LIGHT_TRANSPARENCY_SOLID = 0.0f;
constexpr float LIGHT_TRANSPARENCY_OPEN_AIR = 2.3f / MAX_VIEW_DISTANCE;
struct point_bub_ms {
    int px = 0, py = 0;
    point_bub_ms() = default;
    point_bub_ms( int x, int y ) : px( x ), py( y ) {}
    int x() const { return px; }
    int y() const { return py; }
    static const point_bub_ms zero;
};
const point_bub_ms point_bub_ms::zero{};
struct tripoint_bub_ms {
    int px = 0, py = 0, pz = 0;
    tripoint_bub_ms() = default;
    tripoint_bub_ms( int x, int y, int z ) : px( x ), py( y ), pz( z ) {}
    int &x() { return px; } int x() const { return px; }
    int &y() { return py; } int y() const { return py; }
    int &z() { return pz; } int z() const { return pz; }
    point_bub_ms xy() const { return {px, py}; }
    static const tripoint_bub_ms zero;
};
const tripoint_bub_ms tripoint_bub_ms::zero{};
using tripoint_rel_ms = tripoint_bub_ms;
template<class Point> struct half_open_rectangle {
    Point lo, hi;
    half_open_rectangle( Point a, Point b ) : lo( a ), hi( b ) {}
    bool contains( Point p ) const {
        return p.x() >= lo.x() && p.y() >= lo.y() && p.x() < hi.x() && p.y() < hi.y();
    }
};
namespace cata {
template<class T, class Point> using mdarray = std::array<std::array<T, MAPSIZE_Y>, MAPSIZE_X>;
template<class T> using list = plf::list<T>;
}
namespace ncmm_shadow_fixture {
std::uint32_t bits( float value ) {
    std::uint32_t out; std::memcpy( &out, &value, sizeof( out ) ); return out;
}
struct record {
    std::uint64_t exponent_calls = 0, requests = 0, checks = 0, accumulates = 0;
    std::uint64_t trace = 1469598103934665603ULL;
    void add( std::uint32_t value ) { trace = ( trace ^ value ) * 1099511628211ULL; }
};
record *active = nullptr;
bool circular = false;
float evaluate( float exponent ) {
    if( active ) ++active->exponent_calls;
    return std::exp( exponent );
}
struct counted_exp { float operator()( float v ) const { return evaluate( v ); } };
std::size_t checks = 0, cases = 0;
void require( bool value, const char *message ) {
    ++checks; if( !value ) throw std::runtime_error( message );
}
}
int rl_dist( const tripoint_rel_ms &a, const tripoint_rel_ms &b ) {
    const int dx = a.x()-b.x(), dy = a.y()-b.y(), dz = a.z()-b.z();
    const int result = ncmm_shadow_fixture::circular ?
        static_cast<int>( std::sqrt( static_cast<double>( dx*dx + dy*dy + dz*dz ) ) ) :
        std::max( {std::abs( dx ), std::abs( dy ), std::abs( dz )} );
    if( ncmm_shadow_fixture::active ) {
        ++ncmm_shadow_fixture::active->requests;
        ncmm_shadow_fixture::active->add( static_cast<std::uint32_t>( result ) );
    }
    return result;
}
#define NCMM_SHADOWCAST_MODEL
#include "native_shadowcasting_math.inc"
#include "native_shadowcasting_fragment.hpp"
#include "ncmm_graphics_sight_exp_cache.hpp"

// These helpers deliberately exercise generic fallback; they do not claim to
// reproduce Native shrapnel physics (its .cpp is not part of this frozen input).
fragment_cloud &fragment_cloud::operator=( const float &v ) { velocity = density = v; return *this; }
bool fragment_cloud::operator==( const fragment_cloud &v ) const {
    return velocity == v.velocity && density == v.density;
}
bool operator<( const fragment_cloud &a, const fragment_cloud &b ) {
    return a.velocity < b.velocity || ( a.velocity == b.velocity && a.density < b.density );
}
fragment_cloud shrapnel_calc( const fragment_cloud &n, const fragment_cloud &t, const int &d ) {
    if( ncmm_shadow_fixture::active ) ncmm_shadow_fixture::active->add( 0xf00du );
    return {n.velocity / ( 1.0f + t.velocity*d ), n.density / ( 1.0f + t.density*d )};
}
bool shrapnel_check( const fragment_cloud &t, const fragment_cloud &i ) {
    if( ncmm_shadow_fixture::active ) {
        ++ncmm_shadow_fixture::active->checks;
        ncmm_shadow_fixture::active->add( ncmm_shadow_fixture::bits( i.velocity ) );
    }
    return t.velocity > 0.0f && t.density > 0.0f;
}
fragment_cloud accumulate_fragment_cloud( const fragment_cloud &a, const fragment_cloud &b, const int &d ) {
    if( ncmm_shadow_fixture::active ) ++ncmm_shadow_fixture::active->accumulates;
    return {accumulate_transparency( a.velocity, b.velocity, d ), accumulate_transparency( a.density, b.density, d )};
}
namespace native_sight_reference {
#define NCMM_SHADOWCAST_REFERENCE
#include "native_shadowcasting_primary.inc"
#undef NCMM_SHADOWCAST_REFERENCE
}
namespace native_sight_candidate {
#include "native_shadowcasting_primary.inc"
}
#define NCMM_GRAPHICS_REFERENCE_PRIMARY native_sight_candidate::cast_zlight
#include "ncmm_graphics_sight_reference.inc"
#undef NCMM_GRAPHICS_REFERENCE_PRIMARY
namespace ncmm_shadow_fixture {
bool check( const float &t, const float &i ) {
    if( active ) { ++active->checks; active->add( bits( t ) ); active->add( bits( i ) ); }
    return sight_check( t, i );
}
float accumulate( const float &a, const float &b, const int &d ) {
    if( active ) { ++active->accumulates; active->add( bits( a ) ); active->add( bits( b ) ); active->add( static_cast<std::uint32_t>( d ) ); }
    return accumulate_transparency( a, b, d );
}
template<class T> struct world {
    using grid = cata::mdarray<T, point_bub_ms>;
    using floors_grid = cata::mdarray<bool, point_bub_ms>;
    std::array<std::unique_ptr<grid>, OVERMAP_LAYERS> inputs, reference, candidate;
    std::array<std::unique_ptr<floors_grid>, OVERMAP_LAYERS> floors;
    array_of_grids_of<T> expected{}, actual{};
    array_of_grids_of<const T> in{};
    array_of_grids_of<const bool> floor{};
    world() {
        for( int z = 0; z < OVERMAP_LAYERS; ++z ) {
            inputs[z] = std::make_unique<grid>(); reference[z] = std::make_unique<grid>();
            candidate[z] = std::make_unique<grid>(); floors[z] = std::make_unique<floors_grid>();
            expected[z] = reference[z].get(); actual[z] = candidate[z].get();
            in[z] = inputs[z].get(); floor[z] = floors[z].get();
        }
    }
    void fill( int scene, bool prefilled ) {
        for( int z = 0; z < OVERMAP_LAYERS; ++z ) for( int x = 0; x < MAPSIZE_X; ++x ) for( int y = 0; y < MAPSIZE_Y; ++y ) {
            const unsigned key = ( unsigned( x )*73856093u ) ^ ( unsigned( y )*19349663u ) ^ ( unsigned( z )*83492791u );
            float t = LIGHT_TRANSPARENCY_OPEN_AIR;
            if( scene == 1 ) t = 0.0f;
            if( scene == 2 && ( x%13 == 0 || y%17 == 0 ) && ( x+y )%7 != 0 ) t = 0.0f;
            if( scene == 3 ) t = 0.038f + 0.04f * ( ( x/5+y/7+z/3 )%4 );
            if( scene == 4 ) t = ( key%11 == 0 ) ? 0.0f : ( 0.038f + float( key%3 )*0.12f );
            ( *inputs[z] )[x][y] = T( t );
            ( *floors[z] )[x][y] = scene >= 2 && z%3 == 0 && ( x+y )%9 != 0;
            ( *reference[z] )[x][y] = T( prefilled ? float( key%16 )*0.07f : 0.0f );
            ( *candidate[z] )[x][y] = ( *reference[z] )[x][y];
        }
    }
    std::uint64_t input_identity() const {
        // Includes every input byte, floors and all Z levels; inputs are owned
        // and fully initialized before hashing, including both fragment floats.
        std::uint64_t h = 1469598103934665603ULL;
        auto mix = [&]( const void *p, std::size_t n ) {
            auto b = static_cast<const unsigned char *>( p );
            for( std::size_t i = 0; i < n; ++i ) h = ( h ^ b[i] ) * 1099511628211ULL;
        };
        for( int z = 0; z < OVERMAP_LAYERS; ++z ) { mix( inputs[z].get(), sizeof( grid ) ); mix( floors[z].get(), sizeof( floors_grid ) ); }
        return h;
    }
    void compare() const {
        for( int z = 0; z < OVERMAP_LAYERS; ++z )
            require( std::memcmp( reference[z].get(), candidate[z].get(), sizeof( grid ) ) == 0,
                     "all Native output floats must be bitwise equal, including unseen/boundary/prefilled cells" );
    }
};
struct pair_result { record before, after; double reference_ms, candidate_ms; };
template<class T> pair_result run_pair( world<T> &w, tripoint_bub_ms origin, int offset,
                                       vertical_direction dir, T numerator ) {
    const auto identity = w.input_identity();
    pair_result result{};
    active = &result.before;
    const auto begin = std::chrono::steady_clock::now();
    if constexpr( std::is_same_v<T, float> )
        native_sight_reference::cast_zlight<T, sight_calc, check, accumulate>( w.expected, w.in, w.floor, origin, offset, numerator, dir );
    else native_sight_reference::cast_zlight<T, shrapnel_calc, shrapnel_check, accumulate_fragment_cloud>( w.expected, w.in, w.floor, origin, offset, numerator, dir );
    const auto middle = std::chrono::steady_clock::now();
    active = &result.after;
    if constexpr( std::is_same_v<T, float> )
        native_sight_candidate::cast_zlight<T, sight_calc, check, accumulate>( w.actual, w.in, w.floor, origin, offset, numerator, dir );
    else native_sight_candidate::cast_zlight<T, shrapnel_calc, shrapnel_check, accumulate_fragment_cloud>( w.actual, w.in, w.floor, origin, offset, numerator, dir );
    const auto end = std::chrono::steady_clock::now();
    active = nullptr;
    result.reference_ms = std::chrono::duration<double, std::milli>( middle-begin ).count();
    result.candidate_ms = std::chrono::duration<double, std::milli>( end-middle ).count();
    w.compare();
    require( identity == w.input_identity(), "complete input/floor grids unchanged" );
    require( result.before.requests == result.after.requests, "same number of distance/calc requests" );
    require( result.before.trace == result.after.trace && result.before.checks == result.after.checks && result.before.accumulates == result.after.accumulates,
             "same check/accumulate values and order, including fragment fallback side effects" );
    require( result.after.exponent_calls <= result.before.exponent_calls, "never add exponent evaluations" );
    if constexpr( std::is_same_v<T, float> ) require( result.before.requests == result.before.exponent_calls, "original evaluates one exponent per distance/calc request" );
    ++cases;
    return result;
}
float custom_calc( const float &a, const float &b, const int &d ) {
    if( active ) { ++active->checks; active->add( bits( a ) ); }
    if( d < 0 ) throw std::runtime_error( "owned callback exception" );
    return a+b+d;
}
struct throwing_exp {
    static int calls;
    float operator()( float v ) const {
        ++calls;
        if( v == 2.0f ) throw std::runtime_error( "owned evaluator exception" );
        return std::exp( v );
    }
};
int throwing_exp::calls = 0;
struct exceptional_exp {
    static int calls;
    float operator()( float ) const { ++calls; return 0.0f; }
};
int exceptional_exp::calls = 0;
void primitive_semantics() {
    using cache = ncmm_graphics_shadowcasting::exact_exp_cache<counted_exp>;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for( float v : {0.0f, -0.0f, -80.0f, 80.0f, -81.0f, 81.0f, -104.0f, 100.0f, nan, infinity, -infinity,
                    std::numeric_limits<float>::denorm_min(), 0.038f, 2.3f} ) {
        cache c;
        for( int i = 0; i < 3; ++i ) {
            const float expected = std::exp( v ), actual = c( v );
            require( std::isnan( expected ) ? std::isnan( actual ) : bits( expected ) == bits( actual ), "exact exponent primitive values including exceptional fallback" );
        }
    }
    ncmm_graphics_shadowcasting::calc_cache<float, sight_calc, counted_exp> c;
    for( float numerator : {0.0f, -0.0f, 1.0f, -1.0f, 3.7f, nan, infinity} )
        for( float t : {0.0f, -0.0f, 0.038f, 0.38f} ) for( int d : {0, 1, 60} ) {
            const float expected = numerator/std::exp( t*d ), actual = c( numerator, t, d );
            require( std::isnan( expected ) ? std::isnan( actual ) : bits( expected ) == bits( actual ), "numerator changes still divide every time" );
        }
    record r; active = &r;
    ncmm_graphics_shadowcasting::calc_cache<float, custom_calc, counted_exp> custom;
    require( custom( 1, 2, 3 ) == 6 && custom( 1, 2, 3 ) == 6 && r.checks == 2 && r.exponent_calls == 0,
             "generic float callback runs every time with side effects" );
    bool caught = false; try { custom( 1, 2, -1 ); } catch( const std::runtime_error & ) { caught = true; }
    active = nullptr; require( caught, "generic callback exceptions propagate" );
    static_assert( sizeof( ncmm_graphics_shadowcasting::exact_exp_cache<> ) <= 16 );
    static_assert( std::is_trivially_copyable_v<ncmm_graphics_shadowcasting::exact_exp_cache<>> );
}
void primitive_reuse() {
    record r; active = &r;
    ncmm_graphics_shadowcasting::exact_exp_cache<counted_exp> c;
    c( 0.0f ); c( 0.0f ); c( -0.0f ); c( -0.0f );
    require( r.exponent_calls == 2, "cache key preserves signed-zero bit identity" );
    c( 2.3f ); c( 2.3f ); c( 2.4f ); c( 2.3f );
    require( r.exponent_calls == 5, "single entry invalidates on any exact exponent change" );
    for( float v : {-81.0f, 81.0f, -104.0f, 100.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()} ) {
        const auto before = r.exponent_calls; c( v ); c( v );
        require( r.exponent_calls == before+2, "unsafe exponents always execute the original evaluator" );
    }
    ncmm_graphics_shadowcasting::exact_exp_cache<counted_exp> independent;
    const auto before = r.exponent_calls;
    independent( 2.3f ); independent( 2.3f );
    require( r.exponent_calls == before+1, "new cache has no persistent/shared warm state" );
    active = nullptr;
    ncmm_graphics_shadowcasting::exact_exp_cache<throwing_exp> throwing;
    throwing( 1.0f ); throwing( 1.0f );
    bool caught = false; try { throwing( 2.0f ); } catch( const std::runtime_error & ) { caught = true; }
    require( caught && throwing_exp::calls == 2, "miss evaluator exception propagates without publishing a cache value" );
    require( bits( throwing( 1.0f ) ) == bits( std::exp( 1.0f ) ) && throwing_exp::calls == 2,
             "an exception leaves the previous valid denominator intact" );
    ncmm_graphics_shadowcasting::exact_exp_cache<exceptional_exp> exceptional;
    exceptional( 1.0f ); exceptional( 1.0f );
    require( exceptional_exp::calls == 2, "non-normal evaluated results are never cached" );
    ncmm_graphics_shadowcasting::exact_exp_cache<> standard;
    errno = E2BIG; standard( 1.0f );
    require( errno == E2BIG, "normal miss retains unrelated errno" );
    errno = EDOM; standard( 1.0f );
    require( errno == EDOM, "normal hit retains current unrelated errno" );
}
void diagnostic_reference_semantics() {
    world<float> w;
    for( bool metric : {false, true} ) for( int scene : {0, 2, 3, 4} ) {
        circular = metric; w.fill( scene, true );
        const auto identity = w.input_identity();
        const int offset = scene >= 3 ? 36 : 0;
        record original, diagnostic;
        active = &original;
        native_sight_reference::cast_zlight<float, sight_calc, sight_check, accumulate_transparency>(
            w.expected, w.in, w.floor, {66, 66, 0}, offset, 1.0f, vertical_direction::BOTH );
        active = &diagnostic;
        ncmm_graphics_shadowcasting::reference_cast(
            w.actual, w.in, w.floor, {66, 66, 0}, offset, 1.0f, vertical_direction::BOTH );
        active = nullptr;
        w.compare();
        require( identity == w.input_identity(), "diagnostic reference preserves every input/floor byte" );
        require( original.requests == diagnostic.requests && original.trace == diagnostic.trace,
                 "diagnostic reference uses the complete Native traversal and distance order" );
        require( original.exponent_calls == original.requests &&
                 diagnostic.exponent_calls == original.exponent_calls,
                 "reference wrapper must execute every original exponent, never the cache specialization" );
    }
}
void run() {
    primitive_semantics();
    world<float> w;
    std::uint64_t before = 0, after = 0;
    const tripoint_bub_ms origins[] = {{66,66,0}, {0,0,-10}, {131,131,10}, {1,130,-1}};
    const vertical_direction dirs[] = {vertical_direction::UP, vertical_direction::DOWN, vertical_direction::BOTH};
    for( bool metric : {false, true} ) {
        circular = metric;
        for( int scene = 0; scene < 5; ++scene ) for( const auto origin : origins )
            for( int offset : {0, 52, 60, 61} ) for( auto dir : dirs ) {
                w.fill( scene, ( cases%2 ) != 0 );
                // Keep full 60-tile coverage for air/solid/structured walls.
                // Fine-grained transparency permutations split many more spans;
                // cover those at radius24 as well as the zero/negative radii.
                const int effective_offset = scene >= 3 && offset == 0 ? 36 : offset;
                const auto result = run_pair( w, origin, effective_offset, dir, ( cases%3 == 0 ) ? 1.0f : 3.7f );
                before += result.before.exponent_calls; after += result.after.exponent_calls;
                if( cases == 1 ) std::cout << "First owned-model pair ms: " << result.reference_ms << " -> " << result.candidate_ms << "; not game/frame/paired-hardware acceptance" << std::endl;
                if( cases%100 == 0 ) std::cout << "Native semantic cases completed: " << cases << std::endl;
            }
    }
    world<fragment_cloud> fragment;
    for( bool metric : {false,true} ) for( auto dir : dirs ) for( int scene : {0,2,4} ) {
        circular = metric; fragment.fill( scene, true );
        run_pair( fragment, {66,66,0}, 48, dir, fragment_cloud( 5.0f, 0.8f ) );
    }
    std::cout << "Complete Native semantic matrix: " << cases << " paired cases / " << checks
              << " checks passed before work acceptance; exponent evaluations " << before << " -> " << after << '\n';
    require( before > 100000 && after < before/2, "reduce repeated exact exponent work in full Native body (fail-before control)" );
    primitive_reuse();
    diagnostic_reference_semantics();
    std::cout << "Exact sight denominator cache: " << cases << " pairs / " << checks
              << " checks passed; owned context, not game/RTX/frame timings\n";
}
}
int main() {
    try { ncmm_shadow_fixture::run(); return 0; }
    catch( const std::exception &e ) { std::cerr << e.what() << '\n'; return 1; }
}
