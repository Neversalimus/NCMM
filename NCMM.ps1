param(
    [ValidateSet('Menu','Install','Update','Check','Plan','Package','Probe','DeepProbe','Adapter','SelfTest','RuntimeVerify','OfflineVerify','Diagnostics','Recover','Migrate','SetFeed','Test')]
    [string]$Action='Menu',
    [string]$GameRoot='',
    [string]$BuildRoot='C:\NCMMBuild',
    [ValidateSet('Safe','Balanced','Maximum')][string]$BuildProfile='Balanced',
    [string[]]$Components=@('all'),
    [string]$PackagePath='',
    [string]$ExpectedPackageSha256='',
    [string]$Url='',
    [switch]$AllowStructuralReuse,
    [switch]$AllowUncertified
)
$ErrorActionPreference='Stop'
$PackageRoot=$PSScriptRoot
trap {
    Write-Host ''
    Write-Host ('NCMM ERROR: '+$_.Exception.Message) -ForegroundColor Red
    try {
        $logDir=Join-Path $PackageRoot 'logs'
        New-Item -ItemType Directory -Force $logDir|Out-Null
        $diag='['+[DateTime]::Now.ToString('s')+'] PowerShell ERROR: '+$_.Exception.ToString()
        if($_.InvocationInfo -and $_.InvocationInfo.PositionMessage){$diag += "`r`nPOSITION: "+$_.InvocationInfo.PositionMessage}
        if($_.ScriptStackTrace){$diag += "`r`nSCRIPT STACK:`r`n"+$_.ScriptStackTrace}
        Add-Content -LiteralPath (Join-Path $logDir 'launcher.log') -Value $diag -Encoding UTF8
    } catch {}
    exit 1
}
. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')

function Invoke-StaticTest {
    & (Join-Path $PackageRoot 'ci\Test-Infrastructure083.ps1') -PackageRoot $PackageRoot
    if($LASTEXITCODE -ne 0){throw 'Infrastructure 0.8.3.1 static contract failed.'}
}
function Invoke-Probe([switch]$Deep) {
    $r=Invoke-NcmmProbe $PackageRoot $GameRoot $BuildRoot
    $outDir=Join-Path $BuildRoot 'probe_reports';New-Item -ItemType Directory -Force $outDir|Out-Null
    $out=Join-Path $outDir ('NCMM_PROBE_'+$r.source_commit.Substring(0,12)+'.json')
    Write-NcmmUtf8NoBom $out (($r|ConvertTo-Json -Depth 12)+"`n")
    if(Test-Path (Join-Path $r.game_root 'ncmm') -PathType Container){Write-NcmmUtf8NoBom (Join-Path $r.game_root 'ncmm\experimental_probe.latest.json') (($r|ConvertTo-Json -Depth 12)+"`n")}
    Write-Host ('NCMM probe: '+$r.status+' | '+$r.source_commit) -ForegroundColor $(if($r.status -eq 'INCOMPATIBLE'){'Red'}elseif($r.status -eq 'EXACT_SUPPORTED'){'Green'}else{'Yellow'})
    Write-Host ('Contracts: '+$r.contracts.status+' | drift changed='+$r.drift_summary.changed+' | report='+$out)
    if($r.status -eq 'INCOMPATIBLE'){throw ('Source contracts failed: '+($r.contracts.failed_contracts -join ', '))}
    if($Deep){$d=Invoke-NcmmDeepSourceProbe $PackageRoot $r $BuildRoot $BuildProfile;Write-Host ('Deep source probe: '+$d.status+' | '+$d.report_path) -ForegroundColor $(if($d.status -eq 'PASS'){'Green'}else{'Red'});if($d.status -ne 'PASS'){throw 'Deep source probe failed.'}}
    return $r
}
function New-Adapter {
    $r=Invoke-Probe -Deep
    $existing=Get-NcmmAdapterForCommit $PackageRoot $r.source_commit
    if($existing){Write-Host ('Adapter already exists/revalidated: '+$existing.script_path) -ForegroundColor Green;return}
    & (Join-Path $PackageRoot 'ci\Test-GoldenRegression.ps1') -PackageRoot $PackageRoot -BuildRoot $BuildRoot
    if($LASTEXITCODE -ne 0){throw 'Golden regression failed.'}
    $base=Get-NcmmBaseAdapter $PackageRoot;if(-not $base){throw 'Base adapter missing.'}
    $dir=Join-Path $PackageRoot 'adapters\generated';New-Item -ItemType Directory -Force $dir|Out-Null
    $path=Join-Path $dir ('cdda_'+$r.source_commit.Substring(0,12)+'.ps1')
    $body=@"
param([ValidateSet('Describe','BeforePayload','AfterPayload')][string]`$Mode='Describe',[string]`$ContextJson='')
`$ErrorActionPreference='Stop'
`$packageRoot=Split-Path (Split-Path `$PSScriptRoot -Parent) -Parent
`$basePath=Join-Path `$packageRoot 'adapters\base\cdda_2026_series.ps1'
`$base=& `$basePath -Mode Describe -ContextJson `$ContextJson
switch(`$Mode){
'Describe'{[pscustomobject]@{schema=2;id='generated-$($r.source_commit.Substring(0,12))';inherits=[string]`$base.id;source_family=[string]`$base.source_family;commit='$($r.source_commit)';tag='$($r.source_tag)';folder='$($r.source_folder)';cache_key='$($r.suggested_cache_key)';vcpkg_commit='$($r.vcpkg_baseline)';payload=[string]`$base.payload;contract_registry=[string]`$base.contract_registry;transform_generation=[string]`$base.transform_generation;support='structural-generated';structural_reuse=`$true;deep_probe='passed';golden_regression='passed';generated_utc='$([DateTime]::UtcNow.ToString('o'))'}}
'BeforePayload'{& `$basePath -Mode BeforePayload -ContextJson `$ContextJson;return}
'AfterPayload'{& `$basePath -Mode AfterPayload -ContextJson `$ContextJson;return}
}
"@
    [IO.File]::WriteAllText($path,$body,(New-Object Text.UTF8Encoding($true)))
    Write-Host ('Generated adapter: '+$path) -ForegroundColor Green
}
function Invoke-RuntimeSelfTest {
    $r=Invoke-NcmmRuntimeVerification $PackageRoot $GameRoot -CreateBundle
    Write-Host ('NCMM runtime verification: '+$r.status+' | '+$r.report_path) -ForegroundColor $(if($r.status -eq 'PASS'){'Green'}else{'Red'})
    if($r.bundle){Write-Host ('Bundle: '+$r.bundle)}
    if($r.status -ne 'PASS'){
        Write-Host 'This does NOT roll back the installed Host.' -ForegroundColor Yellow
        foreach($t in @($r.report.tests|Where-Object{$_.status -eq 'FAIL'})){
            Write-Host ('  FAIL '+[string]$t.id+' :: '+[string]$t.detail) -ForegroundColor Yellow
        }
        if($r.report.instruction){Write-Host ([string]$r.report.instruction) -ForegroundColor Yellow}
        throw ('Runtime verification failed: '+($r.report.failed -join ', '))
    }
}
function Invoke-OfflineVerify {
    $r=Invoke-NcmmOfflineVerification $PackageRoot $GameRoot -LaunchDiagnostics -CreateBundle
    Write-Host ('NCMM offline verification: '+$r.status+' | '+$r.report_path) -ForegroundColor $(if($r.status -eq 'PASS'){'Green'}else{'Red'})
    if($r.bundle){Write-Host ('Bundle: '+$r.bundle)}
    if($r.status -ne 'PASS'){throw ('Offline verification failed: '+($r.report.failed -join ', '))}
}

function Show-Menu {
    Write-Host 'NCMM Infrastructure 0.8.3.1 | Host 0.8.0 | API 2.0 Core' -ForegroundColor Cyan
    Write-Host '1  Install / repair'
    Write-Host '2  Check updates'
    Write-Host '3  Apply certified update'
    Write-Host '4  Probe current experimental'
    Write-Host '5  Deep probe + create adapter'
    Write-Host '6  Runtime verify (after first game launch)'
    Write-Host '7  Collect diagnostics ZIP'
    Write-Host '8  Recover interrupted transaction'
    Write-Host '9  Static package test'
    $c=Read-Host 'Choose action'
    switch($c){
        '1'{ return 'Install' }
        '2'{ return 'Check' }
        '3'{ return 'Update' }
        '4'{ return 'Probe' }
        '5'{ return 'Adapter' }
        '6'{ return 'SelfTest' }
        '7'{ return 'Diagnostics' }
        '8'{ return 'Recover' }
        '9'{ return 'Test' }
        default{ throw 'Unknown menu choice.' }
    }
}

if($Action -eq 'Menu'){ $Action = Show-Menu }
[void](Assert-NcmmPackageIntegrity $PackageRoot)
Write-Host 'NCMM Infrastructure 0.8.3.1 package integrity: PASS' -ForegroundColor DarkGreen
switch($Action){
'Install' { if(@($Components).Count -ne 1 -or $Components[0] -ne 'all'){throw 'For selected components use NCMM_Setup.exe; legacy Install is all-components only.'}; & (Join-Path $PackageRoot 'internal\NCMM.Install.ps1') -Mode Install -GameRoot $GameRoot -BuildRoot $BuildRoot -BuildProfile $BuildProfile -AllowStructuralReuse:$AllowStructuralReuse; exit $LASTEXITCODE }
'Recover' { & (Join-Path $PackageRoot 'internal\NCMM.Install.ps1') -Mode Recover -GameRoot $GameRoot -BuildRoot $BuildRoot; exit $LASTEXITCODE }
'Probe' { [void](Invoke-Probe); exit 0 }
'DeepProbe' { [void](Invoke-Probe -Deep); exit 0 }
'Adapter' { New-Adapter; exit 0 }
'SelfTest' { Invoke-RuntimeSelfTest; exit 0 }
'RuntimeVerify' { Invoke-RuntimeSelfTest; exit 0 }
'OfflineVerify' { Invoke-OfflineVerify; exit 0 }
'Diagnostics' { & (Join-Path $PackageRoot 'tools\Collect-NCMMDiagnostics.ps1') -GameRoot $GameRoot -BuildRoot $BuildRoot; exit $LASTEXITCODE }
'Check' { & (Join-Path $PackageRoot 'tools\Invoke-NCMMUpdate.ps1') -Mode Check -GameRoot $GameRoot -BuildRoot $BuildRoot -Components $Components; exit $LASTEXITCODE }
'Plan' { & (Join-Path $PackageRoot 'tools\Invoke-NCMMUpdate.ps1') -Mode Plan -GameRoot $GameRoot -BuildRoot $BuildRoot -Components $Components; exit $LASTEXITCODE }
'Update' { & (Join-Path $PackageRoot 'tools\Invoke-NCMMUpdate.ps1') -Mode Apply -GameRoot $GameRoot -BuildRoot $BuildRoot -Components $Components -AllowUncertified:$AllowUncertified; exit $LASTEXITCODE }
'Package' { if(-not $PackagePath){throw '-PackagePath is required.'}; & (Join-Path $PackageRoot 'tools\Invoke-NCMMUpdate.ps1') -Mode ApplyPackage -GameRoot $GameRoot -BuildRoot $BuildRoot -PackagePath $PackagePath -Components $Components -ExpectedPackageSha256 $ExpectedPackageSha256; exit $LASTEXITCODE }
'Migrate' { & (Join-Path $PackageRoot 'tools\Invoke-NCMMUpdate.ps1') -Mode MigrateState -GameRoot $GameRoot -BuildRoot $BuildRoot; exit $LASTEXITCODE }
'SetFeed' { if($Url -notmatch '^https://'){throw '-Url must be HTTPS.'};$root=Resolve-NcmmGameRoot $GameRoot;$path=Join-Path $root 'ncmm\update.feed.url';Write-NcmmUtf8NoBom $path ($Url.Trim()+"`n");Write-Host ('Update feed: '+$Url) -ForegroundColor Green;exit 0 }
'Test' { Invoke-StaticTest; exit 0 }
default { throw ('Unhandled action: '+$Action) }
}
