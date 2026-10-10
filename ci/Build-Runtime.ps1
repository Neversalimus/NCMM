param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [switch]$SkipTextEncoding,
    [switch]$PayloadOnly
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
& (Join-Path $RepositoryRoot 'ci\Test-AuditLegacy.ps1') -RepositoryRoot $RepositoryRoot
& (Join-Path $RepositoryRoot 'ci\Test-LegacyCharacterPoints.ps1') -RepositoryRoot $RepositoryRoot
$encodingGuard = Join-Path $RepositoryRoot 'ci\Test-TextEncoding.ps1'
if (-not $SkipTextEncoding) {
    & $encodingGuard -RepoRoot $RepositoryRoot
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
# Build/test/package modules from one validated, build-only registry.
. (Join-Path $RepositoryRoot 'ci\NativeModuleCatalog.ps1')
$nativeModules=@(Get-NcmmNativeBuildModules -RepositoryRoot $RepositoryRoot)
$moduleById=@{}
$moduleArchivePaths=@()
$payload=Join-Path $OutputRoot 'payload'
foreach($module in $nativeModules) {
    if($moduleById.ContainsKey($module.Id)) { throw "Duplicate native module: $($module.Id)" }
    $moduleById[$module.Id]=$module
    New-Item -ItemType Directory -Force -Path (Join-Path $payload ('code_mods\' + $module.Folder)) | Out-Null
}
$noticeSource = Join-Path $RepositoryRoot 'THIRD_PARTY_NOTICES.txt'
if (-not (Test-Path $noticeSource -PathType Leaf)) {
    throw "NCMM licensing attribution notice is missing: $noticeSource"
}
$hostVersion=(& (Join-Path $RepositoryRoot 'ci\Get-NcmmCurrentVersion.ps1') -RepositoryRoot $RepositoryRoot).Trim()
if($hostVersion -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw "Invalid NCMM runtime version: $hostVersion"
}
foreach($module in $nativeModules) {
    if($module.Version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
        throw "Invalid native module '$($module.Id)' version: $($module.Version)"
    }
}

function New-NcmmModuleArchive {
    param([Parameter(Mandatory=$true)]$Module)
    $folder=[string]$Module.Folder
    $componentId=[string]$Module.Id
    $version=[string]$Module.Version
    $source=Join-Path $payload ('code_mods\' + $folder)
    if(-not(Test-Path (Join-Path $source 'ncmm_mod.dll') -PathType Leaf) -or
       -not(Test-Path (Join-Path $source 'mod.json') -PathType Leaf)) {
        throw "Cannot package incomplete module: $componentId"
    }
    $stage=Join-Path $OutputRoot ('_module_package_' + $componentId)
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    $moduleDest=Join-Path $stage ('code_mods\' + $folder)
    New-Item -ItemType Directory -Force -Path $moduleDest | Out-Null
    Copy-Item (Join-Path $source 'ncmm_mod.dll') (Join-Path $moduleDest 'ncmm_mod.dll') -Force
    Copy-Item (Join-Path $source 'mod.json') (Join-Path $moduleDest 'mod.json') -Force
    foreach($about in Get-ChildItem $source -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue) {
        Copy-Item $about.FullName (Join-Path $moduleDest $about.Name) -Force
    }
    foreach($dir in @('data','persistent_data')) {
        $src=Join-Path $source $dir
        if(Test-Path $src -PathType Container) {
            Copy-Item $src (Join-Path $moduleDest $dir) -Recurse -Force
        }
    }
    Copy-Item (Join-Path $RepositoryRoot ('components\' + $componentId + '.json')) (Join-Path $stage 'component.json') -Force
    Copy-Item $noticeSource (Join-Path $stage 'THIRD_PARTY_NOTICES.txt') -Force
    $readme=@(
        "NCMM native module: $componentId",
        "Version: $version",
        ("Requires: NCMM Host " + $hostVersion),
        "",
        "Preferred installation: run NCMM_Setup.exe and select this component.",
        "Manual fallback: copy the code_mods folder into the selected CDDA installation."
    ) -join [Environment]::NewLine
    Set-Content (Join-Path $stage 'README.txt') -Value $readme -Encoding UTF8
    $zipName="NCMM_$($Module.ArchiveStem)_v$version.zip"
    $zipPath=Join-Path (Split-Path $OutputRoot -Parent) $zipName
    if(Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    return $zipPath
}

$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { throw "Framework csc.exe not found: $csc" }

$bootstrapOut = Join-Path $payload 'cataclysm-tiles.ncmm-bootstrap.exe'
$sharedRuntimeSource = Join-Path $RepositoryRoot 'runtime\NCMMRuntimeIO.cs'
$runtimeManifest = Join-Path $RepositoryRoot 'runtime\NCMMRuntime.manifest'
if (-not (Test-Path $runtimeManifest -PathType Leaf)) { throw 'NCMM runtime manifest missing.' }
$bootstrapSource = Join-Path $RepositoryRoot 'runtime\NCMMBootstrap.cs'
& $csc /nologo /win32manifest:$runtimeManifest /target:winexe /optimize+ /platform:x64 `
    /reference:System.Web.Extensions.dll `
    /out:$bootstrapOut `
    $sharedRuntimeSource $bootstrapSource
if ($LASTEXITCODE -ne 0) { throw 'Bootstrap compilation failed.' }

$setupCoreSource = Join-Path $RepositoryRoot 'runtime\NCMMSetupCore.cs'
$setupDiagnosticsSource = Join-Path $RepositoryRoot 'runtime\NCMMSetupDiagnostics.cs'
$setupSource = Join-Path $RepositoryRoot 'runtime\NCMMSetup.cs'
if (-not $PayloadOnly) {
    $setupOut = Join-Path $OutputRoot 'NCMM_Setup.exe'
    & $csc /nologo /win32manifest:$runtimeManifest /target:winexe /optimize+ /platform:x64 `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
        /out:$setupOut `
        $sharedRuntimeSource $setupCoreSource $setupDiagnosticsSource $setupSource
    if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed.' }

    $diagnosticsHarnessOut = Join-Path $OutputRoot 'NCMM_Diagnostics2_Harness.exe'
    $diagnosticsHarnessSource = Join-Path $RepositoryRoot 'tests\DiagnosticsHarness.cs'
    & $csc /nologo /win32manifest:$runtimeManifest /target:exe /optimize+ /platform:x64 /main:DiagnosticsHarness `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
        /out:$diagnosticsHarnessOut `
        $sharedRuntimeSource $setupCoreSource $setupDiagnosticsSource $setupSource $diagnosticsHarnessSource
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostics 2.0 harness compilation failed.' }
    & $diagnosticsHarnessOut
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostics 2.0 harness failed.' }
    Remove-Item $diagnosticsHarnessOut -Force -ErrorAction SilentlyContinue

    & (Join-Path $RepositoryRoot 'ci\Test-AuditIO.ps1') -RepositoryRoot $RepositoryRoot
    $failureHarness = Join-Path $RepositoryRoot 'ci\Test-BootstrapFailureHarness.ps1'
    & $failureHarness -RepositoryRoot $RepositoryRoot -BootstrapExe $bootstrapOut
} else {
    Write-Host 'Build-Runtime payload-only mode: release-only C# harnesses and bootstrap failure matrix skipped.' -ForegroundColor DarkGray
}

& (Join-Path $RepositoryRoot 'ci\Test-ManaActionWeaponContracts.ps1') -PackageRoot $RepositoryRoot -RunBehavior
if($LASTEXITCODE -ne 0) { throw 'Mana action resolver contract test failed.' }

# Shared runtime/manifest smoke belongs to the platform, not to AWS.
$platformTestBuild=Join-Path $OutputRoot '_platform_tests_build'
cmake -S (Join-Path $RepositoryRoot 'tests') -B $platformTestBuild -A x64
if($LASTEXITCODE -ne 0) { throw 'NCMM platform smoke CMake configure failed.' }
cmake --build $platformTestBuild --config Release
if($LASTEXITCODE -ne 0) { throw 'NCMM platform smoke build failed.' }
$smoke=Get-ChildItem $platformTestBuild -Filter 'ncmm_smoke_host.exe' -Recurse -File | Select-Object -First 1
$manifestPolicyTest=Get-ChildItem $platformTestBuild -Filter 'ncmm_manifest_policy_test.exe' -Recurse -File | Select-Object -First 1
if(-not $smoke -or -not $manifestPolicyTest) {
    throw 'NCMM platform runtime/manifest smoke executable missing.'
}
& $manifestPolicyTest.FullName
if($LASTEXITCODE -ne 0) { throw 'NCMM platform manifest policy test failed.' }

$sdkGuard=Get-ChildItem $platformTestBuild -Filter 'ncmm_sdk_core_guard_test.exe' -Recurse -File | Select-Object -First 1
if(-not $sdkGuard) { throw 'NCMM SDK Core guard contract binary missing.' }
& $sdkGuard.FullName
if($LASTEXITCODE -ne 0) { throw 'NCMM SDK Core guard behavior test failed.' }

foreach($name in @('ncmm_audit_host_boundaries.exe','ncmm_audit_numeric.exe','ncmm_audit_equipment_layout.exe')) {
    $test=Get-ChildItem $platformTestBuild -Filter $name -Recurse -File|Select-Object -First 1
    if(-not $test){throw "Audit regression executable missing: $name"}
    & $test.FullName
    if($LASTEXITCODE -ne 0){throw "Audit regression failed: $name"}
}

# Prove a freshly generated sixth module compiles and obeys Host ABI/capability
# policy before any production module is packaged.
& (Join-Path $RepositoryRoot 'ci\Test-NCMMNativeScaffold.ps1') -RepositoryRoot $RepositoryRoot -Compile -SmokeHostExecutable $smoke.FullName
if($LASTEXITCODE -ne 0) { throw 'NCMM SDK starter module tests failed.' }

# A module ID not referenced by the semantic test harness must still be able to
# initialize and reject missing declared capabilities through generic onboarding.
$fixture=Get-ChildItem $platformTestBuild -Filter 'ncmm_generic_fixture.dll' -Recurse -File | Select-Object -First 1
if(-not $fixture) { throw 'NCMM generic module smoke fixture missing.' }
& $smoke.FullName $fixture.FullName '--generic=ncmm_generic_fixture@0.0.1'
if($LASTEXITCODE -ne 0) { throw 'Generic onboarding init smoke failed.' }
& $smoke.FullName $fixture.FullName '--generic=ncmm_generic_fixture@0.0.1' '--missing-contract'
if($LASTEXITCODE -ne 0) { throw 'Generic onboarding fail-closed smoke failed.' }
& $smoke.FullName $fixture.FullName '--generic=incorrect_fixture@0.0.1'
if($LASTEXITCODE -eq 0) { throw 'Generic onboarding accepted mismatched module ID.' }
& $smoke.FullName $fixture.FullName '--generic=ncmm_generic_fixture@9.9.9'
if($LASTEXITCODE -eq 0) { throw 'Generic onboarding accepted mismatched module version.' }

foreach($module in $nativeModules) {
    $source=Join-Path $RepositoryRoot ('mods\' + $module.Folder)
    $build=Join-Path $OutputRoot $module.BuildDirectory
    cmake -S $source -B $build -A x64
    if($LASTEXITCODE -ne 0) { throw "CMake configure failed: $($module.Id)" }
    cmake --build $build --config Release
    if($LASTEXITCODE -ne 0) { throw "Native module build failed: $($module.Id)" }
    $dll=Get-ChildItem $build -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
    if(-not $dll) { throw "Native module DLL missing after build: $($module.Id)" }

    foreach($exeName in $module.ExtraSmokeExecutables) {
        $extra=Get-ChildItem $build -Filter ([string]$exeName) -Recurse -File | Select-Object -First 1
        if(-not $extra) { throw "Native module '$($module.Id)' extra test executable missing: $exeName" }
        & $extra.FullName
        if($LASTEXITCODE -ne 0) { throw "Native module '$($module.Id)' extra test failed: $exeName" }
    }
    $smokeOptions=@()
    if($module.SmokeProfile -eq 'generic') {
        $smokeOptions=@("--generic=$($module.Id)@$($module.Version)")
    }
    & $smoke.FullName $dll.FullName @smokeOptions
    if($LASTEXITCODE -ne 0) { throw "Native module '$($module.Id)' runtime smoke failed." }
    if($module.MissingContractSmoke) {
        & $smoke.FullName $dll.FullName @smokeOptions '--missing-contract'
        if($LASTEXITCODE -ne 0) { throw "Native module '$($module.Id)' fail-closed smoke failed." }
    }

    $destination=Join-Path $payload ('code_mods\' + $module.Folder)
    Copy-Item $dll.FullName (Join-Path $destination 'ncmm_mod.dll') -Force
    Copy-Item (Join-Path $source 'mod.json') (Join-Path $destination 'mod.json') -Force
    foreach($about in Get-ChildItem $source -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue) {
        Copy-Item $about.FullName (Join-Path $destination $about.Name) -Force
    }
    foreach($dir in @('data','persistent_data')) {
        $extraDir=Join-Path $source $dir
        if(Test-Path $extraDir -PathType Container) {
            Copy-Item $extraDir (Join-Path $destination $dir) -Recurse -Force
        }
    }
    foreach($rel in $module.RequiredPayloadFiles) {
        $required=Join-Path $destination (([string]$rel).Replace('/',[IO.Path]::DirectorySeparatorChar))
        if(-not(Test-Path $required -PathType Leaf)) {
            throw "Native module '$($module.Id)' required packaged file missing: $rel"
        }
    }
    Write-Host ("Native module {0} {1}: compiled, smoke-tested and staged." -f $module.Id,$module.Version) -ForegroundColor Green
}

# The same production SetupCore is exercised against synthetic CDDA installations.
if(-not $PayloadOnly) {
    & (Join-Path $RepositoryRoot 'ci\Test-InstallationMatrix.ps1') -RepositoryRoot $RepositoryRoot -PayloadRoot $payload -Mode Synthetic
    if($LASTEXITCODE -ne 0) { throw 'NCMM installation lifecycle matrix failed.' }
    foreach($module in $nativeModules) {
        $moduleArchivePaths += New-NcmmModuleArchive -Module $module
    }
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

foreach($module in $nativeModules) {
    Remove-Item (Join-Path $OutputRoot $module.BuildDirectory) -Recurse -Force -ErrorAction SilentlyContinue
}
Remove-Item $platformTestBuild -Recurse -Force -ErrorAction SilentlyContinue

if (-not $PayloadOnly) {
@"
NCMM $hostVersion Runtime
===============
1. Run NCMM_Setup.exe.
2. Select the CDDA folder containing cataclysm-tiles.exe.
3. Choose any of the independently selectable native modules bundled with this release.
4. Click "Install / Repair selected".
5. Launch CDDA normally from CatLauncher, Catapult, or a shortcut.

The Host/runtime is required. Native gameplay modules are independently selectable and optional.
No compiler, Git, CMake, or MSYS2 is required on the player's PC.
If no exact certified host exists for the installed CDDA executable, NCMM starts vanilla only when no save-critical native definitions are installed.
"@ | Set-Content (Join-Path $OutputRoot 'README.txt') -Encoding UTF8
Copy-Item $noticeSource (Join-Path $OutputRoot 'THIRD_PARTY_NOTICES.txt') -Force

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
foreach($moduleZip in $moduleArchivePaths) {
    Copy-Item $moduleZip (Join-Path $packagesDir (Split-Path $moduleZip -Leaf)) -Force
}
$releaseManifest=[ordered]@{
    schema=1
    product='NCMM Full'
    source_commit=(git -C $RepositoryRoot rev-parse HEAD).Trim()
    workflow_run=[string]$env:GITHUB_RUN_ID
    runner_image=[string]$env:ImageVersion
    host_runtime_version=$hostVersion
    recommended_entry='NCMM_Setup.exe'
    bundled_installer=$true
    modules=@(
        foreach($module in $nativeModules) {
            [ordered]@{
                id=$module.Id
                version=$module.Version
                package="NCMM_$($module.ArchiveStem)_v$($module.Version).zip"
            }
        }
    )
}
$releaseManifest | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $fullStage 'release-manifest.json') -Encoding UTF8

$moduleNames=@($nativeModules | ForEach-Object { "$($_.Name) $($_.Version)" })
$fullReadme=@(
    "NCMM Full $hostVersion",
    '====================',
    'Recommended: extract this archive and run NCMM_Setup.exe.',
    '',
    ('Included directly: NCMM Runtime / Host bootstrap and installer; ' + ($moduleNames -join '; ') + '.'),
    '',
    'Standalone packages are preserved in the packages folder.',
    'The installer lets you select and repair any bundled native module independently.'
) -join [Environment]::NewLine
Set-Content (Join-Path $fullStage 'FULL_RELEASE.txt') -Value $fullReadme -Encoding UTF8

$fullZip = Join-Path (Split-Path $OutputRoot -Parent) ("NCMM_Full_v$hostVersion.zip")
if (Test-Path $fullZip) { Remove-Item $fullZip -Force }
Compress-Archive -Path (Join-Path $fullStage '*') -DestinationPath $fullZip -CompressionLevel Optimal
Remove-Item $fullStage -Recurse -Force -ErrorAction SilentlyContinue

Write-Output $fullZip
Write-Output $zip
foreach($moduleZip in $moduleArchivePaths) {
    Write-Output $moduleZip
}

} else {
    Write-Host 'Build-Runtime payload-only mode: release archives were not generated.' -ForegroundColor DarkGray
    Write-Output $payload
}
