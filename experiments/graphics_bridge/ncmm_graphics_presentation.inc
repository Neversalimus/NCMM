// Owned metadata survives no callback. Aim uses absolute coordinates and is
// invalidated at each frame; clicks revalidate visibility, hostility and reach.
ncmm_graphics_presentation_v1 graphics_presentation = {};
std::vector<ncmm_graphics_detail_v1> graphics_presentation_cells;
std::optional<tripoint_abs_ms> graphics_aim,graphics_aim_origin;
float graphics_aim_yaw=0,graphics_aim_pitch=0;
std::string graphics_aim_name;
ncmm_graphics_target::reference graphics_aim_reference;
ncmm_graphics_target::health graphics_aim_health;
bool graphics_aim_hostile=false;
uint64_t graphics_target_frame_sequence=0;
extern uint64_t graphics_creature_world_epoch;
extern std::vector<ncmm_graphics_creature_v1> graphics_creature_snapshots;
void graphics_clear_target()
{
    graphics_aim.reset();graphics_aim_name.clear();graphics_aim_reference={};graphics_aim_health={};graphics_aim_hostile=false;
}
bool graphics_world_target_visible(const tripoint_bub_ms &origin,const tripoint_bub_ms &target);
#if defined(TILES)
ncmm_graphics_target::reference graphics_target_reference(const Creature &creature,const tripoint_bub_ms &position)
{
    const auto absolute=get_map().get_abs(position);
    return {creature.ncmm_graphics_entity_id(),graphics_creature_world_epoch,graphics_target_frame_sequence,
        absolute.x(),absolute.y(),absolute.z()};
}
#endif
float graphics_aim_x=.5f, graphics_aim_y=.4f;
std::vector<uint32_t> graphics_effect_pixels;
uint32_t graphics_effect_width=0, graphics_effect_height=0;
bool graphics_effect_failed=false,graphics_click_pending=false;
uint64_t graphics_click_time=0;
struct {
    std::optional<tripoint_abs_ms> target,origin;
    uint64_t entity=0,world=0;
    float yaw=0,pitch=0;
} graphics_click_request;
size_t graphics_effect_gpu_frames=0, graphics_effect_cpu_frames=0;

int graphics_get_presentation( ncmm_graphics_presentation_v1 *out )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !out || out->struct_size < sizeof( *out ) ) return 0;
    *out = graphics_presentation; return 1;
}
int graphics_get_presentation_cells( ncmm_graphics_detail_v1 *out, size_t count )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !out || count != graphics_presentation_cells.size() ) return 0;
    std::copy( graphics_presentation_cells.begin(), graphics_presentation_cells.end(), out ); return 1;
}
int graphics_set_target( int32_t cell, float x, float y )
{
    if( !graphics_hybrid_frame || !graphics_hybrid_positions || active_module_id != graphics_view.owner ) return 0;
    graphics_clear_target();
    if( cell == -1 ) return 1;
    if( cell < 0 || size_t( cell ) >= graphics_hybrid_frame->base.cell_count || !std::isfinite( x ) || !std::isfinite( y ) || x < 0 || x > 1 || y < 0 || y > 1 ) return 0;
#if defined(TILES)
    if( !graphics_hybrid_frame->base.cells[cell].creature[0] ) return 0;
    const auto &p = ( *graphics_hybrid_positions )[cell];
    if( const Creature *cr = get_creature_tracker().creature_at<Creature>( p ) ) {
        if( cr->is_dead_state() || !get_avatar().sees( get_map(), *cr ) ) return 0;
        if(!(graphics_view.world_render?graphics_world_target_visible(get_avatar().pos_bub(),p):graphics_forward_visible(get_avatar().pos_bub(),p)))return 0;
        if(graphics_multilevel_frame&&(size_t(cell)>=graphics_multilevel_frame->cell_count||
            !(graphics_multilevel_frame->coordinates[cell].flags&NCMM_WORLD_BODY_VISIBLE)))return 0;
        const auto current=graphics_target_reference(*cr,p);
        const auto source=std::find_if(graphics_creature_snapshots.begin(),graphics_creature_snapshots.end(),[&](const auto &s){
            return s.cell==uint32_t(cell)&&s.entity_id==current.entity&&s.world_x==current.x&&s.world_y==current.y&&s.world_z==current.z&&!(s.flags&NCMM_CR_DEAD);
        });
        if(!current.entity||!current.world||!current.frame||source==graphics_creature_snapshots.end())return 0;
        graphics_aim_reference=current;
        graphics_aim_hostile=cr->attitude_to(get_avatar())==Creature::Attitude::HOSTILE;
        // Creature's canonical aggregate is used for characters too. The Host
        // never invents a sum of limbs or derives HP from sprite appearance.
        if(graphics_aim_hostile)graphics_aim_health=ncmm_graphics_target::native_health(cr->get_hp(),cr->get_hp_max());
        graphics_aim = get_map().get_abs( p ); graphics_aim_name = cr->disp_name();
        graphics_aim_origin=get_map().get_abs(get_avatar().pos_bub());graphics_aim_yaw=graphics_camera_yaw;graphics_aim_pitch=graphics_camera_pitch;
        graphics_aim_x=x; graphics_aim_y=y; return 1;
    }
#endif
    return 0;
}
int graphics_submit_effects( const uint32_t *pixels, uint32_t w, uint32_t h, uint32_t pitch )
{
    if( !graphics_hybrid_frame || active_module_id != graphics_view.owner || !pixels || !graphics_effect_pixels.empty() ||
        w < 1 || h < 1 || w > NCMM_GRAPHICS_MAX_FRAME_WIDTH || h > NCMM_GRAPHICS_MAX_FRAME_HEIGHT || pitch != w ) return 0;
    graphics_effect_pixels.assign( pixels, pixels + size_t( w ) * h );
    graphics_effect_width=w; graphics_effect_height=h; return 1;
}
const ncmm_host_graphics_presentation_v1 graphics_presentation_api = { 1, sizeof( ncmm_host_graphics_presentation_v1 ), 1, 0,
    graphics_get_presentation, graphics_get_presentation_cells, graphics_set_target, graphics_submit_effects };

#if defined(TILES)
template<size_t N> void graphics_copy_text( char ( &out )[N], const std::string &s )
{
    // IDs are ASCII. Labels never pass through this bounded ID buffer.
    std::fill( std::begin( out ), std::end( out ), '\0' );
    if( s.size() < N ) std::copy( s.begin(), s.end(), out );
}
uint32_t graphics_item_material( const item &it )
{
    std::string ids;
    for( const auto &m : it.made_of() ) ids += " " + m.first.str();
    return ncmm_graphics_presentation_policy::material( ids );
}
std::array<float,3> graphics_sample_rgb(const tripoint_bub_ms &p)
{
    map &here=get_map();if(!here.ncmm_graphics_loaded(p))return{0,0,0};
    const float raw=std::max(0.f,here.ambient_light_at(p)),level=std::clamp(.025f+std::sqrt(raw/(raw+24.f)),0.f,1.f);
    const auto &color=here.get_cache_ref(p.z()).light_color_cache[p.x()][p.y()];
    const float maximum=std::max({color.r,color.g,color.b,0.f}),tint=std::clamp(maximum/std::max(raw,1.f),0.f,.9f);
    auto channel=[&](float c){return level*(1-tint+(maximum>0?tint*c/maximum:0));};
    return{channel(color.r),channel(color.g),channel(color.b)};
}
float graphics_eye_height()
{
    avatar &you=get_avatar();float height=std::clamp(you.height()*.00885f,.5f,3.5f);
    if(you.is_crouching())height*=.72f;
    if(you.is_prone())height*=.25f;
    if(you.in_vehicle&&get_map().veh_at(you.pos_bub()))height=std::min(height,1.18f);
    return height;
}
void graphics_prepare_presentation( const ncmm_graphics_scene_v1 &base,
                                   const std::vector<tripoint_bub_ms> &positions,
                                   const std::vector<ncmm_graphics_cell_v2> &details,
                                   const ncmm_graphics_multilevel_scene_v1 *world=nullptr )
{
    using namespace ncmm_graphics_presentation_policy;
    avatar &you=get_avatar(); map &here=get_map();
    ++graphics_target_frame_sequence;
    graphics_presentation={}; auto &p=graphics_presentation;
    p.struct_size=sizeof( p ); p.eye_height=graphics_eye_height();
    static std::optional<tripoint_abs_ms> previous_position;
    static uint64_t last_step=0;
    const auto current_position=here.get_abs(you.pos_bub());const uint64_t ticks=SDL_GetTicks();
    if(previous_position&&*previous_position!=current_position)last_step=ticks;
    previous_position=current_position;
    p.movement=last_step ? std::clamp(1.0f-float(ticks-last_step)/340.0f,0.0f,1.0f) : 0.0f;
    p.body_scale=std::clamp( you.height()/175.0f, .45f, 2.0f ); p.held_length=.5f;
    graphics_copy_text( p.dimension, g->get_dimension_prefix().str() ); p.sky_style=sky( p.dimension );
    const char *cpu=std::getenv( "NCMM_FP_CPU" );
    const char *backend=SDL_GetRendererName( get_sdl_renderer().get() );
    if( !graphics_effect_failed && !( cpu && std::string( cpu ) == "1" ) && backend && std::string( backend ) != "software" ) p.flags|=NCMM_PRESENT_GPU_EFFECTS;
    // Outermost visible garments win, separately for each limb.
    for( const item_location &loc : you.get_visible_worn_items() ) if( loc ) {
        for( int side=0; side<2; ++side ) {
            if( loc->covers( bodypart_id( side ? "arm_r" : "arm_l" ) ) ) { graphics_copy_text( p.sleeve[side], loc->typeId().str() ); p.sleeve_material[side]=graphics_item_material( *loc ); }
            if( loc->covers( bodypart_id( side ? "hand_r" : "hand_l" ) ) ) { graphics_copy_text( p.glove[side], loc->typeId().str() ); p.glove_material[side]=graphics_item_material( *loc ); }
        }
    }
    for( const trait_id &trait : you.get_mutations() ) {
        const std::string id=trait.str();
        if( contains( id,"FUR" ) ) p.hand_features|=NCMM_HAND_FUR;
        if( contains( id,"SCALE" ) ) p.hand_features|=NCMM_HAND_SCALES;
        if( contains( id,"CHITIN" ) ) p.hand_features|=NCMM_HAND_CHITIN;
        if( contains( id,"CLAW" )||contains( id,"TALON" ) ) p.hand_features|=NCMM_HAND_CLAWS;
        if( contains( id,"WEBBED" ) ) p.hand_features|=NCMM_HAND_WEBBED;
    }
    p.arm_features=p.hand_features & ( NCMM_HAND_FUR|NCMM_HAND_SCALES|NCMM_HAND_CHITIN );
    // Only visible limb modifications; unrelated internal CBMs do not turn skin into metal.
    for( const bionic_id &bio : you.get_bionics() ) {
        const std::string id=bio.str();
        if( contains( id,"armor_arms" )||contains( id,"alloy_arms" ) ) p.arm_features|=NCMM_HAND_BIONIC;
        if( contains( id,"razor" )||contains( id,"claws" )||contains( id,"finger" ) ) p.hand_features|=NCMM_HAND_BIONIC;
    }
    if( const auto held=you.get_wielded_item() ) {
        p.held_length=std::clamp( units::to_millimeter( held->length() )/1000.0f, .08f, 3.5f );
        if( held->is_gun() ) {
            const std::string skill=held->gun_skill().str(),id=held->typeId().str();
            p.gun_kind=skill=="pistol" ? ( contains( id,"revolver" ) ? NCMM_GUN_REVOLVER : NCMM_GUN_PISTOL ) :
                skill=="shotgun" ? NCMM_GUN_SHOTGUN : skill=="smg" ? NCMM_GUN_SMG :
                skill=="launcher" ? NCMM_GUN_LAUNCHER : skill=="archery" ? NCMM_GUN_BOW : NCMM_GUN_RIFLE;
            for( const item *mod : held->gunmods() ) {
                const std::string id=mod->typeId().str(),slot=mod->type->gunmod ? mod->type->gunmod->location.str() : "";
                if( contains( slot,"sight" )||contains( id,"scope" )||contains( id,"red_dot" ) ) p.gunmods|=NCMM_MOD_OPTIC;
                if( contains( id,"suppress" )||contains( id,"silencer" ) ) p.gunmods|=NCMM_MOD_SUPPRESSOR;
                if( contains( id,"grip" ) ) p.gunmods|=NCMM_MOD_FOREGRIP;
                if( contains( id,"bipod" ) ) p.gunmods|=NCMM_MOD_BIPOD;
                if( contains( id,"laser" ) ) p.gunmods|=NCMM_MOD_LASER;
                if( contains( id,"light" ) ) p.gunmods|=NCMM_MOD_LIGHT;
                if( contains( id,"bayonet" ) ) p.gunmods|=NCMM_MOD_BAYONET;
                if( slot=="underbarrel"&&mod->is_gun() ) p.gunmods|=NCMM_MOD_UNDERBARREL;
                if( contains( slot,"stock" ) ) p.gunmods|=NCMM_MOD_STOCK;
            }
        }
    }
    // Views refer only to source-owned IDs and remain inside this callback.
    std::unordered_map<std::string_view,uint32_t> materials;
    auto material_for=[&](std::string_view id){auto found=materials.find(id);if(found!=materials.end())return found->second;
        const uint32_t value=material(std::string(id));materials.emplace(id,value);return value;};
    graphics_presentation_cells.assign( base.cell_count, {} );
    const vehicle *occupied=nullptr;
    std::map<const vehicle *,std::tuple<int,bool,int,float>> layouts;
    if( const auto vp=here.veh_at( you.pos_bub() ) ) if( you.in_vehicle ) { occupied=&vp->vehicle(); p.eye_height=std::min(p.eye_height,1.18f); }
    for( size_t i=0; i<base.cell_count; ++i ) {
        if( !( base.cells[i].flags&NCMM_GFX_VISIBLE ) ) continue;
        auto &d=graphics_presentation_cells[i]; const auto &pos=positions[i];
        auto *owned=world?graphics_camera_cache.find(world->coordinates[i].x,world->coordinates[i].y,world->coordinates[i].z):nullptr;
        if(owned&&owned->presentation_ready&&!graphics_camera_cache.verify){d=owned->presentation;++graphics_camera_cache.totals.presentation_hits;continue;}
        struct store_presentation {
            ncmm_graphics_snapshot::entry *owned;ncmm_graphics_snapshot::cache &cache;ncmm_graphics_detail_v1 &value;
            ~store_presentation(){if(!owned)return;if(owned->presentation_ready&&cache.verify){++cache.totals.verification_checks;if(std::memcmp(&owned->presentation,&value,sizeof(value)))++cache.totals.verification_mismatches;}
                owned->presentation=value;owned->presentation_ready=true;}
        } save_presentation{owned,graphics_camera_cache,d};
        if(world&&!(world->coordinates[i].flags&NCMM_WORLD_BODY_VISIBLE)){
            d.terrain_material=material_for((world->coordinates[i].flags&NCMM_WORLD_STRUCTURE_VISIBLE)?std::string_view(here.ter(pos).id().str()):std::string_view(base.cells[i].terrain));continue;
        }
        if( const auto vp=here.veh_at( pos ) ) {
            const vehicle *v=&vp->vehicle();auto found=layouts.find(v);
            if(found==layouts.end()){
                int minx=10000,maxx=-10000,miny=10000,maxy=-10000,controls=0;float cx=0;
                for(const vpart_reference &part:v->get_all_parts()){
                    const auto &mount=part.part().mount;
                    if(part.has_feature("STRUCTURE")){minx=std::min(minx,mount.x());maxx=std::max(maxx,mount.x());miny=std::min(miny,mount.y());maxy=std::max(maxy,mount.y());}
                    if(part.has_feature("CONTROLS")){cx+=mount.x();++controls;}
                }
                const int front=controls&&cx/controls<(minx+maxx)*.5f-.25f?-1:1;
                found=layouts.emplace(v,std::make_tuple(front,maxy-miny>=1&&maxx-minx>=2,controls,controls?cx/controls:0.0f)).first;
            }
            d.vehicle_front=std::get<0>(found->second);d.vehicle_automobile=std::get<1>(found->second)?1u:0u;
            d.vehicle_controls=std::get<2>(found->second);d.vehicle_controls_x=std::get<3>(found->second);
        }
        d.terrain_material=material_for(base.cells[i].terrain);d.furniture_material=material_for(base.cells[i].furniture);
        // A spilled liquid can be an item stack, not a map field. Only expose
        // the already-visible top item, and inspect its current phase so a
        // bottle, hidden contents or frozen water cannot become a puddle.
        if(base.cells[i].item[0]&&!here.has_flag(ter_furn_flag::TFLAG_LIQUIDCONT,pos)) {
            const auto items=here.i_at(pos);
            if(!items.empty()&&items.begin()->made_of(phase_id::LIQUID))
                d.item_liquid=liquid(items.begin()->typeId().str(),true);
        }
        if( details[i].terrain_shape==NCMM_SHAPE_WATER ) d.liquid=liquid( base.cells[i].terrain,true );
        if( !world && pos.z()<OVERMAP_HEIGHT && details[i].terrain_flags&NCMM_SURFACE_TERRAIN_CEILING ) {
            const std::string id=here.ter( tripoint_bub_ms( pos.x(),pos.y(),pos.z()+1 ) ).id().str();
            graphics_copy_text( d.ceiling,id ); d.ceiling_material=material_for(here.ter(tripoint_bub_ms(pos.x(),pos.y(),pos.z()+1)).id().str());
        }
        const auto &cache=here.get_cache_ref( pos.z() );
        d.source_light=std::clamp( cache.sm[pos.x()][pos.y()],0.0f,500.0f );
        const auto &col=cache.light_color_cache[pos.x()][pos.y()];
        const float max=std::max( {col.r,col.g,col.b,.01f} );
        d.source_r=max>.01f ? col.r/max : 1; d.source_g=max>.01f ? col.g/max : .90f; d.source_b=max>.01f ? col.b/max : .73f;
        if( base.cells[i].creature[0] ) if( const Creature *cr=get_creature_tracker().creature_at<Creature>( pos ) ) {
            if( you.sees( here,*cr ) ) {
                d.hostile=cr->attitude_to( you )==Creature::Attitude::HOSTILE;
                dimensions size{1.75f,.65f};
                if( const auto *mon=dynamic_cast<const monster *>( cr ) ) {
                    size=creature( units::to_milliliter( mon->get_volume() )/1000.0f, int( mon->get_size() )-1, mon->type->bodytype );
                    if( mon->has_flag( mon_flag_id( "FLIES" ) ) ) d.creature_elevation=.65f;
                } else if( const auto *ch=dynamic_cast<const Character *>( cr ) ) size={ch->height()/100.0f,ch->height()*.0037f};
                d.creature_height=size.height; d.creature_width=size.width;
            }
        }
        const auto ft=here.field_at( pos ).displayed_field_type();
        if( ft.id()!=field_type_str_id( "fd_null" ) ) {
            const std::string id=ft.id().str();
            d.field_intensity=uint32_t( std::clamp( here.get_field_intensity( pos,ft ),0,3 ) );
            d.gas=gas( id,ft.obj().phase==phase_id::GAS );
            if( !d.gas && ( ft.obj().phase==phase_id::LIQUID || ft.obj().moppable ) ) d.field_liquid=liquid( id,ft.obj().phase==phase_id::LIQUID );
        }
        if( occupied ) if( const auto vp=here.veh_at( pos ) ) if( &vp->vehicle()==occupied ) {
            d.vehicle_inside=1; p.occupied_vehicle=details[i].vehicle_group;
        }
    }
    graphics_clear_target(); graphics_effect_pixels.clear();
}
std::string graphics_click_action_impl()
{
    if( !graphics_view.enabled || !graphics_control_context || !character_state_available() ) return "";
    avatar &you=get_avatar(); map &here=get_map();
    if(you.controlling_vehicle||you.is_dead_state())return "";
    if(!graphics_aim) {
        const auto focus=graphics_fps_target(false);const auto held=you.get_wielded_item();
        if(held&&held->is_gun()) {
            if(!focus)return "fire"; // Native coordinate targeting, no invented sky intersection.
            you.last_target.reset();you.last_target_pos=here.get_abs(*focus);
            const auto *creature=get_creature_tracker().creature_at<Creature>(*focus);
            // Neutral/friendly actors retain the complete Native target UI.
            if(creature)return "fire";
            graphics_click_pending=true;graphics_click_time=SDL_GetTicks();
            graphics_click_request={here.get_abs(*focus),here.get_abs(you.pos_bub()),0,graphics_creature_world_epoch,graphics_camera_yaw,graphics_camera_pitch};
            return "fire";
        }
        const auto adjacent=graphics_fps_target(true);
        if(adjacent&&can_interact_at(ACTION_SMASH,here,*adjacent)) {
            graphics_fps_queue_target(*adjacent,"smash");return "smash";
        }
        return "";
    }
    if( you.controlling_vehicle || you.is_dead_state() || !graphics_aim_origin ||
        *graphics_aim_origin!=here.get_abs(you.pos_bub()) || std::abs(std::remainder(graphics_camera_yaw-graphics_aim_yaw,6.2831853f))>.001f || std::abs(graphics_camera_pitch-graphics_aim_pitch)>.001f ) return "";
    if( !graphics_mouse_drag ) {
        float mx=0,my=0; SDL_GetMouseState( &mx,&my );
        const auto q=window_to_display_buffer_coords( SDL_Point{int( mx ),int( my )} );
        if( q.x<graphics_viewport.x || q.x>=graphics_viewport.x+graphics_viewport.w || q.y<graphics_viewport.y || q.y>=graphics_viewport.y+graphics_viewport.h ) return "";
    }
    const auto pos=here.get_bub( *graphics_aim );
    Creature *cr=get_creature_tracker().creature_at<Creature>( pos );
    if( !cr || !graphics_aim_reference.matches(graphics_target_reference(*cr,pos)) || cr->is_dead_state() || cr->attitude_to( you )!=Creature::Attitude::HOSTILE || !you.sees( here,*cr ) ||
        !(graphics_view.world_render?graphics_world_target_visible(you.pos_bub(),pos):graphics_forward_visible(you.pos_bub(),pos)))return "";
    you.last_target=g->shared_from( *cr ); you.last_target_pos=*graphics_aim;
    if( const auto held=you.get_wielded_item() ) if( held->is_gun() || ( held->reach_range( you ).first>1 && you.can_reach_attack( *cr ) ) ) {
        graphics_click_pending=true;graphics_click_time=SDL_GetTicks();
        graphics_click_request={*graphics_aim,here.get_abs(you.pos_bub()),cr->ncmm_graphics_entity_id(),graphics_creature_world_epoch,graphics_camera_yaw,graphics_camera_pitch};return "fire";}
    if(pos.z()!=you.posz())return ""; // An XY move is not a vertical melee action.
    const int dx=pos.x()-you.pos_bub().x(),dy=pos.y()-you.pos_bub().y();
    if( std::max( std::abs( dx ),std::abs( dy ) )!=1 ) return "";
    if( dy<0 ) return dx<0 ? "LEFTUP" : dx>0 ? "RIGHTUP" : "UP";
    if( dy>0 ) return dx<0 ? "LEFTDOWN" : dx>0 ? "RIGHTDOWN" : "DOWN";
    return dx<0 ? "LEFT" : "RIGHT";
}
bool graphics_consume_click_impl(int x,int y,int z)
{
    const bool pending=graphics_click_pending;graphics_click_pending=false;
    if(!pending||!graphics_view.enabled||!graphics_click_request.target||!graphics_click_request.origin||SDL_GetTicks()-graphics_click_time>5000)return false;
    avatar &you=get_avatar();map &here=get_map();const tripoint_bub_ms p(x,y,z);
    if(here.get_abs(p)!=*graphics_click_request.target||here.get_abs(you.pos_bub())!=*graphics_click_request.origin||
       graphics_creature_world_epoch!=graphics_click_request.world||
       std::abs(std::remainder(graphics_camera_yaw-graphics_click_request.yaw,6.2831853f))>.001f||std::abs(graphics_camera_pitch-graphics_click_request.pitch)>.001f)return false;
    const Creature *cr=get_creature_tracker().creature_at<Creature>(p);
    if(!graphics_click_request.entity)return !cr&&here.ncmm_graphics_loaded(p)&&you.sees(here,p)&&
        (graphics_view.world_render?graphics_world_target_visible(you.pos_bub(),p):graphics_forward_visible(you.pos_bub(),p));
    return cr&&cr->ncmm_graphics_entity_id()==graphics_click_request.entity&&!cr->is_dead_state()&&
        cr->attitude_to(you)==Creature::Attitude::HOSTILE&&you.sees(here,*cr)&&
        (graphics_view.world_render?graphics_world_target_visible(you.pos_bub(),p):graphics_forward_visible(you.pos_bub(),p));
}
#else
std::string graphics_click_action_impl() { return ""; }
bool graphics_consume_click_impl(int,int,int) { return false; }
#endif
