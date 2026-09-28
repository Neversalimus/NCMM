function Invoke-NcmmInstallStageOfflineVerify {
    param([string]$PackageRoot,[object]$Context,[object]$Transaction)
    $journal=[string]$Transaction.journal
    Set-NcmmTransactionPhase $journal 'offline_verify' 'running' 'host/bootstrap hashes, binding, module packages and diagnostics-only validation'
    $verify=Invoke-NcmmOfflineVerification $PackageRoot $Context.probe.game_root -LaunchDiagnostics -CreateBundle
    if($verify.status -ne 'PASS'){throw ('Offline installation verification failed: '+($verify.report.failed -join ', '))}
    Set-NcmmTransactionPhase $journal 'offline_verify' 'passed' $verify.report_path
    return $verify
}
