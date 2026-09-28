$ErrorActionPreference = 'Stop'

function Get-NcmmManagedModuleInstallations([string]$PackageRoot,[string]$GameRoot) {
    $known=@{}
    $hasCatalog=$false
    if(-not [string]::IsNullOrWhiteSpace($PackageRoot)){
        $catalogPath=Join-Path $PackageRoot 'components\index.json'
        if(Test-Path $catalogPath -PathType Leaf){
            $catalog=Get-Content $catalogPath -Raw|ConvertFrom-Json
            foreach($component in @($catalog.components|Where-Object{[string]$_.kind -eq 'native_module'})){
                $id=([string]$component.id).Trim()
                if($id){$known[$id]=[pscustomobject]@{id=$id;version=[string]$component.version}}
            }
            $hasCatalog=$true
        }
    }

    $result=@{}
    $statePath=Join-Path $GameRoot 'ncmm\installed-components.json'
    if(Test-Path $statePath -PathType Leaf){
        try{
            $state=Get-Content $statePath -Raw|ConvertFrom-Json
            foreach($component in @($state.components)){
                $id=([string]$component.id).Trim()
                $directory=([string]$component.directory).Trim()
                if(-not $id -or -not $directory){continue}
                if($hasCatalog -and -not $known.ContainsKey($id)){continue}
                if([IO.Path]::IsPathRooted($directory) -or $directory.Contains('..') -or
                   $directory.Contains('\') -or $directory.Contains('/')){
                    throw "Unsafe managed module directory in installed-components.json: $directory"
                }
                $expected=if($known.ContainsKey($id)){[string]$known[$id].version}else{[string]$component.version}
                $result[$id]=[pscustomobject]@{id=$id;directory=$directory;expected_version=$expected;source='installed-components'}
            }
        }catch{
            throw "Could not read NCMM installed-components state: $($_.Exception.Message)"
        }
    }

    # Fallback for pre-state installations: only catalog-recognized native modules are eligible.
    if($hasCatalog){
        $modsRoot=Join-Path $GameRoot 'code_mods'
        if(Test-Path $modsRoot -PathType Container){
            foreach($dir in @(Get-ChildItem $modsRoot -Directory -ErrorAction SilentlyContinue)){
                $manifestPath=Join-Path $dir.FullName 'mod.json'
                if(-not(Test-Path $manifestPath -PathType Leaf)){continue}
                try{$moduleManifest=Get-Content $manifestPath -Raw|ConvertFrom-Json}catch{continue}
                $id=([string]$moduleManifest.id).Trim()
                if(-not $id -or -not $known.ContainsKey($id)){continue}
                if($result.ContainsKey($id)){
                    if([string]$result[$id].directory -ne [string]$dir.Name){
                        throw "Duplicate managed module id '$id' in multiple directories."
                    }
                    continue
                }
                $result[$id]=[pscustomobject]@{
                    id=$id;directory=[string]$dir.Name;expected_version=[string]$known[$id].version;source='manifest-scan'
                }
            }
        }
    }

    return @($result.GetEnumerator()|Sort-Object Name|ForEach-Object{$_.Value})
}

function New-NcmmTransactionSnapshot([string]$GameRoot,[string]$BuildRoot,[string]$PackageRoot='') {
    $id=(Get-Date -Format 'yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    $stage=Join-Path $env:TEMP ('NCMM_INFRA_083_'+$id)
    New-Item -ItemType Directory -Force $stage | Out-Null
    $entries=@(
        @{src='cataclysm-tiles.exe';dst='cataclysm-tiles.exe';kind='file'},
        @{src='cataclysm-tiles.vanilla.exe';dst='cataclysm-tiles.vanilla.exe';kind='file'},
        @{src='cataclysm-tiles.ncmm.exe';dst='cataclysm-tiles.ncmm.exe';kind='file'},
        @{src='ncmm';dst='ncmm';kind='dir'}
    )
    $managed=@(Get-NcmmManagedModuleInstallations $PackageRoot $GameRoot)
    foreach($module in $managed){
        $rel='code_mods\'+[string]$module.directory
        $entries += @{src=$rel;dst=$rel;kind='dir'}
    }

    $presence=@{}
    foreach($e in $entries){
        $s=Join-Path $GameRoot $e.src; $d=Join-Path $stage $e.dst
        $presence[$e.src]=(Test-Path $s)
        if(Test-Path $s){
            New-Item -ItemType Directory -Force (Split-Path -Parent $d)|Out-Null
            if($e.kind -eq 'dir'){Copy-Item $s $d -Recurse -Force}else{Copy-Item $s $d -Force}
        }
    }
    $meta=[ordered]@{
        schema=1;infrastructure='0.8.3.1';id=$id;game_root=$GameRoot;
        created_utc=[DateTime]::UtcNow.ToString('o');presence=$presence;
        managed_modules=@($managed|ForEach-Object{[ordered]@{id=$_.id;directory=$_.directory;expected_version=$_.expected_version}})
    }
    Write-NcmmUtf8NoBom (Join-Path $stage 'transaction_snapshot.json') (($meta|ConvertTo-Json -Depth 8)+"`n")
    $backupRoot=Join-Path $env:USERPROFILE 'Downloads\NCMM_Infrastructure_Backups'
    New-Item -ItemType Directory -Force $backupRoot|Out-Null
    $zip=Join-Path $backupRoot ('ncmm_infra_before_'+$id+'.zip')
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    [pscustomobject]@{id=$id;stage=$stage;backup=$zip;metadata=$meta}
}
function Restore-NcmmTransactionSnapshot([object]$Snapshot) {
    $stage=[string]$Snapshot.stage; $meta=$Snapshot.metadata; $root=[string]$meta.game_root
    $rels=@()
    if($meta.presence -is [Collections.IDictionary]){
        $rels=@($meta.presence.Keys)
    }else{
        $rels=@($meta.presence.PSObject.Properties|ForEach-Object{[string]$_.Name})
    }
    foreach($rel in $rels){
        $baseSafe=$rel -in @('cataclysm-tiles.exe','cataclysm-tiles.vanilla.exe','cataclysm-tiles.ncmm.exe','ncmm')
        $moduleSafe=$rel -match '^code_mods\\[^\\/:*?"<>|]+$'
        if(-not $baseSafe -and -not $moduleSafe){throw "Unsafe transaction snapshot path: $rel"}

        $target=Join-Path $root $rel
        Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue
        $had=$false
        try{
            if($meta.presence -is [Collections.IDictionary]){$had=[bool]$meta.presence[$rel]}
            else{$had=[bool]($meta.presence.PSObject.Properties[$rel].Value)}
        }catch{}
        $saved=Join-Path $stage $rel
        if($had -and (Test-Path $saved)){
            New-Item -ItemType Directory -Force (Split-Path -Parent $target)|Out-Null
            Copy-Item $saved $target -Recurse -Force
        }
    }
}

function New-NcmmVerificationRecord([string]$Id,[bool]$Ok,[string]$Detail) {
    [pscustomobject]@{id=$Id;status=$(if($Ok){'PASS'}else{'FAIL'});detail=$Detail}
}

function New-NcmmVerificationBundle([string]$Root,[string]$Prefix,[string[]]$Names) {
    $bundleRoot=Join-Path $env:USERPROFILE 'Downloads\NCMM_Verification'
    New-Item -ItemType Directory -Force $bundleRoot|Out-Null
    $tmp=Join-Path $env:TEMP ('NCMM_VERIFY_'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force $tmp|Out-Null
    try {
        foreach($n in $Names){
            $s=Join-Path $Root ('ncmm\'+$n)
            if(Test-Path $s -PathType Leaf){Copy-Item $s (Join-Path $tmp $n) -Force}
        }
        $bundle=Join-Path $bundleRoot ($Prefix+'_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.zip')
        Compress-Archive -Path (Join-Path $tmp '*') -DestinationPath $bundle -Force
        return $bundle
    } finally {
        Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-NcmmOfflineVerification([string]$PackageRoot,[string]$GameRoot,[switch]$LaunchDiagnostics,[switch]$CreateBundle) {
    $root=Resolve-NcmmGameRoot $GameRoot
    $manifest=Get-NcmmManifest $PackageRoot
    $tests=@()

    foreach($f in @(
        'cataclysm-tiles.exe','cataclysm-tiles.vanilla.exe','cataclysm-tiles.ncmm.exe','ncmm\host.binding.json'
    )){
        $ok=Test-Path (Join-Path $root $f) -PathType Leaf
        $tests += New-NcmmVerificationRecord ('file.'+$f) $ok $f
    }

    $managedModules=@(Get-NcmmManagedModuleInstallations $PackageRoot $root)
    foreach($module in $managedModules){
        $dirRel='code_mods\'+[string]$module.directory
        foreach($leaf in @('ncmm_mod.dll','mod.json')){
            $rel=$dirRel+'\'+$leaf
            $tests += New-NcmmVerificationRecord ('file.'+$rel) (Test-Path (Join-Path $root $rel) -PathType Leaf) $rel
        }
    }

    $binding=$null
    try{$binding=Get-Content (Join-Path $root 'ncmm\host.binding.json') -Raw|ConvertFrom-Json;$tests += New-NcmmVerificationRecord 'binding.json' $true 'parsed'}
    catch{$tests += New-NcmmVerificationRecord 'binding.json' $false $_.Exception.Message}

    $commit=Read-NcmmSourceCommit $root
    if($binding){
        $tests += New-NcmmVerificationRecord 'binding.source_commit' (([string]$binding.source_commit).ToLowerInvariant() -eq $commit) ('game='+$commit+' binding='+[string]$binding.source_commit)
        $tests += New-NcmmVerificationRecord 'binding.host_version' ([string]$binding.ncmm_version -eq [string]$manifest.host_version) ([string]$binding.ncmm_version)
        $tests += New-NcmmVerificationRecord 'binding.loader_api' ([int]$binding.loader_api -eq [int]$manifest.loader_api) ([string]$binding.loader_api)
        $tests += New-NcmmVerificationRecord 'binding.patch_revision' (([string]$binding.patch_revision) -match '^[0-9a-fA-F]{64}$') ([string]$binding.patch_revision)
    }

    if($LaunchDiagnostics){
        $running=@(Get-Process -Name 'cataclysm-tiles' -ErrorAction SilentlyContinue)
        if($running.Count -gt 0){
            $tests += New-NcmmVerificationRecord 'diagnostics.launch' $true 'SKIP: game process already running'
        } else {
            $p=Start-Process (Join-Path $root 'cataclysm-tiles.exe') -ArgumentList @('--ncmm-offline','--ncmm-diagnose') -WorkingDirectory $root -PassThru -Wait
            $tests += New-NcmmVerificationRecord 'diagnostics.launch' ($p.ExitCode -eq 0) ('exit='+$p.ExitCode)
        }
    }

    foreach($pair in @(@('bootstrap','cataclysm-tiles.exe','ncmm\bootstrap.sha256'),@('vanilla','cataclysm-tiles.vanilla.exe','ncmm\vanilla.sha256'))){
        $bin=Join-Path $root $pair[1];$shaFile=Join-Path $root $pair[2]
        if((Test-Path $bin -PathType Leaf)-and(Test-Path $shaFile -PathType Leaf)){
            $expected=(Get-Content $shaFile -Raw).Trim().ToLowerInvariant();$actual=Get-NcmmHash $bin
            $tests += New-NcmmVerificationRecord ('hash.'+$pair[0]) ($actual -eq $expected) ('expected='+$expected+' actual='+$actual)
        } else {$tests += New-NcmmVerificationRecord ('hash.'+$pair[0]) $false 'missing binary or sha file'}
    }

    if($binding -and (Test-Path (Join-Path $root 'cataclysm-tiles.ncmm.exe') -PathType Leaf)){
        $h=Get-NcmmHash (Join-Path $root 'cataclysm-tiles.ncmm.exe')
        $tests += New-NcmmVerificationRecord 'hash.host' ($h -eq ([string]$binding.host_sha256).ToLowerInvariant()) ('binding='+[string]$binding.host_sha256+' actual='+$h)
        if(Test-Path (Join-Path $root 'cataclysm-tiles.vanilla.exe') -PathType Leaf){
            $vh=Get-NcmmHash (Join-Path $root 'cataclysm-tiles.vanilla.exe')
            $tests += New-NcmmVerificationRecord 'binding.vanilla_hash' ($vh -eq ([string]$binding.vanilla_sha256).ToLowerInvariant()) ('binding='+[string]$binding.vanilla_sha256+' actual='+$vh)
        }
    }

    $runtime=$null
    try{$runtime=Get-Content (Join-Path $root 'ncmm\runtime.state.json') -Raw|ConvertFrom-Json;$tests += New-NcmmVerificationRecord 'runtime.json' $true 'parsed'}
    catch{$tests += New-NcmmVerificationRecord 'runtime.json' $false $_.Exception.Message}
    if($runtime){
        $tests += New-NcmmVerificationRecord 'runtime.schema' ([int]$runtime.schema -eq 1) ([string]$runtime.schema)
        $tests += New-NcmmVerificationRecord 'runtime.host_version' ([string]$runtime.runtime_version -eq [string]$manifest.host_version) ([string]$runtime.runtime_version)
        $tests += New-NcmmVerificationRecord 'runtime.loader_api' ([int]$runtime.loader_api -eq [int]$manifest.loader_api) ([string]$runtime.loader_api)
        $tests += New-NcmmVerificationRecord 'runtime.host_valid' ([bool]$runtime.host_valid) ([string]$runtime.host_status)
        $tests += New-NcmmVerificationRecord 'runtime.source_commit' (([string]$runtime.source_commit).ToLowerInvariant() -eq $commit) ([string]$runtime.source_commit)
        if($binding){
            $tests += New-NcmmVerificationRecord 'runtime.host_hash_binding' (([string]$runtime.host_sha256).ToLowerInvariant() -eq ([string]$binding.host_sha256).ToLowerInvariant()) ('runtime='+[string]$runtime.host_sha256+' binding='+[string]$binding.host_sha256)
            $tests += New-NcmmVerificationRecord 'runtime.binding_hash_echo' (([string]$runtime.binding_host_sha256).ToLowerInvariant() -eq ([string]$binding.host_sha256).ToLowerInvariant()) ([string]$runtime.binding_host_sha256)
        }
        $tests += New-NcmmVerificationRecord 'runtime.auto_disabled' (-not [bool]$runtime.auto_disabled) ([string]$runtime.auto_disabled)
        $modeOk=([string]$runtime.selected_mode -eq 'NCMM_HOST') -or [bool]$runtime.manual_disabled
        $tests += New-NcmmVerificationRecord 'runtime.selected_mode' $modeOk ([string]$runtime.selected_mode)
        if($LaunchDiagnostics){$tests += New-NcmmVerificationRecord 'runtime.diagnostics_only' ([bool]$runtime.diagnostics_only) ([string]$runtime.diagnostics_only)}
    }

    foreach($mod in $managedModules){
        $mp=Join-Path $root ('code_mods\'+[string]$mod.directory+'\mod.json')
        try{
            $mj=Get-Content $mp -Raw|ConvertFrom-Json
            $ok=([string]$mj.id -eq [string]$mod.id) -and
                ([string]$mj.version -eq [string]$mod.expected_version) -and
                ([int]$mj.loader_api -eq [int]$manifest.loader_api)
            $tests += New-NcmmVerificationRecord ('module_manifest.'+[string]$mod.id) $ok (
                ([string]$mj.id)+' '+([string]$mj.version)+' loader_api='+([string]$mj.loader_api)
            )
        } catch {
            $tests += New-NcmmVerificationRecord ('module_manifest.'+[string]$mod.id) $false $_.Exception.Message
        }
    }

    foreach($marker in @('ncmm\recipe_profiler_support.v1','ncmm\runtime_infrastructure.v8766','ncmm\host_api_v2.core')){
        $tests += New-NcmmVerificationRecord ('marker.'+$marker) (Test-Path (Join-Path $root $marker) -PathType Leaf) $marker
    }

    $failed=@($tests|Where-Object{$_.status -eq 'FAIL'})
    $report=[ordered]@{schema=2;verification='offline_install';infrastructure='0.8.3.1';status=$(if($failed.Count -eq 0){'PASS'}else{'FAIL'});game_root=$root;source_commit=$commit;checked_utc=[DateTime]::UtcNow.ToString('o');tests=@($tests);failed=@($failed|ForEach-Object{$_.id})}
    $reportPath=Join-Path $root 'ncmm\offline_verify.latest.json'
    Write-NcmmUtf8NoBom $reportPath (($report|ConvertTo-Json -Depth 10)+"`n")
    $bundle=$null
    if($CreateBundle){$bundle=New-NcmmVerificationBundle $root 'ncmm_offline_verify' @('offline_verify.latest.json','runtime.state.json','host.binding.json','compatibility.latest.json','migration.latest.json','diagnostics.txt','ncmm.log','bootstrap.log','transaction.latest.json','transaction.journal.json')}
    [pscustomobject]@{status=[string]$report.status;report_path=$reportPath;bundle=$bundle;report=$report}
}

function Invoke-NcmmRuntimeVerification([string]$PackageRoot,[string]$GameRoot,[switch]$CreateBundle) {
    $root=Resolve-NcmmGameRoot $GameRoot
    $manifest=Get-NcmmManifest $PackageRoot
    $tests=@()

    $offline=Invoke-NcmmOfflineVerification $PackageRoot $root
    $tests += New-NcmmVerificationRecord 'offline_install_prerequisite' ($offline.status -eq 'PASS') ($offline.report_path)

    $binding=$null
    try{$binding=Get-Content (Join-Path $root 'ncmm\host.binding.json') -Raw|ConvertFrom-Json}catch{}
    $modsPath=Join-Path $root 'ncmm\modules.state.json'
    $mods=$null
    try{$mods=Get-Content $modsPath -Raw|ConvertFrom-Json;$tests += New-NcmmVerificationRecord 'modules.json' $true 'parsed'}
    catch{$tests += New-NcmmVerificationRecord 'modules.json' $false ('Launch Cataclysm normally once, then retry. '+$_.Exception.Message)}

    if($binding -and (Test-Path $modsPath -PathType Leaf) -and $binding.installed_utc){
        try{
            $installedUtc=[DateTime]::Parse([string]$binding.installed_utc).ToUniversalTime()
            $modsUtc=(Get-Item $modsPath).LastWriteTimeUtc
            $tests += New-NcmmVerificationRecord 'modules.fresh_after_install' ($modsUtc -ge $installedUtc) ('installed='+$installedUtc.ToString('o')+' modules='+$modsUtc.ToString('o'))
        } catch {$tests += New-NcmmVerificationRecord 'modules.fresh_after_install' $false $_.Exception.Message}
    }

    if($mods){
        $tests += New-NcmmVerificationRecord 'modules.host_version' ([string]$mods.host_version -eq [string]$manifest.host_version) ([string]$mods.host_version)
        $tests += New-NcmmVerificationRecord 'modules.loader_api' ([int]$mods.loader_api -eq [int]$manifest.loader_api) ([string]$mods.loader_api)
        $apiParts=([string]$manifest.ncmm_api).Split('.')
        if($apiParts.Count -eq 2 -and $mods.api_version){
            $ok=([int]$mods.api_version.major -eq [int]$apiParts[0]) -and ([int]$mods.api_version.minor -eq [int]$apiParts[1])
            $tests += New-NcmmVerificationRecord 'modules.api_version' $ok (([string]$mods.api_version.major)+'.'+([string]$mods.api_version.minor))
        } else {$tests += New-NcmmVerificationRecord 'modules.api_version' $false 'missing api_version'}
        $caps=@($mods.capabilities)
        foreach($cap in $manifest.self_test.required_capabilities){$tests += New-NcmmVerificationRecord ('capability.'+[string]$cap) ($caps -contains [string]$cap) ([string]$cap)}
        foreach($req in $manifest.self_test.required_modules){
            $m=@($mods.modules|Where-Object{[string]$_.id -eq [string]$req.id})|Select-Object -First 1
            $ok=$m -and [string]$m.state -eq 'loaded' -and [string]$m.lifecycle -eq 'active' -and [string]$m.version -eq [string]$req.version
            $detail=if($m){
                $d=([string]$m.version+' '+[string]$m.state+'/'+[string]$m.lifecycle)
                if(-not [string]::IsNullOrWhiteSpace([string]$m.reason)){$d += ' reason='+[string]$m.reason}
                $d
            }else{'missing'}
            $tests += New-NcmmVerificationRecord ('module.'+[string]$req.id) ([bool]$ok) $detail
        }
        $managedRuntimeModules=@(Get-NcmmManagedModuleInstallations $PackageRoot $root)
        foreach($opt in $manifest.self_test.optional_modules){
            $installed=@($managedRuntimeModules|Where-Object{[string]$_.id -eq [string]$opt.id})|Select-Object -First 1
            if($installed){
                $m=@($mods.modules|Where-Object{[string]$_.id -eq [string]$opt.id})|Select-Object -First 1
                $ok=$m -and [string]$m.state -eq 'loaded' -and [string]$m.lifecycle -eq 'active' -and
                    [string]$m.version -eq [string]$opt.version
                $detail=if($m){([string]$m.version+' '+[string]$m.state+'/'+[string]$m.lifecycle)}else{'installed but missing from state'}
                $tests += New-NcmmVerificationRecord ('module.optional.'+[string]$opt.id) ([bool]$ok) $detail
            }
        }
    }

    $tests += New-NcmmVerificationRecord 'host.boot_ready' (Test-Path (Join-Path $root 'ncmm\boot.ready') -PathType Leaf) 'ncmm\boot.ready'
    $failed=@($tests|Where-Object{$_.status -eq 'FAIL'})
    $moduleFailures=@($failed|Where-Object{$_.id -like 'module.*'}|ForEach-Object{[pscustomobject]@{id=$_.id;detail=$_.detail}})
    $needsLaunch=@($failed|Where-Object{$_.id -in @('modules.json','modules.fresh_after_install','host.boot_ready')}).Count -gt 0
    $instruction=if($failed.Count -eq 0){'Runtime verification complete.'}elseif($needsLaunch){'Launch Cataclysm normally once with NCMM Host, reach the main menu or load a world, exit normally, then retry runtime verify.'}else{'Host launched, but a runtime module failed its contract. Inspect module_failures and ncmm.log; reinstall only after the module/Host fix is applied.'}
    $report=[ordered]@{schema=3;verification='runtime_modules';infrastructure='0.8.3.1';status=$(if($failed.Count -eq 0){'PASS'}else{'FAIL'});game_root=$root;checked_utc=[DateTime]::UtcNow.ToString('o');tests=@($tests);failed=@($failed|ForEach-Object{$_.id});module_failures=@($moduleFailures);instruction=$instruction}
    $reportPath=Join-Path $root 'ncmm\runtime_verify.latest.json'
    Write-NcmmUtf8NoBom $reportPath (($report|ConvertTo-Json -Depth 10)+"`n")
    if($report.status -eq 'PASS'){Remove-Item (Join-Path $root 'ncmm\runtime_verification.pending.json') -Force -ErrorAction SilentlyContinue}
    $bundle=$null
    if($CreateBundle){$bundle=New-NcmmVerificationBundle $root 'ncmm_runtime_verify' @('runtime_verify.latest.json','offline_verify.latest.json','runtime_verification.pending.json','runtime.state.json','modules.state.json','host.binding.json','diagnostics.txt','ncmm.log','bootstrap.log','transaction.latest.json','transaction.journal.json')}
    [pscustomobject]@{status=[string]$report.status;report_path=$reportPath;bundle=$bundle;report=$report}
}

function Invoke-NcmmSelfTest([string]$PackageRoot,[string]$GameRoot,[ValidateSet('Offline','Runtime','Full')][string]$Mode='Runtime',[switch]$LaunchDiagnostics,[switch]$CreateBundle) {
    if($Mode -eq 'Offline'){$result=Invoke-NcmmOfflineVerification $PackageRoot $GameRoot -LaunchDiagnostics:$LaunchDiagnostics -CreateBundle:$CreateBundle;return $result}
    if($Mode -eq 'Runtime'){$result=Invoke-NcmmRuntimeVerification $PackageRoot $GameRoot -CreateBundle:$CreateBundle;return $result}
    $offline=Invoke-NcmmOfflineVerification $PackageRoot $GameRoot -LaunchDiagnostics:$LaunchDiagnostics
    if($offline.status -ne 'PASS'){return $offline}
    $result=Invoke-NcmmRuntimeVerification $PackageRoot $GameRoot -CreateBundle:$CreateBundle;return $result
}


function Get-NcmmFeedEntry([string]$PackageRoot,[string]$Commit) {
    $m=Get-NcmmManifest $PackageRoot
    $indexPath=Join-Path $PackageRoot ([string]$m.compatibility_feed)
    if(-not(Test-Path $indexPath -PathType Leaf)){return $null}
    try{$index=Get-Content $indexPath -Raw|ConvertFrom-Json}catch{return $null}
    $row=@($index.entries|Where-Object{([string]$_.commit).ToLowerInvariant() -eq $Commit.ToLowerInvariant()})|Select-Object -First 1
    if(-not $row){return $null}
    $entryPath=Join-Path (Split-Path -Parent $indexPath) ([string]$row.manifest)
    if(-not(Test-Path $entryPath -PathType Leaf)){return $null}
    try{return (Get-Content $entryPath -Raw|ConvertFrom-Json)}catch{return $null}
}
function Set-NcmmTransactionPhase([string]$JournalPath,[string]$Phase,[string]$Status='running',[string]$Details='') {
    if([string]::IsNullOrWhiteSpace($JournalPath)){return}
    $state=[ordered]@{}
    if(Test-Path $JournalPath -PathType Leaf){
        try{$old=Get-Content $JournalPath -Raw|ConvertFrom-Json;foreach($p in $old.PSObject.Properties){$state[$p.Name]=$p.Value}}catch{}
    }
    if(-not $state['schema']){$state['schema']=4}
    if(-not $state['infrastructure']){$state['infrastructure']='0.8.3.1'}
    $events=@();if($state['events']){$events=@($state['events'])}
    $evt=[ordered]@{phase=$Phase;status=$Status;utc=[DateTime]::UtcNow.ToString('o')}
    if(-not [string]::IsNullOrWhiteSpace($Details)){$evt['details']=$Details}
    $events+= [pscustomobject]$evt
    $state['current_phase']=$Phase;$state['status']=$Status;$state['updated_utc']=[DateTime]::UtcNow.ToString('o');$state['events']=$events
    Write-NcmmUtf8NoBom $JournalPath (([pscustomobject]$state|ConvertTo-Json -Depth 12)+"`n")
}
function Restore-NcmmTransactionArchive([string]$BackupZip,[string]$GameRoot) {
    if(-not(Test-Path $BackupZip -PathType Leaf)){throw "Transaction backup archive missing: $BackupZip"}
    $tmp=Join-Path $env:TEMP ('NCMM_RECOVER_'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force $tmp|Out-Null
    try{
        Expand-Archive -LiteralPath $BackupZip -DestinationPath $tmp -Force
        $metaPath=Join-Path $tmp 'transaction_snapshot.json'
        if(-not(Test-Path $metaPath -PathType Leaf)){throw 'Transaction archive has no snapshot metadata.'}
        $meta=Get-Content $metaPath -Raw|ConvertFrom-Json
        if(([IO.Path]::GetFullPath([string]$meta.game_root)).TrimEnd('\') -ne ([IO.Path]::GetFullPath($GameRoot)).TrimEnd('\')){throw 'Transaction archive target does not match current game root.'}
        Restore-NcmmTransactionSnapshot ([pscustomobject]@{stage=$tmp;metadata=$meta})
    } finally {Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue}
}
function Recover-NcmmInterruptedTransaction([string]$PackageRoot,[string]$GameRoot,[string]$BuildRoot='C:\NCMMBuild') {
    $root=Resolve-NcmmGameRoot $GameRoot
    $pendingPath=Join-Path $root 'ncmm\transaction.pending.json'
    if(-not(Test-Path $pendingPath -PathType Leaf)){return [pscustomobject]@{status='none';game_root=$root}}
    try{$pending=Get-Content $pendingPath -Raw|ConvertFrom-Json}catch{throw 'Interrupted NCMM transaction marker is corrupt; refusing to overwrite it.'}
    $latestPath=Join-Path $root 'ncmm\transaction.latest.json'
    if(Test-Path $latestPath -PathType Leaf){
        try{$latest=Get-Content $latestPath -Raw|ConvertFrom-Json;if([string]$latest.transaction_id -eq [string]$pending.transaction_id -and [string]$latest.status -eq 'committed'){Remove-Item $pendingPath -Force;return [pscustomobject]@{status='stale_pending_removed';transaction_id=[string]$pending.transaction_id}}}catch{}
    }
    $backup=[string]$pending.backup
    if([string]::IsNullOrWhiteSpace($backup) -or -not(Test-Path $backup -PathType Leaf)){throw ('Interrupted transaction '+[string]$pending.transaction_id+' has no usable rollback archive: '+$backup)}
    Write-Host ('Interrupted NCMM transaction detected: '+[string]$pending.transaction_id) -ForegroundColor Yellow
    $dir=Join-Path $BuildRoot 'transactions';New-Item -ItemType Directory -Force $dir|Out-Null
    $oldJournal=Join-Path $root 'ncmm\transaction.journal.json'
    if(Test-Path $oldJournal -PathType Leaf){Copy-Item $oldJournal (Join-Path $dir ('interrupted_'+[string]$pending.transaction_id+'.journal.json')) -Force}
    Write-Host 'Restoring the pre-install snapshot before doing any new work...' -ForegroundColor Yellow
    Restore-NcmmTransactionArchive $backup $root
    $report=[ordered]@{schema=1;infrastructure='0.8.3.1';status='recovered';transaction_id=[string]$pending.transaction_id;backup=$backup;recovered_utc=[DateTime]::UtcNow.ToString('o')}
    Write-NcmmUtf8NoBom (Join-Path $dir ('recovery_'+[string]$pending.transaction_id+'.json')) (($report|ConvertTo-Json -Depth 8)+"`n")
    return [pscustomobject]$report
}
