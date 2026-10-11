// Callback-scoped CPU scene bridge for the hybrid renderer. No game/GPU handle
// is exposed to a module; submitted pixels are validated and copied atomically.
const ncmm_graphics_scene_v2 *graphics_hybrid_frame = nullptr;
const std::vector<tripoint_bub_ms> *graphics_hybrid_positions = nullptr;
std::vector<uint32_t> graphics_hybrid_pixels;
uint32_t graphics_hybrid_width = 0, graphics_hybrid_height = 0;
size_t graphics_hybrid_sprite_bytes = 0;
uint64_t graphics_hybrid_tileset_epoch = 0;
size_t graphics_hybrid_vehicle_cells = 0;
size_t graphics_hybrid_light_cells = 0;
size_t graphics_hybrid_colored_cells = 0;
size_t graphics_hybrid_held_sprites = 0;
std::array<size_t, NCMM_SHAPE_COUNT> graphics_hybrid_shape_counts = {};
ncmm_graphics_environment_v2 graphics_hybrid_last_environment = {};

#if defined(TILES)
struct graphics_cpu_sprite {
    uint32_t width = 0, height = 0;
    std::vector<uint32_t> pixels;
};
using graphics_sprite_key = std::tuple<uintptr_t, int, int, int, int>;
std::map<graphics_sprite_key, graphics_cpu_sprite> graphics_cpu_sprites;
SDL_Renderer *graphics_hybrid_renderer_identity = nullptr; // comparison only

bool graphics_read_sprite( const texture_draw_data &atlas, graphics_cpu_sprite &out )
{
    SDL_Renderer *renderer = get_sdl_renderer().get();
    const SDL_Rect rect = atlas.dimensions;
    if( !renderer || !atlas.texture || rect.w < 1 || rect.h < 1 || rect.w > 512 || rect.h > 512 ) return false;
    SDL_Texture *prior_target = SDL_GetRenderTarget( renderer );
    SDL_Rect prior_viewport = {}, prior_clip = {};
    if( !SDL_GetRenderViewport( renderer, &prior_viewport ) || !SDL_GetRenderClipRect( renderer, &prior_clip ) ) return false;
    const bool clipped = SDL_RenderClipEnabled( renderer );
    float scale_x = 1, scale_y = 1;
    if( !SDL_GetRenderScale( renderer, &scale_x, &scale_y ) ) return false;
    int logical_w = 0, logical_h = 0;
    SDL_RendererLogicalPresentation logical_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if( !SDL_GetRenderLogicalPresentation( renderer, &logical_w, &logical_h, &logical_mode ) ) return false;
    Uint8 dr = 0, dg = 0, db = 0, da = 0, tr = 255, tg = 255, tb = 255, ta = 255;
    if( !SDL_GetRenderDrawColor( renderer, &dr, &dg, &db, &da ) ||
        !SDL_GetTextureColorMod( atlas.texture, &tr, &tg, &tb ) || !SDL_GetTextureAlphaMod( atlas.texture, &ta ) ) return false;
    SDL_BlendMode blend = SDL_BLENDMODE_BLEND;
    if( !SDL_GetTextureBlendMode( atlas.texture, &blend ) ) return false;
    SDL_Texture *target = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, rect.w, rect.h );
    if( !target ) return false;
    bool ok = SDL_SetRenderTarget( renderer, target );
    ok = SDL_SetRenderLogicalPresentation( renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED ) && ok;
    ok = SDL_SetRenderScale( renderer, 1, 1 ) && ok;
    ok = SDL_SetRenderViewport( renderer, nullptr ) && ok;
    ok = SDL_SetRenderClipRect( renderer, nullptr ) && ok;
    ok = SDL_SetRenderDrawColor( renderer, 0, 0, 0, 0 ) && ok;
    ok = SDL_RenderClear( renderer ) && ok;
    ok = SDL_SetTextureColorMod( atlas.texture, 255, 255, 255 ) && ok;
    ok = SDL_SetTextureAlphaMod( atlas.texture, 255 ) && ok;
    ok = SDL_SetTextureBlendMode( atlas.texture, SDL_BLENDMODE_NONE ) && ok;
    const SDL_FRect source = { float( rect.x ), float( rect.y ), float( rect.w ), float( rect.h ) };
    const SDL_FRect dest = { 0, 0, float( rect.w ), float( rect.h ) };
    ok = SDL_RenderTexture( renderer, atlas.texture, &source, &dest ) && ok;
    SDL_Surface *surface = ok ? SDL_RenderReadPixels( renderer, nullptr ) : nullptr;
    SDL_Surface *rgba = surface ? SDL_ConvertSurface( surface, SDL_PIXELFORMAT_RGBA8888 ) : nullptr;
    if( rgba ) {
        out.width = uint32_t( rgba->w ); out.height = uint32_t( rgba->h );
        out.pixels.resize( size_t( out.width ) * out.height );
        for( uint32_t y = 0; y < out.height; ++y ) {
            const char *row = static_cast<const char *>( rgba->pixels ) + y * rgba->pitch;
            std::memcpy( out.pixels.data() + size_t( y ) * out.width, row, size_t( out.width ) * 4 );
        }
    } else ok = false;
    if( rgba ) SDL_DestroySurface( rgba );
    if( surface ) SDL_DestroySurface( surface );
    ok = SDL_SetTextureColorMod( atlas.texture, tr, tg, tb ) && ok;
    ok = SDL_SetTextureAlphaMod( atlas.texture, ta ) && ok;
    ok = SDL_SetTextureBlendMode( atlas.texture, blend ) && ok;
    ok = SDL_SetRenderTarget( renderer, prior_target ) && ok;
    ok = SDL_SetRenderLogicalPresentation( renderer, logical_w, logical_h, logical_mode ) && ok;
    ok = SDL_SetRenderScale( renderer, scale_x, scale_y ) && ok;
    ok = SDL_SetRenderViewport( renderer, &prior_viewport ) && ok;
    ok = SDL_SetRenderClipRect( renderer, clipped ? &prior_clip : nullptr ) && ok;
    ok = SDL_SetRenderDrawColor( renderer, dr, dg, db, da ) && ok;
    SDL_DestroyTexture( target );
    return ok;
}
#endif

void graphics_clear_hybrid()
{
    graphics_hybrid_frame = nullptr; graphics_hybrid_positions = nullptr;
    graphics_hybrid_pixels.clear(); graphics_hybrid_width = graphics_hybrid_height = 0;
    graphics_hybrid_sprite_bytes = 0; graphics_hybrid_tileset_epoch = 0;
#if defined(TILES)
    graphics_cpu_sprites.clear(); graphics_hybrid_renderer_identity = nullptr;
#endif
}

int graphics_register_hybrid( const char *id, ncmm_graphics_render_v2 render,
                             ncmm_graphics_input_v1 input, void *data )
{
    if( graphics_render_active || !active_module_matches( id ) || !module_ids.count( id ) || !render ||
        ( !graphics_view.owner.empty() && graphics_view.owner != id ) ) return 0;
    graphics_view = { id, nullptr, input, data, false, render };
    return 1;
}

int graphics_get_sprite( uint32_t index, uint32_t layer, ncmm_graphics_sprite_v2 *out )
{
#if defined(TILES)
    if( !graphics_hybrid_frame || !graphics_hybrid_positions || active_module_id != graphics_view.owner ||
        !out || out->struct_size < sizeof( ncmm_graphics_sprite_v2 ) || layer > NCMM_GFX_FIELD || layer == NCMM_GFX_SOLID ) return 0;
    const auto &scene = *graphics_hybrid_frame;
    std::string id;
    TILE_CATEGORY category = TILE_CATEGORY::TERRAIN;
    tripoint_bub_ms position = get_avatar().pos_bub();
    if( layer == NCMM_GFX_HELD_ITEM ) {
        if( index != UINT32_MAX || !scene.environment.held_item[0] ) return 0;
        id = scene.environment.held_item; category = TILE_CATEGORY::ITEM;
    } else {
        if( index >= scene.base.cell_count || !( scene.base.cells[index].flags & NCMM_GFX_VISIBLE ) ) return 0;
        const auto &cell = scene.base.cells[index];
        const auto &detail = scene.details[index];
        position = ( *graphics_hybrid_positions )[index];
        id = layer == NCMM_GFX_GROUND ? detail.ground : layer == NCMM_GFX_TERRAIN ? cell.terrain :
             layer == NCMM_GFX_FURNITURE ? cell.furniture : layer == NCMM_GFX_ITEM ? cell.item :
             layer == NCMM_GFX_CREATURE ? cell.creature : detail.field;
        category = layer == NCMM_GFX_FURNITURE ? ( cell.flags & NCMM_GFX_VEHICLE ? TILE_CATEGORY::VEHICLE_PART : TILE_CATEGORY::FURNITURE ) :
                   layer == NCMM_GFX_ITEM ? TILE_CATEGORY::ITEM : layer == NCMM_GFX_CREATURE ? TILE_CATEGORY::MONSTER :
                   layer == NCMM_GFX_FIELD ? TILE_CATEGORY::FIELD : TILE_CATEGORY::TERRAIN;
    }
    if( id.empty() ) return 0;
    auto atlas = tilecontext->ncmm_get_texture_draw_data( id, category, position, layer == NCMM_GFX_GROUND );
    if( !atlas && category == TILE_CATEGORY::VEHICLE_PART ) {
        const size_t mark = id.find( '#' );
        if( mark != std::string::npos ) atlas = tilecontext->ncmm_get_texture_draw_data( id.substr( 0, mark ), category, position, false );
    }
    if( !atlas ) return 0;
    const SDL_Rect &r = atlas->dimensions;
    const graphics_sprite_key key{ reinterpret_cast<uintptr_t>( atlas->texture ), r.x, r.y, r.w, r.h };
    auto found = graphics_cpu_sprites.find( key );
    if( found == graphics_cpu_sprites.end() ) {
        if( graphics_cpu_sprites.size() >= 1024 || r.w < 1 || r.h < 1 || r.w > 512 || r.h > 512 ||
            graphics_hybrid_sprite_bytes + size_t( r.w ) * r.h * 4 > 64 * 1024 * 1024 ) return 0;
        graphics_cpu_sprite sprite;
        if( !graphics_read_sprite( *atlas, sprite ) ) return 0;
        graphics_hybrid_sprite_bytes += sprite.pixels.size() * 4;
        found = graphics_cpu_sprites.emplace( key, std::move( sprite ) ).first;
    }
    const auto &sprite = found->second;
    *out = { sizeof( ncmm_graphics_sprite_v2 ), sprite.width, sprite.height, sprite.width, sprite.pixels.data() };
    ++graphics_resolved_textures;
    if( category == TILE_CATEGORY::VEHICLE_PART ) ++graphics_resolved_vehicle_textures;
    if( layer == NCMM_GFX_HELD_ITEM ) ++graphics_hybrid_held_sprites;
    return 1;
#else
    ( void )index; ( void )layer; ( void )out;
    return 0;
#endif
}

int graphics_submit_frame( const uint32_t *rgba, uint32_t width, uint32_t height, uint32_t pitch )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !rgba || !graphics_hybrid_pixels.empty() ||
        width < 1 || height < 1 || width > NCMM_GRAPHICS_MAX_FRAME_WIDTH || height > NCMM_GRAPHICS_MAX_FRAME_HEIGHT ||
        width > uint32_t( graphics_hybrid_frame->base.pixel_width ) || height > uint32_t( graphics_hybrid_frame->base.pixel_height ) || pitch != width ) {
        graphics_frame_invalid = true; return 0;
    }
    graphics_hybrid_pixels.assign( rgba, rgba + size_t( width ) * height );
    graphics_hybrid_width = width; graphics_hybrid_height = height;
    return 1;
}
const ncmm_host_graphics_v2 graphics_hybrid_api = { 2, sizeof( ncmm_host_graphics_v2 ), 2, 0,
    graphics_register_hybrid, graphics_enable, graphics_enabled, graphics_get_sprite, graphics_submit_frame };

#if defined(TILES)
ncmm_graphics_shapes::traits graphics_traits( const map_data_common_t &t, bool opened )
{
    ncmm_graphics_shapes::traits out;
    out.wall = t.has_flag( "WALL" ); out.door = t.has_flag( "DOOR" ); out.window = t.has_flag( "WINDOW" );
    out.tree = t.has_flag( "TREE" ); out.shrub = t.has_flag( "SHRUB" ); out.flat = t.has_flag( "FLAT" );
    out.water = t.has_flag( "SWIMMABLE" ) || t.has_flag( "SHALLOW_WATER" );
    out.transparent = t.transparent; out.impassable = t.movecost == 0; out.open = opened;
    return out;
}

bool graphics_draw_hybrid( const ncmm_graphics_scene_v1 &base, const std::vector<tripoint_bub_ms> &positions, int x, int y )
{
    avatar &you = get_avatar(); map &here = get_map();
    std::vector<ncmm_graphics_cell_v2> details( base.cell_count );
    std::map<const vehicle *, uint32_t> groups;
    std::map<const vehicle *, std::pair<float, float>> vehicle_anchors;
    auto copy_id = []( char ( &dest )[128], const std::string &id ) { if( id.size() < sizeof( dest ) ) std::copy( id.begin(), id.end(), dest ); };
    graphics_hybrid_vehicle_cells = graphics_hybrid_light_cells = graphics_hybrid_held_sprites = 0;
    graphics_hybrid_colored_cells = 0;
    graphics_hybrid_shape_counts.fill( 0 );
    for( size_t i = 0; i < base.cell_count; ++i ) {
        if( !( base.cells[i].flags & NCMM_GFX_VISIBLE ) ) continue;
        const auto &p = positions[i]; auto &d = details[i];
        const ter_t &ter = here.ter( p ).obj(); const furn_t &furn = here.furn( p ).obj();
        d.terrain_shape = ncmm_graphics_shapes::terrain( ter.id.str() + " " + ter.looks_like, graphics_traits( ter, !ter.close.is_null() ) );
        d.terrain_transparent = ter.transparent;
        d.furniture_shape = ncmm_graphics_shapes::furniture( furn.id.str() + " " + furn.looks_like, graphics_traits( furn, !furn.close.is_null() ) );
        ++graphics_hybrid_shape_counts[d.terrain_shape];
        if( here.has_furn( p ) ) ++graphics_hybrid_shape_counts[d.furniture_shape];
        // Axis of a connected wall/fence; only visible neighbor flags influence it.
        const int ix = int( i ) % base.grid_width, iy = int( i ) / base.grid_width;
        auto structural = [&]( int nx, int ny ) {
            if( nx < 0 || ny < 0 || nx >= base.grid_width || ny >= base.grid_height ) return false;
            const auto &c = base.cells[size_t( ny ) * base.grid_width + nx];
            return ( c.flags & NCMM_GFX_VISIBLE ) && ( ( c.flags & NCMM_GFX_WALL ) ||
                ncmm_graphics_shapes::contains( c.terrain, "window" ) || ncmm_graphics_shapes::contains( c.terrain, "fence" ) );
        };
        d.orientation = structural( ix, iy - 1 ) || structural( ix, iy + 1 ) ? 1u : 0u;
        if( !ncmm_graphics_shapes::vertical( d.terrain_shape ) ) copy_id( d.ground, ter.id.str() );
        else copy_id( d.ground, d.terrain_shape == NCMM_SHAPE_TREE || d.terrain_shape == NCMM_SHAPE_SHRUB || d.terrain_shape == NCMM_SHAPE_PLANT ? "t_dirt" : "t_floor" );
        const float raw = std::max( 0.0f, here.ambient_light_at( p ) );
        const float level = std::clamp( 0.025f + std::sqrt( raw / ( raw + 24.0f ) ), 0.0f, 1.0f );
        const auto &color = here.get_cache_ref( p.z() ).light_color_cache[p.x()][p.y()];
        const float maximum = std::max( { color.r, color.g, color.b, 0.0f } );
        const float tint = std::clamp( maximum / std::max( raw, 1.0f ), 0.0f, 0.9f );
        auto channel = [&]( float c ) { return level * ( 1 - tint + ( maximum > 0 ? tint * c / maximum : 0 ) ); };
        d.light_r = channel( color.r ); d.light_g = channel( color.g ); d.light_b = channel( color.b );
        if( std::max( { d.light_r, d.light_g, d.light_b } ) - std::min( { d.light_r, d.light_g, d.light_b } ) > .05f ) ++graphics_hybrid_colored_cells;
        d.emission = float( std::max( ter.light_emitted, furn.light_emitted ) );
        if( maximum > 0 ) ++graphics_hybrid_light_cells;
        const auto field = here.field_at( p ).displayed_field_type();
        if( field.id() != field_type_str_id( "fd_null" ) ) {
            copy_id( d.field, field.id().str() );
            const std::string fid = field.id().str();
            d.field_shape = ncmm_graphics_shapes::contains( fid, "fire" ) || ncmm_graphics_shapes::contains( fid, "smoke" ) ||
                ncmm_graphics_shapes::contains( fid, "gas" ) || ncmm_graphics_shapes::contains( fid, "plasma" ) ? NCMM_SHAPE_BILLBOARD : NCMM_SHAPE_FLAT;
        }
        if( const optional_vpart_position vp = here.veh_at( p ) ) {
            const auto displayed = vp->part_displayed();
            if( displayed ) {
                vehicle &v = vp->vehicle();
                auto group = groups.emplace( &v, uint32_t( groups.size() + 1 ) ).first;
                const auto &part = displayed->part();
                d.vehicle_group = group->second; d.mount_x = part.mount.x(); d.mount_y = part.mount.y();
                d.vehicle_yaw = float( units::to_radians( v.face.dir() ) );
                const float co = std::cos( d.vehicle_yaw ), si = std::sin( d.vehicle_yaw );
                // Derive the continuous anchor from this visible mount, without
                // enumerating or revealing parts in unseen cells.
                auto anchor = vehicle_anchors.emplace( &v, std::make_pair(
                    ix + .5f - d.mount_x * co + d.mount_y * si,
                    iy + .5f - d.mount_x * si - d.mount_y * co ) ).first;
                d.vehicle_x = anchor->second.first; d.vehicle_y = anchor->second.second;
                d.vehicle_health = float( part.health_percent() );
                const std::pair<const char *, uint32_t> features[] = {
                    { "STRUCTURE", NCMM_VEH_FRAME }, { "ROOF", NCMM_VEH_ROOF }, { "BOARDABLE", NCMM_VEH_SEAT },
                    { "OBSTACLE", NCMM_VEH_BOARD }, { "WINDOW", NCMM_VEH_WINDOW }, { "OPENABLE", NCMM_VEH_DOOR },
                    { "WHEEL", NCMM_VEH_WHEEL }, { "CARGO", NCMM_VEH_CARGO }, { "ENGINE", NCMM_VEH_ENGINE }, { "CONE_LIGHT", NCMM_VEH_LIGHT }
                };
                for( const auto &f : features ) if( vp->part_with_feature( f.first, false ) ) d.vehicle_features |= f.second;
                if( !d.vehicle_features ) d.vehicle_features = NCMM_VEH_FRAME;
                if( const auto door = vp->part_with_feature( "OPENABLE", false ) ) {
                    if( door->part().open ) d.vehicle_features |= NCMM_VEH_OPEN;
                    if( door->part().is_broken() ) d.vehicle_features |= NCMM_VEH_BROKEN | NCMM_VEH_OPEN;
                }
                if( part.is_broken() ) d.vehicle_features |= NCMM_VEH_BROKEN;
                ++graphics_hybrid_vehicle_cells;
            }
        }
    }
    // Separate the substrate from vertical terrain. Choose a matching visible
    // neighboring floor, never querying an unseen tile's ID for this purpose.
    for( size_t i = 0; i < base.cell_count; ++i ) {
        if( !( base.cells[i].flags & NCMM_GFX_VISIBLE ) || !ncmm_graphics_shapes::vertical( details[i].terrain_shape ) ) continue;
        const int gx = int( i ) % base.grid_width, gy = int( i ) / base.grid_width;
        for( const point delta : { point( 0, 1 ), point( 0, -1 ), point( 1, 0 ), point( -1, 0 ) } ) {
            const int nx = gx + delta.x, ny = gy + delta.y;
            if( nx < 0 || ny < 0 || nx >= base.grid_width || ny >= base.grid_height ) continue;
            const size_t j = size_t( ny ) * base.grid_width + nx;
            if( !( base.cells[j].flags & NCMM_GFX_VISIBLE ) || ncmm_graphics_shapes::vertical( details[j].terrain_shape ) ||
                ( base.cells[j].flags & NCMM_GFX_NO_FLOOR ) ) continue;
            const uint32_t shape = details[i].terrain_shape;
            const bool plant = shape == NCMM_SHAPE_TREE || shape == NCMM_SHAPE_SHRUB || shape == NCMM_SHAPE_PLANT;
            const std::string ground = base.cells[j].terrain;
            if( plant && !ncmm_graphics_shapes::contains( ground, "dirt" ) && !ncmm_graphics_shapes::contains( ground, "grass" ) &&
                !ncmm_graphics_shapes::contains( ground, "sand" ) && !ncmm_graphics_shapes::contains( ground, "water" ) &&
                !ncmm_graphics_shapes::contains( ground, "fungus" ) ) continue;
            std::fill( std::begin( details[i].ground ), std::end( details[i].ground ), '\0' );
            copy_id( details[i].ground, base.cells[j].terrain ); break;
        }
    }
    ncmm_graphics_environment_v2 env = {};
    const auto sun = sun_azimuth_altitude( calendar::turn );
    auto direction = []( const std::pair<units::angle, units::angle> &angles ) {
        const float a = float( units::to_radians( angles.first ) ), h = float( units::to_radians( angles.second ) );
        return std::array<float, 3>{ -std::sin( a ) * std::cos( h ), std::cos( a ) * std::cos( h ), std::sin( h ) };
    };
    const auto sd = direction( sun ); env.sun_x = sd[0]; env.sun_y = sd[1]; env.sun_z = sd[2];
    env.moon_phase = uint32_t( get_moon_phase( calendar::turn ) );
    // CDDA exposes phases but no lunar orbit. Use its solar astronomy shifted
    // by phase: full moon rises near sunset; first quarter peaks in the evening.
    const auto md = direction( sun_azimuth_altitude( calendar::turn - int( env.moon_phase ) * 3_hours ) );
    env.moon_x = md[0]; env.moon_y = md[1]; env.moon_z = md[2];
    const weather_manager &weather = get_weather();
    const auto &type = weather.weather_id.obj(); copy_id( env.weather_id, type.id.str() );
    env.daylight = std::clamp( ( sun_light_at( calendar::turn ) * type.light_multiplier + type.light_modifier ) / default_daylight_level(), 0.0f, 1.0f );
    env.sky_r = std::clamp( type.tint_color.r, 0.0f, 1.0f ); env.sky_g = std::clamp( type.tint_color.g, 0.0f, 1.0f );
    env.sky_b = std::clamp( type.tint_color.b, 0.0f, 1.0f ); env.sky_tint = std::clamp( type.tint_strength, 0.0f, 1.0f );
    env.cloud_cover = std::clamp( 1.0f - type.sun_multiplier, .06f, 1.0f );
    env.precipitation = type.precip == precip_class::heavy ? .95f : type.precip == precip_class::light ? .45f : type.precip == precip_class::very_light ? .17f : 0;
    env.weather_kind = env.precipitation > 0 ? ( type.rains ? NCMM_WEATHER_RAIN : NCMM_WEATHER_SNOW ) : NCMM_WEATHER_DRY;
    const std::string wid = type.id.str();
    if( ncmm_graphics_shapes::contains( wid, "acid" ) ) env.weather_kind = NCMM_WEATHER_ACID;
    if( ncmm_graphics_shapes::contains( wid, "sleet" ) ) env.weather_kind = NCMM_WEATHER_SLEET;
    if( ncmm_graphics_shapes::contains( wid, "sand" ) || ncmm_graphics_shapes::contains( wid, "dust" ) ) { env.weather_kind = NCMM_WEATHER_DUST; env.precipitation = .65f; }
    if( type.sound_category == weather_sound_category::portal_storm ) { env.weather_kind = NCMM_WEATHER_PORTAL; env.precipitation = .7f; }
    env.fog_density = std::clamp( type.sight_penalty * .025f + env.precipitation * .012f, 0.0f, .2f );
    if( ncmm_graphics_shapes::contains( wid, "fog" ) || ncmm_graphics_shapes::contains( wid, "mist" ) ) env.fog_density = std::max( env.fog_density, .055f );
    if( env.precipitation > 0 ) env.cloud_cover = std::max( env.cloud_cover, .65f );
    const auto wind = convert_wind_to_coord( weather.winddirection );
    env.wind_x = float( wind.x * std::clamp( weather.windspeed, 0, 100 ) ); env.wind_y = float( wind.y * std::clamp( weather.windspeed, 0, 100 ) );
    env.lightning = weather.lightning_active; env.outdoors = here.is_outside( you.pos_bub() );
    env.animation_seconds = SDL_GetTicks() / 1000.0;
    if( const auto held = you.get_wielded_item() ) {
        copy_id( env.held_item, held->typeId().str() ); env.held_kind = held->is_gun() ? 1u : held->is_melee() ? 2u : 3u;
    }
    graphics_hybrid_last_environment = env;
    const ncmm_graphics_scene_v2 scene = { sizeof( ncmm_graphics_scene_v2 ), base, details.data(), details.size(), env };
    const uint64_t epoch = tilecontext->ncmm_graphics_tileset_epoch();
    SDL_Renderer *renderer = get_sdl_renderer().get();
    if( epoch != graphics_hybrid_tileset_epoch || renderer != graphics_hybrid_renderer_identity || graphics_hybrid_sprite_bytes > 48 * 1024 * 1024 ) {
        graphics_cpu_sprites.clear(); graphics_hybrid_sprite_bytes = 0;
        graphics_hybrid_tileset_epoch = epoch; graphics_hybrid_renderer_identity = renderer;
    }
    graphics_hybrid_pixels.clear(); graphics_hybrid_width = graphics_hybrid_height = 0;
    graphics_resolved_textures = graphics_resolved_vehicle_textures = 0;
    graphics_frame_invalid = false; graphics_hybrid_frame = &scene; graphics_hybrid_positions = &positions;
    graphics_render_active = true;
    bool ok = false;
    try {
        module_call_scope scope( graphics_view.owner.c_str() );
        ok = graphics_view.hybrid_render( &scene, graphics_view.user_data ) != 0;
    } catch( ... ) {
        loaded_mod *mod = find_loaded_by_id( graphics_view.owner.c_str() );
        if( mod ) quarantine_runtime_callback( *mod, runtime_callback_kind::ui, "graphics_hybrid_exception" );
    }
    graphics_hybrid_frame = nullptr; graphics_hybrid_positions = nullptr;
    graphics_render_active = false;
    if( !ok || graphics_frame_invalid || graphics_hybrid_pixels.empty() ) {
        graphics_view.enabled = false; graphics_hybrid_pixels.clear();
        add_msg( m_warning, "%s", tr_ui( "First person could not draw this scene; returned to the map. F6 retries.",
            "Не удалось отрисовать вид от первого лица; включена карта. F6 — повторить." ) );
        return false;
    }
    SDL_Rect prior_clip; SDL_GetRenderClipRect( renderer, &prior_clip ); const bool clipped = SDL_RenderClipEnabled( renderer );
    const SDL_Rect clip = { x, y, base.pixel_width, base.pixel_height };
    SDL_Texture *frame = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        int( graphics_hybrid_width ), int( graphics_hybrid_height ) );
    if( !frame ) return false;
    ok = SDL_UpdateTexture( frame, nullptr, graphics_hybrid_pixels.data(), int( graphics_hybrid_width * 4 ) );
    ok = SDL_SetTextureBlendMode( frame, SDL_BLENDMODE_NONE ) && ok;
    ok = SDL_SetTextureScaleMode( frame, SDL_SCALEMODE_LINEAR ) && ok;
    ok = SDL_SetRenderClipRect( renderer, &clip ) && ok;
    const SDL_FRect destination = { float( x ), float( y ), float( base.pixel_width ), float( base.pixel_height ) };
    ok = SDL_RenderTexture( renderer, frame, nullptr, &destination ) && ok;
    ok = SDL_SetRenderClipRect( renderer, clipped ? &prior_clip : nullptr ) && ok;
    SDL_DestroyTexture( frame );
    if( !ok ) graphics_view.enabled = false;
    return ok;
}
#else
bool graphics_draw_hybrid( const ncmm_graphics_scene_v1 &, const std::vector<tripoint_bub_ms> &, int, int ) { return false; }
#endif
