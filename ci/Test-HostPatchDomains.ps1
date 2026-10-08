param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
. (Join-Path $RepositoryRoot 'ci\HostPatchDomains.ps1')
$source=Get-Content (Join-Path $RepositoryRoot 'ci\host-patch-domains.json') -Raw
$baseline=Assert-NcmmHostPatchDomains -RepositoryRoot $RepositoryRoot
if($baseline.Domains -ne 9 -or $baseline.Layers -ne 40) {
    throw "Unexpected baseline Host patch domain coverage: $($baseline.Domains)/$($baseline.Layers)"
}
$fixtures=@(
    @{Label='missing layer';Change={param($m) $m.domains[0].layers=@($m.domains[0].layers|Select-Object -Skip 1)}},
    @{Label='duplicate layer';Change={param($m) $m.domains[1].layers[0]=$m.domains[0].layers[0]}},
    @{Label='unknown layer';Change={param($m) $m.domains[1].layers[0]='Apply-UnknownLayer0123'}},
    @{Label='swapped layer order';Change={param($m) $t=$m.domains[0].layers[0];$m.domains[0].layers[0]=$m.domains[0].layers[1];$m.domains[0].layers[1]=$t}},
    @{Label='duplicate domain';Change={param($m) $m.domains[1].id=$m.domains[0].id}},
    @{Label='missing dependency';Change={param($m) $m.domains[1].after=@('not-a-domain')}},
    @{Label='forward dependency';Change={param($m) $m.domains[0].after=@('runtime-hooks')}},
    @{Label='self dependency';Change={param($m) $m.domains[1].after=@('runtime-hooks')}},
    @{Label='duplicate dependency';Change={param($m) $m.domains[1].after=@('worldgen-settings','worldgen-settings')}},
    @{Label='missing last domain';Change={param($m) $m.domains=@($m.domains|Select-Object -SkipLast 1)}},
    @{Label='invalid source';Change={param($m) $m.source='unexpected/stack.json'}},
    @{Label='invalid schema';Change={param($m) $m.schema=2}}
)
foreach($fixture in $fixtures) {
    $copy=ConvertFrom-Json $source
    & $fixture.Change $copy
    $rejected=$false
    try { $null=Assert-NcmmHostPatchDomains -RepositoryRoot $RepositoryRoot -Manifest $copy }
    catch { $rejected=$true }
    if(-not $rejected) { throw "Invalid Host patch domain fixture accepted: $($fixture.Label)" }
}
Write-Host ("NCMM Host patch domain graph: PASS ({0} domains, {1} layers, {2} mutation fixtures)." -f $baseline.Domains,$baseline.Layers,$fixtures.Count) -ForegroundColor Green
