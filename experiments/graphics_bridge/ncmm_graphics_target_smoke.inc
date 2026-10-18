// Controlled native target/picking/font captures; not a manual combat session.
{
    require(get_options().has_option("NCMM_FIRST_PERSON_TARGET_HUD"),"target_hud_setting_missing");
    const bool previous=get_option<bool>("NCMM_FIRST_PERSON_TARGET_HUD");
    const std::string original_renderer=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    const auto original_time=calendar::turn;
    const bool original_night_vision=you.has_trait(trait_id("DEBUG_NIGHTVISION"));
    struct restore_target_vision {avatar &observer;bool vision;time_point clock;
        ~restore_target_vision(){calendar::turn=clock;if(observer.has_trait(trait_id("DEBUG_NIGHTVISION"))!=vision)observer.toggle_trait(trait_id("DEBUG_NIGHTVISION"));}} restore_vision{you,original_night_vision,original_time};
    if(!original_night_vision)you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
    struct restore_target_option {bool value;~restore_target_option(){get_options().get_option("NCMM_FIRST_PERSON_TARGET_HUD").setValue(value?"true":"false");}} restoring{previous};
    get_options().get_option("NCMM_FIRST_PERSON_TARGET_HUD").setValue("true");
    std::ofstream target_stream(game_root()/"ncmm"/"first-person-target-audit.json");JsonOut target_report(target_stream);
    target_report.start_object();target_report.member("schema",1);target_report.member("actual_cdda_sdl",true);
    target_report.member("base_viewport_width",audit_viewport.x);target_report.member("base_viewport_height",audit_viewport.y);
    target_report.member("manual_playthrough",false);target_report.member("cases");target_report.start_array();
    for(const char *mode:{"cpu","gpu"}){
        floor();you.set_wielded_item(item());
        const auto goal=point_at(0,-5),blocker=point_at(0,-2);
        auto spawn_target=[&](){auto *value=g->place_critter_at(mtype_id("mon_zombie"),goal);
            require(value&&!value->is_dead_state(),"target_hud_native_subject_missing");return value;};
        monster *target=spawn_target();Creature *subject=target;camera(-ncmm_graphics_camera::pi*.5f,-.10f);warm(mode);
        auto record=[&](const char *phase,bool selected,bool hostile,int hp=-1,int maximum=-1){
            checkpoint(std::string("target_hud_")+phase+"_"+mode);
            render();
            if(bool(graphics_aim)!=selected){
                log_line(NCMM_LOG_ERROR,(std::string("Target HUD selection: ")+phase+" renderer="+mode+
                    " expected="+std::to_string(selected)+" actual="+std::to_string(bool(graphics_aim))+
                    " native_visible="+std::to_string(you.sees(here,*subject))).c_str());
                capture(std::string("target-hud-unexpected-")+phase+"-"+mode);
            }
            require(bool(graphics_aim)==selected,"target_hud_native_selection_wrong");
            if(selected){require(graphics_aim_reference.entity==subject->ncmm_graphics_entity_id()&&
                graphics_aim_reference.world==graphics_creature_world_epoch&&graphics_aim_reference.frame==graphics_target_frame_sequence,
                "target_hud_identity_or_generation_wrong");require(graphics_aim_hostile==hostile,"target_hud_native_attitude_wrong");}
            if(selected&&hostile&&maximum>0)require(graphics_aim_health.known&&std::abs(graphics_aim_health.fraction-float(hp)/maximum)<.001f,"target_hud_not_current_native_hp");
            if(std::string(mode)=="gpu"&&(std::string(phase)=="damaged"||std::string(phase)=="npc-damaged"))
                require(graphics_gpu_engine&&graphics_gpu_engine->metrics().generated_cells==0,"target_hud_hp_rebuilt_static_world_geometry");
            const std::string tag=std::string("target-hud-")+phase+"-"+mode;capture(tag);
            target_report.start_object();target_report.member("phase",phase);target_report.member("renderer",mode);
            target_report.member("file","first-person-audit-"+tag+".bmp");target_report.member("selected",bool(graphics_aim));
            target_report.member("hostile",graphics_aim_hostile);target_report.member("known",graphics_aim_health.known);
            target_report.member("fraction",graphics_aim_health.fraction);target_report.member("hp",hp);target_report.member("maximum",maximum);
            target_report.member("entity",graphics_aim_reference.entity);target_report.member("world",graphics_aim_reference.world);target_report.member("frame",graphics_aim_reference.frame);
            target_report.member("gpu",graphics_gpu_frame);
            const auto metrics=graphics_gpu_engine?graphics_gpu_engine->metrics():ncmm_gpu_metrics_v1{};
            target_report.member("device",metrics.device);target_report.member("backend",metrics.backend);
            target_report.member("software_device",bool(metrics.software_device));
            target_report.member("viewport_width",audit_viewport.x);target_report.member("viewport_height",audit_viewport.y);
            target_report.member("window_display_scale",SDL_GetWindowDisplayScale(get_sdl_window()));
            target_report.end_object();target_stream.flush();
        };
        const int maximum=target->get_hp_max();require(maximum>1,"target_hud_native_max_hp_invalid");
        target->set_hp(maximum);record("full",true,true,maximum,maximum);
        target->set_hp(maximum/2);record("damaged",true,true,maximum/2,maximum);
        target->unique_name="Очень длинное кириллическое имя противника у края экрана";
        record("cyrillic-long",true,true,maximum/2,maximum);
        get_options().get_option("NCMM_FIRST_PERSON_TARGET_HUD").setValue("false");record("disabled-name-only",true,true,maximum/2,maximum);
        get_options().get_option("NCMM_FIRST_PERSON_TARGET_HUD").setValue("true");
        const auto old_reference=graphics_aim_reference;clear_monsters();target=spawn_target();subject=target;
        require(!old_reference.matches(graphics_target_reference(*target,goal))&&graphics_click_action_impl().empty(),"target_hud_reused_cell_inherited_old_aim");
        record("replaced-same-cell",true,true,target->get_hp(),target->get_hp_max());
        target->set_hp(0);record("dead",false,false);clear_monsters();target=spawn_target();subject=target;
        here.ter_set(blocker,ter_id("t_wall"));record("opaque-wall",false,false);
        // The central wooden mullion is opaque geometry. Aim beside it for
        // the pane test instead of weakening pixel ownership/occlusion.
        here.ter_set(blocker,ter_id("t_window"));record("window-mullion",false,false);
        camera(-ncmm_graphics_camera::pi*.5f+.03f,-.10f);
        record("glass",true,true,target->get_hp(),target->get_hp_max());
        camera(-ncmm_graphics_camera::pi*.5f,-.10f);
        here.ter_set(blocker,ter_id("t_reinforced_glass"));record("full-height-glass",true,true,target->get_hp(),target->get_hp_max());
        here.ter_set(blocker,ter_id("t_floor"));target->friendly=-1;record("friendly-name-only",true,false);
        target->friendly=0;calendar::turn=original_time+12_hours;record("night",true,true,target->get_hp(),target->get_hp_max());calendar::turn=original_time;
        {
            const bool night_vision=you.has_trait(trait_id("DEBUG_NIGHTVISION"));
            require(!you.has_effect(efftype_id("blind")),"target_hud_blind_fixture_already_affected");
            struct restore_target_blind {avatar &observer;bool night_vision;~restore_target_blind(){
                observer.remove_effect(efftype_id("blind"));
                if(observer.has_trait(trait_id("DEBUG_NIGHTVISION"))!=night_vision)observer.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
                observer.recalc_sight_limits();}} restore_blind{you,night_vision};
            if(night_vision)you.toggle_trait(trait_id("DEBUG_NIGHTVISION"));
            you.add_effect(efftype_id("blind"),1_minutes);you.recalc_sight_limits();refresh();
            require(!you.sees(here,*target),"target_hud_native_blindness_did_not_hide_subject");
            record("blind-hidden",false,false);
        }
        refresh();record("vision-restored",true,true,target->get_hp(),target->get_hp_max());
        camera(ncmm_graphics_camera::pi*.5f,-.10f);record("lost-aim",false,false);
        camera(-ncmm_graphics_camera::pi*.5f,-.10f);clear_monsters();
        target=g->place_critter_at(mtype_id("mon_zombie"),point_at(0,-5,-1));subject=target;
        require(target!=nullptr,"target_hud_other_floor_subject_missing");
        record("other-floor",false,false);clear_monsters();
        target=g->place_critter_at(mtype_id("mon_zombie_hulk"),point_at(0,-2));subject=target;
        require(target!=nullptr,"target_hud_huge_subject_missing");warm(mode);
        record("huge-close",true,true,target->get_hp(),target->get_hp_max());clear_monsters();
        target=g->place_critter_at(mtype_id("mon_flaming_eye"),goal);subject=target;
        require(target!=nullptr,"target_hud_flying_subject_missing");target->anger=100;target->morale=100;
        // The authored flying-eye sprite has an elevated, offset opaque iris.
        // The ground-zombie angle hit transparent pixels below it (verified in
        // actual Native capture). Aim inside the iris; retain pixel ownership.
        camera(-ncmm_graphics_camera::pi*.5f+.06f,-.03f);warm(mode);
        record("flying",true,true,target->get_hp(),target->get_hp_max());
        {
            SDL_Texture *wide=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,720);
            require(wide!=nullptr,"target_hud_wide_canvas_failed");
            struct restore_target_canvas {SDL_Renderer *renderer;SDL_Texture *&canvas;point &viewport;
                SDL_Texture *previous,*owned;point dimensions;
                ~restore_target_canvas(){SDL_SetRenderTarget(renderer,previous);canvas=previous;viewport=dimensions;SDL_DestroyTexture(owned);}
            } restore_canvas{renderer,canvas,audit_viewport,canvas,wide,audit_viewport};
            canvas=wide;audit_viewport=point(1280,720);warm(mode);
            record("wide",true,true,target->get_hp(),target->get_hp_max());
        }
        clear_monsters();
        camera(-ncmm_graphics_camera::pi*.5f,-.10f);
        auto person=make_shared_fast<npc>();person->normalize();person->name="Враждебный NPC";person->set_attitude(NPCATT_KILL);
        person->spawn_at_precise(here.get_abs(goal));overmap_buffer.insert_npc(person);g->load_npcs();person->setpos(here,goal,false);person->set_attitude(NPCATT_KILL);subject=person.get();
        warm(mode);record("npc-native-summary",true,true,person->get_hp(),person->get_hp_max());
        person->set_part_hp_cur(bodypart_id("torso"),person->get_part_hp_cur(bodypart_id("torso"))/2);
        record("npc-damaged",true,true,person->get_hp(),person->get_hp_max());
        camera(-ncmm_graphics_camera::pi*.5f,-.10f);render();press_f6();
        require(!graphics_view.enabled&&!graphics_aim&&graphics_aim_name.empty()&&!graphics_draw(0,0,640,480),"target_hud_survived_explicit_2d_switch");
        press_f6();ncmm::graphics_note_input_context("DEFAULTMODE");
    }
    target_report.end_array();target_report.member("success",true);target_report.end_object();target_stream.flush();floor();warm(original_renderer.c_str());
}
