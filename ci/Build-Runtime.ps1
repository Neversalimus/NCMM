param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [switch]$SkipTextEncoding,
    [switch]$PayloadOnly
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$encodingGuard = Join-Path $RepositoryRoot 'ci\Test-TextEncoding.ps1'
if (-not $SkipTextEncoding) {
    & $encodingGuard -RepoRoot $RepositoryRoot
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$payload = Join-Path $OutputRoot 'payload'
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\AdvancedWorldSettings') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\SurvivorProgression') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\BallisticHitChance') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\EquipmentBodyMap') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\ItemGlyphs') | Out-Null

$awsManifestPath = Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings\mod.json'
$survivorManifestPath = Join-Path $RepositoryRoot 'mods\SurvivorProgression\mod.json'
$ballisticManifestPath = Join-Path $RepositoryRoot 'mods\BallisticHitChance\mod.json'
$equipmentBodyMapManifestPath = Join-Path $RepositoryRoot 'mods\EquipmentBodyMap\mod.json'
$itemGlyphsManifestPath = Join-Path $RepositoryRoot 'mods\ItemGlyphs\mod.json'
$awsManifestSource = Get-Content $awsManifestPath -Raw | ConvertFrom-Json
$survivorManifestSource = Get-Content $survivorManifestPath -Raw | ConvertFrom-Json
$ballisticManifestSource = Get-Content $ballisticManifestPath -Raw | ConvertFrom-Json
$equipmentBodyMapManifestSource = Get-Content $equipmentBodyMapManifestPath -Raw | ConvertFrom-Json
$itemGlyphsManifestSource = Get-Content $itemGlyphsManifestPath -Raw | ConvertFrom-Json
$hostVersion = (& (Join-Path $RepositoryRoot 'ci\Get-NcmmCurrentVersion.ps1') -RepositoryRoot $RepositoryRoot).Trim()
$awsVersion = [string]$awsManifestSource.version
$survivorVersion = [string]$survivorManifestSource.version
$ballisticVersion = [string]$ballisticManifestSource.version
$equipmentBodyMapVersion = [string]$equipmentBodyMapManifestSource.version
$itemGlyphsVersion = [string]$itemGlyphsManifestSource.version
foreach($pair in @(
    @{Name='Host';Value=$hostVersion},
    @{Name='Advanced World Settings';Value=$awsVersion},
    @{Name='Survivor Progression';Value=$survivorVersion},
    @{Name='Ballistic Hit Chance';Value=$ballisticVersion},
    @{Name='Equipment Body Map';Value=$equipmentBodyMapVersion},
    @{Name='Item Glyphs';Value=$itemGlyphsVersion}
)){
    if([string]::IsNullOrWhiteSpace([string]$pair.Value) -or [string]$pair.Value -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$'){
        throw ("Invalid {0} version: {1}" -f $pair.Name,$pair.Value)
    }
}

function New-NcmmModuleArchive {
    param(
        [Parameter(Mandatory=$true)][string]$Folder,
        [Parameter(Mandatory=$true)][string]$ComponentId,
        [Parameter(Mandatory=$true)][string]$Version
    )
    $source = Join-Path $payload ('code_mods\' + $Folder)
    if (-not (Test-Path (Join-Path $source 'ncmm_mod.dll') -PathType Leaf) -or
        -not (Test-Path (Join-Path $source 'mod.json') -PathType Leaf)) {
        throw "Cannot package incomplete module: $ComponentId"
    }

    $stage = Join-Path $OutputRoot ('_module_package_' + $ComponentId)
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    $moduleDest = Join-Path $stage ('code_mods\' + $Folder)
    New-Item -ItemType Directory -Force -Path $moduleDest | Out-Null
    Copy-Item (Join-Path $source 'ncmm_mod.dll') (Join-Path $moduleDest 'ncmm_mod.dll') -Force
    Copy-Item (Join-Path $source 'mod.json') (Join-Path $moduleDest 'mod.json') -Force
    foreach($about in Get-ChildItem $source -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
        Copy-Item $about.FullName (Join-Path $moduleDest $about.Name) -Force
    }
    $dataSource = Join-Path $source 'data'
    if (Test-Path $dataSource -PathType Container) {
        Copy-Item $dataSource (Join-Path $moduleDest 'data') -Recurse -Force
    }
    $persistentDataSource = Join-Path $source 'persistent_data'
    if (Test-Path $persistentDataSource -PathType Container) {
        Copy-Item $persistentDataSource (Join-Path $moduleDest 'persistent_data') -Recurse -Force
    }
    $descriptor = Join-Path $RepositoryRoot ('components\' + $ComponentId + '.json')
    Copy-Item $descriptor (Join-Path $stage 'component.json') -Force

    $readme = @(
        "NCMM native module: $ComponentId",
        "Version: $Version",
        ("Requires: NCMM Host " + $hostVersion),
        "",
        "Preferred installation: run NCMM_Setup.exe and select this component.",
        "Manual fallback: copy the code_mods folder into the selected CDDA installation."
    ) -join [Environment]::NewLine
    Set-Content (Join-Path $stage 'README.txt') -Value $readme -Encoding UTF8

    $zipName = if ($ComponentId -eq 'advanced_world_settings') {
        "NCMM_AdvancedWorldSettings_v$Version.zip"
    } elseif ($ComponentId -eq 'survivor_progression') {
        "NCMM_SurvivorProgression_v$Version.zip"
    } elseif ($ComponentId -eq 'ballistic_hit_chance') {
        "NCMM_BallisticHitChance_v$Version.zip"
    } elseif ($ComponentId -eq 'equipment_body_map') {
        "NCMM_EquipmentBodyMap_v$Version.zip"
    } elseif ($ComponentId -eq 'item_glyphs') {
        "NCMM_ItemGlyphs_v$Version.zip"
    } else {
        throw "Unknown native module component id: $ComponentId"
    }
    $zipPath = Join-Path (Split-Path $OutputRoot -Parent) $zipName
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    return $zipPath
}

$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { throw "Framework csc.exe not found: $csc" }

$bootstrapOut = Join-Path $payload 'cataclysm-tiles.ncmm-bootstrap.exe'
$bootstrapSource = Join-Path $RepositoryRoot 'runtime\NCMMBootstrap.cs'
& $csc /nologo /target:winexe /optimize+ /platform:x64 `
    /reference:System.Web.Extensions.dll `
    /out:$bootstrapOut `
    $bootstrapSource
if ($LASTEXITCODE -ne 0) { throw 'Bootstrap compilation failed.' }

$setupCoreSource = Join-Path $RepositoryRoot 'runtime\NCMMSetupCore.cs'
$setupDiagnosticsSource = Join-Path $RepositoryRoot 'runtime\NCMMSetupDiagnostics.cs'
$setupSource = Join-Path $RepositoryRoot 'runtime\NCMMSetup.cs'
if (-not $PayloadOnly) {
    $setupOut = Join-Path $OutputRoot 'NCMM_Setup.exe'
    & $csc /nologo /target:winexe /optimize+ /platform:x64 `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
        /out:$setupOut `
        $setupCoreSource $setupDiagnosticsSource $setupSource
    if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed.' }

    $diagnosticsHarnessOut = Join-Path $OutputRoot 'NCMM_Diagnostics2_Harness.exe'
    $diagnosticsHarnessSource = Join-Path $RepositoryRoot 'tests\DiagnosticsHarness.cs'
    & $csc /nologo /target:exe /optimize+ /platform:x64 /main:DiagnosticsHarness `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
        /out:$diagnosticsHarnessOut `
        $setupCoreSource $setupDiagnosticsSource $setupSource $diagnosticsHarnessSource
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostics 2.0 harness compilation failed.' }
    & $diagnosticsHarnessOut
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostics 2.0 harness failed.' }
    Remove-Item $diagnosticsHarnessOut -Force -ErrorAction SilentlyContinue

    $failureHarness = Join-Path $RepositoryRoot 'ci\Test-BootstrapFailureHarness.ps1'
    & $failureHarness -RepositoryRoot $RepositoryRoot -BootstrapExe $bootstrapOut
} else {
    Write-Host 'Build-Runtime payload-only mode: release-only C# harnesses and bootstrap failure matrix skipped.' -ForegroundColor DarkGray
}

$awsBuild = Join-Path $OutputRoot '_aws_build'
cmake -S (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings') -B $awsBuild -A x64
if ($LASTEXITCODE -ne 0) { throw 'AWS CMake configure failed.' }
cmake --build $awsBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'AWS build failed.' }
$aws = Get-ChildItem $awsBuild -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
if (-not $aws) { throw 'AWS ncmm_mod.dll not found after build.' }

$smoke = Get-ChildItem $awsBuild -Filter 'ncmm_smoke_host.exe' -Recurse -File | Select-Object -First 1
if (-not $smoke) { throw 'NCMM smoke host not found after build.' }

$manifestPolicyTest = Get-ChildItem $awsBuild -Filter 'ncmm_manifest_policy_test.exe' -Recurse -File | Select-Object -First 1
if (-not $manifestPolicyTest) { throw 'NCMM manifest policy test executable not found after build.' }
& $manifestPolicyTest.FullName
if ($LASTEXITCODE -ne 0) { throw 'NCMM manifest policy test failed.' }

& $smoke.FullName $aws.FullName
if ($LASTEXITCODE -ne 0) { throw 'NCMM/AWS module contract smoke test failed.' }
& $smoke.FullName $aws.FullName '--missing-contract'
if ($LASTEXITCODE -ne 0) { throw 'NCMM/AWS fail-closed smoke test failed.' }

Copy-Item $aws.FullName (Join-Path $payload 'code_mods\AdvancedWorldSettings\ncmm_mod.dll') -Force
Copy-Item (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings\mod.json') (Join-Path $payload 'code_mods\AdvancedWorldSettings\mod.json') -Force
foreach($about in Get-ChildItem (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings') -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
    Copy-Item $about.FullName (Join-Path (Join-Path $payload 'code_mods\AdvancedWorldSettings') $about.Name) -Force
}

$bhcBuild = Join-Path $OutputRoot '_ballistic_hit_chance_build'
cmake -S (Join-Path $RepositoryRoot 'mods\BallisticHitChance') -B $bhcBuild -A x64
if ($LASTEXITCODE -ne 0) { throw 'Ballistic Hit Chance CMake configure failed.' }
cmake --build $bhcBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'Ballistic Hit Chance build failed.' }
$bhc = Get-ChildItem $bhcBuild -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
if (-not $bhc) { throw 'Ballistic Hit Chance ncmm_mod.dll not found after build.' }

& $smoke.FullName $bhc.FullName
if ($LASTEXITCODE -ne 0) { throw 'Ballistic Hit Chance module contract smoke test failed.' }
& $smoke.FullName $bhc.FullName '--missing-contract'
if ($LASTEXITCODE -ne 0) { throw 'Ballistic Hit Chance fail-closed smoke test failed.' }

Copy-Item $bhc.FullName (Join-Path $payload 'code_mods\BallisticHitChance\ncmm_mod.dll') -Force
Copy-Item $ballisticManifestPath (Join-Path $payload 'code_mods\BallisticHitChance\mod.json') -Force
foreach($about in Get-ChildItem (Join-Path $RepositoryRoot 'mods\BallisticHitChance') -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
    Copy-Item $about.FullName (Join-Path (Join-Path $payload 'code_mods\BallisticHitChance') $about.Name) -Force
}

$bhcManifest = $ballisticManifestSource
if ($bhcManifest.loader_api -ne 1 -or $bhcManifest.failure_policy -ne 'disable' -or
    [string]$bhcManifest.version -ne $ballisticVersion) {
    throw "Ballistic Hit Chance manifest contract invalid for version $ballisticVersion."
}
foreach ($required in @(
    'core.v1','locale.v1','api.versioning.v1','host_api.v2.core',
    'settings.typed.v2','runtime_settings.bindings.v2'
)) {
    if (-not ($bhcManifest.requires -contains $required)) {
        throw "Ballistic Hit Chance manifest missing $required"
    }
}
if (($bhcManifest.requires | Select-Object -Unique).Count -ne $bhcManifest.requires.Count) {
    throw 'Ballistic Hit Chance manifest contains duplicate capability requirements.'
}

$ebmBuild = Join-Path $OutputRoot '_equipment_body_map_build'
cmake -S (Join-Path $RepositoryRoot 'mods\EquipmentBodyMap') -B $ebmBuild -A x64
if ($LASTEXITCODE -ne 0) { throw 'Equipment Body Map CMake configure failed.' }
cmake --build $ebmBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'Equipment Body Map build failed.' }
$ebm = Get-ChildItem $ebmBuild -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
if (-not $ebm) { throw 'Equipment Body Map ncmm_mod.dll not found after build.' }

& $smoke.FullName $ebm.FullName
if ($LASTEXITCODE -ne 0) { throw 'Equipment Body Map module contract smoke test failed.' }
& $smoke.FullName $ebm.FullName '--missing-contract'
if ($LASTEXITCODE -ne 0) { throw 'Equipment Body Map fail-closed smoke test failed.' }

Copy-Item $ebm.FullName (Join-Path $payload 'code_mods\EquipmentBodyMap\ncmm_mod.dll') -Force
Copy-Item $equipmentBodyMapManifestPath (Join-Path $payload 'code_mods\EquipmentBodyMap\mod.json') -Force
foreach($about in Get-ChildItem (Join-Path $RepositoryRoot 'mods\EquipmentBodyMap') -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
    Copy-Item $about.FullName (Join-Path (Join-Path $payload 'code_mods\EquipmentBodyMap') $about.Name) -Force
}

$ebmManifest = $equipmentBodyMapManifestSource
if ($ebmManifest.loader_api -ne 1 -or $ebmManifest.failure_policy -ne 'disable' -or
    [string]$ebmManifest.version -ne $equipmentBodyMapVersion) {
    throw "Equipment Body Map manifest contract invalid for version $equipmentBodyMapVersion."
}
foreach ($required in @(
    'core.v1','locale.v1','api.versioning.v1','host_api.v2.core',
    'settings.typed.v2','runtime_settings.bindings.v2'
)) {
    if (-not ($ebmManifest.requires -contains $required)) {
        throw "Equipment Body Map manifest missing $required"
    }
}
if (($ebmManifest.requires | Select-Object -Unique).Count -ne $ebmManifest.requires.Count) {
    throw 'Equipment Body Map manifest contains duplicate capability requirements.'
}

$igBuild = Join-Path $OutputRoot '_item_glyphs_build'
cmake -S (Join-Path $RepositoryRoot 'mods\ItemGlyphs') -B $igBuild -A x64
if ($LASTEXITCODE -ne 0) { throw 'Item Glyphs CMake configure failed.' }
cmake --build $igBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'Item Glyphs build failed.' }
$ig = Get-ChildItem $igBuild -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
if (-not $ig) { throw 'Item Glyphs ncmm_mod.dll not found after build.' }

& $smoke.FullName $ig.FullName
if ($LASTEXITCODE -ne 0) { throw 'Item Glyphs module contract smoke test failed.' }
& $smoke.FullName $ig.FullName '--missing-contract'
if ($LASTEXITCODE -ne 0) { throw 'Item Glyphs fail-closed smoke test failed.' }

Copy-Item $ig.FullName (Join-Path $payload 'code_mods\ItemGlyphs\ncmm_mod.dll') -Force
Copy-Item $itemGlyphsManifestPath (Join-Path $payload 'code_mods\ItemGlyphs\mod.json') -Force
foreach($about in Get-ChildItem (Join-Path $RepositoryRoot 'mods\ItemGlyphs') -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
    Copy-Item $about.FullName (Join-Path (Join-Path $payload 'code_mods\ItemGlyphs') $about.Name) -Force
}

$igManifest = $itemGlyphsManifestSource
if ($igManifest.loader_api -ne 1 -or $igManifest.failure_policy -ne 'disable' -or
    [string]$igManifest.version -ne $itemGlyphsVersion) {
    throw "Item Glyphs manifest contract invalid for version $itemGlyphsVersion."
}
foreach ($required in @(
    'core.v1','locale.v1','api.versioning.v1','host_api.v2.core',
    'settings.typed.v2','runtime_settings.bindings.v2'
)) {
    if (-not ($igManifest.requires -contains $required)) {
        throw "Item Glyphs manifest missing $required"
    }
}
if (($igManifest.requires | Select-Object -Unique).Count -ne $igManifest.requires.Count) {
    throw 'Item Glyphs manifest contains duplicate capability requirements.'
}

$glyphTest = Get-ChildItem $igBuild -Filter 'ncmm_item_glyphs_test.exe' -Recurse -File | Select-Object -First 1
if (-not $glyphTest) { throw 'Item Glyphs classifier test executable missing.' }
& $glyphTest.FullName
if ($LASTEXITCODE -ne 0) { throw 'Item Glyphs classifier/policy test failed.' }

$manifest = $awsManifestSource
if ($manifest.loader_api -ne 1) { throw 'AWS manifest loader_api must be 1.' }
if ($manifest.failure_policy -ne 'disable') { throw 'AWS manifest failure_policy must be disable.' }
foreach ($required in @(
    'core.v1','world_options.v1','world_options.layout.v1','world_settings.v2',
    'world_options.experimental.v1','locale.v1','module_contract.v1','api.versioning.v1',
    'host_api.v2.core','settings.typed.v2','worldgen.bindings.v2'
)) {
    if (-not ($manifest.requires -contains $required)) {
        throw "AWS manifest missing $required"
    }
}
if (($manifest.requires | Select-Object -Unique).Count -ne $manifest.requires.Count) {
    throw 'AWS manifest contains duplicate capability requirements.'
}

$spBuild = Join-Path $OutputRoot '_survivor_progression_build'
cmake -S (Join-Path $RepositoryRoot 'mods\SurvivorProgression') -B $spBuild -A x64
if ($LASTEXITCODE -ne 0) { throw 'Survivor Progression CMake configure failed.' }
cmake --build $spBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'Survivor Progression build failed.' }
$sp = Get-ChildItem $spBuild -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
if (-not $sp) { throw 'Survivor Progression ncmm_mod.dll not found after build.' }

& $smoke.FullName $sp.FullName
if ($LASTEXITCODE -ne 0) { throw 'Survivor Progression vertical-slice smoke test failed.' }

Copy-Item $sp.FullName (Join-Path $payload 'code_mods\SurvivorProgression\ncmm_mod.dll') -Force
Copy-Item (Join-Path $RepositoryRoot 'mods\SurvivorProgression\mod.json') (Join-Path $payload 'code_mods\SurvivorProgression\mod.json') -Force
foreach($about in Get-ChildItem (Join-Path $RepositoryRoot 'mods\SurvivorProgression') -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
    Copy-Item $about.FullName (Join-Path (Join-Path $payload 'code_mods\SurvivorProgression') $about.Name) -Force
}
$survivorData = Join-Path $RepositoryRoot 'mods\SurvivorProgression\data'
if (Test-Path $survivorData -PathType Container) {
    Copy-Item $survivorData (Join-Path $payload 'code_mods\SurvivorProgression\data') -Recurse -Force
}
$survivorPersistentData = Join-Path $RepositoryRoot 'mods\SurvivorProgression\persistent_data'
if (Test-Path $survivorPersistentData -PathType Container) {
    Copy-Item $survivorPersistentData (Join-Path $payload 'code_mods\SurvivorProgression\persistent_data') -Recurse -Force
}
if (-not (Test-Path (Join-Path $payload 'code_mods\SurvivorProgression\persistent_data\dimensional_pouch.json') -PathType Leaf)) {
    throw 'Survivor Progression persistent Dimensional Pouch data was not packaged.'
}
if (-not (Test-Path (Join-Path $payload 'code_mods\SurvivorProgression\persistent_data\mana_hand_carrier.json') -PathType Leaf)) {
    throw 'Survivor Progression persistent Mana Hand carrier data was not packaged.'
}

# Exercise the same production SetupCore used by NCMM_Setup.exe against isolated
# synthetic CDDA installations. The real-install workflow immediately exercises SetupCore
# against official CDDA, so payload-only builds avoid repeating this development matrix.
if (-not $PayloadOnly) {
    & (Join-Path $RepositoryRoot 'ci\Test-InstallationMatrix.ps1') `
        -RepositoryRoot $RepositoryRoot `
        -PayloadRoot $payload `
        -Mode Synthetic
    if ($LASTEXITCODE -ne 0) {
        throw 'NCMM installation lifecycle matrix failed.'
    }
}

$spManifest = $survivorManifestSource
if ($spManifest.loader_api -ne 1 -or $spManifest.failure_policy -ne 'disable' -or
    [string]$spManifest.version -ne $survivorVersion) {
    throw "Survivor Progression manifest contract invalid for version $survivorVersion."
}
foreach ($required in @(
    'core.v1','events.turn.v1','character_state.v1','character.modifiers.v1',
    'ui.basic.v1','ui.tiles.v1','ui.cards.v1','ui.tree.v1','gameplay.metrics.v1',
    'active_mods.v1','ui.theme.v1','active_mods.registry.v2','host_api.v2.core',
    'settings.typed.v2','events.core.v2','character.modifiers.v2','runtime_hooks.registry.v2',
    'ui.layout.v1','module_hotkeys.v1','module_hotkeys.context.v1',
    'api.versioning.v1','state.migration.v1','module.lifecycle.v1'
)) {
    if (-not ($spManifest.requires -contains $required)) {
        throw "Survivor Progression manifest missing $required"
    }
}

if (-not $PayloadOnly) {
    $awsModuleZip = New-NcmmModuleArchive -Folder 'AdvancedWorldSettings' -ComponentId 'advanced_world_settings' -Version $awsVersion
    $survivorModuleZip = New-NcmmModuleArchive -Folder 'SurvivorProgression' -ComponentId 'survivor_progression' -Version $survivorVersion
    $ballisticModuleZip = New-NcmmModuleArchive -Folder 'BallisticHitChance' -ComponentId 'ballistic_hit_chance' -Version $ballisticVersion
    $equipmentBodyMapModuleZip = New-NcmmModuleArchive -Folder 'EquipmentBodyMap' -ComponentId 'equipment_body_map' -Version $equipmentBodyMapVersion
    $itemGlyphsModuleZip = New-NcmmModuleArchive -Folder 'ItemGlyphs' -ComponentId 'item_glyphs' -Version $itemGlyphsVersion
}

# Current NCMM loader hardening is intentionally source-structural: Runtime CI
# guards the invariants even before the certified-host workflow compiles them.
$loaderSource = Get-Content (Join-Path $RepositoryRoot 'host_patch\ncmm_loader.cpp') -Raw
foreach ($requiredLoaderFragment in @(
    'class module_call_scope',
    'bool active_module_matches( const char *module_id )',
    'descriptor_exception',
    'init_exception',
    'shutdown_registered',
    'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH',
    'boot.pending preserved',
    'quarantine_runtime_callback',
    '"runtime_fault"',
    'module_modifiers_quarantined',
    '#include "ncmm_manifest_policy.h"',
    'duplicate_module_id',
    'valid_module_id_v1',
    'ensure_state_migrated',
    'state_migration_failed',
    'api.versioning.v1',
    'module.lifecycle.v1',
    'module_setting_meta',
    'manager_adjust_setting',
    'NCMM_MANAGER',
    'runtime_smoke_requested',
    'gameplay_smoke_requested',
    'run_gameplay_smoke',
    'load_module_data',
    'sync_dimensional_pouch',
    'mg_dimensional_pouch_rank',
    'ncmm_survivor_dimensional_pouch',
    '--ncmm-runtime-smoke',
    '--ncmm-gameplay-smoke',
    'NCMM runtime smoke reached Host ready state'
)) {
    if (-not $loaderSource.Contains($requiredLoaderFragment)) {
        throw "NCMM $hostVersion loader hardening invariant missing: $requiredLoaderFragment"
    }
}

$manifestPolicySource = Get-Content (Join-Path $RepositoryRoot 'host_patch\ncmm_manifest_policy.h') -Raw
foreach ($requiredManifestPolicyFragment in @(
    'manifest_duplicate_key:',
    'manifest_unknown_field:',
    'manifest_missing_field:',
    'valid_utf8_no_controls',
    'validate_manifest_contract_v1'
)) {
    if (-not $manifestPolicySource.Contains($requiredManifestPolicyFragment)) {
        throw "NCMM $hostVersion manifest policy invariant missing: $requiredManifestPolicyFragment"
    }
}

$setupSourceText = (
    (Get-Content (Join-Path $RepositoryRoot 'runtime\NCMMSetupCore.cs') -Raw) +
    "`n" +
    (Get-Content (Join-Path $RepositoryRoot 'runtime\NCMMSetupDiagnostics.cs') -Raw) +
    "`n" +
    (Get-Content (Join-Path $RepositoryRoot 'runtime\NCMMSetup.cs') -Raw)
)
$setupRuntimeVersionMarker = 'internal const string RuntimeVersion = "' + $hostVersion + '";'
foreach ($requiredDiagnosticsFragment in @(
    $setupRuntimeVersionMarker,
    'sb.AppendLine("NCMM v" + RuntimeVersion + " Diagnostics 2.0");',
    '=== Manifest / Duplicate-ID Scan ===',
    '=== Bootstrap Runtime State ===',
    '=== Host Module State ===',
    'diagnostics-latest.txt',
    'Duplicate active module id'
)) {
    if (-not $setupSourceText.Contains($requiredDiagnosticsFragment)) {
        throw "NCMM $hostVersion Diagnostics 2.0 invariant missing: $requiredDiagnosticsFragment"
    }
}

$bootstrapSourceText = Get-Content (Join-Path $RepositoryRoot 'runtime\NCMMBootstrap.cs') -Raw
foreach ($requiredBootstrapFragment in @(
    'public string runtime_version { get; set; }',
    'public string patch_revision { get; set; }',
    'binding.loader_api != LoaderApi',
    'rejected_patch_revision',
    'recoveryBlockedHost',
    'boot.ready proves the previous host reached ready state',
    'runtime_smoke_host_unavailable',
    'gameplay_smoke_host_unavailable',
    '--ncmm-runtime-smoke',
    '--ncmm-gameplay-smoke',
    'stale boot.pending still exists before host launch'
)) {
    if (-not $bootstrapSourceText.Contains($requiredBootstrapFragment)) {
        throw "NCMM $hostVersion bootstrap hardening invariant missing: $requiredBootstrapFragment"
    }
}

$hostPatchSource = Get-Content (Join-Path $RepositoryRoot 'host_patch\Apply-NCMMHostPatch.ps1') -Raw
if ($hostPatchSource.Contains("if (`$LASTEXITCODE -ne 0) { throw 'NCMM source-contract preflight failed.' }")) {
    throw "NCMM $hostVersion regression: PowerShell source-contract preflight still inspects stale LASTEXITCODE."
}

Remove-Item $awsBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $spBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $bhcBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $ebmBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $igBuild -Recurse -Force -ErrorAction SilentlyContinue

if (-not $PayloadOnly) {
@"
NCMM $hostVersion Runtime
===============
1. Run NCMM_Setup.exe.
2. Select the CDDA folder containing cataclysm-tiles.exe.
3. Choose optional components: Advanced World Settings, Survivor Progression, Ballistic Hit Chance, Equipment Body Map, and/or Item Glyphs.
4. Click "Install / Repair selected".
5. Launch CDDA normally from CatLauncher, Catapult, or a shortcut.

The Host/runtime is required. Advanced World Settings, Survivor Progression, Ballistic Hit Chance, Equipment Body Map, and Item Glyphs are independent optional modules.
No compiler, Git, CMake, or MSYS2 is required on the player's PC.
If no exact certified host exists for the installed CDDA executable, NCMM starts vanilla CDDA.
"@ | Set-Content (Join-Path $OutputRoot 'README.txt') -Encoding UTF8

$zip = Join-Path (Split-Path $OutputRoot -Parent) ("NCMM_Runtime_v$hostVersion.zip")
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $OutputRoot '*') -DestinationPath $zip -CompressionLevel Optimal

# Full distribution bundle: ready-to-run runtime at the archive root plus
# standalone component packages for granular installs and repairs.
$fullStage = Join-Path (Split-Path $OutputRoot -Parent) '_full_package'
Remove-Item $fullStage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $fullStage | Out-Null
Copy-Item (Join-Path $OutputRoot '*') $fullStage -Recurse -Force

$packagesDir = Join-Path $fullStage 'packages'
New-Item -ItemType Directory -Force -Path $packagesDir | Out-Null
Copy-Item $zip (Join-Path $packagesDir (Split-Path $zip -Leaf)) -Force
Copy-Item $awsModuleZip (Join-Path $packagesDir (Split-Path $awsModuleZip -Leaf)) -Force
Copy-Item $survivorModuleZip (Join-Path $packagesDir (Split-Path $survivorModuleZip -Leaf)) -Force
Copy-Item $ballisticModuleZip (Join-Path $packagesDir (Split-Path $ballisticModuleZip -Leaf)) -Force
Copy-Item $equipmentBodyMapModuleZip (Join-Path $packagesDir (Split-Path $equipmentBodyMapModuleZip -Leaf)) -Force
Copy-Item $itemGlyphsModuleZip (Join-Path $packagesDir (Split-Path $itemGlyphsModuleZip -Leaf)) -Force

$releaseManifest = [ordered]@{
    schema = 1
    product = 'NCMM Full'
    host_runtime_version = $hostVersion
    recommended_entry = 'NCMM_Setup.exe'
    bundled_installer = $true
    modules = @(
        [ordered]@{ id='advanced_world_settings'; version=$awsVersion; package=(Split-Path $awsModuleZip -Leaf) },
        [ordered]@{ id='survivor_progression'; version=$survivorVersion; package=(Split-Path $survivorModuleZip -Leaf) },
        [ordered]@{ id='ballistic_hit_chance'; version=$ballisticVersion; package=(Split-Path $ballisticModuleZip -Leaf) },
        [ordered]@{ id='equipment_body_map'; version=$equipmentBodyMapVersion; package=(Split-Path $equipmentBodyMapModuleZip -Leaf) },
        [ordered]@{ id='item_glyphs'; version=$itemGlyphsVersion; package=(Split-Path $itemGlyphsModuleZip -Leaf) }
    )
}
$releaseManifest | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $fullStage 'release-manifest.json') -Encoding UTF8

$fullReadme = @(
    "NCMM Full $hostVersion",
    '====================',
    'Recommended: extract this archive and run NCMM_Setup.exe.',
    '',
    "Included directly: NCMM Runtime / Host bootstrap and installer, Advanced World Settings $awsVersion, Survivor Progression $survivorVersion, Ballistic Hit Chance $ballisticVersion, Equipment Body Map $equipmentBodyMapVersion, Item Glyphs $itemGlyphsVersion.",
    '',
    'Standalone packages are preserved in the packages folder.',
    'The installer always installs/repairs NCMM and lets you select AWS, Survivor, Ballistic Hit Chance, Equipment Body Map, and Item Glyphs independently.'
) -join [Environment]::NewLine
Set-Content (Join-Path $fullStage 'FULL_RELEASE.txt') -Value $fullReadme -Encoding UTF8

$fullZip = Join-Path (Split-Path $OutputRoot -Parent) ("NCMM_Full_v$hostVersion.zip")
if (Test-Path $fullZip) { Remove-Item $fullZip -Force }
Compress-Archive -Path (Join-Path $fullStage '*') -DestinationPath $fullZip -CompressionLevel Optimal
Remove-Item $fullStage -Recurse -Force -ErrorAction SilentlyContinue

Write-Output $fullZip
Write-Output $zip
Write-Output $awsModuleZip
Write-Output $survivorModuleZip
Write-Output $ballisticModuleZip
Write-Output $equipmentBodyMapModuleZip
Write-Output $itemGlyphsModuleZip

} else {
    Write-Host 'Build-Runtime payload-only mode: release archives were not generated.' -ForegroundColor DarkGray
    Write-Output $payload
}
