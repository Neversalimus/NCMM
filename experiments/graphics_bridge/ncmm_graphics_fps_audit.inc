// Actual Native input/action qualification, inside the isolated gameplay world.
// Event injection is diagnostic only. Production has one SDL loop/main-thread owner.
{
    checkpoint("fps_controls_begin");
    const std::string previous_keyboard = get_option<std::string>("SDL_KEYBOARD_MODE");
    const std::string previous_renderer = get_option<std::string>("NCMM_FIRST_PERSON_RENDERER");
    const bool previous_aim_after=get_option<bool>("AIM_AFTER_FIRING");
    struct fps_audit_guard {
        std::string keyboard, renderer;
        bool aim_after;
        move_mode_id movement, desired;
        ~fps_audit_guard() {
            graphics_fps_audit_active = false;
            graphics_fps_audit_wait.store(0);
            graphics_release_mouse();
            get_avatar().set_desired_movement_mode(desired);
            get_avatar().set_movement_mode(movement);
            get_options().get_option("SDL_KEYBOARD_MODE").setValue(keyboard);
            get_options().get_option("NCMM_FIRST_PERSON_RENDERER").setValue(renderer);
            get_options().get_option("AIM_AFTER_FIRING").setValue(aim_after?"true":"false");
            options_manager::update_options_cache();
        }
    } restore_fps{previous_keyboard, previous_renderer,previous_aim_after, you.current_movement_mode(),
                  you.get_desired_move_mode()};
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
        std::atomic<bool> stop{false}, done{false}, sent{false}, modal_seen{false};
        std::atomic<uint32_t> last_wait{0};
        std::atomic<int> queued{0};
        std::thread worker;
        fps_events(SDL_WindowID window, float x, float y, std::vector<SDL_Event> events,
                   unsigned duration,bool fractional=false)
            : worker([this, window, x, y, events, duration,fractional]() mutable {
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
                      last_wait.store(waiting);
                      if (!(waiting & 1u))
                          continue;
                      if (waiting & 8u) {
                          key(SDL_SCANCODE_TAB, SDLK_TAB, true);
                          key(SDL_SCANCODE_TAB, SDLK_TAB, false);
                          SDL_Delay(160);
                          continue;
                      }
                      if (!(waiting & 16u)) {
                          // Exercise the same synthetic focus transition as
                          // the explicit focus cases. This is not physical
                          // foreground-window or two-monitor qualification.
                          SDL_Event focus{};
                          focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                          focus.window.windowID = window;
                          SDL_PushEvent(&focus);
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
                      const bool keyboard_only =
                          std::all_of(events.begin(), events.end(), [](const SDL_Event &event) {
                              return event.type == SDL_EVENT_KEY_DOWN ||
                                     event.type == SDL_EVENT_KEY_UP;
                          });
                      if (keyboard_only) {
                          // A scripted chord is one queue batch. Individual
                          // PushEvent calls race the main thread's poll drain
                          // and can submit W before D has even been queued.
                          // SDL 3.4.10 adds this batch under one queue lock.
                          queued.store(SDL_PeepEvents(events.data(), int(events.size()),
                                                      SDL_ADDEVENT, SDL_EVENT_FIRST,
                                                      SDL_EVENT_LAST));
                      } else {
                          // Keep real event filters/watches for window and
                          // mouse fixtures (PeepEvents bypasses those hooks).
                          for (auto event : events) {
                              if (SDL_PushEvent(&event))
                                  queued.fetch_add(1);
                          }
                      }
                      sent.store(queued.load() == int(events.size()));
                      break;
                  }
                  const uint64_t finish = SDL_GetTicks() + duration;
                  while (!stop.load() && SDL_GetTicks() < finish) {
                      if(fractional&&graphics_locomotion_audit_frames.load()>=2)break;
                      SDL_Delay(20);
                      if (graphics_fps_audit_wait.load() & 2u) {
                          modal_seen.store(true);
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
                  done.store(true);
                  // UI redraws may drain a just-queued diagnostic key. Keep
                  // cancel/wake delivery bounded and acknowledged by the main
                  // thread's return; never enqueue gameplay movement here.
                  const auto wake_deadline = SDL_GetTicks() + 12000;
                  while (!stop.load() && SDL_GetTicks() < wake_deadline) {
                      const auto waiting = graphics_fps_audit_wait.load();
                      if (waiting & 2u) {
                          modal_seen.store(true);
                          key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, true);
                          key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, false);
                      } else if (waiting & 1u) {
                          key(SDL_SCANCODE_F8, SDLK_F8, true);
                          key(SDL_SCANCODE_F8, SDLK_F8, false);
                      }
                      SDL_Delay(100);
                  }
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
    size_t dispatch_number = 0;
    auto dispatch = [&](std::vector<SDL_Event> events, int boundaries = 1,
                        unsigned duration = 2000, bool virtual_cursor_outside = false, bool attack_boundary=false,bool fractional=false) {
        checkpoint("fps_input_" + get_option<std::string>("SDL_KEYBOARD_MODE") + "_" +
                   get_option<std::string>("NCMM_FIRST_PERSON_RENDERER") + "_" +
                   std::to_string(++dispatch_number));
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
                event.button.x = virtual_cursor_outside?-16.f:mouse_x;
                event.button.y = virtual_cursor_outside?-16.f:mouse_y;
            }
        graphics_fps_audit_wait.store(0);
        fps_events feed(window, mouse_x, mouse_y, std::move(events), duration,fractional);
        for (int boundary = 0; boundary < boundaries; ++boundary) {
            const auto issued = attack_boundary?graphics_input_router.attack_intents:graphics_input_router.issued;
            for (int retry = 0; retry < 64; ++retry) {
                g->ncmm_graphics_smoke_handle_action();
                // Observe the completed Native step before a following fixture
                // can teleport back to its starting tile. Ordinary play observes
                // it in the next input loop or world redraw instead.
                graphics_locomotion_sync();
                if (feed.sent.load() && ((attack_boundary?graphics_input_router.attack_intents:graphics_input_router.issued) > issued || feed.done.load()))
                    break;
            }
        }
        if (!feed.sent.load())
            log_line(NCMM_LOG_ERROR,
                     ("FPS wait diagnostic bits=" + std::to_string(feed.last_wait.load()) +
                      " owner=" + std::to_string(int(graphics_input_router.owner())) +
                      " focused=" + std::to_string(graphics_fps_focused) +
                      " ui=" + std::to_string(graphics_fps_ui_claimed) +
                      " available=" + std::to_string(graphics_fps_available) +
                      " queued=" + std::to_string(feed.queued.load()))
                         .c_str());
        require(feed.sent.load(), "fps_audit_sdl_script_never_entered_native_wait");
        return feed.modal_seen.load();
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
        float fractional_distance=0;uint32_t presented_frames=0;
    };
    std::vector<sample> samples;
    for (const char *mode : {"keychar", "keycode"}) {
        get_options().get_option("SDL_KEYBOARD_MODE").setValue(mode);
        options_manager::update_options_cache();
        for (const char *renderer_mode : {"cpu", "gpu"}) {
            you.set_desired_movement_mode(move_mode_id("walk"));
            you.set_movement_mode(move_mode_id("walk"));
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
                const auto origin = you.pos_abs();
                const int moves = you.get_moves();
                const auto actions = graphics_input_router.movements;
                // Deliberately disagreeing Russian keycode and physical W.
                dispatch({key_event(SDL_SCANCODE_W, SDL_Keycode(0x0446),true)},1,12000);
                const auto delta = ncmm_graphics_camera::movement(yaw, 1, 0);
                if (you.pos_abs() != origin + tripoint_rel_ms(delta.first, delta.second, 0)) {
                    const auto actual = you.pos_abs() - origin;
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
                require(you.pos_abs() == origin + tripoint_rel_ms(delta.first, delta.second, 0),
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
            const auto origin = you.pos_abs();
            const auto actions = graphics_input_router.movements;
            const int moves = you.get_moves();
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true),
                      key_event(SDL_SCANCODE_D, SDLK_D, true)},1,12000);
            if (you.pos_abs() != origin + tripoint_rel_ms(1, -1, 0) ||
                graphics_input_router.movements != actions + 1 || you.get_moves() >= moves) {
                const auto actual = you.pos_abs() - origin;
                log_line(NCMM_LOG_ERROR,
                         (std::string("FPS chord diagnostic mode=") + mode +
                          " renderer=" + renderer_mode + " actual=" + std::to_string(actual.x()) +
                          "," + std::to_string(actual.y()) +
                          " actions=" + std::to_string(graphics_input_router.movements - actions) +
                          " cost=" + std::to_string(moves - you.get_moves()) +
                          " last=" + graphics_fps_action)
                             .c_str());
            }
            require(you.pos_abs() == origin + tripoint_rel_ms(1, -1, 0) &&
                        graphics_input_router.movements == actions + 1 && you.get_moves() < moves,
                    "fps_audit_chord_not_one_native_diagonal");
            samples.push_back({"wd_one_diagonal", mode, renderer_mode, 1, moves - you.get_moves()});
            floor();
            warm(renderer_mode);
            native_redraw();
            you.set_moves(10000);
            const auto held_origin = you.pos_abs();
            const int held_moves = you.get_moves();
            const auto held_actions = graphics_input_router.movements;
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true)}, 3, 12000);
            require(you.pos_abs() == held_origin + tripoint_rel_ms(0, -3, 0) &&
                        graphics_input_router.movements == held_actions + 3 &&
                        you.get_moves() < held_moves,
                    "fps_audit_held_w_not_three_native_boundaries");
            samples.push_back({"held_w_three_native_boundaries", mode, renderer_mode, 3,
                               held_moves - you.get_moves()});
            floor();
            warm(renderer_mode);
            you.set_moves(10000);
            const auto wall_origin = you.pos_abs();
            const int wall_moves = you.get_moves();
            here.ter_set(here.get_bub(wall_origin) + tripoint_rel_ms(0, -1, 0), ter_id("t_wall"));
            refresh();
            native_redraw();
            const auto wall_actions = graphics_input_router.movements;
            dispatch({key_event(SDL_SCANCODE_W, SDLK_W, true)}, 3, 600);
            require(you.pos_abs() == wall_origin &&
                        graphics_input_router.movements == wall_actions&&you.get_moves()==wall_moves&&
                        std::abs(graphics_walk.motion.position().y)<=.321f,
                    "fps_audit_wall_repeated_held_walk_or_crossed_collision");
            samples.push_back(
                {"wall_blocks_without_turn", mode, renderer_mode, 0, wall_moves - you.get_moves()});
            floor();camera(-ncmm_graphics_camera::pi*.5f,-.08f);warm(renderer_mode);native_redraw();
            graphics_locomotion_reset();graphics_locomotion_audit_frames.store(0);
            const auto fractional_origin=you.pos_abs();const auto fractional_turn=calendar::turn;
            const int fractional_moves=you.get_moves();const auto fractional_actions=graphics_input_router.movements;
            dispatch({key_event(SDL_SCANCODE_W,SDLK_W,true)},1,3000,false,false,true);
            const auto fractional_position=graphics_walk.motion.position();
            const float fractional_distance=std::hypot(fractional_position.x,fractional_position.y);
            require(graphics_locomotion_audit_frames.load()>=2&&fractional_distance>.001f&&fractional_distance<.49f&&
                    you.pos_abs()==fractional_origin&&calendar::turn==fractional_turn&&you.get_moves()==fractional_moves&&
                    graphics_input_router.movements==fractional_actions,"fps_subtile_motion_spent_turn_or_was_not_presented");
            sample fractional_sample{"continuous_subtile_without_turn",mode,renderer_mode,0,0};
            fractional_sample.fractional_distance=fractional_distance;
            fractional_sample.presented_frames=graphics_locomotion_audit_frames.load();samples.push_back(fractional_sample);
            floor();camera(-ncmm_graphics_camera::pi*.5f,-.08f);warm(renderer_mode);native_redraw();
            const auto slide_origin=you.pos_abs();const int slide_moves=you.get_moves();
            for(int y=-5;y<=2;++y)here.ter_set(you.pos_bub()+tripoint_rel_ms(1,y,0),ter_id("t_wall"));
            refresh();native_redraw();const auto slide_actions=graphics_input_router.movements;
            dispatch({key_event(SDL_SCANCODE_W,SDLK_W,true),key_event(SDL_SCANCODE_D,SDLK_D,true)},3,12000);
            require(you.pos_abs()==slide_origin+tripoint_rel_ms(0,-3,0)&&
                    graphics_input_router.movements==slide_actions+3&&you.get_moves()<slide_moves&&
                    graphics_walk.motion.position().x>.05f&&graphics_walk.motion.position().x<=.321f,
                    "fps_continuous_wall_slide_failed");
            samples.push_back({"continuous_wall_slide",mode,renderer_mode,3,slide_moves-you.get_moves()});
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
                     1, 5000);
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
                if (graphics_input_router.captured() ||
                    graphics_input_router.ready(SDL_GetTicks()) || you.pos_abs() != position ||
                    you.get_moves() != before_moves) {
                    const auto delta = you.pos_abs() - position;
                    log_line(
                        NCMM_LOG_ERROR,
                        ("FPS window diagnostic type=" + std::to_string(event_type) +
                         " captured=" + std::to_string(graphics_input_router.captured()) +
                         " ready=" + std::to_string(graphics_input_router.ready(SDL_GetTicks())) +
                         " delta=" + std::to_string(delta.x()) + "," + std::to_string(delta.y()) +
                         " cost=" + std::to_string(before_moves - you.get_moves()) +
                         " owner=" + std::to_string(int(graphics_input_router.owner())) +
                         " initial=" + std::to_string(graphics_fps_initial_capture) +
                         " last=" + graphics_fps_action)
                            .c_str());
                }
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
            for(const char *weapon:{"naginata","spear_steel"}) {
                floor();you.set_moves(10000);you.set_wielded_item(item(itype_id(weapon),calendar::turn));
                // Previous keyboard/renderer matrices spend real stamina. A
                // successful-attack fixture must not inherit a slow-attack
                // confirmation; the production Native warning stays intact.
                you.set_stamina(you.get_stamina_max());
                auto *enemy=g->place_critter_at(mtype_id("mon_zombie"),you.pos_bub()+tripoint_rel_ms(0,-2,0));
                require(enemy&&you.get_wielded_item()->current_reach_range(you).first>1,"fps_audit_reach_fixture");
                const int attack_cost=you.attack_speed(*you.get_wielded_item());
                log_line(NCMM_LOG_INFO,(std::string("FPS reach fixture weapon=")+weapon+" keyboard="+mode+
                    " renderer="+renderer_mode+" native_attack_cost="+std::to_string(attack_cost)+
                    " stamina="+std::to_string(you.get_stamina())+" max_stamina="+std::to_string(you.get_stamina_max())).c_str());
                require(attack_cost<=300,"fps_audit_reach_fixture_requires_native_confirmation");
                camera(-ncmm_graphics_camera::pi*.5f,-.3f);warm(renderer_mode);native_redraw();
                const int before=you.get_moves();const auto position=you.pos_abs();const auto serial=graphics_attack_serial;
                const auto intents=graphics_input_router.attack_intents;
                SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.button=SDL_BUTTON_LEFT;
                SDL_Event release=click;release.type=SDL_EVENT_MOUSE_BUTTON_UP;
                const bool outside=std::string(weapon)=="naginata";
                const bool modal=dispatch({click,release},1,2000,outside);
                log_line(NCMM_LOG_INFO,(std::string("FPS reach diagnostic weapon=")+weapon+" keyboard="+mode+
                    " renderer="+renderer_mode+" native_attack_cost="+std::to_string(attack_cost)+
                    " modal="+std::to_string(modal)+" attacks="+std::to_string(graphics_attack_serial-serial)+
                    " intents="+std::to_string(graphics_input_router.attack_intents-intents)+
                    " moves="+std::to_string(before-you.get_moves())+" pending="+std::to_string(graphics_click_pending)).c_str());
                require(!modal&&graphics_input_router.attack_intents==intents+1&&graphics_attack_serial==serial+1&&you.get_moves()<before&&you.pos_abs()==position&&!graphics_click_pending,
                        "fps_audit_lmb_reach_opened_selector_or_did_not_attack");
                // Exercise the Native 'show' setting and a complete UI redraw,
                // not just the instantaneous return value of SDL_HideCursor.
                const std::string hidden=get_option<std::string>("HIDE_CURSOR");
                get_options().get_option("HIDE_CURSOR").setValue("show");
                ::refresh_mouse_config();native_redraw();
                require(graphics_mouse_drag&&!SDL_CursorVisible(),"fps_audit_imgui_reshowed_captured_cursor");
                graphics_release_mouse();::refresh_mouse_config();
                require(SDL_CursorVisible(),"fps_audit_native_cursor_not_restored");
                get_options().get_option("HIDE_CURSOR").setValue(hidden);::refresh_mouse_config();
                samples.push_back({"lmb_native_reach",mode,renderer_mode,1,before-you.get_moves(),outside?1:0});
            }
            for(int direction=0;direction<2;++direction) {
                floor();you.set_moves(10000);you.set_stamina(you.get_stamina_max());
                you.set_wielded_item(item(itype_id("naginata"),calendar::turn));
                const auto target=you.pos_bub()+tripoint_rel_ms(direction?3:0,-3,0);
                auto *enemy=g->place_critter_at(mtype_id("mon_zombie"),target);
                require(enemy&&you.get_wielded_item()->current_reach_range(you).first==2,
                        "fps_audit_reach_range_fixture");
                // A fixed pitch can pass through transparent pixels between
                // a Native billboard's limbs. Acquire a real centre-ray hit
                // before testing range; never count a click into empty sky.
                // Physical focus is valid even without the optional HUD label.
                bool acquired=false;float selected_pitch=0;
                for(float pitch:{-.3f,-.1f,-.2f,-.05f,0.f}) {
                    camera(direction?-ncmm_graphics_camera::pi*.25f:-ncmm_graphics_camera::pi*.5f,pitch);
                    refresh();warm(renderer_mode);native_redraw();
                    if(graphics_aim==enemy->pos_abs()||graphics_fps_target(false)==target) {
                        acquired=true;selected_pitch=pitch;break;
                    }
                }
                log_line(NCMM_LOG_INFO,(std::string("FPS out-of-range fixture keyboard=")+mode+" renderer="+renderer_mode+
                    " direction="+std::to_string(direction)+" acquired="+std::to_string(acquired)+
                    " pitch="+std::to_string(selected_pitch)+" hud="+std::to_string(graphics_aim==enemy->pos_abs())+
                    " focus="+std::to_string(graphics_fps_target(false)==target)).c_str());
                if(!acquired)capture("reach-range-missed-"+std::string(mode)+"-"+renderer_mode+"-"+std::to_string(direction));
                require(acquired,"fps_audit_out_of_range_reticle_missing");
                require(!graphics_reach_in_range(you,*you.get_wielded_item(),target),"fps_audit_out_of_range_native_distance_fixture");
                const int before=you.get_moves();const auto position=you.pos_abs();
                const auto serial=graphics_attack_serial,intents=graphics_input_router.attack_intents;
                SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.button=SDL_BUTTON_LEFT;
                SDL_Event release=click;release.type=SDL_EVENT_MOUSE_BUTTON_UP;
                const bool modal=dispatch({click,release});
                require(!modal&&graphics_input_router.attack_intents==intents+1&&graphics_attack_serial==serial&&
                        you.get_moves()==before&&you.pos_abs()==position&&!graphics_click_pending,
                        "fps_audit_out_of_range_reach_attacked_or_opened_selector");
                samples.push_back({"lmb_reach_out_of_range",mode,renderer_mode,0,0,direction});
            }
            // Real Native shooting with the setting that normally reopens
            // ACT_AIM after each shot. These FPS cases must never enter TARGET.
            get_options().get_option("AIM_AFTER_FIRING").setValue("true");options_manager::update_options_cache();
            for(int shooting=0;shooting<4;++shooting){
                floor();you.set_moves(10000);you.recoil=6000;
                item gun(itype_id("sw_619"),calendar::turn);
                gun.set_flag(flag_id("WATERPROOF_GUN"));gun.set_flag(flag_id("NEVER_JAMS"));
                if(shooting!=3)gun.ammo_set(itype_id("38_special"),7);
                you.set_wielded_item(gun);
                graphics_prepare_held();require(graphics_held_ready&&(graphics_held_item.flags&NCMM_HELD_COMBUSTION),"fps_powder_ammunition_metadata_missing");
                if(shooting==0)require(g->place_critter_at(mtype_id("mon_zombie"),you.pos_bub()+tripoint_rel_ms(0,-5,0))!=nullptr,"fps_gun_target_fixture");
                camera(-ncmm_graphics_camera::pi*.5f,shooting==0?-.1f:0.f);warm(renderer_mode);native_redraw();
                const int before=you.get_moves(),ammo=you.get_wielded_item()->ammo_remaining();const auto serial=graphics_attack_serial;
                SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;click.button.button=SDL_BUTTON_LEFT;
                SDL_Event release=click;release.type=SDL_EVENT_MOUSE_BUTTON_UP;
                bool modal=false;
                if(shooting==2){
                    SDL_Event aim=click;aim.button.button=SDL_BUTTON_RIGHT;
                    modal=dispatch({aim});
                    require(!modal&&graphics_fps_ads_impl()&&you.get_moves()==before&&!you.activity,"fps_rmb_opened_native_selector");
                }
                // A fresh dispatch deliberately releases capture; restore ADS
                // in the same SDL batch as the click to test simultaneous input.
                if(shooting==2){SDL_Event aim=click;aim.button.button=SDL_BUTTON_RIGHT;SDL_Event aim_up=aim;aim_up.type=SDL_EVENT_MOUSE_BUTTON_UP;
                    modal=dispatch({aim_up,aim,click,release},1,2000,false,true);}
                else modal=dispatch({click,release});
                log_line(NCMM_LOG_INFO,(std::string("FPS gun diagnostic keyboard=")+mode+" renderer="+renderer_mode+" case="+std::to_string(shooting)+
                    " modal="+std::to_string(modal)+" activity="+std::to_string(bool(you.activity))+" captured="+std::to_string(graphics_input_router.captured())+
                    " owner="+std::to_string(int(graphics_input_router.owner()))+" pending="+std::to_string(graphics_fps_gunshot_pending)+
                    " controls="+std::to_string(graphics_control_context)+" ui_claimed="+std::to_string(graphics_fps_ui_claimed)+
                    " imgui_mouse="+std::to_string(cataimgui::client::want_capture_mouse())+" imgui_keys="+std::to_string(cataimgui::client::want_capture_keyboard())+
                    " shots="+std::to_string(ammo-you.get_wielded_item()->ammo_remaining())+" serial="+std::to_string(graphics_attack_serial-serial)+
                    " cost="+std::to_string(before-you.get_moves())).c_str());
                require(!modal&&!you.activity&&graphics_input_router.captured(),"fps_gun_opened_selector_or_lost_capture");
                const int shots=ammo-you.get_wielded_item()->ammo_remaining();
                require(shooting==3?(shots==0&&graphics_attack_serial==serial&&you.get_moves()==before):
                        (shots==1&&graphics_attack_serial==serial+1&&you.get_moves()<before),"fps_gun_ammo_or_native_cost_wrong");
                if(shooting==2){SDL_Event up=release;up.button.button=SDL_BUTTON_RIGHT;dispatch({up});require(!graphics_fps_ads_impl(),"fps_rmb_release_kept_ads");}
                const char *name=shooting==0?"lmb_fps_gun":shooting==1?"lmb_fps_free_fire":shooting==2?"rmb_fps_ads_fire":"lmb_fps_empty";
                samples.push_back({name,mode,renderer_mode,shooting==3?0:1,before-you.get_moves()});
            }
            {
                // Quickstart randomizes strength. A bow's Native readiness gate
                // must pass before this case can qualify reload-and-shoot input.
                // Normalize this fixture only; production retains all requirements.
                struct bow_strength_guard {avatar &who;int bonus;~bow_strength_guard(){who.set_str_bonus(bonus);}}
                    restore_strength{you,you.get_str_bonus()};
                you.mod_str_bonus(std::max(0,12-you.get_str()));
                floor();you.set_moves(10000);you.recoil=0;you.set_stamina(you.get_stamina_max());you.set_wielded_item(item(itype_id("shortbow"),calendar::turn));
                const auto bow=you.get_wielded_item();const auto bow_mode=bow->gun_current_mode();
                graphics_prepare_held();require(graphics_held_ready&&!(graphics_held_item.flags&NCMM_HELD_COMBUSTION),"fps_bow_gained_combustion_effect");
                std::vector<std::string> bow_reasons;
                const bool bow_common=gunmode_checks_common(you,here,bow_reasons,bow_mode);
                const bool bow_ready=gunmode_checks_weapon(you,here,bow_reasons,bow_mode);
                for(const auto &reason:bow_reasons)log_line(NCMM_LOG_INFO,("FPS bow fixture: "+reason).c_str());
                require(bow_common&&bow_ready,"fps_bow_native_readiness_fixture");
                item arrows(itype_id("arrow_wood"),calendar::turn);arrows.charges=2;
                here.add_item_or_charges(you.pos_bub(),arrows);you.ammo_location=item_location();
                std::vector<item::reload_option> bow_ammunition;you.list_ammo(bow,bow_ammunition,false);
                require(std::any_of(bow_ammunition.begin(),bow_ammunition.end(),[](const auto &option){return bool(option);}),
                        "fps_bow_native_ammunition_fixture");
                camera(-ncmm_graphics_camera::pi*.5f,0);warm(renderer_mode);native_redraw();
                const auto bow_serial=graphics_attack_serial;const int bow_moves=you.get_moves();
                SDL_Event bow_click{};bow_click.type=SDL_EVENT_MOUSE_BUTTON_DOWN;bow_click.button.button=SDL_BUTTON_LEFT;
                SDL_Event bow_release=bow_click;bow_release.type=SDL_EVENT_MOUSE_BUTTON_UP;
                const bool bow_modal=dispatch({bow_click,bow_release});
                int arrows_left=0;for(const auto &entry:here.i_at(you.pos_bub()))if(entry.typeId()==itype_id("arrow_wood"))arrows_left+=entry.charges;
                log_line(NCMM_LOG_INFO,(std::string("FPS bow diagnostic keyboard=")+mode+" renderer="+renderer_mode+
                    " str="+std::to_string(you.get_str())+" modal="+std::to_string(bow_modal)+" activity="+std::to_string(bool(you.activity))+
                    " captured="+std::to_string(graphics_input_router.captured())+" serial="+std::to_string(graphics_attack_serial-bow_serial)+
                    " arrows="+std::to_string(arrows_left)+" loaded="+std::to_string(bow->ammo_remaining())+
                    " cost="+std::to_string(bow_moves-you.get_moves())).c_str());
                require(!bow_modal&&!you.activity&&graphics_input_router.captured()&&graphics_attack_serial==bow_serial+1&&you.get_moves()<bow_moves&&arrows_left==1,
                        "fps_bow_did_not_use_costed_native_reload_and_shoot_without_menu");
                samples.push_back({"lmb_fps_bow",mode,renderer_mode,1,bow_moves-you.get_moves()});
            }
            get_options().get_option("AIM_AFTER_FIRING").setValue(previous_aim_after?"true":"false");options_manager::update_options_cache();
            floor();warm(renderer_mode);native_redraw();
            you.set_desired_movement_mode(move_mode_id("walk"));
            you.set_stamina(you.get_stamina_max());
            const int stance_moves = you.get_moves(), run_stamina = you.get_stamina();
            dispatch(tap(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT));
            require(you.is_run_mode_desired(), "fps_audit_shift_did_not_toggle_native_run");
            // Native commits a requested movement mode on the next ordinary
            // action. Verify real running and stamina through a real step.
            dispatch(tap(SDL_SCANCODE_W, SDLK_W));
            require(you.is_running() && you.get_stamina() < run_stamina,
                    "fps_audit_run_step_did_not_use_native_stamina");
            dispatch(tap(SDL_SCANCODE_C, SDLK_C));
            require(you.is_running() && you.is_crouch_mode_desired(),
                    "fps_audit_c_did_not_request_native_crouch");
            const int wait_moves = you.get_moves();
            dispatch(tap(SDL_SCANCODE_SPACE, SDLK_SPACE));
            require(you.is_crouching() && you.get_moves() < wait_moves,
                    "fps_audit_space_did_not_complete_native_pose_and_wait");
            samples.push_back(
                {"native_run_crouch_wait", mode, renderer_mode, 4, stance_moves - you.get_moves()});
            you.set_movement_mode(move_mode_id("walk"));
            you.set_desired_movement_mode(move_mode_id("walk"));

            floor();
            item revolver(itype_id("sw_619"), calendar::turn);
            revolver.ammo_unset();
            you.set_wielded_item(revolver);
            item rounds(itype_id("38_special"), calendar::turn);
            rounds.charges = 2;
            here.add_item_or_charges(you.pos_bub(), rounds);
            warm(renderer_mode);
            native_redraw();
            const int reload_moves = you.get_moves();
            dispatch(tap(SDL_SCANCODE_R, SDLK_R));
            require(graphics_fps_action == "reload_weapon" && bool(you.activity),
                    "fps_audit_r_did_not_start_native_reload");
            for (int turn = 0; turn < 16 && you.activity; ++turn)
                you.activity.do_turn(you);
            require(you.get_wielded_item()->ammo_remaining() > 0 && you.get_moves() < reload_moves,
                    "fps_audit_r_did_not_complete_costed_reload");
            samples.push_back(
                {"r_native_reload", mode, renderer_mode, 1, reload_moves - you.get_moves()});

            floor();
            item light(itype_id("flashlight"), calendar::turn);
            item battery(itype_id("medium_battery_cell"), calendar::turn);
            battery.ammo_set(battery.ammo_default(), 20);
            require(light.put_in(battery, pocket_type::MAGAZINE_WELL).success(),
                    "fps_audit_u_battery_fixture");
            you.set_wielded_item(light);
            warm(renderer_mode);
            native_redraw();
            const int use_moves = you.get_moves();
            dispatch(tap(SDL_SCANCODE_U, SDLK_U));
            require(graphics_fps_action == "apply_wielded" && you.get_wielded_item() &&
                        you.get_wielded_item()->typeId() == itype_id("flashlight_on") &&
                        you.get_moves() <= use_moves,
                    "fps_audit_u_did_not_activate_native_tool");
            samples.push_back(
                {"u_native_tool", mode, renderer_mode, 1, use_moves - you.get_moves()});

            // Real Native selectors own these waits. Cancellation must leave
            // the world unchanged, including the items and the movement cost.
            auto cancel_interaction = [&](const char *name, const tripoint_bub_ms &target,
                                          float pitch, SDL_Scancode key, SDL_Keycode value) {
                checkpoint(std::string("fps_interaction_") + mode + "_" + renderer_mode + "_" +
                           name);
                camera(-ncmm_graphics_camera::pi * .5f, pitch);
                refresh();
                warm(renderer_mode);
                native_redraw();
                if (graphics_fps_target(true) != target) {
                    const auto expected = here.get_abs(target);
                    const auto actual = graphics_fps_focus.target;
                    log_line(NCMM_LOG_ERROR, (std::string("FPS interaction ray diagnostic case=") +
                                              name + " expected=" + std::to_string(expected.x()) +
                                              "," + std::to_string(expected.y()) + "," +
                                              std::to_string(expected.z()) + " actual=" +
                                              (actual ? std::to_string(actual->x()) + "," +
                                                            std::to_string(actual->y()) + "," +
                                                            std::to_string(actual->z())
                                                      : "none") +
                                              " eye=" + std::to_string(graphics_eye_height()) +
                                              " pitch=" + std::to_string(graphics_camera_pitch))
                                                 .c_str());
                }
                require(graphics_fps_target(true) == target,
                        (std::string("fps_audit_interaction_ray_missed:") + name).c_str());
                const auto position = you.pos_abs();
                const int before = you.get_moves();
                const bool modal = dispatch(tap(key, value));
                require(modal && you.pos_abs() == position && you.get_moves() == before &&
                            graphics_fps_action == (key == SDL_SCANCODE_G ? "pickup" : "interact"),
                        "fps_audit_interaction_cancel_not_native_or_changed_world");
                samples.push_back({name, mode, renderer_mode, 1, 0});
            };
            floor();
            you.set_wielded_item(item());
            const auto ground = you.pos_bub() + tripoint_rel_ms(0, -1, 0);
            here.add_item_or_charges(ground, item(itype_id("rock"), calendar::turn));
            cancel_interaction("g_native_pickup_cancel", ground, -.98f, SDL_SCANCODE_G, SDLK_G);
            require(!here.i_at(ground).empty(), "fps_audit_g_cancel_removed_item");
            cancel_interaction("e_ground_item_cancel", ground, -.98f, SDL_SCANCODE_E, SDLK_E);
            require(!here.i_at(ground).empty(), "fps_audit_e_cancel_removed_item");
            here.furn_set(ground, furn_id("f_cupboard"));
            // The existing renderer classifies walkable FLAT_SURF furniture
            // as a flat surface. Aim at that real surface, not an assumed
            // tall cabinet; keep the exact centre-ray ownership requirement.
            cancel_interaction("e_container_cancel", ground, -.98f, SDL_SCANCODE_E, SDLK_E);
            require(here.furn(ground) == furn_id("f_cupboard") && !here.i_at(ground).empty(),
                    "fps_audit_container_cancel_changed_contents");

            floor();
            const auto window_tile = you.pos_bub() + tripoint_rel_ms(0, -1, 0);
            here.ter_set(window_tile, ter_id("t_window_domestic"));
            // Native window operations are inside-only. Give the fixture an
            // actual roof rather than overriding Native availability checks.
            for (int y = -2; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                    here.ter_set(you.pos_bub() + tripoint_rel_ms(x, y, 1), ter_id("t_floor"));
            cancel_interaction("e_window_cancel", window_tile, -.08f, SDL_SCANCODE_E, SDLK_E);
            require(here.ter(window_tile) == ter_id("t_window_domestic"),
                    "fps_audit_window_cancel_changed_window");

            floor();
            const auto npc_tile = you.pos_bub() + tripoint_rel_ms(0, -1, 0);
            auto person = make_shared_fast<npc>();
            person->normalize();
            person->name = "FPS interaction NPC";
            person->set_base_height(175);
            person->spawn_at_precise(here.get_abs(npc_tile));
            overmap_buffer.insert_npc(person);
            g->load_npcs();
            person->setpos(here, npc_tile, false);
            cancel_interaction("e_npc_cancel", npc_tile, -.3f, SDL_SCANCODE_E, SDLK_E);
            require(get_creature_tracker().creature_at<npc>(npc_tile) == person.get(),
                    "fps_audit_npc_cancel_changed_identity");

            floor();
            const auto terminal = you.pos_bub() + tripoint_rel_ms(0, -1, 0);
            require(here.add_computer(terminal, "FPS input audit", 0) != nullptr,
                    "fps_audit_terminal_fixture");
            // The existing fallback console billboard is one metre tall;
            // target its middle instead of grazing its top alpha boundary.
            cancel_interaction("e_terminal_cancel", terminal, -.8f, SDL_SCANCODE_E, SDLK_E);

            floor();
            // At the existing pitch limit, crouching permits a real centre
            // ray to hit the stair underfoot. Native still owns the Z move.
            you.set_movement_mode(move_mode_id("crouch"));
            you.set_desired_movement_mode(move_mode_id("crouch"));
            here.ter_set(you.pos_bub(), ter_id("t_stairs_down"));
            here.ter_set(you.pos_bub() + tripoint_rel_ms(0, 0, -1), ter_id("t_stairs_up"));
            camera(-ncmm_graphics_camera::pi * .5f, -1.2f);
            refresh();
            warm(renderer_mode);
            native_redraw();
            require(graphics_fps_target(true) == you.pos_bub(), "fps_audit_stair_ray_missed");
            const int stair_z = you.posz(), stair_moves = you.get_moves();
            dispatch(tap(SDL_SCANCODE_E, SDLK_E));
            require(you.posz() == stair_z - 1 && you.get_moves() < stair_moves,
                    "fps_audit_e_did_not_use_native_vertical_move");
            samples.push_back(
                {"e_native_stair", mode, renderer_mode, 1, stair_moves - you.get_moves()});
            you.set_movement_mode(move_mode_id("walk"));
            you.set_desired_movement_mode(move_mode_id("walk"));

            floor();
            {
                const auto lower=you.pos_abs();const int dx=ncmm_world::omt_axis(lower.x())==ncmm_world::omt_axis(lower.x()+1)?1:-1;
                const auto destination=lower+tripoint_rel_ms(dx,0,1);
                here.ter_set(you.pos_bub(),ter_id("t_stairs_up"));
                here.ter_set(here.get_bub(lower+tripoint_rel_ms(0,0,1)),ter_id("t_floor"));
                here.ter_set(here.get_bub(lower+tripoint_rel_ms(dx,0,0)),ter_id("t_rock_wall"));
                here.ter_set(here.get_bub(destination),ter_id("t_stairs_down"));
                require(here.get_abs_sub().z()==you.posz(),"fps_audit_offset_fixture_map_level_mismatch");
                require(g->find_stairs(here,you.posz()+1,you.pos_bub())==here.get_bub(destination),"fps_audit_offset_fixture_not_native_stair_pair");
                camera(dx>0?0:ncmm_graphics_camera::pi,.25f);refresh();warm(renderer_mode);
                struct stair_observation {
                    ncmm_graphics_render_multilevel_v1 provider;void *user;tripoint_abs_ms lower,upper;bool from=false,to=false;
                    static int render(const ncmm_graphics_multilevel_scene_v1 *world,void *data){auto &self=*static_cast<stair_observation *>(data);
                        for(uint32_t n=0;n<world->cell_count;++n){const auto &c=world->coordinates[n];
                            if(c.x==self.lower.x()&&c.y==self.lower.y()&&c.z==self.lower.z())self.from|=bool(c.flags&NCMM_WORLD_CONNECTED_UP);
                            if(c.x==self.upper.x()&&c.y==self.upper.y()&&c.z==self.upper.z())self.to|=bool(c.flags&NCMM_WORLD_DOWN);}
                        return self.provider(world,self.user);}
                } observed{graphics_view.world_render,graphics_view.user_data,lower,destination};
                require(observed.provider!=nullptr,"fps_audit_offset_world_provider_missing");
                {
                    struct restore_callback {stair_observation &value;~restore_callback(){graphics_view.world_render=value.provider;graphics_view.user_data=value.user;}} restore{observed};
                    graphics_view.world_render=&stair_observation::render;graphics_view.user_data=&observed;native_redraw();
                }
                require(observed.from&&observed.to,"fps_audit_offset_pair_not_in_actual_renderer_snapshot");
                capture("offset-stair-lower-"+std::string(mode)+"-"+renderer_mode);
                const int before=you.get_moves();
                {cancel_native_ui cancel(window);g->vertical_move(1,false);}
                require(you.pos_abs()==destination&&you.get_moves()<before,"fps_audit_offset_native_destination_or_cost_changed");
                camera(dx>0?ncmm_graphics_camera::pi:0,-.65f);
                capture("offset-stair-upper-"+std::string(mode)+"-"+renderer_mode);
                samples.push_back({"native_offset_stair",mode,renderer_mode,1,before-you.get_moves()});
            }
            floor();
            vehicle *car = here.add_vehicle(vproto_id("car"), point_at(0, -3), 0_degrees, 100,
                                            veh_spawn_status::UNDAMAGED, false);
            require(car != nullptr, "fps_audit_vehicle_fixture");
            car->is_locked = false;
            car->set_owner(you);
            bool vehicle_checked = false;
            for (const vpart_reference &part : car->get_all_parts()) {
                if (!part.has_feature("OPENABLE"))
                    continue;
                const auto p = part.pos_bub(here);
                const auto viewer = p + tripoint_rel_ms(0, 1, 0);
                if (here.veh_at(viewer) || !here.passable(viewer))
                    continue;
                you.setpos(here, viewer, false);
                cancel_interaction("e_vehicle_cancel", p, -.7f, SDL_SCANCODE_E, SDLK_E);
                vehicle_checked = true;
                break;
            }
            require(vehicle_checked, "fps_audit_no_exterior_vehicle_door");

            floor();
            const auto blocker = you.pos_bub() + tripoint_rel_ms(0, -1, 0);
            const auto hidden_door = blocker + tripoint_rel_ms(0, -1, 0);
            here.ter_set(blocker, ter_id("t_wall"));
            here.ter_set(hidden_door, ter_id("t_door_c"));
            camera(-ncmm_graphics_camera::pi * .5f, -.08f);
            refresh();
            warm(renderer_mode);
            native_redraw();
            require(graphics_fps_target(true) == blocker && graphics_fps_hint().empty(),
                    "fps_audit_wall_did_not_hide_interaction");
            const int blocked_moves = you.get_moves();
            dispatch(tap(SDL_SCANCODE_E, SDLK_E));
            require(you.get_moves() == blocked_moves && here.ter(hidden_door) == ter_id("t_door_c"),
                    "fps_audit_e_interacted_through_wall");
            samples.push_back({"e_occluded_refusal", mode, renderer_mode, 0, 0});
            floor();camera(-ncmm_graphics_camera::pi*.5f,-.35f);warm(renderer_mode);native_redraw();
            const uint64_t ground_triangles=graphics_gpu_engine?graphics_gpu_engine->metrics().triangles:0;
            const std::array<const char *,4> lawns={"t_grass","t_grass_long","t_grass_dead","t_grass_golf"};
            for(int y=2;y<=5;++y)for(int x=-2;x<=2;++x)
                here.ter_set(you.pos_bub()+tripoint_rel_ms(x,-y,0),ter_id(lawns[size_t(y-2)]));
            const auto grass_position=you.pos_abs();const int grass_moves=you.get_moves();const auto grass_turn=calendar::turn;
            refresh();warm(renderer_mode);native_redraw();
            require(graphics_grass_active,"fps_audit_native_grass_not_visible_to_renderer");
            if(std::string(renderer_mode)=="gpu")require(graphics_gpu_engine&&graphics_gpu_engine->metrics().triangles>ground_triangles+100,
                    "fps_audit_native_grass_has_no_blade_geometry");
            {
                struct grass_clock {std::optional<double> previous;~grass_clock(){graphics_smoke_seconds=previous;}} grass_time_restore{graphics_smoke_seconds};
                graphics_smoke_seconds=5.0;capture("grass-native-still-"+std::string(mode)+"-"+renderer_mode);
                graphics_smoke_seconds=5.6;capture("grass-native-wind-"+std::string(mode)+"-"+renderer_mode);
            }
            require(you.pos_abs()==grass_position&&you.get_moves()==grass_moves&&calendar::turn==grass_turn,
                    "fps_audit_grass_animation_advanced_native_state");
            samples.push_back({"grass_native_presentation",mode,renderer_mode,0,0});
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
        if(entry.name=="continuous_subtile_without_turn"){
            out.member("fractional_distance",entry.fractional_distance);out.member("presented_frames",entry.presented_frames);
        }
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
