param([string]$RepositoryRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path

$paths = @(
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

$Utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Get-CanonicalTextBytes([string]$Path) {
    $raw = [IO.File]::ReadAllBytes($Path)
    $offset = 0
    if ($raw.Length -ge 3 -and
        $raw[0] -eq 0xEF -and $raw[1] -eq 0xBB -and $raw[2] -eq 0xBF) {
        $offset = 3
    }

    try {
        $text = $Utf8Strict.GetString($raw, $offset, $raw.Length - $offset)
    } catch {
        throw "Patch revision input is not valid UTF-8: $Path"
    }

    $text = (($text -replace "`r`n","`n") -replace "`r","`n")
    return $Utf8NoBom.GetBytes($text)
}

$sha = [Security.Cryptography.SHA256]::Create()
$ms = New-Object IO.MemoryStream
try {
    foreach ($rel in $paths) {
        $path = Join-Path $RepositoryRoot $rel
        if (-not (Test-Path $path -PathType Leaf)) { throw "Missing patch input: $rel" }

        $name = $Utf8NoBom.GetBytes($rel + "`n")
        $ms.Write($name, 0, $name.Length)

        $bytes = Get-CanonicalTextBytes $path
        $ms.Write($bytes, 0, $bytes.Length)

        $ms.WriteByte(10)
    }

    $ms.Position = 0
    $hash = $sha.ComputeHash($ms)
    -join ($hash | ForEach-Object { $_.ToString('x2') })
} finally {
    $sha.Dispose()
    $ms.Dispose()
}
