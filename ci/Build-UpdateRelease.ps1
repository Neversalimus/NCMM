param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent),[string]$OutputDir=(Join-Path $PackageRoot 'release-output'))
$ErrorActionPreference='Stop';. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')
[void](Assert-NcmmPackageIntegrity $PackageRoot)
$manifest=Get-Content (Join-Path $PackageRoot 'compat\compatibility.manifest.json') -Raw|ConvertFrom-Json
$catalog=Get-Content (Join-Path $PackageRoot 'components\index.json') -Raw|ConvertFrom-Json
New-Item -ItemType Directory -Force $OutputDir|Out-Null
$version=[string]$manifest.infrastructure_version;$asset='NCMM_INFRASTRUCTURE_v'+$version+'_UPDATE_DEPENDENCY.zip';$zip=Join-Path $OutputDir $asset
$stage=Join-Path $env:TEMP ('NCMM_RELEASE_STAGE_'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force $stage|Out-Null
try{
  foreach($item in Get-ChildItem $PackageRoot -Force){if($item.Name -in @('certification-output','release-output','watch-output')){continue};$dst=Join-Path $stage $item.Name;if($item.PSIsContainer){Copy-Item $item.FullName $dst -Recurse -Force}else{Copy-Item $item.FullName $dst -Force}}
  Remove-Item $zip -Force -ErrorAction SilentlyContinue;Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
}finally{Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue}
$sha=(Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant();$versions=[ordered]@{};foreach($c in @($catalog.components)){$versions[[string]$c.id]=[string]$c.version}
$meta=[ordered]@{schema=1;version=$version;asset=$asset;sha256=$sha;bytes=(Get-Item $zip).Length;components=$versions;generated_utc=[DateTime]::UtcNow.ToString('o')}
$metaPath=Join-Path $OutputDir 'update-release.json';Write-NcmmUtf8NoBom $metaPath (($meta|ConvertTo-Json -Depth 8)+"`n")
$candidatePath=Join-Path $PackageRoot 'certification-output\update-feed-candidate.json'
if(Test-Path $candidatePath -PathType Leaf){$cand=Get-Content $candidatePath -Raw|ConvertFrom-Json;$obj=[ordered]@{};foreach($p in $cand.PSObject.Properties){$obj[$p.Name]=$p.Value};$obj['package_asset_name']=$asset;$obj['package_sha256']=$sha;Write-NcmmUtf8NoBom (Join-Path $OutputDir 'update-feed-candidate.json') (([pscustomobject]$obj|ConvertTo-Json -Depth 10)+"`n")}
Write-Host ('Update release package: '+$zip) -ForegroundColor Green;Write-Host ('SHA256: '+$sha);Write-Output $zip
