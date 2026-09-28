param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path

$descriptorPath=Join-Path $RepositoryRoot 'components\ncmm_host.json'
$catalogPath=Join-Path $RepositoryRoot 'components\index.json'
if(-not(Test-Path $descriptorPath -PathType Leaf)){throw 'NCMM Host descriptor is missing.'}
if(-not(Test-Path $catalogPath -PathType Leaf)){throw 'NCMM component catalog is missing.'}

$descriptor=Get-Content $descriptorPath -Raw|ConvertFrom-Json
$catalog=Get-Content $catalogPath -Raw|ConvertFrom-Json
if([string]$descriptor.id -ne 'ncmm_host'){throw 'NCMM Host descriptor id mismatch.'}
$version=([string]$descriptor.version).Trim()
if($version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$'){throw "Invalid NCMM Host version: $version"}

$catalogHost=@($catalog.components|Where-Object{[string]$_.id -eq 'ncmm_host'})
if($catalogHost.Count -ne 1){throw "NCMM component catalog expected exactly one ncmm_host, found $($catalogHost.Count)."}
if([string]$catalogHost[0].version -ne $version){
    throw "NCMM Host version drift: descriptor=$version catalog=$([string]$catalogHost[0].version)"
}
Write-Output $version
