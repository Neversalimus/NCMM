// Included only in an explicitly prepared experimental Host, inside ncmm's
// anonymous namespace. Canonical NCMM sources and shipping manifests stay intact.
struct graphics_view_state {
    std::string owner;
    ncmm_graphics_render_v1 render = nullptr;
    ncmm_graphics_input_v1 input = nullptr;
    void *user_data = nullptr;
    bool enabled = false;
};
graphics_view_state graphics_view;
bool graphics_gameplay_context = false;
const ncmm_graphics_scene_v1 *graphics_frame = nullptr;
bool graphics_frame_invalid = false;
std::vector<ncmm_graphics_quad_v1> graphics_commands;
loaded_mod *find_loaded_by_id( const char *module_id );

void graphics_erase( const std::string &id )
{
    if( graphics_view.owner == id ) graphics_view = {};
}

int graphics_register( const char *id, ncmm_graphics_render_v1 render,
                       ncmm_graphics_input_v1 input, void *user_data )
{
    if( !active_module_matches( id ) || !module_ids.count( id ) || !render ||
        ( !graphics_view.owner.empty() && graphics_view.owner != id ) ) return 0;
    graphics_view = { id, render, input, user_data, false };
    return 1;
}

int graphics_enable( const char *id, int enabled )
{
    if( !active_module_matches( id ) || graphics_view.owner != id || !graphics_view.render ||
        ( enabled && !character_state_available() ) || graphics_frame ) return 0;
    graphics_view.enabled = enabled != 0;
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
    if( !graphics_view.render ) return;
    const std::pair<const char *, int> keys[] = {
        { "ncmm.view.turn_left", keycode::f7 }, { "ncmm.view.turn_right", keycode::f8 }
    };
    for( const auto &key : keys ) {
        const std::string label = key.second == keycode::f7 ?
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
                          action == "ncmm.view.turn_right" ? NCMM_GFX_TURN_RIGHT : 0;
    if( turn && graphics_view.render ) {
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
    // Suspend BEFORE menus/targeting draw. handle_input(DEFAULTMODE) resumes.
    static const std::set<std::string> pass = { "UP", "DOWN", "LEFT", "RIGHT",
        "LEFTUP", "RIGHTUP", "LEFTDOWN", "RIGHTDOWN", "pause", "TIMEOUT" };
    if( !pass.count( action ) && action != "ncmm.open." + graphics_view.owner ) {
        graphics_gameplay_context = false;
        if( g ) g->invalidate_main_ui_adaptor();
    }
    return false;
}

bool graphics_draw( int x, int y, int width, int height )
{
#if defined(TILES)
    if( !graphics_view.enabled || !graphics_view.render || !graphics_gameplay_context ||
        !character_state_available() || width < 1 || height < 1 || width > 8192 || height > 8192 ||
        !tilecontext || !tilecontext->is_valid() ) return false;
    avatar &you = get_avatar();
    map &here = get_map();
    if( g->ter_view_p != you.pos_bub() || here.veh_at( you.pos_bub() ) ) return false;
    here.update_visibility_cache( you.posz() );
    const auto &visibility = here.get_visibility_variables_cache();
    constexpr int side = NCMM_GRAPHICS_RADIUS * 2 + 1;
    std::vector<ncmm_graphics_cell_v1> cells( side * side );
    std::vector<tripoint_bub_ms> positions;
    positions.reserve( cells.size() );
    const tripoint_bub_ms origin = you.pos_bub();
    auto copy_id = []( char *dest, const std::string &id ) {
        if( id.size() >= 128 ) return; // fallback geometry, never a truncated identity
        std::copy( id.begin(), id.end(), dest );
    };
    for( int gy = 0; gy < side; ++gy ) for( int gx = 0; gx < side; ++gx ) {
        const tripoint_bub_ms p( origin.x() + gx - NCMM_GRAPHICS_RADIUS,
                                origin.y() + gy - NCMM_GRAPHICS_RADIUS, origin.z() );
        positions.push_back( p );
        auto &cell = cells[gy * side + gx];
        if( !here.inbounds( p ) ) continue;
        const lit_level light = here.apparent_light_at( p, visibility );
        if( here.get_visibility( light, visibility ) != visibility_type::CLEAR ) continue;
        // Vehicles require their own geometry and are deliberately a later stage.
        if( here.veh_at( p ) ) return false;
        cell.flags = NCMM_GFX_VISIBLE;
        cell.light = light == lit_level::LOW ? 0.45f : 1.0f;
        if( here.has_flag( "WALL", p ) || ( here.has_flag( "DOOR", p ) && here.impassable_ter_furn( p ) ) ||
            ( here.impassable_ter_furn( p ) && !here.is_transparent( p ) ) ) cell.flags |= NCMM_GFX_WALL;
        if( here.has_flag( "NO_FLOOR", p ) ) cell.flags |= NCMM_GFX_NO_FLOOR;
        copy_id( cell.terrain, here.ter( p ).id().str() );
        if( here.has_furn( p ) ) copy_id( cell.furniture, here.furn( p ).id().str() );
        if( here.sees_some_items( p, you ) ) {
            const auto items = here.i_at( p );
            if( !items.empty() ) copy_id( cell.item, items.begin()->typeId().str() );
        }
        const monster *mon = get_creature_tracker().creature_at<monster>( p );
        if( mon && you.sees( here, *mon ) ) copy_id( cell.creature, mon->type->id.str() );
    }
    const ncmm_graphics_scene_v1 scene = { sizeof( ncmm_graphics_scene_v1 ), width, height, side, side,
        NCMM_GRAPHICS_RADIUS + 0.5f, NCMM_GRAPHICS_RADIUS + 0.5f, cells.data(), cells.size() };
    loaded_mod *mod = find_loaded_by_id( graphics_view.owner.c_str() );
    if( !mod ) { graphics_view.enabled = false; return false; }
    graphics_commands.clear();
    graphics_frame = &scene;
    graphics_frame_invalid = false;
    bool ok = false;
    try {
        module_call_scope scope( graphics_view.owner.c_str() );
        ok = graphics_view.render( &scene, graphics_view.user_data ) != 0;
    } catch( ... ) {
        quarantine_runtime_callback( *mod, runtime_callback_kind::ui, "graphics_render_exception" );
    }
    graphics_frame = nullptr;
    if( !ok || graphics_frame_invalid || graphics_commands.empty() ) {
        graphics_view.enabled = false;
        graphics_commands.clear();
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
                    q.layer == NCMM_GFX_FURNITURE ? TILE_CATEGORY::FURNITURE : q.layer == NCMM_GFX_ITEM ? TILE_CATEGORY::ITEM : TILE_CATEGORY::MONSTER;
                found = textures.emplace( key, tilecontext->get_texture_draw_data( id, category, positions[q.cell_index] ) ).first;
            }
            if( found->second ) {
                const auto &data = *found->second;
                texture = data.texture;
                const float du = data.uv1.first - data.uv0.first, dv = data.uv1.second - data.uv0.second;
                u0 = data.uv0.first + q.u0 * du; u1 = data.uv0.first + q.u1 * du;
                v0 = data.uv0.second + q.v0 * dv; v1 = data.uv0.second + q.v1 * dv;
            } else if( q.layer != NCMM_GFX_TERRAIN ) continue;
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
