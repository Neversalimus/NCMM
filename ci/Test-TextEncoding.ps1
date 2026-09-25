param(
    [Parameter(Mandatory=$true)][string]$RepoRoot,
    [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path $RepoRoot).Path

$Utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$Utf8Bom = [byte[]](0xEF,0xBB,0xBF)

function Get-Utf8BomCount([byte[]]$Bytes) {
    $count = 0
    $offset = 0
    while ($offset + 2 -lt $Bytes.Length -and
           $Bytes[$offset] -eq 0xEF -and
           $Bytes[$offset + 1] -eq 0xBB -and
           $Bytes[$offset + 2] -eq 0xBF) {
        $count++
        $offset += 3
    }
    return $count
}

function Get-EncodingIssue([string]$Path, [bool]$WorkflowFile) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -ge 4) {
        if (($bytes[0] -eq 0x00 -and $bytes[1] -eq 0x00 -and $bytes[2] -eq 0xFE -and $bytes[3] -eq 0xFF) -or
            ($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE -and $bytes[2] -eq 0x00 -and $bytes[3] -eq 0x00)) {
            return 'UTF-32 BOM is not allowed'
        }
    }
    if ($bytes.Length -ge 2) {
        if (($bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) -or
            ($bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF)) {
            return 'UTF-16 BOM is not allowed'
        }
    }

    $bomCount = Get-Utf8BomCount $bytes
    if ($bomCount -gt 1) {
        return "multiple UTF-8 BOM prefixes detected ($bomCount)"
    }
    if ($WorkflowFile -and $bomCount -ne 0) {
        return 'GitHub workflow must be UTF-8 without BOM'
    }

    $offset = $bomCount * 3
    try {
        $text = $Utf8Strict.GetString($bytes, $offset, $bytes.Length - $offset)
    } catch {
        return 'invalid UTF-8 byte sequence'
    }

    if ($text.IndexOf([char]0xFEFF) -ge 0) {
        return 'embedded U+FEFF/BOM marker detected'
    }
    return $null
}

function Assert-Encoding([string]$Path, [bool]$WorkflowFile) {
    $issue = Get-EncodingIssue $Path $WorkflowFile
    if ($issue) {
        throw "Text encoding guard failed: $Path : $issue"
    }
}

function Invoke-SelfTest {
    $temp = Join-Path $env:TEMP ('ncmm-encoding-selftest-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $temp | Out-Null
    try {
        $good = Join-Path $temp 'good.yml'
        [IO.File]::WriteAllText($good, "name: test`n", (New-Object System.Text.UTF8Encoding($false)))
        if (Get-EncodingIssue $good $true) { throw 'Encoding guard self-test rejected valid BOM-free UTF-8.' }

        $single = Join-Path $temp 'single.ps1'
        [IO.File]::WriteAllText($single, "Write-Host 'ok'`n", (New-Object System.Text.UTF8Encoding($true)))
        if (Get-EncodingIssue $single $false) { throw 'Encoding guard self-test rejected a single UTF-8 BOM outside workflows.' }

        $double = Join-Path $temp 'double.yml'
        $payload = [Text.Encoding]::UTF8.GetBytes("name: test`n")
        $bytes = New-Object byte[] ($Utf8Bom.Length * 2 + $payload.Length)
        [Array]::Copy($Utf8Bom, 0, $bytes, 0, $Utf8Bom.Length)
        [Array]::Copy($Utf8Bom, 0, $bytes, $Utf8Bom.Length, $Utf8Bom.Length)
        [Array]::Copy($payload, 0, $bytes, $Utf8Bom.Length * 2, $payload.Length)
        [IO.File]::WriteAllBytes($double, $bytes)
        $doubleIssue = Get-EncodingIssue $double $true
        if (-not $doubleIssue -or -not $doubleIssue.Contains('multiple UTF-8 BOM')) {
            throw 'Encoding guard self-test did not detect a double UTF-8 BOM.'
        }

        $utf16 = Join-Path $temp 'utf16.ps1'
        [IO.File]::WriteAllText($utf16, "Write-Host 'bad'`n", [Text.Encoding]::Unicode)
        $utf16Issue = Get-EncodingIssue $utf16 $false
        if (-not $utf16Issue -or -not $utf16Issue.Contains('UTF-16')) {
            throw 'Encoding guard self-test did not detect UTF-16.'
        }
    } finally {
        Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($SelfTest) {
    Invoke-SelfTest
    Write-Host 'NCMM text encoding guard self-test: OK' -ForegroundColor Green
}

$extensions = @('.ps1','.cs','.cpp','.h','.json','.md','.txt','.yml','.yaml')
$roots = @(
    (Join-Path $RepoRoot '.github\workflows'),
    (Join-Path $RepoRoot 'ncmm-platform')
)

$checked = 0
foreach ($root in $roots) {
    if (-not (Test-Path $root)) { continue }
    foreach ($file in Get-ChildItem $root -Recurse -File) {
        if ($extensions -notcontains $file.Extension.ToLowerInvariant()) { continue }
        $workflow = $file.FullName -like (Join-Path $RepoRoot '.github\workflows\*')
        Assert-Encoding $file.FullName $workflow
        $checked++
    }
}

$editorConfig = Join-Path $RepoRoot '.editorconfig'
if (Test-Path $editorConfig) {
    Assert-Encoding $editorConfig $false
    $checked++
}

if ($checked -eq 0) {
    throw 'NCMM text encoding guard did not find any text files to validate.'
}
Write-Host "NCMM text encoding guard: OK ($checked files)." -ForegroundColor Green
