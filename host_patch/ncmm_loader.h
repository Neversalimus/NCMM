#pragma once
#include <string>

namespace ncmm
{
void initialize();
void shutdown();
void mark_ready();
void on_turn();
void on_language_changed();
void show_manager();
std::string settings_menu_label();
} // namespace ncmm
