// Main-thread extraction into bounded owned POD. No item/itype pointer is retained.
ncmm_held_item_v1 graphics_held_item = {};
bool graphics_held_ready = false;
ncmm_held_metrics_v1 graphics_held_metrics = {};
template<size_t N> bool graphics_held_id(char (&out)[N],const std::string &id)
{
    if(id.size()>=N)return false;
    std::copy(id.begin(),id.end(),out);out[id.size()]=0;return true;
}
template<size_t N> uint32_t graphics_held_looks(const itype &source,char (&ids)[N][128],uint32_t &flags)
{
    std::array<std::string,N+1> visited;visited[0]=source.id.str();
    itype_id next=source.looks_like;uint32_t count=0;
    while(!next.is_null()&&next.is_valid()){
        const std::string id=next.str();
        for(size_t i=0;i<=count;++i)if(visited[i]==id){flags|=NCMM_HELD_LOOKS_CYCLE;return count;}
        if(count==N){flags|=NCMM_HELD_TRUNCATED;return count;}
        if(!graphics_held_id(ids[count],id)){flags|=NCMM_HELD_TRUNCATED;return count;}
        visited[count+1]=id;++count;next=next.obj().looks_like;
    }
    return count;
}
void graphics_prepare_held()
{
    graphics_held_item={};graphics_held_item.struct_size=sizeof(graphics_held_item);graphics_held_item.version=1;graphics_held_metrics={};
    graphics_held_ready=false;
#if defined(TILES)
    if(!character_state_available())return;
    const auto held=get_avatar().get_wielded_item();if(!held)return;
    auto &out=graphics_held_item;
    if(!graphics_held_id(out.canonical,held->typeId().str()))return;
    out.looks_count=graphics_held_looks(*held->type,out.looks_like,out.flags);
    out.base_length_m=float(units::to_millimeter(held->type->longest_side))/1000.f;
    out.mass_kg=float(units::to_gram(held->weight(false)))/1000.f;
    if(held->has_flag(flag_id("SPEAR")))out.flags|=NCMM_HELD_SPEAR;
    // Native steel grades are distinct material IDs (spear_steel uses
    // mc_steel plus wood). Do not infer the material from an item label.
    static const std::set<material_id> ferrous={material_id("steel"),material_id("iron"),
        material_id("budget_steel"),material_id("lc_steel"),material_id("mc_steel"),
        material_id("hc_steel"),material_id("ch_steel"),material_id("qt_steel")};
    if(held->made_of_any(ferrous))out.flags|=NCMM_HELD_METAL;
    if(held->is_active())out.flags|=NCMM_HELD_LIGHT_ACTIVE;
    if(!held->is_gun()&&held->is_tool()&&(held->type->maximum_charges()>0||held->type->charges_to_use()>0))out.flags|=NCMM_HELD_POWERED;
    static const char *qualities[]={"AXE","HAMMER","PRY","DIG","MINE","SAW","WRENCH","SCREW","CUT"};
    for(size_t i=0;i<NCMM_HELD_QUALITY_COUNT;++i){
        const quality_id id(qualities[i]);
        for(const auto *table:{&held->type->qualities,&held->type->charged_qualities}){
            const auto found=table->find(id);if(found!=table->end())out.qualities[i]=std::max(out.qualities[i],found->second.level);
        }
    }
    if(held->is_gun()){
        out.flags|=NCMM_HELD_GUN;const std::string skill=held->gun_skill().str();
        out.gun_skill=skill=="pistol"?NCMM_HELD_PISTOL:skill=="smg"?NCMM_HELD_SMG:skill=="rifle"?NCMM_HELD_RIFLE:
            skill=="shotgun"?NCMM_HELD_SHOTGUN:NCMM_HELD_OTHER_GUN;
        if(!held->magazine_compatible().empty())out.flags|=NCMM_HELD_DETACHABLE;
        if(held->magazine_current())out.flags|=NCMM_HELD_MAGAZINE_PRESENT;
        if(held->ammo_remaining()>0)out.flags|=NCMM_HELD_AMMO_LOADED;
        // Publish a physical ammunition property, not a guessed translated
        // weapon name. The default round also covers the just-emptied gun.
        const itype *round=held->ammo_data();
        if(!round){const auto fallback=held->ammo_default();if(fallback.is_valid())round=&fallback.obj();}
        if(round&&round->ammo&&((round->ammo->casing&&*round->ammo->casing!=itype_id::NULL_ID())||round->ammo->ammo_effects.count(ammo_effect_str_id("COOKOFF"))||
            round->ammo->ammo_effects.count(ammo_effect_str_id("BLACKPOWDER"))))out.flags|=NCMM_HELD_COMBUSTION;
        if(held->has_flag(flag_id("RELOAD_ONE")))out.flags|=NCMM_HELD_RELOAD_ONE;
        if(held->has_flag(flag_id("RELOAD_EJECT")))out.flags|=NCMM_HELD_RELOAD_EJECT;
        if(held->has_flag(flag_id("PUMP_ACTION")))out.flags|=NCMM_HELD_PUMP_ACTION|NCMM_HELD_MANUAL_ACTION;
        if(held->has_flag(flag_id("BOLT_ACTION"))||held->has_flag(flag_id("LEVER_ACTION")))out.flags|=NCMM_HELD_MANUAL_ACTION;
        if(held->type->gun)for(const auto &mode:held->type->gun->modes)if(mode.second.qty()>1)out.flags|=NCMM_HELD_AUTOMATIC;
        for(const item *mod:held->gunmods()){
            if(!mod->type->gunmod)continue;
            if(out.mod_count==NCMM_HELD_MOD_LIMIT){out.flags|=NCMM_HELD_TRUNCATED;break;}
            auto &record=out.mods[out.mod_count++];const auto &data=*mod->type->gunmod;
            if(!graphics_held_id(record.canonical,mod->typeId().str())||!graphics_held_id(record.location,data.location.str())){out.flags|=NCMM_HELD_TRUNCATED;break;}
            record.looks_count=graphics_held_looks(*mod->type,record.looks_like,record.flags);
            record.sight_dispersion=data.sight_dispersion;record.field_of_view=data.field_of_view;
            record.loudness_multiplier=data.loudness_multiplier;
            record.length_m=float(units::to_millimeter(mod->type->longest_side))/1000.f;
            if(mod->is_active())record.flags|=NCMM_HELD_LIGHT_ACTIVE;
        }
    }
    graphics_held_ready=true;
#endif
}
int graphics_get_held(ncmm_held_item_v1 *out)
{
    if(!out||out->struct_size<sizeof(*out)||!graphics_held_ready||!graphics_render_active||
       graphics_view.owner.empty()||active_module_id!=graphics_view.owner)return 0;
    *out=graphics_held_item;return 1;
}
int graphics_submit_held_metrics(const ncmm_held_metrics_v1 *value)
{
    if(!value||value->struct_size<sizeof(*value)||value->version!=1||!graphics_render_active||
       graphics_view.owner.empty()||active_module_id!=graphics_view.owner||value->matched>3||value->fallback_reason>7||
       value->attachments>4||value->draws>4096||value->pose_matrices>65536||value->static_builds>5||value->static_reuses>10||
       !std::memchr(value->model,0,sizeof(value->model)))return 0;
    for(size_t i=0;i<4;++i)if(!std::memchr(value->accessories[i],0,sizeof(value->accessories[i])))return 0;
    graphics_held_metrics=*value;return 1;
}
const ncmm_host_graphics_held_v1 graphics_held_api={1,sizeof(ncmm_host_graphics_held_v1),1,0,graphics_get_held,graphics_submit_held_metrics};
