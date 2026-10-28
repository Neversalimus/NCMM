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

// Actual Native control flow is compiled against owned definitions below.
// Read counts are component evidence, never elapsed Native
// time or FPS, and do not establish what an optimizing Native compiler hoists.
// Frozen code: Cataclysm: DDA contributors, CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L10515-L10571
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L10527-L10583
// Complete identical body: 2217 bytes including final LF, SHA256
// 99181cb92ff370313fa5870684f513ee0dad010dcc0798f6aa9587b085a66abc.
namespace native_outside_cache_fixture {
using std::size_t;
constexpr int SEEX = 12, SEEY = 12, MAPSIZE_X = 132, MAPSIZE_Y = 132;
constexpr size_t tile_count = size_t( MAPSIZE_X ) * MAPSIZE_Y;
constexpr std::array<std::uint64_t, 4> front_guard{0x123456789abcdef0ULL, 7, 11, 23};
constexpr std::array<std::uint64_t, 4> back_guard{0x0fedcba987654321ULL, 31, 47, 59};
size_t checks = 0, cases = 0;
void require( bool ok, const char *message )
{
    ++checks;
    if( !ok ) throw std::runtime_error( std::string( "Native outside cache: " ) + message );
}

enum class ter_furn_flag : unsigned { TFLAG_INDOORS };
struct point_sm_ms {
    int a = 0, b = 0;
    point_sm_ms() = default;
    point_sm_ms( int x, int y ) : a( x ), b( y ) {}
    int x() const noexcept { return a; }
    int y() const noexcept { return b; }
    static const point_sm_ms zero;
};
const point_sm_ms point_sm_ms::zero{};
struct point { int x, y; point( int a, int b ) : x( a ), y( b ) {} };
struct point_bub_ms {};
struct tripoint_rel_sm { int x, y, z; };

enum read_kind {
    cache_lookup, submap_lookup, unloaded_debug, terrain_read, furniture_read,
    terrain_flag, furniture_flag, invalid_terrain_debug, invalid_furniture_debug,
    null_conversion_debug
};
using read_event = std::array<int, 8>;
struct observations {
    std::vector<read_event> required;
    std::uint64_t terrain_reads = 0, furniture_reads = 0;
    std::uint64_t invalid_terrain = 0, invalid_furniture = 0, invalid_null_conversions = 0;
    bool f_null_registered = true;
    int invalid_terrain_id = 1;
    observations() { required.reserve( 4096 ); }
    void note( read_kind kind, int x, int y, int z, int sx = 0, int sy = 0, int flag = 0 )
    { required.push_back( {int( kind ), x, y, z, sx, sy, flag, 0} ); }
};
observations *active_observations = nullptr;
struct observation_guard {
    observations *previous;
    explicit observation_guard( observations &value ) : previous( active_observations )
    { active_observations = &value; }
    ~observation_guard() { active_observations = previous; }
};
struct furn_str_id {
    static furn_str_id NULL_ID() noexcept { return {}; }
    bool is_valid() const noexcept
    { return active_observations && active_observations->f_null_registered; }
};
void debugmsg( const char *format, int x, int y, int z )
{
    require( std::strcmp( format, "Tried to build outside cache at (%d,%d,%d) but the submap is not loaded" ) == 0,
             "the Native missing-submap diagnostic remains exact" );
    require( active_observations != nullptr, "diagnostic has an owned observation context" );
    active_observations->note( unloaded_debug, x, y, z );
}

// Pointer row access deliberately retains the Native contiguous bool layout.
// The frozen body is not instrumented to count or rewrite individual stamps.
namespace cata {
template<class T, class Coordinate, size_t Width, size_t Height> struct mdarray {
    std::array<std::uint64_t, 4> before = front_guard;
    std::array<T, Width * Height> values{};
    std::array<std::uint64_t, 4> after = back_guard;
    void fill( const T &value ) { values.fill( value ); }
    T *operator[]( size_t x )
    {
        require( x < Width, "padded Native row stays in bounds" );
        return values.data() + x * Height;
    }
    const T *operator[]( size_t x ) const
    {
        require( x < Width, "padded const Native row stays in bounds" );
        return values.data() + x * Height;
    }
    ~mdarray() noexcept( false )
    { require( before == front_guard && after == back_guard, "padded-cache guard words survive every stamp" ); }
};
}
struct definition {
    bool indoors = false, uniform = true;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0, sx = 0, sy = 0;
    bool flag( ter_furn_flag value, read_kind kind ) const
    {
        require( value == ter_furn_flag::TFLAG_INDOORS, "only the original INDOORS flag is queried" );
        if( owner && !uniform ) owner->note( kind, gx, gy, z, sx, sy, int( value ) );
        return indoors;
    }
};
struct ter_t : definition {
    static std::size_t count() noexcept { return 1; }
    bool has_flag( ter_furn_flag value ) const { return flag( value, terrain_flag ); }
};
struct furn_t : definition {
    bool has_flag( ter_furn_flag value ) const { return flag( value, furniture_flag ); }
};
template<class Definition, read_kind InvalidKind> struct native_id {
    const Definition *value = nullptr, *fallback = nullptr;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0, sx = 0, sy = 0;
    bool valid = false;
    native_id() = default;
    native_id( const Definition *v, const Definition *f, observations *o, int x, int y,
               int level, point_sm_ms p, bool present ) : value( v ), fallback( f ), owner( o ),
        gx( x ), gy( y ), z( level ), sx( p.x() ), sy( p.y() ), valid( present ) {}
    // Match the pinned link surface: terrain is_valid() has no external body.
    // Furniture is_valid() is separately defined and linkable in Native.
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
    bool uniform_storage = true, uniform_terrain_valid = true;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0;
    ter_t uniform_terrain, dummy_terrain;
    const furn_t *f_null = nullptr, *dummy_furniture = nullptr;
    const bool *f_null_valid = nullptr;
    std::array<ter_t, SEEX * SEEY> terrain;
    std::array<furn_t, SEEX * SEEY> furniture;
    std::array<bool, SEEX * SEEY> terrain_valid{}, furniture_valid{};
    bool is_uniform() const noexcept { return uniform_storage; }
    ter_id get_ter( point_sm_ms p ) const
    {
        require( p.x() >= 0 && p.x() < SEEX && p.y() >= 0 && p.y() < SEEY,
                 "terrain getter uses a valid local tile" );
        ++owner->terrain_reads;
        if( !is_uniform() ) owner->note( terrain_read, gx, gy, z, p.x(), p.y() );
        const auto at = size_t( p.x() * SEEY + p.y() );
        return {is_uniform() ? &uniform_terrain : &terrain[at], &dummy_terrain, owner,
                gx, gy, z, p, is_uniform() ? uniform_terrain_valid : terrain_valid[at]};
    }
    furn_id get_furn( point_sm_ms p ) const
    {
        require( p.x() >= 0 && p.x() < SEEX && p.y() >= 0 && p.y() < SEEY,
                 "furniture getter uses a valid local tile" );
        ++owner->furniture_reads;
        if( !is_uniform() ) owner->note( furniture_read, gx, gy, z, p.x(), p.y() );
        const auto at = size_t( p.x() * SEEY + p.y() );
        if( is_uniform() && !owner->f_null_registered ) {
            // Missing string ID conversion can warn even if the resulting
            // dummy int ID is valid. A quiet registry guard must precede it.
            ++owner->invalid_null_conversions;
            owner->note( null_conversion_debug, gx, gy, z, p.x(), p.y() );
            return {dummy_furniture, dummy_furniture, owner, gx, gy, z, p, true};
        }
        return {is_uniform() ? f_null : &furniture[at], dummy_furniture, owner,
                gx, gy, z, p, is_uniform() ? *f_null_valid : furniture_valid[at]};
    }
};
struct outside_grid {
    std::array<bool, tile_count> bits{};
    bool *operator[]( size_t x )
    {
        require( x < MAPSIZE_X, "outside-cache row stays in bounds" );
        return bits.data() + x * MAPSIZE_Y;
    }
    const bool *operator[]( size_t x ) const
    {
        require( x < MAPSIZE_X, "outside-cache const row stays in bounds" );
        return bits.data() + x * MAPSIZE_Y;
    }
};
struct level_cache {
    outside_grid outside_cache;
    bool outside_cache_dirty = true, untouched_dirty = true;
    std::uint64_t untouched_revision = 0x92317abc654ULL;
};
struct scenario {
    int width = 1, z = 0, seed = 0, terrain_shape = 0, furniture_shape = 0, missing = 0;
    bool uniform = true, mixed = false, alternate_uniform = false;
    bool terrain_indoors = false, f_null_indoors = false;
    bool cache_present = true, dirty = true;
    bool invalid_uniform_terrain = false, invalid_f_null = false, absent_f_null_registry = false;
    bool invalid_nonuniform_terrain = false, invalid_nonuniform_furniture = false;
    bool dummy_terrain_indoors = false, dummy_furniture_indoors = false;
};
bool tile_pattern( int mode, int x, int y, int gx, int gy, int seed, bool fallback )
{
    switch( mode ) {
        case 0: return fallback;
        case 1: return ( x * 5 + y * 3 + gx + gy * 2 + seed ) % 11 == 0;
        case 2: return x == 0 || x == SEEX - 1 || y == 0 || y == SEEY - 1;
        case 3: return ( x == 0 || x == SEEX - 1 ) && ( y == 0 || y == SEEY - 1 );
        case 4: return x == SEEX / 2 && y == SEEY / 2;
        case 5: return true;
        case 6: return false;
        case 7: return ( ( x + gx * SEEX + seed ) % 7 == 0 ) || ( ( y + gy * SEEY ) % 5 == 0 );
        default: throw std::runtime_error( "invalid outside fixture pattern" );
    }
}
struct map_state {
    int my_MAPSIZE, selected_z;
    bool cache_present, f_null_valid;
    observations observed;
    std::array<std::uint64_t, 4> before = front_guard;
    level_cache cache;
    std::array<std::uint64_t, 4> after = back_guard;
    std::array<bool, tile_count> expected;
    furn_t f_null, dummy_furniture;
    std::vector<std::unique_ptr<submap>> submaps;
    explicit map_state( const scenario &s ) : my_MAPSIZE( s.width ), selected_z( s.z ),
        cache_present( s.cache_present ), f_null_valid( !s.invalid_f_null )
    {
        require( my_MAPSIZE > 0 && my_MAPSIZE <= 11, "Native map width stays bounded" );
        observed.f_null_registered = !s.absent_f_null_registry;
        cache.outside_cache_dirty = s.dirty;
        for( size_t i = 0; i < tile_count; ++i ) {
            cache.outside_cache.bits[i] = ( i * 37 + unsigned( s.seed ) ) % 13 < 5;
        }
        expected = cache.outside_cache.bits;
        f_null.indoors = s.f_null_indoors; f_null.owner = &observed;
        dummy_furniture.indoors = s.dummy_furniture_indoors; dummy_furniture.owner = &observed;
        submaps.reserve( size_t( my_MAPSIZE * my_MAPSIZE ) );
        for( int x = 0; x < my_MAPSIZE; ++x ) for( int y = 0; y < my_MAPSIZE; ++y ) {
            const bool corner = ( x == 0 && y == 0 ) || ( x == my_MAPSIZE - 1 && y == my_MAPSIZE - 1 );
            const bool present = s.missing != 3 && !( s.missing == 1 && corner ) &&
                                 !( s.missing == 2 && ( x + y + s.seed ) % 3 == 0 );
            if( !present ) { submaps.push_back( nullptr ); continue; }
            auto sm = std::make_unique<submap>();
            sm->owner = &observed; sm->gx = x; sm->gy = y; sm->z = s.z;
            sm->uniform_storage = s.mixed ? ( x + y + s.seed ) % 3 != 0 : s.uniform;
            sm->uniform_terrain_valid = !s.invalid_uniform_terrain;
            sm->uniform_terrain.indoors = s.alternate_uniform ? ( x + 2 * y + s.seed ) % 2 != 0 : s.terrain_indoors;
            sm->uniform_terrain.owner = &observed;
            sm->dummy_terrain.indoors = s.dummy_terrain_indoors; sm->dummy_terrain.owner = &observed;
            sm->f_null = &f_null; sm->f_null_valid = &f_null_valid; sm->dummy_furniture = &dummy_furniture;
            for( int sx = 0; sx < SEEX; ++sx ) for( int sy = 0; sy < SEEY; ++sy ) {
                const auto at = size_t( sx * SEEY + sy );
                auto &ter = sm->terrain[at]; auto &furn = sm->furniture[at];
                ter.indoors = tile_pattern( s.terrain_shape, sx, sy, x, y, s.seed, s.terrain_indoors );
                furn.indoors = tile_pattern( s.furniture_shape, sx, sy, x, y, s.seed + 1, s.f_null_indoors );
                for( definition *def : {static_cast<definition *>( &ter ), static_cast<definition *>( &furn )} ) {
                    def->owner = &observed; def->uniform = false;
                    def->gx = x; def->gy = y; def->z = s.z; def->sx = sx; def->sy = sy;
                }
                const bool invalid_edge = ( sx == 0 && sy == 0 ) || ( sx == SEEX - 1 && sy == SEEY - 1 );
                sm->terrain_valid[at] = !( s.invalid_nonuniform_terrain && invalid_edge );
                sm->furniture_valid[at] = !( s.invalid_nonuniform_furniture && invalid_edge );
            }
            submaps.push_back( std::move( sm ) );
        }
        if( !s.cache_present || !s.dirty ) return;
        if( s.z < 0 ) { expected.fill( false ); return; }
        const int extent = my_MAPSIZE * SEEX;
        // Independent mathematical oracle: an output tile is inside iff any
        // existing source tile at Chebyshev distance <= 1 is INDOORS. It neither
        // calls Native getters nor constructs or indexes a padded cache.
        for( int x = 0; x < extent; ++x ) for( int y = 0; y < extent; ++y ) {
            bool indoors = false;
            for( int dx = -1; dx <= 1; ++dx ) for( int dy = -1; dy <= 1; ++dy ) {
                indoors = indoors || raw_indoors( x + dx, y + dy );
            }
            expected[size_t( x * MAPSIZE_Y + y )] = !indoors;
        }
    }
    const submap *raw_submap( int gx, int gy ) const
    { return submaps[size_t( gx * my_MAPSIZE + gy )].get(); }
    bool raw_terrain_indoors( const submap &sm, int sx, int sy ) const
    {
        const auto at = size_t( sx * SEEY + sy );
        const bool valid = sm.uniform_storage ? sm.uniform_terrain_valid : sm.terrain_valid[at];
        return valid ? ( sm.uniform_storage ? sm.uniform_terrain.indoors : sm.terrain[at].indoors ) : sm.dummy_terrain.indoors;
    }
    bool raw_indoors( int x, int y ) const
    {
        const int extent = my_MAPSIZE * SEEX;
        if( x < 0 || y < 0 || x >= extent || y >= extent ) return false;
        const submap *sm = raw_submap( x / SEEX, y / SEEY );
        if( !sm ) return false;
        const int sx = x % SEEX, sy = y % SEEY;
        if( raw_terrain_indoors( *sm, sx, sy ) ) return true;
        if( sm->uniform_storage ) {
            return !observed.f_null_registered || !f_null_valid ? dummy_furniture.indoors : f_null.indoors;
        }
        const auto at = size_t( sx * SEEY + sy );
        return sm->furniture_valid[at] ? sm->furniture[at].indoors : dummy_furniture.indoors;
    }
    std::uint64_t original_terrain_count() const
    {
        if( !cache_present || !cache.outside_cache_dirty || selected_z < 0 ) return 0;
        std::uint64_t total = 0;
        for( const auto &sm : submaps ) if( sm ) total += SEEX * SEEY;
        return total;
    }
    std::uint64_t original_furniture_count() const
    {
        if( !cache_present || !cache.outside_cache_dirty || selected_z < 0 ) return 0;
        std::uint64_t total = 0;
        for( const auto &sm : submaps ) if( sm ) {
            for( int x = 0; x < SEEX; ++x ) for( int y = 0; y < SEEY; ++y ) {
                if( !raw_terrain_indoors( *sm, x, y ) ) ++total;
            }
        }
        return total;
    }
    level_cache *get_cache_lazy( int z )
    {
        observed.note( cache_lookup, 0, 0, z );
        require( z == selected_z, "cache lookup retains the exact requested Z" );
        return cache_present ? &cache : nullptr;
    }
    const submap *get_submap_at_grid( tripoint_rel_sm p )
    {
        observed.note( submap_lookup, p.x, p.y, p.z );
        require( p.x >= 0 && p.y >= 0 && p.x < my_MAPSIZE && p.y < my_MAPSIZE && p.z == selected_z,
                 "every original submap lookup retains its XYZ and order" );
        return raw_submap( p.x, p.y );
    }
};

namespace frozen {
struct map : map_state { using map_state::map_state; void build_outside_cache( int ); };
// BEGIN FROZEN NATIVE OUTSIDE CACHE
void map::build_outside_cache( const int zlev )
{
    auto *ch_lazy = get_cache_lazy( zlev );
    if( !ch_lazy || !ch_lazy->outside_cache_dirty ) {
        return;
    }
    level_cache &ch = *ch_lazy;

    // Make a bigger cache to avoid bounds checking
    // We will later copy it to our regular cache
    const size_t padded_w = MAPSIZE_X + 2;
    const size_t padded_h = MAPSIZE_Y + 2;
    cata::mdarray<bool, point_bub_ms, padded_w, padded_h> padded_cache;

    auto &outside_cache = ch.outside_cache;
    if( zlev < 0 ) {
        std::uninitialized_fill_n(
            &outside_cache[0][0], MAPSIZE_X * MAPSIZE_Y, false );
        return;
    }

    padded_cache.fill( true );

    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const submap *cur_submap = get_submap_at_grid( tripoint_rel_sm{ smx, smy, zlev } );
            if( cur_submap == nullptr ) {
                debugmsg( "Tried to build outside cache at (%d,%d,%d) but the submap is not loaded", smx, smy,
                          zlev );
                continue;
            }

            for( int sx = 0; sx < SEEX; ++sx ) {
                for( int sy = 0; sy < SEEY; ++sy ) {
                    point_sm_ms sp( sx, sy );
                    if( cur_submap->get_ter( sp ).obj().has_flag( ter_furn_flag::TFLAG_INDOORS ) ||
                        cur_submap->get_furn( sp ).obj().has_flag( ter_furn_flag::TFLAG_INDOORS ) ) {
                        const point p( sx + smx * SEEX, sy + smy * SEEY );
                        // Add 1 to both coordinates, because we're operating on the padded cache
                        for( int dx = 0; dx <= 2; dx++ ) {
                            for( int dy = 0; dy <= 2; dy++ ) {
                                padded_cache[p.x + dx][p.y + dy] = false;
                            }
                        }
                    }
                }
            }
        }
    }

    // Copy the padded cache back to the proper one, but with no padding
    for( int x = 0; x < SEEX * my_MAPSIZE; x++ ) {
        std::copy_n( &padded_cache[x + 1][1], SEEX * my_MAPSIZE, &outside_cache[x][0] );
    }

    ch.outside_cache_dirty = false;
}
// END FROZEN NATIVE OUTSIDE CACHE
}
namespace actual {
struct map : map_state { using map_state::map_state; void build_outside_cache( int ); };
#include "ncmm_graphics_outside_cache.inc"
}

struct outcome {
    std::array<bool, tile_count> bits{};
    std::vector<read_event> required;
    bool dirty = false;
    std::uint64_t terrain_reads = 0, furniture_reads = 0;
    std::uint64_t invalid_terrain = 0, invalid_furniture = 0, conversions = 0;
};
template<class Map> outcome run( const scenario &s, bool frozen_source, int invalid_terrain_id )
{
    auto m = std::make_unique<Map>( s );
    m->observed.invalid_terrain_id = invalid_terrain_id;
    const auto expected_terrain = m->original_terrain_count(), expected_furniture = m->original_furniture_count();
    observation_guard guard( m->observed );
    m->build_outside_cache( s.z );
    require( m->before == front_guard && m->after == back_guard, "whole outside-cache guard words survive" );
    require( m->cache.untouched_dirty && m->cache.untouched_revision == 0x92317abc654ULL,
             "unrelated cache fields remain untouched" );
    for( size_t i = 0; i < tile_count; ++i ) {
        require( m->cache.outside_cache.bits[i] == m->expected[i],
                 "all Native outside-cache bits, including untouched tails, match mathematical dilation" );
    }
    const bool expected_dirty = s.cache_present && s.dirty && s.z >= 0 ? false : s.dirty;
    require( m->cache.outside_cache_dirty == expected_dirty,
             "early returns and the underground path preserve the original dirty behavior" );
    require( !m->observed.required.empty() && m->observed.required.front()[0] == cache_lookup,
             "cache lookup remains the first Native read" );
    if( !s.cache_present || !s.dirty || s.z < 0 ) {
        require( m->observed.required.size() == 1 && m->observed.terrain_reads == 0 && m->observed.furniture_reads == 0,
                 "guarded Native return performs no submap, terrain or furniture reads" );
    }
    if( frozen_source ) {
        require( m->observed.terrain_reads == expected_terrain && m->observed.furniture_reads == expected_furniture,
                 "frozen getter counts retain exact Native OR short-circuit behavior" );
    }
    if( s.uniform && !s.mixed && s.terrain_indoors && !s.invalid_uniform_terrain ) {
        require( m->observed.furniture_reads == 0 && m->observed.invalid_null_conversions == 0 &&
                 m->observed.invalid_furniture == 0,
                 "INDOORS terrain never queries or converts f_null, including an absent registry" );
    }
    outcome result;
    result.bits = m->cache.outside_cache.bits; result.required = std::move( m->observed.required );
    result.dirty = m->cache.outside_cache_dirty;
    result.terrain_reads = m->observed.terrain_reads; result.furniture_reads = m->observed.furniture_reads;
    result.invalid_terrain = m->observed.invalid_terrain; result.invalid_furniture = m->observed.invalid_furniture;
    result.conversions = m->observed.invalid_null_conversions;
    return result;
}
std::pair<outcome, outcome> paired( const scenario &s, int invalid_terrain_id = 1 )
{
    auto a = run<frozen::map>( s, true, invalid_terrain_id );
    auto b = run<actual::map>( s, false, invalid_terrain_id ); ++cases;
    require( a.bits == b.bits && a.dirty == b.dirty, "actual complete outside-cache and dirty flag equal the frozen Native oracle" );
    require( a.required == b.required,
             "required ordered lookups, nonuniform reads, flags and diagnostics equal the primary Native source" );
    require( a.invalid_terrain == b.invalid_terrain && a.invalid_furniture == b.invalid_furniture && a.conversions == b.conversions,
             "invalid int IDs and absent f_null retain every original diagnostic" );
    return {std::move( a ), std::move( b )};
}

std::vector<scenario> scenarios()
{
    std::vector<scenario> out;
    for( int width : {1, 3, 11} ) for( bool terrain : {false, true} ) for( bool furniture : {false, true} ) {
        scenario s; s.width = width; s.terrain_indoors = terrain; s.f_null_indoors = furniture;
        s.seed = width + int( terrain ) + 3 * int( furniture ); out.push_back( s );
    }
    for( int width : {1, 3, 11} ) {
        for( int pattern : {1, 2, 3, 4, 7} ) {
            scenario s; s.width = width; s.uniform = false; s.terrain_shape = pattern;
            s.furniture_shape = pattern == 2 ? 6 : 1; s.seed = pattern + width; out.push_back( s );
            s.terrain_shape = 6; s.furniture_shape = pattern; out.push_back( s );
        }
        scenario s; s.width = width; s.alternate_uniform = true; s.seed = width; out.push_back( s );
        s.mixed = true; s.terrain_shape = 2; s.furniture_shape = 3; out.push_back( s );
        s.missing = 1; out.push_back( s ); s.missing = 2; out.push_back( s );
        s = {}; s.width = width; s.f_null_indoors = true; s.missing = 1; out.push_back( s );
        s.missing = 3; out.push_back( s );
        for( bool dirty : {false, true} ) for( bool present : {false, true} ) for( int z : {-10, -1, 0, 5} ) {
            scenario guard; guard.width = width; guard.z = z; guard.dirty = dirty; guard.cache_present = present;
            guard.mixed = true; guard.missing = 1; guard.terrain_shape = 3; guard.furniture_shape = 2;
            guard.seed = width + z + 19; out.push_back( guard );
        }
    }
    for( bool terrain : {false, true} ) for( bool furniture : {false, true} ) for( bool fallback : {false, true} ) {
        scenario s; s.width = 3; s.terrain_indoors = terrain; s.f_null_indoors = furniture;
        s.dummy_terrain_indoors = fallback; s.dummy_furniture_indoors = !fallback;
        s.invalid_uniform_terrain = true; out.push_back( s );
        s.invalid_uniform_terrain = false; s.invalid_f_null = true; out.push_back( s );
        s.invalid_f_null = false; s.absent_f_null_registry = true; out.push_back( s );
        s.invalid_uniform_terrain = true; out.push_back( s );
    }
    for( int width : {1, 3, 11} ) for( bool terrain_dummy : {false, true} ) for( bool furniture_dummy : {false, true} ) {
        scenario s; s.width = width; s.uniform = false; s.terrain_shape = 1; s.furniture_shape = 2;
        s.invalid_nonuniform_terrain = s.invalid_nonuniform_furniture = true;
        s.dummy_terrain_indoors = terrain_dummy; s.dummy_furniture_indoors = furniture_dummy; out.push_back( s );
        s.mixed = true; s.invalid_uniform_terrain = true; s.invalid_f_null = true; s.missing = 2; out.push_back( s );
    }
    return out;
}
void run_cases()
{
    checks = cases = 0;
    for( int id : {std::numeric_limits<int>::min(), -1, 1, std::numeric_limits<int>::max()} )
        for( int width : {1, 3} ) {
            scenario s; s.width = width; s.invalid_uniform_terrain = true;
            const auto result = paired( s, id );
            require( result.first.invalid_terrain == std::uint64_t( width * width * 144 ),
                     "negative and out-of-table IDs keep all original outside-cache diagnostics" );
        }
    const auto matrix = scenarios();
    for( const auto &s : matrix ) paired( s );
    std::cout << "Native outside-cache semantic matrix: " << cases << " pairs passed before uniform getter acceptance\n";
    // The unoptimized actual include must pass all complete state/trace oracles
    // above, and all four cases below, before failing this last read acceptance.
    struct counts { std::uint64_t before_ter, after_ter, before_furn, after_furn, expected_furn; };
    std::vector<counts> measured;
    for( bool terrain : {false, true} ) for( bool furniture : {false, true} ) {
        scenario s; s.width = 11; s.terrain_indoors = terrain; s.f_null_indoors = furniture;
        const auto pair = paired( s );
        measured.push_back( {pair.first.terrain_reads, pair.second.terrain_reads,
                             pair.first.furniture_reads, pair.second.furniture_reads, terrain ? 0u : 121u} );
        std::cout << "Uniform outside getters terrain_indoors=" << terrain << " f_null_indoors=" << furniture
                  << ": terrain " << pair.first.terrain_reads << " -> " << pair.second.terrain_reads
                  << ", furniture " << pair.first.furniture_reads << " -> " << pair.second.furniture_reads << '\n';
    }
    for( const auto &row : measured ) {
        require( row.before_ter == 17424 && row.before_furn == row.expected_furn * 144,
                 "primary whole-bubble counts include the exact terrain-first short circuit" );
        require( row.after_ter == 121 && row.after_furn == row.expected_furn,
                 "each eligible uniform submap requires one terrain and only its necessary furniture query (fail-before control)" );
    }
    std::cout << "Native outside cache: " << cases << " pairs / " << checks
              << " checks passed; getter counts do not measure Native elapsed time\n";
}
}

void test_native_outside_cache()
{
    native_outside_cache_fixture::run_cases();
}
