param([Parameter(Mandatory=$true)][string]$Candidate,[string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$c=Get-Content $Candidate -Raw|ConvertFrom-Json
$candidateStatus=[string]$c.status
if($candidateStatus -notin @('certified','structural_candidate')){throw 'Only CERTIFIED or SOURCE_CERTIFIED structural candidates can be promoted.'}
$commit=([string]$c.commit).ToLowerInvariant();if($commit -notmatch '^[0-9a-f]{40}$'){throw 'Candidate commit invalid.'}
$dst=Join-Path $PackageRoot ('compat\feed\commits\'+$commit+'.json');Copy-Item $Candidate $dst -Force
$indexPath=Join-Path $PackageRoot 'compat\feed\index.json';$idx=Get-Content $indexPath -Raw|ConvertFrom-Json
$rows=@($idx.entries|Where-Object{([string]$_.commit).ToLowerInvariant() -ne $commit});$rows += [pscustomobject]@{commit=$commit;status=$candidateStatus;manifest=('commits/'+$commit+'.json')};$idx.entries=$rows
[IO.File]::WriteAllText($indexPath,($idx|ConvertTo-Json -Depth 10)+"`n",(New-Object Text.UTF8Encoding($false)))
Write-Host ('Promoted '+$candidateStatus+' candidate to compatibility feed: '+$commit) -ForegroundColor Green
& (Join-Path $PackageRoot 'ci\Regenerate-PackageIntegrity.ps1') -PackageRoot $PackageRoot
Write-Host 'Compatibility feed promotion complete; package integrity regenerated.' -ForegroundColor Green
