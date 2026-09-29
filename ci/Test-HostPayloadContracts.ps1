param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$m=Get-Content (Join-Path $PackageRoot 'compat\compatibility.manifest.json') -Raw|ConvertFrom-Json
$common083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1'))

$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'));foreach($n in @('[switch]$HostSourceProbeOnly','Set-InfrastructureTransactionPhase "compile"','Set-InfrastructureTransactionPhase "install"','DEEP_SOURCE_PASS')){if(-not $payload.Contains($n)){throw "Payload contract missing: $n"}}
foreach($badHere in @("'@.TrimEnd(",'"@.TrimEnd(',"'@ @'",'"@ @"')){if($payload.Contains($badHere)){throw ('PowerShell 5.1 unsafe here-string composition: '+$badHere)}}
# Windows PowerShell 5.1 requires a here-string closing marker to be the only token on its line.
$payloadLines=$payload -split "`r?`n"
for($lineIndex=0;$lineIndex -lt $payloadLines.Count;$lineIndex++){
    $line=$payloadLines[$lineIndex]
    $isTerminator=$line.StartsWith("'@") -or $line.StartsWith('"@')
    if($isTerminator -and $line.Substring(2).Trim().Length -gt 0){
        throw ('PowerShell 5.1 unsafe here-string terminator at payload line '+($lineIndex+1)+': '+$line)
    }
}
# Backslash does not escape an apostrophe in a PowerShell single-quoted string.
# These patterns previously turned C++ character literals into three MissingPropertyName parser errors.
foreach($badPsQuoteEscape in @( "\'.\'", "\'\\n\'" )){
    if($payload.Contains($badPsQuoteEscape)){
        throw ('PowerShell 5.1 unsafe backslash-escaped apostrophe in payload: '+$badPsQuoteEscape)
    }
}

if([string]$m.host_version -ne '0.8.1' -or [string]$m.ncmm_api -ne '1.9' -or [string]$m.host_api_v2 -ne '2.0'){throw 'Host API 2.0 Core manifest identity mismatch.'}
foreach($cap in @('host_api.v2.core','events.core.v2','settings.typed.v2','character.modifiers.v2','runtime_hooks.registry.v2','worldgen.bindings.v2','module.lifecycle.query.v2')){if(@($m.self_test.required_capabilities) -notcontains $cap){throw "Host API 2.0 required capability missing: $cap"}}
$hostComponent=Get-Content (Join-Path $PackageRoot 'components\ncmm_host.json') -Raw|ConvertFrom-Json
if([string]$hostComponent.version -ne '0.8.1' -or @($hostComponent.provides) -notcontains 'host_api_v2:2.0'){throw 'Host API 2.0 component catalog mismatch.'}
foreach($n in @('function Apply-NcmmHostApi20Core','#define NCMM_HOST_API_V2_CORE_MAJOR 2u','typedef struct ncmm_host_api_v2_core {','const ncmm_host_api_v2_core api_v2_core = {','runtime_hook_bind_modifier_v2','worldgen_hook_bind_setting_v2','Apply-NcmmHostApi20Core')){if(-not $payload.Contains($n)){throw "Host API 2.0 payload contract missing: $n"}}
# Patched-source audit must distinguish the required query_interface_v2 forward declaration
# from its single implementation.  A raw substring count is intentionally invalid because both
# declaration and definition begin with the same function name/signature.
if($payload.Contains("Needle = 'const void *query_interface_v2('; Expected = 1; Name = 'Host API 2.0 query interface'")){throw 'Stale ambiguous Host API 2.0 query-interface count audit returned.'}
foreach($n in @('Host API 2.0 query interface declaration','Host API 2.0 query interface definition','Host API 2.0 legacy v1 query-interface bridge','Host API 2.0 query-interface declaration/legacy-table/definition order is invalid.')){
    if(-not $payload.Contains($n)){throw ('Host API 2.0 structural query-interface audit missing: '+$n)}
}
$queryAuditProbeRaw = @'
const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor );

const ncmm_host_api_v1 api = {
    &query_interface_v2
};
const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )
{
    return nullptr;
}
'@
# Mirror the same LF normalization semantics as the real patched-source audit below,
# without depending on the payload-local Normalize-Lf helper. On Windows this fixture starts as CRLF.
$queryAuditProbe = $queryAuditProbeRaw.Replace("`r`n","`n").Replace("`r","`n")
$queryDeclProbe='const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor );'
$queryDefProbe="const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )`n{"
if(([regex]::Matches($queryAuditProbe,[regex]::Escape('const void *query_interface_v2('))).Count -ne 2){throw 'Host API 2.0 query-interface regression fixture no longer models declaration + definition.'}
if(([regex]::Matches($queryAuditProbe,[regex]::Escape($queryDeclProbe))).Count -ne 1 -or ([regex]::Matches($queryAuditProbe,[regex]::Escape($queryDefProbe))).Count -ne 1 -or ([regex]::Matches($queryAuditProbe,[regex]::Escape('    &query_interface_v2'))).Count -ne 1){throw 'Host API 2.0 structural query-interface regression fixture failed.'}
if($queryAuditProbe.Contains("`r`n")){throw 'Host API 2.0 structural query-interface regression fixture was not normalized to LF.'}

# MSVC template-order regression: NCMM safe option getters must be declarations in options.h,
# with their value_as<T>() calls defined only in options.cpp after explicit specializations.
foreach($n in @(
    'bool ncmm_get_option_bool_or( const std::string &name, bool fallback );',
    'int ncmm_get_option_int_or( const std::string &name, int fallback );',
    'float ncmm_get_option_float_or( const std::string &name, float fallback );',
    'NCMM v8.7.6.6 fail-safe accessor definitions',
    'NCMM safe option getter definitions must follow cOpt::value_as<T> explicit specializations.'
)){
    if(-not $payload.Contains($n)){throw ('MSVC safe-option accessor ordering contract missing: '+$n)}
}
$getterReplacementMatch=[regex]::Match($payload,'(?s)\$getterReplacement\s*=\s*@''\r?\n(?<body>.*?)\r?\n''@')
if(-not $getterReplacementMatch.Success){throw 'Could not isolate NCMM safe getter header replacement fixture.'}
$getterReplacementBody=$getterReplacementMatch.Groups['body'].Value
foreach($badInline in @('inline bool ncmm_get_option_bool_or','inline int ncmm_get_option_int_or','inline float ncmm_get_option_float_or')){
    if($getterReplacementBody.Contains($badInline)){throw ('MSVC premature value_as<T> instantiation regression returned: '+$badInline)}
}

# Host build success is log+artifact gated, never ExitCode-only. Hotfix 16 also makes
# ncmm_loader.cpp project membership explicit instead of depending on wildcard/incremental evaluation.
foreach($n in @(
    '.ncmm_host_0831_persistence_build.sha256',
    '"v8.2-host-0831-persistence"',
    'Get-ChildItem -LiteralPath $objWinRoot -Filter "ncmm_loader.obj" -File -Recurse',
    '$explicitLoaderItem = ''<ClCompile Include="..\src\ncmm_loader.cpp" />''',
    '$mzWildcardPattern',
    'Cataclysm-libMZ explicit ncmm_loader.cpp project integration audit failed.',
    'Host loader MSBuild project integration: explicit ClCompile item READY.',
    'Cataclysm-libMZ-vcpkg-static-Release-x64.lib',
    'Host loader cache invalidation:',
    '$loaderCompiledThisRun',
    'MSBuild did not compile explicitly integrated ncmm_loader.cpp; rejecting the build.',
    'MSBuild loader object was not recreated from the explicit ClCompile item.',
    'MSBuild loader object is missing/freshness-invalid after explicit ClCompile integration.',
    'Remove-Item $builtHostPath -Force -ErrorAction SilentlyContinue',
    'MSBuild returned exit 0 but compiler/linker failure markers were found; rejecting the build.',
    'MSBuild host executable timestamp predates this build; rejecting stale output.',
    'MSBuild passed gates but the exact expected cataclysm-tiles.exe is missing.',
    'Host API 2.0 public runtime/worldgen hooks are not in the external ncmm namespace after the anonymous namespace closes.'
)){
    if(-not $payload.Contains($n)){throw ('Host build-gate/project-integration contract missing: '+$n)}
}
if($payload.Contains('Get-ChildItem $CddaRoot -Filter "cataclysm-tiles.exe" -Recurse')){
    throw 'Unsafe recursive stale-host fallback returned.'
}

# Model the exact upstream MZ wildcard shape and prove the Hotfix 16 matcher can isolate it.
$mzProjectFixture = @'
  <ItemGroup>
    <ClCompile Include="..\src\*.cpp" Exclude="..\src\main.cpp;..\src\messages.cpp;..\src\a*.cpp;..\src\l*.cpp" />
  </ItemGroup>
'@
$mzProjectFixture = $mzProjectFixture.Replace("`r`n","`n").Replace("`r","`n")
$mzProjectPattern = '(?m)^(?<indent>[ \t]*)<ClCompile Include="\.\.\\src\\\*\.cpp" Exclude="(?<exclude>[^"]*)" />[ \t]*$'
$mzProjectMatch = [regex]::Match($mzProjectFixture,$mzProjectPattern)
if(-not $mzProjectMatch.Success){throw 'Hotfix 16 MZ wildcard regression fixture failed to match upstream project shape.'}
$mzFixtureExplicit = '<ClCompile Include="..\src\ncmm_loader.cpp" />'
$mzFixtureExclude = [string]$mzProjectMatch.Groups['exclude'].Value
$mzFixtureIndent = [string]$mzProjectMatch.Groups['indent'].Value
$mzFixtureReplacement = $mzFixtureIndent + '<ClCompile Include="..\src\*.cpp" Exclude="' + $mzFixtureExclude + ';..\src\ncmm_loader.cpp" />' + "`n" + $mzFixtureIndent + $mzFixtureExplicit
$mzFixturePatched = $mzProjectFixture.Substring(0,$mzProjectMatch.Index) + $mzFixtureReplacement + $mzProjectFixture.Substring($mzProjectMatch.Index + $mzProjectMatch.Length)
if(([regex]::Matches($mzFixturePatched,[regex]::Escape($mzFixtureExplicit))).Count -ne 1 -or ([regex]::Matches($mzFixturePatched,[regex]::Escape('..\src\ncmm_loader.cpp'))).Count -ne 2){throw 'Hotfix 16 MZ explicit loader regression fixture failed.'}

$msbuildFailureProbe='options.cpp(808,1): error C2908: specialization after instantiation'
if(-not ($msbuildFailureProbe -match '(?i)\berror\s+(C|LNK)\d+|\bfatal error\b|\bMSB\d+\b.*(?:error|failed)|:\s*error\b|Build FAILED')){
    throw 'MSBuild canonical-error regression fixture failed.'
}
$loaderCompileProbe="  ncmm_loader.cpp"
if(-not ($loaderCompileProbe -match '(?im)^\s*ncmm_loader\.cpp\s*$')){
    throw 'NCMM loader compile-proof regression fixture failed.'
}

# Migration-audit regression: module-specific integration IDs belong to Survivor,
# while the mechanics contract/Host audit validates generic hooks only.
if($payload.Contains('v8 mechanics contract audit missing:')){throw 'Stale pre-Host-API2 mechanics audit returned.'}
foreach($n in @('Host API 2.0 mechanics contract audit missing:','Survivor 0.9.15 mechanics audit missing:','Host API 2.0 mechanics optimization audit missing:')){
    if(-not $payload.Contains($n)){throw ('Host API 2.0 migration audit contract missing: '+$n)}
}
# Host API 2.0 legacy modifier cleanup must remove both ordinary map entries with a comma
# and the final map entry without a comma.  secx_duration_pct exposed this PS/source-transform edge case.
foreach($legacyCleanupProbe in @(
    '    { "legacy_probe_a", { -1.0, 1.0 } },' + "`n",
    '    { "legacy_probe_b", { -1.0, 1.0 } }' + "`n"
)){
    $legacyCleanupId = if($legacyCleanupProbe.Contains('legacy_probe_a')){'legacy_probe_a'}else{'legacy_probe_b'}
    $legacyCleanupPattern = '(?m)^[ \t]*\{ "' + [regex]::Escape($legacyCleanupId) + '", \{[^\r\n]+\} \}[ \t]*,?[ \t]*\r?\n?'
    if([regex]::Matches($legacyCleanupProbe,$legacyCleanupPattern).Count -ne 1){
        throw ('Host API 2.0 legacy modifier cleanup does not match final/no-comma map entry: '+$legacyCleanupId)
    }
}
if(-not $payload.Contains('\}[ \t]*,?[ \t]*\r?\n?')){throw 'Host API 2.0 optional-comma legacy cleanup pattern missing.'}
# AWS 0.6.2 manifest migration must follow the real generated 0.6.1 manifest contract:
# API 1.7 and requires ending in api.versioning.v1.  It must not depend on ui.theme.v1.
foreach($n in @('[int]$manifestObj.api_min_minor -ne 7','$manifestObj.api_min_minor = 9',"'host_api.v2.core','settings.typed.v2','worldgen.bindings.v2'",'AWS 0.6.2 expected exactly 48 geography bindings')){
    if(-not $payload.Contains($n)){throw ('AWS 0.6.2 semantic manifest/binding migration contract missing: '+$n)}
}
$staleAwsMigration='AWS 0.6.2 manifest capability anchor missing.'
if($payload.Contains($staleAwsMigration)){throw ('Stale AWS 0.6.2 manifest migration contract returned: '+$staleAwsMigration)}
$awsManifestProbeText = @'
{
  "id": "advanced_world_settings",
  "name": "Advanced World Settings",
  "version": "0.6.1",
  "loader_api": 1,
  "api_major": 1,
  "api_min_minor": 7,
  "requires": ["core.v1","world_settings.v2","api.versioning.v1"],
  "failure_policy": "disable"
}
'@
$awsManifestProbe = $awsManifestProbeText | ConvertFrom-Json
$awsManifestProbe.version='0.6.2';$awsManifestProbe.api_min_minor=9;$awsManifestProbeReq=@($awsManifestProbe.requires)
foreach($cap in @('host_api.v2.core','settings.typed.v2','worldgen.bindings.v2')){if($awsManifestProbeReq -notcontains $cap){$awsManifestProbeReq += $cap}}
$awsManifestProbe.requires=@($awsManifestProbeReq);$awsManifestProbeRoundTrip=(($awsManifestProbe|ConvertTo-Json -Depth 8)|ConvertFrom-Json)
if([string]$awsManifestProbeRoundTrip.version -ne '0.6.2' -or [int]$awsManifestProbeRoundTrip.api_min_minor -ne 9){throw 'AWS 0.6.2 semantic manifest migration regression failed.'}
foreach($cap in @('host_api.v2.core','settings.typed.v2','worldgen.bindings.v2')){if(@($awsManifestProbeRoundTrip.requires|Where-Object{$_ -eq $cap}).Count -ne 1){throw ('AWS 0.6.2 semantic manifest capability regression failed: '+$cap)}}

$b=[IO.File]::ReadAllBytes((Join-Path $PackageRoot 'NCMM.cmd'));if($b.Length -ge 3 -and $b[0]-eq 0xEF -and $b[1]-eq 0xBB -and $b[2]-eq 0xBF){throw 'NCMM.cmd must not contain UTF-8 BOM.'}
$cmdText=[IO.File]::ReadAllText((Join-Path $PackageRoot 'NCMM.cmd'))
foreach($bad in @(' -Command ','^|%%{','SHIFT ','%*')){if($cmdText.Contains($bad)){throw ('NCMM.cmd unsafe CMD/PowerShell bridge token: '+$bad)}}
foreach($need in @('choice /c 1234567890','if "%CHOICE_RC%"=="1" set "ACTION=Install"','if /i "%~1"=="selftest"    set "ACTION=RuntimeVerify"','-File "%SCRIPT%" -Action "%ACTION%"','NCMM command FAILED. Exit code:')){if(-not $cmdText.Contains($need)){throw ('NCMM.cmd launcher contract missing: '+$need)}}
$installText=[IO.File]::ReadAllText((Join-Path $PackageRoot 'internal\NCMM.Install.ps1'))
foreach($stale in @('CREATE_ADAPTER_FOR_CURRENT_BUILD.cmd','TRY_CURRENT_EXPERIMENTAL.cmd')){if($installText.Contains($stale)){throw ('Stale removed launcher reference: '+$stale)}}

# HOTFIX18 regression: PowerShell 5.1 treats `$name:` inside an interpolated string as an invalid variable reference.
# Scope-qualified forms such as $env: and $script: are legal; ordinary variables before punctuation must use ${name}:.
$payloadHotfix18 = Get-Content (Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1') -Raw
$unsafeColonReference18 = [regex]'\$(?!(?:global|local|script|private|using|env|function|variable|alias):)([A-Za-z_][A-Za-z0-9_]*)\:'
foreach($line18 in ($payloadHotfix18 -split "`r?`n")) {
    if(($line18.Contains('throw "') -or $line18.Contains('Write-Host "')) -and $unsafeColonReference18.IsMatch($line18)) {
        throw ('PS5.1 unsafe interpolated variable before colon: ' + $line18.Trim())
    }
}
if(-not $payloadHotfix18.Contains('${publicCount20}: $publicNeedle20')) { throw 'Hotfix18 braced publicCount20 diagnostic regression.' }

# HOTFIX17 regression: legacy contextual cleanup must happen before Host API2 public-hook insertion.
$payloadHotfix17 = Get-Content (Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1') -Raw
$cleanupNeedle17 = "`$contextStart20 = `$loader20.IndexOf('double contextual_metaphysics_swap( double value )')"
$cleanupPos17 = $payloadHotfix17.IndexOf($cleanupNeedle17)
$insertNeedle17 = "`$loader20 = `$loader20.Replace(`$publicAnchor20,`$publicHooks20 + `$publicAnchor20)"
$insertPos17 = $payloadHotfix17.IndexOf($insertNeedle17)
if($cleanupPos17 -lt 0 -or $insertPos17 -lt 0 -or $cleanupPos17 -gt $insertPos17) { throw 'Hotfix17 Host API2 cleanup/insertion order regression.' }
foreach($needle17 in @(
    'double runtime_hook_modifier( const char *hook_id, const char *subject_id,',
    'std::string runtime_source_mod_swap( const std::string &source_mod_id )',
    'const std::string &runtime_source_mod()',
    'double runtime_hook_modifier_for_creatures( const char *hook_id,',
    'bool worldgen_hook_bound( const char *hook_id )',
    'int worldgen_hook_bool( const char *hook_id, int fallback )',
    'int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback )',
    'double worldgen_hook_f64( const char *hook_id, double fallback )'
)) {
    if(-not $payloadHotfix17.Contains($needle17)) { throw "Hotfix17 public hook fixture missing: $needle17" }
}


& (Join-Path $PackageRoot 'ci\Test-InstallerTransaction.ps1') -PackageRoot $PackageRoot

# Infrastructure 0.8.3.1 World Settings persistence regression.
foreach($persistNeedle0831 in @(
    'static std::unordered_map<std::string, std::string> ncmm_deferred_option_values;',
    'name.rfind( "NCMM_", 0 ) == 0',
    'ncmm_apply_deferred_option_value( name, options[name] );',
    'ncmm_apply_deferred_option_value( name, opt );',
    'opts.get_option( name ).getPage() == "ncmm_experimental"',
    '.ncmm_host_0831_persistence_build.sha256',
    'v8.2-host-0831-persistence'
)){
    if(-not $payload.Contains($persistNeedle0831)){throw ('World Settings persistence contract missing: '+$persistNeedle0831)}
}
if(([regex]::Matches($payload,[regex]::Escape('ncmm_apply_deferred_option_value( name, options[name] );'))).Count -ne 4){throw 'Expected deferred-value application on all four typed setting creation paths.'}
if(([regex]::Matches($payload,[regex]::Escape('ncmm_apply_deferred_option_value( name, opt );'))).Count -ne 4){throw 'Expected deferred-value application on all four typed setting refresh paths.'}
$deferPos0831=$payload.IndexOf('name.rfind( "NCMM_", 0 ) == 0')
$registerPos0831=$payload.IndexOf('bool options_manager::ncmm_register_world_bool(')
if($deferPos0831 -lt 0 -or $registerPos0831 -lt 0 -or $deferPos0831 -gt $registerPos0831){throw 'Deferred NCMM deserialize gate must be generated before runtime setting registration implementations.'}

foreach($n in @(
    'Apply-NcmmManagerUiV1Source',
    'module_setting_meta',
    'manager_adjust_setting',
    'NCMM_MANAGER',
    'MODULE DETAILS',
)){if(-not $payload.Contains($n)){throw ('NCMM manager/settings payload contract missing: '+$n)}}
Write-Host 'NCMM Host/AWS payload regression contract: PASS' -ForegroundColor Green
