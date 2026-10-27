#include "ncmm_graphics_support_rebase.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

std::size_t ncmm_test_allocation_calls() noexcept;
bool ncmm_test_allocation_enable( bool value ) noexcept;
namespace native_support_rebase_test {
std::size_t checks = 0, cases = 0;
void require( bool value, const char *why )
{
    ++checks;
    if( !value ) throw std::runtime_error( std::string( "Native support rebase: " ) + why );
}
struct offset { int x, y; };
template<bool ParityOrder> struct position {
    int x, y, z;
    position operator+( offset p ) const noexcept { return {x+p.x, y+p.y, z}; }
    bool operator==( const position &p ) const noexcept { return x == p.x && y == p.y && z == p.z; }
    bool operator<( const position &p ) const noexcept
    {
        // The second order deliberately changes under odd translations. The
        // helper's insertion hint must remain correct even when it is unhelpful.
        if constexpr( ParityOrder ) return std::make_tuple( x & 1, z, y, x ) < std::make_tuple( p.x & 1, p.z, p.y, p.x );
        return std::tie( z, y, x ) < std::tie( p.z, p.y, p.x );
    }
};
template<class Position> std::set<Position> make_positions( std::size_t count )
{
    std::set<Position> out;
    for( std::size_t i = 0; i < count; ++i ) out.insert( {int( i % 257 )-128, int( i / 257 )-128, int( i % 21 )-10} );
    require( out.size() == count, "owned input coordinates are distinct" );
    return out;
}
// Independent Native loop from both pinned map::shift bodies, lines9168-9174
// (1040) /9180-9186 (1807). Cataclysm: DDA contributors, CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
template<class Position> void frozen_rebase( std::set<Position> &positions, offset delta )
{
    if( !positions.empty() ) {
        std::set<Position> previous = std::move( positions );
        positions.clear();
        for( const Position &p : previous ) positions.insert( p + delta );
    }
}
struct allocation_window {
    bool previous = ncmm_test_allocation_enable( true );
    std::size_t start = ncmm_test_allocation_calls();
    ~allocation_window() { ncmm_test_allocation_enable( previous ); }
    std::size_t calls() const noexcept { return ncmm_test_allocation_calls()-start; }
};
template<class Position> void matrix()
{
    for( std::size_t count : {0u, 1u, 12u, 144u, 4096u} )
        for( int dx : {-12, 0, 12} ) for( int dy : {-12, 0, 12} ) {
            auto before = make_positions<Position>( count );
            auto expected = before, actual = before;
            frozen_rebase( expected, {dx, dy} );
            ncmm_graphics_support_cache::rebase( actual, offset{dx, dy} );
            require( actual == expected && actual.size() == before.size(), "complete sorted coordinates equal the independent Native loop" );
            for( const auto &p : before ) require( actual.count( p + offset{dx, dy} ) == 1, "every translated XYZ is present exactly once, including negative/outside-bubble positions" );
            ++cases;
        }
}
void run_cases()
{
    matrix<position<false>>(); matrix<position<true>>();
    std::cout << "Native support rebase semantic matrix: " << cases << " cases passed before allocation acceptance\n";
    using point = position<false>;
    auto before = make_positions<point>( 8192 );
    auto original = before, actual = before;
    std::set<std::uintptr_t> addresses;
    for( const auto &p : actual ) addresses.insert( reinterpret_cast<std::uintptr_t>( std::addressof( p ) ) );
    std::size_t baseline_allocations = 0, actual_allocations = 0;
    { allocation_window probe; frozen_rebase( original, {12, -12} ); baseline_allocations = probe.calls(); }
    { allocation_window probe; ncmm_graphics_support_cache::rebase( actual, offset{12, -12} ); actual_allocations = probe.calls(); }
    require( original == actual, "allocation instrumentation preserves the exact Native coordinate result" );
    std::size_t reused = 0;
    for( const auto &p : actual ) reused += addresses.count( reinterpret_cast<std::uintptr_t>( std::addressof( p ) ) );
    std::cout << "Native support rebase allocations: " << baseline_allocations << " -> " << actual_allocations
              << "; retained value-node addresses " << reused << " / " << actual.size() << '\n';
    require( baseline_allocations >= before.size(), "the independent original allocates one replacement value node per coordinate" );
    // MSVC may allocate a constant sentinel when moving std::set. Accept that
    // implementation detail, but never another allocation per coordinate.
    require( actual_allocations <= 2 && reused == actual.size(), "reuse value nodes instead of allocating/copying the entire support queue (fail-before control)" );
    std::set<point> empty;
    std::size_t empty_allocations = 0;
    { allocation_window probe; ncmm_graphics_support_cache::rebase( empty, offset{12, 12} ); empty_allocations = probe.calls(); }
    require( empty.empty() && empty_allocations == 0, "an empty support queue performs no allocation" );
    std::cout << "Native support node rebase: " << cases << " cases / " << checks << " checks; allocation counts are not Native timings\n";
}
}
void test_native_support_rebase() { native_support_rebase_test::run_cases(); }
