#include "ncmm_graphics_context.hpp"
#include <cstdlib>
#include <iostream>
namespace {
int checks = 0;
void require(bool value, const char *message) {
    ++checks;
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}
int main() {
    ncmm_graphics_context::state context;
    require(!context.perspective && !context.controls, "no game context at startup");
    context.note("DEFAULTMODE");
    require(context.perspective && context.controls, "normal camera owns input");
    context.note("TARGET");
    require(!context.perspective && !context.controls, "coordinate-only picker still uses native map");
    context.note("DEFAULTMODE");
    for (int n = 0; n < 100; ++n) {
        const auto token = context.begin_aiming();
        require(context.perspective && !context.controls, "aim backdrop precedes first input/redraw");
        for (const char *category : {"TARGET", "TARGETING", "QUERY_YN", "INVENTORY", "HELP_KEYBINDINGS"}) {
            context.note(category);
            require(context.perspective && !context.controls, "native aim/menu keeps backdrop and owns input");
        }
        context.end_aiming(token);
        require(context.perspective && context.controls && !context.aiming(), "cancel/shot restores prior context once");
        context.end_aiming(token);
        require(context.controls, "duplicate exit cannot change restored input");
    }
    const auto outer = context.begin_aiming(), inner = context.begin_aiming();
    context.end_aiming(inner);
    require(context.aiming() && !context.controls, "nested exit preserves outer aim ownership");
    context.note("LOOK_AROUND");
    require(!context.perspective, "native coordinate look is still native");
    context.note("TARGET");
    require(context.perspective, "return from look preserves aim backdrop");
    context.end_aiming(outer);
    require(context.controls, "outer exit restores normal context");
    const auto abandoned = context.begin_aiming();
    context.reset();
    context.end_aiming(abandoned);
    require(!context.perspective && !context.controls, "world/module reset cannot resurrect an abandoned context");
    context.note("DEFAULTMODE");
    const auto newer = context.begin_aiming();
    context.end_aiming(abandoned);
    require(context.aiming(), "stale guard cannot finish new world's aim");
    context.end_aiming(newer);
    require(context.controls, "new generation restores its own context");
    std::cout << "Native aiming context: " << checks << " checks PASS\n";
}
