param([string]$GameRoot = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function HashFile([string]$path) {
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}
if ([string]::IsNullOrWhiteSpace($GameRoot)) {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = 'Choose your CDDA game directory'
    if ($dialog.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { throw 'No game directory selected.' }
    $GameRoot = $dialog.SelectedPath
}
$game = [IO.Path]::GetFullPath($GameRoot.Trim('" ').TrimEnd('\'))
$package = Split-Path -Parent $PSScriptRoot
$sourceExe = Join-Path $package 'test_host\cataclysm-tiles.ncmm.exe'
$sourceMeta = Join-Path $package 'test_host\host.json'
$vanilla = Join-Path $game 'cataclysm-tiles.vanilla.exe'
$exe = Join-Path $game 'cataclysm-tiles.exe'
$ver = Join-Path $game 'VERSION.txt'
$bootstrapHash = Join-Path $game 'ncmm\bootstrap.sha256'
$module = Join-Path $game 'code_mods\EquipmentBodyMap\ncmm_mod.dll'
foreach ($file in @($sourceExe,$sourceMeta,$vanilla,$exe,$ver,$bootstrapHash,$module)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing $file. Run NCMM_Setup.exe first." }
}
$meta = Get-Content -LiteralPath $sourceMeta -Raw | ConvertFrom-Json
if ([string]$meta.source_commit -ne 'e262adb299a7613b4aedc5f12c08fe0413c56a84' -or
    [string]$meta.ncmm_version -ne '0.8.2' -or [int]$meta.loader_api -ne 1) {
    throw 'Unexpected test Host metadata.'
}
$testHash = [string]$meta.host_sha256
if ($testHash -notmatch '^[0-9a-f]{64}$' -or (HashFile $sourceExe) -ne $testHash) {
    throw 'Test Host digest mismatch; installation refused.'
}
if ((HashFile $exe) -ne (Get-Content -LiteralPath $bootstrapHash -Raw).Trim().ToLowerInvariant()) {
    throw 'NCMM bootstrap mismatch. Run NCMM_Setup.exe first.'
}
$commit = [regex]::Match((Get-Content -LiteralPath $ver -Raw), '(?im)^\s*commit sha:\s*([0-9a-f]{40})\s*$')
if (-not $commit.Success -or $commit.Groups[1].Value.ToLowerInvariant() -ne [string]$meta.source_commit) {
    throw 'This PR187 build requires CDDA experimental 2026-09-23-0546. No changes made.'
}
$vanillaHash = HashFile $vanilla
if (@($meta.vanilla_sha256) -notcontains $vanillaHash) {
    throw 'Vanilla EXE hash does not match the certified test build. No changes made.'
}
$backup = Join-Path $game 'ncmm\ebm-pr187-backup'
$host = Join-Path $game 'cataclysm-tiles.ncmm.exe'
$binding = Join-Path $game 'ncmm\host.binding.json'
$run = Join-Path $game 'RUN_EBM_PR187_TEST.cmd'
$undo = Join-Path $game 'RESTORE_EBM_PR187_TEST.cmd'
if ((Test-Path $backup) -or (Test-Path $run) -or (Test-Path $undo)) {
    throw 'An existing PR187 installation/backup was detected. Restore before reinstalling.'
}
$hadHost = Test-Path $host -PathType Leaf
$hadBinding = Test-Path $binding -PathType Leaf
New-Item -ItemType Directory -Path $backup -Force | Out-Null
try {
    if ($hadHost) { Copy-Item -LiteralPath $host -Destination (Join-Path $backup 'previous-host.exe') }
    if ($hadBinding) { Copy-Item -LiteralPath $binding -Destination (Join-Path $backup 'previous-binding.json') }
    $state = [ordered]@{
        schema = 1
        test_host_sha256 = $testHash
        previous_host_exists = $hadHost
        previous_binding_exists = $hadBinding
        previous_host_sha256 = $(if($hadHost){HashFile $host}else{$null})
        previous_binding_sha256 = $(if($hadBinding){HashFile $binding}else{$null})
    }
    $utf8 = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText((Join-Path $backup 'state.json'), ($state | ConvertTo-Json -Depth 4), $utf8)
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Restore_EBM_Test.ps1') -Destination (Join-Path $backup 'Restore_EBM_Test.ps1')
    Copy-Item -LiteralPath $sourceExe -Destination $host -Force
    if ((HashFile $host) -ne $testHash) { throw 'Installed test Host hash mismatch.' }
    $newBinding = [ordered]@{
        vanilla_sha256 = $vanillaHash
        host_sha256 = $testHash
        source_commit = [string]$meta.source_commit
        upstream_tag = [string]$meta.upstream_tag
        patch_revision = [string]$meta.patch_revision
        ncmm_version = [string]$meta.ncmm_version
        loader_api = [int]$meta.loader_api
        installed_utc = [DateTime]::UtcNow.ToString('o')
    }
    [IO.File]::WriteAllText($binding, ($newBinding | ConvertTo-Json -Depth 5), $utf8)
    $nl = [Environment]::NewLine
    $runText = '@echo off' + $nl + 'cd /d "%~dp0"' + $nl + 'start "" "%~dp0cataclysm-tiles.exe" --ncmm-offline' + $nl
    [IO.File]::WriteAllText($run, $runText, [Text.Encoding]::ASCII)
    $restoreText = '@echo off' + $nl + 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ncmm\ebm-pr187-backup\Restore_EBM_Test.ps1" -GameRoot "%~dp0"' + $nl + 'pause' + $nl
    [IO.File]::WriteAllText($undo, $restoreText, [Text.Encoding]::ASCII)
    Write-Host 'PR187 Equipment Body Map test Host INSTALLED.' -ForegroundColor Green
    Write-Host "Game: $game"
    Write-Host 'Launch via RUN_EBM_PR187_TEST.cmd (offline: prevents online Host replacement).'
    Write-Host 'Restore via RESTORE_EBM_PR187_TEST.cmd.'
} catch {
    try {
        if ($hadHost) { Copy-Item -LiteralPath (Join-Path $backup 'previous-host.exe') -Destination $host -Force }
        elseif (Test-Path $host) { Remove-Item -LiteralPath $host -Force }
        if ($hadBinding) { Copy-Item -LiteralPath (Join-Path $backup 'previous-binding.json') -Destination $binding -Force }
        elseif (Test-Path $binding) { Remove-Item -LiteralPath $binding -Force }
        foreach ($file in @($run,$undo)) { if (Test-Path $file) { Remove-Item -LiteralPath $file -Force } }
        Remove-Item -LiteralPath $backup -Recurse -Force
    } catch { Write-Warning 'Automatic rollback failed; original files are in the PR187 backup directory.' }
    throw
}
