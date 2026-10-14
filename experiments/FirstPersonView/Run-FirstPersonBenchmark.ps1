param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$BundleRoot=$PSScriptRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$TilesetRoot,
    [ValidateRange(0,3600)][int]$SoakSeconds=1800
)
$ErrorActionPreference='Stop'
$GameRoot=(Resolve-Path $GameRoot).Path
$BundleRoot=(Resolve-Path $BundleRoot).Path
$payload=Join-Path $BundleRoot 'payload'
$bundle=Get-Content (Join-Path $payload 'first-person-bundle.json') -Raw | ConvertFrom-Json
if($bundle.schema -ne 1 -or $bundle.source_commit -notin @('3f7fb352bf492ba521bd9408a0c9f6ce239e8d83','074aa98bd5be3de4c35f154082db32a0e63bb0f1')){throw 'An exact supported First Person TEST bundle is required.'}
$version=Get-Content (Join-Path $GameRoot 'VERSION.txt') -Raw
$commit=[regex]::Match($version,'commit sha:\s*([0-9a-fA-F]{7,40})(?![0-9a-fA-F])')
if(-not $commit.Success -or -not $bundle.source_commit.StartsWith($commit.Groups[1].Value.ToLowerInvariant())){throw 'Game data and bundled Host source pins differ.'}
foreach($file in $bundle.files.PSObject.Properties){
    if((Get-FileHash (Join-Path $payload $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Value){throw "Bundle SHA256 mismatch: $($file.Name)"}
}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
foreach($source in @($GameRoot,$BundleRoot)){if($OutputRoot.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'OutputRoot must be outside the game and installer folders.'}}
if(Test-Path $OutputRoot){throw 'OutputRoot must be a new folder; existing files are never replaced.'}
New-Item -ItemType Directory $OutputRoot | Out-Null
$test=Join-Path $OutputRoot 'game'
$evidence=Join-Path $OutputRoot 'evidence'
New-Item -ItemType Directory $test,$evidence | Out-Null
# Copy installation assets only. Player saves, configuration and NCMM state
# remain in the source installation. All bundled modules use fresh defaults.
Get-ChildItem $GameRoot -File | Where-Object {$_.Extension -in @('.dll','.txt','.exe')} | Copy-Item -Destination $test
foreach($name in @('data','gfx','font','sound','lang')){
    $source=Join-Path $GameRoot $name
    if(Test-Path $source){Copy-Item $source (Join-Path $test $name) -Recurse}
}
Copy-Item (Join-Path $payload 'code_mods') (Join-Path $test 'code_mods') -Recurse
Copy-Item (Join-Path $payload 'host/cataclysm-tiles.ncmm.exe') (Join-Path $test 'cataclysm-tiles.ncmm.exe')
$fixture=Join-Path $PSScriptRoot 'multilevel_fixture.json'
if(-not (Test-Path $fixture)){$fixture=Join-Path $PSScriptRoot 'tests/multilevel_fixture.json'}
Copy-Item $fixture (Join-Path $test 'data/json/furniture_and_terrain/ncmm_multilevel_qualification.json')
$udp=$false
if($TilesetRoot){
    $TilesetRoot=(Resolve-Path $TilesetRoot).Path
    $identity=Get-Content (Join-Path $TilesetRoot 'tileset.txt') -Raw
    if($identity -notmatch '(?m)^NAME:\s*UndeadPeople_0J_Hybrid_v3\s*$'){throw 'TilesetRoot is not actual UndeadPeople Hybrid v3.'}
    $dest=Join-Path $test 'gfx/UndeadPeople_0J_Hybrid_v3'
    if(Test-Path $dest){Remove-Item $dest -Recurse -Force}
    Copy-Item $TilesetRoot $dest -Recurse
    $udp=$true
    $assets=@(Get-ChildItem $dest -File -Recurse | Sort-Object FullName | ForEach-Object {[ordered]@{file=$_.FullName.Substring($dest.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
    $assets | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'udp-asset-sha256.json') -Encoding UTF8
}
$names=@('NCMM_FP_GAMEPLAY_AUDIT','NCMM_FP_UDP','NCMM_FP_SOAK_SECONDS','NCMM_FP_SAVE_ONLY','NCMM_FP_APPEARANCE_SMOKE','NCMM_FP_APPEARANCE_BEFORE','NCMM_FP_MULTILEVEL_BEFORE','NCMM_FP_STABLE_CPU_BASELINE')
$prior=@{}
foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
$exitCode=$null
try {
    $env:NCMM_FP_GAMEPLAY_AUDIT='1'
    $env:NCMM_FP_APPEARANCE_SMOKE='1' # Stable monster atlas variants in this diagnostic only.
    $env:NCMM_FP_SOAK_SECONDS=[string]$SoakSeconds
    if($udp){$env:NCMM_FP_UDP='1'}
    $process=Start-Process (Join-Path $test 'cataclysm-tiles.ncmm.exe') -ArgumentList '--ncmm-gameplay-smoke --seed ncmm-gameplay-0121' -WorkingDirectory $test -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds($SoakSeconds+1200)
    while(-not $process.WaitForExit(20000)){
        Write-Host 'Native SDL audit is running; detailed evidence is written into the new output folder.'
        if([DateTime]::UtcNow -gt $deadline){& taskkill.exe /pid $process.Id /t /f;throw 'Native benchmark deadline exceeded.'}
    }
    $process.Refresh();$exitCode=$process.ExitCode
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}
    $runtime=Join-Path $test 'ncmm'
    if(Test-Path $runtime){Get-ChildItem $runtime -File | Where-Object {$_.Name -like 'first-person-*' -or $_.Extension -eq '.log'} | Copy-Item -Destination $evidence}
}
if($exitCode -ne 0){throw "Native audit failed ($exitCode). Evidence: $evidence"}
$audit=Get-Content (Join-Path $evidence 'first-person-gameplay-audit.json') -Raw | ConvertFrom-Json
if($audit.success -ne $true -or $audit.manual_playthrough -ne $false -or $audit.soak.actual_wall_seconds -lt $SoakSeconds){throw 'Native audit is incomplete.'}
$result=[ordered]@{success=$true;source_commit=$bundle.source_commit;ncmm_commit=$bundle.ncmm_source_commit;host_sha256=$bundle.host_sha256;module_sha256=$bundle.files.'code_mods/FirstPersonView/ncmm_mod.dll';manual_playthrough=$false;udp_requested=$udp;soak_seconds=$audit.soak.actual_wall_seconds;evidence=$evidence}
$result | ConvertTo-Json | Set-Content (Join-Path $OutputRoot 'benchmark-provenance.json') -Encoding UTF8
$result | ConvertTo-Json
