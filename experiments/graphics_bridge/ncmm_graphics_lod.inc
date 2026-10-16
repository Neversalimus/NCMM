// Coarse distant geography from already resident, discovered overmap tiles.
// Kept separate from the near-world cells so its budget cannot disclose actors
// or increase the near snapshot radius / simulation reality bubble.
std::vector<ncmm_lod_patch_v1> graphics_lod_patches;
ncmm_graphics_lod_scene_v1 graphics_lod_frame{};
void graphics_build_lod(int32_t ox,int32_t oy,int origin_z,int side,bool enabled){
    graphics_lod_patches.clear();graphics_lod_frame={};
    graphics_lod_frame.struct_size=sizeof(graphics_lod_frame);graphics_lod_frame.version=1;
    if(!enabled||origin_z<0)return;
    graphics_lod_frame.enabled=1;graphics_lod_frame.ground_z=-origin_z*NCMM_WORLD_LEVEL_HEIGHT;
    const auto center=project_to<coords::omt>(get_avatar().pos_abs());
    for(int dy=-15;dy<=15;++dy)for(int dx=-15;dx<=15;++dx){
        const tripoint_abs_omt omt(center.x()+dx,center.y()+dy,0);
        point_abs_om om;point_om_omt local;
        std::tie(om,local)=project_remain<coords::om>(omt.xy());
        const auto *resident=overmap_buffer.ncmm_graphics_loaded_overmap(om);
        const bool known=resident&&resident->seen(tripoint_om_omt(local,0))>om_vision_level::unseen;
        uint32_t kind=NCMM_LOD_UNKNOWN;float height=0;std::array<float,3> color{{.27f,.31f,.27f}};
        if(known){
            const auto &terrain=resident->ter(tripoint_om_omt(local,0)).obj();const auto id=terrain.get_type_id().str();
            const auto has=[&](const char *s){return id.find(s)!=std::string::npos;};
            if(terrain.is_river()||terrain.is_lake()||terrain.is_ocean()||has("water")){kind=NCMM_LOD_WATER;color={{.16f,.29f,.34f}};}
            else if(has("forest")||has("woods")){kind=NCMM_LOD_FOREST;height=5.5f;color={{.18f,.28f,.17f}};}
            else if(terrain.is_road()||terrain.is_highway()||has("bridge")){kind=NCMM_LOD_ROAD;color={{.32f,.33f,.32f}};}
            else if(has("field")||has("rock")||has("swamp")||has("trail")||has("park")||has("crater")||has("empty")){kind=NCMM_LOD_FIELD;color={{.32f,.37f,.23f}};}
            else if(has("house")||has("shop")||has("office")||has("tower")||has("apartment")||has("school")||has("hospital")||has("warehouse")||has("cabin")||has("mall")||has("garage")||has("church")||has("factory")||has("farm")){kind=NCMM_LOD_BUILDING;height=5.f;color={{.43f,.42f,.39f}};}
            else {kind=NCMM_LOD_FIELD;color={{.32f,.37f,.23f}};}
        }
        const auto base=project_to<coords::ms>(omt);
        const float x=float(int64_t(base.x())-ox),y=float(int64_t(base.y())-oy);
        ncmm_world::outside_detail(x,y,24,24,float(side),[&](float px,float py,float w,float d){
            if(graphics_lod_patches.size()>=NCMM_LOD_MAX_PATCHES)return;
            // No silhouette crosses the detailed boundary. A coarse building
            // never overlays a nearby exact house or fills an actual shaft.
            const bool feature=w==24&&d==24&&std::max(std::abs(px+12-side*.5f),std::abs(py+12-side*.5f))>72;
            graphics_lod_patches.push_back({px,py,graphics_lod_frame.ground_z,w,d,feature?height:0,{color[0],color[1],color[2]},kind});
        });
    }
    uint64_t hash=1469598103934665603ull;
    const auto *bytes=reinterpret_cast<const unsigned char *>(graphics_lod_patches.data());
    for(size_t i=0;i<graphics_lod_patches.size()*sizeof(ncmm_lod_patch_v1);++i){hash^=bytes[i];hash*=1099511628211ull;}
    graphics_lod_frame.revision=hash?hash:1;graphics_lod_frame.count=uint32_t(graphics_lod_patches.size());graphics_lod_frame.patches=graphics_lod_patches.data();
}
int graphics_get_lod(ncmm_graphics_lod_scene_v1 *out){
    if(!graphics_multilevel_frame||active_module_id!=graphics_view.owner||!out||out->struct_size<sizeof(*out))return 0;
    *out=graphics_lod_frame;return 1;
}
