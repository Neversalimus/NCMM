#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Compile the complete pinned Native actualize body and its adaptation against
// owned state. Helper models exercise the proven early-return predicates and
// deliberately observable active paths; they do not model full CDDA gameplay.
// Timing from this fixture is never an ordinary Native performance claim.
namespace native_actualize_fixture {
constexpr int SEEX = 12, SEEY = 12, OVERMAP_DEPTH = 10, OVERMAP_HEIGHT = 10;
using time_duration = int;
namespace calendar { int turn = 100; }
enum class ter_furn_flag { TFLAG_PLANT, TFLAG_HARVESTED };
struct point { int a, b; point( int x, int y ) : a( x ), b( y ) {} };
struct point_sm_ms {
    int a = 0, b = 0;
    point_sm_ms() = default;
    point_sm_ms( int x, int y ) : a( x ), b( y ) {}
    int x() const { return a; } int y() const { return b; }
    static const point_sm_ms zero;
};
const point_sm_ms point_sm_ms::zero{};
struct tripoint_rel_sm {
    int a = 0, b = 0, c = -1;
    int x() const { return a; } int y() const { return b; } int z() const { return c; }
};
struct tripoint_bub_ms {
    int a = 0, b = 0, c = 0;
    int x() const { return a; } int y() const { return b; } int z() const { return c; }
    tripoint_bub_ms operator+( point p ) const { return { a + p.a, b + p.b, c }; }
    bool operator==( const tripoint_bub_ms &p ) const { return a == p.a && b == p.b && c == p.c; }
};
namespace coords {
struct ms {};
template<class T> tripoint_bub_ms project_to( const tripoint_rel_sm &p ) { return { p.x() * SEEX, p.y() * SEEY, p.z() }; }
}
tripoint_bub_ms rebase_bub( tripoint_bub_ms p ) { return p; }
struct trap_id {
    int value = 0;
    int to_i() const { return value; }
    bool operator!=( trap_id b ) const { return value != b.value; }
    bool operator==( trap_id b ) const { return value == b.value; }
};
const trap_id tr_null{};
struct furn_t {
    std::vector<int> emissions;
    bool plant = false;
    bool has_flag( ter_furn_flag f ) const { return f == ter_furn_flag::TFLAG_PLANT && plant; }
};
struct ter_t {
    std::vector<int> emissions;
    trap_id trap;
    bool harvested = false;
    bool has_flag( ter_furn_flag f ) const { return f == ter_furn_flag::TFLAG_HARVESTED && harvested; }
    static std::size_t count() { return 4; }
};
class map;
map *active = nullptr;
struct config {
    bool uniform = true, zlevels = true, null_submap = false, registered_null = true;
    int terrain_id = 0, furniture_id = 0, feature = 0;
    int callback_stage = 0, callback_change = 0;
    int elapsed = 100;
    tripoint_rel_sm grid;
    bool vehicles = false, shared_fields = false;
    int throw_stage = 0;
};
struct ter_id {
    int value = 0;
    int to_i() const { return value; }
    bool is_valid() const; // Deliberately undefined, matching pinned linkage.
    const ter_t &obj() const;
    const ter_t &operator*() const { return obj(); }
    bool operator!=( ter_id b ) const { return value != b.value; }
    bool operator==( ter_id b ) const { return value == b.value; }
};
const ter_id ter_t_tree_maple_tapped{ 2 };
struct furn_id {
    int value = 0;
    bool is_valid() const { return value >= 0 && value < 4; }
    const furn_t &obj() const;
    const furn_t &operator*() const { return obj(); }
};
struct furn_str_id {
    static furn_str_id NULL_ID() { return {}; }
    bool is_valid() const;
};
struct item { void overflow( map &, const tripoint_bub_ms & ); };
struct vpart { item base; const item &get_base() const { return base; } };
struct vpart_reference {
    vpart value;
    const vpart &part() const { return value; }
    tripoint_bub_ms pos_bub( map & ) const { return {}; }
};
struct vehicle {
    std::vector<vpart_reference> parts{ 2 };
    const std::vector<vpart_reference> &get_all_parts() const { return parts; }
    void refresh();
};
struct submap {
    bool uniform = true;
    int last_touched = 0;
    ter_id terrain;
    furn_id furniture;
    std::vector<std::unique_ptr<vehicle>> vehicles;
    std::vector<int> fields;
    bool is_uniform() const { return uniform; }
    ter_id get_ter( const point_sm_ms & ) const { return terrain; }
    furn_id get_furn( const point_sm_ms & ) const;
    trap_id get_trap( const point_sm_ms & ) const;
    std::vector<int> &get_field( const point_sm_ms & ) { return fields; }
};
struct state {
    std::vector<int> callbacks;
    std::array<int, 5> effects{};
    std::vector<int> primary_fields, alternate_fields;
    std::array<std::vector<tripoint_bub_ms>, 4> traps;
    std::vector<tripoint_bub_ms> furn_emissions, ter_emissions;
    int primary_touched = 0, alternate_touched = 0, warnings = 0, rng = 0, turn = 0;
    bool primary_uniform = false, alternate_uniform = false, alternate_mapping = false;
    bool operator==( const state &s ) const {
        return callbacks == s.callbacks && effects == s.effects && primary_fields == s.primary_fields &&
               alternate_fields == s.alternate_fields && traps == s.traps && furn_emissions == s.furn_emissions &&
               ter_emissions == s.ter_emissions && primary_touched == s.primary_touched &&
               alternate_touched == s.alternate_touched && warnings == s.warnings && rng == s.rng &&
               turn == s.turn && primary_uniform == s.primary_uniform && alternate_uniform == s.alternate_uniform &&
               alternate_mapping == s.alternate_mapping;
    }
};
class map {
public:
    config cfg;
    bool zlevels;
    int my_MAPSIZE = 11;
    submap primary, alternate;
    submap *mapped = &primary;
    std::array<furn_t, 4> furniture_defs;
    std::array<ter_t, 4> terrain_defs;
    furn_t fallback_furniture;
    ter_t fallback_terrain;
    std::array<std::vector<tripoint_bub_ms>, 4> traplocs;
    std::vector<tripoint_bub_ms> field_furn_locs, field_ter_locs;
    std::vector<int> callbacks;
    std::array<int, 5> effects{};
    int warnings = 0, rng_calls = 0;
    std::size_t trap_reads = 0;
    explicit map( const config &c ) : cfg( c ), zlevels( c.zlevels ) {
        primary.uniform = alternate.uniform = c.uniform;
        primary.last_touched = alternate.last_touched = calendar::turn - c.elapsed;
        primary.terrain.value = alternate.terrain.value = c.terrain_id;
        primary.furniture.value = alternate.furniture.value = c.furniture_id;
        if( c.vehicles ) primary.vehicles.emplace_back( std::make_unique<vehicle>() );
        if( c.shared_fields ) primary.fields = alternate.fields = { 3, 8 };
        if( c.feature & 1 ) furniture_defs[0].plant = true;
        if( c.feature & 2 ) terrain_defs[0].harvested = true;
        if( c.feature & 4 ) furniture_defs[0].emissions.push_back( 1 );
        if( c.feature & 8 ) terrain_defs[0].emissions.push_back( 1 );
        if( c.feature & 16 ) terrain_defs[0].trap.value = 1;
    }
    void actualize( const tripoint_rel_sm & );
    void actualize_original( const tripoint_rel_sm & );
    submap *get_submap_at_grid( const tripoint_rel_sm &p ) {
        if( p.x() < 0 || p.y() < 0 || p.x() >= my_MAPSIZE || p.y() >= my_MAPSIZE ||
            p.z() < -OVERMAP_DEPTH || p.z() > OVERMAP_HEIGHT ) ++warnings;
        return cfg.null_submap ? nullptr : mapped;
    }
    furn_id furn( const tripoint_bub_ms & ) const { return mapped->get_furn( point_sm_ms::zero ); }
    ter_id ter( const tripoint_bub_ms & ) const { return mapped->terrain; }
    void callback( int stage ) {
        callbacks.push_back( stage );
        if( stage == cfg.throw_stage ) throw std::runtime_error( "owned callback exception" );
        if( stage != cfg.callback_stage ) return;
        switch( cfg.callback_change ) {
            case 1: mapped->uniform = false; mapped->fields.push_back( 7 ); furniture_defs[0].plant = true; break;
            case 2: mapped = &alternate; alternate.uniform = false; alternate.fields.push_back( 9 ); break;
            case 3: terrain_defs[0].emissions.push_back( 1 ); terrain_defs[0].harvested = true; break;
            case 4: mapped->fields.push_back( 5 ); break;
            case 5: calendar::turn += 17; break;
            case 6: mapped->terrain = ter_t_tree_maple_tapped; break;
            default: break;
        }
    }
    void process_items_in_vehicles( submap & ) { callback( 3 ); }
    void process_items_in_submap( submap &, const tripoint_rel_sm & ) { callback( 4 ); }
    void fill_funnels( const tripoint_bub_ms &, int ) { callbacks.push_back( 6 ); }
    void grow_plant( const tripoint_bub_ms &p ) {
        const furn_t &f = furn( p ).obj();
        if( !f.has_flag( ter_furn_flag::TFLAG_PLANT ) ) return;
        ++effects[0]; mapped->uniform = false;
    }
    void restock_fruits( const tripoint_bub_ms &p, time_duration ) {
        const ter_t &t = ter( p ).obj();
        if( !t.has_flag( ter_furn_flag::TFLAG_HARVESTED ) ) return;
        ++effects[1];
    }
    void produce_sap( const tripoint_bub_ms &p, time_duration elapsed ) {
        if( elapsed <= 0 ) return;
        if( !( ter( p ) == ter_t_tree_maple_tapped ) ) return;
        ++effects[2]; ++rng_calls;
    }
    void rad_scorch( const tripoint_bub_ms &, time_duration ) {
        const int rads = mapped->uniform ? 0 : 1;
        if( rads == 0 ) return;
        ++effects[3]; ++rng_calls;
    }
    void decay_cosmetic_fields( const tripoint_bub_ms &, time_duration elapsed ) {
        for( int &age : mapped->fields ) { age += elapsed; ++effects[4]; ++rng_calls; }
    }
    state snapshot() const {
        return { callbacks, effects, primary.fields, alternate.fields, traplocs, field_furn_locs, field_ter_locs,
                 primary.last_touched, alternate.last_touched, warnings, rng_calls, calendar::turn,
                 primary.uniform, alternate.uniform, mapped == &alternate };
    }
};
bool furn_str_id::is_valid() const { return active->cfg.registered_null; }
const ter_t &ter_id::obj() const {
    if( value < 0 || value >= 4 ) { ++active->warnings; return active->fallback_terrain; }
    return active->terrain_defs[std::size_t( value )];
}
const furn_t &furn_id::obj() const {
    if( !is_valid() ) { ++active->warnings; return active->fallback_furniture; }
    return active->furniture_defs[std::size_t( value )];
}
furn_id submap::get_furn( const point_sm_ms & ) const {
    if( uniform && !active->cfg.registered_null ) ++active->warnings;
    return furniture;
}
trap_id submap::get_trap( const point_sm_ms & ) const {
    ++active->trap_reads;
    return uniform ? tr_null : trap_id{ 2 };
}
void item::overflow( map &m, const tripoint_bub_ms & ) { m.callback( 1 ); }
void vehicle::refresh() { active->callback( 2 ); }
void debugmsg( const char *, int, int, int ) { ++active->warnings; }
namespace explosion_handler { void process_explosions() { active->callback( 5 ); } }

#define actualize actualize_original
#include "native_actualize_primary.inc"
#undef actualize
#include "../../graphics_bridge/ncmm_graphics_actualize.inc"

std::size_t checks = 0, cases = 0;
void require( bool ok, const char *message ) {
    ++checks;
    if( !ok ) throw std::runtime_error( std::string( "Native actualize model: " ) + message );
}
void paired( const config &c ) {
    calendar::turn = 100;
    map before( c ); active = &before;
    std::string before_exception, after_exception;
    try { before.actualize_original( c.grid ); }
    catch( const std::runtime_error &e ) { before_exception = e.what(); }
    const state expected = before.snapshot();
    calendar::turn = 100;
    map after( c ); active = &after;
    try { after.actualize( c.grid ); }
    catch( const std::runtime_error &e ) { after_exception = e.what(); }
    require( before_exception == after_exception, "callback exceptions retain their propagation boundary" );
    require( after.snapshot() == expected, "protected state, active effects, callback order, RNG and diagnostics match" );
    ++cases;
}
} // namespace native_actualize_fixture

int native_actualize_fixture_tests() {
    using namespace native_actualize_fixture;
    for( int feature = 0; feature < 32; ++feature ) {
        for( bool uniform : { false, true } ) {
            for( bool fields : { false, true } ) {
                for( int z : { -10, -1, 0, 10 } ) {
                    config c; c.feature = feature; c.uniform = uniform; c.shared_fields = fields; c.grid.c = z;
                    paired( c );
                }
            }
        }
    }
    for( int stage = 1; stage <= 5; ++stage ) {
        for( int mutation = 0; mutation <= 6; ++mutation ) {
            for( int elapsed : { -100, 0, 1, 100 } ) {
                config c; c.vehicles = true; c.callback_stage = stage; c.callback_change = mutation; c.elapsed = elapsed;
                paired( c );
            }
        }
    }
    for( int terrain : { std::numeric_limits<int>::min(), -1, 0, 1, 2, 3, 4, std::numeric_limits<int>::max() } ) {
        for( int furniture : { -1, 0, 4 } ) {
            config c; c.terrain_id = terrain; c.furniture_id = furniture; paired( c );
        }
    }
    for( tripoint_rel_sm grid : { tripoint_rel_sm{ -1, 0, -1 }, { 0, -1, -1 }, { 11, 0, -1 },
                                  { 0, 11, -1 }, { 0, 0, -11 }, { 0, 0, 11 }, { 10, 10, -10 } } ) {
        config c; c.grid = grid; paired( c );
    }
    { config c; c.zlevels = false; paired( c ); }
    { config c; c.null_submap = true; paired( c ); }
    { config c; c.registered_null = false; paired( c ); }
    for( int stage = 1; stage <= 5; ++stage ) {
        config c; c.vehicles = true; c.throw_stage = stage; paired( c );
    }
    // Causal read-work gate follows all semantic cases: the original
    // body must pass those first, then fail this gate before the adaptation.
    config c; calendar::turn = 100; map before( c ); active = &before; before.actualize_original( c.grid );
    calendar::turn = 100; map after( c ); active = &after; after.actualize( c.grid );
    require( before.trap_reads == 144, "the frozen original executes all 144 cells" );
    std::cout << "Native actualize model: " << cases << " paired cases, " << checks << " checks; tile trap reads "
              << before.trap_reads << " -> " << after.trap_reads << '\n';
    require( after.trap_reads == 0, "proven underground uniform no-op skips only the tile loop" );
    std::cout << "Native actualize model acceptance: " << cases << " cases / " << checks << " checks passed\n";
    active = nullptr;
    return 0;
}
