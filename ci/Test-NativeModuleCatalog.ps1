param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path
. (Join-Path $root 'ci\NativeModuleCatalog.ps1')

$source=Get-Content (Join-Path $root 'components\native-build.json') -Raw
$modules=@(Get-NcmmNativeBuildModules -RepositoryRoot $root)
$expected=@(
    'advanced_world_settings',
    'ballistic_hit_chance',
    'equipment_body_map',
    'item_glyphs',
    'survivor_progression'
)
if($modules.Count -ne $expected.Count -or
   (@($modules | ForEach-Object { $_.Id }) -join ',') -cne ($expected -join ',')) {
    throw 'Native module build registry baseline ordering/completeness drift.'
}
$archives=@($modules | ForEach-Object { "NCMM_$($_.ArchiveStem)_v$($_.Version).zip" })
$baseline=@(
    'NCMM_AdvancedWorldSettings_v0.6.4.zip',
    'NCMM_BallisticHitChance_v0.1.0.zip',
    'NCMM_EquipmentBodyMap_v0.1.0.zip',
    'NCMM_ItemGlyphs_v0.1.0.zip',
    'NCMM_SurvivorProgression_v0.15.0.zip'
)
if(($archives -join '|') -cne ($baseline -join '|')) {
    throw 'Native archive naming drift: '+($archives -join ', ')
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
    @{Name='duplicate payload';Change={param($r) $r.modules[4].required_payload_files=@('persistent_data/dimensional_pouch.json','persistent_data/dimensional_pouch.json')}},
    @{Name='missing required payload';Change={param($r) $r.modules[4].required_payload_files=@('persistent_data/nonexistent_fixture.json')}},
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
