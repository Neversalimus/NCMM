# Build-time catalog only. No Host ABI, installer state, or CDDA runtime dependency.
# Windows PowerShell 5.1 compatible; safe to dot-source from other CI scripts.
function Get-NcmmNativeBuildModules {
    param(
        [Parameter(Mandatory=$true)][string]$RepositoryRoot,
        [object]$Registry = $null
    )
    $root=(Resolve-Path $RepositoryRoot).Path
    $catalog=Get-Content (Join-Path $root 'components\index.json') -Raw | ConvertFrom-Json
    if([int]$catalog.schema -ne 1) { throw 'Unsupported component catalog schema.' }
    if($null -eq $Registry) {
        $Registry=Get-Content (Join-Path $root 'components\native-build.json') -Raw | ConvertFrom-Json
    }
    if([int]$Registry.schema -ne 1 -or $null -eq $Registry.modules) {
        throw 'Invalid native build registry/schema.'
    }

    $expected=@($catalog.components | Where-Object {
        [string]$_.kind -eq 'native_module' -and [string]$_.delivery -eq 'source_build'
    })
    $entries=@($Registry.modules)
    if($entries.Count -ne $expected.Count) {
        throw "Native module registry count drift: declared=$($entries.Count), source-built=$($expected.Count)."
    }

    $seenIds=@{}
    $seenFolders=@{}
    $seenArchives=@{}
    $seenBuilds=@{}
    $result=@()
    foreach($entry in $entries) {
        $id=[string]$entry.id
        $folder=[string]$entry.folder
        $stem=[string]$entry.archive_stem
        $buildDir=[string]$entry.build_directory
        if($id -cnotmatch '^[a-z][a-z0-9_]{1,63}$' -or
           $folder -cnotmatch '^[A-Za-z][A-Za-z0-9]{1,63}$' -or
           $stem -cnotmatch '^[A-Za-z][A-Za-z0-9]{1,63}$' -or
           $buildDir -cnotmatch '^_[a-z][a-z0-9_]*_build$') {
            throw "Unsafe native module build identity/path: $id / $folder / $stem / $buildDir"
        }
        foreach($pair in @(
            @{Set=$seenIds;Value=$id;Label='id'},
            @{Set=$seenFolders;Value=$folder.ToLowerInvariant();Label='folder'},
            @{Set=$seenArchives;Value=$stem.ToLowerInvariant();Label='archive stem'},
            @{Set=$seenBuilds;Value=$buildDir.ToLowerInvariant();Label='build directory'}
        )) {
            if($pair.Set.ContainsKey($pair.Value)) {
                throw "Duplicate native module $($pair.Label): $($pair.Value)"
            }
            $pair.Set[$pair.Value]=$true
        }
        $component=@($expected | Where-Object { [string]$_.id -ceq $id })
        if($component.Count -ne 1) {
            throw "Unknown or non-source-built native module '$id' in build registry."
        }
        if([bool]$component[0].required) {
            throw "Unexpected required native gameplay module: $id"
        }
        $source=Join-Path $root ('mods\' + $folder)
        if(-not(Test-Path $source -PathType Container)) {
            throw "Native module source folder missing: $folder"
        }
        foreach($file in @('mod.json','CMakeLists.txt')) {
            if(-not(Test-Path (Join-Path $source $file) -PathType Leaf)) {
                throw "Native module '$id' source file missing: $file"
            }
        }
        $manifest=Get-Content (Join-Path $source 'mod.json') -Raw | ConvertFrom-Json
        foreach($field in @('id','name','version')) {
            if([string]$manifest.$field -cne [string]$component[0].$field) {
                throw "Native module '$id' $field disagrees with components/index.json."
            }
        }
        if([int]$manifest.loader_api -ne 1 -or
           [string]$manifest.failure_policy -ne 'disable' -or
           [int]$manifest.api_major -ne 1) {
            throw "Native module '$id' manifest Loader ABI/failure policy drift."
        }
        $caps=@($manifest.requires | ForEach-Object { [string]$_ })
        if($caps.Count -eq 0 -or @($caps | Select-Object -Unique).Count -ne $caps.Count) {
            throw "Native module '$id' has empty/duplicated capability requirements."
        }
        if($null -eq $entry.missing_contract_smoke -or
           $entry.missing_contract_smoke -isnot [bool]) {
            throw "Native module '$id' must declare a boolean missing_contract_smoke."
        }
        if($null -eq $entry.extra_smoke_executables -or
           $null -eq $entry.required_payload_files) {
            throw "Native module '$id' is missing test or payload declarations."
        }
        foreach($list in @(
            @{Value=@($entry.extra_smoke_executables); Label='extra smoke'; Pattern='^[A-Za-z][A-Za-z0-9_]*\.exe$'},
            @{Value=@($entry.required_payload_files); Label='required payload'; Pattern='^[a-zA-Z0-9_/-]+\.[a-zA-Z0-9]+$'}
        )) {
            $found=@{}
            foreach($value in $list.Value) {
                $v=[string]$value
                if($v -cnotmatch $list.Pattern -or $v.Contains('..') -or $v.StartsWith('/')) {
                    throw "Unsafe native module '$id' $($list.Label) value: $v"
                }
                if($found.ContainsKey($v.ToLowerInvariant())) {
                    throw "Duplicate native module '$id' $($list.Label) value: $v"
                }
                $found[$v.ToLowerInvariant()]=$true
                if($list.Label -eq 'required payload' -and
                   -not(Test-Path (Join-Path $source ($v.Replace('/',[IO.Path]::DirectorySeparatorChar))) -PathType Leaf)) {
                    throw "Native module '$id' required payload file missing: $v"
                }
            }
        }
        $result += [pscustomobject]@{
            Id=$id
            Folder=$folder
            ArchiveStem=$stem
            BuildDirectory=$buildDir
            Version=[string]$manifest.version
            Name=[string]$manifest.name
            MissingContractSmoke=[bool]$entry.missing_contract_smoke
            ExtraSmokeExecutables=@($entry.extra_smoke_executables)
            RequiredPayloadFiles=@($entry.required_payload_files)
        }
    }

    # A source directory with mod.json must not disappear silently from the release.
    foreach($dir in Get-ChildItem (Join-Path $root 'mods') -Directory) {
        if((Test-Path (Join-Path $dir.FullName 'mod.json') -PathType Leaf) -and
           -not $seenFolders.ContainsKey($dir.Name.ToLowerInvariant())) {
            throw "Unregistered native module source directory: $($dir.Name)"
        }
    }
    return $result
}
