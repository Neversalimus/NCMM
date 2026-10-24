// Actual Native input/action qualification, inside the isolated gameplay world.
// Event injection is diagnostic only. Production has one SDL loop/main-thread owner.
{
    checkpoint("fps_controls_begin");
    const std::string previous_keyboard = get_option<std::string>("SDL_KEYBOARD_MODE");
    const std::string previous_renderer = get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    struct fps_audit_guard {
        std::string keyboard, renderer;
        ~fps_audit_guard() {
            graphics_fps_audit_active = false;
            graphics_fps_audit_wait.store(0);
            graphics_release_mouse();
            get_options().get_option("SDL_KEYBOARD_MODE").setValue(keyboard);
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(renderer);
            options_manager::update_options_cache();
        }
    } restore_fps{previous_keyboard, previous_renderer};
    graphics_fps_audit_active = true;
    const auto window = SDL_GetWindowID(get_sdl_window());
    auto key_event = [window](SDL_Scancode code, SDL_Keycode key, bool down,
                              bool repeated = false) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.windowID = window;
        event.key.scancode = code;
        event.key.key = key;
        event.key.repeat = repeated;
        return event;
    };
    auto tap = [&](SDL_Scancode code, SDL_Keycode key) {
        return std::vector<SDL_Event>{key_event(code, key, true), key_event(code, key, false)};
    };
    struct fps_events {
        std::atomic<bool> stop{false}, done{false}, sent{false};
        std::thread worker;
        fps_events(SDL_WindowID window, float x, float y, std::vector<SDL_Event> events,
                   unsigned duration)
            : worker([this, window, x, y, events, duration]() {
                  auto key = [window](SDL_Scancode code, SDL_Keycode value, bool down) {
                      SDL_Event event{};
                      event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                      event.key.windowID = window;
                      event.key.scancode = code;
                      event.key.key = value;
                      SDL_PushEvent(&event);
                  };
                  const uint64_t deadline = SDL_GetTicks() + 12000;
                  while (!stop.load() && SDL_GetTicks() < deadline) {
                      SDL_Delay(20);
                      const uint32_t waiting = graphics_fps_audit_wait.load();
                      if (!(waiting & 1u))
                          continue;
                      if (waiting & 8u) {
                          key(SDL_SCANCODE_TAB, SDLK_TAB, true);
                          key(SDL_SCANCODE_TAB, SDLK_TAB, false);
                          SDL_Delay(160);
                          continue;
                      }
                      if (!(waiting & 16u)) {
                          SDL_Event click{};
                          click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                          click.button.windowID = window;
                          click.button.x = x;
                          click.button.y = y;
                          click.button.button = SDL_BUTTON_LEFT;
                          SDL_PushEvent(&click);
                          click.type = SDL_EVENT_MOUSE_BUTTON_UP;
                          SDL_PushEvent(&click);
                          SDL_Delay(160);
                          continue;
                      }
                      for (auto event : events)
                          SDL_PushEvent(&event);
                      sent.store(true);
                      break;
                  }
                  const uint64_t finish = SDL_GetTicks() + duration;
                  while (!stop.load() && SDL_GetTicks() < finish) {
                      SDL_Delay(20);
                      if (graphics_fps_audit_wait.load() & 2u) {
                          key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, true);
                          key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, false);
                      }
                  }
                  for (const auto &pair : std::array<std::pair<SDL_Scancode, SDL_Keycode>, 4>{
                           {{SDL_SCANCODE_W, SDLK_W},
                            {SDL_SCANCODE_A, SDLK_A},
                            {SDL_SCANCODE_S, SDLK_S},
                            {SDL_SCANCODE_D, SDLK_D}}})
                      key(pair.first, pair.second, false);
                  key(SDL_SCANCODE_F8, SDLK_F8, true);
                  key(SDL_SCANCODE_F8, SDLK_F8, false);
                  done.store(true);
              }) {}
        ~fps_events() {
            stop.store(true);
            worker.join();
            // Leave cleanup releases for the existing SDL loop to observe.
            SDL_FlushEvent(SDL_EVENT_KEY_DOWN);
            SDL_FlushEvent(SDL_EVENT_TEXT_INPUT);
            SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_DOWN);
            SDL_FlushEvent(SDL_EVENT_MOUSE_BUTTON_UP);
        }
    };
    auto dispatch = [&](std::vector<SDL_Event> events, int boundaries = 1,
                        unsigned duration = 200) {
        int ww = 0, wh = 0, pw = 0, ph = 0;
        SDL_GetWindowSize(get_sdl_window(), &ww, &wh);
        SDL_GetWindowSizeInPixels(get_sdl_window(), &pw, &ph);
        require(ww > 0 && wh > 0 && pw > 0 && ph > 0, "fps_audit_window_dimensions");
        float mouse_x = 0, mouse_y = 0;
        require(SDL_RenderCoordinatesToWindow(
                    renderer, float(graphics_viewport.x + graphics_viewport.w / 2),
                    float(graphics_viewport.y + graphics_viewport.h / 2), &mouse_x, &mouse_y),
                "fps_audit_display_to_window_coordinates");
        // Settle real startup/resize events before the one-shot event script.
        // A fresh viewport capture is itself routed through SDL below.
        graphics_release_mouse();
        SDL_WarpMouseInWindow(get_sdl_window(), mouse_x, mouse_y);
        inp_mngr.pump_events();
        for (auto &event : events)
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                event.button.windowID = window;
                event.button.x = mouse_x;
                event.button.y = mouse_y;
            }
        graphics_fps_audit_wait.store(0);
        fps_events feed(window, mouse_x, mouse_y, std::move(events), duration);
        for (int boundary = 0; boundary < boundaries; ++boundary) {
            const auto issued = graphics_input_router.issued;
            for (int retry = 0; retry < 64; ++retry) {
                g->ncmm_graphics_smoke_handle_action();
                if (feed.sent.load() && (graphics_input_router.issued > issued || feed.done.load()))
                    break;
            }
        }
        require(feed.sent.load(), "fps_audit_sdl_script_never_entered_native_wait");
    };
    auto native_redraw = [&]() {
        ncmm::graphics_note_input_context("DEFAULTMODE");
        require(SDL_SetRenderTarget(renderer, nullptr), "fps_audit_window_target");
        g->invalidate_main_ui_adaptor();
        ui_manager::redraw();
        refresh_display();
        require(graphics_view.enabled, "fps_audit_view_disabled");
    };
    struct sample {
        std::string name, mode, renderer;
        int actions = 0, cost = 0;
        int direction = -1;
    };
    std::vector<sample> samples;
    for (const char *mode : {"keychar", "keycode"}) {
        get_options().get_option("SDL_KEYBOARD_MODE").setValue(mode);
        options_manager::update_options_cache();
        for (const char *renderer_mode : {"cpu", "gpu"}) {
            floor();
            warm(renderer_mode);
            you.set_wielded_item(item());
            for (int direction = 0; direction < 8; ++direction) {
                you.setpos(here, point_at(0, 0), false);
                you.set_moves(10000);
                const float yaw = direction * ncmm_graphics_camera::pi / 4;
                camera(yaw, -.08f);
                refresh();
                native_redraw();
                const auto origin = you.pos_bub();
                const int moves = you.get_moves();
                const auto actions = graphics_input_router.movements;
                // Deliberately disagreeing Russian keycode and physical W.
                dispatch(tap(SDL_SCANCODE_W, SDL_Keycode(0x0446)));
                const auto delta = ncmm_graphics_camera::movement(yaw, 1, 0);
                if (you.pos_bub() != origin + tripoint_rel_ms(delta.first, delta.second, 0)) {
                    const auto actual = you.pos_bub() - origin;
                    log_line(
                        NCMM_LOG_ERROR,
                        (std::string("FPS W diagnostic mode=") + mode +
                         " renderer=" + renderer_mode + " yaw=" + std::to_string(direction) +
                         " expected=" + std::to_string(delta.first) + "," +
                         std::to_string(delta.second) + " actual=" + std::to_string(actual.x()) +
                         "," + std::to_string(actual.y()) +
                         " actions=" + std::to_string(graphics_input_router.movements - actions) +
                         " cost=" + std::to_string(moves - you.get_moves()) +
                         " last=" + graphics_fps_action)
                            .c_str());
                }
                require(you.pos_bub() == origin + tripoint_rel_ms(delta.first, delta.second, 0),
                        "fps_audit_physical_w_wrong_native_direction");
                require(graphics_input_router.movements == actions + 1 && you.get_moves() < moves,
                        "fps_audit_w_not_one_costed_native_action");
                samples.push_back({"physical_w_8_yaws", mode, renderer_mode, 1,
                                   moves - you.get_moves(), direction});
            }
            you.setpos(here, point_at(0, 0), false);
            you.set_moves(10000);
            camera(-ncmm_graphics_camera::pi * .5f, -.08f);
            refresh();
            native_redraw();
            const auto origin = you.pos_bub();
            const auto actions = graphics_input_router.movements;
            const int moves = you.get_moves();
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true),
                      key_event(SDL_SCANCODE_D, SDLK_D, true),
                      key_event(SDL_SCANCODE_W, SDLK_W, false),
                      key_event(SDL_SCANCODE_D, SDLK_D, false)});
            require(you.pos_bub() == origin + tripoint_rel_ms(1, -1, 0) &&
                        graphics_input_router.movements == actions + 1 && you.get_moves() < moves,
                    "fps_audit_chord_not_one_native_diagonal");
            samples.push_back({"wd_one_diagonal", mode, renderer_mode, 1, moves - you.get_moves()});
            floor();
            warm(renderer_mode);
            native_redraw();
            you.set_moves(10000);
            const auto held_origin = you.pos_bub();
            const int held_moves = you.get_moves();
            const auto held_actions = graphics_input_router.movements;
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true)}, 3, 1500);
            require(you.pos_bub() == held_origin + tripoint_rel_ms(0, -3, 0) &&
                        graphics_input_router.movements == held_actions + 3 &&
                        you.get_moves() < held_moves,
                    "fps_audit_held_w_not_three_native_boundaries");
            samples.push_back({"held_w_three_native_boundaries", mode, renderer_mode, 3,
                               held_moves - you.get_moves()});
            floor();
            warm(renderer_mode);
            you.set_moves(10000);
            const auto wall_origin = you.pos_bub();
            const int wall_moves = you.get_moves();
            here.ter_set(wall_origin + tripoint_rel_ms(0, -1, 0), ter_id("t_wall"));
            refresh();
            native_redraw();
            const auto wall_actions = graphics_input_router.movements;
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true)}, 3, 600);
            require(you.pos_bub() == wall_origin &&
                        graphics_input_router.movements == wall_actions + 1,
                    "fps_audit_wall_repeated_held_walk_or_crossed_collision");
            samples.push_back(
                {"wall_fences_held_w", mode, renderer_mode, 1, wall_moves - you.get_moves()});
            floor();
            warm(renderer_mode);
            native_redraw();
            // A Native inventory owns the next wait; W must not leak through
            // either its hotkeys or its return to DEFAULTMODE.
            const auto inventory_origin = you.pos_abs();
            const int inventory_moves = you.get_moves();
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true),
                      key_event(SDL_SCANCODE_I, SDLK_I, true),
                      key_event(SDL_SCANCODE_I, SDLK_I, false)},
                     1, 500);
            require(you.pos_abs() == inventory_origin && you.get_moves() == inventory_moves,
                    "fps_audit_inventory_leaked_held_walk");
            require(!graphics_input_router.ready(SDL_GetTicks()),
                    "fps_audit_inventory_left_queued_action");
            samples.push_back({"inventory_held_w_cancel", mode, renderer_mode, 1, 0});
            // Window ownership changes are delivered through the actual SDL
            // routing path. This does not claim a physical two-monitor test.
            native_redraw();
            for (Uint32 event_type :
                 {Uint32(SDL_EVENT_WINDOW_FOCUS_LOST), Uint32(SDL_EVENT_WINDOW_RESIZED),
                  Uint32(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)}) {
                SDL_Event event{};
                event.type = event_type;
                event.window.windowID = window;
                if (event_type == SDL_EVENT_WINDOW_RESIZED) {
                    SDL_GetWindowSize(get_sdl_window(), &event.window.data1, &event.window.data2);
                }
                const auto position = you.pos_abs();
                const int before_moves = you.get_moves();
                std::vector<SDL_Event> events{event};
                if (event_type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    SDL_Event gained{};
                    gained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                    gained.window.windowID = window;
                    events.push_back(gained);
                }
                dispatch(std::move(events));
                require(!graphics_input_router.captured() &&
                            !graphics_input_router.ready(SDL_GetTicks()) &&
                            you.pos_abs() == position && you.get_moves() == before_moves,
                        "fps_audit_window_event_kept_capture_or_advanced_game");
                samples.push_back({"focus_resize_dpi_release", mode, renderer_mode, 0, 0});
            }
            floor();
            warm(renderer_mode);
            you.set_wielded_item(item());
            you.set_moves(10000);
            const auto door = you.pos_bub() + tripoint_rel_ms(0, -1, 0);
            here.ter_set(door, ter_id("t_door_c"));
            camera(-ncmm_graphics_camera::pi * .5f, -.08f);
            refresh();
            native_redraw();
            require(graphics_fps_target(true) == door, "fps_audit_reticle_missed_door");
            const int before_door = you.get_moves();
            dispatch(tap(SDL_SCANCODE_E, SDLK_E));
            for (int turn = 0; turn < 4 && you.activity; ++turn)
                you.activity.do_turn(you);
            require(here.ter(door) == ter_id("t_door_o") && you.get_moves() < before_door,
                    "fps_audit_e_did_not_use_native_open_activity");
            samples.push_back(
                {"e_native_open", mode, renderer_mode, 1, before_door - you.get_moves()});
            for (const char *weapon : {"null", "knife_combat"}) {
                floor();
                you.set_moves(10000);
                you.set_wielded_item(std::string(weapon) == "null"
                                         ? item()
                                         : item(itype_id(weapon), calendar::turn));
                monster *enemy = g->place_critter_at(mtype_id("mon_zombie"),
                                                     you.pos_bub() + tripoint_rel_ms(0, -1, 0));
                require(enemy != nullptr, "fps_audit_melee_enemy_missing");
                camera(-ncmm_graphics_camera::pi * .5f, -.3f);
                warm(renderer_mode);
                native_redraw();
                require(graphics_aim == enemy->pos_abs(), "fps_audit_melee_reticle_missing");
                const auto serial = graphics_attack_serial,
                           intents = graphics_input_router.attack_intents;
                const auto position = you.pos_abs();
                const int before = you.get_moves();
                SDL_Event click{};
                click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                click.button.button = SDL_BUTTON_LEFT;
                SDL_Event release = click;
                release.type = SDL_EVENT_MOUSE_BUTTON_UP;
                dispatch({click, release});
                require(graphics_input_router.attack_intents == intents + 1 &&
                            graphics_attack_serial == serial + 1 && you.pos_abs() == position &&
                            you.get_moves() < before,
                        "fps_audit_lmb_melee_not_one_native_attack");
                samples.push_back(
                    {"lmb_native_melee", mode, renderer_mode, 1, before - you.get_moves()});
            }
            floor();
            warm(renderer_mode);
            native_redraw();
            you.set_movement_mode(move_mode_id("walk"));
            dispatch(tap(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT));
            require(you.is_running(), "fps_audit_shift_did_not_toggle_native_run");
            dispatch(tap(SDL_SCANCODE_C, SDLK_C));
            require(you.is_crouching(), "fps_audit_c_did_not_toggle_native_crouch");
            const int wait_moves = you.get_moves();
            dispatch(tap(SDL_SCANCODE_SPACE, SDLK_SPACE));
            require(you.get_moves() < wait_moves, "fps_audit_space_did_not_spend_native_wait");
            samples.push_back(
                {"native_run_crouch_wait", mode, renderer_mode, 3, wait_moves - you.get_moves()});
            you.set_movement_mode(move_mode_id("walk"));
        }
    }
    std::ofstream output(game_root() / "ncmm" / "first-person-fps-input.json");
    JsonOut out(output);
    out.start_object();
    out.member("schema", 1);
    out.member("actual_cdda_sdl", true);
    out.member("manual_playthrough", false);
    out.member("physical_two_monitor_session", false);
    out.member("samples");
    out.start_array();
    for (const auto &entry : samples) {
        out.start_object();
        out.member("scenario", entry.name);
        out.member("keyboard_mode", entry.mode);
        out.member("renderer", entry.renderer);
        out.member("native_actions", entry.actions);
        out.member("native_move_cost", entry.cost);
        if (entry.direction >= 0)
            out.member("direction", entry.direction);
        out.end_object();
    }
    out.end_array();
    out.member("success", true);
    out.end_object();
    output.close();
    require(bool(output), "fps_audit_report_write");
    floor();
    warm(previous_renderer.c_str());
    checkpoint("fps_controls_complete");
}
