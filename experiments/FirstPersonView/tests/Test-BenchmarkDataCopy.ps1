param([string]$Runner=(Join-Path $PSScriptRoot '../Run-FirstPersonBenchmark.ps1'))
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Runner).Path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Runner failed parsing.'}
$function=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Copy-NativeBenchmarkData'},$true)
if(-not $function){throw 'Native data-copy boundary missing.'}
. ([scriptblock]::Create($function.Extent.Text))
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-data-copy-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$rootFull=[IO.Path]::GetFullPath($testRoot).TrimEnd('\','/')
try{
    $source=Join-Path $testRoot 'source[fixture]'
    $destination=Join-Path $testRoot 'new-data'
    foreach($directory in @('json/nested','mods/example','cache/json/nested')){New-Item -ItemType Directory -Path (Join-Path $source $directory) -Force | Out-Null}
    [IO.File]::WriteAllText((Join-Path $source 'json/nested/scene.json'),'exact scene')
    [IO.File]::WriteAllText((Join-Path $source 'mods/example/modinfo.json'),'exact mod')
    [IO.File]::WriteAllText((Join-Path $source 'fontdata.json'),'exact font')
    [IO.File]::WriteAllText((Join-Path $source 'cache/json/nested/scene.json.stale.fb'),'stale derived cache')
    $before=@(Get-ChildItem -LiteralPath $source -Recurse -File | Sort-Object FullName | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName).Hash}) -join ','
    Copy-NativeBenchmarkData $source $destination
    if(Test-Path -LiteralPath (Join-Path $destination 'cache')){throw 'Stale source flexbuffers entered the isolated game.'}
    foreach($relative in @('json/nested/scene.json','mods/example/modinfo.json','fontdata.json')){
        if((Get-FileHash -LiteralPath (Join-Path $source $relative)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $destination $relative)).Hash){throw "Installation asset changed: $relative"}
    }
    $after=@(Get-ChildItem -LiteralPath $source -Recurse -File | Sort-Object FullName | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName).Hash}) -join ','
    if($before -cne $after){throw 'Source installation cache or assets changed.'}
}finally{
    $resolved=(Resolve-Path -LiteralPath $testRoot).Path.TrimEnd('\','/')
    if($resolved -ne $rootFull -or [IO.Path]::GetFileName($resolved) -notlike 'ncmm-data-copy-*'){throw 'Unexpected fixture cleanup path.'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();checks=5} | ConvertTo-Json -Compress
