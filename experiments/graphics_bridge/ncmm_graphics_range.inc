// Native range helpers, included inside the Host's main-thread bridge scope.
// Keep the loaded/Z/light guards before evaluating Native integer distance.
bool graphics_world_in_visual_range(const tripoint_bub_ms &p,int maximum,bool no_sight,ncmm_world::light_range_cache *ranges=nullptr,const tripoint_bub_ms *surface_light=nullptr,std::optional<int> known_distance=std::nullopt)
{
    map &here=get_map();avatar &you=get_avatar();if(!here.ncmm_graphics_loaded(p)||std::abs(p.z()-you.posz())>fov_3d_z_range)return false;
    const auto &lit=surface_light?*surface_light:p;if(!here.ncmm_graphics_loaded(lit))return false;
    const float raw=here.ambient_light_at(lit);const int current=ranges?ranges->get(raw,[&]{return you.sight_range(raw);}):you.sight_range(raw),minimum=std::min(current,maximum);
    // The caller may reuse the exact Native integer for this observer/target.
    // Keep fallback lazy, including when a supplied distance is zero.
    const int wanted=known_distance?*known_distance:rl_dist(you.pos_bub(),p);const bool artificial=raw>here.get_cache_ref(p.z()).natural_light_level_cache;
    if(!(wanted<=minimum||(wanted<=maximum&&artificial)))return false;
    return wanted<=(no_sight?1:artificial?MAX_VIEW_DISTANCE:minimum);
}
bool graphics_world_in_visual_range(const tripoint_bub_ms &p){const auto &you=get_avatar();
    return graphics_world_in_visual_range(p,std::max(you.sight_range(default_daylight_level()),you.sight_range(0)),you.has_effect(efftype_id("no_sight")));}
bool graphics_world_perception(const tripoint_bub_ms &p){return graphics_world_in_visual_range(p)&&get_avatar().sees(get_map(),p);}
