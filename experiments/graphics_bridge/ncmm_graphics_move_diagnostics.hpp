#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>

// Private, opt-in synchronous wall-clock diagnostics. Only the movement
// benchmark installs a stack-owned capture. An ordinary Native call reads the
// null thread-local pointer, then skips every clock, counter and storage write.
// Recorded intervals include clock-probe overhead; nested calls are inclusive
// and must never be added to their parents as independent CPU execution time.
namespace ncmm_graphics_move_diagnostics {
using clock = std::chrono::steady_clock;
using now_reader = clock::time_point ( * )() noexcept;
constexpr std::size_t event_capacity = 8;
constexpr std::size_t boundary_capacity = 9;
enum class boundary : std::uint8_t {
    entry, route, shift, rebase_load_npcs, invalidate, build, spawn, overmap,
    zero_position, zero_overmap, exit
};
inline const char *label( boundary value ) noexcept
{
    switch( value ) {
        case boundary::entry: return "entry";
        case boundary::route: return "route";
        case boundary::shift: return "shift";
        case boundary::rebase_load_npcs: return "rebase_load_npcs";
        case boundary::invalidate: return "invalidate";
        case boundary::build: return "build";
        case boundary::spawn: return "spawn";
        case boundary::overmap: return "overmap";
        case boundary::zero_position: return "zero_position";
        case boundary::zero_overmap: return "zero_overmap";
        case boundary::exit: return "exit";
    }
    return "invalid";
}
struct event {
    std::uint32_t ordinal = 0, depth = 0;
    std::int64_t parent_ordinal = -1;
    std::array<int, 2> input_xy{}, output_xy{}, shift_xy{};
    bool z_level_changed = false, shift_known = false, complete = false, exceptional = false;
    std::size_t count = 0;
    std::array<boundary, boundary_capacity> tags{};
    std::array<clock::time_point, boundary_capacity> points{};
};
struct action_record {
    bool claimed = false, reused_storage = false, nested_capture = false, counter_overflow = false;
    std::uint32_t invocations = 0, recorded = 0, dropped = 0, max_depth = 0;
    std::uint32_t exceptional_invocations = 0, protocol_errors = 0;
    std::array<event, event_capacity> events{};
};
inline void increment( std::uint32_t &value, action_record &record ) noexcept
{
    if( value == std::numeric_limits<std::uint32_t>::max() ) record.counter_overflow = true;
    else ++value;
}
inline clock::time_point steady_now() noexcept { return clock::now(); }
class update_scope;
class action_capture;
inline thread_local action_capture *active_capture = nullptr;
class action_capture {
    friend class update_scope;
    action_record &record_;
    action_capture *previous_;
    update_scope *top_ = nullptr;
    now_reader now_;
public:
    explicit action_capture( action_record &record, now_reader now = steady_now ) noexcept :
        record_( record ), previous_( active_capture ), now_( now )
    {
        if( record_.claimed ) record_.reused_storage = true;
        record_.claimed = true;
        if( previous_ ) {
            previous_->record_.nested_capture = true;
            record_.nested_capture = true;
        }
        active_capture = this;
    }
    action_capture( const action_capture & ) = delete;
    action_capture &operator=( const action_capture & ) = delete;
    ~action_capture() noexcept
    {
        if( top_ || active_capture != this ) increment( record_.protocol_errors, record_ );
        active_capture = previous_;
    }
};
class update_scope {
    action_capture *capture_ = active_capture;
    update_scope *previous_ = nullptr;
    event *event_ = nullptr;
    int *x_ = nullptr, *y_ = nullptr;
    int exceptions_on_entry_ = 0;
    std::uint32_t ordinal_ = 0, depth_ = 0;
    void append( boundary tag ) noexcept
    {
        if( !event_ ) return;
        if( event_->count >= boundary_capacity ) {
            increment( capture_->record_.protocol_errors, capture_->record_ );
            return;
        }
        const auto at = event_->count++;
        event_->tags[at] = tag;
        event_->points[at] = capture_->now_();
    }
public:
    update_scope( int &x, int &y, bool z_level_changed ) noexcept
    {
        if( !capture_ ) return;
        auto &record = capture_->record_;
        previous_ = capture_->top_;
        ordinal_ = record.invocations;
        increment( record.invocations, record );
        depth_ = previous_ ? previous_->depth_ : 0;
        increment( depth_, record );
        if( record.max_depth < depth_ ) record.max_depth = depth_;
        capture_->top_ = this;
        exceptions_on_entry_ = std::uncaught_exceptions();
        if( record.recorded >= event_capacity ) {
            increment( record.dropped, record );
            return;
        }
        event_ = &record.events[record.recorded++];
        event_->ordinal = ordinal_; event_->depth = depth_;
        event_->parent_ordinal = previous_ ? std::int64_t( previous_->ordinal_ ) : -1;
        event_->input_xy = {x, y}; event_->z_level_changed = z_level_changed;
        x_ = &x; y_ = &y;
        append( boundary::entry );
    }
    update_scope( const update_scope & ) = delete;
    update_scope &operator=( const update_scope & ) = delete;
    explicit operator bool() const noexcept { return event_ != nullptr; }
    void route( int x, int y ) noexcept
    {
        if( !event_ ) return;
        if( event_->count != 1 ) increment( capture_->record_.protocol_errors, capture_->record_ );
        event_->shift_xy = {x, y}; event_->shift_known = true;
        append( boundary::route );
    }
    void mark( boundary tag ) noexcept
    {
        if( !event_ ) return;
        const bool zero = event_->shift_known && event_->shift_xy == std::array<int, 2>{0, 0};
        constexpr std::array<boundary, 8> shifted = {boundary::entry, boundary::route, boundary::shift,
            boundary::rebase_load_npcs, boundary::invalidate, boundary::build, boundary::spawn, boundary::overmap};
        constexpr std::array<boundary, 4> stationary = {boundary::entry, boundary::route,
            boundary::zero_position, boundary::zero_overmap};
        const auto at = event_->count;
        const bool correct = event_->shift_known && ( zero ? at < stationary.size() && stationary[at] == tag :
                             at < shifted.size() && shifted[at] == tag );
        if( !correct ) increment( capture_->record_.protocol_errors, capture_->record_ );
        append( tag );
    }
    ~update_scope() noexcept
    {
        if( !capture_ ) return;
        auto &record = capture_->record_;
        const bool exceptional = std::uncaught_exceptions() > exceptions_on_entry_;
        if( exceptional ) increment( record.exceptional_invocations, record );
        if( event_ ) {
            event_->exceptional = exceptional;
            const bool zero = event_->shift_xy == std::array<int, 2>{0, 0};
            const bool all = event_->shift_known && event_->count == ( zero ? 4u : 8u );
            event_->complete = !exceptional && all;
            if( !exceptional && !all ) increment( record.protocol_errors, record );
            event_->output_xy = {*x_, *y_};
            append( boundary::exit );
        }
        if( capture_->top_ != this ) increment( record.protocol_errors, record );
        capture_->top_ = previous_;
    }
};
} // namespace ncmm_graphics_move_diagnostics
