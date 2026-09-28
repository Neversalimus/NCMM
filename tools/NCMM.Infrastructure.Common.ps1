$ErrorActionPreference = 'Stop'

function Write-NcmmUtf8NoBom([string]$Path,[string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))
}
function Get-NcmmHash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-NcmmGitBlobSha1([string]$Path) {
    $bytes=[IO.File]::ReadAllBytes($Path)
    $prefix=[Text.Encoding]::ASCII.GetBytes(('blob '+$bytes.Length+([char]0)))
    $combined=New-Object byte[] ($prefix.Length+$bytes.Length)
    [Array]::Copy($prefix,0,$combined,0,$prefix.Length)
    [Array]::Copy($bytes,0,$combined,$prefix.Length,$bytes.Length)
    $sha=[Security.Cryptography.SHA1]::Create()
    try {
        $hash=$sha.ComputeHash($combined)
        return (($hash|ForEach-Object{$_.ToString('x2')}) -join '')
    } finally { $sha.Dispose() }
}
function Read-NcmmSourceCommit([string]$Root) {
    $version = Join-Path $Root 'VERSION.txt'
    if (Test-Path $version -PathType Leaf) {
        foreach ($line in Get-Content $version -ErrorAction SilentlyContinue) {
            if ($line -match '^\s*commit sha:\s*(\S+)\s*$') { return ([string]$matches[1]).Trim().ToLowerInvariant() }
        }
    }
    $binding = Join-Path $Root 'ncmm\host.binding.json'
    if (Test-Path $binding -PathType Leaf) {
        try { return ([string](Get-Content $binding -Raw | ConvertFrom-Json).source_commit).Trim().ToLowerInvariant() } catch {}
    }
    return ''
}
function Get-NcmmGameTag([string]$Root) {
    $leaf = [IO.Path]::GetFileName(([IO.Path]::GetFullPath($Root)).TrimEnd('\'))
    if ($leaf -match '^cdda_experimental_(\d{4})_(\d{2})_(\d{2})_(\d+)$') {
        return ('cdda-experimental-{0}-{1}-{2}-{3}' -f $matches[1],$matches[2],$matches[3],$matches[4])
    }
    return $leaf.Replace('_','-')
}
function Resolve-NcmmGameRoot([string]$RequestedRoot='') {
    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        $p=[IO.Path]::GetFullPath($RequestedRoot)
        if (-not (Test-Path $p -PathType Container)) { throw "Game root missing: $p" }
        return $p
    }
    $candidates = New-Object System.Collections.Generic.List[object]
    if (-not [string]::IsNullOrWhiteSpace($env:CDDA_ROOT)) { $candidates.Add([pscustomobject]@{Path=$env:CDDA_ROOT;Score=400}) }
    $candidates.Add([pscustomobject]@{Path=(Get-Location).Path;Score=50})
    $assets = Join-Path $env:LOCALAPPDATA 'com.munetmo.cat-launcher\Assets\DarkDaysAhead'
    if (Test-Path $assets -PathType Container) {
        foreach ($d in @(Get-ChildItem $assets -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)) {
            $candidates.Add([pscustomobject]@{Path=$d.FullName;Score=(300-[Math]::Min(250,[int]$candidates.Count))})
        }
    }
    $ranked=@()
    foreach ($c in $candidates) {
        if ([string]::IsNullOrWhiteSpace([string]$c.Path)) { continue }
        try { $p=[IO.Path]::GetFullPath([string]$c.Path) } catch { continue }
        if (-not (Test-Path $p -PathType Container)) { continue }
        if (-not (Test-Path (Join-Path $p 'VERSION.txt') -PathType Leaf)) { continue }
        if (-not (Test-Path (Join-Path $p 'cataclysm-tiles.exe') -PathType Leaf) -and -not (Test-Path (Join-Path $p 'cataclysm-tiles.vanilla.exe') -PathType Leaf)) { continue }
        $commit=Read-NcmmSourceCommit $p
        if ($commit -notmatch '^[0-9a-f]{40}$') { continue }
        $bonus=0
        if (Test-Path (Join-Path $p 'ncmm\host.binding.json') -PathType Leaf) { $bonus+=25 }
        $ranked += [pscustomobject]@{Root=$p;Score=([int]$c.Score+$bonus);Commit=$commit;Modified=(Get-Item $p).LastWriteTimeUtc}
    }
    $best=$ranked | Sort-Object @{Expression='Score';Descending=$true},@{Expression='Modified';Descending=$true} | Select-Object -First 1
    if (-not $best) { throw 'Could not auto-detect a CatLauncher CDDA installation with VERSION.txt.' }
    return [string]$best.Root
}
function Assert-NcmmPackageIntegrity([string]$PackageRoot) {
    $manifestPath=Join-Path $PackageRoot 'compat\compatibility.manifest.json'
    if(-not(Test-Path $manifestPath -PathType Leaf)){throw 'Compatibility manifest missing before integrity check.'}
    $m=Get-Content $manifestPath -Raw|ConvertFrom-Json
    $rel=[string]$m.package_integrity
    if([string]::IsNullOrWhiteSpace($rel)){throw 'Package integrity manifest is not declared.'}
    $ip=Join-Path $PackageRoot $rel
    if(-not(Test-Path $ip -PathType Leaf)){throw "Package integrity manifest missing: $ip"}
    $integrity=Get-Content $ip -Raw|ConvertFrom-Json
    if([int]$integrity.schema -ne 1){throw 'Unsupported package integrity schema.'}
    $failed=@()
    foreach($entry in $integrity.files){
        $fp=Join-Path $PackageRoot ([string]$entry.path)
        if(-not(Test-Path $fp -PathType Leaf)){$failed += (([string]$entry.path)+':missing');continue}
        $actual=Get-NcmmHash $fp
        if($actual -ne ([string]$entry.sha256).ToLowerInvariant()){$failed += (([string]$entry.path)+':sha256_mismatch')}
    }
    if($failed.Count -gt 0){throw ('NCMM Infrastructure package integrity failed: '+($failed -join ', '))}
    return $true
}

function Get-NcmmManifest([string]$PackageRoot) {
    $p=Join-Path $PackageRoot 'compat\compatibility.manifest.json'
    if (-not (Test-Path $p -PathType Leaf)) { throw "Compatibility manifest missing: $p" }
    $m=Get-Content $p -Raw | ConvertFrom-Json
    if ([int]$m.schema -ne 4 -or [string]$m.infrastructure_version -ne '0.8.3.1') { throw 'Unsupported NCMM Infrastructure manifest.' }
    return $m
}
function Get-NcmmAdapters([string]$PackageRoot) {
    $files=@(Get-ChildItem (Join-Path $PackageRoot 'adapters') -Filter '*.ps1' -File -Recurse -ErrorAction SilentlyContinue)
    $out=@()
    foreach ($f in $files) {
        try {
            $d=& $f.FullName -Mode Describe
            if ($d -and ([string]$d.commit -match '^[0-9a-f]{40}$')) {
                $d | Add-Member -NotePropertyName script_path -NotePropertyValue $f.FullName -Force
                $out += $d
            }
        } catch { Write-Host "Ignoring invalid adapter $($f.FullName): $($_.Exception.Message)" -ForegroundColor Yellow }
    }
    return @($out)
}
function Get-NcmmAdapterForCommit([string]$PackageRoot,[string]$Commit) {
    @(Get-NcmmAdapters $PackageRoot) | Where-Object { ([string]$_.commit).ToLowerInvariant() -eq $Commit.ToLowerInvariant() } | Select-Object -First 1
}
function Get-NcmmBaseAdapter([string]$PackageRoot) {
    $m=Get-NcmmManifest $PackageRoot
    $wanted=if($m.PSObject.Properties['adapter_base']){[string]$m.adapter_base}else{[string]$m.base_adapter}
    foreach($file in @(Get-ChildItem (Join-Path $PackageRoot 'adapters\base') -Filter '*.ps1' -File -ErrorAction SilentlyContinue)){
        try{$d=& $file.FullName -Mode Describe;if($d -and [string]$d.id -eq $wanted){$d|Add-Member -NotePropertyName script_path -NotePropertyValue $file.FullName -Force;return $d}}catch{Write-Host "Ignoring invalid base adapter $($file.FullName): $($_.Exception.Message)" -ForegroundColor Yellow}
    }
    return $null
}
function Expand-NcmmSingleRootZip([string]$Zip,[string]$Destination) {
    $tmp=$Destination+'_extract'
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    Expand-Archive -LiteralPath $Zip -DestinationPath $tmp -Force
    $dirs=@(Get-ChildItem $tmp -Directory)
    if ($dirs.Count -ne 1) { throw "Unexpected GitHub archive layout: $Zip" }
    Move-Item $dirs[0].FullName $Destination
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
function Ensure-NcmmProbeSource([string]$BuildRoot,[string]$Commit) {
    $short=$Commit.Substring(0,12)
    $root=Join-Path $BuildRoot ("probe_sources\cdda_"+$short)
    $marker=Join-Path $root '.ncmm_probe_commit'
    if ((Test-Path $marker -PathType Leaf) -and ((Get-Content $marker -Raw).Trim() -eq $Commit) -and (Test-Path (Join-Path $root 'src\options.cpp') -PathType Leaf)) { return $root }
    $downloads=Join-Path $BuildRoot 'downloads'; New-Item -ItemType Directory -Force $downloads | Out-Null
    $zip=Join-Path $downloads ("cdda_probe_"+$Commit+'.zip')
    if (-not (Test-Path $zip -PathType Leaf) -or (Get-Item $zip).Length -lt 10000) {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $url='https://github.com/CleverRaven/Cataclysm-DDA/archive/'+$Commit+'.zip'
        Write-Host "Downloading exact CDDA source for compatibility probe..." -ForegroundColor Cyan
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip
    }
    Expand-NcmmSingleRootZip $zip $root
    Write-NcmmUtf8NoBom $marker ($Commit+"`n")
    return $root
}
function Test-NcmmSourceContracts([string]$SourceRoot,[string]$RegistryPath) {
    $registry=Get-Content $RegistryPath -Raw | ConvertFrom-Json
    $results=@(); $failed=@()
    foreach ($contract in $registry.contracts) {
        $missing=@()
        foreach ($file in $contract.files) {
            $p=Join-Path $SourceRoot ([string]$file.path)
            if (-not (Test-Path $p -PathType Leaf)) { $missing += ('missing_file:'+[string]$file.path); continue }
            $text=[IO.File]::ReadAllText($p)
            foreach ($needle in $file.required) { if (-not $text.Contains([string]$needle)) { $missing += (([string]$file.path)+':'+[string]$needle) } }
        }
        $status=if($missing.Count -eq 0){'compatible'}else{'incompatible'}
        if($status -ne 'compatible'){$failed += [string]$contract.id}
        $results += [ordered]@{id=[string]$contract.id;status=$status;missing=@($missing)}
    }
    [pscustomobject]@{status=$(if($failed.Count -eq 0){'compatible'}else{'incompatible'});contracts=@($results);failed_contracts=@($failed)}
}
function Invoke-NcmmProbe([string]$PackageRoot,[string]$GameRoot,[string]$BuildRoot='C:\NCMMBuild') {
    $root=Resolve-NcmmGameRoot $GameRoot
    $commit=Read-NcmmSourceCommit $root
    if ($commit -notmatch '^[0-9a-f]{40}$') { throw "Could not resolve source commit from $root" }
    $manifest=Get-NcmmManifest $PackageRoot
    $exact=Get-NcmmAdapterForCommit $PackageRoot $commit
    $base=Get-NcmmBaseAdapter $PackageRoot
    if (-not $base) { throw 'Base build adapter is missing.' }
    $source=$null
    if($exact){
        $candidate=Join-Path $BuildRoot (([string]$exact.cache_key)+'_pristine')
        $candidateMarker=Join-Path $candidate '.ncmm_pristine_source_sha'
        if((Test-Path $candidateMarker -PathType Leaf)-and((Get-Content $candidateMarker -Raw).Trim().ToLowerInvariant() -eq $commit)-and(Test-Path (Join-Path $candidate 'src\options.cpp') -PathType Leaf)){
            $source=$candidate
            Write-Host 'Compatibility probe: reusing immutable exact source cache.' -ForegroundColor Green
        }
    }
    if(-not $source){$source=Ensure-NcmmProbeSource $BuildRoot $commit}
    $contractPath=Join-Path $PackageRoot ([string]$manifest.contract_registry)
    $contracts=Test-NcmmSourceContracts $source $contractPath
    $vcpkg=''
    $vcpkgPath=Join-Path $source 'msvc-full-features\vcpkg.json'
    if(Test-Path $vcpkgPath -PathType Leaf){try{$vcpkg=[string](Get-Content $vcpkgPath -Raw|ConvertFrom-Json).'builtin-baseline'}catch{}}
    $drift=@()
    if($base.reference_blobs){
        foreach($relKey in $base.reference_blobs.Keys){
            $rel=[string]$relKey; $expected=[string]$base.reference_blobs[$relKey]; $fp=Join-Path $source $rel
            if(-not(Test-Path $fp -PathType Leaf)){
                $drift += [pscustomobject]@{path=$rel;status='missing';reference_blob=$expected;current_blob=$null}
            } else {
                $actual=Get-NcmmGitBlobSha1 $fp
                $drift += [pscustomobject]@{path=$rel;status=$(if($actual -eq $expected){'unchanged'}else{'changed'});reference_blob=$expected;current_blob=$actual}
            }
        }
    }
    $status=if($contracts.status -ne 'compatible'){'INCOMPATIBLE'}elseif($exact -and [string]$exact.support -eq 'exact'){'EXACT_SUPPORTED'}elseif($exact){'STRUCTURAL_ADAPTER'}else{'STRUCTURALLY_COMPATIBLE'}
    [pscustomobject]@{
        schema=3; infrastructure='0.8.3.1'; status=$status; game_root=$root; source_commit=$commit;
        source_tag=(Get-NcmmGameTag $root); source_folder=[IO.Path]::GetFileName($root);
        exact_adapter=$(if($exact){[string]$exact.id}else{$null}); base_adapter=[string]$base.id;
        vcpkg_baseline=$vcpkg; suggested_cache_key=('cdda_'+$commit.Substring(0,8));
        source_cache=$source; contracts=$contracts; file_drift=@($drift);
        drift_summary=[pscustomobject]@{unchanged=@($drift|Where-Object{$_.status -eq 'unchanged'}).Count;changed=@($drift|Where-Object{$_.status -eq 'changed'}).Count;missing=@($drift|Where-Object{$_.status -eq 'missing'}).Count};
        feed_entry=(Get-NcmmFeedEntry $PackageRoot $commit);
        checked_utc=[DateTime]::UtcNow.ToString('o')
    }
}
function Invoke-NcmmDeepSourceProbe([string]$PackageRoot,[object]$Probe,[string]$BuildRoot='C:\NCMMBuild',[string]$BuildProfile='Balanced') {
    if(-not $Probe){throw 'Deep source probe requires a light probe result.'}
    if([string]$Probe.status -eq 'INCOMPATIBLE'){throw 'Deep source probe blocked because light source contracts failed.'}
    $adapter=Get-NcmmAdapterForCommit $PackageRoot ([string]$Probe.source_commit)
    $reuse=$false
    if(-not $adapter){$adapter=Get-NcmmBaseAdapter $PackageRoot;$reuse=$true}
    if(-not $adapter){throw 'Deep source probe has no base adapter.'}
    $payload=[string]$adapter.payload
    if(-not(Test-Path $payload -PathType Leaf)){throw "Deep source probe payload missing: $payload"}
    $vcpkg=if($reuse){[string]$Probe.vcpkg_baseline}else{[string]$adapter.vcpkg_commit}
    $tag=if($reuse){[string]$Probe.source_tag}else{[string]$adapter.tag}
    $folder=if($reuse){[string]$Probe.source_folder}else{[string]$adapter.folder}
    $cache=if($reuse){[string]$Probe.suggested_cache_key}else{[string]$adapter.cache_key}
    $support=if($reuse){'structural-reuse'}elseif([string]$adapter.support -eq 'exact'){'exact'}else{'generated-structural'}
    if($vcpkg -notmatch '^[0-9a-f]{40}$'){throw 'Deep source probe could not resolve vcpkg baseline.'}
    $reportDir=Join-Path $BuildRoot 'probe_reports';New-Item -ItemType Directory -Force $reportDir|Out-Null
    $reportPath=Join-Path $reportDir ('NCMM_DEEP_PROBE_'+([string]$Probe.source_commit).Substring(0,12)+'.json')
    Remove-Item $reportPath -Force -ErrorAction SilentlyContinue
    $psExe=Join-Path $PSHOME 'powershell.exe';if(-not(Test-Path $psExe -PathType Leaf)){$psExe='powershell.exe'}
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$payload,'-GameRoot',[string]$Probe.game_root,'-BuildRoot',$BuildRoot,'-BuildProfile',$BuildProfile,'-TargetCommit',[string]$Probe.source_commit,'-TargetTag',$tag,'-TargetFolder',$folder,'-TargetCacheKey',$cache,'-TargetVcpkgCommit',$vcpkg,'-TargetSupportMode',$support,'-HostSourceProbeOnly')
    Write-Host ''
    Write-Host 'Running full NCMM deep source transform probe...' -ForegroundColor Cyan
    & $psExe @args
    $ec=$LASTEXITCODE
    if($ec -ne 0){return [pscustomobject]@{status='FAIL';exit_code=$ec;report_path=$reportPath;adapter=[string]$adapter.id;structural_reuse=$reuse}}
    if(-not(Test-Path $reportPath -PathType Leaf)){return [pscustomobject]@{status='FAIL';exit_code=98;report_path=$reportPath;adapter=[string]$adapter.id;structural_reuse=$reuse}}
    try{$report=Get-Content $reportPath -Raw|ConvertFrom-Json}catch{return [pscustomobject]@{status='FAIL';exit_code=99;report_path=$reportPath;adapter=[string]$adapter.id;structural_reuse=$reuse}}
    $ok=[string]$report.status -eq 'DEEP_SOURCE_PASS' -and ([string]$report.source_commit).ToLowerInvariant() -eq ([string]$Probe.source_commit).ToLowerInvariant()
    return [pscustomobject]@{status=$(if($ok){'PASS'}else{'FAIL'});exit_code=$(if($ok){0}else{97});report_path=$reportPath;report=$report;adapter=[string]$adapter.id;structural_reuse=$reuse}
}

function New-NcmmTransactionSnapshot([string]$GameRoot,[string]$BuildRoot) {
    $id=(Get-Date -Format 'yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    $stage=Join-Path $env:TEMP ('NCMM_INFRA_083_'+$id)
    New-Item -ItemType Directory -Force $stage | Out-Null
    $entries=@(
        @{src='cataclysm-tiles.exe';dst='cataclysm-tiles.exe';kind='file'},
        @{src='cataclysm-tiles.vanilla.exe';dst='cataclysm-tiles.vanilla.exe';kind='file'},
        @{src='cataclysm-tiles.ncmm.exe';dst='cataclysm-tiles.ncmm.exe';kind='file'},
        @{src='ncmm';dst='ncmm';kind='dir'},
        @{src='code_mods\SurvivorProgression';dst='code_mods\SurvivorProgression';kind='dir'},
        @{src='code_mods\AdvancedWorldSettings';dst='code_mods\AdvancedWorldSettings';kind='dir'}
    )
    $presence=@{}
    foreach($e in $entries){
        $s=Join-Path $GameRoot $e.src; $d=Join-Path $stage $e.dst
        $presence[$e.src]=(Test-Path $s)
        if(Test-Path $s){New-Item -ItemType Directory -Force (Split-Path -Parent $d)|Out-Null; if($e.kind -eq 'dir'){Copy-Item $s $d -Recurse -Force}else{Copy-Item $s $d -Force}}
    }
    $meta=[ordered]@{schema=1;infrastructure='0.8.3.1';id=$id;game_root=$GameRoot;created_utc=[DateTime]::UtcNow.ToString('o');presence=$presence}
    Write-NcmmUtf8NoBom (Join-Path $stage 'transaction_snapshot.json') (($meta|ConvertTo-Json -Depth 8)+"`n")
    $backupRoot=Join-Path $env:USERPROFILE 'Downloads\NCMM_Infrastructure_Backups';New-Item -ItemType Directory -Force $backupRoot|Out-Null
    $zip=Join-Path $backupRoot ('ncmm_infra_before_'+$id+'.zip'); Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    [pscustomobject]@{id=$id;stage=$stage;backup=$zip;metadata=$meta}
}
function Restore-NcmmTransactionSnapshot([object]$Snapshot) {
    $stage=[string]$Snapshot.stage; $meta=$Snapshot.metadata; $root=[string]$meta.game_root
    foreach($rel in @('cataclysm-tiles.exe','cataclysm-tiles.vanilla.exe','cataclysm-tiles.ncmm.exe','ncmm','code_mods\SurvivorProgression','code_mods\AdvancedWorldSettings')){
        $target=Join-Path $root $rel; Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue
        $had=$false; try{$had=[bool]$meta.presence[$rel]}catch{}
        $saved=Join-Path $stage $rel
        if($had -and (Test-Path $saved)){New-Item -ItemType Directory -Force (Split-Path -Parent $target)|Out-Null;Copy-Item $saved $target -Recurse -Force}
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
        'cataclysm-tiles.exe','cataclysm-tiles.vanilla.exe','cataclysm-tiles.ncmm.exe','ncmm\host.binding.json',
        'code_mods\SurvivorProgression\ncmm_mod.dll','code_mods\SurvivorProgression\mod.json',
        'code_mods\AdvancedWorldSettings\ncmm_mod.dll','code_mods\AdvancedWorldSettings\mod.json'
    )){
        $ok=Test-Path (Join-Path $root $f) -PathType Leaf
        $tests += New-NcmmVerificationRecord ('file.'+$f) $ok $f
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

    foreach($mod in @(
        @{id='survivor_progression';folder='SurvivorProgression';version='0.11.3'},
        @{id='advanced_world_settings';folder='AdvancedWorldSettings';version='0.6.2'}
    )){
        $mp=Join-Path $root ('code_mods\'+$mod.folder+'\mod.json')
        try{
            $mj=Get-Content $mp -Raw|ConvertFrom-Json
            $ok=([string]$mj.id -eq [string]$mod.id) -and ([string]$mj.version -eq [string]$mod.version)
            $tests += New-NcmmVerificationRecord ('module_manifest.'+$mod.id) $ok (([string]$mj.id)+' '+([string]$mj.version))
        } catch {$tests += New-NcmmVerificationRecord ('module_manifest.'+$mod.id) $false $_.Exception.Message}
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
        foreach($opt in $manifest.self_test.optional_modules){
            $folder=if([string]$opt.id -eq 'recipe_finalize_profiler'){'NCMM_Recipe_Finalization_Profiler'}else{[string]$opt.id}
            $dir=Join-Path $root ('code_mods\'+$folder)
            if(Test-Path $dir -PathType Container){
                $m=@($mods.modules|Where-Object{[string]$_.id -eq [string]$opt.id})|Select-Object -First 1
                $ok=$m -and [string]$m.state -eq 'loaded' -and [string]$m.lifecycle -eq 'active' -and [string]$m.version -eq [string]$opt.version
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
