param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$PayloadRoot,
    [ValidateSet('Synthetic','Real')][string]$Mode='Synthetic',
    [ValidateSet('Install','PostRuntime')][string]$RealPhase='Install',
    [string]$GameRoot=''
)
$ErrorActionPreference='Stop'

$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
$PayloadRoot=(Resolve-Path $PayloadRoot).Path
if($Mode -eq 'Real'){
    if([string]::IsNullOrWhiteSpace($GameRoot)){throw 'Real installation matrix mode requires -GameRoot.'}
    $GameRoot=(Resolve-Path $GameRoot).Path
}

$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if(-not(Test-Path $csc -PathType Leaf)){throw "Framework csc.exe not found: $csc"}

$work=Join-Path $env:TEMP ('ncmm-install-matrix-runner-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $work|Out-Null
$exe=Join-Path $work 'NCMM_InstallationMatrix_Harness.exe'
try{
    & $csc /nologo /target:exe /optimize+ /platform:x64 /main:InstallationMatrixHarness `
        /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll `
        /out:$exe `
        (Join-Path $RepositoryRoot 'runtime\NCMMRuntimeIO.cs') `
        (Join-Path $RepositoryRoot 'runtime\NCMMSetupCore.cs') `
        (Join-Path $RepositoryRoot 'tests\InstallationMatrixHarness.cs')
    if($LASTEXITCODE -ne 0 -or -not(Test-Path $exe -PathType Leaf)){
        throw 'NCMM installation matrix harness compilation failed.'
    }

    if($Mode -eq 'Synthetic'){
        & $exe $PayloadRoot
    } elseif($RealPhase -eq 'Install') {
        & $exe --real-install-smoke $GameRoot $PayloadRoot
    } else {
        & $exe --real-runtime-verify $GameRoot
    }
    if($LASTEXITCODE -ne 0){
        throw "NCMM installation matrix failed in $Mode mode with exit $LASTEXITCODE."
    }
} finally {
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}
if($Mode -eq 'Real'){
    Write-Host ("NCMM Installation Matrix Real/"+$RealPhase+": PASS") -ForegroundColor Green
} else {
    Write-Host "NCMM Installation Matrix Synthetic: PASS" -ForegroundColor Green
}
