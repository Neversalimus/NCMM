// Included in the existing opt-in native gameplay audit, with its owned world,
// main UI adaptor and strict process/SDL guards. This is automated UI, not play.
{
    const bool old_aim_after=get_option<bool>("AIM_AFTER_FIRING");
    const std::string old_keyboard=get_option<std::string>("SDL_KEYBOARD_MODE");
    const std::string old_renderer=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    struct fire_audit_guard {
        bool aim_after;std::string keyboard,renderer;
        ~fire_audit_guard(){graphics_fire_capture.active=false;graphics_fire_audit_legacy_context=false;
            graphics_release_mouse();get_options().get_option("AIM_AFTER_FIRING").setValue(aim_after?"true":"false");get_options().get_option("SDL_KEYBOARD_MODE").setValue(keyboard);
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(renderer);options_manager::update_options_cache();}
    } restoring_fire{old_aim_after,old_keyboard,old_renderer};
    get_options().get_option("AIM_AFTER_FIRING").setValue("false");
    options_manager::update_options_cache();
    for(const char *id:{"BRAWLER","GUNSHY"})if(you.has_trait(trait_id(id)))you.toggle_trait(trait_id(id));
    struct input_feed {
        enum class kind { command, click, pointer_click, confirm, cancel };
        std::atomic<bool> stop{false};std::atomic<bool> escape_recovery{false};std::thread worker;
        input_feed(SDL_WindowID window,kind type,float mx=0,float my=0,bool cancel_target=false):worker([this,window,type,mx,my,cancel_target](){
            int waiting=0;uint32_t previous_context=0;
            for(int tick=0;tick<18000&&!stop.load();++tick){
                SDL_Delay(10);
                const auto waiting_input=graphics_fire_input_context.load();const auto context=waiting_input&3u;
                if(!context){waiting=0;previous_context=0;continue;}
                if(context!=previous_context){waiting=0;previous_context=context;}
                ++waiting;if(tick%10!=9||stop.load())continue;
                SDL_Event event={};const bool recovering=waiting>=1000;
                if(recovering||((type==kind::cancel||cancel_target)&&context!=1)){
                    if(recovering)escape_recovery.store(true);
                    event.type=SDL_EVENT_KEY_DOWN;event.key.windowID=window;event.key.key=SDLK_ESCAPE;event.key.scancode=SDL_SCANCODE_ESCAPE;
                    SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
                }else if(context==1&&((type==kind::command)!=bool(waiting_input&8u))){
                    // The real Tab transition is part of the tested command
                    // path. Never inject an internal router state from worker.
                    event.type=SDL_EVENT_KEY_DOWN;event.key.windowID=window;event.key.key=SDLK_TAB;event.key.scancode=SDL_SCANCODE_TAB;
                    SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
                }else if(context==2||(context==1&&type==kind::command)){
                    // Respect the actual native wait's character/keycode mode.
                    // SDL_PushEvent(KEY_DOWN) does not synthesize TEXT_INPUT.
                    if(waiting_input&4u){event.type=SDL_EVENT_TEXT_INPUT;event.text.windowID=window;event.text.text="f";SDL_PushEvent(&event);}
                    else {event.type=SDL_EVENT_KEY_DOWN;event.key.windowID=window;event.key.key=SDLK_F;event.key.scancode=SDL_SCANCODE_F;
                        SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);}
                }else if(context==1&&(type==kind::click||type==kind::pointer_click)){
                    // Exercise constant-look FPS LMB (including a fresh
                    // viewport capture after Esc/menu/Native command mode).
                    // The ordinary click handler revalidates picked identity,
                    // visibility, origin, camera, range and native readiness.
                    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.windowID=window;event.button.x=mx;event.button.y=my;
                    event.button.button=SDL_BUTTON_LEFT;SDL_PushEvent(&event);
                    event.type=SDL_EVENT_MOUSE_BUTTON_UP;SDL_PushEvent(&event);
                }
            }
        }){}
        ~input_feed(){stop.store(true);worker.join();SDL_FlushEvent(SDL_EVENT_KEY_DOWN);SDL_FlushEvent(SDL_EVENT_KEY_UP);
            SDL_FlushEvent(SDL_EVENT_TEXT_INPUT);SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_UP);}
    };
    struct trial {
        std::string scenario,renderer,keyboard,native_keyboard;bool legacy=false,visible=false,escape_recovery=false;
        uint64_t default_keychar_inputs=0,default_keycode_inputs=0;
        uint64_t fps_attack_intents=0;
        int ammo_before=0,ammo_after=0,moves_before=0,moves_after=0;
        uint64_t attack_before=0,attack_after=0;size_t first=0,last=0;
        float yaw_before=0,pitch_before=0,yaw_after=0,pitch_after=0;
        bool enabled_before=false,enabled_after=false;
        std::string native_command;uint32_t dispatch_calls=0;
    };
    std::vector<trial> trials;
    graphics_fire_capture={};
    const auto window=SDL_GetWindowID(get_sdl_window());
    auto redraw_native=[&](){
        require(SDL_SetRenderTarget(renderer,nullptr),"fire_audit_native_target_failed");
        g->invalidate_main_ui_adaptor();ui_manager::redraw();refresh_display();
    };
    auto attempt=[&](const char *scenario,const char *mode,bool legacy=false){
        trial result;result.scenario=scenario;result.renderer=mode;result.legacy=legacy;result.keyboard=get_option<std::string>("SDL_KEYBOARD_MODE");
        result.native_keyboard=inp_mngr.actual_keyboard_mode(keyboard_mode::keycode)==keyboard_mode::keychar?"keychar":"keycode";
        require(result.native_keyboard==result.keyboard,"fire_audit_option_cache_not_applied");
        const auto chars_before=graphics_fire_capture.default_keychar_inputs,codes_before=graphics_fire_capture.default_keycode_inputs;
        const auto fps_attacks_before=graphics_input_router.attack_intents;
        checkpoint(std::string("native_fire_")+mode+"_"+scenario+"_"+std::to_string(trials.size()));
        clear_monsters();graphics_clear_creatures();you.cancel_activity();you.clear_effects();you.set_moves(10000);
        you.set_stamina(you.get_stamina_max());
        const bool cancel=std::string(scenario)=="cancel"||std::string(scenario)=="no_target"||
            std::string(scenario)=="blocked_wall"||std::string(scenario)=="vertical_cancel"||std::string(scenario)=="dead_target";
        const bool no_target=std::string(scenario)=="no_target";
        const bool empty=std::string(scenario)=="empty";
        const bool pointer_click=std::string(scenario)=="pointer_only";
        const bool click=std::string(scenario)=="lmb_single"||pointer_click;
        const bool changed_weapon=std::string(scenario)=="changed_weapon";
        const bool burst=std::string(scenario)=="burst";
        const bool vertical=std::string(scenario)=="vertical_cancel";
        const auto origin=you.pos_bub();
        const tripoint_bub_ms goal(origin.x(),origin.y()-5,origin.z()+(vertical?1:0));
        const tripoint_bub_ms blocker(origin.x(),origin.y()-2,origin.z());
        here.ter_set(goal,ter_id("t_floor"));here.furn_set(goal,furn_id("f_null"));here.clear_fields(goal);
        here.ter_set(blocker,ter_id(std::string(scenario)=="window"?"t_window_empty":
            std::string(scenario)=="blocked_wall"?"t_wall":"t_floor"));
        monster *target=no_target?nullptr:g->place_critter_at(mtype_id("mon_zombie"),goal);
        require(no_target||target,"fire_audit_target_missing");
        if(std::string(scenario)=="dead_target")target->set_hp(0);
        item gun(itype_id(burst?"hk_mp5":changed_weapon?"glock_17":"sw_619"),calendar::turn);
        if(!empty)gun.ammo_set(itype_id(burst||changed_weapon?"9mm":"38_special"),burst?30:changed_weapon?17:7);
        if(burst)require(gun.gun_set_mode(gun_mode_id("AUTO"))||gun.gun_set_mode(gun_mode_id("BURST")),"fire_audit_native_automatic_mode_missing");
        you.set_wielded_item(gun);you.recoil=0;
        camera(-ncmm_graphics_camera::pi*.5f,-.10f);ncmm::graphics_note_input_context("DEFAULTMODE");refresh();warm(mode);
        if(target){you.last_target=g->shared_from(*target);you.last_target_pos=target->pos_abs();result.visible=you.sees(here,*target);}
        else{you.last_target.reset();you.last_target_pos.reset();}
        if(click)require(graphics_aim&&*graphics_aim==target->pos_abs(),"fire_audit_real_picking_did_not_select_target");
        graphics_fire_audit_legacy_context=legacy;graphics_fire_capture.active=true;
        result.first=graphics_fire_capture.frames.size();
        redraw_native();
        result.enabled_before=graphics_view.enabled;result.yaw_before=graphics_camera_yaw;result.pitch_before=graphics_camera_pitch;
        result.moves_before=you.get_moves();result.ammo_before=you.get_wielded_item()->ammo_remaining();result.attack_before=graphics_attack_serial;
        if(pointer_click){
            // SDL_PushEvent does not change SDL_GetMouseState. Position the
            // pointer in this owned test window on the main thread; no RMB or
            // synthetic gameplay/attack call is used for this case.
            float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,float(graphics_viewport.x+graphics_viewport.w/2),
                float(graphics_viewport.y+graphics_viewport.h/2),&x,&y),"fire_audit_pointer_coordinates_missing");
            SDL_WarpMouseInWindow(get_sdl_window(),x,y);
        }
        {
            graphics_fire_input_context.store(0);
            float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,float(graphics_viewport.x+graphics_viewport.w/2),
                float(graphics_viewport.y+graphics_viewport.h/2),&x,&y),"fire_audit_event_coordinates_missing");
            input_feed feed(window,pointer_click?input_feed::kind::pointer_click:click?input_feed::kind::click:input_feed::kind::command,
                x,y,cancel||empty);
            // The normal game retries handle_action after TIMEOUT. A single
            // diagnostic call can return before the delayed SDL event arrives.
            const auto before=graphics_fire_capture.native_command_sequence;
            const std::string expected="fire";
            for(int retry=0;retry<128;++retry){
                g->ncmm_graphics_smoke_handle_action();++result.dispatch_calls;
                // Mouse movement and TIMEOUT can return from the normal input
                // loop without dispatching the requested gameplay command.
                if(graphics_fire_capture.native_command_sequence>before&&graphics_fire_capture.native_command==expected)break;
                if(graphics_fire_capture.native_command_sequence>before){const auto &other=graphics_fire_capture.native_command;
                    require(other=="MOUSE_MOVE"||other=="COORDINATE",("fire_audit_unexpected_native_command:"+other).c_str());}
            }
            result.native_command=graphics_fire_capture.native_command;
            result.escape_recovery=feed.escape_recovery.load();
            require(graphics_fire_capture.native_command_sequence>before,"fire_audit_native_command_never_dispatched");
            require(result.native_command==expected,("fire_audit_unexpected_native_command:"+result.native_command).c_str());
        }
        graphics_release_mouse();
        if(you.activity){
            input_feed feed(window,cancel||empty?input_feed::kind::cancel:input_feed::kind::confirm);
            for(int turn=0;turn<8&&you.activity;++turn)you.activity.do_turn(you);
            result.escape_recovery=result.escape_recovery||feed.escape_recovery.load();
        }
        require(!you.activity,"fire_audit_native_activity_did_not_finish");
        // Run the actual native redraw/present after return, before any
        // artificial DEFAULTMODE notification could conceal a stale context.
        redraw_native();
        result.enabled_after=graphics_view.enabled;result.yaw_after=graphics_camera_yaw;result.pitch_after=graphics_camera_pitch;
        result.ammo_after=you.get_wielded_item()->ammo_remaining();result.moves_after=you.get_moves();result.attack_after=graphics_attack_serial;
        result.last=graphics_fire_capture.frames.size();graphics_fire_capture.active=false;graphics_fire_audit_legacy_context=false;
        result.default_keychar_inputs=graphics_fire_capture.default_keychar_inputs-chars_before;
        result.default_keycode_inputs=graphics_fire_capture.default_keycode_inputs-codes_before;
        result.fps_attack_intents=graphics_input_router.attack_intents-fps_attacks_before;
        require(result.fps_attack_intents==uint64_t(click),"fire_audit_wrong_fps_mouse_ownership");
        {std::ofstream progress(game_root()/"ncmm"/"first-person-fire-trials.jsonl",std::ios::app);JsonOut out(progress);
            out.start_object();out.member("scenario",result.scenario);out.member("renderer",result.renderer);out.member("legacy",result.legacy);out.member("keyboard_mode",result.keyboard);
            out.member("native_keyboard_mode",result.native_keyboard);out.member("default_keychar_inputs",result.default_keychar_inputs);out.member("default_keycode_inputs",result.default_keycode_inputs);
            out.member("native_command",result.native_command);out.member("dispatch_calls",result.dispatch_calls);out.member("escape_recovery",result.escape_recovery);
            out.member("ammo_before",result.ammo_before);out.member("ammo_after",result.ammo_after);out.member("moves_before",result.moves_before);out.member("moves_after",result.moves_after);
            out.member("attack_before",result.attack_before);out.member("attack_after",result.attack_after);out.member("first_frame",result.first);out.member("last_frame",result.last);out.end_object();progress<<'\n';}
        require(result.last>result.first&&!graphics_fire_capture.failed,"fire_audit_missing_presented_pixels");
        require(result.native_keyboard=="keychar"?(result.default_keychar_inputs>0&&result.default_keycode_inputs==0):
            (result.default_keycode_inputs>0&&result.default_keychar_inputs==0),"fire_audit_actual_keyboard_wait_not_proven");
        if(!legacy){
            require(result.enabled_before&&result.enabled_after,"fire_audit_user_mode_changed");
            require(result.yaw_before==result.yaw_after&&result.pitch_before==result.pitch_after,"fire_audit_camera_changed");
            for(size_t i=result.first;i<result.last;++i){const auto &f=graphics_fire_capture.frames[i];
                require(f.enabled&&f.perspective&&f.captured&&f.presented,"fire_audit_intermediate_native_map_flash");
                require(f.gpu==(std::string(mode)=="gpu"),"fire_audit_unreported_cpu_fallback");}
            if(cancel||empty)require(result.attack_after==result.attack_before&&result.ammo_after==result.ammo_before,"fire_audit_cancel_or_failure_emitted_shot");
            else {const auto shots=result.attack_after-result.attack_before;const int spent=result.ammo_before-result.ammo_after;
                require(shots>0&&spent>0&&shots==uint64_t(spent)&&result.moves_after<result.moves_before,"fire_audit_confirmed_native_shot_missing");
                require(burst?shots>=2:shots==1,"fire_audit_native_shot_count_wrong");
                require(!result.escape_recovery,"fire_audit_success_path_used_escape_watchdog");}
        }
        trials.push_back(std::move(result));ncmm::graphics_note_input_context("DEFAULTMODE");
    };
    floor();
    attempt("cancel","cpu",true);
    const auto &control=trials.back();
    require(std::any_of(graphics_fire_capture.frames.begin()+control.first,graphics_fire_capture.frames.begin()+control.last,
        [](const auto &f){return f.presented&&!f.perspective;}),"fire_audit_did_not_reproduce_original_context_regression");
    for(const char *mode:{"cpu","gpu"}){
        floor();
        for(const char *scenario:{"keyboard_single","lmb_single","cancel","empty","no_target","window"}){
            const int repeats=std::string(scenario)=="keyboard_single"||std::string(scenario)=="lmb_single"||std::string(scenario)=="window"?20:10;
            for(int repeat=0;repeat<repeats;++repeat){get_options().get_option("SDL_KEYBOARD_MODE").setValue(repeat%2?"keychar":"keycode");
                options_manager::update_options_cache();attempt(scenario,mode);}
        }
        for(const char *scenario:{"blocked_wall","vertical_cancel","burst","dead_target","changed_weapon","pointer_only"})attempt(scenario,mode);
        {cancel_native_ui cancel_menu(window);game_menus::inv::common();}
        ncmm::graphics_note_input_context("DEFAULTMODE");redraw_native();
        const float yaw=graphics_camera_yaw,pitch=graphics_camera_pitch;
        press_f6();require(!graphics_view.enabled,"fire_audit_explicit_f6_off_failed");
        {ncmm::graphics_targeting_scope inactive_aim(true);ncmm::graphics_note_input_context("TARGET");
            require(!graphics_view.enabled&&!graphics_draw(0,0,640,480),"fire_audit_aim_forced_user_mode_on");}
        require(!graphics_view.enabled&&yaw==graphics_camera_yaw&&pitch==graphics_camera_pitch,"fire_audit_exit_forced_user_mode_on");
        press_f6();ncmm::graphics_note_input_context("DEFAULTMODE");
    }
    std::ofstream fire_stream(game_root()/"ncmm"/"first-person-fire-audit.json");JsonOut fire_report(fire_stream);
    fire_report.start_object();fire_report.member("schema",1);fire_report.member("actual_cdda_sdl",true);fire_report.member("manual_playthrough",false);
    fire_report.member("normal_game_command_dispatch",true);fire_report.member("native_activity_and_target_ui",true);
    fire_report.member("synthetic_attack_notifications",false);fire_report.member("ordinary_ai_turns",false);
    fire_report.member("fps_input_router",true);
    fire_report.member("lmb_path","real SDL FPS LMB, viewport recapture, physical Tab Native-command mode, native picking and dispatch");
    fire_report.member("trials");fire_report.start_array();
    for(const auto &t:trials){fire_report.start_object();fire_report.member("scenario",t.scenario);fire_report.member("renderer",t.renderer);fire_report.member("legacy_control",t.legacy);
        fire_report.member("target_native_visible",t.visible);fire_report.member("escape_recovery",t.escape_recovery);
        fire_report.member("native_command",t.native_command);fire_report.member("dispatch_calls",t.dispatch_calls);fire_report.member("keyboard_mode",t.keyboard);
        fire_report.member("fps_attack_intents",t.fps_attack_intents);
        fire_report.member("native_keyboard_mode",t.native_keyboard);fire_report.member("default_keychar_inputs",t.default_keychar_inputs);fire_report.member("default_keycode_inputs",t.default_keycode_inputs);
        fire_report.member("ammo_before",t.ammo_before);fire_report.member("ammo_after",t.ammo_after);fire_report.member("moves_before",t.moves_before);fire_report.member("moves_after",t.moves_after);
        fire_report.member("attack_serial_before",t.attack_before);fire_report.member("attack_serial_after",t.attack_after);
        fire_report.member("first_frame",t.first);fire_report.member("last_frame_exclusive",t.last);
        fire_report.member("yaw_before",t.yaw_before);fire_report.member("yaw_after",t.yaw_after);fire_report.member("pitch_before",t.pitch_before);fire_report.member("pitch_after",t.pitch_after);
        fire_report.member("enabled_before",t.enabled_before);fire_report.member("enabled_after",t.enabled_after);fire_report.end_object();}
    fire_report.end_array();fire_report.member("frames");fire_report.start_array();
    for(const auto &f:graphics_fire_capture.frames){fire_report.start_object();fire_report.member("ticks",f.ticks);fire_report.member("file",f.file);fire_report.member("width",f.width);fire_report.member("height",f.height);
        fire_report.member("enabled",f.enabled);fire_report.member("perspective",f.perspective);fire_report.member("gpu",f.gpu);fire_report.member("captured",f.captured);fire_report.member("presented",f.presented);fire_report.end_object();}
    fire_report.end_array();fire_report.member("success",true);fire_report.end_object();fire_stream.flush();
    // floor() invalidates the previous camera position. Establish the restored
    // baseline before the following native walking test performs its first step.
    floor();warm(old_renderer.c_str());
}
