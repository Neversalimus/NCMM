// Diagnostic only, in graphics_scene_smoke's disposable Native map. Nothing
// here changes production visibility, terrain classification or GPU validation.
{
    [&]() {
    constexpr int width=640,height=480;
    constexpr uint32_t terrain_nav=NCMM_WORLD_UP|NCMM_WORLD_RAMP|NCMM_WORLD_LADDER|
        NCMM_WORLD_CONNECTED_UP|NCMM_WORLD_CONNECTED_DOWN|NCMM_WORLD_RAMP_END;
    constexpr size_t owned_limit=size_t(NCMM_WORLD_MAX_CELLS)*(sizeof(ncmm_world_cell_v1)+sizeof(ncmm_graphics_cell_v1)+sizeof(ncmm_graphics_cell_v2))+
        size_t(NCMM_WORLD_MAX_SLICES)*sizeof(ncmm_world_slice_v1);
    require(graphics_view.world_render!=nullptr,"static_stair_world_provider_missing");
    require(get_options().has_option("NCMM_FIRST_PERSON_RENDERER"),"static_stair_cpu_option_missing");
    const auto original_position=you.pos_abs();const auto original_origin=here.get_abs_sub();
    const auto original_time=calendar::turn;
    const auto original_weather=get_weather().weather_id;
    const auto original_seconds=graphics_smoke_seconds;
    const auto original_renderer=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    const bool original_vision=you.has_trait(trait_id("DEBUG_NIGHTVISION"));
    const int original_moves=you.get_moves();
    const std::array<float,2> original_camera{{graphics_camera_yaw,graphics_camera_pitch}};
    const std::array<float,2> declared_camera{{-ncmm_graphics_camera::pi*.5f,-.1f}};
    // Invoke the registered module control callback, not an SDL mouse path.
    // Its ordinary controls.set_camera callback synchronizes both cameras.
    const auto set_fixture_camera=[provider=graphics_view.input,user=graphics_view.user_data,owner=graphics_view.owner](float yaw,float pitch){
        if(!provider)return false;
        struct delta_restore {float x,y;~delta_restore(){graphics_mouse_dx=x;graphics_mouse_dy=y;}} restore_delta{graphics_mouse_dx,graphics_mouse_dy};
        graphics_mouse_dx=std::remainder(yaw-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
        graphics_mouse_dy=(graphics_camera_pitch-pitch)/.003f;
        try {module_call_scope scope(owner.c_str());provider(NCMM_GFX_MOUSE_LOOK,user);}catch(...){return false;}
        return std::abs(std::remainder(graphics_camera_yaw-yaw,2*ncmm_graphics_camera::pi))<.000001f&&std::abs(graphics_camera_pitch-pitch)<.000001f;
    };
    const auto target=original_position+tripoint_rel_ms(0,-3,0);
    const auto furniture_control=original_position+tripoint_rel_ms(2,-3,0);
    const auto hidden_control=original_position+tripoint_rel_ms(-2,-6,0);
    const auto upper=target+tripoint_rel_ms(0,0,1);
    std::vector<int> loaded_levels;
    for(int z=here.supports_zlevels()?-OVERMAP_DEPTH:you.posz();z<=(here.supports_zlevels()?OVERMAP_HEIGHT:you.posz());++z)
        if(here.ncmm_graphics_loaded(tripoint_bub_ms(you.pos_bub().xy(),z)))loaded_levels.push_back(z);
    require(!loaded_levels.empty()&&loaded_levels.size()<=NCMM_WORLD_MAX_SLICES,"static_stair_loaded_slice_budget");
    const int low=loaded_levels.front(),high=loaded_levels.back();
    require(size_t(high-low+1)==loaded_levels.size(),"static_stair_loaded_slice_hole");
    const int radius=NCMM_GRAPHICS_V2_RADIUS;
    const tripoint_abs_ms physical_min(original_position.x()-radius,original_position.y()-radius,low),
        physical_max(original_position.x()+radius,original_position.y()+radius,high);
    const int memory_low=std::max(low,you.posz()-fov_3d_z_range),memory_high=std::min(high,you.posz()+fov_3d_z_range);
    const tripoint_abs_ms memory_min(physical_min.xy(),memory_low),memory_max(physical_max.xy(),memory_high);
    const auto omt_center=project_to<coords::omt>(original_position);
    const tripoint_abs_omt omt_min(omt_center.x()-15,omt_center.y()-15,low),omt_max(omt_center.x()+15,omt_center.y()+15,high);

    struct saved_tile {tripoint_bub_ms p;ter_id terrain;furn_id furniture;std::vector<item> items;};
    std::vector<saved_tile> saved;
    monster *hidden_monster=nullptr;
    struct fixture_restore {
        avatar &you;map &here;std::vector<saved_tile> &saved;monster *&subject;
        tripoint_abs_ms position;time_point time;weather_type_id weather;std::optional<double> seconds;
        std::string renderer;bool vision;int moves;std::array<float,2> camera;std::function<bool(float,float)> set_camera;
        ~fixture_restore(){
            if(subject)get_creature_tracker().remove(*subject);
            for(auto &entry:saved){here.ter_set(entry.p,entry.terrain);here.furn_set(entry.p,entry.furniture);
                here.clear_fields(entry.p);here.i_clear(entry.p);
                for(auto &object:entry.items)here.add_item(entry.p,std::move(object));}
            you.setpos(here,here.get_bub(position),false);you.set_moves(moves);calendar::turn=time;get_weather().weather_id=weather;
            if(you.has_trait(trait_id("DEBUG_NIGHTVISION"))!=vision)you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
            you.recalc_sight_limits();graphics_smoke_seconds=seconds;
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(renderer);
            if(!set_camera(camera[0],camera[1]))log_line(NCMM_LOG_ERROR,"Static stair diagnostic could not restore the module camera");
            graphics_clear_world();graphics_clear_creatures();g->reset_light_level();here.invalidate_map_cache(you.posz());
        }
    } restore_fixture{you,here,saved,hidden_monster,original_position,original_time,original_weather,original_seconds,original_renderer,original_vision,original_moves,original_camera,set_fixture_camera};
    // All construction precedes the first ledger. Restore the bounded region
    // on every exit; no live save, turn, NPC action or discovery pass is used.
    for(int z=0;z<=1;++z)for(int dy=-8;dy<=8;++dy)for(int dx=-8;dx<=8;++dx){
        const auto p=here.get_bub(original_position+tripoint_rel_ms(dx,dy,z));
        require(here.ncmm_graphics_loaded(p),"static_stair_fixture_unloaded");
        const auto tile=std::as_const(here).maptile_at(p);
        // This probe restores only the empty-field smoke seed. Restoring an
        // arbitrary copied field would also need submap field-count updates.
        require(tile.get_field().field_count()==0,"static_stair_seed_fields_not_empty");
        saved.push_back({p,tile.get_ter(),tile.get_furn(),{}});
        for(const auto &object:tile.get_items())saved.back().items.push_back(object);
        here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);
        const bool wall=std::abs(dx)==4||std::abs(dy)==4;
        const bool roof=std::abs(dx)<=4&&std::abs(dy)<=4;
        here.ter_set(p,ter_id(z==0?(wall?"t_wall":"t_floor"):(roof?"t_floor":"t_open_air")));
    }
    here.ter_set(here.get_bub(target),ter_id("t_stairs_up"));
    here.ter_set(here.get_bub(upper),ter_id("t_stairs_down"));
    require(g->find_stairs(here,upper.z(),here.get_bub(target))==here.get_bub(upper),"static_stair_native_pair_missing");
    require(furn_str_id("f_ncmm_static_stair_control").is_valid(),"static_stair_furniture_control_missing");
    here.furn_set(here.get_bub(furniture_control),furn_id("f_ncmm_static_stair_control"));
    here.furn_set(here.get_bub(hidden_control),furn_id("f_chair"));
    here.add_item(here.get_bub(hidden_control),item(itype_id("rock"),calendar::turn));
    require(!get_creature_tracker().creature_at(here.get_bub(hidden_control)),"static_stair_hidden_control_occupied");
    hidden_monster=g->place_critter_at(mtype_id("mon_zombie"),here.get_bub(hidden_control));
    require(hidden_monster!=nullptr,"static_stair_hidden_monster_missing");
    require(here.ter(here.get_bub(target)).obj().has_flag(ter_furn_flag::TFLAG_GOES_UP)&&
        !here.furn(here.get_bub(target)).obj().has_flag(ter_furn_flag::TFLAG_GOES_UP)&&
        !here.ter(here.get_bub(furniture_control)).obj().has_flag(ter_furn_flag::TFLAG_GOES_UP)&&
        here.furn(here.get_bub(furniture_control)).obj().has_flag(ter_furn_flag::TFLAG_GOES_UP),"static_stair_control_provenance_wrong");
    get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue("cpu");
    graphics_smoke_seconds=1000.5;
    // Memory preparation may allocate/load memory pages, so it is deliberately
    // outside the measured phases. Native requires equal Z endpoints here.
    you.prepare_map_memory_region(tripoint_abs_ms(physical_min.xy(),you.posz()),tripoint_abs_ms(physical_max.xy(),you.posz()));

    struct hash {
        uint64_t value=1469598103934665603ull;
        void integer(uint64_t n){for(int i=0;i<8;++i){value^=uint8_t(n>>(i*8));value*=1099511628211ull;}}
        void real(double n){uint64_t bits=0;std::memcpy(&bits,&n,sizeof(bits));integer(bits);}
        void text(const std::string &s){integer(s.size());for(unsigned char c:s){value^=c;value*=1099511628211ull;}}
        void point(int x,int y,int z){integer(uint64_t(int64_t(x)));integer(uint64_t(int64_t(y)));integer(uint64_t(int64_t(z)));}
        std::string str()const{return std::to_string(value);}
    };
    struct component {hash digest;uint64_t count=0;};
    struct native_ledger {
        std::map<std::string,component> physical;
        component memory,overmap;uint64_t resident=0;
        bool memory_valid=false,memory_show=false;
        std::array<int,3> avatar{},origin{};int moves=0,hp=0,hp_max=0;
        int64_t turn=0;std::string weather;int winddirection=0,windspeed=0;bool lightning=false;
        std::array<float,2> camera{};float eye=0;double seconds=0;
        std::string scalar_digest,nonlight_digest;
    };
    auto read_ledger=[&](){native_ledger out;
        for(const char *name:{"terrain","furniture","fields","map_items","avatar_items","creatures","vehicles","light"})out.physical.emplace(name,component{});
        auto &terrain=out.physical["terrain"],&furniture=out.physical["furniture"],&fields=out.physical["fields"],&objects=out.physical["map_items"],&light=out.physical["light"];
        auto add_item=[&](component &part,const item &object){std::ostringstream stream;stream.imbue(std::locale::classic());JsonOut json(stream);object.serialize(json);
            const auto bytes=stream.str();require(bytes.size()<=1024*1024,"static_stair_item_ledger_budget");part.digest.text(bytes);++part.count;};
        for(int z=low;z<=high;++z)for(int y=physical_min.y();y<=physical_max.y();++y)for(int x=physical_min.x();x<=physical_max.x();++x){
            const auto p=here.get_bub(tripoint_abs_ms(x,y,z));require(here.ncmm_graphics_loaded(p),"static_stair_physical_ledger_hole");
            const auto tile=std::as_const(here).maptile_at(p);
            terrain.digest.point(x,y,z);terrain.digest.text(tile.get_ter().id().str());++terrain.count;
            furniture.digest.point(x,y,z);furniture.digest.text(tile.get_furn().id().str());++furniture.count;
            fields.digest.point(x,y,z);fields.digest.integer(tile.get_field().field_count());
            for(const auto &entry:tile.get_field()){fields.digest.text(entry.first.id().str());fields.digest.integer(entry.second.get_field_intensity());
                fields.digest.integer(to_turns<int64_t>(entry.second.get_field_age()));fields.digest.integer(entry.second.is_field_alive());++fields.count;}
            objects.digest.point(x,y,z);objects.digest.integer(tile.get_items().size());for(const auto &object:tile.get_items())add_item(objects,object);
            light.digest.point(x,y,z);light.digest.real(here.ambient_light_at(p));
            const auto &rgb=here.get_cache_ref(z).light_color_cache[p.x()][p.y()];light.digest.real(rgb.r);light.digest.real(rgb.g);light.digest.real(rgb.b);++light.count;
        }
        const auto avatar_items=std::as_const(you).inv_dump();out.physical["avatar_items"].digest.integer(avatar_items.size());
        for(const item *object:avatar_items){require(object!=nullptr,"static_stair_avatar_item_null");add_item(out.physical["avatar_items"],*object);}
        std::vector<const Creature *> creatures;for(Creature &creature:g->all_creatures())creatures.push_back(&creature);
        std::sort(creatures.begin(),creatures.end(),[](const Creature *a,const Creature *b){return a->ncmm_graphics_entity_id()<b->ncmm_graphics_entity_id();});
        for(const Creature *creature:creatures){auto &part=out.physical["creatures"];const auto position=creature->pos_abs();part.digest.integer(creature->ncmm_graphics_entity_id());
            part.digest.point(position.x(),position.y(),position.z());part.digest.integer(creature->get_hp());part.digest.integer(creature->get_hp_max());
            part.digest.integer(creature->get_moves());part.digest.integer(creature->is_dead_state());
            part.digest.text(creature->is_avatar()?"avatar":creature->is_npc()?"npc":creature->as_monster()?creature->as_monster()->type->id.str():"other");++part.count;}
        const auto vehicles=here.get_vehicles();out.physical["vehicles"].count=vehicles.size();out.physical["vehicles"].digest.integer(vehicles.size());
        require(vehicles.empty(),"static_stair_fixture_vehicle_not_removed");
        out.memory_valid=you.is_map_memory_valid();out.memory_show=you.should_show_map_memory();
        for(int z=memory_low;z<=memory_high;++z)for(int y=memory_min.y();y<=memory_max.y();++y)for(int x=memory_min.x();x<=memory_max.x();++x){
            const auto &tile=you.get_memorized_tile(tripoint_abs_ms(x,y,z));out.memory.digest.point(x,y,z);out.memory.digest.integer(tile.symbol);
            out.memory.digest.text(tile.get_ter_id());out.memory.digest.text(tile.get_dec_id());out.memory.digest.integer(tile.get_ter_rotation());
            out.memory.digest.integer(tile.get_dec_rotation());out.memory.digest.integer(tile.get_ter_subtile());out.memory.digest.integer(tile.get_dec_subtile());++out.memory.count;}
        // Unlike overmap_buffer.seen(), this cannot load or generate an overmap.
        for(int z=low;z<=high;++z)for(int y=omt_min.y();y<=omt_max.y();++y)for(int x=omt_min.x();x<=omt_max.x();++x){
            const tripoint_abs_omt p(x,y,z);point_abs_om om;point_om_omt local;std::tie(om,local)=project_remain<coords::om>(p.xy());
            const auto *resident=overmap_buffer.ncmm_graphics_loaded_overmap(om);out.overmap.digest.point(x,y,z);out.overmap.digest.integer(resident!=nullptr);
            if(resident){out.overmap.digest.integer(static_cast<int>(resident->seen(tripoint_om_omt(local,z))));++out.resident;}++out.overmap.count;}
        const auto position=you.pos_abs();const auto origin=here.get_abs_sub();out.avatar={position.x(),position.y(),position.z()};out.origin={origin.x(),origin.y(),origin.z()};
        out.moves=you.get_moves();out.hp=you.get_hp();out.hp_max=you.get_hp_max();out.turn=to_turns<int64_t>(calendar::turn-calendar::turn_zero);
        out.weather=get_weather().weather_id.str();out.winddirection=get_weather().winddirection;out.windspeed=get_weather().windspeed;out.lightning=get_weather().lightning_active;
        out.camera={graphics_camera_yaw,graphics_camera_pitch};out.eye=graphics_eye_height();out.seconds=graphics_presentation_seconds();
        hash scalar;for(int n:out.avatar)scalar.integer(n);for(int n:out.origin)scalar.integer(n);scalar.integer(out.moves);scalar.integer(out.hp);scalar.integer(out.hp_max);
        scalar.integer(out.turn);scalar.text(out.weather);scalar.integer(out.winddirection);scalar.integer(out.windspeed);scalar.integer(out.lightning);
        for(float n:out.camera){scalar.real(n);}
        scalar.real(out.eye);scalar.real(out.seconds);out.scalar_digest=scalar.str();
        hash conserved;conserved.text(out.scalar_digest);for(const auto &part:out.physical)if(part.first!="light"){conserved.text(part.first);conserved.integer(part.second.count);conserved.text(part.second.digest.str());}
        conserved.text(out.memory.digest.str());conserved.integer(out.memory.count);conserved.integer(out.memory_valid);conserved.integer(out.memory_show);
        conserved.text(out.overmap.digest.str());conserved.integer(out.overmap.count);conserved.integer(out.resident);out.nonlight_digest=conserved.str();return out;
    };
    auto write_part=[](JsonOut &json,const component &part){json.start_object();json.member("count",part.count);json.member("hash",part.digest.str());json.end_object();};
    auto write_ledger=[&](JsonOut &json,const native_ledger &entry){json.start_object();json.member("physical");json.start_object();json.member("scalars");json.start_object();
        json.member("avatar",entry.avatar);json.member("origin",entry.origin);json.member("avatar_moves",entry.moves);json.member("avatar_hp",entry.hp);json.member("avatar_hp_max",entry.hp_max);
        json.member("turn",entry.turn);json.member("weather",entry.weather);json.member("winddirection",entry.winddirection);json.member("windspeed",entry.windspeed);json.member("lightning",entry.lightning);
        json.member("camera",entry.camera);json.member("eye_height",entry.eye);json.member("animation_seconds",entry.seconds);json.end_object();
        for(const auto &part:entry.physical){json.member(part.first);write_part(json,part.second);}json.end_object();
        json.member("memory");json.start_object();json.member("count",entry.memory.count);json.member("hash",entry.memory.digest.str());json.member("valid",entry.memory_valid);json.member("show",entry.memory_show);json.end_object();
        json.member("overmap");json.start_object();json.member("count",entry.overmap.count);json.member("hash",entry.overmap.digest.str());json.member("resident_tiles",entry.resident);json.end_object();json.end_object();};

    struct owned_snapshot {
        ncmm_graphics_multilevel_scene_v1 scene{};
        std::vector<ncmm_world_cell_v1> coordinates;std::vector<ncmm_world_slice_v1> slices;
        std::vector<ncmm_graphics_cell_v1> cells;std::vector<ncmm_graphics_cell_v2> details;
        void assign(const ncmm_graphics_multilevel_scene_v1 &source){scene=source;
            coordinates.assign(source.coordinates,source.coordinates+source.cell_count);slices.assign(source.slices,source.slices+source.slice_count);
            cells.assign(source.appearance.base.cells,source.appearance.base.cells+source.appearance.base.cell_count);
            details.assign(source.appearance.details,source.appearance.details+source.appearance.detail_count);
            scene.coordinates=coordinates.data();scene.slices=slices.data();scene.appearance.base.cells=cells.data();scene.appearance.details=details.data();}
        size_t retained_bytes()const{return coordinates.capacity()*sizeof(ncmm_world_cell_v1)+slices.capacity()*sizeof(ncmm_world_slice_v1)+
            cells.capacity()*sizeof(ncmm_graphics_cell_v1)+details.capacity()*sizeof(ncmm_graphics_cell_v2);}
    } normal;
    std::string normal_native_digest;
    auto world_digest=[&](const ncmm_graphics_multilevel_scene_v1 &w,bool protected_target){hash h;
        const auto id=[&](const auto &text){h.text(std::string(std::begin(text),std::find(std::begin(text),std::end(text),'\0')));};
        // Content identity excludes administrative epoch/revision/serial fields,
        // including derived slice identities. They legitimately change during
        // a Native cache refresh and do not describe the rendered source.
        h.integer(w.struct_size);h.integer(w.version);
        h.point(w.origin_x,w.origin_y,w.origin_z);h.integer(w.loaded_min_z);h.integer(w.loaded_max_z);h.real(w.level_height);h.real(w.camera_dx);h.real(w.camera_dy);h.real(w.camera_dz);
        const auto &a=w.appearance;const auto &b=a.base;h.integer(a.struct_size);h.integer(b.struct_size);h.integer(b.pixel_width);h.integer(b.pixel_height);
        h.integer(b.grid_width);h.integer(b.grid_height);h.real(b.camera_x);h.real(b.camera_y);h.integer(b.cell_count);h.integer(a.detail_count);h.integer(w.cell_count);h.integer(w.slice_count);
        for(uint32_t i=0;i<w.cell_count;++i){const auto &c=w.coordinates[i];h.point(c.x,c.y,c.z);
            if(!(protected_target&&c.x==target.x()&&c.y==target.y()&&c.z==target.z())){h.integer(c.flags);h.integer(c.direction);}
            h.integer(c.reserved);for(float n:c.lower_light)h.real(n);
            const auto &s=b.cells[i];h.integer(s.flags);h.real(s.light);id(s.terrain);id(s.furniture);id(s.item);id(s.creature);
            const auto &d=a.details[i];h.integer(d.terrain_shape);h.integer(d.furniture_shape);h.integer(d.orientation);h.integer(d.vehicle_features);h.integer(d.vehicle_group);h.integer(d.field_shape);h.integer(d.terrain_flags);
            for(float n:{d.light_r,d.light_g,d.light_b,d.emission,d.vehicle_x,d.vehicle_y,d.vehicle_yaw,d.vehicle_health})h.real(n);
            h.integer(d.mount_x);h.integer(d.mount_y);id(d.ground);id(d.field);}
        for(uint32_t i=0;i<w.slice_count;++i){const auto &s=w.slices[i];h.integer(s.z);h.integer(s.first_cell);h.integer(s.cell_count);h.integer(s.reserved);}
        const auto &e=a.environment;for(float n:{e.sun_x,e.sun_y,e.sun_z,e.moon_x,e.moon_y,e.moon_z,e.daylight,e.cloud_cover,e.fog_density,e.precipitation,e.wind_x,e.wind_y,e.sky_r,e.sky_g,e.sky_b,e.sky_tint})h.real(n);
        h.real(e.animation_seconds);for(uint32_t n:{e.moon_phase,e.weather_kind,e.lightning,e.outdoors,e.held_kind})h.integer(n);id(e.held_item);id(e.weather_id);return h.str();};
    struct cell_observation {
        bool present=false,terrain_up=false,furniture_up=false,sees=false,focus_attempted=false;
        int index=-1,focus=0,target=0;uint32_t flags=0,direction=0;
        std::string terrain,furniture,item,creature,field,raw_terrain,raw_furniture;
        float ambient=0;std::array<float,3> light{},published_light{};
    };
    const auto same_cell=[](const cell_observation &a,const cell_observation &b){
        return std::tie(a.present,a.terrain_up,a.furniture_up,a.sees,a.focus_attempted,a.index,a.focus,a.target,a.flags,a.direction,
                a.terrain,a.furniture,a.item,a.creature,a.field,a.raw_terrain,a.raw_furniture,a.ambient,a.light,a.published_light)==
            std::tie(b.present,b.terrain_up,b.furniture_up,b.sees,b.focus_attempted,b.index,b.focus,b.target,b.flags,b.direction,
                b.terrain,b.furniture,b.item,b.creature,b.field,b.raw_terrain,b.raw_furniture,b.ambient,b.light,b.published_light);};
    auto inspect=[&](const ncmm_graphics_multilevel_scene_v1 &w,const tripoint_abs_ms &position){cell_observation out;
        const auto p=here.get_bub(position);const auto tile=std::as_const(here).maptile_at(p);
        out.raw_terrain=tile.get_ter().id().str();out.raw_furniture=tile.get_furn().id().str();out.terrain_up=tile.get_ter().obj().has_flag(ter_furn_flag::TFLAG_GOES_UP);out.furniture_up=tile.get_furn().obj().has_flag(ter_furn_flag::TFLAG_GOES_UP);
        out.sees=you.sees(here,p);out.ambient=here.ambient_light_at(p);const auto &rgb=here.get_cache_ref(p.z()).light_color_cache[p.x()][p.y()];out.light={rgb.r,rgb.g,rgb.b};
        for(uint32_t i=0;i<w.cell_count;++i){const auto &c=w.coordinates[i];if(c.x!=position.x()||c.y!=position.y()||c.z!=position.z())continue;
            out.present=true;out.index=int(i);out.flags=c.flags;out.direction=c.direction;const auto &s=w.appearance.base.cells[i];
            out.terrain=s.terrain;out.furniture=s.furniture;out.item=s.item;out.creature=s.creature;const auto &detail=w.appearance.details[i];
            out.field=detail.field;out.published_light={detail.light_r,detail.light_g,detail.light_b};break;}return out;};
    auto write_cell=[](JsonOut &json,const cell_observation &c){json.start_object();json.member("present",c.present);json.member("index",c.index);json.member("flags",c.flags);json.member("direction",c.direction);
        json.member("terrain",c.terrain);json.member("furniture",c.furniture);json.member("item",c.item);json.member("creature",c.creature);json.member("field",c.field);
        json.member("raw_terrain",c.raw_terrain);json.member("raw_furniture",c.raw_furniture);json.member("raw_terrain_up",c.terrain_up);json.member("raw_furniture_up",c.furniture_up);
        json.member("native_sees",c.sees);json.member("ambient_light",c.ambient);json.member("light",c.light);json.member("published_light",c.published_light);
        json.member("focus_attempted",c.focus_attempted);json.member("focus_result",c.focus);json.member("target_result",c.target);json.end_object();};
    struct frame_record {
        std::string name,file,digest,protected_digest,native_digest,pixel_fingerprint;bool vision=false;
        uint64_t source_epoch=0,source_revision=0,source_serial=0;
        cell_observation target,furniture,hidden;native_ledger before,after;ncmm_gpu_cpu_metrics_v1 metrics{};std::vector<uint32_t> pixels;
    };
    struct observation {
        ncmm_graphics_render_multilevel_v1 provider;void *user;std::function<int(const ncmm_graphics_multilevel_scene_v1 &)> run;
        static int render(const ncmm_graphics_multilevel_scene_v1 *world,void *data){return world?static_cast<observation *>(data)->run(*world):0;}
    } observed{graphics_view.world_render,graphics_view.user_data,{}};
    struct callback_restore {observation &value;~callback_restore(){graphics_view.world_render=value.provider;graphics_view.user_data=value.user;}} restore_callback{observed};
    graphics_view.world_render=observation::render;graphics_view.user_data=&observed;
    SDL_Renderer *renderer=get_sdl_renderer().get();SDL_Texture *previous=SDL_GetRenderTarget(renderer);
    SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height);
    require(canvas!=nullptr,"static_stair_canvas_allocation_failed");
    struct canvas_restore {SDL_Renderer *renderer;SDL_Texture *canvas,*previous;~canvas_restore(){SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(canvas);}} restore_canvas{renderer,canvas,previous};
    auto refresh_vision=[&](bool debug){if(you.has_trait(trait_id("DEBUG_NIGHTVISION"))!=debug)you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
        you.recalc_sight_limits();g->ter_view_p=you.pos_bub();g->reset_light_level();
        for(int z=low;z<=high;++z)here.invalidate_map_cache(z);
        here.build_map_cache(you.posz());here.invalidate_visibility_cache();
        graphics_snapshot_lease.invalidate();graphics_camera_cache.suspend();
        // No update_visibility_cache, tilecontext->draw, Native UI redraw,
        // memorize calls, events, focus/warp or real-time input polling.
    };
    const auto bind_canvas=[&](){return SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&
        SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr);};
    // Exactly two declared preparation frames, before the first measured ledger.
    // They settle first-use held/equip state; this is not a first-frame timing
    // benchmark. No extra warm/retry loop is allowed after observations begin.
    require(set_fixture_camera(declared_camera[0],declared_camera[1]),"static_stair_declared_camera_not_applied");
    calendar::turn=calendar::turn_zero+12_hours;get_weather().weather_id=weather_type_id("sunny");refresh_vision(false);
    observed.run=[&](const ncmm_graphics_multilevel_scene_v1 &live){return observed.provider(&live,observed.user);};
    for(int preflight=0;preflight<2;++preflight){graphics_smoke_seconds=1000.0+preflight*.5;
        require(bind_canvas()&&graphics_draw(0,0,width,height)&&SDL_FlushRenderer(renderer),"static_stair_preflight_failed");
        require(!graphics_gpu_frame,"static_stair_preflight_used_gpu");++frames;}
    require(you.pos_abs()==original_position&&here.get_abs_sub()==original_origin&&you.get_moves()==original_moves,"static_stair_preflight_moved_native_avatar");
    auto record=[&](const std::string &scene,const std::string &name,const ncmm_graphics_multilevel_scene_v1 *replacement,bool save_normal){frame_record out;out.name=name;
        out.file="first-person-static-stair-"+scene+"-"+name+".bmp";out.vision=you.has_trait(trait_id("DEBUG_NIGHTVISION"));out.before=read_ledger();
        observed.run=[&](const ncmm_graphics_multilevel_scene_v1 &live){require(ncmm_world::valid(live),"static_stair_native_snapshot_invalid");
            const auto &w=replacement?*replacement:live;
            out.native_digest=world_digest(live,false);
            if(replacement)require(out.native_digest==normal_native_digest,"static_stair_live_normal_context_changed");
            if(save_normal){const size_t bytes=size_t(live.cell_count)*(sizeof(ncmm_world_cell_v1)+sizeof(ncmm_graphics_cell_v1)+sizeof(ncmm_graphics_cell_v2))+size_t(live.slice_count)*sizeof(ncmm_world_slice_v1);
                require(bytes<=owned_limit,"static_stair_owned_snapshot_budget");normal.assign(live);normal_native_digest=out.native_digest;
                require(normal.retained_bytes()<=owned_limit,"static_stair_owned_capacity_budget");}
            out.target=inspect(w,target);out.furniture=inspect(w,furniture_control);out.hidden=inspect(live,hidden_control);
            out.digest=world_digest(w,false);out.protected_digest=world_digest(w,true);
            out.source_epoch=w.world_epoch;out.source_revision=w.revision;out.source_serial=w.frame_serial;
            const int result=observed.provider(&w,observed.user);
            if(out.hidden.present){out.hidden.focus_attempted=true;out.hidden.focus=graphics_set_focus(out.hidden.index);out.hidden.target=graphics_set_target(out.hidden.index,.5f,.5f);}
            graphics_set_focus(-1);graphics_set_target(-1,0,0);
            require(!out.hidden.sees&&!out.hidden.furniture.size()&&!out.hidden.item.size()&&!out.hidden.creature.size()&&!out.hidden.field.size()&&!out.hidden.focus&&!out.hidden.target,"static_stair_hidden_control_exposed");
            require(out.digest==world_digest(w,false),"static_stair_provider_modified_snapshot");return result;};
        // The established CPU world cache expires at 250 ms. Wait without
        // changing a source field, epoch, simulation clock or renderer setting.
        SDL_Delay(260);
        require(bind_canvas(),"static_stair_canvas_bind_failed");
        require(graphics_draw(0,0,width,height),"static_stair_native_render_failed");require(SDL_FlushRenderer(renderer),"static_stair_flush_failed");
        require(!graphics_gpu_frame,"static_stair_cpu_probe_used_gpu");out.metrics=graphics_cpu_metrics;
        require(out.metrics.raster_calls>0&&out.metrics.triangles>0,"static_stair_did_not_rasterize");
        SDL_Surface *raw=SDL_RenderReadPixels(renderer,nullptr);SDL_Surface *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;if(raw)SDL_DestroySurface(raw);
        require(image&&image->w==width&&image->h==height,"static_stair_readback_failed");
        out.pixels.reserve(size_t(width)*height);hash pixels;
        for(int y=0;y<height;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);
            for(int x=0;x<width;++x){out.pixels.push_back(row[x]);pixels.integer(row[x]);}}
        out.pixel_fingerprint=pixels.str();const bool saved_image=SDL_SaveBMP(image,(game_root()/"ncmm"/out.file).string().c_str());SDL_DestroySurface(image);
        require(saved_image,"static_stair_capture_write_failed");out.after=read_ledger();++frames;
        require(out.before.nonlight_digest==out.after.nonlight_digest,"static_stair_render_mutated_native_state");
        require(here.get_abs_sub()==original_origin&&you.pos_abs()==original_position&&you.get_moves()==original_moves,"static_stair_probe_moved_native_avatar");return out;};
    auto write_frame=[&](JsonOut &json,const frame_record &entry,bool matrix,int body=0,int semantics=0,bool retained=false){json.start_object();json.member("name",entry.name);
        if(matrix){json.member("body",body);json.member("semantics",semantics);json.member("target_flags",entry.target.flags);json.member("target_direction",entry.target.direction);json.member("published_light",entry.target.published_light);}
        if(!matrix||retained){json.member("debug_nightvision",entry.vision);json.member("ledger_before");write_ledger(json,entry.before);json.member("target");write_cell(json,entry.target);json.member("furniture_control");write_cell(json,entry.furniture);json.member("hidden_control");write_cell(json,entry.hidden);}
        json.member("ledger_after");write_ledger(json,entry.after);json.member("world_digest",entry.digest);json.member("protected_snapshot_digest",entry.protected_digest);json.member("native_source_digest",entry.native_digest);
        json.member("source_epoch",entry.source_epoch);json.member("source_revision",entry.source_revision);json.member("source_serial",entry.source_serial);
        json.member("pixel_fingerprint",entry.pixel_fingerprint);json.member("file",entry.file);
        json.member("triangles",entry.metrics.triangles);json.member("raster_calls",entry.metrics.raster_calls);json.member("raster_width",entry.metrics.width);json.member("raster_height",entry.metrics.height);json.end_object();};

    std::ofstream stream(game_root()/"ncmm"/"first-person-static-stair-audit.json");stream.imbue(std::locale::classic());JsonOut report(stream);
    report.start_object();report.member("schema",2);report.member("diagnostic_only",true);report.member("actual_cdda_sdl",true);report.member("renderer","cpu");report.member("width",width);report.member("height",height);report.member("preflight_frames",2);report.member("owned_snapshot_limit",owned_limit);report.member("fixture_camera",declared_camera);
    report.member("thresholds");report.start_object();report.member("interaction_triangles",24);report.member("interaction_pixels",8);report.member("rgb_interaction",4);report.end_object();
    report.member("retained_thresholds");report.start_object();report.member("geometry_triangles",24);report.member("geometry_pixels",8);report.member("rgb_difference",4);report.end_object();
    report.member("cases");report.start_array();
    const std::array<std::pair<const char *,time_duration>,3> cases{{{"day",12_hours},{"dusk",19_hours},{"night",24_hours}}};
    for(const auto &test:cases){const std::string name=test.first;normal=owned_snapshot{};calendar::turn=calendar::turn_zero+test.second;get_weather().weather_id=weather_type_id("sunny");
        refresh_vision(false);auto first=record(name,"normal_before",nullptr,false);
        refresh_vision(true);auto debug=record(name,"debug",nullptr,false);
        refresh_vision(false);auto last=record(name,"normal_after",nullptr,true);
        const bool repeated=first.pixels==last.pixels;
        std::string inconclusive;
        const auto stable=[&](const native_ledger &a,const native_ledger &b){return a.nonlight_digest==b.nonlight_digest;};
        const auto lit=[&](const native_ledger &a,const native_ledger &b){return a.physical.at("light").digest.str()==b.physical.at("light").digest.str();};
        if(!first.before.memory_valid||!first.before.memory_show||!last.after.memory_valid||!last.after.memory_show)inconclusive="memory_ledger_unavailable";
        else if(!stable(first.before,debug.before)||!stable(first.before,last.after))inconclusive="native_ledger_mismatch";
        else if(!lit(first.before,first.after)||!lit(first.before,debug.before)||!lit(debug.before,debug.after)||!lit(first.before,last.before)||!lit(last.before,last.after))inconclusive="physical_light_mismatch";
        else if(last.target.ambient!=debug.target.ambient||last.target.light!=debug.target.light)inconclusive="target_raw_light_mismatch";
        else if(!repeated)inconclusive="normal_pixels_not_repeatable";
        else if(first.digest!=last.digest||first.protected_digest!=last.protected_digest||!same_cell(first.target,last.target))inconclusive="normal_snapshot_not_repeatable";
        else if(!first.target.present||!last.target.present||!debug.target.present)inconclusive="target_missing_in_native_snapshot";
        std::string retained_inconclusive=inconclusive;
        if(inconclusive.empty()){
            if((first.target.flags&NCMM_WORLD_BODY_VISIBLE)||(last.target.flags&NCMM_WORLD_BODY_VISIBLE))inconclusive="normal_body_already_visible";
            else if(!(debug.target.flags&NCMM_WORLD_BODY_VISIBLE)||!(debug.target.flags&NCMM_WORLD_UP)||!(debug.target.flags&NCMM_WORLD_CONNECTED_UP))inconclusive="debug_terrain_semantics_incomplete";
            else if(!last.target.terrain_up||last.target.furniture_up||debug.target.raw_terrain!=last.target.raw_terrain)inconclusive="terrain_stair_provenance_missing";
            else if(!((debug.target.flags&terrain_nav)&~last.target.flags))inconclusive="terrain_semantics_already_published";
        }
        constexpr uint32_t retained_nav=NCMM_WORLD_UP|NCMM_WORLD_CONNECTED_UP;
        if(retained_inconclusive.empty()){
            if(last.target.flags&NCMM_WORLD_BODY_VISIBLE)retained_inconclusive="normal_body_already_visible";
            else if(!(last.target.flags&NCMM_WORLD_STRUCTURE_VISIBLE))retained_inconclusive="normal_structure_not_visible";
            else if((last.target.flags&retained_nav)!=retained_nav)retained_inconclusive="normal_terrain_navigation_incomplete";
            else if(last.target.flags&(NCMM_WORLD_RAMP|NCMM_WORLD_LADDER))retained_inconclusive="normal_stair_is_not_ordinary";
            else if(!last.target.terrain_up||last.target.furniture_up||debug.target.raw_terrain!=last.target.raw_terrain)retained_inconclusive="terrain_stair_provenance_missing";
        }
        const bool eligible=inconclusive.empty();
        const bool retained_eligible=retained_inconclusive.empty();
        report.start_object();report.member("name",name);report.member("roi",std::array<int,4>{{240,160,160,280}});
        const auto point_array=[](const auto &p){return std::array<int,3>{{p.x(),p.y(),p.z()}};};
        report.member("target",point_array(target));report.member("furniture_control",point_array(furniture_control));report.member("hidden_control",point_array(hidden_control));
        report.member("physical_bounds",std::array<std::array<int,3>,2>{{point_array(physical_min),point_array(physical_max)}});
        report.member("memory_bounds",std::array<std::array<int,3>,2>{{point_array(memory_min),point_array(memory_max)}});
        report.member("omt_bounds",std::array<std::array<int,3>,2>{{point_array(omt_min),point_array(omt_max)}});
        report.member("phases");report.start_array();write_frame(report,first,false);write_frame(report,debug,false);write_frame(report,last,false);report.end_array();
        report.member("matrix_eligible",eligible);report.member("inconclusive_reason",inconclusive);report.member("normal_repeat_pixels_equal",repeated);report.member("owned_snapshot_bytes",normal.retained_bytes());
        report.member("matrix");report.start_array();bool zero_matches=false;
        if(eligible){const size_t index=size_t(last.target.index);require(index<normal.coordinates.size(),"static_stair_owned_target_missing");
            const uint32_t original_flags=normal.coordinates[index].flags,original_direction=normal.coordinates[index].direction;
            for(const auto &variant:std::array<std::array<int,2>,4>{{{{1,1}},{{1,0}},{{0,1}},{{0,0}}}}){const int body=variant[0],semantics=variant[1];
                auto &cell=normal.coordinates[index];cell.flags=original_flags;cell.direction=original_direction;
                if(body)cell.flags|=NCMM_WORLD_BODY_VISIBLE;
                if(semantics){cell.flags|=debug.target.flags&terrain_nav;cell.direction=debug.target.direction;}
                require((cell.flags&~(terrain_nav|NCMM_WORLD_BODY_VISIBLE))==(original_flags&~(terrain_nav|NCMM_WORLD_BODY_VISIBLE)),"static_stair_matrix_changed_other_flags");
                const auto entry=record(name,"matrix"+std::to_string(body)+std::to_string(semantics),&normal.scene,false);
                require(entry.protected_digest==last.protected_digest,"static_stair_matrix_modified_protected_snapshot");
                require(entry.target.published_light==last.target.published_light,"static_stair_matrix_changed_published_light");
                require(entry.after.nonlight_digest==last.after.nonlight_digest,"static_stair_matrix_mutated_native_state");
                if(!body&&!semantics){zero_matches=entry.pixels==last.pixels;require(entry.digest==last.digest,"static_stair_matrix00_changed_normal_snapshot");}
                write_frame(report,entry,true,body,semantics);
            }
            normal.coordinates[index].flags=original_flags;normal.coordinates[index].direction=original_direction;
        }
        report.end_array();report.member("matrix00_normal_pixels_equal",zero_matches);
        report.member("retained_geometry");report.start_object();report.member("eligible",retained_eligible);report.member("inconclusive_reason",retained_inconclusive);
        report.member("frames");report.start_array();bool retained_matches=false;
        if(retained_eligible){const size_t index=size_t(last.target.index);require(index<normal.coordinates.size(),"static_stair_retained_target_missing");
            const uint32_t original_flags=normal.coordinates[index].flags,original_direction=normal.coordinates[index].direction;
            // The positive control is the untouched real normal snapshot.
            // Remove only UP/CONNECTED_UP, preserving DOWN and every other bit.
            for(int keep:{1,0}){auto &cell=normal.coordinates[index];cell.flags=keep?original_flags:original_flags&~retained_nav;cell.direction=keep?original_direction:0;
                require(!(cell.flags&NCMM_WORLD_BODY_VISIBLE)&&(cell.flags&NCMM_WORLD_STRUCTURE_VISIBLE),"static_stair_retained_changed_visibility");
                require((cell.flags&~retained_nav)==(original_flags&~retained_nav),"static_stair_retained_changed_other_flags");
                const auto entry=record(name,keep?"normal_static":"nav_removed",&normal.scene,false);
                require(entry.protected_digest==last.protected_digest,"static_stair_retained_modified_protected_snapshot");
                require(entry.target.published_light==last.target.published_light,"static_stair_retained_changed_published_light");
                require(entry.after.nonlight_digest==last.after.nonlight_digest,"static_stair_retained_mutated_native_state");
                if(keep){retained_matches=entry.pixels==last.pixels;require(entry.digest==last.digest,"static_stair_retained_changed_normal_snapshot");}
                write_frame(report,entry,true,0,keep,true);
            }
            normal.coordinates[index].flags=original_flags;normal.coordinates[index].direction=original_direction;
        }
        report.end_array();report.member("normal_static_pixels_equal",retained_matches);report.end_object();report.end_object();stream.flush();
    }
    report.end_array();require(set_fixture_camera(original_camera[0],original_camera[1]),"static_stair_camera_restore_failed");
    report.member("success",true);report.end_object();stream.close();require(bool(stream),"static_stair_report_write_failed");
    }();
}
