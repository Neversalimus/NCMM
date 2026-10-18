// Callback-scoped CPU scene bridge for the hybrid renderer. No game/GPU handle
// is exposed to a module; submitted pixels are validated and copied atomically.
const ncmm_graphics_scene_v2 *graphics_hybrid_frame = nullptr;
const ncmm_graphics_multilevel_scene_v1 *graphics_multilevel_frame = nullptr;
void graphics_world_extracted(double);
const std::vector<tripoint_bub_ms> *graphics_hybrid_positions = nullptr;
std::vector<uint32_t> graphics_hybrid_pixels;
uint32_t graphics_hybrid_width = 0, graphics_hybrid_height = 0;
size_t graphics_hybrid_sprite_bytes = 0;
// Existing opt-in native benchmark only; no GPU ABI or gameplay timer changes.
struct graphics_sprite_readback_profile {bool enabled=false;uint64_t attempts=0;double ms=0;} graphics_sprite_readbacks;
uint64_t graphics_hybrid_tileset_epoch = 0;
size_t graphics_hybrid_vehicle_cells = 0;
size_t graphics_hybrid_light_cells = 0;
size_t graphics_hybrid_colored_cells = 0;
size_t graphics_hybrid_held_sprites = 0;
size_t graphics_hybrid_tiny_creatures = 0;
std::array<size_t, NCMM_SHAPE_COUNT> graphics_hybrid_shape_counts = {};
ncmm_graphics_environment_v2 graphics_hybrid_last_environment = {};
// Only the isolated qualification fixture assigns an override. Gameplay uses
// the monotonic SDL presentation clock independently of simulation turns.
std::optional<double> graphics_smoke_seconds;
bool graphics_smoke_gpu_gallery = false;
double graphics_presentation_seconds()
{
#if defined(TILES)
    return graphics_smoke_seconds ? *graphics_smoke_seconds : SDL_GetTicks() / 1000.0;
#else
    return 0;
#endif
}
#include "ncmm_graphics_presentation.inc"
#include "ncmm_graphics_creatures.inc"
#include "ncmm_graphics_gpu.inc"

#if defined(TILES)
struct graphics_cpu_sprite {
    uint32_t width = 0, height = 0;
    std::vector<uint32_t> pixels;
};
using graphics_sprite_key = std::tuple<uintptr_t, int, int, int, int>;
std::map<graphics_sprite_key, graphics_cpu_sprite> graphics_cpu_sprites;
std::map<uint64_t, graphics_cpu_sprite> graphics_creature_composites;
SDL_Renderer *graphics_hybrid_renderer_identity = nullptr; // comparison only
SDL_Texture *graphics_stream_texture = nullptr;
uint64_t graphics_stream_instance = 0, graphics_stream_generation = 0;
uint32_t graphics_stream_width = 0, graphics_stream_height = 0;
void graphics_clear_effect_texture();
void graphics_clear_stream()
{
    // SDL destroys all child textures when replacing a renderer. The monotonic
    // instance generation prevents destroying a dangling, reused address.
    if( graphics_stream_texture && graphics_stream_instance == renderer_coordinator.instance_generation() ) SDL_DestroyTexture( graphics_stream_texture );
    graphics_stream_texture = nullptr; graphics_stream_width = graphics_stream_height = 0;
    graphics_clear_effect_texture();
}

#include "ncmm_graphics_effects.inc"

bool graphics_read_sprite( const texture_draw_data &atlas, graphics_cpu_sprite &out )
{
    SDL_Renderer *renderer = get_sdl_renderer().get();
    const SDL_Rect rect = atlas.dimensions;
    if( !renderer || !atlas.texture || rect.w < 1 || rect.h < 1 || rect.w > 512 || rect.h > 512 ) return false;
    struct readback_timer {
        bool enabled;std::chrono::steady_clock::time_point begun;
        ~readback_timer(){if(enabled){++graphics_sprite_readbacks.attempts;graphics_sprite_readbacks.ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();}}
    } timing{graphics_sprite_readbacks.enabled,graphics_sprite_readbacks.enabled?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{}};
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
    SDL_ScaleMode sampling = SDL_SCALEMODE_NEAREST;
    if( !SDL_GetTextureScaleMode( atlas.texture, &sampling ) ) return false;
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
    ok = SDL_SetTextureScaleMode( atlas.texture, SDL_SCALEMODE_NEAREST ) && ok;
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
    ok = SDL_SetTextureScaleMode( atlas.texture, sampling ) && ok;
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
    graphics_clear_world();
    graphics_clear_gpu();
    graphics_clear_creatures();
    graphics_hybrid_frame = nullptr; graphics_hybrid_positions = nullptr;
    graphics_hybrid_pixels.clear(); graphics_hybrid_width = graphics_hybrid_height = 0;
    graphics_presentation_cells.clear(); graphics_effect_pixels.clear(); graphics_aim.reset(); graphics_aim_name.clear();
    graphics_hybrid_sprite_bytes = 0; graphics_hybrid_tileset_epoch = 0;
#if defined(TILES)
    graphics_clear_stream();
    graphics_cpu_sprites.clear(); graphics_creature_composites.clear(); graphics_hybrid_renderer_identity = nullptr;
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
        !out || out->struct_size < sizeof( ncmm_graphics_sprite_v2 ) || layer > NCMM_GFX_GLOVE_RIGHT || layer == NCMM_GFX_SOLID ) return 0;
    const auto &scene = *graphics_hybrid_frame;
    std::string id;
    TILE_CATEGORY category = TILE_CATEGORY::TERRAIN;
    tripoint_bub_ms position = get_avatar().pos_bub();
    if( layer >= NCMM_GFX_SLEEVE_LEFT ) {
        if( index != UINT32_MAX ) return 0;
        const uint32_t slot=layer-NCMM_GFX_SLEEVE_LEFT;
        id=slot<2 ? graphics_presentation.sleeve[slot] : graphics_presentation.glove[slot-2];
        category=TILE_CATEGORY::ITEM;
    } else if( layer == NCMM_GFX_HELD_ITEM ) {
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
    if(layer==NCMM_GFX_CREATURE&&(id=="npc_male"||id=="npc_female"))category=TILE_CATEGORY::NONE;
    std::string physical_id = id;
    if( layer == NCMM_GFX_HELD_ITEM ) {
        for( int hop = 0; hop < 8; ++hop ) {
            const itype_id type( physical_id );
            if( !type.is_valid() ) break;
            const std::string next = ncmm_graphics_shapes::physical_item_id(
                physical_id, type->looks_like.str(), scene.environment.held_kind );
            if( next == physical_id ) break;
            physical_id = next;
        }
    }
    auto atlas = tilecontext->ncmm_get_texture_draw_data( physical_id, category, position, layer == NCMM_GFX_GROUND, layer == NCMM_GFX_CREATURE );
    if( !atlas && physical_id != id ) atlas = tilecontext->ncmm_get_texture_draw_data( id, category, position, false );
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
    if( layer == NCMM_GFX_CREATURE && index < scene.base.cell_count ) {
        const Creature *cr = get_creature_tracker().creature_at<Creature>( position );
        const Character *ch = cr && cr->is_npc() ? cr->as_character() : nullptr;
        if( ch && get_avatar().sees( get_map(), *cr ) ) {
            uint64_t revision = 1469598103934665603ull;
            for( uint32_t pixel : sprite.pixels ) revision = ( revision ^ pixel ) * 1099511628211ull;
            revision = graphics_creature_overlay_revision( *ch, revision );
            auto composed = graphics_creature_composites.find( revision );
            if( composed == graphics_creature_composites.end() && graphics_creature_composites.size() < 128 ) {
                graphics_cpu_sprite result = sprite;
                for( const auto &overlay : ch->get_overlay_ids() ) {
                    if( overlay.first.compare( 0, 5, "worn_" ) && overlay.first.compare( 0, 9, "mutation_" ) ) continue;
                    std::string draw_id;
                    if( !tilecontext->ncmm_find_overlay( ch->male, overlay.first, overlay.second, draw_id ) ) continue;
                    auto source = tilecontext->ncmm_get_texture_draw_data( draw_id, TILE_CATEGORY::NONE, position, false, true );
                    if( !source ) continue;
                    const auto &rect = source->dimensions;
                    const graphics_sprite_key overlay_key{ reinterpret_cast<uintptr_t>( source->texture ), rect.x, rect.y, rect.w, rect.h };
                    auto layer = graphics_cpu_sprites.find( overlay_key );
                    if( layer == graphics_cpu_sprites.end() ) {
                        if( rect.w < 1 || rect.h < 1 || rect.w > 512 || rect.h > 512 || graphics_cpu_sprites.size() >= 1024 ||
                            graphics_hybrid_sprite_bytes + size_t( rect.w ) * rect.h * 4 > 64 * 1024 * 1024 ) continue;
                        graphics_cpu_sprite pixels;
                        if( !graphics_read_sprite( *source, pixels ) ) continue;
                        graphics_hybrid_sprite_bytes += pixels.pixels.size() * 4;
                        layer = graphics_cpu_sprites.emplace( overlay_key, std::move( pixels ) ).first;
                    }
                    const auto &layer_pixels = layer->second;
                    for( uint32_t y = 0; y < result.height; ++y ) for( uint32_t x = 0; x < result.width; ++x ) {
                        const uint32_t pixel = layer_pixels.pixels[size_t( y * layer_pixels.height / result.height ) * layer_pixels.width + x * layer_pixels.width / result.width];
                        const uint32_t alpha = pixel & 255u; if( !alpha ) continue;
                        uint32_t &dst = result.pixels[size_t( y ) * result.width + x];
                        const uint32_t out_alpha = alpha + ( ( dst & 255u ) * ( 255u - alpha ) + 127u ) / 255u;
                        uint32_t color = out_alpha;
                        for( int shift : { 8, 16, 24 } ) {
                            const uint32_t premult = ( ( pixel >> shift ) & 255u ) * alpha + ( ( ( dst >> shift ) & 255u ) * ( dst & 255u ) * ( 255u - alpha ) + 127u ) / 255u;
                            color |= ( ( premult + out_alpha / 2 ) / out_alpha ) << shift;
                        }
                        dst = color;
                    }
                }
                if( graphics_hybrid_sprite_bytes + result.pixels.size() * 4 <= 64 * 1024 * 1024 ) {
                    graphics_hybrid_sprite_bytes += result.pixels.size() * 4;
                    composed = graphics_creature_composites.emplace( revision, std::move( result ) ).first;
                }
            }
            if( composed != graphics_creature_composites.end() ) {
                const auto &pixels = composed->second;
                *out = { sizeof( *out ), pixels.width, pixels.height, pixels.width, pixels.pixels.data() };
            }
        }
    }
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
#if defined(TILES)
    if( graphics_gpu_engine ) graphics_gpu_engine->cpu_frame();
#endif
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
    out.transparent = t.transparent; out.impassable = t.movecost == 0;
    // A closed domestic window also has close=curtains. Only a passable
    // window opening is open geometry; the curtain action is not a hole.
    out.open = opened && (!out.window || t.movecost > 0);
    out.bridge = t.has_flag( "BRIDGE" );
    return out;
}

// Physical terrain adjacency is independent of sparse snapshot/FOV membership.
// The four bits also retain corners and T junctions which a single axis loses.
uint32_t graphics_structure_connections(const tripoint_bub_ms &p,uint32_t shape,uint32_t &orientation)
{
    switch(shape){
        case NCMM_SHAPE_DOOR_CLOSED:case NCMM_SHAPE_DOOR_OPEN:case NCMM_SHAPE_DOOR_FRAME:
        case NCMM_SHAPE_WINDOW:case NCMM_SHAPE_WINDOW_BROKEN:case NCMM_SHAPE_WINDOW_COVERED:
        case NCMM_SHAPE_FENCE:case NCMM_SHAPE_GATE_OPEN:case NCMM_SHAPE_GLASS_WALL:
        case NCMM_SHAPE_GUARDRAIL:case NCMM_SHAPE_SCRAP_FENCE:case NCMM_SHAPE_RESIN_PORTAL:break;
        default:return 0; // Floors, air, cubes and plants have no oriented joints.
    }
    uint32_t connections=0;int horizontal=0,vertical=0;
    for(unsigned side=0;side<4;++side){
        const int dx=side==0?1:side==2?-1:0,dy=side==1?1:side==3?-1:0;
        const tripoint_bub_ms next(p.x()+dx,p.y()+dy,p.z());
        if(!get_map().ncmm_graphics_loaded(next))continue;
        const auto &ter=get_map().ter(next).obj();
        const auto other=ncmm_graphics_shapes::terrain(ter.id.str()+" "+ter.looks_like,graphics_traits(ter,!ter.close.is_null()));
        const bool connected=ncmm_graphics_shapes::fence_shape(shape)?
            (ncmm_graphics_shapes::fence_shape(other)||other==NCMM_SHAPE_WALL):
            ncmm_graphics_shapes::structural_surface(other);
        if(!connected)continue;
        connections|=NCMM_SURFACE_CONNECT_E<<side;
        if(dx)++horizontal;else ++vertical;
    }
    orientation=vertical>horizontal?1u:0u;
    return connections|NCMM_SURFACE_CONNECTIONS;
}

bool graphics_draw_hybrid( const ncmm_graphics_scene_v1 &base, const std::vector<tripoint_bub_ms> &positions, int x, int y,
                           ncmm_graphics_multilevel_scene_v1 *world=nullptr,const std::vector<std::array<float,3>> *facade_lights=nullptr )
{
    ncmm_graphics_trace::cpu_zone snapshot_zone("Snapshot");
    avatar &you = get_avatar(); map &here = get_map();
    std::vector<ncmm_graphics_cell_v2> details( base.cell_count );
    std::map<const vehicle *, uint32_t> groups;
    std::map<const vehicle *, std::pair<float, float>> vehicle_anchors;
    const ncmm_world::coordinates world_coordinates(world,true);
    ncmm_world::definition_cache terrain_shapes,furniture_shapes;
    auto terrain_shape=[&](const ter_t &definition){return terrain_shapes.get(reinterpret_cast<std::uintptr_t>(&definition),[&]{
        return uint32_t(ncmm_graphics_shapes::terrain(definition.id.str()+" "+definition.looks_like,graphics_traits(definition,!definition.close.is_null())));});};
    auto furniture_shape=[&](const furn_t &definition){return furniture_shapes.get(reinterpret_cast<std::uintptr_t>(&definition),[&]{
        return uint32_t(ncmm_graphics_shapes::furniture(definition.id.str()+" "+definition.looks_like,graphics_traits(definition,!definition.close.is_null())));});};
    auto copy_id = []( char ( &dest )[128], const std::string &id ) { if( id.size() < sizeof( dest ) ) std::copy( id.begin(), id.end(), dest ); };
    graphics_hybrid_vehicle_cells = graphics_hybrid_light_cells = graphics_hybrid_held_sprites = 0;
    graphics_hybrid_colored_cells = graphics_hybrid_tiny_creatures = 0;
    graphics_hybrid_shape_counts.fill( 0 );
    for( size_t i = 0; i < base.cell_count; ++i ) {
        if( !( base.cells[i].flags & NCMM_GFX_VISIBLE ) ) continue;
        const auto &p = positions[i]; auto &d = details[i];
        auto *owned=world?graphics_camera_cache.find(world->coordinates[i].x,world->coordinates[i].y,world->coordinates[i].z):nullptr;
        if(owned&&owned->detail_ready&&!graphics_camera_cache.verify){
            d=owned->detail;++graphics_camera_cache.totals.detail_hits;
            if(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE){
                ++graphics_hybrid_shape_counts[d.terrain_shape];
                if(base.cells[i].furniture[0])++graphics_hybrid_shape_counts[d.furniture_shape];
                if(owned->has_source_light)++graphics_hybrid_light_cells;
                if(std::max({d.light_r,d.light_g,d.light_b})-std::min({d.light_r,d.light_g,d.light_b})>.05f)++graphics_hybrid_colored_cells;
            }
            continue;
        }
        struct store_detail {
            ncmm_graphics_snapshot::entry *owned;ncmm_graphics_snapshot::cache &cache;ncmm_graphics_cell_v2 &value;
            ~store_detail(){if(!owned)return;if(owned->detail_ready&&cache.verify){++cache.totals.verification_checks;if(std::memcmp(&owned->detail,&value,sizeof(value)))++cache.totals.verification_mismatches;}
                owned->detail=value;owned->detail_ready=true;}
        } save_detail{owned,graphics_camera_cache,d};
        const ter_t &ter = here.ter( p ).obj(); const furn_t &furn = here.furn( p ).obj();
        if(world&&!(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE)){
            d.terrain_shape=d.furniture_shape=NCMM_SHAPE_FLAT;
            // A neutral structural surface carries no hidden room texture.
            // The shared slab collector uses its untextured fallback material.
            const auto top_rgb=graphics_sample_rgb(p);const auto &lower=world->coordinates[i].lower_light;
            const bool surface=(world->coordinates[i].flags&(NCMM_WORLD_FLOOR_TOP|NCMM_WORLD_STRUCTURE_VISIBLE))!=0;
            d.light_r=surface?top_rgb[0]:lower[0];d.light_g=surface?top_rgb[1]:lower[1];d.light_b=surface?top_rgb[2]:lower[2];
            if(facade_lights&&(world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE)){const auto &rgb=(*facade_lights)[i];d.light_r=rgb[0];d.light_g=rgb[1];d.light_b=rgb[2];}
            if(world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE){
                d.terrain_shape=terrain_shape(ter);
                // Opaque furniture/gas in this voxel is not an opaque window
                // panel. The terrain itself owns its opening and cover state.
                d.terrain_flags=ter.transparent?uint32_t(NCMM_SURFACE_TRANSPARENT):0u;
                d.terrain_flags|=graphics_structure_connections(p,d.terrain_shape,d.orientation);
                copy_id(d.ground,"t_floor");
            }
            continue;
        }
        d.terrain_shape = terrain_shape(ter);
        if(world&&(world->coordinates[i].flags&NCMM_WORLD_RAMP))d.terrain_shape=NCMM_SHAPE_FLAT;
        d.terrain_flags = ter.transparent ? NCMM_SURFACE_TRANSPARENT : 0u;
        // Vehicle interiors also make outside_cache false. They are not
        // building ceilings; only actual terrain above confirms that surface.
        if( p.z() < OVERMAP_HEIGHT && !here.ter( tripoint_bub_ms( p.x(), p.y(), p.z() + 1 ) ).obj().has_flag( "NO_FLOOR" ) ) {
            d.terrain_flags |= NCMM_SURFACE_TERRAIN_CEILING;
        }
        d.furniture_shape = furniture_shape(furn);
        ++graphics_hybrid_shape_counts[d.terrain_shape];
        if( here.has_furn( p ) ) ++graphics_hybrid_shape_counts[d.furniture_shape];
        d.terrain_flags|=graphics_structure_connections(p,d.terrain_shape,d.orientation);
        if( !ncmm_graphics_shapes::vertical( d.terrain_shape ) ) copy_id( d.ground, ter.id.str() );
        else copy_id( d.ground, d.terrain_shape == NCMM_SHAPE_TREE || d.terrain_shape == NCMM_SHAPE_SHRUB || d.terrain_shape == NCMM_SHAPE_PLANT ? "t_dirt" : "t_floor" );
        const float raw = std::max( 0.0f, here.ambient_light_at( p ) );
        const float level = std::clamp( 0.025f + std::sqrt( raw / ( raw + 24.0f ) ), 0.0f, 1.0f );
        const auto &color = here.get_cache_ref( p.z() ).light_color_cache[p.x()][p.y()];
        const float maximum = std::max( { color.r, color.g, color.b, 0.0f } );
        const float tint = std::clamp( maximum / std::max( raw, 1.0f ), 0.0f, 0.9f );
        auto channel = [&]( float c ) { return level * ( 1 - tint + ( maximum > 0 ? tint * c / maximum : 0 ) ); };
        d.light_r = channel( color.r ); d.light_g = channel( color.g ); d.light_b = channel( color.b );
        if(world&&facade_lights&&(world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE)&&!ter.transparent){const auto &rgb=(*facade_lights)[i];d.light_r=rgb[0];d.light_g=rgb[1];d.light_b=rgb[2];}
        if( std::max( { d.light_r, d.light_g, d.light_b } ) - std::min( { d.light_r, d.light_g, d.light_b } ) > .05f ) ++graphics_hybrid_colored_cells;
        d.emission = float( std::max( ter.light_emitted, furn.light_emitted ) );
        if( maximum > 0 ) ++graphics_hybrid_light_cells;
        if(owned)owned->has_source_light=maximum>0;
        if( base.cells[i].creature[0] ) {
            const monster *mon = get_creature_tracker().creature_at<monster>( p );
            if( mon && you.sees( here, *mon ) ) {
                switch( mon->get_size() ) {
                    case creature_size::tiny: d.terrain_flags |= NCMM_CREATURE_TINY; ++graphics_hybrid_tiny_creatures; break;
                    case creature_size::small: d.terrain_flags |= NCMM_CREATURE_SMALL; break;
                    case creature_size::large: d.terrain_flags |= NCMM_CREATURE_LARGE; break;
                    case creature_size::huge: d.terrain_flags |= NCMM_CREATURE_HUGE; break;
                    default: break;
                }
            }
        }
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
                const int ix=world?int(int64_t(world->coordinates[i].x)-world->origin_x):int(i)%base.grid_width,
                          iy=world?int(int64_t(world->coordinates[i].y)-world->origin_y):int(i)/base.grid_width;
                // Derive the continuous anchor from this visible mount, without
                // enumerating or revealing parts in unseen cells.
                auto anchor = vehicle_anchors.emplace( &v, std::make_pair(
                    ix + .5f - d.mount_x * co + d.mount_y * si,
                    iy + .5f - d.mount_x * si - d.mount_y * co ) ).first;
                d.vehicle_x = anchor->second.first; d.vehicle_y = anchor->second.second;
                d.vehicle_health = float( part.health_percent() );
                const std::pair<const char *, uint32_t> features[] = {
                    { "STRUCTURE", NCMM_VEH_FRAME }, { "ROOF", NCMM_VEH_ROOF }, { "SEAT", NCMM_VEH_SEAT },
                    { "OBSTACLE", NCMM_VEH_BOARD }, { "WINDOW", NCMM_VEH_WINDOW }, { "OPENABLE", NCMM_VEH_DOOR },
                    { "WHEEL", NCMM_VEH_WHEEL }, { "CARGO", NCMM_VEH_CARGO }, { "ENGINE", NCMM_VEH_ENGINE }, { "CONE_LIGHT", NCMM_VEH_LIGHT }, { "CONTROLS", NCMM_VEH_CONTROLS }
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
        const int gx=world?int(int64_t(world->coordinates[i].x)-world->origin_x):int(i)%base.grid_width,
                  gy=world?int(int64_t(world->coordinates[i].y)-world->origin_y):int(i)/base.grid_width;
        for( const point delta : { point( 0, 1 ), point( 0, -1 ), point( 1, 0 ), point( -1, 0 ) } ) {
            const int nx = gx + delta.x, ny = gy + delta.y;
            if( nx < 0 || ny < 0 || nx >= base.grid_width || ny >= base.grid_height ) continue;
            const int other=world?world_coordinates.find(nx,ny,positions[i].z()):ny*base.grid_width+nx;if(other<0)continue;
            const size_t j=size_t(other);
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
    env.fog_density = ncmm_graphics_policy::weather_fog_density( type.sight_penalty, env.precipitation,
        ncmm_graphics_shapes::contains( wid, "fog" ) || ncmm_graphics_shapes::contains( wid, "mist" ) );
    if( env.precipitation > 0 ) env.cloud_cover = std::max( env.cloud_cover, .65f );
    const auto wind = convert_wind_to_coord( weather.winddirection );
    env.wind_x = float( wind.x * std::clamp( weather.windspeed, 0, 100 ) ); env.wind_y = float( wind.y * std::clamp( weather.windspeed, 0, 100 ) );
    env.lightning = weather.lightning_active; env.outdoors = here.is_outside( you.pos_bub() );
    env.animation_seconds = graphics_presentation_seconds();
    if( const auto held = you.get_wielded_item() ) {
        copy_id( env.held_item, held->typeId().str() ); env.held_kind = held->is_gun() ? 1u : held->is_melee() ? 2u : 3u;
    }
    graphics_hybrid_last_environment = env;
    graphics_prepare_held();
    graphics_prepare_presentation( base,positions,details,world );
    if(graphics_camera_cache.verify&&graphics_camera_cache.totals.verification_mismatches){
        log_line(NCMM_LOG_ERROR,"NCMM camera snapshot differential mismatch");
        return false;
    }
    const ncmm_graphics_scene_v2 scene = { sizeof( ncmm_graphics_scene_v2 ), base, details.data(), details.size(), env };
    if(world)world->appearance=scene;
    const uint64_t epoch = tilecontext->ncmm_graphics_tileset_epoch();
    SDL_Renderer *renderer = get_sdl_renderer().get();
    if( epoch != graphics_hybrid_tileset_epoch || renderer != graphics_hybrid_renderer_identity || graphics_hybrid_sprite_bytes > 48 * 1024 * 1024 ) {
        if(renderer!=graphics_hybrid_renderer_identity)graphics_effect_failed=false;
        graphics_cpu_sprites.clear(); graphics_creature_composites.clear(); graphics_hybrid_sprite_bytes = 0;
        graphics_clear_gpu();
        graphics_hybrid_tileset_epoch = epoch; graphics_hybrid_renderer_identity = renderer;
    }
    graphics_prepare_creatures( base, positions,world );
    graphics_snapshot_ms = std::chrono::duration<double,std::milli>( std::chrono::steady_clock::now() - graphics_snapshot_begun ).count();
    if(world)graphics_world_extracted(graphics_snapshot_ms);
    graphics_hybrid_pixels.clear(); graphics_hybrid_width = graphics_hybrid_height = 0;
    graphics_resolved_textures = graphics_resolved_vehicle_textures = 0;
    graphics_frame_invalid = false; graphics_gpu_frame = false; graphics_hybrid_frame = world?&world->appearance:&scene; graphics_hybrid_positions = &positions;
    graphics_multilevel_frame=world;
    graphics_render_active = true;
    bool ok = false;
    snapshot_zone.stop();
    const auto callback_begun = std::chrono::steady_clock::now();
    try {
        module_call_scope scope( graphics_view.owner.c_str() );
        ok=world?graphics_view.world_render(world,graphics_view.user_data)!=0:
                 graphics_view.hybrid_render(&scene,graphics_view.user_data)!=0;
    } catch( ... ) {
        loaded_mod *mod = find_loaded_by_id( graphics_view.owner.c_str() );
        if( mod ) quarantine_runtime_callback( *mod, runtime_callback_kind::ui, "graphics_hybrid_exception" );
    }
    graphics_hybrid_frame = nullptr; graphics_hybrid_positions = nullptr;graphics_multilevel_frame=nullptr;
    graphics_render_active = false;
    graphics_callback_ms = std::chrono::duration<double,std::milli>( std::chrono::steady_clock::now() - callback_begun ).count();
    if( !ok || graphics_frame_invalid || ( !graphics_gpu_frame && graphics_hybrid_pixels.empty() ) ) {
        graphics_view.enabled = false; graphics_hybrid_pixels.clear(); graphics_release_mouse(); graphics_cancel_animation();
        add_msg( m_warning, "%s", tr_ui( "First person could not draw this scene; returned to the map. F6 retries.",
            "Не удалось отрисовать вид от первого лица; включена карта. F6 — повторить." ) );
        return false;
    }
    SDL_Rect prior_clip; SDL_GetRenderClipRect( renderer, &prior_clip ); const bool clipped = SDL_RenderClipEnabled( renderer );
    const auto presentation_begun = std::chrono::steady_clock::now();
    const SDL_Rect clip = { x, y, base.pixel_width, base.pixel_height };
    const auto generation = renderer_coordinator.texture_generations();
    if( !graphics_gpu_frame && graphics_stream_texture && ( graphics_stream_instance != generation.instance || graphics_stream_generation != generation.textures ||
        graphics_stream_width != graphics_hybrid_width || graphics_stream_height != graphics_hybrid_height ) ) graphics_clear_stream();
    if( !graphics_gpu_frame && !graphics_stream_texture ) {
        graphics_stream_texture = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
            int( graphics_hybrid_width ), int( graphics_hybrid_height ) );
        graphics_stream_instance = generation.instance; graphics_stream_generation = generation.textures;
        graphics_stream_width = graphics_hybrid_width; graphics_stream_height = graphics_hybrid_height;
    }
    SDL_Texture *frame = graphics_gpu_frame && graphics_gpu_engine ? graphics_gpu_engine->output() : graphics_stream_texture;
    if( !frame ) {graphics_view.enabled=false;graphics_release_mouse();graphics_cancel_animation();return false;}
    ok = graphics_gpu_frame || SDL_UpdateTexture( frame, nullptr, graphics_hybrid_pixels.data(), int( graphics_hybrid_width * 4 ) );
    ok = SDL_SetTextureBlendMode( frame, SDL_BLENDMODE_NONE ) && ok;
    ok = SDL_SetTextureScaleMode( frame, SDL_SCALEMODE_LINEAR ) && ok;
    ok = SDL_SetRenderClipRect( renderer, &clip ) && ok;
    const SDL_FRect destination = { float( x ), float( y ), float( base.pixel_width ), float( base.pixel_height ) };
    ok = SDL_RenderTexture( renderer, frame, nullptr, &destination ) && ok;
    if( ok && !graphics_gpu_frame ) ok = graphics_present_effects( renderer,frame,destination );
    if( ok ) {
        Uint8 r=0,g=0,b=0,a=0;SDL_GetRenderDrawColor( renderer,&r,&g,&b,&a );
        SDL_SetRenderDrawColor( renderer,216,224,224,255 );
        const float cx=x+base.pixel_width*.5f,cy=y+base.pixel_height*.5f;
        SDL_RenderLine( renderer,cx-4,cy,cx-1,cy );SDL_RenderLine( renderer,cx+1,cy,cx+4,cy );
        SDL_RenderLine( renderer,cx,cy-4,cx,cy-1 );SDL_RenderLine( renderer,cx,cy+1,cx,cy+4 );
        SDL_SetRenderDrawColor( renderer,r,g,b,a );
    }
    ncmm_graphics_trace::cpu_zone target_hud_zone("Target HUD");
    if( ok && !graphics_aim_name.empty() ) ncmm_draw_perspective_label( graphics_aim_name,
        x+int( graphics_aim_x*base.pixel_width ), y+int( graphics_aim_y*base.pixel_height ), clip );
    target_hud_zone.stop();
    ok = SDL_SetRenderClipRect( renderer, clipped ? &prior_clip : nullptr ) && ok;
    graphics_frame_pending_present=ok;
    if( !ok ) { graphics_clear_stream(); graphics_view.enabled = false; graphics_release_mouse(); }
    graphics_presentation_ms = std::chrono::duration<double,std::milli>( std::chrono::steady_clock::now() - presentation_begun ).count();
    return ok;
}
#else
bool graphics_draw_hybrid( const ncmm_graphics_scene_v1 &, const std::vector<tripoint_bub_ms> &, int, int ) { return false; }
#endif
