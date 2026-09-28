param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$UpstreamRoot,
    [Parameter(Mandatory=$true)][string]$UpstreamTag,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$UpstreamRoot = (Resolve-Path $UpstreamRoot).Path
$repoTop = Split-Path $RepositoryRoot -Parent
$encodingGuard = Join-Path $RepositoryRoot 'ci\Test-TextEncoding.ps1'
& $encodingGuard -RepoRoot $repoTop
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$commit = (& git -C $UpstreamRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve upstream commit.' }
$patchRevision = (& (Join-Path $RepositoryRoot 'ci\Get-PatchRevision.ps1') -RepositoryRoot $RepositoryRoot).Trim()
if ($patchRevision -notmatch '^[0-9a-f]{64}$') {
    throw "Invalid NCMM patch revision: $patchRevision"
}

function Import-NcmmPayloadFunctions {
    param(
        [Parameter(Mandatory=$true)][string]$PayloadPath,
        [Parameter(Mandatory=$true)][string[]]$Names
    )

    $tokens = $null
    $parseErrors = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseFile(
        $PayloadPath, [ref]$tokens, [ref]$parseErrors )
    if (@($parseErrors).Count -ne 0) {
        $messages = @($parseErrors | ForEach-Object { $_.Message }) -join '; '
        throw "Could not parse canonical NCMM payload: $messages"
    }

    $definitions = @($ast.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst]
    }, $true))

    foreach ($name in $Names) {
        $matches = @($definitions | Where-Object { $_.Name -eq $name })
        if ($matches.Count -ne 1) {
            throw "Canonical payload function '$name' expected exactly once, found $($matches.Count)."
        }

        $definition = [string]$matches[0].Extent.Text
        $pattern = '^\s*function\s+' + [regex]::Escape($name) + '\b'
        $rewriter = New-Object Text.RegularExpressions.Regex(
            $pattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase )
        $scoped = $rewriter.Replace(
            $definition, ('function script:' + $name), 1 )
        if ($scoped -eq $definition) {
            throw "Could not scope canonical payload function '$name'."
        }
        Invoke-Expression $scoped
        if (-not (Get-Command $name -CommandType Function -ErrorAction SilentlyContinue)) {
            throw "Canonical payload function '$name' was not imported."
        }
    }
}

$patchScript = Join-Path $RepositoryRoot 'host_patch\Apply-NCMMHostPatch.ps1'
# Apply-NCMMHostPatch.ps1 owns the stable Host/ABI bridge.  The cumulative payload
# owns additive engine patch layers used by the current modules.  Certified hosts
# must apply the same layers as a local source build or the two installation paths
# silently diverge.
& $patchScript -SourceRoot $UpstreamRoot

$payloadPath = Join-Path $RepositoryRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'
if (-not (Test-Path $payloadPath -PathType Leaf)) {
    throw "Canonical payload is missing: $payloadPath"
}
$stackManifestPath = Join-Path $RepositoryRoot 'ci\host-patch-stack.json'
if (-not (Test-Path $stackManifestPath -PathType Leaf)) {
    throw "Certified-host patch stack manifest is missing: $stackManifestPath"
}
$stackManifest = Get-Content $stackManifestPath -Raw | ConvertFrom-Json
if ([int]$stackManifest.schema -ne 1) {
    throw "Unsupported certified-host patch stack schema: $($stackManifest.schema)"
}
$payloadHelpers = @($stackManifest.helpers | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ })
$engineLayers = @($stackManifest.layers | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ })
if ($payloadHelpers.Count -eq 0 -or $engineLayers.Count -eq 0) {
    throw 'Certified-host patch stack must define helpers and engine layers.'
}
$payloadFunctions = @($payloadHelpers) + @($engineLayers)
if (@($payloadFunctions | Sort-Object -Unique).Count -ne $payloadFunctions.Count) {
    throw 'Certified-host patch stack contains duplicate function names.'
}
Import-NcmmPayloadFunctions -PayloadPath $payloadPath -Names $payloadFunctions

foreach ($layer in $engineLayers) {
    Write-Host "Applying certified-host engine layer: $layer"
    & $layer $UpstreamRoot
}

$buildTimer = [Diagnostics.Stopwatch]::StartNew()
$commonPropsPath = Join-Path $UpstreamRoot 'msvc-full-features\Cataclysm-common.props'
$commonPropsOriginalBytes = $null
Push-Location $UpstreamRoot
try {
    $env:BACKTRACE = '1'
    $env:CDDA_RELEASE_BUILD = '1'
    $env:VCPKG_OVERLAY_TRIPLETS = Join-Path $UpstreamRoot '.github\vcpkg_triplets'

    $msbuildArgs = @(
        '-m',
        '-p:Configuration=Release',
        '-p:Platform=x64',
        '-target:Cataclysm-vcpkg-static',
        'msvc-full-features\Cataclysm-vcpkg-static.sln'
    )

    if ($env:NCMM_SCCACHE_WRAPPER_DIR) {
        $wrapper = Join-Path $env:NCMM_SCCACHE_WRAPPER_DIR 'cl.bat'
        if (-not (Test-Path $wrapper -PathType Leaf)) {
            throw "NCMM sccache wrapper was requested but not found: $wrapper"
        }
        if (-not (Test-Path $commonPropsPath -PathType Leaf)) {
            throw "CDDA compiler props missing: $commonPropsPath"
        }

        # /MP batches translation units into one cl.exe process and defeats
        # per-translation-unit caching.  In CI cache mode MultiToolTask supplies
        # the parallelism while sccache sees one translation unit per invocation.
        # /Z7 keeps debug information inside each object instead of a shared PDB.
        $commonPropsOriginalBytes = [IO.File]::ReadAllBytes($commonPropsPath)
        $commonPropsText = [IO.File]::ReadAllText($commonPropsPath)
        $parallelAnchor = '<MultiProcessorCompilation>true</MultiProcessorCompilation>'
        $debugAnchor = '<DebugInformationFormat>ProgramDatabase</DebugInformationFormat>'
        if (-not $commonPropsText.Contains($parallelAnchor) -or -not $commonPropsText.Contains($debugAnchor)) {
            throw 'CDDA compiler props no longer expose the expected sccache integration anchors.'
        }
        $commonPropsText = $commonPropsText.Replace(
            $parallelAnchor, '<MultiProcessorCompilation>false</MultiProcessorCompilation>')
        $commonPropsText = $commonPropsText.Replace(
            $debugAnchor, '<DebugInformationFormat>OldStyle</DebugInformationFormat>')
        [IO.File]::WriteAllText(
            $commonPropsPath, $commonPropsText, (New-Object Text.UTF8Encoding($false)))

        $msbuildArgs = @(
            '-m',
            '-p:Configuration=Release',
            '-p:Platform=x64',
            '-p:CLToolExe=cl.bat',
            "-p:CLToolPath=$env:NCMM_SCCACHE_WRAPPER_DIR",
            '-p:TrackFileAccess=false',
            '-p:UseMultiToolTask=true',
            '-target:Cataclysm-vcpkg-static',
            'msvc-full-features\Cataclysm-vcpkg-static.sln'
        )
        Write-Host "MSVC compiler cache: ENABLED ($wrapper)"
    } else {
        Write-Host 'MSVC compiler cache: disabled; using upstream /MP settings.'
    }

    & msbuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) { throw 'MSVC CDDA host build failed.' }
} finally {
    if ($null -ne $commonPropsOriginalBytes) {
        [IO.File]::WriteAllBytes($commonPropsPath, $commonPropsOriginalBytes)
    }
    Pop-Location
    $buildTimer.Stop()
}
Write-Host ("MSVC host build elapsed: {0}" -f $buildTimer.Elapsed)

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
    ncmm_version = '0.8.0'
    upstream_tag = $UpstreamTag
    source_commit = $commit
    patch_revision = $patchRevision
    host_sha256 = $hostSha
    vanilla_sha256 = $vanillaHashes.ToArray()
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
