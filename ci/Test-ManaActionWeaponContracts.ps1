param(
    [string]$PackageRoot=(Split-Path $PSScriptRoot -Parent),
    [string]$PatchedSourceRoot='',
    [switch]$RunBehavior
)
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path
$hostSource=[IO.File]::ReadAllText((Join-Path $PackageRoot 'host_patch/ncmm_loader.cpp'))
$payload=[IO.File]::ReadAllText((Join-Path $PackageRoot 'payload/SURVIVOR_0911_0915_v8.7.6.8.ps1'))

function Get-CppFunction([string]$Text,[string]$Signature) {
    $start=$Text.IndexOf($Signature)
    if($start -lt 0){throw ('Missing function: '+$Signature)}
    $open=$Text.IndexOf('{',$start)
    $depth=0
    for($i=$open;$i -lt $Text.Length;$i++) {
        if($Text[$i] -eq '{'){$depth++}
        if($Text[$i] -eq '}'){
            $depth--
            if($depth -eq 0){return $Text.Substring($start,$i-$start+1)}
        }
    }
    throw ('Unclosed function: '+$Signature)
}
function Require([string]$Text,[string]$Needle) {
    if(-not $Text.Contains($Needle)){throw ('Mana action contract missing: '+$Needle)}
}
$capable=Get-CppFunction $hostSource 'bool ranged_weapon_capable('
$resolver=Get-CppFunction $hostSource 'std::vector<item_location> ranged_weapon_candidates('
$binding=Get-CppFunction $hostSource 'bool ranged_weapon_binding_valid('
$activeItems=Get-CppFunction $hostSource 'std::vector<item *> active_mana_hand_items('
$itemLabel=Get-CppFunction $hostSource 'std::string mana_hand_item_label('
$rangedLabel=Get-CppFunction $hostSource 'std::string ranged_weapon_label('
$itemSlot=Get-CppFunction $hostSource 'mana_hand_item_slot mana_hand_item_slot_of('
$itemOwner=Get-CppFunction $hostSource 'mana_hand_ranged_owner mana_hand_ranged_item_owner('
$owner=Get-CppFunction $hostSource 'mana_hand_ranged_owner mana_hand_ranged_mode_owner('
$melee=Get-CppFunction $hostSource 'item *primary_mana_hand_melee_weapon('
Require $capable '!weapon.is_gun() || weapon.is_gunmod()'
Require $capable 'return mode && !mode.melee();'
Require $resolver 'physical && ranged_weapon_capable( *physical, action )'
Require $resolver 'active_mana_hand_items( who )'
Require $resolver 'ranged_weapon_capable( *candidate, action )'
Require $activeItems 'survivor_mana_hand_count()'
Require $activeItems 'mana_hands_pair_slot_id'
Require $activeItems 'mana_hand_3_slot_id'
Require $activeItems 'mana_hand_4_slot_id'
Require $activeItems 'result.push_back( paired )'
Require $activeItems 'return result'
Require $itemLabel 'virtual_item_marker_key'
Require $itemLabel 'mana_hands_pair_slot_id'
Require $itemLabel 'mana_hand_3_slot_id'
Require $itemLabel 'Mana Hands III+IV'
Require $itemLabel 'Mana Hand III'
Require $itemLabel 'Mana Hand IV'
Require $itemLabel 'candidate.tname()'
Require $rangedLabel 'return mana_hand_item_label( weapon );'
foreach($forbiddenRangedLabelPolicy in @('virtual_item_marker_key', 'mana_hands_pair_slot_id', 'mana_hand_3_slot_id', 'Mana Hands III+IV', 'Mana Hand III', 'Mana Hand IV')) {
    if($rangedLabel.Contains($forbiddenRangedLabelPolicy)){throw ('Ranged label duplicated generic Mana Hand label policy: '+$forbiddenRangedLabelPolicy)}
}
foreach($forbiddenResolverPolicy in @('survivor_mana_hand_count()', 'virtual_item_for_slot(', 'mana_hand_3_slot_id', 'mana_hand_4_slot_id', 'mana_hands_pair_slot_id')) {
    if($resolver.Contains($forbiddenResolverPolicy)){throw ('Ranged resolver duplicated active Mana Hand enumeration policy: '+$forbiddenResolverPolicy)}
}
Require $binding 'ranged_weapon_capable( weapon, ranged_weapon_action::fire )'
Require $binding 'mana_hand_ranged_item_owner( who, weapon )'
Require $itemSlot 'survivor_mana_hand_count()'
Require $itemSlot 'virtual_item_matches_slot('
Require $itemSlot 'mana_hands_pair_slot_id'
Require $itemSlot 'mana_hand_3_slot_id'
Require $itemSlot 'mana_hand_4_slot_id'
Require $itemSlot 'mana_hand_item_slot::paired'
Require $itemSlot 'mana_hand_item_slot::hand3'
Require $itemSlot 'mana_hand_item_slot::hand4'
Require $itemOwner '!weapon.is_gun() || weapon.is_gunmod()'
Require $itemOwner 'mana_hand_item_slot_of( who, weapon )'
Require $itemOwner 'mana_hand_ranged_owner::paired'
Require $itemOwner 'mana_hand_ranged_owner::single'
foreach($forbiddenItemOwner in @('virtual_item_matches_slot(', 'survivor_mana_hand_count()', 'mana_hand_3_slot_id', 'mana_hand_4_slot_id', 'mana_hands_pair_slot_id')) {
    if($itemOwner.Contains($forbiddenItemOwner)){throw ('Ranged item owner duplicated generic Mana Hand slot policy: '+$forbiddenItemOwner)}
}
foreach($forbiddenBinding in @('virtual_item_matches_slot(', 'mana_hand_3_slot_id', 'mana_hand_4_slot_id', 'mana_hands_pair_slot_id')) {
    if($binding.Contains($forbiddenBinding)){throw ('Aim binding duplicated Mana Hand item-slot policy: '+$forbiddenBinding)}
}
Require $owner 'mana_hands_pair_slot_id'
Require $owner 'base->gunmods()'
Require $owner 'mana_hand_ranged_owner::paired'
Require $owner 'mana_hand_ranged_owner::single'
Require $melee 'who.get_wielded_item()'
Require $melee 'survivor_mana_hand_count()'
Require $melee 'candidate->is_melee() && !candidate->is_gun()'
foreach($forbidden in @('all_items_loc(', '.obtain(', '.wield(', 'remove_weapon(')) {
    if($resolver.Contains($forbidden)){throw ('Ranged resolver must not scan/move items: '+$forbidden)}
}
$stack=Get-Content (Join-Path $PackageRoot 'ci/host-patch-stack.json') -Raw|ConvertFrom-Json
$fireIndex=[Array]::IndexOf([string[]]$stack.layers,'Apply-SurvivorManaHandFireAction0140')
$reachIndex=[Array]::IndexOf([string[]]$stack.layers,'Apply-SurvivorManaHandReachMelee0140')
$layerIndex=[Array]::IndexOf([string[]]$stack.layers,'Apply-SurvivorActionWeaponSelection0154')
if($fireIndex -lt 0 -or $reachIndex -lt 0 -or $layerIndex -lt 0 -or
   $fireIndex -ge $reachIndex -or $layerIndex -le $reachIndex){
    throw 'FIRE base dispatch must precede reach, with ActionWeaponSelection remaining a later verifier/entry-point layer.'
}
Require $payload 'Apply-SurvivorActionWeaponSelection0154 $CddaRoot'
$actionStart=$payload.IndexOf('function Apply-SurvivorActionWeaponSelection0154')
$actionEnd=$payload.IndexOf('function Apply-SurvivorManaHandsDirectUi0155',$actionStart)
if($actionStart -lt 0 -or $actionEnd -le $actionStart){throw 'ActionWeaponSelection payload boundary missing.'}
$actionSection=$payload.Substring($actionStart,$actionEnd-$actionStart)
Require $actionSection 'Physical reach intercepted F before base-layer ranged capability resolution.'
foreach($forbiddenActionMutation in @(
    'Write-Utf8NoBom $handlePath',
    '$handle = $handle.Substring(',
    '$start = $handle.IndexOf(''    // try reach weapon'')',
    '$reach = $handle.Substring('
)){
    if($actionSection.Contains($forbiddenActionMutation)){
        throw ('ActionWeaponSelection regressed to late handle_action mutation: '+$forbiddenActionMutation)
    }
}

if($PatchedSourceRoot) {
    $handle=[IO.File]::ReadAllText((Join-Path $PatchedSourceRoot 'src/handle_action.cpp'))
    $actor=[IO.File]::ReadAllText((Join-Path $PatchedSourceRoot 'src/activity_actor.cpp'))
    $avatar=[IO.File]::ReadAllText((Join-Path $PatchedSourceRoot 'src/avatar_action.cpp'))
    $ranged=[IO.File]::ReadAllText((Join-Path $PatchedSourceRoot 'src/ranged.cpp'))
    $fire=Get-CppFunction $handle 'static void fire('
    $aim=Get-CppFunction $actor 'item_location aim_activity_actor::get_weapon()'
    $entry=Get-CppFunction $avatar 'void avatar_action::fire_wielded_weapon('
    Require $fire 'if( ncmm_fire_candidates.empty() )'
    if($fire.IndexOf('ncmm::ranged_weapon_candidates(') -gt $fire.IndexOf('reach_attack( you )')) {
        throw 'Physical reach intercepted F before ranged capability resolution.'
    }
    Require $fire 'ncmm::select_ranged_weapon('
    Require $fire 'aim_activity_actor::use_item_location( ncmm_selected_gun )'
    Require $handle 'return ncmm::select_ranged_weapon('
    Require $handle 'ncmm::ranged_weapon_action::reload'
    Require $handle 'ncmm::ranged_weapon_action::controls'
    Require $entry 'ncmm::select_ranged_weapon('
    Require $entry 'aim_activity_actor::use_item_location( weapon )'
    Require $aim 'ncmm::ranged_weapon_binding_valid('
    $common=Get-CppFunction $ranged 'bool gunmode_checks_common('
    Require $common 'ncmm::mana_hand_ranged_mode_owner('
    Require $common 'ncmm::mana_hand_ranged_owner::paired'
    foreach($forbiddenOwner in @('virtual_item_for_slot(', 'gameplay_modifier(', 'runtime_hook_modifier(', '"mana_hand_3"', '"mana_hand_4"', '"mana_hands_34"')) {
        if($common.Contains($forbiddenOwner)){throw ('Ranged common check duplicated Mana Hand ownership policy: '+$forbiddenOwner)}
    }
    foreach($forbidden in @('all_items_loc(', 'virtual_item_for_slot(', 'select_ranged_weapon(', 'gameplay_modifier(', 'virtual_item_matches_slot(', 'mana_hand_3', 'mana_hand_4', 'mana_hands_34')) {
        if($aim.Contains($forbidden)){throw ('Aim must validate its selected item without reselection/scans: '+$forbidden)}
    }
    foreach($text in @($fire,$entry)) {
        foreach($forbidden in @('.obtain(', 'you.wield(', 'remove_weapon(')) {
            if($text.Contains($forbidden)){throw ('FIRE moved a carrier item: '+$forbidden)}
        }
    }
    $game=[IO.File]::ReadAllText((Join-Path $PatchedSourceRoot 'src/game.cpp'))
    Require $game '!ncmm::is_virtual_item( *loc )'
}

if($RunBehavior) {
    $work=Join-Path $env:TEMP ('ncmm-action-resolver-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work|Out-Null
    [IO.File]::WriteAllText((Join-Path $work 'ranged_resolvers.inc'),$capable+"`n"+$activeItems+"`n"+$itemSlot+"`n"+$itemOwner+"`n"+$owner+"`n"+$binding+"`n"+$resolver)
    [IO.File]::WriteAllText((Join-Path $work 'primary_melee_resolver.inc'),$melee)
    Copy-Item (Join-Path $PackageRoot 'tests/mana_action_weapon_test.cpp') $work
    [IO.File]::WriteAllText((Join-Path $work 'CMakeLists.txt'),@'
cmake_minimum_required(VERSION 3.20)
project(ManaActionResolver LANGUAGES CXX)
add_executable(mana_action_weapon_test mana_action_weapon_test.cpp)
target_compile_features(mana_action_weapon_test PRIVATE cxx_std_17)
'@)
    & cmake -S $work -B (Join-Path $work 'build') -A x64
    if($LASTEXITCODE -ne 0){throw 'Mana resolver test configure failed.'}
    & cmake --build (Join-Path $work 'build') --config Release
    if($LASTEXITCODE -ne 0){throw 'Mana resolver test build failed.'}
    & (Join-Path $work 'build/Release/mana_action_weapon_test.exe')
    if($LASTEXITCODE -ne 0){throw 'Mana resolver behavior regression.'}
}
Write-Host 'Mana action weapon contracts: PASS' -ForegroundColor Green
