param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent),[string]$BuildRoot='C:\NCMMBuild',[string]$SurvivorSource='')
$ErrorActionPreference='Stop'
if([string]::IsNullOrWhiteSpace($SurvivorSource)){$SurvivorSource=Join-Path $BuildRoot 'survivor_0910_src\src\survivor_progression.cpp'}
if(-not(Test-Path $SurvivorSource -PathType Leaf)){throw "Golden regression requires transformed Survivor source: $SurvivorSource"}
$text=[IO.File]::ReadAllText($SurvivorSource)
$fixtureDir=Join-Path $PackageRoot 'golden\fixtures'
$fixtures=@(Get-ChildItem $fixtureDir -Filter '*.json' -File|Sort-Object Name)
if($fixtures.Count -ne 6){throw "Expected six golden fixtures, found $($fixtures.Count)."}
$fail=New-Object System.Collections.Generic.List[string]
foreach($f in $fixtures){
    $g=Get-Content $f.FullName -Raw|ConvertFrom-Json
    foreach($token in $g.required_tokens){if(-not $text.Contains([string]$token)){$fail.Add(([string]$g.id)+':missing_token:'+[string]$token)}}
}
foreach($token in @('constexpr int state_schema = 8;','active_mods.registry.v2','mg_arcane_focus','mom_mental_focus','xe_anomaly_method','af_systems_operator','afp_prime_operator','sec_field_researcher','secx_flesh_initiate')){if(-not $text.Contains($token)){$fail.Add('global:'+ $token)}}
# Count final mod-native table rows by stable integration prefixes.  This is a source-level regression guard,
# not a replacement for runtime tests; the installer has its own exact 161-row cumulative audit as well.
$integrationMatches=[regex]::Matches($text,'\{\s*"(?:mg_|mom_|xe_|af_|afp_|sec_|secx_)[^"]+"\s*,\s*branch_id::mastery')
if($integrationMatches.Count -lt 140){$fail.Add('integration_rows_suspicious:'+ $integrationMatches.Count)}
if($fail.Count -gt 0){$fail|ForEach-Object{Write-Host ('GOLDEN FAIL '+$_) -ForegroundColor Red};exit 51}
Write-Host ('Golden regression: PASS ('+$fixtures.Count+' fixtures; integration rows observed '+$integrationMatches.Count+')') -ForegroundColor Green
exit 0
