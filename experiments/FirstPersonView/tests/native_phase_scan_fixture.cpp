#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Actual Native loop, metadata query and early-return prefix, compiled against
// owned data. Active phase behavior is an observable model; this is neither a
// complete weather/freezing simulation nor a Native frame-time benchmark.
namespace native_phase_scan_fixture {
constexpr int SEEX = 12, OVERMAP_DEPTH = 10, OVERMAP_HEIGHT = 10;
struct point { int a, b; point( int x, int y ) : a( x ), b( y ) {} };
struct point_sm_ms {
    int a = 0, b = 0;
    point_sm_ms() = default;
    point_sm_ms( int x, int y ) : a( x ), b( y ) {}
    bool operator<( const point_sm_ms &p ) const { return std::tie( a, b ) < std::tie( p.a, p.b ); }
    static const point_sm_ms zero;
};
const point_sm_ms point_sm_ms::zero{};
struct tripoint_rel_sm { int a, b, c; };
struct point_bub_sm {
    int a = 0, b = 0;
    int x() const { return a; } int y() const { return b; }
};
struct tripoint_bub_ms {
    int a = 0, b = 0, c = 0;
    int x() const { return a; } int y() const { return b; } int z() const { return c; }
    tripoint_bub_ms operator+( point p ) const { return { a + p.a, b + p.b, c }; }
    bool operator==( const tripoint_bub_ms &p ) const { return a == p.a && b == p.b && c == p.c; }
};
using tripoint_abs_ms = tripoint_bub_ms;
class map;
map *active = nullptr;
struct ter_t { std::string phase_method; static std::size_t count() { return 3; } };
struct ter_str_id {
    int value = 0;
    bool is_valid() const;
    const ter_t &obj() const;
};
struct ter_id {
    int value = 0;
    int to_i() const { return value; }
    bool is_valid() const; // Unavailable external Native specialization.
    const ter_str_id &id() const;
};
class submap {
    std::map<point_sm_ms, ter_id> original_terrain;
public:
    bool uniform = true;
    int terrain = 0;
    std::array<int, 144> mixed{};
    bool is_uniform() const { return uniform; }
    ter_id get_ter( const point_sm_ms &p ) const { return { uniform ? terrain : mixed[std::size_t( p.a * SEEX + p.b )] }; }
    bool has_original_ter( const point_sm_ms & ) const;
    ter_id get_original_ter( const point_sm_ms &p ) const { return original_terrain.at( p ); }
    void set_original_ter( const point_sm_ms &p, ter_id id ) { original_terrain[p] = id; }
    void clear() { original_terrain.clear(); }
    const auto &history() const { return original_terrain; }
#include "../../graphics_bridge/ncmm_graphics_submap_phase.inc"
};
// BEGIN FROZEN NATIVE ORIGINAL TERRAIN QUERY
bool submap::has_original_ter( const point_sm_ms &p ) const
{
    return original_terrain.find( p ) != original_terrain.end();
}
// END FROZEN NATIVE ORIGINAL TERRAIN QUERY
struct weather_generator { int temperature( const tripoint_abs_ms &p ) const { return ( p.a * 7 + p.b * 3 + p.c ) % 19; } };
struct config {
    bool uniform = true, zlevels = true, registered = true, replace = false, unload = false;
    int terrain = 0, original_index = -1, original_id = 0, mutation = 0, width = 11;
    int first_z = -1, last_z = -1;
    point_bub_sm grid;
};
struct observations {
    std::vector<tripoint_bub_ms> active_calls;
    std::vector<int> prefix;
    std::vector<int> phases;
    std::vector<std::array<int, 4>> history;
    int warnings = 0, weather_reads = 0, transforms = 0, setups = 0;
    bool operator==( const observations &o ) const {
        return active_calls == o.active_calls && prefix == o.prefix && phases == o.phases && history == o.history &&
               warnings == o.warnings && weather_reads == o.weather_reads && transforms == o.transforms && setups == o.setups;
    }
};
class map {
public:
    config cfg;
    bool zlevels;
    int my_MAPSIZE;
    submap primary, alternate;
    submap *mapped = &primary;
    std::array<ter_t, 3> definitions{{ {""}, {"water_freeze"}, {"unknown_phase"} }};
    std::array<ter_str_id, 3> ids{{ {0}, {1}, {2} }};
    ter_t fallback;
    std::vector<tripoint_bub_ms> active_calls;
    std::vector<int> prefix;
    std::size_t calls = 0;
    int warnings = 0, weather_reads = 0, transforms = 0, setups = 0;
    explicit map( const config &c ) : cfg( c ), zlevels( c.zlevels ), my_MAPSIZE( c.width ) {
        primary.uniform = alternate.uniform = c.uniform;
        primary.terrain = alternate.terrain = c.terrain;
        primary.mixed.fill( c.terrain ); alternate.mixed.fill( c.terrain );
        if( !c.uniform ) primary.mixed.back() = alternate.mixed.back() = 1;
        if( c.original_index >= 0 ) {
            point_sm_ms p( c.original_index / SEEX, c.original_index % SEEX );
            primary.set_original_ter( p, { c.original_id } ); alternate.set_original_ter( p, { c.original_id } );
        }
    }
    bool inbounds( const tripoint_bub_ms &p ) const {
        return p.a >= 0 && p.b >= 0 && p.a < my_MAPSIZE * SEEX && p.b < my_MAPSIZE * SEEX &&
               p.c >= -OVERMAP_DEPTH && p.c <= OVERMAP_HEIGHT;
    }
    static point_sm_ms local( const tripoint_bub_ms &p ) { return { ( p.a % 12 + 12 ) % 12, ( p.b % 12 + 12 ) % 12 }; }
    submap *get_submap_at_grid( tripoint_rel_sm ) { return mapped; }
    bool has_original_terrain_at( const tripoint_bub_ms &p ) {
        if( !inbounds( p ) ) return false;
        if( !mapped ) { ++warnings; return false; }
        return mapped->has_original_ter( local( p ) );
    }
    ter_id get_original_terrain_at( const tripoint_bub_ms &p ) { return mapped->get_original_ter( local( p ) ); }
    ter_id ter( const tripoint_bub_ms &p ) {
        if( !inbounds( p ) ) return {0};
        if( !mapped ) { ++warnings; return {0}; }
        return mapped->get_ter( local( p ) );
    }
    tripoint_abs_ms get_abs( const tripoint_bub_ms &p ) const { return { p.a + 1200, p.b - 2400, p.c }; }
    void ter_set( const tripoint_bub_ms &p, ter_id id ) {
        ++transforms;
        if( mapped && inbounds( p ) ) {
            if( mapped->uniform ) mapped->mixed.fill( mapped->terrain );
            mapped->uniform = false;
            const auto q = local( p ); mapped->mixed[std::size_t( q.a * 12 + q.b )] = id.value;
        }
    }
    void temp_based_phase_change_at( const tripoint_bub_ms &, const weather_generator & );
    void phase_inner( const tripoint_bub_ms &, const weather_generator & );
    void run_original();
    void run();
    void before_phase( int z ) {
        ++setups; prefix.push_back( z );
        if( cfg.replace ) { mapped = &alternate; alternate.uniform = false; alternate.mixed.back() = 1; }
        if( cfg.unload ) mapped = nullptr;
        if( !mapped ) return;
        if( cfg.mutation == 1 ) mapped->set_original_ter( {11,11}, {1} );
        if( cfg.mutation == 2 ) { mapped->uniform = false; mapped->mixed.back() = 1; }
        if( cfg.mutation == 3 ) mapped->clear();
        if( cfg.mutation == 4 ) mapped->terrain = 1;
    }
    observations snapshot() const {
        observations o; o.active_calls = active_calls; o.prefix = prefix;
        o.warnings = warnings; o.weather_reads = weather_reads; o.transforms = transforms; o.setups = setups;
        int index = 0;
        for( const submap *sm : { &primary, &alternate } ) {
            o.phases.push_back( int( sm->uniform ) ); o.phases.push_back( sm->terrain );
            o.phases.insert( o.phases.end(), sm->mixed.begin(), sm->mixed.end() );
            for( const auto &entry : sm->history() ) o.history.push_back( { index, entry.first.a, entry.first.b, entry.second.value } );
            ++index;
        }
        return o;
    }
};
bool ter_str_id::is_valid() const { return active->cfg.registered && value >= 0 && value < 3; }
const ter_t &ter_str_id::obj() const {
    if( !is_valid() ) { ++active->warnings; return active->fallback; }
    return active->definitions[std::size_t( value )];
}
const ter_str_id &ter_id::id() const {
    if( value < 0 || value >= 3 ) { ++active->warnings; return active->ids[0]; }
    return active->ids[std::size_t( value )];
}
void map::temp_based_phase_change_at( const tripoint_bub_ms &p, const weather_generator &wgen ) {
    ++calls;
    phase_inner( p, wgen );
}
// BEGIN COMPILED NATIVE PHASE PREFIX
void map::phase_inner( const tripoint_bub_ms &p, const weather_generator &wgen )
{
    // If we have a recorded original terrain, use its rules to revert or
    if( has_original_terrain_at( p ) ) {
        const ter_id orig_id = get_original_terrain_at( p );
        ter_set( p, orig_id );
    }
    const tripoint_abs_ms abs_p = get_abs( p );
    const ter_str_id cur_id = ter( p ).id();
    const ter_t &cur_ter = cur_id.obj();
    // Bail out early if neither this terrain nor a recorded original
    // terrain declare a phase_method.
    if( cur_ter.phase_method.empty() && !has_original_terrain_at( p ) ) {
        return;
    }
// END COMPILED NATIVE PHASE PREFIX
    if( cur_ter.phase_method == "water_freeze" ) {
        active_calls.push_back( p );
        weather_reads += 10;
        const int result = wgen.temperature( abs_p );
        ter_set( p, { result > 0 ? 0 : 1 } );
        if( mapped ) mapped->set_original_ter( local( p ), {cur_id.value} );
    }
}
void map::run_original() {
    const point_bub_sm grid = cfg.grid;
    const weather_generator wgen;
    for( int z = cfg.first_z; z <= cfg.last_z; ++z ) {
        submap *tmpsub = &primary;
        (void)tmpsub;
        before_phase( z );
        const tripoint_bub_ms sm_origin{ grid.x() * SEEX, grid.y() * SEEX, z };
// BEGIN FROZEN NATIVE PHASE LOOP
        for( int sx = 0; sx < SEEX; ++sx ) {
            for( int sy = 0; sy < SEEX; ++sy ) {
                temp_based_phase_change_at( sm_origin + point( sx, sy ), wgen );
            }
        }
// END FROZEN NATIVE PHASE LOOP
    }
}
void map::run() {
    const point_bub_sm grid = cfg.grid;
    const weather_generator wgen;
    for( int z = cfg.first_z; z <= cfg.last_z; ++z ) {
        submap *tmpsub = &primary;
        before_phase( z );
        const tripoint_bub_ms sm_origin{ grid.x() * SEEX, grid.y() * SEEX, z };
#include "../../graphics_bridge/ncmm_graphics_phase_loop.inc"
    }
}
std::size_t checks = 0, cases = 0;
void require( bool ok, const char *message ) {
    ++checks;
    if( !ok ) throw std::runtime_error( std::string( "Native phase scan: " ) + message );
}
void paired( const config &c, bool full = true ) {
    map before( c ); active = &before; before.run_original();
    map after( c ); active = &after; after.run();
    require( before.snapshot() == after.snapshot(), "phase/restoration state, active call order, weather reads and diagnostics remain exact" );
    if( full ) require( before.calls == after.calls, "ineligible contexts retain every original cell call" );
    ++cases;
}
} // namespace native_phase_scan_fixture

int native_phase_scan_fixture_tests() {
    using namespace native_phase_scan_fixture;
    for( int kind : {0,1,2} ) for( bool uniform : { false, true } ) {
        for( int index : { -1, 0, 1, 12, 71, 143, 144, 255 } ) for( int original : { -1, 0, 1, 2 } ) {
            config c; c.terrain = kind; c.uniform = uniform; c.original_index = index; c.original_id = original;
            paired( c, !( kind == 0 && uniform && index == -1 ) );
        }
    }
    for( int id : { std::numeric_limits<int>::min(), -1, 3, std::numeric_limits<int>::max() } ) {
        config c; c.terrain = id; paired( c );
    }
    for( int width : { 1, 3, 11 } ) for( int z : { -11, -10, -1, 0, 1, 10, 11 } ) {
        config c; c.width = width; c.grid = {width-1,width-1}; c.first_z = c.last_z = z;
        paired( c, z < -10 || z > 10 );
    }
    for( point_bub_sm grid : { point_bub_sm{-1,0}, {0,-1}, {11,0}, {0,11} } ) {
        config c; c.grid = grid; paired( c );
    }
    for( int mutation : { 1, 2, 4 } ) { config c; c.mutation = mutation; paired( c ); }
    { config c; c.original_index = 143; c.mutation = 3; paired( c, false ); }
    { config c; c.replace = true; paired( c ); }
    { config c; c.unload = true; paired( c ); }
    { config c; c.zlevels = false; paired( c ); }
    { config c; c.registered = false; paired( c ); }
    { config c; c.first_z = -10; c.last_z = 10; paired( c, false ); }
    config c;
    map before( c ); active = &before; before.run_original();
    map after( c ); active = &after; after.run();
    require( before.calls == 144, "frozen original executes all144 Native cell calls" );
    std::cout << "Native phase scan: " << cases << " paired cases; cell calls " << before.calls << " -> " << after.calls << '\n';
    require( after.calls == 0, "eligible uniform phase-free history-free submap needs no cell phase calls" );
    config stack; stack.first_z = -10; stack.last_z = 10;
    map all_before( stack ); active = &all_before; all_before.run_original();
    map all_after( stack ); active = &all_after; all_after.run();
    require( all_before.calls == 3024, "the full21-level original stack executes3024 cell calls" );
    require( all_after.calls == 0, "eligible no-op proof applies independently on all21 loaded levels" );
    std::cout << "Native phase scan: " << checks << " checks passed (owned source model)\n";
    active = nullptr;
    return 0;
}


// Adapted from Cataclysm: Dark Days Ahead; CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// Exact supported pins: 3f7fb352bf492ba521bd9408a0c9f6ce239e8d83 and
// 074aa98bd5be3de4c35f154082db32a0e63bb0f1; src/map.cpp and src/mapgen.cpp.
#if 0
// BEGIN FROZEN NATIVE PHASE SOURCE loadn
void map::loadn( const point_bub_sm &grid, bool update_vehicles )
{
    dbg( D_INFO ) << "map::loadn(game[" << g.get() << "], worldx[" << abs_sub.x()
                  << "], worldy[" << abs_sub.y() << "], grid " << grid << ")";

    const tripoint_abs_sm grid_abs_sub = abs_sub + rebase_rel( grid );
    const tripoint_abs_omt grid_abs_omt = project_to<coords::omt>( grid_abs_sub );
    // Get the base submap "grid" is an offset from.
    const tripoint_abs_sm grid_sm_base = project_to<coords::sm>( grid_abs_omt );
    bool map_incomplete = false;

    map &bubble_map = reality_bubble();

    bool const main_inbounds =
        this != &bubble_map && bubble_map.inbounds( project_to<coords::ms>( grid_abs_sub ) );

    // It might be possible to just check the (0, 0) submap as we should never have
    // a case where only one submap is missing from an OMT level.
    for( int gridx = 0; !map_incomplete && gridx <= 1; gridx++ ) {
        for( int gridy = 0; !map_incomplete && gridy <= 1; gridy++ ) {
            for( int gridz = -OVERMAP_DEPTH; gridz <= OVERMAP_HEIGHT; gridz++ ) {
                const tripoint grid_pos( gridx, gridy, gridz );
                if( !MAPBUFFER.submap_exists( grid_sm_base.xy() + grid_pos ) ) {
                    map_incomplete = true;
                    break;
                }
            }
        }
    }

    if( map_incomplete ) {
        smallmap tmp_map;
        swap_map swap( *tmp_map.cast_to_map() );
        tmp_map.main_cleanup_override( false );
        tmp_map.generate( grid_abs_omt, calendar::turn, true );
        _main_requires_cleanup |= main_inbounds && tmp_map.is_main_cleanup_queued();

        for( int gridz = -OVERMAP_DEPTH; gridz <= OVERMAP_HEIGHT; gridz++ ) {
            const tripoint_abs_sm pos = {grid_sm_base.xy(), gridz };
            if( !MAPBUFFER.submap_exists( pos ) ) {
                dbg( D_ERROR ) << "failed to generate a submap at " << pos;
                debugmsg( "failed to generate a submap at %s", pos.to_string() );
                return;
            }
        }
    }

    const int start_z = zlevels ? -OVERMAP_DEPTH : abs_sub.z();
    const int stop_z = zlevels ? OVERMAP_HEIGHT : abs_sub.z();

    submap *tmpsub;

    for( int z = start_z; z <= stop_z; z++ ) {
        const tripoint_abs_sm pos = { grid_abs_sub.xy(), z };
        // New submap changes the content of the map and all caches must be recalculated
        set_transparency_cache_dirty( z );
        set_seen_cache_dirty( z );
        set_outside_cache_dirty( z );
        set_floor_cache_dirty( z );
        set_pathfinding_cache_dirty( z );
        tmpsub = MAPBUFFER.lookup_submap( pos );
        setsubmap( get_nonant( tripoint_rel_sm{ grid.x(), grid.y(), z} ), tmpsub );
        if( !tmpsub->active_items.empty() ) {
            submaps_with_active_items_dirty.emplace( pos );
        }
        if( tmpsub->field_count > 0 ) {
            get_cache( z ).field_cache.set( grid.x() + grid.y() * MAPSIZE );
        }

        // Destroy bugged no-part vehicles
        auto &veh_vec = tmpsub->vehicles;
        for( auto iter = veh_vec.begin(); iter != veh_vec.end(); ) {
            vehicle *veh = iter->get();
            if( veh->part_count() > 0 ) {
                // Always fix submap coordinates for easier Z-level-related operations
                veh->sm_pos = abs_sub.xy() + tripoint_rel_sm{ rebase_rel( grid ), z};
                iter++;
                if( main_inbounds ) {
                    _main_requires_cleanup = true;
                }
            } else {
                if( veh->tracking_on ) {
                    overmap_buffer.remove_vehicle( veh );
                }
                dirty_vehicle_list.erase( veh );
                iter = veh_vec.erase( iter );
            }
        }

        // Update vehicle data
        if( update_vehicles ) {
            level_cache &map_cache = get_cache( z );
            for( const auto &veh : tmpsub->vehicles ) {
                // Only add if not tracking already.
                if( map_cache.vehicle_list.find( veh.get() ) == map_cache.vehicle_list.end() ) {
                    map_cache.vehicle_list.insert( veh.get() );
                    if( !veh->loot_zones.empty() ) {
                        map_cache.zone_vehicles.insert( veh.get() );
                    }
                }
            }
        }

        if( zlevels ) {
            add_tree_tops( { grid.x(), grid.y(), z } );
        }
        // When a submap is loaded into the reality bubble, apply
        // temperature-based phase changes
        const weather_generator &wgen = get_weather().get_cur_weather_gen();
        const tripoint_abs_sm pos_sm = { grid_abs_sub.xy(), z };
        const tripoint_abs_ms sm_abs_ms = project_to<coords::ms>( pos_sm );
        const tripoint_bub_ms sm_origin = get_bub( sm_abs_ms );
        for( int sx = 0; sx < SEEX; ++sx ) {
            for( int sy = 0; sy < SEEX; ++sy ) {
                temp_based_phase_change_at( sm_origin + point( sx, sy ), wgen );
            }
        }
    }
}
// END FROZEN NATIVE PHASE SOURCE loadn
// BEGIN FROZEN NATIVE PHASE SOURCE submap_anchor
        // Tracks original terrain for tiles transformed by phase logic
        std::map<point_sm_ms, ter_id> original_terrain;

    public:
        bool has_original_ter( const point_sm_ms &p ) const;
        ter_id get_original_ter( const point_sm_ms &p ) const;
        void set_original_ter( const point_sm_ms &p, const ter_id &t );
        void clear_original_ter( const point_sm_ms &p );
// END FROZEN NATIVE PHASE SOURCE submap_anchor
// BEGIN FROZEN NATIVE PHASE SOURCE phase_function
void map::temp_based_phase_change_at( const tripoint_bub_ms &p, const weather_generator &wgen )
{
    // If we have a recorded original terrain, use its rules to revert or
    if( has_original_terrain_at( p ) ) {
        const ter_id orig_id = get_original_terrain_at( p );
        ter_set( p, orig_id );
    }
    const tripoint_abs_ms abs_p = get_abs( p );
    const ter_str_id cur_id = ter( p ).id();
    const ter_t &cur_ter = cur_id.obj();
    // Bail out early if neither this terrain nor a recorded original
    // terrain declare a phase_method.
    if( cur_ter.phase_method.empty() && !has_original_terrain_at( p ) ) {
        return;
    }
    units::temperature standard = 0_K;
    // `phase_method` selects which phase behavior to apply.
    if( cur_ter.phase_method == "water_freeze" ) {
        // 10-day average at 08:00.
        //1103515245u = random
        unsigned seed = static_cast<unsigned>( g->get_seed() ) ^
                        static_cast<unsigned>( abs_p.x() * 1103515245u + abs_p.y() );
        units::temperature sum = 0_K;
        for( int d = 0; d < 10; d++ ) {
            time_point base_date = calendar::turn - ( d + 1 ) * 1_days;
            const time_duration time_of_day = ( base_date - calendar::turn_zero ) % 1_days;
            const time_point t = base_date - time_of_day + 8_hours;
            sum += wgen.get_weather_temperature( abs_p, t, seed );
        }
        standard = sum / 10.0;
    } else {
        return;
    }

    auto apply_phase_thresholds = [&]( const ter_t & tt, const ter_str_id & base_id ) -> ter_str_id {
        if( tt.phase_targets.empty() )
        {
            return ter_t_pseudo_phase;
        }
        const size_t targ_n = tt.phase_targets.size();
        const size_t temp_n = tt.phase_temps.size();
        if( temp_n + 1 != targ_n )
        {
            debugmsg( "ter %s: phase_targets and phase_temps length mismatch (expected temps == targets-1)",
                      base_id.str() );
            return ter_t_pseudo_phase;
        }
        std::vector<ter_str_id> targets;
        targets.reserve( targ_n );
        for( size_t i = 0; i < targ_n; ++i )
        {
            ter_str_id target = tt.phase_targets[i];
            if( target == ter_t_pseudo_phase ) {
                target = base_id;
            }
            targets.push_back( target );
        }

        if( standard < tt.phase_temps[0] )
        {
            return targets[0];
        }

        // Intermediate ranges: inclusive upper bound
        for( size_t i = 0; i + 1 < temp_n; ++i )
        {
            if( standard >= tt.phase_temps[i] && standard <= tt.phase_temps[i + 1] ) {
                return targets[i + 1];
            }
        }

        if( standard > tt.phase_temps.back() )
        {
            return targets.back();
        }

        return ter_t_pseudo_phase;
    };

    // No original recorded: evaluate using the current terrain's rules and
    // record original before transforming.

    const ter_str_id chosen = apply_phase_thresholds( cur_ter, cur_id );
    if( chosen != ter_t_pseudo_phase && chosen != cur_id ) {
        // Record original terrain so it can be reverted later.
        ter_set( p, chosen );
        set_original_terrain_at( p, cur_id.id() );
    }
}
// END FROZEN NATIVE PHASE SOURCE phase_function
#endif
