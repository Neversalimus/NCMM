// Isolated real CDDA fixtures. Geometry is produced from terrain/furniture
// definitions and flags through the ordinary graphics callback, never meshes.
{
    using clock=std::chrono::steady_clock;
    const bool before=std::getenv("NCMM_FP_MULTILEVEL_BEFORE")!=nullptr;
    const auto original_position=you.pos_abs();const auto original_time=calendar::turn;
    const float original_yaw=graphics_camera_yaw,original_pitch=graphics_camera_pitch;
    const int original_moves=you.get_moves();const auto original_hp=you.get_part_hp_cur(bodypart_id("torso"));
    const auto original_target=SDL_GetRenderTarget(renderer);
    struct observation {
        ncmm_graphics_render_multilevel_v1 provider=nullptr;void *user=nullptr;
        std::vector<int> levels;uint32_t floors=0,bottoms=0,actors=0;float eye=0;
        std::function<void(const ncmm_graphics_multilevel_scene_v1 &)> inspect;
        static int render(const ncmm_graphics_multilevel_scene_v1 *world,void *data){auto &self=*static_cast<observation *>(data);self.inspect(*world);return self.provider(world,self.user);}
    } observed;
    observed.provider=graphics_view.world_render;observed.user=graphics_view.user_data;
    struct callback_scope {observation &value;~callback_scope(){graphics_view.world_render=value.provider;graphics_view.user_data=value.user;}} callback_guard{observed};
    observed.inspect=[&](const ncmm_graphics_multilevel_scene_v1 &world){
        require(ncmm_world::valid(world),"multilevel_real_world_contract_invalid");observed.levels.clear();observed.floors=observed.bottoms=observed.actors=0;
        observed.eye=graphics_eye_height()+world.camera_dz;
        for(uint32_t z=0;z<world.slice_count;++z)if(world.slices[z].cell_count)observed.levels.push_back(world.slices[z].z);
        for(uint32_t i=0;i<world.cell_count;++i){const auto &c=world.coordinates[i];const auto &a=world.appearance.base.cells[i];
            if(c.flags&NCMM_WORLD_FLOOR_TOP)++observed.floors;if(c.flags&NCMM_WORLD_FLOOR_BOTTOM)++observed.bottoms;
            const auto p=here.get_bub(tripoint_abs_ms(c.x,c.y,c.z));
            if(a.creature[0]){++observed.actors;const auto *who=get_creature_tracker().creature_at<Creature>(p);require(who&&you.sees(here,*who),"multilevel_hidden_creature_identity_exposed");}
            if(a.item[0])require(you.sees(here,p)&&here.sees_some_items(p,you),"multilevel_hidden_item_identity_exposed");
            if(!(c.flags&NCMM_WORLD_BODY_VISIBLE))require(!a.creature[0]&&!a.item[0]&&!a.furniture[0],"multilevel_underside_exposed_upper_room_contents");
        }
    };
    if(observed.provider){graphics_view.world_render=observation::render;graphics_view.user_data=&observed;}
    auto camera=[&](float yaw,float pitch){graphics_mouse_dx=std::remainder(yaw-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;graphics_mouse_dy=(graphics_camera_pitch-pitch)/.003f;
        require(graphics_action("ncmm.view.mouse_look"),"multilevel_camera_input_failed");graphics_mouse_dx=graphics_mouse_dy=0;};
    auto clear_monsters=[&](){const auto all=get_creature_tracker().get_monsters_list();for(const auto &mon:all)get_creature_tracker().remove(*mon);};
    auto terrain=[&](int x,int y,int z,const char *id){const tripoint_bub_ms p(center.x()+x,center.y()+y,z);require(here.ncmm_graphics_loaded(p),"multilevel_fixture_outside_loaded_map");require(ter_id(id).is_valid(),"multilevel_fixture_unknown_terrain");here.ter_set(p,ter_id(id));};
    auto spawn=[&](const char *id,int x,int y,int z){auto mon=make_shared_fast<monster>(mtype_id(id));require(g->place_critter_around(mon,tripoint_bub_ms(center.x()+x,center.y()+y,z),0,true)!=nullptr,"multilevel_fixture_creature_spawn_failed");};
    you.clear_mutations();you.clear_effects();you.set_underwater(false);you.set_base_height(175);you.set_movement_mode(move_mode_id("walk"));
    if(!you.has_trait(trait_id("DEBUG_NIGHTVISION")))you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));you.recalc_sight_limits();
    auto reset=[&](){clear_monsters();for(const auto &vehicle:here.get_vehicles())here.destroy_vehicle(vehicle.v);
        you.setpos(here,here.get_bub(original_position),false);calendar::turn=original_time;get_weather().weather_id=weather_type_id("sunny");
        for(int z=-OVERMAP_DEPTH;z<=OVERMAP_HEIGHT;++z)for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){const tripoint_bub_ms p(center.x()+x,center.y()+y,z);if(!here.ncmm_graphics_loaded(p))continue;
            here.ter_set(p,ter_id(z==0?"t_pavement":"t_open_air"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);here.remove_trap(p);}
        graphics_clear_world();graphics_clear_creatures();camera(-ncmm_graphics_camera::pi*.5f,.08f);
    };
    auto refresh=[&](){g->ter_view_p=you.pos_bub();g->reset_light_level();here.invalidate_map_cache(you.posz());here.build_map_cache(you.posz());here.invalidate_visibility_cache();here.update_visibility_cache(you.posz());};
    auto make_scene=[&](const std::string &name,int layers,int theme){reset();
        const bool underground=theme==3||name=="basement"||name=="lab"||name=="cave";
        const int low=underground?-layers:0;
        for(int z=low;z<low+layers;++z)for(int y=-12;y<=-2;++y)for(int x=-5;x<=5;++x){
            terrain(x,y,z,name=="unknown_mod"?"t_ncmm_x9_unknown_floor":theme==2?"t_dirt":theme==4&&z==0?"t_water_sh":"t_floor");
            if(theme==1||theme==3||name.find("house")!=std::string::npos||name=="high_building"||name=="rooftop"||name=="lab"||name=="cave"||name=="unknown_mod"){
                if(std::abs(x)==5||y==-12||y==-2)terrain(x,y,z,name=="unknown_mod"?"t_ncmm_x9_unknown_support":name=="cave"?"t_rock":"t_wall");
                if((y==-2||y==-12)&&std::abs(x)<=2)terrain(x,y,z,"t_window");}
            if(theme==2&&std::abs(x)==4&&(y%3)==0)terrain(x,y,z,"t_tree");
        }
        if(theme==1||theme==3||name.find("house")!=std::string::npos||name=="high_building"||name=="rooftop"||name=="lab"||name=="cave"||name=="basement"||name=="unknown_mod")
            for(int y=-12;y<=-2;++y)for(int x=-5;x<=5;++x)terrain(x,y,low+layers,"t_floor");
        if(name.find("house")!=std::string::npos){you.setpos(here,tripoint_bub_ms(center.x(),center.y()+10,0),false);camera(-ncmm_graphics_camera::pi*.5f,.2f);}
        if(name=="high_building"){you.setpos(here,tripoint_bub_ms(center.x(),center.y()+25,0),false);camera(-ncmm_graphics_camera::pi*.5f,.3f);}
        if(underground){you.setpos(here,tripoint_bub_ms(center.x(),center.y()-5,low),false);camera(-ncmm_graphics_camera::pi*.5f,.08f);}
        if(name=="stairs"||name=="stairwell"||name=="stairs_down")for(int z=low;z<low+layers-1;++z){terrain(0,-7,z,"t_stairs_up");terrain(0,-7,z+1,"t_stairs_down");}
        if(name=="stairs"||name=="stairwell"){you.setpos(here,tripoint_bub_ms(center.x(),center.y()-4,low),false);camera(-ncmm_graphics_camera::pi*.5f,.25f);}
        if(name=="stairs_down"){you.setpos(here,tripoint_bub_ms(center.x(),center.y()-5,low+layers-1),false);camera(-ncmm_graphics_camera::pi*.5f,-.7f);}
        if(name=="ramp"){terrain(0,-7,0,"t_ramp_up_low");terrain(0,-8,0,"t_ramp_up_high");terrain(0,-7,1,"t_ramp_down_low");terrain(0,-8,1,"t_ramp_down_high");you.setpos(here,tripoint_bub_ms(center.x(),center.y()-4,0),false);camera(-ncmm_graphics_camera::pi*.5f,.35f);}
        if(name=="shaft"||name=="broken_floor"||name=="enemy_below"){
            for(int z=low;z<low+layers;++z)for(int y=-9;y<=-5;++y)for(int x=-2;x<=2;++x)terrain(x,y,z,"t_open_air");
            you.setpos(here,tripoint_bub_ms(center.x(),center.y()-4,low+layers-1),false);camera(-ncmm_graphics_camera::pi*.5f,-.8f);}
        if(name=="rooftop"){you.setpos(here,tripoint_bub_ms(center.x(),center.y()-5,layers),false);camera(-ncmm_graphics_camera::pi*.5f,-.25f);}
        if(name=="bridge")for(int y=-20;y<=4;++y)for(int x=-8;x<=8;++x){terrain(x,y,0,"t_water_dp");if(std::abs(x)<=1)terrain(x,y,1,"t_pavement");}
        if(name=="bridge"){you.setpos(here,tripoint_bub_ms(center.x(),center.y()+2,1),false);camera(-ncmm_graphics_camera::pi*.5f,-.2f);}
        if(name=="enemy_above"){for(int y=-9;y<=-3;++y)for(int x=-2;x<=2;++x)terrain(x,y,1,"t_open_air");terrain(0,-7,1,"t_floor");spawn("mon_zombie",0,-7,1);camera(-ncmm_graphics_camera::pi*.5f,.35f);}
        if(name=="enemy_below"){terrain(0,-7,0,"t_floor");spawn("mon_zombie",0,-7,0);}
        if(name=="flying"){for(int z=1;z<=2;++z)for(int y=-6;y<=-2;++y)for(int x=-2;x<=2;++x)terrain(x,y,z,"t_open_air");spawn("mon_crow",0,-4,2);camera(-ncmm_graphics_camera::pi*.5f,.85f);}
        if(theme==6||name=="smoke")for(int z=low;z<low+layers;++z)here.add_field(tripoint_bub_ms(center.x(),center.y()-7,z),field_type_str_id(z%2?"fd_toxic_gas":"fd_smoke").id(),2);
        if(name=="light_other_z"){calendar::turn=original_time+12_hours;here.add_field(tripoint_bub_ms(center.x(),center.y()-7,1),field_type_str_id("fd_fire").id(),2);}
        if(theme==5)for(int i=0;i<50;++i)spawn(i%3?"mon_zombie":"mon_crow",-4+i%9,-3-(i/9),low+i%layers);
        refresh();
    };
    auto percentile=[](std::vector<double> values,double p){std::sort(values.begin(),values.end());if(values.empty())return 0.0;const double at=p*(values.size()-1);const size_t a=size_t(at),b=std::min(a+1,values.size()-1);return values[a]+(values[b]-values[a])*(at-a);};
    std::ofstream report_stream(game_root()/"ncmm"/(before?"first-person-multilevel-before.json":"first-person-multilevel-after.json"));JsonOut report(report_stream);
    report.start_object();report.member("schema",1);report.member("actual_cdda_sdl",true);report.member("source_fov_z_range",fov_3d_z_range);report.member("configured_fov_toggle_available",false);
    report.member("world_api",observed.provider!=nullptr);report.member("gpu_timing_is_execution",false);report.member("frames");report.start_array();
    SDL_Texture *canvas=nullptr;
    auto render=[&](point resolution){require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"multilevel_canvas_rebind_failed");
        require(graphics_draw(0,0,resolution.x,resolution.y),"multilevel_real_sdl_render_failed");require(SDL_FlushRenderer(renderer),"multilevel_sdl_flush_failed");++frames;};
    auto present=[&](){require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"multilevel_window_target_failed");require(SDL_RenderTexture(renderer,canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"multilevel_actual_present_failed");};
    auto qualify=[&](const std::string &name,int layers,int theme,point resolution,bool gallery){
        canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,resolution.x,resolution.y);require(canvas!=nullptr,"multilevel_canvas_allocation_failed");
        for(const char *mode:{"cpu","gpu"}){get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);graphics_smoke_seconds=1000.5;
            const auto pos=you.pos_abs();const auto time=calendar::turn;const auto moves=you.get_moves();const int hp=you.get_part_hp_cur(bodypart_id("torso"));
            std::vector<std::pair<tripoint_abs_omt,om_vision_level>> discovery;
            for(int z=-OVERMAP_DEPTH;z<=OVERMAP_HEIGHT;++z){const auto p=project_to<coords::omt>(pos);const tripoint_abs_omt tile(p.xy(),z);discovery.emplace_back(tile,overmap_buffer.seen(tile));}
            const auto cold=clock::now();bool ready=false;
            for(int attempt=0;attempt<160;++attempt){render(resolution);present();ready=std::string(mode)=="cpu"||graphics_gpu_frame;
                if(ready&&(!graphics_creature_metrics.queued||!observed.actors))break;SDL_Delay(2);}
            require(ready,"multilevel_gpu_fell_back_to_cpu");
            if(std::string(mode)=="cpu"&&resolution.x>1280&&graphics_cpu_metrics.width<1281){require(graphics_action("ncmm.view.quality"),"multilevel_native_quality_switch_failed");render(resolution);present();}
            const double first_ready=std::chrono::duration<double,std::milli>(clock::now()-cold).count();
            std::vector<double> elapsed,extraction,visibility,geometry,raster,submit;uint64_t cache_peak=0;ncmm_gpu_metrics_v1 gpu_stats{};
            for(int sample=0;sample<(gallery?3:24);++sample){const auto start=clock::now();render(resolution);present();elapsed.push_back(std::chrono::duration<double,std::milli>(clock::now()-start).count());
                extraction.push_back(observed.provider?graphics_world_metrics.extraction_ms:graphics_snapshot_ms);visibility.push_back(observed.provider?graphics_world_metrics.visibility_ms:0);
                gpu_stats=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};geometry.push_back(graphics_gpu_frame?gpu_stats.geometry_ms:graphics_cpu_metrics.geometry_ms);raster.push_back(graphics_gpu_frame?0:graphics_cpu_metrics.raster_ms);submit.push_back(graphics_gpu_frame?gpu_stats.submit_ms:0);
                cache_peak=std::max(cache_peak,graphics_world_metrics.source_cache_bytes+gpu_stats.producer_cache_bytes+gpu_stats.resource_bytes);}
            require(you.pos_abs()==pos&&calendar::turn==time&&you.get_moves()==moves&&you.get_part_hp_cur(bodypart_id("torso"))==hp,"multilevel_render_changed_simulation");
            for(const auto &entry:discovery)require(overmap_buffer.seen(entry.first)==entry.second,"multilevel_render_revealed_overmap_discovery");
            render(resolution);auto *raw=SDL_RenderReadPixels(renderer,nullptr);auto *image=raw?SDL_ConvertSurface(raw,SDL_PIXELFORMAT_RGBA8888):nullptr;require(image!=nullptr,"multilevel_capture_failed");if(raw)SDL_DestroySurface(raw);
            const std::string filename="first-person-world-"+name+"-"+std::to_string(layers)+"-"+std::to_string(resolution.x)+"x"+std::to_string(resolution.y)+"-"+(before?"before-":"after-")+mode+".bmp";
            uint64_t fingerprint=1469598103934665603ull;for(int y=0;y<image->h;++y){const auto *row=reinterpret_cast<const uint32_t *>(static_cast<const char *>(image->pixels)+y*image->pitch);for(int x=0;x<image->w;++x){fingerprint^=row[x];fingerprint*=1099511628211ull;}}
            if(gallery)require(SDL_SaveBMP(image,(game_root()/"ncmm"/filename).string().c_str()),"multilevel_capture_save_failed");SDL_DestroySurface(image);
            report.start_object();report.member("name",name);report.member("constructed_levels",layers);report.member("theme",theme);report.member("width",resolution.x);report.member("height",resolution.y);report.member("renderer",mode);report.member("gallery",gallery);report.member("file",gallery?filename:std::string{});report.member("pixel_fingerprint",fingerprint);
            report.member("median_ms",percentile(elapsed,.5));report.member("p95_ms",percentile(elapsed,.95));report.member("p99_ms",percentile(elapsed,.99));report.member("first_ready_ms",first_ready);report.member("extraction_median_ms",percentile(extraction,.5));report.member("visibility_median_ms",percentile(visibility,.5));report.member("geometry_median_ms",percentile(geometry,.5));report.member("cpu_raster_median_ms",percentile(raster,.5));report.member("gpu_submit_median_ms",percentile(submit,.5));
            report.member("available_levels",graphics_world_metrics.available_slices);report.member("submitted_levels",observed.levels);report.member("submitted_cells",graphics_world_metrics.submitted_cells);report.member("frustum_rejected",graphics_world_metrics.frustum_rejected);report.member("occlusion_rejected",graphics_world_metrics.occlusion_rejected);report.member("budget_rejected",graphics_world_metrics.budget_rejected);report.member("source_cache_bytes",graphics_world_metrics.source_cache_bytes);report.member("source_cache_hits",graphics_world_metrics.source_cache_hits);report.member("source_cache_misses",graphics_world_metrics.source_cache_misses);report.member("snapshot_bytes",graphics_world_metrics.snapshot_bytes);report.member("cache_peak_bytes",cache_peak);
            report.member("floor_tops",observed.floors);report.member("floor_undersides",observed.bottoms);report.member("actors",observed.actors);report.member("triangles",graphics_gpu_frame?gpu_stats.triangles:graphics_cpu_metrics.triangles);report.member("draw_calls",graphics_gpu_frame?gpu_stats.draw_calls:graphics_cpu_metrics.raster_calls);report.member("cpu_raster_calls",graphics_gpu_frame?gpu_stats.cpu_raster_calls:graphics_cpu_metrics.raster_calls);report.member("actual_render_width",graphics_gpu_frame?graphics_hybrid_width:graphics_cpu_metrics.width);report.member("actual_render_height",graphics_gpu_frame?graphics_hybrid_height:graphics_cpu_metrics.height);report.member("software_device",gpu_stats.software_device);report.member("backend",gpu_stats.backend);report.member("device",gpu_stats.device);report.member("simulation_unchanged",true);report.member("discovery_unchanged",true);report.end_object();
        }
        require(SDL_SetRenderTarget(renderer,nullptr),"multilevel_release_canvas_failed");SDL_DestroyTexture(canvas);canvas=nullptr;
    };
    const std::vector<std::pair<std::string,int>> scenes={{"house_one",1},{"house_two",2},{"house_three",3},{"high_building",9},{"basement",2},{"stairs",2},{"stairs_down",2},{"stairwell",3},{"ramp",2},{"shaft",9},{"broken_floor",3},{"rooftop",2},{"bridge",2},{"lab",3},{"cave",1},{"enemy_above",2},{"enemy_below",3},{"flying",3},{"light_other_z",2},{"smoke",3},{"unknown_mod",3}};
    for(const auto &scene:scenes){make_scene(scene.first,scene.second,0);qualify(scene.first,scene.second,0,point(640,480),true);}
    const std::array<const char *,7> themes={"outdoor","urban","forest","underground","water","creatures","volumes"};
    for(int layers:{1,3,5,9})for(int theme=0;theme<7;++theme){make_scene(themes[theme],layers,theme);for(point resolution:{point(640,480),point(1280,800),point(1920,1080),point(2560,1080)})qualify(themes[theme],layers,theme,resolution,false);}
    report.end_array();report.member("success",true);report.end_object();report_stream.close();require(bool(report_stream),"multilevel_report_write_failed");
    graphics_smoke_seconds.reset();clear_monsters();you.setpos(here,here.get_bub(original_position),false);calendar::turn=original_time;you.set_moves(original_moves);get_weather().weather_id=weather_type_id("sunny");camera(original_yaw,original_pitch);refresh();
    require(SDL_SetRenderTarget(renderer,original_target),"multilevel_restore_sdl_target_failed");
    require(you.get_part_hp_cur(bodypart_id("torso"))==original_hp,"multilevel_fixture_changed_avatar_hp");
}
