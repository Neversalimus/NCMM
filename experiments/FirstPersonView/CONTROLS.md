# First Person View 0.13.1 controls

F6 enables the view. Mouse look uses raw SDL relative deltas and the existing
camera callback; it never advances a turn. WASD uses physical SDL scancodes,
including when a Russian keycode or text event accompanies the press.

| Default | Native action |
| --- | --- |
| W A S D, chords | Camera-relative eight-direction walking |
| LMB | Visible hostile melee / reach / gun fire; gun coordinate targeting otherwise |
| RMB | Native gun / reach targeting, with its normal confirmations and cancel |
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
and a bounded 100–400 ms held-step interval (default 140 ms). Numeric values
can be increased, decreased or reset. Duplicate assignments are rejected.
Movement waits for the real Native action boundary; the interval is a minimum,
not a promise to advance the simulation at that frequency.

Native menus, inventory, text fields, search and IME own their original events.
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
actual stair tile. The reticle shows one localized available-action hint.
Native legality and confirmation dialogs remain authoritative.

Vehicle driving uses the existing Native bindings, including steering and
throttle arrows. Walking intents and LMB attacks are disabled in this context.
A viewport click permits independent mouse look; Tab/Esc releases the pointer.
Leaving the driver context fences held keys. FPS WASD throttle/steering is
deliberately deferred; no second vehicle simulation is implemented.

RMB opens the qualified Native aim UI. There is no new weapon-specific ADS
alignment or optical simulation. Existing PSX poses, Mana Hands, reload and
item switching remain driven by Native held-item and attack state.

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

Управление: мышь — осмотр; WASD — ходьба; ЛКМ — атака; ПКМ — штатное
прицеливание; E — действие с объектом; R/G/I/U — перезарядить, подобрать,
инвентарь, применить предмет; Shift/Ctrl — бег/приседание; Space — ожидание.
Tab возвращает оригинальные команды CDDA, F5 открывает настройки, F6 — карту.
В машине сохраняются штатные команды, а обзор мышью независим от руля.

Automated router and renderer checks are separate from real Native gameplay.
The opt-in Native FPS audit writes `first-person-fps-input.json`; the retained
firing audit checks 192 normal trials plus the historical negative control and
every presented frame. Window-event injection is not a physical two-monitor or
manual gameplay session. A release requires both pinned Native Hosts, actual
installer checks and comparable ordinary before/after timing. Unit tests do not
certify latency, real combat or a frame-time target. See the PR qualification
record for the actual completed runs; do not infer success from this document.

SDL references: [relative mode](https://wiki.libsdl.org/SDL3/SDL_SetWindowRelativeMouseMode),
[default unscaled relative input](https://wiki.libsdl.org/SDL3/SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE),
[physical scancode names](https://wiki.libsdl.org/SDL3/SDL_GetScancodeFromName).
