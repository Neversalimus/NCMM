// Real SDL input and game::handle_action; no fixture input lease is acquired.
// The worker only queues SDL events. All Native reads/calls stay on main thread.
{
    g->ncmm_graphics_smoke_prepare_turns();
    const auto native_ui=g->create_or_get_main_ui_adaptor();
    const std::string old_renderer=get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    struct restore_probe {std::string mode;~restore_probe(){graphics_camera_input_probe.active=false;graphics_release_mouse();
        get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(mode);}} restoring{old_renderer};
    get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue("gpu");
    ncmm::graphics_note_input_context("DEFAULTMODE");
    graphics_fps_initial_capture=true;
    require(SDL_SetRenderTarget(get_sdl_renderer().get(),nullptr),"camera_input_window_target_failed");
    graphics_camera_input_probe={};graphics_camera_input_probe.active=true;
    g->invalidate_main_ui_adaptor();ui_manager::redraw();refresh_display();
    require(graphics_gpu_frame&&graphics_view.enabled,"camera_input_initial_gpu_frame_missing");
    require(!graphics_camera_input_probe.reference_pixels.empty()&&!graphics_camera_input_probe.capture_failures,
            "camera_input_initial_pixel_reference_missing");
    const auto position=you.pos_abs();const auto turn=calendar::turn;
    const int moves=you.get_moves(),hp=you.get_part_hp_cur(bodypart_id("torso"));
    const auto attack=graphics_attack_serial;
    const auto before=graphics_camera_cache.totals;
    auto initial_reference=std::move(graphics_camera_input_probe.reference_pixels);
    const auto initial_bounds=graphics_camera_input_probe.reference_bounds;
    graphics_camera_input_probe={};graphics_camera_input_probe.active=true;
    graphics_camera_input_probe.reference_pixels=std::move(initial_reference);
    graphics_camera_input_probe.reference_bounds=initial_bounds;
    graphics_camera_input_probe.last_composed_perspective=true;graphics_camera_input_probe.last_composed_gpu=true;
    std::ofstream progress(game_root()/"ncmm"/"first-person-audit-progress.txt");
    struct input_worker {
        std::atomic<bool> stop{false},done{false};std::thread thread;
        input_worker(SDL_WindowID window,float x,float y):thread([this,window,x,y](){
            SDL_Delay(100);
            for(int sample=0;sample<480&&!stop.load();++sample){
                SDL_Event event={};
                event={};event.type=SDL_EVENT_MOUSE_MOTION;event.motion.windowID=window;
                event.motion.state=0;event.motion.x=x;event.motion.y=y;event.motion.xrel=10;event.motion.yrel=0;
                SDL_PushEvent(&event);SDL_Delay(20);
            }
            // A real registered camera hotkey releases the ordinary Native
            // input wait without moving an avatar or opening another UI.
            SDL_Event key={};key.type=SDL_EVENT_KEY_DOWN;key.key.windowID=window;key.key.key=SDLK_F8;key.key.scancode=SDL_SCANCODE_F8;
            SDL_PushEvent(&key);key.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&key);done.store(true);
        }){}
        ~input_worker(){stop.store(true);thread.join();SDL_FlushEvent(SDL_EVENT_MOUSE_MOTION);
            SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_UP);
            SDL_FlushEvent(SDL_EVENT_KEY_DOWN);SDL_FlushEvent(SDL_EVENT_KEY_UP);}
    };
    size_t input_calls=0;
    {
        input_worker feed(SDL_GetWindowID(get_sdl_window()),float(graphics_viewport.x+graphics_viewport.w/2),float(graphics_viewport.y+graphics_viewport.h/2));
        const auto deadline=SDL_GetTicks()+30000;
        do{
            require(SDL_GetTicks()<deadline,"camera_input_native_loop_timeout");
            g->ncmm_graphics_smoke_handle_action();++input_calls;
            progress<<"camera_input calls="<<input_calls<<" presents="<<graphics_camera_input_probe.presents
                    <<" events="<<graphics_camera_input_probe.mouse_events<<'\n';progress.flush();
        }while(!feed.done.load()&&input_calls<1024);
        require(feed.done.load(),"camera_input_worker_not_complete");
    }
    graphics_camera_input_probe.active=false;graphics_release_mouse();
    const auto after=graphics_camera_cache.totals;
    const auto observed=graphics_camera_input_probe;
    const bool native_unchanged=position==you.pos_abs()&&turn==calendar::turn&&moves==you.get_moves()&&
        hp==you.get_part_hp_cur(bodypart_id("torso"))&&attack==graphics_attack_serial;
    const bool success=native_unchanged&&observed.mouse_events>=400&&observed.rotation_radians>=12.566370614359&&
        observed.presents>=16&&observed.gpu_frames==observed.presents&&!observed.cpu_frames&&!observed.nonperspective_frames&&
        observed.pixel_checks==observed.presents&&!observed.capture_failures&&
        observed.pixel_reference_frames+observed.retained_perspective_frames==observed.presents&&
        after.hits>before.hits+1000&&after.detail_hits>before.detail_hits&&after.presentation_hits>before.presentation_hits;
    std::ofstream output(game_root()/"ncmm"/"first-person-camera-input.json");JsonOut out(output);
    out.start_object();out.member("schema",1);out.member("success",success);out.member("actual_cdda_sdl",true);out.member("manual_playthrough",false);
    out.member("fixture_input_lease",false);out.member("ordinary_performance_measurement",false);out.member("native_unchanged",native_unchanged);
    out.member("input_calls",input_calls);out.member("mouse_events",observed.mouse_events);out.member("rotation_radians",observed.rotation_radians);
    out.member("presented_frames",observed.presents);out.member("gpu_frames",observed.gpu_frames);out.member("cpu_frames",observed.cpu_frames);out.member("nonperspective_frames",observed.nonperspective_frames);
    out.member("snapshot_hits",after.hits-before.hits);out.member("snapshot_misses",after.misses-before.misses);
    out.member("snapshot_generations",after.generations-before.generations);out.member("detail_hits",after.detail_hits-before.detail_hits);
    out.member("presentation_hits",after.presentation_hits-before.presentation_hits);out.member("snapshot_bytes",graphics_camera_cache.bytes());
    out.member("snapshot_key_changes");out.start_array();
    for(size_t i=0;i<after.key_changes.size();++i)out.write(after.key_changes[i]-before.key_changes[i]);
    out.end_array();out.member("snapshot_window_changes",after.window_changes-before.window_changes);
    out.member("snapshot_clears",after.clears-before.clears);
    out.member("lease_invalidations");out.start_object();for(const auto &entry:observed.lease_invalidations)out.member(entry.first,entry.second);out.end_object();
    out.member("retained_perspective_frames",observed.retained_perspective_frames);
    out.member("retained_pixels_verified",observed.pixel_checks==observed.presents&&!observed.capture_failures&&!observed.nonperspective_frames);
    out.member("pixel_checks",observed.pixel_checks);
    out.member("pixel_reference_frames",observed.pixel_reference_frames);
    out.member("unclassified_captures",observed.unclassified_captures);out.member("capture_failures",observed.capture_failures);
    const auto metrics=graphics_gpu_engine->metrics();out.member("device",metrics.device);out.member("backend",metrics.backend);out.member("software_device",bool(metrics.software_device));
    out.end_object();output.close();require(bool(output),"camera_input_report_write_failed");
    // Preserve the exact failing predicates in the runner's progress tail as
    // well as its separately uploaded JSON, including software adapters.
    std::ifstream saved(game_root()/"ncmm"/"first-person-camera-input.json");
    progress<<"camera_input result="<<saved.rdbuf()<<'\n';progress.flush();
    require(success,"camera_input_actual_sdl_reuse_not_proven");
}
