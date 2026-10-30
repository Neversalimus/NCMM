// Runs in the same real Host/process and owned world as the vehicle gallery.
// No extra local installation, resolution-based optimization or preview mesh.
{
    require(SDL_SetRenderTarget(renderer,nullptr),"vehicle_fullsize_unbind_failed");
    SDL_DestroyTexture(canvas);frame_width=1280;frame_height=800;
    canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,frame_width,frame_height);
    vehicle_guard.canvas=canvas;require(canvas!=nullptr,"vehicle_fullsize_canvas_failed");
    const auto clear_cars=[&](){cars.clear();for(const auto &v:get_map().get_vehicles()){get_map().destroy_vehicle(v.v);}};
    const auto create=[&](const char *type,int dx,int dy){
        auto *v=get_map().add_vehicle(vproto_id(type),get_map().get_bub(tripoint_abs_ms(street.x()+dx,street.y()+dy,street.z())),0_degrees,0,veh_spawn_status::UNDAMAGED,false);
        require(v!=nullptr,"vehicle_qualification_create_failed");cars.push_back(v);close_native_openables(*v);return v;
    };
    subject=street;camera(0,14,-.1f);graphics_smoke_seconds=10.;
    for(const std::string scene:{"empty","one","multiple","parking","large"}){
        clear_cars();
        if(scene=="one"){create("car",0,0);}
        if(scene=="multiple"){create("car",-8,0);create("suv",0,0);create("pickup",8,0);}
        if(scene=="parking"){for(int n=0;n<5;++n){create(n%2?"suv":"car",n*8-16,0);}}
        if(scene=="large"){create("bus",0,0);}
        camera(0,14,-.1f);
        for(const char *backend:{"cpu","gpu"})for(unsigned pair_index=0;pair_index<7;++pair_index){
            for(unsigned slot=0;slot<2;++slot){const char *mode=((pair_index+slot)%2)?"modular":"legacy";
                const std::string name="bench-"+scene+"-"+std::to_string(pair_index)+"-";
                // Measure a construction-cold renderer switch, followed by
                // seven unchanged warm frames. Do not call device-cold startup
                // or these hosted software-adapter values hardware FPS.
                draw("bench-prepare",scene,backend,std::strcmp(mode,"modular")==0?"legacy":"modular",false,false,false);
                draw(name+"cold",scene,backend,mode,false,true);
                for(unsigned sample=0;sample<7;++sample){draw(name+"warm-"+std::to_string(sample),scene,backend,mode,false,false);}
            }
        }
        if(scene!="empty"){pair("fullsize",scene);}
    }
    clear_cars();vehicle *custom=create("car",0,0);
    for(int x=3;x<=5;++x){require(custom->install_part(get_map(),point_rel_ms(x,0),vpart_id("frame"))>=0,"vehicle_custom_frame_install_failed");}
    require(custom->install_part(get_map(),point_rel_ms(5,0),vpart_id("wheel_mount_medium"))>=0,"vehicle_custom_mount_install_failed");
    require(custom->install_part(get_map(),point_rel_ms(5,0),vpart_id("wheel"))>=0,"vehicle_custom_wheel_install_failed");
    require(custom->install_part(get_map(),point_rel_ms(4,0),vpart_id("roof"))>=0,"vehicle_custom_roof_install_failed");
    camera(8,10,-.12f);pair("asymmetric-extension","custom");verify_car(*custom);
    // Use the same Native rotation/precalc/displacement sequence as vehicle
    // movement. The fixture does not replace collision/simulation in gameplay.
    custom->turn(30_degrees);custom->precalc_mounts(1,custom->turn_dir,custom->pivot_point(get_map()));
    require(get_map().displace_vehicle(*custom,tripoint_rel_ms(1,0,0)-custom->pivot_displacement()),"vehicle_custom_turn_displacement_failed");
    custom->face.init(custom->turn_dir);pair("turned","custom");verify_car(*custom);
    // Reload the installed atlas through CDDA, with live custom geometry and
    // both renderer paths still active. The old texture epoch must not leak.
    const auto loaded_tileset=get_option<std::string>("TILES");
    const auto previous_tileset_epoch=tilecontext->ncmm_graphics_tileset_epoch();
    tilecontext->load_tileset(loaded_tileset,false,true);
    require(tilecontext->is_valid()&&tilecontext->ncmm_graphics_tileset_id()==loaded_tileset&&tilecontext->ncmm_graphics_tileset_epoch()!=previous_tileset_epoch,"vehicle_actual_tileset_reload_failed");
    ncmm::graphics_note_input_context("DEFAULTMODE");pair("tileset-reload","custom");verify_car(*custom);
    // Test original Native actor start/finish costs and exact stable identities,
    // including vector relocation between start and completion.
    clear_cars();vehicle *door_car=create("car",0,0);int door_index=-1;
    for(const vpart_reference &p:door_car->get_all_parts())if(p.has_feature("DOOR")){
        if(p.part().open){door_car->close(get_map(),int(p.part_index()));}
        if(door_index<0||p.part().mount.y()<door_car->part(door_index).mount.y()){door_index=int(p.part_index());}
    }
    require(door_index>=0,"vehicle_activity_door_missing");
    const auto door_position=door_car->bub_part_pos(get_map(),door_index);
    get_avatar().setpos(get_map(),door_position+tripoint_rel_ms(0,-1,0),false);g->ter_view_p=get_avatar().pos_bub();
    const auto at=get_map().get_abs(door_position);const auto vehicle_id=door_car->ncmm_graphics_vehicle_id(),part_id=door_car->part(door_index).ncmm_graphics_part_id();
    door_car->part(door_index).locked=false;
    bool picked=false;
    for(unsigned probe=0;probe<10&&!picked;++probe){look(ncmm_graphics_camera::pi*.5f,-.3f-.1f*probe);
        draw("activity-pick-probe","car","gpu","modular",false,true,false);
        picked=graphics_vehicle_focus_entity==vehicle_id&&graphics_vehicle_focus_part==part_id;}
    require(picked,"vehicle_actual_displayed_door_pick_missing");draw("activity-source","car","gpu","modular",true);
    graphics_vehicle_activity_request={true,true,graphics_vehicle_focus_entity,graphics_vehicle_focus_part,graphics_vehicle_scene.world_epoch,at.x(),at.y(),at.z()};
    open_tile_activity_actor open_actor(door_position);player_activity open_activity(open_actor);open_actor.start(open_activity,get_avatar());
    require(open_activity.moves_total==to_moves<int>(1_seconds)&&!door_car->part(door_index).open,"vehicle_open_native_cost_changed");
    open_actor.finish(open_activity,get_avatar());require(door_car->part(door_index).open,"vehicle_exact_open_activity_failed");
    draw("activity-open","car","gpu","modular",true);
    graphics_vehicle_activity_request={true,false,vehicle_id,part_id,graphics_vehicle_scene.world_epoch,at.x(),at.y(),at.z()};
    close_tile_activity_actor close_actor(door_position);player_activity close_activity(close_actor);close_actor.start(close_activity,get_avatar());
    require(close_activity.moves_total==90&&door_car->part(door_index).open,"vehicle_close_native_cost_changed");
    close_actor.finish(close_activity,get_avatar());require(!door_car->part(door_index).open,"vehicle_exact_close_activity_failed");
    // A removed pending target must never open a replacement at the same mount.
    draw("activity-closed","car","gpu","modular",false);
    graphics_vehicle_activity_request={true,true,vehicle_id,part_id,graphics_vehicle_scene.world_epoch,at.x(),at.y(),at.z()};
    open_tile_activity_actor stale_actor(door_position);player_activity stale_activity(stale_actor);stale_actor.start(stale_activity,get_avatar());
    require(!stale_activity.is_null(),"vehicle_stale_activity_was_never_started");
    door_car->remove_part(door_car->part(door_index));door_car->part_removal_cleanup(get_map());
    stale_actor.finish(stale_activity,get_avatar());require(stale_activity.is_null(),"vehicle_stale_activity_not_cancelled");
    clear_cars();
    // Exercise Native driving, not a render-only pose or an OS input injection.
    // The real driver command, engine start and vehmove() own speed, collision,
    // steering and passenger relocation. The module only observes their result.
    vehicle *driven=get_map().add_vehicle(vproto_id("car"),get_map().get_bub(street),0_degrees,100,veh_spawn_status::UNDAMAGED,false);
    require(driven!=nullptr,"vehicle_driving_create_failed");cars.push_back(driven);int driver_seat=-1;
    close_native_openables(*driven);
    for(const vpart_reference &p:driven->get_all_parts())if(p.has_feature("SEAT")&&driven->part_with_feature(p.part().mount,"CONTROLS",true)>=0){driver_seat=int(p.part_index());break;}
    require(driver_seat>=0,"vehicle_driving_controls_missing");
    const auto driver_position=driven->bub_part_pos(get_map(),driver_seat);
    get_avatar().setpos(get_map(),driver_position,false);get_map().board_vehicle(driver_position,&get_avatar());
    require(get_avatar().in_vehicle,"vehicle_driving_board_failed");
    bool started_engine=false;
    for(const vpart_reference &p:driven->get_all_parts())if(p.has_feature("ENGINE")){p.part().enabled=true;started_engine|=driven->start_engine(get_map(),p.part());}
    require(started_engine,"vehicle_driving_native_engine_start_failed");driven->engine_on=true;
    // This scenario tests driving, not random spawn locks or hotwiring menus.
    // Native's ordinary control_vehicle() must still grant control itself.
    driven->set_owner(get_avatar());driven->is_locked=false;driven->is_alarm_on=false;get_avatar().set_wielded_item(item());
    g->ncmm_graphics_smoke_controls(get_avatar().pos_bub());require(get_avatar().controlling_vehicle,"vehicle_driving_native_controls_failed");
    const auto start_position=driven->pos_abs();const auto start_heading=driven->face.dir();bool changed_heading=false;
    for(unsigned step=0;step<8;++step){
        get_avatar().set_moves(100);driven->pldrive(get_map(),get_avatar(),step>=4?1:0,step==0?-1:0);get_map().vehmove();
        require(get_avatar().in_vehicle&&get_avatar().controlling_vehicle,"vehicle_driving_lost_driver");
        require(get_avatar().pos_bub()==driven->bub_part_pos(get_map(),driver_seat),"vehicle_driving_passenger_left_actual_seat");
        changed_heading|=driven->face.dir()!=start_heading;g->ter_view_p=get_avatar().pos_bub();
        look(float(units::to_radians(driven->face.dir())),-.04f);
        draw("driving-prime","car","gpu","modular",false,true,false);
        require(!graphics_world_transition_active,"vehicle_driving_used_avatar_walking_interpolation");
        require(graphics_vehicle_scene.eye_valid&&graphics_vehicle_scene.occupied_vehicle==driven->ncmm_graphics_vehicle_id()&&
            graphics_vehicle_scene.occupied_part==driven->part(driver_seat).ncmm_graphics_part_id(),"vehicle_driving_camera_lost_actual_seat");
        graphics_smoke_vehicle_eye=graphics_vehicle_scene.eye_z;pair("driving-"+std::to_string(step),"car");verify_car(*driven);
    }
    require(driven->pos_abs()!=start_position&&changed_heading,"vehicle_native_driving_did_not_move_and_turn");
    driven->stop(get_map());driven->stop_engines(get_map());get_map().unboard_vehicle(get_avatar().pos_bub());graphics_smoke_vehicle_eye.reset();
    require(!get_avatar().in_vehicle&&!get_avatar().controlling_vehicle,"vehicle_driving_exit_failed");
    clear_cars();for(int n=0;n<3;++n){create(n%2?"suv":"car",n*9-9,0);}subject=street;camera(0,14,-.1f);
    const int requested_seconds=std::getenv("NCMM_FP_VEHICLE_SOAK_SECONDS")?std::clamp(std::atoi(std::getenv("NCMM_FP_VEHICLE_SOAK_SECONDS")),0,1800):0;
    const auto started=std::chrono::steady_clock::now();unsigned iteration=0,saves=0,loads=0,mutations=0;
    while(std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<requested_seconds){
        vehicle *v=cars[iteration%cars.size()];
        if(iteration%16==0){v->turn(15_degrees);v->precalc_mounts(1,v->turn_dir,v->pivot_point(get_map()));}
        else{v->precalc_mounts(1,v->face.dir(),v->ncmm_graphics_vehicle_pivot());}
        require(get_map().displace_vehicle(*v,tripoint_rel_ms(iteration%2?-1:1,0,0)-v->pivot_displacement()),"vehicle_soak_move_failed");
        if(iteration%16==0){v->face.init(v->turn_dir);}
        if(iteration%64==0){for(const vpart_reference &p:v->get_all_parts())if(p.has_feature("DOOR")&&p.part().is_available()){
            if(p.part().open){v->close(get_map(),int(p.part_index()));}else{v->open(get_map(),int(p.part_index()));}++mutations;break;}}
        if(iteration%128==0){int armor=-1;for(const vpart_reference &p:v->get_all_parts())if(p.part().info().id.str()=="rebar_plate"){armor=int(p.part_index());break;}
            if(armor>=0){v->remove_part(v->part(armor));v->part_removal_cleanup(get_map());}
            else{require(v->install_part(get_map(),point_rel_ms(0,0),vpart_id("rebar_plate"))>=0,"vehicle_soak_install_failed");}++mutations;}
        if(iteration%384==383){press_f6();require(!graphics_view.enabled,"vehicle_soak_f6_disable_failed");press_f6();require(graphics_view.enabled,"vehicle_soak_f6_enable_failed");}
        graphics_smoke_seconds=20.+iteration*.05;
        look(-ncmm_graphics_camera::pi*.5f+.22f*std::sin(iteration*.05f),-.08f);
        draw("soak-"+std::to_string(iteration),"mixed",(iteration/64)%2?"cpu":"gpu","modular",false,true,iteration%32==0);
        require(graphics_vehicle_metrics.owned_bytes<128u*1024u*1024u,"vehicle_soak_unbounded_module_memory");
        if(iteration%256==255){const auto saved=save_t::from_save_id(get_avatar().get_save_id());require(g->save(),"vehicle_soak_save_failed");++saves;cars.clear();
            require(g->ncmm_graphics_smoke_vehicle_reload(saved),"vehicle_soak_reload_failed");++loads;
            for(const auto &entry:get_map().get_vehicles()){cars.push_back(entry.v);}require(cars.size()==3,"vehicle_soak_reload_lost_car");
            ncmm::graphics_note_input_context("DEFAULTMODE");if(!graphics_view.enabled){press_f6();}}
        require(++iteration<65536,"vehicle_soak_iteration_bound_exceeded");
    }
    // Continue the outer frame array with a compact, explicit long-run record.
    evidence.start_object();evidence.member("label","soak-summary");evidence.member("prototype","mixed");evidence.member("backend",graphics_gpu_frame?"gpu":"cpu");evidence.member("renderer","modular");
    evidence.member("actual_gpu",graphics_gpu_frame);evidence.member("requested_seconds",requested_seconds);
    evidence.member("elapsed_seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count());
    evidence.member("iterations",iteration);evidence.member("saves",saves);evidence.member("loads",loads);evidence.member("mutations",mutations);evidence.end_object();
    clear_cars();camera(0,14,-.1f);draw("qualification-cleanup","empty","gpu","modular",false);
    require(graphics_vehicle_rows.empty(),"vehicle_qualification_cleanup_failed");
}
