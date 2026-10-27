#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Owned component oracle, not a replacement implementation of Native items or
// plf::colony. Stable std::list nodes supply only the iteration/container API.
// Getter cardinalities below are not Native timings or an FPS prediction.
// Frozen primary functions: Cataclysm: DDA contributors, CC-BY-SA-3.0.
// https://creativecommons.org/licenses/by-sa/3.0/
// https://github.com/CleverRaven/Cataclysm-DDA/blob/3f7fb352bf492ba521bd9408a0c9f6ce239e8d83/src/map.cpp#L8879-L8989
// https://github.com/CleverRaven/Cataclysm-DDA/blob/074aa98bd5be3de4c35f154082db32a0e63bb0f1/src/map.cpp#L8890-L9000
namespace native_item_reconciliation_fixture {
using std::int64_t;
constexpr int SEEX = 12, SEEY = 12, OVERMAP_DEPTH = 10, OVERMAP_HEIGHT = 10;
namespace calendar { constexpr int before_time_starts = -1000, turn = 9000; }
std::size_t checks = 0, pairs = 0;
void require( bool ok, const char *reason )
{
    ++checks;
    if( !ok ) throw std::runtime_error( std::string( "Native item reconciliation: " ) + reason );
}
struct point_sm_ms {
    int a = 0, b = 0;
    point_sm_ms() = default; point_sm_ms( int x, int y ) : a( x ), b( y ) {}
    int x() const { return a; } int y() const { return b; }
    static const point_sm_ms zero;
};
const point_sm_ms point_sm_ms::zero{};
struct tripoint_rel_sm { int x, y, z; tripoint_rel_sm( int a, int b, int c ) : x( a ), y( b ), z( c ) {} };
struct tripoint_bub_ms { int x, y, z; tripoint_bub_ms( int a, int b, int c ) : x( a ), y( b ), z( c ) {} };
struct tripoint_abs_ms { int x, y, z; };
struct tripoint_abs_sm { int level = 0; int z() const { return level; } };
enum class reconcile_scope { full_rebuild, additive };
using key = std::array<int, 3>;
using event = std::array<int64_t, 10>;
enum operation {
    get_wakeups, get_reservations, clear_reservations, submap_lookup, mutable_nonuniform_get,
    const_get, absolute_coordinate, absolute_cursor, explicit_cursor, vehicle_cursor_call,
    item_location_call, child_location_call, location_bool, location_get, item_is_craft,
    passive_time, token_peek, environment_time, set_environment, children_query,
    craft_rebuild, wakeup_rebuild, reservation_find, reservation_empty, reservation_erase,
    vehicles_query, vehicle_parts_query, part_index_query, vehicle_part_query,
    vehicle_items_query, avatar_query, npcs_query, character_items_query, callback_mutation
};
struct world;
struct map_state;
struct vehicle;
struct item;
struct map_cursor {
    world *owner; int kind; key position;
    explicit map_cursor( tripoint_abs_ms );
    map_cursor( map_state *, tripoint_bub_ms );
};
struct vehicle_cursor {
    world *owner; int vehicle_id, part;
    vehicle_cursor( vehicle &, int );
};
struct item_location {
    world *owner = nullptr; item *ptr = nullptr;
    int kind = 0, holder = 0, slot = 0, parent = 0;
    key position{}; bool truth = false, null_result = false;
    item_location() = default;
    item_location( map_cursor, item * );
    item_location( vehicle_cursor, item * );
    item_location( const item_location &, item * );
    explicit operator bool() const;
    item *get_item() const;
};
struct item {
    world *owner = nullptr; int id = 0; bool craft = false;
    int passive = calendar::before_time_starts, env = calendar::before_time_starts;
    int64_t token = 0;
    bool empty_claim = false, release_token = false, mutated = false, expired_location = false;
    int erase_on_wakeup = 0, mutation = 0;
    std::vector<item *> children;
    bool is_craft() const; int get_passive_started_at() const;
    int64_t peek_reservation_owner_token() const; int get_env_check_at() const;
    void set_env_check_at( int ); std::vector<item *> all_items_top() const;
};
struct submap {
    world *owner; key grid; bool uniform = true;
    std::array<std::list<item>, SEEX * SEEY> tiles;
    submap( world *w, key p ) : owner( w ), grid( p ) {}
    bool is_uniform() const noexcept { return uniform; }
    std::list<item> &get_items( point_sm_ms );
    const std::list<item> &get_items( point_sm_ms ) const;
};
struct vehicle_part { int index = 0, mount_x = 0, mount_y = 0; std::list<item> items; };
struct vpart_reference {
    world *owner; int vehicle_id, index;
    int part_index() const;
};
struct vehicle {
    world *owner; int id;
    std::vector<vehicle_part> parts;
    std::vector<vpart_reference> get_all_parts();
    vehicle_part &part( int ); std::list<item> &get_items( vehicle_part & );
};
struct wrapped_vehicle { vehicle *v = nullptr; };
struct Character {
    world *owner = nullptr; int id = 0;
    std::list<item> storage;
    std::vector<item_location> locations;
    std::vector<item_location> all_items_loc();
};
struct npc : Character {};
struct item_wakeup_manager {
    world *owner = nullptr;
    std::map<int, event> entries;
    void rebuild_for_item( item_location );
};
struct craft_reservation_index {
    struct record {
        world *owner = nullptr; int64_t token = 0; std::vector<int> claims;
        bool empty() const;
    };
    world *owner = nullptr;
    std::map<int64_t, record> records;
    void clear(); void rebuild_for_craft( item_location );
    const record *find( int64_t ); void erase( int64_t );
};
struct game_view { world *owner = nullptr; std::vector<npc> &all_npcs(); };
struct scenario {
    int width = 1, level = 3, sentinel = 0, mutation = 0;
    bool zlevels = false, nonuniform_empty = false, mixed = false, missing = false, all_missing = false;
    bool core_items = false, foreign_items = false;
    key origin{0, 0, 0};
};
struct world {
    scenario config;
    std::vector<event> trace;
    std::uint64_t mutable_getters = 0, const_getters = 0;
    std::map<key, std::unique_ptr<submap>> submaps;
    // Separate objects model the TWO Native function-static overload sentinels.
    std::list<item> mutable_empty, const_empty, nested_storage;
    std::map<int, item *> identities;
    std::vector<std::unique_ptr<vehicle>> owned_vehicles;
    std::vector<wrapped_vehicle> vehicle_views;
    Character avatar;
    std::vector<npc> npcs;
    item_wakeup_manager wakeups;
    craft_reservation_index reservations;
    game_view game;
    key source{}, future{};
    explicit world( const scenario & );
    void note( operation op, int64_t a = 0, int64_t b = 0, int64_t c = 0, int64_t d = 0,
               int64_t e = 0, int64_t f = 0, int64_t h = 0, int64_t i = 0, int64_t j = 0 )
    { trace.push_back( {int64_t( op ), a, b, c, d, e, f, h, i, j} ); }
    void location_note( operation, const item_location & );
    submap &ensure_submap( key );
    item &add( std::list<item> &, int, bool = false );
    item_location inventory_location( item *, int, bool = true, bool = false );
    void mutate( item & );
    std::vector<int64_t> snapshot() const;
};
world *current = nullptr;
game_view *g = nullptr;
struct active_world {
    world *previous; game_view *previous_game;
    explicit active_world( world &w ) : previous( current ), previous_game( g ) { current = &w; g = &w.game; }
    ~active_world() { current = previous; g = previous_game; }
};
struct map_state {
    world &state; int my_MAPSIZE; bool zlevels; tripoint_abs_sm abs_sub;
    explicit map_state( world &w ) : state( w ), my_MAPSIZE( w.config.width ), zlevels( w.config.zlevels ), abs_sub{w.config.level} {}
    submap *get_submap_at_grid( tripoint_rel_sm p )
    {
        state.note( submap_lookup, p.x, p.y, p.z );
        const key position{p.x, p.y, zlevels ? p.z : abs_sub.z()};
        const auto found = state.submaps.find( position );
        return found == state.submaps.end() ? nullptr : found->second.get();
    }
    tripoint_abs_ms get_abs( tripoint_bub_ms p )
    {
        state.note( absolute_coordinate, p.x, p.y, p.z );
        return {p.x + state.config.origin[0], p.y + state.config.origin[1], p.z};
    }
    std::vector<wrapped_vehicle> &get_vehicles() { state.note( vehicles_query ); return state.vehicle_views; }
};
item_wakeup_manager &get_item_wakeups() { current->note( get_wakeups ); return current->wakeups; }
craft_reservation_index &get_craft_reservations() { current->note( get_reservations ); return current->reservations; }
Character &get_avatar() { current->note( avatar_query ); return current->avatar; }

namespace frozen1040 {
struct map : map_state { using map_state::map_state; void reconcile_loaded_items( reconcile_scope ); };
// BEGIN FROZEN NATIVE RECONCILE 1040
void map::reconcile_loaded_items( const reconcile_scope scope )
{
    item_wakeup_manager &wakeups = get_item_wakeups();
    craft_reservation_index &index = get_craft_reservations();
    if( scope == reconcile_scope::full_rebuild ) {
        index.clear();
    }

    std::vector<int64_t> seen;
    auto reconcile_one = [&wakeups, &index, &seen]( item_location loc ) {
        item *it = loc.get_item();
        if( it == nullptr ) {
            return;
        }
        if( it->is_craft() ) {
            // On the token as well as the live step: the stale branch keeps the token and
            // clears passive_started_at, and that record still needs cleaning.
            const bool live = it->get_passive_started_at() != calendar::before_time_starts;
            const int64_t token = it->peek_reservation_owner_token();
            if( live || token != 0 ) {
                // A pre-feature save was stamped before reservations existed, so nothing
                // would ever call acquisition and the step would run unreserved to
                // completion.  The wakeup rebuild below picks up the cursor set here.
                if( live && token == 0 &&
                    it->get_env_check_at() == calendar::before_time_starts ) {
                    it->set_env_check_at( calendar::turn );
                }
                index.rebuild_for_craft( loc );
                if( it->peek_reservation_owner_token() != 0 ) {
                    seen.push_back( it->peek_reservation_owner_token() );
                }
            }
        }
        wakeups.rebuild_for_item( loc );
    };

    auto walk_recursive = [&reconcile_one]( auto & self, item_location loc ) -> void {
        if( !loc || loc.get_item() == nullptr )
        {
            return;
        }
        reconcile_one( loc );
        for( item *child : loc.get_item()->all_items_top() )
        {
            self( self, item_location( loc, child ) );
        }
    };

    // Submaps rather than tiles, and the z range guarded by zlevels: get_nonant drops the z
    // index on a map that does not support z levels, so every level would alias the one the
    // map holds and the walk would repeat itself once per level.
    for( int gridx = 0; gridx < my_MAPSIZE; gridx++ ) {
        for( int gridy = 0; gridy < my_MAPSIZE; gridy++ ) {
            const int zmin = zlevels ? -OVERMAP_DEPTH : abs_sub.z();
            const int zmax = zlevels ? OVERMAP_HEIGHT : abs_sub.z();
            for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                const tripoint_rel_sm grid( gridx, gridy, gridz );
                submap *const sm = get_submap_at_grid( grid );
                if( sm == nullptr ) {
                    continue;
                }
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        for( item &it : sm->get_items( { sx, sy } ) ) {
                            const tripoint_bub_ms p( sx + gridx * SEEX,
                                                     sy + gridy * SEEY, gridz );
                            item_location loc( map_cursor( get_abs( p ) ), &it );
                            walk_recursive( walk_recursive, loc );
                        }
                    }
                }
            }
        }
    }

    // Every part that holds items, not only the cargo part a tile resolves to, or a second
    // cargo part at the same mount is never walked.
    for( wrapped_vehicle &wv : get_vehicles() ) {
        if( wv.v == nullptr ) {
            continue;
        }
        for( const vpart_reference &vpr : wv.v->get_all_parts() ) {
            vehicle_part &vp = wv.v->part( vpr.part_index() );
            for( item &it : wv.v->get_items( vp ) ) {
                item_location loc( vehicle_cursor( *wv.v, vpr.part_index() ), &it );
                walk_recursive( walk_recursive, loc );
            }
        }
    }

    auto walk_character = [&reconcile_one]( Character & c ) {
        // all_items_loc() is already recursive; reconcile per location directly.
        for( item_location &loc : c.all_items_loc() ) {
            if( loc && loc.get_item() != nullptr ) {
                reconcile_one( loc );
            }
        }
    };
    walk_character( get_avatar() );
    for( npc &n : g->all_npcs() ) {
        walk_character( n );
    }

    // Crafts this pass did not walk are left alone: the index retains earlier entries.
    for( const int64_t token : seen ) {
        const craft_reservation_index::record *rec = index.find( token );
        if( rec != nullptr && rec->empty() ) {
            index.erase( token );
        }
    }
}
// END FROZEN NATIVE RECONCILE 1040
} // namespace frozen1040
namespace frozen1807 {
struct map : map_state { using map_state::map_state; void reconcile_loaded_items( reconcile_scope ); };
// BEGIN FROZEN NATIVE RECONCILE 1807
void map::reconcile_loaded_items( const reconcile_scope scope )
{
    item_wakeup_manager &wakeups = get_item_wakeups();
    craft_reservation_index &index = get_craft_reservations();
    if( scope == reconcile_scope::full_rebuild ) {
        index.clear();
    }

    std::vector<int64_t> seen;
    auto reconcile_one = [&wakeups, &index, &seen]( item_location loc ) {
        item *it = loc.get_item();
        if( it == nullptr ) {
            return;
        }
        if( it->is_craft() ) {
            // On the token as well as the live step: the stale branch keeps the token and
            // clears passive_started_at, and that record still needs cleaning.
            const bool live = it->get_passive_started_at() != calendar::before_time_starts;
            const int64_t token = it->peek_reservation_owner_token();
            if( live || token != 0 ) {
                // A pre-feature save was stamped before reservations existed, so nothing
                // would ever call acquisition and the step would run unreserved to
                // completion.  The wakeup rebuild below picks up the cursor set here.
                if( live && token == 0 &&
                    it->get_env_check_at() == calendar::before_time_starts ) {
                    it->set_env_check_at( calendar::turn );
                }
                index.rebuild_for_craft( loc );
                if( it->peek_reservation_owner_token() != 0 ) {
                    seen.push_back( it->peek_reservation_owner_token() );
                }
            }
        }
        wakeups.rebuild_for_item( loc );
    };

    auto walk_recursive = [&reconcile_one]( auto & self, item_location loc ) -> void {
        if( !loc || loc.get_item() == nullptr )
        {
            return;
        }
        reconcile_one( loc );
        for( item *child : loc.get_item()->all_items_top() )
        {
            self( self, item_location( loc, child ) );
        }
    };

    // Submaps rather than tiles, and the z range guarded by zlevels: get_nonant drops the z
    // index on a map that does not support z levels, so every level would alias the one the
    // map holds and the walk would repeat itself once per level.
    for( int gridx = 0; gridx < my_MAPSIZE; gridx++ ) {
        for( int gridy = 0; gridy < my_MAPSIZE; gridy++ ) {
            const int zmin = zlevels ? -OVERMAP_DEPTH : abs_sub.z();
            const int zmax = zlevels ? OVERMAP_HEIGHT : abs_sub.z();
            for( int gridz = zmin; gridz <= zmax; gridz++ ) {
                const tripoint_rel_sm grid( gridx, gridy, gridz );
                submap *const sm = get_submap_at_grid( grid );
                if( sm == nullptr ) {
                    continue;
                }
                for( int sx = 0; sx < SEEX; ++sx ) {
                    for( int sy = 0; sy < SEEY; ++sy ) {
                        for( item &it : sm->get_items( { sx, sy } ) ) {
                            const tripoint_bub_ms p( sx + gridx * SEEX,
                                                     sy + gridy * SEEY, gridz );
                            item_location loc( map_cursor( this, p ), &it );
                            walk_recursive( walk_recursive, loc );
                        }
                    }
                }
            }
        }
    }

    // Every part that holds items, not only the cargo part a tile resolves to, or a second
    // cargo part at the same mount is never walked.
    for( wrapped_vehicle &wv : get_vehicles() ) {
        if( wv.v == nullptr ) {
            continue;
        }
        for( const vpart_reference &vpr : wv.v->get_all_parts() ) {
            vehicle_part &vp = wv.v->part( vpr.part_index() );
            for( item &it : wv.v->get_items( vp ) ) {
                item_location loc( vehicle_cursor( *wv.v, vpr.part_index() ), &it );
                walk_recursive( walk_recursive, loc );
            }
        }
    }

    auto walk_character = [&reconcile_one]( Character & c ) {
        // all_items_loc() is already recursive; reconcile per location directly.
        for( item_location &loc : c.all_items_loc() ) {
            if( loc && loc.get_item() != nullptr ) {
                reconcile_one( loc );
            }
        }
    };
    walk_character( get_avatar() );
    for( npc &n : g->all_npcs() ) {
        walk_character( n );
    }

    // Crafts this pass did not walk are left alone: the index retains earlier entries.
    for( const int64_t token : seen ) {
        const craft_reservation_index::record *rec = index.find( token );
        if( rec != nullptr && rec->empty() ) {
            index.erase( token );
        }
    }
}
// END FROZEN NATIVE RECONCILE 1807
} // namespace frozen1807
namespace actual1040 {
struct map : map_state { using map_state::map_state; void reconcile_loaded_items( reconcile_scope ); };
#define NCMM_GRAPHICS_RECONCILE_MAP_CURSOR(p) map_cursor( get_abs( p ) )
#include "ncmm_graphics_reconcile_items.inc"
#undef NCMM_GRAPHICS_RECONCILE_MAP_CURSOR
} // namespace actual1040
namespace actual1807 {
struct map : map_state { using map_state::map_state; void reconcile_loaded_items( reconcile_scope ); };
#define NCMM_GRAPHICS_RECONCILE_MAP_CURSOR(p) map_cursor( this, p )
#include "ncmm_graphics_reconcile_items.inc"
#undef NCMM_GRAPHICS_RECONCILE_MAP_CURSOR
} // namespace actual1807

void world::location_note( operation op, const item_location &loc )
{
    note( op, loc.ptr ? loc.ptr->id : 0, loc.kind, loc.holder, loc.slot,
          loc.position[0], loc.position[1], loc.position[2], loc.parent, loc.truth && !loc.null_result );
}
map_cursor::map_cursor( tripoint_abs_ms p ) : owner( current ), kind( 1 ), position{p.x, p.y, p.z}
{ owner->note( absolute_cursor, p.x, p.y, p.z ); }
map_cursor::map_cursor( map_state *source, tripoint_bub_ms p ) : owner( &source->state ), kind( 2 ),
    position{p.x + source->state.config.origin[0], p.y + source->state.config.origin[1], p.z}
{
    require( owner == current, "explicit Native cursor retains the correct map" );
    owner->note( explicit_cursor, p.x, p.y, p.z );
}
vehicle_cursor::vehicle_cursor( vehicle &source, int index ) : owner( source.owner ), vehicle_id( source.id ), part( index )
{ owner->note( vehicle_cursor_call, vehicle_id, part ); }
item_location::item_location( map_cursor cursor, item *value ) : owner( cursor.owner ), ptr( value ), kind( cursor.kind ),
    position( cursor.position ), truth( value != nullptr )
{ owner->location_note( item_location_call, *this ); }
item_location::item_location( vehicle_cursor cursor, item *value ) : owner( cursor.owner ), ptr( value ), kind( 3 ),
    holder( cursor.vehicle_id ), slot( cursor.part ), truth( value != nullptr )
{ owner->location_note( item_location_call, *this ); }
item_location::item_location( const item_location &source, item *value ) : owner( source.owner ), ptr( value ), kind( 5 ),
    holder( source.holder ), slot( source.slot ), parent( source.ptr ? source.ptr->id : 0 ), position( source.position ), truth( value != nullptr ),
    null_result( value && value->expired_location )
{ owner->location_note( child_location_call, *this ); }
item_location::operator bool() const
{ if( owner ) owner->location_note( location_bool, *this ); return truth; }
item *item_location::get_item() const
{ if( owner ) owner->location_note( location_get, *this ); return truth && !null_result ? ptr : nullptr; }
bool item::is_craft() const { owner->note( item_is_craft, id, craft ); return craft; }
int item::get_passive_started_at() const { owner->note( passive_time, id, passive ); return passive; }
int64_t item::peek_reservation_owner_token() const { owner->note( token_peek, id, token ); return token; }
int item::get_env_check_at() const { owner->note( environment_time, id, env ); return env; }
void item::set_env_check_at( int value ) { owner->note( set_environment, id, value ); env = value; }
std::vector<item *> item::all_items_top() const { owner->note( children_query, id ); return children; }
std::list<item> &submap::get_items( point_sm_ms p )
{
    require( p.x() >= 0 && p.x() < SEEX && p.y() >= 0 && p.y() < SEEY, "mutable tile coordinate stays in its submap" );
    ++owner->mutable_getters;
    if( uniform ) return owner->mutable_empty;
    owner->note( mutable_nonuniform_get, grid[0], grid[1], grid[2], p.x(), p.y() );
    return tiles[std::size_t( p.x() * SEEY + p.y() )];
}
const std::list<item> &submap::get_items( point_sm_ms p ) const
{
    ++owner->const_getters;
    owner->note( const_get, grid[0], grid[1], grid[2], p.x(), p.y() );
    if( uniform ) return owner->const_empty;
    return tiles[std::size_t( p.x() * SEEY + p.y() )];
}
int vpart_reference::part_index() const { owner->note( part_index_query, vehicle_id, index ); return index; }
std::vector<vpart_reference> vehicle::get_all_parts()
{
    owner->note( vehicle_parts_query, id ); std::vector<vpart_reference> out;
    for( const auto &p : parts ) out.push_back( {owner, id, p.index} );
    return out;
}
vehicle_part &vehicle::part( int index )
{ owner->note( vehicle_part_query, id, index ); return parts.at( std::size_t( index ) ); }
std::list<item> &vehicle::get_items( vehicle_part &p )
{ owner->note( vehicle_items_query, id, p.index, p.mount_x, p.mount_y ); return p.items; }
std::vector<item_location> Character::all_items_loc()
{ owner->note( character_items_query, id ); return locations; }
std::vector<npc> &game_view::all_npcs() { owner->note( npcs_query ); return owner->npcs; }
void craft_reservation_index::clear() { owner->note( clear_reservations ); records.clear(); }
void craft_reservation_index::rebuild_for_craft( item_location loc )
{
    owner->location_note( craft_rebuild, loc ); item &it = *loc.ptr;
    if( it.release_token ) { records.erase( it.token ); it.token = 0; return; }
    if( !it.token ) it.token = 1000 + it.id;
    record value; value.owner = owner; value.token = it.token;
    if( !it.empty_claim ) value.claims.push_back( it.id );
    records[it.token] = std::move( value );
}
const craft_reservation_index::record *craft_reservation_index::find( int64_t token )
{
    owner->note( reservation_find, token ); const auto found = records.find( token );
    return found == records.end() ? nullptr : &found->second;
}
bool craft_reservation_index::record::empty() const
{ owner->note( reservation_empty, token, claims.size() ); return claims.empty(); }
void craft_reservation_index::erase( int64_t token ) { owner->note( reservation_erase, token ); records.erase( token ); }
void item_wakeup_manager::rebuild_for_item( item_location loc )
{
    owner->location_note( wakeup_rebuild, loc ); item &it = *loc.ptr;
    entries[it.id] = {it.id, loc.kind, loc.holder, loc.slot, loc.position[0], loc.position[1], loc.position[2], it.token, it.env, it.passive};
    if( it.erase_on_wakeup ) owner->reservations.records.erase( it.erase_on_wakeup );
    owner->mutate( it );
}
submap &world::ensure_submap( key position )
{
    auto &value = submaps[position];
    if( !value ) value = std::make_unique<submap>( this, position );
    return *value;
}
item &world::add( std::list<item> &container, int id, bool craft )
{
    require( identities.count( id ) == 0, "fixture item identities are owned and unique" );
    container.emplace_back(); auto &value = container.back();
    value.owner = this; value.id = id; value.craft = craft; identities[id] = &value;
    return value;
}
item_location world::inventory_location( item *value, int holder, bool truth, bool null_result )
{
    item_location loc; loc.owner = this; loc.ptr = value; loc.kind = 4;
    loc.holder = holder; loc.truth = truth; loc.null_result = null_result; return loc;
}
void world::mutate( item &it )
{
    if( !it.mutation || it.mutated ) return;
    it.mutated = true; note( callback_mutation, it.id, it.mutation, future[0], future[1], future[2] );
    if( it.mutation == 2 ) {
        auto &created = add( mutable_empty, 600, true ); created.passive = 100; created.empty_claim = true;
        created.children.push_back( &add( nested_storage, 601 ) );
    } else {
        auto &target = ensure_submap( future ); target.uniform = false;
        auto &created = add( target.tiles[std::size_t( 11 * SEEY + 11 )], 500, true ); created.passive = 100;
    }
}
world::world( const scenario &s ) : config( s )
{
    trace.reserve( 16384 ); avatar.owner = this; avatar.id = 100;
    wakeups.owner = this; reservations.owner = this; game.owner = this;
    npcs.resize( 2 ); for( int n = 0; n < 2; ++n ) { npcs[std::size_t( n )].owner = this; npcs[std::size_t( n )].id = 200 + n; }
    for( int64_t token : {777, 81, 82, 83} ) reservations.records[token] = {this, token, {900 + int( token )}};
    const int zmin = s.zlevels ? -OVERMAP_DEPTH : s.level, zmax = s.zlevels ? OVERMAP_HEIGHT : s.level;
    source = {0, 0, zmin}; future = s.zlevels ? key{0, 0, zmin + 1} : key{0, 1, zmin};
    for( int x = 0; x < s.width; ++x ) for( int y = 0; y < s.width; ++y ) for( int z = zmin; z <= zmax; ++z ) {
        const key position{x, y, z};
        if( s.all_missing || ( s.missing && ( x + y + z - zmin ) % 5 == 3 ) ) { submaps[position] = nullptr; continue; }
        auto &sm = ensure_submap( position ); sm.uniform = !( s.nonuniform_empty || ( s.mixed && ( x + y + z - zmin ) % 3 == 0 ) );
    }
    if( s.sentinel == 1 ) {
        auto &shared = add( mutable_empty, 600, true ); shared.passive = 100; shared.empty_claim = true;
        shared.children.push_back( &add( nested_storage, 601 ) );
    } else if( s.sentinel == 2 ) {
        auto &shared = add( const_empty, 700, true ); shared.passive = 100;
    }
    if( s.core_items || s.mutation == 1 || s.mutation == 2 || s.mutation == 3 ) {
        auto &sm = ensure_submap( source ); sm.uniform = false;
        for( int id = 10; id <= 17; ++id ) {
            const int x = id == 17 ? 11 : ( id - 10 ) % 2, y = id == 17 ? 11 : ( id - 10 ) / 2;
            auto &it = add( sm.tiles[std::size_t( x * SEEY + y )], id, id != 17 );
            if( id == 10 || id == 11 || id == 14 || id == 15 ) it.passive = 100;
            if( id == 11 ) { it.token = 80; it.env = 123; }
            if( id == 12 ) { it.token = 81; it.empty_claim = true; }
            if( id == 14 ) it.release_token = true;
            if( id == 15 ) { it.token = 82; it.empty_claim = true; it.erase_on_wakeup = 82; }
            if( id == 16 ) { it.token = 83; it.release_token = true; }
        }
        auto &child = add( nested_storage, 50, true ); child.passive = 100;
        child.children.push_back( &add( nested_storage, 51 ) );
        auto &expired = add( nested_storage, 52 ); expired.expired_location = true;
        identities.at( 10 )->children = {&child, nullptr, &expired};
        if( s.mutation ) identities.at( 10 )->mutation = s.mutation == 2 ? 2 : 1;
        if( s.mutation == 3 ) submaps[future] = nullptr;
    }
    vehicle_views.push_back( {nullptr} );
    if( s.foreign_items || s.mutation >= 4 ) {
        auto v = std::make_unique<vehicle>(); v->owner = this; v->id = 300; v->parts.resize( 3 );
        for( int i = 0; i < 3; ++i ) { v->parts[std::size_t( i )].index = i; v->parts[std::size_t( i )].mount_x = i == 2 ? 1 : 0; }
        auto &first = add( v->parts[0].items, 20, true ); first.passive = 100;
        first.children.push_back( &add( nested_storage, 60 ) );
        add( v->parts[1].items, 21 );
        auto &third = add( v->parts[2].items, 22, true ); third.token = 98; third.empty_claim = true;
        if( s.mutation == 4 ) first.mutation = 1;
        vehicle_views.push_back( {v.get()} ); owned_vehicles.push_back( std::move( v ) );
        auto empty_vehicle = std::make_unique<vehicle>(); empty_vehicle->owner = this; empty_vehicle->id = 301;
        vehicle_views.push_back( {empty_vehicle.get()} ); owned_vehicles.push_back( std::move( empty_vehicle ) );
        auto &held = add( avatar.storage, 30 ); auto &held_child = add( nested_storage, 70, true ); held_child.passive = 100; held_child.token = 70;
        held.children.push_back( &held_child );
        avatar.locations = {inventory_location( &held, avatar.id ), inventory_location( &held_child, avatar.id ),
                            inventory_location( nullptr, avatar.id, false ), inventory_location( nullptr, avatar.id, true, true )};
        auto &person = npcs[0]; auto &pocket = add( person.storage, 31, true ); pocket.token = 31;
        auto &pocket_child = add( nested_storage, 71 ); pocket.children.push_back( &pocket_child );
        person.locations = {inventory_location( &pocket, person.id ), inventory_location( &pocket_child, person.id ),
                            inventory_location( &pocket_child, person.id, false )};
        if( s.mutation == 5 ) pocket.mutation = 1;
    }
    trace.clear(); mutable_getters = const_getters = 0;
}
std::vector<int64_t> world::snapshot() const
{
    std::vector<int64_t> out;
    auto container = [&]( int kind, key position, int sx, int sy, const std::list<item> &items ) {
        if( items.empty() ) return;
        out.insert( out.end(), {kind, position[0], position[1], position[2], sx, sy, int64_t( items.size() )} );
        for( const auto &it : items ) out.push_back( it.id );
    };
    for( const auto &entry : submaps ) {
        out.insert( out.end(), {1, entry.first[0], entry.first[1], entry.first[2], bool( entry.second ), entry.second ? entry.second->uniform : false} );
        if( entry.second ) for( int x = 0; x < SEEX; ++x ) for( int y = 0; y < SEEY; ++y )
            container( 2, entry.first, x, y, entry.second->tiles[std::size_t( x * SEEY + y )] );
    }
    container( 3, {0, 0, 0}, 0, 0, mutable_empty ); container( 4, {0, 0, 0}, 0, 0, const_empty );
    for( const auto &entry : identities ) {
        const auto &it = *entry.second;
        out.insert( out.end(), {5, it.id, it.craft, it.passive, it.token, it.env, it.mutated, it.expired_location, int64_t( it.children.size() )} );
        for( const auto *child : it.children ) out.push_back( child ? child->id : 0 );
    }
    for( const auto &v : owned_vehicles ) for( const auto &p : v->parts ) container( 6, {v->id, p.index, 0}, p.mount_x, p.mount_y, p.items );
    container( 7, {avatar.id, 0, 0}, 0, 0, avatar.storage );
    for( const auto &person : npcs ) container( 7, {person.id, 0, 0}, 0, 0, person.storage );
    for( const auto &entry : reservations.records ) {
        out.insert( out.end(), {8, entry.first, int64_t( entry.second.claims.size() )} );
        for( int claim : entry.second.claims ) out.push_back( claim );
    }
    for( const auto &entry : wakeups.entries ) { out.push_back( 9 ); out.insert( out.end(), entry.second.begin(), entry.second.end() ); }
    return out;
}
struct outcome {
    std::vector<event> trace;
    std::vector<int64_t> state;
    std::map<int, event> wakeups;
    std::vector<int64_t> tokens;
    std::uint64_t mutable_getters = 0, const_getters = 0;
    bool legacy_env_updated = false, released_token = false, future_item_created = false;
};
template<class Map> outcome run( const scenario &s, reconcile_scope scope )
{
    auto w = std::make_unique<world>( s ); active_world active( *w ); Map map( *w );
    map.reconcile_loaded_items( scope );
    outcome result; result.state = w->snapshot(); result.trace = std::move( w->trace ); result.wakeups = w->wakeups.entries;
    result.mutable_getters = w->mutable_getters; result.const_getters = w->const_getters;
    for( const auto &entry : w->reservations.records ) result.tokens.push_back( entry.first );
    const auto ten = w->identities.find( 10 ), fourteen = w->identities.find( 14 );
    result.legacy_env_updated = ten != w->identities.end() && ten->second->env == calendar::turn;
    result.released_token = fourteen != w->identities.end() && fourteen->second->token == 0;
    result.future_item_created = w->identities.count( 500 ) != 0;
    return result;
}
template<class Frozen, class Actual> std::pair<outcome, outcome> paired( const scenario &s, reconcile_scope scope )
{
    auto original = run<Frozen>( s, scope ), actual = run<Actual>( s, scope ); ++pairs;
    require( original.trace == actual.trace, "ordered item/craft/wakeup/cleanup/vehicle/character and nonuniform getter callbacks remain exact" );
    require( original.state == actual.state, "every owned item, container, reservation and wakeup state equals its pinned frozen oracle" );
    require( original.const_getters == 0 && actual.const_getters == 0, "the Native mutable overload must not be substituted by its separate const sentinel" );
    const bool foreign_retained = std::find( original.tokens.begin(), original.tokens.end(), 777 ) != original.tokens.end();
    require( foreign_retained == ( scope == reconcile_scope::additive ), "full rebuild clears old claims while additive retains unvisited reservations" );
    if( s.core_items ) {
        require( original.legacy_env_updated && original.released_token, "live/no-token migration and released-token branches are exercised" );
        for( int64_t gone : {81, 82, 83} ) require( std::find( original.tokens.begin(), original.tokens.end(), gone ) == original.tokens.end(), "seen cleanup and callback-removed reservations are exercised" );
        require( original.wakeups.count( 10 ) && original.wakeups.count( 50 ) && original.wakeups.count( 51 ), "map children and grandchildren are reconciled" );
        require( !original.wakeups.count( 52 ), "null and expired child locations never reach an item callback" );
    }
    if( s.foreign_items || s.mutation >= 4 ) {
        for( int id : {20, 21, 22, 30, 70, 31, 71} ) require( original.wakeups.count( id ) == 1, "all duplicate-mount vehicle parts and pre-recursive character items are visited" );
        require( original.wakeups.at( 20 )[3] == 0 && original.wakeups.at( 21 )[3] == 1,
                 "separate cargo parts sharing a mount retain their own cursors" );
    }
    if( s.sentinel == 1 ) require( original.wakeups.count( 600 ) && original.wakeups.count( 601 ), "nonempty mutable/empty const sentinel must retain the complete fallback" );
    if( s.sentinel == 2 ) require( original.wakeups.count( 700 ) == 0, "the const sentinel never creates Native mutable items" );
    if( s.mutation == 1 || s.mutation == 3 ) require( original.future_item_created && original.wakeups.count( 500 ), "callbacks materialize a future uniform or missing submap before its turn" );
    if( s.mutation == 2 ) require( original.wakeups.count( 600 ) && original.wakeups.count( 601 ), "callbacks fill the shared mutable sentinel before later submaps are queried" );
    if( s.mutation >= 4 ) require( original.future_item_created && !original.wakeups.count( 500 ), "late vehicle/NPC mutations do not rescan already visited map cells" );
    return {std::move( original ), std::move( actual )};
}
std::vector<scenario> scenarios()
{
    std::vector<scenario> out;
    scenario s; out.push_back( s );
    s.width = 2; s.nonuniform_empty = true; out.push_back( s );
    s = {}; s.width = 2; s.mixed = true; s.core_items = s.foreign_items = true; out.push_back( s );
    s = {}; s.zlevels = true; s.mixed = true; s.core_items = s.foreign_items = true; out.push_back( s );
    s.width = 2; s.missing = true; out.push_back( s );
    s = {}; s.sentinel = 1; s.foreign_items = true; out.push_back( s );
    s = {}; s.sentinel = 2; out.push_back( s );
    s = {}; s.width = 2; s.mutation = 1; s.core_items = true; out.push_back( s );
    s.mutation = 2; out.push_back( s );
    s = {}; s.zlevels = true; s.mutation = 1; s.core_items = true; out.push_back( s );
    s.width = 2; s.mutation = 3; out.push_back( s );
    s = {}; s.width = 2; s.mutation = 4; s.foreign_items = true; out.push_back( s );
    s.mutation = 5; out.push_back( s );
    s = {}; s.width = 3; s.mixed = true; s.core_items = s.foreign_items = true; s.origin = {-360, -624, 0}; out.push_back( s );
    s = {}; s.zlevels = true; s.missing = true; s.sentinel = 1; s.origin = {-120, 24, 0}; out.push_back( s );
    s = {}; s.width = 2; s.all_missing = true; s.foreign_items = true; out.push_back( s );
    s.zlevels = true; out.push_back( s );
    s = {}; s.width = 3; s.zlevels = true; s.mixed = true; s.missing = true; s.core_items = s.foreign_items = true; s.origin = {-48, -96, 0}; out.push_back( s );
    return out;
}
void run_cases()
{
    checks = pairs = 0;
    const auto matrix = scenarios();
    for( const auto &s : matrix ) for( auto scope : {reconcile_scope::full_rebuild, reconcile_scope::additive} ) {
        paired<frozen1040::map, actual1040::map>( s, scope );
        paired<frozen1807::map, actual1807::map>( s, scope );
    }
    std::cout << "Native item-reconciliation semantic matrix: " << pairs << " pinned pairs passed before uniform getter acceptance\n";
    // Last, and only after every semantic pair: the original actual include
    // must fail this bounded repeated-empty-container read acceptance test.
    struct counts { std::uint64_t before, after, expected; };
    std::vector<counts> measured;
    for( bool z : {false, true} ) {
        scenario s; s.width = 11; s.zlevels = z; s.origin = {-360, -624, 0};
        const auto a = paired<frozen1040::map, actual1040::map>( s, reconcile_scope::additive );
        const auto b = paired<frozen1807::map, actual1807::map>( s, reconcile_scope::additive );
        const auto expected = std::uint64_t( 121 ) * ( z ? 21u : 1u );
        measured.push_back( {a.first.mutable_getters, a.second.mutable_getters, expected} );
        measured.push_back( {b.first.mutable_getters, b.second.mutable_getters, expected} );
        std::cout << "Uniform reconciliation getters zlevels=" << z << ": 1040 " << a.first.mutable_getters << " -> " << a.second.mutable_getters
                  << ", 1807 " << b.first.mutable_getters << " -> " << b.second.mutable_getters << ", candidate " << expected << '\n';
    }
    for( const auto &row : measured ) {
        require( row.before == row.expected * 144, "frozen baseline reads every tile list in the complete Native Z range" );
        require( row.after == row.expected, "each empty uniform submap requires exactly one mutable sentinel getter (fail-before control)" );
    }
    std::cout << "Native item reconciliation: " << pairs << " pinned pairs / " << checks << " checks passed; getter counts are not Native elapsed time\n";
}
} // namespace native_item_reconciliation_fixture

void test_native_item_reconciliation()
{
    native_item_reconciliation_fixture::run_cases();
}
