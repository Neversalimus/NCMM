$ErrorActionPreference = 'Stop'

function Assert-NcmmPackageIntegrity([string]$PackageRoot) {
    $manifestPath=Join-Path $PackageRoot 'compat\compatibility.manifest.json'
    if(-not(Test-Path $manifestPath -PathType Leaf)){throw 'Compatibility manifest missing before integrity check.'}
    $m=Get-Content $manifestPath -Raw|ConvertFrom-Json
    $rel=[string]$m.package_integrity
    if([string]::IsNullOrWhiteSpace($rel)){throw 'Package integrity manifest is not declared.'}

    $ip=Join-Path $PackageRoot $rel
    $membershipPath=Join-Path $PackageRoot 'compat\package-files.txt'
    if(-not(Test-Path $ip -PathType Leaf)){throw "Package integrity manifest missing: $ip"}
    if(-not(Test-Path $membershipPath -PathType Leaf)){throw 'Package membership file missing: compat\package-files.txt'}

    $integrity=Get-Content $ip -Raw|ConvertFrom-Json
    if([int]$integrity.schema -ne 1 -or [string]$integrity.algorithm -ne 'sha256'){
        throw 'Unsupported package integrity schema/algorithm.'
    }

    $members=@(
        Get-Content $membershipPath |
        ForEach-Object { ([string]$_).Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') }
    )
    if($members.Count -eq 0){throw 'Package membership is empty.'}

    $memberMap=@{}
    foreach($member in $members){
        if([IO.Path]::IsPathRooted($member) -or $member.Contains('..')){
            throw "Unsafe package membership path: $member"
        }
        if($memberMap.ContainsKey($member)){throw "Duplicate package membership path: $member"}
        $memberMap[$member]=$true
    }

    $entryMap=@{}
    foreach($entry in @($integrity.files)){
        $path=[string]$entry.path
        if([string]::IsNullOrWhiteSpace($path)){throw 'Package integrity contains an empty path.'}
        if([IO.Path]::IsPathRooted($path) -or $path.Contains('..')){throw "Unsafe package integrity path: $path"}
        if($entryMap.ContainsKey($path)){throw "Duplicate package integrity path: $path"}
        $entryMap[$path]=$entry
    }

    if($entryMap.Count -ne $memberMap.Count){
        throw "Package integrity membership count mismatch: integrity=$($entryMap.Count), membership=$($memberMap.Count)."
    }
    foreach($member in $memberMap.Keys){
        if(-not $entryMap.ContainsKey($member)){throw "Package integrity entry missing for membership path: $member"}
    }
    foreach($path in $entryMap.Keys){
        if(-not $memberMap.ContainsKey($path)){throw "Package integrity contains undeclared membership path: $path"}
    }

    $failed=@()
    foreach($path in $memberMap.Keys){
        $entry=$entryMap[$path]
        $fp=Join-Path $PackageRoot $path
        if(-not(Test-Path $fp -PathType Leaf)){$failed += ($path+':missing');continue}

        $item=Get-Item -LiteralPath $fp
        if([int64]$item.Length -ne [int64]$entry.bytes){
            $failed += ($path+':bytes_mismatch')
            continue
        }

        $actual=Get-NcmmHash $fp
        if($actual -ne ([string]$entry.sha256).ToLowerInvariant()){
            $failed += ($path+':sha256_mismatch')
        }
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
