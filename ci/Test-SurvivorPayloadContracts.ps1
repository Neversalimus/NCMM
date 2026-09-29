param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'))

# Survivor Progression 0.10.0 Mechanical Perks regression contracts.
# Historical gameplay/content contracts stay pinned here; build-cache marker/fingerprint are version-current and are checked by the 0.12.0 block below.
$survivor0100=Get-Content (Join-Path $PackageRoot 'components\survivor_progression.json') -Raw|ConvertFrom-Json
if([string]$survivor0100.version -ne '0.12.0'){throw 'Survivor 0.12.0 component identity mismatch.'}
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
if([string]$survivor0110.version -ne '0.12.0'){throw 'Survivor 0.12.0 component identity mismatch.'}
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
    '.survivor_0120_manager_settings_build.sha256',
    'v8.7.6.8-survivor-0.12.0-manager-settings',
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

# HOTFIX13: chargen lifecycle isolation + physical tree-row collision regression.
foreach($lifecycleNeedle013 in @(
    'HOTFIX13 chargen-safe character state availability',
    'return g != nullptr && !g->new_game && world_generator != nullptr &&',
    'if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;',
    'HOTFIX13 tree row physical deconfliction',
    'std::map<int, std::vector<size_t>> ncmm_row_nodes;',
    'std::stable_sort( row_nodes.begin(), row_nodes.end()',
    'next_x2 += 2;'
)) { if(-not $payload.Contains($lifecycleNeedle013)){ throw ('HOTFIX13 lifecycle/layout contract missing: '+$lifecycleNeedle013) } }
if(([regex]::Matches($payload,[regex]::Escape('if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;'))).Count -lt 2){
    throw 'HOTFIX13 runtime-hook lifecycle gate must cover both generic and Creature hook paths.'
}
foreach($stateTransformNeedle013 in @(
    '$characterStateOld013 = @''',
    '$characterStateNew013 = @''',
    "Replace-TextBlock `$loader20 `$characterStateOld013 `$characterStateNew013 'HOTFIX13 chargen-safe character state availability'"
)){if(-not $payload.Contains($stateTransformNeedle013)){throw ('HOTFIX13 character-state transform contract missing: '+$stateTransformNeedle013)}}
foreach($generatedAudit013 in @(
    'HOTFIX13 generated Host character-state lifecycle guard missing.',
    'HOTFIX13 generated Host retained chargen-unsafe character-state predicate.',
    'HOTFIX13 generated Host runtime-hook lifecycle gates expected 2.',
    'HOTFIX13 generated tree-layout audit missing:'
)){if(-not $payload.Contains($generatedAudit013)){throw ('HOTFIX13 generated-source audit contract missing: '+$generatedAudit013)}}

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
    'progression_stat_power_pct',
    'ncmm_on_locale_changed_v1',
    'settings.typed.v2'
)){if(-not $payload.Contains($n)){throw ('Survivor 0.12.0 settings contract missing: '+$n)}}
Write-Host 'NCMM Survivor payload regression contract: PASS' -ForegroundColor Green
