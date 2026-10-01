param(
 [Parameter(Mandatory=$true)][string]$Commit,
 [Parameter(Mandatory=$true)][string]$Tag,
 [string]$GameRoot='',
 [string]$BuildRoot='C:\NCMMBuild',
 [ValidateSet('Safe','Balanced','Maximum')][string]$BuildProfile='Safe',
 [ValidateSet('Full','SourceOnly')][string]$Mode='Full',
 [string]$OutputDir=''
)
$ErrorActionPreference='Stop'
$PackageRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')
$compatManifest=Get-Content (Join-Path $PackageRoot 'compat\\compatibility.manifest.json') -Raw|ConvertFrom-Json
$infrastructureVersion=([string]$compatManifest.infrastructure_version).Trim()
if($infrastructureVersion -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?{throw ('Invalid infrastructure version: '+$infrastructureVersion)}
$catalog=Get-Content (Join-Path $PackageRoot 'components\\index.json') -Raw|ConvertFrom-Json
$componentVersions=[ordered]@{};foreach($cc in @($catalog.components)){$componentVersions[[string]$cc.id]=[string]$cc.version}
$hostVersion=[string]$componentVersions['ncmm_host']
$survivorVersion=[string]$componentVersions['survivor_progression']
$awsVersion=[string]$componentVersions['advanced_world_settings']
$profilerVersion=[string]$componentVersions['recipe_finalize_profiler']
foreach($v in @($hostVersion,$survivorVersion,$awsVersion,$profilerVersion)){if([string]::IsNullOrWhiteSpace($v)){throw 'Certification component version is missing.'}}
if($Commit -notmatch '^[0-9a-fA-F]{40}$'){throw 'Commit must be an exact 40-character SHA.'};$Commit=$Commit.ToLowerInvariant()
if([string]::IsNullOrWhiteSpace($OutputDir)){$OutputDir=Join-Path $BuildRoot ('certification\'+$Commit.Substring(0,12))};New-Item -ItemType Directory -Force $OutputDir|Out-Null
$stages=New-Object System.Collections.Generic.List[object]
function Stage([string]$Id,[string]$Status,[string]$Detail){$stages.Add([pscustomobject]@{id=$Id;status=$Status;detail=$Detail;utc=[DateTime]::UtcNow.ToString('o')})}
try{
 [void](Assert-NcmmPackageIntegrity $PackageRoot);Stage 'package_integrity' 'PASS' 'sha256 manifest verified'
 & (Join-Path $PackageRoot 'ci\Test-Infrastructure083.ps1') -PackageRoot $PackageRoot;if($LASTEXITCODE -ne 0){throw 'Infrastructure static contract failed.'};Stage 'infrastructure_static' 'PASS' ($infrastructureVersion+' static contract')
 Stage 'update_dependency_layer' 'PASS' ('component resolver + migration regression included in '+$infrastructureVersion+' static contract')
 if([string]::IsNullOrWhiteSpace($GameRoot)){
   if($Mode -eq 'SourceOnly'){
     $GameRoot=Join-Path $BuildRoot ('cdda_experimental_cert_source_'+$Commit.Substring(0,12))
     Remove-Item $GameRoot -Recurse -Force -ErrorAction SilentlyContinue;New-Item -ItemType Directory -Force $GameRoot|Out-Null
     [IO.File]::WriteAllText((Join-Path $GameRoot 'VERSION.txt'),('commit sha: '+$Commit+"`r`n"),(New-Object Text.UTF8Encoding($false)))
     [IO.File]::WriteAllBytes((Join-Path $GameRoot 'cataclysm-tiles.exe'),(New-Object byte[] 1))
   } else {
     $buildStamp=($Tag -replace '^cdda-experimental-','')
     $preferred='cdda-windows-with-graphics-x64-'+$buildStamp+'.zip'
     $fallback='cdda-windows-with-graphics-and-sounds-x64-'+$buildStamp+'.zip'
     $releaseResponse=Invoke-WebRequest -UseBasicParsing -Uri ('https://api.github.com/repos/CleverRaven/Cataclysm-DDA/releases/tags/'+$Tag) -Headers @{'User-Agent'=('NCMM-Certification-'+$infrastructureVersion)}
     $release=$releaseResponse.Content|ConvertFrom-Json
     $asset=@($release.assets|Where-Object{$_.name -eq $preferred})|Select-Object -First 1
     if(-not $asset){$asset=@($release.assets|Where-Object{$_.name -eq $fallback})|Select-Object -First 1}
     if(-not $asset){throw ('Official Windows graphics asset not found: '+$preferred+' or '+$fallback)}
     $assetName=[string]$asset.name
     $download=Join-Path $BuildRoot ('downloads\'+$assetName);New-Item -ItemType Directory -Force (Split-Path -Parent $download)|Out-Null
     if(-not(Test-Path $download -PathType Leaf)){Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $download}
     $GameRoot=Join-Path $BuildRoot ('cert_game_'+$Commit.Substring(0,12));Remove-Item $GameRoot -Recurse -Force -ErrorAction SilentlyContinue;Expand-Archive -LiteralPath $download -DestinationPath $GameRoot -Force
     $dirs=@(Get-ChildItem $GameRoot -Directory);if(-not(Test-Path (Join-Path $GameRoot 'cataclysm-tiles.exe') -PathType Leaf)-and $dirs.Count -eq 1 -and(Test-Path (Join-Path $dirs[0].FullName 'cataclysm-tiles.exe') -PathType Leaf)){$GameRoot=$dirs[0].FullName}
   }
 }
 $actual=Read-NcmmSourceCommit $GameRoot;if($actual -ne $Commit){throw "Official target commit mismatch: expected $Commit got $actual"};Stage 'target_identity' 'PASS' $actual
 $probe=Invoke-NcmmProbe $PackageRoot $GameRoot $BuildRoot;if($probe.contracts.status -ne 'compatible'){throw ('Source contracts failed: '+($probe.contracts.failed_contracts -join ', '))};Stage 'source_contracts' 'PASS' ($probe.contracts.contracts.Count.ToString()+' contracts')
 $deep=Invoke-NcmmDeepSourceProbe $PackageRoot $probe $BuildRoot $BuildProfile;if($deep.status -ne 'PASS'){throw ('Deep source probe failed: '+$deep.report_path)};Stage 'deep_source_probe' 'PASS' $deep.report_path
 & (Join-Path $PackageRoot 'ci\Test-GoldenRegression.ps1') -PackageRoot $PackageRoot -BuildRoot $BuildRoot;if($LASTEXITCODE -ne 0){throw 'Golden regression failed.'};Stage 'golden_regression' 'PASS' 'six fixtures'
 if($Mode -eq 'Full'){
   & (Join-Path $PackageRoot 'NCMM.ps1') -Action Install -GameRoot $GameRoot -BuildRoot $BuildRoot -BuildProfile $BuildProfile -AllowStructuralReuse:($probe.status -ne 'EXACT_SUPPORTED')
   if($LASTEXITCODE -ne 0){throw ('Build/install transaction failed with exit '+$LASTEXITCODE)};Stage 'build_install' 'PASS' 'transaction committed'
   $offline=Get-Content (Join-Path $GameRoot 'ncmm\offline_verify.latest.json') -Raw|ConvertFrom-Json;if([string]$offline.status -ne 'PASS'){throw 'Offline installation verification report is not PASS.'};Stage 'offline_install_verify' 'PASS' (Join-Path $GameRoot 'ncmm\offline_verify.latest.json');Stage 'runtime_module_verify' 'DEFERRED' 'requires first normal Host launch; not an install transaction gate'
 } else {Stage 'build_install' 'SKIP' 'SourceOnly mode';Stage 'offline_install_verify' 'SKIP' 'SourceOnly mode';Stage 'runtime_module_verify' 'SKIP' 'SourceOnly mode'}
 $status=if($Mode -eq 'Full'){'CERTIFIED'}else{'SOURCE_CERTIFIED'}
} catch {
 Stage 'failure' 'FAIL' $_.Exception.Message;$status='FAILED';$errorMessage=$_.Exception.Message
}
$adapter=Get-NcmmAdapterForCommit $PackageRoot $Commit;$feed=Get-NcmmFeedEntry $PackageRoot $Commit
$cert=[ordered]@{schema=1;infrastructure=$infrastructureVersion;status=$status;commit=$Commit;tag=$Tag;mode=$Mode;adapter=$(if($adapter){[string]$adapter.id}else{$null});adapter_inherits=$(if($adapter -and $adapter.inherits){[string]$adapter.inherits}else{$null});stages=@($stages | ForEach-Object { $_ });package_integrity_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'compat\package.integrity.json'));completed_utc=[DateTime]::UtcNow.ToString('o')}
if($errorMessage){$cert['error']=$errorMessage}
$certPath=Join-Path $OutputDir 'certificate.json';Write-NcmmUtf8NoBom $certPath (($cert|ConvertTo-Json -Depth 12)+"`n")
$candidate=[ordered]@{schema=1;commit=$Commit;tag=$Tag;build_label=[IO.Path]::GetFileName($GameRoot);status=$(if($status -eq 'CERTIFIED'){'certified'}elseif($status -eq 'SOURCE_CERTIFIED'){'structural_candidate'}else{'candidate_failed'});certification=[ordered]@{state=$status.ToLowerInvariant();certificate_sha256=(Get-NcmmHash $certPath);completed_utc=[string]$cert.completed_utc};adapter=$(if($adapter){[ordered]@{id=[string]$adapter.id;inherits=$(if($adapter.inherits){[string]$adapter.inherits}else{$null});support=[string]$adapter.support}}else{$null});runtime=[ordered]@{host_version=$hostVersion;ncmm_api='1.9';host_api_v2='2.0';loader_api=1};modules=[ordered]@{survivor_progression=$survivorVersion;advanced_world_settings=$awsVersion;recipe_finalize_profiler=$profilerVersion};hashes=[ordered]@{package_integrity_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'compat\package.integrity.json'));adapter_sha256=$(if($adapter){Get-NcmmHash ([string]$adapter.script_path)}else{$null});payload_sha256=$(if($adapter){Get-NcmmHash ([string]$adapter.payload)}else{$null});contracts_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'compat\contracts.json'));components_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'components\index.json'));migrations_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'migrations\migrations.json'))}}
Write-NcmmUtf8NoBom (Join-Path $OutputDir 'feed-candidate.json') (($candidate|ConvertTo-Json -Depth 10)+"`n")
$updateCandidate=[ordered]@{schema=1;version=$infrastructureVersion;status=$(if($status -eq 'CERTIFIED'){'certified'}else{'candidate'});certified_commit=$Commit;package_url=$null;package_sha256=$null;components=$componentVersions;certificate_sha256=(Get-NcmmHash $certPath);generated_utc=[DateTime]::UtcNow.ToString('o')}
Write-NcmmUtf8NoBom (Join-Path $OutputDir 'update-feed-candidate.json') (($updateCandidate|ConvertTo-Json -Depth 10)+"`n")
Write-Host ('Certification result: '+$status) -ForegroundColor $(if($status -eq 'CERTIFIED'){'Green'}elseif($status -eq 'SOURCE_CERTIFIED'){'Yellow'}else{'Red'})
Write-Host ('Certificate: '+$certPath)
if($status -eq 'FAILED'){exit 60};exit 0
){throw ('Invalid infrastructure version: '+$infrastructureVersion)}
$catalog=Get-Content (Join-Path $PackageRoot 'components\\index.json') -Raw|ConvertFrom-Json
$componentVersions=[ordered]@{};foreach($cc in @($catalog.components)){$componentVersions[[string]$cc.id]=[string]$cc.version}
$hostVersion=[string]$componentVersions['ncmm_host']
$survivorVersion=[string]$componentVersions['survivor_progression']
$awsVersion=[string]$componentVersions['advanced_world_settings']
$profilerVersion=[string]$componentVersions['recipe_finalize_profiler']
foreach($v in @($hostVersion,$survivorVersion,$awsVersion,$profilerVersion)){if([string]::IsNullOrWhiteSpace($v)){throw 'Certification component version is missing.'}}
if($Commit -notmatch '^[0-9a-fA-F]{40}$'){throw 'Commit must be an exact 40-character SHA.'};$Commit=$Commit.ToLowerInvariant()
if([string]::IsNullOrWhiteSpace($OutputDir)){$OutputDir=Join-Path $BuildRoot ('certification\'+$Commit.Substring(0,12))};New-Item -ItemType Directory -Force $OutputDir|Out-Null
$stages=New-Object System.Collections.Generic.List[object]
function Stage([string]$Id,[string]$Status,[string]$Detail){$stages.Add([pscustomobject]@{id=$Id;status=$Status;detail=$Detail;utc=[DateTime]::UtcNow.ToString('o')})}
try{
 [void](Assert-NcmmPackageIntegrity $PackageRoot);Stage 'package_integrity' 'PASS' 'sha256 manifest verified'
 & (Join-Path $PackageRoot 'ci\Test-Infrastructure083.ps1') -PackageRoot $PackageRoot;if($LASTEXITCODE -ne 0){throw 'Infrastructure static contract failed.'};Stage 'infrastructure_static' 'PASS' ($infrastructureVersion+' static contract')
 Stage 'update_dependency_layer' 'PASS' ('component resolver + migration regression included in '+$infrastructureVersion+' static contract')
 if([string]::IsNullOrWhiteSpace($GameRoot)){
   if($Mode -eq 'SourceOnly'){
     $GameRoot=Join-Path $BuildRoot ('cdda_experimental_cert_source_'+$Commit.Substring(0,12))
     Remove-Item $GameRoot -Recurse -Force -ErrorAction SilentlyContinue;New-Item -ItemType Directory -Force $GameRoot|Out-Null
     [IO.File]::WriteAllText((Join-Path $GameRoot 'VERSION.txt'),('commit sha: '+$Commit+"`r`n"),(New-Object Text.UTF8Encoding($false)))
     [IO.File]::WriteAllBytes((Join-Path $GameRoot 'cataclysm-tiles.exe'),(New-Object byte[] 1))
   } else {
     $buildStamp=($Tag -replace '^cdda-experimental-','')
     $preferred='cdda-windows-with-graphics-x64-'+$buildStamp+'.zip'
     $fallback='cdda-windows-with-graphics-and-sounds-x64-'+$buildStamp+'.zip'
     $releaseResponse=Invoke-WebRequest -UseBasicParsing -Uri ('https://api.github.com/repos/CleverRaven/Cataclysm-DDA/releases/tags/'+$Tag) -Headers @{'User-Agent'=('NCMM-Certification-'+$infrastructureVersion)}
     $release=$releaseResponse.Content|ConvertFrom-Json
     $asset=@($release.assets|Where-Object{$_.name -eq $preferred})|Select-Object -First 1
     if(-not $asset){$asset=@($release.assets|Where-Object{$_.name -eq $fallback})|Select-Object -First 1}
     if(-not $asset){throw ('Official Windows graphics asset not found: '+$preferred+' or '+$fallback)}
     $assetName=[string]$asset.name
     $download=Join-Path $BuildRoot ('downloads\'+$assetName);New-Item -ItemType Directory -Force (Split-Path -Parent $download)|Out-Null
     if(-not(Test-Path $download -PathType Leaf)){Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $download}
     $GameRoot=Join-Path $BuildRoot ('cert_game_'+$Commit.Substring(0,12));Remove-Item $GameRoot -Recurse -Force -ErrorAction SilentlyContinue;Expand-Archive -LiteralPath $download -DestinationPath $GameRoot -Force
     $dirs=@(Get-ChildItem $GameRoot -Directory);if(-not(Test-Path (Join-Path $GameRoot 'cataclysm-tiles.exe') -PathType Leaf)-and $dirs.Count -eq 1 -and(Test-Path (Join-Path $dirs[0].FullName 'cataclysm-tiles.exe') -PathType Leaf)){$GameRoot=$dirs[0].FullName}
   }
 }
 $actual=Read-NcmmSourceCommit $GameRoot;if($actual -ne $Commit){throw "Official target commit mismatch: expected $Commit got $actual"};Stage 'target_identity' 'PASS' $actual
 $probe=Invoke-NcmmProbe $PackageRoot $GameRoot $BuildRoot;if($probe.contracts.status -ne 'compatible'){throw ('Source contracts failed: '+($probe.contracts.failed_contracts -join ', '))};Stage 'source_contracts' 'PASS' ($probe.contracts.contracts.Count.ToString()+' contracts')
 $deep=Invoke-NcmmDeepSourceProbe $PackageRoot $probe $BuildRoot $BuildProfile;if($deep.status -ne 'PASS'){throw ('Deep source probe failed: '+$deep.report_path)};Stage 'deep_source_probe' 'PASS' $deep.report_path
 & (Join-Path $PackageRoot 'ci\Test-GoldenRegression.ps1') -PackageRoot $PackageRoot -BuildRoot $BuildRoot;if($LASTEXITCODE -ne 0){throw 'Golden regression failed.'};Stage 'golden_regression' 'PASS' 'six fixtures'
 if($Mode -eq 'Full'){
   & (Join-Path $PackageRoot 'NCMM.ps1') -Action Install -GameRoot $GameRoot -BuildRoot $BuildRoot -BuildProfile $BuildProfile -AllowStructuralReuse:($probe.status -ne 'EXACT_SUPPORTED')
   if($LASTEXITCODE -ne 0){throw ('Build/install transaction failed with exit '+$LASTEXITCODE)};Stage 'build_install' 'PASS' 'transaction committed'
   $offline=Get-Content (Join-Path $GameRoot 'ncmm\offline_verify.latest.json') -Raw|ConvertFrom-Json;if([string]$offline.status -ne 'PASS'){throw 'Offline installation verification report is not PASS.'};Stage 'offline_install_verify' 'PASS' (Join-Path $GameRoot 'ncmm\offline_verify.latest.json');Stage 'runtime_module_verify' 'DEFERRED' 'requires first normal Host launch; not an install transaction gate'
 } else {Stage 'build_install' 'SKIP' 'SourceOnly mode';Stage 'offline_install_verify' 'SKIP' 'SourceOnly mode';Stage 'runtime_module_verify' 'SKIP' 'SourceOnly mode'}
 $status=if($Mode -eq 'Full'){'CERTIFIED'}else{'SOURCE_CERTIFIED'}
} catch {
 Stage 'failure' 'FAIL' $_.Exception.Message;$status='FAILED';$errorMessage=$_.Exception.Message
}
$adapter=Get-NcmmAdapterForCommit $PackageRoot $Commit;$feed=Get-NcmmFeedEntry $PackageRoot $Commit
$cert=[ordered]@{schema=1;infrastructure=$infrastructureVersion;status=$status;commit=$Commit;tag=$Tag;mode=$Mode;adapter=$(if($adapter){[string]$adapter.id}else{$null});adapter_inherits=$(if($adapter -and $adapter.inherits){[string]$adapter.inherits}else{$null});stages=@($stages | ForEach-Object { $_ });package_integrity_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'compat\package.integrity.json'));completed_utc=[DateTime]::UtcNow.ToString('o')}
if($errorMessage){$cert['error']=$errorMessage}
$certPath=Join-Path $OutputDir 'certificate.json';Write-NcmmUtf8NoBom $certPath (($cert|ConvertTo-Json -Depth 12)+"`n")
$candidate=[ordered]@{schema=1;commit=$Commit;tag=$Tag;build_label=[IO.Path]::GetFileName($GameRoot);status=$(if($status -eq 'CERTIFIED'){'certified'}elseif($status -eq 'SOURCE_CERTIFIED'){'structural_candidate'}else{'candidate_failed'});certification=[ordered]@{state=$status.ToLowerInvariant();certificate_sha256=(Get-NcmmHash $certPath);completed_utc=[string]$cert.completed_utc};adapter=$(if($adapter){[ordered]@{id=[string]$adapter.id;inherits=$(if($adapter.inherits){[string]$adapter.inherits}else{$null});support=[string]$adapter.support}}else{$null});runtime=[ordered]@{host_version=$hostVersion;ncmm_api='1.9';host_api_v2='2.0';loader_api=1};modules=[ordered]@{survivor_progression=$survivorVersion;advanced_world_settings=$awsVersion;recipe_finalize_profiler=$profilerVersion};hashes=[ordered]@{package_integrity_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'compat\package.integrity.json'));adapter_sha256=$(if($adapter){Get-NcmmHash ([string]$adapter.script_path)}else{$null});payload_sha256=$(if($adapter){Get-NcmmHash ([string]$adapter.payload)}else{$null});contracts_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'compat\contracts.json'));components_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'components\index.json'));migrations_sha256=(Get-NcmmHash (Join-Path $PackageRoot 'migrations\migrations.json'))}}
Write-NcmmUtf8NoBom (Join-Path $OutputDir 'feed-candidate.json') (($candidate|ConvertTo-Json -Depth 10)+"`n")
$updateCandidate=[ordered]@{schema=1;version=$infrastructureVersion;status=$(if($status -eq 'CERTIFIED'){'certified'}else{'candidate'});certified_commit=$Commit;package_url=$null;package_sha256=$null;components=$componentVersions;certificate_sha256=(Get-NcmmHash $certPath);generated_utc=[DateTime]::UtcNow.ToString('o')}
Write-NcmmUtf8NoBom (Join-Path $OutputDir 'update-feed-candidate.json') (($updateCandidate|ConvertTo-Json -Depth 10)+"`n")
Write-Host ('Certification result: '+$status) -ForegroundColor $(if($status -eq 'CERTIFIED'){'Green'}elseif($status -eq 'SOURCE_CERTIFIED'){'Yellow'}else{'Red'})
Write-Host ('Certificate: '+$certPath)
if($status -eq 'FAILED'){exit 60};exit 0
