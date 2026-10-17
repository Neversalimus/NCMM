param(
    [ValidateSet('1040','1807')][string]$Build='1807',
    [ValidateRange(1,20)][int]$Repeats=8,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
if(Test-Path -LiteralPath $OutputRoot){throw 'Reproduction requires a new output folder.'}
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$pins=@{
    '1040'=@{id=12085194422;outer='5b8b01129a9799cc1da9bb27f2cc66eb31697e11ac7895a8fc984d94976584af';inner='a4133d2329b1fe257e82d3021335349054b2544c547c9d0cc3a4f04766d2866f';engine='3f7fb352bf492ba521bd9408a0c9f6ce239e8d83';tag='cdda-experimental-2026-10-01-1040'}
    '1807'=@{id=12086902922;outer='3f758e97ce67176a3f504808156125fea9988bb2ce09426b6ec65d60d544c9c0';inner='268448d4ba69ab5b35d9358add1051b043685793c68a6602f2e28b092c3cf51d';engine='074aa98bd5be3de4c35f154082db32a0e63bb0f1';tag='cdda-experimental-2026-10-06-1807'}
}
$pin=$pins[$Build]
$outer=Join-Path $OutputRoot 'qualified-artifact.zip'
Invoke-WebRequest -UseBasicParsing -Headers @{Authorization="Bearer $env:GH_TOKEN";Accept='application/vnd.github+json'} -Uri ("https://api.github.com/repos/Neversalimus/NCMM/actions/artifacts/"+$pin.id+'/zip') -OutFile $outer
if((Get-FileHash -LiteralPath $outer -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.outer){throw 'Qualified artifact hash mismatch.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead($outer)
$inner=Join-Path $OutputRoot 'qualified-installer.zip'
try {
    $name="NCMM_FirstPersonView_0.12.7_TEST_$($Build)_Windows_x64.zip"
    $entry=@($zip.Entries | Where-Object {[IO.Path]::GetFileName($_.FullName) -eq $name})
    if($entry.Count -ne 1){throw 'Qualified installer missing or ambiguous.'}
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry[0],$inner)
} finally {$zip.Dispose()}
if((Get-FileHash -LiteralPath $inner -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.inner){throw 'Qualified installer hash mismatch.'}
$bundleRoot=Join-Path $OutputRoot 'bundle'
Expand-Archive -LiteralPath $inner -DestinationPath $bundleRoot
$manifests=@(Get-ChildItem -LiteralPath $bundleRoot -Filter first-person-bundle.json -File -Recurse)
if($manifests.Count -ne 1){throw 'Expected exactly one qualified bundle.'}
$bundleRoot=$manifests[0].Directory.Parent.FullName
$manifest=Get-Content -LiteralPath $manifests[0].FullName -Raw | ConvertFrom-Json
if($manifest.source_commit -ne $pin.engine -or $manifest.ncmm_source_commit -ne '4ea4e39e0d6288e0ab8ca4cf62f19c36f0ff4386'){throw 'Qualified runtime source mismatch.'}
$release=(& gh api ("repos/CleverRaven/Cataclysm-DDA/releases/tags/"+$pin.tag)) | ConvertFrom-Json
if($LASTEXITCODE -ne 0){throw 'Official asset lookup failed.'}
$assetName='cdda-windows-with-graphics-x64-'+$pin.tag.Substring('cdda-experimental-'.Length)+'.zip'
$assets=@($release.assets | Where-Object {$_.name -eq $assetName})
if($assets.Count -ne 1 -or [string]$assets[0].digest -notmatch '^sha256:[0-9a-f]{64}$'){throw 'Official archive identity missing.'}
& gh release download $pin.tag -R CleverRaven/Cataclysm-DDA -p $assetName -D $OutputRoot
if($LASTEXITCODE -ne 0){throw 'Official archive download failed.'}
$official=Join-Path $OutputRoot $assetName
if(('sha256:'+(Get-FileHash -LiteralPath $official -Algorithm SHA256).Hash.ToLowerInvariant()) -ne $assets[0].digest){throw 'Official archive digest mismatch.'}
$assetsRoot=Join-Path $OutputRoot 'official'
New-Item -ItemType Directory -Path $assetsRoot | Out-Null
& tar.exe -xf $official -C $assetsRoot
if($LASTEXITCODE -ne 0){throw 'Official assets extraction failed.'}
$exes=@(Get-ChildItem -LiteralPath $assetsRoot -Filter cataclysm-tiles.exe -Recurse -File)
if($exes.Count -ne 1){throw 'Expected exactly one official installation.'}
$results=@()
for($i=1;$i -le $Repeats;$i++){
    $out=Join-Path $OutputRoot ('repeat-'+$i)
    try {
        & (Join-Path $PSScriptRoot 'Run-FirstPersonBenchmark.ps1') -GameRoot $exes[0].Directory.FullName -BundleRoot $bundleRoot -OutputRoot $out -FocusedGpuBenchmark -CaptureCrashDump
        & python (Join-Path $PSScriptRoot 'tests/verify_gpu_profile.py') --input (Join-Path $out 'evidence/first-person-gpu-benchmark.json') --output (Join-Path $out 'evidence/first-person-focused-verified.json')
        if($LASTEXITCODE -ne 0){throw 'Focused profile verification failed.'}
        $results+=@{repeat=$i;success=$true;engine=$pin.engine;installer_sha256=$pin.inner}
    } catch {
        $results+=@{repeat=$i;success=$false;engine=$pin.engine;installer_sha256=$pin.inner;error=$_.Exception.Message}
        $results | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputRoot 'results.json') -Encoding UTF8
        throw
    }
    $results | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutputRoot 'results.json') -Encoding UTF8
}
