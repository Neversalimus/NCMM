#pragma once
#include <string>

class input_context;

namespace ncmm
{
void initialize();
void shutdown();
void mark_ready();
void on_turn();
void on_language_changed();
void register_gameplay_actions( input_context &ctxt );
bool handle_gameplay_action( const std::string &action );
void show_manager();
std::string settings_menu_label();

/** Aggregate runtime gameplay modifier registered by loaded NCMM modules. */
double gameplay_modifier( const char *modifier_id );
} // namespace ncmm
