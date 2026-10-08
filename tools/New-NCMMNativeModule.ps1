param(
 [Parameter(Mandatory=$true)][string]$Id,
 [Parameter(Mandatory=$true)][string]$Name,
 [Parameter(Mandatory=$true)][string]$DestinationRoot,
 [string]$Version='0.1.0',
 [string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
$DestinationRoot=(Resolve-Path $DestinationRoot).Path
if($Id.Length -lt 3 -or $Id.Length -gt 40 -or $Id -cnotmatch '^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$') { throw 'Unsafe module id.' }
if($Name.Length -lt 2 -or $Name.Length -gt 80 -or $Name.Trim() -cne $Name -or
   $Name -cnotmatch '^\p{L}[\p{L}\p{N} ._-]*$') { throw 'Unsafe module display name.' }
if($Version -cnotmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') { throw 'Invalid dotted version.' }
$root=[IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\','/')
$destRoot=[IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\','/')
if($destRoot.Equals($root,[StringComparison]::OrdinalIgnoreCase) -or
   $destRoot.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
 throw 'Scaffolding inside NCMM repository is prohibited until registration is reviewed.'
}
$catalog=Get-Content (Join-Path $RepositoryRoot 'components\index.json') -Raw | ConvertFrom-Json
$registry=Get-Content (Join-Path $RepositoryRoot 'components\native-build.json') -Raw | ConvertFrom-Json
if(@($catalog.components|Where-Object{[string]$_.id -eq $Id}).Count -gt 0 -or
   @($registry.modules|Where-Object{[string]$_.id -eq $Id}).Count -gt 0) { throw "Duplicate module id: $Id" }
$host=@($catalog.components|Where-Object{[string]$_.id -eq 'ncmm_host'})
if($host.Count -ne 1) { throw 'Host catalog identity missing.' }
$folder=(($Id -split '_')|ForEach-Object{$_.Substring(0,1).ToUpperInvariant()+$_.Substring(1)}) -join ''
$group=$Id.Replace('_','-')
$buildDir='_'+$Id+'_build'
if($catalog.atomic_groups.PSObject.Properties.Name -contains $group -or
   @($registry.modules|Where-Object{[string]$_.folder -ieq $folder -or
    [string]$_.archive_stem -ieq $folder -or [string]$_.build_directory -ieq $buildDir}).Count -gt 0 -or
   (Test-Path (Join-Path $RepositoryRoot ('mods\'+$folder)))) { throw 'Module folder/group/archive collision.' }
$dest=Join-Path $DestinationRoot $folder
if(Test-Path -LiteralPath $dest) { throw "Destination exists; refusing overwrite: $dest" }

$capabilities=@('core.v1','api.versioning.v1','host_api.v2.core','settings.typed.v2')
$dependencies=@(
 [ordered]@{component='ncmm_host';min_version=[string]$host[0].version},
 [ordered]@{capability='api.versioning.v1'},
 [ordered]@{capability='host_api.v2.core'},
 [ordered]@{capability='settings.typed.v2'}
)
$catalogEntry=[ordered]@{
 id=$Id;name=$Name;version=$Version;kind='native_module';atomic_group=$group
 delivery='source_build';required=$false;dependencies=$dependencies;descriptor="components/$Id.json"
}
$descriptor=[ordered]@{schema=1}
foreach($key in $catalogEntry.Keys) { $descriptor[$key]=$catalogEntry[$key] }
$buildEntry=[ordered]@{
 id=$Id;folder=$folder;archive_stem=$folder;build_directory=$buildDir
 smoke_profile='generic';missing_contract_smoke=$true
 extra_smoke_executables=@();required_payload_files=@()
}
$manifest=[ordered]@{
 id=$Id;name=$Name;version=$Version;loader_api=1;api_major=1;api_min_minor=9
 requires=$capabilities;failure_policy='disable'
}
$registration=[ordered]@{
 schema=1;policy='review-required-not-registered'
 catalog_component=$catalogEntry;component_descriptor=$descriptor;native_build_entry=$buildEntry
 integration_notes=@(
  'Add module to mods/ only with reviewed catalog changes.',
  'Append catalog_component and atomic_group to components/index.json.',
  'Write component_descriptor to components/<id>.json.',
  'Append native_build_entry to components/native-build.json.',
  'Update compatibility and release feeds; run Runtime CI and Real Installation Matrix.'
 )
}
$cpp=@'
#include "ncmm_api.h"
#include <cstddef>
namespace {
constexpr const char *id = "@@ID@@";
constexpr const char *name = "@@NAME@@";
constexpr const char *version = "@@VERSION@@";
constexpr const char *setting = "NCMM_@@UPPERID@@_ENABLED";
const char *caps[] = { "core.v1", "api.versioning.v1", "host_api.v2.core", "settings.typed.v2" };
int init( const ncmm_host_api_v1 *api ) {
    if( !api || api->abi_version != NCMM_ABI_VERSION || !api->has_capability ||
        !api->query_interface || !api->get_api_version_major || !api->get_api_version_minor ) return 0;
    for( const char *cap : caps ) if( !api->has_capability( cap ) ) return 0;
    if( api->get_api_version_major() != 1u || api->get_api_version_minor() < 9u ) return 0;
    const auto *core = static_cast<const ncmm_host_api_v2_core *>(
        api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 0u ) );
    constexpr size_t size = offsetof( ncmm_host_api_v2_core, world_setting_register_bool ) +
        sizeof( ( ( ncmm_host_api_v2_core * )nullptr )->world_setting_register_bool );
    if( !core || core->abi_version != NCMM_HOST_API_V2_CORE_ABI ||
        core->api_major != 2u || core->struct_size < size ||
        !core->world_setting_register_bool ) return 0;
    if( !core->world_setting_register_bool(
         id, setting, "Enable @@NAME@@",
         "Example LIVE toggle; this module has no gameplay hooks.",
         1, NCMM_WORLD_SETTING_LIVE ) ) return 0;
    return 1;
}
void shutdown() {}
const ncmm_mod_descriptor_v1 descriptor = { NCMM_ABI_VERSION, id, name, version,
    caps, sizeof( caps ) / sizeof( caps[0] ), &init, &shutdown };
}
extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1() {
    return &descriptor;
}
'@
$cpp=$cpp.Replace('@@ID@@',$Id).Replace('@@NAME@@',$Name).
 Replace('@@VERSION@@',$Version).Replace('@@UPPERID@@',$Id.ToUpperInvariant())
$cmake=@'
cmake_minimum_required(VERSION 3.20)
project(NCMMNativeStarter LANGUAGES CXX)
if(NOT NCMM_SDK_INCLUDE_DIR)
    set(NCMM_SDK_INCLUDE_DIR "@@SOURCEDIR@@/../../sdk")
endif()
if(NOT EXISTS "@@SDKDIR@@/ncmm_api.h")
    message(FATAL_ERROR "Pass -DNCMM_SDK_INCLUDE_DIR=<NCMM/sdk path>")
endif()
add_library(ncmm_mod SHARED src/module.cpp)
target_compile_features(ncmm_mod PRIVATE cxx_std_17)
target_include_directories(ncmm_mod PRIVATE "@@SDKDIR@@")
target_compile_definitions(ncmm_mod PRIVATE NCMM_MOD_BUILD)
if(MSVC)
 set_property(TARGET ncmm_mod PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(ncmm_mod PRIVATE /EHsc /utf-8)
endif()
set_target_properties(ncmm_mod PROPERTIES PREFIX "" OUTPUT_NAME "ncmm_mod")
'@
$cmake=$cmake.Replace('@@SOURCEDIR@@',('$'+'{CMAKE_CURRENT_SOURCE_DIR}')).
 Replace('@@SDKDIR@@',('$'+'{NCMM_SDK_INCLUDE_DIR}'))
$readme=@'
# @@NAME@@ - NCMM native SDK starter
ID: @@ID@@; Version: @@VERSION@@.
NOT REGISTERED. Source-only starter; not certified and not installable.
Build with MSVC/CMake passing -DNCMM_SDK_INCLUDE_DIR=<path to NCMM/sdk>.
Exports ncmm_get_descriptor_v1. LIVE toggle: NCMM_@@UPPERID@@_ENABLED.
No gameplay patches. NCMM_REGISTRATION.json is a review blueprint.
'@
$readme=$readme.Replace('@@ID@@',$Id).Replace('@@NAME@@',$Name).
 Replace('@@VERSION@@',$Version).Replace('@@UPPERID@@',$Id.ToUpperInvariant())
$stage=Join-Path $DestinationRoot ('.ncmm-scaffold-'+[guid]::NewGuid().ToString('N'))
$utf8=New-Object Text.UTF8Encoding($false)
try {
 New-Item -ItemType Directory -Path (Join-Path $stage 'src') -Force | Out-Null
 [IO.File]::WriteAllText((Join-Path $stage 'src\module.cpp'),$cpp+[Environment]::NewLine,$utf8)
 [IO.File]::WriteAllText((Join-Path $stage 'CMakeLists.txt'),$cmake+[Environment]::NewLine,$utf8)
 [IO.File]::WriteAllText((Join-Path $stage 'mod.json'),($manifest|ConvertTo-Json -Depth 8)+[Environment]::NewLine,$utf8)
 [IO.File]::WriteAllText((Join-Path $stage 'NCMM_REGISTRATION.json'),($registration|ConvertTo-Json -Depth 12)+[Environment]::NewLine,$utf8)
 [IO.File]::WriteAllText((Join-Path $stage 'README.md'),$readme+[Environment]::NewLine,$utf8)
 if(Test-Path -LiteralPath $dest) { throw "Destination appeared during generation: $dest" }
 Move-Item -LiteralPath $stage -Destination $dest -ErrorAction Stop
} finally {
 if(Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
[pscustomobject]@{Id=$Id;Name=$Name;Version=$Version;Folder=$folder;Destination=$dest;SmokeProfile='generic'}
