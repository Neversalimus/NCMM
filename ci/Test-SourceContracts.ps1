param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [string]$RegistryPath = (Join-Path (Split-Path $PSScriptRoot -Parent) 'compat\contracts.json')
)
$ErrorActionPreference = 'Stop'
$SourceRoot = (Resolve-Path $SourceRoot).Path
$RegistryPath = (Resolve-Path $RegistryPath).Path

$registry = Get-Content $RegistryPath -Raw | ConvertFrom-Json
if ($null -eq $registry -or $registry.schema -ne 1 -or $null -eq $registry.contracts) {
    throw 'Unsupported or invalid NCMM source-contract registry.'
}

$results = @()
$failed = New-Object System.Collections.Generic.List[string]

foreach ($contract in $registry.contracts) {
    $missing = New-Object System.Collections.Generic.List[string]
    foreach ($file in $contract.files) {
        $path = Join-Path $SourceRoot ([string]$file.path)
        if (-not (Test-Path $path)) {
            $missing.Add("missing_file:$($file.path)")
            continue
        }
        $text = [IO.File]::ReadAllText($path)
        foreach ($needle in $file.required) {
            if (-not $text.Contains([string]$needle)) {
                $missing.Add("$($file.path):$needle")
            }
        }
    }

    $status = if ($missing.Count -eq 0) { 'compatible' } else { 'incompatible' }
    if ($status -ne 'compatible') { $failed.Add([string]$contract.id) }

    $results += [ordered]@{
        id = [string]$contract.id
        status = $status
        missing = @($missing)
    }
}

$report = [ordered]@{
    schema = 1
    registry_schema = [int]$registry.schema
    status = if ($failed.Count -eq 0) { 'compatible' } else { 'incompatible' }
    checked_utc = [DateTime]::UtcNow.ToString('o')
    contracts = @($results)
    failed_contracts = @($failed)
}

$reportPath = Join-Path $SourceRoot '.ncmm_contract_report.json'
$report | ConvertTo-Json -Depth 8 | Set-Content $reportPath -Encoding UTF8

if ($failed.Count -ne 0) {
    throw "NCMM source-contract preflight failed: $($failed -join ', ')"
}

Write-Host "NCMM source contracts: compatible ($($results.Count) contracts)."
Write-Output $reportPath
