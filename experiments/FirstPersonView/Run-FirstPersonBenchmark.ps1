param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$BundleRoot=$PSScriptRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$TilesetRoot,
    [ValidateRange(0,3600)][int]$SoakSeconds=1800,
    [switch]$FocusedGpuBenchmark,
    [switch]$MovementBenchmark
)
$ErrorActionPreference='Stop'
if($MovementBenchmark -and $FocusedGpuBenchmark){throw 'Choose one focused benchmark.'}
if($MovementBenchmark -and $PSBoundParameters.ContainsKey('SoakSeconds')){throw 'SoakSeconds belongs to the gameplay audit.'}
if($FocusedGpuBenchmark -and $TilesetRoot){throw 'Focused GPU reproduction uses the official tileset; use the gameplay audit for UDP coverage.'}
if($FocusedGpuBenchmark -and $PSBoundParameters.ContainsKey('SoakSeconds')){throw 'SoakSeconds belongs to the gameplay audit, not focused GPU reproduction.'}
$GameRoot=(Resolve-Path $GameRoot).Path
$BundleRoot=(Resolve-Path $BundleRoot).Path
$payload=Join-Path $BundleRoot 'payload'
$bundle=Get-Content (Join-Path $payload 'first-person-bundle.json') -Raw | ConvertFrom-Json
if($bundle.schema -ne 1 -or $bundle.source_commit -notin @('3f7fb352bf492ba521bd9408a0c9f6ce239e8d83','074aa98bd5be3de4c35f154082db32a0e63bb0f1')){throw 'An exact supported First Person TEST bundle is required.'}
$version=Get-Content (Join-Path $GameRoot 'VERSION.txt') -Raw
$commit=[regex]::Match($version,'commit sha:\s*([0-9a-fA-F]{7,40})(?![0-9a-fA-F])')
if(-not $commit.Success -or -not $bundle.source_commit.StartsWith($commit.Groups[1].Value.ToLowerInvariant())){throw 'Game data and bundled Host source pins differ.'}
foreach($file in $bundle.files.PSObject.Properties){
    if((Get-FileHash (Join-Path $payload $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.Value){throw "Bundle SHA256 mismatch: $($file.Name)"}
}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
foreach($source in @($GameRoot,$BundleRoot)){if($OutputRoot.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'OutputRoot must be outside the game and installer folders.'}}
if(Test-Path $OutputRoot){throw 'OutputRoot must be a new folder; existing files are never replaced.'}
New-Item -ItemType Directory $OutputRoot | Out-Null
$test=Join-Path $OutputRoot 'game'
$evidence=Join-Path $OutputRoot 'evidence'
New-Item -ItemType Directory $test,$evidence | Out-Null
# Copy installation assets only. Player saves, configuration and NCMM state
# remain in the source installation. All bundled modules use fresh defaults.
Get-ChildItem $GameRoot -File | Where-Object {$_.Extension -in @('.dll','.txt','.exe')} | Copy-Item -Destination $test
foreach($name in @('data','gfx','font','sound','lang')){
    $source=Join-Path $GameRoot $name
    if(Test-Path $source){Copy-Item $source (Join-Path $test $name) -Recurse}
}
Copy-Item (Join-Path $payload 'code_mods') (Join-Path $test 'code_mods') -Recurse
Copy-Item (Join-Path $payload 'host/cataclysm-tiles.ncmm.exe') (Join-Path $test 'cataclysm-tiles.ncmm.exe')
$fixture=Join-Path $PSScriptRoot 'multilevel_fixture.json'
if(-not (Test-Path $fixture)){$fixture=Join-Path $PSScriptRoot 'tests/multilevel_fixture.json'}
Copy-Item $fixture (Join-Path $test 'data/json/furniture_and_terrain/ncmm_multilevel_qualification.json')
$udp=$false
if($TilesetRoot){
    $TilesetRoot=(Resolve-Path $TilesetRoot).Path
    $identity=Get-Content (Join-Path $TilesetRoot 'tileset.txt') -Raw
    if($identity -notmatch '(?m)^NAME:\s*UndeadPeople_0J_Hybrid_v3\s*$'){throw 'TilesetRoot is not actual UndeadPeople Hybrid v3.'}
    $dest=Join-Path $test 'gfx/UndeadPeople_0J_Hybrid_v3'
    if(Test-Path $dest){Remove-Item $dest -Recurse -Force}
    Copy-Item $TilesetRoot $dest -Recurse
    $udp=$true
    $assets=@(Get-ChildItem $dest -File -Recurse | Sort-Object FullName | ForEach-Object {[ordered]@{file=$_.FullName.Substring($dest.Length+1).Replace('\','/');sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
    $assets | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'udp-asset-sha256.json') -Encoding UTF8
}
$names=@('NCMM_FP_MOVEMENT_ONLY','NCMM_FP_GAMEPLAY_AUDIT','NCMM_FP_GPU_BENCHMARK_ONLY','NCMM_FP_UDP','NCMM_FP_SOAK_SECONDS','NCMM_FP_SAVE_ONLY','NCMM_FP_APPEARANCE_SMOKE','NCMM_FP_APPEARANCE_BEFORE','NCMM_FP_MULTILEVEL_BEFORE','NCMM_FP_STABLE_CPU_BASELINE')
$prior=@{}
foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$null,'Process')}
$exitCode=$null
try {
    if($MovementBenchmark){$env:NCMM_FP_MOVEMENT_ONLY='1'}elseif($FocusedGpuBenchmark){$env:NCMM_FP_GPU_BENCHMARK_ONLY='1'}else{$env:NCMM_FP_GAMEPLAY_AUDIT='1'}
    $env:NCMM_FP_APPEARANCE_SMOKE='1' # Stable monster atlas variants in this diagnostic only.
    $env:NCMM_FP_SOAK_SECONDS=[string]$SoakSeconds
    if($udp){$env:NCMM_FP_UDP='1'}
    $nativeArgs=@('--ncmm-gameplay-smoke','--seed','ncmm-gameplay-0121','--userdir',('"'+$test+'"'))
    $process=Start-Process (Join-Path $test 'cataclysm-tiles.ncmm.exe') -ArgumentList $nativeArgs -WorkingDirectory $test -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence 'first-person-native-stdout.log') -RedirectStandardError (Join-Path $evidence 'first-person-native-stderr.log')
    $null=$process.Handle # Retain the exit-code handle across repeated timed waits.
    $deadline=[DateTime]::UtcNow.AddSeconds($(if($FocusedGpuBenchmark){600}else{$SoakSeconds+1200}))
    $progressPath=Join-Path $test 'ncmm/first-person-audit-progress.txt'
    $lastProgress='native startup'
    $lastAdvance=[DateTime]::UtcNow
    while(-not $process.WaitForExit(20000)){
        if(Test-Path $progressPath){
            $line=Get-Content $progressPath -Tail 1
            if($line -and $line -ne $lastProgress){$lastProgress=$line;$lastAdvance=[DateTime]::UtcNow}
        }
        try {
            $process.Refresh()
            Write-Host ('Native SDL audit: {0}; process_cpu_seconds={1:F2}; working_set_mib={2:F1}' -f $lastProgress,$process.TotalProcessorTime.TotalSeconds,($process.WorkingSet64/1MB))
        } catch {Write-Host "Native SDL audit: $lastProgress"}
        if(([DateTime]::UtcNow-$lastAdvance).TotalSeconds -gt 180){
            # Capture the native error log and this process's own window before
            # terminating it. A modal CDDA debug prompt must not look like a
            # renderer hang with its diagnostic text discarded.
            $diagnostics=@(
                (Join-Path $test 'config/debug.log'),(Join-Path $test 'debug.log'),
                (Join-Path $test 'ncmm/ncmm.log'),(Join-Path $test 'ncmm/runtime.log'),
                (Join-Path $evidence 'first-person-native-stdout.log'),
                (Join-Path $evidence 'first-person-native-stderr.log')
            )
            $runtimeLogs=Join-Path $test 'ncmm'
            if(Test-Path -LiteralPath $runtimeLogs){$diagnostics+=@(Get-ChildItem -LiteralPath $runtimeLogs -File -Filter '*.log' | Select-Object -ExpandProperty FullName)}
            foreach($diagnostic in @($diagnostics | Select-Object -Unique)){
                if(Test-Path -LiteralPath $diagnostic){
                    Write-Host ('Native diagnostic: '+[IO.Path]::GetFileName($diagnostic))
                    Get-Content -LiteralPath $diagnostic -Tail 32 | Write-Host
                }
            }
            try {
                Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
public static class NCMMBenchmarkWindow {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
    public static bool Capture(IntPtr window,string path) {
        Rect rect;
        if(window==IntPtr.Zero||!GetWindowRect(window,out rect))return false;
        int width=rect.Right-rect.Left,height=rect.Bottom-rect.Top;
        if(width<1||height<1||width>4096||height>4096)return false;
        using(Bitmap bitmap=new Bitmap(width,height))using(Graphics graphics=Graphics.FromImage(bitmap)) {
            graphics.CopyFromScreen(rect.Left,rect.Top,0,0,new Size(width,height));
            bitmap.Save(path,System.Drawing.Imaging.ImageFormat.Png);
            return true;
        }
    }
}
'@
                $process.Refresh()
                $stallImage=Join-Path $evidence 'first-person-native-stall-window.png'
                $captured=[NCMMBenchmarkWindow]::Capture($process.MainWindowHandle,$stallImage)
                Write-Host "Native stalled-window capture: $captured"
                # Keep a bounded copy in CI logs when artifact byte transfer is
                # unavailable. Never emit desktop pixels from a local run.
                if($captured -and $env:GITHUB_ACTIONS -eq 'true'){
                    $pixels=[IO.File]::ReadAllBytes($stallImage)
                    if($pixels.Length -le 262144){
                        $encoded=[Convert]::ToBase64String($pixels)
                        Write-Host "NCMM_STALL_PNG_BEGIN bytes=$($pixels.Length)"
                        for($offset=0;$offset -lt $encoded.Length;$offset+=8192){
                            Write-Host ('NCMM_STALL_PNG_DATA '+$encoded.Substring($offset,[Math]::Min(8192,$encoded.Length-$offset)))
                        }
                        Write-Host 'NCMM_STALL_PNG_END'
                    }
                }
            } catch {Write-Warning "Native window capture unavailable: $_"}
            & taskkill.exe /pid $process.Id /t /f
            $null=$process.WaitForExit(10000)
            throw "Native audit made no progress for 180 seconds: $lastProgress"
        }
        if([DateTime]::UtcNow -gt $deadline){& taskkill.exe /pid $process.Id /t /f;throw "Native benchmark deadline exceeded: $lastProgress"}
    }
    $process.WaitForExit();$exitCode=$process.ExitCode
} finally {
    foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}
    $runtime=Join-Path $test 'ncmm'
    if(Test-Path $runtime){Get-ChildItem $runtime -File | Where-Object {$_.Name -like 'first-person-*' -or $_.Extension -eq '.log'} | Copy-Item -Destination $evidence}
    foreach($name in @('debug.log','debug.log.prev','crash.log')){
        $nativeLog=Join-Path $test ('config/'+$name)
        if(Test-Path $nativeLog){Copy-Item $nativeLog (Join-Path $evidence ('first-person-cdda-'+$name))}
    }
}
if($exitCode -ne 0){
    $reason='native result unavailable'
    $nativeResult=Join-Path $evidence 'first-person-smoke.json'
    if(Test-Path $nativeResult){
        $nativeText=Get-Content $nativeResult -Raw
        try {$reason=($nativeText | ConvertFrom-Json).reason}
        catch {$match=[regex]::Match($nativeText,'"reason"\s*:\s*"([^"\r\n]*)"');if($match.Success){$reason=$match.Groups[1].Value}}
    }
    throw "Native audit failed ($exitCode): $reason. Evidence: $evidence"
}
if($MovementBenchmark){
    $audit=Get-Content (Join-Path $evidence 'first-person-movement-benchmark.json') -Raw | ConvertFrom-Json
    if($audit.success -ne $true -or $audit.actual_cdda_sdl -ne $true -or $audit.simulation_turns_advanced -ne 0 -or @($audit.records).Count -ne 12){throw 'Native movement benchmark is incomplete.'}
    foreach($record in $audit.records){
        if($record.native_steps -ne 12 -or @($record.first_ready_samples_ms).Count -ne 12 -or @($record.easing_frame_samples_ms).Count -eq 0){throw 'Native steps or easing samples are incomplete.'}
        if(@($record.cold_rotation_samples_ms).Count -ne 48 -or @($record.warm_rotation_samples_ms).Count -ne 48){throw 'Continuous cold/warm rotation samples are incomplete.'}
        if($record.renderer -eq 'gpu' -and @($record.frame_renderers_gpu | Where-Object {$_ -ne 1}).Count){throw 'A walking GPU frame fell back to CPU.'}
    }
    $soaked=0
}elseif($FocusedGpuBenchmark){
    $audit=Get-Content (Join-Path $evidence 'first-person-gpu-benchmark.json') -Raw | ConvertFrom-Json
    if($audit.actual_cdda_sdl -ne $true -or $audit.focused_reproduction -ne $true -or @($audit.records).Count -ne 4){throw 'Focused GPU reproduction is incomplete.'}
    $soaked=0
}else{
    $audit=Get-Content (Join-Path $evidence 'first-person-gameplay-audit.json') -Raw | ConvertFrom-Json
    if($audit.success -ne $true -or $audit.manual_playthrough -ne $false -or $audit.soak.actual_wall_seconds -lt $SoakSeconds){throw 'Native audit is incomplete.'}
    $soaked=$audit.soak.actual_wall_seconds
}
$result=[ordered]@{success=$true;source_commit=$bundle.source_commit;ncmm_commit=$bundle.ncmm_source_commit;host_sha256=$bundle.host_sha256;module_sha256=$bundle.files.'code_mods/FirstPersonView/ncmm_mod.dll';manual_playthrough=$false;focused_gpu_benchmark=[bool]$FocusedGpuBenchmark;movement_benchmark=[bool]$MovementBenchmark;udp_requested=$udp;soak_seconds=$soaked;evidence=$evidence}
$result | ConvertTo-Json | Set-Content (Join-Path $OutputRoot 'benchmark-provenance.json') -Encoding UTF8
$result | ConvertTo-Json
