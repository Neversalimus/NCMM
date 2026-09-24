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
$marker = Join-Path $SourceRoot '.ncmm_host_v1_patched'

foreach ($f in @($optionsH,$optionsCpp,$sdl,$mainMenu,$doTurn,$inputH,$inputCpp,$handleAction)) {
    if (-not (Test-Path $f)) { throw "Required source file missing: $f" }
}

# Windows PowerShell 5 defaults are unsafe for UTF-8 source files.
# Always decode strictly as UTF-8 and write UTF-8 without BOM.
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
    if (-not $markerText.Contains('NCMM 0.5.1')) {
        throw 'Older NCMM host patch marker detected; clean upstream source required for NCMM 0.5.1.'
    }

    $h = Read-Utf8 $optionsH
    $c = Read-Utf8 $optionsCpp
    $sd = Read-Utf8 $sdl
    $mm = Read-Utf8 $mainMenu
    $dt = Read-Utf8 $doTurn
    $ih = Read-Utf8 $inputH
    $ic = Read-Utf8 $inputCpp
    $ha = Read-Utf8 $handleAction
    $checks = @(
        @($h,'COPT_WORLDGEN_ONLY'),
        @($h,'ncmm_can_expose_worldgen_option'),
        @($h,'ncmm_expose_worldgen_option'),
        @($c,'case COPT_WORLDGEN_ONLY:'),
        @($c,'is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) )'),
        @($c,'addOptionToPage( name, "world_default" )'),
        @($sd,'ncmm::initialize();'),
        @($mm,'ncmm::settings_menu_label()'),
        @($mm,'ncmm::show_manager();'),
        @($mm,'ncmm::on_language_changed();'),
        @($dt,'ncmm::on_turn();'),
        @($ih,'ncmm_register_default_action'),
        @($ic,'input_manager::ncmm_register_default_action'),
        @($ha,'ncmm::register_gameplay_actions( ctxt );'),
        @($ha,'ncmm::handle_gameplay_action( action )')
    )
    foreach ($x in $checks) {
        if (-not $x[0].Contains($x[1])) {
            throw "Existing NCMM marker found but patched contract missing: $($x[1])"
        }
    }
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
    Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force
    Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.5.1 module contract`n" -Encoding ASCII
    Write-Host 'Existing NCMM upstream patch verified; v0.5.1 loader/API refreshed.'
    exit 0
}

$contractScript = Join-Path (Split-Path $PSScriptRoot -Parent) 'ci\Test-SourceContracts.ps1'
& $contractScript -SourceRoot $SourceRoot
if ($LASTEXITCODE -ne 0) { throw 'NCMM source-contract preflight failed.' }

# Capture all original non-ASCII code points.  The NCMM patch below only inserts
# ASCII into upstream files, so these signatures must survive byte-for-byte.
$hOriginal = Read-Utf8 $optionsH
$cOriginal = Read-Utf8 $optionsCpp
$sdOriginal = Read-Utf8 $sdl
$mmOriginal = Read-Utf8 $mainMenu
$dtOriginal = Read-Utf8 $doTurn
$ihOriginal = Read-Utf8 $inputH
$icOriginal = Read-Utf8 $inputCpp
$haOriginal = Read-Utf8 $handleAction
$hSig = NonAscii-Signature $hOriginal
$cSig = NonAscii-Signature $cOriginal
$sdSig = NonAscii-Signature $sdOriginal
$mmSig = NonAscii-Signature $mmOriginal
$dtSig = NonAscii-Signature $dtOriginal
$ihSig = NonAscii-Signature $ihOriginal
$icSig = NonAscii-Signature $icOriginal
$haSig = NonAscii-Signature $haOriginal

$h = Normalize-Lf $hOriginal
$c = Normalize-Lf $cOriginal
$sd = Normalize-Lf $sdOriginal
$mm = Normalize-Lf $mmOriginal
$dt = Normalize-Lf $dtOriginal
$ih = Normalize-Lf $ihOriginal
$ic = Normalize-Lf $icOriginal
$ha = Normalize-Lf $haOriginal

$h = Replace-ExactlyOnce $h @'
            COPT_NO_SOUND_HIDE,
            /** Hide this option always, it should not be changed by user directly through UI. **/
            COPT_ALWAYS_HIDE
'@ @'
            COPT_NO_SOUND_HIDE,
            /** Hidden in normal/in-game options, visible only in the world-generation options UI. */
            COPT_WORLDGEN_ONLY,
            /** Hide this option always, it should not be changed by user directly through UI. **/
            COPT_ALWAYS_HIDE
'@ 'options.hide-enum'

$h = Replace-ExactlyOnce $h '                bool is_hidden() const;' '                bool is_hidden( bool worldgen_context = false ) const;' 'options.is-hidden-signature'
$h = Replace-ExactlyOnce $h '        void set_world_options( options_container *options );' @'
        void set_world_options( options_container *options );

        /** NCMM: expose an existing, permanently-hidden world option only during world generation. */
        bool ncmm_can_expose_worldgen_option( const std::string &name ) const;
        bool ncmm_expose_worldgen_option( const std::string &name, const translation &menu_text,
                                         const translation &tooltip );
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

    // CDDA init-time cleanup physically removes COPT_ALWAYS_HIDE PageItems.
    // Re-add only on first exposure; repeat exposure is safe.
    if( first_exposure ) {
        addOptionToPage( name, "world_default" );
    }
    return true;
}
'@ 'options.ncmm-expose-implementation'

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

        /** NCMM: register a stable default action without overwriting user remaps. */
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

# NCMM 0.5.1 MCM + module-contract host. main_menu.cpp is handled by the same strict UTF-8
# preservation contract as other upstream sources. Every injected byte is ASCII.
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

Write-Utf8 $optionsH $h
Write-Utf8 $optionsCpp $c
Write-Utf8 $sdl $sd
Write-Utf8 $mainMenu $mm
Write-Utf8 $doTurn $dt
Write-Utf8 $inputH $ih
Write-Utf8 $inputCpp $ic
Write-Utf8 $handleAction $ha

Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force

$h2 = Read-Utf8 $optionsH
$c2 = Read-Utf8 $optionsCpp
$sd2 = Read-Utf8 $sdl
$mm2 = Read-Utf8 $mainMenu
$dt2 = Read-Utf8 $doTurn
$ih2 = Read-Utf8 $inputH
$ic2 = Read-Utf8 $inputCpp
$ha2 = Read-Utf8 $handleAction

if ((NonAscii-Signature $h2) -ne $hSig) { throw 'UTF-8 preservation check failed for options.h' }
if ((NonAscii-Signature $c2) -ne $cSig) { throw 'UTF-8 preservation check failed for options.cpp' }
if ((NonAscii-Signature $sd2) -ne $sdSig) { throw 'UTF-8 preservation check failed for sdltiles.cpp' }
if ((NonAscii-Signature $mm2) -ne $mmSig) { throw 'UTF-8 preservation check failed for main_menu.cpp' }
if ((NonAscii-Signature $dt2) -ne $dtSig) { throw 'UTF-8 preservation check failed for do_turn.cpp' }
if ((NonAscii-Signature $ih2) -ne $ihSig) { throw 'UTF-8 preservation check failed for input.h' }
if ((NonAscii-Signature $ic2) -ne $icSig) { throw 'UTF-8 preservation check failed for input.cpp' }
if ((NonAscii-Signature $ha2) -ne $haSig) { throw 'UTF-8 preservation check failed for handle_action.cpp' }

foreach ($needle in @('COPT_WORLDGEN_ONLY','ncmm_can_expose_worldgen_option','ncmm_expose_worldgen_option')) {
    if (-not $h2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('case COPT_WORLDGEN_ONLY:','is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) )','addOptionToPage( name, "world_default" )','first_exposure')) {
    if (-not $c2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $sd2.Contains('ncmm::initialize();')) { throw 'Post-check failed: ncmm::initialize' }
foreach ($needle in @('ncmm::settings_menu_label()','ncmm::show_manager();','ncmm::on_language_changed();')) {
    if (-not $mm2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $dt2.Contains('ncmm::on_turn();')) { throw 'Post-check failed: ncmm::on_turn' }
if (-not $ih2.Contains('ncmm_register_default_action')) { throw 'Post-check failed: input manager NCMM declaration' }
if (-not $ic2.Contains('input_manager::ncmm_register_default_action')) { throw 'Post-check failed: input manager NCMM implementation' }
foreach ($needle in @('ncmm::register_gameplay_actions( ctxt );','ncmm::handle_gameplay_action( action )')) {
    if (-not $ha2.Contains($needle)) { throw "Post-check failed: $needle" }
}

Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.5.1 module contract`n" -Encoding ASCII
Write-Host 'NCMM 0.5.1 host patch applied and UTF-8 preservation verified.'
