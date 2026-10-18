param([string]$Wrapper=(Join-Path $PSScriptRoot '../tools/compile_cached.cmd'))
$ErrorActionPreference='Stop'
if($env:OS -ne 'Windows_NT'){return}
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if(-not(Test-Path -LiteralPath $csc)){throw 'The Windows compiler-wrapper test requires the standard .NET Framework compiler'}
$scratch=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-fp-wrapper-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch,(Join-Path $scratch 'logs') | Out-Null
@'
using System;
using System.IO;
public static class MockCompiler {
    public static int Main() {
        bool cache=Path.GetFileNameWithoutExtension(Environment.GetCommandLineArgs()[0])=="sccache";
        Console.WriteLine(cache?"MOCK_CACHE":"MOCK_DIRECT_DIAGNOSTIC");
        return int.Parse(Environment.GetEnvironmentVariable(cache?"NCMM_TEST_CACHE_EXIT":"NCMM_TEST_COMPILER_EXIT"));
    }
}
'@ | Set-Content -LiteralPath (Join-Path $scratch 'mock.cs') -Encoding UTF8
& $csc /nologo ('/out:'+(Join-Path $scratch 'sccache.exe')) (Join-Path $scratch 'mock.cs')
if($LASTEXITCODE -ne 0){throw 'Mock compiler build failed'}
Copy-Item -LiteralPath (Join-Path $scratch 'sccache.exe') -Destination (Join-Path $scratch 'cl.exe')
Copy-Item -LiteralPath $Wrapper -Destination (Join-Path $scratch 'cl.bat')
$prior=@{}
foreach($name in @('PATH','NCMM_SCCACHE_WRAPPER_DIR','NCMM_TEST_CACHE_EXIT','NCMM_TEST_COMPILER_EXIT')){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $env:PATH=$scratch+';'+$env:PATH;$env:NCMM_SCCACHE_WRAPPER_DIR=$scratch
    Push-Location -LiteralPath $scratch
    try {
        foreach($case in @(@{cache=0;direct=17;expected=0;retry=$false},@{cache=13;direct=0;expected=0;retry=$true},@{cache=13;direct=17;expected=17;retry=$true})){
            $env:NCMM_TEST_CACHE_EXIT=[string]$case.cache;$env:NCMM_TEST_COMPILER_EXIT=[string]$case.direct
            $output=(& $env:ComSpec /d /c '.\cl.bat /c fixture.cpp' 2>&1 | Out-String);$code=$LASTEXITCODE
            if($code -ne $case.expected -or $output.Contains('MOCK_DIRECT_DIAGNOSTIC') -ne $case.retry){throw "Compiler wrapper changed the exit/diagnostic contract: $code"}
        }
    } finally {Pop-Location}
    $failures=@(Get-ChildItem -LiteralPath (Join-Path $scratch 'logs') -Filter '*.failed.log' -File)
    if($failures.Count -ne 2 -or @($failures | Where-Object {(Get-Content -LiteralPath $_.FullName -Raw) -notmatch 'MOCK_DIRECT_DIAGNOSTIC'}).Count){throw 'Failed compiler diagnostics were not retained'}
} finally {
    foreach($name in $prior.Keys){if($null -eq $prior[$name]){Remove-Item -LiteralPath ('Env:'+$name) -ErrorAction SilentlyContinue}else{[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}}
}
Write-Host 'Cached/direct compiler success, failure propagation and retained diagnostics: PASS'
