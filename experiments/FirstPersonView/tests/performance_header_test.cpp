#include "ncmm_graphics_performance.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

// Use the same enum spelling as the game after including the Windows SDK
// helper. This catches RPC macro pollution in the fast MSVC native job.
enum class creature_size { tiny, small, medium, large, huge };
static_assert(int(creature_size::small) == 1);
static_assert(std::min(2, 3) == 2 && std::max(2, 3) == 3);

int main() {
    const double cpu = ncmm_graphics_performance::cpu_seconds();
    const double thread_before = ncmm_graphics_performance::thread_cpu_seconds();
    const double thread_after = ncmm_graphics_performance::thread_cpu_seconds();
    const auto memory = ncmm_graphics_performance::vram();
    if (!std::isfinite(cpu) || cpu < 0 || (!memory.available && memory.bytes != 0)) return 1;
    if (!std::isfinite(thread_before) || !std::isfinite(thread_after) || thread_before < -1 || thread_after < thread_before) return 1;
    std::puts("Host performance header: compile/runtime PASS");
}
