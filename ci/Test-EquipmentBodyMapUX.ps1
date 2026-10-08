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
Require ($source.Contains('loc->covers( bp )')) 'real worn-item coverage is missing'
Require ($source.Contains('equipment_body_map_focus = focus;')) 'anatomy focus is no longer interactive'
Require ($source.Contains('equipment_body_map_reserved_height <= 11')) 'compact layout contract missing'
Require ($source.Contains('equipment_body_map_reserved_height > 0 ?')) 'worn-column vertical reservation changed'
Require ($source.Contains('set_equipment_body_map();')) 'only regular Inventory should opt into EBM'
Require (-not $source.Contains('const int third = std::max( 6, content_width / 3 );')) 'old text-column pseudo doll returned'

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
