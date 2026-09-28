function Invoke-NcmmInstallStagePreflight {
    param([string]$PackageRoot,[string]$GameRoot,[string]$BuildRoot,[string]$BuildProfile,[bool]$AllowStructuralReuse)
    $resolvedRoot=Resolve-NcmmGameRoot $GameRoot
    [void](Recover-NcmmInterruptedTransaction $PackageRoot $resolvedRoot $BuildRoot)
    $probe=Invoke-NcmmProbe $PackageRoot $resolvedRoot $BuildRoot
    Write-Host ('Compatibility: '+$probe.status) -ForegroundColor $(if($probe.status -eq 'EXACT_SUPPORTED'){'Green'}elseif($probe.status -in @('STRUCTURALLY_COMPATIBLE','STRUCTURAL_ADAPTER')){'Yellow'}else{'Red'})
    if($probe.feed_entry){Write-Host ('Feed: '+[string]$probe.feed_entry.status+' / certification '+[string]$probe.feed_entry.certification.state) -ForegroundColor DarkCyan}else{Write-Host 'Feed: unknown commit' -ForegroundColor Yellow}
    if($probe.status -eq 'INCOMPATIBLE'){throw ('Source contracts failed: '+($probe.contracts.failed_contracts -join ', '))}
    $adapter=Get-NcmmAdapterForCommit $PackageRoot $probe.source_commit
    $usingStructural=$false
    if(-not $adapter){
        if(-not $AllowStructuralReuse){Write-Host '';Write-Host 'This experimental is structurally compatible, but has no adapter yet.' -ForegroundColor Yellow;Write-Host 'Run NCMM.cmd adapter to deep-probe and create an adapter for this build.' -ForegroundColor Yellow;throw 'ADAPTER_REQUIRED'}
        $adapter=Get-NcmmBaseAdapter $PackageRoot;$usingStructural=$true
    }
    if(-not $adapter){throw 'No usable NCMM build adapter.'}
    $vcpkg=if($usingStructural){[string]$probe.vcpkg_baseline}else{[string]$adapter.vcpkg_commit}
    if($vcpkg -notmatch '^[0-9a-f]{40}$'){throw 'Target vcpkg baseline could not be resolved.'}
    $tag=if($usingStructural){[string]$probe.source_tag}else{[string]$adapter.tag}
    $folder=if($usingStructural){[string]$probe.source_folder}else{[string]$adapter.folder}
    $cache=if($usingStructural){[string]$probe.suggested_cache_key}else{[string]$adapter.cache_key}
    $payload=[string]$adapter.payload
    if(-not(Test-Path $payload -PathType Leaf)){throw "Adapter payload missing: $payload"}
    Write-Host '';Write-Host '=== NCMM Infrastructure 0.8.3.1 Staged transactional install ===' -ForegroundColor Cyan
    Write-Host ('Adapter: '+[string]$adapter.id+$(if($adapter.inherits){' <- '+[string]$adapter.inherits}else{''})+$(if($usingStructural){' (STRUCTURAL REUSE)'}else{' (EXACT/GENERATED)'}))
    Write-Host ('Target:  '+$tag+' / '+$probe.source_commit)
    if($usingStructural){$deep=Invoke-NcmmDeepSourceProbe $PackageRoot $probe $BuildRoot $BuildProfile;if($deep.status -ne 'PASS'){throw ('Structural reuse blocked: deep source transform probe failed. Report: '+$deep.report_path)};Write-Host ('Deep source transform probe: PASS ('+$deep.report_path+')') -ForegroundColor Green}
    [pscustomobject]@{probe=$probe;adapter=$adapter;using_structural=$usingStructural;vcpkg=$vcpkg;tag=$tag;folder=$folder;cache=$cache;payload=$payload}
}
