#define NCMM_MOD_BUILD
#include "ncmm_api.h"

#include <cstdint>
#include <string>

namespace
{
const char *const module_id = "survivor_progression";

const char *required_caps[] = {
    "core.v1",
    "locale.v1",
    "module_contract.v1",
    "events.turn.v1",
    "character_state.v1",
    "ui.basic.v1"
};

const ncmm_host_api_v1 *host = nullptr;
int turn_accumulator = 0;

bool russian()
{
    const char *raw = host && host->get_locale ? host->get_locale() : "en";
    const std::string locale = raw ? raw : "en";
    return locale == "ru" || locale.rfind( "ru_", 0 ) == 0;
}

int64_t get_state( const char *key, int64_t fallback )
{
    if( !host || !host->character_state_get_i64 ) {
        return fallback;
    }
    return host->character_state_get_i64( module_id, key, fallback );
}

void set_state( const char *key, int64_t value )
{
    if( host && host->character_state_set_i64 ) {
        host->character_state_set_i64( module_id, key, value );
    }
}

int xp_to_next( int level )
{
    return 30 + ( level - 1 ) * 15;
}

void message( const std::string &text )
{
    if( host && host->ui_message ) {
        host->ui_message( text.c_str() );
    }
}

void open_progression()
{
    if( !host || !host->character_state_available || !host->character_state_available() ) {
        message( russian() ?
                 "Survivor Progression: сначала загрузите персонажа." :
                 "Survivor Progression: load a character first." );
        return;
    }

    int level = static_cast<int>( get_state( "level", 1 ) );
    if( level < 1 ) {
        level = 1;
    }
    const int64_t xp = get_state( "xp", 0 );
    int64_t points = get_state( "perk_points", 0 );
    const bool learner = get_state( "fast_learner", 0 ) != 0;

    std::string title;
    std::string perk;
    std::string close;
    if( russian() ) {
        title = "Survivor Progression v0.1\nУровень " + std::to_string( level ) +
                " | XP " + std::to_string( xp ) + "/" + std::to_string( xp_to_next( level ) ) +
                " | Очки перков " + std::to_string( points );
        perk = learner ?
               "[Куплено] Быстрый ученик — +1 XP выживания в минуту" :
               "Быстрый ученик — 1 очко, +1 XP выживания в минуту";
        close = "Закрыть";
    } else {
        title = "Survivor Progression v0.1\nLevel " + std::to_string( level ) +
                " | XP " + std::to_string( xp ) + "/" + std::to_string( xp_to_next( level ) ) +
                " | Perk points " + std::to_string( points );
        perk = learner ?
               "[Owned] Fast Learner — +1 survival XP per minute" :
               "Fast Learner — 1 point, +1 survival XP per minute";
        close = "Close";
    }

    const char *entries[] = { perk.c_str(), close.c_str() };
    const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
    if( choice != 0 ) {
        return;
    }

    if( learner ) {
        message( russian() ? "Быстрый ученик уже куплен." : "Fast Learner is already owned." );
        return;
    }
    if( points < 1 ) {
        message( russian() ? "Недостаточно очков перков." : "Not enough perk points." );
        return;
    }

    set_state( "fast_learner", 1 );
    set_state( "perk_points", points - 1 );
    message( russian() ?
             "Куплен перк «Быстрый ученик». Теперь XP выживания начисляется вдвое быстрее." :
             "Fast Learner purchased. Survival XP now accumulates twice as fast." );
}

void tick()
{
    if( !host || !host->character_state_available || !host->character_state_available() ) {
        turn_accumulator = 0;
        return;
    }

    ++turn_accumulator;
    if( turn_accumulator < 60 ) {
        return;
    }
    turn_accumulator -= 60;

    int level = static_cast<int>( get_state( "level", 1 ) );
    if( level < 1 ) {
        level = 1;
    }
    if( level >= 30 ) {
        return;
    }

    int64_t xp = get_state( "xp", 0 );
    int64_t points = get_state( "perk_points", 0 );
    const bool learner = get_state( "fast_learner", 0 ) != 0;
    xp += learner ? 2 : 1;

    bool leveled = false;
    while( level < 30 && xp >= xp_to_next( level ) ) {
        xp -= xp_to_next( level );
        ++level;
        ++points;
        leveled = true;
    }

    set_state( "level", level );
    set_state( "xp", xp );
    set_state( "perk_points", points );

    if( leveled ) {
        message( russian() ?
                 "Survivor Progression: новый уровень! Получено очко перка." :
                 "Survivor Progression: level up! You gained a perk point." );
        open_progression();
    }
}

int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) {
        return 0;
    }
    for( const char *capability : required_caps ) {
        if( !api->has_capability || !api->has_capability( capability ) ) {
            return 0;
        }
    }
    if( !api->character_state_available || !api->character_state_get_i64 ||
        !api->character_state_set_i64 || !api->ui_choose || !api->ui_message ) {
        return 0;
    }

    host = api;
    api->log( NCMM_LOG_INFO,
              "Survivor Progression 0.1 initialized: survival XP vertical slice active." );
    return 1;
}

void shutdown()
{
    host = nullptr;
    turn_accumulator = 0;
}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION,
    module_id,
    "Survivor Progression",
    "0.1.0",
    required_caps,
    sizeof( required_caps ) / sizeof( required_caps[0] ),
    &init,
    &shutdown
};
} // namespace

extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1()
{
    return &descriptor;
}

extern "C" NCMM_EXPORT void ncmm_on_turn_v1( const ncmm_host_api_v1 *api )
{
    if( api != nullptr ) {
        host = api;
    }
    tick();
}

extern "C" NCMM_EXPORT void ncmm_open_ui_v1( const ncmm_host_api_v1 *api )
{
    if( api != nullptr ) {
        host = api;
    }
    open_progression();
}
