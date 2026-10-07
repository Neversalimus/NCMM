param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$suspensionSource=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods/SurvivorProgression/src/survivor_progression.cpp'))
foreach($needle in @(
    '"mom_telekinetic_suspension", branch_id::mastery, 3, 9, currency_id::perk, "mom_kinetic_control"',
    '{ "mom_telekinetic_suspension", 3, 1.0 }',
    '{ "encumbrance_pct", -5 }',
    '"Telekinetic Suspension", "Телекинетическая подвеска"'
)) {
    if(-not $suspensionSource.Contains($needle)){throw "Telekinetic Suspension contract missing: $needle"}
}
foreach($needle in @(
    '"gl_ammo_scrounger", branch_id::scavenging, 3, 12, currency_id::perk, "g_awareness"',
    '"gl_provision_scrounger", branch_id::scavenging, 4, 18, currency_id::perk, "gl_ammo_scrounger"',
    '"gl_medical_scrounger", branch_id::scavenging, 5, 24, currency_id::perk, "gl_provision_scrounger"',
    '"gl_rare_find", branch_id::scavenging, 6, 30, currency_id::perk, "gl_medical_scrounger"',
    '{ "sp_loot_ammo_pct", 0.25 }',
    '{ "sp_loot_provisions_pct", 0.25 }',
    '{ "sp_loot_medicine_pct", 0.25 }',
    '{ "sp_loot_rare_pct", 0.02 }',
    'loot_rank_scale[] = { 0.0, 1.0, 2.0, 4.0 }',
    'rare_loot_rank_scale[] = { 0.0, 1.0, 2.5, 4.0 }'
)) {
    if(-not $suspensionSource.Contains($needle)){throw "Scavenging loot perk contract missing: $needle"}
}
# Use the production patch function against a vanilla fixture, including CRLF and
# repeated application. Refuse an unknown upstream bodypart implementation.
$patchPath=Join-Path $PackageRoot 'host_patch/Apply-NCMMHostPatch.ps1'
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($patchPath,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Host patch parse failure'}
foreach($name in @('Normalize-Lf','Replace-ExactlyOnce','Patch-BodypartEncumbrance','Patch-MapgenScavengingLoot')) {
    $definition=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true))
    if($definition.Count -ne 1){throw "Missing production function: $name"}
    Invoke-Expression $definition[0].Extent.Text
}
$fixture="#include `"bodypart.h`"`r`nint bodypart::get_final_encumbrance( const Creature &mon ) const`r`n{`r`n    return std::max( 0.0, mon.enchantment_cache->modify_encumbrance( id, encumb_data.encumbrance ) );`r`n}`r`n"
$patched=Patch-BodypartEncumbrance $fixture
if((Patch-BodypartEncumbrance $patched) -cne $patched){throw 'Encumbrance patch not idempotent'}
foreach($needle in @('!mon.is_avatar()', 'ncmm::gameplay_modifier( "encumbrance_pct" )', 'std::lround( base_encumbrance * multiplier )')) {
    if(-not $patched.Contains($needle)){throw "Encumbrance consumer missing: $needle"}
}
$rejected=$false
try { $null=Patch-BodypartEncumbrance ($fixture.Replace('modify_encumbrance','unknown_encumbrance')) } catch { $rejected=$true }
if(-not $rejected){throw 'Unknown encumbrance source was silently accepted'}

$mapgenFixture=@'
#include "mapgen.h"
                    if( omt->has_flag( oter_flags::pp_generate_ruined ) ) {
                        pp_generator_aftershock_ruin.obj().execute( *this, omt_point, nullptr );
                    }
                }
            }
        }
    }

    const weather_generator &wgen = get_weather().get_cur_weather_gen();
'@
$mapgenPatched=Patch-MapgenScavengingLoot $mapgenFixture
if((Patch-MapgenScavengingLoot $mapgenPatched) -cne $mapgenPatched){throw 'Scavenging mapgen patch not idempotent'}
foreach($needle in @(
    '#include "ncmm_loader.h"',
    'const tripoint_rel_sm ncmm_pos( ncmm_gridx, ncmm_gridy, gridz );',
    '!generated.at( get_nonant( ncmm_pos ) ) || !save_results',
    'ncmm::apply_scavenging_loot_bonus( *this, ncmm_gridx, ncmm_gridy, gridz, when );'
)) {
    if(-not $mapgenPatched.Contains($needle)){throw "Scavenging mapgen consumer missing: $needle"}
}
$mapgenRejected=$false
try { $null=Patch-MapgenScavengingLoot ($mapgenFixture.Replace('pp_generator_aftershock_ruin','unknown_post_process')) } catch { $mapgenRejected=$true }
if(-not $mapgenRejected){throw 'Unknown mapgen source was silently accepted'}
$contractsLoot=Get-Content (Join-Path $PackageRoot 'compat\contracts.json') -Raw|ConvertFrom-Json
if(@($contractsLoot.contracts|Where-Object{$_.id -eq 'mapgen_scavenging_loot.source.v1'}).Count -ne 1){throw 'Scavenging mapgen source contract missing.'}

$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'))

# Survivor Progression 0.10.0 Mechanical Perks regression contracts.
# Historical gameplay/content contracts stay pinned here; build-cache marker/fingerprint are version-current and are checked by the 0.13.0 block below.
$survivor0100=Get-Content (Join-Path $PackageRoot 'components\survivor_progression.json') -Raw|ConvertFrom-Json
if([string]$survivor0100.version -ne '0.15.0'){throw 'Survivor 0.15.0 component identity mismatch.'}
$contracts0100=Get-Content (Join-Path $PackageRoot 'compat\contracts.json') -Raw|ConvertFrom-Json
if(@($contracts0100.contracts|Where-Object{$_.id -eq 'mechanical_combat_hooks.source.v1'}).Count -ne 1){throw 'Mechanical combat source contract missing.'}
foreach($mechanicalNeedle0100 in @(
    'function Apply-SurvivorMechanicalPerks0100',
    'Survivor Progression 0.10.0 Mechanical Perks Pass: READY',
    'combat.melee_crit_chance_pct','combat.melee_crit_damage_pct','combat.ranged_crit_damage_pct',
    'combat.damage_avoid_pct','combat.damage_taken_pct','combat.dodge_attempts_bonus',
    'combat.free_dodge_attempts_bonus','combat.block_attempts_bonus',
    '$characterHealthPath = Join-Path $src "character_health.cpp"',
    '{ "sm_defy_fate", 5, 1.0 }',
    'perkCountAfter0100 -ne $perkCountBefore0100 + 27'
)){
    if(-not $payload.Contains($mechanicalNeedle0100)){throw ('Survivor 0.10.0 mechanical contract missing: '+$mechanicalNeedle0100)}
}
$mechanicalPerkIds0100=@(
    'cm_critical_eye','cm_vital_strike','cm_execution_window','cm_guard_reserve','cm_second_reaction','cm_ballistic_weakpoints','cm_lethal_mastery',
    'sm_damage_control','sm_hard_to_kill','sm_defy_fate','sm_brace_for_impact','sm_indomitable_body',
    'mm_second_dodge','mm_efficient_evasion','mm_afterimage','mm_perfect_step','mm_combat_flow',
    'fm_precision_assembly','fm_field_maintenance','fm_masterwork_discipline',
    'gm_weakpoint_eye','gm_scrap_armor_instinct','gm_escape_route',
    'am_combat_synthesis','am_survival_synthesis','am_reflex_memory','am_apex_adaptation'
)
if($mechanicalPerkIds0100.Count -ne 27){throw 'Survivor 0.10.0 mechanical regression fixture count drift.'}
foreach($mechanicalId0100 in $mechanicalPerkIds0100){
    if(-not $payload.Contains('{ "'+$mechanicalId0100+'", branch_id::')){throw ('Survivor 0.10.0 mechanical perk definition missing: '+$mechanicalId0100)}
}
$exactAdapter0100=[IO.File]::ReadAllText((Join-Path $PackageRoot 'adapters\cdda_2026_09_23_0546.ps1'))
if(-not $exactAdapter0100.Contains("'src/ranged.cpp' = '54426a5dafe395306dc68fa73007f52e6ae2f645'")){throw 'Exact adapter ranged.cpp reference blob missing.'}
foreach($ref0110 in @(
    "'src/character.cpp' = '838812b4540beded00058a6d2b2782f5c586cc21'",
    "'src/melee.cpp' = '8f2a3e570e18648aed83466323b9f2514762a3db'",
    "'src/monster.cpp' = 'e6d6fff55d2c647adfd063e8bd009173307b39de'",
    "'src/npc.cpp' = 'eef674de308711ed7079f59c3f08dab777ef7f8b'",
    "'src/activity_actor.cpp' = '77807e28c54316db8f2b1890ffb81b2f24d18c6e'",
    "'src/trap.cpp' = '8857008a59b421d10ad40cf572c6b034bb8630f9'"
)){if(-not $exactAdapter0100.Contains($ref0110)){throw ('Exact adapter reactive reference blob missing: '+$ref0110)}}

# Survivor Progression 0.11.0 Reactive Mechanics + Technical Mastery regression contracts,
# plus 0.11.1 semantic polish, 0.11.2 edge hardening and 0.11.3 combinatorial edge polish. The 25-node 0.11.0 layer remains intact; no polish pass removes nodes.
$survivor0110=Get-Content (Join-Path $PackageRoot 'components\survivor_progression.json') -Raw|ConvertFrom-Json
if([string]$survivor0110.version -ne '0.15.0'){throw 'Survivor 0.15.0 component identity mismatch.'}
$contracts0110=Get-Content (Join-Path $PackageRoot 'compat\contracts.json') -Raw|ConvertFrom-Json
if(@($contracts0110.contracts|Where-Object{$_.id -eq 'reactive_technical_hooks.source.v4'}).Count -ne 1){throw 'Reactive/technical source contract missing.'}
foreach($reactiveNeedle0110 in @(
    'function Apply-SurvivorReactiveMechanics0110',
    'function Apply-SurvivorReactivePolish0111',
    'function Apply-SurvivorReactiveEdgePolish0112',
    'function Apply-SurvivorCombinatorialEdgePolish0113',
    'function Apply-NcmmReactiveMechanics0112',
    'function Apply-NcmmReactiveMechanics0113',
    'function Assert-NcmmReactiveMechanics0113Source',
    'Assert-NcmmReactiveMechanics0113Source $CddaRoot',
    'Survivor Progression 0.11.0 Reactive Mechanics: READY',
    'Survivor Progression 0.11.1 Reactive Mechanics Polish: READY',
    'Survivor Progression 0.11.2 Reactive Edge-Case Polish: READY',
    'Survivor Progression 0.11.3 Combinatorial Edge Polish: READY',
    '.survivor_0130_mana_hands_build.sha256',
    'v8.7.6.8-survivor-0.13.0-mana-hands',
    '.ncmm_reactive_mechanics_0112',
    '.ncmm_reactive_mechanics_0113',
    'NCMM_EVENT_PLAYER_KILL_V2 = 6u',
    'runtime_player_kill_notify()',
    'combat.riposte_chance_pct',
    'combat.execute_threshold_pct',
    'crafting.failure_save_pct',
    'scavenging.lockpick_roll_flat',
    'perkCountAfter0110 -ne $perkCountBefore0110 + 25',
    'ncmm_vanilla_counter_fired',
    'get_stamina() >= get_stamina_max() / 3',
    '!source->is_dead_state()',
    '!source->is_hallucination()',
    'source->attitude_to( *this ) == Creature::Attitude::HOSTILE',
    'source->is_avatar() && source != this',
    'is_avatar() && dam > 0 && !t.is_hallucination()',
    'ncmm_kill_attitude == MATT_ATTACK || ncmm_kill_attitude == MATT_FLEE',
    'Keep the displayed estimate aligned with the actual NCMM-adjusted success roll.',
    'const bool ncmm_perfect_lockpick = lockpick != nullptr',
    'const int ncmm_lock_floor = ncmm_perfect_lockpick ?',
    'momentum_stack_cap',
    'set_state( "momentum_stacks", 0 );',
    'perkCount0111 -ne $perkCountBeforePolish0111',
    'perkCount0112 -ne $perkCountBefore0112',
    'perkCount0113 -ne $perkCountBefore0113',
    'ncmm_refund_basis',
    'ncmm_hostile_target_before',
    'ncmm_hostile_reactive_target',
    'ncmm_hostile_avatar_kill_before_death',
    'ncmm_vanilla_counter_fired = melee_attack( *source, false, tec );',
    'craft_data_->next_failure_point <= item_counter',
    'ncmm_catastrophic_save',
    'current_stacks + 1',
    'raw_stacks',
    'raw_turns'
)){
    if(-not $payload.Contains($reactiveNeedle0110)){throw ('Survivor 0.11.3 combinatorial edge contract missing: '+$reactiveNeedle0110)}
}
$reactivePerkIds0110=@(
    'cr_riposte','cr_counterflow','cr_critical_surge','cr_execution_protocol','cr_predator_momentum','cr_relentless_momentum',
    'sr_adrenal_recovery','sr_battle_breath',
    'mr_slipstream','mr_breath_return','mr_reactive_step','mr_kinetic_chain',
    'fr_quality_control','fr_second_measure','fr_material_discipline','fr_failure_analysis','fr_zero_defect',
    'gr_trap_reader','gr_lock_whisperer','gr_quick_entry','gr_gentle_tools','gr_alarm_bypass',
    'ar_reactive_synthesis','ar_momentum_engine','ar_perfect_process'
)
if($reactivePerkIds0110.Count -ne 25){throw 'Survivor 0.11.0 reactive regression fixture count drift.'}
if($payload.Contains("Needle = 'const int ncmm_lock_floor = lockpick->has_flag( flag_PERFECT_LOCKPICK ) ?'")){
    throw 'Stale pre-null-safe lockpick floor audit must not return.'
}
if(-not $payload.Contains("Needle = 'const int ncmm_lock_floor = ncmm_perfect_lockpick ?'")){
    throw 'Current lockpick floor audit contract missing.'
}
if(-not $payload.Contains("Needle = 'to_moves<int>( 5_seconds ) : to_moves<int>( 30_seconds );'")){
    throw 'Lockpick vanilla floor-value audit contract missing.'
}
if(-not $payload.Contains("Needle = 'ncmm_lock_moves = std::max( ncmm_lock_floor,'")){
    throw 'Lockpick reduction floor-clamp audit contract missing.'
}
foreach($reactiveId0110 in $reactivePerkIds0110){
    if(-not $payload.Contains('{ "'+$reactiveId0110+'", branch_id::')){throw ('Survivor 0.11.0 reactive perk definition missing: '+$reactiveId0110)}
}
# 0.11.3 installer regression: keep generated gameplay-hook contract parse-safe and fingerprint every engine transform.
foreach($staleCache0113 in @('.survivor_0100_mechanical_build.sha256','v8.7.6.8-survivor-0.10.0-mechanical-api2')){
    if($payload.Contains($staleCache0113)){throw ('Stale Survivor 0.10.0 build-cache identity leaked into current payload: '+$staleCache0113)}
}
$mechanicsContract0113a='$mechanicsDefinition += "`n" + (Get-Command Apply-NcmmReactiveMechanics0112 -CommandType Function).Definition'
$mechanicsContract0113b='$mechanicsDefinition += "`n" + (Get-Command Apply-NcmmReactiveMechanics0113 -CommandType Function).Definition'
if(-not $payload.Contains($mechanicsContract0113a)){throw '0.11.2 gameplay hook body missing from patch-revision contract assembly.'}
if(-not $payload.Contains($mechanicsContract0113b)){throw '0.11.3 gameplay hook body missing from patch-revision contract assembly.'}
if($payload.Contains('.Definition`n$mechanicsDefinition')){throw 'Literal backtick-n statement separator regression in gameplay hook contract assembly.'}
$edgeFixture0113=Join-Path $PackageRoot 'golden\fixtures\survivor_reactive_edge_0113.cpp'
if(-not(Test-Path $edgeFixture0113 -PathType Leaf)){throw 'Survivor 0.11.3 edge fixture missing.'}
$edgeFixtureText0113=[IO.File]::ReadAllText($edgeFixture0113)
foreach($edgeNeedle0113 in @('isolated_refund','hostile_npc_kill_reward','on_kill_momentum','monotonic_failure_point','catastrophic_ui_chance')){
    if(-not $edgeFixtureText0113.Contains($edgeNeedle0113)){throw ('Survivor 0.11.3 edge fixture contract missing: '+$edgeNeedle0113)}
}
foreach($doc0113 in @('SURVIVOR_0.11.3_COMBINATORIAL_EDGE_POLISH_CHANGELOG.txt','SURVIVOR_0.11.3_COMBINATORIAL_EDGE_POLISH_AUDIT.txt')){
    if(-not(Test-Path (Join-Path $PackageRoot $doc0113) -PathType Leaf)){throw ('Survivor 0.11.3 package document missing: '+$doc0113)}
}

# HOTFIX7: 0.11.3 module audit must not inspect engine source before the engine transform runs.
$moduleAuditStart0113=$payload.IndexOf('# Module-only audit: engine-hook invariants are verified after Apply-NcmmReactiveMechanics0113.')
$moduleAuditEnd0113=$payload.IndexOf('Write-Host "Survivor 0.11.3 Combinatorial Edge Polish module audit: PASS', $moduleAuditStart0113)
if($moduleAuditStart0113 -lt 0 -or $moduleAuditEnd0113 -lt 0){throw 'Survivor 0.11.3 split module/engine audit contract missing.'}
$moduleAuditBody0113=$payload.Substring($moduleAuditStart0113,$moduleAuditEnd0113-$moduleAuditStart0113)
if($moduleAuditBody0113.Contains('ncmm_vanilla_counter_fired = melee_attack( *source, false, tec );')){throw 'Survivor 0.11.3 module audit incorrectly inspects CDDA character.cpp.'}
if(([regex]::Matches($payload,[regex]::Escape('Assert-NcmmReactiveMechanics0113Source $CddaRoot'))).Count -lt 2){throw 'Survivor 0.11.3 post-engine source audit is not wired into both install/probe paths.'}
foreach($engineAuditNeedle0113 in @(
    'ncmm_vanilla_counter_fired = melee_attack( *source, false, tec );',
    'const bool ncmm_riposte_executed = melee_attack( *source, false );',
    'is_avatar() && ncmm_hostile_target_before && dam > 0 && !t.is_hallucination()',
    'ncmm_hostile_avatar_kill_before_death',
    'craft_data_->next_failure_point <= item_counter',
    'ncmm_catastrophic_save'
)){if(-not $payload.Contains($engineAuditNeedle0113)){throw ('Survivor 0.11.3 post-engine audit contract missing: '+$engineAuditNeedle0113)}}

# HOTFIX3: 0.11.0 must accept the actual Survivor effect_label signature inherited from the seed/source.
if(-not $payload.Contains("`$effectFnStart0110 = `$sp.IndexOf('std::string effect_label( const std::string &id )')")) {
    throw 'Survivor 0.11.0 effect-label actual-signature contract missing.'
}
if(-not $payload.Contains("if(`$effectFnStart0110 -lt 0) { `$effectFnStart0110 = `$sp.IndexOf('std::string effect_label(') }")) {
    throw 'Survivor 0.11.0 effect-label fallback contract missing.'
}

# HOTFIX10: Host API 2.0 generic gameplay multi-line probes must normalize LF/CRLF before testing.
foreach($runtimeHookNeedle in @(
    'function Test-V82Contains([string]$Text,[string]$Needle)',
    'if( Test-V82Contains $character $turnResetVanilla ) {',
    'if( Test-V82Contains $characterHealth $damageAvoidAnchor ) {',
    '} elseif( Test-V82Contains $creature $damageCopyLegacyV2 ) {'
)){ if(-not $payload.Contains($runtimeHookNeedle)){ throw ('Host API 2.0 normalized multi-line probe contract missing: '+$runtimeHookNeedle) } }
foreach($staleRuntimeHookProbe in @(
    '$character.Contains($turnResetVanilla)',
    '$characterHealth.Contains($damageAvoidAnchor)',
    '$creature.Contains($damageCopyLegacyV2)'
)){ if($payload.Contains($staleRuntimeHookProbe)){ throw ('Host API 2.0 raw CRLF-sensitive multi-line probe returned: '+$staleRuntimeHookProbe) } }

# HOTFIX11: patched-source audit must distinguish the real craft failure cancellation hook
# from the 0.11.3 UI probability mirror. A raw hook-id count of one is stale because final
# crafting.cpp intentionally contains one runtime use and one item_destruction_chance() use.
if($payload.Contains("Needle = 'crafting.failure_save_pct'; Expected = 1; Name = 'craft failure-save hook'")){
    throw 'Stale single-count craft failure-save patched-source audit returned.'
}
foreach($craftAuditNeedle0113 in @(
    "Needle = 'const double ncmm_failure_save = std::max( 0.0, std::min( 35.0,'; Expected = 1; Name = 'craft failure-save runtime hook'",
    "Needle = 'const double ncmm_catastrophic_save = std::max( 0.0, std::min( 35.0,'; Expected = 1; Name = 'craft failure-save UI hook'",
    "is_avatar() && ncmm_hostile_target_before && dam > 0 && !t.is_hallucination()",
    '0.11.3 hostile damaging-critical reward guard missing.'
)){
    if(-not $payload.Contains($craftAuditNeedle0113)){throw ('HOTFIX11 final-source audit contract missing: '+$craftAuditNeedle0113)}
}
if($payload.Contains("if(-not `$meleeReactiveAudit.Contains('is_avatar() && dam > 0 && !t.is_hallucination()'))")){
    throw 'Stale pre-0.11.3 damaging-critical audit returned.'
}
$craftAuditProbe0113=@'
bool item::handle_craft_failure( Character &crafter )
{
    const double ncmm_failure_save = std::max( 0.0, std::min( 35.0,
            ncmm::runtime_hook_modifier( "crafting.failure_save_pct" ) ) );
}
float Character::item_destruction_chance( const recipe &making ) const
{
    const double ncmm_catastrophic_save = std::max( 0.0, std::min( 35.0,
            ncmm::runtime_hook_modifier( "crafting.failure_save_pct" ) ) );
    return 0.0f;
}
'@
if(([regex]::Matches($craftAuditProbe0113,[regex]::Escape('const double ncmm_failure_save = std::max( 0.0, std::min( 35.0,'))).Count -ne 1 -or
   ([regex]::Matches($craftAuditProbe0113,[regex]::Escape('const double ncmm_catastrophic_save = std::max( 0.0, std::min( 35.0,'))).Count -ne 1 -or
   ([regex]::Matches($craftAuditProbe0113,[regex]::Escape('crafting.failure_save_pct'))).Count -ne 2){
    throw 'HOTFIX11 craft failure-save semantic audit regression fixture failed.'
}

# HOTFIX4: 0.11.0 configure-hook insertion must be structural and newline-style agnostic.
foreach($configureNeedle0110 in @(
    '$configCrimsonAnchor0110 =',
    '$configCrimsonPos0110 = $sp.IndexOf($configCrimsonAnchor0110,$configPos0110)',
    '$configReturnPos0110 = $sp.IndexOf(''    return true;'',$configCrimsonPos0110 + $configCrimsonAnchor0110.Length)',
    'Survivor 0.11.0 configure crimson binding anchor missing.',
    'Survivor 0.11.0 configure return anchor missing.',
    '$sp = $sp.Insert($configReturnPos0110,(Normalize-Lf $configSubscribe0110))'
)) {
    if(-not $payload.Contains($configureNeedle0110)){throw ('Survivor 0.11.0 structural configure-anchor contract missing: '+$configureNeedle0110)}
}
if($payload.Contains("`$configTail0110 = @'")){throw 'Survivor 0.11.0 stale exact configure-tail anchor leaked into payload.'}
foreach($fragile011x in @(
    '`$sp.Contains(`$mechArrayOld0110)','`$sp.Contains(`$bindEnd0110)','`$sp.Contains(`$calcTail0110)',
    '`$sp.Contains(`$turnOld0112)','`$sp.Contains(`$calcOld0112)','`$sp.Contains(`$respecOld0112)',
    '`$sp.Contains(`$killOld0113)','`$sp.Contains(`$turnOld0113)'
)) { if($payload.Contains($fragile011x)){ throw "Survivor 0.11.x newline-sensitive multi-line anchor leaked into payload: $fragile011x" } }
# HOTFIX9: 0.9.15 mod-Prime tail is comma-less. 0.10.0 must supply a separator before mechanical perks.
foreach($separatorNeedle0100 in @(
    '$perkArrayPrefix0100 = $sp.Substring(0,$perkArrayEnd0100).TrimEnd()',
    'if(-not $perkArrayPrefix0100.EndsWith('','')) { $perkPrefix0100 = ",`n" }',
    '$firstMechanicalPos0100 = $sp.IndexOf(''{ "cm_critical_eye"'',$perkArrayStart0100)',
    '$separatorProbe0100 = $sp.Substring(0,$firstMechanicalPos0100).TrimEnd()',
    'Survivor 0.10.0 perk-array separator missing before cm_critical_eye.'
)) { if(-not $payload.Contains($separatorNeedle0100)){ throw ('Survivor 0.10.0 perk-array separator contract missing: '+$separatorNeedle0100) } }
if($payload.Contains('$sp = $sp.Insert($perkArrayEnd0100,"`n" + $mechanicalPerks0100.TrimEnd())')) {
    throw 'Survivor 0.10.0 stale comma-blind mechanical-perk insertion leaked into payload.'
}

# HOTFIX8/HOTFIX9: 0.10.0's final appended perk has no trailing comma. 0.11.0 must add one before appending its first record.
foreach($separatorNeedle0110 in @(
    '$perkArrayPrefix0110 = $sp.Substring(0,$perkArrayEnd0110).TrimEnd()',
    'if(-not $perkArrayPrefix0110.EndsWith('','')) { $perkPrefix0110 = ",`n" }',
    '$firstReactivePos0110 = $sp.IndexOf(''{ "cr_riposte"'',$perkArrayStart0110)',
    '$separatorProbe0110 = $sp.Substring(0,$firstReactivePos0110).TrimEnd()',
    'Survivor 0.11.0 perk-array separator missing before cr_riposte.'
)) { if(-not $payload.Contains($separatorNeedle0110)){ throw ('Survivor 0.11.0 perk-array separator contract missing: '+$separatorNeedle0110) } }
if($payload.Contains('$sp = $sp.Insert($perkArrayEnd0110,"`n" + (Normalize-Lf $reactivePerks0110).TrimEnd())')) {
    throw 'Survivor 0.11.0 stale comma-blind reactive-perk insertion leaked into payload.'
}

$separatorFixture0110=Join-Path $PackageRoot 'golden\fixtures\survivor_perk_array_separator_0110.cpp'
if(-not(Test-Path $separatorFixture0110 -PathType Leaf)){throw 'Survivor 0.11.0 perk-array separator fixture missing.'}
$separatorFixtureText0110=[IO.File]::ReadAllText($separatorFixture0110)
foreach($separatorFixtureNeedle0110 in @('secx_prime_vessel','cm_critical_eye','am_apex_adaptation','cr_riposte','std::size( perks ) == 4')){
    if(-not $separatorFixtureText0110.Contains($separatorFixtureNeedle0110)){throw ('Survivor 0.11.0 perk-array separator fixture contract missing: '+$separatorFixtureNeedle0110)}
}

foreach($required011x in @(
    '$sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))',
    '$calcFnAnchor0110 = ''calculated_effects calculate_owned_effects()''',
    '$shutdownFnAnchor0110 = ''void shutdown()''',
    'Replace-TextBlock $sp $mechArrayOld0110 $mechArrayNew0110',
    'Replace-TextBlock $sp $bindEnd0110 $bindEndNew0110',
    'Replace-TextBlock $sp $turnOld0112 $turnNew0112',
    'Replace-TextBlock $sp $killOld0113 $killNew0113'
)) { if(-not $payload.Contains($required011x)){ throw "Survivor 0.11.x normalized-transform contract missing: $required011x" } }

# HOTFIX13: chargen lifecycle isolation is canonical Host state; tree-row
# collision repair remains a Survivor source transform.
$hostSource013=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch\ncmm_loader.cpp'))
foreach($hostLifecycleNeedle013 in @(
    'return g != nullptr && !g->new_game && world_generator != nullptr &&',
    'if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;'
)){
    if(-not $hostSource013.Contains($hostLifecycleNeedle013)){
        throw ('HOTFIX13 canonical Host lifecycle contract missing: '+$hostLifecycleNeedle013)
    }
}
if(([regex]::Matches($hostSource013,[regex]::Escape('if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;'))).Count -ne 2){
    throw 'HOTFIX13 canonical Host runtime-hook lifecycle gate must cover both generic and Creature hook paths.'
}
foreach($retiredStateTransformNeedle013 in @(
    '$characterStateOld013 = @''',
    '$characterStateNew013 = @''',
    'HOTFIX13 chargen-safe character state availability',
    'HOTFIX13 generated Host character-state lifecycle guard missing.',
    'HOTFIX13 generated Host retained chargen-unsafe character-state predicate.',
    'HOTFIX13 generated Host runtime-hook lifecycle gates expected 2.'
)){
    if($payload.Contains($retiredStateTransformNeedle013)){
        throw ('HOTFIX13 retired Host source transform/audit returned to payload: '+$retiredStateTransformNeedle013)
    }
}
foreach($layoutNeedle013 in @(
    'HOTFIX13 tree row physical deconfliction',
    'std::map<int, std::vector<size_t>> ncmm_row_nodes;',
    'std::stable_sort( row_nodes.begin(), row_nodes.end()',
    'next_x2 += 2;',
    'HOTFIX13 generated tree-layout audit missing:'
)){
    if(-not $payload.Contains($layoutNeedle013)){
        throw ('HOTFIX13 tree-layout contract missing: '+$layoutNeedle013)
    }
}

# Model the exact post-routing row deconfliction invariant: a parent-centering collision
# at x2=2 must be repaired to distinct physical lanes, including duplicate declared columns.
$rows013=@(
    [pscustomobject]@{Index=0;Row=7;Column=1;X2=2},
    [pscustomobject]@{Index=1;Row=7;Column=1;X2=2},
    [pscustomobject]@{Index=2;Row=7;Column=2;X2=2},
    [pscustomobject]@{Index=3;Row=8;Column=4;X2=3}
)
foreach($row013 in @($rows013|Group-Object Row)){
    $members013=@($row013.Group)
    if($members013.Count -lt 2){continue}
    $members013=@($members013|Sort-Object Column,Index)
    $next013=[int]$members013[0].Column*2
    foreach($member013 in $members013){
        $declared013=[int]$member013.Column*2
        $next013=[Math]::Max($next013,$declared013)
        $member013.X2=$next013
        $next013+=2
    }
}
$row7x013=@($rows013|Where-Object{$_.Row -eq 7}|Sort-Object X2|ForEach-Object{$_.X2})
if($row7x013.Count -ne 3 -or $row7x013[1]-$row7x013[0] -lt 2 -or $row7x013[2]-$row7x013[1] -lt 2){
    throw 'HOTFIX13 tree row deconfliction regression fixture failed.'
}
if(($rows013|Where-Object{$_.Index -eq 3}).X2 -ne 3){
    throw 'HOTFIX13 singleton row must preserve parent-centered physical x.'
}

foreach($n in @(
    'Apply-NcmmManagerUiV1Source',
    'NCMM_SP_XP_RATE',
    'NCMM_SP_STAT_POWER',
    'configure_progression_settings',
    'progression_xp_rate_pct',
    'scale_configured_xp',
    'progression_stat_power_pct',
    'ncmm_on_locale_changed_v1',
    'settings.typed.v2',
    'Apply-SurvivorRecalibration0120',
    'Apply-NcmmModuleDataBridge0120',
    'ncmm_survivor_recalibration_kit',
    'std::min( result.major_owned, 12 )',
    'get_state( "respec_request", 0 ) > 0',
    'v8.7.6.8-survivor-0.13.0-mana-hands'
)){if(-not $payload.Contains($n)){throw ('Survivor 0.13.0 settings/recalibration contract missing: '+$n)}}
if($payload.Contains('reset.id = "respec"')){throw 'Free Survivor respec UI leaked into the current payload.'}

$recalStart0120=$payload.IndexOf('function Apply-SurvivorRecalibration0120')
$recalEnd0120=$payload.IndexOf('function Apply-AwsHostApi20Migration',$recalStart0120)
if($recalStart0120 -lt 0 -or $recalEnd0120 -le $recalStart0120){
    throw 'Survivor recalibration transform boundary missing.'
}
$recalSection0120=$payload.Substring($recalStart0120,$recalEnd0120-$recalStart0120)
foreach($recalHostMutationForbidden0120 in @(
    '$loaderPath0120 = Join-Path $NcmmRoot ''host_patch\ncmm_loader.cpp''',
    '$headerPath0120 = Join-Path $NcmmRoot ''host_patch\ncmm_loader.h''',
    'Replace-TextBlock $loader0120',
    'Replace-TextBlock $header0120',
    'Write-Utf8NoBom $loaderPath0120',
    'Write-Utf8NoBom $headerPath0120'
)){
    if($recalSection0120.Contains($recalHostMutationForbidden0120)){
        throw ('Survivor recalibration regressed to Host source mutation: '+$recalHostMutationForbidden0120)
    }
}
foreach($recalVerifierNeedle0120 in @(
    'The canonical Host is synchronized later in the cumulative payload.',
    'Apply-NcmmModuleDataBridge0120 verifies the canonical module-data bridge'
)){
    if(-not $recalSection0120.Contains($recalVerifierNeedle0120)){
        throw ('Survivor recalibration Host-verifier handoff missing: '+$recalVerifierNeedle0120)
    }
}

$moduleBridgeStart0120=$payload.IndexOf('function Apply-NcmmModuleDataBridge0120')
$moduleBridgeEnd0120=$payload.IndexOf('function Apply-SurvivorRecalibration0120',$moduleBridgeStart0120)
if($moduleBridgeStart0120 -lt 0 -or $moduleBridgeEnd0120 -le $moduleBridgeStart0120){
    throw 'NCMM module-data bridge transform boundary missing.'
}
$moduleBridgeSection0120=$payload.Substring($moduleBridgeStart0120,$moduleBridgeEnd0120-$moduleBridgeStart0120)
foreach($moduleBridgeVerifierNeedle0120 in @(
    'NCMM module-data canonical Host source missing:',
    'NCMM module-data canonical Host source contract missing:',
    'NCMM module-data canonical Host header contract missing:',
    '#include "init.h"',
    'void load_module_data()',
    'loader.load_data_from_path( data_path, source );',
    'void load_module_data();'
)){
    if(-not $moduleBridgeSection0120.Contains($moduleBridgeVerifierNeedle0120)){
        throw ('NCMM module-data Host verifier contract missing: '+$moduleBridgeVerifierNeedle0120)
    }
}
foreach($moduleBridgeHostMutationForbidden0120 in @(
    'Write-Utf8NoBom $moduleDataHostSource0120',
    'Write-Utf8NoBom $moduleDataHostHeader0120',
    'Replace-TextBlock $moduleDataHostSourceText0120',
    'Replace-TextBlock $moduleDataHostHeaderText0120'
)){
    if($moduleBridgeSection0120.Contains($moduleBridgeHostMutationForbidden0120)){
        throw ('NCMM module-data verifier mutated canonical Host sources: '+$moduleBridgeHostMutationForbidden0120)
    }
}
$canonicalHostSyncCall0120=$payload.LastIndexOf('Apply-NcmmBallisticHost082CanonicalSync')
$moduleDataBridgeCall0120=$payload.LastIndexOf('Apply-NcmmModuleDataBridge0120 $CddaRoot')
if($canonicalHostSyncCall0120 -lt 0 -or $moduleDataBridgeCall0120 -le $canonicalHostSyncCall0120){
    throw 'NCMM module-data Host verifier must run after canonical Host synchronization.'
}

# Survivor 0.14.0 feature retained in 0.15.0: high-level Magiclysm mana vampirism.
$survivorSource0121=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\src\survivor_progression.cpp'))
foreach($manaVampNeedle0121 in @(
    '{ "mg_mana_vampirism", branch_id::mastery, 9, 40, currency_id::perk, "mg_archmage"',
    '{ "mg_mana_vampirism", 5, 1.0 }',
    '{ "mg_mana_vampirism", integration_id::magiclysm }',
    'mg_melee_mana_vamp_pct',
    'combat.melee_mana_vamp_pct',
    'melee.mana-vampirism-hook',
    'dealt_special_dam.total_damage()',
    'ncmm_mana_vamp_fraction',
    'magic->mod_mana( *this, recovered_mana )',
    '!t.is_hallucination()',
    'Survivor Progression 0.15.0 initialized:',
    '"0.15.0",'
)){
    if(-not ($payload.Contains($manaVampNeedle0121) -or $survivorSource0121.Contains($manaVampNeedle0121))){
        throw ('Survivor 0.14.0 mana-vampirism contract missing: '+$manaVampNeedle0121)
    }
}

# Exact rank semantics: 1/2/3/4/5 percent. Fractional carry makes low-damage
# rank-I attacks useful instead of rounding every sub-1-mana hit to zero.
foreach($pct0121 in 1..5){
    $gain0121=[int][Math]::Floor((100.0*$pct0121/100.0)+1.0e-9)
    if($gain0121 -ne $pct0121){throw ('Mana-vamp rank fixture failed at '+$pct0121+'%')}
}
$fraction0121=0.0
$restored0121=0
foreach($hit0121 in 1..5){
    $exact0121=$fraction0121+(20.0*1.0/100.0)
    $gain0121=[int][Math]::Floor($exact0121+1.0e-9)
    $fraction0121=$exact0121-$gain0121
    $restored0121+=$gain0121
}
if($restored0121 -ne 1 -or [Math]::Abs($fraction0121) -gt 1.0e-8){
    throw 'Mana-vamp fractional-carry fixture failed: five 20-damage rank-I hits must restore exactly 1 mana.'
}

# Survivor 0.15.0: Magiclysm Dimensional Pouch.
$survivorSource0150=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\src\survivor_progression.cpp'))
$dimensionalPouchDataPath0150=Join-Path $PackageRoot 'mods\SurvivorProgression\persistent_data\dimensional_pouch.json'
if(-not(Test-Path $dimensionalPouchDataPath0150 -PathType Leaf)){throw 'Survivor 0.15.0 Dimensional Pouch data file missing.'}
$dimensionalPouchData0150=[IO.File]::ReadAllText($dimensionalPouchDataPath0150)
$manaHandCarrierDataPath0151=Join-Path $PackageRoot 'mods\SurvivorProgression\persistent_data\mana_hand_carrier.json'
if(-not(Test-Path $manaHandCarrierDataPath0151 -PathType Leaf)){throw 'Survivor 0.15.0 Mana Hand carrier data file missing.'}
$manaHandCarrierData0151=[IO.File]::ReadAllText($manaHandCarrierDataPath0151)
foreach($pouchNeedle0150 in @(
    '{ "mg_dimensional_pouch", branch_id::mastery, 8, 35, currency_id::perk, "mg_resonant_reserve"',
    '{ "mg_dimensional_pouch", 5, 1.0 }',
    '{ "mg_dimensional_pouch", integration_id::magiclysm }',
    'mg_dimensional_pouch_rank',
    'Survivor Progression 0.15.0 initialized:',
    '"0.15.0",'
)){
    if(-not $survivorSource0150.Contains($pouchNeedle0150)){
        throw ('Survivor 0.15.0 Dimensional Pouch source contract missing: '+$pouchNeedle0150)
    }
}
foreach($pouchDataNeedle0150 in @(
    '"id": "ncmm_survivor_dimensional_pouch"',
    '"INTEGRATED"',
    '"TARDIS"',
    '"pocket_type": "CONTAINER"',
    '"max_contains_volume": "120 L"',
    '"max_item_length": "200 cm"'
)){
    if(-not $dimensionalPouchData0150.Contains($pouchDataNeedle0150)){
        throw ('Survivor 0.15.0 Dimensional Pouch data contract missing: '+$pouchDataNeedle0150)
    }
}
foreach($carrierDataNeedle0151 in @(
    '"id": "ncmm_survivor_mana_hand_carrier"',
    '"INTEGRATED"',
    '"HIDDEN_ITEM"',
    '"TARDIS"',
    '"use_action": {',
    '"type": "effect_on_conditions"',
    '"effect_on_conditions": []',
    '"forbidden": true',
    '"holster": true',
    '"max_item_length": "5 meter"'
)){
    if(-not $manaHandCarrierData0151.Contains($carrierDataNeedle0151)){
        throw ('Survivor 0.15.0 Mana Hand carrier data contract missing: '+$carrierDataNeedle0151)
    }
}
if($manaHandCarrierData0151.Contains('"max_item_length": "5 m"')){
    throw 'Survivor 0.15.0 Mana Hand carrier uses unsupported abbreviated meter unit.'
}
if(([regex]::Matches($manaHandCarrierData0151,[regex]::Escape('"max_item_length": "5 meter"'))).Count -ne 2){
    throw 'Survivor 0.15.0 Mana Hand carrier must expose exactly two 5 meter internal pockets.'
}
if(([regex]::Matches($manaHandCarrierData0151,[regex]::Escape('"moves": 1'))).Count -ne 2){
    throw 'Survivor Mana Hand carrier must expose two nonzero-cost internal pockets.'
}
if($manaHandCarrierData0151.Contains('"moves": 0')){
    throw 'Survivor Mana Hand carrier zero-move obtain_cost regression returned.'
}
foreach($payloadNeedle0150 in @(
    'function Apply-SurvivorDimensionalPouch0150',
    'mg_dimensional_pouch',
    'mg_dimensional_pouch_rank',
    'ncmm_survivor_dimensional_pouch',
    'dimensional_pouch.json',
    'ncmm_survivor_mana_hand_carrier',
    'mana_hand_carrier.json',
    '"use_action": {',
    '"type": "effect_on_conditions"',
    '"effect_on_conditions": []',
    '"flags": [ "INTEGRATED", "UNBREAKABLE", "PERSONAL", "NO_SALVAGE", "ZERO_WEIGHT", "HIDDEN_ITEM", "TARDIS" ]',
    'stash_wielded_item_for_mana_hand',
    'restore_mana_hand_carrier_item',
    'persistent_data',
    'Survivor 0.15.0 Dimensional Pouch: READY'
)){
    if(-not $payload.Contains($payloadNeedle0150)){
        throw ('Survivor 0.15.0 Dimensional Pouch payload contract missing: '+$payloadNeedle0150)
    }
}
$expectedPouchRanks0150=@(
    [pscustomobject]@{Rank=1;Liters=5;LengthCm=120},
    [pscustomobject]@{Rank=2;Liters=10;LengthCm=120},
    [pscustomobject]@{Rank=3;Liters=20;LengthCm=150},
    [pscustomobject]@{Rank=4;Liters=50;LengthCm=150},
    [pscustomobject]@{Rank=5;Liters=120;LengthCm=200}
)
if($expectedPouchRanks0150.Count -ne 5 -or
   $expectedPouchRanks0150[0].Liters -ne 5 -or
   $expectedPouchRanks0150[1].Liters -ne 10 -or
   $expectedPouchRanks0150[2].Liters -ne 20 -or
   $expectedPouchRanks0150[3].Liters -ne 50 -or
   $expectedPouchRanks0150[4].Liters -ne 120 -or
   $expectedPouchRanks0150[0].LengthCm -ne 120 -or
   $expectedPouchRanks0150[1].LengthCm -ne 120 -or
   $expectedPouchRanks0150[2].LengthCm -ne 150 -or
   $expectedPouchRanks0150[3].LengthCm -ne 150 -or
   $expectedPouchRanks0150[4].LengthCm -ne 200){
    throw 'Survivor 0.15.0 Dimensional Pouch rank table drift.'
}

# Survivor 0.14.0: Magiclysm virtual third/fourth mana hands.
$contracts0130=Get-Content (Join-Path $PackageRoot 'compat\contracts.json') -Raw|ConvertFrom-Json
if(@($contracts0130.contracts|Where-Object{$_.id -eq 'magic_virtual_hands.source.v1'}).Count -ne 1){
    throw 'Magic virtual-hands source contract missing.'
}
$adapter0130=[IO.File]::ReadAllText((Join-Path $PackageRoot 'adapters\cdda_2026_09_23_0546.ps1'))
foreach($ref0130 in @(
    "'src/magic.cpp' = 'acd1ca60046ec8ad3a8d483497031ddd9eb8feaf'",
    "'src/handle_action.cpp' = '8a3ebf77fd6a631677a99ddc2d6f9880fcedc9b5'"
)){if(-not $adapter0130.Contains($ref0130)){throw ('Mana-hands exact adapter reference missing: '+$ref0130)}}

$survivorSource0130=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\src\survivor_progression.cpp'))
foreach($handNeedle0130 in @(
    '{ "mg_mana_hand_3", branch_id::mastery, 9, 42, currency_id::perk, "mg_archmage"',
    '{ "mg_mana_hand_4", branch_id::mastery, 10, 48, currency_id::perk, "mg_mana_hand_3"',
    '{ "mg_mana_hand_3", integration_id::magiclysm }',
    '{ "mg_mana_hand_4", integration_id::magiclysm }',
    'mg_virtual_hand_count',
    '!bind("magic.virtual_hand_count",NCMM_SELECTOR_SOURCE_MOD_V2,"magiclysm","mg_virtual_hand_count")',
    'Survivor Progression v0.15.0',
    '"0.15.0",'
)){
    if(-not $survivorSource0130.Contains($handNeedle0130)){
        throw ('Survivor 0.14.0 mana-hand module contract missing: '+$handNeedle0130)
    }
}
foreach($payloadNeedle0130 in @(
    'function Apply-SurvivorManaHands0130',
    'static int ncmm_virtual_hand_count(',
    'ncmm_virtual_limb_encumbrance_average(',
    '"magic.virtual_hand_count"',
    'const int ncmm_virtual_hands =',
    'const int ncmm_virtual_free_hands =',
    'ncmm::active_mana_hand_items( *this )',
    'ncmm::mana_hand_item_slot_of( *this, *candidate )',
    'is_armed() && ncmm_virtual_free_hands <= 0 && !ncmm_virtual_focus',
    '.survivor_0130_mana_hands_build.sha256',
    'v8.7.6.8-survivor-0.13.0-mana-hands'
)){
    if(-not $payload.Contains($payloadNeedle0130)){
        throw ('Survivor 0.13.0 mana-hand payload contract missing: '+$payloadNeedle0130)
    }
}

# Encumbrance dilution is anatomy-aware: virtual hands are zero-encumbrance limbs
# added to the actual physical limb count rather than assuming a two-armed human.
function Test-ManaHandAverage0130([int]$Average,[int]$Physical,[int]$Virtual){
    if($Virtual -le 0 -or $Average -le 0){return $Average}
    if($Physical -le 0){return 0}
    return [int][Math]::Round(($Average*$Physical)/[double]($Physical+$Virtual))
}
if((Test-ManaHandAverage0130 30 2 1) -ne 20){throw 'Third mana hand 2-arm encumbrance fixture failed.'}
if((Test-ManaHandAverage0130 30 2 2) -ne 15){throw 'Fourth mana hand 2-arm encumbrance fixture failed.'}
if((Test-ManaHandAverage0130 30 4 1) -ne 24){throw 'Third mana hand multi-arm encumbrance fixture failed.'}
if((Test-ManaHandAverage0130 30 4 2) -ne 20){throw 'Fourth mana hand multi-arm encumbrance fixture failed.'}

# Survivor 0.14.0: Host API 2.1 logical virtual-item slot regression contract.
$contracts0140=Get-Content (Join-Path $PackageRoot 'compat\contracts.json') -Raw|ConvertFrom-Json
if(@($contracts0140.contracts|Where-Object{$_.id -eq 'magic_virtual_slots.source.v1'}).Count -ne 1){
    throw 'Magic virtual-slot source contract missing.'
}
$hostVirtual0140=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch\ncmm_loader.cpp'))
if(-not $hostVirtual0140.Contains('absolute_path.lexically_normal().lexically_relative( game_root().lexically_normal() )')){
    throw 'NCMM module-data bridge no longer converts absolute module directories into CWD-relative cata_path values.'
}
if($hostVirtual0140.Contains('cata_path{ cata_path::root_path::unknown, persistent_dir }') -or
   $hostVirtual0140.Contains('cata_path{ cata_path::root_path::unknown, data_dir }')){
    throw 'NCMM module-data bridge regressed to wrapping absolute filesystem paths directly in root_path::unknown.'
}
if(-not $hostVirtual0140.Contains('Refusing persistent module data path outside the game root:') -or
   -not $hostVirtual0140.Contains('Refusing active module data path outside the game root:')){
    throw 'NCMM module-data path containment guards are missing.'
}

foreach($uxNeedle0151 in @(
    'return survivor_mana_hand_count() > 0 && loc && loc.held_by( get_avatar() );',
    'This is a two-handed item. It requires both Mana Hands III+IV',
    'This wielded item cannot be transferred to the available Mana Hand configuration.',
    'This item type cannot be held by a Mana Hand.',
    'mana_hand_carrier_type_id( "ncmm_survivor_mana_hand_carrier" )',
    'stash_wielded_item_for_mana_hand',
    'restore_mana_hand_carrier_item',
    'physically_wielded && std::string_view( module_id ) == survivor_module_id',
    'pocket->is_forbidden()'
)){
    if(-not $hostVirtual0140.Contains($uxNeedle0151)){
        throw ('Mana Hand discoverability contract missing: '+$uxNeedle0151)
    }
}
$sdkVirtual0140=[IO.File]::ReadAllText((Join-Path $PackageRoot 'sdk\ncmm_api.h'))
$survivorVirtual0140=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\src\survivor_progression.cpp'))
foreach($needle0140 in @(
    'character.virtual_items.v1',
    'virtual_item_choose_v2',
    'virtual_item_for_slot_internal',
    'virtual_item_matches_slot( const item &candidate',
    'virtual_item_marker_key = "ncmm_virtual_slot"',
    'candidate->uid().get_value()',
    'game_menus::inv::titled_filter_menu',
    'const bool candidate_two_handed = candidate.is_two_handed( you );',
    'NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2',
    'release_virtual_item( item &it )',
    'virtual_item_primary_melee_key = "ncmm_virtual_primary_melee"',
    'virtual_item_primary_melee_enabled( const item &it )',
    'virtual_item_set_primary_melee( item &it, bool enabled )',
    'virtual_item_primary_melee_enabled_v2( const char *module_id, const char *slot_id )',
    'virtual_item_set_primary_melee_v2( const char *module_id, const char *slot_id,',
    '&virtual_item_primary_melee_enabled_v2',
    '&virtual_item_set_primary_melee_v2',
    'virtual_melee_context_suppresses_martial_arts( const Character &who )'
)){
    if(-not $hostVirtual0140.Contains($needle0140)){
        throw ('Survivor 0.14.0 Host virtual-item contract missing: '+$needle0140)
    }
}
foreach($needle0140 in @(
    '#define NCMM_HOST_API_V2_CORE_MINOR 3u',
    'NCMM_HOST_API_V2_CORE_SIZE_2_1',
    'NCMM_HOST_API_V2_CORE_SIZE_2_2',
    'NCMM_HOST_API_V2_CORE_SIZE_2_3',
    'NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2',
    'NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2',
    'virtual_item_choose',
    'virtual_item_clear',
    'virtual_item_name',
    'virtual_item_uid',
    'virtual_item_secondary_melee_enabled',
    'virtual_item_set_secondary_melee',
    'virtual_item_primary_melee_enabled',
    'virtual_item_set_primary_melee'
)){
    if(-not $sdkVirtual0140.Contains($needle0140)){
        throw ('Survivor 0.14.0 SDK virtual-item contract missing: '+$needle0140)
    }
}
foreach($needle0140 in @(
    'host2->virtual_item_choose',
    'host2->virtual_item_clear',
    'host2->virtual_item_name',
    'virtual_item_secondary_controls_available()',
    'host2->api_minor >= 2u',
    'host2->struct_size >= NCMM_HOST_API_V2_CORE_SIZE_2_2',
    'host2->virtual_item_secondary_melee_enabled',
    'host2->virtual_item_set_secondary_melee',
    'virtual_item_primary_controls_available()',
    'host2->api_minor >= 3u',
    'host2->struct_size >= NCMM_HOST_API_V2_CORE_SIZE_2_3',
    'host2->virtual_item_primary_melee_enabled',
    'host2->virtual_item_set_primary_melee',
    'Primary melee: ',
    'Enable primary Mana Hand melee',
    'Disable primary Mana Hand melee',
    'Secondary strike: ',
    'Enable secondary strike',
    'Disable secondary strike',
    'constexpr const char *mana_hand_pair_slot_id = "mana_hands_34";',
    'host2->virtual_item_clear( module_id, mana_hand_pair_slot_id );',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    'api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 1u )',
    '"0.15.0"'
)){
    if(-not $survivorVirtual0140.Contains($needle0140)){
        throw ('Survivor 0.14.0 module virtual-item contract missing: '+$needle0140)
    }
}
foreach($needle0140 in @(
    'function Apply-SurvivorVirtualItemSlots0140',
    'const int ncmm_virtual_free_hands =',
    'bool ncmm_virtual_focus = false;',
    'ncmm::active_mana_hand_items( *this )',
    'ncmm::mana_hand_item_slot_of( *this, *candidate )',
    'required_source_hands',
    'melee::blocking_ability( *candidate )',
    'ncmm::is_virtual_item( *shield )',
    'Apply-SurvivorVirtualItemSlots0140 $CddaRoot',
    'function Apply-SurvivorVirtualItemContext0140',
    'assign to Mana Hand III',
    'release from Mana Hand IV',
    'ncmm::virtual_item_can_assign(',
    'ncmm::virtual_item_assign(',
    'ncmm::virtual_item_clear(',
    'ncmm::mana_hand_inventory_action_visible( locThisItem )',
    'ncmm::mana_hand_inventory_action( locThisItem )',
    "case 'H':",
    'Apply-SurvivorVirtualItemContext0140 $CddaRoot',
    'function Apply-SurvivorVehicleCraftingXp0151',
    '// NCMM Survivor Crafting XP: successful vehicle install.',
    '// NCMM Survivor Crafting XP: successful vehicle removal.',
    'ncmm::gameplay_metric_record_completed_craft( you );',
    'Apply-SurvivorVehicleCraftingXp0151 $CddaRoot',
    'function Apply-SurvivorManaHandSpellcastingAid0140',
    'ncmm_virtual_wield_flags',
    'flag_id( "SPELLCASTING_AID" )',
    'ncmm::active_mana_hand_items( *me_chr_const )',
    'candidate->has_flag( flag )',
    'Apply-SurvivorManaHandSpellcastingAid0140 $CddaRoot',
    'function Apply-SurvivorVirtualItemLifecycle0140',
    'ncmm::release_virtual_item( *target() );',
    'Apply-SurvivorVirtualItemLifecycle0140 $CddaRoot',
    'function Apply-SurvivorManaHandUtility0140',
    'bool ncmm_mana_hand_holds_item( const Character &who, const item &it )',
    'ncmm::active_mana_hand_items( who )',
    'candidate == &it',
    'if( need_wielding && !p.is_wielding( it ) && !ncmm_mana_hand_holds_item( p, it ) ) {',
    'if( need_wielding && !p->is_wielding( it ) && !ncmm_mana_hand_holds_item( *p, it ) ) {',
    'Apply-SurvivorManaHandUtility0140 $CddaRoot',
    'function Apply-SurvivorManaHandSecondaryMelee0140',
    'class ncmm_virtual_melee_scope',
    'who, weapon, suppress_martial_arts',
    'ncmm::active_mana_hand_items( who )',
    'ncmm::mana_hand_item_slot_of( who, *weapon )',
    'ncmm::mana_hand_item_slot::paired',
    'std::clamp( ( who.attack_speed( weapon ) + 9 ) / 10, 5, 50 )',
    'virtual_item_secondary_melee_enabled( *weapon )',
    'who.melee_attack( target, false )',
    'who.magic->mod_mana( who, -mana_cost )',
    'virtual_melee_context_is_wielding( *this, target )',
    'ncmm_run_mana_hand_secondary_melee( *this, t );',
    'enable Mana Hand secondary strike',
    'Apply-SurvivorManaHandSecondaryMelee0140 $CddaRoot',
    'function Apply-SurvivorManaHandPairedGrip0140',
    '"mana_hands_34"',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    "case '5':",
    'Apply-SurvivorManaHandPairedGrip0140 $CddaRoot',
    'function Apply-SurvivorManaHandRanged0140',
    'item_location ncmm_real_weapon;',
    'aim_activity_actor::use_item_location',
    'ncmm_virtual_shot_mana_cost',
    'ncmm_virtual_mana_gun_mode',
    'fire with Mana Hand',
    "case 'g':",
    'Apply-SurvivorManaHandRanged0140 $CddaRoot',
    'function Apply-SurvivorManaHandPairedRanged0140',
    'ncmm_virtual_mana_paired_gun_mode',
    'Apply-SurvivorManaHandPairedRanged0140 $CddaRoot',
    'function Apply-SurvivorManaHandReloadAndShoot0140',
    'ncmm_mana_hand_ras_switch',
    'activity != nullptr ? activity->get_weapon() : you->get_wielded_item()',
    'Apply-SurvivorManaHandReloadAndShoot0140 $CddaRoot',
    'function Apply-SurvivorManaHandFireAction0140',
    'ncmm_fire_candidates',
    'Fire which Mana Hand weapon?',
    'aim_activity_actor::use_item_location( ncmm_selected_gun )',
    'Apply-SurvivorManaHandFireAction0140 $CddaRoot',
    'function Apply-SurvivorManaHandGunControls0140',
    'ncmm_select_mana_hand_gun_control',
    'Reload which Mana Hand weapon?',
    'Burst-fire which Mana Hand weapon?',
    'Change firing mode on which Mana Hand weapon?',
    'Set default ammo for which Mana Hand weapon?',
    'Apply-SurvivorManaHandGunControls0140 $CddaRoot',
    'function Apply-SurvivorManaHandPrimaryMelee0140',
    'use as primary Mana Hand melee',
    "case 'P':",
    'ncmm::virtual_item_set_primary_melee(',
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::paired',
    'ncmm::primary_mana_hand_melee_weapon',
    'ncmm::virtual_melee_context_suppresses_martial_arts',
    'ncmm_virtual_melee_scope ncmm_primary_scope(',
    '*this, *ncmm_primary_weapon, false );',
    'Not enough mana to attack with the primary Mana Hand weapon.',
    'Apply-SurvivorManaHandPrimaryMelee0140 $CddaRoot',
    'function Apply-SurvivorManaHandMartialArts0140',
    'ncmm_primary_mana_hand_martial_weapon',
    'bool valid_weapon = ma.weapon_valid( martial_weapon );',
    'Apply-SurvivorManaHandMartialArts0140 $CddaRoot',
    'function Apply-SurvivorManaHandReachMelee0140',
    'ncmm_primary_mana_hand_reach_weapon',
    'ncmm_primary_mana_hand_has_reach',
    'item_location( you, ncmm_reach_weapon )',
    'item_location reach_weapon = used_weapon();',
    'Not enough mana for a primary Mana Hand reach attack.',
    'Apply-SurvivorManaHandReachMelee0140 $CddaRoot',
    'function Apply-SurvivorManaHandSmash0140',
    'ncmm_primary_mana_hand_smash_weapon',
    'Not enough mana to smash with the primary Mana Hand weapon.',
    'Apply-SurvivorManaHandSmash0140 $CddaRoot',
    'function Apply-SurvivorManaHandAutoattack0140',
    '#include "character_martial_arts.h"',
    'ncmm_primary_mana_hand_autoattack_weapon',
    'ncmm_primary_mana_hand_autoattack_reach',
    'Apply-SurvivorManaHandAutoattack0140 $CddaRoot',
    'function Apply-SurvivorManaHandThrow0140',
    'ncmm_is_mana_hand_throw_item',
    'ncmm_select_mana_hand_throw_item',
    'Apply-SurvivorManaHandThrow0140 $CddaRoot',
    'function Apply-SurvivorManaHandAutoMining0140',
    'ncmm::active_mana_hand_items( you )',
    'Apply-SurvivorManaHandAutoMining0140 $CddaRoot',
    'function Apply-SurvivorManaHandTargetPractice0140',
    'ncmm_target_practice_mana_hand_gun',
    'Apply-SurvivorManaHandTargetPractice0140 $CddaRoot',
    'function Apply-SurvivorManaHandMend0140',
    'ncmm_select_mana_hand_mend_item',
    'Apply-SurvivorManaHandMend0140 $CddaRoot',
    'function Apply-SurvivorManaHandCrutches0140',
    'ncmm_mana_hand_has_crutches',
    'Apply-SurvivorManaHandCrutches0140 $CddaRoot',
    'function Apply-SurvivorManaHandHeldUtilities0140',
    'ncmm_mana_hand_holds_flag',
    'ncmm_mana_hand_holds_item',
    'Apply-SurvivorManaHandHeldUtilities0140 $CddaRoot',
    'function Apply-SurvivorManaHandDirectCount0152',
    'Apply-SurvivorManaHandDirectCount0152 $CddaRoot'
)){
    if(-not $payload.Contains($needle0140)){
        throw ('Survivor 0.14.0 virtual-item payload contract missing: '+$needle0140)
    }
}
if($payload.Contains('item_location::type::mana_hand')){
    throw 'Virtual Mana Hand slots must not introduce a synthetic item_location type.'
}

$autoattackStart0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoattack0140')
$autoattackEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandThrow0140',$autoattackStart0140)
if($autoattackStart0140 -lt 0 -or $autoattackEnd0140 -le $autoattackStart0140){throw 'Mana Hand autoattack transform boundary missing.'}
$autoattackSection0140=$payload.Substring($autoattackStart0140,$autoattackEnd0140-$autoattackStart0140)
if(-not $autoattackSection0140.Contains('#include "character_martial_arts.h"')){throw 'Mana Hand autoattack must include the complete character_martial_arts type.'}
if($autoattackSection0140.Contains('#include "martialarts.h"')){throw 'Mana Hand autoattack uses incomplete martial-arts header.'}

$vehicleXpStart0151=$payload.IndexOf('function Apply-SurvivorVehicleCraftingXp0151')
$vehicleXpEnd0151=$payload.IndexOf('function Apply-SurvivorManaHandSpellcastingAid0140',$vehicleXpStart0151)
if($vehicleXpStart0151 -lt 0 -or $vehicleXpEnd0151 -le $vehicleXpStart0151){
    throw 'Early vehicle Crafting XP transform boundary missing.'
}
$vehicleXpSection0151=$payload.Substring($vehicleXpStart0151,$vehicleXpEnd0151-$vehicleXpStart0151)
foreach($vehicleXpNeedle0151 in @(
    '// NCMM Survivor Crafting XP: successful vehicle install.',
    '// NCMM Survivor Crafting XP: successful vehicle removal.',
    'ncmm::gameplay_metric_record_completed_craft( you );'
)){
    if(-not $vehicleXpSection0151.Contains($vehicleXpNeedle0151)){
        throw ('Early vehicle Crafting XP canonical marker missing: '+$vehicleXpNeedle0151)
    }
}
if($vehicleXpSection0151.Contains('NCMM Survivor vehicle install Crafting XP') -or
   $vehicleXpSection0151.Contains('NCMM Survivor vehicle removal Crafting XP')){
    throw 'Vehicle Crafting XP transforms must share canonical idempotence markers.'
}
foreach($manaAccountingNeedle0140 in @(
    'const int ncmm_virtual_shot_mana_cost = ncmm_planned_shots * 5;',
    'who.magic->mod_mana( who, -( ncmm_fired * 5 ) );'
)){
    if(-not $payload.Contains($manaAccountingNeedle0140)){
        throw ('Mana Hand ranged exact per-shot mana accounting missing: '+$manaAccountingNeedle0140)
    }
}
if($payload.Contains('std::min( 100, ncmm_planned_shots * 5 )') -or
   $payload.Contains('std::min( 100, ncmm_fired * 5 )')){
    throw 'Mana Hand firearm mana cost must not cap long bursts at 100 mana.'
}
foreach($manaVampOwnerNeedle0140 in @(
    'static const Character *ncmm_mana_vamp_owner = nullptr;',
    'static auto ncmm_mana_vamp_owner_id = getID();',
    'ncmm_mana_vamp_owner != this ||',
    'ncmm_mana_vamp_owner_id != ncmm_current_mana_vamp_owner_id'
)){
    if(-not $payload.Contains($manaVampOwnerNeedle0140)){
        throw ('Mana-vamp per-character fractional carry guard missing: '+$manaVampOwnerNeedle0140)
    }
}

$manaAidStart0140=$payload.IndexOf('function Apply-SurvivorManaHandSpellcastingAid0140')
$manaAidEnd0140=$payload.IndexOf('function Apply-SurvivorVirtualItemLifecycle0140',$manaAidStart0140)
if($manaAidStart0140 -lt 0 -or $manaAidEnd0140 -le $manaAidStart0140){throw 'Mana Hand spellcasting-aid hardening section missing.'}
$manaAidSection0140=$payload.Substring($manaAidStart0140,$manaAidEnd0140-$manaAidStart0140)
if($manaAidSection0140.Contains('flag_id( "MAGIC_FOCUS" )')){
    throw 'Mana Hand spellcasting-aid bridge leaked MAGIC_FOCUS back into global wielded semantics.'
}
foreach($aidNeedle0140 in @(
    'ncmm::active_mana_hand_items( *me_chr_const )',
    'candidate->has_flag( flag )'
)){
    if(-not $manaAidSection0140.Contains($aidNeedle0140)){throw ('Mana Hand spellcasting-aid Host enumeration missing: '+$aidNeedle0140)}
}
foreach($aidForbidden0140 in @('gameplay_modifier(', 'virtual_item_for_slot(', '"mana_hand_3"', '"mana_hand_4"', '"mana_hands_34"')){
    if($manaAidSection0140.Contains($aidForbidden0140)){throw ('Mana Hand spellcasting-aid duplicated active-item policy: '+$aidForbidden0140)}
}
foreach($sourcePath0140 in @('src/game.cpp','src/talker_character.cpp','src/item_location.cpp','src/iuse_actor.cpp','src/melee.cpp','src/martialarts.cpp','src/character.cpp','src/character_inventory.cpp','src/activity_actor_definitions.h','src/activity_actor.cpp','src/ranged.cpp','src/avatar_action.cpp','src/weather.cpp','src/item.cpp','src/suffer.cpp','src/item_container.cpp')){
    $sourceEntry0140=@($contracts0140.contracts|Where-Object{$_.id -eq 'magic_virtual_slots.source.v1'}).files|Where-Object{$_.path -eq $sourcePath0140}
    if(@($sourceEntry0140).Count -ne 1){throw ('Mana Hand source contract missing hardening path: '+$sourcePath0140)}
}

$manaLifecycleStart0140=$payload.IndexOf('function Apply-SurvivorVirtualItemLifecycle0140')
$manaUtilityStart0140=$payload.IndexOf('function Apply-SurvivorManaHandUtility0140',$manaLifecycleStart0140)
if($manaLifecycleStart0140 -lt 0 -or $manaUtilityStart0140 -le $manaLifecycleStart0140){
    throw 'Mana Hand lifecycle transform boundary missing.'
}
$manaLifecycleSection0140=$payload.Substring($manaLifecycleStart0140,$manaUtilityStart0140-$manaLifecycleStart0140)
foreach($lifeNeedle0140 in @(
    '$container1831Old0140life',
    '$containerOldNorm0140life = (Normalize-Lf $containerOld0140life).TrimEnd()',
    '$container1831OldNorm0140life = (Normalize-Lf $container1831Old0140life).TrimEnd()',
    'Contains($containerOldNorm0140life)',
    'Contains($container1831OldNorm0140life)',
    'container.remove_items_with(',
    'Mana Hand lifecycle contained-item removal legacy',
    'Mana Hand lifecycle contained-item removal 1831'
)){
    if(-not $manaLifecycleSection0140.Contains($lifeNeedle0140)){
        throw ('Mana Hand lifecycle portability regression contract missing: '+$lifeNeedle0140)
    }
}
$itemLocationContract0140=@($contracts0140.contracts|Where-Object{$_.id -eq 'magic_virtual_slots.source.v1'}).files|Where-Object{$_.path -eq 'src/item_location.cpp'}
if(@($itemLocationContract0140.required) -contains 'container->remove_item( *target() );'){
    throw 'Mana Hand lifecycle source contract regressed to the pre-1831 container-removal implementation detail.'
}

# Multiline clean-source matching used by certified Mana Hand layers must be EOL-safe.
foreach($eolSafeNeedle0140 in @(
    'function Test-TextBlock([string]$Text,[string]$Block)',
    'function Count-TextBlock([string]$Text,[string]$Block)',
    '$attackCount0140 = Count-TextBlock $melee0140 $attackOld0140',
    '$single3Count0140ctxPair = Count-TextBlock $game0140ctx $single3Old0140ctxPair',
    '$primaryMenuCount0140ctx = Count-TextBlock $game0140ctx $primaryMenuAnchor0140ctx',
    '$primaryHandlerCount0140ctx = Count-TextBlock $game0140ctx $primaryHandlerAnchor0140ctx',
    '$rangedMenuCount0140ctx = Count-TextBlock $game0140ctx $rangedMenuAnchor0140ctx',
    '$rangedHandlerCount0140ctx = Count-TextBlock $game0140ctx $rangedHandlerAnchor0140ctx',
    '$avatarEntryCount0140range = Count-TextBlock $avatar0140range $avatarEntryOld0140range',
    '$avatarActivityCount0140range = Count-TextBlock $avatar0140range $avatarActivityOld0140range',
    '$rasSwitchCount0140range = Count-TextBlock $ranged0140range $rasSwitchOld0140range',
    '$fireCount0140reach = Count-TextBlock $handle0140fire $fireOld0140reach',
    '$canReachCount0140final = Count-TextBlock $melee0140 $canReachOld0140final',
    '$reachAttackCount0140final = Count-TextBlock $melee0140 $reachAttackOld0140final',
    '$autoCount0140 = Count-TextBlock $avatar0140auto $autoOld0140',
    '$mineCount0140 = Count-TextBlock $avatar0140mine $mineOld0140',
    'if(-not (Test-TextBlock $game0140pr $pairMenuFlags0140pr))',
    'if(-not (Test-TextBlock $game0140pr $pairHandlerFlags0140pr))',
    "if(-not `$game0140pr.Contains('ncmm::mana_hand_ranged_item_owner( u, oThisItem )'))",
    "if(-not `$actor0140pr.Contains('ncmm::ranged_weapon_binding_valid( get_avatar(), *ncmm_candidate )'))"
)){
    if(-not $payload.Contains($eolSafeNeedle0140)){
        throw ('Mana Hand certified-host EOL-safe matching contract missing: '+$eolSafeNeedle0140)
    }
}

$manaSlotsStart0140=$payload.IndexOf('function Apply-SurvivorVirtualItemSlots0140')
$manaSlotsEnd0140=$payload.IndexOf('function Apply-SurvivorVirtualItemContext0140',$manaSlotsStart0140)
if($manaSlotsStart0140 -lt 0 -or $manaSlotsEnd0140 -le $manaSlotsStart0140){
    throw 'Mana Hand virtual-slot transform boundary missing.'
}
$manaSlotsSection0140=$payload.Substring($manaSlotsStart0140,$manaSlotsEnd0140-$manaSlotsStart0140)
$manaHandsStart0130=$payload.IndexOf('function Apply-SurvivorManaHands0130')
$manaHandsEnd0130=$payload.IndexOf('function Apply-SurvivorVirtualItemSlots0140',$manaHandsStart0130)
if($manaHandsStart0130 -lt 0 -or $manaHandsEnd0130 -le $manaHandsStart0130){
    throw 'Mana Hand 0.13.0 base transform boundary missing.'
}
$manaHandsSection0130=$payload.Substring($manaHandsStart0130,$manaHandsEnd0130-$manaHandsStart0130)
$spellOccupancyStart0140=$manaHandsSection0130.IndexOf('$freeHandNew0130 = @''')
$spellOccupancyEnd0140=$manaHandsSection0130.IndexOf("'@",$spellOccupancyStart0140+24)
if($spellOccupancyStart0140 -lt 0 -or $spellOccupancyEnd0140 -le $spellOccupancyStart0140){
    throw 'Mana Hand base spellcasting occupancy template boundary missing.'
}
$spellOccupancy0140=$manaHandsSection0130.Substring(
    $spellOccupancyStart0140,$spellOccupancyEnd0140-$spellOccupancyStart0140)
foreach($spellOccupancyNeedle0140 in @(
    'ncmm::runtime_hook_modifier(',
    'ncmm::active_mana_hand_items( *this )',
    'ncmm::mana_hand_item_slot_of( *this, *candidate )',
    'ncmm::mana_hand_item_slot::hand3',
    'ncmm::mana_hand_item_slot::hand4',
    'ncmm::mana_hand_item_slot::paired',
    'required_source_hands',
    'ncmm_virtual_hands < required_source_hands',
    'candidate->has_flag( flag_MAGIC_FOCUS )',
    'const int ncmm_virtual_free_hands =',
    'is_armed() && ncmm_virtual_free_hands <= 0 && !ncmm_virtual_focus'
)){
    if(-not $spellOccupancy0140.Contains($spellOccupancyNeedle0140)){
        throw ('Mana Hand final spellcasting occupancy contract missing from base layer: '+$spellOccupancyNeedle0140)
    }
}
foreach($spellOccupancyForbidden0140 in @(
    'virtual_item_for_slot(',
    'item *ncmm_mana_hand_3',
    'item *ncmm_mana_hand_4',
    'item *ncmm_mana_hands_34'
)){
    if($spellOccupancy0140.Contains($spellOccupancyForbidden0140)){
        throw ('Mana Hand spellcasting occupancy duplicated slot policy: '+$spellOccupancyForbidden0140)
    }
}
foreach($obsoleteSpellRewrite0140 in @(
    '$handleOld0140 = @''',
    '$handleNew0140 = @''',
    'Mana-hand occupied/focus semantics',
    'Write-Utf8NoBom $handle0140Path $handle0140'
)){
    if($manaSlotsSection0140.Contains($obsoleteSpellRewrite0140)){
        throw ('VirtualItemSlots still rewrites final spellcasting occupancy: '+$obsoleteSpellRewrite0140)
    }
}

$shieldTemplateStart0140=$manaSlotsSection0140.IndexOf('$bestShieldNew0140 = @''')
$shieldTemplateEnd0140=$manaSlotsSection0140.IndexOf("'@",$shieldTemplateStart0140+24)
if($shieldTemplateStart0140 -lt 0 -or $shieldTemplateEnd0140 -le $shieldTemplateStart0140){
    throw 'Mana Hand final shield-selection template boundary missing.'
}
$shieldTemplate0140=$manaSlotsSection0140.Substring(
    $shieldTemplateStart0140,$shieldTemplateEnd0140-$shieldTemplateStart0140)
foreach($shieldNeedle0140 in @(
    'ncmm::active_mana_hand_items( *this )',
    'melee::blocking_ability( *candidate )',
    'value > best_value',
    'item_location( *this, candidate )'
)){
    if(-not $shieldTemplate0140.Contains($shieldNeedle0140)){
        throw ('Mana Hand final shield-selection contract missing: '+$shieldNeedle0140)
    }
}
foreach($shieldForbidden0140 in @(
    'virtual_item_for_slot(',
    'gameplay_modifier(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"',
    'consider_virtual_shield'
)){
    if($shieldTemplate0140.Contains($shieldForbidden0140)){
        throw ('Mana Hand shield selection duplicated active-item slot policy: '+$shieldForbidden0140)
    }
}

$manaContextStart0140=$payload.IndexOf('function Apply-SurvivorVirtualItemContext0140')
$manaContextEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandSpellcastingAid0140',$manaContextStart0140)
if($manaContextStart0140 -lt 0 -or $manaContextEnd0140 -le $manaContextStart0140){throw 'Mana Hand context transform boundary missing.'}
$manaContextSection0140=$payload.Substring($manaContextStart0140,$manaContextEnd0140-$manaContextStart0140)
$contextOldStart0140=$manaContextSection0140.IndexOf('$switchOld0140ctx = @''')
$contextNewStart0140=$manaContextSection0140.IndexOf('$switchNew0140ctx = @''',$contextOldStart0140)
$contextApplyStart0140=$manaContextSection0140.IndexOf('$game0140ctx = Replace-TextBlock',$contextNewStart0140)
if($contextOldStart0140 -lt 0 -or $contextNewStart0140 -le $contextOldStart0140 -or $contextApplyStart0140 -le $contextNewStart0140){
    throw 'Mana Hand context switch transform structure missing.'
}
$contextOld0140=$manaContextSection0140.Substring($contextOldStart0140,$contextNewStart0140-$contextOldStart0140)
$contextNew0140=$manaContextSection0140.Substring($contextNewStart0140,$contextApplyStart0140-$contextNewStart0140)
foreach($oldNeedle0140 in @('switch( cMenu ) {',"case 'a': {")){
    if(-not $contextOld0140.Contains($oldNeedle0140)){throw ('Mana Hand context old switch anchor missing: '+$oldNeedle0140)}
}
foreach($forbiddenOld0140 in @("case '3':","case '4':","case 'M':")){
    if($contextOld0140.Contains($forbiddenOld0140)){throw ('Mana Hand context old switch anchor already contains injected action: '+$forbiddenOld0140)}
}
foreach($newNeedle0140 in @(
    "case '3':",
    "case '4':",
    "case 'M':",
    'const int ncmm_mana_hands_now = static_cast<int>(',
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::paired',
    'This item is not held by an available Mana Hand.'
)){
    if(-not $contextNew0140.Contains($newNeedle0140)){throw ('Mana Hand context generated switch missing: '+$newNeedle0140)}
}

foreach($directManaNeedle0151 in @(
    'ncmm::mana_hand_inventory_action_visible( locThisItem )',
    'addentry( ''H'', ncmm::localized_text(',
    '"Mana Hand"',
    "case 'H':",
    'ncmm::mana_hand_inventory_action( locThisItem );'
)){
    if(-not $manaContextSection0140.Contains($directManaNeedle0151)){
        throw ('Mana Hand direct inventory UX regression missing: '+$directManaNeedle0151)
    }
}


$directMenuStart0151=$manaContextSection0140.IndexOf('$menuNew0140ctx = @''')
$directMenuEnd0151=$manaContextSection0140.IndexOf("'@",$directMenuStart0151+24)
$directSwitchStart0151=$manaContextSection0140.IndexOf('$switchNew0140ctx = @''')
$directSwitchEnd0151=$manaContextSection0140.IndexOf("'@",$directSwitchStart0151+26)
if($directMenuStart0151 -lt 0 -or $directMenuEnd0151 -le $directMenuStart0151 -or
   $directSwitchStart0151 -lt 0 -or $directSwitchEnd0151 -le $directSwitchStart0151){
    throw 'Mana Hand direct inventory base context template boundary missing.'
}
$directMenu0151=$manaContextSection0140.Substring(
    $directMenuStart0151,$directMenuEnd0151-$directMenuStart0151)
$directSwitch0151=$manaContextSection0140.Substring(
    $directSwitchStart0151,$directSwitchEnd0151-$directSwitchStart0151)
foreach($directMenuNeedle0151 in @(
    'ncmm::mana_hand_inventory_action_visible( locThisItem )',
    'addentry( ''H'', ncmm::localized_text(',
    '"Mana Hand"'
)){
    if(-not $directMenu0151.Contains($directMenuNeedle0151)){
        throw ('Mana Hand direct inventory action missing from base menu: '+$directMenuNeedle0151)
    }
}
foreach($directSwitchNeedle0151 in @(
    "case 'H':",
    'ncmm::mana_hand_inventory_action( locThisItem );',
    "case '3':"
)){
    if(-not $directSwitch0151.Contains($directSwitchNeedle0151)){
        throw ('Mana Hand direct inventory handler missing from base switch: '+$directSwitchNeedle0151)
    }
}
foreach($obsoleteDirectRewrite0151 in @(
    '$manaUxMenuOld0151 = @''',
    '$manaUxMenuNew0151 = @''',
    '$manaUxSwitchOld0151 = @''',
    '$manaUxSwitchNew0151 = @''',
    'Mana Hand direct inventory action',
    'Mana Hand direct inventory handler'
)){
    if($manaContextSection0140.Contains($obsoleteDirectRewrite0151)){
        throw ('Mana Hand context still rewrites final direct inventory UI: '+$obsoleteDirectRewrite0151)
    }
}


$pairCleanMenuStart0140=$manaContextSection0140.IndexOf('$menuNew0140ctx = @''')
$pairCleanMenuEnd0140=$manaContextSection0140.IndexOf("'@",$pairCleanMenuStart0140+22)
$pairCleanSwitchStart0140=$manaContextSection0140.IndexOf('$switchNew0140ctx = @''')
$pairCleanSwitchEnd0140=$manaContextSection0140.IndexOf("'@",$pairCleanSwitchStart0140+24)
if($pairCleanMenuStart0140 -lt 0 -or $pairCleanMenuEnd0140 -le $pairCleanMenuStart0140 -or
   $pairCleanSwitchStart0140 -lt 0 -or $pairCleanSwitchEnd0140 -le $pairCleanSwitchStart0140){
    throw 'Mana Hand paired clean-base context template boundary missing.'
}
$pairCleanMenu0140=$manaContextSection0140.Substring(
    $pairCleanMenuStart0140,$pairCleanMenuEnd0140-$pairCleanMenuStart0140)
$pairCleanSwitch0140=$manaContextSection0140.Substring(
    $pairCleanSwitchStart0140,$pairCleanSwitchEnd0140-$pairCleanSwitchStart0140)
foreach($pairCleanMenuNeedle0140 in @(
    'item *ncmm_mana_pair_item = ncmm_mana_hands >= 2 ?',
    'NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    'ncmm_mana_pair_item == nullptr && ncmm_mana_hands >= 1',
    'ncmm_mana_pair_item == nullptr && ncmm_mana_hands >= 2',
    "addentry( '5'",
    '"survivor_progression", "mana_hands_34"'
)){
    if(-not $pairCleanMenu0140.Contains($pairCleanMenuNeedle0140)){
        throw ('Mana Hand paired clean-base menu missing: '+$pairCleanMenuNeedle0140)
    }
}
foreach($pairCleanSwitchNeedle0140 in @(
    "case '5': {",
    'Release the paired Mana Hands item first.',
    'item *ncmm_pair = ncmm_mana_hands_now >= 2 ?',
    'ncmm_pair == &oThisItem',
    'Both Mana Hands must be free and the item must be two-handed.',
    '"survivor_progression", "mana_hands_34"'
)){
    if(-not $pairCleanSwitch0140.Contains($pairCleanSwitchNeedle0140)){
        throw ('Mana Hand paired clean-base handler missing: '+$pairCleanSwitchNeedle0140)
    }
}
foreach($pairCleanTemplate0140 in @($pairCleanMenu0140,$pairCleanSwitch0140)){
    if($pairCleanTemplate0140.Contains('NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2')){
        throw 'Paired Mana Hand clean-base context must allow firearms.'
    }
}

foreach($pairCompatNeedle0140 in @(
    'Compatibility path: upgrade older 0.14.0 sources',
    'if(-not $game0140ctx.Contains(''ncmm_mana_pair_item''))',
    '$pointerOld0140ctxPair = @''',
    '$pointerNew0140ctxPair = @''',
    '$case5New0140ctxPair = @''',
    'Mana Hand final paired context handler'
)){
    if(-not $manaContextSection0140.Contains($pairCompatNeedle0140)){
        throw ('Mana Hand paired compatibility path missing: '+$pairCompatNeedle0140)
    }
}
$pairCompatMenuStart0140=$manaContextSection0140.IndexOf('$pointerNew0140ctxPair = @''')
$pairCompatMenuEnd0140=$manaContextSection0140.IndexOf("'@",$pairCompatMenuStart0140+28)
$pairCompatHandlerStart0140=$manaContextSection0140.IndexOf('$case5New0140ctxPair = @''')
$pairCompatHandlerEnd0140=$manaContextSection0140.IndexOf("'@",$pairCompatHandlerStart0140+28)
if($pairCompatMenuStart0140 -lt 0 -or $pairCompatMenuEnd0140 -le $pairCompatMenuStart0140 -or
   $pairCompatHandlerStart0140 -lt 0 -or $pairCompatHandlerEnd0140 -le $pairCompatHandlerStart0140){
    throw 'Mana Hand paired compatibility template boundary missing.'
}
$pairCompatMenu0140=$manaContextSection0140.Substring(
    $pairCompatMenuStart0140,$pairCompatMenuEnd0140-$pairCompatMenuStart0140)
$pairCompatHandler0140=$manaContextSection0140.Substring(
    $pairCompatHandlerStart0140,$pairCompatHandlerEnd0140-$pairCompatHandlerStart0140)
foreach($pairCompatTemplate0140 in @($pairCompatMenu0140,$pairCompatHandler0140)){
    if($pairCompatTemplate0140.Contains('NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2')){
        throw 'Paired Mana Hand compatibility path must allow firearms.'
    }
}

$pairContextWritePos0140=$manaContextSection0140.IndexOf('Write-Utf8NoBom $game0140ctxPath $game0140ctx')
if($pairContextWritePos0140 -le $pairCleanMenuStart0140 -or
   $pairContextWritePos0140 -le $pairCleanSwitchStart0140){
    throw 'Mana Hand paired clean-base context must be defined before the base game.cpp write.'
}

$primaryCleanMenuStart0140=$manaContextSection0140.IndexOf('$menuNew0140ctx = @''')
$primaryCleanMenuEnd0140=$manaContextSection0140.IndexOf("'@",$primaryCleanMenuStart0140+22)
$primaryCleanSwitchStart0140=$manaContextSection0140.IndexOf('$switchNew0140ctx = @''')
$primaryCleanSwitchEnd0140=$manaContextSection0140.IndexOf("'@",$primaryCleanSwitchStart0140+24)
if($primaryCleanMenuStart0140 -lt 0 -or $primaryCleanMenuEnd0140 -le $primaryCleanMenuStart0140 -or
   $primaryCleanSwitchStart0140 -lt 0 -or $primaryCleanSwitchEnd0140 -le $primaryCleanSwitchStart0140){
    throw 'Primary Mana Hand clean-base context template boundary missing.'
}
$primaryCleanMenu0140=$manaContextSection0140.Substring(
    $primaryCleanMenuStart0140,$primaryCleanMenuEnd0140-$primaryCleanMenuStart0140)
$primaryCleanSwitch0140=$manaContextSection0140.Substring(
    $primaryCleanSwitchStart0140,$primaryCleanSwitchEnd0140-$primaryCleanSwitchStart0140)
foreach($primaryCleanMenuNeedle0140 in @(
    'const bool ncmm_primary_enabled =',
    'ncmm::virtual_item_primary_melee_enabled( oThisItem )',
    "addentry( 'P'",
    'use as primary Mana Hand melee'
)){
    if(-not $primaryCleanMenu0140.Contains($primaryCleanMenuNeedle0140)){
        throw ('Primary Mana Hand clean-base menu missing: '+$primaryCleanMenuNeedle0140)
    }
}
foreach($primaryCleanSwitchNeedle0140 in @(
    "case 'P': {",
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::virtual_item_set_primary_melee(',
    'Primary Mana Hand melee enabled. Physical wielded weapons keep priority.'
)){
    if(-not $primaryCleanSwitch0140.Contains($primaryCleanSwitchNeedle0140)){
        throw ('Primary Mana Hand clean-base handler missing: '+$primaryCleanSwitchNeedle0140)
    }
}
if(-not $manaContextSection0140.Contains('Compatibility path: upgrade older 0.14.0 sources that already contain Mana Hand')){
    throw 'Primary Mana Hand compatibility path comment/boundary missing.'
}

$rangedCleanMenuStart0140=$manaContextSection0140.IndexOf('$menuNew0140ctx = @''')
$rangedCleanMenuEnd0140=$manaContextSection0140.IndexOf("'@",$rangedCleanMenuStart0140+22)
$rangedCleanSwitchStart0140=$manaContextSection0140.IndexOf('$switchNew0140ctx = @''')
$rangedCleanSwitchEnd0140=$manaContextSection0140.IndexOf("'@",$rangedCleanSwitchStart0140+24)
if($rangedCleanMenuStart0140 -lt 0 -or $rangedCleanMenuEnd0140 -le $rangedCleanMenuStart0140 -or
   $rangedCleanSwitchStart0140 -lt 0 -or $rangedCleanSwitchEnd0140 -le $rangedCleanSwitchStart0140){
    throw 'Mana Hand ranged clean-base context template boundary missing.'
}
$rangedCleanMenu0140=$manaContextSection0140.Substring(
    $rangedCleanMenuStart0140,$rangedCleanMenuEnd0140-$rangedCleanMenuStart0140)
$rangedCleanSwitch0140=$manaContextSection0140.Substring(
    $rangedCleanSwitchStart0140,$rangedCleanSwitchEnd0140-$rangedCleanSwitchStart0140)
foreach($rangedCleanMenuNeedle0140 in @(
    'const bool ncmm_mana_ranged_eligible =',
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )',
    'ncmm::mana_hand_ranged_owner::none',
    "addentry( 'g'",
    'fire with Mana Hand'
)){
    if(-not $rangedCleanMenu0140.Contains($rangedCleanMenuNeedle0140)){
        throw ('Mana Hand ranged clean-base menu missing: '+$rangedCleanMenuNeedle0140)
    }
}
foreach($rangedCleanSwitchNeedle0140 in @(
    "case 'g': {",
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )',
    'ncmm::mana_hand_ranged_owner::none',
    'oThisItem.uses_firing_requirements()',
    'aim_activity_actor::use_item_location( locThisItem )'
)){
    if(-not $rangedCleanSwitch0140.Contains($rangedCleanSwitchNeedle0140)){
        throw ('Mana Hand ranged clean-base handler missing: '+$rangedCleanSwitchNeedle0140)
    }
}
if(-not $manaContextSection0140.Contains('context actions but predate the ranged entry and handler.')){
    throw 'Mana Hand ranged compatibility path comment/boundary missing.'
}

foreach($primaryContextNeedle0140 in @(
    '$primaryMenuAnchor0140ctx = @''',
    '$primaryHandlerAnchor0140ctx = @''',
    '$primaryMenuCount0140ctx = Count-TextBlock $game0140ctx $primaryMenuAnchor0140ctx',
    '$primaryHandlerCount0140ctx = Count-TextBlock $game0140ctx $primaryHandlerAnchor0140ctx',
    'final primary Mana Hand melee menu',
    'final primary Mana Hand melee handler',
    'use as primary Mana Hand melee',
    "case 'P':",
    'ncmm::virtual_item_set_primary_melee(',
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::paired',
    'This item is not held by an available Mana Hand.',
    'This item is not eligible for primary Mana Hand melee.'
)){
    if(-not $manaContextSection0140.Contains($primaryContextNeedle0140)){
        throw ('Primary Mana Hand final context missing from base layer: '+$primaryContextNeedle0140)
    }
}
$primaryContextFinalizePos0140=$manaContextSection0140.IndexOf('$primaryMenuAnchor0140ctx = @''')
if($primaryContextFinalizePos0140 -lt 0 -or $pairContextWritePos0140 -le $primaryContextFinalizePos0140){
    throw 'Primary Mana Hand context must finalize before the base game.cpp write.'
}

foreach($rangedContextNeedle0140 in @(
    '$rangedMenuAnchor0140ctx = @''',
    '$rangedHandlerAnchor0140ctx = @''',
    '$rangedMenuCount0140ctx = Count-TextBlock $game0140ctx $rangedMenuAnchor0140ctx',
    '$rangedHandlerCount0140ctx = Count-TextBlock $game0140ctx $rangedHandlerAnchor0140ctx',
    'final Mana Hand ranged context entry',
    'final Mana Hand ranged context handler',
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )',
    'ncmm::mana_hand_ranged_owner::none',
    'fire with Mana Hand',
    "case 'g':",
    'aim_activity_actor::use_item_location( locThisItem )'
)){
    if(-not $manaContextSection0140.Contains($rangedContextNeedle0140)){
        throw ('Mana Hand final ranged context missing from base layer: '+$rangedContextNeedle0140)
    }
}
$rangedContextFinalizePos0140=$manaContextSection0140.IndexOf('$rangedMenuAnchor0140ctx = @''')
if($rangedContextFinalizePos0140 -lt 0 -or $pairContextWritePos0140 -le $rangedContextFinalizePos0140){
    throw 'Mana Hand ranged context must finalize before the base game.cpp write.'
}


$manaUtilityStart0140=$payload.IndexOf('function Apply-SurvivorManaHandUtility0140')
$manaSecondaryStart0140=$payload.IndexOf('function Apply-SurvivorManaHandSecondaryMelee0140',$manaUtilityStart0140)
if($manaUtilityStart0140 -lt 0 -or $manaSecondaryStart0140 -le $manaUtilityStart0140){throw 'Mana Hand utility/secondary transform boundary missing.'}
$manaUtilitySection0140=$payload.Substring($manaUtilityStart0140,$manaSecondaryStart0140-$manaUtilityStart0140)
if($manaUtilitySection0140.Contains('bool Character::is_wielding')){
    throw 'Mana Hand utility layer must not patch Character::is_wielding semantics.'
}
foreach($utilityNeedle0140 in @(
    'ncmm::active_mana_hand_items( who )',
    'candidate == &it',
    'ncmm_mana_hand_holds_item( p, it )',
    'ncmm_mana_hand_holds_item( *p, it )'
)){
    if(-not $manaUtilitySection0140.Contains($utilityNeedle0140)){
        throw ('Mana Hand utility regression contract missing: '+$utilityNeedle0140)
    }
}
foreach($utilityForbidden0140 in @('gameplay_modifier(', 'virtual_item_for_slot(', '"mana_hand_3"', '"mana_hand_4"', '"mana_hands_34"')){
    if($manaUtilitySection0140.Contains($utilityForbidden0140)){throw ('Mana Hand utility duplicated active-item policy: '+$utilityForbidden0140)}
}

$manaPairStart0140=$payload.IndexOf('function Apply-SurvivorManaHandPairedGrip0140',$manaSecondaryStart0140)
if($manaPairStart0140 -le $manaSecondaryStart0140){throw 'Mana Hand paired-grip transform boundary missing.'}
$manaSecondarySection0140=$payload.Substring($manaSecondaryStart0140,$manaPairStart0140-$manaSecondaryStart0140)
foreach($secondaryNeedle0140 in @(
    'class ncmm_virtual_melee_scope',
    'bool suppress_martial_arts = true',
    'ncmm::virtual_melee_context_begin(',
    'who, weapon, suppress_martial_arts',
    'who_.recalculate_enchantment_cache();',
    'ncmm::active_mana_hand_items( who )',
    'ncmm::mana_hand_item_slot_of( who, *weapon )',
    'ncmm::mana_hand_item_slot::none',
    'ncmm::mana_hand_item_slot::paired',
    'std::clamp( ( who.attack_speed( weapon ) + 9 ) / 10, 5, 50 )',
    '!ncmm::virtual_item_secondary_melee_enabled( *weapon )',
    'weapon->is_gun()',
    'weapon->is_two_handed( who )',
    'who.melee_attack( target, false )',
    'who.magic->mod_mana( who, -mana_cost )',
    'ncmm::virtual_melee_context_item( *this )',
    'ncmm::virtual_melee_context_item( c )',
    'ncmm::virtual_melee_context_is_wielding( *this, target )',
    'ncmm_run_mana_hand_secondary_melee( *this, t );',
    'ncmm::primary_mana_hand_melee_weapon( *this )',
    'ncmm_virtual_melee_scope ncmm_primary_scope(',
    '*this, *ncmm_primary_weapon, false );',
    'Not enough mana to attack with the primary Mana Hand weapon.',
    '$canReachCount0140final = Count-TextBlock $melee0140 $canReachOld0140final',
    '$reachAttackCount0140final = Count-TextBlock $melee0140 $reachAttackOld0140final',
    'Mana Hand final vertical reach selection',
    'Mana Hand final primary reach attack pipeline',
    'item *ncmm_primary_reach_weapon = nullptr;',
    'std::make_unique<ncmm_virtual_melee_scope>',
    'item_location reach_weapon = used_weapon();',
    'handle_melee_wear( reach_weapon );',
    'get_total_melee_stamina_cost( &reach_item )',
    'const bool ncmm_allow_virtual_reach_weapon = ncmm_primary_reach_weapon != nullptr;',
    'ncmm_allow_virtual_reach_weapon, forced_movecost',
    'Not enough mana for a primary Mana Hand reach attack.',
    'NCMM Mana Hand secondary strikes do not trigger martial-art event chains.',
    'ncmm::virtual_melee_context_suppresses_martial_arts( *this ) ? tec_none.obj()'
)){
    if(-not $manaSecondarySection0140.Contains($secondaryNeedle0140)){
        throw ('Mana Hand secondary-melee regression contract missing: '+$secondaryNeedle0140)
    }
}
if($manaSecondarySection0140.Contains('set_wielded_item(') -or
   $manaSecondarySection0140.Contains('u.wield(')){
    throw 'Mana Hand secondary melee must not move the virtual item into Character::weapon.'
}
foreach($secondarySelectorForbidden0140 in @(
    'ncmm_mana_hand_count_for_melee',
    'gameplay_modifier( "mg_virtual_hand_count" )',
    'virtual_item_for_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($manaSecondarySection0140.Contains($secondarySelectorForbidden0140)){
        throw ('Mana Hand secondary strikes duplicated Host active-item/ownership policy: '+$secondarySelectorForbidden0140)
    }
}

if(([regex]::Matches($manaSecondarySection0140,[regex]::Escape('if( !ncmm::virtual_melee_context_suppresses_martial_arts( *this ) ) {'))).Count -lt 4){
    throw 'Mana Hand secondary melee must suppress martial-art event chains only for suppressing virtual scopes.'
}

$secondaryReachOldStart0140=$manaSecondarySection0140.IndexOf('$reachAttackOld0140final = @''')
$secondaryReachOldEnd0140=$manaSecondarySection0140.IndexOf("'@",$secondaryReachOldStart0140+31)
$secondaryReachNewStart0140=$manaSecondarySection0140.IndexOf('$reachAttackNew0140final = @''',$secondaryReachOldEnd0140)
$secondaryReachNewEnd0140=$manaSecondarySection0140.IndexOf("'@",$secondaryReachNewStart0140+31)
$secondaryMartialGuardStart0140=$manaSecondarySection0140.IndexOf('$missRecoveryOld0140 =')
if($secondaryReachOldStart0140 -lt 0 -or $secondaryReachOldEnd0140 -le $secondaryReachOldStart0140 -or
   $secondaryReachNewStart0140 -le $secondaryReachOldEnd0140 -or
   $secondaryReachNewEnd0140 -le $secondaryReachNewStart0140 -or
   $secondaryMartialGuardStart0140 -le $secondaryReachNewEnd0140){
    throw 'Final Mana Hand reach clean-anchor ordering/boundary missing.'
}
$secondaryReachOld0140=$manaSecondarySection0140.Substring(
    $secondaryReachOldStart0140,$secondaryReachOldEnd0140-$secondaryReachOldStart0140)
$secondaryReachNew0140=$manaSecondarySection0140.Substring(
    $secondaryReachNewStart0140,$secondaryReachNewEnd0140-$secondaryReachNewStart0140)
if(-not $secondaryReachOld0140.Contains(
    'const ma_technique miss_recovery = martial_arts_data->get_miss_recovery( *this );')){
    throw 'Final Mana Hand reach old template must anchor on clean vanilla miss recovery.'
}
if($secondaryReachOld0140.Contains('virtual_melee_context_suppresses_martial_arts')){
    throw 'Final Mana Hand reach old template depends on an earlier Mana Hand transform.'
}
if(-not $secondaryReachNew0140.Contains(
    'ncmm::virtual_melee_context_suppresses_martial_arts( *this ) ? tec_none.obj() :')){
    throw 'Final Mana Hand reach new template must preserve martial-art suppression.'
}
if(-not $manaSecondarySection0140.Contains('if($missRecoveryCount0140 -ne 1)')){
    throw 'Mana Hand shared miss-recovery pass must run after reach finalization and match one vanilla site.'
}
foreach($upgradeNeedle0140 in @(
    '$secondaryMenuOld0140ctx = @''',
    '$secondarySwitchOld0140ctx = @''',
    'Mana Hand secondary-melee context menu upgrade',
    'Mana Hand secondary-melee context handler upgrade',
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::paired'
)){
    if(-not $manaContextSection0140.Contains($upgradeNeedle0140)){
        throw ('Mana Hand context update-path regression contract missing: '+$upgradeNeedle0140)
    }
}
$secondaryBaseMenuTemplateStart0140=$manaContextSection0140.IndexOf('$menuNew0140ctx = @''')
$secondaryBaseMenuTemplateEnd0140=$manaContextSection0140.IndexOf("'@",$secondaryBaseMenuTemplateStart0140+22)
$secondaryBaseSwitchTemplateStart0140=$manaContextSection0140.IndexOf('$switchNew0140ctx = @''')
$secondaryBaseSwitchTemplateEnd0140=$manaContextSection0140.IndexOf("'@",$secondaryBaseSwitchTemplateStart0140+24)
if($secondaryBaseMenuTemplateStart0140 -lt 0 -or
   $secondaryBaseMenuTemplateEnd0140 -le $secondaryBaseMenuTemplateStart0140 -or
   $secondaryBaseSwitchTemplateStart0140 -lt 0 -or
   $secondaryBaseSwitchTemplateEnd0140 -le $secondaryBaseSwitchTemplateStart0140){
    throw 'Mana Hand secondary clean-base context template boundary missing.'
}
$secondaryBaseMenu0140=$manaContextSection0140.Substring(
    $secondaryBaseMenuTemplateStart0140,
    $secondaryBaseMenuTemplateEnd0140-$secondaryBaseMenuTemplateStart0140)
$secondaryBaseSwitch0140=$manaContextSection0140.Substring(
    $secondaryBaseSwitchTemplateStart0140,
    $secondaryBaseSwitchTemplateEnd0140-$secondaryBaseSwitchTemplateStart0140)

$secondaryMenuPolicyStart0140=$secondaryBaseMenu0140.IndexOf(
    'const ncmm::mana_hand_item_slot ncmm_secondary_slot =')
$secondaryMenuPolicyEnd0140=$secondaryBaseMenu0140.IndexOf(
    'if( ncmm_secondary_melee_eligible )',$secondaryMenuPolicyStart0140)
if($secondaryMenuPolicyStart0140 -lt 0 -or $secondaryMenuPolicyEnd0140 -le $secondaryMenuPolicyStart0140){
    throw 'Mana Hand secondary clean-base context-menu policy boundary missing.'
}
$secondaryMenuPolicy0140=$secondaryBaseMenu0140.Substring(
    $secondaryMenuPolicyStart0140,$secondaryMenuPolicyEnd0140-$secondaryMenuPolicyStart0140)
foreach($secondaryMenuNeedle0140 in @(
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::none',
    'ncmm::mana_hand_item_slot::paired',
    'ncmm_secondary_bound',
    'ncmm_pair_secondary == oThisItem.is_two_handed( u )'
)){
    if(-not $secondaryMenuPolicy0140.Contains($secondaryMenuNeedle0140)){
        throw ('Mana Hand secondary clean-base menu policy missing: '+$secondaryMenuNeedle0140)
    }
}
foreach($secondaryContextForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    'item *ncmm_bound3',
    'item *ncmm_bound4',
    'item *ncmm_bound_pair'
)){
    if($secondaryMenuPolicy0140.Contains($secondaryContextForbidden0140)){
        throw ('Mana Hand secondary clean-base menu duplicated Host ownership policy: '+$secondaryContextForbidden0140)
    }
}

$secondarySwitchPolicyStart0140=$secondaryBaseSwitch0140.IndexOf("case 'M': {")
$secondarySwitchPolicyEnd0140=$secondaryBaseSwitch0140.IndexOf(
    "case 'a': {",$secondarySwitchPolicyStart0140)
if($secondarySwitchPolicyStart0140 -lt 0 -or $secondarySwitchPolicyEnd0140 -le $secondarySwitchPolicyStart0140){
    throw 'Mana Hand secondary clean-base context-handler policy boundary missing.'
}
$secondarySwitchPolicy0140=$secondaryBaseSwitch0140.Substring(
    $secondarySwitchPolicyStart0140,$secondarySwitchPolicyEnd0140-$secondarySwitchPolicyStart0140)
foreach($secondarySwitchNeedle0140 in @(
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::none',
    'ncmm::mana_hand_item_slot::paired',
    'ncmm_pair_secondary != oThisItem.is_two_handed( u )'
)){
    if(-not $secondarySwitchPolicy0140.Contains($secondarySwitchNeedle0140)){
        throw ('Mana Hand secondary clean-base handler policy missing: '+$secondarySwitchNeedle0140)
    }
}
foreach($secondaryContextForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    'item *ncmm_bound3',
    'item *ncmm_bound4',
    'item *ncmm_bound_pair'
)){
    if($secondarySwitchPolicy0140.Contains($secondaryContextForbidden0140)){
        throw ('Mana Hand secondary clean-base handler duplicated Host ownership policy: '+$secondaryContextForbidden0140)
    }
}

$secondaryCompatMenuStart0140=$manaContextSection0140.IndexOf('$secondaryMenuNew0140ctx = @''')
$secondaryCompatMenuEnd0140=$manaContextSection0140.IndexOf("'@",$secondaryCompatMenuStart0140+31)
$secondaryCompatSwitchStart0140=$manaContextSection0140.IndexOf('$secondarySwitchNew0140ctx = @''')
$secondaryCompatSwitchEnd0140=$manaContextSection0140.IndexOf("'@",$secondaryCompatSwitchStart0140+33)
if($secondaryCompatMenuStart0140 -lt 0 -or $secondaryCompatMenuEnd0140 -le $secondaryCompatMenuStart0140 -or
   $secondaryCompatSwitchStart0140 -lt 0 -or $secondaryCompatSwitchEnd0140 -le $secondaryCompatSwitchStart0140){
    throw 'Mana Hand secondary compatibility upgrade template boundary missing.'
}
$secondaryCompatMenu0140=$manaContextSection0140.Substring(
    $secondaryCompatMenuStart0140,$secondaryCompatMenuEnd0140-$secondaryCompatMenuStart0140)
$secondaryCompatSwitch0140=$manaContextSection0140.Substring(
    $secondaryCompatSwitchStart0140,$secondaryCompatSwitchEnd0140-$secondaryCompatSwitchStart0140)
foreach($compatSecondaryNeedle0140 in @(
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::paired'
)){
    if(-not $secondaryCompatMenu0140.Contains($compatSecondaryNeedle0140) -or
       -not $secondaryCompatSwitch0140.Contains($compatSecondaryNeedle0140)){
        throw ('Mana Hand secondary compatibility policy missing: '+$compatSecondaryNeedle0140)
    }
}

foreach($staleSecondaryContext0140 in @(
    'ncmm_mana_bound_here',
    'ncmm_mana_single_bound_here',
    'ncmm_mana_pair_bound_here'
)){
    if($manaContextSection0140.Contains($staleSecondaryContext0140)){
        throw ('Mana Hand context regressed to stale secondary ownership helper: '+$staleSecondaryContext0140)
    }
}

$manaRangeStart0140=$payload.IndexOf('function Apply-SurvivorManaHandRanged0140',$manaPairStart0140)
if($manaRangeStart0140 -le $manaPairStart0140){throw 'Mana Hand ranged transform boundary missing.'}
$manaPairSection0140=$payload.Substring($manaPairStart0140,$manaRangeStart0140-$manaPairStart0140)
foreach($pairVerifierNeedle0140 in @(
    'Verifying Survivor 0.14.0 Mana Hand paired-grip context...',
    '$game0140pair = [IO.File]::ReadAllText($game0140pairPath)',
    'Survivor 0.14.0 paired-grip final context missing:',
    'Survivor 0.14.0 Mana Hand paired-grip context: VERIFIED',
    "case '5':",
    '"survivor_progression", "mana_hands_34"'
)){
    if(-not $manaPairSection0140.Contains($pairVerifierNeedle0140)){
        throw ('Mana Hand paired-grip verifier contract missing: '+$pairVerifierNeedle0140)
    }
}
foreach($pairMutationForbidden0140 in @(
    'Replace-TextBlock',
    'Write-Utf8NoBom',
    'Normalize-Lf',
    '$pointerOld0140pair',
    '$pointerNew0140pair',
    '$single3Old0140pair',
    '$single4Old0140pair',
    '$menuInsertOld0140pair',
    '$singleHandlerAnchor0140pair',
    '$case5New0140pair'
)){
    if($manaPairSection0140.Contains($pairMutationForbidden0140)){
        throw ('PairedGrip verifier regressed to source mutation: '+$pairMutationForbidden0140)
    }
}
if($manaPairSection0140.Contains('item_location::type::mana_hand') -or
   $manaPairSection0140.Contains('set_wielded_item(') -or
   $manaPairSection0140.Contains('u.wield(')){
    throw 'Mana Hand paired grip verifier must not synthesize locations or move the real item.'
}

$manaPairedRangeStart0140=$payload.IndexOf('function Apply-SurvivorManaHandPairedRanged0140',$manaRangeStart0140)
if($manaPairedRangeStart0140 -le $manaRangeStart0140){throw 'Paired Mana Hand ranged transform boundary missing.'}
$manaRangeEnd0140=$manaPairedRangeStart0140
$manaRangeSection0140=$payload.Substring($manaRangeStart0140,$manaRangeEnd0140-$manaRangeStart0140)
foreach($rangeNeedle0140 in @(
    'item_location ncmm_real_weapon;',
    'static aim_activity_actor use_item_location( const item_location &weapon );',
    'aim_activity_actor aim_activity_actor::use_item_location( const item_location &weapon )',
    'act.ncmm_real_weapon = weapon;',
    '#include "ncmm_loader.h"',
    'ncmm::ranged_weapon_binding_valid( get_avatar(), *ncmm_candidate )',
    'jsout.member( "ncmm_real_weapon", ncmm_real_weapon );',
    'data.read( "ncmm_real_weapon", actor.ncmm_real_weapon );',
    'aim_actor.ncmm_real_weapon = this->ncmm_real_weapon;',
    'item::reload_option opt = get_avatar().select_ammo( ncmm_real_weapon, true );',
    'const int ncmm_virtual_shot_mana_cost = ncmm_planned_shots * 5;',
    'who.magic->mod_mana( who, -( ncmm_fired * 5 ) );',
    'ncmm::mana_hand_ranged_mode_owner( you, gmode ? &*gmode : nullptr )',
    'ncmm::mana_hand_ranged_owner::paired',
    'ncmm_virtual_mana_gun_mode',
    'ncmm_virtual_mana_paired_gun_mode',
    'gmode->has_flag( flag_FIRE_TWOHAND )',
    'gmode->has_flag( flag_RELOAD_AND_SHOOT )',
    'ncmm_mana_hand_ras_switch',
    'item_location gun = activity != nullptr ? activity->get_weapon() : you->get_wielded_item();',
    '$avatar0140rangePath = Join-Path $src0140range ''avatar_action.cpp''',
    '// NCMM action-specific ranged entry point.',
    'const item_location weapon = ncmm::select_ranged_weapon(',
    'ncmm::ranged_weapon_action::fire',
    'aim_activity_actor::use_item_location( weapon )',
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )',
    'fire with Mana Hand',
    "case 'g':",
    'aim_activity_actor::use_item_location( locThisItem )'
)){
    if(-not $manaRangeSection0140.Contains($rangeNeedle0140)){
        throw ('Mana Hand ranged regression contract missing: '+$rangeNeedle0140)
    }
}
if($manaRangeSection0140.Contains('set_wielded_item(') -or
   $manaRangeSection0140.Contains('u.wield(')){
    throw 'Mana Hand ranged support must not move the gun into Character::weapon.'
}
if($manaRangeSection0140.Contains('"mana_hands_34"')){
    throw 'Base Mana Hand ranged layer duplicated paired slot IDs instead of using Host ownership.'
}
if($manaRangeSection0140.Contains('Reload-and-shoot firing modes are not yet supported by Mana Hands.')){
    throw 'Base Mana Hand ranged layer reintroduced the temporary reload-and-shoot rejection.'
}
foreach($rangeAvatarNeedle0140 in @(
    '$avatarEntryOld0140range = @''',
    '$avatarEntryNew0140range = @''',
    '$avatarEntryCount0140range = Count-TextBlock $avatar0140range $avatarEntryOld0140range',
    '$avatarActivityCount0140range = Count-TextBlock $avatar0140range $avatarActivityOld0140range',
    'final Mana Hand ranged avatar entry',
    'final Mana Hand ranged activity entry',
    '// NCMM action-specific ranged entry point.',
    'aim_activity_actor::use_item_location( weapon )'
)){
    if(-not $manaRangeSection0140.Contains($rangeAvatarNeedle0140)){
        throw ('Mana Hand ranged avatar base-layer contract missing: '+$rangeAvatarNeedle0140)
    }
}

foreach($rangeContextVerifierNeedle0140 in @(
    '$game0140range = [IO.File]::ReadAllText($game0140rangePath)',
    'Mana Hand ranged context missing final base-layer boundary:',
    'fire with Mana Hand',
    "case 'g':",
    'aim_activity_actor::use_item_location( locThisItem )',
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )'
)){
    if(-not $manaRangeSection0140.Contains($rangeContextVerifierNeedle0140)){
        throw ('Mana Hand ranged context verifier contract missing: '+$rangeContextVerifierNeedle0140)
    }
}
foreach($rangeGameMutationForbidden0140 in @(
    'Normalize-Lf ([IO.File]::ReadAllText($game0140rangePath))',
    'Replace-TextBlock $game0140range',
    'Write-Utf8NoBom $game0140rangePath',
    '$menuOld0140range',
    '$menuNew0140range',
    '$handlerOld0140range',
    '$handlerNew0140range'
)){
    if($manaRangeSection0140.Contains($rangeGameMutationForbidden0140)){
        throw ('Mana Hand ranged layer regressed to late game.cpp mutation: '+$rangeGameMutationForbidden0140)
    }
}

$manaRasStart0140=$payload.IndexOf('function Apply-SurvivorManaHandReloadAndShoot0140',$manaPairedRangeStart0140)
if($manaRasStart0140 -le $manaPairedRangeStart0140){throw 'Mana Hand reload-and-shoot transform boundary missing.'}
$manaPairedRangeEnd0140=$manaRasStart0140
$manaPairedRangeSection0140=$payload.Substring($manaPairedRangeStart0140,$manaPairedRangeEnd0140-$manaPairedRangeStart0140)
foreach($pairedRangeNeedle0140 in @(
    'Verifying Survivor 0.14.0 paired Mana Hand ranged support...',
    '$pairMenuFlags0140pr = @''',
    '$pairHandlerFlags0140pr = @''',
    'Paired Mana Hand final firearm menu flags missing.',
    'Paired Mana Hand final firearm handler flags missing.',
    'Paired Mana Hand ranged item-owner boundary missing.',
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )',
    'Paired Mana Hand aim Host-resolver boundary missing.',
    'Paired Mana Hand ranged Host-owner boundary missing:',
    'ncmm::mana_hand_ranged_mode_owner( you, gmode ? &*gmode : nullptr )',
    'ncmm::mana_hand_ranged_owner::paired',
    '!ncmm_virtual_mana_paired_gun_mode &&',
    'Survivor 0.14.0 paired Mana Hand ranged support: VERIFIED'
)){
    if(-not $manaPairedRangeSection0140.Contains($pairedRangeNeedle0140)){
        throw ('Paired Mana Hand ranged verifier contract missing: '+$pairedRangeNeedle0140)
    }
}
foreach($pairedRangeMutationForbidden0140 in @(
    'Replace-TextBlock',
    'Write-Utf8NoBom',
    '$pairMenuFlagsOld0140pr',
    '$pairMenuFlagsNew0140pr',
    '$pairHandlerFlagsOld0140pr',
    '$pairHandlerFlagsNew0140pr',
    '.Replace('
)){
    if($manaPairedRangeSection0140.Contains($pairedRangeMutationForbidden0140)){
        throw ('Paired Mana Hand ranged verifier regressed to source mutation: '+$pairedRangeMutationForbidden0140)
    }
}
if($manaPairedRangeSection0140.Contains('set_wielded_item(') -or
   $manaPairedRangeSection0140.Contains('u.wield(') -or
   $manaPairedRangeSection0140.Contains('item_location::type::mana_hand')){
    throw 'Paired Mana Hand ranged verifier must keep the real gun in its vanilla item_location.'
}
foreach($finalRangeNeedle0140 in @(
    '// NCMM paired Mana Hand physical-hand exemptions.',
    'ncmm::mana_hand_ranged_item_owner( u, oThisItem )',
    'ncmm::mana_hand_ranged_mode_owner( you, gmode ? &*gmode : nullptr )',
    'ncmm_ranged_owner == ncmm::mana_hand_ranged_owner::paired'
)){
    if(-not $manaRangeSection0140.Contains($finalRangeNeedle0140)){
        throw ('Base ranged layer must emit final Host-owned paired semantics: '+$finalRangeNeedle0140)
    }
}

$manaFireStart0140=$payload.IndexOf('function Apply-SurvivorManaHandFireAction0140',$manaRasStart0140)
if($manaFireStart0140 -le $manaRasStart0140){throw 'Mana Hand FIRE action transform boundary missing.'}
$manaRasEnd0140=$manaFireStart0140
$manaRasSection0140=$payload.Substring($manaRasStart0140,$manaRasEnd0140-$manaRasStart0140)
foreach($rasNeedle0140 in @(
    'Verifying Survivor 0.14.0 Mana Hand reload-and-shoot support...',
    '$rasOutput0140 = [IO.File]::ReadAllText($ranged0140rasPath)',
    'ncmm_mana_hand_ras_switch',
    'item_location gun = activity != nullptr ? activity->get_weapon() : you->get_wielded_item();',
    'if( !gun ) {',
    'item::reload_option opt = you->select_ammo( gun );',
    'activity->reload_loc = opt.ammo;',
    'Mana Hand reload-and-shoot support: VERIFIED'
)){
    if(-not $manaRasSection0140.Contains($rasNeedle0140)){
        throw ('Mana Hand reload-and-shoot verifier contract missing: '+$rasNeedle0140)
    }
}
foreach($rasMutationForbidden0140 in @(
    'Replace-TextBlock',
    'Write-Utf8NoBom',
    'Normalize-Lf',
    '$switchOld0140ras',
    '$switchNew0140ras',
    '$unsupported0140ras'
)){
    if($manaRasSection0140.Contains($rasMutationForbidden0140)){
        throw ('Mana Hand reload-and-shoot verifier regressed to source mutation: '+$rasMutationForbidden0140)
    }
}
if($manaRasSection0140.Contains('set_wielded_item(') -or
   $manaRasSection0140.Contains('u.wield(')){
    throw 'Mana Hand reload-and-shoot support must not move the real gun into Character::weapon.'
}

$manaGunControlsStart0140=$payload.IndexOf('function Apply-SurvivorManaHandGunControls0140',$manaFireStart0140)
if($manaGunControlsStart0140 -le $manaFireStart0140){throw 'Mana Hand standard gun-control transform boundary missing.'}
$manaFireEnd0140=$manaGunControlsStart0140
$manaFireSection0140=$payload.Substring($manaFireStart0140,$manaFireEnd0140-$manaFireStart0140)
foreach($fireNeedle0140 in @(
    'Mana Hand FIRE include',
    '$reachHelperOld0140fire = @''',
    '$reachHelperNew0140fire = @''',
    '$fireCount0140reach = Count-TextBlock $handle0140fire $fireOld0140reach',
    'final Mana Hand reach target selection',
    'final Mana Hand reach FIRE dispatch',
    'class ncmm_virtual_reach_scope',
    'ncmm_primary_mana_hand_reach_weapon',
    'ncmm_primary_mana_hand_has_reach',
    'return ncmm::primary_mana_hand_melee_weapon( you );',
    'const auto ncmm_fire_candidates =',
    'if( ncmm_fire_candidates.empty() )',
    'const bool ncmm_physical_ranged_ready =',
    'ncmm::ranged_weapon_capable(',
    'if( !ncmm_physical_ranged_ready && !ncmm_fire_candidates.empty() )',
    'ncmm::select_ranged_weapon(',
    'Fire which Mana Hand weapon?',
    'you.has_trait( trait_GUNSHY ) && ncmm_selected_gun->is_firearm()',
    'aim_activity_actor::use_item_location( ncmm_selected_gun )'
)){
    if(-not $manaFireSection0140.Contains($fireNeedle0140)){
        throw ('Mana Hand FIRE action regression contract missing: '+$fireNeedle0140)
    }
}
if($manaFireSection0140.Contains('set_wielded_item(') -or
   $manaFireSection0140.Contains('u.wield(') -or
   $manaFireSection0140.Contains('ncmm_selected_gun.obtain(')){
    throw 'Mana Hand FIRE action must keep the real gun in its vanilla item_location.'
}
if($manaFireSection0140.Contains('ncmm_mana_fire_candidates')){
    throw 'Mana Hand FIRE action must reuse the base-layer ranged candidate gate instead of resolving candidates twice.'
}
if($manaFireSection0140.Contains('before ReachMelee later rewrites')){
    throw 'Mana Hand FIRE base layer still documents a late ReachMelee rewrite dependency.'
}
if($manaFireSection0140.Contains('requires the existing ncmm_loader include.')){
    throw 'Mana Hand FIRE action regressed to an earlier-layer include dependency.'
}
$fireHostStart0140=$manaFireSection0140.IndexOf('$fireNew0140 = @''')
$fireHostEnd0140=$manaFireSection0140.IndexOf("'@",$fireHostStart0140+20)
if($fireHostStart0140 -lt 0 -or $fireHostEnd0140 -le $fireHostStart0140){
    throw 'Mana Hand FIRE Host-selector block missing.'
}
$fireHostBlock0140=$manaFireSection0140.Substring(
    $fireHostStart0140,$fireHostEnd0140-$fireHostStart0140)
foreach($fireHostForbidden0140 in @(
    'runtime_hook_modifier(',
    'virtual_item_for_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($fireHostBlock0140.Contains($fireHostForbidden0140)){
        throw ('Mana Hand FIRE duplicated Host slot policy: '+$fireHostForbidden0140)
    }
}

$manaGunControlsEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandPrimaryMelee0140',$manaGunControlsStart0140)
if($manaGunControlsEnd0140 -le $manaGunControlsStart0140){throw 'Mana Hand standard gun-control transform end missing.'}
$manaGunControlsSection0140=$payload.Substring($manaGunControlsStart0140,$manaGunControlsEnd0140-$manaGunControlsStart0140)
foreach($controlNeedle0140 in @(
    'Mana Hand gun-control include',
    'ncmm_select_mana_hand_gun_control',
    'return ncmm::select_ranged_weapon(',
    'ncmm::ranged_weapon_action::reload',
    'ncmm::ranged_weapon_action::controls',
    'Reload which Mana Hand weapon?',
    'reload( ncmm_reload_gun, false, false );',
    'reload( ncmm_reload_gun, false );',
    'Burst-fire which Mana Hand weapon?',
    'aim_activity_actor::use_item_location( ncmm_burst_gun )',
    'Change firing mode on which Mana Hand weapon?',
    'gun_cycle_mode();',
    'Set default ammo for which Mana Hand weapon?',
    'player_character.select_ammo( *ammo_weapon, false )',
    'function Apply-SurvivorManaHandReloadCarrier0153',
    'Mana Hand reload-in-place include',
    '!ncmm::is_virtual_item( *loc )',
    'Apply-SurvivorManaHandReloadCarrier0153 $CddaRoot'
)){
    if(-not $manaGunControlsSection0140.Contains($controlNeedle0140)){
        throw ('Mana Hand standard gun-control regression contract missing: '+$controlNeedle0140)
    }
}
if($manaGunControlsSection0140.Contains('set_wielded_item(') -or
   $manaGunControlsSection0140.Contains('u.wield(') -or
   $manaGunControlsSection0140.Contains('ncmm_reload_gun.obtain(') -or
   $manaGunControlsSection0140.Contains('ncmm_burst_gun.obtain(')){
    throw 'Mana Hand standard gun controls must keep the real gun in its vanilla item_location.'
}
foreach($includeOrderDependency0140 in @(
    'require the existing ncmm_loader include.',
    'requires the existing ncmm_loader include.'
)){
    if($manaGunControlsSection0140.Contains($includeOrderDependency0140)){
        throw ('Mana Hand action layer regressed to an earlier-layer include dependency: '+$includeOrderDependency0140)
    }
}
$controlHostStart0140=$manaGunControlsSection0140.IndexOf('$helperNew0140ctrl = @''')
$controlHostEnd0140=$manaGunControlsSection0140.IndexOf("'@",$controlHostStart0140+20)
if($controlHostStart0140 -lt 0 -or $controlHostEnd0140 -le $controlHostStart0140){
    throw 'Mana Hand gun-control Host-selector helper missing.'
}
$controlHostBlock0140=$manaGunControlsSection0140.Substring(
    $controlHostStart0140,$controlHostEnd0140-$controlHostStart0140)
foreach($controlHostForbidden0140 in @(
    'runtime_hook_modifier(',
    'virtual_item_for_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($controlHostBlock0140.Contains($controlHostForbidden0140)){
        throw ('Mana Hand gun controls duplicated Host slot policy: '+$controlHostForbidden0140)
    }
}

$pairPickerStart0140=$survivorVirtual0140.IndexOf('Choose two-handed item for Mana Hands III+IV')
if($pairPickerStart0140 -lt 0){throw 'Paired Mana Hand module picker missing.'}
$pairPickerSection0140=$survivorVirtual0140.Substring($pairPickerStart0140,[Math]::Min(1800,$survivorVirtual0140.Length-$pairPickerStart0140))
if($pairPickerSection0140.Contains('NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2')){
    throw 'Paired Mana Hand module picker still rejects firearms.'
}
if(-not $survivorVirtual0140.Contains('including firearms.')){
    throw 'Fourth Mana Hand description does not document paired firearm support.'
}

$manaPrimaryStart0140=$payload.IndexOf('function Apply-SurvivorManaHandPrimaryMelee0140')
$manaMartialStart0140=$payload.IndexOf('function Apply-SurvivorManaHandMartialArts0140',$manaPrimaryStart0140)
$manaReachStart0140=$payload.IndexOf('function Apply-SurvivorManaHandReachMelee0140',$manaMartialStart0140)
if($manaPrimaryStart0140 -lt 0 -or $manaMartialStart0140 -le $manaPrimaryStart0140 -or
   $manaReachStart0140 -le $manaMartialStart0140){
    throw 'Primary Mana Hand martial-arts/reach transform boundary missing.'
}
$manaPrimarySection0140=$payload.Substring($manaPrimaryStart0140,$manaMartialStart0140-$manaPrimaryStart0140)
foreach($primaryVerifierNeedle0140 in @(
    '$game0140pm = [IO.File]::ReadAllText($game0140pmPath)',
    'Primary Mana Hand context missing final base-layer boundary:',
    'use as primary Mana Hand melee',
    "case 'P':",
    'ncmm::virtual_item_set_primary_melee(',
    'ncmm::mana_hand_item_slot_of( u, oThisItem )',
    'ncmm::mana_hand_item_slot::paired'
)){
    if(-not $manaPrimarySection0140.Contains($primaryVerifierNeedle0140)){
        throw ('Primary Mana Hand context verifier contract missing: '+$primaryVerifierNeedle0140)
    }
}
foreach($primaryForbidden0140 in @(
    'gameplay_modifier( "mg_virtual_hand_count" )',
    'virtual_item_for_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"',
    'item *ncmm_bound3',
    'item *ncmm_bound4',
    'item *ncmm_bound_pair'
)){
    if($manaPrimarySection0140.Contains($primaryForbidden0140)){
        throw ('Primary Mana Hand verifier duplicated Host ownership policy: '+$primaryForbidden0140)
    }
}
foreach($primaryGameMutationForbidden0140 in @(
    'Normalize-Lf ([IO.File]::ReadAllText($game0140pmPath))',
    'Replace-TextBlock $game0140pm',
    'Write-Utf8NoBom $game0140pmPath',
    '$menuAnchor0140pm',
    '$menuNew0140pm',
    '$handlerAnchor0140pm',
    '$handlerNew0140pm'
)){
    if($manaPrimarySection0140.Contains($primaryGameMutationForbidden0140)){
        throw ('Primary Mana Hand layer regressed to late game.cpp mutation: '+$primaryGameMutationForbidden0140)
    }
}
foreach($primaryPipelineNeedle0140 in @(
    '$melee0140pm = [IO.File]::ReadAllText($melee0140pmPath)',
    'Primary Mana Hand melee pipeline missing final base-layer boundary:',
    'ncmm_virtual_melee_scope ncmm_primary_scope(',
    '*this, *ncmm_primary_weapon, false );'
)){
    if(-not $manaPrimarySection0140.Contains($primaryPipelineNeedle0140)){
        throw ('Primary Mana Hand final melee verifier contract missing: '+$primaryPipelineNeedle0140)
    }
}
foreach($primaryMeleeMutationForbidden0140 in @(
    '$scopeOld0140pm',
    '$scopeNew0140pm',
    '$wrapperOld0140pm',
    '$wrapperNew0140pm',
    'Replace-TextBlock $melee0140pm',
    '$melee0140pm.Replace(',
    'Write-Utf8NoBom $melee0140pmPath',
    'Normalize-Lf ([IO.File]::ReadAllText($melee0140pmPath))'
)){
    if($manaPrimarySection0140.Contains($primaryMeleeMutationForbidden0140)){
        throw ('Primary Mana Hand layer regressed to late melee.cpp mutation: '+$primaryMeleeMutationForbidden0140)
    }
}
$manaMartialSection0140=$payload.Substring($manaMartialStart0140,$manaReachStart0140-$manaMartialStart0140)
foreach($martialNeedle0140 in @(
    'item_location ncmm_mana_hand_martial_context_weapon( const Character &who )',
    'item_location ncmm_primary_mana_hand_martial_weapon( const Character &who )',
    'ncmm::virtual_melee_context_active( who )',
    'const item_location weapon = ncmm_mana_hand_martial_context_weapon( u );',
    'const bool virtual_scope =',
    'ncmm::primary_mana_hand_melee_weapon( mutable_who )',
    'bool is_armed = weapon || u.is_armed();',
    'ncmm::virtual_melee_context_begin( mutable_owner, *martial_weapon, false )',
    'martial_weapon, owner',
    'bool valid_weapon = ma.weapon_valid( martial_weapon );',
    'const item *weapon = martial_weapon.get_item();'
)){
    if(-not $manaMartialSection0140.Contains($martialNeedle0140)){
        throw ('Mana Hand martial-arts regression contract missing: '+$martialNeedle0140)
    }
}
if($manaMartialSection0140.Contains('set_wielded_item(') -or
   $manaMartialSection0140.Contains('.obtain(')){
    throw 'Mana Hand martial-arts parity must not physically wield or move the virtual weapon.'
}
$requirementsHotStart0140=$manaMartialSection0140.IndexOf('$requirementsNew0140ma = @''')
$requirementsHotEnd0140=$manaMartialSection0140.IndexOf("'@",$requirementsHotStart0140)
if($requirementsHotStart0140 -lt 0 -or $requirementsHotEnd0140 -le $requirementsHotStart0140){
    throw 'Mana Hand martial-arts hot-path contract boundary missing.'
}
$requirementsHot0140=$manaMartialSection0140.Substring($requirementsHotStart0140,$requirementsHotEnd0140-$requirementsHotStart0140)
if($requirementsHot0140.Contains('ncmm_primary_mana_hand_martial_weapon') -or
   $requirementsHot0140.Contains('virtual_item_for_slot') -or
   $requirementsHot0140.Contains('runtime_hook_modifier')){
    throw 'Mana Hand martial-art requirements regressed to inventory-scanning slot resolution.'
}
$manaSmashStart0140=$payload.IndexOf('function Apply-SurvivorManaHandSmash0140',$manaReachStart0140)
if($manaSmashStart0140 -le $manaReachStart0140){throw 'Primary Mana Hand smash transform boundary missing.'}
$manaReachSection0140=$payload.Substring($manaReachStart0140,$manaSmashStart0140-$manaReachStart0140)
foreach($reachNeedle0140 in @(
    '$handle0140reach = [IO.File]::ReadAllText($handle0140reachPath)',
    'Primary Mana Hand reach dispatch missing final FIRE base-layer boundary:',
    'class ncmm_virtual_reach_scope',
    'ncmm::virtual_melee_context_begin( who, weapon, false )',
    'ncmm_primary_mana_hand_reach_weapon',
    'ncmm_primary_mana_hand_has_reach',
    'target_handler::mode_reach(',
    'item_location( you, ncmm_reach_weapon )',
    'return ncmm::primary_mana_hand_melee_weapon( you );',
    'if( ncmm_fire_candidates.empty() )',
    '$melee0140reach = [IO.File]::ReadAllText($melee0140reachPath)',
    'Primary Mana Hand reach pipeline missing final base-layer boundary:',
    'item *ncmm_primary_reach_weapon = nullptr;',
    'std::make_unique<ncmm_virtual_melee_scope>',
    'item_location reach_weapon = used_weapon();',
    'get_total_melee_stamina_cost( &reach_item )',
    'Not enough mana for a primary Mana Hand reach attack.'
)){
    if(-not $manaReachSection0140.Contains($reachNeedle0140)){
        throw ('Mana Hand reach-melee verifier contract missing: '+$reachNeedle0140)
    }
}
if($manaReachSection0140.Contains('set_wielded_item(') -or
   $manaReachSection0140.Contains('u.wield(') -or
   $manaReachSection0140.Contains('.obtain(')){
    throw 'Mana Hand reach melee must not move the real item into Character::weapon.'
}
foreach($reachMutationForbidden0140 in @(
    'Normalize-Lf ([IO.File]::ReadAllText($handle0140reachPath))',
    'Replace-TextBlock $handle0140reach',
    'Write-Utf8NoBom $handle0140reachPath',
    '$reachHelperOld0140 =',
    '$reachHelperNew0140 =',
    '$fireOld0140reach = @''',
    '$fireNew0140reach = @''',
    'Normalize-Lf ([IO.File]::ReadAllText($melee0140reachPath))',
    'Replace-TextBlock $melee0140reach',
    'Write-Utf8NoBom $melee0140reachPath',
    '$canReachOld0140 =',
    '$canReachNew0140 =',
    '$reachAttackOld0140 =',
    '$reachAttackNew0140 ='
)){
    if($manaReachSection0140.Contains($reachMutationForbidden0140)){
        throw ('Mana Hand ReachMelee regressed to late melee.cpp mutation: '+$reachMutationForbidden0140)
    }
}

if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandReloadCarrier0153 -CommandType Function).Definition')){
    throw 'Mana Hand reload-in-place transform missing from mechanics patch revision.'
}

$manaSmashEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoattack0140',$manaSmashStart0140)
if($manaSmashEnd0140 -le $manaSmashStart0140){throw 'Primary Mana Hand smash transform end missing.'}
$manaSmashSection0140=$payload.Substring($manaSmashStart0140,$manaSmashEnd0140-$manaSmashStart0140)
foreach($smashNeedle0140 in @(
    'item_location ncmm_smash_weapon = get_wielded_item();',
    'ncmm::virtual_melee_context_item( *this )',
    'item *ncmm_primary_mana_hand_smash_weapon( avatar &you )',
    'return ncmm::primary_mana_hand_melee_weapon( you );',
    'class ncmm_mana_hand_smash_scope',
    'ncmm::virtual_melee_context_begin( who, *weapon, true )',
    'std::clamp( ( attack_speed( *ncmm_smash_weapon ) + 9 ) / 10, 5, 50 )',
    'Not enough mana to smash with the primary Mana Hand weapon.',
    'if( !has_weapon() && ncmm_smash_weapon == nullptr )',
    'weapon.remove_item();',
    'The magical hand, not either physical hand, absorbed the break.'
)){
    if(-not $manaSmashSection0140.Contains($smashNeedle0140)){
        throw ('Mana Hand smash regression contract missing: '+$smashNeedle0140)
    }
}
if($manaSmashSection0140.Contains('set_wielded_item(') -or
   $manaSmashSection0140.Contains('u.wield(') -or
   $manaSmashSection0140.Contains('.obtain(')){
    throw 'Mana Hand smash must not move the real item into Character::weapon.'
}
if($manaSmashSection0140.Contains('selected_force_unarmed()')){
    throw 'Mana Hand smash must not inherit martial-art force-unarmed semantics.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandSmash0140 -CommandType Function).Definition')){
    throw 'Mana Hand smash transform missing from mechanics patch revision.'
}

$manaAutoStart0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoattack0140',$manaSmashStart0140)
$manaAutoEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandThrow0140',$manaAutoStart0140)
if($manaAutoStart0140 -le $manaSmashStart0140 -or $manaAutoEnd0140 -le $manaAutoStart0140){throw 'Primary Mana Hand autoattack transform boundary missing.'}
$manaAutoSection0140=$payload.Substring($manaAutoStart0140,$manaAutoEnd0140-$manaAutoStart0140)
foreach($autoNeedle0140 in @(
    '#include "character_martial_arts.h"',
    'class ncmm_mana_hand_autoattack_scope',
    'ncmm_primary_mana_hand_autoattack_weapon',
    'ncmm_primary_mana_hand_autoattack_reach',
    'return ncmm::primary_mana_hand_melee_weapon( you );',
    'ncmm::virtual_melee_context_begin( who, weapon, false )',
    'item *ncmm_autoattack_weapon = nullptr;',
    'you.reach_attack( best.pos_bub() );'
)){if(-not $manaAutoSection0140.Contains($autoNeedle0140)){throw ('Mana Hand autoattack regression contract missing: '+$autoNeedle0140)}}
if($manaAutoSection0140.Contains('set_wielded_item(') -or $manaAutoSection0140.Contains('you.wield(') -or $manaAutoSection0140.Contains('.obtain(')){throw 'Mana Hand autoattack must not move the real item into Character::weapon.'}
if($manaAutoSection0140.Contains('you.martial_arts_data->selected_force_unarmed()') -and
   -not $manaAutoSection0140.Contains('#include "character_martial_arts.h"')){
    throw 'Mana Hand autoattack directly uses character_martial_arts without its complete type include.'
}
if($manaAutoSection0140.Contains('#include "martialarts.h"')){
    throw 'Mana Hand autoattack regression contract rejects the incomplete martialarts.h include.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandAutoattack0140 -CommandType Function).Definition')){throw 'Mana Hand autoattack transform missing from mechanics patch revision.'}

foreach($primaryConsumer0140 in @(
    $manaMartialSection0140,
    $manaReachSection0140,
    $manaSmashSection0140,
    $manaAutoSection0140
)){
    foreach($duplicatedSelector0140 in @(
        'ncmm::gameplay_modifier( "mg_virtual_hand_count" )',
        'ncmm::virtual_item_for_slot(',
        '"survivor_progression", "mana_hands_34"',
        '"survivor_progression", slots[i]',
        'ncmm::virtual_item_primary_melee_enabled'
    )){
        if($primaryConsumer0140.Contains($duplicatedSelector0140)){
            throw ('Primary Mana Hand consumer duplicated Host selector policy: '+$duplicatedSelector0140)
        }
    }
}

$manaThrowStart0140=$payload.IndexOf('function Apply-SurvivorManaHandThrow0140',$manaAutoStart0140)
$manaThrowEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoMining0140',$manaThrowStart0140)
if($manaThrowStart0140 -le $manaAutoStart0140 -or $manaThrowEnd0140 -le $manaThrowStart0140){throw 'Mana Hand throw transform boundary missing.'}
$manaThrowSection0140=$payload.Substring($manaThrowStart0140,$manaThrowEnd0140-$manaThrowStart0140)
foreach($throwNeedle0140 in @(
    'ncmm_is_mana_hand_throw_item',
    'ncmm::mana_hand_item_slot_of( you, *candidate )',
    'ncmm_select_mana_hand_throw_item',
    'ncmm::active_mana_hand_items( you )',
    'ncmm::mana_hand_item_label( *candidate )',
    'const bool ncmm_virtual_throw =',
    'if( !in_mech && !ncmm_virtual_throw )',
    'item_location weapon = ( in_mech || ncmm_virtual_throw ) ? loc : you.get_wielded_item();',
    'if( in_mech || ncmm_virtual_throw )',
    'loc.remove_item();',
    'weapon = ncmm_select_mana_hand_throw_item( you );',
    'Throw from which Mana Hand?'
)){if(-not $manaThrowSection0140.Contains($throwNeedle0140)){throw ('Mana Hand throw regression contract missing: '+$throwNeedle0140)}}
if($manaThrowSection0140.Contains('set_wielded_item(') -or
   $manaThrowSection0140.Contains('.obtain(')){
    throw 'Mana Hand throw must not move the virtual item into Character::weapon.'
}
$throwIdentityStart0140=$manaThrowSection0140.IndexOf('bool ncmm_is_mana_hand_throw_item')
$throwIdentityEnd0140=$manaThrowSection0140.IndexOf('item_location ncmm_select_mana_hand_throw_item',$throwIdentityStart0140)
if($throwIdentityStart0140 -lt 0 -or $throwIdentityEnd0140 -le $throwIdentityStart0140){
    throw 'Mana Hand throw identity helper boundary missing.'
}
$throwIdentity0140=$manaThrowSection0140.Substring($throwIdentityStart0140,$throwIdentityEnd0140-$throwIdentityStart0140)
foreach($throwIdentityForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    'virtual_item_matches_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($throwIdentity0140.Contains($throwIdentityForbidden0140)){
        throw ('Mana Hand throw identity duplicated Host slot policy: '+$throwIdentityForbidden0140)
    }
}
$throwPickerStart0140=$manaThrowSection0140.IndexOf('item_location ncmm_select_mana_hand_throw_item')
$throwPickerEnd0140=$manaThrowSection0140.IndexOf('} // namespace',$throwPickerStart0140)
if($throwPickerStart0140 -lt 0 -or $throwPickerEnd0140 -le $throwPickerStart0140){throw 'Mana Hand throw picker boundary missing.'}
$throwPicker0140=$manaThrowSection0140.Substring($throwPickerStart0140,$throwPickerEnd0140-$throwPickerStart0140)
foreach($throwPickerForbidden0140 in @('gameplay_modifier(', 'virtual_item_for_slot(', '"mana_hand_3"', '"mana_hand_4"', '"mana_hands_34"')){
    if($throwPicker0140.Contains($throwPickerForbidden0140)){throw ('Mana Hand throw picker duplicated Host action policy: '+$throwPickerForbidden0140)}
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandThrow0140 -CommandType Function).Definition')){throw 'Mana Hand throw transform missing from mechanics patch revision.'}


$manaMineStart0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoMining0140',$manaThrowStart0140)
$manaMineEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandTargetPractice0140',$manaMineStart0140)
if($manaMineStart0140 -le $manaThrowStart0140 -or $manaMineEnd0140 -le $manaMineStart0140){throw 'Mana Hand auto-mining transform boundary missing.'}
$manaMineSection0140=$payload.Substring($manaMineStart0140,$manaMineEnd0140-$manaMineStart0140)
foreach($mineNeedle0140 in @(
    'ncmm::active_mana_hand_items( you )',
    'candidate->has_flag( flag_DIG_TOOL )',
    'candidate->type->can_use( "PICKAXE" )',
    'weapon = item_location( you, candidate );',
    'item_location weapon = you.get_wielded_item();',
    'if( !weapon &&',
    'm.has_flag( ter_furn_flag::TFLAG_MINEABLE, dest_loc ) &&',
    'g->mostseen == 0 ) {',
    'you.invoke_item( &*weapon, "PICKAXE", dest_loc );'
)){if(-not $manaMineSection0140.Contains($mineNeedle0140)){throw ('Mana Hand auto-mining regression contract missing: '+$mineNeedle0140)}}
foreach($mineForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"',
    'ncmm_mana_hand_auto_mining_tool'
)){
    if($manaMineSection0140.Contains($mineForbidden0140)){
        throw ('Mana Hand auto-mining duplicated Host active-item policy: '+$mineForbidden0140)
    }
}
if($manaMineSection0140.Contains('set_wielded_item(') -or
   $manaMineSection0140.Contains('you.wield(') -or
   $manaMineSection0140.Contains('.obtain(')){
    throw 'Mana Hand auto-mining must not move the real tool into Character::weapon.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandAutoMining0140 -CommandType Function).Definition')){throw 'Mana Hand auto-mining transform missing from mechanics patch revision.'}


$manaPracticeStart0140=$payload.IndexOf('function Apply-SurvivorManaHandTargetPractice0140',$manaMineStart0140)
$manaPracticeEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandMend0140',$manaPracticeStart0140)
if($manaPracticeStart0140 -le $manaMineStart0140 -or $manaPracticeEnd0140 -le $manaPracticeStart0140){throw 'Mana Hand target-practice transform boundary missing.'}
$manaPracticeSection0140=$payload.Substring($manaPracticeStart0140,$manaPracticeEnd0140-$manaPracticeStart0140)
foreach($practiceNeedle0140 in @(
    'ncmm_target_practice_mana_hand_gun',
    'ncmm::mana_hand_item_slot_of( who, *gun )',
    'ncmm::mana_hand_item_slot::none',
    'if( !ncmm_virtual_target_gun && !who.is_wielding( *gun_loc ) )',
    '!ncmm_target_practice_mana_hand_gun( who, gun )',
    'constexpr int ncmm_target_practice_mana_cost = 5;',
    'who.magic->available_mana() < ncmm_target_practice_mana_cost',
    'who.magic->mod_mana( who, -ncmm_target_practice_mana_cost );'
)){if(-not $manaPracticeSection0140.Contains($practiceNeedle0140)){throw ('Mana Hand target-practice regression contract missing: '+$practiceNeedle0140)}}
foreach($practiceIdentityForbidden0140 in @(
    'virtual_item_for_slot(',
    'virtual_item_matches_slot(',
    'gameplay_modifier(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($manaPracticeSection0140.Contains($practiceIdentityForbidden0140)){
        throw ('Mana Hand target practice duplicated Host slot policy: '+$practiceIdentityForbidden0140)
    }
}
if($manaPracticeSection0140.Contains('set_wielded_item(') -or
   $manaPracticeSection0140.Contains('.obtain(')){
    throw 'Mana Hand target practice must not move the real virtual gun into Character::weapon.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandTargetPractice0140 -CommandType Function).Definition')){throw 'Mana Hand target-practice transform missing from mechanics patch revision.'}


$manaMendStart0140=$payload.IndexOf('function Apply-SurvivorManaHandMend0140',$manaPracticeStart0140)
$manaMendEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandCrutches0140',$manaMendStart0140)
if($manaMendStart0140 -le $manaPracticeStart0140 -or $manaMendEnd0140 -le $manaMendStart0140){throw 'Mana Hand mend transform boundary missing.'}
$manaMendSection0140=$payload.Substring($manaMendStart0140,$manaMendEnd0140-$manaMendStart0140)
foreach($mendNeedle0140 in @(
    'ncmm_select_mana_hand_mend_item',
    'ncmm::active_mana_hand_items( you )',
    'ncmm::mana_hand_item_label( *candidate )',
    'Mend which Mana Hand item?',
    'if( you.is_armed() )',
    'loc = you.get_wielded_item();',
    'loc = ncmm_select_mana_hand_mend_item( you, ncmm_had_mend_candidates );',
    'you.mend_item( item_location( loc ) );'
)){if(-not $manaMendSection0140.Contains($mendNeedle0140)){throw ('Mana Hand mend regression contract missing: '+$mendNeedle0140)}}
foreach($mendPickerForbidden0140 in @('gameplay_modifier(', 'virtual_item_for_slot(', '"mana_hand_3"', '"mana_hand_4"', '"mana_hands_34"')){
    if($manaMendSection0140.Contains($mendPickerForbidden0140)){throw ('Mana Hand mend picker duplicated Host action policy: '+$mendPickerForbidden0140)}
}
if($manaMendSection0140.Contains('set_wielded_item(') -or
   $manaMendSection0140.Contains('you.wield(') -or
   $manaMendSection0140.Contains('.obtain(')){
    throw 'Mana Hand mend must not move the real virtual item into Character::weapon.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandMend0140 -CommandType Function).Definition')){throw 'Mana Hand mend transform missing from mechanics patch revision.'}


$manaCrutchStart0140=$payload.IndexOf('function Apply-SurvivorManaHandCrutches0140',$manaMendStart0140)
$manaCrutchEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandHeldUtilities0140',$manaCrutchStart0140)
if($manaCrutchStart0140 -le $manaMendStart0140 -or $manaCrutchEnd0140 -le $manaCrutchStart0140){throw 'Mana Hand crutch transform boundary missing.'}
$manaCrutchSection0140=$payload.Substring($manaCrutchStart0140,$manaCrutchEnd0140-$manaCrutchStart0140)
foreach($crutchNeedle0140 in @(
    'ncmm_mana_hand_has_crutches',
    'ncmm::active_mana_hand_items( who )',
    'ncmm_mana_hand_has_crutches( you )',
    'return ( !enough_working_legs() &&',
    '!weapon.has_flag( flag_CRUTCHES ) &&',
    '!ncmm_mana_hand_has_crutches( *this ) ) ||'
)){if(-not $manaCrutchSection0140.Contains($crutchNeedle0140)){throw ('Mana Hand crutch regression contract missing: '+$crutchNeedle0140)}}
foreach($crutchPolicyForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    'virtual_item_matches_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($manaCrutchSection0140.Contains($crutchPolicyForbidden0140)){
        throw ('Mana Hand crutch support duplicated active-item Host policy: '+$crutchPolicyForbidden0140)
    }
}
if($manaCrutchSection0140.Contains('set_wielded_item(') -or
   $manaCrutchSection0140.Contains('you.wield(') -or
   $manaCrutchSection0140.Contains('.obtain(')){
    throw 'Mana Hand crutch support must not move the real virtual item into Character::weapon.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandCrutches0140 -CommandType Function).Definition')){throw 'Mana Hand crutch transform missing from mechanics patch revision.'}


$manaHeldStart0140=$payload.IndexOf('function Apply-SurvivorManaHandHeldUtilities0140',$manaCrutchStart0140)
$manaHeldEnd0140=$payload.IndexOf('function Apply-SurvivorCraftCompletionMetric0140',$manaHeldStart0140)
if($manaHeldStart0140 -le $manaCrutchStart0140 -or $manaHeldEnd0140 -le $manaHeldStart0140){throw 'Mana Hand held-utility transform boundary missing.'}
$manaHeldSection0140=$payload.Substring($manaHeldStart0140,$manaHeldEnd0140-$manaHeldStart0140)
foreach($heldNeedle0140 in @(
    'ncmm_mana_hand_holds_flag',
    'ncmm_mana_hand_holds_item',
    'amount <= 0 || target.has_trait( trait_FEATHERS ) ||',
    'ncmm_mana_hand_holds_flag( target, json_flag_RAIN_PROTECT )',
    'ncmm_mana_hand_holds_flag( *carrier, flag_RAIN_PROTECT )',
    'ncmm_mana_hand_holds_flag( you, flag_RAIN_PROTECT )',
    'ncmm_mana_hand_holds_item( p, this )',
    'ncmm::mana_hand_item_slot_of( who, *candidate )',
    'ncmm::active_mana_hand_items( who )'
)){if(-not $manaHeldSection0140.Contains($heldNeedle0140)){throw ('Mana Hand held-utility regression contract missing: '+$heldNeedle0140)}}
$rainproofGuardPos0140=$manaHeldSection0140.IndexOf('( !one_in( 50 ) && target.worn_with_flag( json_flag_RAINPROOF ) )')
$virtualUmbrellaPos0140=$manaHeldSection0140.IndexOf('ncmm_mana_hand_holds_flag( target, json_flag_RAIN_PROTECT )')
if($rainproofGuardPos0140 -lt 0 -or $virtualUmbrellaPos0140 -lt 0 -or
   $rainproofGuardPos0140 -gt $virtualUmbrellaPos0140){
    throw 'Mana Hand umbrella lookup must remain after the cheap vanilla RAINPROOF guard.'
}
if($manaHeldSection0140.Contains('wielded_with_flag(')){
    throw 'Mana Hand held utilities must not broaden generic wielded flag semantics.'
}
$heldFlagStart0140=$manaHeldSection0140.IndexOf('static bool ncmm_mana_hand_holds_flag( const Character &who, const flag_id &flag )')
$heldFlagEnd0140=$manaHeldSection0140.IndexOf('$weather0140held = Normalize-Lf',$heldFlagStart0140)
if($heldFlagStart0140 -lt 0 -or $heldFlagEnd0140 -le $heldFlagStart0140){
    throw 'Mana Hand held-flag helper boundary missing.'
}
$heldFlagHelper0140=$manaHeldSection0140.Substring($heldFlagStart0140,$heldFlagEnd0140-$heldFlagStart0140)
foreach($heldPolicyForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    'virtual_item_matches_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($heldFlagHelper0140.Contains($heldPolicyForbidden0140)){
        throw ('Mana Hand held-flag helper duplicated active-item Host policy: '+$heldPolicyForbidden0140)
    }
}
$heldIdentityStart0140=$manaHeldSection0140.IndexOf('static bool ncmm_mana_hand_holds_item( const Character &who, const item *candidate )')
$heldIdentityEnd0140=$manaHeldSection0140.IndexOf('int item::get_remaining_capacity_for_liquid',$heldIdentityStart0140)
if($heldIdentityStart0140 -lt 0 -or $heldIdentityEnd0140 -le $heldIdentityStart0140){
    throw 'Mana Hand held-item identity helper boundary missing.'
}
$heldIdentity0140=$manaHeldSection0140.Substring($heldIdentityStart0140,$heldIdentityEnd0140-$heldIdentityStart0140)
foreach($heldIdentityForbidden0140 in @(
    'gameplay_modifier(',
    'virtual_item_for_slot(',
    'virtual_item_matches_slot(',
    '"mana_hand_3"',
    '"mana_hand_4"',
    '"mana_hands_34"'
)){
    if($heldIdentity0140.Contains($heldIdentityForbidden0140)){
        throw ('Mana Hand held-item identity duplicated Host slot policy: '+$heldIdentityForbidden0140)
    }
}
if($manaHeldSection0140.Contains('set_wielded_item(') -or
   $manaHeldSection0140.Contains('you.wield(') -or
   $manaHeldSection0140.Contains('.obtain(')){
    throw 'Mana Hand held utilities must not move the real virtual item into Character::weapon.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandHeldUtilities0140 -CommandType Function).Definition')){throw 'Mana Hand held-utility transform missing from mechanics patch revision.'}


# Mana Hand direct-count reconciliation contract:
# gameplay/slot ownership and combat selection must read the same aggregate modifier.
$manaDirectStart0152=$payload.IndexOf('function Apply-SurvivorManaHandDirectCount0152',$manaHeldStart0140)
$manaDirectEnd0152=$payload.IndexOf('function Apply-SurvivorActionWeaponSelection0154',$manaDirectStart0152)
if($manaDirectStart0152 -le $manaHeldStart0140 -or $manaDirectEnd0152 -le $manaDirectStart0152){
    throw 'Mana Hand direct-count reconciliation transform boundary missing.'
}
$manaDirectSection0152=$payload.Substring($manaDirectStart0152,$manaDirectEnd0152-$manaDirectStart0152)
foreach($directNeedle0152 in @(
    'ncmm::gameplay_modifier( "mg_virtual_hand_count" )',
    '$legacyPattern0152direct',
    '$verifiedCount0152direct',
    'ncmm::primary_mana_hand_melee_weapon',
    'Legacy source-scoped Mana Hand count survived source generation',
    'Mana Hand direct count: VERIFIED',
    'direct-count verifier deferred during copy-audit source generation.'
)){
    if(-not $manaDirectSection0152.Contains($directNeedle0152)){
        throw ('Mana Hand direct-count verifier contract missing: '+$directNeedle0152)
    }
}
if($manaDirectSection0152.Contains('[regex]::Replace(') -or
   $manaDirectSection0152.Contains('Write-Utf8NoBom')){
    throw 'Mana Hand direct-count verifier must not mutate patched CDDA sources.'
}
if($manaDirectSection0152.Contains("'magic.cpp'")){
    throw 'Mana Hand direct-count verifier must not inspect spell-source selection.'
}
$legacyDirectCall0152='ncmm::runtime_hook_modifier\(\s*"magic\.virtual_hand_count",\s*nullptr,\s*"magiclysm",\s*nullptr,\s*nullptr\s*\)'
if([regex]::IsMatch($payload,$legacyDirectCall0152)){
    throw 'Mana Hand source transforms still emit the legacy source-scoped hand-count call.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandDirectCount0152 -CommandType Function).Definition')){
    throw 'Mana Hand direct-count reconciliation missing from mechanics patch revision.'
}
if(([regex]::Matches($payload,[regex]::Escape('Apply-SurvivorManaHandDirectCount0152 $CddaRoot'))).Count -lt 3){
    throw 'Mana Hand direct-count reconciliation must run during initial transform, deep probe, and normal build.'
}

$actionSelectionStart0154=$manaDirectEnd0152
$actionSelectionEnd0154=$payload.IndexOf('function Apply-SurvivorManaHandsDirectUi0155',$actionSelectionStart0154)
if($actionSelectionStart0154 -lt 0 -or $actionSelectionEnd0154 -le $actionSelectionStart0154){
    throw 'ActionWeaponSelection 0.15.4 payload boundary missing.'
}
$actionSelectionSection0154=$payload.Substring(
    $actionSelectionStart0154,$actionSelectionEnd0154-$actionSelectionStart0154)
foreach($actionSelectionNeedle0154 in @(
    'Verifying action-specific Mana Hand ranged selection...',
    '$avatar = [IO.File]::ReadAllText($avatarPath)',
    '$actor = [IO.File]::ReadAllText($actorPath)',
    'const auto ncmm_fire_candidates =',
    'if( ncmm_fire_candidates.empty() )',
    'Physical reach intercepted F before base-layer ranged capability resolution.',
    '// NCMM action-specific ranged entry point.',
    'const item_location weapon = ncmm::select_ranged_weapon(',
    'aim_activity_actor::use_item_location( weapon )',
    'Action-specific Mana Hand ranged selection: VERIFIED'
)){
    if(-not $actionSelectionSection0154.Contains($actionSelectionNeedle0154)){
        throw ('ActionWeaponSelection final-boundary verifier contract missing: '+$actionSelectionNeedle0154)
    }
}
foreach($actionSelectionForbidden0154 in @(
    'Replace-TextBlock',
    'Write-Utf8NoBom',
    'Normalize-Lf',
    '$old = @''',
    '$new = @''',
    'Write-Utf8NoBom $handlePath',
    '$handle = $handle.Substring(',
    '$reach = $handle.Substring(',
    '$prefix = @'''
)){
    if($actionSelectionSection0154.Contains($actionSelectionForbidden0154)){
        throw ('ActionWeaponSelection regressed to late source mutation: '+$actionSelectionForbidden0154)
    }
}

$craftMetricStart0140=$payload.IndexOf('function Apply-SurvivorCraftCompletionMetric0140',$manaHeldStart0140)
$craftMetricEnd0140=$payload.IndexOf('function Apply-SurvivorXpBalance0140',$craftMetricStart0140)
if($craftMetricStart0140 -le $manaHeldStart0140 -or $craftMetricEnd0140 -le $craftMetricStart0140){throw 'Survivor craft-completion metric transform boundary missing.'}
$craftMetricSection0140=$payload.Substring($craftMetricStart0140,$craftMetricEnd0140-$craftMetricStart0140)
foreach($craftMetricNeedle0140 in @(
    'void Character::complete_craft( item &craft, const std::optional<tripoint_bub_ms> &loc )',
    'eoc->activate_activation_only( d, "a recipe", "crafting", "recipe" );',
    'ncmm::gameplay_metric_record_completed_craft( *this );'
)){if(-not $craftMetricSection0140.Contains($craftMetricNeedle0140)){throw ('Survivor exact craft-completion metric regression missing: '+$craftMetricNeedle0140)}}
if(-not $payload.Contains('(Get-Command Apply-SurvivorCraftCompletionMetric0140 -CommandType Function).Definition')){throw 'Craft-completion metric transform missing from mechanics patch revision.'}
if(([regex]::Matches($payload,[regex]::Escape('Apply-SurvivorCraftCompletionMetric0140 $CddaRoot'))).Count -ne 2){throw 'Craft-completion metric must be applied in both deep-probe and normal build paths.'}

$vehicleMetricStart0151=$payload.IndexOf('function Apply-SurvivorVehicleCraftingMetric0151',$craftMetricStart0140)
if($vehicleMetricStart0151 -le $craftMetricStart0140){throw 'Vehicle-work Crafting XP transform missing.'}
$vehicleMetricEnd0151=$payload.IndexOf('function Apply-SurvivorXpBalance0140',$vehicleMetricStart0151)
if($vehicleMetricEnd0151 -le $vehicleMetricStart0151){throw 'Vehicle-work Crafting XP transform boundary missing.'}
$vehicleMetricSection0151=$payload.Substring($vehicleMetricStart0151,$vehicleMetricEnd0151-$vehicleMetricStart0151)
foreach($vehicleNeedle0151 in @(
    'src'') ''activity_actor.cpp''',
    '// NCMM Survivor Crafting XP: successful vehicle install.',
    '// NCMM Survivor Crafting XP: successful vehicle removal.',
    'ncmm::gameplay_metric_record_completed_craft( you );'
)){
    if(-not $vehicleMetricSection0151.Contains($vehicleNeedle0151)){
        throw ('Vehicle-work Crafting XP regression missing: '+$vehicleNeedle0151)
    }
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorVehicleCraftingMetric0151 -CommandType Function).Definition')){throw 'Vehicle-work Crafting XP transform missing from mechanics patch revision.'}
if(([regex]::Matches($payload,[regex]::Escape('Apply-SurvivorVehicleCraftingMetric0151 $CddaRoot'))).Count -ne 2){throw 'Vehicle-work Crafting XP must be applied in both deep-probe and normal build paths.'}

$xpBalanceStart0140=$payload.IndexOf('function Apply-SurvivorXpBalance0140',$craftMetricStart0140)
$xpBalanceEnd0140=$payload.IndexOf('# Keep the patch-revision contract aware',$xpBalanceStart0140)
if($xpBalanceStart0140 -le $craftMetricStart0140 -or $xpBalanceEnd0140 -le $xpBalanceStart0140){throw 'Survivor XP-balance transform boundary missing.'}
$xpBalanceSection0140=$payload.Substring($xpBalanceStart0140,$xpBalanceEnd0140-$xpBalanceStart0140)
foreach($xpBalanceNeedle0140 in @(
    'case branch_id::survival: return 200;',
    'case branch_id::mobility: return 115;',
    'case branch_id::scavenging: return 80;',
    '"balance_fraction"',
    'const int64_t adjusted = anti_farm_adjust( branch, raw_gained );',
    'const int64_t gained = apply_branch_xp_balance( branch, adjusted );',
    'void gameplay_metric_record_completed_craft( const Character &who )',
    '++gameplay_metric_values["crafting.completed"];',
    'Host 0.8.2 canonical sync runs before this Survivor balance pass.',
    'Survivor XP balance canonical Host header missing:',
    'Survivor XP balance canonical Host source missing:',
    'Ambiguous canceled activity craft metric path survived canonical Host sync.'
)){if(-not $xpBalanceSection0140.Contains($xpBalanceNeedle0140)){throw ('Survivor XP-balance regression missing: '+$xpBalanceNeedle0140)}}
foreach($xpHostMutationForbidden0140 in @(
    '$hostHeaderBalance = Replace-TextBlock',
    '$hostLoaderBalance = Replace-TextBlock',
    'Write-Utf8NoBom $hostHeaderBalancePath',
    'Write-Utf8NoBom $hostLoaderBalancePath',
    '$hostLoaderBalance = $hostLoaderBalance.Remove('
)){
    if($xpBalanceSection0140.Contains($xpHostMutationForbidden0140)){
        throw ('Survivor XP balance regressed to Host source mutation: '+$xpHostMutationForbidden0140)
    }
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorXpBalance0140 -CommandType Function).Definition')){throw 'XP-balance transform missing from mechanics patch revision.'}
if(([regex]::Matches($payload,[regex]::Escape('Apply-SurvivorXpBalance0140'))).Count -lt 2){throw 'XP-balance transform is not applied after canonical module sync.'}
$canonicalHostSyncCall0140=$payload.LastIndexOf('Apply-NcmmBallisticHost082CanonicalSync')
$xpBalanceApplyCall0140=$payload.LastIndexOf('Apply-SurvivorXpBalance0140')
if($canonicalHostSyncCall0140 -lt 0 -or $xpBalanceApplyCall0140 -le $canonicalHostSyncCall0140){
    throw 'XP-balance Host verifier must run after canonical Host synchronization.'
}

# Balance hotfix: passive movement remains a valid Mobility source, but its base rate
# is intentionally half of the original 1 XP / 150 movement events.
$survivorSource=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\src\survivor_progression.cpp'))
foreach($balanceNeedle in @(
    'constexpr int64_t mobility_steps_per_xp = 300;',
    'steps / mobility_steps_per_xp',
    'steps % mobility_steps_per_xp'
)){if(-not $survivorSource.Contains($balanceNeedle)){throw ('Survivor Mobility XP nerf contract missing: '+$balanceNeedle)}}
if($survivorSource.Contains('steps / 150') -or $survivorSource.Contains('steps % 150')){
    throw 'Stale Survivor Mobility XP 1/150 rate returned.'
}

foreach($balanceNeedle0140 in @(
    'int branch_xp_balance_pct( branch_id branch )',
    'case branch_id::survival: return 200;',
    'case branch_id::mobility: return 115;',
    'case branch_id::scavenging: return 80;',
    '"balance_fraction"',
    'const int64_t adjusted = anti_farm_adjust( branch, raw_gained );',
    'const int64_t gained = apply_branch_xp_balance( branch, adjusted );'
)){if(-not $survivorSource.Contains($balanceNeedle0140)){throw ('Survivor long-run XP balance contract missing: '+$balanceNeedle0140)}}
if($survivorSource.Contains('const int64_t gained = anti_farm_adjust( branch, raw_gained );')){
    throw 'Stale pre-balance branch XP award path returned.'
}
$craftContract0140=@($contracts0100.contracts|Where-Object{$_.id -eq 'reactive_technical_hooks.source.v4'}).files|Where-Object{$_.path -eq 'src/crafting.cpp'}
foreach($craftContractNeedle0140 in @(
    'void Character::complete_craft( item &craft, const std::optional<tripoint_bub_ms> &loc )',
    'eoc->activate_activation_only( d, "a recipe", "crafting", "recipe" );'
)){if(-not @($craftContract0140.required) -contains $craftContractNeedle0140){throw ('Craft completion source contract missing: '+$craftContractNeedle0140)}}

# Certification/source-cache hardening: a cached archive is not trusted merely
# because options.cpp and the VS solution exist.  Mana Hands and other host
# transforms require the full touched-source sentinel set, and an incomplete
# cached ZIP gets one forced redownload before certification fails.
foreach($sourceCacheNeedle in @(
    'function Get-MissingCddaSourceSentinels([string]$Root)',
    "'src\magic.cpp'",
    "'src\handle_action.cpp'",
    'Incomplete CDDA source cache detected; invalidating cached ZIP and retrying exact download once...',
    'Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue',
    'Fresh exact CDDA pristine cache failed contamination/source-completeness validation after retry.',
    'CDDA working source incomplete after pristine sync:'
)){
    if(-not $payload.Contains($sourceCacheNeedle)){
        throw ('CDDA source-cache self-healing regression missing: '+$sourceCacheNeedle)
    }
}
if(([regex]::Matches($payload,[regex]::Escape('Get-MissingCddaSourceSentinels'))).Count -lt 3){
    throw 'CDDA source-completeness helper is not wired into pristine validation and working-tree validation.'
}


# Mana Hands discoverability: the integrated control item is user-facing and opens
# only the dedicated Mana Hands hub through its exact itype id.
$manaCarrier0155=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\persistent_data\mana_hand_carrier.json'))
$survivorSource0155=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\src\survivor_progression.cpp'))
foreach($needle0155 in @(
    '"id": "ncmm_survivor_mana_hand_carrier"',
    '"str_sp": "Mana Hands"',
    'Activate it to manage Mana Hand III, Mana Hand IV and the paired III+IV grip.',
    '"use_action": {',
    '"type": "effect_on_conditions"',
    '"menu_text": "Manage Mana Hands"',
    '"effect_on_conditions": []'
)){
    if(-not $manaCarrier0155.Contains($needle0155)){throw ('Mana Hands carrier UX contract missing: '+$needle0155)}
}
if($manaCarrier0155.Contains('"str_sp": "mana hand anchor"')){
    throw 'Technical mana hand anchor name leaked back into the player UI.'
}
foreach($needle0155 in @(
    'void show_mana_hands_menu()',
    '"Mana Hands\nChoose a hand to equip, replace or release its held item."',
    '"Mana Hand III',
    '"Mana Hand IV',
    'extern "C" NCMM_EXPORT int ncmm_on_item_activate_v1',
    'std::string_view( item_type_id ) != "ncmm_survivor_mana_hand_carrier"',
    'show_mana_hands_menu();'
)){
    if(-not $survivorSource0155.Contains($needle0155)){throw ('Mana Hands dedicated UI contract missing: '+$needle0155)}
}
foreach($needle0155 in @(
    'function Apply-SurvivorManaHandsDirectUi0155',
    'Apply-SurvivorManaHandsDirectUi0155 $CddaRoot',
    'method.empty() && ncmm::handle_item_activation( loc )'
)){
    if(-not $payload.Contains($needle0155)){throw ('Mana Hands activation payload contract missing: '+$needle0155)}
}

# Architecture guard: Mana Hands transforms may patch vanilla source, but
# generated-code anchors are forbidden unless they are an explicitly retained
# VirtualItemContext0140 compatibility-upgrade boundary.
$generatedManaMarkers0165=@(
    'ncmm::',
    'ncmm_',
    'mana_hand',
    'virtual_item'
)

$manaTransformMatches0165=[regex]::Matches(
    $payload,
    '(?ms)^function (Apply-SurvivorManaHand[A-Za-z0-9_]+)\([^\r\n]*\) \{(.*?)(?=^function |\z)'
)
foreach($manaTransformMatch0165 in $manaTransformMatches0165){
    $manaTransformName0165=$manaTransformMatch0165.Groups[1].Value
    $manaTransformBody0165=$manaTransformMatch0165.Groups[2].Value
    $anchorTemplateMatches0165=[regex]::Matches(
        $manaTransformBody0165,
        "(?ms)\x24([A-Za-z0-9_]*(?:Old|Anchor)[A-Za-z0-9_]*)\s*=\s*@'\r?\n(.*?)\r?\n'@"
    )
    foreach($anchorTemplateMatch0165 in $anchorTemplateMatches0165){
        $anchorTemplateName0165=$anchorTemplateMatch0165.Groups[1].Value
        $anchorTemplateBody0165=$anchorTemplateMatch0165.Groups[2].Value
        foreach($generatedMarker0165 in $generatedManaMarkers0165){
            if($anchorTemplateBody0165.Contains($generatedMarker0165)){
                throw (
                    'Mana Hands transform-on-transform anchor returned in '+
                    $manaTransformName0165+'/'+$anchorTemplateName0165+': '+
                    $generatedMarker0165
                )
            }
        }
    }
}
if($manaTransformMatches0165.Count -lt 10){
    throw 'Mana Hands cross-transform architecture guard did not inspect the expected transform set.'
}

$virtualContextArchitectureMatch0165=[regex]::Match(
    $payload,
    '(?ms)^function Apply-SurvivorVirtualItemContext0140\([^\r\n]*\) \{(.*?)(?=^function |\z)'
)
if(-not $virtualContextArchitectureMatch0165.Success){
    throw 'VirtualItemContext0140 architecture-guard boundary missing.'
}
$virtualContextArchitectureBody0165=$virtualContextArchitectureMatch0165.Groups[1].Value
$virtualContextAllowedGeneratedAnchors0165=@(
    'secondaryMenuOld0140ctx',
    'secondarySwitchOld0140ctx',
    'pointerOld0140ctxPair',
    'single3Old0140ctxPair',
    'single4Old0140ctxPair',
    'menuInsertOld0140ctxPair',
    'singleHandlerAnchor0140ctxPair',
    'primaryMenuAnchor0140ctx',
    'rangedMenuAnchor0140ctx'
)
$virtualContextGeneratedAnchors0165=@()
$virtualContextTemplateMatches0165=[regex]::Matches(
    $virtualContextArchitectureBody0165,
    "(?ms)\x24([A-Za-z0-9_]*(?:Old|Anchor)[A-Za-z0-9_]*)\s*=\s*@'\r?\n(.*?)\r?\n'@"
)
foreach($virtualContextTemplateMatch0165 in $virtualContextTemplateMatches0165){
    $virtualContextTemplateName0165=$virtualContextTemplateMatch0165.Groups[1].Value
    $virtualContextTemplateBody0165=$virtualContextTemplateMatch0165.Groups[2].Value
    $virtualContextGenerated0165=$false
    foreach($generatedMarker0165 in $generatedManaMarkers0165){
        if($virtualContextTemplateBody0165.Contains($generatedMarker0165)){
            $virtualContextGenerated0165=$true
            break
        }
    }
    if(-not $virtualContextGenerated0165){
        continue
    }
    if($virtualContextAllowedGeneratedAnchors0165 -notcontains $virtualContextTemplateName0165){
        throw (
            'VirtualItemContext0140 gained an unapproved generated-code anchor: '+
            $virtualContextTemplateName0165
        )
    }
    $virtualContextGeneratedAnchors0165 += $virtualContextTemplateName0165
}
foreach($allowedContextAnchor0165 in $virtualContextAllowedGeneratedAnchors0165){
    if($virtualContextGeneratedAnchors0165 -notcontains $allowedContextAnchor0165){
        throw (
            'VirtualItemContext0140 expected compatibility anchor missing: '+
            $allowedContextAnchor0165
        )
    }
}
if($virtualContextGeneratedAnchors0165.Count -ne $virtualContextAllowedGeneratedAnchors0165.Count){
    throw 'VirtualItemContext0140 generated-anchor allowlist count drifted.'
}

# Architecture guard: Survivor module transforms may inspect canonical Host
# sources, but Host ownership stays with the NCMM infrastructure layer.  Track
# host_patch path variables inside every Apply-Survivor* function and reject
# writes through those variables or direct host_patch write expressions.
$survivorTransformMatches0166=[regex]::Matches(
    $payload,
    '(?ms)^function (Apply-Survivor[A-Za-z0-9_]+)(?:\([^\r\n]*\))?\s*\{(.*?)(?=^function |\z)'
)
$survivorHostReaderCount0166=0
foreach($survivorTransformMatch0166 in $survivorTransformMatches0166){
    $survivorTransformName0166=$survivorTransformMatch0166.Groups[1].Value
    $survivorTransformBody0166=$survivorTransformMatch0166.Groups[2].Value

    $hostPathBindings0166=[regex]::Matches(
        $survivorTransformBody0166,
        "(?m)\x24([A-Za-z0-9_]+)\s*=\s*Join-Path\s+\x24NcmmRoot\s+'host_patch\\[^']+'"
    )
    foreach($hostPathBinding0166 in $hostPathBindings0166){
        $survivorHostReaderCount0166++
        $hostPathVariable0166=$hostPathBinding0166.Groups[1].Value
        $escapedHostPathVariable0166=[regex]::Escape('$'+$hostPathVariable0166)
        foreach($hostWritePattern0166 in @(
            '(?m)Write-Utf8NoBom\s+'+$escapedHostPathVariable0166+'(?:\s|$)',
            '(?m)\[IO\.File\]::(?:WriteAllText|WriteAllBytes|AppendAllText)\(\s*'+$escapedHostPathVariable0166,
            '(?m)(?:Set-Content|Add-Content|Out-File)[^\r\n]*'+$escapedHostPathVariable0166
        )){
            if([regex]::IsMatch($survivorTransformBody0166,$hostWritePattern0166)){
                throw (
                    'Survivor transform attempted to mutate canonical Host path: '+
                    $survivorTransformName0166+' / $'+$hostPathVariable0166
                )
            }
        }
    }

    foreach($directHostWritePattern0166 in @(
        "(?m)Write-Utf8NoBom\s+\(Join-Path\s+\x24NcmmRoot\s+'host_patch\\",
        "(?m)\[IO\.File\]::(?:WriteAllText|WriteAllBytes|AppendAllText)\(\s*\(Join-Path\s+\x24NcmmRoot\s+'host_patch\\",
        "(?m)(?:Set-Content|Add-Content|Out-File)[^\r\n]*Join-Path\s+\x24NcmmRoot\s+'host_patch\\"
    )){
        if([regex]::IsMatch($survivorTransformBody0166,$directHostWritePattern0166)){
            throw ('Survivor transform contains direct canonical Host write: '+$survivorTransformName0166)
        }
    }
}
if($survivorTransformMatches0166.Count -lt 35){
    throw 'Survivor Host-ownership guard did not inspect the expected transform set.'
}
if($survivorHostReaderCount0166 -lt 1){
    throw 'Survivor Host-ownership guard no longer exercises the canonical Host read-only verifier path.'
}

Write-Host 'NCMM Survivor payload regression contract: PASS' -ForegroundColor Green
& (Join-Path $PSScriptRoot 'Test-ManaActionWeaponContracts.ps1') -PackageRoot $PackageRoot
