// Included after the existing scene/control implementations in the Native Host.
// SDL observation, action ownership and Native dispatch share the main thread.
static std::string manager_setting_value(const module_setting_meta &);
static bool manager_adjust_setting(const module_setting_meta &, int);
static bool manager_persist_settings();
static bool manager_reset_setting(const module_setting_meta &);

double graphics_fps_sensitivity = 1;
bool graphics_fps_invert = false;
bool graphics_fps_enabled = true;
#if defined(TILES)

struct graphics_fps_focus_state {
    std::optional<tripoint_abs_ms> target, origin;
    float yaw = 0, pitch = 0;
    uint64_t revision = 0, entity = 0;
} graphics_fps_focus;
std::optional<tripoint_abs_ms> graphics_fps_dispatch_target, graphics_fps_open_target;
std::string graphics_fps_dispatch_action, graphics_fps_action;
std::optional<tripoint_abs_ms> graphics_fps_move_origin;
std::array<ncmm_graphics_input::button, SDL_SCANCODE_COUNT> graphics_fps_keys;
bool graphics_fps_keys_ready = false, graphics_fps_focused = true, graphics_fps_available = true;
bool graphics_fps_initial_capture = false, graphics_fps_ui_claimed = false;
bool graphics_fps_outgoing_owned = false;
// Opt-in Native fixture signalling only. A worker may enqueue SDL events; it
// never reads Native objects or polls hardware button state.
bool graphics_fps_audit_active = false, graphics_fps_audit_default = false,
     graphics_fps_audit_waiting = false;
std::atomic<uint32_t> graphics_fps_audit_wait{0};
void graphics_fps_publish_wait() {
    if (graphics_fps_audit_active)
        graphics_fps_audit_wait.store(!graphics_fps_audit_waiting
                                          ? 0u
                                          : (graphics_fps_audit_default ? 1u : 2u) |
                                                (graphics_input_router.command_mode() ? 8u : 0u) |
                                                (graphics_input_router.captured() ? 16u : 0u));
}
void graphics_fps_reset() {
    graphics_input_router.reset();
    graphics_fps_focus = {};
    graphics_fps_dispatch_target.reset();
    graphics_fps_open_target.reset();
    graphics_fps_move_origin.reset();
    graphics_fps_action.clear();
    graphics_fps_dispatch_action.clear();
    graphics_fps_initial_capture = false;
    graphics_fps_keys_ready = false;
    graphics_fps_ui_claimed = false;
    graphics_fps_available = true;
}
void graphics_fps_on_enable() {
    graphics_fps_initial_capture = true;
    graphics_fps_available = true;
    if (world_setting_get_bool("NCMM_FP_HELP", 1)) {
        add_msg(m_info, "%s",
                tr_ui("FPS: mouse look, WASD move, LMB attack, RMB aim, E interact; Tab native "
                      "commands; Esc release; F5 settings; F6 map.",
                      "FPS: мышь — осмотр, WASD — ходьба, ЛКМ — атака, ПКМ — прицел, E — "
                      "взаимодействие; Tab — команды; Esc — курсор; F5 — настройки; F6 — карта."));
        for (const auto &setting : module_settings)
            if (setting.module_id == graphics_view.owner && setting.setting_id == "NCMM_FP_HELP") {
                if (manager_adjust_setting(setting, 1))
                    manager_persist_settings();
                break;
            }
    }
}
struct graphics_fps_binding {
    const char *id, *fallback;
    ncmm_graphics_input::button action;
};
const graphics_fps_binding graphics_fps_bindings[] = {
    {"NCMM_FP_FORWARD", "W", ncmm_graphics_input::button::forward},
    {"NCMM_FP_BACK", "S", ncmm_graphics_input::button::back},
    {"NCMM_FP_LEFT", "A", ncmm_graphics_input::button::left},
    {"NCMM_FP_RIGHT", "D", ncmm_graphics_input::button::right},
    {"NCMM_FP_INTERACT", "E", ncmm_graphics_input::button::interact},
    {"NCMM_FP_RELOAD", "R", ncmm_graphics_input::button::reload},
    {"NCMM_FP_PICKUP", "G", ncmm_graphics_input::button::pickup},
    {"NCMM_FP_INVENTORY", "I", ncmm_graphics_input::button::inventory},
    {"NCMM_FP_COMMANDS", "Tab", ncmm_graphics_input::button::commands},
    {"NCMM_FP_USE", "U", ncmm_graphics_input::button::use}};
void graphics_fps_refresh_settings() {
    graphics_fps_enabled = world_setting_get_bool("NCMM_FP_ENABLED", 1) != 0;
    const double sensitivity = world_setting_get_f64("NCMM_FP_SENSITIVITY", 1);
    graphics_fps_sensitivity = std::isfinite(sensitivity) ? std::clamp(sensitivity, .05, 4.0) : 1;
    graphics_fps_invert = world_setting_get_bool("NCMM_FP_INVERT_Y", 0) != 0;
    graphics_input_router.repeat_ms(
        uint32_t(std::clamp<int64_t>(world_setting_get_i64("NCMM_FP_REPEAT_MS", 140), 100, 400)));
    graphics_fps_keys.fill(ncmm_graphics_input::button::none);
    std::array<bool, SDL_SCANCODE_COUNT> assigned{};
    for (const auto &binding : graphics_fps_bindings) {
        const auto code =
            SDL_GetScancodeFromName(world_setting_get_string(binding.id, binding.fallback));
        // Conflicting assignments never fire two actions. The settings dialog
        // refuses duplicates; malformed external options leave that key inert.
        if (code > SDL_SCANCODE_UNKNOWN && code < SDL_SCANCODE_COUNT) {
            if (assigned[code])
                graphics_fps_keys[code] = ncmm_graphics_input::button::none;
            else {
                graphics_fps_keys[code] = binding.action;
                assigned[code] = true;
            }
        }
    }
    graphics_fps_keys[SDL_SCANCODE_LSHIFT] = graphics_fps_keys[SDL_SCANCODE_RSHIFT] =
        ncmm_graphics_input::button::run;
    graphics_fps_keys[SDL_SCANCODE_LCTRL] = graphics_fps_keys[SDL_SCANCODE_RCTRL] =
        ncmm_graphics_input::button::crouch;
    graphics_fps_keys[SDL_SCANCODE_C] = ncmm_graphics_input::button::crouch;
    graphics_fps_keys[SDL_SCANCODE_SPACE] = ncmm_graphics_input::button::wait;
    graphics_fps_keys[SDL_SCANCODE_F5] = ncmm_graphics_input::button::settings;
    graphics_fps_keys_ready = true;
}
void graphics_fps_sync() {
    ncmm_graphics_input::environment environment;
    environment.enabled = graphics_view.enabled;
    environment.available =
        graphics_fps_available && character_state_available() && !graphics_view.owner.empty();
    environment.focused = graphics_fps_focused;
    environment.fps = graphics_fps_enabled;
    environment.gameplay = graphics_control_context && !graphics_fps_ui_claimed;
    environment.aiming = graphics_context.aiming();
    environment.interacting =
        !graphics_control_context && graphics_fps_dispatch_action == "interact";
    environment.vehicle = character_state_available() && get_avatar().controlling_vehicle;
    graphics_input_router.update(environment);
    if (graphics_fps_initial_capture &&
        graphics_input_router.owner() == ncmm_graphics_input::context::gameplay) {
        if (graphics_input_router.capture())
            graphics_fps_initial_capture = false;
    }
    if (!graphics_apply_mouse_capture(graphics_input_router.captured()))
        graphics_input_router.release();
    graphics_fps_publish_wait();
}
void graphics_fps_input_begin(const std::string &category) {
    graphics_input_router.native_wait(category == "DEFAULTMODE");
    graphics_fps_audit_default = category == "DEFAULTMODE";
    graphics_fps_audit_waiting = true;
    if (category == "DEFAULTMODE")
        graphics_fps_refresh_settings();
    graphics_fps_sync();
}
void graphics_fps_input_end(const std::string &action) {
    graphics_input_router.native_wait(false);
    graphics_fps_audit_waiting = false;
    if (graphics_fps_audit_active)
        graphics_fps_audit_wait.store(0);
    if (!graphics_fps_outgoing_owned && action != "TIMEOUT" &&
        !ncmm_graphics_snapshot::input_lease::camera_action(action))
        graphics_input_router.interrupt();
    graphics_fps_outgoing_owned = false;
}
void graphics_fps_native_begin() {
    if (graphics_fps_move_origin && character_state_available())
        graphics_input_router.completed_move(get_map().get_abs(get_avatar().pos_bub()) !=
                                             *graphics_fps_move_origin);
    graphics_fps_move_origin.reset();
    graphics_fps_open_target.reset();
    graphics_fps_dispatch_action.clear();
}
void graphics_fps_rendered(bool success) {
    graphics_fps_available = success;
    if (!success) {
        graphics_fps_initial_capture = false;
        graphics_release_mouse();
        graphics_fps_focus = {};
    } else if (graphics_input_router.captured() &&
               (graphics_deferred_dx != 0 || graphics_deferred_dy != 0)) {
        const float dx = graphics_deferred_dx, dy = graphics_deferred_dy;
        graphics_deferred_dx = graphics_deferred_dy = 0;
        graphics_process_mouse(3, 0, 0, dx, dy, true);
    }
}
bool graphics_route_sdl_impl(const SDL_Event &event, const SDL_Event &display, bool allowed) {
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        graphics_fps_focused = false;
        graphics_fps_initial_capture = false;
        graphics_fps_sync();
        return false;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
        graphics_fps_focused = true;
        graphics_fps_initial_capture = false;
        if (!graphics_fps_keys_ready)
            graphics_fps_refresh_settings();
        int count = 0;
        const bool *keys = SDL_GetKeyboardState(&count);
        uint32_t held = 0;
        for (int code = 1; keys && code < count && code < SDL_SCANCODE_COUNT; ++code)
            if (keys[code] && graphics_fps_keys[code] != ncmm_graphics_input::button::none)
                held |= uint32_t(1) << unsigned(graphics_fps_keys[code]);
        graphics_input_router.reconcile_focus(held);
        graphics_fps_sync();
        return false;
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED ||
        event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        graphics_fps_initial_capture = false;
        graphics_release_mouse();
        return false;
    }
    if (!graphics_fps_keys_ready)
        graphics_fps_refresh_settings();
    graphics_fps_ui_claimed = !allowed;
    graphics_fps_sync();
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        if (event.type == SDL_EVENT_KEY_DOWN && (event.key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI))) {
            graphics_fps_initial_capture = false;
            graphics_release_mouse();
            return false;
        }
        if (event.key.scancode == SDL_SCANCODE_ESCAPE && event.type == SDL_EVENT_KEY_DOWN &&
            (graphics_input_router.owner() == ncmm_graphics_input::context::gameplay ||
             graphics_input_router.owner() == ncmm_graphics_input::context::vehicle) &&
            graphics_input_router.captured()) {
            graphics_fps_initial_capture = false;
            graphics_release_mouse();
            return true;
        }
        if (event.key.scancode > SDL_SCANCODE_UNKNOWN && event.key.scancode < SDL_SCANCODE_COUNT) {
            const auto key = graphics_fps_keys[event.key.scancode];
            if (!graphics_fps_enabled && key == ncmm_graphics_input::button::commands)
                return false;
            const bool consumed = graphics_input_router.key(key, event.type == SDL_EVENT_KEY_DOWN,
                                                            event.key.repeat, SDL_GetTicks());
            graphics_fps_sync();
            return consumed;
        }
    }
    const bool gameplay = graphics_input_router.owner() == ncmm_graphics_input::context::gameplay;
    const bool vehicle = graphics_input_router.owner() == ncmm_graphics_input::context::vehicle;
    if (event.type == SDL_EVENT_TEXT_INPUT && gameplay)
        return true; // no duplicate keychar/IME command
    if (event.type == SDL_EVENT_MOUSE_MOTION && (gameplay || vehicle) &&
        graphics_input_router.captured())
        return graphics_process_mouse(3, display.motion.x, display.motion.y, event.motion.xrel,
                                      event.motion.yrel, allowed);
    if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
        (gameplay || vehicle) &&
        (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)) {
        const bool inside = display.button.x >= graphics_viewport.x &&
                            display.button.x < graphics_viewport.x + graphics_viewport.w &&
                            display.button.y >= graphics_viewport.y &&
                            display.button.y < graphics_viewport.y + graphics_viewport.h;
        if (!inside)
            return false;
        if (!graphics_input_router.captured()) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                graphics_input_router.capture();
                graphics_fps_sync();
                return true; // capture, never attack
            }
            return true;
        }
        if (vehicle)
            return true; // Native keys drive; free look never sends a drive/attack action.
        return graphics_input_router.key(
            event.button.button == SDL_BUTTON_LEFT ? ncmm_graphics_input::button::attack
                                                   : ncmm_graphics_input::button::aim,
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN, false, SDL_GetTicks());
    }
    return false;
}
bool graphics_fps_pending_impl() {
    return graphics_input_router.ready(SDL_GetTicks());
}
int graphics_set_focus(int32_t cell) {
    if (!graphics_hybrid_frame || !graphics_hybrid_positions ||
        active_module_id != graphics_view.owner)
        return 0;
    graphics_fps_focus = {};
    if (cell < 0)
        return 1;
    if (size_t(cell) >= graphics_hybrid_positions->size() ||
        size_t(cell) >= graphics_hybrid_frame->base.cell_count)
        return 0;
    if (graphics_multilevel_frame &&
        !(graphics_multilevel_frame->coordinates[cell].flags & NCMM_WORLD_BODY_VISIBLE))
        return 0;
    if (!(graphics_hybrid_frame->base.cells[cell].flags & NCMM_GFX_VISIBLE))
        return 0;
    graphics_fps_focus.target = get_map().get_abs((*graphics_hybrid_positions)[cell]);
    graphics_fps_focus.origin = get_map().get_abs(get_avatar().pos_bub());
    graphics_fps_focus.yaw = graphics_camera_yaw;
    graphics_fps_focus.pitch = graphics_camera_pitch;
    graphics_fps_focus.revision = graphics_world_revision;
    if (const auto *creature =
            get_creature_tracker().creature_at<Creature>((*graphics_hybrid_positions)[cell]))
        graphics_fps_focus.entity = creature->ncmm_graphics_entity_id();
    return 1;
}
const ncmm_host_graphics_focus_v1 graphics_focus_api = {1, sizeof(ncmm_host_graphics_focus_v1), 1,
                                                        0, graphics_set_focus};
std::optional<tripoint_bub_ms> graphics_fps_target(bool adjacent) {
    if (!graphics_fps_focus.target || !graphics_fps_focus.origin || !character_state_available())
        return std::nullopt;
    avatar &you = get_avatar();
    map &here = get_map();
    if (here.get_abs(you.pos_bub()) != *graphics_fps_focus.origin ||
        graphics_world_revision != graphics_fps_focus.revision ||
        std::abs(std::remainder(graphics_camera_yaw - graphics_fps_focus.yaw, 6.2831853f)) >
            .001f ||
        std::abs(graphics_camera_pitch - graphics_fps_focus.pitch) > .001f)
        return std::nullopt;
    const auto p = here.get_bub(*graphics_fps_focus.target);
    if (!here.ncmm_graphics_loaded(p) || !you.sees(here, p))
        return std::nullopt;
    const auto *creature = get_creature_tracker().creature_at<Creature>(p);
    if ((creature ? creature->ncmm_graphics_entity_id() : 0) != graphics_fps_focus.entity)
        return std::nullopt;
    if (adjacent && (p.z() != you.posz() || rl_dist(p, you.pos_bub()) > 1))
        return std::nullopt;
    return p;
}
std::vector<action_id> graphics_fps_interactions(const tripoint_bub_ms &p) {
    std::vector<action_id> result;
    for (action_id action : {ACTION_OPEN, ACTION_CLOSE, ACTION_EXAMINE, ACTION_PICKUP, ACTION_CHAT,
                             ACTION_MOVE_UP, ACTION_MOVE_DOWN}) {
        if ((action == ACTION_MOVE_UP || action == ACTION_MOVE_DOWN) && p != get_avatar().pos_bub())
            continue;
        if (action == ACTION_CHAT) {
            const auto *creature = get_creature_tracker().creature_at<Creature>(p);
            if (!creature || !creature->is_npc())
                continue;
        }
        if (can_interact_at(action, get_map(), p))
            result.push_back(action);
    }
    return result;
}
std::string graphics_fps_hint() {
    if (graphics_input_router.owner() == ncmm_graphics_input::context::vehicle)
        return tr_ui("Vehicle: Native controls; click for free look, Tab for pointer",
                     "Транспорт: штатные команды; щелчок — осмотр, Tab — курсор");
    if (graphics_input_router.owner() == ncmm_graphics_input::context::commands &&
        graphics_fps_enabled)
        return tr_ui("Native commands — Tab returns to FPS",
                     "Штатные команды — Tab возвращает FPS");
    if (!graphics_fps_enabled || !graphics_control_context || !graphics_input_router.captured())
        return {};
    // A camera-only frame may keep the same target. Native availability is
    // queried once per target and explicit input-lease generation, not per frame.
    static std::optional<tripoint_abs_ms> cached_target;
    static uint64_t cached_revision = 0, cached_generation = 0, cached_entity = 0;
    static std::string hint;
    const uint64_t generation = graphics_snapshot_lease.generation();
    if (cached_target == graphics_fps_focus.target && cached_revision == graphics_world_revision &&
        cached_generation == generation && cached_entity == graphics_fps_focus.entity)
        return hint;
    cached_target = graphics_fps_focus.target;
    cached_revision = graphics_world_revision;
    cached_generation = generation;
    cached_entity = graphics_fps_focus.entity;
    hint.clear();
    const auto target = graphics_fps_target(true);
    if (!target)
        return {};
    const auto actions = graphics_fps_interactions(*target);
    if (actions.empty())
        return {};
    const std::string key = world_setting_get_string("NCMM_FP_INTERACT", "E");
    if (actions.size() > 1)
        return hint = key + tr_ui(" — Interact", " — Взаимодействовать");
    const auto ctxt = get_default_mode_input_context();
    return hint = key + " — " + ctxt.get_action_name(action_ident(actions.front()));
}
void graphics_fps_settings() {
    graphics_release_mouse();
    bool dirty = false;
    for (;;) {
        uilist menu;
        menu.title = tr_ui("First person controls — physical keys",
                           "Управление первым лицом — физические клавиши");
        std::vector<module_setting_meta> entries;
        for (const auto &setting : module_settings)
            if (setting.module_id == graphics_view.owner &&
                setting.setting_id.rfind("NCMM_FP_", 0) == 0 &&
                setting.setting_id != "NCMM_FP_HELP") {
                entries.push_back(setting);
                menu.addentry(int(entries.size()) - 1, true, -1,
                              setting.name + ": " + manager_setting_value(setting));
            }
        menu.query();
        if (menu.ret < 0 || size_t(menu.ret) >= entries.size())
            break;
        const auto &setting = entries[size_t(menu.ret)];
        bool changed = false;
        if (setting.type == "enum") {
            uilist choices;
            choices.title = setting.name;
            for (size_t i = 0; i < setting.choices.size(); ++i)
                choices.addentry(int(i), true, -1, setting.choices[i].second);
            choices.query();
            if (choices.ret >= 0 && size_t(choices.ret) < setting.choices.size()) {
                const auto &value = setting.choices[size_t(choices.ret)].first;
                bool duplicate = false;
                for (const auto &binding : graphics_fps_bindings)
                    if (setting.setting_id != binding.id &&
                        value == world_setting_get_string(binding.id, binding.fallback))
                        duplicate = true;
                if (duplicate)
                    popup(tr_ui("That physical key is already assigned.",
                                "Эта физическая клавиша уже назначена."));
                else {
                    // Use the same NCMM adjustment/persistence path as its
                    // existing world-options screen, including deferred values.
                    for (size_t n = 0;
                         n < setting.choices.size() &&
                         world_setting_get_string(setting.setting_id.c_str(), "") != value;
                         ++n)
                        changed = manager_adjust_setting(setting, 1) || changed;
                }
            }
        } else if (setting.type == "bool")
            changed = manager_adjust_setting(setting, 1);
        else {
            uilist adjust;
            adjust.title = setting.name + ": " + manager_setting_value(setting);
            adjust.addentry(0, true, -1, tr_ui("Decrease", "Уменьшить"));
            adjust.addentry(1, true, -1, tr_ui("Increase", "Увеличить"));
            adjust.addentry(2, true, -1, tr_ui("Reset", "По умолчанию"));
            adjust.query();
            if (adjust.ret == 0 || adjust.ret == 1)
                changed = manager_adjust_setting(setting, adjust.ret == 0 ? -1 : 1);
            else if (adjust.ret == 2)
                changed = manager_reset_setting(setting);
        }
        dirty |= changed;
    }
    if (dirty && !manager_persist_settings())
        popup(tr_ui("Could not save control settings.",
                    "Не удалось сохранить настройки управления."));
    graphics_fps_refresh_settings();
    graphics_fps_initial_capture = true;
}
const std::string *graphics_fps_take_action_impl(const std::string &category) {
    if (category != "DEFAULTMODE")
        return nullptr;
    const auto value = graphics_input_router.take(SDL_GetTicks());
    if (value.action == ncmm_graphics_input::command::none)
        return nullptr;
    graphics_fps_action.clear();
    graphics_fps_dispatch_target.reset();
    graphics_fps_dispatch_action.clear();
    using command = ncmm_graphics_input::command;
    if (value.action == command::move) {
        const auto delta =
            ncmm_graphics_camera::movement(graphics_camera_yaw, value.forward, value.right);
        graphics_fps_action = delta.second < 0   ? (delta.first < 0   ? "LEFTUP"
                                                    : delta.first > 0 ? "RIGHTUP"
                                                                      : "UP")
                              : delta.second > 0 ? (delta.first < 0   ? "LEFTDOWN"
                                                    : delta.first > 0 ? "RIGHTDOWN"
                                                                      : "DOWN")
                              : delta.first < 0  ? "LEFT"
                                                 : "RIGHT";
        graphics_fps_move_origin = get_map().get_abs(get_avatar().pos_bub());
    } else if (value.action == command::attack)
        graphics_fps_action = graphics_click_action_impl();
    else if (value.action == command::aim) {
        const auto held = get_avatar().get_wielded_item();
        if (held && (held->is_gun() || held->reach_range(get_avatar()).first > 1))
            graphics_fps_action = "fire";
    } else if (value.action == command::interact) {
        const auto target = graphics_fps_target(true);
        if (target && !graphics_fps_interactions(*target).empty()) {
            graphics_fps_action = "interact";
            graphics_fps_dispatch_target = get_map().get_abs(*target);
        }
    } else if (value.action == command::reload)
        graphics_fps_action = "reload_weapon";
    else if (value.action == command::pickup) {
        graphics_fps_action = "pickup";
        const auto target = graphics_fps_target(true);
        if (target && can_interact_at(ACTION_PICKUP, get_map(), *target))
            graphics_fps_dispatch_target = get_map().get_abs(*target);
    } else if (value.action == command::inventory)
        graphics_fps_action = "inventory";
    else if (value.action == command::run)
        graphics_fps_action = "toggle_run";
    else if (value.action == command::crouch)
        graphics_fps_action = "toggle_crouch";
    else if (value.action == command::wait)
        graphics_fps_action = "pause";
    else if (value.action == command::use)
        graphics_fps_action = "apply_wielded";
    else if (value.action == command::settings)
        graphics_fps_action = "ncmm.view.fps_settings";
    if (graphics_fps_action.empty())
        graphics_fps_action = "TIMEOUT";
    graphics_fps_dispatch_action = graphics_fps_action;
    graphics_fps_outgoing_owned = true;
    return &graphics_fps_action;
}
void graphics_fps_queue_target(const tripoint_bub_ms &target, const std::string &action) {
    graphics_fps_dispatch_target = get_map().get_abs(target);
    graphics_fps_dispatch_action = action;
}
std::optional<tripoint_bub_ms> graphics_take_fps_target_impl(const std::string &action) {
    const auto target = graphics_fps_dispatch_target;
    graphics_fps_dispatch_target.reset();
    if (!target || action != graphics_fps_dispatch_action)
        return std::nullopt;
    const auto current = graphics_fps_target(true);
    if (!current || get_map().get_abs(*current) != *target)
        return std::nullopt;
    if (action == "interact")
        graphics_fps_open_target = target;
    return current;
}
std::optional<tripoint_bub_ms>
graphics_targeted_open_impl(const std::optional<tripoint_bub_ms> &target) {
    const auto expected = graphics_fps_open_target;
    graphics_fps_open_target.reset();
    return expected && target && get_map().get_abs(*target) == *expected ? target : std::nullopt;
}
#else
void graphics_fps_reset() {
    graphics_input_router.reset();
}
void graphics_fps_on_enable() {}
void graphics_fps_sync() {}
void graphics_fps_settings() {}
void graphics_fps_input_begin(const std::string &) {}
void graphics_fps_input_end(const std::string &) {}
void graphics_fps_native_begin() {}
void graphics_fps_rendered(bool) {}
std::string graphics_fps_hint() {
    return {};
}
int graphics_set_focus(int32_t) {
    return 0;
}
bool graphics_route_sdl_impl(const SDL_Event &, const SDL_Event &, bool) {
    return false;
}
bool graphics_fps_pending_impl() {
    return false;
}
const std::string *graphics_fps_take_action_impl(const std::string &) {
    return nullptr;
}
std::optional<tripoint_bub_ms> graphics_fps_target(bool) {
    return std::nullopt;
}
std::optional<tripoint_bub_ms> graphics_take_fps_target_impl(const std::string &) {
    return std::nullopt;
}
std::optional<tripoint_bub_ms> graphics_targeted_open_impl(const std::optional<tripoint_bub_ms> &) {
    return std::nullopt;
}
#endif
