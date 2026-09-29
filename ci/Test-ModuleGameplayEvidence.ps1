param(
    [Parameter(Mandatory=$true)][string]$EvidenceDirectory,
    [int]$ExpectedWorlds = 3
)
$ErrorActionPreference = 'Stop'

$dir = (Resolve-Path $EvidenceDirectory).Path
$files = @(Get-ChildItem $dir -Filter 'module-gameplay-smoke-*.json' -File | Sort-Object Name)
if ($files.Count -ne $ExpectedWorlds) {
    throw "Expected $ExpectedWorlds module gameplay evidence files, found $($files.Count)."
}

$seenSeeds = @{}
$seenWorlds = @{}
$totalEffectAssertions = 0
$totalIntegrationInert = 0

foreach ($file in $files) {
    try {
        $proof = Get-Content $file.FullName -Raw | ConvertFrom-Json
    } catch {
        throw "Gameplay evidence is not valid JSON: $($file.FullName) :: $($_.Exception.Message)"
    }

    if ([int]$proof.schema -ne 1) { throw "Gameplay evidence schema mismatch: $($file.Name)" }
    if ([int]$proof.aws_settings -ne 48) { throw "AWS setting coverage mismatch: $($file.Name)" }
    if ($proof.aws_world_saved -ne $true) { throw "AWS world was not saved: $($file.Name)" }
    if ($proof.aws_world_reloaded -ne $true) { throw "AWS world was not reloaded: $($file.Name)" }
    if ($proof.aws_overmap_generated -ne $true) { throw "AWS overmap was not generated: $($file.Name)" }

    if ([int]$proof.survivor_catalog -ne 369) { throw "Survivor catalog coverage mismatch: $($file.Name)" }
    if ([int]$proof.survivor_perks_exercised -ne 369) { throw "Survivor full-catalog gameplay coverage mismatch: $($file.Name)" }
    if ([int]$proof.survivor_effect_assertions -le 0) { throw "Survivor real Host effect assertions missing: $($file.Name)" }
    if ([int]$proof.survivor_integrations_inert -le 0) { throw "Survivor conditional integration coverage missing: $($file.Name)" }
    if ([int]$proof.survivor_real_character_checks -ne 9) { throw "Survivor real Character coverage mismatch: $($file.Name)" }
    if ($proof.survivor_cleanup -ne $true) { throw "Survivor cleanup failed: $($file.Name)" }

    $seed = [string]$proof.seed
    if ([string]::IsNullOrWhiteSpace($seed)) { throw "Gameplay evidence seed is missing: $($file.Name)" }
    if ($seenSeeds.ContainsKey($seed)) { throw "Duplicate gameplay evidence seed: $seed" }
    $seenSeeds[$seed] = $true

    $world = [string]$proof.world
    if ([string]::IsNullOrWhiteSpace($world) -or -not $world.StartsWith('NCMM_AWS_SMOKE_')) {
        throw "Gameplay evidence world name is invalid: $($file.Name)"
    }
    if ($seenWorlds.ContainsKey($world)) { throw "Duplicate gameplay evidence world: $world" }
    $seenWorlds[$world] = $true

    $totalEffectAssertions += [int]$proof.survivor_effect_assertions
    $totalIntegrationInert += [int]$proof.survivor_integrations_inert
}

if ($seenSeeds.Count -ne $ExpectedWorlds -or $seenWorlds.Count -ne $ExpectedWorlds) {
    throw 'Gameplay evidence does not contain the expected number of distinct seeds/worlds.'
}

Write-Host (
    "NCMM Module Gameplay Evidence: PASS (worlds=$ExpectedWorlds; " +
    "AWS=48 settings/world; Survivor=369 perks/world; " +
    "effect_assertions=$totalEffectAssertions; conditional_inert=$totalIntegrationInert)"
) -ForegroundColor Green
