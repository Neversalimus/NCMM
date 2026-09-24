param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$UpstreamRoot,
    [Parameter(Mandatory=$true)][string]$UpstreamTag,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$UpstreamRoot = (Resolve-Path $UpstreamRoot).Path
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$commit = (& git -C $UpstreamRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve upstream commit.' }
$patchRevision = (& (Join-Path $RepositoryRoot 'ci\Get-PatchRevision.ps1') -RepositoryRoot $RepositoryRoot).Trim()

$patchScript = Join-Path $RepositoryRoot 'host_patch\Apply-NCMMHostPatch.ps1'
& $patchScript -SourceRoot $UpstreamRoot
if ($LASTEXITCODE -ne 0) { throw 'NCMM host contract patch failed.' }

Push-Location $UpstreamRoot
try {
    $env:BACKTRACE = '1'
    $env:CDDA_RELEASE_BUILD = '1'
    $env:VCPKG_OVERLAY_TRIPLETS = Join-Path $UpstreamRoot '.github\vcpkg_triplets'
    & msbuild -m -p:Configuration=Release -p:Platform=x64 '-target:Cataclysm-vcpkg-static;JsonFormatter-vcpkg-static;zzip' 'msvc-full-features\Cataclysm-vcpkg-static.sln'
    if ($LASTEXITCODE -ne 0) { throw 'MSVC CDDA host build failed.' }
} finally { Pop-Location }

$canonicalBuiltHost = Join-Path $UpstreamRoot 'cataclysm-tiles.exe'
if (Test-Path $canonicalBuiltHost) {
    $builtHost = Get-Item $canonicalBuiltHost
} else {
    $builtHost = Get-ChildItem $UpstreamRoot -Filter 'cataclysm-tiles.exe' -Recurse -File |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
}
if (-not $builtHost) { throw 'Built cataclysm-tiles.exe not found.' }

$hostDest = Join-Path $OutputRoot 'cataclysm-tiles.ncmm.exe'
Copy-Item $builtHost.FullName $hostDest -Force
$hostSha = (Get-FileHash $hostDest -Algorithm SHA256).Hash.ToLowerInvariant()

$releaseDir = Join-Path $OutputRoot '_official'
New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
Push-Location $releaseDir
try {
    & gh release download $UpstreamTag -R CleverRaven/Cataclysm-DDA -p 'cdda-windows-with-graphics-x64-*.zip' -p 'cdda-windows-with-graphics-and-sounds-x64-*.zip' --clobber
    if ($LASTEXITCODE -ne 0) { throw 'Could not download official Windows CDDA release assets.' }
} finally { Pop-Location }

$vanillaHashes = New-Object System.Collections.Generic.List[string]
foreach ($zip in Get-ChildItem $releaseDir -Filter '*.zip' -File) {
    $extract = Join-Path $releaseDir ([IO.Path]::GetFileNameWithoutExtension($zip.Name))
    Expand-Archive $zip.FullName $extract -Force
    $exe = Get-ChildItem $extract -Filter 'cataclysm-tiles.exe' -Recurse -File | Select-Object -First 1
    if (-not $exe) { throw "Official asset $($zip.Name) did not contain cataclysm-tiles.exe" }
    $hash = (Get-FileHash $exe.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not $vanillaHashes.Contains($hash)) { $vanillaHashes.Add($hash) }
}
if ($vanillaHashes.Count -eq 0) { throw 'No official vanilla executable hashes were collected.' }

$contractReportPath = Join-Path $UpstreamRoot '.ncmm_contract_report.json'
$contractIds = @()
if (Test-Path $contractReportPath) {
    $contractReport = Get-Content $contractReportPath -Raw | ConvertFrom-Json
    $contractIds = @($contractReport.contracts | Where-Object { $_.status -eq 'compatible' } | ForEach-Object { $_.id })
}

$metadata = [ordered]@{
    schema = 1
    compatibility_schema = 1
    source_contracts = @($contractIds)
    loader_api = 1
    ncmm_version = '0.6.0'
    upstream_tag = $UpstreamTag
    source_commit = $commit
    patch_revision = $patchRevision
    host_sha256 = $hostSha
    vanilla_sha256 = @($vanillaHashes)
    built_utc = [DateTime]::UtcNow.ToString('o')
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputRoot 'host.json') -Encoding UTF8
Remove-Item $releaseDir -Recurse -Force -ErrorAction SilentlyContinue

$zipOut = Join-Path (Split-Path $OutputRoot -Parent) ("ncmm-host-win64-{0}.zip" -f $UpstreamTag)
if (Test-Path $zipOut) { Remove-Item $zipOut -Force }
Compress-Archive -Path $hostDest,(Join-Path $OutputRoot 'host.json') -DestinationPath $zipOut -CompressionLevel Optimal
Write-Host "Host package: $zipOut"
Write-Host "Source commit: $commit"
Write-Host "Vanilla hashes: $($vanillaHashes -join ', ')"
