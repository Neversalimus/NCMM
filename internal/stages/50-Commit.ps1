function Invoke-NcmmInstallStageCommit {
    param([string]$PackageRoot,[object]$Context,[object]$Transaction,[object]$OfflineVerification,[string]$BuildRoot)
    $probe=$Context.probe;$adapter=$Context.adapter;$pending=$Transaction.pending;$snap=$Transaction.snapshot;$journal=[string]$Transaction.journal
    & $adapter.script_path -Mode AfterPayload -ContextJson (($pending|ConvertTo-Json -Compress -Depth 8))
    Set-NcmmTransactionPhase $journal 'commit' 'passed' 'offline-verified transaction accepted; runtime verification deferred until first normal Host launch'
    $runtimePending=[ordered]@{schema=1;infrastructure='0.8.3.1';status='pending_first_host_launch';transaction_id=$snap.id;host_version='0.8.0';survivor='0.11.3';advanced_world_settings='0.6.2';created_utc=[DateTime]::UtcNow.ToString('o');instruction='Start Cataclysm normally once, reach the main menu or load a world, exit normally, then run NCMM.cmd selftest.'}
    Write-NcmmUtf8NoBom (Join-Path $probe.game_root 'ncmm\runtime_verification.pending.json') (($runtimePending|ConvertTo-Json -Depth 6)+"`n")
    $done=[ordered]@{schema=4;infrastructure='0.8.3.1';pipeline='staged-v1';transaction_id=$snap.id;status='committed_offline_verified';runtime_verification='pending_first_host_launch';game_root=$probe.game_root;source_commit=$probe.source_commit;adapter=[string]$adapter.id;adapter_inherits=$(if($adapter.inherits){[string]$adapter.inherits}else{$null});structural_reuse=[bool]$Context.using_structural;backup=$snap.backup;journal=$journal;offline_verification=$OfflineVerification.report_path;diagnostic_bundle=$OfflineVerification.bundle;completed_utc=[DateTime]::UtcNow.ToString('o')}
    Write-NcmmUtf8NoBom (Join-Path $probe.game_root 'ncmm\transaction.latest.json') (($done|ConvertTo-Json -Depth 8)+"`n")
    Remove-Item (Join-Path $probe.game_root 'ncmm\transaction.pending.json') -Force -ErrorAction SilentlyContinue
    $reportDir=Join-Path $BuildRoot 'transactions';New-Item -ItemType Directory -Force $reportDir|Out-Null
    Write-NcmmUtf8NoBom (Join-Path $reportDir ('transaction_'+$snap.id+'.json')) (($done|ConvertTo-Json -Depth 8)+"`n")
    return [pscustomobject]$done
}
