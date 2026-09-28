param(
 [Parameter(Mandatory=$true)][string]$ReleaseMetadata,
 [Parameter(Mandatory=$true)][string]$PackageUrl,
 [string]$PackageRoot=(Split-Path $PSScriptRoot -Parent),
 [switch]$Certified
)
$ErrorActionPreference='Stop';if($PackageUrl -notmatch '^https://'){throw 'PackageUrl must use HTTPS.'}
$r=Get-Content $ReleaseMetadata -Raw|ConvertFrom-Json;if([string]$r.sha256 -notmatch '^[0-9a-f]{64}$'){throw 'Release metadata SHA256 invalid.'}
$indexPath=Join-Path $PackageRoot 'compat\update\index.json';$feed=Get-Content $indexPath -Raw|ConvertFrom-Json
$rows=@($feed.releases|Where-Object{[string]$_.version -ne [string]$r.version})
$rows += [pscustomobject]@{version=[string]$r.version;status=$(if($Certified){'certified'}else{'candidate'});certification=$(if($Certified){'promoted-certified'}else{'manual-candidate'});package_url=$PackageUrl;package_sha256=[string]$r.sha256;components=$r.components;minimum_updater='0.8.3'}
$feed.releases=$rows;$feed.generated_utc=[DateTime]::UtcNow.ToString('o');[IO.File]::WriteAllText($indexPath,(($feed|ConvertTo-Json -Depth 12)+"`n"),(New-Object Text.UTF8Encoding($false)))
Write-Host ('Update release promoted to feed: '+$r.version+' -> '+$PackageUrl) -ForegroundColor Green
& (Join-Path $PackageRoot 'ci\Regenerate-PackageIntegrity.ps1') -PackageRoot $PackageRoot
