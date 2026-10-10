param(
    [string]$RepositoryRoot=(Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [Parameter(Mandatory=$true)][string]$HostRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
$HostRoot=(Resolve-Path $HostRoot).Path
$GameRoot=(Resolve-Path $GameRoot).Path
New-Item -ItemType Directory -Force $OutputRoot | Out-Null
$OutputRoot=(Resolve-Path $OutputRoot).Path
$source=Get-Content (Join-Path $HostRoot 'first-person-source.json') -Raw | ConvertFrom-Json
if($source.source_commit -ne '074aa98bd5be3de4c35f154082db32a0e63bb0f1' -or
   $source.tag -ne 'cdda-experimental-2026-10-06-1807' -or $source.status -ne 'experimental-not-certified'){
    throw 'The preview requires the exact experimental 1807 graphics Host.'
}
if(-not(Test-Path (Join-Path $HostRoot 'cataclysm-tiles.exe') -PathType Leaf)){
    throw 'A fully linked experimental Host is required.'
}
$versionText=Get-Content (Join-Path $GameRoot 'VERSION.txt') -Raw
if($versionText -notmatch 'commit sha:\s*074aa98'){throw 'Official 1807 game identity differs.'}
$stage=Join-Path $OutputRoot '_first_person_installer'
if(Test-Path $stage){Remove-Item $stage -Recurse -Force}
New-Item -ItemType Directory -Force $stage | Out-Null
$runtimeBuild=Join-Path $OutputRoot '_runtime_build'
& (Join-Path $RepositoryRoot 'ci/Build-Runtime.ps1') -RepositoryRoot $RepositoryRoot -OutputRoot $runtimeBuild -PayloadOnly
Copy-Item (Join-Path $runtimeBuild 'payload') $stage -Recurse
$payload=Join-Path $stage 'payload'
$fpBuild=Join-Path $OutputRoot '_fp_build'
& cmake -S $PSScriptRoot -B $fpBuild -A x64
if($LASTEXITCODE -ne 0){throw 'First Person View CMake configure failed.'}
& cmake --build $fpBuild --config Release
if($LASTEXITCODE -ne 0){throw 'First Person View DLL build failed.'}
& ctest --test-dir $fpBuild -C Release --output-on-failure
if($LASTEXITCODE -ne 0){throw 'First Person View renderer tests failed.'}
$fp=Join-Path $payload 'code_mods/FirstPersonView'
New-Item -ItemType Directory -Force $fp | Out-Null
Copy-Item (Join-Path $fpBuild 'Release/ncmm_mod.dll') $fp
Copy-Item (Join-Path $PSScriptRoot 'mod.json') $fp
$hostDest=Join-Path $payload 'host'
New-Item -ItemType Directory -Force $hostDest | Out-Null
Copy-Item (Join-Path $HostRoot 'cataclysm-tiles.exe') (Join-Path $hostDest 'cataclysm-tiles.ncmm.exe')
Copy-Item (Join-Path $HostRoot 'first-person-source.json') $hostDest
Copy-Item (Join-Path $HostRoot 'LICENSE.txt') (Join-Path $stage 'CDDA-LICENSE.txt')
Copy-Item (Join-Path $RepositoryRoot 'THIRD_PARTY_NOTICES.txt') $stage

# Use the same installer UI, discovery and transaction implementation. Preview
# hooks are compiled explicitly; no alternate ad-hoc file-copy installer exists.
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
$io=Join-Path $RepositoryRoot 'runtime/NCMMRuntimeIO.cs'
$core=Join-Path $RepositoryRoot 'runtime/NCMMSetupCore.cs'
$diagnostics=Join-Path $RepositoryRoot 'runtime/NCMMSetupDiagnostics.cs'
$bridge=Join-Path $PSScriptRoot 'FirstPersonInstallerBridge.cs'
$manifest=Join-Path $RepositoryRoot 'runtime/NCMMRuntime.manifest'
$bootstrap=Join-Path $payload 'cataclysm-tiles.ncmm-bootstrap.exe'
& $csc /nologo /define:NCMM_FIRST_PERSON_PREVIEW /win32manifest:$manifest /target:winexe /optimize+ /platform:x64 /reference:System.Web.Extensions.dll /out:$bootstrap $io (Join-Path $RepositoryRoot 'runtime/NCMMBootstrap.cs')
if($LASTEXITCODE -ne 0){throw 'Preview bootstrap compilation failed.'}
$ui=Join-Path $OutputRoot '_FirstPersonSetup.cs'
$text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'runtime/NCMMSetup.cs'))
$text=$text.Replace('"NCMM " + SetupCore.RuntimeVersion + " Setup"','"NCMM First Person View TEST - CDDA 1807"')
$text=$text.Replace('certified Host','experimental TEST Host').Replace('Certified Host','Experimental TEST Host').Replace('CERTIFIED HOST','BUNDLED TEST HOST')
$text=$text.Replace('Fetching current experimental TEST Host for the selected CDDA installation...','Verifying the bundled experimental TEST Host...')
[IO.File]::WriteAllText($ui,$text,(New-Object Text.UTF8Encoding($false)))
$setup=Join-Path $stage 'NCMM_Setup_FirstPerson_TEST.exe'
& $csc /nologo /define:NCMM_FIRST_PERSON_PREVIEW /win32manifest:$manifest /target:winexe /optimize+ /platform:x64 /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll /out:$setup $io $core $diagnostics $bridge $ui
if($LASTEXITCODE -ne 0){throw 'Preview installer compilation failed.'}

$files=[ordered]@{}
foreach($file in Get-ChildItem $payload -Recurse -File | Sort-Object FullName){
    $rel=$file.FullName.Substring($payload.Length+1).Replace('\','/')
    $files[$rel]=(Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$revisionParts=@((Join-Path $PSScriptRoot 'src/module.cpp'),(Join-Path $PSScriptRoot 'src/renderer.hpp'),(Join-Path $HostRoot 'first-person-source.json')) | ForEach-Object {(Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant()}
$digest=[Security.Cryptography.SHA256]::Create()
try{$revision='fp-'+([BitConverter]::ToString($digest.ComputeHash([Text.Encoding]::UTF8.GetBytes(($revisionParts -join '-'))))).Replace('-','').ToLowerInvariant()}finally{$digest.Dispose()}
$bundle=[ordered]@{
    schema=1; source_commit=$source.source_commit; upstream_tag=$source.tag
    vanilla_sha256=(Get-FileHash (Join-Path $GameRoot 'cataclysm-tiles.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    host_sha256=$files['host/cataclysm-tiles.ncmm.exe']; patch_revision=$revision
    ncmm_source_commit=(git -C $RepositoryRoot rev-parse HEAD).Trim(); files=$files
}
[IO.File]::WriteAllText((Join-Path $payload 'first-person-bundle.json'),($bundle | ConvertTo-Json -Depth 8),(New-Object Text.UTF8Encoding($false)))
$harness=Join-Path $OutputRoot 'FirstPersonInstallerTests.exe'
& $csc /nologo /define:NCMM_FIRST_PERSON_PREVIEW /win32manifest:$manifest /target:exe /optimize+ /platform:x64 /main:FirstPersonInstallerTests /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll /out:$harness $io $core $bridge (Join-Path $PSScriptRoot 'tests/FirstPersonInstallerTests.cs')
if($LASTEXITCODE -ne 0){throw 'Preview install matrix compilation failed.'}
& $harness $GameRoot $payload
if($LASTEXITCODE -ne 0){throw 'Preview install matrix failed.'}

@'
NCMM First Person View 0.1.0 TEST / CDDA 1807 / Windows x64

1. Extract the complete archive. Run NCMM_Setup_FirstPerson_TEST.exe.
2. Select an official cdda-experimental-2026-10-06-1807 Windows x64 graphics installation.
3. Keep First Person View selected; other bundled modules are optional.
4. Install / Repair selected. Wait for INSTALL COMPLETE.
5. Launch CDDA normally. F6 toggles first person; F7/F8 rotate the camera.

Only the exact official executable is accepted, with SHA256 verification.
This is an experimental build with a locally bundled Host. Automatic stable
Host updates are disabled in this preview. Use the normal NCMM installer to
return to the current certified Host, or Restore vanilla EXE for vanilla play.
Uncheck First Person View and install again to remove its managed DLL/manifest.

Current limits: tile-center movement, one z-level, simple opaque walls/doors,
no ceiling, mouse look or NPC/vehicle geometry. Menus and unsupported vehicle
scenes temporarily use the normal terrain view. Existing movement keys retain
world directions. Camera rotation does not spend a turn. Saves are unchanged.

The bundle includes NCMM Runtime 0.8.2 and the normal optional native modules.
The game itself and external tilesets are not bundled. Keep your existing data,
tilesets and configuration with the selected official CDDA installation.
'@ | Set-Content (Join-Path $stage 'README.txt') -Encoding UTF8
$provenance=[ordered]@{schema=1;product='NCMM First Person View TEST';status='experimental-not-certified';recommended_entry='NCMM_Setup_FirstPerson_TEST.exe';source_commit=$bundle.ncmm_source_commit;engine_source=$source.source_commit;upstream_tag=$source.tag;workflow_run=[string]$env:GITHUB_RUN_ID;installer_sha256=(Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant();host_sha256=$bundle.host_sha256;vanilla_sha256=$bundle.vanilla_sha256;module_sha256=$files['code_mods/FirstPersonView/ncmm_mod.dll']}
[IO.File]::WriteAllText((Join-Path $stage 'release-manifest.json'),($provenance | ConvertTo-Json -Depth 6),(New-Object Text.UTF8Encoding($false)))
$zip=Join-Path $OutputRoot 'NCMM_FirstPersonView_0.1.0_TEST_1807_Windows_x64.zip'
if(Test-Path $zip){Remove-Item $zip -Force}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Output $zip
