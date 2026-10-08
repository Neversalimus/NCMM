param([Parameter(Mandatory=$true)][string]$RepositoryRoot)
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools\NCMM.Update.Common.ps1')
$work=Join-Path $env:TEMP ('ncmm-dependency-audit-'+[guid]::NewGuid().ToString('N'))
$pkg=Join-Path $work 'package'; $game=Join-Path $work 'game'
New-Item -ItemType Directory -Force (Join-Path $pkg 'components'),(Join-Path $game 'ncmm') | Out-Null
$catalog=[pscustomobject]@{infrastructure_version='0.8.3.1';components=@(
  [pscustomobject]@{id='ncmm_host';kind='native_host';required=$true;atomic_group='host';provides=@('core.v1','future.core');dependencies=@()},
  [pscustomobject]@{id='audit_fixture';kind='native_module';required=$false;atomic_group='fixture';provides=@();dependencies=@([pscustomobject]@{component='ncmm_host';min_version='0.8.1'},[pscustomobject]@{capability='future.core'})}
)}
$catalog|ConvertTo-Json -Depth 12|Set-Content (Join-Path $pkg 'components\index.json') -Encoding UTF8
@{ncmm_version='0.8.2'}|ConvertTo-Json|Set-Content (Join-Path $game 'ncmm\host.binding.json') -Encoding UTF8
@{modules=@();capabilities=@('core.v1')}|ConvertTo-Json|Set-Content (Join-Path $game 'ncmm\modules.state.json') -Encoding UTF8
$feed=[pscustomobject]@{releases=@([pscustomobject]@{version='1.0.0';status='certified';package_url='https://example.invalid/package.zip';package_sha256=('a'*64);components=[pscustomobject]@{ncmm_host='0.8.3';audit_fixture='0.1.0'}})}
$plan=Resolve-NcmmDependencyPlan $pkg $game $feed @('audit_fixture')
$plan|ConvertTo-Json -Depth 12
if(-not $plan.can_apply -or @($plan.expanded) -contains 'ncmm_host'){throw 'Capability overclaim did not reproduce.'}
Write-Host 'REPRODUCED | requested module accepted because of capability provided only by unselected release Host'
$control=Resolve-NcmmDependencyPlan $pkg $game $feed @('audit_fixture','ncmm_host')
if(-not $control.can_apply -or @($control.expanded) -notcontains 'ncmm_host'){throw 'Valid control plan rejected.'}
Write-Host 'CONTROL_PASS | explicitly selected provider is available'
$futureState=Join-Path $game 'ncmm\update.state.json'
@{schema=999;history=@();sentinel='untouched'}|ConvertTo-Json|Set-Content $futureState -Encoding UTF8
$result=Invoke-NcmmUpdateStateMigration $RepositoryRoot $game
if([int]$result.state.schema -ne 999){throw 'Unexpected future-schema behavior.'}
Write-Host 'REPRODUCED | newer update-state schema 999 accepted and rewritten rather than rejected'
