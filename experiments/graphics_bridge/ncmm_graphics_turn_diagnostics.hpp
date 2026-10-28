#pragma once
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>

// Only the controlled gameplay audit installs a capture around real do_turn.
// Ordinary drawing reads a null TLS pointer: no clock, allocation or recording.
// Draw wall time is the union of outermost callbacks, not a sum of nested work.
namespace ncmm_graphics_turn_diagnostics {
using clock = std::chrono::steady_clock;
using now_reader = clock::time_point ( * )() noexcept;
enum class backend : std::uint8_t { cpu, gpu, legacy };
enum fault : std::uint32_t {
    reused = 1, nested = 2, protocol = 4, exceptional = 8, backwards = 16,
    overflow = 32, invalid_reader = 64
};
struct turn_record {
    bool claimed = false, returned = false, closed = false;
    std::uint32_t errors = 0, draw_calls = 0, outer_draws = 0, max_depth = 0;
    std::uint32_t cpu_draws = 0, gpu_draws = 0, legacy_draws = 0, skipped_draws = 0;
    std::int64_t wall_ns = 0, draw_wall_ns = 0;
};
inline bool valid( const turn_record &r ) noexcept
{
    return r.claimed && r.returned && r.closed && !r.errors &&
           std::uint64_t( r.cpu_draws ) + r.gpu_draws + r.legacy_draws + r.skipped_draws == r.draw_calls &&
           r.outer_draws <= r.draw_calls && ( r.draw_calls ? r.outer_draws && r.max_depth &&
               r.max_depth <= r.draw_calls - r.outer_draws + 1 : !r.outer_draws && !r.max_depth && !r.draw_wall_ns ) &&
           r.wall_ns >= 0 && r.draw_wall_ns >= 0 && r.draw_wall_ns <= r.wall_ns;
}
inline clock::time_point steady_now() noexcept { return clock::now(); }
class turn_capture;
class draw_scope;
inline thread_local turn_capture *active_capture = nullptr;
class turn_capture {
    friend class draw_scope;
    turn_record &record_;
    turn_capture *previous_;
    draw_scope *top_ = nullptr;
    now_reader now_;
    clock::time_point begin_, last_;
    std::uint32_t depth_ = 0;
    int exceptions_ = std::uncaught_exceptions();
    clock::time_point stamp() noexcept
    {
        const auto at = now_();
        if( at < last_ ) { record_.errors |= backwards; return last_; }
        last_ = at;
        return at;
    }
    void increment( std::uint32_t &value ) noexcept
    {
        if( value == std::numeric_limits<std::uint32_t>::max() ) record_.errors |= overflow;
        else ++value;
    }
public:
    explicit turn_capture( turn_record &record, now_reader reader = steady_now ) noexcept :
        record_( record ), previous_( active_capture ), now_( reader ? reader : steady_now )
    {
        if( !reader ) record_.errors |= invalid_reader;
        if( record_.claimed ) record_.errors |= reused;
        record_.claimed = true;
        if( previous_ ) { record_.errors |= nested; previous_->record_.errors |= nested; }
        begin_ = last_ = now_();
        active_capture = this;
    }
    turn_capture( const turn_capture & ) = delete;
    turn_capture &operator=( const turn_capture & ) = delete;
    void complete() noexcept
    {
        if( record_.returned || top_ || active_capture != this ) record_.errors |= protocol;
        record_.returned = true;
    }
    ~turn_capture() noexcept
    {
        if( top_ || depth_ || active_capture != this || !record_.returned ) record_.errors |= protocol;
        if( std::uncaught_exceptions() != exceptions_ ) record_.errors |= exceptional;
        record_.wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>( stamp() - begin_ ).count();
        record_.closed = true;
        active_capture = previous_;
    }
};
class draw_scope {
    turn_capture *capture_ = active_capture;
    draw_scope *previous_ = nullptr;
    clock::time_point begin_;
    bool completed_ = false;
    int exceptions_ = 0;
public:
    draw_scope() noexcept
    {
        if( !capture_ ) return;
        auto &r = capture_->record_;
        if( r.returned || r.closed ) r.errors |= protocol;
        previous_ = capture_->top_; capture_->top_ = this;
        capture_->increment( r.draw_calls ); capture_->increment( capture_->depth_ );
        if( r.max_depth < capture_->depth_ ) r.max_depth = capture_->depth_;
        if( !previous_ ) { capture_->increment( r.outer_draws ); begin_ = capture_->stamp(); }
        exceptions_ = std::uncaught_exceptions();
    }
    draw_scope( const draw_scope & ) = delete;
    draw_scope &operator=( const draw_scope & ) = delete;
    void complete( bool rendered, backend mode ) noexcept
    {
        if( !capture_ ) return;
        auto &r = capture_->record_;
        if( completed_ || capture_->top_ != this || active_capture != capture_ ) { r.errors |= protocol; return; }
        completed_ = true;
        if( !rendered ) capture_->increment( r.skipped_draws );
        else if( mode == backend::cpu ) capture_->increment( r.cpu_draws );
        else if( mode == backend::gpu ) capture_->increment( r.gpu_draws );
        else if( mode == backend::legacy ) capture_->increment( r.legacy_draws );
        else r.errors |= protocol;
    }
    ~draw_scope() noexcept
    {
        if( !capture_ ) return;
        auto &r = capture_->record_;
        if( !completed_ || capture_->top_ != this || active_capture != capture_ || !capture_->depth_ ) r.errors |= protocol;
        if( std::uncaught_exceptions() != exceptions_ ) r.errors |= exceptional;
        if( !previous_ ) {
            const auto span = std::chrono::duration_cast<std::chrono::nanoseconds>( capture_->stamp() - begin_ ).count();
            if( span < 0 || r.draw_wall_ns > std::numeric_limits<std::int64_t>::max() - span ) r.errors |= overflow;
            else r.draw_wall_ns += span;
        }
        if( capture_->depth_ ) --capture_->depth_;
        capture_->top_ = previous_;
    }
};
} // namespace ncmm_graphics_turn_diagnostics
