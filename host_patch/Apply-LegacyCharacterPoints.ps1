param([Parameter(Mandatory=$true)][string]$SourceRoot)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $SourceRoot).Path
$utf8=New-Object Text.UTF8Encoding($false,$true)
$spec=ConvertFrom-Json ([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'legacy-character-points.patch.json'),$utf8))
if([int]$spec.schema -ne 1 -or @($spec.operations).Count -eq 0){throw 'Invalid legacy chargen patch schema.'}
$texts=@{}; $originals=@{}
foreach($rel in @('src/newcharacter.cpp','src/player_difficulty.h')) {
    $p=Join-Path $root $rel
    if(-not(Test-Path -LiteralPath $p -PathType Leaf)){throw "Missing chargen source: $rel"}
    $texts[$rel]=[IO.File]::ReadAllText($p,$utf8).Replace("`r`n","`n").Replace("`r","`n")
    $originals[$rel]=$texts[$rel]
}
$already=$texts['src/newcharacter.cpp'].Contains('#include "ncmm_legacy_chargen.h"')
foreach($op in $spec.operations) {
    $rel=[string]$op.file
    if(-not $texts.ContainsKey($rel) -or [int]$op.count -lt 1 -or
       [string]::IsNullOrEmpty([string]$op.before) -or [string]::IsNullOrEmpty([string]$op.after)) {
        throw 'Invalid chargen patch operation.'
    }
    if(-not $already) {
        $matches=[regex]::Matches($texts[$rel],[regex]::Escape([string]$op.before)).Count
        if($matches -ne [int]$op.count) {
            throw "Legacy chargen '$($op.name)' anchor mismatch ($matches vs $($op.count)); source not modified."
        }
        $texts[$rel]=$texts[$rel].Replace([string]$op.before,[string]$op.after)
    }
}
# Validate every transformed block even on a repeated application. No marker-only success.
foreach($op in $spec.operations) {
    $matches=[regex]::Matches($texts[[string]$op.file],[regex]::Escape([string]$op.after)).Count
    if($matches -ne [int]$op.count){throw "Legacy chargen '$($op.name)' postcondition failed; source not modified."}
}
foreach($name in @('ncmm_character_points.hpp','ncmm_legacy_chargen.h')) {
    if(-not(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) {throw "Missing chargen implementation: $name"}
}
foreach($rel in $texts.Keys) {
    if($texts[$rel] -cne $originals[$rel]) {[IO.File]::WriteAllText((Join-Path $root $rel),$texts[$rel],$utf8)}
}
foreach($name in @('ncmm_character_points.hpp','ncmm_legacy_chargen.h')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $root ('src/'+$name)) -Force
}
Write-Host ('Legacy Character Points engine bridge: PASS ('+@($spec.operations).Count+' exact contracts; already='+$already+').')
