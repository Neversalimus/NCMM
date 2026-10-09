param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$runtimeManifest=Join-Path $RepositoryRoot 'runtime\NCMMRuntime.manifest'
if(-not(Test-Path $runtimeManifest -PathType Leaf)){throw 'NCMM runtime manifest missing.'}
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$out=Join-Path $env:TEMP ('ncmm-audit-io-'+[guid]::NewGuid().ToString('N')+'.exe')
try {
    & $csc /nologo /win32manifest:$runtimeManifest /target:exe /platform:x64 /optimize+ /main:AuditIOHarness /reference:System.Web.Extensions.dll /out:$out `
        (Join-Path $RepositoryRoot 'runtime\NCMMRuntimeIO.cs') `
        (Join-Path $RepositoryRoot 'runtime\NCMMBootstrap.cs') `
        (Join-Path $RepositoryRoot 'tests\AuditIOHarness.cs')
    if($LASTEXITCODE -ne 0){throw 'Audit I/O harness compilation failed.'}
    & $out
    if($LASTEXITCODE -ne 0){throw 'Audit I/O regressions failed.'}
} finally {Remove-Item $out -Force -ErrorAction SilentlyContinue}
