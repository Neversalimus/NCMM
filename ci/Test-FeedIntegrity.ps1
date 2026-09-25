param(
    [string]$FeedPath,
    [string]$RejectedPath,
    [string]$ExpectedRuntimeVersion,
    [string]$ExpectedPatchRevision,
    [string]$Repository = 'Neversalimus/NCMM',
    [switch]$Online,
    [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Is-Hex([string]$Value, [int]$Length) {
    return $null -ne $Value -and $Value -match ("^[0-9a-fA-F]{" + $Length + "}$")
}

function Validate-FeedObject(
    $Feed,
    $Rejected,
    [string]$RuntimeVersion,
    [string]$PatchRevision,
    [string]$Repo,
    [bool]$CheckOnline
) {
    Assert-True ($null -ne $Feed) 'Feed JSON is null.'
    Assert-True ($Feed.schema -eq 1) 'Feed schema must be 1.'
    Assert-True ($Feed.loader_api -eq 1) 'Feed loader_api must be 1.'
    Assert-True (-not [String]::IsNullOrWhiteSpace([string]$Feed.runtime_version)) 'Feed runtime_version is missing.'
    Assert-True (Is-Hex ([string]$Feed.patch_revision) 64) 'Feed patch_revision must be 64 hex characters.'

    if (-not [String]::IsNullOrWhiteSpace($RuntimeVersion)) {
        Assert-True ([String]::Equals([string]$Feed.runtime_version, $RuntimeVersion, [StringComparison]::OrdinalIgnoreCase)) `
            "Feed runtime_version '$($Feed.runtime_version)' != expected '$RuntimeVersion'."
    }
    if (-not [String]::IsNullOrWhiteSpace($PatchRevision)) {
        Assert-True ([String]::Equals([string]$Feed.patch_revision, $PatchRevision, [StringComparison]::OrdinalIgnoreCase)) `
            "Feed patch_revision '$($Feed.patch_revision)' != expected '$PatchRevision'."
    }

    $generated = [DateTimeOffset]::MinValue
    Assert-True ([DateTimeOffset]::TryParse([string]$Feed.generated_utc, [ref]$generated)) 'Feed generated_utc is invalid.'

    Assert-True ($null -ne $Feed.hosts) 'Feed hosts object is missing.'
    $hostProperties = @($Feed.hosts.PSObject.Properties)
    $byTag = @{}
    $releaseCache = @{}
    $shortRevision = ([string]$Feed.patch_revision).Substring(0, 12).ToLowerInvariant()
    $repoPattern = [regex]::Escape($Repo)

    foreach ($prop in $hostProperties) {
        $vanillaSha = [string]$prop.Name
        $entry = $prop.Value
        Assert-True (Is-Hex $vanillaSha 64) "Invalid vanilla SHA key: $vanillaSha"
        Assert-True ($null -ne $entry) "Null feed entry for $vanillaSha."
        Assert-True (Is-Hex ([string]$entry.source_commit) 40) "Invalid source_commit for $vanillaSha."
        Assert-True (-not [String]::IsNullOrWhiteSpace([string]$entry.upstream_tag)) "Missing upstream_tag for $vanillaSha."
        Assert-True (([string]$entry.upstream_tag) -notmatch '[\\/]') "Unsafe upstream_tag for $vanillaSha."
        Assert-True (Is-Hex ([string]$entry.host_sha256) 64) "Invalid host_sha256 for $vanillaSha."
        Assert-True ($entry.loader_api -eq $Feed.loader_api) "loader_api mismatch for $vanillaSha."
        Assert-True ([String]::Equals([string]$entry.ncmm_version, [string]$Feed.runtime_version, [StringComparison]::OrdinalIgnoreCase)) `
            "ncmm_version mismatch for $vanillaSha."
        Assert-True ([String]::Equals([string]$entry.patch_revision, [string]$Feed.patch_revision, [StringComparison]::OrdinalIgnoreCase)) `
            "patch_revision mismatch for $vanillaSha."

        $url = [string]$entry.host_url
        Assert-True (-not [String]::IsNullOrWhiteSpace($url)) "Missing host_url for $vanillaSha."
        $pattern = "^https://github\.com/$repoPattern/releases/download/(?<tag>[^/]+)/cataclysm-tiles\.ncmm\.exe$"
        $match = [regex]::Match($url, $pattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase)
        Assert-True $match.Success "Host URL is not an immutable release asset URL for ${Repo}: $url"
        $releaseTag = $match.Groups['tag'].Value
        $expectedReleaseTag = "ncmm-host-$($entry.upstream_tag)-r$shortRevision"
        Assert-True ([String]::Equals($releaseTag, $expectedReleaseTag, [StringComparison]::Ordinal)) `
            "Release tag '$releaseTag' != expected '$expectedReleaseTag'."

        $tagKey = [string]$entry.upstream_tag
        $signature = "$($entry.source_commit)|$($entry.host_sha256)|$url|$($entry.patch_revision)|$($entry.ncmm_version)|$($entry.loader_api)"
        if ($byTag.ContainsKey($tagKey)) {
            Assert-True ([String]::Equals($byTag[$tagKey], $signature, [StringComparison]::OrdinalIgnoreCase)) `
                "Entries for upstream tag '$tagKey' disagree."
        } else {
            $byTag[$tagKey] = $signature
        }

        if ($CheckOnline -and -not $releaseCache.ContainsKey($releaseTag)) {
            $json = & gh api "repos/$Repo/releases/tags/$releaseTag"
            if ($LASTEXITCODE -ne 0) { throw "GitHub release lookup failed: $releaseTag" }
            $releaseCache[$releaseTag] = $json | ConvertFrom-Json
        }
        if ($CheckOnline) {
            $release = $releaseCache[$releaseTag]
            $assets = @($release.assets | Where-Object { $_.name -eq 'cataclysm-tiles.ncmm.exe' })
            Assert-True ($assets.Count -eq 1) "Release '$releaseTag' must contain exactly one cataclysm-tiles.ncmm.exe asset."
            $asset = $assets[0]
            $expectedDigest = "sha256:$(([string]$entry.host_sha256).ToLowerInvariant())"
            Assert-True ([String]::Equals([string]$asset.digest, $expectedDigest, [StringComparison]::OrdinalIgnoreCase)) `
                "GitHub asset digest mismatch for '$releaseTag': '$($asset.digest)' != '$expectedDigest'."
            Assert-True ([String]::Equals([string]$asset.browser_download_url, $url, [StringComparison]::Ordinal)) `
                "GitHub asset URL mismatch for '$releaseTag'."
        }
    }

    if ($null -ne $Rejected) {
        Assert-True ($Rejected.schema -eq 1) 'Rejected feed schema must be 1.'
        Assert-True ($Rejected.loader_api -eq $Feed.loader_api) 'Rejected feed loader_api mismatch.'
        Assert-True ([String]::Equals([string]$Rejected.patch_revision, [string]$Feed.patch_revision, [StringComparison]::OrdinalIgnoreCase)) `
            'Rejected feed patch_revision must match the active feed revision.'

        if ($null -ne $Rejected.rejected) {
            foreach ($prop in @($Rejected.rejected.PSObject.Properties)) {
                Assert-True (-not $byTag.ContainsKey([string]$prop.Name)) `
                    "Upstream tag '$($prop.Name)' is both certified and rejected for the same patch revision."
            }
        }
    }

    return [pscustomobject]@{
        HostEntries = $hostProperties.Count
        UpstreamTags = $byTag.Count
        OnlineReleases = $releaseCache.Count
    }
}

function New-TestFixture {
    $rev = ('a' * 64)
    $short = $rev.Substring(0, 12)

    $entry = [pscustomobject]@{
        source_commit = ('c' * 40)
        upstream_tag = 'cdda-experimental-2099-01-01-0001'
        host_url = "https://github.com/Neversalimus/NCMM/releases/download/ncmm-host-cdda-experimental-2099-01-01-0001-r$short/cataclysm-tiles.ncmm.exe"
        host_sha256 = ('d' * 64)
        patch_revision = $rev
        ncmm_version = '0.7.2'
        loader_api = 1
    }

    $hosts = [pscustomobject]@{}
    $hosts | Add-Member -NotePropertyName ('b' * 64) -NotePropertyValue $entry

    $feed = [pscustomobject]@{
        schema = 1
        loader_api = 1
        generated_utc = [DateTimeOffset]::UtcNow.ToString('o')
        runtime_version = '0.7.2'
        patch_revision = $rev
        hosts = $hosts
    }

    $rejected = [pscustomobject]@{
        schema = 1
        loader_api = 1
        patch_revision = $rev
        rejected = [pscustomobject]@{}
    }

    return [pscustomobject]@{
        rev = $rev
        feed = $feed
        rejected = $rejected
        entry = $entry
    }
}

function Invoke-SelfTest {
    $fixture = New-TestFixture
    [void](Validate-FeedObject $fixture.feed $fixture.rejected '0.7.2' $fixture.rev 'Neversalimus/NCMM' $false)

    $fixture = New-TestFixture
    $fixture.entry.PSObject.Properties['patch_revision'].Value = ('e' * 64)
    $failed = $false
    try {
        [void](Validate-FeedObject $fixture.feed $fixture.rejected '0.7.2' $fixture.rev 'Neversalimus/NCMM' $false)
    } catch {
        $failed = $true
    }
    Assert-True $failed 'Self-test failed to reject entry/feed patch revision mismatch.'

    $fixture = New-TestFixture
    $fixture.rejected.rejected | Add-Member `
        -NotePropertyName 'cdda-experimental-2099-01-01-0001' `
        -NotePropertyValue ([pscustomobject]@{})
    $failed = $false
    try {
        [void](Validate-FeedObject $fixture.feed $fixture.rejected '0.7.2' $fixture.rev 'Neversalimus/NCMM' $false)
    } catch {
        $failed = $true
    }
    Assert-True $failed 'Self-test failed to reject certified/rejected overlap.'

    $fixture = New-TestFixture
    $fixture.entry.PSObject.Properties['host_url'].Value = 'https://example.invalid/host.exe'
    $failed = $false
    try {
        [void](Validate-FeedObject $fixture.feed $fixture.rejected '0.7.2' $fixture.rev 'Neversalimus/NCMM' $false)
    } catch {
        $failed = $true
    }
    Assert-True $failed 'Self-test failed to reject a non-release host URL.'

    Write-Host 'NCMM Feed Integrity Auditor self-test: PASS' -ForegroundColor Green
}

if ($SelfTest) {
    Invoke-SelfTest
    if ([String]::IsNullOrWhiteSpace($FeedPath)) { exit 0 }
}

if ([String]::IsNullOrWhiteSpace($FeedPath)) { throw 'FeedPath is required unless running SelfTest only.' }
if (-not (Test-Path $FeedPath)) { throw "FeedPath not found: $FeedPath" }
$feedObject = Get-Content $FeedPath -Raw | ConvertFrom-Json

$rejectedObject = $null
if (-not [String]::IsNullOrWhiteSpace($RejectedPath)) {
    if (-not (Test-Path $RejectedPath)) { throw "RejectedPath not found: $RejectedPath" }
    $rejectedObject = Get-Content $RejectedPath -Raw | ConvertFrom-Json
}

$result = Validate-FeedObject $feedObject $rejectedObject $ExpectedRuntimeVersion `
    $ExpectedPatchRevision $Repository ([bool]$Online)

Write-Host ("NCMM Feed Integrity Auditor: PASS ({0} host entries / {1} upstream tags / {2} online releases)." -f `
    $result.HostEntries, $result.UpstreamTags, $result.OnlineReleases) -ForegroundColor Green
