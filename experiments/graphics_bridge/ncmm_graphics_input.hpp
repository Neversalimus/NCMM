#pragma once
#include <algorithm>
#include <cstdint>

namespace ncmm_graphics_input {
// Host-owned input intent only. The Native main thread remains the only action
// consumer; no queue, game objects, coordinates or simulation live here.
enum class context {
    gameplay,
    aiming,
    interacting,
    vehicle,
    commands,
    modal,
    map,
    unfocused,
    unavailable
};
enum class button : unsigned {
    forward,
    back,
    left,
    right,
    attack,
    aim,
    interact,
    reload,
    pickup,
    inventory,
    run,
    crouch,
    wait,
    use,
    commands,
    settings,
    count,
    none = count
};
enum class command {
    none,
    move,
    attack,
    aim,
    interact,
    reload,
    pickup,
    inventory,
    run,
    crouch,
    wait,
    use,
    settings
};
struct intent {
    command action = command::none;
    int forward = 0, right = 0;
};
struct environment {
    bool enabled = false, available = false, focused = true, fps = true, gameplay = false,
         aiming = false, interacting = false, vehicle = false;
};
class router {
    static constexpr uint32_t bit(button b) {
        return unsigned(b) < unsigned(button::count) ? uint32_t(1) << unsigned(b) : 0;
    }
    static constexpr uint32_t movement_mask = 15;
    uint32_t observed_ = 0, blocked_ = 0;
    intent edge_{}, pending_{};
    intent boundary_{};bool continuous_walk_=false;
    context context_ = context::map;
    bool command_mode_ = false, captured_ = false, resume_ = false, waiting_ = false,
         move_issued_ = false;
    uint64_t next_move_ = 0;
    uint32_t repeat_ms_ = 140;
    bool automatic_fire_=false;uint32_t fire_repeat_ms_=240;uint64_t next_fire_=0;
    static intent movement(uint32_t mask) {
        return {command::move,
                int(bool(mask & bit(button::forward))) - int(bool(mask & bit(button::back))),
                int(bool(mask & bit(button::right))) - int(bool(mask & bit(button::left)))};
    }
    void clear_intent() {
        blocked_ |= observed_;
        edge_ = {};
        boundary_ = {};
        pending_ = {};
        move_issued_ = false;
        next_move_ = 0;
        next_fire_=0;
    }

  public:
    uint64_t events = 0, issued = 0, movements = 0, transitions = 0, attack_intents = 0,
             aim_intents = 0;
    context owner() const { return context_; }
    bool captured() const { return captured_; }
    bool command_mode() const { return command_mode_; }
    bool walking_suspended()const{return !captured_||context_!=context::gameplay;}
    bool walking_owned()const{return waiting_&&!walking_suspended();}
    bool walking_held()const{return !walking_suspended()&&(observed_&~blocked_&movement_mask);}
    void cancel_boundary(){boundary_={};}
    void continuous_walk(bool value){continuous_walk_=value;if(!value)boundary_={};}
    bool continuous_walk()const{return continuous_walk_;}
    intent walking(bool presentation=false){
        if(!continuous_walk_||walking_suspended()||(!waiting_&&!presentation))return {};
        auto value=movement(observed_&~blocked_&movement_mask);
        // The explicit outer Native input lease also owns its pre-poll
        // redraw. Presentation may sample held state there, never an edge or
        // a costed command; take()/boundary() still require the real wait.
        if(!waiting_)return value;
        // Preserve a very short press as a fractional nudge, never a full tile.
        if(!value.forward&&!value.right)value=edge_;
        edge_={};return value;
    }
    void boundary(int x,int y){
        if(continuous_walk_&&walking_owned()&&pending_.action==command::none&&boundary_.action==command::none&&(x||y))
            boundary_={command::move,std::clamp(x,-1,1),std::clamp(y,-1,1)};
    }
    bool aiming()const{return captured_&&context_==context::gameplay&&(observed_&~blocked_&bit(button::aim));}
    void automatic_fire(bool enabled,uint32_t interval=240){automatic_fire_=enabled;fire_repeat_ms_=std::clamp(interval,120u,1200u);}
    void repeat_ms(uint32_t value) { repeat_ms_ = std::clamp(value, 100u, 400u); }
    void native_wait(bool value) { waiting_ = value; }
    void interrupt() { clear_intent(); }
    // The main SDL thread supplies one snapshot on focus regain. A release
    // outside this window may have no KEY_UP event; a still-held key is fenced.
    void reconcile_focus(uint32_t held) {
        release();
        observed_ = blocked_ = held & ((uint32_t(1) << unsigned(button::count)) - 1);
    }
    // Native modal windows may drain releases without routing them to FPS.
    // Keep still-held controls fenced, but never retain a released key forever.
    void reconcile_released(uint32_t held){observed_&=held;blocked_&=held;}
    static int presentation_wait(int native_ms,uint64_t now,uint64_t last,uint32_t interval){
        const uint64_t elapsed=now>=last?now-last:interval;
        const int left=elapsed>=interval?1:int(interval-elapsed);
        return native_ms<0?left:std::min(native_ms,left);
    }
    void release(bool resume = false) {
        captured_ = false;
        resume_ = resume;
        clear_intent();
    }
    void reset() {
        release();
        observed_ = blocked_ = 0;
        waiting_ = false;
        command_mode_ = false;
        context_ = context::map;
        continuous_walk_=false;
    }
    bool capture() {
        if ((context_ != context::gameplay && context_ != context::vehicle) || !waiting_)
            return false;
        clear_intent();
        captured_ = true;
        resume_ = false;
        return true;
    }
    // Modal entry releases and fences all held keys. Only a previously owned
    // gameplay capture may resume; focus loss always requires a fresh click.
    void update(environment e) {
        context next = !e.enabled                  ? context::map
                       : !e.available              ? context::unavailable
                       : !e.focused                ? context::unfocused
                       : e.aiming                  ? context::aiming
                       : e.interacting             ? context::interacting
                       : !e.gameplay               ? context::modal
                       : (!e.fps || command_mode_) ? context::commands
                       : e.vehicle                 ? context::vehicle
                                                   : context::gameplay;
        if (next == context_) {
            if (next == context::gameplay && resume_ && waiting_)
                capture();
            return;
        }
        const bool can_resume =
            next == context::modal || next == context::aiming || next == context::interacting;
        if (captured_)
            release(can_resume);
        else
            clear_intent();
        if (next == context::unfocused || next == context::unavailable || next == context::map ||
            next == context::commands || next == context::vehicle)
            resume_ = false;
        context_ = next;
        ++transitions;
        if (next == context::gameplay && resume_ && waiting_)
            capture();
    }
    bool key(button b, bool down, bool repeat, uint64_t now) {
        const uint32_t mask = bit(b);
        if (!mask)
            return false;
        ++events;
        const bool was_down = (observed_ & mask) != 0;
        if (down)
            observed_ |= mask;
        else {
            observed_ &= ~mask;
            blocked_ &= ~mask;
        }
        const bool ordinary = context_ == context::gameplay || context_ == context::commands ||
                              context_ == context::vehicle;
        if (b == button::commands && ordinary) {
            if (down && !repeat && !was_down) {
                command_mode_ = !command_mode_;
                release();
            }
            return true;
        }
        if (context_ != context::gameplay)
            return false;
        if(b==button::aim&&!down&&was_down&&captured_&&pending_.action==command::none)pending_.action=command::aim;
        // Mapped FPS keys never accidentally become vanilla letter commands
        // while the cursor is released. Tab explicitly grants Native ownership.
        if (!down || repeat || was_down)
            return true;
        // A held walking key may begin during a costed action. This preserves
        // physical state for the next input lifetime without queuing a turn.
        if(continuous_walk_&&(mask&movement_mask)&&captured_&&!waiting_&&!(blocked_&mask))return true;
        if (!waiting_ || !captured_ || (blocked_ & mask)) {
            blocked_ |= mask;
            return true;
        }
        if (mask & movement_mask) {
            edge_ = movement(observed_ & ~blocked_ & movement_mask);
            if (!edge_.forward && !edge_.right)
                edge_ = {};
            next_move_ = now;
            return true;
        }
        command action = command::none;
        switch (b) {
        case button::attack:
            action = command::attack;
            break;
        case button::aim:
            action = command::aim;
            break;
        case button::interact:
            action = command::interact;
            break;
        case button::reload:
            action = command::reload;
            break;
        case button::pickup:
            action = command::pickup;
            break;
        case button::inventory:
            action = command::inventory;
            break;
        case button::run:
            action = command::run;
            break;
        case button::crouch:
            action = command::crouch;
            break;
        case button::wait:
            action = command::wait;
            break;
        case button::use:
            action = command::use;
            break;
        case button::settings:
            action = command::settings;
            break;
        default:
            break;
        }
        // ADS is live presentation state. A simultaneous LMB must not be
        // swallowed by its zero-cost pose wakeup in the same SDL batch.
        if (pending_.action == command::none || (action==command::attack&&pending_.action==command::aim))
            pending_.action = action;
        return true;
    }
    bool ready(uint64_t now) const {
        if (!waiting_ || !captured_ || context_ != context::gameplay)
            return false;
        if (pending_.action != command::none)
            return true;
        if(automatic_fire_&&(observed_&~blocked_&bit(button::attack))&&now>=next_fire_)return true;
        if(continuous_walk_)return boundary_.action!=command::none;
        const auto move = movement(observed_ & ~blocked_ & movement_mask);
        return now >= next_move_ &&
               ((move.forward || move.right) || (edge_.forward || edge_.right));
    }
    intent take(uint64_t now) {
        if (!ready(now))
            return {};
        if (pending_.action != command::none) {
            const auto value = pending_;
            pending_ = {};
            edge_ = {};
            if (value.action != command::run && value.action != command::crouch&&value.action!=command::attack&&value.action!=command::aim)
                blocked_ |= observed_ & movement_mask;
            if (value.action == command::attack)
                {++attack_intents;next_fire_=now+fire_repeat_ms_;}
            if (value.action == command::aim)
                ++aim_intents;
            ++issued;
            return value;
        }
        if(automatic_fire_&&(observed_&~blocked_&bit(button::attack))&&now>=next_fire_&&
           (!continuous_walk_||boundary_.action==command::none)){
            next_fire_=now+fire_repeat_ms_;++attack_intents;++issued;return {command::attack};}
        if(continuous_walk_){const auto value=boundary_;boundary_={};move_issued_=true;++issued;++movements;return value;}
        auto value = movement(observed_ & ~blocked_ & movement_mask);
        if (!value.forward && !value.right)
            value = edge_;
        edge_ = {};
        next_move_ = now + repeat_ms_;
        move_issued_ = true;
        ++issued;
        ++movements;
        return value;
    }
    // Called at the next real Native action boundary. A wall, danger refusal,
    // interrupted step or melee encounter requires a new physical press.
    void completed_move(bool position_changed) {
        if (move_issued_ && !position_changed) {
            blocked_ |= observed_ & movement_mask;
            edge_ = {};
        }
        move_issued_ = false;
    }
};
} // namespace ncmm_graphics_input
