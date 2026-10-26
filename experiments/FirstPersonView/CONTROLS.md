# First Person View 0.13.1 controls

F6 enables the view. Mouse look uses raw SDL relative deltas and the existing
camera callback; it never advances a turn. WASD uses physical SDL scancodes,
including when a Russian keycode or text event accompanies the press.
Walking uses a continuous position inside the authoritative Native tile. Short
movements and stopping between tile centres do not advance simulation time.
Crossing a tile boundary requests one ordinary Native movement action; only a
successful action rebases the continuous position, without a camera jump.

| Default | Native action |
| --- | --- |
| W A S D, chords | Continuous camera-relative walking; normalized diagonal speed |
| LMB | Visible hostile melee / reach / gun fire; gun coordinate targeting otherwise |
| RMB | Hold firearm ADS; release returns to the hip pose |
| E | `interact` at the current visible adjacent XYZ; Native action menu if needed |
| R | `reload_weapon` |
| G | `pickup`, with the Native selection, quantity and container UI |
| I | `inventory` |
| U | `apply_wielded` |
| Shift | `toggle_run`, including Native stamina/time effects |
| Ctrl or C | `toggle_crouch` |
| Space | `pause` |
| Tab | FPS / Native command mode |
| Esc | Native cancel in a dialog; release cursor in FPS gameplay |
| F5 | Control settings |
| F6 | View / Native map |
| F7 F8 / F9 F10 | Turn / pitch |
| F11 / F12 | Existing quality / renderer selection |

F5 uses the existing NCMM world-setting metadata and persistence. It provides
FPS enable/disable, sensitivity, Y inversion, ten primary physical-key bindings
and continuous walking speed (1–6 tiles per second, default 3). Running,
crouching, prone movement and difficult terrain adjust that speed. The older
100–400 ms held-step interval remains for fallback tile controls. Numeric values
can be increased, decreased or reset. Duplicate assignments are rejected.
Movement waits for the real Native action boundary. A slow frame or a busy
simulation cannot accumulate movement debt or queue several future turns.
A body-radius collider stops before solid terrain, closed doors, unsupported
edges, vehicles and occupied tiles, and slides along walls. Use E to open doors
and LMB to attack instead of automatically attacking by walking into a creature.

Native menus, inventory, text fields, search and IME own their original events.
Legacy "press any key" windows use the same modal input lease, including
terminal login prompts that bypass `input_context::handle_input`.
The Native error window also retains its original C/I/Space controls.
Tab releases the cursor and restores original bindings while retaining the 3D
backdrop where supported. Returning to FPS needs a viewport click; that click
only captures and cannot attack. Esc, focus loss, resize/DPI changes, F6,
renderer failure and unload clear held intents and release relative mode.
Modal return can resume a previous gameplay capture, with all held keys fenced.

E reuses the existing centre ray, including transparent glass as an interaction
occluder. It retains owned absolute coordinates and entity identity, never an
entity pointer. Immediately before Native dispatch it rechecks the avatar
origin, camera, world revision, entity, visibility and adjacent range. Native
`handle_interact` selects the available door, furniture, item, device, NPC,
vehicle or vertical action. Stair movement still originates at the avatar's
actual stair tile. At the current pitch limit, crouch and look down to target
the stair underfoot, or use its original command in Native mode.
The reticle shows one localized available-action hint.
Native legality and confirmation dialogs remain authoritative.

Vehicle driving uses the existing Native bindings, including steering and
throttle arrows. Walking intents and LMB attacks are disabled in this context.
A viewport click permits independent mouse look; Tab/Esc releases the pointer.
Leaving the driver context fences held keys. FPS WASD throttle/steering is
deliberately deferred; no second vehicle simulation is implemented.

RMB retains the aligned firearm ADS pose without opening Native target menus.
Native aiming remains available in Tab command mode. Existing PSX poses, Mana
Hands, reload and item switching remain driven by Native held-item and attack
state. Small camera footfalls and walking hand sway follow actual distance;
ADS reduces their amplitude. Releasing movement brakes at the current position.

The small Host router contains button state, one pending action and one movement
edge. It has no action queue, simulation, avatar coordinates or Native pointers.
It observes the existing SDL loop before ImGui/Native translation. Physical key
repeats cannot create extra shots. A failed movement fences held walking until
a fresh press. Mouse callbacks deferred during render avoid re-entering the
module while borrowed scene data is active.

The optional `ncmm.host_api.v1.first_person_focus` query adds only an owned
interaction cell notification. Existing Core, Graphics and GPU Mesh ABI layouts
and required capabilities are unchanged. Creature combat picking and rendered
pixels keep their established contract.

## Проверка и ограничения

Управление: мышь — осмотр; WASD — плавная ходьба; ЛКМ — атака; ПКМ — удерживать
прицеливание; E — действие с объектом; R/G/I/U — перезарядить, подобрать,
инвентарь, применить предмет; Shift/Ctrl — бег/приседание; Space — ожидание.
Tab возвращает оригинальные команды CDDA, F5 открывает настройки, F6 — карту.
В машине сохраняются штатные команды, а обзор мышью независим от руля.
Внутри клетки можно двигаться и останавливаться свободно. Игровой шаг
выполняется при пересечении границы клетки; его стоимость и допустимость
проверяет CDDA. Стены не расходуют ходы от одного удерживания клавиши.

Automated router and renderer checks are separate from real Native gameplay.
The opt-in Native FPS audit prepares 172 controlled cases and writes
`first-person-fps-input.json`; the retained
firing audit checks 196 normal trials, including four forced Native misfires,
plus the historical negative control and
every presented frame. Window-event injection is not a physical two-monitor or
manual gameplay session. A release requires both pinned Native Hosts, actual
installer checks and comparable ordinary before/after timing. Unit tests do not
certify latency, real combat or a frame-time target. See the PR qualification
record for the actual completed runs; do not infer success from this document.

SDL references: [relative mode](https://wiki.libsdl.org/SDL3/SDL_SetWindowRelativeMouseMode),
[default unscaled relative input](https://wiki.libsdl.org/SDL3/SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE),
[physical scancode names](https://wiki.libsdl.org/SDL3/SDL_GetScancodeFromName).
