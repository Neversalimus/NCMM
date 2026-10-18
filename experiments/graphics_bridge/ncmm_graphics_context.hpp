#pragma once
#include "ncmm_graphics_shapes.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ncmm_graphics_context {
// Temporary screen/input ownership is separate from the user's F6 preference.
// Only the actual native aiming lifetime may preserve a TARGET backdrop.
class state {
    struct saved { uint64_t token; std::string category; };
    std::vector<saved> aiming_;
    std::string category_;
    uint64_t serial_ = 0;
    void update() {
        const bool target = category_ == "TARGET" || category_ == "TARGETING";
        perspective = !category_.empty() && (ncmm_graphics_shapes::stays_perspective(category_) ||
                      (target && !aiming_.empty()));
        controls = category_ == "DEFAULTMODE" && aiming_.empty();
    }
public:
    bool perspective = false, controls = false;
    void note(const std::string &category) { category_ = category; update(); }
    uint64_t begin_aiming() {
        const auto token = ++serial_;
        aiming_.push_back({token, category_});
        note("TARGET"); // Before the target UI's first redraw, not its first input.
        return token;
    }
    void end_aiming(uint64_t token) {
        // A lifecycle reset invalidates outstanding guards. Never restore them.
        if (!token || aiming_.empty() || aiming_.back().token != token) return;
        category_ = std::move(aiming_.back().category);
        aiming_.pop_back();
        update();
    }
    void reset() { aiming_.clear(); category_.clear(); perspective = controls = false; }
    bool aiming() const { return !aiming_.empty(); }
};
}
