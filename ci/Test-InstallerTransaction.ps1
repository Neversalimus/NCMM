param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'))

# Infrastructure 0.8.3.1 staged installer regression contracts.
$install083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\NCMM.Install.ps1'))
foreach($stageName083 in @('Invoke-NcmmInstallStagePreflight','Invoke-NcmmInstallStageSnapshot','Invoke-NcmmInstallStagePayloadEngine','Invoke-NcmmInstallStageOfflineVerify','Invoke-NcmmInstallStageCommit')){
    if(-not $install083.Contains($stageName083)){throw ('Staged installer orchestration missing: '+$stageName083)}
}
$common083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1'))
foreach($need083 in @('function Invoke-NcmmOfflineVerification','function Invoke-NcmmRuntimeVerification','offline_verify.latest.json','runtime_verify.latest.json','runtime_verification.pending.json')){
    if(-not $common083.Contains($need083)){throw ('Verification split contract missing: '+$need083)}
}
if($install083.Contains('Invoke-NcmmSelfTest')){throw 'Install transaction must not gate commit on runtime module self-test.'}
$commit083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\stages\50-Commit.ps1'))
if(-not $commit083.Contains("runtime_verification='pending_first_host_launch'")){throw 'Runtime verification pending-state contract missing.'}
$offline083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\stages\40-OfflineVerify.ps1'))
if(-not $offline083.Contains('Invoke-NcmmOfflineVerification')){throw 'Offline verification transaction gate missing.'}
$offlineFn083=$common083.IndexOf('function Invoke-NcmmOfflineVerification')
$runtimeFn083=$common083.IndexOf('function Invoke-NcmmRuntimeVerification')
if($offlineFn083 -lt 0 -or $runtimeFn083 -le $offlineFn083){throw 'Verification function ordering invalid.'}
$offlineBody083=$common083.Substring($offlineFn083,$runtimeFn083-$offlineFn083)
foreach($runtimeOnly083 in @('modules.state.json','modules.host_version','modules.api_version','capability.','module.survivor_progression','module.advanced_world_settings')){
    if($offlineBody083.Contains($runtimeOnly083)){throw ('Runtime-only check leaked into offline install verification: '+$runtimeOnly083)}
}
if(-not $offlineBody083.Contains("@('--ncmm-offline','--ncmm-diagnose')")){throw 'Offline verification lost diagnostics-only bootstrap validation.'}
$runtimeBody083=$common083.Substring($runtimeFn083)
foreach($runtimeNeed083 in @('modules.state.json','modules.host_version','modules.api_version','host.boot_ready','modules.fresh_after_install')){if(-not $runtimeBody083.Contains($runtimeNeed083)){throw ('Runtime verification contract missing: '+$runtimeNeed083)}}
foreach($phase083 in @('legacy_generate','api2_migrate','source_preflight','cdda_patch')){if(-not $payload.Contains('Set-InfrastructureTransactionPhase "'+$phase083+'"')){throw ('Payload stage checkpoint missing: '+$phase083)}}

Write-Host 'NCMM staged installer transaction contract: PASS' -ForegroundColor Green
