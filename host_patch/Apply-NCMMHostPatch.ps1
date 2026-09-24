param(
    [Parameter(Mandatory=$true)][string]$SourceRoot
)
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$src = Join-Path $SourceRoot 'src'
$optionsH = Join-Path $src 'options.h'
$optionsCpp = Join-Path $src 'options.cpp'
$sdl = Join-Path $src 'sdltiles.cpp'
$marker = Join-Path $SourceRoot '.ncmm_host_v1_patched'
foreach ($f in @($optionsH,$optionsCpp,$sdl)) { if (-not (Test-Path $f)) { throw "Required source file missing: $f" } }

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

if (Test-Path $marker) {
    $h = Get-Content $optionsH -Raw
    $c = Get-Content $optionsCpp -Raw
    $sd = Get-Content $sdl -Raw
    $checks = @(
        @($h,'COPT_WORLDGEN_ONLY'),
        @($h,'ncmm_can_expose_worldgen_option'),
        @($h,'ncmm_expose_worldgen_option'),
        @($c,'case COPT_WORLDGEN_ONLY:'),
        @($c,'it.data ).is_hidden( world_options_only )'),
        @($c,'curr_item.data ).is_hidden( world_options_only )'),
        @($c,'addOptionToPage( name, "world_default" )'),
        @($sd,'ncmm::initialize();')
    )
    foreach ($x in $checks) { if (-not $x[0].Contains($x[1])) { throw "Existing NCMM marker found but patched contract missing: $($x[1])" } }
    Write-Host 'NCMM Host v1 patch already present and verified.'
    exit 0
}

$h = Normalize-Lf (Get-Content $optionsH -Raw)
$c = Normalize-Lf (Get-Content $optionsCpp -Raw)
$sd = Normalize-Lf (Get-Content $sdl -Raw)

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
           it->second.hide == COPT_ALWAYS_HIDE;
}

bool options_manager::ncmm_expose_worldgen_option( const std::string &name,
        const translation &menu_text, const translation &tooltip )
{
    if( !ncmm_can_expose_worldgen_option( name ) ) {
        return false;
    }
    cOpt &opt = options.find( name )->second;
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;

    // CDDA init-time cleanup physically removes COPT_ALWAYS_HIDE PageItems.
    // Re-add the option after NCMM exposes it so world-generation UI can render it.
    addOptionToPage( name, "world_default" );
    return true;
}
'@ 'options.ncmm-expose-implementation'

$sd = Replace-ExactlyOnce $sd '#include "options.h"' "#include ""options.h""`n#include ""ncmm_loader.h""" 'sdl.include-ncmm'
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

Set-Content -Path $optionsH -Value $h -NoNewline -Encoding UTF8
Set-Content -Path $optionsCpp -Value $c -NoNewline -Encoding UTF8
Set-Content -Path $sdl -Value $sd -NoNewline -Encoding UTF8
Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path (Split-Path $PSScriptRoot -Parent) 'sdk\ncmm_api.h') (Join-Path $src 'ncmm_api.h') -Force

$h2 = Get-Content $optionsH -Raw
$c2 = Get-Content $optionsCpp -Raw
$sd2 = Get-Content $sdl -Raw
foreach ($needle in @('COPT_WORLDGEN_ONLY','ncmm_can_expose_worldgen_option','ncmm_expose_worldgen_option')) { if (-not $h2.Contains($needle)) { throw "Post-check failed: $needle" } }
foreach ($needle in @('case COPT_WORLDGEN_ONLY:','it.data ).is_hidden( world_options_only )','curr_item.data ).is_hidden( world_options_only )','options_manager::ncmm_can_expose_worldgen_option','options_manager::ncmm_expose_worldgen_option','addOptionToPage( name, "world_default" )')) { if (-not $c2.Contains($needle)) { throw "Post-check failed: $needle" } }
if (-not $sd2.Contains('ncmm::initialize();')) { throw 'Post-check failed: ncmm::initialize' }
Set-Content -Path $marker -Value "NCMM Host API v1`n" -Encoding ASCII
Write-Host 'NCMM Host API v1 patch applied and post-verified.'
