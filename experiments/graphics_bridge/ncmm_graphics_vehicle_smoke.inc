// Opt-in qualification inside the real pinned CDDA Host, CI only. The caller
// supplies require(), press_f6(), frames and the initially enabled provider.
{
    require(!std::getenv("NCMM_FP_GAMEPLAY_AUDIT")&&!std::getenv("NCMM_FP_MOVEMENT_ONLY"),"vehicle_fixture_requires_exclusive_mode");
    const std::string old_mode=get_option<std::string>("NCMM_FIRST_PERSON_VEHICLES");
    const std::string old_renderer=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    SDL_Renderer *renderer=get_sdl_renderer().get();SDL_Texture *previous=SDL_GetRenderTarget(renderer);
    SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,640,480);
    int frame_width=640,frame_height=480;
    require(canvas!=nullptr,"vehicle_canvas_failed");
    struct vehicle_fixture_guard {SDL_Renderer *renderer;SDL_Texture *previous,*canvas;std::string mode,backend;
        ~vehicle_fixture_guard(){graphics_smoke_seconds.reset();graphics_smoke_vehicle_eye.reset();SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(canvas);
            get_options().get_option("NCMM_FIRST_PERSON_VEHICLES").setValue(mode);
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(backend);}
    } vehicle_guard{renderer,previous,canvas,old_mode,old_renderer};
    get_options().get_option("NCMM_FIRST_PERSON_VEHICLES").setValue("modular");
    for(int y=-48;y<=48;++y)for(int x=-48;x<=48;++x){const tripoint_bub_ms p(center.x()+x,center.y()+y,0);
        get_map().furn_set(p,furn_id("f_null"));get_map().ter_set(p,ter_id("t_pavement"));get_map().clear_fields(p);
        get_map().furn_set(tripoint_bub_ms(p.x(),p.y(),1),furn_id("f_null"));get_map().ter_set(tripoint_bub_ms(p.x(),p.y(),1),ter_id("t_open_air"));}
    // Keep CDDA's data, perception, actual part API and SDL submission. No mesh
    // preview or fixture collision replacement. The full-size qualification
    // below selects the same existing high-quality CPU setting for both modes.
    const auto street=get_map().get_abs(center);auto subject=street;std::vector<vehicle *> cars;
    const auto close_native_openables=[&](vehicle &car){
        // Native randomly opens even UNDAMAGED spawns. Give the intact
        // comparison a known state; later cases explicitly open/damage parts.
        for(const vpart_reference &p:car.get_all_parts())if(!p.part().removed&&!p.part().is_fake&&p.has_feature("OPENABLE")&&p.part().open){
            car.close(get_map(),int(p.part_index()));}
        for(const vpart_reference &p:car.get_all_parts())if(!p.part().removed&&!p.part().is_fake&&p.has_feature("OPENABLE")){
            require(!p.part().open,"vehicle_intact_baseline_not_closed");}
    };
    std::ofstream stream(game_root()/"ncmm"/"first-person-vehicles.json");JsonOut evidence(stream);
    evidence.start_object();evidence.member("schema",1);evidence.member("actual_cdda",true);evidence.member("manual_playthrough",false);
    evidence.member("intact_baseline_doors_closed",true);
    evidence.member("width",640);evidence.member("height",480);evidence.member("horizon_radius",NCMM_GRAPHICS_V2_RADIUS);
    evidence.member("frames");evidence.start_array();
    auto look=[&](float wanted,float pitch){
        graphics_mouse_dx=std::remainder(wanted-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;
        graphics_mouse_dy=(graphics_camera_pitch-pitch)/.003f;
        require(graphics_action("ncmm.view.mouse_look"),"vehicle_camera_action_failed");graphics_mouse_dx=graphics_mouse_dy=0;
    };
    auto camera=[&](int dx,int dy,float pitch,int elevation=0){
        const int z=subject.z()+elevation;if(get_map().get_abs_sub().z()!=z){g->vertical_shift(z);}
        const auto p=get_map().get_bub(tripoint_abs_ms(subject.x()+dx,subject.y()+dy,z));
        get_avatar().setpos(get_map(),p,false);g->ter_view_p=p;look(std::atan2(float(-dy),float(-dx)),pitch);
    };
    auto draw=[&](const std::string &label,const std::string &prototype,const char *backend,const char *mode,bool capture,bool refresh=true,bool record=true){
        const auto begun=std::chrono::steady_clock::now();
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(backend);
        get_options().get_option("NCMM_FIRST_PERSON_VEHICLES").setValue(mode);
        map &world=get_map();
        if(refresh){g->reset_light_level();world.invalidate_map_cache(get_avatar().posz());world.invalidate_visibility_cache();}
        world.build_map_cache(get_avatar().posz());world.update_visibility_cache(get_avatar().posz());
        std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;
        tilecontext->draw(point::zero,get_avatar().pos_bub(),frame_width,frame_height,overlays,blocks);
        require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&
            SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"vehicle_canvas_bind_failed");
        require(graphics_draw(0,0,frame_width,frame_height),"vehicle_actual_frame_failed");require(SDL_FlushRenderer(renderer),"vehicle_flush_failed");
        const bool gpu=std::strcmp(backend,"gpu")==0;
        require(!gpu||graphics_gpu_frame,"vehicle_gpu_fell_back");
        require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_RenderTexture(renderer,canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"vehicle_present_failed");
        graphics_world_frame_presented();require(SDL_SetRenderTarget(renderer,canvas),"vehicle_capture_rebind_failed");++frames;
        const double frame_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begun).count();
        if(frames%16==0){std::ofstream progress(game_root()/"ncmm"/"first-person-vehicle-progress.txt");
            progress<<"vehicles: frame="<<frames<<" scene="<<prototype<<" phase="<<label<<'\n';
            require(bool(progress),"vehicle_progress_write_failed");}
        if(!record){return frame_ms;}
        evidence.start_object();evidence.member("label",label);evidence.member("prototype",prototype);evidence.member("backend",backend);evidence.member("renderer",mode);
        evidence.member("frame_width",frame_width);evidence.member("frame_height",frame_height);
        evidence.member("camera_xyz",std::array<int,3>{get_avatar().pos_abs().x(),get_avatar().pos_abs().y(),get_avatar().pos_abs().z()});
        evidence.member("yaw",graphics_camera_yaw);evidence.member("pitch",graphics_camera_pitch);evidence.member("eye_height",graphics_presentation.eye_height);
        evidence.member("actual_gpu",graphics_gpu_frame);evidence.member("snapshot_vehicles",graphics_vehicle_rows.size());evidence.member("snapshot_parts",graphics_vehicle_parts.size());
        evidence.member("occupied_vehicle",graphics_vehicle_scene.occupied_vehicle);evidence.member("occupied_part",graphics_vehicle_scene.occupied_part);
        evidence.member("occupied_eye_valid",bool(graphics_vehicle_scene.eye_valid));
        evidence.member("occupied_eye_xy",std::array<float,2>{graphics_vehicle_scene.eye_x,graphics_vehicle_scene.eye_y});
        evidence.member("vehicle_extraction_ms",graphics_vehicle_extraction_ms);evidence.member("vehicle_preparation_ms",graphics_vehicle_metrics.preparation_ms);
        evidence.member("native_global_eoc_queue_size",g->queued_global_effect_on_conditions.list.size());
        evidence.member("vehicle_discovery_body_cells",graphics_vehicle_discovery.body_cells);evidence.member("vehicle_discovery_floor_cells",graphics_vehicle_discovery.floor_cells);
        evidence.member("vehicle_discovery_vehicles",graphics_vehicle_discovery.vehicles);evidence.member("vehicle_discovery_parts",graphics_vehicle_discovery.parts);
        evidence.member("vehicle_discovery_roof_cells",graphics_vehicle_discovery.roof_cells);evidence.member("vehicle_discovery_groups",graphics_vehicle_discovery.groups);
        evidence.member("vehicle_exterior_surface_parts",std::count_if(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[](const auto &p){return bool(p.state&NCMM_VP_EXTERIOR_SURFACE);}));
        evidence.member("vehicle_owned_bytes",graphics_vehicle_metrics.owned_bytes);evidence.member("vehicle_generated",graphics_vehicle_metrics.generated);
        evidence.member("vehicle_reused",graphics_vehicle_metrics.reused);evidence.member("vehicle_topology_builds",graphics_vehicle_metrics.topology_builds);
        evidence.member("vehicle_submitted_parts",graphics_vehicle_metrics.submitted_parts);
        evidence.member("host_world_snapshot_bytes",graphics_world_snapshot_storage_bytes());
        evidence.member("host_sprite_cache_bytes",graphics_hybrid_sprite_bytes);evidence.member("cpu_renderer_cache_bytes",graphics_cpu_metrics.cache_bytes);
        if(!gpu){
            evidence.member("cpu_width",graphics_cpu_metrics.width);evidence.member("cpu_height",graphics_cpu_metrics.height);
            evidence.member("cpu_callback_ms",graphics_cpu_metrics.frame_ms);evidence.member("cpu_geometry_ms",graphics_cpu_metrics.geometry_ms);
            evidence.member("cpu_raster_ms",graphics_cpu_metrics.raster_ms);evidence.member("cpu_effects_ms",graphics_cpu_metrics.effects_ms);
            evidence.member("cpu_raster_calls",graphics_cpu_metrics.raster_calls);evidence.member("cpu_triangles",graphics_cpu_metrics.triangles);
        }
        evidence.member("native_lights_on",std::count_if(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[](const auto &p){return (p.state&NCMM_VP_LIGHT_ON)!=0;}));
        evidence.member("full_frame_ms",frame_ms);
        if(graphics_gpu_engine){const auto metrics=graphics_gpu_engine->metrics();
            evidence.member("device",std::string(metrics.device));evidence.member("software_device",bool(metrics.software_device));
            evidence.member("gpu_draw_calls",metrics.draw_calls);evidence.member("gpu_triangles",metrics.triangles);
            evidence.member("gpu_upload_bytes",metrics.upload_bytes);evidence.member("gpu_resource_bytes",metrics.resource_bytes);
            // Existing CPU-side timers of the most recent 3D submission.
            // They may overlap and are not hardware execution/FPS timings.
            evidence.member("gpu_frame_serial",metrics.frame_serial);
            evidence.member("gpu_resource_ms",metrics.resource_ms);evidence.member("gpu_upload_ms",metrics.upload_ms);
            evidence.member("gpu_submit_ms",metrics.submit_ms);
            evidence.member("gpu_producer_cache_bytes",metrics.producer_cache_bytes);evidence.member("gpu_frame_staging_bytes",graphics_gpu_engine->recording().frame_upload_capacity_bytes);}
        if(capture){const auto name="vehicle-"+prototype+"-"+label+"-"+backend+"-"+mode+".bmp";
            SDL_Surface *image=SDL_RenderReadPixels(renderer,nullptr);require(image!=nullptr,"vehicle_capture_failed");
            const bool saved=SDL_SaveBMP(image,(game_root()/"ncmm"/name).string().c_str());SDL_DestroySurface(image);require(saved,"vehicle_capture_write_failed");evidence.member("file",name);}
        evidence.end_object();stream.flush();return frame_ms;
    };
    auto pair=[&](const std::string &label,const std::string &prototype){
        graphics_smoke_seconds=5.;
        for(const char *backend:{"cpu","gpu"})for(const char *mode:{"legacy","modular"})draw(label,prototype,backend,mode,true);
    };
    auto verify_car=[&](vehicle &v){
        const auto found=std::find_if(graphics_vehicle_rows.begin(),graphics_vehicle_rows.end(),[&](const auto &r){return r.entity_id==v.ncmm_graphics_vehicle_id();});
        require(found!=graphics_vehicle_rows.end(),"vehicle_snapshot_missing_actual_car");
        size_t count=0;std::set<uint64_t> ids;
        for(const vpart_reference &ref:v.get_all_parts())if(!ref.part().removed&&!ref.part().is_fake){++count;ids.insert(ref.part().ncmm_graphics_part_id());}
        require(count==found->part_count,"vehicle_snapshot_lost_individual_parts");
        for(uint32_t n=0;n<found->part_count;++n){const auto &p=graphics_vehicle_parts[found->first_part+n];
            require(ids.erase(p.part_id)==1,"vehicle_snapshot_false_or_duplicate_part");
            for(const vpart_reference &ref:v.get_all_parts())if(ref.part().ncmm_graphics_part_id()==p.part_id){
                require(bool(p.features&NCMM_VP_FRAME)==(ref.info().location.str()=="structure"),"vehicle_native_structure_location_not_mapped");
                require(bool(p.features&NCMM_VP_CONTROL_PANEL)==ncmm_graphics_vehicle_policy::control_panel(ref.has_feature("CTRL_ELECTRONIC"),ref.has_feature("DOME_LIGHT")),"vehicle_electronic_appliance_misidentified_as_instruments");
                require(p.mount_x==ref.part().mount.x()&&p.mount_y==ref.part().mount.y(),"vehicle_native_mount_changed");
                const auto color=curses_color_to_SDL(ref.info().color);
                require(std::abs(p.color[0]-color.r/255.f)<.00001f&&std::abs(p.color[1]-color.g/255.f)<.00001f&&std::abs(p.color[2]-color.b/255.f)<.00001f,"vehicle_native_definition_colour_changed");
            }
            require((p.state&NCMM_VP_TOPOLOGY_ONLY)?p.cell_index==-1:p.cell_index>=0,"vehicle_hidden_part_gained_geometry");}
        require(ids.empty(),"vehicle_snapshot_lost_mount_layer");
    };
    camera(0,10,-.06f);
    draw("empty","empty","cpu","legacy",false);draw("empty","empty","cpu","modular",false);
    draw("empty","empty","gpu","legacy",false);draw("empty","empty","gpu","modular",false);
    const std::array<const char *,14> prototypes{{"car","suv","cube_van","semi_truck","bus","pickup","motorcycle","bicycle","quad_bike","apc","truck_trailer","flatbed_truck","shopping_cart","canoe"}};
    for(const char *prototype:prototypes){
        vehicle *car=get_map().add_vehicle(vproto_id(prototype),get_map().get_bub(street),0_degrees,0,veh_spawn_status::UNDAMAGED,false);
        require(car!=nullptr,"vehicle_gallery_prototype_not_created");cars.push_back(car);
        close_native_openables(*car);
        int min_x=1000000,max_x=-1000000,min_y=1000000,max_y=-1000000;
        for(const vpart_reference &p:car->get_all_parts())if(!p.part().removed&&!p.part().is_fake){const auto at=get_map().get_abs(p.pos_bub(get_map()));
            min_x=std::min(min_x,at.x());max_x=std::max(max_x,at.x());min_y=std::min(min_y,at.y());max_y=std::max(max_y,at.y());}
        subject=tripoint_abs_ms((min_x+max_x)/2,(min_y+max_y)/2,street.z());
        const int radius=std::max(max_x-min_x,max_y-min_y)/2+6;
        const struct view {const char *name;int x,y;float pitch;} views[]={
            {"front",10,0,-.05f},{"rear",-10,0,-.05f},{"left",0,-10,-.05f},{"right",0,10,-.05f},
            {"front-three-quarter",9,9,-.10f},{"rear-three-quarter",-9,9,-.10f},{"top-oblique",5,5,-.58f}};
        for(const auto &v:views){const bool upper=std::strcmp(v.name,"top-oblique")==0;
            const float pitch=upper?-std::atan2(NCMM_WORLD_LEVEL_HEIGHT+graphics_eye_height()-1.f,float(radius)*std::sqrt(2.f)):v.pitch;
            camera(v.x? (v.x>0?radius:-radius):0,v.y? (v.y>0?radius:-radius):0,pitch,upper?1:0);pair(v.name,prototype);verify_car(*car);
            if(upper&&std::strcmp(prototype,"truck_trailer")==0){require(std::any_of(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[](const auto &p){return bool(p.state&NCMM_VP_EXTERIOR_SURFACE);}),"vehicle_upper_view_lost_native_exterior_shell");
                require(std::any_of(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[](const auto &p){return (p.features&NCMM_VP_WHEEL)&&!(p.state&NCMM_VP_TOPOLOGY_ONLY);}),"vehicle_upper_view_lost_exposed_native_wheels");}}
        camera(0,8,-.08f);
        if(std::strcmp(prototype,"car")!=0){
            std::vector<int> seats;for(const vpart_reference &p:car->get_all_parts())if(p.has_feature("SEAT")){seats.push_back(int(p.part_index()));}
            std::stable_sort(seats.begin(),seats.end(),[&](int a,int b){return (car->part_with_feature(car->part(a).mount,"CONTROLS",true)>=0)>(car->part_with_feature(car->part(b).mount,"CONTROLS",true)>=0);});
            for(size_t n=0;n<std::min(size_t(2),seats.size());++n){const auto pos=car->bub_part_pos(get_map(),seats[n]);
                get_avatar().setpos(get_map(),pos,false);get_map().board_vehicle(pos,&get_avatar());require(get_avatar().in_vehicle,"vehicle_family_board_failed");
                look(float(units::to_radians(car->face.dir())),-.04f);
                draw("seat-prime",prototype,"gpu","modular",false,true,false);
                require(graphics_vehicle_scene.occupied_part&&graphics_vehicle_scene.eye_valid,"vehicle_family_seat_camera_missing");graphics_smoke_vehicle_eye=graphics_vehicle_scene.eye_z;
                pair(n==0?"driver":"passenger",prototype);
                get_map().unboard_vehicle(get_avatar().pos_bub());graphics_smoke_vehicle_eye.reset();require(!get_avatar().in_vehicle,"vehicle_family_exit_failed");}
            camera(0,8,-.08f);
        }
        if(std::strcmp(prototype,"car")==0){
            int door_index=-1,wheel_index=-1,roof_index=-1,min_part_y=1000,max_part_y=-1000;std::vector<int> seats;
            for(const vpart_reference &p:car->get_all_parts()){
                if(p.part().removed||p.part().is_fake)continue;
                min_part_y=std::min(min_part_y,p.part().mount.y());max_part_y=std::max(max_part_y,p.part().mount.y());
                if(p.has_feature("OPENABLE")&&door_index<0)door_index=int(p.part_index());
                if(p.has_feature("WHEEL")&&wheel_index<0)wheel_index=int(p.part_index());
                if(p.has_feature("ROOF")&&roof_index<0)roof_index=int(p.part_index());
                if(p.has_feature("SEAT"))seats.push_back(int(p.part_index()));}
            require(door_index>=0&&wheel_index>=0&&roof_index>=0&&seats.size()>=2,"vehicle_real_fixture_parts_missing");
            std::stable_sort(seats.begin(),seats.end(),[&](int a,int b){
                return (car->part_with_feature(car->part(a).mount,"CONTROLS",true)>=0)>(car->part_with_feature(car->part(b).mount,"CONTROLS",true)>=0);});
            const int mutation_side=car->part(door_index).mount.y()*2<=min_part_y+max_part_y?-8:8;
            const auto mutation_view=[&](){camera(0,mutation_side,-.08f);};
            mutation_view();pair("mutation-before",prototype);
            car->open(get_map(),door_index);pair("open-door",prototype);
            const uint64_t opened=car->part(door_index).ncmm_graphics_part_id();
            const auto door=std::find_if(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[&](const auto &p){return p.part_id==opened;});
            require(door!=graphics_vehicle_parts.end()&&(door->state&NCMM_VP_OPEN),"vehicle_open_not_native_confirmed");
            car->close(get_map(),door_index);pair("closed-door",prototype);
            for(size_t n=0;n<2;++n){const auto pos=car->bub_part_pos(get_map(),seats[n]);
                get_avatar().setpos(get_map(),pos,false);get_map().board_vehicle(pos,&get_avatar());
                require(get_avatar().in_vehicle,"vehicle_board_failed");
                look(float(units::to_radians(car->face.dir())),-.04f);
                draw("seat-prime",prototype,"gpu","modular",false,true,false);
                require(graphics_vehicle_scene.eye_valid,"vehicle_seat_prime_missing");graphics_smoke_vehicle_eye=graphics_vehicle_scene.eye_z;
                pair(n==0?"driver":"passenger",prototype);
                require(graphics_vehicle_scene.occupied_part&&graphics_vehicle_scene.eye_valid,"vehicle_seat_camera_missing");
                get_map().unboard_vehicle(get_avatar().pos_bub());graphics_smoke_vehicle_eye.reset();camera(0,8,-.08f);require(!get_avatar().in_vehicle,"vehicle_exit_failed");}
            mutation_view();const auto hp=car->part(door_index).hp();car->set_hp(car->part(door_index),std::max(1,hp/3),true);
            pair("damaged",prototype);car->set_hp(car->part(door_index),0,true);pair("broken-door",prototype);
            car->set_hp(car->part(door_index),hp,true);
            const int wheel_side=car->part(wheel_index).mount.y()*2<=min_part_y+max_part_y?-8:8;
            camera(0,wheel_side,-.08f);pair("wheel-before",prototype);
            const auto wheel_hp=car->part(wheel_index).hp();car->set_hp(car->part(wheel_index),0,true);pair("broken-wheel",prototype);
            car->set_hp(car->part(wheel_index),wheel_hp,true);
            const uint64_t removed=car->part(wheel_index).ncmm_graphics_part_id();
            std::set<uint64_t> remaining;
            for(const vpart_reference &p:car->get_all_parts())if(!p.part().removed&&!p.part().is_fake&&p.part().ncmm_graphics_part_id()!=removed){remaining.insert(p.part().ncmm_graphics_part_id());}
            car->remove_part(car->part(wheel_index));require(car->part(wheel_index).removed,"vehicle_wheel_remove_failed");car->part_removal_cleanup(get_map());
            for(const vpart_reference &p:car->get_all_parts())if(!p.part().removed&&!p.part().is_fake){require(remaining.erase(p.part().ncmm_graphics_part_id())==1,"vehicle_vector_relocation_changed_part_identity");}
            require(remaining.empty(),"vehicle_cleanup_lost_unrelated_part_identity");
            pair("removed-wheel",prototype);verify_car(*car);
            require(std::none_of(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[&](const auto &p){return p.part_id==removed;}),"vehicle_ghost_removed_wheel");
            const int added=car->install_part(get_map(),point_rel_ms(0,0),vpart_id("rebar_plate"));
            require(added>=0,"vehicle_armor_install_failed");pair("installed-armor",prototype);verify_car(*car);
            for(const vpart_reference &p:car->get_all_parts())if(p.has_feature("CONE_LIGHT")){p.part().enabled=false;}
            get_map().set_lightmap_cache_dirty(get_avatar().posz());pair("headlights-off",prototype);
            require(std::none_of(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[](const auto &p){return (p.features&NCMM_VP_LIGHT)&&!(p.features&NCMM_VP_LIGHT_OMNI)&&(p.state&NCMM_VP_LIGHT_ON);}),"vehicle_disabled_headlight_emits");
            car->charge_battery(get_map(),10000,false);
            for(const vpart_reference &p:car->get_all_parts())if(p.has_feature("CONE_LIGHT")){p.part().enabled=true;}
            get_map().set_lightmap_cache_dirty(get_avatar().posz());pair("headlights-on",prototype);
            require(std::any_of(graphics_vehicle_parts.begin(),graphics_vehicle_parts.end(),[](const auto &p){return (p.features&NCMM_VP_LIGHT)&&!(p.features&NCMM_VP_LIGHT_OMNI)&&(p.state&NCMM_VP_LIGHT_ON);}),"vehicle_powered_headlight_missing");
            calendar::turn=calendar::turn_zero;
            if(get_avatar().has_trait(trait_id("DEBUG_NIGHTVISION"))){get_avatar().toggle_trait(trait_id("DEBUG_NIGHTVISION"));}
            camera(9,0,-.10f);pair("night-headlights",prototype);
            for(const vpart_reference &p:car->get_all_parts())if(p.has_feature("CONE_LIGHT")){p.part().enabled=false;}
            get_map().set_lightmap_cache_dirty(get_avatar().posz());pair("night",prototype);
            calendar::turn=calendar::turn_zero+12_hours;get_avatar().toggle_trait(trait_id("DEBUG_NIGHTVISION"));camera(0,8,-.08f);
            get_weather().weather_id=weather_type_id("rain");pair("rain",prototype);
            get_weather().weather_id=weather_type_id("fog");pair("fog",prototype);get_weather().weather_id=weather_type_id("sunny");
            {
                // Exercise actual Native definition colours, as authored mod
                // parts do. Both renderers see the same temporary definitions.
                std::map<vpart_info *,nc_color> paints;
                for(const vpart_reference &part:car->get_all_parts())if(part.has_feature("ROOF")||part.has_feature("HALF_BOARD")||
                    (part.has_feature("OBSTACLE")&&!part.has_feature("WINDOW")&&!part.has_feature("DOOR"))){
                    auto &info=const_cast<vpart_info &>(part.info());paints.emplace(&info,info.color);}
                require(!paints.empty(),"vehicle_native_paint_definitions_missing");
                struct native_paint_restore {std::map<vpart_info *,nc_color> &values;
                    ~native_paint_restore(){for(const auto &entry:values)entry.first->color=entry.second;}} restore{paints};
                for(const auto &entry:paints){entry.first->color=c_red;}pair("paint-red",prototype);verify_car(*car);
                for(const auto &entry:paints){entry.first->color=c_blue;}pair("paint-blue",prototype);verify_car(*car);
            }
        }
        get_map().destroy_vehicle(car);cars.clear();subject=street;camera(0,10,-.06f);draw("deleted",prototype,"gpu","modular",false);
        require(graphics_vehicle_rows.empty()&&graphics_vehicle_parts.empty(),"vehicle_delete_retained_snapshot");
    }
    // Mixed parking and repeated dynamic observations; actual Native moves,
    // not camera motion pretending to be vehicle travel.
    for(int n=0;n<5;++n){auto *v=get_map().add_vehicle(vproto_id(n%2?"suv":"car"),get_map().get_bub(tripoint_abs_ms(street.x()+n*8-16,street.y()-6,street.z())),0_degrees,0,veh_spawn_status::UNDAMAGED,false);
        require(v!=nullptr,"vehicle_parking_create_failed");cars.push_back(v);}
    camera(0,12,-.10f);pair("parking","mixed");
    for(unsigned n=0;n<128;++n){const int sign=n%2?-1:1;
        require(get_map().displace_vehicle(*cars[n%cars.size()],tripoint_rel_ms(sign,0,0)),"vehicle_real_move_failed");
        graphics_smoke_seconds=6.+n*.05;
        draw("moving-"+std::to_string(n),"mixed",n%2?"cpu":"gpu","modular",n<4);
    }
    pair("after-motion","mixed");
    // A full Native quit/setup/load destroys the old mapbuffer lifetimes.
    // Saved vehicles and every installed layer must receive fresh runtime IDs.
    std::set<uint64_t> before_load_vehicles,before_load_parts;
    for(const auto &entry:get_map().get_vehicles()){
        before_load_vehicles.insert(entry.v->ncmm_graphics_vehicle_id());
        for(const vpart_reference &p:entry.v->get_all_parts())if(!p.part().removed&&!p.part().is_fake){before_load_parts.insert(p.part().ncmm_graphics_part_id());}}
    vehicle *retired=get_map().add_vehicle(vproto_id("bicycle"),get_map().get_bub(tripoint_abs_ms(street.x(),street.y()+5,street.z())),0_degrees,0,veh_spawn_status::UNDAMAGED,false);
    require(retired!=nullptr,"vehicle_retired_fixture_create_failed");
    const auto retired_identity=retired->ncmm_graphics_vehicle_id();
    draw("before-retire","mixed","gpu","modular",false);verify_car(*retired);
    get_map().destroy_vehicle(retired);
    const auto save=save_t::from_save_id(get_avatar().get_save_id());
    require(g->save(),"vehicle_native_save_failed");cars.clear();
    require(g->ncmm_graphics_smoke_vehicle_reload(save),"vehicle_native_reload_failed");
    ncmm::graphics_note_input_context("DEFAULTMODE");if(!graphics_view.enabled)press_f6();
    camera(0,12,-.10f);pair("save-reload","mixed");
    require(graphics_vehicle_rows.size()==5,"vehicle_reload_lost_live_snapshot");
    require(std::none_of(graphics_vehicle_rows.begin(),graphics_vehicle_rows.end(),[&](const auto &v){return v.entity_id==retired_identity;}),"vehicle_reload_reused_dead_identity");
    for(const auto &entry:get_map().get_vehicles()){
        require(!before_load_vehicles.count(entry.v->ncmm_graphics_vehicle_id()),"vehicle_full_reload_kept_old_object_identity");
        for(const vpart_reference &p:entry.v->get_all_parts())if(!p.part().removed&&!p.part().is_fake){require(!before_load_parts.count(p.part().ncmm_graphics_part_id()),"vehicle_full_reload_kept_old_part_identity");}
        verify_car(*entry.v);}
    for(const auto &v:get_map().get_vehicles())get_map().destroy_vehicle(v.v);
    draw("world-empty","empty","gpu","modular",false);require(graphics_vehicle_parts.empty(),"vehicle_world_cleanup_failed");
    #include "ncmm_graphics_vehicle_qualification.inc"
    evidence.end_array();evidence.member("success",true);evidence.member("native_parts_verified",true);evidence.member("native_driving_verified",true);
    evidence.member("native_reload_lifecycle","quit_cleanup_setup_load");evidence.member("hardware_performance_qualified",false);
    evidence.end_object();stream.close();require(bool(stream),"vehicle_evidence_write_failed");
    press_f6();require(!graphics_view.enabled,"vehicle_f6_restore_failed");
}
