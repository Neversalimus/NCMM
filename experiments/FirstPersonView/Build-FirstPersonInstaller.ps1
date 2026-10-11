param(
    [string]$RepositoryRoot=(Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [Parameter(Mandatory=$true)][string]$HostRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string[]]$AdditionalGameRoots=@(),
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
$HostRoot=(Resolve-Path $HostRoot).Path
$GameRoot=(Resolve-Path $GameRoot).Path
$AdditionalGameRoots=@($AdditionalGameRoots | ForEach-Object {(Resolve-Path $_).Path})
$gameRoots=@($GameRoot)+$AdditionalGameRoots
New-Item -ItemType Directory -Force $OutputRoot | Out-Null
$OutputRoot=(Resolve-Path $OutputRoot).Path
$source=Get-Content (Join-Path $HostRoot 'first-person-source.json') -Raw | ConvertFrom-Json
$targets=@{
    '3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'=@{Tag='cdda-experimental-2026-10-01-1040';Build='1040'}
    '074aa98bd5be3de4c35f154082db32a0e63bb0f1'=@{Tag='cdda-experimental-2026-10-06-1807';Build='1807'}
}
$target=$targets[[string]$source.source_commit]
if(-not $target -or $source.tag -ne $target.Tag -or $source.status -ne 'experimental-not-certified'){
    throw 'The preview requires an exact experimental 1040 or 1807 graphics Host.'
}
$build=[string]$target.Build
if(-not(Test-Path (Join-Path $HostRoot 'cataclysm-tiles.exe') -PathType Leaf)){
    throw 'A fully linked experimental Host is required.'
}
$vanillaHashes=@()
foreach($root in $gameRoots){
    $versionText=Get-Content (Join-Path $root 'VERSION.txt') -Raw
    $versionMatch=[regex]::Match($versionText,'commit sha:\s*([0-9a-fA-F]{7,40})(?![0-9a-fA-F])')
    if(-not $versionMatch.Success -or -not ([string]$source.source_commit).StartsWith($versionMatch.Groups[1].Value.ToLowerInvariant())){
        throw "Official $build game identity differs: $root"
    }
    $vanillaHashes+=(Get-FileHash (Join-Path $root 'cataclysm-tiles.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
}
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
# Keep the production bootstrap outside the package to test upgrades from an
# existing normal NCMM installation, rather than merely reinstalling the preview.
$normalBootstrap=Join-Path $OutputRoot '_normal-bootstrap.exe'
Copy-Item $bootstrap $normalBootstrap
& $csc /nologo /define:NCMM_FIRST_PERSON_PREVIEW /win32manifest:$manifest /target:winexe /optimize+ /platform:x64 /reference:System.Web.Extensions.dll /out:$bootstrap $io (Join-Path $RepositoryRoot 'runtime/NCMMBootstrap.cs')
if($LASTEXITCODE -ne 0){throw 'Preview bootstrap compilation failed.'}
$ui=Join-Path $OutputRoot '_FirstPersonSetup.cs'
$text=[IO.File]::ReadAllText((Join-Path $RepositoryRoot 'runtime/NCMMSetup.cs'))
$text=$text.Replace('"NCMM " + SetupCore.RuntimeVersion + " Setup"',('"NCMM First Person View TEST - CDDA '+$build+'"'))
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
$revisionParts=@(@(Get-ChildItem (Join-Path $PSScriptRoot 'src') -File | Sort-Object Name | ForEach-Object {$_.FullName})+(Join-Path $HostRoot 'first-person-source.json')) | ForEach-Object {(Get-FileHash $_ -Algorithm SHA256).Hash.ToLowerInvariant()}
$digest=[Security.Cryptography.SHA256]::Create()
try{$revision='fp-'+([BitConverter]::ToString($digest.ComputeHash([Text.Encoding]::UTF8.GetBytes(($revisionParts -join '-'))))).Replace('-','').ToLowerInvariant()}finally{$digest.Dispose()}
$bundle=[ordered]@{
    schema=1; source_commit=$source.source_commit; upstream_tag=$source.tag
    vanilla_sha256=$vanillaHashes[0]
    supported_vanilla_sha256=@($vanillaHashes | Select-Object -Unique)
    host_sha256=$files['host/cataclysm-tiles.ncmm.exe']; patch_revision=$revision
    ncmm_source_commit=(git -C $RepositoryRoot rev-parse HEAD).Trim(); files=$files
}
[IO.File]::WriteAllText((Join-Path $payload 'first-person-bundle.json'),($bundle | ConvertTo-Json -Depth 8),(New-Object Text.UTF8Encoding($false)))
$harness=Join-Path $OutputRoot 'FirstPersonInstallerTests.exe'
& $csc /nologo /define:NCMM_FIRST_PERSON_PREVIEW /win32manifest:$manifest /target:exe /optimize+ /platform:x64 /main:FirstPersonInstallerTests /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll /out:$harness $io $core $bridge (Join-Path $PSScriptRoot 'tests/FirstPersonInstallerTests.cs')
if($LASTEXITCODE -ne 0){throw 'Preview install matrix compilation failed.'}
& $harness $GameRoot $payload $normalBootstrap @AdditionalGameRoots
if($LASTEXITCODE -ne 0){throw 'Preview install matrix failed.'}

@"
NCMM First Person View installer 0.7.1 TEST / CDDA $build / Windows x64

1. Extract the complete archive. Run NCMM_Setup_FirstPerson_TEST.exe.
2. Select the game folder containing cataclysm-tiles.exe and VERSION.txt for
   $($source.tag). Both official Windows x64 graphics packages are supported,
   with or without bundled sounds (including the CatLauncher download).
3. Keep First Person View selected; other bundled modules are optional.
4. Install / Repair selected. Wait for INSTALL COMPLETE.
5. Launch CDDA normally. F6 toggles first person; F7/F8 rotate; F9/F10 look up/down.
   F11 switches balanced / high quality; balanced is the default.
   Hold the right mouse button over the viewport to look around.
   In first person, arrows move forward/backward and strafe relative to the gaze.

Official executable identities are checked with SHA256. Existing normal NCMM,
preview NCMM and legacy CML bootstraps are recognized by their hash receipts;
the original game backup is verified and preserved when upgrading.
This version adds a hybrid 2.5D/3D software renderer: continuous perspective
floors, filtered tileset materials, modeled vehicle chassis/roof/doors/glass/
seats/wheels, vertical door/window/fence geometry and grounded plants.
Version 0.4 adds sloped windshields, beveled body edges, real wheel-arch openings,
rounded tyres/rims, bumpers, grilles, wipers, door seams/handles and mirrors.
Near surfaces gain distance-filtered material detail and paint/metal highlights.
Transparent sprites are alpha-filtered and rooted using their actual bounds.
Lighting uses CDDA's visible-cell intensity and RGB light cache, face shading
and directional sunlight shadows. Sky, clouds, sun, phased moon and weather
follow the game calendar and current weather; rain/snow stay outside roofs.
The sun uses the game's astronomy; lunar position is an approximation based
on the game's actual phase. Wielded items generate a camera-local silhouette
from the active tileset, with an extruded contour and curved forearms/hands.
An independent depth buffer handles hand/item overlaps and nearby walls.
Changing or unwielding items updates both geometry and that buffer.
Version 0.7 adds material-specific doors, glass, bridges and guardrails, dimension skies,
physical firearm families and attachments, dressed/mutated hands, aimed creature labels,
LMB combat, animated liquid surfaces, volumetric gases/light and GPU atmosphere composition (F12: CPU).
It joins roof panels and adds actual control-mount dashboards/steering
wheels and keeps the full car geometry beyond the 2D visibility mask.
Both hands grip the generated polearm shaft and move with the weapon.
Actual melee attacks trigger a swing or polearm thrust; shots trigger recoil.
Boulders have irregular faceted geometry. Forward terrain is no longer covered
by black unknown-tile planes; real light and opaque walls still govern the view.
Ordinary commands and inventory retain the perspective backdrop. Coordinate
look/targeting and the overmap temporarily use the normal map.
Long CatLauncher paths and Cyrillic folder names are supported during install,
rollback and restore, including existing nested Host backup history.
This is an experimental build with a locally bundled Host. Automatic stable
Host updates are disabled in this preview. Use the normal NCMM installer to
return to the current certified Host, or Restore vanilla EXE for vanilla play.
Uncheck First Person View and install again to remove its managed DLL/manifest.

Current limits: tile-center movement, one visible z-level, automatic coarse
vehicle geometry, sprite-derived held items, generic environment dimensions.
NPC appearance is not modeled yet. Non-arrow movement bindings keep their world
directions. Mouse capture releases when menus open, focus is lost, or F6 disables
the view. Camera rotation does not spend a turn. Saves are unchanged.
Balanced mode limits the internal raster to 1280x800; F11 enables up to
1920x1080. Software rendering uses up to four CPU row bands and caches identical
redraws briefly. Hands and weather compose over an owned world frame, and the
SDL streaming texture is reused. World and camera changes refresh immediately.

The bundle includes NCMM Runtime 0.8.2 and the normal optional native modules.
The game itself and external tilesets are not bundled. Keep your existing data,
tilesets and configuration with the selected official CDDA installation.
"@ | Set-Content (Join-Path $stage 'README.txt') -Encoding UTF8
$provenance=[ordered]@{schema=1;product='NCMM First Person View TEST';installer_revision='0.7.1';status='experimental-not-certified';recommended_entry='NCMM_Setup_FirstPerson_TEST.exe';source_commit=$bundle.ncmm_source_commit;engine_source=$source.source_commit;upstream_tag=$source.tag;workflow_run=[string]$env:GITHUB_RUN_ID;installer_sha256=(Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant();host_sha256=$bundle.host_sha256;vanilla_sha256=$bundle.vanilla_sha256;supported_vanilla_sha256=$bundle.supported_vanilla_sha256;module_sha256=$files['code_mods/FirstPersonView/ncmm_mod.dll']}
[IO.File]::WriteAllText((Join-Path $stage 'release-manifest.json'),($provenance | ConvertTo-Json -Depth 6),(New-Object Text.UTF8Encoding($false)))
$zip=Join-Path $OutputRoot ("NCMM_FirstPersonView_0.7.1_TEST_${build}_Windows_x64.zip")
if(Test-Path $zip){Remove-Item $zip -Force}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Write-Output $zip
