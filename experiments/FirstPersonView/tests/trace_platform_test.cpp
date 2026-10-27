#if defined(_WIN32) && !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include "ncmm_graphics_trace.hpp"
#include "ncmm_graphics_move_diagnostics.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <vector>
#if defined(_WIN32)
// CDDA includes other SDK headers after the trace wrapper. FAR must retain its
// Win32 meaning even when the native near/far identifiers are restored.
#include <objbase.h>
#endif
enum class native_distance { near, far };
void ncmm_move_diagnostics_cross_tu();

namespace allocation_probe { inline bool enabled = false; inline std::size_t calls = 0; }
void *operator new( std::size_t size )
{
    if( allocation_probe::enabled ) ++allocation_probe::calls;
    if( void *p = std::malloc( size ? size : 1 ) ) return p;
    throw std::bad_alloc();
}
void *operator new[]( std::size_t size ) { return ::operator new( size ); }
void operator delete( void *p ) noexcept { std::free( p ); }
void operator delete[]( void *p ) noexcept { std::free( p ); }
void operator delete( void *p, std::size_t ) noexcept { std::free( p ); }
void operator delete[]( void *p, std::size_t ) noexcept { std::free( p ); }
void *operator new( std::size_t size, const std::nothrow_t & ) noexcept
{ try { return ::operator new( size ); } catch( ... ) { return nullptr; } }
void *operator new[]( std::size_t size, const std::nothrow_t & ) noexcept
{ try { return ::operator new[]( size ); } catch( ... ) { return nullptr; } }
void operator delete( void *p, const std::nothrow_t & ) noexcept { ::operator delete( p ); }
void operator delete[]( void *p, const std::nothrow_t & ) noexcept { ::operator delete[]( p ); }

namespace native_update_fixture {
namespace diag = ncmm_graphics_move_diagnostics;
std::size_t checks = 0, clock_reads = 0;
void require( bool ok, const char *reason )
{
    ++checks;
    if( !ok ) throw std::runtime_error( reason );
}
diag::clock::time_point test_now() noexcept
{ return diag::clock::time_point( std::chrono::nanoseconds( static_cast<long long>( ++clock_reads ) * 100 ) ); }
struct point_rel_sm {
    int a = 0, b = 0;
    point_rel_sm() = default;
    point_rel_sm( int x, int y ) : a( x ), b( y ) {}
    int &x() { return a; } int &y() { return b; }
    int x() const { return a; } int y() const { return b; }
    static const point_rel_sm zero;
    point_rel_sm &operator-=( point_rel_sm v ) { a -= v.a; b -= v.b; return *this; }
};
const point_rel_sm point_rel_sm::zero{};
bool operator==( point_rel_sm a, point_rel_sm b ) { return a.a == b.a && a.b == b.b; }
bool operator!=( point_rel_sm a, point_rel_sm b ) { return !( a == b ); }
struct point_rel_ms {
    int a = 0, b = 0;
    point_rel_ms operator-() const { return {-a, -b}; }
    point_rel_ms raw() const { return *this; }
};
struct tripoint_bub_ms {
    int a = 0, b = 0, c = 0;
    tripoint_bub_ms() = default;
    tripoint_bub_ms( int x, int y, int z ) : a( x ), b( y ), c( z ) {}
    int x() const { return a; } int y() const { return b; } int z() const { return c; }
};
struct tripoint_rel_sm { point_rel_sm xy; int z; };
template<class T> struct inclusive_rectangle { T lo, hi; inclusive_rectangle( T a, T b ) : lo( a ), hi( b ) {} };
point_rel_sm clamp( point_rel_sm p, inclusive_rectangle<point_rel_sm> r )
{ return {std::clamp( p.a, r.lo.a, r.hi.a ), std::clamp( p.b, r.lo.b, r.hi.b )}; }
namespace coords {
struct ms {};
template<class> point_rel_ms project_to( point_rel_sm p ) { return {p.a * 12, p.b * 12}; }
}
constexpr int HALF_MAPSIZE_X = 60, HALF_MAPSIZE_Y = 60, SEEX = 12, SEEY = 12;
constexpr int MAPSIZE_X = 132, MAPSIZE_Y = 132, OVERMAP_DEPTH = 10, OVERMAP_HEIGHT = 10;
struct world;
struct map {
    world *state;
    void shift( point_rel_sm );
    tripoint_bub_ms get_abs_sub();
    void invalidate_map_cache( int ); void build_map_cache( int ); void spawn_monsters( bool );
};
struct avatar {
    world *state; std::array<int, 3> absolute{}; int moves = 90123;
    void shift_destination( point_rel_ms ); void setpos( map &, tripoint_bub_ms );
};
struct npc {
    world *state; int id; tripoint_bub_ms position;
    void shift( point_rel_sm ); tripoint_bub_ms pos_bub( map & ); void on_unload();
};
struct tracker { std::vector<std::shared_ptr<npc>> active_npc; };
struct scent_map { world *state; void shift( point_rel_ms ); };
struct world {
    std::vector<std::array<int, 5>> trace;
    point_rel_sm origin{}; int level = 0;
    map here{this}; avatar player{this}; tracker creatures; scent_map scent{this};
    bool throw_load = false, nested = false, entered_nested = false;
    point_rel_sm ( *nested_call )( void * ) = nullptr; void *nested_game = nullptr;
    void log( int kind, int a = 0, int b = 0, int c = 0, int d = 0 ) { trace.push_back( {kind, a, b, c, d} ); }
    world()
    {
        trace.reserve( 1024 );
        creatures.active_npc.push_back( std::make_shared<npc>( npc{this, 1, {64, 64, 0}} ) );
        creatures.active_npc.push_back( std::make_shared<npc>( npc{this, 2, {1, 1, 0}} ) );
    }
};
world *current = nullptr;
map &get_map() { current->log( 0 ); return current->here; }
void map::shift( point_rel_sm p ) { state->log( 1, p.a, p.b ); state->origin.a += p.a; state->origin.b += p.b; }
tripoint_bub_ms map::get_abs_sub() { state->log( 2, state->level ); return {state->origin.a, state->origin.b, state->level}; }
void map::invalidate_map_cache( int z ) { state->log( 3, z ); }
void map::build_map_cache( int z ) { state->log( 4, z ); }
void map::spawn_monsters( bool flag ) { state->log( 5, flag ); }
void avatar::shift_destination( point_rel_ms p ) { state->log( 6, p.a, p.b ); }
void avatar::setpos( map &, tripoint_bub_ms p )
{ state->log( 7, p.a, p.b, p.c ); absolute = {p.a + state->origin.a * 12, p.b + state->origin.b * 12, p.c}; }
void npc::shift( point_rel_sm p ) { state->log( 8, id, p.a, p.b ); position.a -= p.a * 12; position.b -= p.b * 12; }
tripoint_bub_ms npc::pos_bub( map & ) { state->log( 9, id, position.a, position.b, position.c ); return position; }
void npc::on_unload() { state->log( 10, id ); }
void scent_map::shift( point_rel_ms p ) { state->log( 11, p.a, p.b ); }
struct game_state {
    world &state; avatar &u; tracker *critter_tracker; scent_map &scent;
    explicit game_state( world &w ) : state( w ), u( w.player ), critter_tracker( &w.creatures ), scent( w.scent ) {}
    void shift_monsters( tripoint_rel_sm p ) { state.log( 12, p.xy.a, p.xy.b, p.z ); }
    void load_npcs()
    {
        state.log( 13 );
        if( state.throw_load ) throw std::runtime_error( "injected Native load exception" );
        if( state.nested && !state.entered_nested ) {
            state.entered_nested = true; state.nested_call( state.nested_game );
        }
    }
    void update_overmap_seen() { state.log( 14 ); }
};
namespace ncmm {
// Stage14 suppresses renderer notifications around the same Native cache
// writes. It neither adds nor removes a Native operation in this oracle.
struct graphics_native_cache_reset { graphics_native_cache_reset( const map *, int ) noexcept {} };
}
namespace frozen {
struct game : game_state { using game_state::game_state; point_rel_sm update_map( int &, int &, bool ); };
// BEGIN FROZEN NATIVE UPDATE MAP
point_rel_sm game::update_map( int &x, int &y, bool z_level_changed )
{
    map &here = get_map();

    point_rel_sm shift;

    while( x < HALF_MAPSIZE_X ) {
        x += SEEX;
        shift.x()--;
    }
    while( x >= HALF_MAPSIZE_X + SEEX ) {
        x -= SEEX;
        shift.x()++;
    }
    while( y < HALF_MAPSIZE_Y ) {
        y += SEEY;
        shift.y()--;
    }
    while( y >= HALF_MAPSIZE_Y + SEEY ) {
        y -= SEEY;
        shift.y()++;
    }

    if( shift == point_rel_sm::zero ) {
        // adjust player position
        u.setpos( here, tripoint_bub_ms( x, y, here.get_abs_sub().z() ) );
        if( z_level_changed ) {
            // Update what parts of the world map we can see
            // We may be able to see farther now that the z-level has changed.
            update_overmap_seen();
        }
        // Not actually shifting the submaps, all the stuff below would do nothing
        return point_rel_sm::zero;
    }

    // this handles loading/unloading submaps that have scrolled on or off the viewport
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    inclusive_rectangle<point_rel_sm> size_1( {-1, -1}, { 1, 1 } );
    point_rel_sm remaining_shift = shift;
    while( remaining_shift != point_rel_sm::zero ) {
        point_rel_sm this_shift = clamp( remaining_shift, size_1 );
        here.shift( point_rel_sm( this_shift ) );
        remaining_shift -= this_shift;
    }

    // Shift monsters
    shift_monsters( { shift, 0 } );
    const point_rel_ms shift_ms = coords::project_to<coords::ms>( shift );
    u.shift_destination( -shift_ms );

    // Shift NPCs
    for( auto it = critter_tracker->active_npc.begin(); it != critter_tracker->active_npc.end(); ) {
        ( *it )->shift( shift );
        const tripoint_bub_ms pos = ( *it )->pos_bub( here );

        // TODO: This could probably be replaced by an inbounds check.
        if( pos.x() < 0 || pos.x() >= MAPSIZE_X ||
            pos.y() < 0 || pos.y() >= MAPSIZE_Y ) {
            //Remove the npc from the active list. It remains in the overmap list.
            ( *it )->on_unload();
            it = critter_tracker->active_npc.erase( it );
        } else {
            ++it;
        }
    }

    scent.shift( shift_ms );

    // Also ensure the player is on current z-level
    // m.get_abs_sub().z should later be removed, when there is no longer such a thing
    // as "current z-level"
    u.setpos( here, tripoint_bub_ms( x, y, here.get_abs_sub().z() ) );

    // Only do the loading after all coordinates have been shifted.

    // Check for overmap saved npcs that should now come into view.
    // Put those in the active list.
    load_npcs();

    // Make sure map cache is consistent since it may have shifted.
    for( int zlev = -OVERMAP_DEPTH; zlev <= OVERMAP_HEIGHT; ++zlev ) {
        here.invalidate_map_cache( zlev );
    }
    here.build_map_cache( here.get_abs_sub().z() );

    // Spawn monsters if appropriate
    // This call will generate new monsters in addition to loading, so it's placed after NPC loading
    here.spawn_monsters( false ); // Static monsters

    // Update what parts of the world map we can see
    update_overmap_seen();

    return shift;
}
// END FROZEN NATIVE UPDATE MAP
} // namespace frozen
namespace actual {
struct game : game_state { using game_state::game_state; point_rel_sm update_map( int &, int &, bool ); };
#include "ncmm_graphics_update_map.inc"
} // namespace actual
struct outcome {
    std::vector<std::array<int, 5>> trace;
    std::array<int, 3> absolute{};
    int moves = 0, x = 0, y = 0; point_rel_sm returned, origin;
    std::vector<int> npcs; bool threw = false;
};
template<class Game> outcome run( int x, int y, bool z_changed, diag::action_record *record = nullptr,
                                  bool nested = false, bool fail = false )
{
    world w; current = &w; Game g( w );
    w.player.absolute = {x, y, 0}; w.throw_load = fail; w.nested = nested; w.nested_game = &g;
    w.nested_call = []( void *p ) { int a = 65, b = 65; return static_cast<Game *>( p )->update_map( a, b, false ); };
    outcome out;
    auto call = [&]() { try { out.returned = g.update_map( x, y, z_changed ); } catch( const std::runtime_error & ) { out.threw = true; } };
    if( record ) { diag::action_capture capture( *record, test_now ); call(); }
    else call();
    out.trace = w.trace; out.absolute = w.player.absolute; out.moves = w.player.moves;
    out.x = x; out.y = y; out.origin = w.origin;
    for( const auto &person : w.creatures.active_npc ) out.npcs.push_back( person->id );
    current = nullptr; return out;
}
void same_native( const outcome &a, const outcome &b )
{
    require( a.trace == b.trace, "Native calls, arguments and order remain exact" );
    require( a.absolute == b.absolute && a.moves == b.moves && a.moves == 90123,
             "Native position and untouched action cost remain exact" );
    require( a.x == b.x && a.y == b.y && a.returned == b.returned && a.origin == b.origin &&
             a.npcs == b.npcs && a.threw == b.threw, "Native return, rebased coordinates, NPCs and exceptions remain exact" );
}
bool qualifies( const diag::action_record &record )
{
    if( !record.claimed || record.reused_storage || record.nested_capture || record.counter_overflow ||
        record.invocations != 1 || record.recorded != 1 || record.dropped || record.max_depth != 1 ||
        record.exceptional_invocations || record.protocol_errors ) return false;
    const auto &e = record.events[0];
    if( e.ordinal || e.depth != 1 || e.parent_ordinal != -1 || !e.shift_known || !e.complete || e.exceptional ) return false;
    const bool zero = e.shift_xy == std::array<int, 2>{0, 0};
    const std::vector<diag::boundary> tags = zero ? std::vector<diag::boundary>{diag::boundary::entry, diag::boundary::route,
        diag::boundary::zero_position, diag::boundary::zero_overmap, diag::boundary::exit} :
        std::vector<diag::boundary>{diag::boundary::entry, diag::boundary::route, diag::boundary::shift,
        diag::boundary::rebase_load_npcs, diag::boundary::invalidate, diag::boundary::build, diag::boundary::spawn,
        diag::boundary::overmap, diag::boundary::exit};
    if( e.count != tags.size() ) return false;
    for( std::size_t i = 0; i < tags.size(); ++i ) {
        if( e.tags[i] != tags[i] || ( i && e.points[i] < e.points[i - 1] ) ) return false;
    }
    for( std::size_t axis = 0; axis < 2; ++axis )
        if( std::int64_t( e.input_xy[axis] ) - e.output_xy[axis] != std::int64_t( e.shift_xy[axis] ) * 12 ) return false;
    return true;
}
void diagnostic_tests()
{
    require( diag::active_capture == nullptr, "diagnostics default to disabled" );
    const auto initial_clocks = clock_reads, initial_allocations = allocation_probe::calls;
    allocation_probe::enabled = true;
    for( int i = 0; i < 64; ++i ) {
        int x = 60, y = 60; diag::update_scope disabled( x, y, false );
        if( disabled ) { disabled.route( 0, 0 ); disabled.mark( diag::boundary::zero_position ); }
    }
    allocation_probe::enabled = false;
    require( clock_reads == initial_clocks && allocation_probe::calls == initial_allocations,
             "disabled probes perform no clocks or allocations" );
    diag::action_record cross_tu;
    { diag::action_capture capture( cross_tu, test_now ); ncmm_move_diagnostics_cross_tu(); }
    require( qualifies( cross_tu ), "one external inline TLS capture is shared across real translation units" );
    std::size_t pairs = 0, captured = 0;
    for( int x : {59, 60, 71, 72, 84} ) for( int y : {-1, 59, 60, 71, 72, 145} ) for( bool z : {false, true} ) {
        const auto frozen = run<frozen::game>( x, y, z );
        same_native( frozen, run<actual::game>( x, y, z ) );
        diag::action_record record; const auto observed = run<actual::game>( x, y, z, &record );
        same_native( frozen, observed ); ++pairs; captured += record.invocations;
        // Run every Native oracle pair before the fail-before hook assertion.
        if( record.recorded ) {
            require( qualifies( record ), "complete adjacent scopes cover the actual Native branch" );
            const auto &e = record.events[0];
            require( e.input_xy == std::array<int, 2>{x, y} && e.output_xy == std::array<int, 2>{observed.x, observed.y} &&
                     e.shift_xy == std::array<int, 2>{observed.returned.a, observed.returned.b} && e.z_level_changed == z,
                     "per-call route and actual shift belong to the same Native invocation" );
        }
    }
    std::cout << "Native update_map actual-source oracle: " << pairs << " pairs, captured invocations " << captured << " / " << pairs << '\n';
    require( captured == pairs, "actual-source internal hook must capture every Native invocation (fail-before control)" );
    diag::action_record complete; run<actual::game>( 59, 72, false, &complete ); require( qualifies( complete ), "shift qualification" );
    auto bad = complete; bad.events[0].count = 0; require( !qualifies( bad ), "missing boundaries are rejected" );
    bad = complete; bad.events[0].tags[4] = diag::boundary::build; require( !qualifies( bad ), "misassigned adjacent scopes are rejected" );
    bad = complete; bad.events[0].points[3] = bad.events[0].points[2] - std::chrono::nanoseconds( 1 );
    require( !qualifies( bad ), "overlapping or backward scopes are rejected" );
    diag::action_record nested;
    same_native( run<frozen::game>( 59, 60, false, nullptr, true ), run<actual::game>( 59, 60, false, &nested, true ) );
    require( nested.invocations == 2 && nested.recorded == 2 && nested.max_depth == 2 && nested.events[1].parent_ordinal == 0 &&
             nested.events[1].depth == 2 && !qualifies( nested ), "reentrant Native calls preserve ancestry and reject independent timing qualification" );
    diag::action_record exceptional;
    same_native( run<frozen::game>( 59, 60, false, nullptr, false, true ), run<actual::game>( 59, 60, false, &exceptional, false, true ) );
    require( exceptional.exceptional_invocations == 1 && exceptional.events[0].exceptional && !exceptional.events[0].complete &&
             !qualifies( exceptional ) && diag::active_capture == nullptr, "exception unwinding restores disabled capture without invented boundaries" );
    world w; current = &w; actual::game g( w ); diag::action_record overflow;
    { diag::action_capture capture( overflow, test_now );
      for( int i = 0; i < 12; ++i ) { int x = 65, y = 65; require( g.update_map( x, y, false ) == point_rel_sm::zero, "overflow retains Native result" ); } }
    current = nullptr;
    require( overflow.invocations == 12 && overflow.recorded == diag::event_capacity && overflow.dropped == 4 && !qualifies( overflow ),
             "bounded capture counts dropped invocations without overwriting storage" );
    diag::action_record outer, inner;
    { diag::action_capture first( outer, test_now );
      { diag::action_capture second( inner, test_now ); require( diag::active_capture == &second, "nested stack capture active" ); }
      require( diag::active_capture == &first, "nested stack capture restores parent" ); }
    require( outer.nested_capture && inner.nested_capture && diag::active_capture == nullptr, "nested capture metadata is honest" );
    { diag::action_capture reuse( complete, test_now ); }
    require( complete.reused_storage && !qualifies( complete ), "receipt reuse cannot silently append to another action" );
    std::cout << "Native update_map diagnostics: " << checks << " checks passed; injected clock values are not performance measurements\n";
}
} // namespace native_update_fixture
int main(int argc,char **argv)
{
    const bool capture=argc>1&&std::strcmp(argv[1],"capture")==0;
#if defined(_WIN32)
    _putenv_s("NCMM_FP_TRACE_CAPTURE_SCENE",capture?"quiet":"");
#else
    if(capture)setenv("NCMM_FP_TRACE_CAPTURE_SCENE","quiet",1);else unsetenv("NCMM_FP_TRACE_CAPTURE_SCENE");
#endif
    ncmm_graphics_trace::note_present();ncmm_graphics_trace::note_present();
    if(ncmm_graphics_trace::presented_frames!=(capture?2u:0u))return 2;
    if(capture&&std::strcmp(ncmm_graphics_trace::capture_label(),"FP_SCENE:quiet"))return 3;
    try { native_update_fixture::diagnostic_tests(); }
    catch( const std::exception &e ) { std::cerr << e.what() << '\n'; return 4; }
    ncmm_graphics_trace::cpu_zone scope("Trace SDK include compatibility");
    return native_distance::near == native_distance::far;
}
