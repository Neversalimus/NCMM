#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// PRIVATE DRAFT: no main and no build integration. Copy into tests/ only after
// the stage is accepted. The actual include is found through first_person_target's
// existing graphics_bridge include directory, as for the floor-cache fixture.
// This is a bounded value/ordered-read oracle, not a Native iterator, lightmap
// timing, visibility-discovery, or gameplay qualification.
// Frozen code: Cataclysm: DDA contributors, CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/lightmap.cpp#L263-L342
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/lightmap.cpp#L263-L342
// Primary body, identical on both pins: 3826 bytes including final LF;
// SHA256 5dec4f06915166dad4638e9656dbd9f672acb20bc82a8b74ebe84a8bea229d5d.
namespace native_vision_transparency_fixture {
using std::memcpy;
constexpr int SEEX = 12, SEEY = 12, MAPSIZE = 11, MAPSIZE_X = 132, MAPSIZE_Y = 132;
constexpr std::size_t tile_count = std::size_t( MAPSIZE_X ) * MAPSIZE_Y;
// Injected scalar sentinels. The complete primary body only compares/stores
// these constants; their actual lightmap.h values are not present in the cached
// references and are not claimed here. Both bodies receive the same values.
constexpr float LIGHT_TRANSPARENCY_SOLID = 0.0f;
constexpr float LIGHT_TRANSPARENCY_OPEN_AIR = 1.0f;
constexpr int effect_quadruped_full = 1;
std::size_t checks = 0, cases = 0;
std::string case_name;
void require( bool ok, const char *message )
{
    ++checks;
    if( !ok ) throw std::runtime_error( "Native vision transparency: " + case_name + ": " + message );
}
std::uint32_t float_bits( float value )
{
    static_assert( sizeof( float ) == sizeof( std::uint32_t ) && std::numeric_limits<float>::is_iec559, "IEEE 32-bit float fixture" );
    std::uint32_t result;
    std::memcpy( &result, &value, sizeof( result ) );
    return result;
}
float from_bits( std::uint32_t value )
{
    float result;
    std::memcpy( &result, &value, sizeof( result ) );
    return result;
}
struct point_sm_ms {
    int a = 0, b = 0;
    point_sm_ms() = default;
    point_sm_ms( int x, int y ) : a( x ), b( y ) {}
    int x() const { return a; } int y() const { return b; }
    static const point_sm_ms zero;
};
const point_sm_ms point_sm_ms::zero{};
struct point_bub_ms {};
struct tripoint_bub_ms {
    int a = 0, b = 0, c = 0;
    tripoint_bub_ms() = default;
    tripoint_bub_ms( int x, int y, int z ) : a( x ), b( y ), c( z ) {}
    int x() const { return a; } int y() const { return b; } int z() const { return c; }
    bool operator==( const tripoint_bub_ms &p ) const { return a == p.a && b == p.b && c == p.c; }
    bool operator!=( const tripoint_bub_ms &p ) const { return !( *this == p ); }
};
struct tripoint_rel_sm { int x, y, z; };
enum class ter_furn_flag : unsigned { TFLAG_TRANSLUCENT };
enum read_kind { cache_lookup, player_lookup, player_position, crouch_query, effect_query,
                 running_query, prone_query, radius_query, coverage_query, bounds_query,
                 submap_lookup, unloaded_debug, terrain_read, terrain_flag, invalid_terrain_debug };
using read_event = std::array<int, 8>;
struct observations {
    std::vector<read_event> required;
    std::uint64_t terrain_reads = 0, object_reads = 0, flag_reads = 0, invalid_terrain = 0;
    void note( read_kind kind, int x = 0, int y = 0, int z = 0, int sx = 0, int sy = 0, int detail = 0 )
    { required.push_back( {int( kind ), x, y, z, sx, sy, detail, 0} ); }
};
struct Character {
    observations *owner = nullptr;
    tripoint_bub_ms position;
    bool crouching = false, prone = false, quadruped = false, running = false;
    tripoint_bub_ms pos_bub() const
    { owner->note( player_position, position.x(), position.y(), position.z() ); return position; }
    bool is_crouching() const { owner->note( crouch_query ); return crouching; }
    bool is_prone() const { owner->note( prone_query ); return prone; }
    bool has_effect( int effect ) const
    { require( effect == effect_quadruped_full, "only the original effect is queried" ); owner->note( effect_query ); return quadruped; }
    bool is_running() const { owner->note( running_query ); return running; }
};
observations *active_observations = nullptr;
Character *active_character = nullptr;
struct observation_guard {
    observations *previous_observations;
    Character *previous_character;
    observation_guard( observations &o, Character &c ) : previous_observations( active_observations ), previous_character( active_character )
    { active_observations = &o; active_character = &c; }
    ~observation_guard() { active_observations = previous_observations; active_character = previous_character; }
};
Character &get_player_character()
{
    require( active_observations && active_character, "player provider is fixture owned" );
    active_observations->note( player_lookup );
    return *active_character;
}
void debugmsg( const char *format, int x, int y, int z )
{
    require( std::strcmp( format, "Tried to build transparency cache at (%d,%d,%d) but the submap is not loaded" ) == 0,
             "the exact Native null-submap diagnostic is retained" );
    require( active_observations != nullptr, "diagnostic provider is fixture owned" );
    active_observations->note( unloaded_debug, x, y, z );
}
struct ter_t {
    bool translucent = false, uniform = true;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0, sx = 0, sy = 0;
    bool has_flag( ter_furn_flag flag ) const
    {
        require( flag == ter_furn_flag::TFLAG_TRANSLUCENT, "only the original terrain flag is queried" );
        ++owner->flag_reads;
        if( !uniform ) owner->note( terrain_flag, gx, gy, z, sx, sy, int( flag ) );
        return translucent;
    }
};
struct ter_id {
    const ter_t *value = nullptr, *fallback = nullptr;
    observations *owner = nullptr;
    bool valid = false;
    int gx = 0, gy = 0, z = 0, sx = 0, sy = 0;
    // Native int_id validity is a quiet bounds/query. Invalid obj() diagnoses
    // on EVERY call; it cannot be skipped or hoisted into a single diagnostic.
    bool is_valid() const noexcept { return valid && value != nullptr; }
    const ter_t &obj() const
    {
        ++owner->object_reads;
        if( is_valid() ) return *value;
        ++owner->invalid_terrain;
        owner->note( invalid_terrain_debug, gx, gy, z, sx, sy );
        return *fallback;
    }
};
struct submap {
    bool uniform_storage = true, uniform_valid = true;
    observations *owner = nullptr;
    int gx = 0, gy = 0, z = 0;
    ter_t uniform_terrain, dummy_terrain;
    std::array<ter_t, SEEX * SEEY> terrain;
    std::array<bool, SEEX * SEEY> valid{};
    bool is_uniform() const noexcept { return uniform_storage; }
    ter_id get_ter( const point_sm_ms &p ) const
    {
        require( p.x() >= 0 && p.x() < SEEX && p.y() >= 0 && p.y() < SEEY, "terrain coordinate stays inside its submap" );
        ++owner->terrain_reads;
        if( !is_uniform() ) owner->note( terrain_read, gx, gy, z, p.x(), p.y() );
        const std::size_t at = std::size_t( p.x() * SEEY + p.y() );
        return {is_uniform() ? &uniform_terrain : &terrain[at], &dummy_terrain, owner,
                is_uniform() ? uniform_valid : valid[at], gx, gy, z, p.x(), p.y()};
    }
};
namespace cata {
template<class T, class Coordinate> using mdarray = std::array<std::array<T, MAPSIZE_Y>, MAPSIZE_X>;
}
using float_grid = cata::mdarray<float, point_bub_ms>;
static_assert( sizeof( float_grid ) == tile_count * sizeof( float ), "the full copied cache is contiguous without extra bytes" );
struct level_cache {
    std::array<std::uint64_t, 4> before{0x9182736455aa33ccULL, 5, 9, 17};
    float_grid transparency_cache;
    std::array<std::uint64_t, 4> between{0x1122334455667788ULL, 4, 7, 19};
    float_grid vision_transparency_cache;
    std::array<std::uint64_t, 4> after{0x13579bdf2468ace0ULL, 21, 29, 33};
    std::bitset<MAPSIZE * MAPSIZE> transparency_cache_dirty;
    std::array<unsigned, 13> untouched_aux{};
};
struct scenario {
    int width = 1, z = 0, seed = 0, dirty_mode = 1, terrain_shape = 0, missing = 0, invalid = 0;
    int posture = 0, player_kind = 0, coverage_mode = 4, float_mode = 0;
    bool uniform_translucent = false, dummy_translucent = false;
};
struct map_state {
    int my_MAPSIZE, selected_z;
    observations observed;
    Character player;
    level_cache cache;
    std::vector<std::unique_ptr<submap>> current;
    std::array<int, tile_count> cover;
    std::array<std::uint64_t, 32> protected_payload{};
    explicit map_state( const scenario &s ) : my_MAPSIZE( s.width ), selected_z( s.z )
    {
        require( my_MAPSIZE >= 1 && my_MAPSIZE <= MAPSIZE, "bounded submap window" );
        player.owner = &observed;
        const int extent = my_MAPSIZE * SEEX;
        player.position = s.player_kind == 1 ? tripoint_bub_ms{0, 0, s.z} :
                          s.player_kind == 2 ? tripoint_bub_ms{extent - 1, extent - 1, s.z} :
                          s.player_kind == 3 ? tripoint_bub_ms{-1, extent / 2, s.z} :
                          s.player_kind == 4 ? tripoint_bub_ms{extent, extent / 2, s.z} :
                          tripoint_bub_ms{extent / 2, extent / 2, s.z + ( s.player_kind == 5 ? 1 : 0 )};
        player.crouching = ( s.posture & 1 ) != 0;
        player.prone = ( s.posture & 2 ) != 0;
        player.quadruped = ( s.posture & 4 ) != 0;
        player.running = ( s.posture & 8 ) != 0;
        for( std::size_t i = 0; i < protected_payload.size(); ++i ) protected_payload[i] = 0x123456789abcdef0ULL ^ ( i * 31 + unsigned( s.seed ) );
        for( std::size_t i = 0; i < cache.untouched_aux.size(); ++i ) cache.untouched_aux[i] = unsigned( i * 13 + s.seed );
        // Quiet NaNs of both signs/payloads, infinities, signed zero, subnormals,
        // the injected constants, and ordinary finite values are copied bitwise.
        constexpr std::array<std::uint32_t, 16> patterns{{
            0x00000000u, 0x80000000u, 0x3f800000u, 0x7fc00001u,
            0x7fc01234u, 0xffc05678u, 0x7f800000u, 0xff800000u,
            0x00000001u, 0x80000001u, 0x3d1d2f1bu, 0xbf800000u,
            0x00800000u, 0x80800000u, 0x7f7fffffu, 0xff7fffffu
        }};
        for( int x = 0; x < MAPSIZE_X; ++x ) for( int y = 0; y < MAPSIZE_Y; ++y ) {
            const std::size_t at = std::size_t( x * MAPSIZE_Y + y );
            const std::uint32_t source = s.float_mode == 1 ? 0x00000000u : s.float_mode == 2 ? 0x80000000u :
                                         s.float_mode == 3 ? 0x3f800000u : patterns[( at + unsigned( s.seed ) ) % patterns.size()];
            cache.transparency_cache[x][y] = from_bits( source );
            cache.vision_transparency_cache[x][y] = from_bits( patterns[( at + unsigned( s.seed ) + 7 ) % patterns.size()] );
            constexpr std::array<int, 5> cover_values{{0, 29, 30, 31, 99}};
            cover[at] = s.coverage_mode < 4 ? cover_values[std::size_t( s.coverage_mode )] :
                        cover_values[( std::size_t( x * 3 + y ) + unsigned( s.seed ) ) % cover_values.size()];
        }
        for( int x = 0; x < MAPSIZE; ++x ) for( int y = 0; y < MAPSIZE; ++y ) {
            const bool dirty = s.dirty_mode == 1 || ( s.dirty_mode == 2 && ( x + y + s.seed ) % 2 == 0 ) ||
                               ( s.dirty_mode == 3 && x == MAPSIZE - 1 && y == MAPSIZE - 1 );
            cache.transparency_cache_dirty[std::size_t( x * MAPSIZE + y )] = dirty;
        }
        current.resize( std::size_t( my_MAPSIZE * my_MAPSIZE ) );
        for( int x = 0; x < my_MAPSIZE; ++x ) for( int y = 0; y < my_MAPSIZE; ++y ) {
            const bool absent = ( s.missing == 1 && x == 0 && y == 0 ) ||
                                ( s.missing == 2 && x == my_MAPSIZE - 1 && y == my_MAPSIZE - 1 ) ||
                                ( s.missing == 3 && x == y ) ||
                                ( s.missing == 4 && !cache.transparency_cache_dirty[std::size_t( x * MAPSIZE + y )] );
            if( absent ) continue;
            auto sm = std::make_unique<submap>();
            sm->owner = &observed; sm->gx = x; sm->gy = y; sm->z = s.z;
            sm->uniform_storage = s.terrain_shape == 0 || ( s.terrain_shape == 5 && ( x + 2 * y + s.seed ) % 3 == 0 );
            sm->uniform_valid = s.invalid != 1;
            sm->uniform_terrain = {s.uniform_translucent, true, &observed, x, y, s.z, 0, 0};
            sm->dummy_terrain = {s.dummy_translucent, sm->uniform_storage, &observed, x, y, s.z, 0, 0};
            for( int sx = 0; sx < SEEX; ++sx ) for( int sy = 0; sy < SEEY; ++sy ) {
                const bool edge = sx == 0 || sy == 0 || sx == SEEX - 1 || sy == SEEY - 1;
                const bool translucent = s.terrain_shape == 1 ? false : s.terrain_shape == 2 ? true :
                                         s.terrain_shape == 3 ? ( sx + sy + s.seed ) % 2 != 0 :
                                         s.terrain_shape == 4 ? edge : ( sx * 3 + sy + s.seed ) % 5 == 0;
                const std::size_t at = std::size_t( sx * SEEY + sy );
                sm->terrain[at] = {translucent, false, &observed, x, y, s.z, sx, sy};
                sm->valid[at] = !( s.invalid == 2 && edge );
            }
            current[std::size_t( x * my_MAPSIZE + y )] = std::move( sm );
        }
    }
    level_cache &get_cache( int z )
    { require( z == selected_z, "only the selected z cache is accessed" ); observed.note( cache_lookup, 0, 0, z ); return cache; }
    const submap *get_submap_at_grid( const tripoint_rel_sm &p )
    {
        require( p.x >= 0 && p.x < my_MAPSIZE && p.y >= 0 && p.y < my_MAPSIZE && p.z == selected_z, "submap lookup remains bounded on selected z" );
        observed.note( submap_lookup, p.x, p.y, p.z );
        return current[std::size_t( p.x * my_MAPSIZE + p.y )].get();
    }
    bool inbounds( const tripoint_bub_ms &p ) const
    {
        // Injected bounded API result, not a copied Native inbounds body.
        player.owner->note( bounds_query, p.x(), p.y(), p.z() );
        return p.x() >= 0 && p.x() < my_MAPSIZE * SEEX && p.y() >= 0 && p.y() < my_MAPSIZE * SEEY && p.z() == selected_z;
    }
    std::vector<tripoint_bub_ms> points_in_radius( const tripoint_bub_ms &p, std::size_t radius, std::size_t radiusz = 0 ) const
    {
        require( radius == 1 && radiusz == 0, "the original bounded square range is requested" );
        player.owner->note( radius_query, p.x(), p.y(), p.z() );
        // Owned clipped square. Public Native map.h promises square/contained
        // points; its tripoint_range implementation/order is NOT frozen here.
        // Both bodies consume precisely this same supplied ordered range.
        std::vector<tripoint_bub_ms> result;
        for( int x = std::max( 0, p.x() - 1 ); x <= std::min( my_MAPSIZE * SEEX - 1, p.x() + 1 ); ++x )
            for( int y = std::max( 0, p.y() - 1 ); y <= std::min( my_MAPSIZE * SEEY - 1, p.y() + 1 ); ++y )
                result.emplace_back( x, y, p.z() );
        return result;
    }
    int coverage( const tripoint_bub_ms &p ) const
    {
        require( p.x() >= 0 && p.x() < MAPSIZE_X && p.y() >= 0 && p.y() < MAPSIZE_Y, "coverage index remains bounded" );
        player.owner->note( coverage_query, p.x(), p.y(), p.z() );
        return cover[std::size_t( p.x() * MAPSIZE_Y + p.y() )];
    }
    std::vector<std::uint64_t> protected_content() const
    {
        std::vector<std::uint64_t> result;
        result.insert( result.end(), cache.before.begin(), cache.before.end() );
        result.insert( result.end(), cache.between.begin(), cache.between.end() );
        result.insert( result.end(), cache.after.begin(), cache.after.end() );
        result.insert( result.end(), cache.untouched_aux.begin(), cache.untouched_aux.end() );
        result.insert( result.end(), protected_payload.begin(), protected_payload.end() );
        for( int value : cover ) result.push_back( std::uint64_t( value ) );
        result.insert( result.end(), {std::uint64_t( player.position.x() ), std::uint64_t( player.position.y() ), std::uint64_t( player.position.z() ),
                       player.crouching, player.prone, player.quadruped, player.running, std::uint64_t( my_MAPSIZE ), std::uint64_t( selected_z )} );
        for( const auto &sm : current ) {
            result.push_back( sm != nullptr );
            if( !sm ) continue;
            result.insert( result.end(), {sm->uniform_storage, sm->uniform_valid, sm->uniform_terrain.translucent, sm->dummy_terrain.translucent} );
            for( std::size_t i = 0; i < sm->terrain.size(); ++i )
                result.push_back( std::uint64_t( sm->terrain[i].translucent ) | ( std::uint64_t( sm->valid[i] ) << 1 ) );
        }
        return result;
    }
};
namespace frozen {
struct map : map_state { using map_state::map_state; bool build_vision_transparency_cache( int zlev ); };
// BEGIN FROZEN NATIVE VISION TRANSPARENCY
bool map::build_vision_transparency_cache( int zlev )
{
    level_cache &map_cache = get_cache( zlev );

    // We copy the transparency_cache so we need to recalc if it's dirty
    if( map_cache.transparency_cache_dirty.none() /*&& map_cache.vision_transparency_cache_dirty.none()*/ ) {
        return false;
    }

    const cata::mdarray<float, point_bub_ms> &transparency_cache = map_cache.transparency_cache;
    cata::mdarray<float, point_bub_ms> &vision_transparency_cache = map_cache.vision_transparency_cache;

    // TODO: Should only copy if transparency_cache was dirty
    memcpy( &vision_transparency_cache, &transparency_cache, sizeof( transparency_cache ) );

    const Character &player_character = get_player_character();
    const tripoint_bub_ms p = player_character.pos_bub();
    const bool is_player_z = p.z() == zlev;

    bool dirty = false;

    if( is_player_z ) {
        // This segment handles vision when the player is crouching or prone. It only checks adjacent tiles.
        // If you change this, also consider creature::sees and map::obstacle_coverage.
        // TODO: Is fairly nonsense because it changes vision for everyone only (eg if you @ crouch behind the window W then the NPC N and monster M can't see each other bc the window is counted as opaque)
        // .N.
        // .@.
        // #W#
        // .M.
        const bool is_crouching = player_character.is_crouching();
        const bool low_profile = player_character.has_effect( effect_quadruped_full ) &&
                                 player_character.is_running();
        const bool is_prone = player_character.is_prone();
        if( is_crouching || is_prone || low_profile ) {
            for( const tripoint_bub_ms &loc : points_in_radius( p, 1 ) ) {
                if( loc != p && coverage( loc ) >= 30 ) {
                    // If we're crouching or prone behind an obstacle, we can't see past it.
                    dirty |= vision_transparency_cache[loc.x()][loc.y()] != LIGHT_TRANSPARENCY_SOLID;
                    vision_transparency_cache[loc.x()][loc.y()] = LIGHT_TRANSPARENCY_SOLID;
                }
            }
        }
    }

    // This segment handles blocking vision through TRANSLUCENT flagged terrain.
    // Traverse the submaps in order (else map::ter() calls get_submap each time)
    for( int smx = 0; smx < my_MAPSIZE; ++smx ) {
        for( int smy = 0; smy < my_MAPSIZE; ++smy ) {
            const submap *cur_submap = get_submap_at_grid( tripoint_rel_sm{smx, smy, zlev} );
            if( cur_submap == nullptr ) {
                debugmsg( "Tried to build transparency cache at (%d,%d,%d) but the submap is not loaded", smx, smy,
                          zlev );
                continue;
            }
            if( !map_cache.transparency_cache_dirty[smx * MAPSIZE + smy] ) {
                continue;
            }
            for( int smi = 0; smi < SEEX; smi++ ) {
                for( int smj = 0; smj < SEEY; smj++ ) {
                    if( cur_submap->get_ter( point_sm_ms{smi, smj} ).obj().has_flag(
                            ter_furn_flag::TFLAG_TRANSLUCENT ) ) {
                        const int i = smi + ( smx * SEEX );
                        const int j = smj + ( smy * SEEY );
                        dirty |= vision_transparency_cache[i][j] != LIGHT_TRANSPARENCY_SOLID;
                        vision_transparency_cache[i][j] = LIGHT_TRANSPARENCY_SOLID;
                    }
                }
            }
        }
    }

    // The tile player is standing on should always be visible
    // Shouldn't this be handled in the player's seen cache instead??
    if( is_player_z && inbounds( p ) ) {
        vision_transparency_cache[p.x()][p.y()] = LIGHT_TRANSPARENCY_OPEN_AIR;
    }

    map_cache.transparency_cache_dirty.reset();
    return dirty;
}
// END FROZEN NATIVE VISION TRANSPARENCY
} // namespace frozen
namespace actual {
struct map : map_state { using map_state::map_state; bool build_vision_transparency_cache( int zlev ); };
#include "ncmm_graphics_vision_transparency.inc"
} // namespace actual

std::vector<std::uint32_t> grid_bits( const float_grid &grid )
{
    // Copy the object representation directly: inspecting a float by value must
    // not itself canonicalize a NaN or lose its payload before comparison.
    std::vector<std::uint32_t> result( tile_count );
    std::memcpy( result.data(), &grid, sizeof( grid ) );
    return result;
}
struct outcome {
    bool returned = false;
    std::bitset<MAPSIZE * MAPSIZE> dirty_before, dirty_after;
    std::vector<std::uint32_t> source_bits, vision_bits;
    std::vector<std::uint64_t> protected_bytes;
    std::vector<read_event> required;
    std::uint64_t terrain_reads = 0, object_reads = 0, flag_reads = 0, invalid_terrain = 0;
};
template<class Map> outcome execute( const scenario &s )
{
    auto map = std::make_unique<Map>( s );
    const auto source_before = grid_bits( map->cache.transparency_cache );
    const auto protected_before = map->protected_content();
    outcome result;
    result.dirty_before = map->cache.transparency_cache_dirty;
    observation_guard observe( map->observed, map->player );
    result.returned = map->build_vision_transparency_cache( s.z );
    result.dirty_after = map->cache.transparency_cache_dirty;
    result.source_bits = grid_bits( map->cache.transparency_cache );
    result.vision_bits = grid_bits( map->cache.vision_transparency_cache );
    result.protected_bytes = map->protected_content();
    require( source_before == result.source_bits, "the full source float cache is bitwise unchanged" );
    require( protected_before == result.protected_bytes, "all bounds guards, coverage, physical IDs/flags, player and unrelated memory remain exact" );
    result.required = std::move( map->observed.required );
    result.terrain_reads = map->observed.terrain_reads;
    result.object_reads = map->observed.object_reads;
    result.flag_reads = map->observed.flag_reads;
    result.invalid_terrain = map->observed.invalid_terrain;
    return result;
}
std::pair<outcome, outcome> paired( const scenario &s )
{
    case_name = "case " + std::to_string( cases ) + " width=" + std::to_string( s.width ) + " z=" + std::to_string( s.z ) +
                " shape=" + std::to_string( s.terrain_shape ) + " dirty=" + std::to_string( s.dirty_mode );
    auto original = execute<frozen::map>( s );
    auto observed = execute<actual::map>( s );
    require( original.returned == observed.returned, "dirty return matches the independent primary oracle" );
    require( original.dirty_before == observed.dirty_before && original.dirty_after == observed.dirty_after, "all 121 dirty bits and reset remain exact" );
    require( original.protected_bytes == observed.protected_bytes, "the two owned scenarios have identical protected content" );
    require( original.required == observed.required, "cache/player/range/coverage/submap/null and ordered nonuniform getter/flag reads remain exact" );
    require( original.invalid_terrain == observed.invalid_terrain, "every invalid-ID obj diagnostic is preserved" );
    require( original.source_bits.size() == tile_count && observed.source_bits.size() == tile_count &&
             original.vision_bits.size() == tile_count && observed.vision_bits.size() == tile_count, "both caches retain their full Native array dimensions" );
    for( std::size_t i = 0; i < tile_count; ++i ) {
        require( original.source_bits[i] == observed.source_bits[i], "all source float BITS match, including NaN payloads and signed zero" );
        require( original.vision_bits[i] == observed.vision_bits[i], "all destination float BITS match, including NaN payloads and signed zero" );
    }
    if( original.dirty_before.none() ) {
        require( !original.returned && original.terrain_reads == 0 && observed.terrain_reads == 0 &&
                 original.required.size() == 1 && original.required.front()[0] == int( cache_lookup ),
                 "no-dirty early return performs only the original cache lookup" );
    } else {
        require( original.dirty_after.none(), "the original function resets the full dirty bitset after processing" );
        const int extent = s.width * SEEX;
        const int px = s.player_kind == 1 ? 0 : s.player_kind == 2 ? extent - 1 : s.player_kind == 3 ? -1 : s.player_kind == 4 ? extent : extent / 2;
        const int py = s.player_kind == 1 ? 0 : s.player_kind == 2 ? extent - 1 : extent / 2;
        if( s.player_kind != 5 && px >= 0 && px < extent && py >= 0 && py < extent )
            require( original.vision_bits[std::size_t( px * MAPSIZE_Y + py )] == float_bits( LIGHT_TRANSPARENCY_OPEN_AIR ),
                     "player tile reopening remains last, including translucent terrain and non-dirty submaps" );
    }
    ++cases;
    return {std::move( original ), std::move( observed )};
}
void run_cases()
{
    checks = cases = 0;
    for( int width : {1, 3, 11} ) for( int z : {-10, 0, 10} )
        for( int dirty : {0, 1, 2, 3} ) for( bool translucent : {false, true} ) {
            scenario s; s.width = width; s.z = z; s.dirty_mode = dirty; s.uniform_translucent = translucent;
            s.seed = z + 11; paired( s );
        }
    for( int posture = 0; posture < 16; ++posture ) for( int player = 0; player < 6; ++player ) for( int cover_mode : {0, 4} ) {
        scenario s; s.width = 1 + 2 * ( posture % 2 ); s.z = ( posture % 3 ) - 1; s.seed = posture;
        s.posture = posture; s.player_kind = player; s.coverage_mode = cover_mode;
        s.terrain_shape = posture % 6; s.uniform_translucent = ( posture & 2 ) != 0; s.dirty_mode = player % 2 + 1;
        paired( s );
    }
    for( int posture : {1, 2, 4, 12} ) for( int cover_mode : {1, 2, 3} ) {
        scenario s; s.posture = posture; s.coverage_mode = cover_mode; s.float_mode = 3; paired( s );
    }
    for( int width : {1, 3, 11} ) for( int shape = 1; shape <= 5; ++shape ) for( int dirty : {1, 2} ) for( int seed : {0, 7} ) {
        scenario s; s.width = width; s.terrain_shape = shape; s.dirty_mode = dirty; s.seed = seed;
        s.uniform_translucent = ( seed & 1 ) != 0; s.posture = seed; paired( s );
    }
    for( int width : {1, 3, 11} ) for( int z : {-10, 10} ) for( int missing = 1; missing <= 4; ++missing ) for( int dirty : {0, 1, 2} ) {
        scenario s; s.width = width; s.z = z; s.missing = missing; s.dirty_mode = dirty; s.seed = 1;
        s.uniform_translucent = true; s.terrain_shape = missing; paired( s );
    }
    // In particular, a missing CLEAN submap still diagnoses before the dirty
    // guard, provided some other dirty bit makes the function enter its body.
    {
        scenario s; s.width = 3; s.dirty_mode = 2; s.seed = 1; s.missing = 4;
        const auto result = paired( s );
        std::size_t nulls = 0;
        for( const auto &e : result.first.required ) if( e[0] == int( unloaded_debug ) ) ++nulls;
        require( nulls == 5, "five missing clean submaps diagnose before their clean-bit guard" );
    }
    for( int width : {1, 3} ) for( int invalid : {1, 2} ) for( bool dummy : {false, true} )
        for( bool translucent : {false, true} ) for( int dirty : {1, 2} ) {
            scenario s; s.width = width; s.invalid = invalid; s.dummy_translucent = dummy; s.uniform_translucent = translucent;
            s.terrain_shape = invalid == 1 ? 0 : 4; s.dirty_mode = dirty;
            const auto result = paired( s );
            if( invalid == 1 && width == 1 ) require( result.first.invalid_terrain == 144, "invalid uniform terrain still diagnoses on every original obj()" );
        }
    // Independent literal expectations pin down the comparison/dirty subtleties.
    for( int mode : {1, 2} ) {
        scenario s; s.float_mode = mode; s.uniform_translucent = true;
        const auto result = paired( s );
        require( !result.first.returned, "+0 and -0 equal SOLID: rewriting to +0 does not set dirty; player reopening alone also does not set dirty" );
        require( result.first.vision_bits.front() == float_bits( LIGHT_TRANSPARENCY_SOLID ), "translucent source cells store the exact SOLID bits" );
    }
    {
        scenario s; s.float_mode = 3; s.uniform_translucent = true;
        const auto result = paired( s );
        require( result.first.returned, "a non-solid translucent source cell sets dirty" );
    }
    {
        scenario s; s.float_mode = 0; s.uniform_translucent = false; s.player_kind = 5;
        const auto result = paired( s );
        require( !result.first.returned && result.first.vision_bits == result.first.source_bits,
                 "non-player z with ordinary terrain retains memcpy's exact NaN/inf/signed-zero bits" );
    }
    {
        scenario s; s.float_mode = 3; s.uniform_translucent = false; s.posture = 1; s.coverage_mode = 2;
        const auto result = paired( s );
        require( result.first.returned, "crouching at coverage 30 sets dirty independently of ordinary terrain" );
    }
    // Final fail-before control: every semantic and ordered-read case above must
    // pass against the untouched actual Native body before this assertion fails.
    scenario dense; dense.width = 11; dense.dirty_mode = 1; dense.uniform_translucent = false;
    const auto result = paired( dense );
    std::cout << "Native vision transparency semantic cases: " << cases << "; checks before reduction gate: " << checks << '\n';
    std::cout << "Native uniform non-translucent reads: terrain " << result.first.terrain_reads << " -> " << result.second.terrain_reads
              << "; obj " << result.first.object_reads << " -> " << result.second.object_reads
              << "; flag " << result.first.flag_reads << " -> " << result.second.flag_reads << '\n';
    require( result.first.terrain_reads == 17424 && result.first.object_reads == 17424 && result.first.flag_reads == 17424,
             "the independent baseline performs 144 terrain/obj/flag reads per dirty submap" );
    require( result.second.terrain_reads == 121 && result.second.object_reads == 121 && result.second.flag_reads == 121,
             "fully dirty uniform non-translucent 11x11 requires exactly one terrain/obj/flag read per submap (fail-before control)" );
    std::cout << "Native vision transparency: " << cases << " cases / " << checks << " checks passed\n";
}
} // namespace native_vision_transparency_fixture

void test_native_vision_transparency()
{
    native_vision_transparency_fixture::run_cases();
}
