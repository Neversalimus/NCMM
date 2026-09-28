param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$repoTop = Split-Path $RepositoryRoot -Parent
$encodingGuard = Join-Path $RepositoryRoot 'ci\Test-TextEncoding.ps1'
& $encodingGuard -RepoRoot $repoTop
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$payload = Join-Path $OutputRoot 'payload'
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\AdvancedWorldSettings') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\SurvivorProgression') | Out-Null

$hostDescriptor = Get-Content (Join-Path $RepositoryRoot 'components\ncmm_host.json') -Raw | ConvertFrom-Json
$awsManifestPath = Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings\mod.json'
$survivorManifestPath = Join-Path $RepositoryRoot 'mods\SurvivorProgression\mod.json'
$awsManifestSource = Get-Content $awsManifestPath -Raw | ConvertFrom-Json
$survivorManifestSource = Get-Content $survivorManifestPath -Raw | ConvertFrom-Json
$hostVersion = [string]$hostDescriptor.version
$awsVersion = [string]$awsManifestSource.version
$survivorVersion = [string]$survivorManifestSource.version
foreach($pair in @(
    @{Name='Host';Value=$hostVersion},
    @{Name='Advanced World Settings';Value=$awsVersion},
    @{Name='Survivor Progression';Value=$survivorVersion}
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
    } else {
        "NCMM_SurvivorProgression_v$Version.zip"
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

$setupOut = Join-Path $OutputRoot 'NCMM_Setup.exe'
$setupCoreSource = Join-Path $RepositoryRoot 'runtime\NCMMSetupCore.cs'
$setupDiagnosticsSource = Join-Path $RepositoryRoot 'runtime\NCMMSetupDiagnostics.cs'
$setupSource = Join-Path $RepositoryRoot 'runtime\NCMMSetup.cs'
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

$manifest = Get-Content (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings\mod.json') -Raw | ConvertFrom-Json
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

$spManifest = Get-Content (Join-Path $RepositoryRoot 'mods\SurvivorProgression\mod.json') -Raw | ConvertFrom-Json
if ($spManifest.loader_api -ne 1 -or $spManifest.failure_policy -ne 'disable' -or
    [string]$spManifest.version -ne $survivorVersion) {
    throw "Survivor Progression manifest contract invalid for version $survivorVersion."
}
foreach ($required in @(
    'core.v1','events.turn.v1','character_state.v1','character.modifiers.v1',
    'ui.basic.v1','ui.tiles.v1','ui.cards.v1','ui.tree.v1','gameplay.metrics.v1',
    'active_mods.v1','ui.theme.v1','active_mods.registry.v2','host_api.v2.core',
    'events.core.v2','character.modifiers.v2','runtime_hooks.registry.v2',
    'ui.layout.v1','module_hotkeys.v1','module_hotkeys.context.v1',
    'api.versioning.v1','state.migration.v1','module.lifecycle.v1'
)) {
    if (-not ($spManifest.requires -contains $required)) {
        throw "Survivor Progression manifest missing $required"
    }
}

$awsModuleZip = New-NcmmModuleArchive -Folder 'AdvancedWorldSettings' -ComponentId 'advanced_world_settings' -Version $awsVersion
$survivorModuleZip = New-NcmmModuleArchive -Folder 'SurvivorProgression' -ComponentId 'survivor_progression' -Version $survivorVersion

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
    'module.lifecycle.v1'
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

@"
NCMM $hostVersion Runtime
===============
1. Run NCMM_Setup.exe.
2. Select the CDDA folder containing cataclysm-tiles.exe.
3. Choose optional components: Advanced World Settings and/or Survivor Progression.
4. Click "Install / Repair selected".
5. Launch CDDA normally from CatLauncher, Catapult, or a shortcut.

The Host/runtime is required. Advanced World Settings and Survivor Progression are independent optional modules.
No compiler, Git, CMake, or MSYS2 is required on the player's PC.
If no exact certified host exists for the installed CDDA executable, NCMM starts vanilla CDDA.
"@ | Set-Content (Join-Path $OutputRoot 'README.txt') -Encoding UTF8

Remove-Item $awsBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $spBuild -Recurse -Force -ErrorAction SilentlyContinue
$zip = Join-Path (Split-Path $OutputRoot -Parent) ("NCMM_Runtime_v$hostVersion.zip")
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $OutputRoot '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Output $zip
Write-Output $awsModuleZip
Write-Output $survivorModuleZip
