param(
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [string]$BundleRoot,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$TilesetRoot,
    [ValidateRange(0,3600)][int]$SoakSeconds=1800,
    [switch]$FocusedGpuBenchmark,
    [switch]$MovementBenchmark,
    [switch]$CaptureCrashDump,
    [switch]$GpuDebug,
    [string]$DumpAnalyzer
)
$ErrorActionPreference='Stop'
function Resolve-NativeBenchmarkRoot([string]$Path,[string[]]$Sources){
    $full=[IO.Path]::GetFullPath($Path)
    # Conservative harness budget for encoded avatar names and memory archives;
    # this does not change upstream save IO or claim arbitrary long-path support.
    if($full.Length -gt 80){throw 'Native CDDA save paths require a diagnostic OutputRoot of at most 80 characters. Choose a new short folder such as C:\ncmm-test\run-1. No game or save has been changed.'}
    foreach($source in $Sources){
        $parent=[IO.Path]::GetFullPath($source).TrimEnd('\','/')
        if($full.Equals($parent,[StringComparison]::OrdinalIgnoreCase) -or
           $full.StartsWith($parent+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
            throw 'OutputRoot must be outside the game and installer folders.'
        }
    }
    return $full
}
if(-not $BundleRoot){$BundleRoot=$PSScriptRoot}
if(-not $DumpAnalyzer){$DumpAnalyzer=Join-Path $PSScriptRoot 'ncmm_native_dump_analyzer.exe'}
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
$OutputRoot=Resolve-NativeBenchmarkRoot $OutputRoot @($GameRoot,$BundleRoot)
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
# Close only this diagnostic child's explicit fatal-error dialog. CDDA writes
# the native stack after its crash modal returns; an ordinary prompt is untouched.
if(-not ('NCMMNativeCrashDialog' -as [type])){
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class NCMMNativeCrashDialog {
    private delegate bool EnumProc(IntPtr window, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr data);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder text, int maximum);
    [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    public static bool IsCrashText(string text) {
        return text != null && text.IndexOf("The program has crashed.", StringComparison.Ordinal) >= 0;
    }
    private static string WindowText(IntPtr window) {
        var text = new StringBuilder(32768);
        GetWindowText(window, text, text.Capacity);
        return text.ToString();
    }
    public static string CloseOwnedCrash(uint processId) {
        string crash = null;
        EnumWindows(delegate(IntPtr window, IntPtr data) {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if(owner != processId || WindowText(window) != "Error") return true;
            var className = new StringBuilder(256);
            GetClassName(window, className, className.Capacity);
            if(className.ToString() != "#32770") return true;
            var content = new StringBuilder();
            EnumChildWindows(window, delegate(IntPtr child, IntPtr unused) {
                content.AppendLine(WindowText(child));
                return true;
            }, IntPtr.Zero);
            if(!IsCrashText(content.ToString())) return true;
            crash = content.ToString();
            // CDDA writes its backtrace only after this owned crash modal closes.
            PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
            return false;
        }, IntPtr.Zero);
        return crash;
    }
}
'@
}
$names=@('NCMM_FP_MOVEMENT_ONLY','NCMM_FP_GAMEPLAY_AUDIT','NCMM_FP_GPU_BENCHMARK_ONLY','NCMM_FP_UDP','NCMM_FP_SOAK_SECONDS','NCMM_FP_SAVE_ONLY','NCMM_FP_APPEARANCE_SMOKE','NCMM_FP_APPEARANCE_BEFORE','NCMM_FP_MULTILEVEL_BEFORE','NCMM_FP_STABLE_CPU_BASELINE','SDL_RENDER_GPU_DEBUG','SDL_ASSERT')
$prior=@{}
# Modern .NET preserves an empty environment variable. PowerShell can convert
# $null to that empty string at the .NET call boundary, and native getenv then
# treats SAVE_ONLY (or a competing diagnostic mode) as enabled. Remove it.
foreach($name in $names){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process');Remove-Item -LiteralPath ('Env:'+ $name) -ErrorAction SilentlyContinue}
$exitCode=$null
$debugger=$null
$failure=$null
$process=$null
$lastProgress='native startup'
try {
    if($MovementBenchmark){$env:NCMM_FP_MOVEMENT_ONLY='1'}elseif($FocusedGpuBenchmark){$env:NCMM_FP_GPU_BENCHMARK_ONLY='1'}else{$env:NCMM_FP_GAMEPLAY_AUDIT='1'}
    if($GpuDebug){$env:SDL_RENDER_GPU_DEBUG='1'}
    $env:SDL_ASSERT='abort' # Diagnostic child must fail with evidence, never wait in SDL's assertion modal.
    $env:NCMM_FP_APPEARANCE_SMOKE='1' # Stable monster atlas variants in this diagnostic only.
    $env:NCMM_FP_SOAK_SECONDS=[string]$SoakSeconds
    if($udp){$env:NCMM_FP_UDP='1'}
    $nativeArgs=@('--ncmm-gameplay-smoke','--seed','ncmm-gameplay-0121','--userdir',('"'+$test+'"'))
    $process=Start-Process (Join-Path $test 'cataclysm-tiles.ncmm.exe') -ArgumentList $nativeArgs -WorkingDirectory $test -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence 'first-person-native-stdout.log') -RedirectStandardError (Join-Path $evidence 'first-person-native-stderr.log')
    $null=$process.Handle # Retain the exit-code handle across repeated timed waits.
    if($CaptureCrashDump){
        if(-not ('NCMMNativeDebugger' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'NativeDiagnosticDebugger.cs')}
        $debugger=[NCMMNativeDebugger]::Start([uint32]$process.Id,$evidence)
    }
    $deadline=[DateTime]::UtcNow.AddSeconds($(if($FocusedGpuBenchmark){600}else{$SoakSeconds+1200}))
    $progressPath=Join-Path $test 'ncmm/first-person-audit-progress.txt'
    $lastProgress='native startup'
    $lastAdvance=[DateTime]::UtcNow
    while(-not $process.WaitForExit(20000)){
        $nativeCrash=[NCMMNativeCrashDialog]::CloseOwnedCrash([uint32]$process.Id)
        if($nativeCrash){
            $nativeCrash | Set-Content (Join-Path $evidence 'first-person-native-crash-window.txt') -Encoding UTF8
            Write-Host "Native crash dialog: $nativeCrash"
            if(-not $process.WaitForExit(30000)){
                & taskkill.exe /pid $process.Id /t /f
                $null=$process.WaitForExit(10000)
            }
            throw "Native diagnostic crashed. Evidence: $evidence"
        }
        if(Test-Path $progressPath){
            $line=Get-Content $progressPath -Tail 1
            if($line -and $line -ne $lastProgress){$lastProgress=[string]$line;$lastAdvance=[DateTime]::UtcNow}
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
                $drawingReferences=@('System.Drawing')
                foreach($drawingAssembly in @('System.Drawing.Common.dll','System.Drawing.Primitives.dll','System.Private.Windows.Core.dll','System.Private.Windows.GdiPlus.dll')){
                    $drawingPath=Join-Path $PSHOME $drawingAssembly
                    if(Test-Path -LiteralPath $drawingPath){$drawingReferences+=$drawingPath}
                }
                Add-Type -ReferencedAssemblies $drawingReferences -TypeDefinition @'
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
} catch {
    $failure=$_.Exception.Message
    if($process -and -not $process.HasExited){$process.Kill();$null=$process.WaitForExit(10000)}
    throw
} finally {
    if($debugger){$debugger.Dispose()}
    if($process -and $process.HasExited){$process.WaitForExit();$exitCode=$process.ExitCode}
    if($progressPath -and (Test-Path -LiteralPath $progressPath)){$lastProgress=[string](Get-Content -LiteralPath $progressPath -Tail 1)}
    [ordered]@{schema=1;child_pid=$(if($process){$process.Id}else{$null});exit_signed=$exitCode;exit_hex=$(if($null -ne $exitCode){'0x'+([BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$exitCode),0)).ToString('X8')}else{$null});last_progress=$lastProgress;wrapper_failure=$failure;debugger_requested=[bool]$CaptureCrashDump;debugger_error=$(if($debugger){$debugger.Error}else{$null});crash_dumps=$(if($debugger){$debugger.Dumps}else{0});host_sha256=$bundle.host_sha256;module_sha256=$bundle.files.'code_mods/FirstPersonView/ncmm_mod.dll';ncmm_commit=$bundle.ncmm_source_commit;engine_commit=$bundle.source_commit;focused_gpu=[bool]$FocusedGpuBenchmark;movement=[bool]$MovementBenchmark} | ConvertTo-Json | Set-Content (Join-Path $evidence 'first-person-process-result.json') -Encoding UTF8
    try { if($CaptureCrashDump -and (Test-Path -LiteralPath $DumpAnalyzer) -and (Test-Path -LiteralPath (Join-Path $evidence 'first-person-debug-events.log'))){
        $events=Get-Content -LiteralPath (Join-Path $evidence 'first-person-debug-events.log') -Raw
        $fault=[regex]::Match($events,'exception=0xC0000005 address=0x[0-9A-Fa-f]+ thread=(\d+)')
        if($fault.Success){foreach($dump in @(Get-ChildItem -LiteralPath $evidence -Filter '*.dmp' -File)){
            & $DumpAnalyzer $dump.FullName $fault.Groups[1].Value ($test+';'+$env:WINDIR+'\System32') 2>&1 | Set-Content -LiteralPath ($dump.FullName+'.stack.txt') -Encoding UTF8
        }}
    }
    } catch {Write-Warning "Postmortem stack analysis unavailable: $_"}
    foreach($name in $names){
        if($null -eq $prior[$name]){Remove-Item -LiteralPath ('Env:'+ $name) -ErrorAction SilentlyContinue}
        else{[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}
    }
    $runtime=Join-Path $test 'ncmm'
    if(Test-Path $runtime){Get-ChildItem $runtime -File | Where-Object {$_.Name -like 'first-person-*' -or $_.Extension -eq '.log'} | Copy-Item -Destination $evidence}
    foreach($name in @('debug.log','debug.log.prev','crash.log')){
        $nativeLog=Join-Path $test ('config/'+$name)
        if(Test-Path $nativeLog){Copy-Item $nativeLog (Join-Path $evidence ('first-person-cdda-'+$name))}
    }
}
try {
if($exitCode -ne 0){
    $reason='native result unavailable'
    $nativeResult=Join-Path $evidence 'first-person-smoke.json'
    if(Test-Path $nativeResult){
        $nativeText=Get-Content $nativeResult -Raw
        try {$reason=($nativeText | ConvertFrom-Json).reason}
        catch {$match=[regex]::Match($nativeText,'"reason"\s*:\s*"([^"\r\n]*)"');if($match.Success){$reason=$match.Groups[1].Value}}
    }
    throw "Native audit failed ($exitCode), phase=$lastProgress; bootstrap status=$reason. Evidence: $evidence"
}
if($debugger -and $debugger.Error){throw "Native diagnostic debugger failed: $($debugger.Error)"}
if($debugger -and $debugger.Dumps -gt 0){throw 'Native diagnostic captured an access violation even though the process exited successfully.'}
$auditName=if($MovementBenchmark){'first-person-movement-benchmark.json'}elseif($FocusedGpuBenchmark){'first-person-gpu-benchmark.json'}else{'first-person-gameplay-audit.json'}
if(-not (Test-Path -LiteralPath (Join-Path $evidence $auditName))){throw "Requested native benchmark report is missing: $auditName; final phase=$lastProgress"}
if($MovementBenchmark){
    $audit=Get-Content (Join-Path $evidence 'first-person-movement-benchmark.json') -Raw | ConvertFrom-Json
    if($audit.success -ne $true -or $audit.actual_cdda_sdl -ne $true -or $audit.simulation_turns_advanced -ne 0 -or @($audit.records).Count -ne 12){throw 'Native movement benchmark is incomplete.'}
    foreach($record in $audit.records){
        if($record.native_steps -ne 12 -or @($record.first_ready_samples_ms).Count -ne 12 -or @($record.easing_frame_samples_ms).Count -eq 0){throw 'Native steps or easing samples are incomplete.'}
        if(@($record.cold_rotation_samples_ms).Count -ne 48 -or @($record.warm_rotation_samples_ms).Count -ne 48){throw 'Continuous cold/warm rotation samples are incomplete.'}
        if($record.renderer -eq 'gpu' -and @($record.frame_renderers_gpu | Where-Object {$_ -ne 1}).Count){throw 'A walking GPU frame fell back to CPU.'}
    }
    foreach($record in $audit.records){
        if(@($record.first_frame_after_step_samples_ms).Count -ne 12){throw 'First rendered-frame samples are incomplete.'}
    }
    if($audit.facade_regression.normal_vision -ne $true -or $audit.facade_regression.connected_front_wall_cells -ne 18 -or $audit.facade_regression.slabs_connected -ne $true){throw 'Ordinary-vision facade continuity failed.'}
    $horizon=$audit.horizon_regression
    if($horizon.known_forest_patches -le 20 -or $horizon.unknown_neutral_patches -le 20 -or $horizon.near_overlap -ne $false -or $horizon.native_perception -ne $true -or $horizon.cpu_and_gpu -ne $true){throw 'Known/unknown horizon or detailed-world separation failed.'}
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
} catch {
    # Process exit and report qualification are separate. Retain a failed JSON
    # parse, incomplete matrix or missing report in the machine-readable receipt.
    $validationFailure=$_.Exception.Message
    $receiptPath=Join-Path $evidence 'first-person-process-result.json'
    $receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    $receipt.wrapper_failure=$validationFailure
    $receipt | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8
    throw
}
