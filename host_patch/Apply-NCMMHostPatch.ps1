param(
    [Parameter(Mandatory=$true)][string]$SourceRoot
)
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$src = Join-Path $SourceRoot 'src'
$optionsH = Join-Path $src 'options.h'
$optionsCpp = Join-Path $src 'options.cpp'
$sdl = Join-Path $src 'sdltiles.cpp'
$mainMenu = Join-Path $src 'main_menu.cpp'
$doTurn = Join-Path $src 'do_turn.cpp'
$inputH = Join-Path $src 'input.h'
$inputCpp = Join-Path $src 'input.cpp'
$handleAction = Join-Path $src 'handle_action.cpp'
$characterCpp = Join-Path $src 'character.cpp'
$characterHealthCpp = Join-Path $src 'character_health.cpp'
$meleeCpp = Join-Path $src 'melee.cpp'
$knowledgeCpp = Join-Path $src 'character_knowledge.cpp'
$craftingCpp = Join-Path $src 'crafting.cpp'
$marker = Join-Path $SourceRoot '.ncmm_host_v1_patched'

foreach ($f in @($optionsH,$optionsCpp,$sdl,$mainMenu,$doTurn,$inputH,$inputCpp,$handleAction,
                  $characterCpp,$characterHealthCpp,$meleeCpp,$knowledgeCpp,$craftingCpp)) {
    if (-not (Test-Path $f)) { throw "Required source file missing: $f" }
}

$Utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Read-Utf8([string]$Path) {
    return [System.IO.File]::ReadAllText($Path, $Utf8Strict)
}

function Write-Utf8([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, $Utf8NoBom)
}

function Normalize-Lf([string]$Text) {
    if ($null -eq $Text) { return $Text }
    return $Text.Replace("`r`n","`n").Replace("`r","`n")
}

function Replace-ExactlyOnce([string]$Text,[string]$Old,[string]$New,[string]$Contract) {
    $Text = Normalize-Lf $Text
    $Old = Normalize-Lf $Old
    $New = Normalize-Lf $New
    $count = ([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1) { throw "NCMM contract '$Contract' expected exactly once, found $count. Host patch DISABLED." }
    return $Text.Replace($Old,$New)
}

function NonAscii-Signature([string]$Text) {
    $sb = New-Object System.Text.StringBuilder
    foreach ($ch in $Text.ToCharArray()) {
        if ([int][char]$ch -gt 127) {
            [void]$sb.Append($ch)
        }
    }
    return $sb.ToString()
}

if (Test-Path $marker) {
    $markerText = [System.IO.File]::ReadAllText($marker)
    if (-not $markerText.Contains('NCMM 0.7.1')) {
        throw 'Older NCMM host patch marker detected; clean upstream source required for NCMM 0.7.1.'
    }

    $h = Read-Utf8 $optionsH
    $c = Read-Utf8 $optionsCpp
    $sd = Read-Utf8 $sdl
    $mm = Read-Utf8 $mainMenu
    $dt = Read-Utf8 $doTurn
    $ih = Read-Utf8 $inputH
    $ic = Read-Utf8 $inputCpp
    $ha = Read-Utf8 $handleAction
    $ch = Read-Utf8 $characterCpp
    $hh = Read-Utf8 $characterHealthCpp
    $me = Read-Utf8 $meleeCpp
    $kn = Read-Utf8 $knowledgeCpp
    $cr = Read-Utf8 $craftingCpp
    $checks = @(
        @($h,'COPT_WORLDGEN_ONLY'),
        @($h,'ncmm_begin_worldgen_group'),
        @($h,'ncmm_set_worldgen_string_choices'),
        @($c,'case COPT_WORLDGEN_ONLY:'),
        @($c,'is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) )'),
        @($c,'options_manager::ncmm_begin_worldgen_group'),
        @($c,'options_manager::ncmm_set_worldgen_string_choices'),
        @($sd,'ncmm::initialize();'),
        @($mm,'ncmm::settings_menu_label()'),
        @($mm,'ncmm::show_manager();'),
        @($mm,'ncmm::on_language_changed();'),
        @($mm,'ncmm::register_gameplay_actions( ctxt_default );'),
        @($dt,'ncmm::on_turn();'),
        @($ih,'ncmm_register_default_action'),
        @($ic,'alternate_type'),
        @($ha,'ncmm::register_gameplay_actions( ctxt );'),
        @($ha,'ncmm::handle_gameplay_action( action )'),
        @($ch,'ncmm::gameplay_modifier( "str_flat" )'),
        @($ch,'ncmm::gameplay_modifier( "speed_pct" )'),
        @($hh,'ncmm::gameplay_modifier( "stamina_max_pct" )'),
        @($me,'ncmm::gameplay_modifier( "dodge_flat" )'),
        @($kn,'ncmm::gameplay_modifier( "read_speed_pct" )'),
        @($cr,'ncmm::gameplay_modifier( "craft_speed_pct" )')
    )
    foreach ($x in $checks) {
        if (-not $x[0].Contains($x[1])) {
            throw "Existing NCMM marker found but patched contract missing: $($x[1])"
        }
    }
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_fault_policy.h') (Join-Path $src 'ncmm_fault_policy.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_manifest_policy.h') (Join-Path $src 'ncmm_manifest_policy.h') -Force
    Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force

    # Existing-patch verification must inspect the source variables read above.
    # The old branch referenced $ch2/$hh2/... before those variables existed.
    foreach ($needle in @('ncmm::gameplay_modifier( "str_flat" )','ncmm::gameplay_modifier( "speed_pct" )',
                            'ncmm::gameplay_modifier( "carry_weight_pct" )','ncmm::gameplay_modifier( "move_cost_pct" )',
                            'ncmm::gameplay_modifier( "melee_hit_flat" )')) {
        if (-not $ch.Contains($needle)) { throw "Post-check failed: $needle" }
    }
    foreach ($needle in @('ncmm::gameplay_modifier( "stamina_max_pct" )','ncmm::gameplay_modifier( "healing_pct" )')) {
        if (-not $hh.Contains($needle)) { throw "Post-check failed: $needle" }
    }
    if (-not $me.Contains('ncmm::gameplay_modifier( "dodge_flat" )')) { throw 'Post-check failed: dodge_flat' }
    if (-not $kn.Contains('ncmm::gameplay_modifier( "read_speed_pct" )')) { throw 'Post-check failed: read_speed_pct' }
    if (-not $cr.Contains('ncmm::gameplay_modifier( "craft_speed_pct" )')) { throw 'Post-check failed: craft_speed_pct' }

    Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.7.1 module contract`n" -Encoding ASCII
    Write-Host 'Existing NCMM upstream patch verified; v0.7.1 loader/API refreshed.'
    exit 0
}

$contractScript = Join-Path (Split-Path $PSScriptRoot -Parent) 'ci\Test-SourceContracts.ps1'
# PowerShell script invocation reports failures through terminating exceptions under
# ErrorActionPreference=Stop. $LASTEXITCODE belongs to native processes and may be
# stale from an earlier git/gh command, so it must not gate this contract preflight.
& $contractScript -SourceRoot $SourceRoot

$hOriginal = Read-Utf8 $optionsH
$cOriginal = Read-Utf8 $optionsCpp
$sdOriginal = Read-Utf8 $sdl
$mmOriginal = Read-Utf8 $mainMenu
$dtOriginal = Read-Utf8 $doTurn
$ihOriginal = Read-Utf8 $inputH
$icOriginal = Read-Utf8 $inputCpp
$haOriginal = Read-Utf8 $handleAction
$chOriginal = Read-Utf8 $characterCpp
$hhOriginal = Read-Utf8 $characterHealthCpp
$meOriginal = Read-Utf8 $meleeCpp
$knOriginal = Read-Utf8 $knowledgeCpp
$crOriginal = Read-Utf8 $craftingCpp
$hSig = NonAscii-Signature $hOriginal
$cSig = NonAscii-Signature $cOriginal
$sdSig = NonAscii-Signature $sdOriginal
$mmSig = NonAscii-Signature $mmOriginal
$dtSig = NonAscii-Signature $dtOriginal
$ihSig = NonAscii-Signature $ihOriginal
$icSig = NonAscii-Signature $icOriginal
$haSig = NonAscii-Signature $haOriginal
$chSig = NonAscii-Signature $chOriginal
$hhSig = NonAscii-Signature $hhOriginal
$meSig = NonAscii-Signature $meOriginal
$knSig = NonAscii-Signature $knOriginal
$crSig = NonAscii-Signature $crOriginal

$h = Normalize-Lf $hOriginal
$c = Normalize-Lf $cOriginal
$sd = Normalize-Lf $sdOriginal
$mm = Normalize-Lf $mmOriginal
$dt = Normalize-Lf $dtOriginal
$ih = Normalize-Lf $ihOriginal
$ic = Normalize-Lf $icOriginal
$ha = Normalize-Lf $haOriginal
$ch = Normalize-Lf $chOriginal
$hh = Normalize-Lf $hhOriginal
$me = Normalize-Lf $meOriginal
$kn = Normalize-Lf $knOriginal
$cr = Normalize-Lf $crOriginal

$h = Replace-ExactlyOnce $h @'
            COPT_NO_SOUND_HIDE,
            /** Hide this option always, it should not be changed by user directly through UI. **/
            COPT_ALWAYS_HIDE
'@ @'
            COPT_NO_SOUND_HIDE,
            /** Hidden in normal/in-game options, visible only in the world-generation/current-world options UI. */
            COPT_WORLDGEN_ONLY,
            /** Hide this option always, it should not be changed by user directly through UI. **/
            COPT_ALWAYS_HIDE
'@ 'options.hide-enum'

$h = Replace-ExactlyOnce $h '                bool is_hidden() const;' '                bool is_hidden( bool worldgen_context = false ) const;' 'options.is-hidden-signature'
$h = Replace-ExactlyOnce $h '        void set_world_options( options_container *options );' @'
        void set_world_options( options_container *options );

        /** NCMM: expose and lay out existing hidden world options without inventing new world state. */
        bool ncmm_can_expose_worldgen_option( const std::string &name ) const;
        bool ncmm_expose_worldgen_option( const std::string &name, const translation &menu_text,
                                         const translation &tooltip );
        bool ncmm_begin_worldgen_group( const std::string &group_id, const translation &name,
                                        const translation &tooltip );
        void ncmm_end_worldgen_group();
        bool ncmm_set_worldgen_string_choices( const std::string &name,
                const std::vector<id_and_option> &items );
'@ 'options.ncmm-api-declaration'

$c = Replace-ExactlyOnce $c 'bool options_manager::cOpt::is_hidden() const' 'bool options_manager::cOpt::is_hidden( bool worldgen_context ) const' 'options.is-hidden-definition'
$c = Replace-ExactlyOnce $c @'
        case COPT_ALWAYS_HIDE:
            return true;
'@ @'
        case COPT_WORLDGEN_ONLY:
            return !worldgen_context;

        case COPT_ALWAYS_HIDE:
            return true;
'@ 'options.worldgen-hide-case'
$c = Replace-ExactlyOnce $c '                    && !get_options().get_option( it.data ).is_hidden();' '                    && !get_options().get_option( it.data ).is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) );' 'options.worldgen-visibility'
$c = Replace-ExactlyOnce $c '                    && !get_options().get_option( curr_item.data ).is_hidden();' '                    && !get_options().get_option( curr_item.data ).is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) );' 'options.worldgen-selectability'
$c = Replace-ExactlyOnce $c @'
bool options_manager::has_option( const std::string &name ) const
{
    return options.count( name );
}
'@ @'
bool options_manager::has_option( const std::string &name ) const
{
    return options.count( name );
}

bool options_manager::ncmm_can_expose_worldgen_option( const std::string &name ) const
{
    auto it = options.find( name );
    return it != options.end() && it->second.sPage == "world_default" &&
           ( it->second.hide == COPT_ALWAYS_HIDE || it->second.hide == COPT_WORLDGEN_ONLY );
}

bool options_manager::ncmm_expose_worldgen_option( const std::string &name,
        const translation &menu_text, const translation &tooltip )
{
    if( !ncmm_can_expose_worldgen_option( name ) ) {
        return false;
    }
    cOpt &opt = options.find( name )->second;
    const bool first_exposure = opt.hide == COPT_ALWAYS_HIDE;
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;

    if( world_options.has_value() ) {
        auto world_it = ( **world_options ).find( name );
        if( world_it != ( **world_options ).end() ) {
            world_it->second.sMenuText = menu_text;
            world_it->second.sTooltip = tooltip;
            world_it->second.hide = COPT_WORLDGEN_ONLY;
        }
    }

    // CDDA init-time cleanup physically removes COPT_ALWAYS_HIDE PageItems.
    // Re-add only on first exposure; active NCMM group is captured by addOptionToPage.
    if( first_exposure ) {
        addOptionToPage( name, "world_default" );
    }
    return true;
}

bool options_manager::ncmm_begin_worldgen_group( const std::string &group_id,
        const translation &name, const translation &tooltip )
{
    if( group_id.empty() || !adding_to_group_.empty() ) {
        return false;
    }

    for( Group &group : groups_ ) {
        if( group.id_ == group_id ) {
            group.name_ = name;
            group.tooltip_ = tooltip;
            adding_to_group_ = group_id;
            return true;
        }
    }

    groups_.emplace_back( group_id, name, tooltip );
    add_empty_line( "world_default" );
    find_page( "world_default" ).items_.emplace_back(
        ItemType::GroupHeader, group_id, group_id );
    adding_to_group_ = group_id;
    return true;
}

void options_manager::ncmm_end_worldgen_group()
{
    adding_to_group_.clear();
}

bool options_manager::ncmm_set_worldgen_string_choices( const std::string &name,
        const std::vector<id_and_option> &items )
{
    if( items.empty() ) {
        return false;
    }

    auto apply_choices = [&]( cOpt & opt ) {
        if( opt.sPage != "world_default" || opt.eType != cOpt::CVT_STRING ) {
            return false;
        }
        const std::string current = opt.sSet;
        const std::string default_value = opt.sDefault;
        const auto contains = [&]( const std::string & value ) {
            return std::any_of( items.begin(), items.end(), [&]( const id_and_option & item ) {
                return item.first == value;
            } );
        };

        opt.sType = "string_select";
        opt.eType = cOpt::CVT_STRING;
        opt.vItems = items;
        opt.iMaxLength = 0;
        opt.sSet = contains( current ) ? current : items.front().first;
        opt.sDefault = contains( default_value ) ? default_value : items.front().first;
        return true;
    };

    auto it = options.find( name );
    if( it == options.end() || !apply_choices( it->second ) ) {
        return false;
    }

    if( world_options.has_value() ) {
        auto world_it = ( **world_options ).find( name );
        if( world_it != ( **world_options ).end() ) {
            apply_choices( world_it->second );
        }
    }
    return true;
}
'@ 'options.ncmm-expose-layout-implementation'

$sd = Replace-ExactlyOnce $sd '#include "options.h"' ('#include "options.h"' + "`n" + '#include "ncmm_loader.h"') 'sdl.include-ncmm'
$sd = Replace-ExactlyOnce $sd @'
    get_options().init();
    get_options().load();
    set_language_from_options(); //Prevent translated language strings from causing an error if language not set
'@ @'
    get_options().init();
    get_options().load();
    set_language_from_options(); //Prevent translated language strings from causing an error if language not set
    ncmm::initialize();
'@ 'sdl.initialize-ncmm'

$dt = Replace-ExactlyOnce $dt '#include "npc.h"' ('#include "npc.h"' + "`n" + '#include "ncmm_loader.h"') 'turn.include-ncmm'
$dt = Replace-ExactlyOnce $dt @'
    } else {
        gamemode->per_turn();
        calendar::turn += 1_turns;
    }
    //used for dimension swapping
'@ @'
    } else {
        gamemode->per_turn();
        calendar::turn += 1_turns;
    }
    ncmm::on_turn();
    //used for dimension swapping
'@ 'turn.dispatch-ncmm'

$ih = Replace-ExactlyOnce $ih '        void save();' @'
        void save();

        /** NCMM: register a stable keyboard-any default without overwriting user remaps. */
        void ncmm_register_default_action( const std::string &action_descriptor,
                                           const translation &name,
                                           const input_event &default_event );
'@ 'input.ncmm-default-action-declaration'

$ic = Replace-ExactlyOnce $ic @'
void input_manager::check_keybind( const std::string &category, const std::string &keybind,
                                   const std::string &context ) const
'@ @'
void input_manager::ncmm_register_default_action( const std::string &action_descriptor,
        const translation &name, const input_event &default_event )
{
    action_attributes &basic = basic_action_contexts[default_context_id][action_descriptor];
    basic.name = name;
    basic.is_user_created = false;
    basic.input_events.clear();
    basic.input_events.push_back( default_event );

    // CDDA may run DEFAULTMODE as keycode or keychar. Native "keyboard_any"
    // bindings contain both representations; NCMM defaults must do the same.
    if( default_event.type == input_event_t::keyboard_code ||
        default_event.type == input_event_t::keyboard_char ) {
        const input_event_t alternate_type =
            default_event.type == input_event_t::keyboard_code ?
            input_event_t::keyboard_char : input_event_t::keyboard_code;
        const std::string portable_name =
            get_keyname( default_event.get_first_input(), default_event.type, true );
        const int alternate_code = get_keycode( alternate_type, portable_name );
        if( alternate_code != 0 ) {
            const input_event alternate( default_event.modifiers, alternate_code, alternate_type );
            if( std::find( basic.input_events.begin(), basic.input_events.end(), alternate ) ==
                basic.input_events.end() ) {
                basic.input_events.push_back( alternate );
            }
        }
    }

    t_actions &active = action_contexts[default_context_id];
    const auto it = active.find( action_descriptor );
    if( it == active.end() ) {
        active[action_descriptor] = basic;
    } else {
        // Preserve user-selected input events; refresh only the display name.
        it->second.name = name;
    }
}

void input_manager::check_keybind( const std::string &category, const std::string &keybind,
                                   const std::string &context ) const
'@ 'input.ncmm-default-action-implementation'

$ha = Replace-ExactlyOnce $ha @'
#include "mutation.h"
#include "options.h"
'@ @'
#include "mutation.h"
#include "ncmm_loader.h"
#include "options.h"
'@ 'gameplay.include-ncmm'

$ha = Replace-ExactlyOnce $ha @'
    } else {
        ctxt = get_default_mode_input_context();
    }
'@ @'
    } else {
        ctxt = get_default_mode_input_context();
        ncmm::register_gameplay_actions( ctxt );
    }
'@ 'gameplay.register-ncmm-actions'

$ha = Replace-ExactlyOnce $ha @'
    if( uquit == QUIT_WATCH && action == "QUIT" ) {
        uquit = QUIT_DIED;
        return false;
    }

    if( act == ACTION_NULL ) {
        act = look_up_action( action );
'@ @'
    if( uquit == QUIT_WATCH && action == "QUIT" ) {
        uquit = QUIT_DIED;
        return false;
    }

    if( act == ACTION_NULL && ncmm::handle_gameplay_action( action ) ) {
        player_character.clear_destination();
        destination_preview.clear();
        return false;
    }

    if( act == ACTION_NULL ) {
        act = look_up_action( action );
'@ 'gameplay.dispatch-ncmm-actions'

$mm = Replace-ExactlyOnce $mm '#include "options.h"' ('#include "options.h"' + "`n" + '#include "ncmm_loader.h"') 'main-menu.include-ncmm'
$mm = Replace-ExactlyOnce $mm @'
    vSettingsSubItems.emplace_back( pgettext( "Main Menu|Settings", "<I|i>mGui Demo Screen" ) );
'@ @'
    vSettingsSubItems.emplace_back( pgettext( "Main Menu|Settings", "<I|i>mGui Demo Screen" ) );
    vSettingsSubItems.emplace_back( ncmm::settings_menu_label() );
'@ 'main-menu.settings-item'
$mm = Replace-ExactlyOnce $mm @'
                        // The language may have changed- gracefully handle this.
                        init_strings();
'@ @'
                        // The language may have changed- gracefully handle this.
                        init_strings();
                        ncmm::on_language_changed();
'@ 'main-menu.locale-refresh'
$mm = Replace-ExactlyOnce $mm @'
                    } else if( sel2 == 1 ) { /// Keybindings
                        input_context ctxt_default = get_default_mode_input_context();
                        ctxt_default.display_menu();
'@ @'
                    } else if( sel2 == 1 ) { /// Keybindings
                        input_context ctxt_default = get_default_mode_input_context();
                        ncmm::register_gameplay_actions( ctxt_default );
                        ctxt_default.display_menu();
'@ 'main-menu.ncmm-keybindings'
$mm = Replace-ExactlyOnce $mm @'
                    } else if( sel2 == 6 ) { /// ImGui demo
                        imgui_demo_ui demo;
                        demo.run();
                    }
'@ @'
                    } else if( sel2 == 6 ) { /// ImGui demo
                        imgui_demo_ui demo;
                        demo.run();
                    } else if( static_cast<std::size_t>( sel2 ) + 1 == vSettingsSubItems.size() ) {
                        ncmm::show_manager();
                    }
'@ 'main-menu.ncmm-manager-action'


# NCMM 0.7.1 generic character modifier hooks.
$ch = Replace-ExactlyOnce $ch '#include "npc.h"' ('#include "npc.h"' + "`n" + '#include "ncmm_loader.h"') 'character.include-ncmm'
$ch = Replace-ExactlyOnce $ch @'
int Character::get_str() const
{
    return std::min( character_max_str, std::max( 0, get_str_base() + get_str_bonus() ) );
}
int Character::get_dex() const
{
    return std::min( character_max_dex, std::max( 0, get_dex_base() + get_dex_bonus() ) );
}
int Character::get_per() const
{
    return std::min( character_max_per, std::max( 0, get_per_base() + get_per_bonus() ) );
}
int Character::get_int() const
{
    return std::min( character_max_int, std::max( 0, get_int_base() + get_int_bonus() ) );
}
'@ @'
int Character::get_str() const
{
    const int ncmm_bonus = is_avatar() ? static_cast<int>( std::lround( ncmm::gameplay_modifier( "str_flat" ) ) ) : 0;
    return std::min( character_max_str, std::max( 0, get_str_base() + get_str_bonus() + ncmm_bonus ) );
}
int Character::get_dex() const
{
    const int ncmm_bonus = is_avatar() ? static_cast<int>( std::lround( ncmm::gameplay_modifier( "dex_flat" ) ) ) : 0;
    return std::min( character_max_dex, std::max( 0, get_dex_base() + get_dex_bonus() + ncmm_bonus ) );
}
int Character::get_per() const
{
    const int ncmm_bonus = is_avatar() ? static_cast<int>( std::lround( ncmm::gameplay_modifier( "per_flat" ) ) ) : 0;
    return std::min( character_max_per, std::max( 0, get_per_base() + get_per_bonus() + ncmm_bonus ) );
}
int Character::get_int() const
{
    const int ncmm_bonus = is_avatar() ? static_cast<int>( std::lround( ncmm::gameplay_modifier( "int_flat" ) ) ) : 0;
    return std::min( character_max_int, std::max( 0, get_int_base() + get_int_bonus() + ncmm_bonus ) );
}
'@ 'character.primary-stats'

$ch = Replace-ExactlyOnce $ch @'
int Character::get_speed() const
{
    if( has_flag( json_flag_STEADY ) ) {
        return get_speed_base() + std::max( 0, get_speed_bonus() );
    }
    return Creature::get_speed();
}
'@ @'
int Character::get_speed() const
{
    int result = has_flag( json_flag_STEADY ) ?
                 get_speed_base() + std::max( 0, get_speed_bonus() ) :
                 Creature::get_speed();
    if( is_avatar() ) {
        const double multiplier = std::max( 0.1, 1.0 + ncmm::gameplay_modifier( "speed_pct" ) / 100.0 );
        result = static_cast<int>( std::lround( result * multiplier ) );
    }
    return std::max( 1, result );
}
'@ 'character.speed'

$ch = Replace-ExactlyOnce $ch @'
float Character::get_hit_base() const
{
    /** @EFFECT_DEX increases hit base, slightly */
    return get_dex() / 4.0f;
}
'@ @'
float Character::get_hit_base() const
{
    /** @EFFECT_DEX increases hit base, slightly */
    float result = get_dex() / 4.0f;
    if( is_avatar() ) {
        result += static_cast<float>( ncmm::gameplay_modifier( "melee_hit_flat" ) );
    }
    return result;
}
'@ 'character.melee-hit'

$ch = Replace-ExactlyOnce $ch @'
    ret = enchantment_cache->modify_value( enchant_vals::mod::CARRY_WEIGHT, ret );

    if( ret < 0_gram ) {
'@ @'
    ret = enchantment_cache->modify_value( enchant_vals::mod::CARRY_WEIGHT, ret );
    if( is_avatar() ) {
        const double multiplier = std::max( 0.0, 1.0 + ncmm::gameplay_modifier( "carry_weight_pct" ) / 100.0 );
        ret *= multiplier;
    }

    if( ret < 0_gram ) {
'@ 'character.carry-weight'

$ch = Replace-ExactlyOnce $ch @'
int Character::run_cost( int base_cost, bool diag ) const
{
    float movecost = static_cast<float>( base_cost );
    if( diag ) {
        movecost /= M_SQRT2; // because effect logic assumes 100 base cost
    }
    run_cost_effects( movecost );
    if( diag ) {
        movecost *= M_SQRT2;
    }
    return static_cast<int>( movecost );
}
'@ @'
int Character::run_cost( int base_cost, bool diag ) const
{
    float movecost = static_cast<float>( base_cost );
    if( diag ) {
        movecost /= M_SQRT2; // because effect logic assumes 100 base cost
    }
    run_cost_effects( movecost );
    if( diag ) {
        movecost *= M_SQRT2;
    }
    if( is_avatar() ) {
        const double multiplier = std::max( 0.25, 1.0 + ncmm::gameplay_modifier( "move_cost_pct" ) / 100.0 );
        movecost *= multiplier;
    }
    return std::max( 1, static_cast<int>( movecost ) );
}
'@ 'character.move-cost'

$hh = Replace-ExactlyOnce $hh '#include "npc.h"' ('#include "npc.h"' + "`n" + '#include "ncmm_loader.h"') 'health.include-ncmm'
$hh = Replace-ExactlyOnce $hh @'
    max_stamina = enchantment_cache->modify_value( enchant_vals::mod::MAX_STAMINA, max_stamina );

    return max_stamina;
'@ @'
    max_stamina = enchantment_cache->modify_value( enchant_vals::mod::MAX_STAMINA, max_stamina );
    if( is_avatar() ) {
        const double multiplier = std::max( 0.1, 1.0 + ncmm::gameplay_modifier( "stamina_max_pct" ) / 100.0 );
        max_stamina = static_cast<int>( std::lround( max_stamina * multiplier ) );
    }

    return std::max( 1, max_stamina );
'@ 'health.max-stamina'
$hh = Replace-ExactlyOnce $hh @'
    float final_rate = awake_rate + asleep_rate;
    // Most common case: awake player with no regenerative abilities
'@ @'
    float final_rate = awake_rate + asleep_rate;
    // Positive healing perks must not amplify negative/degenerative rates.
    if( is_avatar() && final_rate > 0.0f ) {
        final_rate *= static_cast<float>( std::max( 0.0, 1.0 + ncmm::gameplay_modifier( "healing_pct" ) / 100.0 ) );
    }
    // Most common case: awake player with no regenerative abilities
'@ 'health.healing'

$me = Replace-ExactlyOnce $me '#include "npc.h"' ('#include "npc.h"' + "`n" + '#include "ncmm_loader.h"') 'melee.include-ncmm'
$me = Replace-ExactlyOnce $me @'
    ret /= anatomy( get_all_body_parts() ).get_size_ratio( anatomy_human_anatomy );
    add_msg_debug( debugmode::DF_MELEE, "Dodge after bodysize modifier %.1f", ret );

    return std::max( 0.0f, ret );
'@ @'
    ret /= anatomy( get_all_body_parts() ).get_size_ratio( anatomy_human_anatomy );
    add_msg_debug( debugmode::DF_MELEE, "Dodge after bodysize modifier %.1f", ret );

    if( is_avatar() ) {
        ret += static_cast<float>( ncmm::gameplay_modifier( "dodge_flat" ) );
    }
    return std::max( 0.0f, ret );
'@ 'melee.dodge'

$kn = Replace-ExactlyOnce $kn '#include "monster.h"' ('#include "monster.h"' + "`n" + '#include "ncmm_loader.h"') 'knowledge.include-ncmm'
$kn = Replace-ExactlyOnce $kn @'
    if( ret < 1_seconds ) {
        ret = 1_seconds;
    }
    return ret * 100 / 1_minutes;
'@ @'
    if( ret < 1_seconds ) {
        ret = 1_seconds;
    }
    int result = ret * 100 / 1_minutes;
    if( is_avatar() ) {
        const double multiplier = std::max( 0.1, 1.0 + ncmm::gameplay_modifier( "read_speed_pct" ) / 100.0 );
        result = static_cast<int>( std::lround( result / multiplier ) );
    }
    return std::max( 1, result );
'@ 'knowledge.read-speed'

$cr = Replace-ExactlyOnce $cr '#include "npc.h"' ('#include "npc.h"' + "`n" + '#include "ncmm_loader.h"') 'crafting.include-ncmm'
$cr = Replace-ExactlyOnce $cr @'
float Character::mut_crafting_speed_multiplier( const recipe &rec ) const
{
    return rec.has_flag( flag_NO_ENCHANTMENT ) ? 1.0f : 1.0 + enchantment_cache->get_value_multiply(
               enchant_vals::mod::CRAFTING_SPEED_MULTIPLIER );
}
'@ @'
float Character::mut_crafting_speed_multiplier( const recipe &rec ) const
{
    float result = rec.has_flag( flag_NO_ENCHANTMENT ) ? 1.0f : 1.0f +
                   enchantment_cache->get_value_multiply( enchant_vals::mod::CRAFTING_SPEED_MULTIPLIER );
    if( is_avatar() ) {
        result *= static_cast<float>( std::max( 0.1, 1.0 + ncmm::gameplay_modifier( "craft_speed_pct" ) / 100.0 ) );
    }
    return result;
}
'@ 'crafting.mutation-speed'
$cr = Replace-ExactlyOnce $cr @'
    const float result = enchantment_cache->modify_value( enchant_vals::mod::CRAFTING_SPEED_MULTIPLIER,
                         crafting_speed );

    add_msg_debug( debugmode::DF_CHARACTER, "Limb score multiplier %.1f, crafting speed multiplier %1f",
                   get_limb_score( limb_score_manip ), result );

    return std::max( result, 0.0f );
'@ @'
    float result = enchantment_cache->modify_value( enchant_vals::mod::CRAFTING_SPEED_MULTIPLIER,
                   crafting_speed );
    if( is_avatar() ) {
        result *= static_cast<float>( std::max( 0.1, 1.0 + ncmm::gameplay_modifier( "craft_speed_pct" ) / 100.0 ) );
    }

    add_msg_debug( debugmode::DF_CHARACTER, "Limb score multiplier %.1f, crafting speed multiplier %1f",
                   get_limb_score( limb_score_manip ), result );

    return std::max( result, 0.0f );
'@ 'crafting.recipe-speed'

Write-Utf8 $optionsH $h
Write-Utf8 $optionsCpp $c
Write-Utf8 $sdl $sd
Write-Utf8 $mainMenu $mm
Write-Utf8 $doTurn $dt
Write-Utf8 $inputH $ih
Write-Utf8 $inputCpp $ic
Write-Utf8 $handleAction $ha
Write-Utf8 $characterCpp $ch
Write-Utf8 $characterHealthCpp $hh
Write-Utf8 $meleeCpp $me
Write-Utf8 $knowledgeCpp $kn
Write-Utf8 $craftingCpp $cr

Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_fault_policy.h') (Join-Path $src 'ncmm_fault_policy.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_manifest_policy.h') (Join-Path $src 'ncmm_manifest_policy.h') -Force
Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force

$h2 = Read-Utf8 $optionsH
$c2 = Read-Utf8 $optionsCpp
$sd2 = Read-Utf8 $sdl
$mm2 = Read-Utf8 $mainMenu
$dt2 = Read-Utf8 $doTurn
$ih2 = Read-Utf8 $inputH
$ic2 = Read-Utf8 $inputCpp
$ha2 = Read-Utf8 $handleAction
$ch2 = Read-Utf8 $characterCpp
$hh2 = Read-Utf8 $characterHealthCpp
$me2 = Read-Utf8 $meleeCpp
$kn2 = Read-Utf8 $knowledgeCpp
$cr2 = Read-Utf8 $craftingCpp

if ((NonAscii-Signature $h2) -ne $hSig) { throw 'UTF-8 preservation check failed for options.h' }
if ((NonAscii-Signature $c2) -ne $cSig) { throw 'UTF-8 preservation check failed for options.cpp' }
if ((NonAscii-Signature $sd2) -ne $sdSig) { throw 'UTF-8 preservation check failed for sdltiles.cpp' }
if ((NonAscii-Signature $mm2) -ne $mmSig) { throw 'UTF-8 preservation check failed for main_menu.cpp' }
if ((NonAscii-Signature $dt2) -ne $dtSig) { throw 'UTF-8 preservation check failed for do_turn.cpp' }
if ((NonAscii-Signature $ih2) -ne $ihSig) { throw 'UTF-8 preservation check failed for input.h' }
if ((NonAscii-Signature $ic2) -ne $icSig) { throw 'UTF-8 preservation check failed for input.cpp' }
if ((NonAscii-Signature $ha2) -ne $haSig) { throw 'UTF-8 preservation check failed for handle_action.cpp' }
if ((NonAscii-Signature $ch2) -ne $chSig) { throw 'UTF-8 preservation check failed for character.cpp' }
if ((NonAscii-Signature $hh2) -ne $hhSig) { throw 'UTF-8 preservation check failed for character_health.cpp' }
if ((NonAscii-Signature $me2) -ne $meSig) { throw 'UTF-8 preservation check failed for melee.cpp' }
if ((NonAscii-Signature $kn2) -ne $knSig) { throw 'UTF-8 preservation check failed for character_knowledge.cpp' }
if ((NonAscii-Signature $cr2) -ne $crSig) { throw 'UTF-8 preservation check failed for crafting.cpp' }

foreach ($needle in @('COPT_WORLDGEN_ONLY','ncmm_begin_worldgen_group','ncmm_set_worldgen_string_choices')) {
    if (-not $h2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('case COPT_WORLDGEN_ONLY:','is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) )','options_manager::ncmm_begin_worldgen_group','options_manager::ncmm_set_worldgen_string_choices')) {
    if (-not $c2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $sd2.Contains('ncmm::initialize();')) { throw 'Post-check failed: ncmm::initialize' }
foreach ($needle in @('ncmm::settings_menu_label()','ncmm::show_manager();','ncmm::on_language_changed();','ncmm::register_gameplay_actions( ctxt_default );')) {
    if (-not $mm2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $dt2.Contains('ncmm::on_turn();')) { throw 'Post-check failed: ncmm::on_turn' }
if (-not $ih2.Contains('ncmm_register_default_action')) { throw 'Post-check failed: input manager NCMM declaration' }
foreach ($needle in @('input_manager::ncmm_register_default_action','alternate_type','portable_name','keyboard_char','keyboard_code')) {
    if (-not $ic2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('ncmm::register_gameplay_actions( ctxt );','ncmm::handle_gameplay_action( action )')) {
    if (-not $ha2.Contains($needle)) { throw "Post-check failed: $needle" }
}

Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.7.1 module contract`n" -Encoding ASCII
Write-Host 'NCMM 0.7.1 host patch applied and UTF-8 preservation verified.'
