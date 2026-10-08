param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path
$generator=Join-Path $root 'tools\New-NCMMNativeModule.ps1'
$preview=Join-Path $root 'tools\New-NCMMModuleRegistrationPreview.ps1'
$tmp=Join-Path $env:TEMP ('ncmm-registration-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $tmp | Out-Null
$src=Join-Path $tmp 'source'
$destA=Join-Path $tmp 'preview-a'
$destB=Join-Path $tmp 'preview-b'
New-Item -ItemType Directory -Force $src,$destA,$destB | Out-Null
$tracked=@('components\index.json','components\native-build.json')
$baseline=@{}
foreach($path in $tracked) {
  $baseline[$path]=(Get-FileHash (Join-Path $root $path) -Algorithm SHA256).Hash
}
$utf8=New-Object Text.UTF8Encoding($false)
function Write-NcmmFixture($Path,$Value) {
  [IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 30)+[Environment]::NewLine,$utf8)
}
function Check-Rejection([string]$Name,[hashtable]$Params) {
  $rejected=$false
  try { $null=& $preview @Params }
  catch { $rejected=$true }
  if(-not $rejected) { throw "Registration preview accepted invalid fixture: $Name" }
}
try {
  $starter=& $generator -Id 'next_generation_mod' -Name 'Next Generation Mod' -Version '2.3.4' -DestinationRoot $src -RepositoryRoot $root
  $one=& $preview -ModuleRoot $starter.Destination -DestinationRoot $destA -RepositoryRoot $root
  $two=& $preview -ModuleRoot $starter.Destination -DestinationRoot $destB -RepositoryRoot $root
  if($one.Status -cne 'review-required' -or $two.Status -cne 'review-required' -or
     $one.Folder -cne 'NextGenerationMod') { throw 'Unexpected registration preview result.' }
  $files=@('components\index.json','components\native-build.json',
           'components\next_generation_mod.json','REVIEW_REQUIRED.txt')
  foreach($file in $files) {
    $a=Join-Path $one.Destination $file
    $b=Join-Path $two.Destination $file
    if(-not(Test-Path $a -PathType Leaf) -or -not(Test-Path $b -PathType Leaf) -or
       (Get-FileHash $a -Algorithm SHA256).Hash -cne
       (Get-FileHash $b -Algorithm SHA256).Hash) {
      throw "Registration preview is incomplete or nondeterministic: $file"
    }
  }
  $previewCatalog=Get-Content (Join-Path $one.Destination 'components\index.json') -Raw|ConvertFrom-Json
  $previewBuild=Get-Content (Join-Path $one.Destination 'components\native-build.json') -Raw|ConvertFrom-Json
  $previewDescriptor=Get-Content (Join-Path $one.Destination 'components\next_generation_mod.json') -Raw|ConvertFrom-Json
  if(@($previewCatalog.components|Where-Object { $_.id -eq 'next_generation_mod' }).Count -ne 1 -or
     @($previewCatalog.atomic_groups.'next-generation-mod').Count -ne 1 -or
     $previewCatalog.atomic_groups.'next-generation-mod'[0] -cne 'next_generation_mod' -or
     @($previewBuild.modules|Where-Object { $_.id -eq 'next_generation_mod' }).Count -ne 1 -or
     $previewDescriptor.id -cne 'next_generation_mod') {
    throw 'Staged component catalog, group, build recipe or descriptor mismatch.'
  }
  $planPath=Join-Path $starter.Destination 'NCMM_REGISTRATION.json'
  $manifestPath=Join-Path $starter.Destination 'mod.json'
  $originalPlan=[IO.File]::ReadAllText($planPath,[Text.Encoding]::UTF8)
  $originalManifest=[IO.File]::ReadAllText($manifestPath,[Text.Encoding]::UTF8)
  $invalid=@(
    @{Name='bad registration schema';Kind='plan';Change={param($j) $j.schema=10}},
    @{Name='unsafe publish policy';Kind='plan';Change={param($j) $j.policy='automatically-publish'}},
    @{Name='descriptor version drift';Kind='plan';Change={param($j) $j.component_descriptor.version='9.9.9'}},
    @{Name='unknown folder';Kind='plan';Change={param($j) $j.native_build_entry.folder='../outside'}},
    @{Name='duplicate or unsafe package';Kind='plan';Change={param($j) $j.native_build_entry.archive_stem='ItemGlyphs'}},
    @{Name='build profile bypass';Kind='plan';Change={param($j) $j.native_build_entry.smoke_profile='skip'}},
    @{Name='missing capability smoke';Kind='plan';Change={param($j) $j.native_build_entry.missing_contract_smoke=$false}},
    @{Name='source payload injection';Kind='plan';Change={param($j) $j.native_build_entry.required_payload_files=@('../invalid')}},
    @{Name='descriptor dependency drift';Kind='plan';Change={param($j) $j.component_descriptor.dependencies[1].capability='unknown.v1'}},
    @{Name='wrong host version floor';Kind='plan';Change={param($j) $j.catalog_component.dependencies[0].min_version='0.0.0'}},
    @{Name='missing catalog capability';Kind='plan';Change={param($j) $j.catalog_component.dependencies[2].capability='other.capability'}},
    @{Name='wrong manifest ABI';Kind='manifest';Change={param($j) $j.loader_api=5}},
    @{Name='wrong manifest identity';Kind='manifest';Change={param($j) $j.id='item_glyphs'}},
    @{Name='missing manifest capability';Kind='manifest';Change={param($j) $j.requires=@('core.v1')}}
  )
  foreach($fixture in $invalid) {
    try {
      if($fixture.Kind -eq 'plan') {
        $data=ConvertFrom-Json $originalPlan
        & $fixture.Change $data
        Write-NcmmFixture $planPath $data
      } else {
        $data=ConvertFrom-Json $originalManifest
        & $fixture.Change $data
        Write-NcmmFixture $manifestPath $data
      }
      $arguments=@{ModuleRoot=$starter.Destination;DestinationRoot=$destB;RepositoryRoot=$root}
      # destB already has preview: choose a fresh directory so the failure
      # must come from validation, never the output collision.
      $fixtureTarget=Join-Path $tmp ('negative-'+[guid]::NewGuid().ToString('N'))
      New-Item -ItemType Directory $fixtureTarget | Out-Null
      $arguments.DestinationRoot=$fixtureTarget
      Check-Rejection -Name $fixture.Name -Params $arguments
      if(@(Get-ChildItem $fixtureTarget -Force).Count -ne 0) {
        throw "Rejected registration left behind output: $($fixture.Name)"
      }
    } finally {
      [IO.File]::WriteAllText($planPath,$originalPlan,$utf8)
      [IO.File]::WriteAllText($manifestPath,$originalManifest,$utf8)
    }
  }
  Check-Rejection -Name 'overwrite existing preview' -Params @{
    ModuleRoot=$starter.Destination;DestinationRoot=$destA;RepositoryRoot=$root
  }
  Check-Rejection -Name 'output inside live repo' -Params @{
    ModuleRoot=$starter.Destination;DestinationRoot=$root;RepositoryRoot=$root
  }
  Check-Rejection -Name 'output inside source' -Params @{
    ModuleRoot=$starter.Destination;DestinationRoot=$starter.Destination;RepositoryRoot=$root
  }
  foreach($path in $tracked) {
    if((Get-FileHash (Join-Path $root $path) -Algorithm SHA256).Hash -cne $baseline[$path]) {
      throw "Registration preview mutated live repository: $path"
    }
  }
  Write-Host ("NCMM registration preview: PASS ({0} deterministic staged files, {1} negative mutation fixtures + 3 path/overwrite guards)." -f $files.Count,$invalid.Count) -ForegroundColor Green
} finally {
  Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
}
