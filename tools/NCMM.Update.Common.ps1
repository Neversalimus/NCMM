$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'NCMM.Infrastructure.Common.ps1')

function Convert-NcmmVersion([string]$Value) {
    if( [string]::IsNullOrWhiteSpace($Value) ) { return ([version]'0.0.0.0') }
    $m = [regex]::Match($Value, '^(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:\.(\d+))?')
    if( -not $m.Success ) { throw "Unsupported numeric version: $Value" }
    $parts = @(0,0,0,0)
    for( $i = 1; $i -le 4; $i++ ) { if( $m.Groups[$i].Success ) { $parts[$i-1] = [int]$m.Groups[$i].Value } }
    return ([version](('{0}.{1}.{2}.{3}' -f $parts[0],$parts[1],$parts[2],$parts[3])))
}

function Get-NcmmComponentCatalog([string]$PackageRoot) {
    $path = Join-Path $PackageRoot 'components\index.json'
    if( -not (Test-Path $path -PathType Leaf) ) { throw 'Component catalog missing.' }
    return (Get-Content $path -Raw | ConvertFrom-Json)
}

function Get-NcmmInstalledComponentMap([string]$PackageRoot,[string]$GameRoot) {
    $root = Resolve-NcmmGameRoot $GameRoot
    $catalog = Get-NcmmComponentCatalog $PackageRoot
    $map = @{}
    $map['ncmm_infrastructure'] = [string]$catalog.infrastructure_version
    $bindingPath = Join-Path $root 'ncmm\host.binding.json'
    if( Test-Path $bindingPath -PathType Leaf ) {
        try { $b = Get-Content $bindingPath -Raw | ConvertFrom-Json; if($b.ncmm_version){$map['ncmm_host']=[string]$b.ncmm_version} } catch {}
    }
    $modulesPath = Join-Path $root 'ncmm\modules.state.json'
    if( Test-Path $modulesPath -PathType Leaf ) {
        try { $m=Get-Content $modulesPath -Raw|ConvertFrom-Json; foreach($x in @($m.modules)){ if($x.id){$map[[string]$x.id]=[string]$x.version} } } catch {}
    }
    return $map
}

function Get-NcmmInstalledCapabilities([string]$GameRoot) {
    $root=Resolve-NcmmGameRoot $GameRoot
    $path=Join-Path $root 'ncmm\modules.state.json'
    if(-not(Test-Path $path -PathType Leaf)){return @()}
    try{$m=Get-Content $path -Raw|ConvertFrom-Json;return @($m.capabilities|ForEach-Object{[string]$_})}catch{return @()}
}

function Get-NcmmReleaseCapabilitySet([object]$Catalog,[object]$Release) {
    # Native PowerShell hashtable is deliberate here. Windows PowerShell 5.1
    # has binder bugs around generic ICollection/List/HashSet values when they
    # are converted with @() or embedded into PSCustomObject literals.
    $set=@{}
    foreach($c in @($Catalog.components)){
        if($Release.components.PSObject.Properties.Name -contains [string]$c.id){
            foreach($p in @($c.provides)){
                $cap=[string]$p
                if($cap -and $cap -notmatch '^[a-z_]+:'){
                    $set[$cap]=$true
                }
            }
        }
    }
    return $set
}

function Resolve-NcmmDependencyPlan([string]$PackageRoot,[string]$GameRoot,[object]$Feed,[string[]]$Requested=@()) {
    $catalog=Get-NcmmComponentCatalog $PackageRoot
    $installed=Get-NcmmInstalledComponentMap $PackageRoot $GameRoot
    $installedCaps=@(Get-NcmmInstalledCapabilities $GameRoot)
    $releases=@($Feed.releases|Where-Object{$_.status -notin @('revoked','incompatible')})
    if($releases.Count -eq 0){throw 'Update feed contains no usable release.'}
    $release=@($releases|Sort-Object @{Expression={Convert-NcmmVersion ([string]$_.version)};Descending=$true}|Select-Object -First 1)[0]
    $available=@($release.components.PSObject.Properties.Name)

    if($Requested.Count -eq 0 -or $Requested -contains 'all'){
        $targets=@()
        foreach($id in $available){
            $cc=@($catalog.components|Where-Object{[string]$_.id -eq [string]$id}|Select-Object -First 1)[0]
            if([bool]$cc.required -or $installed.ContainsKey([string]$id)){
                $targets += [string]$id
            }
        }
    }else{
        $targets=@($Requested)
    }

    $known=@($catalog.components|ForEach-Object{[string]$_.id})
    foreach($id in $targets){
        if($known -notcontains [string]$id){throw "Unknown component requested: $id"}
    }

    $requestedTargets=@($targets)
    # Resolve component dependencies before atomic-group expansion. Optional native
    # modules have independent atomic groups; selecting Survivor must never pull AWS
    # (or vice versa), while a missing/outdated Host is still added automatically.
    $selected=@{}
    foreach($id in $targets){$selected[[string]$id]=$true}
    $changed=$true
    while($changed){
        $changed=$false
        foreach($id in @($selected.Keys)){
            $component=@($catalog.components|Where-Object{[string]$_.id -eq [string]$id}|Select-Object -First 1)[0]
            foreach($d in @($component.dependencies)){
                if(-not $d.component){continue}
                $dep=[string]$d.component
                if($known -notcontains $dep){throw "$id depends on unknown component $dep"}
                $needDependency=$false
                if(-not $installed.ContainsKey($dep)){$needDependency=$true}
                elseif($d.min_version -and
                       (Convert-NcmmVersion ([string]$installed[$dep])) -lt
                       (Convert-NcmmVersion ([string]$d.min_version))){$needDependency=$true}
                if($needDependency -and $available -contains $dep -and -not $selected.ContainsKey($dep)){
                    $selected[$dep]=$true
                    $changed=$true
                }
            }
        }
    }
    $targets=@($selected.Keys)

    # Use a native hashtable as a set. This avoids the Windows PowerShell 5.1
    # PSToObjectArrayBinder bug that can occur with generic HashSet/List values.
    $groups=@{}
    foreach($id in $targets){
        $component=@($catalog.components|Where-Object{[string]$_.id -eq [string]$id}|Select-Object -First 1)[0]
        $groups[[string]$component.atomic_group]=$true
    }

    $expanded=@()
    foreach($component in @($catalog.components)){
        if($groups.ContainsKey([string]$component.atomic_group) -and
           $available -contains [string]$component.id){
            $expanded += [string]$component.id
        }
    }
    $expandedUnique=@($expanded|Select-Object -Unique)

    $actions=@()
    foreach($id in $expandedUnique){
        $desired=[string]$release.components.$id
        $current=if($installed.ContainsKey($id)){[string]$installed[$id]}else{'0.0.0'}
        $cmp=(Convert-NcmmVersion $desired).CompareTo((Convert-NcmmVersion $current))
        if($current -eq '0.0.0'){$action='install'}elseif($cmp -gt 0){$action='update'}elseif($cmp -lt 0){$action='downgrade'}else{$action='keep'}
        $component=@($catalog.components|Where-Object{[string]$_.id -eq [string]$id}|Select-Object -First 1)[0]
        $actions += [pscustomobject]@{
            component=$id
            current=$current
            desired=$desired
            action=$action
            atomic_group=[string]$component.atomic_group
        }
    }

    $resolvedCaps=Get-NcmmReleaseCapabilitySet $catalog $release
    foreach($cap in $installedCaps){
        $resolvedCaps[[string]$cap]=$true
    }

    $errors=@()
    foreach($id in $expandedUnique){
        $component=@($catalog.components|Where-Object{[string]$_.id -eq [string]$id}|Select-Object -First 1)[0]
        foreach($d in @($component.dependencies)){
            if($d.component){
                $dep=[string]$d.component
                if($available -contains $dep){
                    $depVersion=[string]$release.components.$dep
                }elseif($installed.ContainsKey($dep)){
                    $depVersion=[string]$installed[$dep]
                }else{
                    $depVersion='0.0.0'
                }
                if($d.min_version -and
                   (Convert-NcmmVersion $depVersion) -lt (Convert-NcmmVersion ([string]$d.min_version))){
                    $errors += "$id requires $dep >= $($d.min_version), resolved $depVersion"
                }
            }
            if($d.capability -and -not $resolvedCaps.ContainsKey([string]$d.capability)){
                $errors += "$id requires capability $($d.capability), which is not resolved"
            }
        }
    }

    $groupNames=@($groups.Keys|Sort-Object)
    return [pscustomobject]@{
        schema=1
        release_version=[string]$release.version
        release_status=[string]$release.status
        requested=@($requestedTargets)
        expanded=@($expandedUnique)
        atomic_groups=@($groupNames)
        actions=@($actions)
        errors=@($errors)
        can_apply=($errors.Count -eq 0)
        package_url=[string]$release.package_url
        package_sha256=[string]$release.package_sha256
        resolved_utc=[DateTime]::UtcNow.ToString('o')
    }
}

function Read-NcmmUpdateFeed([string]$PackageRoot,[string]$FeedPath,[string]$FeedUrl) {
    if($FeedPath){return (Get-Content (Resolve-Path $FeedPath) -Raw|ConvertFrom-Json)}
    if($FeedUrl){
        $tmp=Join-Path $env:TEMP ('ncmm_feed_'+[guid]::NewGuid().ToString('N')+'.json')
        try{Invoke-WebRequest -UseBasicParsing -Uri $FeedUrl -OutFile $tmp;return (Get-Content $tmp -Raw|ConvertFrom-Json)}finally{Remove-Item $tmp -Force -ErrorAction SilentlyContinue}
    }
    $defaultUrl='https://raw.githubusercontent.com/Neversalimus/NCMM/main/compat/update/index.json'
    $tmp=Join-Path $env:TEMP ('ncmm_feed_'+[guid]::NewGuid().ToString('N')+'.json')
    try{
        try{Invoke-WebRequest -UseBasicParsing -Uri $defaultUrl -OutFile $tmp;return (Get-Content $tmp -Raw|ConvertFrom-Json)}catch{Write-Host ('Remote update feed unavailable; using embedded feed. '+$_.Exception.Message) -ForegroundColor DarkYellow}
    } finally {Remove-Item $tmp -Force -ErrorAction SilentlyContinue}
    return (Get-Content (Join-Path $PackageRoot 'compat\update\index.json') -Raw|ConvertFrom-Json)
}

function Invoke-NcmmUpdateStateMigration([string]$PackageRoot,[string]$GameRoot) {
    $root=Resolve-NcmmGameRoot $GameRoot
    $ncmm=Join-Path $root 'ncmm';New-Item -ItemType Directory -Force $ncmm|Out-Null
    $path=Join-Path $ncmm 'update.state.json'
    $registry=Get-Content (Join-Path $PackageRoot 'migrations\migrations.json') -Raw|ConvertFrom-Json
    if(Test-Path $path -PathType Leaf){try{$state=Get-Content $path -Raw|ConvertFrom-Json}catch{throw "Invalid update state JSON: $path"}}else{$state=[pscustomobject]@{schema=0}}
    $target=[int]$registry.update_state_schema
    $history=@()
    while([int]$state.schema -lt $target){
        $migration=@($registry.migrations|Where-Object{$_.scope -eq 'update_state' -and [int]$_.from -eq [int]$state.schema}|Select-Object -First 1)[0]
        if(-not $migration){throw "No update-state migration from schema $($state.schema)"}
        $script=Join-Path $PackageRoot ([string]$migration.script)
        $state=& $script -State $state
        if([int]$state.schema -ne [int]$migration.to){throw "Migration $($migration.id) did not commit target schema"}
        $history += [pscustomobject]@{id=[string]$migration.id;utc=[DateTime]::UtcNow.ToString('o')}
    }
    if(-not $state.PSObject.Properties['history']){$state|Add-Member -NotePropertyName history -NotePropertyValue @()}
    $state.history=@($state.history)+@($history)
    Write-NcmmUtf8NoBom $path (($state|ConvertTo-Json -Depth 12)+"`n")
    return [pscustomobject]@{path=$path;state=$state;migrations=@($history)}
}
