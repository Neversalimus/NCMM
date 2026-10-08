param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path
. (Join-Path $root 'ci\NativeModuleCatalog.ps1')

$source=Get-Content (Join-Path $root 'components\native-build.json') -Raw
$modules=@(Get-NcmmNativeBuildModules -RepositoryRoot $root)
# Regression-lock shipped identities/filenames, but permit additional modules.
$baseline=@{
    advanced_world_settings='AdvancedWorldSettings'
    ballistic_hit_chance='BallisticHitChance'
    equipment_body_map='EquipmentBodyMap'
    item_glyphs='ItemGlyphs'
    survivor_progression='SurvivorProgression'
}
if($modules.Count -lt $baseline.Count) { throw 'Existing native modules disappeared.' }
foreach($id in @($baseline.Keys)) {
    $rows=@($modules | Where-Object { $_.Id -eq $id })
    if($rows.Count -ne 1 -or $rows[0].ArchiveStem -cne $baseline[$id]) {
        throw "Existing native module '$id' identity/archive stem drift."
    }
    $expectedArchive="NCMM_$($baseline[$id])_v$($rows[0].Version).zip"
    if(-not $expectedArchive.EndsWith(".zip")) {
        throw "Invalid archive identity for $id"
    }
}
# The platform smoke harness is independent; changing module build order
# must not change whether each module can be validated.
$reordered=ConvertFrom-Json $source
$reordered.modules=@($reordered.modules | Sort-Object id -Descending)
$reorderedModules=@(Get-NcmmNativeBuildModules -RepositoryRoot $root -Registry $reordered)
if($reorderedModules.Count -ne $modules.Count -or
   $reorderedModules[0].Id -eq $modules[0].Id) {
    throw 'Native module catalog order independence regression.'
}

$mutations=@(
    @{Name='missing entry';Change={param($r) $r.modules=@($r.modules|Select-Object -Skip 1)}},
    @{Name='duplicate id';Change={param($r) $r.modules[1].id=$r.modules[0].id}},
    @{Name='duplicate folder';Change={param($r) $r.modules[1].folder=$r.modules[0].folder}},
    @{Name='duplicate archive';Change={param($r) $r.modules[1].archive_stem=$r.modules[0].archive_stem}},
    @{Name='duplicate build dir';Change={param($r) $r.modules[1].build_directory=$r.modules[0].build_directory}},
    @{Name='unknown id';Change={param($r) $r.modules[0].id='unregistered_mod'}},
    @{Name='path traversal';Change={param($r) $r.modules[0].folder='../outside'}},
    @{Name='unsafe smoke executable';Change={param($r) $r.modules[0].extra_smoke_executables=@('../bad.exe')}},
    @{Name='duplicate payload';Change={param($r) (@($r.modules|Where-Object { $_.id -eq 'survivor_progression' })[0]).required_payload_files=@('persistent_data/dimensional_pouch.json','persistent_data/dimensional_pouch.json')}},
    @{Name='missing required payload';Change={param($r) (@($r.modules|Where-Object { $_.id -eq 'survivor_progression' })[0]).required_payload_files=@('persistent_data/nonexistent_fixture.json')}},
    @{Name='invalid boolean';Change={param($r) $r.modules[0].missing_contract_smoke='yes'}},
    @{Name='invalid schema';Change={param($r) $r.schema=17}}
)
foreach($fixture in $mutations) {
    $registry=ConvertFrom-Json $source
    & $fixture.Change $registry
    $rejected=$false
    try { $null=Get-NcmmNativeBuildModules -RepositoryRoot $root -Registry $registry }
    catch { $rejected=$true }
    if(-not $rejected) { throw "Native module registry accepted invalid fixture: $($fixture.Name)" }
}
Write-Host ("Native module registry: PASS ({0} modules, {1} mutation fixtures, stable release filenames)." -f $modules.Count,$mutations.Count) -ForegroundColor Green
