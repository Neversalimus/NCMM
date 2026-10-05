param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
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
foreach($payloadNeedle0150 in @(
    'function Apply-SurvivorDimensionalPouch0150',
    'mg_dimensional_pouch',
    'mg_dimensional_pouch_rank',
    'ncmm_survivor_dimensional_pouch',
    'dimensional_pouch.json',
    'ncmm_survivor_mana_hand_carrier',
    'mana_hand_carrier.json',
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
    'is_armed() && ncmm_virtual_hands <= 0',
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
    'const bool ncmm_virtual_focus =',
    'consider_virtual_shield',
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
    'NCMM Survivor vehicle install Crafting XP',
    'NCMM Survivor vehicle removal Crafting XP',
    'ncmm::gameplay_metric_record_completed_craft( you );',
    'Apply-SurvivorVehicleCraftingXp0151 $CddaRoot',
    'function Apply-SurvivorManaHandSpellcastingAid0140',
    'ncmm_virtual_wield_flags',
    'flag_id( "SPELLCASTING_AID" )',
    'const int ncmm_virtual_hands =',
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    'Apply-SurvivorManaHandSpellcastingAid0140 $CddaRoot',
    'function Apply-SurvivorVirtualItemLifecycle0140',
    'ncmm::release_virtual_item( *target() );',
    'Apply-SurvivorVirtualItemLifecycle0140 $CddaRoot',
    'function Apply-SurvivorManaHandUtility0140',
    'bool ncmm_mana_hand_holds_item( const Character &who, const item &it )',
    'ncmm::virtual_item_for_slot( "survivor_progression", "mana_hand_3" ) == &it',
    'ncmm::virtual_item_for_slot( "survivor_progression", "mana_hand_4" ) == &it',
    'if( need_wielding && !p.is_wielding( it ) && !ncmm_mana_hand_holds_item( p, it ) ) {',
    'if( need_wielding && !p->is_wielding( it ) && !ncmm_mana_hand_holds_item( *p, it ) ) {',
    'Apply-SurvivorManaHandUtility0140 $CddaRoot',
    'function Apply-SurvivorManaHandSecondaryMelee0140',
    'class ncmm_virtual_melee_scope',
    'virtual_melee_context_begin( who, weapon )',
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
    'NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2',
    "case '5':",
    'item *ncmm_paired_weapon =',
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
    'Reload-and-shoot firing modes are not yet supported by Mana Hands.',
    'Apply-SurvivorManaHandPairedRanged0140 $CddaRoot',
    'function Apply-SurvivorManaHandReloadAndShoot0140',
    'ncmm_mana_hand_ras_switch',
    'activity != nullptr ? activity->get_weapon() : you->get_wielded_item()',
    'Apply-SurvivorManaHandReloadAndShoot0140 $CddaRoot',
    'function Apply-SurvivorManaHandFireAction0140',
    'ncmm_mana_fire_candidates',
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
    'ncmm_primary_mana_hand_melee_weapon',
    'ncmm::virtual_melee_context_suppresses_martial_arts',
    'ncmm_primary_scope( *this, *ncmm_primary_weapon, false )',
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
    'ncmm_primary_mana_hand_autoattack_weapon',
    'ncmm_primary_mana_hand_autoattack_reach',
    'Apply-SurvivorManaHandAutoattack0140 $CddaRoot',
    'function Apply-SurvivorManaHandThrow0140',
    'ncmm_is_mana_hand_throw_item',
    'ncmm_select_mana_hand_throw_item',
    'Apply-SurvivorManaHandThrow0140 $CddaRoot',
    'function Apply-SurvivorManaHandAutoMining0140',
    'ncmm_mana_hand_auto_mining_tool',
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
    'container.remove_items_with(',
    '$containerOldNorm0140life = (Normalize-Lf $containerOld0140life).TrimEnd()',
    '$container1831OldNorm0140life = (Normalize-Lf $container1831Old0140life).TrimEnd()',
    '$itemLocation0140life.Contains($containerOldNorm0140life)',
    '$itemLocation0140life.Contains($container1831OldNorm0140life)',
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
    'item *ncmm_bound3 = ncmm_mana_hands_now >= 1 ?',
    'item *ncmm_bound4 = ncmm_mana_hands_now >= 2 ?',
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


$manaUxOldStart0151=$manaContextSection0140.IndexOf('$manaUxMenuOld0151 = @''')
$manaUxNewStart0151=$manaContextSection0140.IndexOf('$manaUxMenuNew0151 = @''',$manaUxOldStart0151)
if($manaUxOldStart0151 -lt 0 -or $manaUxNewStart0151 -le $manaUxOldStart0151){
    throw 'Mana Hand direct inventory clean-source menu transform structure missing.'
}
$manaUxOld0151=$manaContextSection0140.Substring($manaUxOldStart0151,$manaUxNewStart0151-$manaUxOldStart0151)
if(-not $manaUxOld0151.Contains('const int ncmm_mana_hands = static_cast<int>(')){
    throw 'Mana Hand direct inventory action must anchor after the generated III/IV menu on clean source.'
}
if($manaUxOld0151.Contains('if( bHPR ) {')){
    throw 'Mana Hand direct inventory action regressed to the consumed vanilla bHPR anchor.'
}


$manaUtilityStart0140=$payload.IndexOf('function Apply-SurvivorManaHandUtility0140')
$manaSecondaryStart0140=$payload.IndexOf('function Apply-SurvivorManaHandSecondaryMelee0140',$manaUtilityStart0140)
if($manaUtilityStart0140 -lt 0 -or $manaSecondaryStart0140 -le $manaUtilityStart0140){throw 'Mana Hand utility/secondary transform boundary missing.'}
$manaUtilitySection0140=$payload.Substring($manaUtilityStart0140,$manaSecondaryStart0140-$manaUtilityStart0140)
if($manaUtilitySection0140.Contains('bool Character::is_wielding')){
    throw 'Mana Hand utility layer must not patch Character::is_wielding semantics.'
}
foreach($utilityNeedle0140 in @(
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    'hand_count >= 1',
    'hand_count >= 2',
    'ncmm_mana_hand_holds_item( p, it )',
    'ncmm_mana_hand_holds_item( *p, it )'
)){
    if(-not $manaUtilitySection0140.Contains($utilityNeedle0140)){
        throw ('Mana Hand utility regression contract missing: '+$utilityNeedle0140)
    }
}

$manaPairStart0140=$payload.IndexOf('function Apply-SurvivorManaHandPairedGrip0140',$manaSecondaryStart0140)
if($manaPairStart0140 -le $manaSecondaryStart0140){throw 'Mana Hand paired-grip transform boundary missing.'}
$manaSecondarySection0140=$payload.Substring($manaSecondaryStart0140,$manaPairStart0140-$manaSecondaryStart0140)
foreach($secondaryNeedle0140 in @(
    'class ncmm_virtual_melee_scope',
    'ncmm::virtual_melee_context_begin( who, weapon )',
    'who_.recalculate_enchantment_cache();',
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
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
    'NCMM Mana Hand secondary strikes do not trigger martial-art event chains.',
    'ncmm::virtual_melee_context_active( *this ) ? tec_none.obj()'
)){
    if(-not $manaSecondarySection0140.Contains($secondaryNeedle0140)){
        throw ('Mana Hand secondary-melee regression contract missing: '+$secondaryNeedle0140)
    }
}
if($manaSecondarySection0140.Contains('set_wielded_item(') -or
   $manaSecondarySection0140.Contains('u.wield(')){
    throw 'Mana Hand secondary melee must not move the virtual item into Character::weapon.'
}

if(([regex]::Matches($manaSecondarySection0140,[regex]::Escape('if( !ncmm::virtual_melee_context_active( *this ) ) {'))).Count -lt 4){
    throw 'Mana Hand secondary melee must suppress all martial-art event chains while scoped.'
}
foreach($upgradeNeedle0140 in @(
    '$secondaryMenuOld0140ctx = @''',
    '$secondarySwitchOld0140ctx = @''',
    'Mana Hand secondary-melee context menu upgrade',
    'Mana Hand secondary-melee context handler upgrade',
    'item *ncmm_bound3 = ncmm_mana_hands_now >= 1 ?',
    'item *ncmm_bound4 = ncmm_mana_hands_now >= 2 ?'
)){
    if(-not $manaContextSection0140.Contains($upgradeNeedle0140)){
        throw ('Mana Hand context update-path regression contract missing: '+$upgradeNeedle0140)
    }
}

$manaRangeStart0140=$payload.IndexOf('function Apply-SurvivorManaHandRanged0140',$manaPairStart0140)
if($manaRangeStart0140 -le $manaPairStart0140){throw 'Mana Hand ranged transform boundary missing.'}
$manaPairSection0140=$payload.Substring($manaPairStart0140,$manaRangeStart0140-$manaPairStart0140)
foreach($pairNeedle0140 in @(
    'item *ncmm_mana_hands_34 = ncmm_virtual_hands >= 2 ?',
    'ncmm_mana_hands_34 != nullptr ? 2 :',
    'consider_virtual_shield( "mana_hands_34" );',
    'item *ncmm_pair = ncmm_virtual_hands >= 2 ?',
    '"mana_hands_34" ) == &it',
    'item *ncmm_mana_pair_item = ncmm_mana_hands >= 2 ?',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2',
    "case '5':",
    'ncmm_mana_pair_item == nullptr && ncmm_mana_hands >= 1',
    'ncmm_mana_pair_item == nullptr && ncmm_mana_hands >= 2',
    'item *ncmm_paired_weapon =',
    'ncmm_paired_weapon->is_two_handed( who )',
    '!ncmm_paired_weapon->is_gun()'
)){
    if(-not $manaPairSection0140.Contains($pairNeedle0140)){
        throw ('Mana Hand paired-grip regression contract missing: '+$pairNeedle0140)
    }
}
if(([regex]::Matches($manaPairSection0140,[regex]::Escape('"mana_hands_34"'))).Count -lt 8){
    throw 'Mana Hand paired grip must use one dedicated pair slot across all integrations.'
}
if($manaPairSection0140.Contains('item_location::type::mana_hand') -or
   $manaPairSection0140.Contains('set_wielded_item(') -or
   $manaPairSection0140.Contains('u.wield(')){
    throw 'Mana Hand paired grip must not synthesize locations or move the real item.'
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
    'const int ncmm_ranged_hand_count =',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
    'if( ncmm_candidate == ncmm_hand3 || ncmm_candidate == ncmm_hand4 )',
    'jsout.member( "ncmm_real_weapon", ncmm_real_weapon );',
    'data.read( "ncmm_real_weapon", actor.ncmm_real_weapon );',
    'aim_actor.ncmm_real_weapon = this->ncmm_real_weapon;',
    'item::reload_option opt = get_avatar().select_ammo( ncmm_real_weapon, true );',
    'const int ncmm_virtual_shot_mana_cost = ncmm_planned_shots * 5;',
    'who.magic->mod_mana( who, -( ncmm_fired * 5 ) );',
    'ncmm_virtual_mana_gun_mode',
    'gmode->has_flag( flag_FIRE_TWOHAND )',
    'gmode->has_flag( flag_RELOAD_AND_SHOOT )',
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
    throw 'Mana Hand ranged v1 base layer must remain single-hand only; paired support belongs to the additive paired-ranged layer.'
}

$manaRasStart0140=$payload.IndexOf('function Apply-SurvivorManaHandReloadAndShoot0140',$manaPairedRangeStart0140)
if($manaRasStart0140 -le $manaPairedRangeStart0140){throw 'Mana Hand reload-and-shoot transform boundary missing.'}
$manaPairedRangeEnd0140=$manaRasStart0140
$manaPairedRangeSection0140=$payload.Substring($manaPairedRangeStart0140,$manaPairedRangeEnd0140-$manaPairedRangeStart0140)
foreach($pairedRangeNeedle0140 in @(
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2;',
    'const bool ncmm_mana_ranged_pair =',
    'const bool ncmm_ranged_pair =',
    '"survivor_progression", "mana_hands_34"',
    'item *ncmm_pair = ncmm_ranged_hand_count >= 2 ?',
    'ncmm_candidate == ncmm_pair',
    'bool ncmm_virtual_mana_paired_gun_mode = false;',
    'item *ncmm_pair_base =',
    '!ncmm_virtual_mana_paired_gun_mode &&',
    'Reload-and-shoot firing modes are not yet supported by Mana Hands.',
    'This firing mode needs paired Mana Hands III+IV.'
)){
    if(-not $manaPairedRangeSection0140.Contains($pairedRangeNeedle0140)){
        throw ('Paired Mana Hand ranged regression contract missing: '+$pairedRangeNeedle0140)
    }
}
if($manaPairedRangeSection0140.Contains('set_wielded_item(') -or
   $manaPairedRangeSection0140.Contains('u.wield(') -or
   $manaPairedRangeSection0140.Contains('item_location::type::mana_hand')){
    throw 'Paired Mana Hand ranged support must keep the real gun in its vanilla item_location.'
}

$manaFireStart0140=$payload.IndexOf('function Apply-SurvivorManaHandFireAction0140',$manaRasStart0140)
if($manaFireStart0140 -le $manaRasStart0140){throw 'Mana Hand FIRE action transform boundary missing.'}
$manaRasEnd0140=$manaFireStart0140
$manaRasSection0140=$payload.Substring($manaRasStart0140,$manaRasEnd0140-$manaRasStart0140)
foreach($rasNeedle0140 in @(
    'ncmm_mana_hand_ras_switch',
    'item_location gun = activity != nullptr ? activity->get_weapon() : you->get_wielded_item();',
    'item::reload_option opt = you->select_ammo( gun );',
    'activity->reload_loc = opt.ammo;',
    'Replace-TextBlock $ranged0140ras $unsupported0140ras',
    'if($rasOutput0140.Contains'
)){
    if(-not $manaRasSection0140.Contains($rasNeedle0140)){
        throw ('Mana Hand reload-and-shoot regression contract missing: '+$rasNeedle0140)
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
    'const bool ncmm_physical_ranged_ready =',
    'std::vector<item_location> ncmm_mana_fire_candidates;',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
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

$manaGunControlsEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandPrimaryMelee0140',$manaGunControlsStart0140)
if($manaGunControlsEnd0140 -le $manaGunControlsStart0140){throw 'Mana Hand standard gun-control transform end missing.'}
$manaGunControlsSection0140=$payload.Substring($manaGunControlsStart0140,$manaGunControlsEnd0140-$manaGunControlsStart0140)
foreach($controlNeedle0140 in @(
    'ncmm_select_mana_hand_gun_control',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
    'Reload which Mana Hand weapon?',
    'reload( ncmm_reload_gun, false, false );',
    'reload( ncmm_reload_gun, false );',
    'Burst-fire which Mana Hand weapon?',
    'aim_activity_actor::use_item_location( ncmm_burst_gun )',
    'Change firing mode on which Mana Hand weapon?',
    'gun_cycle_mode();',
    'Set default ammo for which Mana Hand weapon?',
    'player_character.select_ammo( *ammo_weapon, false )'
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
$manaMartialSection0140=$payload.Substring($manaMartialStart0140,$manaReachStart0140-$manaMartialStart0140)
foreach($martialNeedle0140 in @(
    'item_location ncmm_mana_hand_martial_context_weapon( const Character &who )',
    'item_location ncmm_primary_mana_hand_martial_weapon( const Character &who )',
    'ncmm::virtual_melee_context_active( who )',
    'const item_location weapon = ncmm_mana_hand_martial_context_weapon( u );',
    'const bool virtual_scope =',
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    '"survivor_progression", "mana_hands_34"',
    'ncmm::virtual_item_primary_melee_enabled( *paired )',
    'bool is_armed = weapon || u.is_armed();',
    'ncmm::virtual_melee_context_begin( mutable_owner, *martial_weapon, false )',
    'martial_weapon, owner',
    'bool valid_weapon = ma.weapon_valid( martial_weapon );',
    'item *weapon = martial_weapon.get_item();'
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
    'class ncmm_virtual_reach_scope',
    'ncmm::virtual_melee_context_begin( who, weapon, false )',
    'ncmm_primary_mana_hand_reach_weapon',
    'ncmm_primary_mana_hand_has_reach',
    'target_handler::mode_reach(',
    'item_location( you, ncmm_reach_weapon )',
    'ncmm_primary_mana_hand_melee_weapon( *this )',
    'std::make_unique<ncmm_virtual_melee_scope>',
    'item_location reach_weapon = used_weapon();',
    'handle_melee_wear( reach_weapon );',
    'get_total_melee_stamina_cost( &reach_item )',
    'magic->mod_mana( *this, -ncmm_reach_mana_cost )'
)){
    if(-not $manaReachSection0140.Contains($reachNeedle0140)){
        throw ('Mana Hand reach-melee regression contract missing: '+$reachNeedle0140)
    }
}
if($manaReachSection0140.Contains('set_wielded_item(') -or
   $manaReachSection0140.Contains('u.wield(') -or
   $manaReachSection0140.Contains('.obtain(')){
    throw 'Mana Hand reach melee must not move the real item into Character::weapon.'
}

$manaSmashEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoattack0140',$manaSmashStart0140)
if($manaSmashEnd0140 -le $manaSmashStart0140){throw 'Primary Mana Hand smash transform end missing.'}
$manaSmashSection0140=$payload.Substring($manaSmashStart0140,$manaSmashEnd0140-$manaSmashStart0140)
foreach($smashNeedle0140 in @(
    'item_location ncmm_smash_weapon = get_wielded_item();',
    'ncmm::virtual_melee_context_item( *this )',
    'item *ncmm_primary_mana_hand_smash_weapon( avatar &you )',
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
    'class ncmm_mana_hand_autoattack_scope',
    'ncmm_primary_mana_hand_autoattack_weapon',
    'ncmm_primary_mana_hand_autoattack_reach',
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", slots[i]',
    'ncmm::virtual_item_primary_melee_enabled',
    'ncmm::virtual_melee_context_begin( who, weapon, false )',
    'item *ncmm_autoattack_weapon = nullptr;',
    'you.reach_attack( best.pos_bub() );'
)){if(-not $manaAutoSection0140.Contains($autoNeedle0140)){throw ('Mana Hand autoattack regression contract missing: '+$autoNeedle0140)}}
if($manaAutoSection0140.Contains('set_wielded_item(') -or $manaAutoSection0140.Contains('you.wield(') -or $manaAutoSection0140.Contains('.obtain(')){throw 'Mana Hand autoattack must not move the real item into Character::weapon.'}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandAutoattack0140 -CommandType Function).Definition')){throw 'Mana Hand autoattack transform missing from mechanics patch revision.'}


$manaThrowStart0140=$payload.IndexOf('function Apply-SurvivorManaHandThrow0140',$manaAutoStart0140)
$manaThrowEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoMining0140',$manaThrowStart0140)
if($manaThrowStart0140 -le $manaAutoStart0140 -or $manaThrowEnd0140 -le $manaThrowStart0140){throw 'Mana Hand throw transform boundary missing.'}
$manaThrowSection0140=$payload.Substring($manaThrowStart0140,$manaThrowEnd0140-$manaThrowStart0140)
foreach($throwNeedle0140 in @(
    'ncmm_is_mana_hand_throw_item',
    'ncmm_select_mana_hand_throw_item',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
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
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandThrow0140 -CommandType Function).Definition')){throw 'Mana Hand throw transform missing from mechanics patch revision.'}


$manaMineStart0140=$payload.IndexOf('function Apply-SurvivorManaHandAutoMining0140',$manaThrowStart0140)
$manaMineEnd0140=$payload.IndexOf('function Apply-SurvivorManaHandTargetPractice0140',$manaMineStart0140)
if($manaMineStart0140 -le $manaThrowStart0140 -or $manaMineEnd0140 -le $manaMineStart0140){throw 'Mana Hand auto-mining transform boundary missing.'}
$manaMineSection0140=$payload.Substring($manaMineStart0140,$manaMineEnd0140-$manaMineStart0140)
foreach($mineNeedle0140 in @(
    'ncmm_mana_hand_auto_mining_tool',
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    '"survivor_progression", slot',
    '"mana_hands_34"',
    '"mana_hand_3"',
    '"mana_hand_4"',
    'candidate->has_flag( flag_DIG_TOOL )',
    'candidate->type->can_use( "PICKAXE" )',
    'item_location weapon = you.get_wielded_item();',
    'if( !weapon &&',
    'm.has_flag( ter_furn_flag::TFLAG_MINEABLE, dest_loc ) &&',
    'g->mostseen == 0 ) {',
    'you.invoke_item( &*weapon, "PICKAXE", dest_loc );'
)){if(-not $manaMineSection0140.Contains($mineNeedle0140)){throw ('Mana Hand auto-mining regression contract missing: '+$mineNeedle0140)}}
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
    'ncmm::virtual_item_matches_slot(',
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
    'if( !ncmm_virtual_target_gun && !who.is_wielding( *gun_loc ) )',
    '!ncmm_target_practice_mana_hand_gun( who, gun )',
    'constexpr int ncmm_target_practice_mana_cost = 5;',
    'who.magic->available_mana() < ncmm_target_practice_mana_cost',
    'who.magic->mod_mana( who, -ncmm_target_practice_mana_cost );'
)){if(-not $manaPracticeSection0140.Contains($practiceNeedle0140)){throw ('Mana Hand target-practice regression contract missing: '+$practiceNeedle0140)}}
if($manaPracticeSection0140.Contains('virtual_item_for_slot(')){
    throw 'Mana Hand target practice must use O(1) slot identity validation, not full inventory reconciliation.'
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
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
    'Mend which Mana Hand item?',
    'if( you.is_armed() )',
    'loc = you.get_wielded_item();',
    'loc = ncmm_select_mana_hand_mend_item( you, ncmm_had_mend_candidates );',
    'you.mend_item( item_location( loc ) );'
)){if(-not $manaMendSection0140.Contains($mendNeedle0140)){throw ('Mana Hand mend regression contract missing: '+$mendNeedle0140)}}
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
    '"magic.virtual_hand_count", nullptr, "magiclysm"',
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"',
    'ncmm_mana_hand_has_crutches( you )',
    'return ( !enough_working_legs() &&',
    '!weapon.has_flag( flag_CRUTCHES ) &&',
    '!ncmm_mana_hand_has_crutches( *this ) ) ||'
)){if(-not $manaCrutchSection0140.Contains($crutchNeedle0140)){throw ('Mana Hand crutch regression contract missing: '+$crutchNeedle0140)}}
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
    '"survivor_progression", "mana_hands_34"',
    '"survivor_progression", "mana_hand_3"',
    '"survivor_progression", "mana_hand_4"'
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
if($manaHeldSection0140.Contains('set_wielded_item(') -or
   $manaHeldSection0140.Contains('you.wield(') -or
   $manaHeldSection0140.Contains('.obtain(')){
    throw 'Mana Hand held utilities must not move the real virtual item into Character::weapon.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandHeldUtilities0140 -CommandType Function).Definition')){throw 'Mana Hand held-utility transform missing from mechanics patch revision.'}


# Mana Hand direct-count reconciliation contract:
# gameplay/slot ownership and combat selection must read the same aggregate modifier.
$manaDirectStart0152=$payload.IndexOf('function Apply-SurvivorManaHandDirectCount0152',$manaHeldStart0140)
$manaDirectEnd0152=$payload.IndexOf('function Apply-SurvivorCraftCompletionMetric0140',$manaDirectStart0152)
if($manaDirectStart0152 -le $manaHeldStart0140 -or $manaDirectEnd0152 -le $manaDirectStart0152){
    throw 'Mana Hand direct-count reconciliation transform boundary missing.'
}
$manaDirectSection0152=$payload.Substring($manaDirectStart0152,$manaDirectEnd0152-$manaDirectStart0152)
foreach($directNeedle0152 in @(
    'ncmm::gameplay_modifier( "mg_virtual_hand_count" )',
    '$legacyPattern0152direct',
    'ncmm_primary_mana_hand_melee_weapon',
    'Source-scoped Mana Hand count survived direct-count reconciliation'
)){
    if(-not $manaDirectSection0152.Contains($directNeedle0152)){
        throw ('Mana Hand direct-count reconciliation contract missing: '+$directNeedle0152)
    }
}
if($manaDirectSection0152.Contains("'magic.cpp'")){
    throw 'Mana Hand direct-count reconciliation must not rewrite spell-source selection.'
}
if(-not $payload.Contains('(Get-Command Apply-SurvivorManaHandDirectCount0152 -CommandType Function).Definition')){
    throw 'Mana Hand direct-count reconciliation missing from mechanics patch revision.'
}
if(([regex]::Matches($payload,[regex]::Escape('Apply-SurvivorManaHandDirectCount0152 $CddaRoot'))).Count -lt 3){
    throw 'Mana Hand direct-count reconciliation must run during initial transform, deep probe, and normal build.'
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
    '++gameplay_metric_values["crafting.completed"];'
)){if(-not $xpBalanceSection0140.Contains($xpBalanceNeedle0140)){throw ('Survivor XP-balance regression missing: '+$xpBalanceNeedle0140)}}
if(-not $payload.Contains('(Get-Command Apply-SurvivorXpBalance0140 -CommandType Function).Definition')){throw 'XP-balance transform missing from mechanics patch revision.'}
if(([regex]::Matches($payload,[regex]::Escape('Apply-SurvivorXpBalance0140'))).Count -lt 2){throw 'XP-balance transform is not applied after canonical module sync.'}

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

Write-Host 'NCMM Survivor payload regression contract: PASS' -ForegroundColor Green
