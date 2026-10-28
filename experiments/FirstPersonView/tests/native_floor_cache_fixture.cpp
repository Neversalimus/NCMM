#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Actual Native control flow is compiled below against owned, deterministic
// terrain/furniture definitions. Callback counts are component evidence, never
// Native timings or FPS. The independent primary oracle must remain frozen.
// Frozen code: Cataclysm: DDA contributors, CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L10672-L10727
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L10684-L10739
// Identical primary body: 2349 bytes including final LF, SHA256
// 80c7cfd987d5550275fd2eabf182d2027591c99d22fa8ca58024da46c2495eb6.
namespace native_floor_cache_fixture {
constexpr int SEEX = 12, SEEY = 12, MAPSIZE_X = 132, MAPSIZE_Y = 132, OVERMAP_DEPTH = 10;
constexpr std::size_t tile_count = std::size_t( MAPSIZE_X ) * MAPSIZE_Y;
std::size_t checks = 0, cases = 0;
void require( bool ok, const char *message )
{
    ++checks;
    if( !ok ) throw std::runtime_error( std::string( "Native floor cache: " ) + message );
}
enum class ter_furn_flag : unsigned {
    TFLAG_NO_FLOOR, TFLAG_NO_FLOOR_WATER, TFLAG_GOES_DOWN,
    TFLAG_TRANSPARENT_FLOOR, TFLAG_SUN_ROOF_ABOVE
};
constexpr unsigned sun = 1u << unsigned( ter_furn_flag::TFLAG_SUN_ROOF_ABOVE );
struct point_sm_ms {
    int a = 0, b = 0;
    point_sm_ms() = default;
    point_sm_ms( int x, int y ) : a( x ), b( y ) {}
    int x() const { return a; } int y() const { return b; }
    static const point_sm_ms zero;
};
const point_sm_ms point_sm_ms::zero{};
struct point { int x, y; point( int a, int b ) : x( a ), y( b ) {} };
struct tripoint_rel_sm { int x, y, z; };
enum read_kind { cache_lookup, submap_lookup, unloaded_debug, terrain_read, furniture_read,
                 terrain_flag, furniture_flag, invalid_terrain_debug, invalid_furniture_debug, null_conversion_debug };
using read_event = std::array<int, 8>;
struct observations {
    std::vector<read_event> required;
    std::uint64_t terrain_reads = 0, furniture_reads = 0, invalid_terrain = 0, invalid_furniture = 0;
    std::uint64_t invalid_null_conversions = 0;
    bool f_null_registered = true;
    int invalid_terrain_id = 1;
    observations() { required.reserve( 4096 ); }
    void note( read_kind kind, int x, int y, int z, int sx = 0, int sy = 0, int flag = 0 )
    { required.push_back( {int( kind ), x, y, z, sx, sy, flag, 0} ); }
};
observations *active_observations = nullptr;
struct furn_str_id {
    static furn_str_id NULL_ID() noexcept { return {}; }
    bool is_valid() const noexcept { return active_observations && active_observations->f_null_registered; }
};
struct observation_guard {
    observations *previous;
    explicit observation_guard( observations &value ) : previous( active_observations ) { active_observations = &value; }
    ~observation_guard() { active_observations = previous; }
};
void debugmsg( const char *format, int x, int y, int z )
{
    require( std::strcmp( format, "Tried to build floor cache at (%d,%d,%d) but the submap is not loaded" ) == 0,
             "the original unloaded-submap diagnostic remains exact" );
    require( active_observations != nullptr, "debug callback has an owned fixture" );
    active_observations->note( unloaded_debug, x, y, z );
}
struct definition {
    unsigned flags = 0;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0, sx = 0, sy = 0;
    bool uniform = true;
    bool flag( ter_furn_flag value, read_kind kind ) const
    {
        if( owner && !uniform ) owner->note( kind, gx, gy, z, sx, sy, int( value ) );
        return ( flags & ( 1u << unsigned( value ) ) ) != 0;
    }
};
struct ter_t : definition {
    static std::size_t count() noexcept { return 1; }
    bool has_flag( ter_furn_flag f ) const { return flag( f, terrain_flag ); }
};
struct furn_t : definition { bool has_flag( ter_furn_flag f ) const { return flag( f, furniture_flag ); } };
template<class Definition, read_kind InvalidKind> struct native_id {
    const Definition *value = nullptr, *fallback = nullptr;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0, sx = 0, sy = 0;
    bool valid = false;
    native_id() = default;
    native_id( const Definition *v, const Definition *f, observations *o, int x, int y,
               int level, point_sm_ms p, bool present ) : value( v ), fallback( f ), owner( o ),
        gx( x ), gy( y ), z( level ), sx( p.x() ), sy( p.y() ), valid( present ) {}
    // These pinned engines define terrain is_valid() inline in mapdata.cpp,
    // without an externally linkable body. Deliberately leave it undefined;
    // only the furniture specialization below is available to the include.
    bool is_valid() const noexcept;
    int to_i() const noexcept { return valid && value ? 0 : owner ? owner->invalid_terrain_id : -1; }
    const Definition &obj() const
    {
        if( valid && value != nullptr ) return *value;
        if( owner ) {
            if constexpr( InvalidKind == invalid_terrain_debug ) ++owner->invalid_terrain;
            else ++owner->invalid_furniture;
            owner->note( InvalidKind, gx, gy, z, sx, sy );
        }
        if( fallback ) return *fallback;
        static const Definition dummy;
        return dummy;
    }
};
template<> bool native_id<furn_t, invalid_furniture_debug>::is_valid() const noexcept
{
    return valid && value != nullptr;
}
using ter_id = native_id<ter_t, invalid_terrain_debug>;
using furn_id = native_id<furn_t, invalid_furniture_debug>;
struct submap {
    bool uniform_storage = true;
    bool is_uniform() const noexcept { return uniform_storage; }
    bool uniform_terrain_valid = true;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0;
    ter_t uniform_terrain, dummy_terrain;
    const furn_t *f_null = nullptr, *dummy_furniture = nullptr;
    const bool *f_null_valid = nullptr;
    std::array<ter_t, SEEX * SEEY> terrain;
    std::array<furn_t, SEEX * SEEY> furniture;
    std::array<bool, SEEX * SEEY> terrain_valid{}, furniture_valid{};
    ter_id get_ter( point_sm_ms p ) const
    {
        require( p.x() >= 0 && p.x() < SEEX && p.y() >= 0 && p.y() < SEEY, "terrain coordinate stays inside its submap" );
        ++owner->terrain_reads;
        if( !is_uniform() ) owner->note( terrain_read, gx, gy, z, p.x(), p.y() );
        const auto index = std::size_t( p.x() * SEEY + p.y() );
        return {is_uniform() ? &uniform_terrain : &terrain[index], &dummy_terrain, owner,
                gx, gy, z, p, is_uniform() ? uniform_terrain_valid : terrain_valid[index]};
    }
    furn_id get_furn( point_sm_ms p ) const
    {
        require( p.x() >= 0 && p.x() < SEEX && p.y() >= 0 && p.y() < SEEY, "furniture coordinate stays inside its submap" );
        ++owner->furniture_reads;
        if( !is_uniform() ) owner->note( furniture_read, gx, gy, z, p.x(), p.y() );
        const auto index = std::size_t( p.x() * SEEY + p.y() );
        if( is_uniform() && !owner->f_null_registered ) {
            // Native string-ID conversion diagnoses before int-ID obj(). A
            // missing registry must use the original per-cell conversion path.
            // This controlled registry returns a valid, empty dummy int-ID.
            ++owner->invalid_null_conversions;
            owner->note( null_conversion_debug, gx, gy, z, p.x(), p.y() );
            return {dummy_furniture, dummy_furniture, owner, gx, gy, z, p, true};
        }
        return {is_uniform() ? f_null : &furniture[index], dummy_furniture, owner,
                gx, gy, z, p, is_uniform() ? *f_null_valid : furniture_valid[index]};
    }
};
struct floor_grid {
    // Flat backing makes the Native &floor_cache[0][0] fill a real contiguous
    // bool array while preserving its exact [x][y] indexing and full dimensions.
    std::array<bool, tile_count> bits{};
    bool *operator[]( std::size_t x ) { return bits.data() + x * MAPSIZE_Y; }
    const bool *operator[]( std::size_t x ) const { return bits.data() + x * MAPSIZE_Y; }
};
struct level_cache { floor_grid floor_cache; bool floor_cache_dirty = true, no_floor_gaps = false; };
struct scenario {
    int width = 1, z = 0, terrain_shape = 0, furniture_shape = 0, seed = 0, missing = 0;
    unsigned uniform_flags = 0;
    bool zlevels = true, dirty = true, cache_present = true, f_null_sun = false, mixed_grids = false;
    bool invalid_uniform_terrain = false, invalid_f_null = false, invalid_nonuniform_terrain = false, invalid_nonuniform_furniture = false;
    bool absent_f_null_registry = false;
};
struct map_state {
    int my_MAPSIZE;
    bool zlevels;
    int selected_z;
    bool cache_present, f_null_valid;
    observations observed;
    std::array<std::uint64_t, 4> before{0x9182736455aa33ccULL, 5, 9, 17};
    level_cache cache;
    std::array<std::uint64_t, 4> after{0x13579bdf2468ace0ULL, 21, 29, 33};
    furn_t f_null, dummy_furniture;
    std::vector<std::unique_ptr<submap>> current, below;
    explicit map_state( const scenario &s ) : my_MAPSIZE( s.width ), zlevels( s.zlevels ), selected_z( s.z ),
        cache_present( s.cache_present ), f_null_valid( !s.invalid_f_null )
    {
        require( my_MAPSIZE > 0 && my_MAPSIZE <= 11, "fixture uses a valid Native map extent" );
        observed.f_null_registered = !s.absent_f_null_registry;
        cache.floor_cache_dirty = s.dirty; cache.no_floor_gaps = ( s.seed & 1 ) != 0;
        for( std::size_t i = 0; i < tile_count; ++i ) cache.floor_cache.bits[i] = ( i * 37 + unsigned( s.seed ) ) % 7 < 3;
        // A uniform submap uses the shared f_null definition, which mods may
        // legally give SUN_ROOF_ABOVE. No per-submap substitute is allowed.
        f_null.flags = s.f_null_sun ? sun : 0;
        f_null.owner = &observed; dummy_furniture.owner = &observed;
        current.reserve( std::size_t( my_MAPSIZE * my_MAPSIZE ) );
        below.reserve( std::size_t( my_MAPSIZE * my_MAPSIZE ) );
        for( int x = 0; x < my_MAPSIZE; ++x ) for( int y = 0; y < my_MAPSIZE; ++y ) {
            bool cur_present = true, lower_present = true;
            const bool corner = ( x == 0 && y == 0 ) || ( x == my_MAPSIZE - 1 && y == my_MAPSIZE - 1 );
            if( corner && ( s.missing == 1 || s.missing == 3 ) ) cur_present = false;
            if( corner && ( s.missing == 2 || s.missing == 3 ) ) lower_present = false;
            current.push_back( cur_present ? make_submap( s, x, y, false ) : nullptr );
            below.push_back( lower_present ? make_submap( s, x, y, true ) : nullptr );
        }
    }
    std::unique_ptr<submap> make_submap( const scenario &s, int x, int y, bool lower )
    {
        auto sm = std::make_unique<submap>();
        sm->uniform_storage = lower ? s.furniture_shape == 0 : s.terrain_shape == 0;
        if( s.mixed_grids ) sm->uniform_storage = lower ? ( x * 3 + y + s.seed ) % 3 == 0 : ( x + y + s.seed ) % 2 == 0;
        sm->owner = &observed; sm->gx = x; sm->gy = y; sm->z = s.z - ( lower ? 1 : 0 );
        sm->uniform_terrain_valid = !s.invalid_uniform_terrain;
        sm->uniform_terrain.flags = s.uniform_flags;
        sm->uniform_terrain.owner = &observed;
        sm->f_null = &f_null; sm->f_null_valid = &f_null_valid; sm->dummy_furniture = &dummy_furniture;
        sm->dummy_terrain.owner = &observed;
        for( int sx = 0; sx < SEEX; ++sx ) for( int sy = 0; sy < SEEY; ++sy ) {
            const auto at = std::size_t( sx * SEEY + sy );
            auto &ter = sm->terrain[at]; auto &furn = sm->furniture[at];
            const int shape = s.mixed_grids ? 2 : s.terrain_shape;
            const unsigned pattern = unsigned( sx + 7 * sy + 3 * x + 5 * y + s.seed ) & 15u;
            ter.flags = shape == 1 ? 0u : shape == 2 ? pattern : shape == 3 ?
                        ( sx == 0 || sx == 11 || sy == 0 || sy == 11 ? 1u << unsigned( ( sx + sy ) % 4 ) : 0u ) :
                        shape == 4 ? 1u << unsigned( ( sx + sy + s.seed ) % 4 ) : s.uniform_flags;
            const int furniture_shape = s.mixed_grids ? 1 : s.furniture_shape;
            furn.flags = furniture_shape == 1 ? ( ( sx + sy + x + s.seed ) % 3 == 0 ? sun : 0u ) :
                         furniture_shape == 2 ? ( sx == 0 || sx == 11 || sy == 0 || sy == 11 ? sun : 0u ) :
                         furniture_shape == 3 ? sun : 0u;
            for( definition *def : {static_cast<definition *>( &ter ), static_cast<definition *>( &furn )} ) {
                def->owner = &observed; def->uniform = false;
                def->gx = x; def->gy = y; def->z = sm->z; def->sx = sx; def->sy = sy;
            }
            const bool edge = ( sx == 0 && sy == 0 ) || ( sx == 11 && sy == 11 );
            sm->terrain_valid[at] = !( s.invalid_nonuniform_terrain && edge );
            sm->furniture_valid[at] = !( s.invalid_nonuniform_furniture && edge );
        }
        return sm;
    }
    level_cache *get_cache_lazy( int z )
    {
        observed.note( cache_lookup, 0, 0, z );
        return cache_present && z == selected_z ? &cache : nullptr;
    }
    const submap *get_submap_at_grid( tripoint_rel_sm p )
    {
        observed.note( submap_lookup, p.x, p.y, p.z );
        require( p.x >= 0 && p.x < my_MAPSIZE && p.y >= 0 && p.y < my_MAPSIZE, "submap lookup stays in bounds" );
        const auto at = std::size_t( p.x * my_MAPSIZE + p.y );
        // Native get_nonant aliases Z on maps without Z levels; the lower
        // query then returns current furniture, not a fabricated second level.
        if( !zlevels || p.z == selected_z ) return current[at].get();
        if( p.z == selected_z - 1 ) return below[at].get();
        return nullptr;
    }
};
namespace frozen {
struct map : map_state { using map_state::map_state; bool build_floor_cache( int ); };
// BEGIN FROZEN NATIVE FLOOR CACHE
bool map::build_floor_cache( const int zlev )
{
    auto *ch_lazy = get_cache_lazy( zlev );
    if( !ch_lazy || !ch_lazy->floor_cache_dirty ) {
        return false;
    }
    level_cache &ch = *ch_lazy;

    auto &floor_cache = ch.floor_cache;
    std::uninitialized_fill_n(
        &floor_cache[0][0], MAPSIZE_X * MAPSIZE_Y, true );
    bool &no_floor_gaps = ch.no_floor_gaps;
    no_floor_gaps = true;

    bool lowest_z_lev = zlev <= -OVERMAP_DEPTH;

    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const submap *cur_submap = get_submap_at_grid( tripoint_rel_sm{ smx, smy, zlev } );
            const submap *below_submap = !lowest_z_lev ? get_submap_at_grid( tripoint_rel_sm{ smx, smy, zlev - 1 } ) :
                                         nullptr;

            if( cur_submap == nullptr ) {
                debugmsg( "Tried to build floor cache at (%d,%d,%d) but the submap is not loaded", smx, smy, zlev );
                continue;
            }
            if( !lowest_z_lev && below_submap == nullptr ) {
                debugmsg( "Tried to build floor cache at (%d,%d,%d) but the submap is not loaded", smx, smy,
                          zlev - 1 );
                continue;
            }

            for( int sx = 0; sx < SEEX; ++sx ) {
                for( int sy = 0; sy < SEEY; ++sy ) {
                    point_sm_ms sp( sx, sy );
                    const ter_t &terrain = cur_submap->get_ter( sp ).obj();
                    if( terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR ) ||
                        terrain.has_flag( ter_furn_flag::TFLAG_NO_FLOOR_WATER ) ||
                        terrain.has_flag( ter_furn_flag::TFLAG_GOES_DOWN ) ||
                        terrain.has_flag( ter_furn_flag::TFLAG_TRANSPARENT_FLOOR ) ) {
                        if( below_submap &&
                            below_submap->get_furn( sp ).obj().has_flag( ter_furn_flag::TFLAG_SUN_ROOF_ABOVE ) ) {
                            continue;
                        }
                        const point p( sx + smx * SEEX, sy + smy * SEEY );
                        floor_cache[p.x][p.y] = false;
                        no_floor_gaps = false;
                    }
                }
            }
        }
    }

    ch.floor_cache_dirty = false;
    return zlevels;
}
// END FROZEN NATIVE FLOOR CACHE
} // namespace frozen
namespace actual {
struct map : map_state { using map_state::map_state; bool build_floor_cache( int ); };
#include "ncmm_graphics_floor_cache.inc"
} // namespace actual
struct outcome {
    std::array<bool, tile_count> floor;
    bool returned, dirty, no_floor_gaps;
    std::array<std::uint64_t, 4> before, after;
    std::vector<read_event> required;
    std::uint64_t terrain_reads, furniture_reads, invalid_terrain, invalid_furniture;
    std::uint64_t invalid_null_conversions;
};
template<class Map> outcome run( const scenario &s, int invalid_terrain_id )
{
    auto map = std::make_unique<Map>( s );
    map->observed.invalid_terrain_id = invalid_terrain_id;
    observation_guard observe( map->observed );
    const bool returned = map->build_floor_cache( s.z );
    return {map->cache.floor_cache.bits, returned, map->cache.floor_cache_dirty, map->cache.no_floor_gaps,
            map->before, map->after, std::move( map->observed.required ), map->observed.terrain_reads,
            map->observed.furniture_reads, map->observed.invalid_terrain, map->observed.invalid_furniture,
            map->observed.invalid_null_conversions};
}
std::pair<outcome, outcome> paired( const scenario &s, int invalid_terrain_id = 1 )
{
    auto original = run<frozen::map>( s, invalid_terrain_id );
    auto observed = run<actual::map>( s, invalid_terrain_id ); ++cases;
    for( std::size_t i = 0; i < tile_count; ++i ) require( original.floor[i] == observed.floor[i], "every floor-cache bit equals the frozen Native oracle" );
    require( original.returned == observed.returned && original.dirty == observed.dirty && original.no_floor_gaps == observed.no_floor_gaps,
             "Native return, dirty flag and no_floor_gaps equal the oracle" );
    require( original.before == observed.before && original.after == observed.after, "full-grid writes preserve both bounds guards" );
    require( original.required == observed.required, "cache/submap/debug and ordered nonuniform reads remain exact" );
    require( original.invalid_terrain == observed.invalid_terrain && original.invalid_furniture == observed.invalid_furniture &&
             original.invalid_null_conversions == observed.invalid_null_conversions,
             "invalid IDs preserve every diagnostic from obj(), including uniform fallback" );
    return {std::move( original ), std::move( observed )};
}
void run_cases()
{
    checks = cases = 0;
    for( int id : {std::numeric_limits<int>::min(), -1, 1, std::numeric_limits<int>::max()} )
        for( int width : {1, 3} ) {
            scenario s; s.width = width; s.uniform_flags = 1; s.invalid_uniform_terrain = true;
            const auto result = paired( s, id );
            require( result.first.invalid_terrain == std::uint64_t( width * width * 144 ),
                     "negative and out-of-table integer IDs keep every original floor diagnostic" );
        }
    for( unsigned mask = 0; mask < 16; ++mask ) for( bool roof : {false, true} )
        for( bool zlevels : {false, true} ) for( bool lowest : {false, true} ) for( int width : {1, 3} ) {
            scenario s; s.uniform_flags = mask; s.f_null_sun = roof; s.zlevels = zlevels;
            s.z = lowest ? -OVERMAP_DEPTH : 0; s.width = width; s.seed = int( mask ); paired( s );
        }
    for( int terrain_shape = 1; terrain_shape <= 4; ++terrain_shape ) for( int furn_shape = 0; furn_shape <= 2; ++furn_shape )
        for( bool roof : {false, true} ) for( bool zlevels : {false, true} ) for( bool lowest : {false, true} ) {
            scenario s; s.terrain_shape = terrain_shape; s.furniture_shape = furn_shape; s.f_null_sun = roof;
            s.zlevels = zlevels; s.z = lowest ? -OVERMAP_DEPTH : 0;
            s.seed = terrain_shape * 3 + furn_shape; s.width = 1 + s.seed % 3; paired( s );
        }
    for( int seed = 0; seed < 8; ++seed ) for( bool zlevels : {false, true} ) for( bool lowest : {false, true} ) {
        scenario s; s.width = 3; s.mixed_grids = true; s.seed = seed; s.uniform_flags = unsigned( seed * 3 ) & 15u;
        s.f_null_sun = ( seed & 1 ) != 0; s.zlevels = zlevels; s.z = lowest ? -OVERMAP_DEPTH : 0; paired( s );
    }
    for( int missing = 1; missing <= 3; ++missing ) for( bool zlevels : {false, true} )
        for( bool lowest : {false, true} ) for( int width : {1, 3} ) {
            scenario s; s.width = width; s.uniform_flags = 15; s.missing = missing; s.zlevels = zlevels;
            s.z = lowest ? -OVERMAP_DEPTH : 0; s.seed = missing; paired( s );
        }
    for( int state = 0; state < 3; ++state ) for( bool zlevels : {false, true} ) for( bool lowest : {false, true} ) {
        scenario s; s.cache_present = state == 0; s.dirty = state == 1; s.zlevels = zlevels;
        s.z = lowest ? -OVERMAP_DEPTH : 0; s.seed = state; paired( s );
    }
    for( int kind = 0; kind < 4; ++kind ) for( bool zlevels : {false, true} ) for( bool roof : {false, true} ) {
        scenario s; s.uniform_flags = 1; s.zlevels = zlevels; s.f_null_sun = roof;
        s.invalid_uniform_terrain = kind == 0; s.invalid_f_null = kind == 1;
        s.invalid_nonuniform_terrain = kind == 2; s.invalid_nonuniform_furniture = kind == 3;
        if( kind >= 2 ) { s.terrain_shape = 4; s.furniture_shape = 1; }
        const auto result = paired( s );
        if( kind == 0 ) require( result.first.invalid_terrain == 144, "invalid uniform terrain diagnoses on every Native obj() call" );
        if( kind == 1 ) require( result.first.invalid_furniture == 144, "invalid shared f_null diagnoses on every Native obj() call" );
    }
    for( bool roof : {false, true} ) for( bool zlevels : {false, true} ) for( int z : {-OVERMAP_DEPTH, 10} ) {
        scenario s; s.width = 11; s.terrain_shape = 3; s.furniture_shape = 2;
        s.zlevels = zlevels; s.f_null_sun = roof; s.z = z; s.seed = 7; paired( s );
    }
    // Uniform current terrain does not imply a uniform lower storey. A solid
    // current floor never reads lower furniture; a gap still needs each actual
    // SUN_ROOF_ABOVE predicate, including invalid and missing lower definitions.
    for( unsigned mask = 0; mask < 16; ++mask ) for( int lower_shape : {1, 2} )
        for( bool zlevels : {false, true} ) for( int z : {-OVERMAP_DEPTH, 0, 10} ) for( int width : {1, 11} ) {
            scenario s; s.uniform_flags = mask; s.furniture_shape = lower_shape;
            s.zlevels = zlevels; s.z = z; s.width = width; paired( s );
        }
    for( unsigned mask : {0u, 1u, 15u} ) for( int kind = 0; kind < 4; ++kind ) {
        scenario s; s.width = 3; s.uniform_flags = mask; s.furniture_shape = 1;
        s.invalid_uniform_terrain = kind == 0; s.invalid_nonuniform_furniture = kind == 1;
        s.missing = kind == 2 ? 2 : 0; s.absent_f_null_registry = kind == 3; paired( s );
    }
    for( bool zlevels : {false, true} ) for( bool roof : {false, true} ) for( bool nonuniform : {false, true} ) {
        scenario s; s.uniform_flags = 1; s.absent_f_null_registry = true;
        s.zlevels = zlevels; s.f_null_sun = roof; s.terrain_shape = nonuniform ? 4 : 0;
        const auto result = paired( s );
        require( result.first.invalid_null_conversions == ( !zlevels && nonuniform ? 0u : 144u ),
                 "absent f_null string conversion keeps every diagnostic, respecting Native Z alias" );
    }
    std::cout << "Native floor-cache frozen-oracle matrix: " << cases << " cases passed before the final optimization assertion\n";
    // Deliberately last: an unoptimized actual include must first pass every
    // semantic oracle above, then fail only this measurable repeated-read case.
    scenario dense; dense.width = 11; dense.uniform_flags = 1; dense.z = 0;
    const auto result = paired( dense );
    std::cout << "Native uniform floor-cache reads: terrain " << result.first.terrain_reads << " -> " << result.second.terrain_reads
              << ", furniture " << result.first.furniture_reads << " -> " << result.second.furniture_reads << '\n';
    require( result.first.terrain_reads == 17424 && result.first.furniture_reads == 17424, "the frozen baseline performs 144 terrain/furniture reads per submap" );
    require( result.second.terrain_reads <= 121 && result.second.furniture_reads <= 121,
             "fully uniform 11x11 floor build requires at most one terrain and one furniture read per submap (fail-before control)" );
    scenario solid_above_mixed; solid_above_mixed.width = 11; solid_above_mixed.furniture_shape = 1;
    const auto mixed_result = paired( solid_above_mixed );
    std::cout << "Native uniform solid floors over mixed lower submaps: terrain " << mixed_result.first.terrain_reads << " -> " << mixed_result.second.terrain_reads
              << ", furniture " << mixed_result.first.furniture_reads << " -> " << mixed_result.second.furniture_reads << '\n';
    require( mixed_result.first.terrain_reads == 17424 && !mixed_result.first.furniture_reads,
             "Native solid floors do not consult lower furniture even when it is mixed" );
    require( mixed_result.second.terrain_reads <= 121 && !mixed_result.second.furniture_reads,
             "uniform solid floor predicates are resolved once despite mixed lower storage (fail-before control)" );
    std::cout << "Native floor-cache component: " << cases << " cases / " << checks << " checks passed; no Native timing claim\n";
}
} // namespace native_floor_cache_fixture

void test_native_floor_cache()
{
    native_floor_cache_fixture::run_cases();
}
