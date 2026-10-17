param([string]$Runner=(Join-Path $PSScriptRoot '../Run-FirstPersonBenchmark.ps1'))
$ErrorActionPreference='Stop'
# Exercise the runner's real isolation/restoration statements without launching
# a game. In PowerShell 7.5+ an empty variable is distinct from an absent one.
$tokens=$null
$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $Runner).Path,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'Runner failed parsing.'}
$assignment=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$names'},$true))
$loops=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.ForEachStatementAst] -and $n.Condition.Extent.Text -eq '$names'},$true))
if($assignment.Count -ne 1 -or $loops.Count -ne 2){throw 'Expected exactly one controlled environment list and two isolation/restoration loops.'}
$names=& ([scriptblock]::Create($assignment[0].Right.Extent.Text))
$clear=[scriptblock]::Create($loops[0].Extent.Text)
$restore=[scriptblock]::Create($loops[1].Extent.Text)
$original=@{}
foreach($name in $names){$original[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$checks=0
try {
    foreach($scenario in @('absent','present','empty')){
        foreach($name in $names){Remove-Item -LiteralPath ('Env:'+ $name) -ErrorAction SilentlyContinue}
        if($scenario -eq 'present'){foreach($name in $names){[Environment]::SetEnvironmentVariable($name,'sentinel-'+$name,'Process')}}
        if($scenario -eq 'empty'){foreach($name in $names){[Environment]::SetEnvironmentVariable($name,'','Process')}}
        $expected=@{}
        foreach($name in $names){$expected[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
        $prior=@{}
        & $clear
        foreach($name in $names){
            if(Test-Path -LiteralPath ('Env:'+ $name)){throw "Competing native getenv flag survives: $name ($scenario)"}
            $checks++
        }
        $env:NCMM_FP_GPU_BENCHMARK_ONLY='1'
        $env:SDL_ASSERT='abort'
        try {throw 'Simulated diagnostic failure'} catch {} finally {& $restore}
        foreach($name in $names){
            $actual=[Environment]::GetEnvironmentVariable($name,'Process')
            if(($null -eq $actual) -ne ($null -eq $expected[$name]) -or $actual -cne $expected[$name]){
                throw "Caller environment changed: $name ($scenario)"
            }
            $checks++
        }
    }
} finally {
    foreach($name in $names){
        if($null -eq $original[$name]){Remove-Item -LiteralPath ('Env:'+ $name) -ErrorAction SilentlyContinue}
        else{[Environment]::SetEnvironmentVariable($name,$original[$name],'Process')}
    }
}
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();checks=$checks} | ConvertTo-Json -Compress
