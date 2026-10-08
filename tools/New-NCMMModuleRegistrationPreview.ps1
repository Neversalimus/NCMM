<#
.SYNOPSIS
Prepare a review-only NCMM module registration patch outside the repository.
.DESCRIPTION
Validates the scaffold's three registries and emits staged JSON, but does not
touch repo catalogs, feeds, Host, gameplay or the game installation.
#>
param(
  [Parameter(Mandatory=$true)][string]$ModuleRoot,
  [Parameter(Mandatory=$true)][string]$DestinationRoot,
  [string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path.TrimEnd('\','/')
$module=(Resolve-Path $ModuleRoot).Path.TrimEnd('\','/')
$out=(Resolve-Path $DestinationRoot).Path.TrimEnd('\','/')
$cmp=[StringComparison]::OrdinalIgnoreCase
if($module.Equals($root,$cmp) -or
   $module.StartsWith($root+[IO.Path]::DirectorySeparatorChar,$cmp) -or
   $out.Equals($root,$cmp) -or
   $out.StartsWith($root+[IO.Path]::DirectorySeparatorChar,$cmp) -or
   $out.Equals($module,$cmp) -or
   $out.StartsWith($module+[IO.Path]::DirectorySeparatorChar,$cmp)) {
  throw 'Source and output must be isolated from NCMM repository and from one another.'
}
foreach($p in @('mod.json','NCMM_REGISTRATION.json','CMakeLists.txt','src\module.cpp')) {
  if(-not(Test-Path (Join-Path $module $p) -PathType Leaf)) {
    throw "Missing module source/registration file: $p"
  }
}
$manifest=[IO.File]::ReadAllText((Join-Path $module 'mod.json'),[Text.Encoding]::UTF8) | ConvertFrom-Json
$plan=[IO.File]::ReadAllText((Join-Path $module 'NCMM_REGISTRATION.json'),[Text.Encoding]::UTF8) | ConvertFrom-Json
$id=[string]$manifest.id
if($id -cnotmatch '^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$' -or
   $id.Length -lt 3 -or $id.Length -gt 40 -or
   [string]$manifest.version -cnotmatch '^\d+\.\d+\.\d+(?:\.\d+)?$' -or
   [int]$manifest.loader_api -ne 1 -or [int]$manifest.api_major -ne 1 -or
   [string]$manifest.failure_policy -cne 'disable' -or
   [int]$plan.schema -ne 1 -or
   [string]$plan.policy -cne 'review-required-not-registered') {
  throw 'Unsafe/unsupported module manifest or registration preview.'
}
$catalogPath=Join-Path $root 'components\index.json'
$buildPath=Join-Path $root 'components\native-build.json'
$catalog=Get-Content $catalogPath -Raw | ConvertFrom-Json
$build=Get-Content $buildPath -Raw | ConvertFrom-Json
if([int]$catalog.schema -ne 1 -or [int]$build.schema -ne 1) {
  throw 'Unsupported component/build registry schema.'
}
$entry=$plan.catalog_component
$descriptor=$plan.component_descriptor
$recipe=$plan.native_build_entry
$expectedFolder=(($id -split '_') | ForEach-Object {
  $_.Substring(0,1).ToUpperInvariant()+$_.Substring(1)
}) -join ''
$group=$id.Replace('_','-')
$buildDir='_'+$id+'_build'
if([string]$recipe.id -cne $id -or
   [string]$recipe.folder -cne $expectedFolder -or
   [string]$recipe.archive_stem -cne $expectedFolder -or
   [string]$recipe.build_directory -cne $buildDir -or
   [string]$recipe.smoke_profile -cne 'generic' -or
   $recipe.missing_contract_smoke -isnot [bool] -or
   -not $recipe.missing_contract_smoke -or
   @($recipe.extra_smoke_executables).Count -ne 0 -or
   @($recipe.required_payload_files).Count -ne 0 -or
   (Split-Path $module -Leaf) -cne $expectedFolder) {
  throw 'Native build recipe differs from supported scaffold contract.'
}
foreach($field in @('id','name','version')) {
  if([string]$entry.$field -cne [string]$manifest.$field -or
     [string]$descriptor.$field -cne [string]$manifest.$field) {
    throw "Catalog/descriptor identity mismatch: $field"
  }
}
if([int]$descriptor.schema -ne 1 -or
   [string]$entry.kind -cne 'native_module' -or
   [string]$entry.delivery -cne 'source_build' -or
   [bool]$entry.required -or [string]$entry.atomic_group -cne $group -or
   [string]$entry.descriptor -cne "components/$id.json") {
  throw 'Invalid native module component description.'
}
foreach($field in @('kind','delivery','required','atomic_group','descriptor')) {
  if([string]$entry.$field -cne [string]$descriptor.$field) {
    throw "Component descriptor drift: $field"
  }
}
$dependencies=@($entry.dependencies)
$descriptorDeps=@($descriptor.dependencies)
if($dependencies.Count -ne $descriptorDeps.Count -or $dependencies.Count -lt 2) {
  throw 'Invalid catalog/descriptor dependencies.'
}
for($i=0;$i -lt $dependencies.Count;$i++) {
  foreach($field in @('component','min_version','capability')) {
    if([string]$dependencies[$i].$field -cne [string]$descriptorDeps[$i].$field) {
      throw "Dependency descriptor drift: $i/$field"
    }
  }
}
$hostEntries=@($catalog.components | Where-Object { [string]$_.id -eq 'ncmm_host' })
if($hostEntries.Count -ne 1) { throw 'Host component identity unavailable.' }
if([string]$dependencies[0].component -cne 'ncmm_host' -or
   [string]$dependencies[0].min_version -cne [string]$hostEntries[0].version) {
  throw 'Host dependency/version floor mismatch.'
}
$caps=@($manifest.requires | ForEach-Object { [string]$_ })
if($caps.Count -ne 4 -or @($caps|Select-Object -Unique).Count -ne $caps.Count) {
  throw 'Starter capability set is invalid.'
}
foreach($required in @('core.v1','api.versioning.v1','host_api.v2.core','settings.typed.v2')) {
  if($caps -cnotcontains $required) { throw "Missing required capability: $required" }
}
$dependencyCaps=@($dependencies | Where-Object { $_.capability } | ForEach-Object { [string]$_.capability })
if($dependencyCaps.Count -ne 3 -or
   @($dependencyCaps|Select-Object -Unique).Count -ne 3) {
  throw 'Invalid capability dependency list.'
}
foreach($required in @('api.versioning.v1','host_api.v2.core','settings.typed.v2')) {
  if($dependencyCaps -cnotcontains $required) { throw "Missing component capability dependency: $required" }
}
if(@($catalog.components | Where-Object { [string]$_.id -eq $id }).Count -ne 0 -or
   @($build.modules | Where-Object {
      [string]$_.id -eq $id -or
      [string]$_.folder -ieq $expectedFolder -or
      [string]$_.archive_stem -ieq $expectedFolder -or
      [string]$_.build_directory -ieq $buildDir
   }).Count -ne 0 -or
   $catalog.atomic_groups.PSObject.Properties.Name -contains $group -or
   (Test-Path (Join-Path $root ('mods\'+$expectedFolder))) -or
   (Test-Path (Join-Path $root ("components\$id.json")))) {
  throw "Module ID, group, folder or archive collides with registered content: $id"
}

# Draft JSON files go to an independent directory, never into the running repo.
$target=Join-Path $out ("ncmm-registration-"+$id)
if(Test-Path -LiteralPath $target) { throw "Preview destination exists; refusing overwrite: $target" }
$stage=Join-Path $out ('.ncmm-registration-'+[guid]::NewGuid().ToString('N'))
$utf8=New-Object Text.UTF8Encoding($false)
try {
  New-Item -ItemType Directory -Force (Join-Path $stage 'components') | Out-Null
  $catalog.atomic_groups | Add-Member -NotePropertyName $group -NotePropertyValue @($id) -ErrorAction Stop
  $catalog.components=@($catalog.components)+@($entry)
  $build.modules=@($build.modules)+@($recipe)
  [IO.File]::WriteAllText((Join-Path $stage 'components\index.json'),
    ($catalog|ConvertTo-Json -Depth 30)+[Environment]::NewLine,$utf8)
  [IO.File]::WriteAllText((Join-Path $stage 'components\native-build.json'),
    ($build|ConvertTo-Json -Depth 30)+[Environment]::NewLine,$utf8)
  [IO.File]::WriteAllText((Join-Path $stage ("components\$id.json")),
    ($descriptor|ConvertTo-Json -Depth 30)+[Environment]::NewLine,$utf8)
  $notes=@(
    'NCMM MODULE REGISTRATION PREVIEW - NOT A RELEASE'
    "Module: $id"
    "Source folder (to review): mods/$expectedFolder"
    'These staged JSON files are NOT installed or integrated into NCMM.'
    'A reviewer must merge source files, verify the feed/version/capability metadata,'
    'compatibility manifest, packaging integrity, and execute Runtime/Matrix CI.'
    'Do not publish this preview or copy its DLL directly into a game.'
  ) -join [Environment]::NewLine
  [IO.File]::WriteAllText((Join-Path $stage 'REVIEW_REQUIRED.txt'),
    $notes+[Environment]::NewLine,$utf8)
  if(Test-Path -LiteralPath $target) { throw 'Preview destination appeared during staging.' }
  Move-Item -LiteralPath $stage -Destination $target -ErrorAction Stop
} finally {
  if(Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
[pscustomobject]@{
  Id=$id;Version=[string]$manifest.version;Folder=$expectedFolder
  Destination=$target;Status='review-required'
}
