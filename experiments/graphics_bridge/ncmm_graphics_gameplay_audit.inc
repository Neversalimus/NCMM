// Opt-in, bounded automation in an owned smoke world. This is not manual play.
{
    [&](){
    using audit_clock=std::chrono::steady_clock;
    const auto original_position=you.pos_abs(),home=original_position;
    const auto original_origin=here.get_abs_sub();
    const int original_moves=you.get_moves();const float original_yaw=graphics_camera_yaw,original_pitch=graphics_camera_pitch;
    // Match run_game_loop before calling do_turn: the earlier API smoke only
    // constructs an avatar and deliberately skips game::start_game.
    g->ncmm_graphics_smoke_prepare_turns();
    const auto native_ui=g->create_or_get_main_ui_adaptor();ui_manager::redraw();
    SDL_Renderer *renderer=get_sdl_renderer().get();SDL_Texture *previous_target=SDL_GetRenderTarget(renderer);
    const bool viewmodel_gallery=std::getenv("NCMM_FP_VIEWMODEL_GALLERY")!=nullptr;
    point audit_viewport=viewmodel_gallery?point(1280,800):point(640,480);
    SDL_Texture *canvas=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,audit_viewport.x,audit_viewport.y);
    require(canvas!=nullptr,"audit_canvas_failed");
    struct audit_canvas_guard {SDL_Renderer *renderer;SDL_Texture *canvas,*previous;
        ~audit_canvas_guard(){SDL_SetRenderTarget(renderer,previous);SDL_DestroyTexture(canvas);if(graphics_gpu_engine)graphics_gpu_engine->profile(false);}
    } canvas_guard{renderer,canvas,previous_target};
    const auto milliseconds=[](auto begin){return std::chrono::duration<double,std::milli>(audit_clock::now()-begin).count();};
    const auto percentile=[](std::vector<double> v,double p){if(v.empty())return 0.0;std::sort(v.begin(),v.end());const double at=p*(v.size()-1);const size_t a=size_t(at),b=std::min(a+1,v.size()-1);return v[a]+(v[b]-v[a])*(at-a);};
    auto point_at=[&](int x,int y,int z=0){return here.get_bub(tripoint_abs_ms(home.x()+x,home.y()+y,z));};
    auto camera=[&](float yaw,float pitch){graphics_mouse_dx=std::remainder(yaw-graphics_camera_yaw,2*ncmm_graphics_camera::pi)/.003f;graphics_mouse_dy=(graphics_camera_pitch-pitch)/.003f;
        require(graphics_action("ncmm.view.mouse_look"),"audit_camera_action_failed");graphics_mouse_dx=graphics_mouse_dy=0;};
    auto clear_monsters=[&](){const auto all=get_creature_tracker().get_monsters_list();for(const auto &mon:all)get_creature_tracker().remove(*mon);};
    auto refresh=[&](){g->ter_view_p=you.pos_bub();g->reset_light_level();here.invalidate_map_cache(you.posz());here.build_map_cache(you.posz());here.invalidate_visibility_cache();here.update_visibility_cache(you.posz());};
    auto floor=[&](){clear_monsters();for(const auto &veh:here.get_vehicles())here.destroy_vehicle(veh.v);
        for(int z=-1;z<=2;++z)for(int y=-24;y<=24;++y)for(int x=-24;x<=24;++x){const auto p=point_at(x,y,z);if(!here.ncmm_graphics_loaded(p))continue;
            here.ter_set(p,ter_id(z<=0?"t_floor":"t_open_air"));here.furn_set(p,furn_id("f_null"));here.clear_fields(p);here.i_clear(p);here.remove_trap(p);
            if(npc *n=get_creature_tracker().creature_at<npc>(p)){const auto id=n->getID();g->remove_npc(id);overmap_buffer.remove_npc(id);}}
        you.setpos(here,point_at(0,0),false);
        // Native vertical_move reads the map's current Z, not avatar.posz().
        // Diagnostic teleport/setup after a stair must restore both domains.
        if(here.get_abs_sub().z()!=you.posz())g->vertical_shift(you.posz());
        you.set_moves(10000);g->set_safe_mode(SAFE_MODE_OFF);graphics_clear_world();graphics_clear_creatures();camera(-ncmm_graphics_camera::pi*.5f,-.08f);refresh();};
    // UI construction/redraw may pump and discard a prequeued key. Feed the
    // normal SDL queue after the menu has entered its own input loop instead.
    // The worker touches no game state and is joined before leaving the scope.
    struct cancel_native_ui {
        std::atomic<bool> stop{false};std::thread worker;
        explicit cancel_native_ui(SDL_WindowID window):worker([this,window](){
            for(int tick=0;tick<1500&&!stop.load();++tick){SDL_Delay(10);if(tick%20!=19||stop.load())continue;
                SDL_Event event={};event.type=SDL_EVENT_KEY_DOWN;event.key.windowID=window;event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;SDL_PushEvent(&event);
                event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);}
        }){}
        ~cancel_native_ui(){stop.store(true);worker.join();SDL_FlushEvent(SDL_EVENT_KEY_DOWN);SDL_FlushEvent(SDL_EVENT_KEY_UP);}
    };
    std::ofstream progress_stream(game_root()/"ncmm"/"first-person-audit-progress.txt");std::string phase="initialization";
    auto checkpoint=[&](const std::string &name){phase=name;progress_stream<<SDL_GetTicks()<<" "<<name<<" frames="<<frames<<'\n';progress_stream.flush();};
    auto f12=[&](){press_view_key(SDLK_F12);};
    std::ofstream stream(game_root()/"ncmm"/"first-person-gameplay-audit.json");JsonOut report(stream);
    report.start_object();report.member("schema",1);report.member("actual_cdda_sdl",true);report.member("manual_playthrough",false);report.member("gpu_execution_timestamps_available",false);
    report.member("fixture","controlled native actions in an isolated generated smoke world");report.member("width",audit_viewport.x);report.member("height",audit_viewport.y);
    auto write_stages=[&](const std::map<std::string,std::vector<double>> &measurements){report.start_object();for(const auto &[name,samples]:measurements){if(samples.empty())continue;report.member(name);report.start_object();report.member("samples",samples.size());report.member("median",percentile(samples,.5));report.member("p95",percentile(samples,.95));report.member("p99",percentile(samples,.99));report.member("max",*std::max_element(samples.begin(),samples.end()));report.end_object();}report.end_object();};
    std::map<std::string,std::vector<double>> stages;bool audit_measurements=true;
    struct combat_trial {int spawned=0,turns=0,shots=0,melee=0,observed_enemy_attacks=0;};std::vector<combat_trial> combat_trials;
    uint64_t resource_peak=0,sprite_peak=0,world_peak=0,rss_peak=0,native_atlas_peak=0;std::string capture_name;
    auto render=[&](){
        const bool measured=capture_name.empty()&&audit_measurements;
        const auto pos=you.pos_abs();const auto turn=calendar::turn;const int moves=you.get_moves(),hp=you.get_part_hp_cur(bodypart_id("torso"));
        const auto begin=audit_clock::now();refresh();const double native_cache=milliseconds(begin);
        std::multimap<point,formatted_text> overlays;color_block_overlay_container blocks;
        const auto tiles=audit_clock::now();
        tilecontext->draw(point::zero,you.pos_bub(),audit_viewport.x,audit_viewport.y,overlays,blocks);
        const double native_tiles=milliseconds(tiles);
        require(SDL_SetRenderTarget(renderer,canvas)&&SDL_SetRenderLogicalPresentation(renderer,0,0,SDL_LOGICAL_PRESENTATION_DISABLED)&&SDL_SetRenderScale(renderer,1,1)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr),"audit_canvas_rebind_failed");
        if(graphics_gpu_engine)graphics_gpu_engine->profile();
        const auto overlay=audit_clock::now();require(graphics_draw(0,0,audit_viewport.x,audit_viewport.y),"audit_real_render_failed");const double overlay_ms=milliseconds(overlay);
        if(!capture_name.empty()){auto *image=SDL_RenderReadPixels(renderer,nullptr);require(image!=nullptr,"audit_capture_failed");const bool saved=SDL_SaveBMP(image,(game_root()/"ncmm"/capture_name).string().c_str());SDL_DestroySurface(image);require(saved,"audit_capture_write_failed");capture_name.clear();}
        const auto present=audit_clock::now();require(SDL_SetRenderTarget(renderer,nullptr)&&SDL_SetRenderViewport(renderer,nullptr)&&SDL_SetRenderClipRect(renderer,nullptr)&&SDL_RenderTexture(renderer,canvas,nullptr,nullptr)&&SDL_RenderPresent(renderer),"audit_actual_present_failed");graphics_world_frame_presented();
        if(graphics_gpu_frame&&graphics_gpu_engine){const auto m=graphics_gpu_engine->metrics();require(m.active&&!m.cpu_raster_calls,"audit_gpu_used_cpu_raster");resource_peak=std::max(resource_peak,m.resource_bytes);}
        if(measured){
        stages["full_frame_wall_ms"].push_back(milliseconds(begin));stages[graphics_gpu_frame?"gpu_full_frame_wall_ms":"cpu_full_frame_wall_ms"].push_back(milliseconds(begin));stages["native_cache_cpu_ms"].push_back(native_cache);stages["native_tiles_cpu_ms"].push_back(native_tiles);stages["perspective_callback_wall_ms"].push_back(overlay_ms);stages["actual_present_wall_ms"].push_back(milliseconds(present));
        stages["world_extract_cpu_ms"].push_back(graphics_world_metrics.extraction_ms);stages["world_visibility_cpu_ms"].push_back(graphics_world_metrics.visibility_ms);stages["ui_composition_record_cpu_ms"].push_back(graphics_presentation_ms);
        if(graphics_gpu_frame&&graphics_gpu_engine){const auto m=graphics_gpu_engine->metrics();const auto &p=graphics_gpu_engine->recording();
            stages["gpu_geometry_prepare_cpu_ms"].push_back(m.geometry_ms);stages["gpu_resource_update_cpu_ms"].push_back(m.resource_ms);stages["gpu_upload_record_cpu_ms"].push_back(m.upload_ms);stages["gpu_commands_record_cpu_ms"].push_back(m.render_record_ms);stages["gpu_submit_cpu_ms"].push_back(m.submit_ms);
            stages["gpu_batching_pick_cpu_ms"].push_back(p.batching_pick_ms);stages["gpu_shadow_commands_cpu_ms"].push_back(p.shadow_ms);stages["gpu_world_draw_commands_cpu_ms"].push_back(p.world_ms);stages["gpu_creature_draw_commands_cpu_ms"].push_back(p.creature_ms);stages["gpu_held_draw_commands_cpu_ms"].push_back(p.held_ms);stages["gpu_volume_commands_cpu_ms"].push_back(p.volume_ms);stages["gpu_compose_commands_cpu_ms"].push_back(p.compose_ms);
        }else{stages["cpu_geometry_ms"].push_back(graphics_cpu_metrics.geometry_ms);stages["cpu_raster_ms"].push_back(graphics_cpu_metrics.raster_ms);stages["cpu_effects_ms"].push_back(graphics_cpu_metrics.effects_ms);}
        }
        sprite_peak=std::max(sprite_peak,uint64_t(graphics_hybrid_sprite_bytes));world_peak=std::max(world_peak,graphics_world_sources.bytes());
        require(graphics_hybrid_sprite_bytes<=64*1024*1024&&graphics_world_sources.bytes()<8*1024*1024&&resource_peak<=NCMM_GPU_RESOURCE_LIMIT,"audit_renderer_memory_budget_exceeded");
        require(ncmm_native_atlas::retained_bytes()<=ncmm_native_atlas::byte_limit,"audit_native_atlas_memory_budget_exceeded");
        native_atlas_peak=std::max(native_atlas_peak,uint64_t(ncmm_native_atlas::retained_bytes()));
        require(you.pos_abs()==pos&&you.get_moves()==moves&&calendar::turn==turn&&you.get_part_hp_cur(bodypart_id("torso"))==hp,"audit_renderer_changed_native_state");++frames;
        if(frames%32==0)checkpoint(phase);
    };
    auto warm=[&](const char *mode){const auto begin=audit_clock::now();
        struct measurement_guard {bool &flag;bool previous;~measurement_guard(){flag=previous;}} restoring{audit_measurements,audit_measurements};audit_measurements=false;
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);bool ready=false;
        for(int n=0;n<180;++n){render();ready=std::string(mode)=="cpu"||graphics_gpu_frame;if(ready&&!graphics_creature_metrics.queued&&!graphics_creature_metrics.fallback)break;SDL_Delay(2);}
        require(ready&&!graphics_creature_metrics.queued&&!graphics_creature_metrics.fallback,"audit_models_or_gpu_never_ready");return milliseconds(begin);};
    auto capture=[&](const std::string &name){capture_name="first-person-audit-"+name+".bmp";render();};
    auto settle=[&](){for(int n=0;n<100&&graphics_world_transition_active;++n){SDL_Delay(5);render();}require(!graphics_world_transition_active,"audit_camera_never_settled");};
    // The scope covers only the real turn. Setup, reporting and the explicit
    // render after an empty turn stay outside it. Callback wall time is an
    // inclusive outermost union; the remainder is not a pure AI measurement.
    struct turn_sample {int initial_enemies,round;std::string requested_renderer;
        ncmm_graphics_turn_diagnostics::turn_record timing;};
    std::vector<turn_sample> native_turn_samples;native_turn_samples.reserve(26);
    auto measured_native_turn=[&](int initial_enemies,int round,const char *failure){
        turn_sample sample{initial_enemies,round,get_option<std::string>("NCMM_FIRST_PERSON_RENDERER"),{}};
        bool ended=false;
        {ncmm_graphics_turn_diagnostics::turn_capture capture(sample.timing);
            ended=g->do_turn();capture.complete();}
        require(!ended,failure);
        require(ncmm_graphics_turn_diagnostics::valid(sample.timing),"audit_native_turn_capture_invalid");
        const auto &t=sample.timing;
        const double wall=t.wall_ns/1000000.0,draw=t.draw_wall_ns/1000000.0;
        const std::string group=initial_enemies?"native_turn_"+std::to_string(initial_enemies)+"_enemies":"native_turn_empty";
        stages[initial_enemies?"combat_native_turn_wall_ms":"native_game_turn_wall_ms"].push_back(wall);
        stages[group+"_wall_ms"].push_back(wall);
        stages[group+"_perspective_union_wall_ms"].push_back(draw);
        stages[group+"_other_wall_ms"].push_back((t.wall_ns-t.draw_wall_ns)/1000000.0);
        native_turn_samples.push_back(std::move(sample));
    };
    floor();warm("cpu");
    report.member("actions");report.start_array();
    auto action=[&](const char *name,const std::function<void()> &run){checkpoint(name);const auto begin=audit_clock::now();
        const auto native_errors=capture_debugmsg_during(run);
        if(!native_errors.empty())log_line(NCMM_LOG_ERROR,("Native audit diagnostic: "+native_errors).c_str());
        require(native_errors.empty(),"audit_native_debug_message");
        report.start_object();report.member("name",name);report.member("native_action_wall_ms",milliseconds(begin));report.member("success",true);report.end_object();stream.flush();checkpoint(std::string(name)+" PASS");};
    auto target_cases=[&](){
        #include "ncmm_graphics_target_smoke.inc"
    };
    if(std::getenv("NCMM_FP_TARGET_HUD_ONLY")){
        action("native_target_health_identity_visibility_and_font",target_cases);report.end_array();
        require(here.get_abs_sub()==original_origin,"target_hud_probe_changed_map_origin");
        you.set_moves(original_moves);camera(original_yaw,original_pitch);clear_monsters();graphics_clear_world();
        report.member("diagnostic_only",true);report.member("target_gallery_only",true);
        report.member("ordinary_performance_measurement",false);report.member("success",true);report.end_object();
        stream.close();require(bool(stream),"target_hud_probe_report_write_failed");checkpoint("target_hud_gallery_complete");return;
    }
    auto fps_cases = [&]() {
        #include "ncmm_graphics_fps_audit.inc"
    };
    if(std::getenv("NCMM_FP_FPS_INPUT_ONLY")) {
        action("native_fps_input_and_actions",fps_cases);
        action("native_fps_renderer_hotkey_cycle",[&](){for(int i=0;i<3;++i)f12();});
        report.end_array();
        you.set_moves(original_moves);camera(original_yaw,original_pitch);clear_monsters();graphics_clear_world();
        report.member("diagnostic_only",true);report.member("ordinary_performance_measurement",false);
        report.member("success",true);report.end_object();stream.close();
        require(bool(stream),"fps_input_only_report_write");return;
    }
    action("native_held_assets_and_shared_psx_contacts",[&](){graphics_run_held_gallery(you,here,require,frames);floor();warm("cpu");});
    action("native_firing_ui_context_and_presented_frames",[&](){
        #include "ncmm_graphics_fire_audit.inc"
    });
    action("native_fps_input_and_actions",fps_cases);
    action("native_target_health_identity_visibility_and_font",target_cases);
    action("native_walk_and_visual_continuity",[&](){
        for(const tripoint_rel_ms step:{tripoint_rel_ms(1,0,0),tripoint_rel_ms(0,-1,0),tripoint_rel_ms(-1,0,0),tripoint_rel_ms(0,1,0)}){
            const auto before=you.pos_abs();const int moves=you.get_moves();require(avatar_action::move(you,here,step),"audit_native_walk_failed");const auto after=you.pos_abs();const int spent=moves-you.get_moves();
            require(after.x()==before.x()+step.x()&&after.y()==before.y()+step.y()&&spent>0,"audit_native_walk_cost_or_position_wrong");render();
            const auto offset=graphics_world_camera_motion.sample(SDL_GetTicks());require(std::abs(offset.x+step.x())<.04f&&std::abs(offset.y+step.y())<.04f,"audit_walking_camera_discontinuity");settle();
            require(you.pos_abs()==after&&you.get_moves()==moves-spent,"audit_walking_easing_spent_native_moves");}
    });
    action("native_door_open_close_and_furniture",[&](){const auto door=point_at(0,-1);here.ter_set(door,ter_id("t_door_c"));render();require(here.open_door(you,door,true),"audit_door_open_failed");render();require(here.close_door(door,true,false),"audit_door_close_failed");render();
        here.ter_set(door,ter_id("t_floor"));const auto chair=point_at(1,-1);here.furn_set(chair,furn_id("f_chair"));{cancel_native_ui cancel(SDL_GetWindowID(get_sdl_window()));g->ncmm_graphics_smoke_examine(chair);}ncmm::graphics_note_input_context("DEFAULTMODE");capture("furniture");});
    action("native_pickup_inventory_cancel_and_item_use",[&](){you.set_wielded_item(item());you.set_moves(10000);const auto p=you.pos_bub();item &rock=here.add_item_or_charges(p,item(itype_id("rock"),calendar::turn));
        std::vector<item_location> targets{item_location(map_cursor(p),&rock)};std::vector<int> quantities{0};bool stashed=false;Pickup::pick_info info;
        checkpoint("native_pickup_begin");require(Pickup::do_pickup(targets,quantities,true,stashed,info)&&targets.empty()&&here.i_at(p).empty(),"audit_native_pickup_failed");
        checkpoint("native_inventory_cancel_begin");{cancel_native_ui cancel(SDL_GetWindowID(get_sdl_window()));game_menus::inv::common();}checkpoint("native_inventory_cancel_returned");ncmm::graphics_note_input_context("DEFAULTMODE");render();
        checkpoint("native_flashlight_battery_prepare");item light(itype_id("flashlight"),calendar::turn),cell(itype_id("medium_battery_cell"),calendar::turn);
        const auto charge=cell.ammo_default();require(!charge.is_null(),"audit_native_battery_ammo_missing");cell.ammo_set(charge,20);
        require(cell.ammo_remaining()==20&&light.put_in(cell,pocket_type::MAGAZINE_WELL).success()&&light.ammo_remaining()==20,"audit_native_battery_install_failed");
        you.set_wielded_item(light);item_location loc=you.get_wielded_item();checkpoint("native_flashlight_activation_begin");avatar_action::use_item(you,loc,"transform");
        require(you.get_wielded_item()&&you.get_wielded_item()->typeId()==itype_id("flashlight_on"),"audit_native_item_activation_failed");render();
    });
    action("native_melee_aim_fire_and_reload_completion",[&](){clear_monsters();const auto p=point_at(0,-1);monster *target=g->place_critter_at(mtype_id("mon_zombie"),p);require(target!=nullptr,"audit_combat_spawn_failed");
        you.set_wielded_item(item(itype_id("knife_combat"),calendar::turn));camera(-ncmm_graphics_camera::pi*.5f,-.3f);warm("cpu");require(graphics_aim.has_value()&&graphics_aim_name==target->disp_name(),"audit_reticle_selected_wrong_creature");
        const auto serial=graphics_attack_serial;const int moves=you.get_moves();you.melee_attack(*target,false);require(graphics_attack_serial>serial&&graphics_attack_kind==1&&you.get_moves()<moves,"audit_native_melee_or_animation_missing");capture("melee");
        if(target->is_dead_state()){get_creature_tracker().remove(*target);target=g->place_critter_at(mtype_id("mon_zombie"),p);require(target!=nullptr,"audit_second_combat_spawn_failed");}
        item revolver(itype_id("sw_619"),calendar::turn);revolver.ammo_set(itype_id("38_special"),3);you.set_wielded_item(revolver);you.set_moves(10000);item &gun=*you.get_wielded_item();
        const int ammo=gun.ammo_remaining();const auto shot=graphics_attack_serial;const int fired=you.fire_gun(here,target->pos_bub(),1,gun);
        require(fired==1&&gun.ammo_remaining()==ammo-1&&graphics_attack_serial>shot&&graphics_attack_kind==2,"audit_native_shot_or_animation_missing");capture("shot");
        item rounds(itype_id("38_special"),calendar::turn);rounds.charges=2;item &placed=here.add_item_or_charges(you.pos_bub(),rounds);item_location source(map_cursor(you.pos_bub()),&placed);
        require(gun.reload(you,source,1)&&gun.ammo_remaining()==ammo,"audit_native_reload_completion_failed");render();
        here.ter_set(point_at(0,-1),ter_id("t_wall"));monster *hidden=g->place_critter_at(mtype_id("mon_zombie"),point_at(0,-3));require(hidden!=nullptr,"audit_occlusion_spawn_failed");refresh();require(!you.sees(here,*hidden),"audit_native_wall_did_not_occlude");render();
        for(const auto &cr:graphics_creature_snapshots)require(cr.world_x!=hidden->pos_abs().x()||cr.world_y!=hidden->pos_abs().y()||cr.world_z!=hidden->pos_abs().z(),"audit_hidden_enemy_identity_published");
        here.ter_set(point_at(0,-1),ter_id("t_floor"));clear_monsters();graphics_clear_creatures();camera(-ncmm_graphics_camera::pi*.5f,-.08f);
    });
    action("native_stairs_and_basement",[&](){you.setpos(here,point_at(0,0),false);graphics_clear_world();render();
        for(int direction:{1,-1}){here.ter_set(point_at(0,0),ter_id(direction>0?"t_stairs_up":"t_stairs_down"));here.ter_set(point_at(0,0,direction),ter_id(direction>0?"t_stairs_down":"t_stairs_up"));refresh();render();
            for(int delta:{direction,-direction}){const int z=you.posz(),moves=you.get_moves();const float eye=z*2.5f+graphics_eye_height();const auto begin=audit_clock::now();g->vertical_move(delta,false);require(you.posz()==z+delta&&you.get_moves()<moves,"audit_native_stair_move_failed");
                const auto pos=you.pos_abs();const int spent=moves-you.get_moves();render();const float shown=you.posz()*2.5f+graphics_eye_height()+graphics_world_camera_motion.sample(SDL_GetTicks()).z;
                require(std::abs(shown-eye)<.1f,"audit_stair_first_frame_discontinuous");
                require(std::abs(graphics_world_ray_eye.z-(shown-you.posz()*NCMM_WORLD_LEVEL_HEIGHT))<.1f,"audit_stair_visibility_detached_from_display_eye");
                stages["native_stair_first_ready_wall_ms"].push_back(milliseconds(begin));settle();require(you.pos_abs()==pos&&you.get_moves()==moves-spent,"audit_stair_easing_changed_native_cost");}}
        here.ter_set(point_at(0,0),ter_id("t_floor"));here.ter_set(point_at(0,0,1),ter_id("t_floor"));you.set_base_height(400);render();require(graphics_world_ray_eye.z>0&&graphics_world_ray_eye.z<=NCMM_WORLD_LEVEL_HEIGHT-.17f,"audit_giant_camera_passed_ceiling");you.set_base_height(175);here.ter_set(point_at(0,0,1),ter_id("t_open_air"));
    });
    action("rapid_tall_avatar_stairs_respect_scene_bounds",[&](){
        here.ter_set(point_at(0,0),ter_id("t_stairs_up"));here.ter_set(point_at(0,0,1),ter_id("t_ncmm_x9_stairs_both"));here.ter_set(point_at(0,0,2),ter_id("t_stairs_down"));here.ter_set(point_at(0,0,3),ter_id("t_floor"));you.set_base_height(400);graphics_clear_world();render();
        for(int delta:{1,1,-1,-1}){const int before=you.posz();g->vertical_move(delta,false);require(you.posz()==before+delta,"audit_rapid_tall_native_stair_move_failed");render();require(graphics_view.enabled&&std::abs(graphics_world_camera_motion.sample(SDL_GetTicks()).z)<=5,"audit_rapid_tall_stairs_disabled_renderer");}
        settle();you.set_base_height(175);here.ter_set(point_at(0,0),ter_id("t_floor"));for(int z:{1,2,3})here.ter_set(point_at(0,0,z),ter_id("t_open_air"));graphics_clear_world();render();
    });
    action("native_vehicle_board_controls_displacement_and_exit",[&](){you.set_wielded_item(item());vehicle *car=here.add_vehicle(vproto_id("car"),point_at(5,-4),0_degrees,100,veh_spawn_status::UNDAMAGED,false);require(car!=nullptr,"audit_vehicle_spawn_failed");car->set_owner(you);car->engine_on=true;bool checked=false;
        // The spawned car's random lock state depends on preceding combat RNG.
        // This fixture tests Native driving, not the hotwire menu. The ordinary
        // control_vehicle pathway still has to grant control itself.
        checkpoint(std::string("native_vehicle_fixture_initially_locked_")+std::to_string(car->is_locked));
        car->is_locked=false;car->is_alarm_on=false;
        for(const vpart_reference &part:car->get_all_parts())if(part.has_feature("SEAT")&&car->avail_part_with_feature(part.mount_pos(),"CONTROLS")>=0){const auto seat=part.pos_bub(here);you.setpos(here,seat,false);here.board_vehicle(seat,&you);render();require(you.in_vehicle&&graphics_presentation.occupied_vehicle&&graphics_presentation.eye_height<=1.18f,"audit_vehicle_eye_or_boarding_failed");
            {cancel_native_ui cancel(SDL_GetWindowID(get_sdl_window()));g->ncmm_graphics_smoke_controls(you.pos_bub());}require(you.controlling_vehicle,"audit_native_vehicle_controls_failed");
            // Match map::move_vehicle's mount/pivot preparation before using
            // its lower-level displacement operation in this owned fixture.
            car->precalc_mounts(1,car->face.dir(),car->pivot_point(here));
            const auto displacement=tripoint_rel_ms(1,0,0)-car->pivot_displacement();const auto before=you.pos_abs();
            require(here.displace_vehicle(*car,displacement),"audit_native_vehicle_displacement_failed");render();
            require(you.pos_abs().x()==before.x()+1&&you.pos_abs().y()==before.y()&&you.pos_abs().z()==before.z(),"audit_native_vehicle_passenger_not_displaced");
            require(!graphics_world_transition_active,"audit_vehicle_motion_used_walking_offset");capture("vehicle");
            here.unboard_vehicle(you.pos_bub());you.controlling_vehicle=false;require(!you.in_vehicle,"audit_native_vehicle_exit_failed");checked=true;break;}
        require(checked,"audit_vehicle_driver_seat_missing");here.destroy_vehicle(car);you.setpos(here,point_at(0,0),false);g->update_map(you);clear_monsters();graphics_clear_world();render();
    });
    action("native_combat_1_5_20_ai_and_render_state_guards",[&](){
        // Controlled combat protects this disposable avatar with an existing
        // vanilla debug trait. Enemy AI, attacks and damage to targets remain
        // native; this does not establish ordinary-game combat balance.
        const trait_id protection("DEBUG_NODMG");require(protection.is_valid(),"audit_combat_protection_trait_missing");if(!you.has_trait(protection))you.toggle_trait(protection);
        for(int count:{1,5,20}){floor();combat_trial trial;trial.spawned=count;std::set<uint64_t> enemy_attack_events;
            for(int i=0;i<count;++i)require(g->place_critter_at(mtype_id("mon_zombie"),point_at(count==1?0:i%5-2,-1-i/5))!=nullptr,"audit_combat_group_spawn_failed");
            item revolver(itype_id("sw_619"),calendar::turn);revolver.ammo_set(itype_id("38_special"),7);you.set_wielded_item(revolver);warm("cpu");warm("gpu");
            for(int round=0;round<6;++round){
                // Fixed starting state for each scripted round: the audit
                // measures native attacks/render guards, not fatigue/knockdown
                // attrition. Enemy actions still run below; player games do not.
                you.clear_effects();you.set_movement_mode(move_mode_id("walk"));
                you.set_stamina(you.get_stamina_max());
                const auto targets=get_creature_tracker().get_monsters_list();
                if(round&&!targets.empty()){auto nearest=targets.front();for(const auto &m:targets)if(!m->is_dead_state()&&rl_dist(you.pos_bub(),m->pos_bub())<rl_dist(you.pos_bub(),nearest->pos_bub()))nearest=m;
                    if(!nearest->is_dead_state()){if(rl_dist(you.pos_bub(),nearest->pos_bub())<=1){const auto weapon=you.get_wielded_item();require(weapon&&you.attack_speed(*weapon)<=300,"audit_combat_weapon_requires_interactive_confirmation");you.melee_attack(*nearest,false);++trial.melee;}
                        if(!nearest->is_dead_state()&&you.get_wielded_item()&&you.get_wielded_item()->ammo_remaining()>0)trial.shots+=you.fire_gun(here,nearest->pos_bub(),1,*you.get_wielded_item());}}
                checkpoint("native_combat_"+std::to_string(count)+"_turn_"+std::to_string(round));you.set_moves(0);measured_native_turn(count,round,"audit_combat_native_turn_ended_game");++trial.turns;
                for(const auto &entry:graphics_creature_records)if(entry.second.snapshot.event==NCMM_CR_MELEE)enemy_attack_events.insert(entry.second.snapshot.event_serial);
                trial.observed_enemy_attacks=int(enemy_attack_events.size());
                warm("cpu");warm("gpu");
            }
            require(trial.observed_enemy_attacks>0,"audit_combat_enemy_ai_never_attacked");capture("combat-"+std::to_string(count));combat_trials.push_back(trial);clear_monsters();you.clear_effects();you.set_movement_mode(move_mode_id("walk"));you.recalc_sight_limits();
        }
        if(you.has_trait(protection))you.toggle_trait(protection);
        floor();warm("cpu");
    });
    action("native_turn_processing",[&](){clear_monsters();for(int i=0;i<8;++i){you.set_moves(0);measured_native_turn(0,i,"audit_native_turn_ended_game");render();}});
    action("native_f6_and_f12_dispatch",[&](){const auto pos=you.pos_abs();const int moves=you.get_moves();const auto turn=calendar::turn;press_f6();require(!graphics_view.enabled&&graphics_world_sources.bytes()==0,"audit_f6_cache_not_released");press_f6();
        const std::string before=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");for(int n=0;n<3;++n){f12();render();}require(get_option<std::string>("NCMM_FIRST_PERSON_RENDERER")==before,"audit_f12_cycle_not_restored");require(you.pos_abs()==pos&&you.get_moves()==moves&&calendar::turn==turn,"audit_toggle_changed_native_state");
    });
    report.end_array();report.member("native_action_stages");write_stages(stages);report.member("combat_trials");report.start_array();for(const auto &t:combat_trials){report.start_object();report.member("spawned",t.spawned);report.member("native_turns",t.turns);report.member("native_shots",t.shots);report.member("native_melee",t.melee);report.member("observed_enemy_attacks",t.observed_enemy_attacks);report.member("avatar_debug_protection",true);report.member("avatar_stamina_reset_each_turn",true);report.member("avatar_combat_state_reset_each_turn",true);report.end_object();}report.end_array();
    require(native_turn_samples.size()==26,"audit_native_turn_samples_missing");
    report.member("native_turn_diagnostics");report.start_object();report.member("schema",1);
    report.member("controlled_fixture_without_interactive_main_loop",true);
    report.member("ordinary_performance_measurement",false);
    report.member("draw_wall_scope","union of outermost graphics_draw callbacks inside do_turn");
    report.member("remainder_scope","other Native work, UI, waits and scheduling; not pure AI");
    report.member("samples");report.start_array();for(const auto &s:native_turn_samples){const auto &t=s.timing;
        report.start_object();report.member("initial_enemies",s.initial_enemies);report.member("round",s.round);
        report.member("requested_renderer",s.requested_renderer);report.member("valid",ncmm_graphics_turn_diagnostics::valid(t));
        report.member("wall_ns",t.wall_ns);report.member("draw_wall_ns",t.draw_wall_ns);report.member("other_wall_ns",t.wall_ns-t.draw_wall_ns);
        report.member("draw_calls",t.draw_calls);report.member("outer_draws",t.outer_draws);report.member("max_depth",t.max_depth);
        report.member("cpu_draws",t.cpu_draws);report.member("gpu_draws",t.gpu_draws);report.member("legacy_draws",t.legacy_draws);report.member("skipped_draws",t.skipped_draws);
        report.member("errors",t.errors);report.member("claimed",t.claimed);report.member("returned",t.returned);report.member("closed",t.closed);report.end_object();
    }report.end_array();report.end_object();
    const std::string original_tileset=get_option<std::string>("TILES");std::vector<std::string> tilesets{original_tileset};if(std::getenv("NCMM_FP_UDP"))tilesets.push_back("UndeadPeople_0J_Hybrid_v3");
    report.member("tilesets");report.start_array();
    for(size_t ti=0;ti<tilesets.size();++ti){const auto &tileset=tilesets[ti];checkpoint("load_tileset_"+tileset);const auto reload_begin=audit_clock::now();get_options().get_option("TILES").setValue(tileset);tilecontext->load_tileset(tileset,false,true);
        require(tilecontext->is_valid()&&tilecontext->ncmm_graphics_tileset_id()==tileset,"audit_wrong_loaded_tileset");const double reload_ms=milliseconds(reload_begin);floor();
        report.start_object();report.member("id",tileset);report.member("reload_wall_ms",reload_ms);report.member("actual_loaded_id",tilecontext->ncmm_graphics_tileset_id());
        if(viewmodel_gallery){
            report.member("viewmodel_gallery");report.start_array();
            const std::vector<std::string> mutations={"FUR","SCALES","CHITIN","CLAWS","WEBBED"};
            auto gallery=[&](const std::string &name,const std::function<void(const char *)> &setup,int delay=300){
                for(const char *mode:{"cpu","gpu"}){checkpoint(tileset+"_gallery_"+name+"_"+mode);
                    if(you.in_vehicle)here.unboard_vehicle(you.pos_bub());
                    you.clear_worn();you.clear_bionics();
                    for(const auto &id:mutations){const trait_id trait(id);require(trait.is_valid(),"gallery_mutation_id_invalid");if(you.has_trait(trait))you.toggle_trait(trait);}
                    floor();you.set_wielded_item(item());get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);setup(mode);warm(mode);SDL_Delay(delay);
                    const std::string stem="gallery-"+std::to_string(ti)+"-"+name+"-"+mode;capture(stem);
                    require(std::string(mode)!="gpu"||graphics_gpu_frame,"gallery_gpu_fell_back");
                    report.start_object();report.member("name",name);report.member("renderer",mode);report.member("capture","first-person-audit-"+stem+".bmp");
                    report.member("width",audit_viewport.x);report.member("height",audit_viewport.y);report.member("internal_width",graphics_hybrid_width);report.member("internal_height",graphics_hybrid_height);
                    report.member("held",graphics_hybrid_last_environment.held_item);report.member("gun_kind",graphics_presentation.gun_kind);report.member("gunmods",graphics_presentation.gunmods);
                    report.member("left_glove",graphics_presentation.glove[0]);report.member("right_glove",graphics_presentation.glove[1]);report.member("left_sleeve",graphics_presentation.sleeve[0]);report.member("right_sleeve",graphics_presentation.sleeve[1]);
                    report.member("hand_features",graphics_presentation.hand_features);report.member("arm_features",graphics_presentation.arm_features);report.member("attack_serial",graphics_attack_serial);report.member("attack_kind",graphics_attack_kind);
                    report.member("occupied_vehicle",bool(graphics_presentation.occupied_vehicle));report.member("pitch",graphics_camera_pitch);report.member("yaw",graphics_camera_yaw);
                    const auto m=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
                    report.member("device",m.device);report.member("backend",m.backend);report.member("software_device",bool(m.software_device));report.member("gpu",graphics_gpu_frame);report.end_object();stream.flush();}}
            ;
            auto wield=[&](const char *id){const itype_id type(id);require(type.is_valid(),"gallery_item_id_invalid");you.set_wielded_item(item(type,calendar::turn));};
            gallery("bare",[&](const char *){});
            gallery("black_gloves",[&](const char *){require(you.wear_item(item(itype_id("gloves_tactical"),calendar::turn),false).has_value(),"gallery_gloves_failed");});
            gallery("sleeves",[&](const char *){require(you.wear_item(item(itype_id("jacket_leather"),calendar::turn),false).has_value(),"gallery_sleeves_failed");});
            for(const auto &entry:std::vector<std::pair<std::string,std::string>>{{"pistol","glock_19"},{"revolver","sw_619"},{"rifle","modular_m27_assault_rifle"},{"shotgun","remington_870"},{"knife","knife_combat"},{"pole","naginata"},{"flashlight","flashlight"},{"umbrella","umbrella"},{"torch","torch"}})
                gallery(entry.first,[&](const char *){wield(entry.second.c_str());});
            gallery("rifle_attachments",[&](const char *){wield("modular_m27_assault_rifle");auto held=you.get_wielded_item();
                for(const char *id:{"rifle_scope","suppressor","grip"})held->force_insert_item(item(itype_id(id),calendar::turn),pocket_type::MOD);});
            gallery("unarmed_attack",[&](const char *mode){monster *target=g->place_critter_at(mtype_id("mon_zombie"),point_at(-1,-1));require(target,"gallery_punch_target_missing");warm(mode);
                const auto serial=graphics_attack_serial;you.melee_attack(*target,false);require(graphics_attack_serial>serial&&graphics_attack_kind==1,"gallery_native_punch_missing");},110);
            gallery("shot_recoil",[&](const char *mode){wield("sw_619");auto gun=you.get_wielded_item();gun->ammo_set(itype_id("38_special"),3);
                monster *target=g->place_critter_at(mtype_id("mon_zombie"),point_at(0,-4));require(target,"gallery_shot_target_missing");warm(mode);
                const auto serial=graphics_attack_serial;const int ammo=gun->ammo_remaining();require(you.fire_gun(here,target->pos_bub(),1,*gun)==1&&gun->ammo_remaining()==ammo-1&&graphics_attack_serial>serial&&graphics_attack_kind==2,"gallery_native_shot_missing");},80);
            gallery("item_switch",[&](const char *mode){wield("knife_combat");warm(mode);SDL_Delay(260);render();wield("modular_m27_assault_rifle");},100);
            gallery("camera_turn",[&](const char *){wield("glock_19");camera(-.45f,.1f);});
            gallery("look_up",[&](const char *){wield("glock_19");camera(-ncmm_graphics_camera::pi*.5f,1.f);});
            gallery("look_down",[&](const char *){wield("glock_19");camera(-ncmm_graphics_camera::pi*.5f,-1.f);});
            gallery("vehicle",[&](const char *){vehicle *car=here.add_vehicle(vproto_id("car"),point_at(5,-4),0_degrees,100,veh_spawn_status::UNDAMAGED,false);require(car,"gallery_car_missing");bool boarded=false;
                for(const auto &part:car->get_all_parts())if(part.has_feature("SEAT")&&car->avail_part_with_feature(part.mount_pos(),"CONTROLS")>=0){const auto seat=part.pos_bub(here);you.setpos(here,seat,false);here.board_vehicle(seat,&you);boarded=true;break;}
                require(boarded&&you.in_vehicle,"gallery_board_failed");wield("glock_19");});
            gallery("near_wall",[&](const char *){wield("knife_combat");for(int x=-1;x<=1;++x)here.ter_set(point_at(x,-1),ter_id("t_wall"));here.ter_set(point_at(-1,0),ter_id("t_wall"));here.ter_set(point_at(1,0),ter_id("t_wall"));});
            for(const auto &id:mutations)gallery("mutation_"+id,[&](const char *){you.toggle_trait(trait_id(id));});
            gallery("bionic",[&](const char *){you.add_bionic(bionic_id("bio_armor_arms"));you.add_bionic(bionic_id("bio_razors"));});
            if(you.in_vehicle)here.unboard_vehicle(you.pos_bub());
            you.clear_worn();you.clear_bionics();
            for(const auto &id:mutations)if(you.has_trait(trait_id(id)))you.toggle_trait(trait_id(id));
            you.set_wielded_item(item());floor();report.end_array();
        }
        report.member("cases");report.start_array();
        for(const auto &name:std::vector<std::string>{"city","dense_smoke","multi_level","creatures_1","creatures_5","creatures_20","creatures_50","creatures_100","large_and_flying","lod_distances","unknown_mod_objects","rotation"}){
            floor();
            if(name=="city"||name=="multi_level")for(int z=0;z<(name=="multi_level"?3:1);++z)for(int y=-20;y<=-4;++y)for(int x=-15;x<=15;++x){const auto p=point_at(x,y,z);here.ter_set(p,ter_id((x%10==0||y%8==0)?"t_wall":"t_floor"));if(x%10==0&&y%8==4)here.ter_set(p,ter_id("t_window"));}
            if(name=="dense_smoke")for(int y=-16;y<-2;++y)for(int x=-8;x<=8;++x)here.add_field(point_at(x,y),field_type_str_id("fd_smoke").id(),3);
            int count=name=="creatures_100"?100:name=="creatures_50"?50:name=="creatures_20"?20:name=="creatures_5"?5:name=="creatures_1"?1:0;
            for(int i=0;i<count;++i)require(g->place_critter_at(mtype_id(i%2?"mon_zombie":"mon_dog"),point_at(-9+i%10*2,-3-i/10*2))!=nullptr,"audit_group_spawn_failed");
            if(name=="large_and_flying"){require(g->place_critter_at(mtype_id("mon_zombie_hulk"),point_at(-2,-5))!=nullptr,"audit_large_spawn_failed");require(g->place_critter_at(mtype_id("mon_crow"),point_at(2,-5,1))!=nullptr,"audit_flying_spawn_failed");camera(-ncmm_graphics_camera::pi*.5f,.2f);}
            if(name=="lod_distances"){for(int distance:{2,12,40})require(g->place_critter_at(mtype_id("mon_zombie"),point_at(distance==2?-1:distance==12?0:1,-distance))!=nullptr,"audit_lod_spawn_failed");require(g->place_critter_at(mtype_id("mon_crow"),point_at(-2,-40))!=nullptr,"audit_lod_bird_spawn_failed");}
            if(name=="unknown_mod_objects"){here.ter_set(point_at(-1,-4),ter_id("t_ncmm_x9_unknown_support"));here.furn_set(point_at(1,-4),furn_id("f_ncmm_x9_unknown_furniture"));}
            for(const char *mode:{"cpu","gpu"}){checkpoint(tileset+"_"+name+"_"+mode);if(name=="rotation")camera(-ncmm_graphics_camera::pi*.5f,-.08f);
                const double first_ready=warm(mode);stages.clear();const auto cold_rss=ncmm_graphics_performance::resident_bytes();
                std::map<std::string,std::vector<double>> continuous_stages;std::vector<uint32_t> continuous_renderers;double recovery_ready=0;
                if(name=="rotation"){
                    checkpoint(tileset+"_rotation_continuous_"+mode);
                    for(int n=0;n<24;++n){camera(-ncmm_graphics_camera::pi*.5f+n*.12f,.15f*std::sin(n*.3f));render();continuous_renderers.push_back(graphics_gpu_frame?1u:0u);}
                    continuous_stages=std::move(stages);stages.clear();recovery_ready=warm(mode);
                    checkpoint(tileset+"_rotation_warmed_poses_"+mode);
                }
                for(int n=0;n<24;++n){if(name=="rotation"){camera(-ncmm_graphics_camera::pi*.5f+n*.12f,.15f*std::sin(n*.3f));stages["rotation_pose_ready_wall_ms"].push_back(warm(mode));}render();if(std::string(mode)=="gpu")require(graphics_gpu_frame,"audit_stable_gpu_fell_back");}
                capture(std::to_string(ti)+"-"+name+"-"+mode);report.start_object();report.member("name",name);report.member("renderer",mode);report.member("first_ready_wall_ms",first_ready);report.member("spawned_creatures",count);report.member("visible_creatures",graphics_creature_snapshots.size());report.member("lod",std::array<uint32_t,4>{graphics_creature_metrics.lod[0],graphics_creature_metrics.lod[1],graphics_creature_metrics.lod[2],graphics_creature_metrics.lod[3]});report.member("model_fallback",graphics_creature_metrics.fallback);report.member("resolved_sprites",graphics_resolved_textures);report.member("sprite_cache_bytes",graphics_hybrid_sprite_bytes);report.member("creature_cache_bytes",graphics_creature_metrics.cache_bytes);report.member("process_resident_before",cold_rss);report.member("process_resident_after",ncmm_graphics_performance::resident_bytes());
                const auto m=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};report.member("device",m.device);report.member("backend",m.backend);report.member("software_device",bool(m.software_device));report.member("stages");write_stages(stages);
                if(name=="rotation"){report.member("continuous_rotation");report.start_object();report.member("requested_renderer",mode);report.member("angle_step_radians",.12);report.member("frame_renderers_gpu",continuous_renderers);report.member("frame_wall_samples",continuous_stages["full_frame_wall_ms"]);report.member("gpu_frames",std::count(continuous_renderers.begin(),continuous_renderers.end(),1u));report.member("cpu_frames",std::count(continuous_renderers.begin(),continuous_renderers.end(),0u));report.member("recovery_first_ready_wall_ms",recovery_ready);report.member("stages");write_stages(continuous_stages);report.end_object();}
                report.end_object();stream.flush();}
        }
        report.end_array();report.end_object();stream.flush();
    }
    report.end_array();
    const char *duration=std::getenv("NCMM_FP_SOAK_SECONDS");const long soak_seconds=duration?std::strtol(duration,nullptr,10):0;require(soak_seconds>=0&&soak_seconds<=3600,"audit_soak_duration_invalid");
    checkpoint("automated_soak");const auto simulation_time=calendar::turn;floor();warm("gpu");stages.clear();const auto soak_begin=audit_clock::now();uint64_t soak_frames=0,toggles=0,switches=0,map_loads=0,saves=0;
    const auto rss_begin=ncmm_graphics_performance::resident_bytes();std::vector<uint64_t> rss_samples;std::vector<uint32_t> frame_renderers;
    while(milliseconds(soak_begin)<soak_seconds*1000.0){
        const auto loop_begin=audit_clock::now();
        if(soak_frames&&soak_frames%64==0){press_f6();require(!graphics_view.enabled&&graphics_world_sources.bytes()==0,"audit_soak_f6_retained_cache");press_f6();++toggles;}
        if(soak_frames&&soak_frames%96==0){f12();++switches;calendar::turn=simulation_time+(switches%2?12_hours:0_hours);get_weather().weather_id=weather_type_id(switches%2?"rain":"sunny");}
        if(soak_frames&&soak_frames%256==0){const auto begin=audit_clock::now();clear_monsters();here.load(tripoint_abs_sm(map_loads%2?0:12,0,0),true);const tripoint_bub_ms at(60,60,0);
            if(const auto vp=here.veh_at(at))here.destroy_vehicle(&vp->vehicle());
            here.ter_set(at,ter_id("t_pavement"));here.furn_set(at,furn_id("f_null"));here.clear_fields(at);here.remove_trap(at);
            you.setpos(here,at,false);graphics_clear_world();++map_loads;stages["native_map_load_wall_ms"].push_back(milliseconds(begin));}
        if(soak_frames&&soak_frames%512==0){const auto begin=audit_clock::now();require(g->save(),"audit_soak_native_save_failed");++saves;stages["native_save_wall_ms"].push_back(milliseconds(begin));}
        camera(graphics_camera_yaw+.035f,.12f*std::sin(float(soak_frames)*.02f));render();
        frame_renderers.push_back(graphics_gpu_frame?1u:0u);
        if(soak_frames%32==0){const auto rss=ncmm_graphics_performance::resident_bytes();rss_samples.push_back(rss);rss_peak=std::max(rss_peak,rss);}
        stages["soak_loop_with_native_work_wall_ms"].push_back(milliseconds(loop_begin));++soak_frames;SDL_Delay(10);
    }
    report.member("soak");report.start_object();report.member("automated",true);report.member("requested_seconds",soak_seconds);report.member("actual_wall_seconds",milliseconds(soak_begin)/1000);report.member("frames",soak_frames);report.member("f6_cycles",toggles);report.member("f12_switches",switches);report.member("map_loads",map_loads);report.member("native_saves",saves);report.member("process_rss_begin",rss_begin);report.member("process_rss_end",ncmm_graphics_performance::resident_bytes());report.member("process_rss_peak",rss_peak);report.member("rss_samples",rss_samples);report.member("frame_wall_samples",stages["full_frame_wall_ms"]);report.member("loop_wall_samples",stages["soak_loop_with_native_work_wall_ms"]);report.member("frame_renderers_gpu",frame_renderers);report.member("gpu_resource_peak",resource_peak);report.member("sprite_cache_peak",sprite_peak);report.member("native_atlas_cache_peak",native_atlas_peak);report.member("native_atlas_encoding_ms_cumulative",ncmm_native_atlas::encoding_ms());report.member("world_cache_peak",world_peak);report.member("stages");write_stages(stages);report.end_object();
    calendar::turn=simulation_time;here.load(original_origin,true);g->vertical_shift(original_position.z());you.setpos(here,here.get_bub(original_position),false);you.set_moves(original_moves);camera(original_yaw,original_pitch);clear_monsters();graphics_clear_world();
    get_options().get_option("TILES").setValue(original_tileset);tilecontext->load_tileset(original_tileset,false,true);ncmm::graphics_note_input_context("DEFAULTMODE");get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue("cpu");
    report.member("success",true);report.end_object();stream.close();require(bool(stream),"audit_report_write_failed");checkpoint("native_multilevel_save_load_next");
    log_line(NCMM_LOG_INFO,"Automated native gameplay audit completed; this is not a manual playthrough.");
    }();
}
