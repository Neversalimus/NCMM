param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/NCMM.Update.Common.ps1')
$work=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-audit-legacy-'+[guid]::NewGuid().ToString('N'))
function Check([bool]$ok,[string]$message){if(-not $ok){throw $message}}
function JsonFile([string]$path,$obj){New-Item -ItemType Directory -Force (Split-Path $path -Parent)|Out-Null;[IO.File]::WriteAllText($path,($obj|ConvertTo-Json -Depth 20),(New-Object Text.UTF8Encoding($false)))}
try {
    New-Item -ItemType Directory -Force $work|Out-Null
    $game=Join-Path $work 'game';New-Item -ItemType Directory -Force (Join-Path $game 'ncmm')|Out-Null
    [IO.File]::WriteAllText((Join-Path $game 'cataclysm-tiles.exe'),'fixture')
    $state=Join-Path $game 'ncmm/update.state.json'
    JsonFile $state ([ordered]@{schema=999;future='keep exactly'})
    $before=[IO.File]::ReadAllBytes($state)
    $rejected=$false;try{Invoke-NcmmUpdateStateMigration $RepositoryRoot $game|Out-Null}catch{$rejected=$true}
    Check $rejected 'Future schema was accepted'
    Check ([Convert]::ToBase64String($before) -ceq [Convert]::ToBase64String([IO.File]::ReadAllBytes($state))) 'Future state was overwritten'
    $package=Join-Path $work 'package'
    $catalog=[pscustomobject]@{infrastructure_version='1.0.0';components=@(
        [pscustomobject]@{id='consumer';atomic_group='consumer';required=$false;provides=@();dependencies=@([pscustomobject]@{capability='test.cap'})},
        [pscustomobject]@{id='provider';atomic_group='provider';required=$false;provides=@('test.cap');dependencies=@()})}
    JsonFile (Join-Path $package 'components/index.json') $catalog
    $feed=[pscustomobject]@{releases=@([pscustomobject]@{version='1.0.0';status='certified';components=[pscustomobject]@{consumer='1.0.0';provider='1.0.0'}})}
    $plan=Resolve-NcmmDependencyPlan $package $game $feed @('consumer')
    Check (-not $plan.can_apply) 'An unselected feed provider supplied capability'
    $plan=Resolve-NcmmDependencyPlan $package $game $feed @('consumer','provider')
    Check $plan.can_apply 'An explicitly selected provider did not supply capability'
    $catalog.components[0].dependencies=@([pscustomobject]@{component='provider';min_version='2.0.0'})
    JsonFile (Join-Path $package 'components/index.json') $catalog
    JsonFile (Join-Path $game 'ncmm/modules.state.json') ([pscustomobject]@{modules=@([pscustomobject]@{id='provider';version='3.0.0'});capabilities=@('test.cap')})
    $plan=Resolve-NcmmDependencyPlan $package $game $feed @('consumer')
    Check $plan.can_apply 'Planner ignored the retained installed dependency version'
    $plan=Resolve-NcmmDependencyPlan $package $game $feed @('consumer','provider')
    Check (-not $plan.can_apply) 'Planner accepted an incompatible selected provider downgrade'
    $qualified=[pscustomobject]@{runs=@([pscustomobject]@{id=1;head_sha=('a'*40);status='completed';conclusion='success';path='.github/workflows/ncmm-installation-matrix.yml'});jobs=@([pscustomobject]@{name='real-install-smoke';conclusion='success';steps=@(
        [pscustomobject]@{name='Launch certified Host twice through installed bootstrap';conclusion='success'},
        [pscustomobject]@{name='Run real AWS worldgen + Survivor gameplay smoke';conclusion='success'},
        [pscustomobject]@{name='Verify certified Host + modules and restore vanilla';conclusion='success'})})}
    $fixture=Join-Path $work 'qualification.json';JsonFile $fixture $qualified
    & (Join-Path $RepositoryRoot 'ci/Test-ReleaseQualification.ps1') -SourceSha ('a'*40) -FixturePath $fixture
    foreach($fault in @('sha','skipped-gameplay','wrong-workflow')) {
        $copy=$qualified|ConvertTo-Json -Depth 20|ConvertFrom-Json
        if($fault -eq 'sha'){$copy.runs[0].head_sha='b'*40}
        if($fault -eq 'skipped-gameplay'){$copy.jobs[0].steps[1].conclusion='skipped'}
        if($fault -eq 'wrong-workflow'){$copy.runs[0].path='.github/workflows/unrelated.yml'}
        JsonFile $fixture $copy;$rejected=$false
        try{& (Join-Path $RepositoryRoot 'ci/Test-ReleaseQualification.ps1') -SourceSha ('a'*40) -FixturePath $fixture}catch{$rejected=$true}
        Check $rejected "Unqualified release accepted: $fault"
    }
    Write-Host 'Audit legacy planning/schema + exact release qualification: PASS' -ForegroundColor Green
} finally {Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue}
