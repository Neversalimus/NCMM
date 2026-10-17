param([string]$Runner=(Join-Path $PSScriptRoot '../Run-FirstPersonBenchmark.ps1'))
$ErrorActionPreference='Stop'
if($env:OS -ne 'Windows_NT'){
    Write-Host 'Window-capture compilation applies only to the Windows native runner.'
    return
}
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Runner).Path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Runner failed parsing.'}
$assignment=$ast.Find({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$drawingReferences'},$true)
$loop=$ast.Find({param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Variable.Extent.Text -eq '$drawingAssembly'},$true)
$compile=$ast.Find({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'Add-Type' -and $n.Extent.Text.Contains('public static class NCMMBenchmarkWindow')},$true)
if(-not $assignment -or -not $loop -or -not $compile){throw 'Actual native capture compilation statements missing.'}
. ([scriptblock]::Create($assignment.Extent.Text))
. ([scriptblock]::Create($loop.Extent.Text))
# Compile the actual stalled-window helper without reading or capturing a window.
. ([scriptblock]::Create($compile.Extent.Text))
if(-not ('NCMMBenchmarkWindow' -as [type])){throw 'Native capture helper did not compile.'}
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();checks=1;captures=0} | ConvertTo-Json -Compress
