param(
    [string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent),
    [switch]$Check
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path
$payload=Join-Path $root 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'
if(-not(Test-Path -LiteralPath $payload -PathType Leaf)){throw 'Canonical Survivor payload missing'}
$text=[IO.File]::ReadAllText($payload,[Text.Encoding]::UTF8)
$changed=0
$paths=@(
    'sdk\ncmm_api.h',
    'host_patch\ncmm_loader.cpp',
    'host_patch\ncmm_item_glyphs.h',
    'host_patch\ncmm_loader.h',
    'host_patch\Apply-NCMMHostPatch.ps1',
    'compat\contracts.json',
    'runtime\NCMMBootstrap.cs',
    'runtime\NCMMSetupCore.cs',
    'tests\smoke_host.cpp',
    'mods\BallisticHitChance\CMakeLists.txt',
    'mods\BallisticHitChance\mod.json',
    'mods\BallisticHitChance\src\ballistic_hit_chance.cpp'
)
# Fail closed if a new embedded snapshot appears.  A silently untracked snapshot
# is especially dangerous because source and payload can then drift independently.
$embedded=@(
    [regex]::Matches($text,"Write-NcmmCanonicalPayloadFile '([^']+)' '") |
      ForEach-Object { $_.Groups[1].Value }
)
$unexpected=@(Compare-Object -ReferenceObject $paths -DifferenceObject $embedded)
if($embedded.Count -ne $paths.Count -or $unexpected.Count -ne 0){
    throw ("Canonical snapshot inventory drift: expected="+($paths -join ',')+
           "; embedded="+($embedded -join ','))
}
foreach($rel in $paths){
    $path=Join-Path $root $rel
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Source missing: $rel"}
    # ReadAllText consumes a UTF-8 BOM; the embedded byte-for-byte snapshot does not.
    # Preserve the preamble while still canonicalizing line endings below.
    $source=(New-Object Text.UTF8Encoding($false,$true)).GetString([IO.File]::ReadAllBytes($path))
    $normalized=$source.Replace(([string][char]13+[char]10),[string][char]10).Replace([string][char]13,[string][char]10)
    $expected=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($normalized))
    $prefix="Write-NcmmCanonicalPayloadFile '$rel' '"
    $start=$text.IndexOf($prefix,[StringComparison]::Ordinal)
    if($start -lt 0 -or $text.IndexOf($prefix,$start+1,[StringComparison]::Ordinal) -ge 0){
        throw "Missing or duplicate canonical snapshot: $rel"
    }
    $begin=$start+$prefix.Length
    $end=$text.IndexOf("'",$begin,[StringComparison]::Ordinal)
    if($end -lt 0){throw "Unclosed canonical snapshot: $rel"}
    $current=$text.Substring($begin,$end-$begin)
    if($current -ne $expected){
        $changed++
        if(-not $Check){
            $text=$text.Substring(0,$begin)+$expected+$text.Substring($end)
        }
        Write-Host "Canonical snapshot drift: $rel" -ForegroundColor Yellow
    }
}
if($Check){
    if($changed){throw "$changed canonical snapshot(s) out of sync. Run ci\Sync-CanonicalPayload.ps1, then ci\Regenerate-PackageIntegrity.ps1."}
    Write-Host "Canonical source snapshots: PASS ($($paths.Count)/$($paths.Count))." -ForegroundColor Green
}else{
    if($changed){
        [IO.File]::WriteAllText($payload,$text,(New-Object Text.UTF8Encoding($false)))
    }
    Write-Host "Canonical source snapshots synchronized: $changed changed (of $($paths.Count))."
}
