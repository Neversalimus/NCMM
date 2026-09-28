param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent),[string]$OutputDir=(Join-Path $PackageRoot 'watch-output'))
$ErrorActionPreference='Stop';New-Item -ItemType Directory -Force $OutputDir|Out-Null
$api='https://api.github.com/repos/CleverRaven/Cataclysm-DDA/releases?per_page=30';$headers=@{'User-Agent'='NCMM-Experimental-Watcher'};$rels=Invoke-RestMethod -UseBasicParsing -Headers $headers -Uri $api
$exp=@($rels|Where-Object{$_.tag_name -like 'cdda-experimental-*'}|Sort-Object {[datetime]$_.published_at} -Descending|Select-Object -First 1);if(-not $exp){throw 'No CDDA experimental release found.'};$exp=$exp[0]
$tag=[string]$exp.tag_name;$commit=[string]$exp.target_commitish
if($commit -notmatch '^[0-9a-f]{40}$'){
  $resolved=Invoke-RestMethod -UseBasicParsing -Headers $headers -Uri ('https://api.github.com/repos/CleverRaven/Cataclysm-DDA/commits/'+$tag)
  $commit=[string]$resolved.sha
}
$feed=Get-Content (Join-Path $PackageRoot 'compat\feed\index.json') -Raw|ConvertFrom-Json;$known=@($feed.entries|Where-Object{$_.commit -eq $commit}).Count -gt 0
$r=[ordered]@{schema=1;tag=$tag;commit=$commit;published_at=$exp.published_at;known=$known;action=$(if($known){'none'}else{'source_certification_requested'});checked_utc=[DateTime]::UtcNow.ToString('o')}
$p=Join-Path $OutputDir 'latest-experimental.json';[IO.File]::WriteAllText($p,(($r|ConvertTo-Json -Depth 8)+"`n"),(New-Object Text.UTF8Encoding($false)));Write-Host ($r|ConvertTo-Json -Compress);if($known){exit 0}else{exit 10}
