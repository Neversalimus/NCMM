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
$marker = Join-Path $SourceRoot '.ncmm_host_v1_patched'

foreach ($f in @($optionsH,$optionsCpp,$sdl,$mainMenu)) {
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
    $h = Read-Utf8 $optionsH
    $c = Read-Utf8 $optionsCpp
    $sd = Read-Utf8 $sdl
    $mm = Read-Utf8 $mainMenu
    $checks = @(
        @($h,'COPT_WORLDGEN_ONLY'),
        @($h,'ncmm_can_expose_worldgen_option'),
        @($h,'ncmm_expose_worldgen_option'),
        @($c,'case COPT_WORLDGEN_ONLY:'),
        @($c,'it.data ).is_hidden( world_options_only )'),
        @($c,'curr_item.data ).is_hidden( world_options_only )'),
        @($c,'addOptionToPage( name, "world_default" )'),
        @($sd,'ncmm::initialize();'),
        @($mm,'ncmm::settings_menu_label()'),
        @($mm,'ncmm::show_manager();'),
        @($mm,'ncmm::on_language_changed();')
    )
    foreach ($x in $checks) {
        if (-not $x[0].Contains($x[1])) {
            throw "Existing NCMM marker found but patched contract missing: $($x[1])"
        }
    }
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
    Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force
    Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.4.1 module contract`n" -Encoding ASCII
    Write-Host 'Existing NCMM upstream patch verified; v0.4.0 loader/API refreshed.'
    exit 0
}

# Capture all original non-ASCII code points.  The NCMM patch below only inserts
# ASCII into upstream files, so these signatures must survive byte-for-byte.
$hOriginal = Read-Utf8 $optionsH
$cOriginal = Read-Utf8 $optionsCpp
$sdOriginal = Read-Utf8 $sdl
$mmOriginal = Read-Utf8 $mainMenu
$hSig = NonAscii-Signature $hOriginal
$cSig = NonAscii-Signature $cOriginal
$sdSig = NonAscii-Signature $sdOriginal
$mmSig = NonAscii-Signature $mmOriginal

$h = Normalize-Lf $hOriginal
$c = Normalize-Lf $cOriginal
$sd = Normalize-Lf $sdOriginal
$mm = Normalize-Lf $mmOriginal

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
$c = Replace-ExactlyOnce $c '                    && !get_options().get_option( it.data ).is_hidden();' '                    && !get_options().get_option( it.data ).is_hidden( world_options_only );' 'options.worldgen-visibility'
$c = Replace-ExactlyOnce $c '                    && !get_options().get_option( curr_item.data ).is_hidden();' '                    && !get_options().get_option( curr_item.data ).is_hidden( world_options_only );' 'options.worldgen-selectability'
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

# NCMM 0.4.1 MCM + module-contract host. main_menu.cpp is handled by the same strict UTF-8
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

Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force

$h2 = Read-Utf8 $optionsH
$c2 = Read-Utf8 $optionsCpp
$sd2 = Read-Utf8 $sdl
$mm2 = Read-Utf8 $mainMenu

if ((NonAscii-Signature $h2) -ne $hSig) { throw 'UTF-8 preservation check failed for options.h' }
if ((NonAscii-Signature $c2) -ne $cSig) { throw 'UTF-8 preservation check failed for options.cpp' }
if ((NonAscii-Signature $sd2) -ne $sdSig) { throw 'UTF-8 preservation check failed for sdltiles.cpp' }
if ((NonAscii-Signature $mm2) -ne $mmSig) { throw 'UTF-8 preservation check failed for main_menu.cpp' }

foreach ($needle in @('COPT_WORLDGEN_ONLY','ncmm_can_expose_worldgen_option','ncmm_expose_worldgen_option')) {
    if (-not $h2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('case COPT_WORLDGEN_ONLY:','it.data ).is_hidden( world_options_only )','curr_item.data ).is_hidden( world_options_only )','addOptionToPage( name, "world_default" )','first_exposure')) {
    if (-not $c2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $sd2.Contains('ncmm::initialize();')) { throw 'Post-check failed: ncmm::initialize' }
foreach ($needle in @('ncmm::settings_menu_label()','ncmm::show_manager();','ncmm::on_language_changed();')) {
    if (-not $mm2.Contains($needle)) { throw "Post-check failed: $needle" }
}

Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.4.1 module contract`n" -Encoding ASCII
Write-Host 'NCMM 0.4.1 host patch applied and UTF-8 preservation verified.'
