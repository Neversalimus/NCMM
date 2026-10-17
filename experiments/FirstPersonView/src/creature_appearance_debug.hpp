#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace first_person::creature {
struct model;
struct impostor_atlas;
// Explicit diagnostic IO is compiled once, outside the renderer include graph.
// Only owned immutable assets enter this function; it never accesses CDDA/SDL.
bool write_appearance_debug(const model &,const impostor_atlas &,uint64_t,
                            std::string_view label,const std::string &root);
}
