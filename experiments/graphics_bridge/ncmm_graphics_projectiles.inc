// All producer/consumer calls are on Native's main thread. The bounded ring
// owns coordinates only, never a projectile, item, creature or map reference.
struct graphics_projectile_record {ncmm_graphics_projectile_v1 value{};double queued=0;ncmm_projectile::presentation_clock clock;ncmm_projectile_aim_v1 aim{};};
ncmm_projectile_aim_v1 graphics_projectile_aim{};
std::array<graphics_projectile_record,NCMM_PROJECTILE_LIMIT> graphics_projectile_records{};
uint64_t graphics_projectile_serial=0;size_t graphics_projectile_cursor=0;
void graphics_projectiles_reset(){graphics_projectile_records={};graphics_projectile_cursor=0;graphics_projectile_aim={};}
void graphics_projectiles_presented(uint64_t ticks){for(auto &record:graphics_projectile_records)if(record.value.serial)record.clock.presented(ticks/1000.0);}
bool graphics_projectiles_active(uint64_t ticks){
    const double now=ticks/1000.0;
    for(const auto &record:graphics_projectile_records)if(record.value.serial&&record.value.world_epoch==graphics_world_epoch&&
        (record.clock.committed?now-record.clock.start<1.5:now-record.queued<2.0))return true;
    return false;
}
bool graphics_projectile_presentation_impl(){
    return graphics_view.enabled&&graphics_view.world_render&&graphics_fps_enabled&&graphics_gameplay_context&&!graphics_input_router.command_mode();
}
void graphics_note_projectile_impl(Creature *origin,const map &here,const projectile &projectile,
        const tripoint_bub_ms &source,const tripoint_bub_ms &end,const std::vector<tripoint_bub_ms> &path){
#if defined(TILES)
    if(!graphics_projectile_presentation_impl()||!character_state_available()||origin!=&get_avatar()||&here!=&get_map())return;
    ncmm_graphics_projectile_v1 value{};value.serial=++graphics_projectile_serial;value.world_epoch=graphics_world_epoch;
    value.kind=projectile.count>1?NCMM_PROJECTILE_PELLET:NCMM_PROJECTILE_BULLET;
    const std::string drop=projectile.get_drop().typeId().str();
    if(drop.find("arrow")!=std::string::npos)value.kind=NCMM_PROJECTILE_ARROW;
    else if(drop.find("bolt")!=std::string::npos)value.kind=NCMM_PROJECTILE_BOLT;
    else if(!projectile.get_drop().is_null())value.kind=NCMM_PROJECTILE_THROWN;
    for(const auto &effect:projectile.proj_effects){const auto &id=effect.str();
        if(id.find("LASER")!=std::string::npos)value.kind=NCMM_PROJECTILE_LASER;
        else if(id.find("PLASMA")!=std::string::npos)value.kind=NCMM_PROJECTILE_PLASMA;
        else if(id.find("STREAM")!=std::string::npos||id.find("FLAME")!=std::string::npos||id=="JET")value.kind=NCMM_PROJECTILE_FLAME;
    }
    if(source==get_avatar().pos_bub())if(const auto held=get_avatar().get_wielded_item()){const auto &id=held->typeId().str();if(id.size()<sizeof(value.weapon))std::copy(id.begin(),id.end(),value.weapon);}
    auto append=[&](const tripoint_bub_ms &p){const auto at=here.get_abs(p);value.path[value.count++]={at.x(),at.y(),at.z()};};
    const auto endpoint=std::find(path.begin(),path.end(),end);
    if(end==source||endpoint==path.end())return;
    append(source);
    for(size_t n=1;n<=size_t(endpoint-path.begin())&&value.count<NCMM_PROJECTILE_POINTS;++n){const auto &p=path[n];
        if(!here.ncmm_graphics_loaded(p)||!get_avatar().sees(here,p))break;
        append(p);if(p==end)break;
    }
    if(value.count<2)return;
    auto &record=graphics_projectile_records[graphics_projectile_cursor++%graphics_projectile_records.size()];
    record={value,SDL_GetTicks()/1000.0,{},value.weapon[0]?graphics_projectile_aim:ncmm_projectile_aim_v1{}};
    graphics_mouse_dirty=true;
#else
    (void)origin;(void)here;(void)projectile;(void)source;(void)end;(void)path;
#endif
}
int graphics_get_projectile_paths(ncmm_graphics_projectile_v1 *out,size_t capacity){
#if defined(TILES)
    if(!out||capacity<NCMM_PROJECTILE_LIMIT||active_module_id!=graphics_view.owner||!graphics_render_active)return -1;
    const double now=SDL_GetTicks()/1000.0;size_t count=0;
    for(auto &record:graphics_projectile_records){auto &value=record.value;
        if(!value.serial||value.world_epoch!=graphics_world_epoch||(!record.clock.committed&&now-record.queued>2.0))continue;
        if(record.clock.committed&&now-record.clock.start>1.5)continue;
        out[count]=value;out[count++].seconds=record.clock.sample(now);
    }
    return int(count);
#else
    (void)out;(void)capacity;return 0;
#endif
}
const ncmm_host_graphics_projectiles_v1 graphics_projectiles_api={1,sizeof(ncmm_host_graphics_projectiles_v1),1,0,graphics_get_projectile_paths};
int graphics_get_projectile_paths_v2(ncmm_graphics_projectile_v2 *out,size_t capacity){
    if(!out||capacity<NCMM_PROJECTILE_LIMIT)return -1;
    std::array<ncmm_graphics_projectile_v1,NCMM_PROJECTILE_LIMIT> paths{};
    const int count=graphics_get_projectile_paths(paths.data(),paths.size());
    for(int n=0;n<count;++n){out[n]={};out[n].base=paths[n];
        for(const auto &record:graphics_projectile_records)if(record.value.serial==paths[n].serial){out[n].aim=record.aim;break;}}
    return count;
}
const ncmm_host_graphics_projectiles_v2 graphics_projectiles_v2_api={1,sizeof(ncmm_host_graphics_projectiles_v2),2,0,graphics_get_projectile_paths_v2};
