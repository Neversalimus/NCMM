param(
    [ValidateSet('Install','Recover')][string]$Mode='Install',
    [string]$GameRoot='',
    [string]$BuildRoot='C:\NCMMBuild',
    [ValidateSet('Safe','Balanced','Maximum')][string]$BuildProfile='Balanced',
    [switch]$AllowStructuralReuse
)
$ErrorActionPreference='Stop'
$PackageRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')
foreach($stage in @('10-Preflight.ps1','20-Snapshot.ps1','30-PayloadEngine.ps1','40-OfflineVerify.ps1','50-Commit.ps1','60-RuntimeVerify.ps1')){. (Join-Path $PSScriptRoot ('stages\'+$stage))}

[void](Assert-NcmmPackageIntegrity $PackageRoot)
Write-Host 'NCMM Infrastructure 0.8.3.1 package integrity: PASS' -ForegroundColor DarkGreen
& (Join-Path $PackageRoot 'ci\Test-Infrastructure083.ps1') -PackageRoot $PackageRoot
if($LASTEXITCODE -ne 0){throw 'Infrastructure 0.8.3.1 static contract failed.'}
# Coordinate with native Setup and Bootstrap using the same retained file lock.
$resolvedRoot=Resolve-NcmmGameRoot $GameRoot
$lockPath=Join-Path $resolvedRoot '.ncmm-install.lock'
if(Test-Path -LiteralPath $lockPath) {
    if((Get-Item -LiteralPath $lockPath -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw 'Installation lock path is a reparse point.'
    }
}
$installLock=[IO.File]::Open($lockPath,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
if($Mode -eq 'Recover'){$r=Recover-NcmmInterruptedTransaction $PackageRoot $GameRoot $BuildRoot;Write-Host ('Recovery status: '+[string]$r.status);exit 0}

$context=Invoke-NcmmInstallStagePreflight $PackageRoot $GameRoot $BuildRoot $BuildProfile ([bool]$AllowStructuralReuse)
$transaction=Invoke-NcmmInstallStageSnapshot $PackageRoot $context $BuildRoot
$snap=$transaction.snapshot;$journal=[string]$transaction.journal
try {
    Invoke-NcmmInstallStagePayloadEngine $PackageRoot $context $transaction $BuildRoot $BuildProfile
    $offline=Invoke-NcmmInstallStageOfflineVerify $PackageRoot $context $transaction
    $done=Invoke-NcmmInstallStageCommit $PackageRoot $context $transaction $offline $BuildRoot
    Write-Host ''
    Write-Host 'NCMM Infrastructure 0.8.3.1: COMMITTED (offline verified)' -ForegroundColor Green
    Write-Host ('Rollback backup: '+$snap.backup)
    Write-Host ('Journal: '+$journal)
    Write-Host ('Offline verification: '+$offline.report_path)
    if($offline.bundle){Write-Host ('Diagnostic bundle: '+$offline.bundle)}
    Write-Host ''
    Write-Host 'Runtime verification is intentionally deferred until the Host has actually loaded code_mods.' -ForegroundColor Cyan
    Write-Host 'Start Cataclysm normally once, reach the main menu or load a world, exit normally, then run: NCMM.cmd selftest' -ForegroundColor Cyan
} catch {
    try{
        Set-NcmmTransactionPhase $journal 'rollback' 'running' $_.Exception.Message
        $failDir=Join-Path $BuildRoot 'transactions';New-Item -ItemType Directory -Force $failDir|Out-Null
        if(Test-Path $journal -PathType Leaf){Copy-Item $journal (Join-Path $failDir ('failed_'+$snap.id+'.journal.json')) -Force}
    }catch{}
    Write-Host '';Write-Host ('Transaction failed: '+$_.Exception.Message) -ForegroundColor Red
    Write-Host 'Restoring pre-install NCMM runtime snapshot...' -ForegroundColor Yellow
    try {Restore-NcmmTransactionSnapshot $snap;Write-Host 'Infrastructure rollback: COMPLETE' -ForegroundColor Green}catch{Write-Host ('Infrastructure rollback failed: '+$_.Exception.Message) -ForegroundColor Red;Write-Host ('Backup: '+$snap.backup) -ForegroundColor Yellow}
    throw
} finally {
    Remove-Item Env:NCMM_INFRA_JOURNAL_PATH -ErrorAction SilentlyContinue
    Remove-Item $snap.stage -Recurse -Force -ErrorAction SilentlyContinue
}

} finally { $installLock.Dispose() }
