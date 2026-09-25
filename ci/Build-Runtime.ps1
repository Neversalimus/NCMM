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
$setupSource = Join-Path $RepositoryRoot 'runtime\NCMMSetup.cs'
& $csc /nologo /target:winexe /optimize+ /platform:x64 `
    /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
    /out:$setupOut `
    $setupSource
if ($LASTEXITCODE -ne 0) { throw 'Setup compilation failed.' }

$awsBuild = Join-Path $OutputRoot '_aws_build'
cmake -S (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings') -B $awsBuild -A x64
if ($LASTEXITCODE -ne 0) { throw 'AWS CMake configure failed.' }
cmake --build $awsBuild --config Release
if ($LASTEXITCODE -ne 0) { throw 'AWS build failed.' }
$aws = Get-ChildItem $awsBuild -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
if (-not $aws) { throw 'AWS ncmm_mod.dll not found after build.' }

$smoke = Get-ChildItem $awsBuild -Filter 'ncmm_smoke_host.exe' -Recurse -File | Select-Object -First 1
if (-not $smoke) { throw 'NCMM smoke host not found after build.' }
& $smoke.FullName $aws.FullName
if ($LASTEXITCODE -ne 0) { throw 'NCMM/AWS module contract smoke test failed.' }
& $smoke.FullName $aws.FullName '--missing-contract'
if ($LASTEXITCODE -ne 0) { throw 'NCMM/AWS fail-closed smoke test failed.' }

Copy-Item $aws.FullName (Join-Path $payload 'code_mods\AdvancedWorldSettings\ncmm_mod.dll') -Force
Copy-Item (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings\mod.json') (Join-Path $payload 'code_mods\AdvancedWorldSettings\mod.json') -Force

$manifest = Get-Content (Join-Path $RepositoryRoot 'mods\AdvancedWorldSettings\mod.json') -Raw | ConvertFrom-Json
if ($manifest.loader_api -ne 1) { throw 'AWS manifest loader_api must be 1.' }
if ($manifest.failure_policy -ne 'disable') { throw 'AWS manifest failure_policy must be disable.' }
foreach ($required in @('core.v1','world_options.v1','world_options.layout.v1','locale.v1','module_contract.v1')) {
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
if ($spManifest.loader_api -ne 1 -or $spManifest.failure_policy -ne 'disable' -or $spManifest.version -ne '0.8.1') {
    throw 'Survivor Progression 0.8.1 manifest contract invalid.'
}
foreach ($required in @('core.v1','events.turn.v1','character_state.v1','character.modifiers.v1','ui.basic.v1','module_hotkeys.v1')) {
    if (-not ($spManifest.requires -contains $required)) {
        throw "Survivor Progression manifest missing $required"
    }
}

# NCMM 0.6.3 loader hardening is intentionally source-structural: Runtime CI
# guards the invariants even before the certified-host workflow compiles them.
$loaderSource = Get-Content (Join-Path $RepositoryRoot 'host_patch\ncmm_loader.cpp') -Raw
foreach ($requiredLoaderFragment in @(
    'class module_call_scope',
    'bool active_module_matches( const char *module_id )',
    'descriptor_exception',
    'init_exception',
    'shutdown_registered',
    'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH',
    'boot.pending preserved'
)) {
    if (-not $loaderSource.Contains($requiredLoaderFragment)) {
        throw "NCMM 0.6.3 loader hardening invariant missing: $requiredLoaderFragment"
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
        throw "NCMM 0.6.3 bootstrap hardening invariant missing: $requiredBootstrapFragment"
    }
}

$hostPatchSource = Get-Content (Join-Path $RepositoryRoot 'host_patch\Apply-NCMMHostPatch.ps1') -Raw
if ($hostPatchSource.Contains("if (`$LASTEXITCODE -ne 0) { throw 'NCMM source-contract preflight failed.' }")) {
    throw 'NCMM 0.6.3 regression: PowerShell source-contract preflight still inspects stale LASTEXITCODE.'
}

@'
NCMM 0.6.3 Runtime
===============
1. Run NCMM_Setup.exe.
2. Select the CDDA folder containing cataclysm-tiles.exe.
3. Click "Install / Repair NCMM + bundled mods".
4. Launch CDDA normally from CatLauncher, Catapult, or a shortcut.

No compiler, Git, CMake, or MSYS2 is required on the player's PC.
If no exact certified host exists for the installed CDDA executable, NCMM starts vanilla CDDA.
'@ | Set-Content (Join-Path $OutputRoot 'README.txt') -Encoding UTF8

Remove-Item $awsBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $spBuild -Recurse -Force -ErrorAction SilentlyContinue
$zip = Join-Path (Split-Path $OutputRoot -Parent) 'NCMM_Runtime_v0.6.3.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $OutputRoot '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Output $zip
