param([string]$Runner=(Join-Path $PSScriptRoot '../Run-FirstPersonBenchmark.ps1'))
$ErrorActionPreference='Stop'
$tokens=$null
$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Runner).Path,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Runner failed parsing.'}
$blocks=@($ast.EndBlock.Statements | Where-Object {$_ -is [Management.Automation.Language.TryStatementAst]})
if($blocks.Count -ne 2){throw 'Expected process handling and final evidence validation.'}
$validate=[scriptblock]::Create($blocks[-1].Extent.Text)
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-evidence-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$rootFull=[IO.Path]::GetFullPath($testRoot).TrimEnd('\','/')
try {
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
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();cases=5} | ConvertTo-Json -Compress
