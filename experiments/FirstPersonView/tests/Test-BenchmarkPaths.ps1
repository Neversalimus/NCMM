param([string]$Runner=(Join-Path $PSScriptRoot '../Run-FirstPersonBenchmark.ps1'))
$ErrorActionPreference='Stop'
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Runner).Path,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Runner failed parsing.'}
$function=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Resolve-NativeBenchmarkRoot'},$true)
if(-not $function){throw 'Native path preflight missing.'}
. ([scriptblock]::Create($function.Extent.Text))
$volume=[IO.Path]::GetPathRoot([IO.Path]::GetFullPath([IO.Path]::GetTempPath()))
$game=Join-Path $volume 'ncmm-source-game'
$bundle=Join-Path $volume 'ncmm-source-bundle'
$checks=0
foreach($length in @(79,80,81,150)){
    $path=Join-Path $volume ('a'*($length-$volume.Length))
    $failed=$false
    try{$result=Resolve-NativeBenchmarkRoot $path @($game,$bundle)}catch{$failed=$true}
    if($failed -ne ($length -gt 80)){throw "Wrong diagnostic save-path budget: $length"}
    if(-not $failed -and $result.Length -ne $length){throw 'Path was silently relocated.'}
    $checks++
}
foreach($path in @($game,(Join-Path $game 'inside'),$bundle,(Join-Path $bundle 'inside'))){
    $failed=$false
    try{Resolve-NativeBenchmarkRoot $path @($game,$bundle) | Out-Null}catch{$failed=$true}
    if(-not $failed){throw 'Diagnostic root was accepted inside source assets.'}
    $checks++
}
$sibling=Resolve-NativeBenchmarkRoot ($game+'-diagnostic') @($game,$bundle)
if($sibling -ne $game+'-diagnostic'){throw 'Safe sibling was rejected or relocated.'}
$checks++
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();checks=$checks} | ConvertTo-Json -Compress
