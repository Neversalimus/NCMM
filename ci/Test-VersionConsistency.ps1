param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [string]$ExpectedVersion = '',
    [string]$LegacyFeedVersion = ''
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path

$canonicalVersion = (& (Join-Path $RepositoryRoot 'ci\Get-NcmmCurrentVersion.ps1') -RepositoryRoot $RepositoryRoot).Trim()
if ([String]::IsNullOrWhiteSpace($ExpectedVersion)) {
    $ExpectedVersion = $canonicalVersion
} elseif (-not [String]::Equals($ExpectedVersion,$canonicalVersion,[StringComparison]::OrdinalIgnoreCase)) {
    throw "Explicit expected version '$ExpectedVersion' differs from canonical Host version '$canonicalVersion'."
}

function Assert-Contains([string]$Rel,[string]$Needle) {
    $path = Join-Path $RepositoryRoot $Rel
    if (-not (Test-Path $path -PathType Leaf)) { throw "Missing version-contract file: $Rel" }
    $text = [IO.File]::ReadAllText($path)
    if (-not $text.Contains($Needle)) { throw "Version consistency failed: $Rel missing: $Needle" }
}

$markers = @(
    @('runtime/NCMMBootstrap.cs',('private const string RuntimeVersion = "'+$ExpectedVersion+'";')),
    @('runtime/NCMMSetupCore.cs',('internal const string RuntimeVersion = "'+$ExpectedVersion+'";')),
    @('runtime/NCMMSetup.cs','Text = "NCMM " + SetupCore.RuntimeVersion + " Setup";'),
    @('host_patch/ncmm_loader.cpp',('return "'+$ExpectedVersion+'";')),
    @('ci/Build-HostPackage.ps1','Get-NcmmCurrentVersion.ps1'),
    @('ci/Build-HostPackage.ps1','ncmm_version = $ncmmVersion'),
    @('.github/workflows/ncmm-runtime.yml','Get-NcmmCurrentVersion.ps1'),
    @('.github/workflows/ncmm-runtime.yml',"Get-ImmutableReleaseTag.ps1 -Prefix 'ncmm-runtime'"),
    @('.github/workflows/ncmm-runtime.yml',"Get-ImmutableReleaseTag.ps1 -Prefix 'ncmm-aws'"),
    @('.github/workflows/ncmm-runtime.yml',"Get-ImmutableReleaseTag.ps1 -Prefix 'ncmm-survivor'"),
    @('.github/workflows/ncmm-host.yml','Get-NcmmCurrentVersion.ps1'),
    @('.github/workflows/ncmm-host.yml','NCMM_VERSION: ${{ needs.discover.outputs.ncmm_version }}'),
    @('.github/workflows/ncmm-feed-audit.yml','Get-NcmmCurrentVersion.ps1'),
    @('tests/smoke_host.cpp',('return "'+$ExpectedVersion+'-smoke";'))
)
foreach ($pair in $markers) { Assert-Contains $pair[0] $pair[1] }

$feed = Get-Content (Join-Path $RepositoryRoot 'feed\index.json') -Raw | ConvertFrom-Json
$feedVersion = [string]$feed.runtime_version
$feedIsCurrent = [String]::Equals($feedVersion,$ExpectedVersion,[StringComparison]::OrdinalIgnoreCase)
$feedIsLegacy = -not [String]::IsNullOrWhiteSpace($LegacyFeedVersion) -and
    [String]::Equals($feedVersion,$LegacyFeedVersion,[StringComparison]::OrdinalIgnoreCase)
if (-not $feedIsCurrent -and -not $feedIsLegacy) {
    if ([String]::IsNullOrWhiteSpace($LegacyFeedVersion)) {
        throw "Feed runtime_version '$feedVersion' != '$ExpectedVersion'."
    }
    throw "Feed runtime_version '$feedVersion' is neither current '$ExpectedVersion' nor allowed migration feed '$LegacyFeedVersion'."
}
foreach ($entry in @($feed.hosts.PSObject.Properties | ForEach-Object { $_.Value })) {
    if (-not [String]::Equals([string]$entry.ncmm_version,$feedVersion,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Feed host '$($entry.upstream_tag)' version '$($entry.ncmm_version)' does not match feed '$feedVersion'."
    }
}
if ($feedIsLegacy) {
    Write-Host "NCMM version consistency: PASS (source $ExpectedVersion; transactional legacy feed $feedVersion)" -ForegroundColor Yellow
} else {
    Write-Host "NCMM version consistency: PASS ($ExpectedVersion)" -ForegroundColor Green
}
