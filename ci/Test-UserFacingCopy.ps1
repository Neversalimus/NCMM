param(
    [string]$SourceRoot = (Split-Path $PSScriptRoot -Parent)
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $SourceRoot).Path

$targets = @{
    Survivor = Join-Path $root 'mods\SurvivorProgression\src\survivor_progression.cpp'
    AWS      = Join-Path $root 'mods\AdvancedWorldSettings\src\aws.cpp'
    Host     = Join-Path $root 'host_patch\ncmm_loader.cpp'
}

foreach( $name in $targets.Keys ) {
    if( -not ( Test-Path $targets[$name] -PathType Leaf ) ) {
        throw "User-facing copy audit target missing: $name -> $($targets[$name])"
    }
}

$survivor = [IO.File]::ReadAllText( $targets.Survivor )
$aws = [IO.File]::ReadAllText( $targets.AWS )
$hostText = [IO.File]::ReadAllText( $targets.Host )

$checks = @(
    @{ Name='Survivor dev label Mechanical'; Text=$survivor; Pattern='Mechanical:' },
    @{ Name='Survivor dev label Technical'; Text=$survivor; Pattern='Technical:' },
    @{ Name='Survivor dev label Fieldcraft'; Text=$survivor; Pattern='Fieldcraft' },
    @{ Name='Survivor design taxonomy Cross-discipline'; Text=$survivor; Pattern='Cross-discipline' },
    @{ Name='Survivor engine wording damage packet'; Text=$survivor; Pattern='damage packet' },
    @{ Name='Survivor engine wording damage pipeline'; Text=$survivor; Pattern='damage pipeline' },
    @{ Name='Survivor engine wording failure event'; Text=$survivor; Pattern='failure event' },
    @{ Name='Survivor developer roll wording'; Text=$survivor; Pattern='actual [A-Za-z -]*roll' },
    @{ Name='Survivor developer Russian roll wording'; Text=$survivor; Pattern='\u0440\u0435\u0430\u043b\u044c\u043d[^"\r\n]*\u0431\u0440\u043e\u0441' },
    @{ Name='Survivor XP implementation wording'; Text=$survivor; Pattern='XP-awarding' },
    @{ Name='Survivor layout implementation wording'; Text=$survivor; Pattern='Routed tree' },
    @{ Name='Survivor state implementation wording'; Text=$survivor; Pattern='complete Survivor state' },
    @{ Name='Survivor integration-anchor wording'; Text=$survivor; Pattern='integration anchor' },
    @{ Name='Survivor sourced-ability wording'; Text=$survivor; Pattern='Prime-sourced' },
    @{ Name='Survivor stat-perk implementation wording'; Text=$survivor; Pattern='stat perks' },
    @{ Name='Survivor legacy compact branch-level token'; Text=$survivor; Pattern='BLv' },
    @{ Name='Survivor legacy Prime design label'; Text=$survivor; Pattern='PRIME TRADEOFF' },
    @{ Name='Survivor Russian translocation mistranslation'; Text=$survivor; Pattern='\u0442\u0440\u0430\u043d\u0441\u043b\u044f\u0446\u0438' },
    @{ Name='Survivor old world-mod wording'; Text=$survivor; Pattern='World mod:' },
    @{ Name='Survivor generic-bonus implementation wording'; Text=$survivor; Pattern='generic Survivor' },

    @{ Name='Survivor broken Russian percent grammar - speed'; Text=$survivor; Pattern='[+-][0-9]+(?:[.,][0-9]+)?% \\u0441\\u043a\\u043e\\u0440\\u043e\\u0441\\u0442(?:[,;." ]|$)' },
    @{ Name='Survivor broken Russian percent grammar - stamina'; Text=$survivor; Pattern='[+-][0-9]+(?:[.,][0-9]+)?% \\u0432\\u044b\\u043d\\u043e\\u0441\\u043b\\u0438\\u0432\\u043e\\u0441\\u0442\\u044c(?:[,;." ]|$)' },
    @{ Name='Survivor broken Russian percent grammar - crafting'; Text=$survivor; Pattern='[+-][0-9]+(?:[.,][0-9]+)?% \\u043a\\u0440\\u0430\\u0444\\u0442(?:[,;." ]|$)' },
    @{ Name='Survivor broken Russian percent grammar - reading'; Text=$survivor; Pattern='[+-][0-9]+(?:[.,][0-9]+)?% \\u0447\\u0442\\u0435\\u043d\\u0438\\u0435(?:[,;." ]|$)' },
    @{ Name='AWS internal new-map marker'; Text=$aws; Pattern='\[NEW MAP\]' },
    @{ Name='AWS implementation region wording'; Text=$aws; Pattern='default-region overmaps' },
    @{ Name='AWS statistical implementation name'; Text=$aws; Pattern='distribution sigma' },
    @{ Name='AWS formula implementation name'; Text=$aws; Pattern='frequency divisor' },
    @{ Name='AWS internal urbanity name'; Text=$aws; Pattern='urbanity multiplier' },

    @{ Name='Host raw callback quarantine popup'; Text=$hostText; Pattern='callback failed and was quarantined' },
    @{ Name='Host raw quarantine state'; Text=$hostText; Pattern='ON / quarantined' },
    @{ Name='Host raw failed state'; Text=$hostText; Pattern='ON / failed' },
    @{ Name='Host raw internal reason in player label'; Text=$hostText; Pattern='label \+= " - " \+ entry\.reason' }
)

$failures = New-Object System.Collections.Generic.List[string]
foreach( $check in $checks ) {
    if( [regex]::IsMatch( [string]$check.Text, [string]$check.Pattern,
            [Text.RegularExpressions.RegexOptions]::IgnoreCase ) ) {
        $failures.Add( [string]$check.Name )
    }
}

$required = @(
    @{ Name='Survivor human overview'; Text=$survivor; Needle='View your level, points, branch progress and active bonuses.' },
    @{ Name='Survivor human Prime confirmation'; Text=$survivor; Needle='Choose this Prime specialization?' },
    @{ Name='Survivor explicit drawback label'; Text=$survivor; Needle='DRAWBACK:' },
    @{ Name='Survivor natural mod requirement'; Text=$survivor; Needle='Requires mod: ' },
    @{ Name='AWS natural new-area warning'; Text=$aws; Needle='Affects only areas generated after this change.' },
    @{ Name='Host natural UI failure message'; Text=$hostText; Needle="This mod's interface failed to open and has been disabled for this session." },
    @{ Name='Host natural settings badge'; Text=$hostText; Needle=' [SETTINGS]' }
)

foreach( $check in $required ) {
    if( -not ( [string]$check.Text ).Contains( [string]$check.Needle ) ) {
        $failures.Add( 'Missing expected player copy: ' + [string]$check.Name )
    }
}

if( $failures.Count -gt 0 ) {
    throw ( 'User-facing copy audit failed:' + [Environment]::NewLine + ' - ' +
            ( $failures -join ( [Environment]::NewLine + ' - ' ) ) )
}

Write-Host 'User-facing copy audit: PASS' -ForegroundColor Green
