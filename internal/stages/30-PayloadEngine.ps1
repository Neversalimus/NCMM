function Invoke-NcmmInstallStagePayloadEngine {
    param([string]$PackageRoot,[object]$Context,[object]$Transaction,[string]$BuildRoot,[string]$BuildProfile)
    $probe=$Context.probe;$adapter=$Context.adapter;$pending=$Transaction.pending;$journal=[string]$Transaction.journal
    Set-NcmmTransactionPhase $journal 'payload_engine' 'running' 'legacy generation + API2 migration + CDDA patch/build/install engine'
    & $adapter.script_path -Mode BeforePayload -ContextJson (($pending|ConvertTo-Json -Compress -Depth 8))
    $psExe=Join-Path $PSHOME 'powershell.exe';if(-not(Test-Path $psExe -PathType Leaf)){$psExe='powershell.exe'}
    $supportMode=if([bool]$Context.using_structural){'structural-reuse'}elseif([string]$adapter.support -eq 'exact'){'exact'}else{'generated-structural'}
    $env:NCMM_INFRA_JOURNAL_PATH=$journal
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',[string]$Context.payload,'-GameRoot',$probe.game_root,'-BuildRoot',$BuildRoot,'-BuildProfile',$BuildProfile,'-TargetCommit',$probe.source_commit,'-TargetTag',[string]$Context.tag,'-TargetFolder',[string]$Context.folder,'-TargetCacheKey',[string]$Context.cache,'-TargetVcpkgCommit',[string]$Context.vcpkg,'-TargetSupportMode',$supportMode)
    & $psExe @args
    if($LASTEXITCODE -ne 0){throw "Payload engine failed with exit code $LASTEXITCODE"}
    Set-NcmmTransactionPhase $journal 'payload_engine' 'passed' 'native build/install engine returned success'
}
