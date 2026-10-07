param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$m=Get-Content (Join-Path $PackageRoot 'compat\compatibility.manifest.json') -Raw|ConvertFrom-Json
$common083=[IO.File]::ReadAllText((Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1'))

$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'));foreach($n in @('[switch]$HostSourceProbeOnly','Set-InfrastructureTransactionPhase "compile"','Set-InfrastructureTransactionPhase "install"','DEEP_SOURCE_PASS')){if(-not $payload.Contains($n)){throw "Payload contract missing: $n"}}


# Certified Hosts must apply every engine transform required by current Mana Hands.
# A module-only/generic Host can successfully hold the item while combat still falls
# back to fists, so this is a release-blocking parity contract rather than UI coverage.
$certifiedHostStackPath=Join-Path $PackageRoot 'ci\host-patch-stack.json'
$certifiedHostStack=Get-Content $certifiedHostStackPath -Raw|ConvertFrom-Json
foreach($requiredHelper in @('Normalize-Path','Test-TextBlock','Count-TextBlock')){
    if(@($certifiedHostStack.helpers) -notcontains $requiredHelper){
        throw ('Certified Host patch stack is missing helper required by Mana Hand transforms: '+$requiredHelper)
    }
}
$requiredManaHostLayers=@(
    'Apply-NcmmRuntimeGameplayHooksV2',
    'Apply-SurvivorManaHands0130',
    'Apply-SurvivorVirtualItemSlots0140',
    'Apply-SurvivorVirtualItemContext0140',
    'Apply-SurvivorVehicleCraftingXp0151',
    'Apply-SurvivorManaHandSpellcastingAid0140',
    'Apply-SurvivorVirtualItemLifecycle0140',
    'Apply-SurvivorManaHandUtility0140',
    'Apply-SurvivorManaHandSecondaryMelee0140',
    'Apply-SurvivorManaHandPairedGrip0140',
    'Apply-SurvivorManaHandRanged0140',
    'Apply-SurvivorManaHandPairedRanged0140',
    'Apply-SurvivorManaHandReloadAndShoot0140',
    'Apply-SurvivorManaHandFireAction0140',
    'Apply-SurvivorManaHandGunControls0140',
    'Apply-SurvivorManaHandReloadCarrier0153',
    'Apply-SurvivorManaHandPrimaryMelee0140',
    'Apply-SurvivorManaHandMartialArts0140',
    'Apply-SurvivorManaHandReachMelee0140',
    'Apply-SurvivorManaHandSmash0140',
    'Apply-SurvivorManaHandAutoattack0140',
    'Apply-SurvivorManaHandThrow0140',
    'Apply-SurvivorManaHandAutoMining0140',
    'Apply-SurvivorManaHandTargetPractice0140',
    'Apply-SurvivorManaHandMend0140',
    'Apply-SurvivorManaHandCrutches0140',
    'Apply-SurvivorManaHandHeldUtilities0140',
    'Apply-SurvivorCraftCompletionMetric0140',
    'Apply-SurvivorVehicleCraftingMetric0151',
    'Apply-SurvivorManaHandDirectCount0152',
    'Apply-SurvivorActionWeaponSelection0154',
    'Apply-SurvivorManaHandsDirectUi0155'
)
$actualCertifiedLayers=@($certifiedHostStack.layers|ForEach-Object{[string]$_})
$previousLayerIndex=-1
foreach($requiredLayer in $requiredManaHostLayers){
    $layerIndex=[Array]::IndexOf([string[]]$actualCertifiedLayers,$requiredLayer)
    if($layerIndex -lt 0){
        throw ('Certified Host is missing required Mana Hand engine layer: '+$requiredLayer)
    }
    if($layerIndex -le $previousLayerIndex){
        throw ('Certified Host Mana Hand engine layer order regression: '+$requiredLayer)
    }
    $previousLayerIndex=$layerIndex
    if(-not $payload.Contains('function '+$requiredLayer)){
        throw ('Certified Host layer has no canonical payload definition: '+$requiredLayer)
    }
}
if([Array]::IndexOf([string[]]$actualCertifiedLayers,'Apply-SurvivorManaHandDirectCount0152') -lt
   [Array]::IndexOf([string[]]$actualCertifiedLayers,'Apply-SurvivorManaHandPrimaryMelee0140')){
    throw 'Mana Hand direct-count reconciliation must run after primary melee injection.'
}


# RANDOM_DAMAGE tooltip regression: runtime spell-power hooks must affect both the
# real cast and the pre-cast spell description.  The complete-gate probe forces
# already-patched source caches to receive this newer UI hook as well.
foreach($n in @(
    'NCMM effective RANDOM_DAMAGE tooltip',
    '$magicProbe.Contains(''NCMM effective RANDOM_DAMAGE tooltip'')',
    '''magic.random-damage-tooltip''',
    'const int vanilla_damage = static_cast<int>( value * temp_damage_multiplyer );',
    'ncmm_spell_multiplier( *this, ncmm_spell_modifier::power, 0.0 )'
)){
    if(-not $payload.Contains($n)){throw ('Random spell-damage tooltip contract missing: '+$n)}
}
$tooltipProbeLow=[Math]::Round([Math]::Truncate(31.0)*1.06,0,[MidpointRounding]::AwayFromZero)
$tooltipProbeHigh=[Math]::Round([Math]::Truncate(63.0)*1.06,0,[MidpointRounding]::AwayFromZero)
if($tooltipProbeLow -ne 33 -or $tooltipProbeHigh -ne 67){
    throw 'Random spell-damage tooltip regression fixture failed for 31-63 at +6% power.'
}
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

$hostComponent=Get-Content (Join-Path $PackageRoot 'components\ncmm_host.json') -Raw|ConvertFrom-Json
$expectedHostVersion=([string]$hostComponent.version).Trim()
if($expectedHostVersion -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$'){throw "Invalid Host component version: $expectedHostVersion"}
if([string]$m.host_version -ne $expectedHostVersion -or [string]$m.ncmm_api -ne '1.9' -or [string]$m.host_api_v2 -ne '2.3'){throw 'Host API 2.3 Core manifest identity mismatch.'}
foreach($cap in @('host_api.v2.core','character.virtual_items.v1','events.core.v2','settings.typed.v2','character.modifiers.v2','runtime_settings.bindings.v2','runtime_hooks.registry.v2','worldgen.bindings.v2','module.lifecycle.query.v2')){if(@($m.self_test.required_capabilities) -notcontains $cap){throw "Host API 2.1 required capability missing: $cap"}}
if(@($hostComponent.provides) -notcontains 'host_api_v2:2.3' -or @($hostComponent.provides) -notcontains 'character.virtual_items.v1' -or @($hostComponent.provides) -notcontains 'runtime_settings.bindings.v2'){throw 'Host API 2.3 component catalog mismatch.'}
foreach($n in @('function Apply-NcmmHostApi20Core','#define NCMM_HOST_API_V2_CORE_MAJOR 2u','typedef struct ncmm_host_api_v2_core {','const ncmm_host_api_v2_core api_v2_core = {','runtime_hook_bind_modifier_v2','worldgen_hook_bind_setting_v2','Apply-NcmmHostApi20Core')){if(-not $payload.Contains($n)){throw "Host API 2.0 payload contract missing: $n"}}
# Patched-source audit must distinguish the required query_interface_v2 forward declaration
# from its single implementation.  A raw substring count is intentionally invalid because both
# declaration and definition begin with the same function name/signature.
$sdkCurrent=Get-Content (Join-Path $PackageRoot 'sdk\ncmm_api.h') -Raw
$hostHeaderCurrent=Get-Content (Join-Path $PackageRoot 'host_patch\ncmm_loader.h') -Raw
$hostSourceCurrent=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch\ncmm_loader.cpp'))
if(([regex]::Matches($hostSourceCurrent,[regex]::Escape('remove_worn_items_with( []( const item &candidate )'))).Count -ne 2){
    throw 'Host worn-item cleanup predicates must use const item& for cross-version CDDA compatibility.'
}
if($hostSourceCurrent.Contains('remove_worn_items_with( []( item &candidate )')){
    throw 'Host worn-item cleanup still uses legacy mutable predicate signature.'
}
$manaHandCarrierCurrent=[IO.File]::ReadAllText((Join-Path $PackageRoot 'mods\SurvivorProgression\persistent_data\mana_hand_carrier.json'))
if(([regex]::Matches($manaHandCarrierCurrent,[regex]::Escape('"max_item_length": "5 meter"'))).Count -ne 2){
    throw 'Mana Hand carrier must use CDDA-supported meter length units for both internal pockets.'
}
if(([regex]::Matches($manaHandCarrierCurrent,[regex]::Escape('"moves": 1'))).Count -ne 2){
    throw 'Mana Hand carrier must use a nonzero obtain cost for both internal pockets.'
}
if($manaHandCarrierCurrent.Contains('"moves": 0')){
    throw 'Mana Hand carrier zero-move pockets trigger item_location::obtain_cost debug errors.'
}
if(([regex]::Matches($payload,[regex]::Escape('"moves": 1'))).Count -lt 2){
    throw 'Cumulative payload Mana Hand carrier obtain cost is stale.'
}
if($manaHandCarrierCurrent.Contains('"max_item_length": "5 m"')){
    throw 'Mana Hand carrier uses unsupported abbreviated meter unit.'
}
if(([regex]::Matches($payload,[regex]::Escape('"max_item_length": "5 meter"'))).Count -lt 2){
    throw 'Cumulative payload Mana Hand carrier length unit is stale.'
}


# Canonical files embedded in the cumulative payload must stay synchronized with
# the checked-in sources. This closes the gap where static package checks passed
# while certification regenerated an older Host or smoke harness.
$canonicalHostMatch=[regex]::Match($payload,"Write-NcmmCanonicalPayloadFile 'host_patch\\ncmm_loader\.cpp' '([^']+)'")
if(-not $canonicalHostMatch.Success){throw 'Canonical Host payload entry missing.'}
$canonicalHostText=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($canonicalHostMatch.Groups[1].Value))
$hostSourceLf=$hostSourceCurrent.Replace("`r`n","`n").Replace("`r","`n")
$canonicalHostLf=$canonicalHostText.Replace("`r`n","`n").Replace("`r","`n")
if($canonicalHostLf -cne $hostSourceLf){throw 'Canonical Host payload is stale versus host_patch/ncmm_loader.cpp.'}

$canonicalHeaderMatch=[regex]::Match($payload,"Write-NcmmCanonicalPayloadFile 'host_patch\\ncmm_loader\.h' '([^']+)'")
if(-not $canonicalHeaderMatch.Success){throw 'Canonical Host header payload entry missing.'}
$canonicalHeaderText=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($canonicalHeaderMatch.Groups[1].Value))
$hostHeaderLf=$hostHeaderCurrent.Replace("`r`n","`n").Replace("`r","`n")
$canonicalHeaderLf=$canonicalHeaderText.Replace("`r`n","`n").Replace("`r","`n")
if($canonicalHeaderLf -cne $hostHeaderLf){throw 'Canonical Host header payload is stale versus host_patch/ncmm_loader.h.'}

$smokeSourceCurrent=[IO.File]::ReadAllText((Join-Path $PackageRoot 'tests\smoke_host.cpp'))
$canonicalSmokeMatch=[regex]::Match($payload,"Write-NcmmCanonicalPayloadFile 'tests\\smoke_host\.cpp' '([^']+)'")
if(-not $canonicalSmokeMatch.Success){throw 'Canonical smoke_host payload entry missing.'}
$canonicalSmokeText=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($canonicalSmokeMatch.Groups[1].Value))
$smokeSourceLf=$smokeSourceCurrent.Replace("`r`n","`n").Replace("`r","`n")
$canonicalSmokeLf=$canonicalSmokeText.Replace("`r`n","`n").Replace("`r","`n")
if($canonicalSmokeLf -cne $smokeSourceLf){throw 'Canonical smoke_host payload is stale versus tests/smoke_host.cpp.'}
foreach($n in @('#define NCMM_HOST_API_V2_CORE_MINOR 3u','NCMM_HOST_API_V2_CORE_SIZE_2_1','NCMM_HOST_API_V2_CORE_SIZE_2_2','NCMM_HOST_API_V2_CORE_SIZE_2_3','NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2','NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2','NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2','NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2','NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2','virtual_item_choose','virtual_item_clear','virtual_item_name','virtual_item_uid','virtual_item_secondary_melee_enabled','virtual_item_set_secondary_melee','virtual_item_primary_melee_enabled','virtual_item_set_primary_melee')){if(-not $sdkCurrent.Contains($n)){throw "Host API 2.3 SDK virtual-item extension contract missing: $n"}}
foreach($n in @('character.virtual_items.v1','virtual_item_choose_v2','virtual_item_for_slot_internal','virtual_item_matches_slot( const item &candidate','candidate.uid().get_value() != wanted_uid','virtual_item_candidate_runtime_valid( candidate, stored_flags )','virtual_item_can_assign_internal','virtual_item_assign_internal','item_location mutable_loc = loc;','item *selected = mutable_loc.get_item();','virtual_item_can_assign( const char *module_id','virtual_item_assign( const char *module_id','virtual_item_clear( const char *module_id','release_virtual_item( item &it )','virtual_item_secondary_melee_key = "ncmm_virtual_secondary_melee"','virtual_item_secondary_melee_enabled( const item &it )','virtual_item_set_secondary_melee( item &it, bool enabled )','virtual_item_secondary_melee_enabled_v2( const char *module_id, const char *slot_id )','virtual_item_set_secondary_melee_v2( const char *module_id, const char *slot_id,','!bound->is_melee() || bound->is_gun()','&virtual_item_secondary_melee_enabled_v2','&virtual_item_set_secondary_melee_v2','virtual_item_primary_melee_enabled_v2( const char *module_id, const char *slot_id )','virtual_item_set_primary_melee_v2( const char *module_id, const char *slot_id,','&virtual_item_primary_melee_enabled_v2','&virtual_item_set_primary_melee_v2','virtual_melee_context_begin( Character &who, item &weapon,','virtual_melee_context_end( Character &who )','virtual_melee_context_suppresses_martial_arts( const Character &who )','virtual_melee_context_is_wielding( const Character &who, const item &it )','virtual_item_primary_melee_key = "ncmm_virtual_primary_melee"','virtual_item_primary_melee_enabled( const item &it )','virtual_item_set_primary_melee( item &it, bool enabled )','const bool candidate_two_handed = candidate.is_two_handed( you );','NCMM_VIRTUAL_ITEM_ALLOW_TWO_HANDED_V2','NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2','NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2','virtual_item_marker_key = "ncmm_virtual_slot"','existing_marker.rfind( module_prefix, 0 ) != 0','virtual_item_for_slot_internal( module_id.c_str(), slot_id.c_str() ) == &it','mana_hand_inventory_action_visible( const item_location &loc )','mana_hand_inventory_action( item_location loc )','gameplay_modifier( "mg_virtual_hand_count" )','Hold in Mana Hand III','mana_hand_carrier_type_id( "ncmm_survivor_mana_hand_carrier" )','stash_wielded_item_for_mana_hand','restore_mana_hand_carrier_item','pocket->is_forbidden()','survivor_wield_transfer','sync_mana_hand_carrier','item moved = you.remove_weapon();','Mana Hand wield transfer bind: can_assign=','wield_transfer_bound_uid','get_map().add_item_or_charges','you.Character::wield( *stored, 0 )')){if(-not $hostSourceCurrent.Contains($n)){throw "Host API 2.1 Host virtual-item extension contract missing: $n"}}
foreach($n in @(
    'bool virtual_item_matches_slot( const item &candidate, const char *module_id,',
    'bool mana_hand_inventory_action_visible( const item_location &loc );',
    'bool mana_hand_inventory_action( item_location loc );',
    'std::string mana_hand_item_label( const item &candidate );',
    'void gameplay_metric_record_completed_craft( const Character &who );'
)){
    if(-not $hostHeaderCurrent.Contains($n)){throw ('Gameplay metrics craft-completion header contract missing: '+$n)}
}
foreach($n in @(
    'std::string mana_hand_item_label( const item &candidate )',
    'return label + ": " + candidate.tname();'
)){
    if(-not $hostSourceCurrent.Contains($n)){throw ('Mana Hand action-label source contract missing: '+$n)}
}
foreach($n in @(
    'void gameplay_metric_record_completed_craft( const Character &who )',
    '++gameplay_metric_values["crafting.completed"];'
)){
    if(-not $hostSourceCurrent.Contains($n)){throw ('Gameplay metrics craft-completion source contract missing: '+$n)}
}
if($hostSourceCurrent.Contains('case event_type::character_finished_activity:')){
    throw 'Ambiguous canceled activity craft metric path returned.'
}

# Runtime event routing and hook lookup are gameplay hot paths. Keep event delivery
# subscription-specific and index hook rules by hook id instead of scanning the
# full registry on every combat/spell query.
foreach($n in @(
    's.event_id != event_id',
    'runtime_hook_rule_indices_v2',
    'runtime_hook_rule_indices_v2[hook_id].push_back( runtime_hook_rules_v2.size() - 1 );',
    'const auto hook_it = runtime_hook_rule_indices_v2.find( hook_id );',
    'for( const size_t index : hook_it->second )'
)){
    if(-not $hostSourceCurrent.Contains($n)){throw ('Runtime hot-path optimization contract missing: '+$n)}
}

if(([regex]::Matches($hostSourceCurrent,[regex]::Escape('runtime_hook_rule_indices_v2.clear();'))).Count -lt 3){
    throw 'Runtime hook index is not cleared across module removal + Host lifecycle.'
}
if(([regex]::Matches($hostSourceCurrent,[regex]::Escape('runtime_setting_bindings_v2.clear();'))).Count -ne 2){
    throw 'Runtime setting bindings are not cleared across Host initialize/shutdown.'
}

foreach($n in @(
    'size_t mana_hands_check_count = 0;',
    'mana_hands_checks',
    '"mana_hands_hook_baseline"',
    '"mana_hands_uid_reconcile"',
    '"mana_hands_duplicate_cleanup"',
    '"mana_hands_melee_mode_exclusivity"',
    '"mana_hands_gun_melee_mode_rejected"',
    '"mana_hands_paired_bind"',
    '"mana_hands_paired_melee_modes"',
    '"mana_hands_stale_cleanup"',
    '"mana_hands_wield_transfer_setup"',
    '"mana_hands_wield_transfer_bind"',
    '"mana_hands_wield_transfer_carrier"',
    '"mana_hands_wield_transfer_release"',
    '"mana_hands_wield_transfer_cleanup"',
    'item wield_transfer_cleanup = get_avatar().remove_weapon();',
    'item( itype_id( "hatchet" ) )',
    'item( itype_id( "glock_19" ) )',
    'paired_probe.set_flag( flag_id( "ALWAYS_TWOHAND" ) )',
    'wield_transfer_probe.set_flag( flag_id( "ALWAYS_TWOHAND" ) )',
    'item_location_inside_mana_hand_carrier( wield_transfer_bound_loc )',
    '/16 real binding/state checks PASS.',
    'NCMM gameplay smoke checkpoint: Mana Hands '
)){
    if(-not $hostSourceCurrent.Contains($n)){throw ('Real Mana Hands gameplay smoke contract missing: '+$n)}
}
if($hostSourceCurrent.Contains('!you.is_armed() && you.wield( loc )')){throw 'Mana Hand release regressed to zero-move pocket obtain_cost path.'}
if($hostSourceCurrent.Contains('if( !get_avatar().unwield() )')){throw 'Headless Mana Hands smoke cleanup regressed to interactive Character::unwield()/dispose_item UI.'}
if($hostSourceCurrent.Contains('item moved = *selected;') -and $hostSourceCurrent.Contains('loc.remove_item();')){throw 'Mana Hand wield transfer regressed to general item_location removal instead of Character::remove_weapon().'}
if(-not $hostSourceCurrent.Contains('if( !loc.held_by( you ) && !physically_wielded )')){throw 'Mana Hand wield eligibility lost the exact-current-weapon ownership fallback.'}
if(-not $hostSourceCurrent.Contains('const item *location_item = loc.get_item();')){throw 'Mana Hand wield eligibility lost const-correct item_location access.'}
if(-not $hostSourceCurrent.Contains('const item *wielded_item = you.get_wielded_item().get_item();')){throw 'Mana Hand wield eligibility lost const-correct wielded-item access.'}
if(-not $hostSourceCurrent.Contains('Mana Hand wield eligibility rejected candidate: two_handed=')){throw 'Mana Hand wield eligibility diagnostics are missing.'}
if($hostSourceCurrent.Contains('you.wield( *stored, 0 )')){throw 'Mana Hand release regressed to avatar overload hiding the Character obtain-cost override.'}
if($hostSourceCurrent.Contains('return !it.get_var( virtual_item_marker_key, "" ).empty();')){throw 'Unsafe marker-only virtual-item identity check returned.'}

$transferReleaseStart=$hostSourceCurrent.IndexOf('bool release_virtual_item( item &it )')
$transferReleaseEnd=$hostSourceCurrent.IndexOf('bool is_virtual_item( const item &it )',$transferReleaseStart)
if($transferReleaseStart -lt 0 -or $transferReleaseEnd -le $transferReleaseStart){throw 'Virtual-item transfer-release function boundary missing.'}
$transferReleaseSection=$hostSourceCurrent.Substring($transferReleaseStart,$transferReleaseEnd-$transferReleaseStart)
if($transferReleaseSection.Contains('virtual_item_clear_internal( module_id.c_str(), slot_id.c_str() );')){throw 'Vanilla item transfer must not recursively restore a Mana Hand carrier item.'}
foreach($n in @(
    'it.erase_var( virtual_item_marker_key );',
    'virtual_item_state_set_uid_internal( module_id.c_str(), slot_id.c_str(), 0 );',
    'virtual_item_state_set_flags_internal( module_id.c_str(), slot_id.c_str(), 0u );'
)){if(-not $transferReleaseSection.Contains($n)){throw ('Virtual-item transfer detach contract missing: '+$n)}}
foreach($n in @(
    'virtual_item_clear_internal( survivor_module_id, mana_hand_3_slot_id );',
    'virtual_item_clear_internal( survivor_module_id, mana_hand_4_slot_id );',
    'virtual_item_clear_internal( survivor_module_id, mana_hands_pair_slot_id );'
)){if(-not $hostSourceCurrent.Contains($n)){throw ('Explicit Mana Hand release restore contract missing: '+$n)}}
$runtimeValidStart=$hostSourceCurrent.IndexOf('bool virtual_item_candidate_valid_impl( const item &candidate, uint32_t flags,')
$runtimeValidEnd=$hostSourceCurrent.IndexOf('item_location mana_hand_carrier_location(', $runtimeValidStart)
if($runtimeValidStart -lt 0 -or $runtimeValidEnd -le $runtimeValidStart){throw 'Virtual-item shared runtime-validity function boundary missing.'}
$runtimeValidSection=$hostSourceCurrent.Substring($runtimeValidStart,$runtimeValidEnd-$runtimeValidStart)
foreach($n in @(
    'NCMM_VIRTUAL_ITEM_REJECT_CHARGES_V2',
    'candidate.count_by_charges()',
    'NCMM_VIRTUAL_ITEM_REJECT_LIQUIDS_V2',
    'candidate.made_of( phase_id::LIQUID )',
    'candidate.made_of( phase_id::GAS )',
    'NCMM_VIRTUAL_ITEM_REQUIRE_TWO_HANDED_V2',
    'NCMM_VIRTUAL_ITEM_REJECT_GUNS_V2',
    '!allow_physical_wielded'
)){
    if(-not $runtimeValidSection.Contains($n)){throw ('Virtual-item persisted restriction runtime check missing: '+$n)}
}
foreach($n in @(
    'return virtual_item_candidate_valid_impl( candidate, flags, false );',
    'physically_wielded && std::string_view( module_id ) == survivor_module_id',
    'virtual_item_candidate_valid_impl(',
    'candidate, flags, survivor_wield_transfer )'
)){
    if(-not $hostSourceCurrent.Contains($n)){throw ('Mana Hand wield-transfer isolation contract missing: '+$n)}
}
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
# AWS 0.6.3 manifest migration must follow the real generated 0.6.1 manifest contract:
# API 1.7 and requires ending in api.versioning.v1.  It must not depend on ui.theme.v1.
foreach($n in @('[int]$manifestObj.api_min_minor -ne 7','$manifestObj.api_min_minor = 9',"'host_api.v2.core','settings.typed.v2','worldgen.bindings.v2'",'AWS 0.6.3 expected exactly 48 geography bindings')){
    if(-not $payload.Contains($n)){throw ('AWS 0.6.3 semantic manifest/binding migration contract missing: '+$n)}
}
$staleAwsMigration='AWS 0.6.3 manifest capability anchor missing.'
if($payload.Contains($staleAwsMigration)){throw ('Stale AWS 0.6.3 manifest migration contract returned: '+$staleAwsMigration)}
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
$awsManifestProbe.version='0.6.3';$awsManifestProbe.api_min_minor=9;$awsManifestProbeReq=@($awsManifestProbe.requires)
foreach($cap in @('host_api.v2.core','settings.typed.v2','worldgen.bindings.v2')){if($awsManifestProbeReq -notcontains $cap){$awsManifestProbeReq += $cap}}
$awsManifestProbe.requires=@($awsManifestProbeReq);$awsManifestProbeRoundTrip=(($awsManifestProbe|ConvertTo-Json -Depth 8)|ConvertFrom-Json)
if([string]$awsManifestProbeRoundTrip.version -ne '0.6.3' -or [int]$awsManifestProbeRoundTrip.api_min_minor -ne 9){throw 'AWS 0.6.3 semantic manifest migration regression failed.'}
foreach($cap in @('host_api.v2.core','settings.typed.v2','worldgen.bindings.v2')){if(@($awsManifestProbeRoundTrip.requires|Where-Object{$_ -eq $cap}).Count -ne 1){throw ('AWS 0.6.3 semantic manifest capability regression failed: '+$cap)}}

# AWS 0.6.4 selective-scope safety contract. The historical 0.6.3 migration
# above remains intentional; this pass hardens it without rewriting old fixtures.
foreach($n in @(
    'function Apply-AwsSelectiveScopes064',
    'AWS 0.6.4 expected exactly 50 geography bindings',
    'NCMM_AWS_SCOPE_CITIES',
    'NCMM_AWS_SCOPE_ECOLOGY',
    'NCMM_AWS_SCOPE_WATER',
    'NCMM_AWS_SCOPE_TRANSPORT',
    'geography.scope.cities.enabled',
    'geography.scope.ecology.enabled',
    'geography.scope.water.enabled',
    'geography.scope.transport.enabled',
    'const bool ncmm_geo_trails'
)){
    if(-not $payload.Contains($n)){throw ('AWS 0.6.4 selective-scope contract missing: '+$n)}
}
if(-not $payload.Contains('Remove-AwsLineContaining $aws ''NCMM_AWS_PLACE_SPECIALS","Generate special locations"''')){
    throw 'AWS 0.6.4 protected special-location control removal missing.'
}
if(-not $payload.Contains('Remove-AwsLineContaining $aws ''NCMM_AWS_NEIGHBOR_CONNECTIONS","Connect neighboring map regions"''')){
    throw 'AWS 0.6.4 protected neighbor-connection control removal missing.'
}


# Scope-aware worldgen dispatch is final in the checked-in canonical Host.
foreach($awsScopeHostNeedle0169 in @(
    'bool worldgen_scope_hook_enabled( const char *scope_hook_id )',
    'bool worldgen_hook_scope_enabled( const char *hook_id )',
    'id.rfind( "geography.scope.", 0 ) == 0',
    'scope_hook = "geography.scope.cities.enabled"',
    'scope_hook = "geography.scope.ecology.enabled"',
    'scope_hook = "geography.scope.water.enabled"',
    'scope_hook = "geography.scope.transport.enabled"',
    'return scope_hook == nullptr || worldgen_scope_hook_enabled( scope_hook );',
    'worldgen_hook_scope_enabled( hook_id );',
    'aws_setting_count != 50',
    'aws_hook_count != 50'
)){
    if(-not $hostSourceCurrent.Contains($awsScopeHostNeedle0169)){
        throw ('Canonical AWS selective-scope Host surface missing: '+$awsScopeHostNeedle0169)
    }
}

$awsScopeStart0169=$payload.IndexOf('function Apply-AwsSelectiveScopes064')
$awsScopeEnd0169=$payload.IndexOf('function Apply-PlayerFacingCopyPolishFinal',$awsScopeStart0169)
if($awsScopeStart0169 -lt 0 -or $awsScopeEnd0169 -le $awsScopeStart0169){
    throw 'AWS 0.6.4 selective-scope payload boundary missing.'
}
$awsScopeSection0169=$payload.Substring($awsScopeStart0169,$awsScopeEnd0169-$awsScopeStart0169)
foreach($awsScopeNeedle0169 in @(
    'AWS 0.6.4 expected exactly 50 geography bindings',
    'NCMM_AWS_SCOPE_CITIES',
    'NCMM_AWS_SCOPE_ECOLOGY',
    'NCMM_AWS_SCOPE_WATER',
    'NCMM_AWS_SCOPE_TRANSPORT',
    'canonical Host.  This compatibility stage advances only the AWS module/manifest.'
)){
    if(-not $awsScopeSection0169.Contains($awsScopeNeedle0169)){
        throw ('AWS selective-scope module contract missing: '+$awsScopeNeedle0169)
    }
}
foreach($awsScopeForbidden0169 in @(
    '$loader = Normalize-Lf ([IO.File]::ReadAllText($loaderPath))',
    'Write-Utf8NoBom $loaderPath $loader',
    '$loaderAudit064',
    'worldgen_hook_scope_enabled',
    'aws_setting_count != 50',
    'aws_hook_count != 50'
)){
    if($awsScopeSection0169.Contains($awsScopeForbidden0169)){
        throw ('AWS selective-scope migration still rewrites transient Host: '+$awsScopeForbidden0169)
    }
}

foreach($railroadFallbackNeedle064 in @(
    'const bool ncmm_place_railroads = ncmm_geo &&',
    'get_options().has_option( "NCMM_AWS_PLACE_RAILROADS" ) ?',
    'get_option<bool>( "NCMM_AWS_PLACE_RAILROADS" ) :',
    'settings->place_railroads;'
)){
    if(-not $payload.Contains($railroadFallbackNeedle064)){
        throw ('AWS 0.6.4 railroad region-fallback contract missing: '+$railroadFallbackNeedle064)
    }
}

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

foreach($uiNeedle in @(
    'NCMM world settings Default/Experimental split',
    'draw_world_page_tab( iWorldOptPage, "Default" )',
    'draw_world_page_tab( iExperimentalPage, "Experimental" )',
    'worldgen_visible ? "ncmm_experimental" : "world_default"',
    'const size_t card_body_lines = detail_panel ? 2 : 3;'
)){
    if(-not $payload.Contains($uiNeedle)){throw ('NCMM world-settings/card UI regression contract missing: '+$uiNeedle)}
}
$deferPos0831=$payload.IndexOf('name.rfind( "NCMM_", 0 ) == 0')
$registerPos0831=$payload.IndexOf('bool options_manager::ncmm_register_world_bool(')
if($deferPos0831 -lt 0 -or $registerPos0831 -lt 0 -or $deferPos0831 -gt $registerPos0831){throw 'Deferred NCMM deserialize gate must be generated before runtime setting registration implementations.'}

foreach($n in @(
    'Apply-NcmmManagerUiV1Source',
    'module_setting_meta',
    'manager_adjust_setting',
    'manager_persist_settings',
    'NCMM_MANAGER',
    'MODULE DETAILS'
)){if(-not $payload.Contains($n)){throw ('NCMM manager/settings payload contract missing: '+$n)}}

# The canonical checked-in Host, not the historical ManagerUi migration, owns
# the final manager surface.  Keep these semantics independently guarded before
# pruning any legacy source-generation stage.
foreach($managerHostNeedle0167 in @(
    'struct module_setting_meta {',
    'std::string manager_description( const std::filesystem::path &directory )',
    'std::vector<const module_setting_meta *> manager_settings_for( const std::string &module_id )',
    'std::string manager_setting_value( const module_setting_meta &setting )',
    'bool manager_adjust_setting( const module_setting_meta &setting, int direction )',
    'bool manager_persist_settings()',
    'void show_manager()',
    'input_context ctxt( "NCMM_MANAGER", keyboard_mode::keychar );',
    'tr_ui( "MODULE DETAILS",',
    'std::string settings_menu_label()',
    'std::string version_label()'
)){
    if(-not $hostSourceCurrent.Contains($managerHostNeedle0167)){
        throw ('Canonical NCMM manager Host surface missing: '+$managerHostNeedle0167)
    }
}
foreach($managerHeaderNeedle0167 in @(
    'std::string settings_menu_label();',
    'std::string version_label();',
    'void register_gameplay_actions( input_context &ctxt );',
    'bool handle_gameplay_action( const std::string &action );'
)){
    if(-not $hostHeaderCurrent.Contains($managerHeaderNeedle0167)){
        throw ('Canonical NCMM manager Host declaration missing: '+$managerHeaderNeedle0167)
    }
}


# Player-facing copy is also final in the checked-in canonical Host.  The legacy
# copy-polish stage must not rewrite transient Host sources before canonical sync.
foreach($playerCopyHostNeedle0168 in @(
    'No NCMM mods are installed.',
    'ON / error',
    'ON / needs attention',
    'ON / incompatible',
    'ON / restart required',
    'Open mod interface',
    'Disable mod',
    'Mod disabled. Restart CDDA to apply.'
)){
    if(-not $hostSourceCurrent.Contains($playerCopyHostNeedle0168)){
        throw ('Canonical NCMM player-facing Host copy missing: '+$playerCopyHostNeedle0168)
    }
}
foreach($obsoletePlayerCopyHostNeedle0168 in @(
    'No NCMM code mods are installed.',
    'ON / quarantined',
    'ON / suspended',
    'ON / rejected',
    'ON / not loaded',
    'Disable module',
    'Module disabled. Restart CDDA to apply.'
)){
    if($hostSourceCurrent.Contains($obsoletePlayerCopyHostNeedle0168)){
        throw ('Canonical NCMM Host still contains obsolete manager copy: '+$obsoletePlayerCopyHostNeedle0168)
    }
}

$playerCopyStart0168=$payload.IndexOf('function Apply-PlayerFacingCopyPolishFinal')
$playerCopyEnd0168=$payload.IndexOf('function Apply-SurvivorManaHands0130',$playerCopyStart0168)
if($playerCopyStart0168 -lt 0 -or $playerCopyEnd0168 -le $playerCopyStart0168){
    throw 'Player-facing copy polish payload boundary missing.'
}
$playerCopySection0168=$payload.Substring($playerCopyStart0168,$playerCopyEnd0168-$playerCopyStart0168)
foreach($playerCopyNeedle0168 in @(
    'Write-Utf8NoBom $awsPath $aws',
    'std::string rpg_detail_body( const perk_def &perk, const std::string &body,',
    'Copy-Item $spPath',
    'checked-in canonical Host owns final manager/player-facing copy'
)){
    if(-not $playerCopySection0168.Contains($playerCopyNeedle0168)){
        throw ('Player-facing copy polish payload contract missing: '+$playerCopyNeedle0168)
    }
}
foreach($playerCopyForbidden0168 in @(
    '$loader = Normalize-Lf ([IO.File]::ReadAllText($loaderPath))',
    'Write-Utf8NoBom $loaderPath $loader',
    'No NCMM code mods are installed.',
    'ON / quarantined'
)){
    if($playerCopySection0168.Contains($playerCopyForbidden0168)){
        throw ('Player-facing copy polish still rewrites transient Host: '+$playerCopyForbidden0168)
    }
}


# Real gameplay smoke must remain generated from the portable payload as well as
# present in the checked-in Host source.  This guards the semantic QA path itself.
foreach($gameplaySmokeNeedle in @(
    'bool gameplay_smoke_requested()',
    'int run_gameplay_smoke()',
    '--ncmm-gameplay-smoke',
    'gameplay-smoke.json',
    'NCMM Gameplay Smoke',
    'survivor_perk_count < survivor_minimum_perk_count',
    'overmap_buffer.create_custom_overmap',
    'ncmm_test_perk_count_v1',
    'constexpr size_t survivor_minimum_perk_count = 373;',
    'find_perk_index( "mg_mana_hand_3" )',
    'find_perk_index( "mg_mana_hand_4" )',
    'find_perk_index( "mg_dimensional_pouch" )',
    'dimensional_pouch_check_count',
    'main-menu.ncmm-gameplay-smoke'
)){
    if(-not $payload.Contains($gameplaySmokeNeedle)){
        throw ('NCMM real gameplay smoke payload contract missing: '+$gameplaySmokeNeedle)
    }
}
$gameplayHost=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch\ncmm_loader.cpp'))
foreach($gameplayHostNeedle in @(
    'bool gameplay_smoke_requested()',
    'int run_gameplay_smoke()',
    'world->save()',
    'world_generator->get_world( world_name )',
    'overmap_buffer.create_custom_overmap',
    'worldgen_hook_scope_enabled',
    'aws_setting_count != 50',
    'aws_hook_count != 50',
    'aws_scope_fallback_mismatch',
    'aws_protected_hook_exposed',
    'survivor_perk_count < survivor_minimum_perk_count',
    'constexpr size_t survivor_minimum_perk_count = 373;',
    'find_perk_index( "mg_mana_hand_3" )',
    'find_perk_index( "mg_mana_hand_4" )',
    'find_perk_index( "mg_dimensional_pouch" )',
    'dimensional_pouch_check_count',
    'pocket_data *dimensional_pouch_container_pocket()',
    'pocket.type != pocket_type::CONTAINER',
    'pocket_data *pocket = dimensional_pouch_container_pocket();',
    'dimensional_pouch_pocket = dimensional_pouch_container_pocket();',
    'survivor_real_strength_mismatch',
    'survivor_real_carry_mismatch'
)){
    if(-not $gameplayHost.Contains($gameplayHostNeedle)){
        throw ('NCMM real gameplay smoke Host contract missing: '+$gameplayHostNeedle)
    }
}
if($gameplayHost.Contains('dimensional_pouch_type_id.obj().pockets.size() != 1')){
    throw 'Dimensional Pouch regressed to raw itype pocket-count validation; CDDA adds MIGRATION pockets during finalize.'
}
if($gameplayHost.Contains('dimensional_pouch_type_id.obj().pockets.front()')){
    throw 'Dimensional Pouch regressed to raw itype pocket ordering instead of selecting the CONTAINER pocket.'
}

# Mana Hands control item: exact-id activation bridge only.
$manaUiSdk=[IO.File]::ReadAllText((Join-Path $PackageRoot 'sdk\ncmm_api.h'))
$manaUiHost=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch\ncmm_loader.cpp'))
$manaUiHeader=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch\ncmm_loader.h'))
foreach($needle0155ui in @(
    '#define NCMM_ITEM_ACTIVATE_ENTRYPOINT "ncmm_on_item_activate_v1"',
    'typedef int ( *ncmm_on_item_activate_v1_fn )'
)){
    if(-not $manaUiSdk.Contains($needle0155ui)){throw ('Mana Hands item callback SDK contract missing: '+$needle0155ui)}
}
foreach($needle0155ui in @(
    'item_id != "ncmm_survivor_mana_hand_carrier"',
    'find_loaded_by_id( "survivor_progression" )',
    'mod->item_activate( &api, item_id.c_str() )',
    'GetProcAddress( module, NCMM_ITEM_ACTIVATE_ENTRYPOINT )'
)){
    if(-not $manaUiHost.Contains($needle0155ui)){throw ('Mana Hands exact item activation Host contract missing: '+$needle0155ui)}
}
if(-not $manaUiHeader.Contains('bool handle_item_activation( const item_location &loc );')){
    throw 'Mana Hands direct UI Host declaration missing.'
}
foreach($needle0155ui in @(
    'function Apply-SurvivorManaHandsDirectUi0155',
    'method.empty() && ncmm::handle_item_activation( loc )',
    '// NCMM exact-id Mana Hands control item.'
)){
    if(-not $payload.Contains($needle0155ui)){throw ('Mana Hands direct UI payload contract missing: '+$needle0155ui)}
}

Write-Host 'NCMM Host/AWS payload regression contract: PASS' -ForegroundColor Green
