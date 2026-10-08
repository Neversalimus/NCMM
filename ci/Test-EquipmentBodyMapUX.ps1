param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $RepositoryRoot).Path
$source = [IO.File]::ReadAllText((Join-Path $root 'host_patch\Apply-NCMMHostPatch.ps1'), [Text.Encoding]::UTF8)

function Require([bool]$ok, [string]$why) {
    if(-not $ok) { throw "Equipment Body Map UX contract: $why" }
}

# The runtime hit tester and painter must use the SAME geometry tables.
Require ($source.Contains('ncmm_equipment_doll_hit( p.x - doll_x, p.y - doll_y,')) 'mouse hit geometry differs from drawn sprite'
Require ($source.Contains('const ncmm_doll_cell *cells = compact ? ncmm_doll_compact : ncmm_doll_full;')) 'renderer does not use shared geometry'
Require ($source.Contains('const int doll_y = panel_top + 2;')) 'sprite starting row changed'
Require ($source.Contains('u.encumb( bp )')) 'effective character encumbrance is not displayed'
Require ($source.Contains('zone_present[zone] = u.has_part( bp, body_part_filter::equivalent );')) 'missing-limb guard removed'
Require ($source.Contains('if( !zone_present[zone] ) {')) 'missing-limb short circuit removed'
Require ($source.Contains('if( input.action == "NCMM_BODY_MAP_FOCUS" ) {')) 'body-map focus must not reach inventory item actions'
Require ($source.Contains('const std::string summary = compact || content_width < 33 ?')) 'narrow status must prioritize encumbrance'
Require ($source.Contains('if( content_width < 33 ) {')) 'narrow full-height gear inspector is required'
Require ($source.Contains('if( shown < 3 ) {')) 'narrow inspector must show more than one worn item'
Require ($source.Contains('std::string first_worn = ncmm::localized_text(')) 'compact inspector must show actual worn item'
Require ($source.Contains("inventory.body-map-focus-only-noop")) 'focus action transform contract absent'
Require ($source.Contains('loc->covers( bp )')) 'real worn-item coverage is missing'
Require ($source.Contains('equipment_body_map_focus = focus;')) 'anatomy focus is no longer interactive'
Require ($source.Contains('equipment_body_map_reserved_height <= 11')) 'compact layout contract missing'
Require ($source.Contains('equipment_body_map_reserved_height > 0 ?')) 'worn-column vertical reservation changed'
Require ($source.Contains('equipment_body_map_reserved_height > 0 &&')) 'hidden panel must gate row reclamation'
Require ($source.Contains('own_gear_column.set_height( client_height );')) 'hidden panel must restore full worn column height'
Require ($source.Contains('own_gear_column.prepare_paging( filter );')) 'hidden panel must restore worn paging'
Require ($source.Contains('set_equipment_body_map();')) 'only regular Inventory should opt into EBM'
Require (-not $source.Contains('const int third = std::max( 6, content_width / 3 );')) 'old text-column pseudo doll returned'

# EBM v2.1: encumbrance heat must reflect actual encumbrance even on naked/mutated
# zones.  No selection/focus overlay may hide a yellow/red condition.
Require ($source.Contains('const auto enc_color = [&]( int zone ) -> nc_color {')) 'dedicated effective encumbrance heat map removed'
Require ($source.Contains('nc_color tint = enc_color( zone );')) 'sprite does not use effective encumbrance heat'
Require ($source.Contains('} else if( encumbrance[zone] < 10 ) {')) 'focus or selected coverage can obscure elevated encumbrance'
Require ($source.Contains('enc_color( zone ), label')) '12-zone numerical readout hides risk coloring'
Require ($source.Contains('equipment_body_map_focus == zone && !focus_marker_drawn')) 'focus marker no longer independent of heat'
Require ($source.Contains('const std::string marker = focused ? ">" : selected_covers[zone] ? "*" : "";')) 'selected coverage no longer readable when high encumbrance'
Require ($source.Contains('const std::string worn_label = ncmm::localized_text( "Worn", u8"\u041d\u0430\u0434\u0435\u0442\u043e" );')) 'worn-count noun is not inflection neutral'
Require ($source.Contains('worn_label + " " + number_of_worn')) 'count preceded by noun, avoid invalid Russian noun cases'
Require ($source.Contains('if( !compact && content_width >= 33 ) {')) 'numeric matrix not confined to wide layout'
Require ($source.Contains('const auto enc_row = [&]( int row, int first, int second, int third )')) 'per-zone numeric matrix absent'
Require ($source.Contains('detail_line( ncmm::localized_text( "Layer: "')) 'selected armor layer detail lost'
Require ($source.Contains('if( compact || content_width < 33 ) {')) 'narrow/compact status disappeared'
$expectedReadout = @(
    'enc_row( panel_top + 12, 0, 1, 2 );',
    'enc_row( panel_top + 13, 3, 4, 5 );',
    'enc_row( panel_top + 14, 6, 7, -1 );',
    'enc_row( panel_top + 15, 8, 9, -1 );',
    'enc_row( panel_top + 16, 10, 11, -1 );'
)
foreach($line in $expectedReadout) {
    Require ($source.Contains($line)) ("missing body-zone comparison row: " + $line)
}
$heatStart = $source.IndexOf('const auto enc_color = [&]( int zone ) -> nc_color {')
$heatEnd = $source.IndexOf('const ncmm_doll_cell *cells = compact ?', $heatStart)
$heat = $source.Substring($heatStart, $heatEnd - $heatStart)
Require ($heat.Contains('const int enc = encumbrance[zone];')) 'heat depends on worn count'
Require ($heat.Contains('enc >= 70 ? c_red : enc >= 40 ? c_light_red :')) 'danger color thresholds changed'
Require (-not $heat.Contains('worn_count')) 'heat wrongly gated on worn clothes'
Write-Host 'Equipment Body Map v2.1 independent heatmap, localized count, 12-zone numbers: PASS' -ForegroundColor Green


$cellPattern = '\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(?:u8)?"([^"]*)"\s*,\s*(?:u8)?"([^"]*)"\s*,\s*(?:u8)?"([^"]*)"\s*\}'
foreach($layout in @('full', 'compact')) {
    $match = [regex]::Match($source, ('(?s)static const ncmm_doll_cell ncmm_doll_' + $layout + '\[\] = \{(.*?)\};'))
    Require $match.Success ("layout not found: " + $layout)
    $cells = @([regex]::Matches($match.Groups[1].Value, $cellPattern))
    Require ($cells.Count -ge 12) ("too few cells in " + $layout)
    $maxY = if($layout -eq 'full') { 9 } else { 6 }
    $occupied = @{}
    $zones = @{}
    foreach($cell in $cells) {
        $zone = [int]$cell.Groups[1].Value
        $x = [int]$cell.Groups[2].Value
        $y = [int]$cell.Groups[3].Value
        $empty = $cell.Groups[4].Value
        $worn = $cell.Groups[5].Value
        $stacked = $cell.Groups[6].Value
        Require ($zone -ge 0 -and $zone -le 11) ("invalid body-zone id in " + $layout)
        Require ($y -ge 0 -and $y -le $maxY) ("sprite outside allocated height in " + $layout)
        $emptyWidth = [regex]::Replace($empty, '\\u[0-9A-Fa-f]{4}', '#').Length
        $wornWidth = [regex]::Replace($worn, '\\u[0-9A-Fa-f]{4}', '#').Length
        $stackedWidth = [regex]::Replace($stacked, '\\u[0-9A-Fa-f]{4}', '#').Length
        Require ($x -ge 0 -and $x + $emptyWidth -le 14) ("sprite outside 14-column body silhouette in " + $layout)
        Require ($emptyWidth -gt 0 -and $emptyWidth -eq $wornWidth -and $emptyWidth -eq $stackedWidth) ("glyph widths differ in " + $layout)
        $zones[$zone] = $true
        for($dx = 0; $dx -lt $emptyWidth; ++$dx) {
            $pos = ('{0}:{1}' -f $y,($x+$dx))
            Require (-not $occupied.ContainsKey($pos)) ("overlapping mouse targets in " + $layout + " at " + $pos)
            $occupied[$pos] = $zone
        }
    }
    Require ($zones.Count -eq 12) ("not all 12 body zones are reachable in " + $layout)
    Write-Host ("Equipment Body Map {0}: PASS ({1} cells, {2} distinct hit positions, 12 zones)." -f $layout,$cells.Count,$occupied.Count) -ForegroundColor Green
}
Require ($source.Contains('for( int row = panel_top + 1; row < footer_y; ++row )')) 'panel does not clear stale pixels'
Write-Host 'Equipment Body Map UX static layout contracts: PASS' -ForegroundColor Green
