param(
    [string]$Repository=$env:GITHUB_REPOSITORY,
    [string]$SourceSha=$env:GITHUB_SHA,
    [string]$FixturePath=''
)
$ErrorActionPreference='Stop'
if($SourceSha -notmatch '^[0-9a-f]{40}$'){throw 'Release requires an exact tested source SHA.'}
$requiredSteps=@('Launch certified Host twice through installed bootstrap',
    'Run real AWS worldgen + Survivor gameplay smoke',
    'Verify certified Host + modules and restore vanilla')
if($FixturePath){
    $fixture=Get-Content -LiteralPath $FixturePath -Raw|ConvertFrom-Json
    $runs=@($fixture.runs)
}else{
    $response=& gh api "repos/$Repository/actions/workflows/ncmm-installation-matrix.yml/runs?head_sha=$SourceSha&status=success&per_page=100"
    if($LASTEXITCODE -ne 0){throw 'Could not read release qualification evidence.'}
    $runs=@(($response|ConvertFrom-Json).workflow_runs)
}
$qualified=$false
foreach($run in $runs){
    if($run.head_sha -ne $SourceSha -or $run.status -ne 'completed' -or $run.conclusion -ne 'success' -or
       $run.path -ne '.github/workflows/ncmm-installation-matrix.yml'){continue}
    if($FixturePath){$jobs=@($fixture.jobs)}else{
        $response=& gh api "repos/$Repository/actions/runs/$($run.id)/jobs?filter=latest&per_page=100"
        if($LASTEXITCODE -ne 0){throw 'Could not read qualification job details.'}
        $jobs=@(($response|ConvertFrom-Json).jobs)
    }
    foreach($job in $jobs){
        if($job.name -ne 'real-install-smoke' -or $job.conclusion -ne 'success'){continue}
        $passed=@($job.steps|Where-Object{$_.conclusion -eq 'success'}|ForEach-Object{$_.name})
        $missing=@($requiredSteps|Where-Object{$passed -notcontains $_})
        if($missing.Count -eq 0){$qualified=$true;break}
    }
    if($qualified){break}
}
if(-not $qualified){throw "No complete Real Installation Matrix evidence for EXACT source $SourceSha. Release blocked."}
Write-Host "Release qualification: PASS (exact source $SourceSha, real launches, gameplay, restoration)." -ForegroundColor Green
