param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$payload = Join-Path $OutputRoot 'payload'
New-Item -ItemType Directory -Force -Path (Join-Path $payload 'code_mods\AdvancedWorldSettings') | Out-Null

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
    /reference:System.Windows.Forms.dll /reference:System.Drawing.dll `
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

@'
NCMM 0.4.0 Runtime
===============
1. Run NCMM_Setup.exe.
2. Select the CDDA folder containing cataclysm-tiles.exe.
3. Click "Install / Repair NCMM + AWS".
4. Launch CDDA normally from CatLauncher, Catapult, or a shortcut.

No compiler, Git, CMake, or MSYS2 is required on the player's PC.
If no exact certified host exists for the installed CDDA executable, NCMM starts vanilla CDDA.
'@ | Set-Content (Join-Path $OutputRoot 'README.txt') -Encoding UTF8

Remove-Item $awsBuild -Recurse -Force -ErrorAction SilentlyContinue
$zip = Join-Path (Split-Path $OutputRoot -Parent) 'NCMM_Runtime_v0.4.0.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $OutputRoot '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Output $zip
