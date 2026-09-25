param([Parameter(Mandatory=$true)][string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$revisionScript = Join-Path $RepositoryRoot 'ci\Get-PatchRevision.ps1'

$inputs = @(
    'host_patch/Apply-NCMMHostPatch.ps1',
    'sdk/ncmm_api.h',
    'host_patch/ncmm_loader.h',
    'host_patch/ncmm_loader.cpp',
    'host_patch/ncmm_fault_policy.h',
    'host_patch/ncmm_manifest_policy.h',
    'compat/contracts.json',
    'ci/Test-SourceContracts.ps1',
    'ci/Build-HostPackage.ps1',
    'ci/Get-PatchRevision.ps1'
)

$tempBase = Join-Path ([IO.Path]::GetTempPath()) ('ncmm-patch-revision-' + [guid]::NewGuid().ToString('N'))
$lfRoot = Join-Path $tempBase 'lf'
$crlfRoot = Join-Path $tempBase 'crlf'
$Utf8NoBom = New-Object Text.UTF8Encoding($false)

try {
    foreach ($root in @($lfRoot,$crlfRoot)) {
        New-Item -ItemType Directory -Force -Path $root | Out-Null
    }

    foreach ($rel in $inputs) {
        $source = Join-Path $RepositoryRoot $rel
        if (-not (Test-Path $source -PathType Leaf)) { throw "Missing portability-test input: $rel" }

        $text = [IO.File]::ReadAllText($source)
        $text = (($text -replace "`r`n","`n") -replace "`r","`n")

        foreach ($variant in @(
            [pscustomobject]@{ Root=$lfRoot; Newline="`n" },
            [pscustomobject]@{ Root=$crlfRoot; Newline="`r`n" }
        )) {
            $dest = Join-Path $variant.Root $rel
            New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null
            $variantText = $text.Replace("`n", $variant.Newline)
            [IO.File]::WriteAllText($dest, $variantText, $Utf8NoBom)
        }
    }

    $lf = (& $revisionScript -RepositoryRoot $lfRoot).Trim()
    if ($lf -notmatch '^[0-9a-f]{64}$') {
        throw "LF patch revision calculation failed: $lf"
    }

    $crlf = (& $revisionScript -RepositoryRoot $crlfRoot).Trim()
    if ($crlf -notmatch '^[0-9a-f]{64}$') {
        throw "CRLF patch revision calculation failed: $crlf"
    }

    if ($lf -ne $crlf) {
        throw "Patch revision is line-ending dependent: LF=$lf CRLF=$crlf"
    }

    $current = (& $revisionScript -RepositoryRoot $RepositoryRoot).Trim()
    if ($current -ne $lf) {
        throw "Current checkout patch revision differs from canonical fixture: current=$current canonical=$lf"
    }

    Write-Host "NCMM patch revision portability: PASS ($current)" -ForegroundColor Green
} finally {
    Remove-Item $tempBase -Recurse -Force -ErrorAction SilentlyContinue
}
