function Invoke-NcmmInstallStageSnapshot {
    param([string]$PackageRoot,[object]$Context,[string]$BuildRoot)
    $probe=$Context.probe;$adapter=$Context.adapter;$usingStructural=[bool]$Context.using_structural
    $snap=New-NcmmTransactionSnapshot $probe.game_root $BuildRoot
    New-Item -ItemType Directory -Force (Join-Path $probe.game_root 'ncmm')|Out-Null
    $journal=Join-Path $probe.game_root 'ncmm\transaction.journal.json'
    $pending=[ordered]@{schema=4;infrastructure='0.8.3.1';pipeline='staged-v1';transaction_id=$snap.id;status='pending';current_phase='snapshot';game_root=$probe.game_root;source_commit=$probe.source_commit;adapter=[string]$adapter.id;adapter_inherits=$(if($adapter.inherits){[string]$adapter.inherits}else{$null});structural_reuse=$usingStructural;backup=$snap.backup;journal=$journal;started_utc=[DateTime]::UtcNow.ToString('o')}
    Write-NcmmUtf8NoBom (Join-Path $probe.game_root 'ncmm\transaction.pending.json') (($pending|ConvertTo-Json -Depth 8)+"`n")
    Write-NcmmUtf8NoBom $journal (($pending|ConvertTo-Json -Depth 8)+"`n")
    Set-NcmmTransactionPhase $journal 'snapshot' 'passed' ('backup='+$snap.backup)
    [pscustomobject]@{snapshot=$snap;pending=$pending;journal=$journal}
}
