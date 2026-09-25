param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [string]$ExpectedVersion = '0.7.1'
)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
function Assert-Contains([string]$Rel,[string]$Needle) {
    $path = Join-Path $RepositoryRoot $Rel
    if (-not (Test-Path $path -PathType Leaf)) { throw "Missing version-contract file: $Rel" }
    $text = [IO.File]::ReadAllText($path)
    if (-not $text.Contains($Needle)) { throw "Version consistency failed: $Rel missing: $Needle" }
}
$markers = @(
    @('runtime/NCMMBootstrap.cs',('private const string RuntimeVersion = "'+$ExpectedVersion+'";')),
    @('runtime/NCMMSetup.cs',('NCMM '+$ExpectedVersion+' Setup')),
    @('host_patch/ncmm_loader.cpp',('return "'+$ExpectedVersion+'";')),
    @('ci/Build-HostPackage.ps1',("ncmm_version = '"+$ExpectedVersion+"'")),
    @('.github/workflows/ncmm-runtime.yml',('ncmm-runtime-v'+$ExpectedVersion)),
    @('.github/workflows/ncmm-host.yml',('NCMM '+$ExpectedVersion+' certification')),
    @('.github/workflows/ncmm-feed-audit.yml',("-ExpectedRuntimeVersion '"+$ExpectedVersion+"'")),
    @('tests/smoke_host.cpp',('return "'+$ExpectedVersion+'-smoke";'))
)
foreach ($pair in $markers) { Assert-Contains $pair[0] $pair[1] }
$feed = Get-Content (Join-Path $RepositoryRoot 'feed\index.json') -Raw | ConvertFrom-Json
if (-not [String]::Equals([string]$feed.runtime_version,$ExpectedVersion,[StringComparison]::OrdinalIgnoreCase)) {
    throw "Feed runtime_version '$($feed.runtime_version)' != '$ExpectedVersion'."
}
foreach ($entry in @($feed.hosts.PSObject.Properties | ForEach-Object { $_.Value })) {
    if (-not [String]::Equals([string]$entry.ncmm_version,$ExpectedVersion,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Feed host '$($entry.upstream_tag)' version mismatch."
    }
}
Write-Host "NCMM version consistency: PASS ($ExpectedVersion)" -ForegroundColor Green
