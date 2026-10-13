// Read-only, visible-only snapshots. Everything here runs on CDDA's main thread.
ncmm_graphics_creature_scene_v1 graphics_creature_scene = {};
ncmm_graphics_creature_metrics_v1 graphics_creature_metrics = {};
std::vector<ncmm_graphics_creature_v1> graphics_creature_snapshots;
uint64_t graphics_creature_world_epoch = 1, graphics_creature_next_id = 0, graphics_creature_events = 0;
#if defined(TILES)
struct graphics_creature_record {
    weak_ptr_fast<Creature> reference;
    ncmm_graphics_creature_v1 snapshot = {};
    double seen = 0;
    uint64_t presented_event_serial = 0;
    void present_event( double now ) {
        if( snapshot.event_serial != presented_event_serial ) {
            snapshot.event_seconds = now;
            presented_event_serial = snapshot.event_serial;
        }
    }
};
std::map<const Creature *, graphics_creature_record> graphics_creature_records;
std::map<SDL_Texture *, uint64_t> graphics_creature_sheets;
uint64_t graphics_creature_sheet_epoch = 0;
#endif
void graphics_clear_creatures()
{
    ++graphics_creature_world_epoch;
    graphics_creature_scene = {}; graphics_creature_metrics = {}; graphics_creature_snapshots.clear();
#if defined(TILES)
    graphics_creature_records.clear(); graphics_creature_sheets.clear(); graphics_creature_sheet_epoch = 0;
#endif
}
int graphics_get_creature_scene( ncmm_graphics_creature_scene_v1 *out )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !out || out->struct_size < sizeof( *out ) ) return 0;
    *out = graphics_creature_scene; return 1;
}
int graphics_get_creatures( ncmm_graphics_creature_v1 *out, size_t count )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || ( !out && count ) || count != graphics_creature_snapshots.size() ) return 0;
    std::copy( graphics_creature_snapshots.begin(), graphics_creature_snapshots.end(), out ); return 1;
}
int graphics_submit_creature_metrics( const ncmm_graphics_creature_metrics_v1 *data )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !data || data->struct_size < sizeof( *data ) ) return 0;
    graphics_creature_metrics = *data; return 1;
}
const ncmm_host_graphics_creatures_v1 graphics_creatures_api = { 1, sizeof( ncmm_host_graphics_creatures_v1 ), 1, 0,
    graphics_get_creature_scene, graphics_get_creatures, graphics_submit_creature_metrics };
#if defined(TILES)
graphics_creature_record *graphics_track_creature( Creature &cr )
{
    auto found = graphics_creature_records.find( &cr );
    if( found != graphics_creature_records.end() && found->second.reference.lock().get() == &cr ) return &found->second;
    auto owned = g->shared_from( cr );
    if( !owned || graphics_creature_records.size() >= 4096 ) return nullptr;
    auto &record = graphics_creature_records[&cr]; record = {}; record.reference = owned;
    record.snapshot.entity_id = ++graphics_creature_next_id;
    record.snapshot.heading = cr.facing == FacingDirection::LEFT ? 3.14159265f : 0.f;
    return &record;
}
void graphics_notify_creature( Creature *cr, uint32_t event, float heading )
{
    if( !g || !graphics_view.enabled || !cr || cr->is_avatar() || event < NCMM_CR_MELEE || event > NCMM_CR_DIE ) return;
    if( !get_avatar().sees( get_map(), *cr ) ) return;
    auto *record = graphics_track_creature( *cr ); if( !record ) return;
    auto &s = record->snapshot;
    if( ( s.flags & NCMM_CR_DEAD ) && event != NCMM_CR_DIE ) return;
    s.event = event; s.event_serial = ++graphics_creature_events; s.event_seconds = graphics_presentation_seconds();
    if( std::isfinite( heading ) && std::abs( heading ) <= 3.142f ) s.heading = heading;
    if( event == NCMM_CR_DIE ) {
        s.flags |= NCMM_CR_DEAD;
        const auto pos = cr->pos_abs(); s.world_x = pos.x(); s.world_y = pos.y(); s.world_z = pos.z();
    }
}
void graphics_creature_type( const mtype &type, ncmm_graphics_creature_v1 &s )
{
            s.liters = float( units::to_milliliter( type.volume ) ) / 1000.f;
            s.size_category = uint32_t( std::clamp( int( type.size ) - 1, 0, 4 ) );
            graphics_copy_text( s.bodytype, type.bodytype );
            std::string species;
            for( const auto &id : type.species ) { if( !species.empty() ) species += ' '; species += id.str(); }
            graphics_copy_text( s.species, species );
            const std::pair<const char *, uint32_t> traits[] = {
                { "HUMAN", NCMM_CR_HUMAN }, { "FLIES", NCMM_CR_FLIES }, { "SWIMS", NCMM_CR_SWIMS },
                { "AQUATIC", NCMM_CR_AQUATIC }, { "DIGS", NCMM_CR_BURROWS }, { "PLASTIC", NCMM_CR_AMORPHOUS },
                { "ELECTRONIC", NCMM_CR_MECHANICAL }, { "NOHEAD", NCMM_CR_NOHEAD }, { "IMMOBILE", NCMM_CR_IMMOBILE },
                { "RANGED_ATTACKER", NCMM_CR_RANGED }
            };
            for( const auto &trait : traits ) if( type.has_flag( mon_flag_id( trait.first ) ) ) s.flags |= trait.second;
            if( type.made_of( material_id( "fur" ) ) ) s.flags |= NCMM_CR_FUR;
            if( type.made_of( material_id( "chitin" ) ) ) s.flags |= NCMM_CR_CHITIN;
            if( type.made_of( material_id( "steel" ) ) && type.in_species( species_id( "ROBOT" ) ) ) s.flags |= NCMM_CR_MECHANICAL;
    s.kilograms = float( units::to_gram( type.weight ) ) / 1000.f;
    graphics_copy_text( s.type_id, type.id.str() );
    const auto size = ncmm_graphics_presentation_policy::creature( s.liters, int( type.size ) - 1, type.bodytype );
    s.height = size.height; s.width = size.width;
}
uint64_t graphics_creature_overlay_revision( const Character &ch, uint64_t h )
{
    for( const auto &overlay : ch.get_overlay_ids() ) {
        if( overlay.first.compare( 0, 5, "worn_" ) && overlay.first.compare( 0, 9, "mutation_" ) ) continue;
        for( const auto &text : { overlay.first, overlay.second } ) { for( unsigned char c : text ) h = ( h ^ c ) * 1099511628211ull; h = ( h ^ 255u ) * 1099511628211ull; }
    }
    return h;
}
void graphics_prepare_creatures( const ncmm_graphics_scene_v1 &base,
                                const std::vector<tripoint_bub_ms> &positions,const ncmm_graphics_multilevel_scene_v1 *world=nullptr )
{
    const double now = graphics_presentation_seconds(); avatar &you = get_avatar(); map &here = get_map();
    graphics_creature_snapshots.clear();
    if( graphics_creature_sheet_epoch != graphics_hybrid_tileset_epoch ) { graphics_creature_sheets.clear(); graphics_creature_sheet_epoch = graphics_hybrid_tileset_epoch; }
    for( size_t i = 0; i < base.cell_count; ++i ) {
        if( !( base.cells[i].flags & NCMM_GFX_VISIBLE ) || !base.cells[i].creature[0] || (world&&!(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE)) ) continue;
        Creature *cr = get_creature_tracker().creature_at<Creature>( positions[i] );
        if( !cr || cr->is_avatar() || cr->is_dead_state() || !you.sees( here, *cr ) ) continue;
        auto *record = graphics_track_creature( *cr ); if( !record ) continue;
        auto &s = record->snapshot; const auto pos = cr->pos_abs();
        if( record->seen > 0 && ( s.world_x != pos.x() || s.world_y != pos.y() ) ) s.heading = std::atan2( float( pos.y() - s.world_y ), float( pos.x() - s.world_x ) );
        s.world_x = pos.x(); s.world_y = pos.y(); s.world_z = pos.z(); s.cell = uint32_t( i ); s.flags = 0;
        s.kilograms = float( units::to_gram( cr->get_weight() ) ) / 1000.f; s.speed = float( cr->get_speed() );
        const auto &physical = graphics_presentation_cells[i];
        s.height = physical.creature_height; s.width = physical.creature_width; s.elevation = physical.creature_elevation;
        graphics_copy_text( s.type_id, base.cells[i].creature );
        if( const monster *mon = cr->as_monster() ) {
            graphics_creature_type( *mon->type, s );
            s.liters = float( units::to_milliliter( mon->get_volume() ) ) / 1000.f;
        } else if( const Character *ch = cr->as_character() ) {
            s.flags |= NCMM_CR_NPC | NCMM_CR_HUMAN; s.liters = std::max( .1f, s.kilograms ); s.size_category = 2;
            graphics_copy_text( s.bodytype, "human" ); graphics_copy_text( s.species, "HUMAN" );
            if( ch->is_prone() ) s.flags |= NCMM_CR_PRONE;
            if( ch->is_running() ) s.speed *= 1.5f;
            for( const trait_id &trait : ch->get_mutations() ) {
                const auto id = trait.str();
                if( id.find( "FUR" ) != std::string::npos ) s.flags |= NCMM_CR_FUR;
                if( id.find( "SCALE" ) != std::string::npos ) s.flags |= NCMM_CR_SCALES;
                if( id.find( "CHITIN" ) != std::string::npos ) s.flags |= NCMM_CR_CHITIN;
            }
        }
        s.height = physical.creature_height; s.width = physical.creature_width;
        s.kilograms = float( units::to_gram( cr->get_weight() ) ) / 1000.f;
        if( cr->is_underwater() ) s.flags |= NCMM_CR_SUBMERGED;
        // The selected atlas rectangle distinguishes deterministic tileset
        // variants. No texture address or SDL handle crosses the API.
        const auto category = cr->is_npc() ? TILE_CATEGORY::NONE : TILE_CATEGORY::MONSTER;
        auto atlas = tilecontext->ncmm_get_texture_draw_data( s.type_id, category, positions[i], false, true );
        s.appearance_revision = 1469598103934665603ull;
        if( atlas ) {
            auto sheet = graphics_creature_sheets.find( atlas->texture );
            if( sheet == graphics_creature_sheets.end() ) sheet = graphics_creature_sheets.emplace( atlas->texture, graphics_creature_sheets.size() + 1 ).first;
            s.appearance_revision ^= sheet->second;
            for( int value : { atlas->dimensions.x, atlas->dimensions.y, atlas->dimensions.w, atlas->dimensions.h } ) s.appearance_revision = ( s.appearance_revision ^ uint32_t( value ) ) * 1099511628211ull;
        }
        if( const Character *ch = cr->as_character() ) s.appearance_revision = graphics_creature_overlay_revision( *ch, s.appearance_revision );
        record->present_event( now );
        record->seen = now; graphics_creature_snapshots.push_back( s );
    }
    // Death uses an already observed snapshot and a still-visible current cell.
    // Disappearing/invisible live entities never acquire a death animation.
    for( auto &entry : graphics_creature_records ) {
        auto &record = entry.second;
        auto s = record.snapshot;
        const bool presented = record.presented_event_serial == s.event_serial;
        if( !( s.flags & NCMM_CR_DEAD ) || !s.type_id[0] || ( presented && ( now - s.event_seconds > .55 || now < s.event_seconds ) ) ) continue;
        const auto pos = here.get_bub( tripoint_abs_ms( s.world_x, s.world_y, s.world_z ) );
        for( size_t i = 0; i < positions.size(); ++i ) if( positions[i] == pos ) {
            if( (base.cells[i].flags & NCMM_GFX_VISIBLE)&&(!world||(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE)) ) {
                record.present_event( now ); record.seen = now;
                s = record.snapshot; s.cell = uint32_t( i ); graphics_creature_snapshots.push_back( s );
            }
            break;
        }
    }
    // Observe first, then prune: a long simulation turn must not consume an
    // unseen clip. Unavailable/hidden records still expire and never reveal it.
    for( auto it = graphics_creature_records.begin(); it != graphics_creature_records.end(); ) {
        if( now - it->second.seen > 2.0 ) it = graphics_creature_records.erase( it );
        else ++it;
    }
    const auto generation = renderer_coordinator.texture_generations();
    graphics_creature_scene = { sizeof( ncmm_graphics_creature_scene_v1 ), uint32_t( graphics_creature_snapshots.size() ),
        graphics_creature_world_epoch, graphics_hybrid_tileset_epoch ^ ( generation.instance << 32 ) ^ generation.textures, now };
}
#endif
