#include "ncmm_graphics_input.hpp"
#include <cstdio>
#include <cstdlib>
using namespace ncmm_graphics_input;
namespace {
unsigned checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FPS input: %s\n", message);
        std::exit(1);
    }
}
environment gameplay() {
    environment e;
    e.enabled = e.available = e.focused = e.fps = e.gameplay = true;
    return e;
}
void start(router &r) {
    r.native_wait(true);
    r.update(gameplay());
    require(r.capture(), "capture real gameplay only");
}
} // namespace
int main() {
    router r;
    require(!r.capture(), "no capture without actual gameplay");
    start(r);
    r.key(button::forward, true, false, 0);
    r.key(button::right, true, false, 0);
    auto step = r.take(0);
    require(step.action == command::move && step.forward == 1 && step.right == 1,
            "chord is one diagonal");
    require(r.take(0).action == command::none, "no same-frame repeat");
    r.native_wait(false);
    require(r.take(10000).action == command::none, "busy Native cannot consume");
    r.completed_move(true);
    r.native_wait(true);
    require(r.take(140).action == command::move, "one repeat at next ready boundary");
    r.completed_move(false);
    require(!r.ready(10000), "wall fences held movement");
    r.key(button::forward, false, false, 10001);
    r.key(button::right, false, false, 10001);
    r.key(button::forward, true, false, 10002);
    require(r.take(10002).action == command::move, "fresh press after wall");
    auto modal = gameplay();
    modal.gameplay = false;
    r.update(modal);
    require(r.owner() == context::modal && !r.captured() && !r.ready(100000),
            "modal owns all input");
    require(!r.key(button::inventory, true, false, 100003), "letters belong to modal UI");
    r.update(gameplay());
    require(r.captured(), "safe menu return restores previous capture");
    require(!r.ready(100004), "menu never restores stale W");
    r.key(button::forward, false, false, 100005);
    r.key(button::forward, true, false, 100006);
    require(r.take(100006).action == command::move, "fresh post-menu press works");
    auto lost = gameplay();
    lost.focused = false;
    r.update(lost);
    r.update(gameplay());
    require(!r.captured() && !r.ready(200000), "Alt Tab needs fresh viewport capture");
    require(r.capture(), "fresh viewport click captures");
    r.key(button::commands, true, false, 200001);
    r.update(gameplay());
    require(r.owner() == context::commands && !r.captured(), "explicit Native command mode");
    require(!r.key(button::reload, true, false, 200002), "Native commands keep their letters");
    r.key(button::commands, false, false, 200003);
    r.key(button::commands, true, false, 200004);
    r.update(gameplay());
    require(r.owner() == context::gameplay && !r.captured(),
            "return from commands needs safe click");
    require(r.capture(), "recapture after commands");
    r.key(button::attack, true, false, 200005);
    for (unsigned i = 0; i < 100; ++i)
        r.key(button::attack, true, true, 200005 + i);
    require(r.take(200200).action == command::attack, "one attack edge");
    require(!r.ready(200201), "OS repeats do not create shots");
    r.key(button::attack, false, false, 200202);
    r.key(button::attack, true, false, 200203);
    require(r.take(200203).action == command::attack, "next physical click is another action");
    auto aim = gameplay();
    aim.aiming = true;
    r.update(aim);
    require(r.owner() == context::aiming && !r.captured(), "native targeting owns its input");
    require(!r.key(button::wait, true, false, 200204), "target cancel and text pass through");
    r.update(gameplay());
    require(r.captured() && !r.ready(200205), "target return clears attack and wait");
    auto vehicle = gameplay();
    vehicle.vehicle = true;
    r.update(vehicle);
    require(r.owner() == context::vehicle && !r.captured() && !r.ready(200206),
            "vehicle uses safe native fallback");
    require(r.capture(), "vehicle can capture independent free look");
    require(!r.key(button::forward, true, false, 200206) && !r.ready(200207),
            "vehicle movement stays Native and never creates a walking intent");
    r.key(button::commands, false, false, 200207);
    r.key(button::commands, true, false, 200207);
    r.update(vehicle);
    require(r.owner() == context::commands && !r.captured(),
            "vehicle Tab releases to Native commands");
    r.key(button::commands, false, false, 200207);
    r.key(button::commands, true, false, 200207);
    r.update(vehicle);
    require(r.owner() == context::vehicle && !r.captured(),
            "vehicle return needs a fresh safe click");
    r.update(gameplay());
    require(!r.ready(200207), "vehicle exit cannot move avatar");
    auto off = gameplay();
    off.enabled = false;
    r.update(off);
    require(!r.captured(), "F6 always releases");
    r.reset();
    start(r);
    r.key(button::forward, true, false, 1);
    r.key(button::forward, false, false, 2);
    require(r.take(2).action == command::move, "short tap survives a single SDL poll batch");
    require(!r.ready(1000), "short tap never repeats");
    r.key(button::forward, true, false, 1001);
    r.key(button::back, true, false, 1001);
    require(!r.ready(1001), "opposite chord is stationary");
    r.reset();
    start(r);
    r.key(button::forward, true, false, 0);
    r.key(button::inventory, true, false, 0);
    require(r.take(0).action == command::inventory && !r.ready(100000),
            "a pending menu action fences held movement before the modal opens");
    require(r.attack_intents == 2, "only the two consumed physical attack edges are counted");
    r.reset();
    start(r);
    r.key(button::forward, true, false, 0);
    auto lost_again = gameplay();
    lost_again.focused = false;
    r.update(lost_again);
    r.reconcile_focus(0); // W released outside this window: no KEY_UP arrived.
    r.update(gameplay());
    require(!r.captured() && r.capture(), "focus regain still requires a viewport click");
    r.key(button::forward, true, false, 1);
    require(r.take(1).action == command::move,
            "first fresh W works after a missing outside KEY_UP");
    r.reconcile_focus(1u << unsigned(button::forward));
    require(r.capture() && !r.ready(10000), "a physically held key stays fenced on focus regain");
    auto modal_return = gameplay();
    modal_return.gameplay = false;
    r.update(modal_return);
    r.native_wait(false);
    r.update(gameplay());
    require(!r.captured(), "a modal cannot resume capture between Native waits");
    r.native_wait(true);
    r.update(gameplay());
    require(r.captured() && !r.ready(10001),
            "capture resumes at the next real Native wait with held input fenced");
    // Repeated transition stress checks absence of queued turns/shots, not a
    // mirrored implementation: every forbidden owner must consume zero actions.
    for (unsigned i = 0; i < 1000; ++i) {
        router probe;
        start(probe);
        probe.key(button::forward, true, false, 0);
        probe.key(button::attack, true, false, 0);
        auto env = gameplay();
        switch (i % 5) {
        case 0:
            env.gameplay = false;
            break;
        case 1:
            env.focused = false;
            break;
        case 2:
            env.enabled = false;
            break;
        case 3:
            env.aiming = true;
            break;
        default:
            env.available = false;
            break;
        }
        probe.update(env);
        require(!probe.captured() && probe.take(100000).action == command::none,
                "no action survives ownership loss");
        probe.update(gameplay());
        require(!probe.ready(100001), "held buttons stay fenced after return");
    }
    std::printf("FPS router: %u checks PASS; no game or SDL calls\n", checks);
    return 0;
}
