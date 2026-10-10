param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$UpstreamRoot,
    [Parameter(Mandatory=$true)][string]$UpstreamTag,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$VanillaIdentityCachePath = '',
    [string]$VanillaAssetFingerprint = ''
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$UpstreamRoot = (Resolve-Path $UpstreamRoot).Path
$encodingGuard = Join-Path $RepositoryRoot 'ci\Test-TextEncoding.ps1'
& $encodingGuard -RepoRoot $RepositoryRoot
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$commit = (& git -C $UpstreamRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw 'Could not resolve upstream commit.' }
$patchRevision = (& (Join-Path $RepositoryRoot 'ci\Get-PatchRevision.ps1') -RepositoryRoot $RepositoryRoot).Trim()
if ($patchRevision -notmatch '^[0-9a-f]{64}$') {
    throw "Invalid NCMM patch revision: $patchRevision"
}
$ncmmVersion = (& (Join-Path $RepositoryRoot 'ci\Get-NcmmCurrentVersion.ps1') -RepositoryRoot $RepositoryRoot).Trim()
if ($ncmmVersion -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw "Invalid NCMM version: $ncmmVersion"
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

# Imported payload engine layers execute in this script scope rather than in the
# cumulative payload's original top-level scope. Preserve the one package context
# variable still intentionally consumed by a verifier-only layer.
$script:NcmmRoot = $RepositoryRoot
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

& (Join-Path $RepositoryRoot 'ci\Test-ManaActionWeaponContracts.ps1') `
    -PackageRoot $RepositoryRoot -PatchedSourceRoot $UpstreamRoot

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

# The Host is a modified CDDA executable. Ship the exact upstream license
# accompanying the source commit used for this binary, not a detached copy
# from the NCMM repository or an unrelated/latest CDDA revision.
$upstreamLicense = Join-Path $UpstreamRoot 'LICENSE.txt'
$noticeSource = Join-Path $RepositoryRoot 'THIRD_PARTY_NOTICES.txt'
if (-not (Test-Path $upstreamLicense -PathType Leaf)) {
    throw "Exact upstream CDDA license is missing: $upstreamLicense"
}
if (-not (Test-Path $noticeSource -PathType Leaf)) {
    throw "NCMM third-party attribution notice is missing: $noticeSource"
}
if (-not ((Get-Content $upstreamLicense -Raw).Contains('Creative Commons Attribution-ShareAlike 3.0'))) {
    throw 'Unexpected CDDA license in the exact upstream source; review before distribution.'
}
if (-not ((Get-Content $noticeSource -Raw).Contains('https://creativecommons.org/licenses/by-sa/3.0/'))) {
    throw 'NCMM third-party notice is missing the CDDA license URI.'
}
$licenseDest = Join-Path $OutputRoot 'CDDA_LICENSE.txt'
$noticeDest = Join-Path $OutputRoot 'THIRD_PARTY_NOTICES.txt'
Copy-Item $upstreamLicense $licenseDest -Force
Copy-Item $noticeSource $noticeDest -Force

if (($VanillaIdentityCachePath -and -not $VanillaAssetFingerprint) -or
    ($VanillaAssetFingerprint -and -not $VanillaIdentityCachePath)) {
    throw 'Vanilla identity cache path and asset fingerprint must be supplied together.'
}
if ($VanillaAssetFingerprint -and $VanillaAssetFingerprint -notmatch '^[0-9a-f]{64}$') {
    throw "Invalid vanilla asset fingerprint: $VanillaAssetFingerprint"
}

$vanillaHashes = New-Object System.Collections.Generic.List[string]
$vanillaCacheHit = $false
if ($VanillaIdentityCachePath -and (Test-Path $VanillaIdentityCachePath -PathType Leaf)) {
    try {
        $cached = Get-Content $VanillaIdentityCachePath -Raw | ConvertFrom-Json
        $cachedHashes = @($cached.vanilla_sha256 | ForEach-Object { ([string]$_).ToLowerInvariant() })
        $cacheValid = (
            [int]$cached.schema -eq 1 -and
            [string]$cached.upstream_tag -eq $UpstreamTag -and
            [string]$cached.source_commit -eq $commit -and
            [string]$cached.asset_fingerprint -eq $VanillaAssetFingerprint -and
            $cachedHashes.Count -gt 0 -and
            @($cachedHashes | Where-Object { $_ -notmatch '^[0-9a-f]{64}$' }).Count -eq 0
        )
        if ($cacheValid) {
            foreach ($hash in $cachedHashes) {
                if (-not $vanillaHashes.Contains($hash)) { $vanillaHashes.Add($hash) }
            }
            $vanillaCacheHit = $true
            Write-Host "Official vanilla identity cache: HIT ($VanillaAssetFingerprint)"
        } else {
            Write-Warning 'Official vanilla identity cache was present but did not match the exact release identity; recomputing.'
        }
    } catch {
        Write-Warning "Official vanilla identity cache could not be read; recomputing: $($_.Exception.Message)"
    }
}

$releaseDir = $null
if (-not $vanillaCacheHit) {
    $releaseDir = Join-Path $OutputRoot '_official'
    New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
    Push-Location $releaseDir
    try {
        & gh release download $UpstreamTag -R CleverRaven/Cataclysm-DDA -p 'cdda-windows-with-graphics-x64-*.zip' -p 'cdda-windows-with-graphics-and-sounds-x64-*.zip' --clobber
        if ($LASTEXITCODE -ne 0) { throw 'Could not download official Windows CDDA release assets.' }
    } finally { Pop-Location }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    foreach ($zip in Get-ChildItem $releaseDir -Filter '*.zip' -File) {
        $archive = [IO.Compression.ZipFile]::OpenRead($zip.FullName)
        try {
            $entries = @($archive.Entries | Where-Object {
                [IO.Path]::GetFileName($_.FullName) -ieq 'cataclysm-tiles.exe'
            })
            if ($entries.Count -ne 1) {
                throw "Official asset $($zip.Name) expected exactly one cataclysm-tiles.exe, found $($entries.Count)."
            }

            $stream = $entries[0].Open()
            $hasher = [Security.Cryptography.SHA256]::Create()
            try {
                $hashBytes = $hasher.ComputeHash($stream)
                $hash = -join ($hashBytes | ForEach-Object { $_.ToString('x2') })
            } finally {
                $hasher.Dispose()
                $stream.Dispose()
            }
            if (-not $vanillaHashes.Contains($hash)) { $vanillaHashes.Add($hash) }
        } finally {
            $archive.Dispose()
        }
    }
    if ($vanillaHashes.Count -eq 0) { throw 'No official vanilla executable hashes were collected.' }

    if ($VanillaIdentityCachePath) {
        $cacheParent = Split-Path $VanillaIdentityCachePath -Parent
        New-Item -ItemType Directory -Force -Path $cacheParent | Out-Null
        [ordered]@{
            schema = 1
            upstream_tag = $UpstreamTag
            source_commit = $commit
            asset_fingerprint = $VanillaAssetFingerprint
            vanilla_sha256 = $vanillaHashes.ToArray()
            generated_utc = [DateTime]::UtcNow.ToString('o')
        } | ConvertTo-Json -Depth 4 | Set-Content $VanillaIdentityCachePath -Encoding UTF8
        Write-Host "Official vanilla identity cache: STORED ($VanillaAssetFingerprint)"
    }
}

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
    ncmm_version = $ncmmVersion
    upstream_tag = $UpstreamTag
    source_commit = $commit
    patch_revision = $patchRevision
    host_sha256 = $hostSha
    vanilla_sha256 = $vanillaHashes.ToArray()
    built_utc = [DateTime]::UtcNow.ToString('o')
    build_provenance = [ordered]@{
        ncmm_source = (git -C $RepositoryRoot rev-parse HEAD).Trim()
        workflow_run = [string]$env:GITHUB_RUN_ID
        workflow_attempt = [string]$env:GITHUB_RUN_ATTEMPT
        runner_image = [string]$env:ImageVersion
        toolchain_lock_sha256 = (Get-FileHash (Join-Path $RepositoryRoot 'ci/toolchain.lock.json') -Algorithm SHA256).Hash.ToLowerInvariant()
        vcpkg_commit = 'f6672d8e480ccdecddfad3fd1b838ba369ffe6cd'
        recipe_identity = $patchRevision
    }
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputRoot 'host.json') -Encoding UTF8
if ($releaseDir) {
    Remove-Item $releaseDir -Recurse -Force -ErrorAction SilentlyContinue
}

$zipOut = Join-Path (Split-Path $OutputRoot -Parent) ("ncmm-host-win64-{0}.zip" -f $UpstreamTag)
if (Test-Path $zipOut) { Remove-Item $zipOut -Force }
Compress-Archive -Path $hostDest,(Join-Path $OutputRoot 'host.json'),$licenseDest,$noticeDest -DestinationPath $zipOut -CompressionLevel Optimal
Write-Host "Host package: $zipOut"
Write-Host "Source commit: $commit"
Write-Host "Vanilla hashes: $($vanillaHashes -join ', ')"
