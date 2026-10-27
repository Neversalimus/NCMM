#include "ncmm_graphics_move_diagnostics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Complete pinned Native bodies run against owned API/state/ordered-call
// models. This does not reimplement loadn/actualize or measure Native timings.
// Primary code: Cataclysm: DDA contributors, CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L9059-L9185
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L9070-L9197
void ncmm_shift_diagnostics_cross_tu();
namespace native_map_shift_fixture {
namespace diag = ncmm_graphics_move_diagnostics;
constexpr int SEEX = 12, SEEY = 12, MAPSIZE = 11, MAPSIZE_X = 132;
constexpr int OVERMAP_DEPTH = 10, OVERMAP_HEIGHT = 10, levels = 21;
std::size_t checks = 0, pairs = 0;
std::int64_t ticks = 0;
std::uint64_t clock_reads = 0;
diag::clock::time_point test_now() noexcept
{
    ++clock_reads;
    return diag::clock::time_point( std::chrono::nanoseconds( ++ticks ) );
}
void require( bool value, const char *message )
{
    ++checks;
    if( !value ) throw std::runtime_error( std::string( "Native map::shift: " ) + message );
}
struct point_rel_sm {
    int a = 0, b = 0;
    point_rel_sm() = default;
    point_rel_sm( int x, int y ) : a( x ), b( y ) {}
    int x() const { return a; } int y() const { return b; }
    bool operator==( point_rel_sm p ) const { return a == p.a && b == p.b; }
    static const point_rel_sm zero;
};
const point_rel_sm point_rel_sm::zero{};
struct point { int x, y; point( int a, int b ) : x( a ), y( b ) {} };
struct tripoint_rel_sm {
    int a, b, c;
    tripoint_rel_sm( int x, int y, int z ) : a( x ), b( y ), c( z ) {}
    tripoint_rel_sm operator+( point_rel_sm p ) const { return {a + p.a, b + p.b, c}; }
    std::string to_string() const { return std::to_string( a ) + "," + std::to_string( b ) + "," + std::to_string( c ); }
};
struct tripoint_abs_sm {
    int a = 400, b = -300, c = -2;
    tripoint_abs_sm operator+( point_rel_sm p ) const { return {a + p.a, b + p.b, c}; }
};
struct tripoint_bub_ms {
    int a, b, c;
    tripoint_bub_ms operator+( point p ) const { return {a + p.x, b + p.y, c}; }
    bool operator<( tripoint_bub_ms p ) const { return std::tie( a, b, c ) < std::tie( p.a, p.b, p.c ); }
};
enum class reconcile_scope { full_rebuild, additive };
enum call_kind { origin_get, unload, origin_set, preview, traps, remote_get, cache_get,
                 levels_clear, list_clear, bits_shift, grid_copy, grid_get, list_update,
                 load, levels_rebuild, remote_set, actualize, reconcile, debug, cleanup, call_count };
struct call {
    call_kind kind;
    std::array<int, 10> args{};
    std::string text;
    bool operator==( const call &other ) const { return kind == other.kind && args == other.args && text == other.text; }
};
struct vehicle { int id = 0; bool zones_dirty = false; };
struct submap { int id; };
struct bits { int z = 0, kind = 0; std::uint64_t value = 0; };
struct level_cache {
    std::vector<vehicle *> vehicle_list;
    bits map_memory_cache_dec, map_memory_cache_ter, field_cache;
    bool map_memory_sweep_pending = false;
};
struct scenario {
    int width = 3, pattern = 0;
    bool zlevels = true;
    call_kind fail = call_count;
    int fail_on = 1;
    bool recursive = false;
};
struct map_state;
map_state *current = nullptr;
struct game {
    map_state *owner;
    void shift_destination_preview( point p );
    vehicle *remoteveh();
    void setremoteveh( vehicle *v );
};
game *g = nullptr;
struct map_state {
    const scenario settings;
    const int my_MAPSIZE;
    const bool zlevels;
    tripoint_abs_sm origin;
    std::vector<call> trace;
    std::array<std::size_t, call_count> counts{};
    std::vector<std::unique_ptr<submap>> owned;
    std::vector<submap *> submap_slots;
    std::array<level_cache, levels> caches;
    std::array<vehicle, levels * 2> cars;
    std::set<tripoint_bub_ms> support_cache_dirty;
    std::vector<std::array<int, 3>> actualized;
    game owner_game{this};
    vehicle *remote = nullptr;
    int vehicle_levels = 7, next_id = 1;
    point destination{0, 0}, trap_shift{0, 0};
    diag::clock::time_point last_cleanup{};
    void ( *recursive_call )( map_state * ) = nullptr;
    bool recursed = false;
    explicit map_state( scenario s ) : settings( s ), my_MAPSIZE( s.width ), zlevels( s.zlevels ),
        submap_slots( std::size_t( my_MAPSIZE * my_MAPSIZE * levels ) )
    {
        trace.reserve( 20000 );
        for( std::size_t i = 0; i < cars.size(); ++i ) cars[i].id = int( i ) + 1;
        for( int z = 0; z < levels; ++z ) {
            auto &c = caches[z];
            c.vehicle_list = {&cars[z * 2], &cars[z * 2 + 1]};
            c.map_memory_cache_dec = {z - OVERMAP_DEPTH, 0, std::uint64_t( z + 11 )};
            c.map_memory_cache_ter = {z - OVERMAP_DEPTH, 1, std::uint64_t( z + 17 )};
            c.field_cache = {z - OVERMAP_DEPTH, 2, std::uint64_t( z + 23 )};
            c.map_memory_sweep_pending = settings.pattern == 3 && z % 2 == 0;
        }
        for( std::size_t i = 0; i < submap_slots.size(); ++i ) {
            owned.emplace_back( std::make_unique<submap>( submap{next_id++} ) );
            submap_slots[i] = settings.pattern == 1 && i % 7 == 0 ? nullptr : owned.back().get();
        }
        if( settings.pattern != 1 ) {
            remote = &cars[7];
            support_cache_dirty = {{-12, 0, -10}, {0, 0, 0}, {131, 131, 10}};
        }
    }
    void note( call_kind k, std::initializer_list<int> args = {}, std::string text = {} )
    {
        call entry{k, {}, std::move( text )};
        require( args.size() <= entry.args.size(), "bounded ordered-call fixture" );
        std::copy( args.begin(), args.end(), entry.args.begin() );
        trace.push_back( std::move( entry ) ); ++counts[k]; ++ticks;
        if( settings.fail == k && int( counts[k] ) == settings.fail_on ) throw std::runtime_error( "controlled Native callback exception" );
    }
    std::size_t index( tripoint_rel_sm p ) const
    {
        require( p.a >= 0 && p.a < my_MAPSIZE && p.b >= 0 && p.b < my_MAPSIZE &&
                 p.c >= -OVERMAP_DEPTH && p.c <= OVERMAP_HEIGHT, "Native grid coordinates stay in bounds" );
        return std::size_t( ( p.a * my_MAPSIZE + p.b ) * levels + p.c + OVERMAP_DEPTH );
    }
    tripoint_abs_sm get_abs_sub() { note( origin_get, {origin.a, origin.b, origin.c} ); return origin; }
    void set_abs_sub( tripoint_abs_sm p ) { note( origin_set, {p.a, p.b, p.c} ); origin = p; }
    void on_unload( tripoint_rel_sm p ) { note( unload, {p.a, p.b, p.c, origin.a, origin.b} ); }
    void shift_traps( point_rel_sm p ) { note( traps, {p.a, p.b} ); trap_shift.x += p.a; trap_shift.y += p.b; }
    level_cache *get_cache_lazy( int z )
    {
        note( cache_get, {z} );
        return settings.pattern == 2 || ( settings.pattern == 1 && z % 3 == 0 ) ? nullptr : &caches[std::size_t( z + OVERMAP_DEPTH )];
    }
    void clear_vehicle_level_caches() { note( levels_clear ); vehicle_levels = 0; }
    void clear_vehicle_list( int z ) { note( list_clear, {z} ); caches[std::size_t( z + OVERMAP_DEPTH )].vehicle_list.clear(); }
    void copy_grid( tripoint_rel_sm to, tripoint_rel_sm from )
    {
        auto *value = submap_slots[index( from )];
        note( grid_copy, {to.a, to.b, to.c, from.a, from.b, from.c, value ? value->id : 0} );
        submap_slots[index( to )] = value;
    }
    submap *get_submap_at_grid( tripoint_rel_sm p )
    {
        auto *value = submap_slots[index( p )]; note( grid_get, {p.a, p.b, p.c, value ? value->id : 0} ); return value;
    }
    void update_vehicle_list( submap *value, int z )
    {
        note( list_update, {value->id, z} );
        caches[std::size_t( z + OVERMAP_DEPTH )].vehicle_list.push_back( &cars[std::size_t( value->id ) % cars.size()] );
    }
    void loadn( point_rel_sm p, bool vehicles )
    {
        note( load, {p.a, p.b, int( vehicles ), origin.a, origin.b} );
        for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; ++z ) {
            owned.emplace_back( std::make_unique<submap>( submap{next_id++} ) );
            submap_slots[index( {p.a, p.b, z} )] = settings.pattern == 2 && z % 4 == 0 ? nullptr : owned.back().get();
        }
    }
    void rebuild_vehicle_level_caches() { note( levels_rebuild ); vehicle_levels = next_id; }
    void actualize( tripoint_rel_sm p )
    {
        note( call_kind::actualize, {p.a, p.b, p.c} ); actualized.push_back( {p.a, p.b, p.c} );
        if( settings.recursive && !recursed ) { recursed = true; recursive_call( this ); }
    }
    void reconcile_loaded_items( reconcile_scope scope ) { note( reconcile, {int( scope )} ); }
    std::vector<std::uint64_t> snapshot() const
    {
        std::vector<std::uint64_t> out;
        auto add = [&]( auto value ) { out.push_back( std::uint64_t( value ) ); };
        for( int v : {origin.a, origin.b, origin.c, destination.x, destination.y, trap_shift.x, trap_shift.y,
                      vehicle_levels, next_id, remote ? remote->id : 0, int( recursed )} ) add( v );
        for( auto *sm : submap_slots ) add( sm ? sm->id : 0 );
        for( const auto &v : cars ) { add( v.id ); add( v.zones_dirty ); }
        for( const auto &c : caches ) {
            add( c.map_memory_cache_dec.value ); add( c.map_memory_cache_ter.value ); add( c.field_cache.value );
            add( c.map_memory_sweep_pending ); add( c.vehicle_list.size() );
            for( auto *v : c.vehicle_list ) add( v->id );
        }
        add( support_cache_dirty.size() );
        for( const auto &p : support_cache_dirty ) { add( p.a ); add( p.b ); add( p.c ); }
        add( actualized.size() );
        for( const auto &p : actualized ) for( int v : p ) add( v );
        return out;
    }
};
void game::shift_destination_preview( point p ) { owner->note( preview, {p.x, p.y} ); owner->destination.x += p.x; owner->destination.y += p.y; }
vehicle *game::remoteveh() { owner->note( remote_get, {owner->remote ? owner->remote->id : 0} ); return owner->remote; }
void game::setremoteveh( vehicle *v ) { owner->note( remote_set, {v ? v->id : 0} ); owner->remote = v; }
void debugmsg( const char *text ) { current->note( debug, {}, text ); }
void debugmsg( const char *text, const std::string &where ) { current->note( debug, {}, std::string( text ) + ':' + where ); }
template<int Width, int Stride> void shift_bitset_cache( bits &value, point_rel_sm p )
{
    current->note( bits_shift, {value.z, value.kind, Width, Stride, p.a, p.b} );
    value.value = value.value * 131u + unsigned( p.a + 2 ) * 7u + unsigned( p.b + 2 );
}
// The Native local vector's cleanup is observed through a fixture-only type;
// it has real vector storage, and its destructor runs before the first local's
// diagnostic destructor. Its injected clock is explicitly not a measurement.
namespace observed_std {
using ::std::abs; using ::std::move; using ::std::set;
template<class T> struct vector : ::std::vector<T> {
    ~vector() noexcept
    {
        current->note( cleanup, {int( this->size() )} );
        current->last_cleanup = test_now();
    }
};
}

namespace frozen1040 {
namespace std = observed_std;
struct map : map_state { using map_state::map_state; void shift( const point_rel_sm &sp ); };
// BEGIN FROZEN NATIVE MAP SHIFT 1040
void map::shift( const point_rel_sm &sp )
{
    if( !zlevels ) {
        debugmsg( "map::shift called from map that doesn't support Z levels" );
        return;
    }

    // Special case of 0-shift; refresh the map
    if( sp == point_rel_sm::zero ) {
        return; // Skip this?
    }

    if( std::abs( sp.x() ) > 1 || std::abs( sp.y() ) > 1 ) {
        debugmsg( "map::shift called with a shift of more than one submap" );
        return;
    }

    const tripoint_abs_sm abs = get_abs_sub();
    std::vector<tripoint_rel_sm> loaded_grids;

    const int zmin = -OVERMAP_DEPTH;
    const int zmax = OVERMAP_HEIGHT;

    const int x_start = sp.x() >= 0 ? 0 : my_MAPSIZE - 1;
    const int x_stop = sp.x() >= 0 ? my_MAPSIZE : -1;
    const int x_step = sp.x() >= 0 ? 1 : -1;
    const int y_start = sp.y() >= 0 ? 0 : my_MAPSIZE - 1;
    const int y_stop = sp.y() >= 0 ? my_MAPSIZE : -1;
    const int y_step = sp.y() >= 0 ? 1 : -1;

    // Have to run on_unload before changing the abs_sub for the map.
    // TODO: can probably skip a bunch of iteration here.
    for( int gridx = x_start; gridx != x_stop; gridx += x_step ) {
        for( int gridy = y_start; gridy != y_stop; gridy += y_step ) {
            // The sp.x() and sp.y() check here establishes that we are moving
            // in the x or y direction respectively. If we are not moving in the x direction,
            // we don't want to call on_unload on grid points where gridx == x_start.
            if( ( sp.x() && gridx == x_start ) || ( sp.y() && gridy == y_start ) ) {
                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    const tripoint_rel_sm grid( gridx, gridy, gridz );
                    on_unload( grid );
                }
            }
        }
    }
    set_abs_sub( abs + sp );

    g->shift_destination_preview( { -sp.x() * SEEX, -sp.y() * SEEY } );

    shift_traps( sp );

    vehicle *remoteveh = g->remoteveh();

    for( int gridz = zmin; gridz <= zmax; gridz++ ) {
        level_cache *cache = get_cache_lazy( gridz );
        if( !cache ) {
            continue;
        }
        for( vehicle *veh : cache->vehicle_list ) {
            veh->zones_dirty = true;
        }
    }

    // Shift the map sx submaps to the right and sy submaps down.
    // sx and sy should never be bigger than +/-1.
    // absx and absy are our position in the world, for saving/loading purposes.
    clear_vehicle_level_caches();

    for( int gridz = zmin; gridz <= zmax; gridz++ ) {
        // Clear vehicle list and rebuild after shift
        // mlangsdorf 2020 - this is kind of insane, building the cache is not free, why are
        // we doing this?
        clear_vehicle_list( gridz );
        level_cache *cache = get_cache_lazy( gridz );
        if( cache ) {
            shift_bitset_cache<MAPSIZE_X, SEEX>( cache->map_memory_cache_dec, sp );
            shift_bitset_cache<MAPSIZE_X, SEEX>( cache->map_memory_cache_ter, sp );
            shift_bitset_cache<MAPSIZE, 1>( cache->field_cache, sp );
        }
    }

    for( int gridx = x_start; gridx != x_stop; gridx += x_step ) {
        for( int gridy = y_start; gridy != y_stop; gridy += y_step ) {
            if( gridx + sp.x() != x_stop && gridy + sp.y() != y_stop ) {
                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    const tripoint_rel_sm grid( gridx, gridy, gridz );

                    copy_grid( grid, grid + sp );
                    submap *const cur_submap = get_submap_at_grid( grid );
                    if( cur_submap == nullptr ) {
                        debugmsg( "Tried to update vehicle list at %s but the submap is not loaded", grid.to_string() );
                        continue;
                    }
                    update_vehicle_list( cur_submap, gridz );
                }
            } else {
                loadn( { gridx, gridy }, true );

                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    loaded_grids.emplace_back( gridx, gridy, gridz );
                }
            }
        }
    }

    rebuild_vehicle_level_caches();

    g->setremoteveh( remoteveh );

    if( !support_cache_dirty.empty() ) {
        std::set<tripoint_bub_ms> old_cache = std::move( support_cache_dirty );
        support_cache_dirty.clear();
        for( const tripoint_bub_ms &pt : old_cache ) {
            support_cache_dirty.insert( pt + point( -sp.x() * SEEX, -sp.y() * SEEY ) );
        }
    }
    // actualize after loading all submaps to prevent errors
    // with entities at the edges
    for( tripoint_rel_sm loaded_grid : loaded_grids ) {
        actualize( loaded_grid );
    }
    if( !loaded_grids.empty() ) {
        // A shift walks the bubble it now has, so the crafts it scrolled away from are
        // not seen and must keep the claims their leases still cover.
        reconcile_loaded_items( reconcile_scope::additive );
    }
}
// END FROZEN NATIVE MAP SHIFT 1040
}

#define NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(c)
namespace actual1040 {
namespace std = observed_std;
struct map : map_state { using map_state::map_state; void shift( const point_rel_sm &sp ); };
#include "ncmm_graphics_map_shift.inc"
}
#undef NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP

namespace frozen1807 {
namespace std = observed_std;
struct map : map_state { using map_state::map_state; void shift( const point_rel_sm &sp ); };
// BEGIN FROZEN NATIVE MAP SHIFT 1807
void map::shift( const point_rel_sm &sp )
{
    if( !zlevels ) {
        debugmsg( "map::shift called from map that doesn't support Z levels" );
        return;
    }

    // Special case of 0-shift; refresh the map
    if( sp == point_rel_sm::zero ) {
        return; // Skip this?
    }

    if( std::abs( sp.x() ) > 1 || std::abs( sp.y() ) > 1 ) {
        debugmsg( "map::shift called with a shift of more than one submap" );
        return;
    }

    const tripoint_abs_sm abs = get_abs_sub();
    std::vector<tripoint_rel_sm> loaded_grids;

    const int zmin = -OVERMAP_DEPTH;
    const int zmax = OVERMAP_HEIGHT;

    const int x_start = sp.x() >= 0 ? 0 : my_MAPSIZE - 1;
    const int x_stop = sp.x() >= 0 ? my_MAPSIZE : -1;
    const int x_step = sp.x() >= 0 ? 1 : -1;
    const int y_start = sp.y() >= 0 ? 0 : my_MAPSIZE - 1;
    const int y_stop = sp.y() >= 0 ? my_MAPSIZE : -1;
    const int y_step = sp.y() >= 0 ? 1 : -1;

    // Have to run on_unload before changing the abs_sub for the map.
    // TODO: can probably skip a bunch of iteration here.
    for( int gridx = x_start; gridx != x_stop; gridx += x_step ) {
        for( int gridy = y_start; gridy != y_stop; gridy += y_step ) {
            // The sp.x() and sp.y() check here establishes that we are moving
            // in the x or y direction respectively. If we are not moving in the x direction,
            // we don't want to call on_unload on grid points where gridx == x_start.
            if( ( sp.x() && gridx == x_start ) || ( sp.y() && gridy == y_start ) ) {
                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    const tripoint_rel_sm grid( gridx, gridy, gridz );
                    on_unload( grid );
                }
            }
        }
    }
    set_abs_sub( abs + sp );

    g->shift_destination_preview( { -sp.x() * SEEX, -sp.y() * SEEY } );

    shift_traps( sp );

    vehicle *remoteveh = g->remoteveh();

    for( int gridz = zmin; gridz <= zmax; gridz++ ) {
        level_cache *cache = get_cache_lazy( gridz );
        if( !cache ) {
            continue;
        }
        for( vehicle *veh : cache->vehicle_list ) {
            veh->zones_dirty = true;
        }
    }

    // Shift the map sx submaps to the right and sy submaps down.
    // sx and sy should never be bigger than +/-1.
    // absx and absy are our position in the world, for saving/loading purposes.
    clear_vehicle_level_caches();

    for( int gridz = zmin; gridz <= zmax; gridz++ ) {
        // Clear vehicle list and rebuild after shift
        // mlangsdorf 2020 - this is kind of insane, building the cache is not free, why are
        // we doing this?
        clear_vehicle_list( gridz );
        level_cache *cache = get_cache_lazy( gridz );
        if( cache ) {
            shift_bitset_cache<MAPSIZE_X, SEEX>( cache->map_memory_cache_dec, sp );
            shift_bitset_cache<MAPSIZE_X, SEEX>( cache->map_memory_cache_ter, sp );
            cache->map_memory_sweep_pending = true;
            shift_bitset_cache<MAPSIZE, 1>( cache->field_cache, sp );
        }
    }

    for( int gridx = x_start; gridx != x_stop; gridx += x_step ) {
        for( int gridy = y_start; gridy != y_stop; gridy += y_step ) {
            if( gridx + sp.x() != x_stop && gridy + sp.y() != y_stop ) {
                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    const tripoint_rel_sm grid( gridx, gridy, gridz );

                    copy_grid( grid, grid + sp );
                    submap *const cur_submap = get_submap_at_grid( grid );
                    if( cur_submap == nullptr ) {
                        debugmsg( "Tried to update vehicle list at %s but the submap is not loaded", grid.to_string() );
                        continue;
                    }
                    update_vehicle_list( cur_submap, gridz );
                }
            } else {
                loadn( { gridx, gridy }, true );

                for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                    loaded_grids.emplace_back( gridx, gridy, gridz );
                }
            }
        }
    }

    rebuild_vehicle_level_caches();

    g->setremoteveh( remoteveh );

    if( !support_cache_dirty.empty() ) {
        std::set<tripoint_bub_ms> old_cache = std::move( support_cache_dirty );
        support_cache_dirty.clear();
        for( const tripoint_bub_ms &pt : old_cache ) {
            support_cache_dirty.insert( pt + point( -sp.x() * SEEX, -sp.y() * SEEY ) );
        }
    }
    // actualize after loading all submaps to prevent errors
    // with entities at the edges
    for( tripoint_rel_sm loaded_grid : loaded_grids ) {
        actualize( loaded_grid );
    }
    if( !loaded_grids.empty() ) {
        // A shift walks the bubble it now has, so the crafts it scrolled away from are
        // not seen and must keep the claims their leases still cover.
        reconcile_loaded_items( reconcile_scope::additive );
    }
}
// END FROZEN NATIVE MAP SHIFT 1807
}

#define NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP(c) c->map_memory_sweep_pending = true;
namespace actual1807 {
namespace std = observed_std;
struct map : map_state { using map_state::map_state; void shift( const point_rel_sm &sp ); };
#include "ncmm_graphics_map_shift.inc"
}
#undef NCMM_GRAPHICS_SHIFT_MEMORY_SWEEP

struct outcome {
    std::vector<call> trace;
    std::vector<std::uint64_t> state;
    std::array<std::size_t, call_count> counts{};
    diag::clock::time_point cleanup{};
    bool exceptional = false;
};
template<class Map> outcome run( scenario s, const std::vector<point_rel_sm> &steps, diag::action_record *record = nullptr )
{
    auto m = std::make_unique<Map>( s );
    map_state *previous = current; game *prior_game = g;
    struct restore { map_state *old; game *old_game; ~restore() { current = old; g = old_game; } } guard{previous, prior_game};
    current = m.get(); g = &m->owner_game;
    m->recursive_call = []( map_state *owner ) { static_cast<Map *>( owner )->shift( {0, 1} ); };
    outcome result;
    auto invoke = [&]() {
        int dx = 0, dy = 0;
        for( auto p : steps ) { dx += p.a; dy += p.b; }
        int x = 60 + dx * SEEX, y = 60 + dy * SEEY;
        diag::update_scope parent( x, y, false );
        if( parent ) parent.route( dx, dy );
        for( auto p : steps ) m->shift( p );
        x = y = 60;
        if( parent ) {
            if( dx == 0 && dy == 0 ) {
                parent.mark( diag::boundary::zero_position ); parent.mark( diag::boundary::zero_overmap );
            } else {
                for( auto tag : {diag::boundary::shift, diag::boundary::rebase_load_npcs, diag::boundary::invalidate,
                                 diag::boundary::build, diag::boundary::spawn, diag::boundary::overmap} ) parent.mark( tag );
            }
        }
    };
    auto attempt = [&]() { try { invoke(); } catch( const std::runtime_error & ) { result.exceptional = true; } };
    if( record ) { diag::action_capture capture( *record, test_now ); attempt(); } else attempt();
    result.trace = std::move( m->trace ); result.state = m->snapshot(); result.counts = m->counts; result.cleanup = m->last_cleanup;
    return result;
}
void same_native( const outcome &a, const outcome &b )
{
    require( a.trace == b.trace, "full Native call order, arguments, debug and cleanup remain exact" );
    require( a.state == b.state && a.counts == b.counts && a.exceptional == b.exceptional,
             "all owned grid/cache/vehicle/support/remote/actualization state and exceptions remain exact" );
}
bool qualifies( const diag::action_record &r, const std::vector<point_rel_sm> &steps )
{
    if( !r.claimed || r.reused_storage || r.nested_capture || r.counter_overflow || r.invocations != 1 || r.recorded != 1 ||
        r.dropped || r.max_depth != 1 || r.exceptional_invocations || r.protocol_errors || r.shift_exceptional_invocations ||
        r.shift_invocations != steps.size() || r.shift_recorded != steps.size() || r.shift_dropped ||
        r.shift_max_depth != ( steps.empty() ? 0u : 1u ) ) return false;
    const auto &parent = r.events[0];
    if( !parent.complete || parent.exceptional ) return false;
    for( std::size_t i = 0; i < steps.size(); ++i ) {
        const auto &e = r.shifts[i];
        if( e.ordinal != i || e.depth != 1 || e.parent_ordinal != -1 || e.parent_update_ordinal != 0 ||
            e.shift_xy != std::array<int, 2>{steps[i].a, steps[i].b} || !e.route_accepted || !e.complete || e.exceptional ||
            e.count != diag::shift_boundary_capacity ) return false;
        for( std::size_t n = 0; n < e.count; ++n )
            if( unsigned( e.tags[n] ) != n || ( n && e.points[n] < e.points[n - 1] ) ) return false;
        if( e.points.front() < parent.points[1] || e.points.back() > parent.points[2] ||
            ( i && e.points.front() < r.shifts[i - 1].points.back() ) ) return false;
    }
    return true;
}
template<class Frozen, class Actual> void matrix( bool later_pin )
{
    std::size_t accepted = 0, captured = 0;
    for( int width : {1, 3, 11} ) for( int dx : {-1, 0, 1} ) for( int dy : {-1, 0, 1} ) if( dx || dy )
        for( int pattern = 0; pattern < 4; ++pattern ) {
            scenario s; s.width = width; s.pattern = pattern;
            const std::vector<point_rel_sm> steps{{dx, dy}};
            const auto frozen = run<Frozen>( s, steps );
            same_native( frozen, run<Actual>( s, steps ) );
            diag::action_record r; const auto actual = run<Actual>( s, steps, &r ); same_native( frozen, actual ); ++pairs; ++accepted;
            const int exposed = width * ( std::abs( dx ) + std::abs( dy ) ) - std::abs( dx * dy );
            const int retained = ( width - std::abs( dx ) ) * ( width - std::abs( dy ) );
            require( !frozen.exceptional && frozen.counts[unload] == std::size_t( exposed * levels ) &&
                     frozen.counts[grid_copy] == std::size_t( retained * levels ) && frozen.counts[load] == std::size_t( exposed ) &&
                     frozen.counts[actualize] == std::size_t( exposed * levels ) && frozen.counts[reconcile] == 1,
                     "all-Z unload/copy/load/actualize/additive-reconcile counts match Native's two-dimensional route" );
            if( r.shift_recorded ) {
                require( qualifies( r, steps ), "complete adjacent map::shift scopes have the actual direction and update parent" );
                require( r.shifts[0].points.back() > actual.cleanup, "map::shift exit follows local-vector destruction" );
                captured += r.shift_recorded;
            }
        }
    std::cout << "Native map::shift " << ( later_pin ? "1807" : "1040" ) << " semantic pairs " << accepted << ", captures " << captured << '\n';
    // Every actual-source semantic pair above must pass before this red gate.
    require( captured == accepted, "the actual map::shift body must capture all internal intervals (fail-before control)" );
    for( bool zlevels : {false, true} ) for( point_rel_sm p : {point_rel_sm{0, 0}, {2, 0}, {-2, 1}, {0, 2}} ) {
        scenario s; s.width = 3; s.zlevels = zlevels;
        diag::action_record r; const auto actual = run<Actual>( s, {p}, &r );
        same_native( run<Frozen>( s, {p} ), actual ); ++pairs;
        require( r.shift_invocations == 1 && !r.shifts[0].route_accepted && !r.shifts[0].complete &&
                 !r.shifts[0].exceptional && r.shifts[0].count == 2 && r.protocol_errors == 0,
                 "guarded Native returns record entry/exit honestly without invented inner boundaries" );
    }
    for( auto kind : {unload, load, actualize, reconcile} ) for( int at : {1, 2} ) {
        if( kind == reconcile && at == 2 ) continue;
        scenario s; s.fail = kind; s.fail_on = at;
        diag::action_record r; const auto actual = run<Actual>( s, {{1, -1}}, &r );
        same_native( run<Frozen>( s, {{1, -1}} ), actual ); ++pairs;
        require( actual.exceptional && r.shift_exceptional_invocations == 1 && r.exceptional_invocations == 1 &&
                 r.shifts[0].exceptional && !r.shifts[0].complete && !qualifies( r, {{1, -1}} ) && diag::active_capture == nullptr,
                 "callback exceptions preserve exact partial state and bounded incomplete evidence" );
    }
    const std::vector<point_rel_sm> multiple{{1, -1}, {1, -1}, {1, 0}};
    scenario s; diag::action_record r;
    same_native( run<Frozen>( s, multiple ), run<Actual>( s, multiple, &r ) ); ++pairs;
    require( qualifies( r, multiple ), "one update_map can contain several ordered single-submap shifts" );
    s.recursive = true; diag::action_record nested;
    same_native( run<Frozen>( s, {{1, 0}} ), run<Actual>( s, {{1, 0}}, &nested ) ); ++pairs;
    require( nested.shift_invocations == 2 && nested.shift_max_depth == 2 && nested.shifts[1].parent_ordinal == 0 &&
             !qualifies( nested, {{1, 0}} ), "nested map::shift retains ancestry and cannot qualify as independent timing" );
    s = {}; s.width = 1; diag::action_record overflow;
    const std::vector<point_rel_sm> long_route( diag::shift_event_capacity + 3, point_rel_sm{1, 0} );
    same_native( run<Frozen>( s, long_route ), run<Actual>( s, long_route, &overflow ) ); ++pairs;
    require( overflow.shift_invocations == long_route.size() && overflow.shift_recorded == diag::shift_event_capacity &&
             overflow.shift_dropped == 3 && !qualifies( overflow, long_route ), "bounded child overflow preserves Native execution without overwriting evidence" );
}
void run_cases()
{
    const auto before = clock_reads;
    for( int i = 0; i < 64; ++i ) { diag::shift_scope disabled( 1, 0 ); if( disabled ) disabled.accept(); }
    require( clock_reads == before && !diag::active_capture, "disabled shift probes never call a clock" );
    matrix<frozen1040::map, actual1040::map>( false );
    matrix<frozen1807::map, actual1807::map>( true );
    diag::action_record cross;
    { diag::action_capture capture( cross, test_now ); ncmm_shift_diagnostics_cross_tu(); }
    require( qualifies( cross, {{1, 0}} ), "map and action scopes share external inline TLS across actual translation units" );
    diag::action_record saturated; saturated.shift_invocations = std::numeric_limits<std::uint32_t>::max();
    scenario s; s.width = 1;
    same_native( run<frozen1040::map>( s, {{1, 0}} ), run<actual1040::map>( s, {{1, 0}}, &saturated ) );
    require( saturated.counter_overflow && saturated.shift_invocations == std::numeric_limits<std::uint32_t>::max() &&
             saturated.shift_recorded == 1 && !qualifies( saturated, {{1, 0}} ), "child counters saturate honestly without wrapping" );
    std::cout << "Native map::shift diagnostics: " << pairs << " pairs / " << checks << " checks; injected clocks are not timings\n";
}
} // namespace native_map_shift_fixture

void test_native_map_shift() { native_map_shift_fixture::run_cases(); }
