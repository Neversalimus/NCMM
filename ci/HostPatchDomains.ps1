# Read-only architecture contract. Never executes or rewrites Host patch layers.
function Assert-NcmmHostPatchDomains {
    param(
        [Parameter(Mandatory=$true)][string]$RepositoryRoot,
        [object]$Manifest=$null,
        [object]$Stack=$null
    )
    $root=(Resolve-Path $RepositoryRoot).Path
    if($null -eq $Stack) {
        $Stack=Get-Content (Join-Path $root 'ci\host-patch-stack.json') -Raw | ConvertFrom-Json
    }
    if($null -eq $Manifest) {
        $Manifest=Get-Content (Join-Path $root 'ci\host-patch-domains.json') -Raw | ConvertFrom-Json
    }
    if([int]$Manifest.schema -ne 1 -or
       [string]$Manifest.source -cne 'ci/host-patch-stack.json' -or
       [int]$Stack.schema -ne 1) {
        throw 'Unsupported Host patch domain/source schema.'
    }
    $domains=@($Manifest.domains)
    $layers=@($Stack.layers | ForEach-Object { [string]$_ })
    if($domains.Count -eq 0 -or $layers.Count -eq 0) {
        throw 'Host patch domain graph or canonical stack is empty.'
    }
    $seenDomains=@{}
    $seenLayers=@{}
    $flattened=@()
    foreach($domain in $domains) {
        $id=[string]$domain.id
        if($id -cnotmatch '^[a-z][a-z0-9-]{2,63}$' -or
           $seenDomains.ContainsKey($id)) {
            throw "Invalid/duplicate Host patch domain id: $id"
        }
        if([string]::IsNullOrWhiteSpace([string]$domain.description)) {
            throw "Host patch domain '$id' is missing a description."
        }
        if($null -eq $domain.after -or $null -eq $domain.layers) {
            throw "Host patch domain '$id' must explicitly declare dependencies and layers."
        }
        $dependencies=@($domain.after)
        $domainLayerList=@($domain.layers)
        if($domainLayerList.Count -eq 0) {
            throw "Host patch domain '$id' cannot be empty."
        }
        $seenRequirements=@{}
        foreach($required in $dependencies) {
            $requiredId=[string]$required
            if($requiredId -eq $id -or
               $seenRequirements.ContainsKey($requiredId) -or
               -not $seenDomains.ContainsKey($requiredId)) {
                throw "Host patch domain '$id' has missing/forward/duplicate/cyclic requirement: $requiredId"
            }
            $seenRequirements[$requiredId]=$true
        }
        foreach($layer in $domainLayerList) {
            $name=[string]$layer
            if($name -cnotmatch '^(Apply|Assert)-[A-Za-z0-9]+$' -or
               $seenLayers.ContainsKey($name)) {
                throw "Host patch domain '$id' has unsafe or duplicate layer: $name"
            }
            $seenLayers[$name]=$id
            $flattened += $name
        }
        $seenDomains[$id]=$true
    }
    if($flattened.Count -ne $layers.Count) {
        throw "Host patch domain coverage mismatch: $($flattened.Count) vs canonical $($layers.Count)."
    }
    for($index=0; $index -lt $layers.Count; ++$index) {
        if($flattened[$index] -cne $layers[$index]) {
            throw "Host patch domain order/membership drift at index $index : expected '$($layers[$index])', got '$($flattened[$index])'."
        }
    }
    [pscustomobject]@{ Domains=$domains.Count; Layers=$flattened.Count }
}
