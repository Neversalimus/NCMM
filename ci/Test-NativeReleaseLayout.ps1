param(
    [string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent),
    [Parameter(Mandatory=$true)][string]$DistRoot
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path
$dist=(Resolve-Path $DistRoot).Path
. (Join-Path $root 'ci\NativeModuleCatalog.ps1')
$modules=@(Get-NcmmNativeBuildModules -RepositoryRoot $root)
$version=(& (Join-Path $root 'ci\Get-NcmmCurrentVersion.ps1') -RepositoryRoot $root).Trim()

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-NcmmZipIndex($Zip) {
    $index=@{}
    foreach($entry in $Zip.Entries) {
        $name=([string]$entry.FullName).Replace('\','/')
        if($name.Contains('../') -or $name.StartsWith('/') -or $name.Contains(':')) {
            throw "Unsafe archive entry: $name"
        }
        if($index.ContainsKey($name)) { throw "Duplicate archive entry: $name" }
        $index[$name]=$entry
    }
    return $index
}
function Require-NcmmZipFile($Index,[string]$Name) {
    if(-not $Index.ContainsKey($Name) -or $Index[$Name].Length -le 0) {
        throw "Missing/empty release archive entry: $Name"
    }
}
function Read-NcmmZipJson($Index,[string]$Name) {
    Require-NcmmZipFile $Index $Name
    $stream=$Index[$Name].Open()
    $reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true)
    try { return ($reader.ReadToEnd() | ConvertFrom-Json) }
    finally { $reader.Dispose() }
}

$fullPath=Join-Path $dist ("NCMM_Full_v$version.zip")
$runtimePath=Join-Path $dist ("NCMM_Runtime_v$version.zip")
foreach($path in @($fullPath,$runtimePath)) {
    if(-not(Test-Path $path -PathType Leaf)) { throw "Missing NCMM release archive: $path" }
}
$full=[IO.Compression.ZipFile]::OpenRead($fullPath)
$runtime=[IO.Compression.ZipFile]::OpenRead($runtimePath)
try {
    $fullFiles=Get-NcmmZipIndex $full
    $runtimeFiles=Get-NcmmZipIndex $runtime
    Require-NcmmZipFile $fullFiles 'NCMM_Setup.exe'
    Require-NcmmZipFile $runtimeFiles 'NCMM_Setup.exe'
    $manifest=Read-NcmmZipJson $fullFiles 'release-manifest.json'
    if([int]$manifest.schema -ne 1 -or [string]$manifest.host_runtime_version -ne $version) {
        throw 'NCMM Full release-manifest schema/Host drift.'
    }
    $listed=@($manifest.modules)
    if($listed.Count -ne $modules.Count) {
        throw "Release manifest module count drift: $($listed.Count) != $($modules.Count)"
    }
    foreach($module in $modules) {
        $rows=@($listed | Where-Object { [string]$_.id -ceq $module.Id })
        if($rows.Count -ne 1 -or [string]$rows[0].version -cne $module.Version) {
            throw "Release manifest module identity/version drift: $($module.Id)"
        }
        $package="NCMM_$($module.ArchiveStem)_v$($module.Version).zip"
        if([string]$rows[0].package -cne $package) {
            throw "Release manifest package drift: $($module.Id)"
        }
        Require-NcmmZipFile $fullFiles ("packages/" + $package)
        $base="payload/code_mods/$($module.Folder)/"
        Require-NcmmZipFile $fullFiles ($base+'ncmm_mod.dll')
        Require-NcmmZipFile $fullFiles ($base+'mod.json')
        Require-NcmmZipFile $runtimeFiles ($base+'ncmm_mod.dll')
        Require-NcmmZipFile $runtimeFiles ($base+'mod.json')
        foreach($rel in $module.RequiredPayloadFiles) {
            Require-NcmmZipFile $fullFiles ($base+[string]$rel)
            Require-NcmmZipFile $runtimeFiles ($base+[string]$rel)
        }
        $moduleZipPath=Join-Path $dist $package
        if(-not(Test-Path $moduleZipPath -PathType Leaf)) {
            throw "Standalone module package missing: $package"
        }
        $standalone=[IO.Compression.ZipFile]::OpenRead($moduleZipPath)
        try {
            $files=Get-NcmmZipIndex $standalone
            Require-NcmmZipFile $files 'component.json'
            Require-NcmmZipFile $files ("code_mods/$($module.Folder)/ncmm_mod.dll")
            Require-NcmmZipFile $files ("code_mods/$($module.Folder)/mod.json")
            foreach($rel in $module.RequiredPayloadFiles) {
                Require-NcmmZipFile $files ("code_mods/$($module.Folder)/"+[string]$rel)
            }
            $descriptor=Read-NcmmZipJson $files 'component.json'
            if([string]$descriptor.id -cne $module.Id -or
               [string]$descriptor.version -cne $module.Version) {
                throw "Standalone package descriptor drift: $($module.Id)"
            }
        } finally { $standalone.Dispose() }
    }
    $installed=@($fullFiles.Keys | Where-Object {
        $_ -match '^payload/code_mods/[^/]+/mod\.json$'
    })
    if($installed.Count -ne $modules.Count) {
        throw "Unregistered/duplicate module in Full archive: $($installed.Count) != $($modules.Count)"
    }
    Write-Host "NCMM native release archives: PASS ($($modules.Count) modules, Full + Runtime + standalone entries)." -ForegroundColor Green
} finally {
    $runtime.Dispose()
    $full.Dispose()
}
