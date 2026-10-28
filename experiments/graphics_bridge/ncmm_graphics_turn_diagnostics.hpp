#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
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
enum class callback_boundary : std::uint8_t {
    entry, native_cache_begin, native_cache_end, visibility_begin, visibility_end,
    module_begin, module_end, returned
};
constexpr std::size_t callback_capacity = 32, callback_boundary_capacity = 8;
inline const char *label( callback_boundary value ) noexcept
{
    switch( value ) {
        case callback_boundary::entry: return "entry";
        case callback_boundary::native_cache_begin: return "native_cache_begin";
        case callback_boundary::native_cache_end: return "native_cache_end";
        case callback_boundary::visibility_begin: return "visibility_begin";
        case callback_boundary::visibility_end: return "visibility_end";
        case callback_boundary::module_begin: return "module_begin";
        case callback_boundary::module_end: return "module_end";
        case callback_boundary::returned: return "returned";
    }
    return "invalid";
}
struct callback_record {
    std::uint32_t ordinal = 0, image_width = 0, image_height = 0;
    int viewport_width = 0, viewport_height = 0;
    bool returned = false, rendered = false;
    backend mode = backend::legacy;
    std::uint32_t source_cells = 0, source_slices = 0, cpu_reports = 0;
    struct cpu_work {
        std::uint32_t width = 0, height = 0, raster_calls = 0;
        std::uint64_t triangles = 0, cache_bytes = 0;
        double frame_ms = 0, geometry_ms = 0, raster_ms = 0, effects_ms = 0;
    } cpu;
    std::size_t count = 0;
    std::array<callback_boundary, callback_boundary_capacity> tags{};
    std::array<std::int64_t, callback_boundary_capacity> offsets_ns{};
};
enum fault : std::uint32_t {
    reused = 1, nested = 2, protocol = 4, exceptional = 8, backwards = 16,
    overflow = 32, invalid_reader = 64
};
struct turn_record {
    bool claimed = false, returned = false, closed = false;
    std::uint32_t errors = 0, draw_calls = 0, outer_draws = 0, max_depth = 0;
    std::uint32_t cpu_draws = 0, gpu_draws = 0, legacy_draws = 0, skipped_draws = 0;
    std::int64_t wall_ns = 0, draw_wall_ns = 0;
    bool callback_details = false;
    std::uint32_t callback_count = 0;
    std::array<callback_record, callback_capacity> callbacks{};
};
inline bool valid_callbacks( const turn_record &r ) noexcept
{
    if( !r.callback_details ) return r.callback_count == 0;
    if( r.callback_count != r.outer_draws || r.callback_count > callback_capacity ) return false;
    std::int64_t sum = 0;
    std::uint32_t cpu = 0, gpu = 0, legacy = 0, skipped = 0;
    for( std::size_t i = 0; i < r.callback_count; ++i ) {
        const auto &c = r.callbacks[i];
        if( c.ordinal != i || !c.returned || c.count < 2 || c.count > callback_boundary_capacity ||
            c.tags[0] != callback_boundary::entry || c.offsets_ns[0] != 0 ||
            c.tags[c.count - 1] != callback_boundary::returned ) return false;
        for( std::size_t j = 1; j < c.count; ++j ) {
            if( c.offsets_ns[j] < c.offsets_ns[j - 1] ||
                ( j + 1 < c.count && std::size_t( c.tags[j] ) != j ) ) return false;
        }
        const auto span = c.offsets_ns[c.count - 1];
        if( span < 0 || sum > std::numeric_limits<std::int64_t>::max() - span ) return false;
        sum += span;
        if( c.mode != backend::cpu && c.mode != backend::gpu && c.mode != backend::legacy ) return false;
        if( c.cpu_reports > 1 ) return false;
        if( c.cpu_reports ) {
            if( c.mode != backend::cpu || c.count != callback_boundary_capacity ||
                c.cpu.width != c.image_width || c.cpu.height != c.image_height ) return false;
            for( double value : {c.cpu.frame_ms, c.cpu.geometry_ms, c.cpu.raster_ms, c.cpu.effects_ms} )
                if( !std::isfinite( value ) || value < 0 ) return false;
            const double module_ms = ( c.offsets_ns[6] - c.offsets_ns[5] ) / 1000000.0;
            if( c.cpu.frame_ms > module_ms + .003 ) return false;
        }
        if( !c.rendered ) ++skipped;
        else if( c.mode == backend::legacy ) ++legacy;
        else {
            if( c.count != callback_boundary_capacity || c.viewport_width < 1 || c.viewport_width > 8192 ||
                c.viewport_height < 1 || c.viewport_height > 8192 || !c.image_width || c.image_width > 8192 ||
                !c.image_height || c.image_height > 8192 || !c.source_slices || c.source_slices > 63 ) return false;
            if( c.mode == backend::cpu ) ++cpu;
            else ++gpu;
        }
    }
    return sum == r.draw_wall_ns && cpu <= r.cpu_draws && gpu <= r.gpu_draws &&
           legacy <= r.legacy_draws && skipped <= r.skipped_draws;
}
inline bool valid( const turn_record &r ) noexcept
{
    return r.claimed && r.returned && r.closed && !r.errors &&
           std::uint64_t( r.cpu_draws ) + r.gpu_draws + r.legacy_draws + r.skipped_draws == r.draw_calls &&
           r.outer_draws <= r.draw_calls && ( r.draw_calls ? r.outer_draws && r.max_depth &&
               r.max_depth <= r.draw_calls - r.outer_draws + 1 : !r.outer_draws && !r.max_depth && !r.draw_wall_ns ) &&
           r.wall_ns >= 0 && r.draw_wall_ns >= 0 && r.draw_wall_ns <= r.wall_ns && valid_callbacks( r );
}
inline clock::time_point steady_now() noexcept { return clock::now(); }
class turn_capture;
class draw_scope;
inline thread_local turn_capture *active_capture = nullptr;
class turn_capture {
    friend class draw_scope;
    friend void mark( callback_boundary, std::uint32_t, std::uint32_t ) noexcept;
    friend void observe_cpu( const callback_record::cpu_work & ) noexcept;
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
    explicit turn_capture( turn_record &record, now_reader reader = steady_now, bool callback_details = false ) noexcept :
        record_( record ), previous_( active_capture ), now_( reader ? reader : steady_now )
    {
        if( !reader ) record_.errors |= invalid_reader;
        if( record_.claimed ) record_.errors |= reused;
        record_.claimed = true;
        record_.callback_details = callback_details;
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
    friend void mark( callback_boundary, std::uint32_t, std::uint32_t ) noexcept;
    friend void observe_cpu( const callback_record::cpu_work & ) noexcept;
    turn_capture *capture_ = active_capture;
    draw_scope *previous_ = nullptr;
    callback_record *callback_ = nullptr;
    clock::time_point begin_;
    bool completed_ = false;
    int exceptions_ = 0;
public:
    explicit draw_scope( int viewport_width = 0, int viewport_height = 0 ) noexcept
    {
        if( !capture_ ) return;
        auto &r = capture_->record_;
        if( r.returned || r.closed ) r.errors |= protocol;
        previous_ = capture_->top_; capture_->top_ = this;
        capture_->increment( r.draw_calls ); capture_->increment( capture_->depth_ );
        if( r.max_depth < capture_->depth_ ) r.max_depth = capture_->depth_;
        if( !previous_ ) {
            capture_->increment( r.outer_draws ); begin_ = capture_->stamp();
            if( r.callback_details ) {
                if( r.callback_count >= callback_capacity ) r.errors |= overflow;
                else {
                    callback_ = &r.callbacks[r.callback_count]; callback_->ordinal = r.callback_count++;
                    callback_->viewport_width = viewport_width; callback_->viewport_height = viewport_height;
                    callback_->tags[0] = callback_boundary::entry; callback_->count = 1;
                }
            }
        }
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
        if( callback_ ) { callback_->rendered = rendered; callback_->mode = mode; }
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
            if( callback_ ) {
                if( callback_->count >= callback_boundary_capacity ) r.errors |= overflow;
                else {
                    const auto at = callback_->count++;
                    callback_->tags[at] = callback_boundary::returned; callback_->offsets_ns[at] = span;
                    callback_->returned = completed_;
                }
            }
        }
        if( capture_->depth_ ) --capture_->depth_;
        capture_->top_ = previous_;
    }
};
// Only the current outermost draw owns a partition. A nested draw's markers
// are ignored, so its inclusive work stays in its parent's current interval.
// Off by default; no clock or allocation on the ordinary rendering path.
inline void mark( callback_boundary tag, std::uint32_t first = 0, std::uint32_t second = 0 ) noexcept
{
    auto *capture = active_capture;
    if( !capture || !capture->top_ || !capture->top_->callback_ ) return;
    auto *draw = capture->top_; auto &c = *draw->callback_;
    if( draw->completed_ || c.count >= callback_boundary_capacity - 1 ||
        std::size_t( tag ) != c.count ||
        ( tag != callback_boundary::module_begin && tag != callback_boundary::module_end && ( first || second ) ) ) {
        capture->record_.errors |= protocol; return;
    }
    const auto at = c.count++;
    c.tags[at] = tag;
    c.offsets_ns[at] = std::chrono::duration_cast<std::chrono::nanoseconds>( capture->stamp() - draw->begin_ ).count();
    if( tag == callback_boundary::module_begin ) { c.source_cells = first; c.source_slices = second; }
    if( tag == callback_boundary::module_end ) { c.image_width = first; c.image_height = second; }
}
// Observe the existing validated Host API event directly. No stale global
// metrics, extra poll, clock or module call is needed. Reported component
// scopes can overlap (held raster is inside geometry); do not sum them.
inline void observe_cpu( const callback_record::cpu_work &value ) noexcept
{
    auto *capture = active_capture;
    if( !capture || !capture->top_ || !capture->top_->callback_ ) return;
    auto *draw = capture->top_; auto &c = *draw->callback_;
    if( !c.source_slices ) return; // Legacy callbacks have no world module scope.
    if( draw->completed_ || c.count != std::size_t( callback_boundary::module_end ) ) {
        capture->record_.errors |= protocol; return;
    }
    capture->increment( c.cpu_reports );
    if( c.cpu_reports != 1 ) capture->record_.errors |= protocol;
    c.cpu = value;
}
} // namespace ncmm_graphics_turn_diagnostics
