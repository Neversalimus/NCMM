$ErrorActionPreference = 'Stop'

function Write-NcmmUtf8NoBom([string]$Path,[string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))
}
function Get-NcmmHash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-NcmmGitBlobSha1([string]$Path) {
    $bytes=[IO.File]::ReadAllBytes($Path)
    $prefix=[Text.Encoding]::ASCII.GetBytes(('blob '+$bytes.Length+([char]0)))
    $combined=New-Object byte[] ($prefix.Length+$bytes.Length)
    [Array]::Copy($prefix,0,$combined,0,$prefix.Length)
    [Array]::Copy($bytes,0,$combined,$prefix.Length,$bytes.Length)
    $sha=[Security.Cryptography.SHA1]::Create()
    try {
        $hash=$sha.ComputeHash($combined)
        return (($hash|ForEach-Object{$_.ToString('x2')}) -join '')
    } finally { $sha.Dispose() }
}
function Read-NcmmSourceCommit([string]$Root) {
    $version = Join-Path $Root 'VERSION.txt'
    if (Test-Path $version -PathType Leaf) {
        foreach ($line in Get-Content $version -ErrorAction SilentlyContinue) {
            if ($line -match '^\s*commit sha:\s*(\S+)\s*$') { return ([string]$matches[1]).Trim().ToLowerInvariant() }
        }
    }
    $binding = Join-Path $Root 'ncmm\host.binding.json'
    if (Test-Path $binding -PathType Leaf) {
        try { return ([string](Get-Content $binding -Raw | ConvertFrom-Json).source_commit).Trim().ToLowerInvariant() } catch {}
    }
    return ''
}
function Get-NcmmGameTag([string]$Root) {
    $leaf = [IO.Path]::GetFileName(([IO.Path]::GetFullPath($Root)).TrimEnd('\'))
    if ($leaf -match '^cdda_experimental_(\d{4})_(\d{2})_(\d{2})_(\d+)$') {
        return ('cdda-experimental-{0}-{1}-{2}-{3}' -f $matches[1],$matches[2],$matches[3],$matches[4])
    }
    return $leaf.Replace('_','-')
}
function Resolve-NcmmGameRoot([string]$RequestedRoot='') {
    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        $p=[IO.Path]::GetFullPath($RequestedRoot)
        if (-not (Test-Path $p -PathType Container)) { throw "Game root missing: $p" }
        return $p
    }
    $candidates = New-Object System.Collections.Generic.List[object]
    if (-not [string]::IsNullOrWhiteSpace($env:CDDA_ROOT)) { $candidates.Add([pscustomobject]@{Path=$env:CDDA_ROOT;Score=400}) }
    $candidates.Add([pscustomobject]@{Path=(Get-Location).Path;Score=50})
    $assets = Join-Path $env:LOCALAPPDATA 'com.munetmo.cat-launcher\Assets\DarkDaysAhead'
    if (Test-Path $assets -PathType Container) {
        foreach ($d in @(Get-ChildItem $assets -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)) {
            $candidates.Add([pscustomobject]@{Path=$d.FullName;Score=(300-[Math]::Min(250,[int]$candidates.Count))})
        }
    }
    $ranked=@()
    foreach ($c in $candidates) {
        if ([string]::IsNullOrWhiteSpace([string]$c.Path)) { continue }
        try { $p=[IO.Path]::GetFullPath([string]$c.Path) } catch { continue }
        if (-not (Test-Path $p -PathType Container)) { continue }
        if (-not (Test-Path (Join-Path $p 'VERSION.txt') -PathType Leaf)) { continue }
        if (-not (Test-Path (Join-Path $p 'cataclysm-tiles.exe') -PathType Leaf) -and -not (Test-Path (Join-Path $p 'cataclysm-tiles.vanilla.exe') -PathType Leaf)) { continue }
        $commit=Read-NcmmSourceCommit $p
        if ($commit -notmatch '^[0-9a-f]{40}$') { continue }
        $bonus=0
        if (Test-Path (Join-Path $p 'ncmm\host.binding.json') -PathType Leaf) { $bonus+=25 }
        $ranked += [pscustomobject]@{Root=$p;Score=([int]$c.Score+$bonus);Commit=$commit;Modified=(Get-Item $p).LastWriteTimeUtc}
    }
    $best=$ranked | Sort-Object @{Expression='Score';Descending=$true},@{Expression='Modified';Descending=$true} | Select-Object -First 1
    if (-not $best) { throw 'Could not auto-detect a CatLauncher CDDA installation with VERSION.txt.' }
    return [string]$best.Root
}
