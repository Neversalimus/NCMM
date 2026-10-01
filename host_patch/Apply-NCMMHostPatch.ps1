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
$rangedCpp = Join-Path $src 'ranged.cpp'
$dispersionH = Join-Path $src 'dispersion.h'
$dispersionCpp = Join-Path $src 'dispersion.cpp'
$inventoryUiH = Join-Path $src 'inventory_ui.h'
$inventoryUiCpp = Join-Path $src 'inventory_ui.cpp'
$gameInventoryCpp = Join-Path $src 'game_inventory.cpp'
$advancedInvCpp = Join-Path $src 'advanced_inv.cpp'
$marker = Join-Path $SourceRoot '.ncmm_host_v1_patched'

foreach ($f in @($optionsH,$optionsCpp,$sdl,$mainMenu,$doTurn,$inputH,$inputCpp,$handleAction,
                  $characterCpp,$characterHealthCpp,$meleeCpp,$knowledgeCpp,$craftingCpp,
                  $rangedCpp,$dispersionH,$dispersionCpp,$inventoryUiH,$inventoryUiCpp,
                  $gameInventoryCpp,$advancedInvCpp)) {
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
    if (-not $markerText.Contains('NCMM 0.8.2')) {
        throw 'Older NCMM host patch marker detected; clean upstream source required for NCMM 0.8.2.'
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
    $rg = Read-Utf8 $rangedCpp
    $dh = Read-Utf8 $dispersionH
    $dc = Read-Utf8 $dispersionCpp
    $iuh = Read-Utf8 $inventoryUiH
    $iuc = Read-Utf8 $inventoryUiCpp
    $gic = Read-Utf8 $gameInventoryCpp
    $aic = Read-Utf8 $advancedInvCpp
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
        @($mm,'ncmm::version_label()'),
        @($mm,'ncmm::gameplay_smoke_requested()'),
        @($mm,'ncmm::run_gameplay_smoke()'),
        @($mm,'ncmm::show_manager();'),
        @($mm,'ncmm::on_language_changed();'),
        @($mm,'ncmm::register_gameplay_actions( ctxt_default );'),
        @($dt,'ncmm::on_turn();'),
        @($ih,'ncmm_register_default_action'),
        @($ih,'ncmm_register_context_default_action'),
        @($ic,'alternate_type'),
        @($ha,'ncmm::register_gameplay_actions( ctxt );'),
        @($ha,'ncmm::handle_gameplay_action( action )'),
        @($ch,'ncmm::gameplay_modifier( "str_flat" )'),
        @($ch,'ncmm::gameplay_modifier( "speed_pct" )'),
        @($hh,'ncmm::gameplay_modifier( "stamina_max_pct" )'),
        @($me,'ncmm::gameplay_modifier( "dodge_flat" )'),
        @($kn,'ncmm::gameplay_modifier( "read_speed_pct" )'),
        @($cr,'ncmm::gameplay_modifier( "craft_speed_pct" )'),
        @($rg,'targeting.hit_probability.enabled'),
        @($rg,'exact_hit_probability'),
        @($dh,'probability_below'),
        @($dc,'dispersion_sources::probability_below'),
        @($iuh,'set_equipment_body_map'),
        @($iuc,'inventory.body_map.enabled'),
        @($iuc,'draw_equipment_body_map'),
        @($iuc,'Explicit human-shaped paper doll.'),
        @($iuc,'NCMM_BODY_MAP_FOCUS'),
        @($iuc,'inventory.body_map.right_arrow_gear'),
        @($iuc,'res.action == "ANY_INPUT" && ch == KEY_RIGHT'),
        @($gic,'set_equipment_body_map();'),
        @($iuc,'ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) )'),
        @($iuc,'ncmm::inventory_item_symbol( *entry.any_item() )'),
        @($aic,'#include "ncmm_loader.h"'),
        @($aic,'ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) )'),
        @($aic,'ncmm::inventory_item_symbol( it )')
    )
    foreach ($x in $checks) {
        if (-not $x[0].Contains($x[1])) {
            throw "Existing NCMM marker found but patched contract missing: $($x[1])"
        }
    }
    foreach ($pair in @(@($iuc,2),@($aic,1))) {
        $count = ([regex]::Matches($pair[0],[regex]::Escape('ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) )'))).Count
        if ($count -ne $pair[1]) { throw 'Existing NCMM marker has invalid Item Glyphs symbol-slot gates.' }
    }
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
    Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_fault_policy.h') (Join-Path $src 'ncmm_fault_policy.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_manifest_policy.h') (Join-Path $src 'ncmm_manifest_policy.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_item_glyphs.h') (Join-Path $src 'ncmm_item_glyphs.h') -Force
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

    Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.8.2 module contract`n" -Encoding ASCII
    Write-Host 'Existing NCMM upstream patch verified; v0.8.2 loader/API refreshed.'
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
$rgOriginal = Read-Utf8 $rangedCpp
$dhOriginal = Read-Utf8 $dispersionH
$dcOriginal = Read-Utf8 $dispersionCpp
$iuhOriginal = Read-Utf8 $inventoryUiH
$iucOriginal = Read-Utf8 $inventoryUiCpp
$gicOriginal = Read-Utf8 $gameInventoryCpp
$aicOriginal = Read-Utf8 $advancedInvCpp
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
$rgSig = NonAscii-Signature $rgOriginal
$dhSig = NonAscii-Signature $dhOriginal
$dcSig = NonAscii-Signature $dcOriginal
$iuhSig = NonAscii-Signature $iuhOriginal
$iucSig = NonAscii-Signature $iucOriginal
$gicSig = NonAscii-Signature $gicOriginal
$aicSig = NonAscii-Signature $aicOriginal

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
$rg = Normalize-Lf $rgOriginal
$dh = Normalize-Lf $dhOriginal
$dc = Normalize-Lf $dcOriginal
$iuh = Normalize-Lf $iuhOriginal
$iuc = Normalize-Lf $iucOriginal
$gic = Normalize-Lf $gicOriginal
$aic = Normalize-Lf $aicOriginal


# Item Glyphs reuses the existing two-cell symbol slot in both inventory UIs.
$iuc = Replace-ExactlyOnce $iuc @'
    if( get_option<bool>( "ITEM_SYMBOLS" ) ) {
        res += 2;
    }
'@ @'
    if( ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) ) ) {
        res += 2;
    }
'@ 'inventory.item-glyphs-indent'

$iuc = Replace-ExactlyOnce $iuc @'
            if( get_option<bool>( "ITEM_SYMBOLS" ) ) {
                const nc_color color = entry.any_item()->color();
                mvwputch( win, point( xx, yy ), color, entry.any_item()->symbol() );
                xx += 2;
            }
'@ @'
            if( ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) ) ) {
                const nc_color color = entry.any_item()->color();
                mvwputch( win, point( xx, yy ), color, ncmm::inventory_item_symbol( *entry.any_item() ) );
                xx += 2;
            }
'@ 'inventory.item-glyphs-draw'

$aic = Replace-ExactlyOnce $aic @'
#include "advanced_inv.h"
'@ @'
#include "advanced_inv.h"
#include "ncmm_loader.h"
'@ 'advanced-inventory.item-glyphs-include'

$aic = Replace-ExactlyOnce $aic @'
        if( get_option<bool>( "ITEM_SYMBOLS" ) ) {
            item_name = string_format( "%s %s", it.symbol(), item_name );
        }
'@ @'
        if( ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) ) ) {
            item_name = string_format( "%s %s", ncmm::inventory_item_symbol( it ), item_name );
        }
'@ 'advanced-inventory.item-glyphs-prefix'

# Equipment Body Map: opt-in normal-inventory panel driven by generic runtime hooks.
$iuh = Replace-ExactlyOnce $iuh @'
        /** Specify whether the header should show stats (weight and volume). */
        void set_display_stats( bool display_stats ) {
            this->display_stats = display_stats;
        }
'@ @'
        /** Specify whether the header should show stats (weight and volume). */
        void set_display_stats( bool display_stats ) {
            this->display_stats = display_stats;
        }
        /** Enable the optional equipment body-map panel for this selector. */
        void set_equipment_body_map( bool enabled = true ) {
            equipment_body_map = enabled;
        }
'@ 'inventory.body-map-public-opt-in'

$iuh = Replace-ExactlyOnce $iuh @'
        void draw_header( const catacurses::window &w ) const;
        void draw_footer( const catacurses::window &w ) const;
        void draw_columns( const catacurses::window &w );
        void draw_frame( const catacurses::window &w ) const;
'@ @'
        void draw_header( const catacurses::window &w ) const;
        void draw_footer( const catacurses::window &w ) const;
        void draw_columns( const catacurses::window &w );
        void draw_frame( const catacurses::window &w ) const;
        void draw_equipment_body_map( const catacurses::window &w ) const;
        bool equipment_body_map_requested() const;
'@ 'inventory.body-map-private-methods'

$iuh = Replace-ExactlyOnce $iuh @'
        bool is_empty = true;
        bool display_stats = true;
        bool use_invlet = true;
'@ @'
        bool is_empty = true;
        bool display_stats = true;
        bool use_invlet = true;
        bool equipment_body_map = false;
        size_t equipment_body_map_reserved_height = 0;
        int equipment_body_map_focus = -1;
'@ 'inventory.body-map-state'

$iuc = Replace-ExactlyOnce $iuc '#include "basecamp.h"' @'
#include "basecamp.h"
#include "bodypart.h"
'@ 'inventory.body-map-includes-a'

$iuc = Replace-ExactlyOnce $iuc '#include "messages.h"' @'
#include "messages.h"
#include "ncmm_loader.h"
'@ 'inventory.body-map-includes-b'

$iuc = Replace-ExactlyOnce $iuc @'
void inventory_selector::prepare_layout( size_t client_width, size_t client_height )
{
    // This block adds categories and should go before any width evaluations
    const bool initial = get_active_column().get_highlighted_index() == static_cast<size_t>( -1 );
'@ @'
bool inventory_selector::equipment_body_map_requested() const
{
    return equipment_body_map &&
           ncmm::runtime_setting_hook_bound( "inventory.body_map.enabled" ) &&
           ncmm::runtime_setting_hook_bool( "inventory.body_map.enabled", 1 ) != 0;
}

void inventory_selector::prepare_layout( size_t client_width, size_t client_height )
{
    constexpr size_t full_body_map_height = 17;
    constexpr size_t compact_body_map_height = 11;
    constexpr size_t min_worn_list_height = 7;

    equipment_body_map_reserved_height = 0;
    if( equipment_body_map_requested() && !own_gear_column.empty() ) {
        if( client_height >= full_body_map_height + min_worn_list_height ) {
            equipment_body_map_reserved_height = full_body_map_height;
        } else if( client_height >= compact_body_map_height + min_worn_list_height ) {
            equipment_body_map_reserved_height = compact_body_map_height;
        }
    }

    // This block adds categories and should go before any width evaluations
    const bool initial = get_active_column().get_highlighted_index() == static_cast<size_t>( -1 );
'@ 'inventory.body-map-layout-reserve'

$iuc = Replace-ExactlyOnce $iuc @'
    for( inventory_column *&elem : columns ) {
        elem->set_height( client_height );
        elem->prepare_paging( filter );
        elem->reset_width( columns );
    }
'@ @'
    for( inventory_column *&elem : columns ) {
        const size_t column_height =
            elem == &own_gear_column && equipment_body_map_reserved_height > 0 ?
            client_height - equipment_body_map_reserved_height : client_height;
        elem->set_height( column_height );
        elem->prepare_paging( filter );
        elem->reset_width( columns );
    }
'@ 'inventory.body-map-worn-column-height'
$iuc = Replace-ExactlyOnce $iuc @'
    // Handle screen overflow
    rearrange_columns( client_width );
'@ @'
    // Keep vanilla horizontal layout.  The paper doll consumes vertical space
    // only inside the existing worn-items column.
    rearrange_columns( client_width );
    if( !own_gear_column.visible() || own_gear_column.get_width() < 20 ) {
        equipment_body_map_reserved_height = 0;
    }
'@ 'inventory.body-map-rearrange-width'
$iuc = Replace-ExactlyOnce $iuc @'
    draw_frame( w_inv );
    draw_header( w_inv );
    draw_columns( w_inv );
    draw_footer( w_inv );
'@ @'
    draw_frame( w_inv );
    draw_header( w_inv );
    draw_columns( w_inv );
    draw_equipment_body_map( w_inv );
    draw_footer( w_inv );
'@ 'inventory.body-map-refresh'

$iuc = Replace-ExactlyOnce $iuc @'
    inventory_input res{ action, ch, nullptr };

    if( res.action == "SELECT" || res.action == "COORDINATE" || res.action == "MOUSE_MOVE" ||
'@ @'
    inventory_input res{ action, ch, nullptr };

    // EBM compatibility alias: only consume Right Arrow when CDDA did not resolve
    // it to any real action.  Restrict it to the normal inventory's main column,
    // so user remaps, multiselect/drop menus, map columns and gear-column input
    // always keep their native behavior.
    if( res.action == "ANY_INPUT" && ch == KEY_RIGHT &&
        equipment_body_map_requested() && &get_active_column() == &own_inv_column &&
        own_gear_column.visible() && own_gear_column.activatable() &&
        ncmm::runtime_setting_hook_bound( "inventory.body_map.right_arrow_gear" ) &&
        ncmm::runtime_setting_hook_bool( "inventory.body_map.right_arrow_gear", 1 ) != 0 ) {
        res.action = "PREV_COLUMN";
    }

    if( res.action == "SELECT" || res.action == "COORDINATE" || res.action == "MOUSE_MOVE" ||
'@ 'inventory.body-map-safe-right-arrow-alias'

$iuc = Replace-ExactlyOnce $iuc @'
            if( window_contains_point_relative( w_inv, p ) ) {
                res.entry = find_entry_by_coordinate( p );
'@ @'
            if( window_contains_point_relative( w_inv, p ) ) {
                if( equipment_body_map_reserved_height > 0 && equipment_body_map_requested() &&
                    own_gear_column.visible() &&
                    ( res.action == "SELECT" || res.action == "MOUSE_MOVE" ) ) {
                    const auto visible_columns = get_visible_columns();
                    const int screen_width = getmaxx( w_inv ) - 2 * ( border + 1 );
                    const bool centered = are_columns_centered( screen_width );
                    const int free_space = screen_width - get_columns_width( visible_columns );
                    const int max_gap = visible_columns.size() > 1 ?
                                        free_space / static_cast<int>( visible_columns.size() - 1 ) :
                                        free_space;
                    const int gap = centered ? max_gap : std::min<int>( max_gap, normal_column_gap );
                    const int gap_rounding_error = centered && visible_columns.size() > 1 ?
                                                   free_space % static_cast<int>( visible_columns.size() - 1 ) : 0;
                    int panel_x = border + 1;
                    bool found_worn_column = false;
                    for( size_t i = 0; i < visible_columns.size(); ++i ) {
                        inventory_column *elem = visible_columns[i];
                        if( i + 1 == visible_columns.size() ) {
                            panel_x += gap_rounding_error;
                        }
                        if( elem == &own_gear_column ) {
                            found_worn_column = true;
                            break;
                        }
                        panel_x += static_cast<int>( elem->get_width() ) + gap;
                    }
                    if( found_worn_column ) {
                        const int panel_width = static_cast<int>( own_gear_column.get_width() );
                        const int content_x = panel_x + 1;
                        const int content_width = std::max( 1, panel_width - 2 );
                        const int panel_top = getmaxy( w_inv ) - border -
                                              static_cast<int>( equipment_body_map_reserved_height );
                        const bool compact = equipment_body_map_reserved_height <= 11 || panel_width < 38;
                        const int third = std::max( 6, content_width / 3 );
                        const int left_x = content_x;
                        const int center_x = content_x + ( content_width - third ) / 2;
                        const int right_x = content_x + content_width - third;
                        const auto inside = [&]( int x, int width ) {
                            return p.x >= x && p.x < x + width;
                        };

                        int focus = -1;
                        int row = panel_top + 2;
                        if( p.y == row && inside( center_x, third ) ) {
                            focus = 0;
                        }
                        ++row;
                        if( !compact ) {
                            if( p.y == row && inside( center_x, third ) ) {
                                focus = 1;
                            }
                            ++row;
                            if( p.y == row && inside( center_x, third ) ) {
                                focus = 2;
                            }
                            ++row;
                        } else {
                            const int half = std::max( 5, content_width / 2 );
                            if( p.y == row ) {
                                if( inside( content_x, half ) ) {
                                    focus = 1;
                                } else if( inside( content_x + content_width - half, half ) ) {
                                    focus = 2;
                                }
                            }
                            ++row;
                        }
                        if( p.y == row ) {
                            if( inside( left_x, third ) ) {
                                focus = 4;
                            } else if( inside( center_x, third ) ) {
                                focus = 3;
                            } else if( inside( right_x, third ) ) {
                                focus = 5;
                            }
                        }
                        ++row;
                        if( p.y == row ) {
                            if( inside( left_x, third ) ) {
                                focus = 6;
                            } else if( inside( right_x, third ) ) {
                                focus = 7;
                            }
                        }
                        ++row;
                        if( p.y == row ) {
                            if( inside( left_x, third ) ) {
                                focus = 8;
                            } else if( inside( right_x, third ) ) {
                                focus = 9;
                            }
                        }
                        ++row;
                        if( p.y == row ) {
                            if( inside( left_x, third ) ) {
                                focus = 10;
                            } else if( inside( right_x, third ) ) {
                                focus = 11;
                            }
                        }

                        if( focus >= 0 ) {
                            equipment_body_map_focus = focus;
                            res.action = "NCMM_BODY_MAP_FOCUS";
                            res.entry = nullptr;
                            return res;
                        }
                    }
                }
                res.entry = find_entry_by_coordinate( p );
'@ 'inventory.body-map-hit-test'

$iuc = Replace-ExactlyOnce $iuc @'
void inventory_selector::draw_frame( const catacurses::window &w ) const
{
    draw_border( w );

    const int y = border + get_header_height();
    wattron( w, BORDER_COLOR );
    mvwhline( w, point( 0, y ), LINE_XXXO, 1 );
    mvwhline( w, point( getmaxx( w ) - border, y ), LINE_XOXX, 1 );
    wattroff( w, BORDER_COLOR );
}
'@ @'
void inventory_selector::draw_frame( const catacurses::window &w ) const
{
    draw_border( w );

    const int y = border + get_header_height();
    wattron( w, BORDER_COLOR );
    mvwhline( w, point( 0, y ), LINE_XXXO, 1 );
    mvwhline( w, point( getmaxx( w ) - border, y ), LINE_XOXX, 1 );
    wattroff( w, BORDER_COLOR );
}

void inventory_selector::draw_equipment_body_map( const catacurses::window &w ) const
{
    if( equipment_body_map_reserved_height == 0 || !equipment_body_map_requested() ||
        !own_gear_column.visible() ) {
        return;
    }

    const auto visible_columns = get_visible_columns();
    const int screen_width = getmaxx( w ) - 2 * ( border + 1 );
    const bool centered = are_columns_centered( screen_width );
    const int free_space = screen_width - get_columns_width( visible_columns );
    const int max_gap = visible_columns.size() > 1 ?
                        free_space / static_cast<int>( visible_columns.size() - 1 ) :
                        free_space;
    const int gap = centered ? max_gap : std::min<int>( max_gap, normal_column_gap );
    const int gap_rounding_error = centered && visible_columns.size() > 1 ?
                                   free_space % static_cast<int>( visible_columns.size() - 1 ) : 0;

    int panel_x = border + 1;
    bool found_worn_column = false;
    for( size_t i = 0; i < visible_columns.size(); ++i ) {
        inventory_column *elem = visible_columns[i];
        if( i + 1 == visible_columns.size() ) {
            panel_x += gap_rounding_error;
        }
        if( elem == &own_gear_column ) {
            found_worn_column = true;
            break;
        }
        panel_x += static_cast<int>( elem->get_width() ) + gap;
    }
    if( !found_worn_column ) {
        return;
    }

    const int panel_width = static_cast<int>( own_gear_column.get_width() );
    if( panel_width < 20 ) {
        return;
    }

    const int footer_y = getmaxy( w ) - border;
    const int panel_top = footer_y - static_cast<int>( equipment_body_map_reserved_height );
    const int content_x = panel_x + 1;
    const int content_width = std::max( 1, panel_width - 2 );
    const bool compact = equipment_body_map_reserved_height <= 11 || panel_width < 38;

    mvwhline( w, point( panel_x, panel_top ), c_dark_gray, LINE_OXOX, panel_width );

    const std::string heading =
        ncmm::localized_text( "EQUIPMENT", u8"\u042D\u041A\u0418\u041F\u0418\u0420\u041E\u0412\u041A\u0410" );
    const int heading_x = content_x +
                          std::max( 0, ( content_width - utf8_width( heading, true ) ) / 2 );
    int y = panel_top + 1;
    trim_and_print( w, point( heading_x, y++ ), content_width, c_light_cyan, heading );

    const inventory_entry &highlighted = get_highlighted();
    const item *selected = highlighted.is_item() ? highlighted.any_item().get_item() : nullptr;
    const std::vector<item_location> worn_items = u.worn.top_items_loc( u );

    const auto worn_count = [&]( const bodypart_str_id & part ) {
        const bodypart_id bp = part.id();
        int count = 0;
        for( const item_location &loc : worn_items ) {
            if( loc && loc->covers( bp ) ) {
                ++count;
            }
        }
        return count;
    };
    const auto selected_covers = [&]( const bodypart_str_id & part ) {
        return selected != nullptr && selected->is_armor() && selected->covers( part.id() );
    };
    const auto zone_color = [&]( int zone_index, const bodypart_str_id & part ) {
        if( equipment_body_map_focus == zone_index ) {
            return c_light_green;
        }
        if( selected_covers( part ) ) {
            return c_yellow;
        }
        const int count = worn_count( part );
        if( count >= 3 ) {
            return c_cyan;
        }
        if( count == 2 ) {
            return c_light_blue;
        }
        if( count == 1 ) {
            return c_light_gray;
        }
        return c_dark_gray;
    };
    const auto zone_text = [&]( const bodypart_str_id & part,
                                const std::string & label, int width ) {
        const std::string count = std::to_string( worn_count( part ) );
        const int room = std::max( 1, width - utf8_width( count, true ) - 2 );
        return trim_by_length( label, room ) + "[" + count + "]";
    };
    const auto print_zone = [&]( int zone_index, int x, int row, int width,
                                 const bodypart_str_id & part,
                                 const std::string & label ) {
        if( row >= footer_y || width <= 0 ) {
            return;
        }
        trim_and_print( w, point( x, row ), width, zone_color( zone_index, part ),
                        zone_text( part, label, width ) );
    };

    const std::string head = ncmm::localized_text( "HEAD", u8"\u0413\u041E\u041B\u041E\u0412\u0410" );
    const std::string eyes = ncmm::localized_text( "EYES", u8"\u0413\u041B\u0410\u0417\u0410" );
    const std::string mouth = ncmm::localized_text( "MOUTH", u8"\u0420\u041E\u0422" );
    const std::string torso = ncmm::localized_text( "TORSO", u8"\u0422\u041E\u0420\u0421" );
    const std::string arm_l = ncmm::localized_text( "L ARM", u8"\u041B.\u0420\u0423\u041A\u0410" );
    const std::string arm_r = ncmm::localized_text( "R ARM", u8"\u041F.\u0420\u0423\u041A\u0410" );
    const std::string hand_l = ncmm::localized_text( "L HAND", u8"\u041B.\u041A\u0418\u0421\u0422\u042C" );
    const std::string hand_r = ncmm::localized_text( "R HAND", u8"\u041F.\u041A\u0418\u0421\u0422\u042C" );
    const std::string leg_l = ncmm::localized_text( "L LEG", u8"\u041B.\u041D\u041E\u0413\u0410" );
    const std::string leg_r = ncmm::localized_text( "R LEG", u8"\u041F.\u041D\u041E\u0413\u0410" );
    const std::string foot_l = ncmm::localized_text( "L FOOT", u8"\u041B.\u0421\u0422\u041E\u041F\u0410" );
    const std::string foot_r = ncmm::localized_text( "R FOOT", u8"\u041F.\u0421\u0422\u041E\u041F\u0410" );

    const int third = std::max( 6, content_width / 3 );
    const int left_x = content_x;
    const int center_x = content_x + ( content_width - third ) / 2;
    const int right_x = content_x + content_width - third;

    // Explicit human-shaped paper doll.  Counts are the number of worn top-level
    // items that actually cover each CDDA body part.  A highlighted armor item
    // paints every body part it covers yellow.
    print_zone( 0, center_x, y++, third, body_part_head, head );
    if( !compact ) {
        print_zone( 1, center_x, y++, third, body_part_eyes, eyes );
        print_zone( 2, center_x, y++, third, body_part_mouth, mouth );
    } else {
        const int half = std::max( 5, content_width / 2 );
        print_zone( 1, content_x, y, half, body_part_eyes, eyes );
        print_zone( 2, content_x + content_width - half, y++, half, body_part_mouth, mouth );
    }

    print_zone( 4, left_x, y, third, body_part_arm_l, arm_l );
    print_zone( 3, center_x, y, third, body_part_torso, torso );
    print_zone( 5, right_x, y++, third, body_part_arm_r, arm_r );

    print_zone( 6, left_x, y, third, body_part_hand_l, hand_l );
    print_zone( 7, right_x, y++, third, body_part_hand_r, hand_r );

    print_zone( 8, left_x, y, third, body_part_leg_l, leg_l );
    print_zone( 9, right_x, y++, third, body_part_leg_r, leg_r );

    print_zone( 10, left_x, y, third, body_part_foot_l, foot_l );
    print_zone( 11, right_x, y++, third, body_part_foot_r, foot_r );

    if( equipment_body_map_focus >= 0 && y < footer_y ) {
        const bodypart_str_id *focused_part = nullptr;
        const std::string *focused_label = nullptr;
        switch( equipment_body_map_focus ) {
            case 0: focused_part = &body_part_head; focused_label = &head; break;
            case 1: focused_part = &body_part_eyes; focused_label = &eyes; break;
            case 2: focused_part = &body_part_mouth; focused_label = &mouth; break;
            case 3: focused_part = &body_part_torso; focused_label = &torso; break;
            case 4: focused_part = &body_part_arm_l; focused_label = &arm_l; break;
            case 5: focused_part = &body_part_arm_r; focused_label = &arm_r; break;
            case 6: focused_part = &body_part_hand_l; focused_label = &hand_l; break;
            case 7: focused_part = &body_part_hand_r; focused_label = &hand_r; break;
            case 8: focused_part = &body_part_leg_l; focused_label = &leg_l; break;
            case 9: focused_part = &body_part_leg_r; focused_label = &leg_r; break;
            case 10: focused_part = &body_part_foot_l; focused_label = &foot_l; break;
            case 11: focused_part = &body_part_foot_r; focused_label = &foot_r; break;
            default: break;
        }
        if( focused_part != nullptr && focused_label != nullptr ) {
            std::string items;
            const bodypart_id focused_id = focused_part->id();
            for( const item_location &loc : worn_items ) {
                if( loc && loc->covers( focused_id ) ) {
                    if( !items.empty() ) {
                        items += "; ";
                    }
                    items += loc->display_name();
                }
            }
            if( items.empty() ) {
                items = ncmm::localized_text( "nothing", u8"\u043D\u0438\u0447\u0435\u0433\u043E" );
            }
            const std::string focus_line = *focused_label + ": " + items;
            trim_and_print( w, point( content_x, y++ ), content_width,
                            c_light_green, focus_line );
        }
    }

    if( !compact && selected != nullptr && y < footer_y ) {
        ++y;
        const std::string selected_label =
            ncmm::localized_text( "Selected", u8"\u0412\u044B\u0431\u0440\u0430\u043D\u043E" ) +
            ": " + selected->display_name();
        trim_and_print( w, point( content_x, y++ ), content_width, c_light_gray, selected_label );

        if( selected->is_armor() && y < footer_y ) {
            std::string coverage;
            const auto append_coverage = [&]( const bodypart_str_id & part,
                                              const std::string & label ) {
                if( selected->covers( part.id() ) ) {
                    if( !coverage.empty() ) {
                        coverage += ", ";
                    }
                    coverage += label;
                }
            };
            append_coverage( body_part_head, head );
            append_coverage( body_part_eyes, eyes );
            append_coverage( body_part_mouth, mouth );
            append_coverage( body_part_torso, torso );
            append_coverage( body_part_arm_l, arm_l );
            append_coverage( body_part_arm_r, arm_r );
            append_coverage( body_part_hand_l, hand_l );
            append_coverage( body_part_hand_r, hand_r );
            append_coverage( body_part_leg_l, leg_l );
            append_coverage( body_part_leg_r, leg_r );
            append_coverage( body_part_foot_l, foot_l );
            append_coverage( body_part_foot_r, foot_r );
            if( !coverage.empty() ) {
                const std::string coverage_line =
                    ncmm::localized_text( "Covers", u8"\u041F\u043E\u043A\u0440\u044B\u0432\u0430\u0435\u0442" ) +
                    ": " + coverage;
                trim_and_print( w, point( content_x, y++ ), content_width,
                                c_yellow, coverage_line );
            }
        }
    }

    if( selected != nullptr && selected->is_armor() &&
        ncmm::runtime_setting_hook_bool( "inventory.body_map.show_layers", 1 ) != 0 &&
        y < footer_y ) {
        std::string layers;
        for( const layer_level layer : selected->get_layer() ) {
            if( !layers.empty() ) {
                layers += " / ";
            }
            layers += item::layer_to_string( layer );
        }
        if( !layers.empty() ) {
            const std::string layer_line =
                ncmm::localized_text( "Layer", u8"\u0421\u043B\u043E\u0439" ) + ": " + layers;
            trim_and_print( w, point( content_x, y ), content_width,
                            c_light_gray, layer_line );
        }
    }
}
'@ 'inventory.body-map-frame-and-render'
$gic = Replace-ExactlyOnce $gic @'
    inventory_pick_selector inv_s( you, inv_s_p );

    inv_s.set_title( _( "Inventory" ) );
'@ @'
    inventory_pick_selector inv_s( you, inv_s_p );
    inv_s.set_equipment_body_map();

    inv_s.set_title( _( "Inventory" ) );
'@ 'inventory.body-map-normal-inventory-only'

# Ballistic Hit Chance: deterministic CDF for the exact dispersion model.
$dh = Replace-ExactlyOnce $dh @'
        double avg() const;
'@ @'
        double avg() const;

        /** Deterministic CDF of roll(): P( dispersion roll < threshold ). */
        double probability_below( double threshold ) const;
'@ 'dispersion.probability-declaration'

$dc = Replace-ExactlyOnce $dc '#include <algorithm>' @'
#include <algorithm>
#include <cmath>
#include <cstdint>
'@ 'dispersion.probability-includes'

$dc = Replace-ExactlyOnce $dc @'
double dispersion_sources::avg() const
{
    return max() / 2.0;
}
'@ @'
double dispersion_sources::avg() const
{
    return max() / 2.0;
}

double dispersion_sources::probability_below( double threshold ) const
{
    if( !std::isfinite( threshold ) ) {
        return threshold > 0.0 ? 1.0 : 0.0;
    }
    if( threshold <= 0.0 ) {
        return 0.0;
    }
    // roll() clamps the final result to 3600 arcminutes.
    if( threshold > 3600.0 ) {
        return 1.0;
    }

    long double multiplier = 1.0L;
    for( const double source : multipliers ) {
        multiplier *= static_cast<long double>( source );
    }
    if( multiplier == 0.0L ) {
        return 1.0;
    }
    // Supported weapon-dispersion multipliers are non-negative.
    if( multiplier < 0.0L ) {
        return 0.0;
    }

    const long double scaled_threshold =
        static_cast<long double>( threshold ) / multiplier;

    std::vector<long double> linear;
    linear.reserve( linear_sources.size() );
    for( const double source : linear_sources ) {
        if( source > 0.0 ) {
            linear.push_back( static_cast<long double>( source ) );
        }
    }

    // Exact CDF for a sum of independent U( 0, a_i ) variables.
    const auto uniform_sum_cdf = [&linear]( long double x ) -> long double {
        if( linear.empty() ) {
            return x > 0.0L ? 1.0L : 0.0L;
        }
        if( x <= 0.0L ) {
            return 0.0L;
        }

        const std::size_t n = linear.size();
        long double max_sum = 0.0L;
        long double denominator = 1.0L;
        for( const long double source : linear ) {
            max_sum += source;
            denominator *= source;
        }
        if( x >= max_sum ) {
            return 1.0L;
        }

        long double factorial = 1.0L;
        for( std::size_t i = 2; i <= n; ++i ) {
            factorial *= static_cast<long double>( i );
        }
        denominator *= factorial;

        // Current firearm paths have only a handful of linear sources.
        if( n >= 63 ) {
            return 0.0L;
        }

        const std::uint64_t combinations = std::uint64_t{ 1 } << n;
        long double sum = 0.0L;
        for( std::uint64_t mask = 0; mask < combinations; ++mask ) {
            long double shift = 0.0L;
            unsigned parity = 0;
            for( std::size_t i = 0; i < n; ++i ) {
                if( ( mask & ( std::uint64_t{ 1 } << i ) ) != 0 ) {
                    shift += linear[i];
                    parity ^= 1u;
                }
            }
            const long double remainder = x - shift;
            if( remainder <= 0.0L ) {
                continue;
            }
            const long double term = std::pow( remainder, static_cast<int>( n ) );
            sum += parity != 0u ? -term : term;
        }
        return std::clamp( sum / denominator, 0.0L, 1.0L );
    };

    if( normal_sources.empty() ) {
        return static_cast<double>(
                   std::clamp( uniform_sum_cdf( scaled_threshold ), 0.0L, 1.0L ) );
    }

    // Weapon paths have one clamped normal source: gun/ammo dispersion.
    // rng_normal( 0, hi ) is N( hi/2, hi/4 ) clamped to [0, hi].
    const long double hi = std::max(
                               0.0L, static_cast<long double>( normal_sources.front() ) );
    if( hi == 0.0L ) {
        return static_cast<double>(
                   std::clamp( uniform_sum_cdf( scaled_threshold ), 0.0L, 1.0L ) );
    }

    const long double mean = hi / 2.0L;
    const long double sigma = hi / 4.0L;
    const auto normal_cdf = [mean, sigma]( long double x ) -> long double {
        constexpr long double sqrt_two =
            1.414213562373095048801688724209698L;
        return 0.5L * ( 1.0L + std::erf( ( x - mean ) /
                                         ( sigma * sqrt_two ) ) );
    };
    const auto normal_pdf = [mean, sigma]( long double x ) -> long double {
        constexpr long double sqrt_two_pi =
            2.506628274631000502415765284811045L;
        const long double z = ( x - mean ) / sigma;
        return std::exp( -0.5L * z * z ) / ( sigma * sqrt_two_pi );
    };

    // With no positive uniform sources the clamped-normal CDF is available
    // directly; avoiding numerical integration also preserves the endpoint
    // atom at hi under the strict hit condition ( dispersion < threshold ).
    if( linear.empty() ) {
        if( scaled_threshold > hi ) {
            return 1.0;
        }
        return static_cast<double>(
                   std::clamp( normal_cdf( scaled_threshold ), 0.0L, 1.0L ) );
    }

    const long double mass_low = normal_cdf( 0.0L );
    const long double mass_high = 1.0L - normal_cdf( hi );

    // Simpson integration of the continuous interior. 256 panels are well
    // below the visible 0.1% precision for supported firearm distributions.
    constexpr int panels = 256;
    const long double step = hi / static_cast<long double>( panels );
    long double integral = 0.0L;
    for( int i = 0; i <= panels; ++i ) {
        const long double x = step * static_cast<long double>( i );
        const long double value =
            normal_pdf( x ) * uniform_sum_cdf( scaled_threshold - x );
        const int weight = ( i == 0 || i == panels ) ? 1 :
                           ( i % 2 == 0 ? 2 : 4 );
        integral += static_cast<long double>( weight ) * value;
    }
    integral *= step / 3.0L;

    const long double probability =
        mass_low * uniform_sum_cdf( scaled_threshold ) +
        integral +
        mass_high * uniform_sum_cdf( scaled_threshold - hi );

    return static_cast<double>( std::clamp( probability, 0.0L, 1.0L ) );
}
'@ 'dispersion.probability-implementation'


$rg = Replace-ExactlyOnce $rg '#include "npc.h"' @'
#include "npc.h"
#include "ncmm_loader.h"
'@ 'ranged.include-ncmm'

$rg = Replace-ExactlyOnce $rg @'
static const flag_id json_flag_SINGLE_ACTION( "SINGLE_ACTION" );
'@ @'
static const flag_id json_flag_SINGLE_ACTION( "SINGLE_ACTION" );
static const json_character_flag json_flag_HARDTOHIT( "HARDTOHIT" );
'@ 'ranged.hard-to-hit-flag'

$rg = Replace-ExactlyOnce $rg @'
    int chance_to_hit; // all hit probabilities summed up for sorting
    double confidence;
    double steadiness;
'@ @'
    int chance_to_hit; // all hit probabilities summed up for sorting
    double confidence;
    double steadiness;
    double exact_hit_probability = -1.0;
'@ 'ranged.prediction-field'

$rg = Replace-ExactlyOnce $rg @'
        if( prediction.is_default ) {
            prediction.moves += aim_to_selected.moves;
            prediction.steadiness = selected_steadiness;
        } else {
            // predict how long it'll take to reach from current recoil
            // to the current aim mode's threshold.
            const recoil_prediction aim_to_type = ( aim_type == ui.get_selected_aim_type() ) ? aim_to_selected :
                                                  predict_recoil( you, weapon, target, ui.get_sight_dispersion(), aim_type, you.recoil );
            prediction.steadiness = calc_steadiness( you, weapon, pos, aim_to_type.recoil );
        }

        // make a copy of the given dispersion, apply the aiming and calculate hit confidence
'@ @'
        double predicted_recoil_for_shot = you.recoil;
        if( prediction.is_default ) {
            prediction.moves += aim_to_selected.moves;
            prediction.steadiness = selected_steadiness;
            predicted_recoil_for_shot = aim_to_selected.recoil;
        } else {
            // predict how long it'll take to reach from current recoil
            // to the current aim mode's threshold.
            const recoil_prediction aim_to_type = ( aim_type == ui.get_selected_aim_type() ) ? aim_to_selected :
                                                  predict_recoil( you, weapon, target, ui.get_sight_dispersion(), aim_type, you.recoil );
            prediction.steadiness = calc_steadiness( you, weapon, pos, aim_to_type.recoil );
            predicted_recoil_for_shot = aim_to_type.recoil;
        }

        // make a copy of the given dispersion, apply the aiming and calculate hit confidence
'@ 'ranged.predicted-recoil'

$rg = Replace-ExactlyOnce $rg @'
        current_dispersion.add_range( aim_type.has_threshold ? aim_type.threshold :
                                      aim_to_selected.recoil );

        // this loop fills in the "confidence" values; the chances of great/good/graze outcomes
        prediction.confidence = confidence_estimate( target, current_dispersion );
'@ @'
        current_dispersion.add_range( aim_type.has_threshold ? aim_type.threshold :
                                      aim_to_selected.recoil );

        if( ncmm_hit_probability_enabled() ) {
            // Actual fire_gun() combines character and vehicle recoil into one
            // uniform dispersion source via recoil_total().
            dispersion_sources exact_dispersion = you.get_weapon_dispersion( weapon );
            exact_dispersion.add_range( predicted_recoil_for_shot + you.recoil_vehicle() );
            Creature *target_critter = get_creature_tracker().creature_at( pos );
            prediction.exact_hit_probability =
                ncmm_exact_hit_probability( exact_dispersion, target, target_critter );
        }

        // this loop fills in the "confidence" values; the chances of great/good/graze outcomes
        prediction.confidence = confidence_estimate( target, current_dispersion );
'@ 'ranged.exact-in-prediction'

$rg = Replace-ExactlyOnce $rg @'
Target_attributes::Target_attributes( int rng, double target_size, float light_target,
                                      bool can_see )
{
    range = rng;
    size = target_size;
    size_in_moa = target_size_in_moa( range, size );
    light = light_target;
    visible = can_see;
}

/*
* struct used to hold the information on entire aim_type prediction;
'@ @'
Target_attributes::Target_attributes( int rng, double target_size, float light_target,
                                      bool can_see )
{
    range = rng;
    size = target_size;
    size_in_moa = target_size_in_moa( range, size );
    light = light_target;
    visible = can_see;
}

static bool ncmm_hit_probability_enabled()
{
    return ncmm::runtime_setting_hook_bool( "targeting.hit_probability.enabled", 0 ) != 0;
}

static bool ncmm_hit_probability_decimal()
{
    return ncmm::runtime_setting_hook_bool( "targeting.hit_probability.decimal", 0 ) != 0;
}

static std::string ncmm_hit_probability_text( double probability )
{
    probability = std::clamp( probability, 0.0, 1.0 );
    const char *color = probability >= 0.85 ? "green" :
                        probability >= 0.60 ? "light_green" :
                        probability >= 0.35 ? "yellow" : "light_red";
    const double percent = probability * 100.0;
    return ncmm_hit_probability_decimal() ?
           string_format( "<color_%s>%.1f%%</color>", color, percent ) :
           string_format( "<color_%s>%.0f%%</color>", color, percent );
}

static double ncmm_exact_hit_probability( const dispersion_sources &dispersion,
        const Target_attributes &target, const Creature *target_critter )
{
    double probability = dispersion.probability_below( target.size_in_moa );

    // HARDTOHIT rolls dispersion twice and keeps the worse result.
    if( target_critter != nullptr && target_critter->as_character() != nullptr &&
        target_critter->as_character()->has_flag( json_flag_HARDTOHIT ) ) {
        probability *= probability;
    }

    // RANGE_DODGE applies to firearms too. Gun projectiles use speed 1000,
    // so the separate slow-projectile dodge roll does not apply here.
    if( target_critter != nullptr ) {
        const double range_dodge = std::clamp(
                                       target_critter->calculate_by_enchantment(
                                           1.0, enchant_vals::mod::RANGE_DODGE ) - 1.0,
                                       0.0, 1.0 );
        probability *= 1.0 - range_dodge;
    }

    return std::clamp( probability, 0.0, 1.0 );
}

static bool ncmm_projectile_is_wide( const item &weapon )
{
    const auto &effects = weapon.ammo_effects();
    return effects.count( ammo_effect_WIDE ) != 0 ||
           effects.count( ammo_effect_SHOT ) != 0 ||
           effects.count( ammo_effect_BOUNCE ) != 0 ||
           ( weapon.has_ammo_data() && weapon.ammo_data()->phase == phase_id::LIQUID );
}

static Target_attributes ncmm_gun_target_attributes( const Character &you,
        const item &weapon, const tripoint_bub_ms &pos )
{
    Target_attributes result( you.pos_bub(), pos );
    Creature *target_critter = get_creature_tracker().creature_at( pos );
    if( target_critter != nullptr && target_critter->as_monster() != nullptr &&
        ncmm_projectile_is_wide( weapon ) ) {
        result.size = occupied_tile_fraction( target_critter->get_size() );
        result.size_in_moa = target_size_in_moa( result.range, result.size );
    }
    return result;
}

/*
* struct used to hold the information on entire aim_type prediction;
'@ 'ranged.exact-probability-helpers'


$rg = Replace-ExactlyOnce $rg @'
    // Start printing by available width of aim window
    if( narrow ) {
'@ @'
    // Ballistic Hit Chance gets a dedicated compact layout.  Replacing the
    // five-column confidence table avoids overlap on 34-42 column sidebars.
    if( narrow && ncmm_hit_probability_enabled() ) {
        for( const aim_type_prediction &out : sorted ) {
            if( out.exact_hit_probability < 0.0 ) {
                continue;
            }
            const std::string col_hl = out.is_default ? "light_green" : "light_gray";
            const int pct_x = std::max( 1, width - 11 );
            trim_and_print( w, point( 1, line_number ), std::max( 1, pct_x - 2 ),
                            color_from_string( col_hl ), out.name );
            print_colored_text( w, point( pct_x, line_number ), col, col,
                                ncmm_hit_probability_text( out.exact_hit_probability ) );
            right_print( w, line_number++, 1, c_light_blue,
                         string_format( "%d", out.moves ) );
        }
        return line_number;
    }

    // Start printing by available width of aim window
    if( narrow ) {
'@ 'ranged.compact-exact-hit-table'

$rg = Replace-ExactlyOnce $rg @'
            std::string desc = time ==  0 ?
                               string_format( "<color_white>[%s]</color> <color_%s>%s %s</color> | %s: <color_light_blue>%3d</color>",
                                              out.hotkey, col_hl, out.name, _( "Aim" ), _( "Moves to fire" ), out.moves ) :
                               string_format( "<color_white>[%s]</color> <color_%s>%s %s</color> | %s: <color_light_blue>%3d</color> (%d)",
                                              out.hotkey, col_hl, out.name, _( "Aim" ), _( "Moves to fire" ), out.moves, time );

            print_colored_text( w, point( 1, line_number++ ), col, col, desc );
'@ @'
            std::string desc;
            if( out.exact_hit_probability >= 0.0 ) {
                // Keep the exact-probability row compact enough for the normal
                // right-side targeting panel. The vanilla verbose labels plus
                // an appended hit percentage overflow at ~45-50 columns.
                desc = string_format(
                           "<color_white>[%s]</color> <color_%s>%s</color> | %s | <color_light_blue>%d</color>",
                           out.hotkey, col_hl, out.name,
                           ncmm_hit_probability_text( out.exact_hit_probability ),
                           out.moves );
                if( time != 0 ) {
                    desc += string_format( " (%d)", time );
                }
            } else {
                desc = time == 0 ?
                       string_format( "<color_white>[%s]</color> <color_%s>%s %s</color> | %s: <color_light_blue>%3d</color>",
                                      out.hotkey, col_hl, out.name, _( "Aim" ), _( "Moves to fire" ), out.moves ) :
                       string_format( "<color_white>[%s]</color> <color_%s>%s %s</color> | %s: <color_light_blue>%3d</color> (%d)",
                                      out.hotkey, col_hl, out.name, _( "Aim" ), _( "Moves to fire" ), out.moves, time );
            }

            print_colored_text( w, point( 1, line_number++ ), col, col, desc );
'@ 'ranged.full-hit'

$rg = Replace-ExactlyOnce $rg @'
    // This is absolute accuracy for the player.
    // TODO: push the calculations duplicated from Creature::deal_projectile_attack() and
    // Creature::projectile_attack() into shared methods.
    // Dodge doesn't affect gun attacks

    dispersion_sources dispersion = you.get_weapon_dispersion( weapon );
'@ @'
    // Legacy confidence UI remains intact. Ballistic Hit Chance separately
    // mirrors projectile dispersion, HARDTOHIT and RANGE_DODGE.

    dispersion_sources dispersion = you.get_weapon_dispersion( weapon );
'@ 'ranged.aim-comment'

$rg = Replace-ExactlyOnce $rg @'
    const std::vector<aim_type_prediction> aim_chances = calculate_ranged_chances( ui, you,
            target_ui::TargetMode::Fire, ctxt, weapon, dispersion, confidence_config,
            Target_attributes( you.pos_bub(), pos ), pos, load_loc );
'@ @'
    const Target_attributes target = ncmm_gun_target_attributes( you, weapon, pos );
    const std::vector<aim_type_prediction> aim_chances = calculate_ranged_chances( ui, you,
            target_ui::TargetMode::Fire, ctxt, weapon, dispersion, confidence_config,
            target, pos, load_loc );
'@ 'ranged.adjust-target-size'


$rg = Replace-ExactlyOnce $rg @'
    str = string_format( _( "Recoil: %s" ), str );
    nc_color clr = c_light_gray;
    print_colored_text( w_target, point( 1, text_y++ ), clr, clr, str );
}

void target_ui::panel_spell_info( int &text_y )
'@ @'
    str = string_format( _( "Recoil: %s" ), str );
    nc_color clr = c_light_gray;
    print_colored_text( w_target, point( 1, text_y++ ), clr, clr, str );

    if( mode == TargetMode::Fire && status == Status::Good && src != dst &&
        ncmm_hit_probability_enabled() && relevant != nullptr &&
        !relevant->gun_current_mode().melee() ) {
        const gun_mode current_mode = relevant->gun_current_mode();
        const item &weapon = *current_mode;
        const Target_attributes target = ncmm_gun_target_attributes( *you, weapon, dst );
        Creature *target_critter = get_creature_tracker().creature_at( dst );

        const auto probability_at_recoil = [&]( double raw_recoil ) {
            dispersion_sources exact_dispersion = you->get_weapon_dispersion( weapon );
            exact_dispersion.add_range( raw_recoil + you->recoil_vehicle() );
            return ncmm_exact_hit_probability( exact_dispersion, target, target_critter );
        };

        const double current_probability = probability_at_recoil( you->recoil );
        const std::string current_line =
            ncmm::localized_text( "Hit now", u8"\u041F\u043E\u043F\u0430\u0434\u0430\u043D\u0438\u0435 \u0441\u0435\u0439\u0447\u0430\u0441" ) + ": " +
            ncmm_hit_probability_text( current_probability );
        print_colored_text( w_target, point( 1, text_y++ ), clr, clr, current_line );

        if( current_mode.qty > 1 ) {
            map &here = get_map();
            bool bipod = here.has_flag_ter_or_furn(
                              ter_furn_flag::TFLAG_MOUNTABLE, you->pos_bub( here ) ) ||
                          you->is_prone();
            if( !bipod ) {
                if( const optional_vpart_position vp = here.veh_at( you->pos_abs() ) ) {
                    bipod = vp->vehicle().has_part( you->pos_abs(), "MOUNTABLE" );
                }
            }

            const double absorb =
                std::min( you->get_skill_level( weapon.gun_skill() ),
                          static_cast<float>( MAX_SKILL ) ) /
                static_cast<double>( MAX_SKILL * 2 );
            const int recoil_per_shot = weapon.gun_recoil( *you, bipod );
            const int immediate_recoil = static_cast<int>(
                you->calculate_by_enchantment( 5.0, enchant_vals::mod::RECOIL_MODIFIER ) *
                ( recoil_per_shot * ( 1.0 - absorb ) ) );
            const bool volley = current_mode.flags.count( "VOLLEY" ) != 0;

            // Match fire_gun(): the selected burst may be shortened by the
            // ammunition/energy actually available at the moment of firing.
            const int actual_shots = std::max(
                                         0, std::min( current_mode.qty,
                                                 weapon.shots_remaining( here, you ) ) );
            if( actual_shots > 0 ) {
                // This line is the immediate-burst counterpart of "Hit now":
                // it starts from current recoil. Future aim-mode rows below remain
                // responsible for showing the result after additional aiming.
                const double initial_recoil = you->recoil;

                std::string burst =
                    ncmm::localized_text(
                        "Burst now",
                        u8"\u041E\u0447\u0435\u0440\u0435\u0434\u044C \u0441\u0435\u0439\u0447\u0430\u0441" ) + ": ";
                const int shown_front = std::min( actual_shots, 5 );
                for( int shot = 0; shot < shown_front; ++shot ) {
                    if( shot > 0 ) {
                        burst += " / ";
                    }
                    const double shot_recoil = initial_recoil +
                                               ( volley ? 0.0 :
                                                 static_cast<double>( immediate_recoil ) * shot );
                    burst += ncmm_hit_probability_text(
                                 probability_at_recoil( shot_recoil ) );
                }

                if( actual_shots > 5 ) {
                    burst += actual_shots > 6 ? " / ... / " : " / ";
                    const double last_recoil = initial_recoil +
                                               ( volley ? 0.0 :
                                                 static_cast<double>( immediate_recoil ) *
                                                 ( actual_shots - 1 ) );
                    burst += ncmm_hit_probability_text(
                                 probability_at_recoil( last_recoil ) );
                }
                print_colored_text( w_target, point( 1, text_y++ ), clr, clr, burst );
            }
        }
    }
}

void target_ui::panel_spell_info( int &text_y )
'@ 'ranged.current-and-burst'

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

        /** NCMM: register a stable global default without overwriting user remaps. */
        void ncmm_register_default_action( const std::string &action_descriptor,
                                           const translation &name,
                                           const input_event &default_event );

        /**
         * NCMM: register a default only in one input context (DEFAULTMODE for module hotkeys).
         * Existing user bindings in that context are preserved.
         */
        void ncmm_register_context_default_action( const std::string &action_descriptor,
                                                   const translation &name,
                                                   const input_event &default_event,
                                                   const std::string &context );
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

void input_manager::ncmm_register_context_default_action(
        const std::string &action_descriptor,
        const translation &name,
        const input_event &default_event,
        const std::string &context )
{
    if( action_descriptor.empty() || context.empty() ) {
        return;
    }

    action_attributes &basic = basic_action_contexts[context][action_descriptor];
    basic.name = name;
    basic.is_user_created = false;
    basic.input_events.clear();
    basic.input_events.push_back( default_event );

    // Match CDDA's native "keyboard_any" semantics for context-scoped NCMM hotkeys.
    // Tiles gameplay can deliver function keys as keyboard_char (KEY_F(n)) even when
    // the manifest default was registered from the SDL-style keyboard_code value.
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

    t_actions &active = action_contexts[context];
    const auto it = active.find( action_descriptor );
    if( it == active.end() ) {
        active[action_descriptor] = basic;
    } else {
        // The user-keybinding file was loaded before NCMM reaches gameplay.
        // Never overwrite its selected events; only refresh the display name.
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
    world_generator->set_active_world( nullptr );
    world_generator->init();

    init_strings();
'@ @'
    world_generator->set_active_world( nullptr );
    world_generator->init();

    if( ncmm::gameplay_smoke_requested() ) {
        std::exit( ncmm::run_gameplay_smoke() );
    }

    init_strings();
'@ 'main-menu.ncmm-gameplay-smoke'

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


$mm = Replace-ExactlyOnce $mm @'
    int window_width = getmaxx( w_open );
    int window_height = getmaxy( w_open );

    // Draw horizontal line
'@ @'
    int window_width = getmaxx( w_open );
    int window_height = getmaxy( w_open );

    const std::string ncmm_version = ncmm::version_label();
    const int ncmm_version_width = utf8_width( ncmm_version, true );
    if( window_width > ncmm_version_width + 4 ) {
        mvwprintz( w_open, point( window_width - ncmm_version_width - 2, 1 ),
                   c_dark_gray, "%s", ncmm_version );
    }

    // Draw horizontal line
'@ 'main-menu.ncmm-version-label'

# NCMM generic character modifier hooks.
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
Write-Utf8 $rangedCpp $rg
Write-Utf8 $dispersionH $dh
Write-Utf8 $dispersionCpp $dc
Write-Utf8 $inventoryUiH $iuh
Write-Utf8 $inventoryUiCpp $iuc
Write-Utf8 $gameInventoryCpp $gic
Write-Utf8 $advancedInvCpp $aic

Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.h') (Join-Path $src 'ncmm_loader.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_loader.cpp') (Join-Path $src 'ncmm_loader.cpp') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_fault_policy.h') (Join-Path $src 'ncmm_fault_policy.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_manifest_policy.h') (Join-Path $src 'ncmm_manifest_policy.h') -Force
Copy-Item (Join-Path $PSScriptRoot 'ncmm_item_glyphs.h') (Join-Path $src 'ncmm_item_glyphs.h') -Force
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
$rg2 = Read-Utf8 $rangedCpp
$dh2 = Read-Utf8 $dispersionH
$dc2 = Read-Utf8 $dispersionCpp
$iuh2 = Read-Utf8 $inventoryUiH
$iuc2 = Read-Utf8 $inventoryUiCpp
$gic2 = Read-Utf8 $gameInventoryCpp
$aic2 = Read-Utf8 $advancedInvCpp

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
if ((NonAscii-Signature $rg2) -ne $rgSig) { throw 'UTF-8 preservation check failed for ranged.cpp' }
if ((NonAscii-Signature $dh2) -ne $dhSig) { throw 'UTF-8 preservation check failed for dispersion.h' }
if ((NonAscii-Signature $dc2) -ne $dcSig) { throw 'UTF-8 preservation check failed for dispersion.cpp' }
if ((NonAscii-Signature $iuh2) -ne $iuhSig) { throw 'UTF-8 preservation check failed for inventory_ui.h' }
if ((NonAscii-Signature $iuc2) -ne $iucSig) { throw 'UTF-8 preservation check failed for inventory_ui.cpp' }
if ((NonAscii-Signature $gic2) -ne $gicSig) { throw 'UTF-8 preservation check failed for game_inventory.cpp' }
if ((NonAscii-Signature $aic2) -ne $aicSig) { throw 'UTF-8 preservation check failed for advanced_inv.cpp' }

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
foreach ($needle in @('input_manager::ncmm_register_default_action','input_manager::ncmm_register_context_default_action','alternate_type','portable_name','keyboard_char','keyboard_code')) {
    if (-not $ic2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('ncmm::register_gameplay_actions( ctxt );','ncmm::handle_gameplay_action( action )')) {
    if (-not $ha2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('targeting.hit_probability.enabled','exact_hit_probability','ncmm_hit_probability_text','Hit now')) {
    if (-not $rg2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $dh2.Contains('probability_below')) { throw 'Post-check failed: dispersion probability declaration' }
if (-not $dc2.Contains('dispersion_sources::probability_below')) { throw 'Post-check failed: dispersion probability implementation' }
foreach ($needle in @('set_equipment_body_map','equipment_body_map_reserved_height')) {
    if (-not $iuh2.Contains($needle)) { throw "Post-check failed: $needle" }
}
foreach ($needle in @('inventory.body_map.enabled','inventory.body_map.show_layers','inventory.body_map.right_arrow_gear','res.action == "ANY_INPUT" && ch == KEY_RIGHT','draw_equipment_body_map','Explicit human-shaped paper doll.','NCMM_BODY_MAP_FOCUS')) {
    if (-not $iuc2.Contains($needle)) { throw "Post-check failed: $needle" }
}
if (-not $gic2.Contains('set_equipment_body_map();')) { throw 'Post-check failed: normal inventory body-map opt-in' }
foreach ($pair in @(@($iuc2,'ncmm::inventory_item_symbol( *entry.any_item() )'),
                    @($aic2,'ncmm::inventory_item_symbol( it )'),
                    @($aic2,'#include "ncmm_loader.h"'))) {
    if (-not $pair[0].Contains($pair[1])) { throw "Post-check failed: $($pair[1])" }
}
foreach ($pair in @(@($iuc2,2),@($aic2,1))) {
    $count = ([regex]::Matches($pair[0],[regex]::Escape('ncmm::inventory_symbols_enabled( get_option<bool>( "ITEM_SYMBOLS" ) )'))).Count
    if ($count -ne $pair[1]) { throw 'Post-check failed: Item Glyphs symbol-slot gates' }
}

Set-Content -Path $marker -Value "NCMM Host API v1 / NCMM 0.8.2 module contract`n" -Encoding ASCII
Write-Host 'NCMM 0.8.2 host patch applied and UTF-8 preservation verified.'
