param([Parameter(Mandatory=$true)][string]$Plan,[int]$StartIndex=0,[int]$Count=8)
$ErrorActionPreference='Stop'
$planFile=(Resolve-Path -LiteralPath $Plan).Path
$taskLogRoot=Split-Path -Parent $planFile
if($StartIndex -lt 0 -or $Count -lt 1){throw 'Invalid run interval.'}
$settings=Get-Content -LiteralPath $planFile -Raw | ConvertFrom-Json
if($settings.schema -ne 1 -or $settings.expected_vsync -ne 1 -or @($settings.runs).Count -ne 8){throw 'Predeclared ordinary four-mode plan required.'}
$components=@{baseline=@($false,$false,$false);assets_hud=@($false,$true,$true);optimization=@($true,$false,$false);combined=@($true,$true,$true)}
$priorVsync=[Environment]::GetEnvironmentVariable('SDL_RENDER_VSYNC','Process')
try {
    Remove-Item -LiteralPath Env:SDL_RENDER_VSYNC -ErrorAction SilentlyContinue
    for($index=$StartIndex;$index -lt [Math]::Min($StartIndex+$Count,8);$index++){
        $entry=$settings.runs[$index]
        if(-not $components.ContainsKey($entry.mode)){throw 'Unknown component mode.'}
        if(Get-Process -Name 'cataclysm-tiles*' -ErrorAction SilentlyContinue){throw 'Another Native game is active; ordinary timing must be isolated.'}
        $bundle=if($entry.mode -eq 'baseline'){$settings.baseline_bundle}else{$settings.combined_bundle}
        $runner=Join-Path $bundle 'Run-FirstPersonBenchmark.ps1'
        if(Test-Path -LiteralPath $entry.root){throw 'Each comparison output must be a fresh directory.'}
        $stamp=[ordered]@{schema=1;mode=$entry.mode;direction=$entry.direction;components=$components[$entry.mode];
            profiler_attached=$false;gpu_debug=$false;vsync_environment_hint=$null;
            plan_sha256=(Get-FileHash -LiteralPath $planFile -Algorithm SHA256).Hash.ToLowerInvariant();
            runner_sha256=(Get-FileHash -LiteralPath $runner -Algorithm SHA256).Hash.ToLowerInvariant();
            controller_sha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant();
            started_utc=[DateTime]::UtcNow.ToString('o')}
        $arguments=@{GameRoot=$settings.game_root;BundleRoot=$bundle;OutputRoot=$entry.root;TilesetRoot=$settings.tileset_root;
            MovementBenchmark=$true;Extended=$true;FullSizeGpuMatrix=$true}
        if($entry.mode -eq 'assets_hud'){$arguments.DisableCameraSnapshot=$true}
        if($entry.mode -eq 'optimization'){$arguments.DisableHeldAssets=$true;$arguments.DisableTargetHud=$true}
        $stem='four-'+$index+'-'+$entry.mode+'-'+$entry.direction
        $log=Join-Path $taskLogRoot ($stem+'.log')
        if(Test-Path -LiteralPath $log){throw 'Preserve earlier comparison logs.'}
        nvidia-smi --query-gpu=name,driver_version,pstate,clocks.current.graphics,clocks.current.memory,temperature.gpu,power.draw,utilization.gpu,memory.used --format=csv | Set-Content (Join-Path $taskLogRoot ($stem+'-gpu-before.csv'))
        Write-Output ('Starting ordinary Native comparison '+$stem)
        try {
            & $runner @arguments *> $log
            if(-not $?){throw 'Native benchmark wrapper failed.'}
            $provenance=Get-Content -LiteralPath (Join-Path $entry.root 'benchmark-provenance.json') -Raw | ConvertFrom-Json
            if($provenance.success -ne $true){throw 'Native wrapper did not verify the run.'}
            $stamp.wrapper_success=$true
        } finally {
            $stamp.finished_utc=[DateTime]::UtcNow.ToString('o')
            if(Test-Path -LiteralPath $entry.root){$stamp | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $entry.root 'ordinary-launch.json') -Encoding UTF8}
            nvidia-smi --query-gpu=name,driver_version,pstate,clocks.current.graphics,clocks.current.memory,temperature.gpu,power.draw,utilization.gpu,memory.used --format=csv | Set-Content (Join-Path $taskLogRoot ($stem+'-gpu-after.csv'))
        }
        Write-Output ('Completed '+$stem)
    }
} finally {[Environment]::SetEnvironmentVariable('SDL_RENDER_VSYNC',$priorVsync,'Process')}
