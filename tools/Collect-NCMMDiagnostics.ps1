param([string]$GameRoot='', [string]$BuildRoot='C:\NCMMBuild', [string]$OutputDir='')
$ErrorActionPreference='Stop'
$PackageRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'NCMM.Infrastructure.Common.ps1')
$root=Resolve-NcmmGameRoot $GameRoot
if([string]::IsNullOrWhiteSpace($OutputDir)){$OutputDir=Join-Path $env:USERPROFILE 'Downloads\NCMM_Diagnostics'}
New-Item -ItemType Directory -Force $OutputDir|Out-Null
$tmp=Join-Path $env:TEMP ('NCMM_DIAG_'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force $tmp|Out-Null
try{
    $commit=Read-NcmmSourceCommit $root
    $sys=[ordered]@{schema=1;generated_utc=[DateTime]::UtcNow.ToString('o');os=[Environment]::OSVersion.VersionString;is_64bit_os=[Environment]::Is64BitOperatingSystem;is_64bit_process=[Environment]::Is64BitProcess;logical_processors=[Environment]::ProcessorCount;powershell=$PSVersionTable.PSVersion.ToString()}
    Write-NcmmUtf8NoBom (Join-Path $tmp 'system.json') (($sys|ConvertTo-Json -Depth 6)+"`n")
    $game=[ordered]@{schema=1;build_label=[IO.Path]::GetFileName($root);source_commit=$commit;game_root=$root;launch_exe_present=(Test-Path (Join-Path $root 'cataclysm-tiles.exe') -PathType Leaf);vanilla_backup_present=(Test-Path (Join-Path $root 'cataclysm-tiles.vanilla.exe') -PathType Leaf);host_present=(Test-Path (Join-Path $root 'cataclysm-tiles.ncmm.exe') -PathType Leaf)}
    Write-NcmmUtf8NoBom (Join-Path $tmp 'game.json') (($game|ConvertTo-Json -Depth 6)+"`n")
    $feed=Get-NcmmFeedEntry $PackageRoot $commit
    $installedProbe=$null;$installedProbePath=Join-Path $root 'ncmm\experimental_probe.latest.json'
    if(Test-Path $installedProbePath -PathType Leaf){try{$installedProbe=Get-Content $installedProbePath -Raw|ConvertFrom-Json}catch{}}
    $compat=[ordered]@{schema=1;infrastructure='0.8.3.1';source_commit=$commit;feed_entry=$feed;installed_probe=$installedProbe;note='Offline collector: no source download or network probe is performed.'}
    Write-NcmmUtf8NoBom (Join-Path $tmp 'compatibility.json') (($compat|ConvertTo-Json -Depth 12)+"`n")
    $copy=@{
      'ncmm\host.binding.json'='host.binding.json';'ncmm\runtime.state.json'='runtime.state.json';'ncmm\modules.state.json'='modules.state.json';
      'ncmm\ncmm.log'='ncmm.log';'ncmm\diagnostics.txt'='diagnostics.txt';'ncmm\offline_verify.latest.json'='offline_verify.latest.json';'ncmm\runtime_verify.latest.json'='runtime_verify.latest.json';'ncmm\runtime_verification.pending.json'='runtime_verification.pending.json';
      'ncmm\transaction.latest.json'='last_transaction.json';'ncmm\transaction.journal.json'='transaction.journal.json';
      'ncmm\compatibility.latest.json'='runtime_compatibility.latest.json';'ncmm\migration.latest.json'='migration.latest.json';
      'ncmm\recipe_finalize_profile.txt'='recipe_finalize_profile.txt';
      'ncmm\update.state.json'='update.state.json';'ncmm\update.plan.latest.json'='update.plan.latest.json';'ncmm\update.feed.url'='update.feed.url'
    }
    foreach($k in $copy.Keys){$s=Join-Path $root $k;if(Test-Path $s -PathType Leaf){Copy-Item $s (Join-Path $tmp $copy[$k]) -Force}}
    foreach($pkg in @(@{s='components\index.json';d='component.catalog.json'},@{s='migrations\migrations.json';d='migration.registry.json'},@{s='compat\update\index.json';d='embedded.update.feed.json'})){$src=Join-Path $PackageRoot $pkg.s;if(Test-Path $src -PathType Leaf){Copy-Item $src (Join-Path $tmp $pkg.d) -Force}}
    $note=@"
NCMM Infrastructure 0.8.3.1 diagnostic bundle.
Save/world directories are deliberately excluded.
No files from save/, config/, memorial/, graveyard/ or user screenshots are collected.
"@
    Write-NcmmUtf8NoBom (Join-Path $tmp 'README.txt') $note
    $zip=Join-Path $OutputDir ('NCMM_DIAGNOSTICS_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.zip')
    Compress-Archive -Path (Join-Path $tmp '*') -DestinationPath $zip -Force
    Write-Host ('NCMM diagnostics bundle: '+$zip) -ForegroundColor Green
    Write-Output $zip
} finally {Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue}
