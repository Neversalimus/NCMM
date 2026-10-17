param([string]$Runner=(Join-Path $PSScriptRoot '../Run-FirstPersonBenchmark.ps1'))
$ErrorActionPreference='Stop'
$tokens=$null
$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Runner).Path,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Runner failed parsing.'}
$progressFunction=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Read-NativeBenchmarkProgress'},$true)
if(-not $progressFunction){throw 'Native progress reader missing.'}
. ([scriptblock]::Create($progressFunction.Extent.Text))
$blocks=@($ast.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.TryStatementAst]})
if($blocks.Count -ne 2){throw 'Expected process handling and final evidence validation.'}
$validate=[scriptblock]::Create($blocks[-1].Extent.Text)
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-evidence-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$rootFull=[IO.Path]::GetFullPath($testRoot).TrimEnd('\','/')
try {
    $progress=Join-Path $testRoot 'heartbeat[fixture].txt'
    if($null -ne (Read-NativeBenchmarkProgress $progress)){throw 'Missing heartbeat was accepted.'}
    [IO.File]::WriteAllBytes($progress,[byte[]]@())
    if($null -ne (Read-NativeBenchmarkProgress $progress)){throw 'Empty heartbeat was accepted.'}
    [IO.File]::WriteAllText($progress,"old`r`nnext frames=32`r`n",[Text.Encoding]::ASCII)
    if((Read-NativeBenchmarkProgress $progress) -ne 'next frames=32'){throw 'ASCII heartbeat lost.'}
    # Exact malformed-byte shape retained from a Russian-locale native soak.
    $localized=[byte[]]([Text.Encoding]::ASCII.GetBytes('180')+[byte]0xC2+[Text.Encoding]::ASCII.GetBytes("426 automated_soak frames=2")+[byte]0xC2+[Text.Encoding]::ASCII.GetBytes("304`r`n"))
    [IO.File]::WriteAllBytes($progress,$localized)
    $first=Read-NativeBenchmarkProgress $progress
    if(-not $first -or -not $first.Contains('automated_soak frames=2') -or -not $first.EndsWith('304')){throw 'Localized native heartbeat lost.'}
    if((Read-NativeBenchmarkProgress $progress) -ne $first){throw 'Unchanged heartbeat claimed new progress.'}
    $localized[0]=[byte][char]'2'
    [IO.File]::WriteAllBytes($progress,$localized)
    if((Read-NativeBenchmarkProgress $progress) -eq $first){throw 'Advancing localized heartbeat was ignored.'}
    $unicode='phase '+[char]0x2603
    [IO.File]::WriteAllText($progress,("old`n"+$unicode+"`n"),[Text.Encoding]::UTF8)
    if((Read-NativeBenchmarkProgress $progress) -ne $unicode){throw 'UTF8 heartbeat lost.'}
    [IO.File]::WriteAllText($progress,"old`nutf16 frames=64`n",[Text.Encoding]::Unicode)
    if((Read-NativeBenchmarkProgress $progress) -ne 'utf16 frames=64'){throw 'BOM detection failed.'}
    [IO.File]::WriteAllText($progress,"old`npartial frames=96",[Text.Encoding]::ASCII)
    if((Read-NativeBenchmarkProgress $progress) -ne 'partial frames=96'){throw 'Final unterminated heartbeat lost.'}
    $writer=[IO.File]::Open($progress,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
    try {
        foreach($tick in @(1,2)){
            $bytes=[Text.Encoding]::ASCII.GetBytes("live frames=$tick`n")
            $writer.Write($bytes,0,$bytes.Length);$writer.Flush()
            if((Read-NativeBenchmarkProgress $progress) -ne "live frames=$tick"){throw 'Live writer heartbeat was not readable.'}
        }
    } finally {$writer.Dispose()}
    foreach($case in @('missing','malformed','incomplete','dump','valid')){
        $OutputRoot=Join-Path $testRoot $case
        $evidence=Join-Path $OutputRoot 'evidence'
        New-Item -ItemType Directory -Path $evidence | Out-Null
        $exitCode=0
        $debugger=$null
        $lastProgress='native startup'
        $MovementBenchmark=$false
        $FocusedGpuBenchmark=$true
        $udp=$false
        $bundle=@{source_commit='engine';ncmm_source_commit='runtime';host_sha256='host';files=@{'code_mods/FirstPersonView/ncmm_mod.dll'='module'}}
        $receiptPath=Join-Path $evidence 'first-person-process-result.json'
        @{exit_signed=0;wrapper_failure=$null} | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8
        $auditPath=Join-Path $evidence 'first-person-gpu-benchmark.json'
        if($case -eq 'malformed'){'{"bad":' | Set-Content -LiteralPath $auditPath -Encoding UTF8}
        elseif($case -ne 'missing'){
            $records=if($case -eq 'incomplete'){@(1,2,3)}else{@(1,2,3,4)}
            @{actual_cdda_sdl=$true;focused_reproduction=$true;records=$records} | ConvertTo-Json | Set-Content -LiteralPath $auditPath -Encoding UTF8
        }
        if($case -eq 'dump'){$debugger=[PSCustomObject]@{Error=$null;Dumps=1}}
        $failed=$false
        try {& $validate | Out-Null} catch {$failed=$true}
        $receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
        if($case -eq 'valid'){
            if($failed -or $receipt.wrapper_failure -or -not (Test-Path -LiteralPath (Join-Path $OutputRoot 'benchmark-provenance.json'))){throw 'Valid focused evidence was rejected.'}
        }else{
            if(-not $failed -or -not $receipt.wrapper_failure){throw "Failed qualification was not retained: $case"}
            if(Test-Path -LiteralPath (Join-Path $OutputRoot 'benchmark-provenance.json')){throw "False successful provenance: $case"}
        }
    }
} finally {
    # This is the newly created fixture root, never a caller-supplied game path.
    $resolved=(Resolve-Path -LiteralPath $testRoot).Path.TrimEnd('\','/')
    if($resolved -ne $rootFull -or [IO.Path]::GetFileName($resolved) -notlike 'ncmm-evidence-*'){throw 'Unexpected fixture cleanup path.'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();cases=5;progress_checks=10} | ConvertTo-Json -Compress
