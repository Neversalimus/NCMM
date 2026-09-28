param(
    [ValidateSet('Check','Plan','Apply','MigrateState','ApplyPackage')][string]$Mode='Check',
    [string]$GameRoot='',
    [string]$BuildRoot='C:\NCMMBuild',
    [string[]]$Components=@('all'),
    [string]$FeedPath='',
    [string]$FeedUrl='',
    [string]$PackagePath='',
    [switch]$AllowUncertified
)
$ErrorActionPreference='Stop'
$PackageRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'NCMM.Update.Common.ps1')
[void](Assert-NcmmPackageIntegrity $PackageRoot)
$root=Resolve-NcmmGameRoot $GameRoot
$stateResult=Invoke-NcmmUpdateStateMigration $PackageRoot $root
if($Mode -eq 'MigrateState'){Write-Host ('Update state schema: '+$stateResult.state.schema+' | '+$stateResult.path) -ForegroundColor Green;exit 0}

if($Mode -eq 'ApplyPackage'){
    if(-not $PackagePath){throw '-PackagePath is required for ApplyPackage.'}
    $zip=(Resolve-Path $PackagePath).Path
    $stage=Join-Path $env:TEMP ('NCMM_UPDATE_PACKAGE_'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force $stage|Out-Null
    try{
        Expand-Archive -Path $zip -DestinationPath $stage -Force
        $manifestPath=Join-Path $stage 'compat\compatibility.manifest.json'
        if(-not(Test-Path $manifestPath -PathType Leaf)){throw 'Update package compatibility manifest is missing.'}
        $manifest=Get-Content $manifestPath -Raw|ConvertFrom-Json
        $entry=Join-Path $stage 'NCMM.ps1'
        if(-not(Test-Path $entry -PathType Leaf)){throw "Update package entrypoint missing: $entry"}
        . (Join-Path $stage 'tools\NCMM.Infrastructure.Common.ps1')
        [void](Assert-NcmmPackageIntegrity $stage)
        Write-Host ('Validated local update package: '+$manifest.infrastructure_version) -ForegroundColor Green
        & $entry -Action Install -GameRoot $root -BuildRoot $BuildRoot
        exit $LASTEXITCODE
    } finally {Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue}
}

if(-not $FeedPath -and -not $FeedUrl){
    $configuredFeed=Join-Path $root 'ncmm\update.feed.url'
    if(Test-Path $configuredFeed -PathType Leaf){$FeedUrl=([IO.File]::ReadAllText($configuredFeed)).Trim()}
}
$feed=Read-NcmmUpdateFeed $PackageRoot $FeedPath $FeedUrl
$plan=Resolve-NcmmDependencyPlan $PackageRoot $root $feed $Components
$planPath=Join-Path $root 'ncmm\update.plan.latest.json'
Write-NcmmUtf8NoBom $planPath (($plan|ConvertTo-Json -Depth 12)+"`n")
Write-Host ('Update release: '+$plan.release_version+' ['+$plan.release_status+']') -ForegroundColor Cyan
foreach($a in $plan.actions){Write-Host ('  {0,-28} {1,-10} {2} -> {3}  group={4}' -f $a.component,$a.action,$a.current,$a.desired,$a.atomic_group)}
if(($plan.requested -join '|') -ne ($plan.expanded -join '|')){Write-Host ('Atomic expansion: '+($plan.requested -join ', ')+' -> '+($plan.expanded -join ', ')) -ForegroundColor Yellow}
if(-not $plan.can_apply){foreach($e in $plan.errors){Write-Host ('ERROR: '+$e) -ForegroundColor Red};exit 31}

$state=$stateResult.state
$state.last_check_utc=[DateTime]::UtcNow.ToString('o')
$state.last_plan=$planPath
if($FeedUrl){$state.last_feed=$FeedUrl}elseif($FeedPath){$state.last_feed=$FeedPath}else{$state.last_feed='embedded'}
Write-NcmmUtf8NoBom $stateResult.path (($state|ConvertTo-Json -Depth 12)+"`n")
if($Mode -in @('Check','Plan')){Write-Host ('Plan: '+$planPath) -ForegroundColor Green;exit 0}

$changes=@($plan.actions|Where-Object{$_.action -ne 'keep'})
if($changes.Count -eq 0){Write-Host 'Everything selected is already current.' -ForegroundColor Green;exit 0}
if(-not $AllowUncertified -and [string]$plan.release_status -notin @('certified','embedded-current','supported_exact')){throw 'Refusing automatic apply of an uncertified release. Use -AllowUncertified only for deliberate testing.'}
if([string]::IsNullOrWhiteSpace($plan.package_url)){throw 'Feed has no package_url for this release. Use NCMM.cmd package <zip> with a downloaded NCMM package, or publish a certified package URL+SHA in the update feed.'}
$tmp=Join-Path $env:TEMP ('NCMM_UPDATE_'+[guid]::NewGuid().ToString('N')+'.zip')
try{
    Invoke-WebRequest -UseBasicParsing -Uri $plan.package_url -OutFile $tmp
    if($plan.package_sha256){$actual=(Get-FileHash $tmp -Algorithm SHA256).Hash.ToLowerInvariant();if($actual -ne ([string]$plan.package_sha256).ToLowerInvariant()){throw 'Downloaded update package SHA256 mismatch.'}}
    & $PSCommandPath -Mode ApplyPackage -GameRoot $root -BuildRoot $BuildRoot -PackagePath $tmp
    exit $LASTEXITCODE
} finally {Remove-Item $tmp -Force -ErrorAction SilentlyContinue}
