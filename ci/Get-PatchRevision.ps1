param([string]$RepositoryRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path

$paths = @(
    'host_patch/Apply-NCMMHostPatch.ps1',
    'compat/survivor_mod_mechanics_v82.contract.txt',
    'compat/world_settings_v2_geography.contract.txt',
    'runtime/NCMMBootstrap.cs',
    'sdk/ncmm_api.h',
    'host_patch/ncmm_loader.h',
    'host_patch/ncmm_loader.cpp',
    'host_patch/ncmm_fault_policy.h',
    'host_patch/ncmm_manifest_policy.h',
    'compat/contracts.json',
    'ci/Test-SourceContracts.ps1',
    'ci/host-patch-stack.json'
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

$payloadRelativePath = 'payload/SURVIVOR_0911_0915_v8.7.6.8.ps1'
$stackManifestRelativePath = 'ci/host-patch-stack.json'
$stackManifestPath = Join-Path $RepositoryRoot $stackManifestRelativePath
if (-not (Test-Path $stackManifestPath -PathType Leaf)) {
    throw "Missing host patch stack manifest: $stackManifestRelativePath"
}
$stackManifest = Get-Content $stackManifestPath -Raw | ConvertFrom-Json
if ([int]$stackManifest.schema -ne 1) {
    throw "Unsupported host patch stack schema: $($stackManifest.schema)"
}
$payloadFunctionNames = @(
    @($stackManifest.helpers | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ }) +
    @($stackManifest.layers | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ })
)
if ($payloadFunctionNames.Count -eq 0 -or
    @($payloadFunctionNames | Sort-Object -Unique).Count -ne $payloadFunctionNames.Count) {
    throw 'Host patch stack function list is empty or contains duplicates.'
}

function Get-CanonicalStringBytes([string]$Text) {
    $normalized = (($Text -replace "`r`n","`n") -replace "`r","`n")
    return $Utf8NoBom.GetBytes($normalized)
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

    $payloadPath = Join-Path $RepositoryRoot $payloadRelativePath
    if (-not (Test-Path $payloadPath -PathType Leaf)) {
        throw "Missing canonical payload for patch revision: $payloadRelativePath"
    }
    $tokens = $null
    $parseErrors = $null
    $payloadAst = [System.Management.Automation.Language.Parser]::ParseFile(
        $payloadPath, [ref]$tokens, [ref]$parseErrors )
    if (@($parseErrors).Count -ne 0) {
        $messages = @($parseErrors | ForEach-Object { $_.Message }) -join '; '
        throw "Canonical payload parse failed during patch revision: $messages"
    }
    $functionDefinitions = @($payloadAst.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst]
    }, $true))

    foreach ($functionName in $payloadFunctionNames) {
        $matches = @($functionDefinitions | Where-Object { $_.Name -eq $functionName })
        if ($matches.Count -ne 1) {
            throw "Patch revision function '$functionName' expected exactly once, found $($matches.Count)."
        }
        $label = $Utf8NoBom.GetBytes($payloadRelativePath + '::' + $functionName + "`n")
        $ms.Write($label, 0, $label.Length)
        $functionBytes = Get-CanonicalStringBytes ([string]$matches[0].Extent.Text)
        $ms.Write($functionBytes, 0, $functionBytes.Length)
        $ms.WriteByte(10)
    }

    $ms.Position = 0
    $hash = $sha.ComputeHash($ms)
    -join ($hash | ForEach-Object { $_.ToString('x2') })
} finally {
    $sha.Dispose()
    $ms.Dispose()
}
