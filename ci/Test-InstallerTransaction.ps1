param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'))

# Infrastructure 0.8.3.1 staged installer regression contracts.
$install083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\NCMM.Install.ps1'))
foreach($stageName083 in @('Invoke-NcmmInstallStagePreflight','Invoke-NcmmInstallStageSnapshot','Invoke-NcmmInstallStagePayloadEngine','Invoke-NcmmInstallStageOfflineVerify','Invoke-NcmmInstallStageCommit')){
    if(-not $install083.Contains($stageName083)){throw ('Staged installer orchestration missing: '+$stageName083)}
}
. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')
$transaction083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Transaction.ps1'))
foreach($fn083 in @('Invoke-NcmmOfflineVerification','Invoke-NcmmRuntimeVerification')){
    if(-not(Get-Command $fn083 -CommandType Function -ErrorAction SilentlyContinue)){
        throw ('Verification API missing after Infrastructure loader import: '+$fn083)
    }
}
foreach($need083 in @('offline_verify.latest.json','runtime_verify.latest.json','runtime_verification.pending.json')){
    if(-not $transaction083.Contains($need083)){throw ('Verification split contract missing: '+$need083)}
}
if($install083.Contains('Invoke-NcmmSelfTest')){throw 'Install transaction must not gate commit on runtime module self-test.'}
$commit083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\stages\50-Commit.ps1'))
if(-not $commit083.Contains("runtime_verification='pending_first_host_launch'")){throw 'Runtime verification pending-state contract missing.'}
$offline083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\stages\40-OfflineVerify.ps1'))
if(-not $offline083.Contains('Invoke-NcmmOfflineVerification')){throw 'Offline verification transaction gate missing.'}
$offlineFn083=$transaction083.IndexOf('function Invoke-NcmmOfflineVerification')
$runtimeFn083=$transaction083.IndexOf('function Invoke-NcmmRuntimeVerification')
if($offlineFn083 -lt 0 -or $runtimeFn083 -le $offlineFn083){throw 'Verification function ordering invalid.'}
$offlineBody083=$transaction083.Substring($offlineFn083,$runtimeFn083-$offlineFn083)
foreach($runtimeOnly083 in @('modules.state.json','modules.host_version','modules.api_version','capability.','module.survivor_progression','module.advanced_world_settings')){
    if($offlineBody083.Contains($runtimeOnly083)){throw ('Runtime-only check leaked into offline install verification: '+$runtimeOnly083)}
}
if(-not $offlineBody083.Contains("@('--ncmm-offline','--ncmm-diagnose')")){throw 'Offline verification lost diagnostics-only bootstrap validation.'}
$runtimeBody083=$transaction083.Substring($runtimeFn083)
foreach($runtimeNeed083 in @('modules.state.json','modules.host_version','modules.api_version','host.boot_ready','modules.fresh_after_install')){if(-not $runtimeBody083.Contains($runtimeNeed083)){throw ('Runtime verification contract missing: '+$runtimeNeed083)}}
foreach($phase083 in @('legacy_generate','api2_migrate','source_preflight','cdda_patch')){if(-not $payload.Contains('Set-InfrastructureTransactionPhase "'+$phase083+'"')){throw ('Payload stage checkpoint missing: '+$phase083)}}

if(-not $transaction083.Contains("reason='+[string]`$m.reason")){throw 'Runtime verifier no longer reports module reason.'}
if(-not $transaction083.Contains('module_failures=@($moduleFailures)')){throw 'Runtime verifier module failure summary missing.'}

if($transaction083.Contains('code_mods\SurvivorProgression') -or
   $transaction083.Contains('code_mods\AdvancedWorldSettings')){
    throw 'Transaction layer still hardcodes gameplay module directories.'
}
if(-not(Get-Command Get-NcmmManagedModuleInstallations -CommandType Function -ErrorAction SilentlyContinue)){
    throw 'Managed-module discovery API missing.'
}
$tmpModules083=Join-Path $env:TEMP ('NCMM_TX_MODULES_'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force (Join-Path $tmpModules083 'ncmm')|Out-Null
try{
    [ordered]@{
        schema=1;runtime_version='0.8.0';components=@(
            [ordered]@{id='ncmm_host';version='0.8.0';directory=$null},
            [ordered]@{id='survivor_progression';version='0.11.3';directory='SurvivorProgression'}
        )
    }|ConvertTo-Json -Depth 6|Set-Content (Join-Path $tmpModules083 'ncmm\installed-components.json') -Encoding UTF8
    $managed083=@(Get-NcmmManagedModuleInstallations $PackageRoot $tmpModules083)
    if($managed083.Count -ne 1 -or [string]$managed083[0].id -ne 'survivor_progression'){
        throw 'Managed-module discovery did not preserve an independent single-module selection.'
    }
}finally{
    Remove-Item $tmpModules083 -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'NCMM staged installer transaction contract: PASS' -ForegroundColor Green
