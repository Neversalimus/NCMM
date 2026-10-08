param([string]$GameRoot = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function HashFile([string]$path) {
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}
if ([string]::IsNullOrWhiteSpace($GameRoot)) {
    if ((Split-Path $PSScriptRoot -Leaf) -eq 'ebm-pr187-backup') {
        $GameRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    } else {
        Add-Type -AssemblyName System.Windows.Forms
        $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
        $dialog.Description = 'Choose CDDA folder containing PR187 backup'
        if ($dialog.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { throw 'No game folder selected.' }
        $GameRoot = $dialog.SelectedPath
    }
}
$game = [IO.Path]::GetFullPath($GameRoot.Trim('" ').TrimEnd('\'))
$backup = Join-Path $game 'ncmm\ebm-pr187-backup'
$stateFile = Join-Path $backup 'state.json'
if (-not (Test-Path -LiteralPath $stateFile -PathType Leaf)) {
    throw 'No PR187 backup found; nothing to restore.'
}
$state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
if ([int]$state.schema -ne 1) { throw 'Unknown backup schema.' }
$host = Join-Path $game 'cataclysm-tiles.ncmm.exe'
$binding = Join-Path $game 'ncmm\host.binding.json'
if (-not (Test-Path -LiteralPath $host -PathType Leaf) -or
    (HashFile $host) -ne [string]$state.test_host_sha256) {
    throw 'Host changed after PR187 installation. Refusing to overwrite it. Check backup manually.'
}
if ($state.previous_host_exists) {
    $old = Join-Path $backup 'previous-host.exe'
    if (-not (Test-Path -LiteralPath $old) -or (HashFile $old) -ne [string]$state.previous_host_sha256) {
        throw 'Original Host backup is missing or corrupted; no restoration attempted.'
    }
}
if ($state.previous_binding_exists) {
    $old = Join-Path $backup 'previous-binding.json'
    if (-not (Test-Path -LiteralPath $old) -or (HashFile $old) -ne [string]$state.previous_binding_sha256) {
        throw 'Original Host binding backup is missing or corrupted; no restoration attempted.'
    }
}
if ($state.previous_host_exists) {
    Copy-Item -LiteralPath (Join-Path $backup 'previous-host.exe') -Destination $host -Force
} else {
    Remove-Item -LiteralPath $host -Force
}
if ($state.previous_binding_exists) {
    Copy-Item -LiteralPath (Join-Path $backup 'previous-binding.json') -Destination $binding -Force
} elseif (Test-Path -LiteralPath $binding) {
    Remove-Item -LiteralPath $binding -Force
}
if ($state.previous_host_exists -and (HashFile $host) -ne [string]$state.previous_host_sha256) {
    throw 'Post-restore original Host hash mismatch; backup retained.'
}
if ($state.previous_binding_exists -and (HashFile $binding) -ne [string]$state.previous_binding_sha256) {
    throw 'Post-restore original binding hash mismatch; backup retained.'
}
Remove-Item -LiteralPath $backup -Recurse -Force
foreach ($name in @('RUN_EBM_PR187_TEST.cmd','RESTORE_EBM_PR187_TEST.cmd')) {
    $file = Join-Path $game $name
    if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
}
Write-Host 'PR187 test Host removed. Original Host and binding restored.' -ForegroundColor Green
Write-Host 'Use your regular CatLauncher or NCMM launcher again.'
