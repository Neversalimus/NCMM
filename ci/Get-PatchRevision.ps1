param([string]$RepositoryRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
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
$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $ms = New-Object System.IO.MemoryStream
    foreach ($rel in $paths) {
        $path = Join-Path $RepositoryRoot $rel
        if (-not (Test-Path $path)) { throw "Missing patch input: $rel" }
        $name = [Text.Encoding]::UTF8.GetBytes($rel + "`n")
        $ms.Write($name,0,$name.Length)
        $bytes = [IO.File]::ReadAllBytes($path)
        $ms.Write($bytes,0,$bytes.Length)
        $nl = [byte[]](10)
        $ms.Write($nl,0,1)
    }
    $ms.Position = 0
    $hash = $sha.ComputeHash($ms)
    -join ($hash | ForEach-Object { $_.ToString('x2') })
} finally {
    $sha.Dispose()
    if ($ms) { $ms.Dispose() }
}
