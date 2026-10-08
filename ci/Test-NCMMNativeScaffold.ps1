param(
  [string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent),
  [switch]$Compile,
  [string]$SmokeHostExecutable=''
)
$ErrorActionPreference='Stop'
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
$generator=Join-Path $RepositoryRoot 'tools\New-NCMMNativeModule.ps1'
$tmp=Join-Path $env:TEMP ('ncmm-sdk-scaffold-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
$catalogFiles=@('components\index.json','components\native-build.json')
$before=@{}
foreach($path in $catalogFiles) {
  $before[$path]=(Get-FileHash (Join-Path $RepositoryRoot $path) -Algorithm SHA256).Hash
}
function Assert-Refused {
  param([string]$Label,[hashtable]$Args)
  $rejected=$false
  try { $null=& $generator @Args }
  catch { $rejected=$true }
  if(-not $rejected) { throw "Scaffolder accepted invalid request: $Label" }
}
try {
  $a=Join-Path $tmp 'one'
  $b=Join-Path $tmp 'two'
  New-Item -ItemType Directory -Force $a,$b | Out-Null
  $first=& $generator -Id 'sample_future_mod' -Name 'Sample Future Mod' -Version '1.2.3' -DestinationRoot $a -RepositoryRoot $RepositoryRoot
  $second=& $generator -Id 'sample_future_mod' -Name 'Sample Future Mod' -Version '1.2.3' -DestinationRoot $b -RepositoryRoot $RepositoryRoot
  if($first.Folder -cne 'SampleFutureMod' -or $first.SmokeProfile -cne 'generic' -or
     $second.Folder -cne $first.Folder) { throw 'Unexpected generated module identity.' }
  $required=@('CMakeLists.txt','mod.json','NCMM_REGISTRATION.json','README.md','src\module.cpp')
  foreach($file in $required) {
    $left=Join-Path $first.Destination $file
    $right=Join-Path $second.Destination $file
    if(-not(Test-Path $left -PathType Leaf) -or
       -not(Test-Path $right -PathType Leaf) -or
       (Get-FileHash $left -Algorithm SHA256).Hash -cne (Get-FileHash $right -Algorithm SHA256).Hash) {
       throw "Generated file missing or nondeterministic: $file"
    }
    $bytes=[IO.File]::ReadAllBytes($left)
    if($bytes.Length -lt 10 -or
       ($bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191)) {
       throw "Generated file is empty or has UTF-8 BOM: $file"
    }
  }
  $manifest=Get-Content (Join-Path $first.Destination 'mod.json') -Raw | ConvertFrom-Json
  $registration=Get-Content (Join-Path $first.Destination 'NCMM_REGISTRATION.json') -Raw | ConvertFrom-Json
  if($manifest.id -cne 'sample_future_mod' -or $manifest.version -cne '1.2.3' -or
     $manifest.loader_api -ne 1 -or $manifest.failure_policy -ne 'disable' -or
     $registration.policy -cne 'review-required-not-registered' -or
     $registration.catalog_component.atomic_group -cne 'sample-future-mod' -or
     $registration.component_descriptor.id -cne $manifest.id -or
     $registration.native_build_entry.folder -cne $first.Folder -or
     $registration.native_build_entry.smoke_profile -cne 'generic' -or
     -not $registration.native_build_entry.missing_contract_smoke) {
    throw 'Generated manifest and build/catalog registration disagree.'
  }
  foreach($cap in @('api.versioning.v1','host_api.v2.core','settings.typed.v2')) {
    if(@($manifest.requires) -cnotcontains $cap -or
       @($registration.catalog_component.dependencies|Where-Object { $_.capability -ceq $cap }).Count -ne 1) {
      throw "Generated capability contract drift: $cap"
    }
  }
  $valid=[ordered]@{
    Id='another_valid_mod';Name='Another Valid Mod';DestinationRoot=$a;RepositoryRoot=$RepositoryRoot
  }
  $bad=@(
    @{Label='existing id';Change={param($p) $p.Id='item_glyphs'}},
    @{Label='uppercase id';Change={param($p) $p.Id='Uppercase'}},
    @{Label='path separator';Change={param($p) $p.Id='../escape'}},
    @{Label='consecutive underscores';Change={param($p) $p.Id='unsafe__id'}},
    @{Label='too short id';Change={param($p) $p.Id='ab'}},
    @{Label='shell code in name';Change={param($p) $p.Name='Test "quoted"'}},
    @{Label='newline name';Change={param($p) $p.Name="Invalid$([Environment]::NewLine)Name"}},
    @{Label='bad version';Change={param($p) $p.Version='1.2.3/../'}},
    @{Label='repo root';Change={param($p) $p.DestinationRoot=$RepositoryRoot}},
    @{Label='repo subdirectory';Change={param($p) $p.DestinationRoot=(Join-Path $RepositoryRoot 'tools')}},
    @{Label='output collision';Change={param($p) $p.Id='sample_future_mod'}}
  )
  foreach($fixture in $bad) {
    $args=@{}
    foreach($key in $valid.Keys) { $args[$key]=$valid[$key] }
    & $fixture.Change $args
    Assert-Refused -Label $fixture.Label -Args $args
  }
  $russian=& $generator -Id 'sample_russian_mod' -Name 'Тестовый мод' -DestinationRoot $a -RepositoryRoot $RepositoryRoot
  $rus=Get-Content (Join-Path $russian.Destination 'mod.json') -Raw | ConvertFrom-Json
  if($rus.name -cne 'Тестовый мод') { throw 'Unicode module display name roundtrip failed.' }

  foreach($path in $catalogFiles) {
    if((Get-FileHash (Join-Path $RepositoryRoot $path) -Algorithm SHA256).Hash -cne $before[$path]) {
      throw "Scaffolder modified production registry: $path"
    }
  }
  if($Compile) {
    if(-not(Test-Path $SmokeHostExecutable -PathType Leaf)) {
      throw 'Compile smoke requires production smoke host executable.'
    }
    $build=Join-Path $tmp 'build'
    $sdk=Join-Path $RepositoryRoot 'sdk'
    cmake -S $first.Destination -B $build -A x64 ("-DNCMM_SDK_INCLUDE_DIR="+$sdk)
    if($LASTEXITCODE -ne 0) { throw 'Generated CMake configure failed.' }
    cmake --build $build --config Release
    if($LASTEXITCODE -ne 0) { throw 'Generated module compilation failed.' }
    $dll=Get-ChildItem $build -Filter 'ncmm_mod.dll' -Recurse -File | Select-Object -First 1
    if(-not $dll) { throw 'Generated module DLL missing.' }
    & $SmokeHostExecutable $dll.FullName '--generic=sample_future_mod@1.2.3'
    if($LASTEXITCODE -ne 0) { throw 'Generated module normal lifecycle smoke failed.' }
    & $SmokeHostExecutable $dll.FullName '--generic=sample_future_mod@1.2.3' '--missing-contract'
    if($LASTEXITCODE -ne 0) { throw 'Generated module missing-capability smoke failed.' }
    & $SmokeHostExecutable $dll.FullName '--generic=wrong_module@1.2.3'
    if($LASTEXITCODE -eq 0) { throw 'Generated module smoke accepted wrong ID.' }
  }
  Write-Host ("NCMM SDK scaffolder: PASS ({0} deterministic files, {1} reject cases; compile={2})." -f $required.Count,$bad.Count,$Compile) -ForegroundColor Green
} finally {
  Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
