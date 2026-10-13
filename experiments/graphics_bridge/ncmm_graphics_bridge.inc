// Included only in an explicitly prepared experimental Host, inside ncmm's
// anonymous namespace. Canonical NCMM sources and shipping manifests stay intact.
struct graphics_view_state {
    std::string owner;
    ncmm_graphics_render_v1 render = nullptr;
    ncmm_graphics_input_v1 input = nullptr;
    void *user_data = nullptr;
    bool enabled = false;
    ncmm_graphics_render_v2 hybrid_render = nullptr;
    ncmm_graphics_render_multilevel_v1 world_render = nullptr;
};
graphics_view_state graphics_view;
bool graphics_gameplay_context = false;
const ncmm_graphics_scene_v1 *graphics_frame = nullptr;
bool graphics_frame_invalid = false;
bool graphics_render_active = false;
std::vector<ncmm_graphics_quad_v1> graphics_commands;
size_t graphics_resolved_textures = 0;
size_t graphics_resolved_vehicle_textures = 0;
size_t graphics_ceiling_cells = 0;
int graphics_snapshot_radius = 0;
size_t graphics_snapshot_far_cells = 0;
size_t graphics_snapshot_light_queries = 0;
std::chrono::steady_clock::time_point graphics_snapshot_begun;
double graphics_snapshot_ms = 0, graphics_callback_ms = 0, graphics_presentation_ms = 0;
loaded_mod *find_loaded_by_id( const char *module_id );
void graphics_clear_hybrid();
void graphics_clear_gpu();
void graphics_release_mouse();
void graphics_cancel_animation();
void graphics_clear_world();

void graphics_erase( const std::string &id )
{
    if( graphics_view.owner == id ) { graphics_release_mouse(); graphics_cancel_animation(); graphics_view = {}; graphics_clear_hybrid(); }
}

int graphics_register( const char *id, ncmm_graphics_render_v1 render,
                       ncmm_graphics_input_v1 input, void *user_data )
{
    if( graphics_render_active || !active_module_matches( id ) || !module_ids.count( id ) || !render ||
        ( !graphics_view.owner.empty() && graphics_view.owner != id ) ) return 0;
    graphics_view = { id, render, input, user_data, false };
    return 1;
}

int graphics_enable( const char *id, int enabled )
{
    if( !active_module_matches( id ) || graphics_view.owner != id ||
        ( !graphics_view.render && !graphics_view.hybrid_render && !graphics_view.world_render ) ||
        ( enabled && !character_state_available() ) || graphics_render_active ) return 0;
    const bool changed = graphics_view.enabled != ( enabled != 0 );
    graphics_view.enabled = enabled != 0;
    if( changed || !graphics_view.enabled ) graphics_cancel_animation();
    if( !graphics_view.enabled ) { graphics_release_mouse(); graphics_clear_gpu(); graphics_clear_world(); }
#if defined(TILES)
    if( changed && character_state_available() ) {
        add_msg( m_info, "%s", graphics_view.enabled ?
                 tr_ui( "First person ON. Hold RMB: look; arrows: relative movement; F6: map; F11: quality.",
                        "Первое лицо. Зажать ПКМ: осмотр; стрелки: движение по взгляду; F6: карта; F11: качество." ) :
                 tr_ui( "First person OFF.", "Вид от первого лица выключен." ) );
    }
#endif
    if( g ) g->invalidate_main_ui_adaptor();
    return 1;
}

int graphics_enabled( const char *id )
{
    return active_module_matches( id ) && graphics_view.owner == id && graphics_view.enabled;
}

int graphics_submit( const ncmm_graphics_quad_v1 *quads, size_t count )
{
    if( !graphics_frame || active_module_id != graphics_view.owner || !quads ||
        count > NCMM_GRAPHICS_MAX_QUADS - graphics_commands.size() ) {
        graphics_frame_invalid = true;
        return 0;
    }
    for( size_t i = 0; i < count; ++i ) {
        if( !ncmm_graphics_policy::valid_quad( quads[i], *graphics_frame ) ) {
            graphics_frame_invalid = true;
            return 0;
        }
    }
    graphics_commands.insert( graphics_commands.end(), quads, quads + count );
    return 1;
}

const ncmm_host_graphics_v1 graphics_api = {
    NCMM_GRAPHICS_ABI, sizeof( ncmm_host_graphics_v1 ), 1, 0,
    graphics_register, graphics_enable, graphics_enabled, graphics_submit
};

void graphics_register_actions( input_context &ctxt )
{
    if( !graphics_view.render && !graphics_view.hybrid_render && !graphics_view.world_render ) return;
    const std::pair<const char *, int> keys[] = {
        { "ncmm.view.turn_left", keycode::f7 }, { "ncmm.view.turn_right", keycode::f8 },
        { "ncmm.view.look_up", keycode::f9 }, { "ncmm.view.look_down", keycode::f10 }, { "ncmm.view.quality", keycode::f11 },
        { "ncmm.view.acceleration", keycode::f12 }
    };
    for( const auto &key : keys ) {
        const std::string label = key.second == keycode::f12 ? tr_ui( "First person: Auto / CPU / GPU renderer", "Первое лицо: рендерер Auto / CPU / GPU" ) :
            key.second == keycode::f11 ? tr_ui( "First person: balanced / high quality", "Первое лицо: сбалансированное / высокое качество" ) :
            key.second == keycode::f9 ? tr_ui( "First person: look up", "Первое лицо: смотреть вверх" ) :
            key.second == keycode::f10 ? tr_ui( "First person: look down", "Первое лицо: смотреть вниз" ) :
            key.second == keycode::f7 ?
            tr_ui( "First person: turn left", "Первое лицо: повернуть влево" ) :
            tr_ui( "First person: turn right", "Первое лицо: повернуть вправо" );
        inp_mngr.ncmm_register_context_default_action( key.first, no_translation( label ),
            input_event( key.second, input_event_t::keyboard_code ), "DEFAULTMODE" );
        ctxt.register_action( key.first, no_translation( label ) );
    }
}

bool graphics_action( const std::string &action )
{
    const uint32_t turn = action == "ncmm.view.turn_left" ? NCMM_GFX_TURN_LEFT :
                          action == "ncmm.view.turn_right" ? NCMM_GFX_TURN_RIGHT :
                          action == "ncmm.view.look_up" ? NCMM_GFX_LOOK_UP :
                          action == "ncmm.view.look_down" ? NCMM_GFX_LOOK_DOWN :
                          action == "ncmm.view.quality" ? NCMM_GFX_QUALITY :
                          action == "ncmm.view.mouse_look" ? NCMM_GFX_MOUSE_LOOK :
                          action == "ncmm.view.acceleration" ? NCMM_GFX_ACCELERATION : 0;
    if( turn && ( graphics_view.render || graphics_view.hybrid_render || graphics_view.world_render ) ) {
        if( graphics_view.enabled && graphics_view.input ) {
            loaded_mod *mod = find_loaded_by_id( graphics_view.owner.c_str() );
            if( mod ) {
                try {
                    module_call_scope scope( graphics_view.owner.c_str() );
                    graphics_view.input( turn, graphics_view.user_data );
                } catch( ... ) {
                    quarantine_runtime_callback( *mod, runtime_callback_kind::ui, "graphics_input_exception" );
                }
            }
            if( g ) g->invalidate_main_ui_adaptor();
        }
        return true;
    }
    // Commands do not switch renderers. Actual coordinate-based map contexts
    // suspend the view when they enter input_context::handle_input().
    return false;
}

bool graphics_world_needs_redraw();
#include "ncmm_graphics_controls.inc"
#include "ncmm_graphics_hybrid.inc"
#include "ncmm_graphics_world.inc"

bool graphics_draw( int x, int y, int width, int height )
{
#if defined(TILES)
    if( !graphics_view.enabled || ( !graphics_view.render && !graphics_view.hybrid_render && !graphics_view.world_render ) || !graphics_gameplay_context ||
        !character_state_available() || width < 1 || height < 1 || width > 8192 || height > 8192 ||
        !tilecontext || !tilecontext->is_valid() ) return false;
    if(graphics_view.world_render)return graphics_draw_world(x,y,width,height);
    graphics_snapshot_begun = std::chrono::steady_clock::now();
    avatar &you = get_avatar();
    map &here = get_map();
    // The perspective camera follows the avatar, independently of a saved map
    // offset. Menus and targeting are already suspended by their input context.
    here.update_visibility_cache( you.posz() );
    const auto &visibility = here.get_visibility_variables_cache();
    // V1 retains its original small snapshot. V2 uses the loaded reality bubble
    // only; inbounds checks leave unavailable edge cells empty rather than
    // loading/generating terrain or changing simulation range while rendering.
    const int radius = graphics_view.hybrid_render ? NCMM_GRAPHICS_V2_RADIUS : NCMM_GRAPHICS_RADIUS;
    const int side = radius * 2 + 1;
    std::vector<ncmm_graphics_cell_v1> cells( side * side );
    std::vector<tripoint_bub_ms> positions;
    positions.reserve( cells.size() );
    const tripoint_bub_ms origin = you.pos_bub();
    graphics_viewport = { x, y, width, height };
    graphics_ceiling_cells = 0;
    graphics_snapshot_radius = radius;
    graphics_snapshot_far_cells = 0;
    graphics_snapshot_light_queries = 0;
    std::vector<uint8_t> transparency;
    if( graphics_view.hybrid_render ) transparency.resize( cells.size() );
    for( int gy = 0; gy < side; ++gy ) for( int gx = 0; gx < side; ++gx ) {
        const tripoint_bub_ms p( origin.x() + gx - radius, origin.y() + gy - radius, origin.z() );
        positions.push_back( p );
        if( !transparency.empty() && here.inbounds( p ) ) {
            transparency[gy * side + gx] = here.ter( p ).obj().transparent &&
                ( !here.has_furn( p ) || here.furn( p ).obj().transparent );
        }
    }
    auto copy_id = []( char *dest, const std::string &id ) {
        if( id.size() >= 128 ) return; // fallback geometry, never a truncated identity
        std::copy( id.begin(), id.end(), dest );
    };
    for( int gy = 0; gy < side; ++gy ) for( int gx = 0; gx < side; ++gx ) {
        const tripoint_bub_ms &p = positions[gy * side + gx];
        auto &cell = cells[gy * side + gx];
        if( !here.inbounds( p ) ) continue;
        // Reject the rear/outside-cone cells before asking the lighting cache.
        // Simulation and creature AI remain owned by CDDA, independently of
        // this callback-scoped visual snapshot.
        if( graphics_view.hybrid_render && !graphics_forward_visible( origin, p, &transparency, radius ) ) continue;
        const lit_level light = here.apparent_light_at( p, visibility );
        ++graphics_snapshot_light_queries;
        if( !graphics_view.hybrid_render && here.get_visibility( light, visibility ) != visibility_type::CLEAR ) continue;
        cell.flags = NCMM_GFX_VISIBLE;
        if( graphics_view.hybrid_render && std::max( std::abs( gx - radius ), std::abs( gy - radius ) ) > NCMM_GRAPHICS_RADIUS ) ++graphics_snapshot_far_cells;
        cell.light = light == lit_level::LOW ? 0.45f : 1.0f;
        if( here.has_flag( "WALL", p ) || ( here.has_flag( "DOOR", p ) && here.impassable_ter_furn( p ) ) ||
            ( here.impassable_ter_furn( p ) && !here.is_transparent( p ) ) ) cell.flags |= NCMM_GFX_WALL;
        if( here.has_flag( "NO_FLOOR", p ) ) cell.flags |= NCMM_GFX_NO_FLOOR;
        if( !here.is_outside( p ) ) {
            cell.flags |= NCMM_GFX_CEILING;
            ++graphics_ceiling_cells;
        }
        copy_id( cell.terrain, here.ter( p ).id().str() );
        if( here.has_furn( p ) ) copy_id( cell.furniture, here.furn( p ).id().str() );
        if( const optional_vpart_position vp = here.veh_at( p ) ) {
            const auto displayed = vp->part_displayed();
            if( displayed ) {
                const vpart_display part( displayed->part() );
                cell.flags |= NCMM_GFX_VEHICLE;
                std::fill( std::begin( cell.furniture ), std::end( cell.furniture ), '\0' );
                copy_id( cell.furniture, part.get_tileset_id() );
                // Respect open doors and broken parts. Passable frames/seats
                // remain billboards; closed obstacles provide ray occlusion.
                if( vp->obstacle_at_part() ) cell.flags |= NCMM_GFX_WALL;
            }
        }
        if( here.sees_some_items( p, you ) ) {
            const auto items = here.i_at( p );
            if( !items.empty() ) copy_id( cell.item, items.begin()->typeId().str() );
        }
        const monster *mon = get_creature_tracker().creature_at<monster>( p );
        if( mon && you.sees( here, *mon ) ) copy_id( cell.creature, mon->type->id.str() );
        else if(const Creature *cr=get_creature_tracker().creature_at<Creature>(p)){
            if(cr->is_npc()&&you.sees(here,*cr))if(const auto *ch=dynamic_cast<const Character *>(cr))copy_id(cell.creature,ch->male?"npc_male":"npc_female");
        }
    }
    const ncmm_graphics_scene_v1 scene = { sizeof( ncmm_graphics_scene_v1 ), width, height, side, side,
        radius + 0.5f, radius + 0.5f, cells.data(), cells.size() };
    loaded_mod *mod = find_loaded_by_id( graphics_view.owner.c_str() );
    if( !mod ) { graphics_view.enabled = false; return false; }
    if( graphics_view.hybrid_render ) return graphics_draw_hybrid( scene, positions, x, y );
    graphics_commands.clear();
    graphics_frame = &scene;
    graphics_render_active = true;
    graphics_frame_invalid = false;
    bool ok = false;
    try {
        module_call_scope scope( graphics_view.owner.c_str() );
        ok = graphics_view.render( &scene, graphics_view.user_data ) != 0;
    } catch( ... ) {
        quarantine_runtime_callback( *mod, runtime_callback_kind::ui, "graphics_render_exception" );
    }
    graphics_frame = nullptr;
    graphics_render_active = false;
    if( !ok || graphics_frame_invalid || graphics_commands.empty() ) {
        graphics_view.enabled = false;
        graphics_commands.clear();
        add_msg( m_warning, "%s", tr_ui( "First person could not draw this scene; returned to the map. F6 retries.",
                 "Не удалось отрисовать вид от первого лица; включена карта. F6 — повторить." ) );
        return false;
    }
    SDL_Renderer *renderer = get_sdl_renderer().get();
    SDL_Rect prior_clip;
    const bool had_clip = SDL_RenderClipEnabled( renderer );
    SDL_GetRenderClipRect( renderer, &prior_clip );
    const SDL_Rect clip = { x, y, width, height };
    if( !SDL_SetRenderClipRect( renderer, &clip ) ) return false;
    SDL_Texture *batch_texture = nullptr;
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    auto flush = [&]() {
        if( vertices.empty() ) return true;
        const bool result = SDL_RenderGeometry( renderer, batch_texture, vertices.data(),
            static_cast<int>( vertices.size() ), indices.data(), static_cast<int>( indices.size() ) );
        vertices.clear(); indices.clear();
        return result;
    };
    // Resolve each cell/layer once per frame, and never retain GPU handles across frames.
    std::map<std::pair<uint32_t,uint32_t>,std::optional<texture_draw_data>> textures;
    graphics_resolved_textures = 0;
    graphics_resolved_vehicle_textures = 0;
    for( const auto &q : graphics_commands ) {
        SDL_Texture *texture = nullptr;
        float u0 = q.u0, v0 = q.v0, u1 = q.u1, v1 = q.v1;
        if( q.layer != NCMM_GFX_SOLID ) {
            const auto key = std::make_pair( q.cell_index, q.layer );
            auto found = textures.find( key );
            if( found == textures.end() ) {
                const auto &cell = cells[q.cell_index];
                const char *id = q.layer == NCMM_GFX_TERRAIN ? cell.terrain :
                    q.layer == NCMM_GFX_FURNITURE ? cell.furniture : q.layer == NCMM_GFX_ITEM ? cell.item : cell.creature;
                const TILE_CATEGORY category = q.layer == NCMM_GFX_TERRAIN ? TILE_CATEGORY::TERRAIN :
                    q.layer == NCMM_GFX_FURNITURE ? ( cell.flags & NCMM_GFX_VEHICLE ? TILE_CATEGORY::VEHICLE_PART : TILE_CATEGORY::FURNITURE ) :
                    q.layer == NCMM_GFX_ITEM ? TILE_CATEGORY::ITEM : TILE_CATEGORY::MONSTER;
                auto atlas = tilecontext->get_texture_draw_data( id, category, positions[q.cell_index] );
                if( !atlas && category == TILE_CATEGORY::VEHICLE_PART ) {
                    // Memorized vpart IDs can include #variant. Some tilesets
                    // only provide the base part/looks_like sprite; preserve it.
                    const std::string part_id( id );
                    const size_t separator = part_id.find( '#' );
                    if( separator != std::string::npos ) atlas = tilecontext->get_texture_draw_data(
                        part_id.substr( 0, separator ), category, positions[q.cell_index] );
                }
                found = textures.emplace( key, std::move( atlas ) ).first;
                if( found->second ) {
                    ++graphics_resolved_textures;
                    if( q.layer == NCMM_GFX_FURNITURE && ( cell.flags & NCMM_GFX_VEHICLE ) )
                        ++graphics_resolved_vehicle_textures;
                }
            }
            if( found->second ) {
                const auto &data = *found->second;
                texture = data.texture;
                const float du = data.uv1.first - data.uv0.first, dv = data.uv1.second - data.uv0.second;
                u0 = data.uv0.first + q.u0 * du; u1 = data.uv0.first + q.u1 * du;
                v0 = data.uv0.second + q.v0 * dv; v1 = data.uv0.second + q.v1 * dv;
            } else if( q.layer != NCMM_GFX_TERRAIN &&
                       !( q.layer == NCMM_GFX_FURNITURE && ( cells[q.cell_index].flags & NCMM_GFX_VEHICLE ) ) ) continue;
        }
        if( texture != batch_texture || vertices.size() >= 16000 ) {
            if( !flush() ) { ok = false; break; }
            batch_texture = texture;
        }
        const SDL_FColor color = { ( q.rgba >> 24 ) / 255.0f, ( ( q.rgba >> 16 ) & 255 ) / 255.0f,
                                  ( ( q.rgba >> 8 ) & 255 ) / 255.0f, ( q.rgba & 255 ) / 255.0f };
        const int base = static_cast<int>( vertices.size() );
        vertices.push_back( { { x + q.x0, y + q.y0 }, color, { u0, v0 } } );
        vertices.push_back( { { x + q.x1, y + q.y0 }, color, { u1, v0 } } );
        vertices.push_back( { { x + q.x1, y + q.y1 }, color, { u1, v1 } } );
        vertices.push_back( { { x + q.x0, y + q.y1 }, color, { u0, v1 } } );
        for( int i : { 0, 1, 2, 0, 2, 3 } ) indices.push_back( base + i );
    }
    ok = flush() && ok;
    ok = SDL_SetRenderClipRect( renderer, had_clip ? &prior_clip : nullptr ) && ok;
    if( !ok ) graphics_view.enabled = false;
    return ok;
#else
    ( void )x; ( void )y; ( void )width; ( void )height;
    return false;
#endif
}

// Only the existing explicit --ncmm-gameplay-smoke path invokes this fixture.
// It runs in a disposable CI game copy, after the real world/avatar are loaded.
#include "ncmm_graphics_smoke.inc"
