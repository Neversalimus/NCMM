param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$PackageRoot=(Resolve-Path $PackageRoot).Path

. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')
. (Join-Path $PackageRoot 'tools\NCMM.Update.Common.ps1')

function Assert-NcmmFieldEqual([string]$Id,[string]$Field,$Expected,$Actual) {
    if([string]$Expected -ne [string]$Actual){
        throw "Component '$Id' field '$Field' drift: catalog='$Expected' descriptor='$Actual'."
    }
}

$catalogPath=Join-Path $PackageRoot 'components\index.json'
$catalog=Get-Content $catalogPath -Raw|ConvertFrom-Json
if([int]$catalog.schema -ne 1 -or [string]$catalog.infrastructure_version -ne '0.8.3.1'){
    throw 'Component catalog schema/infrastructure mismatch.'
}

$components=@($catalog.components)
$ids=@($components|ForEach-Object{[string]$_.id})
if(@($ids|Select-Object -Unique).Count -ne $ids.Count){throw 'Component catalog contains duplicate ids.'}
foreach($id in @('ncmm_infrastructure','ncmm_host','survivor_progression','advanced_world_settings','ballistic_hit_chance','recipe_finalize_profiler')){
    if(@($components|Where-Object{[string]$_.id -eq $id}).Count -ne 1){throw "Component missing/duplicate: $id"}
}

# Every component must belong to exactly the declared atomic group, with no ghost ids.
$groupMembership=@{}
foreach($property in @($catalog.atomic_groups.PSObject.Properties)){
    foreach($id in @($property.Value)){
        $key=[string]$id
        if($ids -notcontains $key){throw "Atomic group '$($property.Name)' references unknown component '$key'."}
        if(-not $groupMembership.ContainsKey($key)){$groupMembership[$key]=@()}
        $groupMembership[$key]=@($groupMembership[$key])+[string]$property.Name
    }
}
foreach($component in $components){
    $id=[string]$component.id
    $groups=@($groupMembership[$id])
    if($groups.Count -ne 1 -or $groups[0] -ne [string]$component.atomic_group){
        throw "Component '$id' atomic-group membership drift."
    }

    $descriptorRel=[string]$component.descriptor
    $descriptorPath=Join-Path $PackageRoot ($descriptorRel.Replace('/',[IO.Path]::DirectorySeparatorChar))
    if(-not(Test-Path $descriptorPath -PathType Leaf)){throw "Component descriptor missing: $descriptorRel"}
    $descriptor=Get-Content $descriptorPath -Raw|ConvertFrom-Json

    foreach($field in @('id','name','version','kind','atomic_group','delivery','required','descriptor')){
        Assert-NcmmFieldEqual $id $field $component.$field $descriptor.$field
    }

    $catalogDeps=@($component.dependencies)
    $descriptorDeps=@($descriptor.dependencies)
    if($catalogDeps.Count -ne $descriptorDeps.Count){throw "Component '$id' dependency count drift."}
    for($i=0;$i -lt $catalogDeps.Count;$i++){
        foreach($field in @('component','min_version','capability')){
            Assert-NcmmFieldEqual $id ("dependencies[$i].$field") $catalogDeps[$i].$field $descriptorDeps[$i].$field
        }
    }

    $catalogProvides=@($component.provides|ForEach-Object{[string]$_}|Sort-Object)
    $descriptorProvides=@($descriptor.provides|ForEach-Object{[string]$_}|Sort-Object)
    if(($catalogProvides -join "`n") -ne ($descriptorProvides -join "`n")){
        throw "Component '$id' capability provider list drift."
    }
}

$hostComponent=@($components|Where-Object{$_.id -eq 'ncmm_host'})[0]
$survivor=@($components|Where-Object{$_.id -eq 'survivor_progression'})[0]
$aws=@($components|Where-Object{$_.id -eq 'advanced_world_settings'})[0]
$ballistic=@($components|Where-Object{$_.id -eq 'ballistic_hit_chance'})[0]
if(-not [bool]$hostComponent.required){throw 'NCMM Host must remain required.'}
if([bool]$survivor.required -or [bool]$aws.required -or [bool]$ballistic.required){throw 'Gameplay modules must remain independently optional.'}
if(@([string]$survivor.atomic_group,[string]$aws.atomic_group,[string]$ballistic.atomic_group|Select-Object -Unique).Count -ne 3){throw 'Optional gameplay modules must keep separate atomic groups.'}

$moduleManifests=@{
    survivor_progression='mods\SurvivorProgression\mod.json'
    advanced_world_settings='mods\AdvancedWorldSettings\mod.json'
    ballistic_hit_chance='mods\BallisticHitChance\mod.json'
    item_glyphs='mods\ItemGlyphs\mod.json'
}
foreach($id in $moduleManifests.Keys){
    $component=@($components|Where-Object{$_.id -eq $id})[0]
    $manifest=Get-Content (Join-Path $PackageRoot $moduleManifests[$id]) -Raw|ConvertFrom-Json
    foreach($field in @('id','name','version')){
        Assert-NcmmFieldEqual $id ("mod.json.$field") $component.$field $manifest.$field
    }
}

$feed=Get-Content (Join-Path $PackageRoot 'compat\update\index.json') -Raw|ConvertFrom-Json
if([string]$feed.infrastructure_version -ne [string]$catalog.infrastructure_version){throw 'Update feed infrastructure version drift.'}
$release=@($feed.releases)[0]
if($null -eq $release){throw 'Update feed has no release.'}
foreach($component in $components){
    $id=[string]$component.id
    if($release.components.PSObject.Properties.Name -notcontains $id){throw "Update feed component missing: $id"}
    if([string]$release.components.$id -ne [string]$component.version){throw "Update feed version drift for $id."}
}

# Preserve the Windows PowerShell 5.1 collection-shape regression guard.
$capSetProbe=Get-NcmmReleaseCapabilitySet $catalog $release
if(-not ($capSetProbe -is [hashtable])){throw ('PS5.1 capability set type drift: '+$capSetProbe.GetType().FullName)}
$capSetProbe['__ncmm_ps51_mutation_probe__']=$true
if(-not $capSetProbe.ContainsKey('__ncmm_ps51_mutation_probe__')){throw 'PS5.1 capability set mutation probe failed.'}
$capSetProbe.Remove('__ncmm_ps51_mutation_probe__')

# Dependency planning must keep gameplay modules independent while pulling Host when needed.
$tmp=Join-Path $env:TEMP ('NCMM_COMPONENT_TEST_'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force (Join-Path $tmp 'ncmm')|Out-Null
try{
    [IO.File]::WriteAllText(
        (Join-Path $tmp 'VERSION.txt'),
        'commit sha: 3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'+"`r`n",
        (New-Object Text.UTF8Encoding($false))
    )
    [IO.File]::WriteAllBytes((Join-Path $tmp 'cataclysm-tiles.exe'),(New-Object byte[] 1))
    Write-NcmmUtf8NoBom (Join-Path $tmp 'ncmm\host.binding.json') (([ordered]@{
        ncmm_version=[string]$hostComponent.version
        source_commit='3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'
    }|ConvertTo-Json)+"`n")
    Write-NcmmUtf8NoBom (Join-Path $tmp 'ncmm\modules.state.json') (([ordered]@{
        capabilities=@('api.versioning.v1','active_mods.registry.v2','world_settings.v2','host_api.v2.core','settings.typed.v2','character.modifiers.v2','runtime_hooks.registry.v2','runtime_settings.bindings.v2')
        modules=@(
            @{id='survivor_progression';version=[string]$survivor.version},
            @{id='advanced_world_settings';version=[string]$aws.version},
            @{id='ballistic_hit_chance';version=[string]$ballistic.version}
        )
    }|ConvertTo-Json -Depth 6)+"`n")

    $plan=Resolve-NcmmDependencyPlan $PackageRoot $tmp $feed @('survivor_progression')
    if($plan.expanded -notcontains 'survivor_progression'){throw 'Survivor missing from independent update plan.'}
    if($plan.expanded -contains 'advanced_world_settings'){throw 'AWS was incorrectly pulled into Survivor update plan.'}
    if($plan.expanded -contains 'recipe_finalize_profiler'){throw 'Optional profiler was incorrectly pulled into Survivor update plan.'}
    if($plan.expanded -contains 'ballistic_hit_chance'){throw 'Ballistic Hit Chance was incorrectly pulled into Survivor update plan.'}

    $ballisticPlan=Resolve-NcmmDependencyPlan $PackageRoot $tmp $feed @('ballistic_hit_chance')
    if($ballisticPlan.expanded -notcontains 'ballistic_hit_chance'){throw 'Ballistic Hit Chance missing from independent update plan.'}
    foreach($other in @('survivor_progression','advanced_world_settings','recipe_finalize_profiler')){
        if($ballisticPlan.expanded -contains $other){throw "Unrelated component '$other' was incorrectly pulled into Ballistic Hit Chance update plan."}
    }

    Remove-Item (Join-Path $tmp 'ncmm\host.binding.json') -Force
    $planWithoutHost=Resolve-NcmmDependencyPlan $PackageRoot $tmp $feed @('survivor_progression')
    foreach($id in @('ncmm_host','survivor_progression')){
        if($planWithoutHost.expanded -notcontains $id){throw "Missing dependency-expanded component: $id"}
    }
    if($planWithoutHost.expanded -contains 'advanced_world_settings'){throw 'AWS was incorrectly pulled into dependency-expanded Survivor plan.'}

    $migration=Invoke-NcmmUpdateStateMigration $PackageRoot $tmp
    if([int]$migration.state.schema -ne 2){throw 'Update-state migration did not reach schema 2.'}
    $migration2=Invoke-NcmmUpdateStateMigration $PackageRoot $tmp
    if(@($migration2.migrations).Count -ne 0){throw 'Update-state migrations are not idempotent.'}
}finally{
    Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
}

$compatibility=Get-Content (Join-Path $PackageRoot 'compat\compatibility.manifest.json') -Raw|ConvertFrom-Json
$requiredNative=@($components|Where-Object{[string]$_.kind -eq 'native_module' -and [bool]$_.required})
$optionalNative=@($components|Where-Object{[string]$_.kind -eq 'native_module' -and -not [bool]$_.required})
$requiredIds=@($compatibility.self_test.required_modules|ForEach-Object{[string]$_.id}|Sort-Object)
$expectedRequiredIds=@($requiredNative|ForEach-Object{[string]$_.id}|Sort-Object)
if(($requiredIds -join "`n") -ne ($expectedRequiredIds -join "`n")){
    throw 'Compatibility self-test required_modules drift from component catalog.'
}
$optionalIds=@($compatibility.self_test.optional_modules|ForEach-Object{[string]$_.id}|Sort-Object)
$expectedOptionalIds=@($optionalNative|ForEach-Object{[string]$_.id}|Sort-Object)
if(($optionalIds -join "`n") -ne ($expectedOptionalIds -join "`n")){
    throw 'Compatibility self-test optional_modules drift from component catalog.'
}
foreach($entry in @($compatibility.self_test.required_modules)+@($compatibility.self_test.optional_modules)){
    $component=@($components|Where-Object{[string]$_.id -eq [string]$entry.id})[0]
    if($null -eq $component -or [string]$component.version -ne [string]$entry.version){
        throw "Compatibility self-test version drift for $([string]$entry.id)."
    }
}

Write-Host 'NCMM component catalog/dependency contract: PASS' -ForegroundColor Green
