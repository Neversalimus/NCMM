param(
    [string]$GameRoot = "",
    [string]$BuildRoot = "C:\NCMMBuild",
    [ValidateSet("Safe","Balanced","Maximum")]
    [string]$BuildProfile = "Balanced",
    [string]$TargetCommit = "e262adb299a7613b4aedc5f12c08fe0413c56a84",
    [string]$TargetTag = "cdda-experimental-2026-09-23-0546",
    [string]$TargetFolder = "cdda_experimental_2026_09_23_0546",
    [string]$TargetCacheKey = "cdda_0546",
    [string]$TargetVcpkgCommit = "f6672d8e480ccdecddfad3fd1b838ba369ffe6cd",
    [string]$TargetSupportMode = "exact",
    [switch]$HostSourceProbeOnly
)

$ErrorActionPreference = "Stop"

$CddaCommit = ([string]$TargetCommit).Trim().ToLowerInvariant()
$CddaTag = ([string]$TargetTag).Trim()
$CddaFolder = ([string]$TargetFolder).Trim()
$CddaCacheKey = ([regex]::Replace(([string]$TargetCacheKey).Trim(), '[^A-Za-z0-9_.-]', '_'))
$VcpkgCommit = ([string]$TargetVcpkgCommit).Trim().ToLowerInvariant()
if ($CddaCommit -notmatch '^[0-9a-f]{40}$') { throw "Invalid target CDDA commit: $TargetCommit" }
if ($VcpkgCommit -notmatch '^[0-9a-f]{40}$') { throw "Invalid target vcpkg baseline/commit: $TargetVcpkgCommit" }
if ([string]::IsNullOrWhiteSpace($CddaCacheKey)) { throw 'TargetCacheKey must not be empty.' }

function Resolve-BuildTuning([string]$Profile) {
    $logical = [Math]::Max(1,[Environment]::ProcessorCount)
    switch ($Profile) {
        "Safe" {
            $target = [Math]::Max(1,[int][Math]::Floor($logical * 0.35))
            $nodes = [Math]::Max(1,[Math]::Min(2,$target))
            $priority = "BelowNormal"
        }
        "Maximum" {
            $target = $logical
            $nodes = [Math]::Max(1,[Math]::Min(8,$target))
            $priority = "Normal"
        }
        default {
            $target = [Math]::Max(1,[int][Math]::Floor($logical * 0.55))
            $nodes = [Math]::Max(1,[Math]::Min(4,$target))
            $priority = "BelowNormal"
        }
    }
    $cl = [Math]::Max(1,[int][Math]::Floor($target / $nodes))
    $effective = [Math]::Max(1,$nodes * $cl)
    return [pscustomobject]@{
        Profile = $Profile
        LogicalThreads = $logical
        TargetThreads = $target
        MsBuildNodes = $nodes
        ClMpCount = $cl
        EffectiveCompileSlots = $effective
        VcpkgJobs = $target
        Priority = $priority
    }
}

function Start-TunedProcess(
    [string]$FilePath,
    [string]$ArgumentList,
    [string]$WorkingDirectory,
    [string]$Stdout,
    [string]$Stderr
) {
    $proc = Start-Process -FilePath $FilePath `
        -ArgumentList $ArgumentList `
        -WorkingDirectory $WorkingDirectory `
        -RedirectStandardOutput $Stdout `
        -RedirectStandardError $Stderr `
        -NoNewWindow -PassThru
    try {
        if ($script:BuildTuning.Priority -ne "Normal") {
            $proc.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
        }
    } catch {
        Write-Host "Could not lower build process priority; continuing with OS default." -ForegroundColor DarkYellow
    }

    # PowerShell 5.1 can occasionally expose an empty ExitCode through a returned
    # Process wrapper even though the native process has already completed.  Capture
    # the exit status inside this function after an explicit wait/refresh and return
    # a stable value object instead of leaking the live Process instance to callers.
    [void]$proc.WaitForExit()
    $proc.Refresh()
    $exitCode = $null
    try {
        if ($proc.HasExited) {
            $exitCode = [int]$proc.ExitCode
        }
    } catch {
        $exitCode = $null
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        ProcessId = $proc.Id
        HasExited = $proc.HasExited
    }
}

$script:BuildTuning = Resolve-BuildTuning $BuildProfile
# vcpkg and cl.exe both honor environment-level concurrency caps.  Keep the
# MSBuild property too, but CL_MPCount in the environment is the authoritative
# guard when a project emits bare /MP.
$env:VCPKG_MAX_CONCURRENCY = [string]$script:BuildTuning.VcpkgJobs
$env:CL_MPCount = [string]$script:BuildTuning.ClMpCount
Write-Host ("Build profile: {0} | logical {1} | target {2} | MSBuild nodes {3} | CL /MP {4} | priority {5}" -f `
    $script:BuildTuning.Profile,$script:BuildTuning.LogicalThreads,$script:BuildTuning.TargetThreads,`
    $script:BuildTuning.MsBuildNodes,$script:BuildTuning.ClMpCount,$script:BuildTuning.Priority) -ForegroundColor DarkCyan

function Write-Utf8NoBom([string]$Path,[string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    [IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))
}

function Hash-File([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Set-InfrastructureTransactionPhase([string]$Phase,[string]$Status='running',[string]$Details='') {
    $journal=[string]$env:NCMM_INFRA_JOURNAL_PATH
    if([string]::IsNullOrWhiteSpace($journal)){return}
    try {
        $state=[ordered]@{}
        if(Test-Path $journal -PathType Leaf){$old=Get-Content $journal -Raw|ConvertFrom-Json;foreach($p in $old.PSObject.Properties){$state[$p.Name]=$p.Value}}
        if(-not $state['schema']){$state['schema']=3};if(-not $state['infrastructure']){$state['infrastructure']='0.8.2'}
        $events=@();if($state['events']){$events=@($state['events'])}
        $evt=[ordered]@{phase=$Phase;status=$Status;utc=[DateTime]::UtcNow.ToString('o')};if(-not [string]::IsNullOrWhiteSpace($Details)){$evt['details']=$Details}
        $events+=[pscustomobject]$evt;$state['current_phase']=$Phase;$state['status']=$Status;$state['updated_utc']=[DateTime]::UtcNow.ToString('o');$state['events']=$events
        Write-Utf8NoBom $journal (([pscustomobject]$state|ConvertTo-Json -Depth 12)+"`n")
    } catch { Write-Host ('Infrastructure phase journal warning: '+$_.Exception.Message) -ForegroundColor DarkYellow }
}


function Hash-Text([string]$Text) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes([string]$Text)
        $hash = $sha.ComputeHash($bytes)
        return (($hash | ForEach-Object { $_.ToString("x2") }) -join "")
    } finally {
        $sha.Dispose()
    }
}

function Replace-Once([string]$Text,[string]$Old,[string]$New,[string]$Name) {
    $Text = Normalize-Lf $Text
    $Old = Normalize-Lf $Old
    $New = Normalize-Lf $New
    $count = ([regex]::Matches($Text,[regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "$Name expected once, found $count"
    }
    return $Text.Replace($Old,$New)
}

function Normalize-Lf([string]$Text) {
    if ($null -eq $Text) { return $Text }
    return $Text.Replace("`r`n","`n").Replace("`r","`n")
}

function Replace-TextBlock([string]$Text,[string]$Old,[string]$New,[string]$Name) {
    # Exact block replacement with newline normalization on BOTH the source and
    # PowerShell here-string anchors.  Windows PowerShell here-strings are CRLF
    # even when the downloaded C++ source is LF.
    $Text = Normalize-Lf $Text
    $Old = (Normalize-Lf $Old).TrimEnd()
    $New = (Normalize-Lf $New).TrimEnd()
    $count = ([regex]::Matches($Text,[regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "$Name expected exactly once, found $count"
    }
    return $Text.Replace($Old,$New)
}

function Replace-CppRange([string]$Text,[string]$Start,[string]$End,[string]$Replacement,[string]$Name) {
    # Source files downloaded from GitHub are LF, while Windows PowerShell here-strings
    # inherit CRLF from this installer.  Normalize both sides before contract matching so
    # multiline markers cannot fail only because of newline encoding.
    $Text = Normalize-Lf $Text
    $Start = Normalize-Lf $Start
    $End = Normalize-Lf $End
    $Replacement = Normalize-Lf $Replacement
    $a = $Text.IndexOf($Start)
    if ($a -lt 0) { throw "$Name start marker missing: $Start" }
    $b = $Text.IndexOf($End,$a)
    if ($b -le $a) { throw "$Name end marker missing: $End" }
    return $Text.Substring(0,$a) + $Replacement.TrimEnd() + "`n`n" + $Text.Substring($b)
}

# Regression guard for the v8.1.2 Windows CRLF/LF contract bug.
$eolProbeText = "BEGIN`nSTART`nEND_A`nEND_B`nTAIL"
$eolProbeEnd = "END_A`r`nEND_B"
$eolProbeResult = Replace-CppRange $eolProbeText "START" $eolProbeEnd "REPLACED" "installer EOL-normalization self-test"
if (-not $eolProbeResult.Contains("REPLACED") -or -not $eolProbeResult.Contains("END_A`nEND_B")) {
    throw "Installer EOL-normalization self-test failed."
}
$blockProbeText = "HEAD`nalpha`nbeta`nTAIL"
$blockProbeOld = "alpha`r`nbeta"
$blockProbeNew = "gamma`r`ndelta"
$blockProbeResult = Replace-TextBlock $blockProbeText $blockProbeOld $blockProbeNew "installer block EOL-normalization self-test"
if (-not $blockProbeResult.Contains("gamma`ndelta")) {
    throw "Installer block EOL-normalization self-test failed."
}

function Test-VsRoot([string]$Root) {
    if ([string]::IsNullOrWhiteSpace($Root) -or -not (Test-Path $Root -PathType Container)) {
        return $null
    }

    $msbuildCandidates = @(
        (Join-Path $Root "MSBuild\Current\Bin\MSBuild.exe"),
        (Join-Path $Root "MSBuild\Current\Bin\amd64\MSBuild.exe"),
        (Join-Path $Root "MSBuild\17.0\Bin\MSBuild.exe")
    )
    $msbuild = $msbuildCandidates |
        Where-Object { Test-Path $_ -PathType Leaf } |
        Select-Object -First 1

    $vcvars = Join-Path $Root "VC\Auxiliary\Build\vcvars64.bat"
    $toolsRoot = Join-Path $Root "VC\Tools\MSVC"
    $cl = $null

    if (Test-Path $toolsRoot -PathType Container) {
        $cl = Get-ChildItem $toolsRoot -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object {
                $candidate = Join-Path $_.FullName "bin\Hostx64\x64\cl.exe"
                if (Test-Path $candidate -PathType Leaf) {
                    $candidate
                }
            } |
            Select-Object -First 1
    }

    if ($msbuild -and (Test-Path $vcvars -PathType Leaf) -and $cl) {
        return [pscustomobject]@{
            Root = $Root
            MSBuild = [string]$msbuild
            VcVars = $vcvars
            CL = [string]$cl
        }
    }
    return $null
}

function Get-VsWhereCandidates {
    $result = New-Object System.Collections.Generic.List[string]

    $pf86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    $pf64 = [Environment]::GetEnvironmentVariable("ProgramFiles")

    foreach ($base in @($pf86,$pf64)) {
        if ([string]::IsNullOrWhiteSpace($base)) { continue }
        $candidate = Join-Path $base "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $candidate -PathType Leaf) {
            $result.Add($candidate)
        }
    }

    foreach ($drive in [IO.DriveInfo]::GetDrives()) {
        if (-not $drive.IsReady -or $drive.DriveType -ne [IO.DriveType]::Fixed) {
            continue
        }
        foreach ($relative in @(
            "Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe",
            "Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
        )) {
            $candidate = Join-Path $drive.RootDirectory.FullName $relative
            if (Test-Path $candidate -PathType Leaf) {
                $result.Add($candidate)
            }
        }
    }

    return @($result | Select-Object -Unique)
}

function Find-Vs {
    $diagnostics = New-Object System.Collections.Generic.List[string]
    $roots = New-Object System.Collections.Generic.List[string]

    # Developer Command Prompt / PATH fallback.  This catches portable/custom
    # installations even when vswhere/registry metadata is incomplete.
    $pathCl = Get-Command cl.exe -ErrorAction SilentlyContinue
    $pathMsbuild = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($pathCl) {
        $diagnostics.Add("PATH cl.exe: $($pathCl.Source)")
        $clPath = [IO.Path]::GetFullPath($pathCl.Source)
        $vcMarker = "\VC\Tools\MSVC\"
        $idx = $clPath.IndexOf($vcMarker, [StringComparison]::OrdinalIgnoreCase)
        if ($idx -gt 0) {
            $roots.Add($clPath.Substring(0,$idx))
        }
    }
    if ($pathMsbuild) {
        $diagnostics.Add("PATH MSBuild.exe: $($pathMsbuild.Source)")
        $msPath = [IO.Path]::GetFullPath($pathMsbuild.Source)
        $msMarker = "\MSBuild\"
        $idx = $msPath.IndexOf($msMarker, [StringComparison]::OrdinalIgnoreCase)
        if ($idx -gt 0) {
            $roots.Add($msPath.Substring(0,$idx))
        }
    }

    foreach ($vswhere in Get-VsWhereCandidates) {
        $diagnostics.Add("vswhere: $vswhere")
        try {
            $installations = @(
                & $vswhere -all -products * -prerelease `
                    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                    -property installationPath 2>$null
            )
            foreach ($installation in $installations) {
                if (-not [string]::IsNullOrWhiteSpace($installation)) {
                    $roots.Add($installation.Trim())
                }
            }

            $allInstallations = @(
                & $vswhere -all -products * -prerelease -property installationPath 2>$null
            )
            foreach ($installation in $allInstallations) {
                if (-not [string]::IsNullOrWhiteSpace($installation)) {
                    $normalized = $installation.Trim()
                    $diagnostics.Add("VS installation: $normalized")
                    $roots.Add($normalized)
                }
            }
        } catch {
            $diagnostics.Add("vswhere failed: $($_.Exception.Message)")
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:VSINSTALLDIR)) {
        $roots.Add($env:VSINSTALLDIR.TrimEnd('\'))
    }

    foreach ($registryPath in @(
        "HKLM:\SOFTWARE\Microsoft\VisualStudio\SxS\VS7",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7"
    )) {
        if (-not (Test-Path $registryPath)) { continue }
        try {
            $properties = Get-ItemProperty $registryPath
            foreach ($property in $properties.PSObject.Properties) {
                if ($property.Name -match '^(17|18)\.') {
                    $value = [string]$property.Value
                    if (-not [string]::IsNullOrWhiteSpace($value)) {
                        $roots.Add($value.TrimEnd('\'))
                    }
                }
            }
        } catch {
            $diagnostics.Add("Registry probe failed: $registryPath")
        }
    }

    foreach ($drive in [IO.DriveInfo]::GetDrives()) {
        if (-not $drive.IsReady -or $drive.DriveType -ne [IO.DriveType]::Fixed) {
            continue
        }

        # Generic Visual Studio major-version discovery.
        # Current installs may use folders such as "18\Community" rather than
        # the old "2022\Community" layout.
        foreach ($baseRelative in @(
            "Program Files\Microsoft Visual Studio",
            "Program Files (x86)\Microsoft Visual Studio",
            "Microsoft Visual Studio",
            "Visual Studio"
        )) {
            $base = Join-Path $drive.RootDirectory.FullName $baseRelative
            if (-not (Test-Path $base -PathType Container)) {
                continue
            }

            Get-ChildItem $base -Directory -ErrorAction SilentlyContinue |
                ForEach-Object {
                    $major = $_
                    foreach ($edition in @("Community","BuildTools","Professional","Enterprise")) {
                        $candidate = Join-Path $major.FullName $edition
                        if (Test-Path $candidate -PathType Container) {
                            $roots.Add($candidate)
                        }
                    }
                }
        }

        # Legacy/custom shorthand folders.
        foreach ($edition in @("Community","BuildTools","Professional","Enterprise")) {
            foreach ($relative in @(
                "VS2022\$edition",
                "VS18\$edition",
                "VS2026\$edition"
            )) {
                $candidate = Join-Path $drive.RootDirectory.FullName $relative
                if (Test-Path $candidate -PathType Container) {
                    $roots.Add($candidate)
                }
            }
        }
    }

    foreach ($root in @($roots | Select-Object -Unique)) {
        $diagnostics.Add("Probe root: $root")
        $found = Test-VsRoot $root
        if ($found) {
            Write-Host "Visual Studio C++ toolchain found:" -ForegroundColor Green
            Write-Host "  Root:    $($found.Root)"
            Write-Host "  MSBuild: $($found.MSBuild)"
            Write-Host "  cl.exe:  $($found.CL)"
            return $found
        }
    }

    $diagPath = Join-Path $BuildRoot "SURVIVOR_0910_VS_DETECTION.txt"
    $report = @()
    $report += "Survivor 0.9.10 Visual Studio detection"
    $report += "UTC: $([DateTime]::UtcNow.ToString('o'))"
    $report += ""
    $report += $diagnostics
    $report += ""
    $report += "Required: MSBuild + vcvars64.bat + x64 cl.exe."
    $report += "Expected component: Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    $report += "Workload: Desktop development with C++ / Microsoft.VisualStudio.Workload.NativeDesktop"
    $report += "or Build Tools workload Microsoft.VisualStudio.Workload.VCTools."
    Write-Utf8NoBom $diagPath (($report -join "`n") + "`n")

    $hasVsInstall = $diagnostics | Where-Object { $_ -like "VS installation:*" } | Select-Object -First 1
    if ($hasVsInstall) {
        throw "Visual Studio is installed, but a usable x64 C++ toolchain was not found. Detection report: $diagPath`nInstall the Visual Studio C++ x64 workload if cl.exe/vcvars64.bat are missing, then rerun."
    }
    throw "No usable Visual Studio installation was detected. Detection report: $diagPath`nInstall Visual Studio/Build Tools with the C++ x64 workload, then rerun."
}

function Normalize-Path([string]$Path) {
    if (-not $Path) { return "" }
    try { return ([IO.Path]::GetFullPath($Path)).TrimEnd('\').ToLowerInvariant() }
    catch { return $Path.TrimEnd('\').ToLowerInvariant() }
}

function Stop-TargetGameProcesses([string]$Root) {
    $rootNorm = Normalize-Path $Root
    Get-Process -ErrorAction SilentlyContinue |
        Where-Object {
            $_.ProcessName -like "cataclysm-tiles*" -or
            $_.ProcessName -like "cataclysm*"
        } |
        ForEach-Object {
            $proc = $_
            $path = ""
            try { $path = $proc.Path } catch {}
            if (-not $path) {
                try {
                    $cim = Get-CimInstance Win32_Process -Filter "ProcessId=$($proc.Id)" -ErrorAction SilentlyContinue
                    if ($cim) { $path = [string]$cim.ExecutablePath }
                } catch {}
            }
            if ($path -and (Normalize-Path $path).StartsWith($rootNorm + "\")) {
                Write-Host "Stopping Cataclysm PID $($proc.Id): $path" -ForegroundColor Yellow
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            }
        }
    Start-Sleep -Milliseconds 700
}

function Wait-Unlocked([string]$Path,[int]$Seconds=20) {
    if (-not (Test-Path $Path -PathType Leaf)) { return }
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        try {
            $s = [IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
            $s.Close()
            return
        } catch {
            Start-Sleep -Milliseconds 500
        }
    }
    throw "File is still locked: $Path"
}

function Compile-Survivor([string]$SourceRoot,[string]$SdkRoot,[object]$Vs,[string]$ReleaseRoot) {
    $out = Join-Path $ReleaseRoot "0.12.0"
    $src = Join-Path $SourceRoot "src\survivor_progression.cpp"
    $manifest = Join-Path $SourceRoot "mod.json"
    $sdkHeader = Join-Path $SdkRoot "ncmm_api.h"
    $dll = Join-Path $out "ncmm_mod.dll"
    $obj = Join-Path $out "survivor_progression.obj"
    $cmd = Join-Path $out "build.cmd"
    $cacheMarker = Join-Path $ReleaseRoot ".survivor_0120_manager_settings_build.sha256"

    foreach ($p in @($src,$manifest,$sdkHeader,$Vs.CL)) {
        if (-not (Test-Path $p -PathType Leaf)) {
            throw "Survivor compile input missing: $p"
        }
    }

    $compilerIdentity = [string](Get-Item $Vs.CL).VersionInfo.FileVersion
    $compileRecipe = '/nologo /std:c++17 /EHsc /O2 /MT /LD + SDK include + survivor source'
    $sourceManifestSha = Hash-File $manifest
    $fingerprint = Hash-Text ((@(
        "v8.7.6.8-survivor-0.12.0-manager-settings",
        (Hash-File $src),
        $sourceManifestSha,
        (Hash-File $sdkHeader),
        $compileRecipe,
        $compilerIdentity,
        [string]$Vs.CL
    )) -join "|")

    $cacheLines = @()
    if (Test-Path $cacheMarker -PathType Leaf) { $cacheLines = @(Get-Content $cacheMarker) }
    $cachedManifest = Join-Path $out "mod.json"
    if ($cacheLines.Count -ge 3 -and
        (Test-Path $dll -PathType Leaf) -and
        (Test-Path $cachedManifest -PathType Leaf) -and
        $cacheLines[0].Trim() -eq $fingerprint -and
        $cacheLines[1].Trim() -eq (Hash-File $dll) -and
        $cacheLines[2].Trim() -eq $sourceManifestSha -and
        (Hash-File $cachedManifest) -eq $sourceManifestSha) {
        Write-Host "Survivor 0.12.0 module build cache: HIT (DLL + manifest verified)" -ForegroundColor Green
        return $out
    }

    Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $out | Out-Null

    $cmdText = @"
@echo off
call "$($Vs.VcVars)" >nul
if errorlevel 1 exit /b 101
cl.exe /nologo /std:c++17 /EHsc /O2 /MT /LD /I"$SdkRoot" /Fo"$obj" "$src" /link /OUT:"$dll"
exit /b %ERRORLEVEL%
"@
    [IO.File]::WriteAllText($cmd,$cmdText,[Text.Encoding]::ASCII)

    Write-Host "Compiling Survivor 0.12.0..." -ForegroundColor Cyan
    $compilerOutput = @(& cmd.exe /d /c "`"$cmd`"" 2>&1)
    $compilerCode = $LASTEXITCODE
    foreach ($line in $compilerOutput) {
        Write-Host ([string]$line)
    }
    if ($compilerCode -ne 0 -or -not (Test-Path $dll -PathType Leaf)) {
        throw "Survivor 0.12.0 compile failed with exit code $compilerCode"
    }

    Copy-Item $manifest (Join-Path $out "mod.json") -Force
    foreach($about in Get-ChildItem $SourceRoot -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
        Copy-Item $about.FullName (Join-Path $out $about.Name) -Force
    }
    Remove-Item $obj,$cmd -Force -ErrorAction SilentlyContinue
    $dllSha = Hash-File $dll
    Write-Utf8NoBom $cacheMarker ($fingerprint + "`n" + $dllSha + "`n" + $sourceManifestSha + "`n")

    $zip = Join-Path $ReleaseRoot "SurvivorProgression_0.12.0_LOCAL.zip"
    Remove-Item $zip -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip -Force
    return $out
}


function Compile-AdvancedWorldSettings([string]$RepoRoot,[string]$SdkRoot,[object]$Vs,[string]$ReleaseRoot) {
    $sourceRoot = Join-Path $RepoRoot "mods\AdvancedWorldSettings"
    $src = Join-Path $sourceRoot "src\aws.cpp"
    $manifest = Join-Path $sourceRoot "mod.json"
    $sdkHeader = Join-Path $SdkRoot "ncmm_api.h"
    $out = Join-Path $ReleaseRoot "AdvancedWorldSettings_0.6.2"
    $dll = Join-Path $out "ncmm_mod.dll"
    $obj = Join-Path $out "advanced_world_settings.obj"
    $cmdFile = Join-Path $out "build.cmd"
    $cacheMarker = Join-Path $ReleaseRoot ".aws_061_wsapi2_ui17_build.sha256"

    foreach ($p in @($src,$manifest,$sdkHeader,$Vs.CL)) {
        if (-not (Test-Path $p -PathType Leaf)) {
            throw "Advanced World Settings compile input missing: $p"
        }
    }

    $compilerIdentity = [string](Get-Item $Vs.CL).VersionInfo.FileVersion
    $compileRecipe = '/nologo /std:c++17 /EHsc /O2 /MT /LD + SDK include + AWS 0.6.2 Host API2 source'
    $manifestSha = Hash-File $manifest
    $fingerprint = Hash-Text ((@(
        "aws-0.6.2-host-api2-worldgen-bindings",
        (Hash-File $src),
        $manifestSha,
        (Hash-File $sdkHeader),
        $compileRecipe,
        $compilerIdentity,
        [string]$Vs.CL
    )) -join "|")

    $cacheLines = @()
    if (Test-Path $cacheMarker -PathType Leaf) { $cacheLines = @(Get-Content $cacheMarker) }
    $cachedManifest = Join-Path $out "mod.json"
    if ($cacheLines.Count -ge 3 -and
        (Test-Path $dll -PathType Leaf) -and
        (Test-Path $cachedManifest -PathType Leaf) -and
        $cacheLines[0].Trim() -eq $fingerprint -and
        $cacheLines[1].Trim() -eq (Hash-File $dll) -and
        $cacheLines[2].Trim() -eq $manifestSha -and
        (Hash-File $cachedManifest) -eq $manifestSha) {
        Write-Host "Advanced World Settings 0.6.2 build cache: HIT" -ForegroundColor Green
        return $out
    }

    Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $out | Out-Null
    $cmdText = @"
@echo off
call "$($Vs.VcVars)" >nul
if errorlevel 1 exit /b 101
cl.exe /nologo /std:c++17 /EHsc /O2 /MT /LD /I"$SdkRoot" /Fo"$obj" "$src" /link /OUT:"$dll"
exit /b %ERRORLEVEL%
"@
    [IO.File]::WriteAllText($cmdFile,$cmdText,[Text.Encoding]::ASCII)

    Write-Host "Compiling Advanced World Settings 0.6.2 / Host API 2.0 World Settings..." -ForegroundColor Cyan
    $compilerOutput = @(& cmd.exe /d /c "`"$cmdFile`"" 2>&1)
    $compilerCode = $LASTEXITCODE
    foreach ($line in $compilerOutput) { Write-Host ([string]$line) }
    if ($compilerCode -ne 0 -or -not (Test-Path $dll -PathType Leaf)) {
        throw "Advanced World Settings 0.6.2 compile failed with exit code $compilerCode"
    }

    Copy-Item $manifest (Join-Path $out "mod.json") -Force
    foreach($about in Get-ChildItem $sourceRoot -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
        Copy-Item $about.FullName (Join-Path $out $about.Name) -Force
    }
    Remove-Item $obj,$cmdFile -Force -ErrorAction SilentlyContinue
    $dllSha = Hash-File $dll
    Write-Utf8NoBom $cacheMarker ($fingerprint + "`n" + $dllSha + "`n" + $manifestSha + "`n")
    return $out
}


function Download-Archive([string]$Url,[string]$OutFile) {
    $parent = Split-Path -Parent $OutFile
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    # PowerShell 5.1 parsing guard: never mix Test-Path cmdlet arguments and
    # boolean operators in the same unparenthesized command expression.
    if ([IO.File]::Exists($OutFile)) {
        $existingLength = (Get-Item -LiteralPath $OutFile).Length
        if ($existingLength -gt 10000) {
            Write-Host "Download cache hit: $OutFile" -ForegroundColor DarkGray
            return
        }

        # A previous interrupted download should not poison the next run.
        Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Download: $Url"
    try {
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $OutFile -ErrorAction Stop
    } catch {
        Write-Host "Invoke-WebRequest failed, trying WebClient..." -ForegroundColor Yellow
        Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
        $client = New-Object Net.WebClient
        $client.Headers.Add("User-Agent","NCMM-Survivor-0910")
        $client.DownloadFile($Url,$OutFile)
    }

    if (-not [IO.File]::Exists($OutFile)) {
        throw "Download did not create the expected file: $Url"
    }

    $downloadedLength = (Get-Item -LiteralPath $OutFile).Length
    if ($downloadedLength -lt 10000) {
        Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
        throw "Downloaded file is suspiciously small ($downloadedLength bytes): $Url"
    }
}

function Expand-SingleRootZip([string]$Zip,[string]$Destination,[string]$ExpectedMarker) {
    $staging = $Destination + "_extract"
    Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $staging | Out-Null
    Expand-Archive -LiteralPath $Zip -DestinationPath $staging -Force

    $dirs = @(Get-ChildItem -LiteralPath $staging -Directory)
    if ($dirs.Count -ne 1) {
        throw "Archive layout unexpected: $Zip"
    }
    $source = $dirs[0].FullName
    if (-not (Test-Path -LiteralPath (Join-Path $source $ExpectedMarker) -PathType Leaf)) {
        throw "Archive marker missing after extraction: $ExpectedMarker"
    }

    Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
    Move-Item -LiteralPath $source -Destination $Destination
    Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
}

function Test-PristineCddaCache([string]$Root,[string]$Commit,[string]$VcpkgBaseline) {
    $marker = Join-Path $Root ".ncmm_pristine_source_sha"
    $solution = Join-Path $Root "msvc-full-features\Cataclysm-vcpkg-static.sln"
    $manifest = Join-Path $Root "msvc-full-features\vcpkg.json"
    $optionsCpp = Join-Path $Root "src\options.cpp"
    if (-not (Test-Path $marker -PathType Leaf) -or
        -not (Test-Path $solution -PathType Leaf) -or
        -not (Test-Path $manifest -PathType Leaf) -or
        -not (Test-Path $optionsCpp -PathType Leaf)) {
        return $false
    }
    if ((Get-Content $marker -Raw).Trim() -ne $Commit) { return $false }
    try {
        $meta = Get-Content $manifest -Raw | ConvertFrom-Json
        if ([string]$meta.'builtin-baseline' -ne $VcpkgBaseline) { return $false }
    } catch { return $false }

    # A pristine cache is immutable. Any NCMM marker/helper invalidates it.
    foreach ($patchMarker in @(
        '.ncmm_host_v1_patched',
        '.ncmm_world_settings_v2_patched',
        '.ncmm_survivor_modstats_v8',
        '.ncmm_survivor_modstats_v82',
        '.ncmm_survivor_modstats_v83',
        '.ncmm_runtime_gameplay_hooks_v2',
        '.ncmm_reactive_mechanics_0112',
        '.ncmm_recipe_finalize_profiler_v1',
        '.ncmm_runtime_infra_v8766'
    )) {
        if (Test-Path (Join-Path $Root $patchMarker) -PathType Leaf) { return $false }
    }
    if (Test-Path (Join-Path $Root 'src\ncmm_loader.cpp') -PathType Leaf) { return $false }
    foreach ($probe in @(
        @{ Path = 'src\magic.cpp'; Needle = 'ncmm_spell_source_of' },
        @{ Path = 'src\character_knowledge.cpp'; Needle = 'ncmm_survivor_unique_skill_bonus' },
        @{ Path = 'src\magic.cpp'; Needle = 'ncmm_spell_hook_id' },
        @{ Path = 'src\character_knowledge.cpp'; Needle = 'ncmm_runtime_skill_bonus' },
        @{ Path = 'src\creature.cpp'; Needle = 'combat.damage_to_species_pct' },
        @{ Path = 'src\creature.cpp'; Needle = 'combat.damage_taken_pct' },
        @{ Path = 'src\character_health.cpp'; Needle = 'combat.damage_avoid_pct' },
        @{ Path = 'src\monster.cpp'; Needle = 'combat.on_kill_moves' },
        @{ Path = 'src\crafting.cpp'; Needle = 'crafting.failure_save_pct' },
        @{ Path = 'src\character.cpp'; Needle = 'combat.dodge_attempts_bonus' },
        @{ Path = 'src\melee.cpp'; Needle = 'combat.melee_crit_chance_pct' },
        @{ Path = 'src\ranged.cpp'; Needle = 'combat.ranged_crit_damage_pct' },
        @{ Path = 'src\options.h'; Needle = 'ncmm_register_world_bool' },
        @{ Path = 'src\worldfactory.cpp'; Needle = 'ncmm_experimental' },
        @{ Path = 'src\options.h'; Needle = 'ncmm_get_option_bool_or' },
        @{ Path = 'src\recipe_dictionary.cpp'; Needle = 'ncmm_recipe_profiler_requested' }
    )) {
        $probePath = Join-Path $Root $probe.Path
        if ((Test-Path $probePath -PathType Leaf) -and
            ([IO.File]::ReadAllText($probePath).Contains([string]$probe.Needle))) {
            return $false
        }
    }
    return $true
}

function Invoke-RobocopyTree([string]$Source,[string]$Destination) {
    $robocopy = Join-Path $env:SystemRoot "System32\robocopy.exe"
    if (-not (Test-Path $robocopy -PathType Leaf)) {
        throw "Windows robocopy.exe is required for timestamp-preserving source-cache sync."
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    & $robocopy $Source $Destination /E /COPY:DAT /DCOPY:DAT /R:2 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    $code = $LASTEXITCODE
    # Robocopy uses 0..7 for successful/no-op/copy-difference outcomes.
    if ($code -ge 8) {
        throw "robocopy source-cache sync failed with exit code ${code}: $Source -> $Destination"
    }
}

function Ensure-CddaBuildCache([string]$Root,[string]$BuildRoot,[string]$Commit,[string]$VcpkgBaseline,[string]$CacheKey) {
    # NCMM Infrastructure 0.8.0: cache identity is adapter/commit scoped.
    # Each supported or structurally-reused experimental receives its own immutable
    # pristine tree and incremental patched worktree, avoiding cross-build contamination.
    $pristine = Join-Path $BuildRoot ($CacheKey + "_pristine")
    $downloads = Join-Path $BuildRoot "downloads"
    New-Item -ItemType Directory -Force -Path $downloads | Out-Null
    $zip = Join-Path $downloads ("cdda_" + $Commit + ".zip")

    if (-not (Test-PristineCddaCache $pristine $Commit $VcpkgBaseline)) {
        Write-Host "" 
        Write-Host "Preparing immutable exact CDDA 0546 source cache..." -ForegroundColor Cyan
        Download-Archive ("https://github.com/CleverRaven/Cataclysm-DDA/archive/" + $Commit + ".zip") $zip
        Expand-SingleRootZip $zip $pristine "msvc-full-features\Cataclysm-vcpkg-static.sln"
        $manifest = Join-Path $pristine "msvc-full-features\vcpkg.json"
        $meta = Get-Content $manifest -Raw | ConvertFrom-Json
        if ([string]$meta.'builtin-baseline' -ne $VcpkgBaseline) {
            throw "CDDA pristine-source vcpkg baseline mismatch. Expected $VcpkgBaseline, got $($meta.'builtin-baseline')."
        }
        Write-Utf8NoBom (Join-Path $pristine ".ncmm_pristine_source_sha") ($Commit + "`n")
        if (-not (Test-PristineCddaCache $pristine $Commit $VcpkgBaseline)) {
            throw "Fresh exact CDDA pristine cache failed contamination/integrity validation."
        }
        Write-Host "Immutable CDDA source cache: READY" -ForegroundColor Green
    } else {
        Write-Host "Immutable CDDA source cache: OK" -ForegroundColor Green
    }

    $workMarker = Join-Path $Root ".ncmm_working_source_sha"
    $legacyMarker = Join-Path $Root ".ncmm_pristine_source_sha"
    $solution = Join-Path $Root "msvc-full-features\Cataclysm-vcpkg-static.sln"
    $manifest = Join-Path $Root "msvc-full-features\vcpkg.json"
    $workMarkerValid = $false
    foreach ($candidateMarker in @($workMarker,$legacyMarker)) {
        if ((Test-Path $candidateMarker -PathType Leaf) -and
            ((Get-Content $candidateMarker -Raw).Trim() -eq $Commit)) {
            $workMarkerValid = $true
            break
        }
    }
    $workReusable = (Test-Path $Root -PathType Container) -and
                    $workMarkerValid -and
                    (Test-Path $solution -PathType Leaf) -and
                    (Test-Path $manifest -PathType Leaf)

    if (-not $workReusable) {
        Write-Host "Creating incremental NCMM CDDA working tree..." -ForegroundColor Cyan
        Remove-Item $Root -Recurse -Force -ErrorAction SilentlyContinue
        Invoke-RobocopyTree $pristine $Root
        Remove-Item (Join-Path $Root ".ncmm_pristine_source_sha") -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "Resetting NCMM-touched CDDA source while preserving build/vcpkg caches..." -ForegroundColor Cyan
        $workSrc = Join-Path $Root "src"
        Remove-Item $workSrc -Recurse -Force -ErrorAction SilentlyContinue
        Invoke-RobocopyTree (Join-Path $pristine "src") $workSrc

        # Build metadata is normally restored transactionally, but refresh the
        # small files that our build path may temporarily touch. Do not delete
        # msvc-full-features: it contains expensive incremental/vcpkg caches.
        foreach ($relative in @(
            'msvc-full-features\Cataclysm-common.props',
            'msvc-full-features\Cataclysm-vcpkg-static.sln',
            'msvc-full-features\vcpkg.json',
            'msvc-full-features\vcpkg-configuration.json'
        )) {
            $sourceFile = Join-Path $pristine $relative
            if (Test-Path $sourceFile -PathType Leaf) {
                Copy-Item -LiteralPath $sourceFile -Destination (Join-Path $Root $relative) -Force
            }
        }
    }

    foreach ($patchMarker in @(
        '.ncmm_host_v1_patched',
        '.ncmm_world_settings_v2_patched',
        '.ncmm_survivor_modstats_v8',
        '.ncmm_survivor_modstats_v82',
        '.ncmm_survivor_modstats_v83',
        '.ncmm_runtime_gameplay_hooks_v2',
        '.ncmm_reactive_mechanics_0112',
        '.ncmm_recipe_finalize_profiler_v1',
        '.ncmm_runtime_infra_v8766',
        '.ncmm_pristine_source_sha'
    )) {
        Remove-Item (Join-Path $Root $patchMarker) -Force -ErrorAction SilentlyContinue
    }
    Write-Utf8NoBom $workMarker ($Commit + "`n")

    # Final guard: a reset working tree must not contain NCMM C++ signatures.
    foreach ($probe in @(
        @{ Path = 'src\ncmm_loader.cpp'; Needle = '' },
        @{ Path = 'src\magic.cpp'; Needle = 'ncmm_spell_source_of' },
        @{ Path = 'src\character_knowledge.cpp'; Needle = 'ncmm_survivor_unique_skill_bonus' },
        @{ Path = 'src\magic.cpp'; Needle = 'ncmm_spell_hook_id' },
        @{ Path = 'src\character_knowledge.cpp'; Needle = 'ncmm_runtime_skill_bonus' },
        @{ Path = 'src\creature.cpp'; Needle = 'combat.damage_to_species_pct' },
        @{ Path = 'src\monster.cpp'; Needle = 'combat.on_kill_moves' },
        @{ Path = 'src\crafting.cpp'; Needle = 'crafting.failure_save_pct' },
        @{ Path = 'src\options.h'; Needle = 'ncmm_register_world_bool' },
        @{ Path = 'src\worldfactory.cpp'; Needle = 'ncmm_experimental' },
        @{ Path = 'src\options.h'; Needle = 'ncmm_get_option_bool_or' },
        @{ Path = 'src\recipe_dictionary.cpp'; Needle = 'ncmm_recipe_profiler_requested' }
    )) {
        $probePath = Join-Path $Root $probe.Path
        if ($probe.Path -eq 'src\ncmm_loader.cpp') {
            if (Test-Path $probePath -PathType Leaf) {
                throw "Working-source reset failed: NCMM loader survived pristine restore."
            }
        } elseif ((Test-Path $probePath -PathType Leaf) -and
                  ([IO.File]::ReadAllText($probePath).Contains([string]$probe.Needle))) {
            throw "Working-source reset failed; stale NCMM signature survived: $($probe.Needle)"
        }
    }
    Write-Host "Exact CDDA working source: CLEAN; incremental build caches preserved" -ForegroundColor Green
}

function Find-BootstrapGit([string]$VsRoot) {
    $candidates = New-Object System.Collections.Generic.List[string]

    $cmd = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        $candidates.Add($cmd.Source)
    }

    foreach ($path in @(
        "C:\Program Files\Git\cmd\git.exe",
        "C:\Program Files\Git\bin\git.exe",
        (Join-Path $env:LOCALAPPDATA "Programs\Git\cmd\git.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Git\bin\git.exe")
    )) {
        if ($path -and (Test-Path $path -PathType Leaf)) {
            $candidates.Add($path)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($VsRoot)) {
        foreach ($relative in @(
            "Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe",
            "Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\mingw64\bin\git.exe",
            "Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\usr\bin\git.exe"
        )) {
            $path = Join-Path $VsRoot $relative
            if (Test-Path $path -PathType Leaf) {
                $candidates.Add($path)
            }
        }
    }

    $found = @($candidates | Select-Object -Unique) | Select-Object -First 1
    if ($found) {
        return $found
    }

    $winget = Get-Command winget.exe -ErrorAction SilentlyContinue
    if (-not $winget) {
        throw "Git is required only to bootstrap the pinned vcpkg repository, and no bundled Git or winget.exe was found."
    }

    Write-Host ""
    Write-Host "Pinned vcpkg requires a real Git object database." -ForegroundColor Yellow
    Write-Host "Installing Git for Windows automatically via winget..." -ForegroundColor Cyan

    $wingetArgs = @(
        "install",
        "--id","Git.Git",
        "--exact",
        "--source","winget",
        "--accept-package-agreements",
        "--accept-source-agreements",
        "--silent"
    )
    $proc = Start-Process -FilePath $winget.Source -ArgumentList ($wingetArgs -join " ") `
        -Verb RunAs -PassThru -Wait
    if ($proc.ExitCode -notin @(0,3010,1641)) {
        throw "Git for Windows installation failed with exit code $($proc.ExitCode)."
    }

    foreach ($path in @(
        "C:\Program Files\Git\cmd\git.exe",
        "C:\Program Files\Git\bin\git.exe",
        (Join-Path $env:LOCALAPPDATA "Programs\Git\cmd\git.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\Git\bin\git.exe")
    )) {
        if ($path -and (Test-Path $path -PathType Leaf)) {
            return $path
        }
    }

    throw "Git installation completed, but git.exe was not found. Open a new terminal and rerun this script."
}


function Ensure-VcpkgFullHistory([string]$Root,[string]$GitExe) {
    $gitDir = Join-Path $Root ".git"
    if (-not (Test-Path $gitDir -PathType Container)) {
        return
    }

    $shallow = Join-Path $gitDir "shallow"
    if (Test-Path $shallow -PathType Leaf) {
        Write-Host ""
        Write-Host "Repairing shallow vcpkg clone -> full history..." -ForegroundColor Yellow

        & $GitExe -C $Root fetch --unshallow origin
        if ($LASTEXITCODE -ne 0) {
            throw "Could not unshallow vcpkg repository."
        }

        if (Test-Path $shallow -PathType Leaf) {
            throw "vcpkg repository is still shallow after fetch --unshallow."
        }

        Write-Host "vcpkg Git history: FULL" -ForegroundColor Green
    } else {
        Write-Host "vcpkg Git history: FULL" -ForegroundColor Green
    }
}

function Ensure-VcpkgCache([string]$Root,[string]$BuildRoot,[string]$Commit,[string]$GitExe) {
    $marker = Join-Path $Root ".ncmm_vcpkg_sha"
    $exe = Join-Path $Root "vcpkg.exe"
    $gitHead = Join-Path $Root ".git\HEAD"

    # v12 created a depth-1 repository. Repair it in-place so the expensive
    # bootstrap/download work is reused instead of deleting C:\NCMMBuild\vcpkg.
    if (Test-Path $gitHead -PathType Leaf) {
        Ensure-VcpkgFullHistory $Root $GitExe
    }
    $downloads = Join-Path $BuildRoot "vcpkg_downloads"
    New-Item -ItemType Directory -Force -Path $downloads | Out-Null

    $valid = $false
    if ((Test-Path $marker -PathType Leaf) -and
        (Test-Path $exe -PathType Leaf) -and
        (Test-Path $gitHead -PathType Leaf)) {
        $stored = (Get-Content $marker -Raw).Trim()
        if ($stored -eq $Commit) {
            $oldPreference = $ErrorActionPreference
            try {
                $ErrorActionPreference = "Continue"
                $headLines = @(& $GitExe -C $Root rev-parse HEAD 2>$null)
                $code = $LASTEXITCODE
            } finally {
                $ErrorActionPreference = $oldPreference
            }
            $head = ($headLines -join "`n").Trim()
            if ($code -eq 0 -and
                [String]::Equals($head,$Commit,[StringComparison]::OrdinalIgnoreCase)) {
                $valid = $true
            }
        }
    }

    if (-not $valid) {
        Write-Host ""
        Write-Host "Preparing pinned vcpkg Git cache..." -ForegroundColor Cyan
        Remove-Item $Root -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $Root | Out-Null

        & $GitExe -C $Root init
        if ($LASTEXITCODE -ne 0) { throw "git init failed for vcpkg." }

        & $GitExe -C $Root config core.longpaths true
        if ($LASTEXITCODE -ne 0) { throw "git config core.longpaths failed." }

        & $GitExe -C $Root remote add origin "https://github.com/microsoft/vcpkg.git"
        if ($LASTEXITCODE -ne 0) { throw "git remote add failed for vcpkg." }

        # v13: vcpkg versioning cannot work from a shallow clone because
        # versions/baseline.json may reference historical port git trees.
        # Fetch the normal remote refs with full history.
        & $GitExe -C $Root fetch origin
        if ($LASTEXITCODE -ne 0) {
            throw "Could not fetch full vcpkg history."
        }

        & $GitExe -C $Root checkout --detach $Commit
        if ($LASTEXITCODE -ne 0) { throw "Could not checkout pinned vcpkg commit." }

        $head = ((@(& $GitExe -C $Root rev-parse HEAD)) -join "`n").Trim()
        if (-not [String]::Equals($head,$Commit,[StringComparison]::OrdinalIgnoreCase)) {
            throw "Pinned vcpkg checkout mismatch: $head"
        }

        & cmd.exe /d /c "`"$Root\bootstrap-vcpkg.bat`" -disableMetrics"
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe -PathType Leaf)) {
            throw "vcpkg bootstrap failed."
        }
        Write-Utf8NoBom $marker ($Commit + "`n")
    }

    Ensure-VcpkgFullHistory $Root $GitExe

    $resolvedHead = ((@(& $GitExe -C $Root rev-parse HEAD 2>$null)) -join "`n").Trim()
    if ($LASTEXITCODE -ne 0 -or
        -not [String]::Equals($resolvedHead,$Commit,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Pinned vcpkg HEAD mismatch after history repair: $resolvedHead"
    }

    $baselineProbe = @(& $GitExe -C $Root show "$Commit`:versions/baseline.json" 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace(($baselineProbe -join "`n"))) {
        throw "Pinned vcpkg Git baseline probe failed."
    }

    $env:VCPKG_DOWNLOADS = $downloads
    Write-Host "vcpkg baseline probe: OK" -ForegroundColor Green

    & $exe integrate install
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg integrate install failed."
    }
}

function Ensure-BuildFreeSpace([string]$Root,[double]$MinimumGb) {
    $driveRoot = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Root))
    $drive = New-Object IO.DriveInfo($driveRoot)
    $freeGb = [math]::Round($drive.AvailableFreeSpace / 1GB,1)
    Write-Host "Build drive free space: $freeGb GB"
    if ($freeGb -lt $MinimumGb) {
        throw "At least $MinimumGb GB free is required for the first CDDA/vcpkg build cache."
    }
}


function Ensure-CddaVcpkgDependencies([string]$CddaRoot,[string]$VcpkgRoot,[string]$BuildRoot) {
    $vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
    $manifestRoot = Join-Path $CddaRoot "msvc-full-features"
    $manifest = Join-Path $manifestRoot "vcpkg.json"
    $config = Join-Path $manifestRoot "vcpkg-configuration.json"
    $installRoot = Join-Path $manifestRoot "vcpkg_installed\x64-windows-static"
    $tripletRoot = Join-Path $installRoot "x64-windows-static"
    $overlay = Join-Path $CddaRoot ".github\vcpkg_triplets"
    $downloads = Join-Path $BuildRoot "vcpkg_downloads"
    $logs = Join-Path $BuildRoot "logs"
    $cacheMarker = Join-Path $manifestRoot ".ncmm_vcpkg_dependencies_v82.sha256"

    foreach ($p in @($vcpkg,$manifest)) {
        if (-not (Test-Path $p -PathType Leaf)) {
            throw "vcpkg dependency setup file missing: $p"
        }
    }
    if (-not (Test-Path $overlay -PathType Container)) {
        throw "CDDA vcpkg overlay triplets missing: $overlay"
    }

    New-Item -ItemType Directory -Force $installRoot,$downloads,$logs | Out-Null
    $env:VCPKG_DOWNLOADS = $downloads

    $fingerprintParts = New-Object System.Collections.Generic.List[string]
    $fingerprintParts.Add("v8.2")
    $fingerprintParts.Add($VcpkgCommit)
    $fingerprintParts.Add((Hash-File $manifest))
    if (Test-Path $config -PathType Leaf) {
        $fingerprintParts.Add((Hash-File $config))
    }
    foreach ($f in @(Get-ChildItem $overlay -File -Recurse -ErrorAction SilentlyContinue | Sort-Object FullName)) {
        $fingerprintParts.Add($f.FullName.Substring($overlay.Length))
        $fingerprintParts.Add((Hash-File $f.FullName))
    }
    $fingerprint = Hash-Text (($fingerprintParts.ToArray()) -join "|")

    if ((Test-Path $cacheMarker -PathType Leaf) -and
        (Test-Path $tripletRoot -PathType Container) -and
        (Test-Path (Join-Path $tripletRoot "include") -PathType Container) -and
        (Test-Path (Join-Path $tripletRoot "lib") -PathType Container) -and
        ((Get-Content $cacheMarker -Raw).Trim() -eq $fingerprint)) {
        Write-Host "CDDA vcpkg dependencies: READY (fingerprint cache hit)" -ForegroundColor Green
        return
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $stdout = Join-Path $logs ("vcpkg_install_" + $stamp + ".out.log")
    $stderr = Join-Path $logs ("vcpkg_install_" + $stamp + ".err.log")

    Write-Host ""
    Write-Host "Ensuring CDDA vcpkg manifest dependencies..." -ForegroundColor Cyan
    Write-Host "  manifest: $manifest"
    Write-Host "  triplet:  x64-windows-static"
    Write-Host "  install:  $installRoot"

    $argLine = @(
        "install",
        "--triplet", "x64-windows-static",
        "--host-triplet", "x64-windows-static",
        "--x-manifest-root=`"$manifestRoot`"",
        "--x-install-root=`"$installRoot`"",
        "--overlay-triplets=`"$overlay`"",
        "--clean-after-build"
    ) -join " "

    $proc = Start-TunedProcess -FilePath $vcpkg `
        -ArgumentList $argLine `
        -WorkingDirectory $manifestRoot `
        -Stdout $stdout `
        -Stderr $stderr

    $outText = ""
    $errText = ""
    if (Test-Path $stdout) { $outText = Get-Content $stdout -Raw -ErrorAction SilentlyContinue }
    if (Test-Path $stderr) { $errText = Get-Content $stderr -Raw -ErrorAction SilentlyContinue }

    if ($outText) { Write-Host $outText.TrimEnd() }
    if ($errText) { Write-Host $errText.TrimEnd() -ForegroundColor Yellow }

    if ($proc.ExitCode -ne 0) {
        $combinedFailure = ($outText + "`n" + $errText)

        if ($combinedFailure -match "shallow repository|failed to unpack tree object|while checking out port") {
            Write-Host ""
            Write-Host "Detected vcpkg version-history failure. Running one full-history repair + retry..." -ForegroundColor Yellow

            $bootstrapGit = Find-BootstrapGit ""
            Ensure-VcpkgFullHistory $VcpkgRoot $bootstrapGit

            $retryStamp = Get-Date -Format "yyyyMMdd_HHmmss"
            $retryOut = Join-Path $logs ("vcpkg_install_retry_" + $retryStamp + ".out.log")
            $retryErr = Join-Path $logs ("vcpkg_install_retry_" + $retryStamp + ".err.log")

            $retry = Start-TunedProcess -FilePath $vcpkg `
                -ArgumentList $argLine `
                -WorkingDirectory $manifestRoot `
                -Stdout $retryOut `
                -Stderr $retryErr

            $retryOutText = ""
            $retryErrText = ""
            if (Test-Path $retryOut) { $retryOutText = Get-Content $retryOut -Raw -ErrorAction SilentlyContinue }
            if (Test-Path $retryErr) { $retryErrText = Get-Content $retryErr -Raw -ErrorAction SilentlyContinue }
            if ($retryOutText) { Write-Host $retryOutText.TrimEnd() }
            if ($retryErrText) { Write-Host $retryErrText.TrimEnd() -ForegroundColor Yellow }

            if ($retry.ExitCode -ne 0) {
                throw "vcpkg dependency installation retry failed (exit $($retry.ExitCode)). Logs: $retryOut ; $retryErr"
            }
        } else {
            throw "vcpkg dependency installation failed (exit $($proc.ExitCode)). Logs: $stdout ; $stderr"
        }
    }

    if (-not (Test-Path $tripletRoot -PathType Container)) {
        throw "vcpkg reported success but install root is missing: $tripletRoot"
    }
    Write-Utf8NoBom $cacheMarker ($fingerprint + "`n")
    Write-Host "CDDA vcpkg dependencies: READY" -ForegroundColor Green
}

function Build-Host-NoPdb([string]$CddaRoot,[string]$VcpkgRoot,[string]$BuildRoot,[object]$Vs,[string]$PatchRevision) {
    $propsPath = Join-Path $CddaRoot "msvc-full-features\Cataclysm-common.props"
    $solution = Join-Path $CddaRoot "msvc-full-features\Cataclysm-vcpkg-static.sln"
    $logs = Join-Path $BuildRoot "logs"
    New-Item -ItemType Directory -Force $logs | Out-Null

    if (-not (Test-Path $propsPath -PathType Leaf) -or
        -not (Test-Path $solution -PathType Leaf)) {
        throw "CDDA MSVC project files missing."
    }

    $builtHostPath = Join-Path $CddaRoot "cataclysm-tiles.exe"
    $cacheMarker = Join-Path $BuildRoot ".ncmm_host_0831_persistence_build.sha256"
    $compilerIdentity = [string](Get-Item $Vs.CL).VersionInfo.FileVersion
    $hostFingerprint = Hash-Text ((@(
        "v8.2-host-0831-persistence",
        $CddaCommit,
        $VcpkgCommit,
        $PatchRevision,
        $compilerIdentity,
        [string]$Vs.CL
    )) -join "|")
    $hostCacheLines = @()
    if (Test-Path $cacheMarker -PathType Leaf) { $hostCacheLines = @(Get-Content $cacheMarker) }
    if ($hostCacheLines.Count -ge 2 -and
        (Test-Path $builtHostPath -PathType Leaf) -and
        ((Get-Item $builtHostPath).Length -gt 10000000) -and
        $hostCacheLines[0].Trim() -eq $hostFingerprint -and
        $hostCacheLines[1].Trim() -eq (Hash-File $builtHostPath)) {
        Write-Host "Host build cache: HIT (inputs + binary hash verified)" -ForegroundColor Green
        return $builtHostPath
    }

    Ensure-CddaVcpkgDependencies $CddaRoot $VcpkgRoot $BuildRoot

    # Hotfix 16: the upstream Cataclysm-libMZ project discovers sources through a wildcard.
    # Live Hotfix 15 proved that relying on wildcard/incremental evaluation is insufficient for
    # a newly generated NCMM translation unit: ncmm_loader.obj and the MZ archive were deleted,
    # yet MSBuild recreated the archive without compiling ncmm_loader.cpp.  Make loader membership
    # deterministic for this build by excluding it from the wildcard and adding one explicit
    # ClCompile item.  The project file is restored byte-for-byte in the build finally block.
    #
    # The working source tree is reset transactionally while objwin is deliberately preserved, so
    # an older ncmm_loader.obj can be newer than the freshly copied source and MSBuild
    # may skip the loader translation unit.  That leaves the old Host symbols linkable
    # while every new Host API 2.0 generic hook becomes an unresolved external.
    # Force the loader TU and its static archive to be recreated on every host cache miss.
    $loaderSourcePath = Join-Path $CddaRoot "src\ncmm_loader.cpp"
    if (-not (Test-Path $loaderSourcePath -PathType Leaf)) {
        throw "NCMM host loader source missing before MSBuild: $loaderSourcePath"
    }

    # Hotfix 16: deterministic project membership.  Do not depend on MSBuild expanding
    # the upstream ..\src\*.cpp wildcard to include a generated source file.
    $mzProjectPath = Join-Path $CddaRoot "msvc-full-features\Cataclysm-libMZ-vcpkg-static.vcxproj"
    if (-not (Test-Path $mzProjectPath -PathType Leaf)) {
        throw "Cataclysm-libMZ project missing before NCMM explicit loader integration: $mzProjectPath"
    }
    $mzProjectOriginal = [IO.File]::ReadAllText($mzProjectPath)
    $mzProjectLf = $mzProjectOriginal.Replace("`r`n","`n").Replace("`r","`n")
    $explicitLoaderItem = '<ClCompile Include="..\src\ncmm_loader.cpp" />'
    if ($mzProjectLf.Contains($explicitLoaderItem)) {
        throw 'Cataclysm-libMZ project already contains an explicit ncmm_loader.cpp item before NCMM injection.'
    }
    $mzWildcardPattern = '(?m)^(?<indent>[ \t]*)<ClCompile Include="\.\.\\src\\\*\.cpp" Exclude="(?<exclude>[^"]*)" />[ \t]*$'
    $mzWildcardMatch = [regex]::Match($mzProjectLf,$mzWildcardPattern)
    if (-not $mzWildcardMatch.Success) {
        throw 'Cataclysm-libMZ wildcard ClCompile contract missing; cannot integrate ncmm_loader.cpp deterministically.'
    }
    $mzExclude = [string]$mzWildcardMatch.Groups['exclude'].Value
    if ($mzExclude.Split(';') -contains '..\src\ncmm_loader.cpp') {
        throw 'Cataclysm-libMZ wildcard unexpectedly already excludes ncmm_loader.cpp without an explicit item.'
    }
    $mzIndent = [string]$mzWildcardMatch.Groups['indent'].Value
    $mzReplacement = $mzIndent + '<ClCompile Include="..\src\*.cpp" Exclude="' + $mzExclude + ';..\src\ncmm_loader.cpp" />' + "`n" +
                     $mzIndent + $explicitLoaderItem
    $mzProjectPatched = $mzProjectLf.Substring(0,$mzWildcardMatch.Index) + $mzReplacement +
                        $mzProjectLf.Substring($mzWildcardMatch.Index + $mzWildcardMatch.Length)
    if (([regex]::Matches($mzProjectPatched,[regex]::Escape($explicitLoaderItem))).Count -ne 1 -or
        ([regex]::Matches($mzProjectPatched,[regex]::Escape('..\src\ncmm_loader.cpp'))).Count -ne 2) {
        throw 'Cataclysm-libMZ explicit ncmm_loader.cpp project integration audit failed.'
    }
    Write-Host 'Host loader MSBuild project integration: explicit ClCompile item READY.' -ForegroundColor DarkCyan

    $objWinRoot = Join-Path $CddaRoot "objwin"
    $loaderObjPath = Join-Path $CddaRoot "objwin\Release\x64\Cataclysm-libMZ-vcpkg-static\ncmm_loader.obj"
    $loaderObjCandidates = @()
    if (Test-Path $objWinRoot -PathType Container) {
        $loaderObjCandidates = @(Get-ChildItem -LiteralPath $objWinRoot -Filter "ncmm_loader.obj" -File -Recurse -ErrorAction SilentlyContinue)
    }
    foreach ($loaderObj in $loaderObjCandidates) {
        Remove-Item -LiteralPath $loaderObj.FullName -Force -ErrorAction SilentlyContinue
        if (Test-Path $loaderObj.FullName -PathType Leaf) {
            throw "Could not invalidate stale NCMM loader object: $($loaderObj.FullName)"
        }
    }
    $mzLibPath = Join-Path $CddaRoot "objwin\Release\x64\Cataclysm-libMZ-vcpkg-static\Cataclysm-libMZ-vcpkg-static-Release-x64.lib"
    Remove-Item -LiteralPath $mzLibPath -Force -ErrorAction SilentlyContinue
    if (Test-Path $mzLibPath -PathType Leaf) {
        throw "Could not invalidate stale Cataclysm-libMZ archive: $mzLibPath"
    }
    (Get-Item -LiteralPath $loaderSourcePath).LastWriteTimeUtc = [DateTime]::UtcNow
    Write-Host ("Host loader cache invalidation: removed {0} ncmm_loader.obj file(s); MZ archive reset; explicit project item armed." -f $loaderObjCandidates.Count) -ForegroundColor DarkCyan

    # A cache miss must never be allowed to fall back to a host executable produced
    # by an older patch/build.  Remove the exact output before invoking MSBuild.
    Remove-Item $builtHostPath -Force -ErrorAction SilentlyContinue
    if (Test-Path $builtHostPath -PathType Leaf) {
        throw "Could not remove stale host build output before MSBuild: $builtHostPath"
    }
    $buildStartedUtc = [DateTime]::UtcNow

    $original = [IO.File]::ReadAllText($propsPath)
    $patched = [regex]::Replace(
        $original,
        '<GenerateDebugInformation>[^<]*</GenerateDebugInformation>',
        '<GenerateDebugInformation>false</GenerateDebugInformation>'
    )
    $patched = [regex]::Replace(
        $patched,
        '<StripPrivateSymbols>[^<]*</StripPrivateSymbols>',
        '<StripPrivateSymbols></StripPrivateSymbols>'
    )
    if ($patched -eq $original) {
        throw "Could not temporarily disable linker PDB generation."
    }

    Get-Process mspdbsrv -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    foreach ($p in @(
        (Join-Path $CddaRoot "cataclysm-tiles.pdb"),
        (Join-Path $CddaRoot "cataclysm-tiles.stripped.pdb"),
        (Join-Path $CddaRoot "cataclysm-tiles.ilk")
    )) {
        Remove-Item $p -Force -ErrorAction SilentlyContinue
    }

    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $stdout = Join-Path $logs ("msbuild_survivor0910_" + $stamp + ".out.log")
    $stderr = Join-Path $logs ("msbuild_survivor0910_" + $stamp + ".err.log")
    $code = -1

    try {
        Write-Utf8NoBom $propsPath $patched
        Write-Utf8NoBom $mzProjectPath $mzProjectPatched

        # Re-read the actual project bytes that MSBuild will consume.  This is a hard gate,
        # not just a check of the in-memory replacement string.
        $mzProjectBuildText = [IO.File]::ReadAllText($mzProjectPath)
        if (([regex]::Matches($mzProjectBuildText,[regex]::Escape($explicitLoaderItem))).Count -ne 1 -or
            -not $mzProjectBuildText.Contains('..\src\ncmm_loader.cpp')) {
            throw 'Cataclysm-libMZ project write verification failed for explicit ncmm_loader.cpp integration.'
        }

        $env:BACKTRACE = "1"
        $env:CDDA_RELEASE_BUILD = "1"
        $env:VCPKG_ROOT = $VcpkgRoot
        $env:VCPKG_OVERLAY_TRIPLETS = Join-Path $CddaRoot ".github\vcpkg_triplets"
        $env:VCPKG_DOWNLOADS = Join-Path $BuildRoot "vcpkg_downloads"

        Write-Host ""
        Write-Host "Incremental host build (local no-PDB link)..." -ForegroundColor Cyan
        Write-Host "  MSBuild: $($Vs.MSBuild)"
        Write-Host "  solution: $solution"
        Write-Host "  vcpkg manifest auto-install: DISABLED (dependencies preinstalled)"
        Write-Host "  build profile: $($script:BuildTuning.Profile) | MSBuild /m:$($script:BuildTuning.MsBuildNodes) | CL_MPCount=$($script:BuildTuning.ClMpCount) | priority=$($script:BuildTuning.Priority)"

        $manifestInstallRoot = Join-Path $CddaRoot "msvc-full-features\vcpkg_installed\x64-windows-static"

        $argLine = @(
            "-m:$($script:BuildTuning.MsBuildNodes)",
            "-nologo",
            "-verbosity:minimal",
            "-p:Configuration=Release",
            "-p:Platform=x64",
            "-p:BuildInParallel=true",
            "-p:CL_MPCount=$($script:BuildTuning.ClMpCount)",
            "-p:VcpkgManifestInstall=false",
            "-p:VcpkgInstalledDir=`"$manifestInstallRoot`"",
            "-target:Cataclysm-vcpkg-static",
            "`"$solution`""
        ) -join " "

        $proc = Start-TunedProcess -FilePath $Vs.MSBuild `
            -ArgumentList $argLine `
            -WorkingDirectory $CddaRoot `
            -Stdout $stdout `
            -Stderr $stderr
        $code = $proc.ExitCode
    } finally {
        [IO.File]::WriteAllText($propsPath,$original,(New-Object Text.UTF8Encoding($false)))
        [IO.File]::WriteAllText($mzProjectPath,$mzProjectOriginal,(New-Object Text.UTF8Encoding($false)))
    }

    $outText = ""
    $errText = ""
    if (Test-Path $stdout) { $outText = Get-Content $stdout -Raw -ErrorAction SilentlyContinue }
    if (Test-Path $stderr) { $errText = Get-Content $stderr -Raw -ErrorAction SilentlyContinue }

    # Never trust ExitCode alone.  MSBuild/PowerShell wrappers can surface zero even
    # while compiler failures were emitted to redirected output.  Canonical errors
    # in either log are authoritative and always fail the build.
    $combinedExitProbe = $outText + "`n" + $errText
    $hasArtifactLine = $combinedExitProbe -match '(?im)^\s*Cataclysm-vcpkg-static\.vcxproj\s*->\s*.*cataclysm-tiles\.exe\s*$'
    $loaderCompiledThisRun = $combinedExitProbe -match '(?im)^\s*ncmm_loader\.cpp\s*$'
    $hasCanonicalFailure = $combinedExitProbe -match '(?i)\berror\s+(C|LNK)\d+|\bfatal error\b|\bMSB\d+\b.*(?:error|failed)|:\s*error\b|Build FAILED'

    if ($hasCanonicalFailure) {
        if ($code -eq 0) {
            Write-Host "MSBuild returned exit 0 but compiler/linker failure markers were found; rejecting the build." -ForegroundColor Red
        }
        $code = -998
    } elseif ($null -eq $code -or [string]::IsNullOrWhiteSpace([string]$code)) {
        if ($hasArtifactLine -and (Test-Path $builtHostPath -PathType Leaf)) {
            Write-Host "MSBuild ExitCode was unavailable; clean logs and fresh artifact path were observed." -ForegroundColor Yellow
            $code = 0
        } else {
            Write-Host "MSBuild ExitCode was unavailable and success could not be proven from this run." -ForegroundColor Red
            $code = -999
        }
    }

    # Hotfix 16 requires proof that the explicitly integrated ncmm_loader translation unit was
    # actually compiled in this invocation.  Without that evidence a stale loader object
    # could silently reintroduce the exact Host API 2.0 unresolved-symbol failure.
    if ($code -eq 0 -and -not $loaderCompiledThisRun) {
        Write-Host "MSBuild did not compile explicitly integrated ncmm_loader.cpp; rejecting the build." -ForegroundColor Red
        $code = -995
    }
    if ($code -eq 0) {
        if (-not (Test-Path $loaderObjPath -PathType Leaf)) {
            Write-Host "MSBuild loader object was not recreated from the explicit ClCompile item." -ForegroundColor Red
            $code = -994
        } else {
            $loaderObjInfo = Get-Item $loaderObjPath
            if ($loaderObjInfo.Length -lt 1024 -or $loaderObjInfo.LastWriteTimeUtc -lt $buildStartedUtc.AddSeconds(-5)) {
                Write-Host "MSBuild loader object is missing/freshness-invalid after explicit ClCompile integration." -ForegroundColor Red
                $code = -993
            }
        }
    }

    # The expected output was deleted before MSBuild, so existence now proves that
    # this run recreated it.  Also reject implausibly old timestamps.
    if ($code -eq 0) {
        if (-not (Test-Path $builtHostPath -PathType Leaf)) {
            Write-Host "MSBuild reported success but did not recreate the expected host executable." -ForegroundColor Red
            $code = -997
        } else {
            $freshInfo = Get-Item $builtHostPath
            if ($freshInfo.LastWriteTimeUtc -lt $buildStartedUtc.AddSeconds(-5)) {
                Write-Host "MSBuild host executable timestamp predates this build; rejecting stale output." -ForegroundColor Red
                $code = -996
            }
        }
    }

    if ($code -ne 0) {
        Write-Host ""
        Write-Host "=== MSBUILD PRIMARY ERRORS ===" -ForegroundColor Red

        $combined = New-Object System.Collections.Generic.List[string]
        if ($outText) {
            foreach ($line in ($outText -split "`r?`n")) { $combined.Add($line) }
        }
        if ($errText) {
            foreach ($line in ($errText -split "`r?`n")) { $combined.Add($line) }
        }

        $errorIndexes = New-Object System.Collections.Generic.List[int]
        for ($i = 0; $i -lt $combined.Count; ++$i) {
            $line = $combined[$i]
            if ($line -match '(?i)\berror\s+(C|LNK)\d+|\bfatal error\b|\bMSB\d+\b.*(?:error|failed)|:\s*error\b|Build FAILED') {
                $errorIndexes.Add($i)
            }
        }

        if ($errorIndexes.Count -gt 0) {
            $printed = New-Object 'System.Collections.Generic.HashSet[int]'
            foreach ($index in $errorIndexes) {
                $from = [Math]::Max(0,$index - 3)
                $to = [Math]::Min($combined.Count - 1,$index + 5)
                for ($j = $from; $j -le $to; ++$j) {
                    if ($printed.Add($j)) {
                        Write-Host $combined[$j]
                    }
                }
                Write-Host "---"
            }
        } else {
            Write-Host "No canonical compiler/linker error line detected; showing tail."
        }

        Write-Host ""
        Write-Host "=== MSBUILD FAILURE TAIL ===" -ForegroundColor Red
        $combined | Select-Object -Last 180 | ForEach-Object { Write-Host $_ }

        throw "NCMM 0.8.0 host build/link failed (exit $code). Full logs: $stdout ; $stderr"
    }

    if ($outText) {
        ($outText -split "`r?`n") |
            Where-Object { $_ -match 'error|warning|Build succeeded|Build FAILED|MSB\d+' } |
            Select-Object -Last 40 |
            ForEach-Object { Write-Host $_ }
    }

    if (-not (Test-Path $builtHostPath -PathType Leaf)) {
        throw "MSBuild passed gates but the exact expected cataclysm-tiles.exe is missing. Logs: $stdout ; $stderr"
    }

    $hostSha = Hash-File $builtHostPath
    Write-Utf8NoBom $cacheMarker ($hostFingerprint + "`n" + $hostSha + "`n")
    Write-Host "Host build: READY" -ForegroundColor Green
    return $builtHostPath
}

function Apply-NcmmRuntimeGameplayHooksV2([string]$Root) {
    Write-Host "Applying Host API 2.0 generic runtime gameplay hooks..." -ForegroundColor Cyan
    $src = Join-Path $Root "src"
    $magicPath = Join-Path $src "magic.cpp"
    $knowledgePath = Join-Path $src "character_knowledge.cpp"
    $creaturePath = Join-Path $src "creature.cpp"
    $characterPath = Join-Path $src "character.cpp"
    $characterHealthPath = Join-Path $src "character_health.cpp"
    $meleePath = Join-Path $src "melee.cpp"
    $rangedPath = Join-Path $src "ranged.cpp"
    $marker = Join-Path $Root ".ncmm_runtime_gameplay_hooks_v2"
    foreach ($legacy in @('.ncmm_survivor_modstats_v8','.ncmm_survivor_modstats_v82','.ncmm_survivor_modstats_v83')) {
        Remove-Item (Join-Path $Root $legacy) -Force -ErrorAction SilentlyContinue
    }
    foreach ($p in @($magicPath,$knowledgePath,$creaturePath,$characterPath,$characterHealthPath,$meleePath,$rangedPath)) {
        if (-not (Test-Path $p -PathType Leaf)) { throw "Host API 2.0 runtime-hook source file missing: $p" }
    }

    $Utf8NoBomV82 = New-Object System.Text.UTF8Encoding($false)
    function Read-V82([string]$Path) { return [IO.File]::ReadAllText($Path) }
    function Write-V82([string]$Path,[string]$Text) { [IO.File]::WriteAllText($Path,$Text,$Utf8NoBomV82) }
    function Replace-V82Once([string]$Text,[string]$Old,[string]$New,[string]$Name) {
        $Text = Normalize-Lf $Text; $Old = (Normalize-Lf $Old).TrimEnd(); $New = (Normalize-Lf $New).TrimEnd()
        $count = ([regex]::Matches($Text,[regex]::Escape($Old))).Count
        if ($count -ne 1) { throw "Host API 2.0 source contract '$Name' expected exactly once, found $count" }
        return $Text.Replace($Old,$New)
    }
    function Replace-V82Range([string]$Text,[string]$Start,[string]$End,[string]$Replacement,[string]$Name) {
        $Text = Normalize-Lf $Text; $Start = Normalize-Lf $Start; $End = Normalize-Lf $End; $Replacement = Normalize-Lf $Replacement
        $a = $Text.IndexOf($Start); if ($a -lt 0) { throw "Host API 2.0 source range '$Name' start not found: $Start" }
        $b = $Text.IndexOf($End,$a + $Start.Length); if ($b -lt 0 -or $b -le $a) { throw "Host API 2.0 source range '$Name' end not found: $End" }
        return $Text.Substring(0,$a) + $Replacement.TrimEnd() + "`n`n" + $Text.Substring($b)
    }
    function Replace-V82OneOf([string]$Text,[object[]]$Candidates,[string]$New,[string]$Name) {
        $Text = Normalize-Lf $Text; $New = (Normalize-Lf $New).TrimEnd(); $matched = $null; $total = 0
        foreach ($candidateRaw in $Candidates) {
            $candidate = (Normalize-Lf ([string]$candidateRaw)).TrimEnd(); $count = ([regex]::Matches($Text,[regex]::Escape($candidate))).Count
            if ($count -gt 0) { $total += $count; $matched = $candidate }
        }
        if ($total -ne 1 -or $null -eq $matched) { throw "Host API 2.0 source contract '$Name' expected one known source form, found $total" }
        return $Text.Replace($matched,$New)
    }
    function Test-V82Contains([string]$Text,[string]$Needle) {
        return (Normalize-Lf $Text).Contains((Normalize-Lf $Needle).TrimEnd())
    }

    $magicProbe = Read-V82 $magicPath; $knowledgeProbe = Read-V82 $knowledgePath; $creatureProbe = Read-V82 $creaturePath
    $characterProbe = Read-V82 $characterPath; $characterHealthProbe = Read-V82 $characterHealthPath
    $meleeProbe = Read-V82 $meleePath; $rangedProbe = Read-V82 $rangedPath
    $complete = $magicProbe.Contains('static const char *ncmm_spell_hook_id(') -and
                $knowledgeProbe.Contains('static float ncmm_runtime_skill_bonus( const skill_id &ident )') -and
                $creatureProbe.Contains('combat.damage_to_species_pct') -and
                $creatureProbe.Contains('combat.damage_taken_pct') -and
                $characterHealthProbe.Contains('combat.damage_avoid_pct') -and
                $characterProbe.Contains('combat.dodge_attempts_bonus') -and
                $characterProbe.Contains('combat.free_dodge_attempts_bonus') -and
                $characterProbe.Contains('combat.block_attempts_bonus') -and
                $meleeProbe.Contains('combat.melee_crit_chance_pct') -and
                $meleeProbe.Contains('combat.melee_crit_damage_pct') -and
                $rangedProbe.Contains('combat.ranged_crit_damage_pct')
    if ($complete) {
        if (-not (Test-Path $marker -PathType Leaf)) { Set-Content -Path $marker -Value "Recovered Host API 2.0 runtime-hook marker`n" -Encoding ASCII }
        foreach ($forbidden in @('"mg_','"mom_','"xe_','"af_','"afp_','"secx_','"sec_damage_pct','"sec_resist_pct','SECROZED_','SFLESH')) {
            if ($magicProbe.Contains($forbidden) -or $knowledgeProbe.Contains($forbidden) -or $creatureProbe.Contains($forbidden) -or
                $characterProbe.Contains($forbidden) -or $characterHealthProbe.Contains($forbidden) -or
                $meleeProbe.Contains($forbidden) -or $rangedProbe.Contains($forbidden)) {
                throw "Host API 2.0 generic CDDA hooks still contain module-specific token: $forbidden"
            }
        }
        Write-Host "Host API 2.0 generic runtime gameplay hooks already present." -ForegroundColor Green
        return
    }
    if ((Test-Path $marker -PathType Leaf)) { Remove-Item $marker -Force }

    $magic = Read-V82 $magicPath
    if (-not $magic.Contains('#include "ncmm_loader.h"')) {
        $magic = Replace-V82Once $magic '#include "magic.h"' ('#include "magic.h"' + "`n" + '#include "ncmm_loader.h"') 'magic.include-ncmm'
    }

    $magicHelper = @'

enum class ncmm_spell_modifier {
    cost,
    cast_time,
    failure,
    experience,
    power,
    range,
    area,
    duration
};

static const char *ncmm_spell_hook_id( ncmm_spell_modifier modifier )
{
    switch( modifier ) {
        case ncmm_spell_modifier::cost: return "spell.cost_pct";
        case ncmm_spell_modifier::cast_time: return "spell.cast_time_pct";
        case ncmm_spell_modifier::failure: return "spell.failure_pct";
        case ncmm_spell_modifier::experience: return "spell.experience_pct";
        case ncmm_spell_modifier::power: return "spell.power_pct";
        case ncmm_spell_modifier::range: return "spell.range_pct";
        case ncmm_spell_modifier::area: return "spell.area_pct";
        case ncmm_spell_modifier::duration: return "spell.duration_pct";
    }
    return nullptr;
}

static double ncmm_spell_source_modifier( const spell &sp, ncmm_spell_modifier modifier )
{
    const char *hook = ncmm_spell_hook_id( modifier );
    if( hook == nullptr ) return 0.0;
    const std::string source = sp.get_src().str();
    return ncmm::runtime_hook_modifier( hook, nullptr,
                                       source.empty() ? nullptr : source.c_str(), nullptr, nullptr );
}

static double ncmm_spell_multiplier( const spell &sp, ncmm_spell_modifier modifier,
                                     double minimum = 0.10 )
{
    return std::max( minimum, 1.0 + ncmm_spell_source_modifier( sp, modifier ) / 100.0 );
}

enum class ncmm_shared_mana_modifier_kind { maximum, regeneration };

static double ncmm_shared_mana_modifier( ncmm_shared_mana_modifier_kind modifier )
{
    return ncmm::runtime_hook_modifier(
        modifier == ncmm_shared_mana_modifier_kind::maximum ?
        "magic.mana.max_pct" : "magic.mana.regen_pct" );
}

class ncmm_spell_source_scope
{
    public:
        explicit ncmm_spell_source_scope( const spell &sp ) :
            previous_( ncmm::runtime_source_mod_swap( sp.get_src().str() ) ) {}
        ncmm_spell_source_scope( const ncmm_spell_source_scope & ) = delete;
        ncmm_spell_source_scope &operator=( const ncmm_spell_source_scope & ) = delete;
        ~ncmm_spell_source_scope() { ncmm::runtime_source_mod_swap( previous_ ); }
    private:
        std::string previous_;
};

static float ncmm_spell_skill_level( const spell &sp, const Character &guy )
{
    const ncmm_spell_source_scope source_scope( sp );
    return guy.get_skill_level( sp.skill() );
}
'@
    $helperAnchor = 'static std::map<spell_id, spell_migration> spell_migrations;'
    if ($magic.Contains('static double ncmm_spell_source_modifier')) {
        $magic = Replace-V82Range $magic 'static double ncmm_spell_source_modifier' 'static std::string target_to_string' $magicHelper 'magic.upgrade-helper'
    } elseif (-not $magic.Contains('ncmm_spell_hook_id')) {
        $magic = Replace-V82Once $magic $helperAnchor ($helperAnchor + $magicHelper) 'magic.mod-source-helper'
    }

    $energyVanilla = @'
    return std::max( cost * temp_spell_cost_multiplyer, 0.0f );
'@
    $energyV8 = @'
    const double multiplier = guy.is_avatar() ? ncmm_spell_multiplier( *this, "_spell_cost_pct" ) : 1.0;
    const double adjusted = std::max( cost * temp_spell_cost_multiplyer, 0.0f ) * multiplier;
    return std::max( 0, static_cast<int>( std::lround( adjusted ) ) );
'@
    $energyV82 = @'
    const int vanilla_cost = static_cast<int>( std::max( cost * temp_spell_cost_multiplyer, 0.0f ) );
    const double multiplier = guy.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::cost ) : 1.0;
    return std::max( 0, static_cast<int>( std::lround( vanilla_cost * multiplier ) ) );
'@
    $magic = Replace-V82OneOf $magic @($energyVanilla,$energyV8,$energyV82) $energyV82 'magic.energy-cost'

    $castVanilla = @'
    return std::max( casting_time * temp_cast_time_multiplyer, 0.0f );
'@
    $castV8 = @'
    const double multiplier = guy.is_avatar() ? ncmm_spell_multiplier( *this, "_cast_time_pct" ) : 1.0;
    const double adjusted = std::max( casting_time * temp_cast_time_multiplyer, 0.0f ) * multiplier;
    return std::max( 0, static_cast<int>( std::lround( adjusted ) ) );
'@
    $castV82 = @'
    const int vanilla_time = static_cast<int>( std::max( casting_time * temp_cast_time_multiplyer, 0.0f ) );
    const double multiplier = guy.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::cast_time ) : 1.0;
    return std::max( 0, static_cast<int>( std::lround( vanilla_time * multiplier ) ) );
'@
    $magic = Replace-V82OneOf $magic @($castVanilla,$castV8,$castV82) $castV82 'magic.cast-time'

    $skillNormalOld = @'
    const float effective_skill = 2 * ( get_effective_level() - get_difficulty(
                                            guy ) ) + guy.get_int() +
                                  guy.get_skill_level( skill() );
'@
    $skillNormalNew = @'
    const float effective_skill = 2 * ( get_effective_level() - get_difficulty(
                                            guy ) ) + guy.get_int() +
                                  ncmm_spell_skill_level( *this, guy );
'@
    if ($magic.Contains((Normalize-Lf $skillNormalOld).TrimEnd())) {
        $magic = Replace-V82Once $magic $skillNormalOld $skillNormalNew 'magic.fail-effective-skill'
    }
    $skillPsiOld = @'
        const float psi_effective_skill_initial = 2 * ( ( guy.get_skill_level(
                    skill() ) * 2 ) - get_difficulty(
'@
    $skillPsiNew = @'
        const float psi_effective_skill_initial = 2 * ( ( ncmm_spell_skill_level(
                    *this, guy ) * 2 ) - get_difficulty(
'@
    if ($magic.Contains((Normalize-Lf $skillPsiOld).TrimEnd())) {
        $magic = Replace-V82Once $magic $skillPsiOld $skillPsiNew 'magic.fail-psi-skill'
    }

    $customFailFormulaOld = @'
        const_dialogue d( get_const_talker_for( guy ), nullptr );
        d.set_value( "spell_id", id().str() );
        fail_chance = type->magic_type.value()->failure_chance_formula_id.value()->eval( d );
'@
    $customFailFormulaNew = @'
        const ncmm_spell_source_scope source_scope( *this );
        const_dialogue d( get_const_talker_for( guy ), nullptr );
        d.set_value( "spell_id", id().str() );
        fail_chance = type->magic_type.value()->failure_chance_formula_id.value()->eval( d );
'@
    if ($magic.Contains((Normalize-Lf $customFailFormulaOld).TrimEnd())) {
        $magic = Replace-V82Once $magic $customFailFormulaOld $customFailFormulaNew 'magic.custom-fail-metaphysics-context'
    }

    $failVanilla = @'
    return clamp( fail_chance, 0.0f, 1.0f );
'@
    $failV8 = @'
    const double multiplier = guy.is_avatar() ? ncmm_spell_multiplier( *this, "_fail_pct", 0.0 ) : 1.0;
    const double adjusted = clamp( fail_chance, 0.0f, 1.0f ) * multiplier;
    return clamp( static_cast<float>( adjusted ), 0.0f, 1.0f );
'@
    $failV82 = @'
    const double multiplier = guy.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::failure, 0.0 ) : 1.0;
    const double adjusted = clamp( fail_chance, 0.0f, 1.0f ) * multiplier;
    return clamp( static_cast<float>( adjusted ), 0.0f, 1.0f );
'@
    $magic = Replace-V82OneOf $magic @($failVanilla,$failV8,$failV82) $failV82 'magic.fail-chance'

    $expSkillOld = '    const float spellcraft_modifier = guy.get_skill_level( skill() ) / 10.0f;'
    $expSkillNew = '    const float spellcraft_modifier = ncmm_spell_skill_level( *this, guy ) / 10.0f;'
    if ($magic.Contains($expSkillOld)) {
        $magic = Replace-V82Once $magic $expSkillOld $expSkillNew 'magic.exp-skill'
    }

    $castingExp = @'
int spell::casting_exp( const Character &guy ) const
{
    int result = 0;
    if( type->magic_type.has_value() && type->magic_type.value()->casting_xp_formula_id.has_value() ) {
        const ncmm_spell_source_scope source_scope( *this );
        const_dialogue d( get_const_talker_for( guy ), nullptr );
        d.set_value( "spell_id", id().str() );
        result = std::round( type->magic_type.value()->casting_xp_formula_id.value()->eval( d ) );
    } else {
        const int base_casting_xp = 75;
        result = std::round( guy.adjust_for_focus( base_casting_xp * exp_modifier( guy ) ) );
    }
    const double multiplier = guy.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::experience, 0.0 ) : 1.0;
    result = static_cast<int>( std::lround( result * multiplier ) );
    return std::max( 0, result );
}
'@
    $magic = Replace-V82Range $magic 'int spell::casting_exp( const Character &guy ) const' 'std::string spell::enumerate_targets() const' $castingExp 'magic.casting-exp'

    $damage = @'
int spell::damage( const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    const int leveled_damage = min_leveled_damage( caster );
    double result = 0.0;

    if( has_flag( spell_flag::RANDOM_DAMAGE ) ) {
        result = rng( std::min( leveled_damage, static_cast<int>( type->max_damage.evaluate( d ) ) ),
                      std::max( leveled_damage,
                                static_cast<int>( type->max_damage.evaluate( d ) ) ) ) * temp_damage_multiplyer;
    } else if( type->min_damage.evaluate( d ) >= 0 ||
               type->max_damage.evaluate( d ) >= type->min_damage.evaluate( d ) ) {
        result = std::min( leveled_damage,
                           static_cast<int>( type->max_damage.evaluate( d ) ) ) * temp_damage_multiplyer;
    } else {
        result = std::max( leveled_damage,
                           static_cast<int>( type->max_damage.evaluate( d ) ) ) * temp_damage_multiplyer;
    }

    const int vanilla_damage = static_cast<int>( result );
    if( !caster.is_avatar() ) {
        return vanilla_damage;
    }
    return static_cast<int>( std::lround( vanilla_damage *
                             ncmm_spell_multiplier( *this, ncmm_spell_modifier::power, 0.0 ) ) );
}
'@
    $magic = Replace-V82Range $magic 'int spell::damage( const Creature &caster ) const' 'int spell::min_leveled_accuracy( const Creature &caster ) const' $damage 'magic.spell-power'

    $damageDot = @'
double spell::damage_dot( const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    const double leveled_dot = min_leveled_dot( caster );
    double result;
    if( type->min_dot.evaluate( d ) >= 0.0 ||
        type->max_dot.evaluate( d ) >= type->min_dot.evaluate( d ) ) {
        result = std::min( leveled_dot, type->max_dot.evaluate( d ) );
    } else {
        result = std::max( leveled_dot, type->max_dot.evaluate( d ) );
    }
    if( caster.is_avatar() ) {
        result *= ncmm_spell_multiplier( *this, ncmm_spell_modifier::power, 0.0 );
    }
    return result;
}
'@
    $magic = Replace-V82Range $magic 'double spell::damage_dot( const Creature &caster ) const' 'damage_over_time_data spell::damage_over_time' $damageDot 'magic.spell-dot-power'

    $aoe = @'
int spell::aoe( const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    const int leveled_aoe = min_leveled_aoe( caster );
    int return_value;

    if( has_flag( spell_flag::RANDOM_AOE ) ) {
        return_value = rng( std::min( leveled_aoe, static_cast<int>( type->max_aoe.evaluate( d ) ) ),
                            std::max( leveled_aoe, static_cast<int>( type->max_aoe.evaluate( d ) ) ) );
    } else if( type->max_aoe.evaluate( d ) >= type->min_aoe.evaluate( d ) ) {
        return_value = std::min( leveled_aoe, static_cast<int>( type->max_aoe.evaluate( d ) ) );
    } else {
        return_value = std::max( leveled_aoe, static_cast<int>( type->max_aoe.evaluate( d ) ) );
    }
    const int vanilla_aoe = static_cast<int>( return_value * temp_aoe_multiplyer );
    const double multiplier = caster.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::area, 0.0 ) : 1.0;
    return static_cast<int>( std::lround( vanilla_aoe * multiplier ) );
}

std::set<tripoint_bub_ms> spell::effect_area( const spell_effect::override_parameters &params,
        const tripoint_bub_ms &source, const tripoint_bub_ms &target ) const
{
    return type->spell_area_function( params, source, target );
}

std::set<tripoint_bub_ms> spell::effect_area( const tripoint_bub_ms &source,
        const tripoint_bub_ms &target, const Creature &caster ) const
{
    return effect_area( spell_effect::override_parameters( *this, caster ), source, target );
}
'@
    # End at in_aoe intentionally: old v8 accidentally deleted the two effect_area definitions.
    # Including them in the replacement repairs already-v8-patched caches as well as pristine source.
    $magic = Replace-V82Range $magic 'int spell::aoe( const Creature &caster ) const' 'bool spell::in_aoe(' $aoe 'magic.aoe-and-effect-area'

    $inAoeOld = @'
bool spell::in_aoe( const tripoint_bub_ms &source, const tripoint_bub_ms &target,
                    const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    if( has_flag( spell_flag::RANDOM_AOE ) ) {
        return rl_dist( source, target ) <= type->max_aoe.evaluate( d );
    } else {
        return rl_dist( source, target ) <= aoe( caster );
    }
}
'@
    $inAoeNew = @'
bool spell::in_aoe( const tripoint_bub_ms &source, const tripoint_bub_ms &target,
                    const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    if( has_flag( spell_flag::RANDOM_AOE ) ) {
        const double multiplier = caster.is_avatar() ?
                                  ncmm_spell_multiplier( *this, ncmm_spell_modifier::area, 0.0 ) : 1.0;
        const double max_aoe = type->max_aoe.evaluate( d ) * multiplier;
        return rl_dist( source, target ) <= max_aoe;
    }
    return rl_dist( source, target ) <= aoe( caster );
}
'@
    if ($magic.Contains((Normalize-Lf $inAoeOld).TrimEnd())) {
        $magic = Replace-V82Once $magic $inAoeOld $inAoeNew 'magic.random-aoe-reach'
    }

    $range = @'
int spell::range( const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    const int leveled_range = type->min_range.evaluate( d ) + std::round( get_effective_level() *
                              type->range_increment.evaluate( d ) );
    float range;
    if( type->max_range.evaluate( d ) >= type->min_range.evaluate( d ) ) {
        range = std::min( leveled_range, static_cast<int>( type->max_range.evaluate( d ) ) );
    } else {
        range = std::max( leveled_range, static_cast<int>( type->max_range.evaluate( d ) ) );
    }
    const int vanilla_range = static_cast<int>( std::max( range * temp_range_multiplyer, 0.0f ) );
    const double multiplier = caster.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::range, 0.0 ) : 1.0;
    return std::max( 0, static_cast<int>( std::lround( vanilla_range * multiplier ) ) );
}
'@
    $magic = Replace-V82Range $magic 'int spell::range( const Creature &caster ) const' 'std::vector<tripoint_bub_ms> spell::targetable_locations' $range 'magic.range'

    $duration = @'
int spell::duration( const Creature &caster ) const
{
    const_dialogue d( get_const_talker_for( caster ), nullptr );
    const int leveled_duration = min_leveled_duration( caster );
    int return_value;
    if( has_flag( spell_flag::RANDOM_DURATION ) ) {
        return_value = rng( std::min( leveled_duration,
                                      static_cast<int>( type->max_duration.evaluate( d ) ) ),
                            std::max( leveled_duration,
                                      static_cast<int>( type->max_duration.evaluate( d ) ) ) );
    } else if( type->max_duration.evaluate( d ) >= type->min_duration.evaluate( d ) ) {
        return_value = std::min( leveled_duration, static_cast<int>( type->max_duration.evaluate( d ) ) );
    } else {
        return_value = std::max( leveled_duration, static_cast<int>( type->max_duration.evaluate( d ) ) );
    }
    const int vanilla_duration = static_cast<int>( std::max( return_value * temp_duration_multiplyer, 0.0f ) );
    const double multiplier = caster.is_avatar() ?
                              ncmm_spell_multiplier( *this, ncmm_spell_modifier::duration, 0.0 ) : 1.0;
    return std::max( 0, static_cast<int>( std::lround( vanilla_duration * multiplier ) ) );
}
'@
    $magic = Replace-V82Range $magic 'int spell::duration( const Creature &caster ) const' 'std::string spell::duration_string' $duration 'magic.duration'

    # CDDA exposes one known_magic mana pool for the avatar. Magiclysm and Xedra
    # therefore intentionally contribute to the same maximum/regeneration pool.
    # Source-scoped spell cost/power/range/etc. remain isolated per mod.
    $maxManaShared = @'
int known_magic::max_mana( const Character &guy ) const
{
    const float int_bonus = ( ( 0.2f + guy.get_int() * 0.1f ) - 1.0f ) * mana_base;
    int penalty_calc = std::round( std::max<int64_t>( 0,
                                   units::to_kilojoule( guy.get_power_level() ) ) );

    const int bionic_penalty = guy.enchantment_cache->modify_value(
                                   enchant_vals::mod::BIONIC_MANA_PENALTY, penalty_calc );

    const float unaugmented_mana = std::max( 0.0f,
                                   ( mana_base + int_bonus ) - bionic_penalty );
    const double vanilla = guy.calculate_by_enchantment( unaugmented_mana,
                           enchant_vals::mod::MAX_MANA, true );
    if( !guy.is_avatar() ) {
        return vanilla;
    }
    const double bonus = ncmm_shared_mana_modifier( ncmm_shared_mana_modifier_kind::maximum );
    if( bonus == 0.0 ) {
        return vanilla;
    }
    return vanilla * std::max( 0.10, 1.0 + bonus / 100.0 );
}
'@
    $magic = Replace-V82Range $magic 'int known_magic::max_mana( const Character &guy ) const' 'void known_magic::update_mana( const Character &guy, float turns )' $maxManaShared 'magic.shared-max-mana'

    $updateManaShared = @'
void known_magic::update_mana( const Character &guy, float turns )
{
    const double full_replenish = to_turns<double>( 8_hours );
    const double ratio = turns / full_replenish;
    double regen = std::max( 0.0,
                   guy.calculate_by_enchantment( static_cast<double>( max_mana( guy ) ),
                                                  enchant_vals::mod::REGEN_MANA ) );
    if( guy.is_avatar() ) {
        const double bonus = ncmm_shared_mana_modifier( ncmm_shared_mana_modifier_kind::regeneration );
        if( bonus != 0.0 ) {
            regen *= std::max( 0.10, 1.0 + bonus / 100.0 );
        }
    }
    mod_mana( guy, std::floor( ratio * regen ) );
}
'@
    $magic = Replace-V82Range $magic 'void known_magic::update_mana( const Character &guy, float turns )' 'std::vector<spell_id> known_magic::spells() const' $updateManaShared 'magic.shared-mana-regen'

    $knowledge = Read-V82 $knowledgePath
    $uniqueSkillHelper = @'
static float ncmm_runtime_skill_bonus( const skill_id &ident )
{
    const std::string subject = ident.str();
    const std::string hook = "skill." + subject + ".flat";
    const std::string &source = ncmm::runtime_source_mod();
    return static_cast<float>( ncmm::runtime_hook_modifier(
        hook.c_str(), subject.c_str(), source.empty() ? nullptr : source.c_str(), nullptr, nullptr ) );
}

float Character::get_skill_level( const skill_id &ident ) const
{
    float result = enchantment_cache->modify_value( ident,
                   _skills->get_skill_level( ident ) + _skills->get_progress_level( ident ) );
    if( is_avatar() ) result += ncmm_runtime_skill_bonus( ident );
    return result;
}
'@
    $knowledge = Replace-V82Range $knowledge 'float Character::get_skill_level( const skill_id &ident ) const' 'float Character::get_skill_level( const skill_id &ident, const item &context ) const' $uniqueSkillHelper 'knowledge.skill-no-context'

    $skillContext = @'
float Character::get_skill_level( const skill_id &ident, const item &context ) const
{
    float result = enchantment_cache->modify_value( ident, _skills->get_skill_level( ident,
                   context ) + _skills->get_progress_level( ident, context ) );
    if( is_avatar() ) {
        result += ncmm_runtime_skill_bonus( ident );
    }
    return result;
}
'@
    $knowledge = Replace-V82Range $knowledge 'float Character::get_skill_level( const skill_id &ident, const item &context ) const' 'int Character::get_knowledge_level( const skill_id &ident ) const' $skillContext 'knowledge.skill-context'

    foreach ($needle in @('ncmm_spell_hook_id','ncmm_spell_source_scope','ncmm_spell_skill_level','ncmm_shared_mana_modifier','spell::effect_area','spell::damage_dot')) {
        if (-not $magic.Contains($needle)) { throw "Host API 2.0 transformed magic source missing: $needle" }
    }
    $effectAreaCount = ([regex]::Matches($magic,[regex]::Escape('std::set<tripoint_bub_ms> spell::effect_area'))).Count
    if ($effectAreaCount -ne 2) { throw "Host API 2.0 expected two spell::effect_area overloads, found $effectAreaCount" }
    if (-not $knowledge.Contains('ncmm_runtime_skill_bonus')) { throw 'Host API 2.0 transformed skill source missing generic skill hook.' }
    if ($knowledge.Contains('return std::max( 0.0f, result );')) { throw 'Host API 2.0 skill hook changed vanilla negative-skill semantics.' }
    if (-not $magic.Contains('const double max_aoe = type->max_aoe.evaluate( d ) * multiplier;')) { throw 'Host API 2.0 RANDOM_AOE hook changed vanilla continuous comparison.' }
    foreach ($forbidden in @('"mg_','"mom_','"xe_','"af_','"afp_','"secx_','SECROZED_','SFLESH')) {
        if ($magic.Contains($forbidden) -or $knowledge.Contains($forbidden)) { throw "Host API 2.0 generic magic/skill source contains module token: $forbidden" }
    }

    # Survivor 0.11.0 mechanical-perk hooks are generic Host API 2.0 contracts.
    # CDDA knows only hook names; all modifier ownership/value semantics stay in modules.
    $character = Read-V82 $characterPath
    if( -not $character.Contains('#include "ncmm_loader.h"') ) {
        $character = Replace-V82Once $character '#include "character.h"' ('#include "character.h"' + "`n" + '#include "ncmm_loader.h"') 'character.include-ncmm'
    }
    $turnResetVanilla = @'
    // We can dodge again! Assuming we can actually move...
    if( in_sleep_state() ) {
        blocks_left = 0;
        set_dodges_left( 0 );
        set_free_dodges_left( 0 );
    } else if( moves > 0 ) {
        blocks_left = get_num_blocks();
        set_dodges_left( get_num_dodges() );
        set_free_dodges_left( get_num_free_dodges() );
    }
'@
    $turnResetMechanical = @'
    // We can dodge again! Assuming we can actually move...
    if( in_sleep_state() ) {
        blocks_left = 0;
        set_dodges_left( 0 );
        set_free_dodges_left( 0 );
    } else if( moves > 0 ) {
        const int ncmm_block_bonus = is_avatar() ? std::max( 0, std::min( 4,
                                     static_cast<int>( std::lround( ncmm::runtime_hook_modifier(
                                             "combat.block_attempts_bonus" ) ) ) ) ) : 0;
        const int ncmm_dodge_bonus = is_avatar() ? std::max( 0, std::min( 4,
                                     static_cast<int>( std::lround( ncmm::runtime_hook_modifier(
                                             "combat.dodge_attempts_bonus" ) ) ) ) ) : 0;
        const int ncmm_free_dodge_bonus = is_avatar() ? std::max( 0, std::min( 4,
                                          static_cast<int>( std::lround( ncmm::runtime_hook_modifier(
                                                  "combat.free_dodge_attempts_bonus" ) ) ) ) ) : 0;
        blocks_left = get_num_blocks() + ncmm_block_bonus;
        set_dodges_left( std::max( 0, get_num_dodges() + ncmm_dodge_bonus ) );
        set_free_dodges_left( std::max( 0, get_num_free_dodges() + ncmm_free_dodge_bonus ) );
    }
'@
    if( Test-V82Contains $character $turnResetVanilla ) {
        $character = Replace-V82Once $character $turnResetVanilla $turnResetMechanical 'character.mechanical-defense-reset'
    } elseif( -not $character.Contains('combat.dodge_attempts_bonus') ) {
        throw 'Host API 2.0 character mechanical-defense hook is neither pristine nor already patched.'
    }

    $characterHealth = Read-V82 $characterHealthPath
    if( -not $characterHealth.Contains('#include "ncmm_loader.h"') ) {
        $characterHealth = Replace-V82Once $characterHealth '#include "character.h"' ('#include "character.h"' + "`n" + '#include "ncmm_loader.h"') 'character-health.include-ncmm'
    }
    $damageAvoidAnchor = @'
    if( has_effect( effect_incorporeal ) || has_flag( json_flag_CANNOT_TAKE_DAMAGE ) ) {
        return dealt_damage_instance();
    }

    //damage applied here
'@
    $damageAvoidMechanical = @'
    if( has_effect( effect_incorporeal ) || has_flag( json_flag_CANNOT_TAKE_DAMAGE ) ) {
        return dealt_damage_instance();
    }

    if( is_avatar() ) {
        const double avoid_pct = std::max( 0.0, std::min( 95.0,
                                   ncmm::runtime_hook_modifier( "combat.damage_avoid_pct" ) ) );
        if( avoid_pct > 0.0 && rng_float( 0.0, 100.0 ) < avoid_pct ) {
            return dealt_damage_instance();
        }
    }

    //damage applied here
'@
    if( Test-V82Contains $characterHealth $damageAvoidAnchor ) {
        $characterHealth = Replace-V82Once $characterHealth $damageAvoidAnchor $damageAvoidMechanical 'character-health.full-damage-avoidance'
    } elseif( -not $characterHealth.Contains('combat.damage_avoid_pct') ) {
        throw 'Host API 2.0 full-damage avoidance hook is neither pristine nor already patched.'
    }

    $melee = Read-V82 $meleePath
    if( -not $melee.Contains('#include "ncmm_loader.h"') ) {
        $melee = Replace-V82Once $melee '#include "melee.h"' ('#include "melee.h"' + "`n" + '#include "ncmm_loader.h"') 'melee.include-ncmm'
    }
    $critChanceVanilla = '    double ma_buff_crit_chance = mabuff_critical_hit_chance_bonus() / 100;'
    $critChanceMechanical = @'
    double ma_buff_crit_chance = mabuff_critical_hit_chance_bonus() / 100;
    if( is_avatar() ) {
        const double ncmm_bonus = ncmm::runtime_hook_modifier( "combat.melee_crit_chance_pct" );
        ma_buff_crit_chance += std::max( -0.95, std::min( 0.50, ncmm_bonus / 100.0 ) );
    }
'@
    if( $melee.Contains($critChanceVanilla) ) {
        $melee = Replace-V82Once $melee $critChanceVanilla $critChanceMechanical 'melee.crit-chance-hook'
    } elseif( -not $melee.Contains('combat.melee_crit_chance_pct') ) {
        throw 'Host API 2.0 melee critical-chance hook is neither pristine nor already patched.'
    }
    $meleeDamageVanilla = '    di.add_damage( dt, dmg, arpen, armor_mult, dmg_mul );'
    $meleeDamageMechanical = @'
    if( crit && u.is_avatar() ) {
        const double ncmm_bonus = ncmm::runtime_hook_modifier( "combat.melee_crit_damage_pct" );
        dmg_mul *= std::max( 0.10, 1.0 + ncmm_bonus / 100.0 );
    }
    di.add_damage( dt, dmg, arpen, armor_mult, dmg_mul );
'@
    if( $melee.Contains($meleeDamageVanilla) ) {
        $melee = Replace-V82Once $melee $meleeDamageVanilla $meleeDamageMechanical 'melee.crit-damage-hook'
    } elseif( -not $melee.Contains('combat.melee_crit_damage_pct') ) {
        throw 'Host API 2.0 melee critical-damage hook is neither pristine nor already patched.'
    }

    $ranged = Read-V82 $rangedPath
    if( -not $ranged.Contains('#include "ncmm_loader.h"') ) {
        $ranged = Replace-V82Once $ranged '#include "ranged.h"' ('#include "ranged.h"' + "`n" + '#include "ncmm_loader.h"') 'ranged.include-ncmm'
    }
    $rangedCritVanilla = '        proj.critical_multiplier = ammo->critical_multiplier;'
    $rangedCritMechanical = @'
        proj.critical_multiplier = ammo->critical_multiplier;
        if( guy.is_avatar() ) {
            const double ncmm_bonus = ncmm::runtime_hook_modifier( "combat.ranged_crit_damage_pct" );
            proj.critical_multiplier *= std::max( 0.10, 1.0 + ncmm_bonus / 100.0 );
        }
'@
    if( $ranged.Contains($rangedCritVanilla) ) {
        $ranged = Replace-V82Once $ranged $rangedCritVanilla $rangedCritMechanical 'ranged.crit-damage-hook'
    } elseif( -not $ranged.Contains('combat.ranged_crit_damage_pct') ) {
        throw 'Host API 2.0 ranged critical-damage hook is neither pristine nor already patched.'
    }

    $creature = Read-V82 $creaturePath
    if( -not $creature.Contains('#include "ncmm_loader.h"') ) {
        $creature = Replace-V82Once $creature '#include "monster.h"' ('#include "monster.h"' + "`n" + '#include "ncmm_loader.h"') 'creature.include-ncmm'
    }

    $damageCopyVanilla = '    damage_instance d = dam; // copy, since we will mutate in absorb_hit'
    $damageCopyLegacyV2 = @'
    damage_instance d = dam; // copy, since we will mutate in absorb_hit
    if( source != nullptr ) {
        double multiplier = 1.0;
        if( source->is_avatar() ) {
            const double bonus = ncmm::runtime_hook_modifier_for_creatures(
                                     "combat.damage_to_species_pct", source, this );
            multiplier *= std::max( 0.10, 1.0 + bonus / 100.0 );
        }
        if( is_avatar() ) {
            const double reduction = ncmm::runtime_hook_modifier_for_creatures(
                                         "combat.resist_from_species_pct", source, this );
            multiplier *= std::max( 0.10, 1.0 - reduction / 100.0 );
        }
        if( multiplier != 1.0 ) d.mult_damage( multiplier );
    }
'@
    $damageCopyMechanical = @'
    damage_instance d = dam; // copy, since we will mutate in absorb_hit
    double multiplier = 1.0;
    if( source != nullptr && source->is_avatar() ) {
        const double bonus = ncmm::runtime_hook_modifier_for_creatures(
                                 "combat.damage_to_species_pct", source, this );
        multiplier *= std::max( 0.10, 1.0 + bonus / 100.0 );
    }
    if( is_avatar() ) {
        if( source != nullptr ) {
            const double species_reduction = ncmm::runtime_hook_modifier_for_creatures(
                                                 "combat.resist_from_species_pct", source, this );
            multiplier *= std::max( 0.10, 1.0 - species_reduction / 100.0 );
        }
        const double general_reduction = ncmm::runtime_hook_modifier( "combat.damage_taken_pct" );
        multiplier *= std::max( 0.10, 1.0 - general_reduction / 100.0 );
    }
    if( multiplier != 1.0 ) d.mult_damage( multiplier );
'@
    if( $creature.Contains($damageCopyVanilla) ) {
        $creature = Replace-V82Once $creature $damageCopyVanilla $damageCopyMechanical 'creature.generic-runtime-hooks-v3'
    } elseif( Test-V82Contains $creature $damageCopyLegacyV2 ) {
        $creature = Replace-V82Once $creature $damageCopyLegacyV2 $damageCopyMechanical 'creature.upgrade-runtime-hooks-v2-to-v3'
    } elseif( -not $creature.Contains('combat.damage_to_species_pct') -or -not $creature.Contains('combat.damage_taken_pct') ) {
        throw 'Host API 2.0 creature hook is neither pristine, v2, nor already v3-patched.'
    }
    foreach ($forbidden in @('"sec_damage_pct','"sec_resist_pct','SECROZED_','SFLESH')) {
        if ($creature.Contains($forbidden)) { throw "Host API 2.0 creature source contains Survivor-specific token: $forbidden" }
    }

    Write-V82 $magicPath $magic
    Write-V82 $knowledgePath $knowledge
    Write-V82 $characterPath $character
    Write-V82 $characterHealthPath $characterHealth
    Write-V82 $meleePath $melee
    Write-V82 $rangedPath $ranged
    Write-V82 $creaturePath $creature
    Set-Content -Path $marker -Value "Host API 2.0 generic runtime gameplay hooks v3 / Survivor mechanical surface`n" -Encoding ASCII
    Write-Host "Host API 2.0 generic runtime gameplay hooks: APPLIED" -ForegroundColor Green
}



function Apply-NcmmReactiveMechanics0112([string]$Root) {
    Write-Host "Applying Host API 2.0 reactive/technical mechanics hooks for Survivor 0.11.2 edge-case polish..." -ForegroundColor Cyan
    $src = Join-Path $Root "src"
    $characterPath = Join-Path $src "character.cpp"
    $meleePath = Join-Path $src "melee.cpp"
    $creaturePath = Join-Path $src "creature.cpp"
    $monsterPath = Join-Path $src "monster.cpp"
    $craftingPath = Join-Path $src "crafting.cpp"
    $activityActorPath = Join-Path $src "activity_actor.cpp"
    $trapPath = Join-Path $src "trap.cpp"
    $loaderHeaderPath0111 = Join-Path $src "ncmm_loader.h"
    $loaderSourcePath0111 = Join-Path $src "ncmm_loader.cpp"
    $marker = Join-Path $Root ".ncmm_reactive_mechanics_0112"
    foreach($p in @($characterPath,$meleePath,$creaturePath,$monsterPath,$craftingPath,$activityActorPath,$trapPath,$loaderHeaderPath0111,$loaderSourcePath0111)) {
        if(-not (Test-Path $p -PathType Leaf)) { throw "Reactive mechanics source file missing: $p" }
    }
    $Utf8NoBom0111 = New-Object System.Text.UTF8Encoding($false)
    function Read-0111([string]$Path) { return Normalize-Lf ([IO.File]::ReadAllText($Path)) }
    function Write-0111([string]$Path,[string]$Text) { [IO.File]::WriteAllText($Path,(Normalize-Lf $Text),$Utf8NoBom0111) }
    function Replace-0111([string]$Text,[string]$Old,[string]$New,[string]$Name) {
        $Text=Normalize-Lf $Text; $Old=(Normalize-Lf $Old).TrimEnd(); $New=(Normalize-Lf $New).TrimEnd()
        $count=([regex]::Matches($Text,[regex]::Escape($Old))).Count
        if($count -ne 1) { throw "Reactive mechanics contract '$Name' expected exactly once, found $count" }
        return $Text.Replace($Old,$New)
    }

    $probes = @(
        @{Path=$characterPath;Needle='source->attitude_to( *this ) == Creature::Attitude::HOSTILE'},
        @{Path=$meleePath;Needle='is_avatar() && dam > 0 && !t.is_hallucination()'},
        @{Path=$creaturePath;Needle='combat.execute_threshold_pct'},
        @{Path=$monsterPath;Needle='ncmm_kill_attitude == MATT_ATTACK || ncmm_kill_attitude == MATT_FLEE'},
        @{Path=$craftingPath;Needle='crafting.failure_save_pct'},
        @{Path=$activityActorPath;Needle='const bool ncmm_perfect_lockpick = lockpick != nullptr'},
        @{Path=$trapPath;Needle='scavenging.trap_detection_flat'},
        @{Path=$loaderHeaderPath0111;Needle='void runtime_player_kill_notify();'},
        @{Path=$loaderSourcePath0111;Needle='void runtime_player_kill_notify()'}
    )
    $already=$true
    foreach($probe in $probes) { if(-not (Read-0111 $probe.Path).Contains($probe.Needle)) { $already=$false; break } }
    if($already) {
        if(-not (Test-Path $marker -PathType Leaf)) { Set-Content -Path $marker -Value "Recovered Survivor 0.11.2 reactive edge marker`n" -Encoding ASCII }
        Write-Host "Reactive/technical mechanics hooks already present." -ForegroundColor Green
        return
    }
    Remove-Item $marker -Force -ErrorAction SilentlyContinue

    # Successful dodge: immediate flow rewards plus guarded automatic riposte.
    $character = Read-0111 $characterPath
    if(-not $character.Contains('#include "ncmm_loader.h"')) { throw 'Survivor 0.11.2 requires v3 character runtime hooks first.' }
    $dodgeOld = @'
    magic->break_channeling( *this );

    // For adjacent attackers check for techniques usable upon successful dodge
    if( source && square_dist( pos_bub(), source->pos_bub() ) == 1 ) {
        matec_id tec = std::get<0>( pick_technique( *source, used_weapon(), false, true, false ) );

        if( tec != tec_none && !is_dead_state() ) {
            if( get_stamina() < get_stamina_max() / 3 ) {
                add_msg( m_bad, _( "You try to counterattack, but you are too exhausted!" ) );
            } else {
                melee_attack( *source, false, tec );
            }
        }
    }
'@
    $dodgeNew = @'
    magic->break_channeling( *this );

    // Preserve vanilla martial-arts dodge counters. NCMM riposte is a fallback,
    // never a replacement or an automatic double-counter.
    bool ncmm_vanilla_counter_fired = false;
    if( source && square_dist( pos_bub(), source->pos_bub() ) == 1 ) {
        matec_id tec = std::get<0>( pick_technique( *source, used_weapon(), false, true, false ) );

        if( tec != tec_none && !is_dead_state() ) {
            if( get_stamina() < get_stamina_max() / 3 ) {
                add_msg( m_bad, _( "You try to counterattack, but you are too exhausted!" ) );
            } else {
                ncmm_vanilla_counter_fired = melee_attack( *source, false, tec );
            }
        }
    }

    static thread_local bool ncmm_riposte_in_progress = false;
    if( is_avatar() ) {
        const int ncmm_dodge_moves = clamp<int>( static_cast<int>(
                                         ncmm::runtime_hook_modifier( "combat.on_dodge_moves" ) ), 0, 50 );
        if( ncmm_dodge_moves > 0 ) mod_moves( ncmm_dodge_moves );
        const double ncmm_dodge_stamina_pct = std::max( 0.0, std::min( 10.0,
                ncmm::runtime_hook_modifier( "combat.on_dodge_stamina_pct" ) ) );
        if( ncmm_dodge_stamina_pct > 0.0 ) {
            mod_stamina( static_cast<int>( get_stamina_max() * ncmm_dodge_stamina_pct / 100.0 ) );
        }
        const double ncmm_riposte_chance = std::max( 0.0, std::min( 60.0,
                ncmm::runtime_hook_modifier( "combat.riposte_chance_pct" ) ) );
        if( !ncmm_vanilla_counter_fired && !ncmm_riposte_in_progress && source != nullptr &&
            square_dist( pos_bub(), source->pos_bub() ) == 1 && !source->is_dead_state() &&
            !source->is_hallucination() &&
            source->attitude_to( *this ) == Creature::Attitude::HOSTILE &&
            !is_dead_state() && get_stamina() >= get_stamina_max() / 3 &&
            ncmm_riposte_chance > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_riposte_chance ) {
            ncmm_riposte_in_progress = true;
            const int ncmm_moves_before = get_moves();
            melee_attack( *source, false );
            const int ncmm_spent = std::max( 0, ncmm_moves_before - get_moves() );
            const double ncmm_refund_pct = std::max( 0.0, std::min( 100.0,
                    ncmm::runtime_hook_modifier( "combat.riposte_refund_pct" ) ) );
            if( ncmm_spent > 0 && ncmm_refund_pct > 0.0 ) {
                mod_moves( static_cast<int>( ncmm_spent * ncmm_refund_pct / 100.0 ) );
            }
            ncmm_riposte_in_progress = false;
        }
    }
'@
    $character = Replace-0111 $character $dodgeOld $dodgeNew 'character.after-dodge-riposte'
    Write-0111 $characterPath $character

    # Melee critical: immediate move/stamina surge; static crit chance/damage remains in v3 hook layer.
    $melee = Read-0111 $meleePath
    $critOld = @'
            if( critical_hit ) {
                // trigger martial arts on-crit effects
                martial_arts_data->ma_oncrit_effects( *this );
            }
'@
    $critNew = @'
            if( critical_hit ) {
                if( is_avatar() && dam > 0 && !t.is_hallucination() ) {
                    const int ncmm_crit_moves = clamp<int>( static_cast<int>(
                                                   ncmm::runtime_hook_modifier( "combat.on_crit_moves" ) ), 0, 50 );
                    if( ncmm_crit_moves > 0 ) mod_moves( ncmm_crit_moves );
                    const double ncmm_crit_stamina_pct = std::max( 0.0, std::min( 10.0,
                            ncmm::runtime_hook_modifier( "combat.on_crit_stamina_pct" ) ) );
                    if( ncmm_crit_stamina_pct > 0.0 ) {
                        mod_stamina( static_cast<int>( get_stamina_max() * ncmm_crit_stamina_pct / 100.0 ) );
                    }
                }
                // trigger martial arts on-crit effects
                martial_arts_data->ma_oncrit_effects( *this );
            }
'@
    $melee = Replace-0111 $melee $critOld $critNew 'melee.after-crit-effects'
    Write-0111 $meleePath $melee

    # Generic outgoing damage and low-health execution window.
    $creature = Read-0111 $creaturePath
    $damageOld = @'
    damage_instance d = dam; // copy, since we will mutate in absorb_hit
    double multiplier = 1.0;
    if( source != nullptr && source->is_avatar() ) {
        const double bonus = ncmm::runtime_hook_modifier_for_creatures(
                                 "combat.damage_to_species_pct", source, this );
        multiplier *= std::max( 0.10, 1.0 + bonus / 100.0 );
    }
    if( is_avatar() ) {
'@
    $damageNew = @'
    damage_instance d = dam; // copy, since we will mutate in absorb_hit
    double multiplier = 1.0;
    if( source != nullptr && source->is_avatar() && source != this ) {
        const double bonus = ncmm::runtime_hook_modifier_for_creatures(
                                 "combat.damage_to_species_pct", source, this );
        multiplier *= std::max( 0.10, 1.0 + bonus / 100.0 );
        const double general_bonus = ncmm::runtime_hook_modifier( "combat.damage_dealt_pct" );
        multiplier *= std::max( 0.10, 1.0 + general_bonus / 100.0 );
        const double execute_threshold = std::max( 0.0, std::min( 40.0,
                ncmm::runtime_hook_modifier( "combat.execute_threshold_pct" ) ) );
        if( execute_threshold > 0.0 && hp_percentage() <= execute_threshold ) {
            const double execute_bonus = std::max( 0.0, std::min( 200.0,
                    ncmm::runtime_hook_modifier( "combat.execute_damage_pct" ) ) );
            multiplier *= 1.0 + execute_bonus / 100.0;
        }
    }
    if( is_avatar() ) {
'@
    $creature = Replace-0111 $creature $damageOld $damageNew 'creature.execute-and-momentum-damage'
    Write-0111 $creaturePath $creature

    # Player-attributed kill: immediate recovery hooks + generic API2 event for module-owned momentum state.
    $monster = Read-0111 $monsterPath
    if(-not $monster.Contains('#include "ncmm_loader.h"')) {
        $monster = Replace-0111 $monster '#include "monster.h"' ('#include "monster.h"' + "`n" + '#include "ncmm_loader.h"') 'monster.include-ncmm'
    }
    $loaderHeader0111 = Read-0111 $loaderHeaderPath0111
    $loaderSource0111 = Read-0111 $loaderSourcePath0111
    if(-not $loaderHeader0111.Contains('void runtime_player_kill_notify();')) {
        $loaderHeader0111 = Replace-0111 $loaderHeader0111 'void runtime_event_notify( uint32_t event_id );' ('void runtime_event_notify( uint32_t event_id );' + "`n" + 'void runtime_player_kill_notify();') 'loader.named-player-kill-decl'
        Write-0111 $loaderHeaderPath0111 $loaderHeader0111
    }
    if(-not $loaderSource0111.Contains('void runtime_player_kill_notify()')) {
        $runtimeEventImpl0111 = @'
void runtime_event_notify( uint32_t event_id )
{
    dispatch_event_v2( event_id );
}
'@
        $runtimeEventImplNew0111 = @'
void runtime_event_notify( uint32_t event_id )
{
    dispatch_event_v2( event_id );
}

void runtime_player_kill_notify()
{
    dispatch_event_v2( NCMM_EVENT_PLAYER_KILL_V2 );
}
'@
        $loaderSource0111 = Replace-0111 $loaderSource0111 $runtimeEventImpl0111 $runtimeEventImplNew0111 'loader.named-player-kill-impl'
        Write-0111 $loaderSourcePath0111 $loaderSource0111
    }
    $killOld = @'
            get_event_bus().send_with_talker( ch, this, e );
'@
    $killNew = @'
            get_event_bus().send_with_talker( ch, this, e );
            // Only XP-awarding hostile combat kills fuel reactive perks or Momentum.
            // Fleeing enemies still count; friendly/passive/neutral kills do not.
            const monster_attitude ncmm_kill_attitude = attitude( ch );
            if( ch->is_avatar() && kill_xp > 0 &&
                ( ncmm_kill_attitude == MATT_ATTACK || ncmm_kill_attitude == MATT_FLEE ) ) {
                const int ncmm_kill_moves = std::max( 0, std::min( 75, static_cast<int>(
                                                ncmm::runtime_hook_modifier( "combat.on_kill_moves" ) ) ) );
                if( ncmm_kill_moves > 0 ) ch->mod_moves( ncmm_kill_moves );
                const double ncmm_kill_stamina_pct = std::max( 0.0, std::min( 15.0,
                        ncmm::runtime_hook_modifier( "combat.on_kill_stamina_pct" ) ) );
                if( ncmm_kill_stamina_pct > 0.0 ) {
                    ch->mod_stamina( static_cast<int>( ch->get_stamina_max() * ncmm_kill_stamina_pct / 100.0 ) );
                }
                ncmm::runtime_player_kill_notify();
            }
'@
    $monster = Replace-0111 $monster $killOld $killNew 'monster.player-kill-event'
    Write-0111 $monsterPath $monster

    # Crafting: real outcome control, not just speed bonuses.
    $crafting = Read-0111 $craftingPath
    if(-not $crafting.Contains('#include "ncmm_loader.h"')) {
        $crafting = Replace-0111 $crafting '#include "crafting.h"' ('#include "crafting.h"' + "`n" + '#include "ncmm_loader.h"') 'crafting.include-ncmm'
    }
    $craftRollOld = @'
float Character::crafting_success_roll( const recipe &making ) const
{
    craft_roll_data data = recipe_success_roll_data( making );
    float craft_roll = std::max( normal_roll( data.center, data.stddev ), 0.0 );

    add_msg_debug( debugmode::DF_CHARACTER, "Crafting skill roll: %f, final difficulty %g", craft_roll,
'@
    $craftRollNew = @'
float Character::crafting_success_roll( const recipe &making ) const
{
    craft_roll_data data = recipe_success_roll_data( making );
    float craft_roll = std::max( normal_roll( data.center, data.stddev ), 0.0 );
    if( is_avatar() ) {
        craft_roll += std::max( 0.0, std::min( 2.0,
                                   ncmm::runtime_hook_modifier( "crafting.success_roll_flat" ) ) );
    }

    add_msg_debug( debugmode::DF_CHARACTER, "Crafting skill roll: %f, final difficulty %g", craft_roll,
'@
    $crafting = Replace-0111 $crafting $craftRollOld $craftRollNew 'crafting.success-roll-hook'
    $recipeChanceOld = @'
float Character::recipe_success_chance( const recipe &making ) const
{
    // We calculate the failure chance of a recipe by performing a normal roll with a given
    // standard deviation and center, then subtracting a "final difficulty" score from that.
    // If that result is above 1, there is no chance of failure.
    craft_roll_data data = recipe_success_roll_data( making );

    return normal_roll_chance( data.center, data.stddev, 1.f + data.final_difficulty );
}
'@
    $recipeChanceNew = @'
float Character::recipe_success_chance( const recipe &making ) const
{
    // Keep the displayed estimate aligned with the actual NCMM-adjusted success roll.
    craft_roll_data data = recipe_success_roll_data( making );
    const double ncmm_success_bonus = is_avatar() ? std::max( 0.0, std::min( 2.0,
            ncmm::runtime_hook_modifier( "crafting.success_roll_flat" ) ) ) : 0.0;

    return normal_roll_chance( data.center, data.stddev,
                               1.f + data.final_difficulty - static_cast<float>( ncmm_success_bonus ) );
}
'@
    $crafting = Replace-0111 $crafting $recipeChanceOld $recipeChanceNew 'crafting.success-chance-ui-hook'
    $failureOpenOld = @'
    if( !is_craft() ) {
        debugmsg( "handle_craft_failure() called on non-craft '%s.'  Aborting.", tname() );
        return false;
    }

    // The completed result will have a crafting defect (if any are defined).
'@
    $failureOpenNew = @'
    if( !is_craft() ) {
        debugmsg( "handle_craft_failure() called on non-craft '%s.'  Aborting.", tname() );
        return false;
    }
    if( crafter.is_avatar() ) {
        const double ncmm_failure_save = std::max( 0.0, std::min( 35.0,
                ncmm::runtime_hook_modifier( "crafting.failure_save_pct" ) ) );
        if( ncmm_failure_save > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_failure_save ) {
            set_next_failure_point( crafter );
            return false;
        }
    }

    // The completed result will have a crafting defect (if any are defined).
'@
    $crafting = Replace-0111 $crafting $failureOpenOld $failureOpenNew 'crafting.failure-save-hook'
    $successRollOld = '    const double success_roll = crafter.crafting_failure_roll( get_making() );'
    $successRollNew = @'
    const double success_roll = crafter.crafting_failure_roll( get_making() );
    const double ncmm_component_protection = crafter.is_avatar() ? std::max( 0.0, std::min( 75.0,
            ncmm::runtime_hook_modifier( "crafting.component_loss_reduction_pct" ) ) ) : 0.0;
'@
    $crafting = Replace-0111 $crafting $successRollOld $successRollNew 'crafting.component-protection-setup'
    $destroyCheckOld = @'
        // If we roll success, skip destroying a component
        if( x_in_y( success_roll, 1.0 ) ) {
            continue;
        }
'@
    $destroyCheckNew = @'
        // If we roll success, or NCMM technical discipline protects this component, skip destruction.
        if( x_in_y( success_roll, 1.0 ) ||
            ( ncmm_component_protection > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_component_protection ) ) {
            continue;
        }
'@
    $crafting = Replace-0111 $crafting $destroyCheckOld $destroyCheckNew 'crafting.component-loss-hook'
    $progressOld = '    const double percent_progress_loss = rng_exponential( 0.25, 0.35 ) * ( 1.0 - success_roll );'
    $progressNew = @'
    double percent_progress_loss = rng_exponential( 0.25, 0.35 ) * ( 1.0 - success_roll );
    if( crafter.is_avatar() ) {
        const double ncmm_progress_protection = std::max( 0.0, std::min( 80.0,
                ncmm::runtime_hook_modifier( "crafting.progress_loss_reduction_pct" ) ) );
        percent_progress_loss *= std::max( 0.0, 1.0 - ncmm_progress_protection / 100.0 );
    }
'@
    $crafting = Replace-0111 $crafting $progressOld $progressNew 'crafting.progress-loss-hook'
    Write-0111 $craftingPath $crafting

    # Scavenging technical surface: lockpicking and hazard recognition.
    $activity = Read-0111 $activityActorPath
    if(-not $activity.Contains('#include "ncmm_loader.h"')) {
        $activity = Replace-0111 $activity '#include "activity_actor_definitions.h"' ('#include "activity_actor_definitions.h"' + "`n" + '#include "ncmm_loader.h"') 'activity-actor.include-ncmm'
    }
    $lockMovesOld = @'
    if( lockpick->has_flag( flag_PERFECT_LOCKPICK ) ) {
        return to_moves<int>( 5_seconds );
    } else {
        /** @EFFECT_DEX speeds up door lock picking */
        return to_moves<int>(
                   std::max( 30_seconds,
                             ( 10_minutes - time_duration::from_minutes( qual + static_cast<float>( who.get_dex() ) / 4.0f +
                                     weighted_skill_average ) ) * duration_proficiency_factor ) );
    }
'@
    $lockMovesNew = @'
    const bool ncmm_perfect_lockpick = lockpick != nullptr && lockpick->has_flag( flag_PERFECT_LOCKPICK );
    int ncmm_lock_moves = 0;
    if( ncmm_perfect_lockpick ) {
        ncmm_lock_moves = to_moves<int>( 5_seconds );
    } else {
        /** @EFFECT_DEX speeds up door lock picking */
        ncmm_lock_moves = to_moves<int>(
                              std::max( 30_seconds,
                                        ( 10_minutes - time_duration::from_minutes( qual + static_cast<float>( who.get_dex() ) / 4.0f +
                                                weighted_skill_average ) ) * duration_proficiency_factor ) );
    }
    if( who.is_avatar() ) {
        const double ncmm_time_reduction = std::max( 0.0, std::min( 60.0,
                ncmm::runtime_hook_modifier( "scavenging.lockpick_time_pct" ) ) );
        const int ncmm_lock_floor = ncmm_perfect_lockpick ?
                                    to_moves<int>( 5_seconds ) : to_moves<int>( 30_seconds );
        ncmm_lock_moves = std::max( ncmm_lock_floor,
                                   static_cast<int>( ncmm_lock_moves * ( 1.0 - ncmm_time_reduction / 100.0 ) ) );
    }
    return ncmm_lock_moves;
'@
    $activity = Replace-0111 $activity $lockMovesOld $lockMovesNew 'lockpick.time-hook'
    $meanRollOld = @'
    const float mean_roll = weighted_skill_average + ( weighted_stat_average / 4 ) + proficiency_effect
                            + tool_effect;
'@
    $meanRollNew = @'
    float mean_roll = weighted_skill_average + ( weighted_stat_average / 4 ) + proficiency_effect
                      + tool_effect;
    if( who.is_avatar() ) {
        mean_roll += static_cast<float>( std::max( 0.0, std::min( 5.0,
                ncmm::runtime_hook_modifier( "scavenging.lockpick_roll_flat" ) ) ) );
    }
'@
    $activity = Replace-0111 $activity $meanRollOld $meanRollNew 'lockpick.roll-hook'
    $damageToolOld = @'
    } else if( lock_roll > ( 1.5 * pick_roll ) ) {
        if( it->inc_damage() ) {
'@
    $damageToolNew = @'
    } else if( lock_roll > ( 1.5 * pick_roll ) ) {
        const double ncmm_tool_protection = who.is_avatar() ? std::max( 0.0, std::min( 90.0,
                ncmm::runtime_hook_modifier( "scavenging.lockpick_tool_protection_pct" ) ) ) : 0.0;
        if( ncmm_tool_protection > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_tool_protection ) {
            who.add_msg_if_player( m_bad, _( "The lock stumps your efforts to pick it." ) );
        } else if( it->inc_damage() ) {
'@
    $activity = Replace-0111 $activity $damageToolOld $damageToolNew 'lockpick.tool-protection-hook'
    $alarmOld = @'
    if( !perfect && ter_type == ter_t_door_locked_alarm && ( lock_roll + dice( 1, 30 ) ) > pick_roll ) {
        sounds::sound( who.pos_bub(), 40, sounds::sound_t::alarm, _( "an alarm sound!" ), true,
'@
    $alarmNew = @'
    const double ncmm_alarm_avoid = who.is_avatar() ? std::max( 0.0, std::min( 75.0,
            ncmm::runtime_hook_modifier( "scavenging.lockpick_alarm_avoid_pct" ) ) ) : 0.0;
    if( !perfect && ter_type == ter_t_door_locked_alarm && ( lock_roll + dice( 1, 30 ) ) > pick_roll &&
        !( ncmm_alarm_avoid > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_alarm_avoid ) ) {
        sounds::sound( who.pos_bub(), 40, sounds::sound_t::alarm, _( "an alarm sound!" ), true,
'@
    $activity = Replace-0111 $activity $alarmOld $alarmNew 'lockpick.alarm-hook'
    Write-0111 $activityActorPath $activity

    $trap = Read-0111 $trapPath
    if(-not $trap.Contains('#include "ncmm_loader.h"')) {
        $trap = Replace-0111 $trap '#include "trap.h"' ('#include "trap.h"' + "`n" + '#include "ncmm_loader.h"' + "`n" + '#include <algorithm>') 'trap.include-ncmm'
    }
    $trapRollOld = @'
    const float mean_roll = weighted_stat_average + ( traps_skill_level / 3.0f ) +
                            proficiency_effect - distance_penalty - sleepiness_penalty - encumbrance_penalty;

    const int roll = std::round( normal_roll( mean_roll, 3 ) );
'@
    $trapRollNew = @'
    float mean_roll = weighted_stat_average + ( traps_skill_level / 3.0f ) +
                      proficiency_effect - distance_penalty - sleepiness_penalty - encumbrance_penalty;
    if( p.is_avatar() ) {
        mean_roll += static_cast<float>( std::max( 0.0, std::min( 6.0,
                ncmm::runtime_hook_modifier( "scavenging.trap_detection_flat" ) ) ) );
    }

    const int roll = std::round( normal_roll( mean_roll, 3 ) );
'@
    $trap = Replace-0111 $trap $trapRollOld $trapRollNew 'trap.detection-hook'
    Write-0111 $trapPath $trap

    Set-Content -Path $marker -Value "Host API 2.0 reactive/technical mechanics v3 / Survivor 0.11.2 edge-hardened surface`n" -Encoding ASCII
    Write-Host "Host API 2.0 reactive/technical mechanics hooks: APPLIED (0.11.2 edge polish)" -ForegroundColor Green
}

function Apply-NcmmReactiveMechanics0113([string]$Root) {
    Write-Host "Applying Survivor 0.11.3 combinatorial edge refinements..." -ForegroundColor Cyan
    $src = Join-Path $Root "src"
    $characterPath = Join-Path $src "character.cpp"
    $meleePath = Join-Path $src "melee.cpp"
    $creaturePath = Join-Path $src "creature.cpp"
    $monsterPath = Join-Path $src "monster.cpp"
    $npcPath = Join-Path $src "npc.cpp"
    $craftingPath = Join-Path $src "crafting.cpp"
    $marker = Join-Path $Root ".ncmm_reactive_mechanics_0113"
    foreach($p in @($characterPath,$meleePath,$creaturePath,$monsterPath,$npcPath,$craftingPath)) {
        if(-not (Test-Path $p -PathType Leaf)) { throw "0.11.3 combinatorial source file missing: $p" }
    }
    $Utf8NoBom0113 = New-Object System.Text.UTF8Encoding($false)
    function Read-0113([string]$Path) { return Normalize-Lf ([IO.File]::ReadAllText($Path)) }
    function Write-0113([string]$Path,[string]$Text) { [IO.File]::WriteAllText($Path,(Normalize-Lf $Text),$Utf8NoBom0113) }
    function Replace-0113([string]$Text,[string]$Old,[string]$New,[string]$Name) {
        $Text=Normalize-Lf $Text; $Old=(Normalize-Lf $Old).TrimEnd(); $New=(Normalize-Lf $New).TrimEnd()
        $count=([regex]::Matches($Text,[regex]::Escape($Old))).Count
        if($count -ne 1) { throw "0.11.3 contract '$Name' expected exactly once, found $count" }
        return $Text.Replace($Old,$New)
    }

    $probes = @(
        @{Path=$characterPath;Needle='const int ncmm_refund_basis = std::max( 0, attack_speed( ncmm_riposte_item ) );'},
        @{Path=$characterPath;Needle='!is_mounted() &&'},
        @{Path=$meleePath;Needle='ncmm_hostile_target_before'},
        @{Path=$creaturePath;Needle='ncmm_hostile_reactive_target'},
        @{Path=$npcPath;Needle='ncmm_hostile_avatar_kill_before_death'},
        @{Path=$craftingPath;Needle='craft_data_->next_failure_point <= item_counter'},
        @{Path=$craftingPath;Needle='ncmm_catastrophic_save'}
    )
    $already=$true
    foreach($probe in $probes) { if(-not (Read-0113 $probe.Path).Contains($probe.Needle)) { $already=$false; break } }
    if($already) {
        if(-not (Test-Path $marker -PathType Leaf)) { Set-Content -Path $marker -Value "Recovered Survivor 0.11.3 combinatorial edge marker`n" -Encoding ASCII }
        Write-Host "Survivor 0.11.3 combinatorial CDDA hooks already present." -ForegroundColor Green
        return
    }
    Remove-Item $marker -Force -ErrorAction SilentlyContinue

    # Riposte refund is based on pre-attack base melee cost, not net moves after nested crit/kill rewards.
    $character = Read-0113 $characterPath
    if(-not $character.Contains('#include "item.h"')) {
        $character = Replace-0113 $character '#include "item_location.h"' ('#include "item.h"' + "`n" + '#include "item_location.h"') 'character.explicit-item-include'
    }
    $riposteOld0113 = @'
        if( !ncmm_vanilla_counter_fired && !ncmm_riposte_in_progress && source != nullptr &&
            square_dist( pos_bub(), source->pos_bub() ) == 1 && !source->is_dead_state() &&
            !source->is_hallucination() &&
            source->attitude_to( *this ) == Creature::Attitude::HOSTILE &&
            !is_dead_state() && get_stamina() >= get_stamina_max() / 3 &&
            ncmm_riposte_chance > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_riposte_chance ) {
            ncmm_riposte_in_progress = true;
            const int ncmm_moves_before = get_moves();
            melee_attack( *source, false );
            const int ncmm_spent = std::max( 0, ncmm_moves_before - get_moves() );
            const double ncmm_refund_pct = std::max( 0.0, std::min( 100.0,
                    ncmm::runtime_hook_modifier( "combat.riposte_refund_pct" ) ) );
            if( ncmm_spent > 0 && ncmm_refund_pct > 0.0 ) {
                mod_moves( static_cast<int>( ncmm_spent * ncmm_refund_pct / 100.0 ) );
            }
            ncmm_riposte_in_progress = false;
        }
'@
    $riposteNew0113 = @'
        if( !ncmm_vanilla_counter_fired && !ncmm_riposte_in_progress && source != nullptr &&
            square_dist( pos_bub(), source->pos_bub() ) == 1 && !source->is_dead_state() &&
            !source->is_hallucination() && !is_mounted() &&
            source->attitude_to( *this ) == Creature::Attitude::HOSTILE &&
            !is_dead_state() && get_stamina() >= get_stamina_max() / 3 &&
            ncmm_riposte_chance > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_riposte_chance ) {
            const item_location ncmm_riposte_weapon = used_weapon();
            const item &ncmm_riposte_item = ncmm_riposte_weapon ? *ncmm_riposte_weapon : null_item_reference();
            const int ncmm_refund_basis = std::max( 0, attack_speed( ncmm_riposte_item ) );
            const double ncmm_refund_pct = std::max( 0.0, std::min( 100.0,
                    ncmm::runtime_hook_modifier( "combat.riposte_refund_pct" ) ) );
            ncmm_riposte_in_progress = true;
            const bool ncmm_riposte_executed = melee_attack( *source, false );
            ncmm_riposte_in_progress = false;
            if( ncmm_riposte_executed && ncmm_refund_basis > 0 && ncmm_refund_pct > 0.0 ) {
                mod_moves( static_cast<int>( std::lround( ncmm_refund_basis * ncmm_refund_pct / 100.0 ) ) );
            }
        }
'@
    $character = Replace-0113 $character $riposteOld0113 $riposteNew0113 'character.riposte-refund-isolation'
    Write-0113 $characterPath $character

    # Snapshot hostility before damage so crit rewards cannot be farmed by striking neutral/friendly targets.
    $melee = Read-0113 $meleePath
    $hostilityAnchor0113 = '    const bool hits = hit_spread >= 0;'
    $hostilityNew0113 = @'
    const bool hits = hit_spread >= 0;
    bool ncmm_hostile_target_before = false;
    if( is_avatar() ) {
        ncmm_hostile_target_before = t.attitude_to( *this ) == Creature::Attitude::HOSTILE;
        if( const monster *ncmm_target_mon = t.as_monster() ) {
            const monster_attitude ncmm_target_mood = ncmm_target_mon->attitude( this );
            ncmm_hostile_target_before = ncmm_hostile_target_before || ncmm_target_mood == MATT_FLEE;
        }
    }
'@
    $melee = Replace-0113 $melee $hostilityAnchor0113 $hostilityNew0113 'melee.pre-hit-hostility-snapshot'
    $melee = Replace-0113 $melee 'if( is_avatar() && dam > 0 && !t.is_hallucination() ) {' 'if( is_avatar() && ncmm_hostile_target_before && dam > 0 && !t.is_hallucination() ) {' 'melee.hostile-crit-reward'
    Write-0113 $meleePath $melee

    # Momentum/general reactive damage and Execute are combat rewards: never amplify friendly/neutral damage.
    $creature = Read-0113 $creaturePath
    $damageOld0113 = @'
        const double general_bonus = ncmm::runtime_hook_modifier( "combat.damage_dealt_pct" );
        multiplier *= std::max( 0.10, 1.0 + general_bonus / 100.0 );
        const double execute_threshold = std::max( 0.0, std::min( 40.0,
                ncmm::runtime_hook_modifier( "combat.execute_threshold_pct" ) ) );
        if( execute_threshold > 0.0 && hp_percentage() <= execute_threshold ) {
            const double execute_bonus = std::max( 0.0, std::min( 200.0,
                    ncmm::runtime_hook_modifier( "combat.execute_damage_pct" ) ) );
            multiplier *= 1.0 + execute_bonus / 100.0;
        }
'@
    $damageNew0113 = @'
        bool ncmm_hostile_reactive_target = attitude_to( *source ) == Creature::Attitude::HOSTILE;
        if( monster *ncmm_target_mon = as_monster() ) {
            if( Character *ncmm_source_character = source->as_character() ) {
                ncmm_hostile_reactive_target = ncmm_hostile_reactive_target ||
                                               ncmm_target_mon->attitude( ncmm_source_character ) == MATT_FLEE;
            }
        }
        if( ncmm_hostile_reactive_target ) {
            const double general_bonus = ncmm::runtime_hook_modifier( "combat.damage_dealt_pct" );
            multiplier *= std::max( 0.10, 1.0 + general_bonus / 100.0 );
            const double execute_threshold = std::max( 0.0, std::min( 40.0,
                    ncmm::runtime_hook_modifier( "combat.execute_threshold_pct" ) ) );
            if( execute_threshold > 0.0 && hp_percentage() <= execute_threshold ) {
                const double execute_bonus = std::max( 0.0, std::min( 200.0,
                        ncmm::runtime_hook_modifier( "combat.execute_damage_pct" ) ) );
                multiplier *= 1.0 + execute_bonus / 100.0;
            }
        }
'@
    $creature = Replace-0113 $creature $damageOld0113 $damageNew0113 'creature.hostile-reactive-damage'
    Write-0113 $creaturePath $creature

    # Hostile human NPC kills are real combat kills too. Snapshot hostility before npc::die()
    # detaches faction state, then emit the same generic player-kill event after actual death.
    $npc = Read-0113 $npcPath
    if(-not $npc.Contains('#include "ncmm_loader.h"')) {
        $npc = Replace-0113 $npc '#include "npc.h"' ('#include "npc.h"' + "`n" + '#include "ncmm_loader.h"') 'npc.include-ncmm'
    }
    $npcDeathOpenOld0113 = @'
void npc::die( map *here, Creature *nkiller )
{
    if( dead ) {
        // We are already dead, don't die again, note that npc::dead is
        // *only* set to true in this function!
        return;
    }
    prevent_death_reminder = false;
'@
    $npcDeathOpenNew0113 = @'
void npc::die( map *here, Creature *nkiller )
{
    if( dead ) {
        // We are already dead, don't die again, note that npc::dead is
        // *only* set to true in this function!
        return;
    }
    const bool ncmm_hostile_avatar_kill_before_death = nkiller != nullptr && nkiller->is_avatar() &&
            !is_hallucination() && !is_fake() && guaranteed_hostile();
    prevent_death_reminder = false;
'@
    $npc = Replace-0113 $npc $npcDeathOpenOld0113 $npcDeathOpenNew0113 'npc.pre-death-hostility-snapshot'
    $npcKillOld0113 = @'
    if( Character *ch = dynamic_cast<Character *>( killer ) ) {
        get_event_bus().send<event_type::character_kills_character>( ch->getID(), getID(), get_name(),
                myclass.c_str() );
    }
    Character &player_character = get_player_character();
'@
    $npcKillNew0113 = @'
    if( Character *ch = dynamic_cast<Character *>( killer ) ) {
        get_event_bus().send<event_type::character_kills_character>( ch->getID(), getID(), get_name(),
                myclass.c_str() );
        if( ncmm_hostile_avatar_kill_before_death && ch->is_avatar() ) {
            const int ncmm_kill_moves = std::max( 0, std::min( 75, static_cast<int>(
                                            ncmm::runtime_hook_modifier( "combat.on_kill_moves" ) ) ) );
            if( ncmm_kill_moves > 0 ) ch->mod_moves( ncmm_kill_moves );
            const double ncmm_kill_stamina_pct = std::max( 0.0, std::min( 15.0,
                    ncmm::runtime_hook_modifier( "combat.on_kill_stamina_pct" ) ) );
            if( ncmm_kill_stamina_pct > 0.0 ) {
                ch->mod_stamina( static_cast<int>( ch->get_stamina_max() * ncmm_kill_stamina_pct / 100.0 ) );
            }
            ncmm::runtime_player_kill_notify();
        }
    }
    Character &player_character = get_player_character();
'@
    $npc = Replace-0113 $npc $npcKillOld0113 $npcKillNew0113 'npc.hostile-player-kill-event'
    Write-0113 $npcPath $npc

    # A cancelled craft failure must advance its failure point monotonically, even after a zero roll.
    # Also keep the catastrophic-failure UI estimate aware of cancellation perks.
    $crafting = Read-0113 $craftingPath
    $failureSaveOld0113 = @'
        if( ncmm_failure_save > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_failure_save ) {
            set_next_failure_point( crafter );
            return false;
        }
'@
    $failureSaveNew0113 = @'
        if( ncmm_failure_save > 0.0 && rng_float( 0.0, 100.0 ) < ncmm_failure_save ) {
            set_next_failure_point( crafter );
            if( craft_data_->next_failure_point <= item_counter ) {
                craft_data_->next_failure_point = std::min( 10000000, item_counter + 1 );
            }
            return false;
        }
'@
    $crafting = Replace-0113 $crafting $failureSaveOld0113 $failureSaveNew0113 'crafting.failure-save-monotonicity'
    $destructionOld0113 = @'
float Character::item_destruction_chance( const recipe &making ) const
{
    // If a normal roll with these parameters rolls over 1, we will not have a catastrophic failure
    // If we roll under one, we will
    craft_roll_data data = recipe_failure_roll_data( making );

    // normal_roll_chance returns the chance that we roll over, we want the chance we roll under
    return 1.f - normal_roll_chance( data.center, data.stddev, 1.f + data.final_difficulty );
}
'@
    $destructionNew0113 = @'
float Character::item_destruction_chance( const recipe &making ) const
{
    // Keep the displayed catastrophic-failure estimate aware of NCMM failure cancellation.
    // Component protection can reduce real losses further, so this remains a conservative estimate.
    craft_roll_data data = recipe_failure_roll_data( making );
    float chance = 1.f - normal_roll_chance( data.center, data.stddev, 1.f + data.final_difficulty );
    if( is_avatar() ) {
        const double ncmm_catastrophic_save = std::max( 0.0, std::min( 35.0,
                ncmm::runtime_hook_modifier( "crafting.failure_save_pct" ) ) );
        chance *= static_cast<float>( std::max( 0.0, 1.0 - ncmm_catastrophic_save / 100.0 ) );
    }
    return clamp( chance, 0.0f, 1.0f );
}
'@
    $crafting = Replace-0113 $crafting $destructionOld0113 $destructionNew0113 'crafting.catastrophic-ui-alignment'
    Write-0113 $craftingPath $crafting

    Set-Content -Path $marker -Value "Survivor 0.11.3 combinatorial edge hooks / isolated riposte refund + hostile reactive gates + monotonic craft save`n" -Encoding ASCII
    Write-Host "Survivor 0.11.3 combinatorial CDDA hook polish: APPLIED" -ForegroundColor Green
}

function Assert-NcmmReactiveMechanics0113Source([string]$Root) {
    $src = Join-Path $Root "src"
    $checks = @(
        @{ Path=(Join-Path $src 'character.cpp'); Needles=@(
            'ncmm_vanilla_counter_fired = melee_attack( *source, false, tec );',
            '!source->is_hallucination() && !is_mounted() &&',
            'const int ncmm_refund_basis = std::max( 0, attack_speed( ncmm_riposte_item ) );',
            'const bool ncmm_riposte_executed = melee_attack( *source, false );'
        ) },
        @{ Path=(Join-Path $src 'melee.cpp'); Needles=@(
            'ncmm_hostile_target_before',
            'is_avatar() && ncmm_hostile_target_before && dam > 0 && !t.is_hallucination()'
        ) },
        @{ Path=(Join-Path $src 'creature.cpp'); Needles=@(
            'ncmm_hostile_reactive_target',
            'runtime_hook_modifier( "combat.execute_threshold_pct" )'
        ) },
        @{ Path=(Join-Path $src 'npc.cpp'); Needles=@(
            'ncmm_hostile_avatar_kill_before_death',
            'ncmm::runtime_player_kill_notify();'
        ) },
        @{ Path=(Join-Path $src 'crafting.cpp'); Needles=@(
            'craft_data_->next_failure_point <= item_counter',
            'ncmm_catastrophic_save',
            'chance *= static_cast<float>( std::max( 0.0, 1.0 - ncmm_catastrophic_save / 100.0 ) );'
        ) }
    )
    foreach($check0113 in $checks) {
        if(-not (Test-Path $check0113.Path -PathType Leaf)) {
            throw "Survivor 0.11.3 engine audit missing file: $($check0113.Path)"
        }
        $body0113 = Normalize-Lf ([IO.File]::ReadAllText($check0113.Path))
        foreach($needle0113 in $check0113.Needles) {
            if(-not $body0113.Contains($needle0113)) {
                throw "Survivor 0.11.3 engine audit missing: $needle0113"
            }
        }
    }
    Write-Host "Survivor 0.11.3 combinatorial CDDA source audit: PASS" -ForegroundColor Green
}

function Apply-WorldSettingsV2Patch([string]$Root) {
    Write-Host "Applying World Settings API v2 geography hooks..." -ForegroundColor Cyan
    $src = Join-Path $Root "src"
    $optionsH = Join-Path $src "options.h"
    $optionsCpp = Join-Path $src "options.cpp"
    $overmapPath = Join-Path $src "overmap.cpp"
    $overmapHPath = Join-Path $src "overmap.h"
    $cityPath = Join-Path $src "overmap_city.cpp"
    $waterPath = Join-Path $src "overmap_water.cpp"
    $highwayPath = Join-Path $src "overmap_highway.cpp"
    $worldFactoryPath = Join-Path $src "worldfactory.cpp"
    $marker = Join-Path $Root ".ncmm_world_settings_v2_patched"
    foreach ($p in @($optionsH,$optionsCpp,$overmapPath,$overmapHPath,$cityPath,$waterPath,$highwayPath,$worldFactoryPath)) {
        if (-not (Test-Path $p -PathType Leaf)) { throw "World Settings v2 required source file missing: $p" }
    }

    function Read-WS([string]$Path) { return Normalize-Lf ([IO.File]::ReadAllText($Path)) }
    function Write-WS([string]$Path,[string]$Body) { Write-Utf8NoBom $Path (Normalize-Lf $Body) }
    function Replace-WSOnce([string]$Body,[string]$Old,[string]$New,[string]$Name) {
        return Replace-TextBlock $Body $Old $New ("World Settings v2 " + $Name)
    }
    function Replace-WSRange([string]$Body,[string]$Start,[string]$End,[string]$New,[string]$Name) {
        return Replace-CppRange $Body $Start $End $New ("World Settings v2 " + $Name)
    }

    if (Test-Path $marker -PathType Leaf) {
        $checks = @{
            $optionsH = @('ncmm_register_world_bool','ncmm_register_world_int','ncmm_register_world_float','ncmm_register_world_enum')
            $optionsCpp = @('options_manager::ncmm_register_world_bool','options_manager::ncmm_register_world_enum','options_manager::ncmm_ensure_experimental_page','ncmm_world_scoped_page( iCurrentPage )','ncmm::on_language_changed();','ncmm_deferred_option_values','name.rfind( "NCMM_", 0 ) == 0')
            $worldFactoryPath = @('opts.get_option( name ).getPage() == "ncmm_experimental"')
            $cityPath = @('NCMM_AWS_CUSTOM_GEOGRAPHY','NCMM_AWS_CITY_SIZE','NCMM_AWS_CITY_SPACING','NCMM_AWS_MAX_URBANITY','NCMM_AWS_MEGACITY','NCMM_AWS_SHOP_RADIUS','NCMM_AWS_PARK_RADIUS')
            $overmapPath = @('NCMM_AWS_ENABLE_FORESTS','ncmm_disable_forests','NCMM_AWS_ENABLE_SWAMPS','NCMM_AWS_RAVINE_COUNT','NCMM_AWS_TRAIL_CHANCE','NCMM_AWS_PLACE_SPECIALS')
            $overmapHPath = @('void set_options( int row_override = -1')
            $waterPath = @('NCMM_AWS_RIVER_FREQUENCY','NCMM_AWS_LAKE_THRESHOLD','NCMM_AWS_OCEAN_THRESHOLD')
            $highwayPath = @('NCMM_AWS_HIGHWAY_GRID_ROW','NCMM_AWS_HIGHWAY_STRAIGHTNESS','ncmm_custom_grid','ncmm_lakes_enabled','ncmm_oceans_disabled')
        }
        foreach ($entry in $checks.GetEnumerator()) {
            $body = Read-WS $entry.Key
            foreach ($needle in $entry.Value) {
                if (-not $body.Contains($needle)) { throw "World Settings v2 marker exists but hook is incomplete: $needle" }
            }
        }
        Write-Host "World Settings API v2 geography hooks already present and verified." -ForegroundColor Green
        return
    }

    # options_manager: typed synthetic per-world settings. They live on ncmm_experimental,
    # are copied into WORLD_OPTIONS, and persist through the same world-save path as world_default.
    $h = Read-WS $optionsH
    $declAnchor = @'
        bool ncmm_set_worldgen_string_choices( const std::string &name,
                const std::vector<id_and_option> &items );
'@
    $declNew = @'
        bool ncmm_set_worldgen_string_choices( const std::string &name,
                const std::vector<id_and_option> &items );

        /** NCMM World Settings API v2: create/update typed synthetic per-world settings. */
        bool ncmm_register_world_bool( const std::string &name, const translation &menu_text,
                                       const translation &tooltip, bool default_value );
        bool ncmm_register_world_int( const std::string &name, const translation &menu_text,
                                      const translation &tooltip, int min_value, int max_value,
                                      int default_value );
        bool ncmm_register_world_float( const std::string &name, const translation &menu_text,
                                        const translation &tooltip, float min_value, float max_value,
                                        float default_value, float step );
        bool ncmm_register_world_enum( const std::string &name, const translation &menu_text,
                                       const translation &tooltip,
                                       const std::vector<id_and_option> &items,
                                       const std::string &default_value );

        /** NCMM experimental per-world page used for non-vanilla world generation controls. */
        void ncmm_ensure_experimental_page();
        bool ncmm_begin_experimental_group( const std::string &group_id,
                                             const translation &name,
                                             const translation &tooltip );
'@
    $h = Replace-WSOnce $h $declAnchor $declNew 'options declarations'
    Write-WS $optionsH $h

    $c = Read-WS $optionsCpp

    # Infrastructure 0.8.3.1: NCMM modules register synthetic settings after options.json is loaded.
    # Persisted NCMM_* values therefore must be deferred instead of creating a placeholder VOID cOpt.
    # Otherwise a subsequent module registration sees an existing option with the wrong type/page and init fails.
    $deferredStateOld = @'
static const std::string blank_value( 1, 001 ); // because "" might be valid
'@
    $deferredStateNew = @'
static const std::string blank_value( 1, 001 ); // because "" might be valid

// NCMM runtime-owned options are registered after options.json is loaded. Keep their serialized
// values out-of-band until the owning code module declares the real typed option.
static std::unordered_map<std::string, std::string> ncmm_deferred_option_values;

static void ncmm_apply_deferred_option_value( const std::string &name, options_manager::cOpt &opt )
{
    const auto it = ncmm_deferred_option_values.find( name );
    if( it == ncmm_deferred_option_values.end() ) {
        return;
    }
    opt.setValue( it->second );
    ncmm_deferred_option_values.erase( it );
}
'@
    $c = Replace-WSOnce $c $deferredStateOld $deferredStateNew 'deferred NCMM option persistence state'

    $deserializeDeferredOld = @'
        add_retry( name, value );
        options[ name ].setValue( value );
'@
    $deserializeDeferredNew = @'
        // NCMM synthetic options do not exist yet: native code modules are initialized after
        // options_manager::load(). Defer only the NCMM namespace so vanilla unknown-option
        // behavior remains unchanged.
        if( options.find( name ) == options.end() && name.rfind( "NCMM_", 0 ) == 0 ) {
            ncmm_deferred_option_values[name] = value;
            continue;
        }

        add_retry( name, value );
        options[ name ].setValue( value );
'@
    $c = Replace-WSOnce $c $deserializeDeferredOld $deserializeDeferredNew 'defer unknown persisted NCMM options'

    # v8.7.5: changing USE_LANG inside the options screen must notify NCMM modules too.
    # The host already exposes ncmm::on_language_changed(), but options.cpp did not dispatch it.
    if (-not $c.Contains('#include "ncmm_loader.h"')) {
        $c = Replace-WSOnce $c '#include "mapsharing.h"' ('#include "mapsharing.h"' + "`n" + '#include "ncmm_loader.h"') 'options locale include'
    }
    $localeDispatchOld875 = @'
    if( lang_changed ) {
        update_global_locale();
        set_language_from_options();
    }
'@
    $localeDispatchNew875 = @'
    if( lang_changed ) {
        update_global_locale();
        set_language_from_options();
        ncmm::on_language_changed();
    }
'@
    if (-not $c.Contains('ncmm::on_language_changed();')) {
        $c = Replace-WSOnce $c $localeDispatchOld875 $localeDispatchNew875 'options locale dispatch'
    }

    # NCMM 0.7.3: synthetic non-vanilla settings live on a dedicated Experimental page
    # while retaining the same per-world serialization semantics as world_default.
    $experimentalPageOld = @'
    const int iWorldOptPage = std::find_if( pages_.begin(), pages_.end(), [&]( const Page & p ) {
        return p.id_ == "world_default";
    } ) - pages_.begin();
'@
    $experimentalPageNew = @'
    const int iWorldOptPage = std::find_if( pages_.begin(), pages_.end(), [&]( const Page & p ) {
        return p.id_ == "world_default";
    } ) - pages_.begin();
    const int iExperimentalPage = std::find_if( pages_.begin(), pages_.end(), [&]( const Page & p ) {
        return p.id_ == "ncmm_experimental";
    } ) - pages_.begin();
    const auto ncmm_world_scoped_page = [&]( int page ) {
        return page == iWorldOptPage || page == iExperimentalPage;
    };
'@
    $c = Replace-WSOnce $c $experimentalPageOld $experimentalPageNew 'experimental show page index'

    # CDDA 0546 contains this world-container selector twice in options_manager::show():
    # once in the redraw callback and once in the input loop.  Treat the pair as one
    # source contract instead of using Replace-ExactlyOnce on the first occurrence.
    $showContainerOld = @'
        options_manager::options_container &cOPTIONS = ( ingame || world_options_only ) &&
                iCurrentPage == iWorldOptPage ?
                ACTIVE_WORLD_OPTIONS : OPTIONS;
'@
    $showContainerNew = @'
        options_manager::options_container &cOPTIONS = ( ingame || world_options_only ) &&
                ncmm_world_scoped_page( iCurrentPage ) ?
                ACTIVE_WORLD_OPTIONS : OPTIONS;
'@
    $showContainerOldNorm = (Normalize-Lf $showContainerOld).TrimEnd()
    $showContainerNewNorm = (Normalize-Lf $showContainerNew).TrimEnd()
    $containerCountBefore = ([regex]::Matches($c,[regex]::Escape($showContainerOldNorm))).Count
    if ($containerCountBefore -ne 2) {
        throw "World Settings v2 experimental world container expected exactly twice before patch, found $containerCountBefore"
    }
    for ($containerPass = 1; $containerPass -le 2; $containerPass++) {
        $containerAt = $c.IndexOf($showContainerOldNorm,[StringComparison]::Ordinal)
        if ($containerAt -lt 0) {
            throw "World Settings v2 experimental world container pass $containerPass could not find the remaining selector"
        }
        $c = $c.Substring(0,$containerAt) + $showContainerNewNorm + $c.Substring($containerAt + $showContainerOldNorm.Length)
    }
    $containerOldAfter = ([regex]::Matches($c,[regex]::Escape($showContainerOldNorm))).Count
    $containerNewAfter = ([regex]::Matches($c,[regex]::Escape($showContainerNewNorm))).Count
    if ($containerOldAfter -ne 0 -or $containerNewAfter -ne 2) {
        throw "World Settings v2 experimental world container post-check failed: old=$containerOldAfter new=$containerNewAfter"
    }

    $c = $c.Replace(
        'is_hidden( world_options_only || ( ingame && iCurrentPage == iWorldOptPage ) )',
        'is_hidden( world_options_only || iCurrentPage == iExperimentalPage || ( ingame && iCurrentPage == iWorldOptPage ) )' )

    $experimentalWarningOld = @'
        if( ingame && iCurrentPage == iWorldOptPage ) {
            mvwprintz( w_options_tooltip, point( 3, 5 ), c_light_red, "%s", _( "Note: " ) );
            wprintz( w_options_tooltip, c_white, "%s",
                     _( "Some of these options may produce unexpected results if changed." ) );
        }
'@
    $experimentalWarningNew = @'
        if( iCurrentPage == iExperimentalPage ) {
            mvwprintz( w_options_tooltip, point( 3, 5 ), c_light_red, "%s", _( "Experimental: " ) );
            wprintz( w_options_tooltip, c_white, "%s",
                     _( "NCMM world-generation controls; changes affect only newly generated map areas unless stated otherwise." ) );
        } else if( ingame && iCurrentPage == iWorldOptPage ) {
            mvwprintz( w_options_tooltip, point( 3, 5 ), c_light_red, "%s", _( "Note: " ) );
            wprintz( w_options_tooltip, c_white, "%s",
                     _( "Some of these options may produce unexpected results if changed." ) );
        }
'@
    $c = Replace-WSOnce $c $experimentalWarningOld $experimentalWarningNew 'experimental warning'

    $experimentalGetOptionOld = @'
        if( opt->second.getPage() != "world_default" ) {
            // Requested a non-world option, deliver it.
            return opt->second;
        }
'@
    $experimentalGetOptionNew = @'
        if( opt->second.getPage() != "world_default" &&
            opt->second.getPage() != "ncmm_experimental" ) {
            // Requested a non-world option, deliver it.
            return opt->second;
        }
'@
    $c = Replace-WSOnce $c $experimentalGetOptionOld $experimentalGetOptionNew 'experimental get_option world scope'

    $experimentalDefaultsOld = @'
        if( elem.second.getPage() == "world_default" ) {
            result.insert( elem );
        }
'@
    $experimentalDefaultsNew = @'
        if( elem.second.getPage() == "world_default" ||
            elem.second.getPage() == "ncmm_experimental" ) {
            result.insert( elem );
        }
'@
    $c = Replace-WSOnce $c $experimentalDefaultsOld $experimentalDefaultsNew 'experimental world defaults'

    $registerImpl = @'
void options_manager::ncmm_ensure_experimental_page()
{
    const auto existing = std::find_if( pages_.begin(), pages_.end(), []( const Page &page ) {
        return page.id_ == "ncmm_experimental";
    } );
    if( existing != pages_.end() ) {
        return;
    }

    const auto debug_page = std::find_if( pages_.begin(), pages_.end(), []( const Page &page ) {
        return page.id_ == "debug";
    } );
    pages_.emplace( debug_page, "ncmm_experimental", to_translation( "Experimental" ) );
}

bool options_manager::ncmm_begin_experimental_group( const std::string &group_id,
        const translation &name, const translation &tooltip )
{
    if( group_id.empty() || !adding_to_group_.empty() ) {
        return false;
    }
    ncmm_ensure_experimental_page();

    for( Group &group : groups_ ) {
        if( group.id_ == group_id ) {
            group.name_ = name;
            group.tooltip_ = tooltip;
            adding_to_group_ = group_id;
            return true;
        }
    }

    groups_.emplace_back( group_id, name, tooltip );
    add_empty_line( "ncmm_experimental" );
    find_page( "ncmm_experimental" ).items_.emplace_back(
        ItemType::GroupHeader, group_id, group_id );
    adding_to_group_ = group_id;
    return true;
}

bool options_manager::ncmm_register_world_bool( const std::string &name,
        const translation &menu_text, const translation &tooltip, bool default_value )
{
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, default_value, COPT_WORLDGEN_ONLY );
        ncmm_apply_deferred_option_value( name, options[name] );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" || opt.eType != cOpt::CVT_BOOL ) {
        return false;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.bDefault = default_value;
    ncmm_apply_deferred_option_value( name, opt );
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            w->second.sMenuText = menu_text;
            w->second.sTooltip = tooltip;
            w->second.hide = COPT_WORLDGEN_ONLY;
            w->second.sPage = "ncmm_experimental";
        }
    }
    return true;
}

bool options_manager::ncmm_register_world_int( const std::string &name,
        const translation &menu_text, const translation &tooltip, int min_value,
        int max_value, int default_value )
{
    if( min_value > max_value || default_value < min_value || default_value > max_value ) {
        return false;
    }
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, min_value, max_value, default_value,
             COPT_WORLDGEN_ONLY );
        ncmm_apply_deferred_option_value( name, options[name] );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" || opt.eType != cOpt::CVT_INT ) {
        return false;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.iMin = min_value;
    opt.iMax = max_value;
    opt.iDefault = default_value;
    opt.iSet = std::clamp( opt.iSet, min_value, max_value );
    ncmm_apply_deferred_option_value( name, opt );
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            w->second.sMenuText = menu_text;
            w->second.sTooltip = tooltip;
            w->second.hide = COPT_WORLDGEN_ONLY;
            w->second.sPage = "ncmm_experimental";
            w->second.iMin = min_value;
            w->second.iMax = max_value;
            w->second.iDefault = default_value;
            w->second.iSet = std::clamp( w->second.iSet, min_value, max_value );
        }
    }
    return true;
}

bool options_manager::ncmm_register_world_float( const std::string &name,
        const translation &menu_text, const translation &tooltip, float min_value,
        float max_value, float default_value, float step )
{
    if( min_value > max_value || default_value < min_value || default_value > max_value || step <= 0.0f ) {
        return false;
    }
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, min_value, max_value,
             default_value, step, COPT_WORLDGEN_ONLY );
        ncmm_apply_deferred_option_value( name, options[name] );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" || opt.eType != cOpt::CVT_FLOAT ) {
        return false;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.fMin = min_value;
    opt.fMax = max_value;
    opt.fDefault = default_value;
    opt.fStep = step;
    opt.fSet = std::clamp( opt.fSet, min_value, max_value );
    ncmm_apply_deferred_option_value( name, opt );
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            w->second.sMenuText = menu_text;
            w->second.sTooltip = tooltip;
            w->second.hide = COPT_WORLDGEN_ONLY;
            w->second.sPage = "ncmm_experimental";
            w->second.fMin = min_value;
            w->second.fMax = max_value;
            w->second.fDefault = default_value;
            w->second.fStep = step;
            w->second.fSet = std::clamp( w->second.fSet, min_value, max_value );
        }
    }
    return true;
}

bool options_manager::ncmm_register_world_enum( const std::string &name,
        const translation &menu_text, const translation &tooltip,
        const std::vector<id_and_option> &items, const std::string &default_value )
{
    if( items.empty() ) {
        return false;
    }
    const auto contains = [&]( const std::string &value ) {
        return std::any_of( items.begin(), items.end(), [&]( const id_and_option &item ) {
            return item.first == value;
        } );
    };
    if( !contains( default_value ) ) {
        return false;
    }
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, items, default_value, COPT_WORLDGEN_ONLY );
        ncmm_apply_deferred_option_value( name, options[name] );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" || opt.eType != cOpt::CVT_STRING ) {
        return false;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.sType = "string_select";
    opt.vItems = items;
    opt.sDefault = default_value;
    if( !contains( opt.sSet ) ) {
        opt.sSet = default_value;
    }
    ncmm_apply_deferred_option_value( name, opt );
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            w->second.sMenuText = menu_text;
            w->second.sTooltip = tooltip;
            w->second.hide = COPT_WORLDGEN_ONLY;
            w->second.sPage = "ncmm_experimental";
            w->second.sType = "string_select";
            w->second.vItems = items;
            w->second.sDefault = default_value;
            if( !contains( w->second.sSet ) ) {
                w->second.sSet = default_value;
            }
        }
    }
    return true;
}

'@
    $c = Replace-WSOnce $c 'void options_manager::update_global_locale()' ($registerImpl + 'void options_manager::update_global_locale()') 'options implementation'
    Write-WS $optionsCpp $c

    # Infrastructure 0.8.3.1: world saves serialize NCMM experimental settings through WORLD_OPTIONS,
    # but vanilla 0546 restores only entries on world_default. Treat ncmm_experimental as the same
    # world-scoped persistence domain so existing worlds retain their AWS values.
    $wf = Read-WS $worldFactoryPath
    $worldLoadOld = @'
        if( opts.has_option( name ) && opts.get_option( name ).getPage() == "world_default" ) {
            WORLD_OPTIONS[ name ].setValue( value );
        }
'@
    $worldLoadNew = @'
        if( opts.has_option( name ) &&
            ( opts.get_option( name ).getPage() == "world_default" ||
              opts.get_option( name ).getPage() == "ncmm_experimental" ) ) {
            WORLD_OPTIONS[ name ].setValue( value );
        }
'@
    $wf = Replace-WSOnce $wf $worldLoadOld $worldLoadNew 'restore experimental settings from world save'
    Write-WS $worldFactoryPath $wf

    # City size/spacing/urbanity. Applied only to the default world region.
    $city = Read-WS $cityPath
    if (-not $city.Contains('#include "options.h"')) {
        $city = Replace-WSOnce $city '#include "omdata.h"' ('#include "omdata.h"' + "`n" + '#include "options.h"') 'city options include'
    }
    $cityOld = @'
    const region_settings_city &city_settings = settings->get_settings_city();
    int op_city_spacing = city_settings.city_spacing;
    int op_city_size = city_settings.city_size;
    int max_urbanity = settings->max_urban;
'@
    $cityNew = @'
    const region_settings_city &city_settings = settings->get_settings_city();
    const bool ncmm_geo = settings->id.str() == "default" &&
                          get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                          get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                          get_options().has_option( "NCMM_AWS_CITY_SIZE" );
    int op_city_spacing = ncmm_geo ? get_option<int>( "NCMM_AWS_CITY_SPACING" ) : city_settings.city_spacing;
    int op_city_size = ncmm_geo ? get_option<int>( "NCMM_AWS_CITY_SIZE" ) : city_settings.city_size;
    int max_urbanity = ncmm_geo ? get_option<int>( "NCMM_AWS_MAX_URBANITY" ) : settings->max_urban;
    const bool ncmm_megacity = ncmm_geo && get_options().has_option( "NCMM_AWS_MEGACITY" ) ?
                                get_option<bool>( "NCMM_AWS_MEGACITY" ) : city_settings.is_megacity;
'@
    $city = Replace-WSOnce $city $cityOld $cityNew 'city size spacing urbanity megacity'
    $city = Replace-WSOnce $city '    if( city_settings.is_megacity ) {' '    if( ncmm_megacity ) {' 'megacity mode'

    $buildingMixOld = @'
    const region_settings_city &city_spec = settings->get_settings_city();
    int shop_radius = city_spec.shop_radius;
    int park_radius = city_spec.park_radius;

    int shop_sigma = city_spec.shop_sigma;
    int park_sigma = city_spec.park_sigma;
'@
    $buildingMixNew = @'
    const region_settings_city &city_spec = settings->get_settings_city();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_SHOP_RADIUS" );
    int shop_radius = ncmm_geo ? get_option<int>( "NCMM_AWS_SHOP_RADIUS" ) : city_spec.shop_radius;
    int park_radius = ncmm_geo ? get_option<int>( "NCMM_AWS_PARK_RADIUS" ) : city_spec.park_radius;

    int shop_sigma = ncmm_geo ? get_option<int>( "NCMM_AWS_SHOP_SIGMA" ) : city_spec.shop_sigma;
    int park_sigma = ncmm_geo ? get_option<int>( "NCMM_AWS_PARK_SIGMA" ) : city_spec.park_sigma;
'@
    $city = Replace-WSOnce $city $buildingMixOld $buildingMixNew 'city shop/park distribution'
    Write-WS $cityPath $city

    $om = Read-WS $overmapPath
    $geoGenerate = @'
    std::vector<Highway_path> highway_paths;
    calculate_urbanity();
    calculate_forestosity();
    const bool ncmm_geo = settings->id.str() == "default" &&
                          get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                          get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                          get_options().has_option( "NCMM_AWS_CITY_SIZE" );
    const auto geo_enabled = [&]( const char *id ) {
        return !ncmm_geo || !get_options().has_option( id ) || get_option<bool>( id );
    };
    if( settings->neighbor_connections && geo_enabled( "NCMM_AWS_NEIGHBOR_CONNECTIONS" ) ) {
        populate_connections_out_from_neighbors( neighbor_overmaps );
    }
    if( settings->overmap_river && geo_enabled( "NCMM_AWS_ENABLE_RIVERS" ) ) {
        place_rivers( neighbor_overmaps );
    }
    if( settings->overmap_lake && geo_enabled( "NCMM_AWS_ENABLE_LAKES" ) ) {
        place_lakes( neighbor_overmaps );
    }
    if( settings->overmap_ocean && geo_enabled( "NCMM_AWS_ENABLE_OCEANS" ) ) {
        place_oceans( neighbor_overmaps );
    }
    if( settings->overmap_forest && geo_enabled( "NCMM_AWS_ENABLE_FORESTS" ) ) {
        place_forests();
    }
    if( settings->overmap_forest && settings->place_swamps && geo_enabled( "NCMM_AWS_ENABLE_SWAMPS" ) ) {
        place_swamps();
    }
    if( settings->overmap_ravine && geo_enabled( "NCMM_AWS_ENABLE_RAVINES" ) ) {
        place_ravines();
    }
    if( settings->overmap_river && geo_enabled( "NCMM_AWS_ENABLE_RIVERS" ) ) {
        polish_river( neighbor_overmaps );
    }
    if( settings->overmap_highway && geo_enabled( "NCMM_AWS_ENABLE_HIGHWAYS" ) ) {
        highway_paths = place_highways( neighbor_overmaps );
    }
    if( settings->city_spec ) {
        place_cities();
    }
    if( settings->overmap_highway && geo_enabled( "NCMM_AWS_ENABLE_HIGHWAYS" ) ) {
        place_highway_interchanges( highway_paths );
    }
    if( settings->city_spec ) {
        build_cities();
    }
    if( settings->forest_trail && geo_enabled( "NCMM_AWS_ENABLE_TRAILS" ) ) {
        place_forest_trails();
    }
    if( settings->place_railroads_before_roads ) {
        if( settings->place_railroads && geo_enabled( "NCMM_AWS_PLACE_RAILROADS" ) ) {
            place_railroads( neighbor_overmaps );
        }
        if( settings->place_roads && geo_enabled( "NCMM_AWS_PLACE_ROADS" ) ) {
            place_roads( neighbor_overmaps );
        }
    } else {
        if( settings->place_roads && geo_enabled( "NCMM_AWS_PLACE_ROADS" ) ) {
            place_roads( neighbor_overmaps );
        }
        if( settings->place_railroads && geo_enabled( "NCMM_AWS_PLACE_RAILROADS" ) ) {
            place_railroads( neighbor_overmaps );
        }
    }
    if( settings->place_specials && geo_enabled( "NCMM_AWS_PLACE_SPECIALS" ) ) {
        place_specials( enabled_specials );
    }
    if( settings->overmap_highway && geo_enabled( "NCMM_AWS_ENABLE_HIGHWAYS" ) ) {
        finalize_highways( highway_paths );
    }
    if( settings->forest_trail && geo_enabled( "NCMM_AWS_ENABLE_TRAILS" ) ) {
        place_forest_trailheads();
    }
    if( settings->overmap_river && geo_enabled( "NCMM_AWS_ENABLE_RIVERS" ) ) {
        polish_river( neighbor_overmaps );
    }
'@
    $om = Replace-WSRange $om '    std::vector<Highway_path> highway_paths;' '    // TODO: there is no reason we can''t generate the sublevels in one pass' $geoGenerate 'generation feature gates'

    $forestosityGateOld = @'
    const region_settings_forest &settings_forest = settings->get_settings_forest();
    float northern_forest_increase = settings_forest.forest_increase[static_cast<int>
'@
    $forestosityGateNew = @'
    const region_settings_forest &settings_forest = settings->get_settings_forest();
    const bool ncmm_disable_forests = settings->id.str() == "default" &&
                                      get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                      get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                      get_options().has_option( "NCMM_AWS_ENABLE_FORESTS" ) &&
                                      !get_option<bool>( "NCMM_AWS_ENABLE_FORESTS" );
    if( ncmm_disable_forests ) {
        forest_size_adjust = 0;
        forestosity = 0;
        return;
    }
    float northern_forest_increase = settings_forest.forest_increase[static_cast<int>
'@
    $om = Replace-WSOnce $om $forestosityGateOld $forestosityGateNew 'disabled-forest forestosity reset'

    $forestosityOld = @'
    forest_size_adjust = std::min<float>( forest_size_adjust,
                                          settings_forest.max_forest - settings_forest.noise_threshold_forest );
'@
    $forestosityNew = @'
    const float ncmm_forest_threshold = settings->id.str() == "default" &&
                                        get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                        get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                        get_options().has_option( "NCMM_AWS_FOREST_THRESHOLD" ) ?
                                        get_option<float>( "NCMM_AWS_FOREST_THRESHOLD" ) :
                                        settings_forest.noise_threshold_forest;
    forest_size_adjust = std::min<float>( forest_size_adjust,
                                          settings_forest.max_forest - ncmm_forest_threshold );
'@
    $om = Replace-WSOnce $om $forestosityOld $forestosityNew 'forestosity threshold clamp'

    $forestNew = @'
void overmap::place_forests()
{
    const region_settings_forest &settings_forest = settings->get_settings_forest();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_FOREST_THRESHOLD" );
    const float forest_threshold = ncmm_geo ? get_option<float>( "NCMM_AWS_FOREST_THRESHOLD" ) :
                                   settings_forest.noise_threshold_forest;
    const float thick_threshold_raw = ncmm_geo ? get_option<float>( "NCMM_AWS_FOREST_THICK_THRESHOLD" ) :
                                      settings_forest.noise_threshold_forest_thick;
    const float thick_threshold = std::max( forest_threshold, thick_threshold_raw );
    const oter_id default_oter_id( settings->default_oter[OVERMAP_DEPTH] );
    const om_noise::om_noise_layer_forest f( global_base_point(), g->get_seed() );

    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt p( x, y, 0 );
            const oter_id &oter = ter( p );
            if( oter != default_oter_id ) {
                continue;
            }
            const float n = f.noise_at( p.xy() );
            if( n + forest_size_adjust > thick_threshold ) {
                ter_set( p, oter_forest_thick );
            } else if( n + forest_size_adjust > forest_threshold ) {
                ter_set( p, oter_forest );
            }
        }
    }
}
'@
    $om = Replace-WSRange $om 'void overmap::place_forests()' 'bool overmap::omt_lake_noise_threshold' $forestNew 'forest thresholds'

    $swampNew = @'
void overmap::place_swamps()
{
    const region_settings_forest &settings_forest = settings->get_settings_forest();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_SWAMP_ADJ_THRESHOLD" );
    int flood_min = ncmm_geo ? get_option<int>( "NCMM_AWS_FLOODPLAIN_MIN" ) :
                    settings_forest.river_floodplain_buffer_distance_min;
    int flood_max = ncmm_geo ? get_option<int>( "NCMM_AWS_FLOODPLAIN_MAX" ) :
                    settings_forest.river_floodplain_buffer_distance_max;
    if( flood_min > flood_max ) {
        std::swap( flood_min, flood_max );
    }
    const float swamp_adj = ncmm_geo ? get_option<float>( "NCMM_AWS_SWAMP_ADJ_THRESHOLD" ) :
                            settings_forest.noise_threshold_swamp_adjacent_water;
    const float swamp_isolated = ncmm_geo ? get_option<float>( "NCMM_AWS_SWAMP_ISOLATED_THRESHOLD" ) :
                                 settings_forest.noise_threshold_swamp_isolated;

    std::unique_ptr<cata::mdarray<int, point_om_omt>> floodptr =
                std::make_unique<cata::mdarray<int, point_om_omt>>( 0 );
    cata::mdarray<int, point_om_omt> &floodplain = *floodptr;
    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt pos( x, y, 0 );
            if( is_ot_match( "river", ter_unsafe( pos ), ot_match_type::contains ) ) {
                std::vector<point_om_omt> buffered_points =
                    closest_points_first( pos.xy(), rng( flood_min, flood_max ) );
                for( const point_om_omt &p : buffered_points )  {
                    if( !inbounds( p ) ) {
                        continue;
                    }
                    floodplain[p] += 1;
                }
            }
        }
    }

    const om_noise::om_noise_layer_floodplain f( global_base_point(), g->get_seed() );
    for( int x = 0; x < OMAPX; x++ ) {
        for( int y = 0; y < OMAPY; y++ ) {
            const tripoint_om_omt pos( x, y, 0 );
            if( !is_ot_match( "forest", ter( pos ), ot_match_type::contains ) ) {
                continue;
            }
            const bool should_flood = floodplain[x][y] > 0 && !one_in( floodplain[x][y] ) &&
                                      f.noise_at( { x, y } ) > swamp_adj;
            const bool should_isolated_swamp = f.noise_at( pos.xy() ) > swamp_isolated;
            if( should_flood || should_isolated_swamp ) {
                ter_set( pos, oter_forest_water );
            }
        }
    }
}
'@
    $om = Replace-WSRange $om 'void overmap::place_swamps()' 'void overmap::place_roads(' $swampNew 'swamp and floodplain controls'

    # Existing road/rail/trailhead checks must honor a city-size override of zero.
    $roadNeedle = 'int op_city_size = settings->get_settings_city().city_size;'
    $roadCount = ([regex]::Matches($om,[regex]::Escape($roadNeedle))).Count
    if ($roadCount -ne 3) { throw "World Settings v2 expected three city-size urbanity/road/rail checks, found $roadCount" }
    $om = $om.Replace($roadNeedle,
        'const bool ncmm_geo_city = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_CITY_SIZE" );' + "`n" +
        '    int op_city_size = ncmm_geo_city ? get_option<int>( "NCMM_AWS_CITY_SIZE" ) : settings->get_settings_city().city_size;')

    $trailNew = @'
void overmap::place_forest_trails()
{
    std::unordered_set<point_om_omt> visited;
    const auto is_forest = [&]( const point_om_omt & p ) {
        if( !inbounds( p, 1 ) ) {
            return false;
        }
        const oter_id current_terrain = ter( tripoint_om_omt( p, 0 ) );
        return current_terrain == oter_forest || current_terrain == oter_forest_thick ||
               current_terrain == oter_forest_water;
    };
    const region_settings_forest_trail &forest_trail = settings->get_settings_forest_trail();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_TRAIL_CHANCE" );
    const int trail_chance = ncmm_geo ? get_option<int>( "NCMM_AWS_TRAIL_CHANCE" ) : forest_trail.chance;
    const int minimum_forest_size = ncmm_geo ? get_option<int>( "NCMM_AWS_TRAIL_MIN_FOREST" ) : forest_trail.minimum_forest_size;

    for( int i = 0; i < OMAPX; i++ ) {
        for( int j = 0; j < OMAPY; j++ ) {
            tripoint_om_omt seed_point( i, j, 0 );
            oter_id oter = ter( seed_point );
            if( !is_ot_match( "forest", oter, ot_match_type::prefix ) ) {
                continue;
            }
            if( visited.find( seed_point.xy() ) != visited.end() ) {
                continue;
            }
            std::vector<point_om_omt> forest_points =
                ff::point_flood_fill_4_connected<std::vector>( seed_point.xy(), visited, is_forest );
            if( forest_points.empty() || forest_points.size() < static_cast<size_t>( minimum_forest_size ) ) {
                continue;
            }
            if( !one_in( std::max( 1, trail_chance ) ) ) {
                continue;
            }

            auto north_south_most = std::minmax_element( forest_points.begin(), forest_points.end(),
            []( const point_om_omt & lhs, const point_om_omt & rhs ) { return lhs.y() < rhs.y(); } );
            auto west_east_most = std::minmax_element( forest_points.begin(), forest_points.end(),
            []( const point_om_omt & lhs, const point_om_omt & rhs ) { return lhs.x() < rhs.x(); } );
            point_om_omt northmost = *north_south_most.first;
            point_om_omt southmost = *north_south_most.second;
            point_om_omt westmost = *west_east_most.first;
            point_om_omt eastmost = *west_east_most.second;
            point_om_omt center( westmost.x() + ( eastmost.x() - westmost.x() ) / 2,
                                 northmost.y() + ( southmost.y() - northmost.y() ) / 2 );
            point_om_omt actual_center_point = *std::min_element( forest_points.begin(), forest_points.end(),
            [&center]( const point_om_omt & lhs, const point_om_omt & rhs ) {
                return square_dist( lhs, center ) < square_dist( rhs, center );
            } );
            int max_random_points = forest_trail.random_point_min + forest_points.size() /
                                    forest_trail.random_point_size_scalar;
            max_random_points = std::min( max_random_points, forest_trail.random_point_max );
            std::vector<point_om_omt> chosen_points = { actual_center_point };
            int random_point_count = 0;
            std::shuffle( forest_points.begin(), forest_points.end(), rng_get_engine() );
            for( const auto &random_point : forest_points ) {
                if( random_point_count >= max_random_points ) {
                    break;
                }
                ++random_point_count;
                chosen_points.emplace_back( random_point );
            }
            if( one_in( forest_trail.border_point_chance ) ) chosen_points.emplace_back( northmost );
            if( one_in( forest_trail.border_point_chance ) ) chosen_points.emplace_back( southmost );
            if( one_in( forest_trail.border_point_chance ) ) chosen_points.emplace_back( westmost );
            if( one_in( forest_trail.border_point_chance ) ) chosen_points.emplace_back( eastmost );
            const overmap_connection_id &trail_connection = settings->overmap_connection.trail_connection;
            connect_closest_points( chosen_points, 0, *trail_connection );
        }
    }
}
'@
    $om = Replace-WSRange $om 'void overmap::place_forest_trails()' 'void overmap::place_forest_trailheads()' $trailNew 'forest trail frequency'

    $trailheadNew = @'
void overmap::place_forest_trailheads()
{
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_CITY_SIZE" );
    const int city_size = ncmm_geo ? get_option<int>( "NCMM_AWS_CITY_SIZE" ) : settings->get_settings_city().city_size;
    if( city_size <= 0 ) {
        return;
    }
    const region_settings_forest_trail &settings_forest_trail = settings->get_settings_forest_trail();
    const int trailhead_chance = ncmm_geo && get_options().has_option( "NCMM_AWS_TRAILHEAD_CHANCE" ) ?
                                 get_option<int>( "NCMM_AWS_TRAILHEAD_CHANCE" ) : settings_forest_trail.trailhead_chance;
    const int road_distance = ncmm_geo && get_options().has_option( "NCMM_AWS_TRAILHEAD_ROAD_DISTANCE" ) ?
                              get_option<int>( "NCMM_AWS_TRAILHEAD_ROAD_DISTANCE" ) : settings_forest_trail.trailhead_road_distance;

    const auto trailhead_close_to_road = [&]( const tripoint_om_omt & trailhead ) {
        bool close = false;
        for( const tripoint_om_omt &nearby_point : closest_points_first( trailhead, road_distance ) ) {
            if( check_ot( "road", ot_match_type::contains, nearby_point ) ) {
                close = true;
            }
        }
        return close;
    };
    const auto try_place_trailhead_special = [&]( const tripoint_om_omt & trail_end,
    const om_direction::type & dir ) {
        overmap_special_id trailhead = settings_forest_trail.trailheads.pick();
        if( one_in( std::max( 1, trailhead_chance ) ) && trailhead_close_to_road( trail_end ) &&
            can_place_special( *trailhead, trail_end, dir, false ) ) {
            const city &nearest_city = get_nearest_city( trail_end );
            place_special( *trailhead, trail_end, dir, nearest_city, false, false );
        }
    };
    for( int i = 2; i < OMAPX - 2; i++ ) {
        for( int j = 2; j < OMAPY - 2; j++ ) {
            const tripoint_om_omt p( i, j, 0 );
            oter_id oter = ter( p );
            if( is_ot_match( "forest_trail_end", oter, ot_match_type::prefix ) ) {
                try_place_trailhead_special( p, static_cast<om_direction::type>( oter->get_rotation() ) );
            }
        }
    }
}
'@
    $om = Replace-WSRange $om 'void overmap::place_forest_trailheads()' 'void overmap::place_forests()' $trailheadNew 'forest trailheads'

    $ravineNew = @'
void overmap::place_ravines()
{
    const region_settings_ravine &settings_ravine = settings->get_settings_ravine();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_RAVINE_COUNT" );
    const int num_ravines = ncmm_geo ? get_option<int>( "NCMM_AWS_RAVINE_COUNT" ) : settings_ravine.num_ravines;
    if( num_ravines == 0 ) {
        return;
    }
    const oter_id rift( "ravine" );
    const oter_id rift_edge( "ravine_edge" );
    const oter_id rift_floor = settings_ravine.ravine_bottom_terrain_str_id.id();
    const oter_id rift_floor_edge( "ravine_floor_edge" );
    std::set<point_om_omt> rift_points;
    const pf::two_node_scoring_fn<point_om_omt> estimate =
    [&]( pf::directed_node<point_om_omt>, const std::optional<pf::directed_node<point_om_omt>> & ) {
        return pf::node_score( 0, rng( 1, 2 ) );
    };
    const int ravine_range = ncmm_geo ? get_option<int>( "NCMM_AWS_RAVINE_RANGE" ) : settings_ravine.ravine_range;
    const int ravine_width = ncmm_geo ? get_option<int>( "NCMM_AWS_RAVINE_WIDTH" ) : settings_ravine.ravine_width;
    const int ravine_depth = ncmm_geo ? get_option<int>( "NCMM_AWS_RAVINE_DEPTH" ) : settings_ravine.ravine_depth;
    for( int n = 0; n < num_ravines; n++ ) {
        const point_rel_omt offset( rng( -ravine_range, ravine_range ), rng( -ravine_range, ravine_range ) );
        const point_om_omt origin( rng( 0, OMAPX ), rng( 0, OMAPY ) );
        const point_om_omt destination = origin + offset;
        if( !inbounds( destination, ravine_width * 3 ) ) {
            continue;
        }
        const auto path = pf::greedy_path( origin, destination, point_om_omt( OMAPX, OMAPY ), estimate );
        for( const auto &node : path.nodes ) {
            for( int i = 1 - ravine_width; i < ravine_width; i++ ) {
                for( int j = 1 - ravine_width; j < ravine_width; j++ ) {
                    const point_om_omt n = node.pos + point( j, i );
                    if( inbounds( n, 1 ) ) {
                        rift_points.emplace( n );
                    }
                }
            }
        }
    }
    for( const point_om_omt &p : rift_points ) {
        bool edge = false;
        for( int ni = -1; ni <= 1 && !edge; ni++ ) {
            for( int nj = -1; nj <= 1 && !edge; nj++ ) {
                const point_om_omt n = p + point_rel_omt( ni, nj );
                if( rift_points.find( n ) == rift_points.end() || !inbounds( n ) ) {
                    edge = true;
                }
            }
        }
        for( int z = 0; z >= ravine_depth; z-- ) {
            if( z == ravine_depth ) {
                ter_set( tripoint_om_omt( p, z ), edge ? rift_floor_edge : rift_floor );
            } else {
                ter_set( tripoint_om_omt( p, z ), edge ? rift_edge : rift );
            }
        }
    }
}
'@
    $om = Replace-WSRange $om 'void overmap::place_ravines()' 'pf::directed_path<point_om_omt> overmap::lay_out_connection' $ravineNew 'ravine controls'
    Write-WS $overmapPath $om

    $water = Read-WS $waterPath
    if (-not $water.Contains('#include "options.h"')) {
        $water = Replace-WSOnce $water '#include "omdata.h"' ('#include "omdata.h"' + "`n" + '#include "options.h"') 'water options include'
    }
    $riverSettingsOld = @'
    const region_settings_river &settings_river = settings->get_settings_river();
    const int OMAPX_edge = OMAPX - 1;
'@
    $riverSettingsNew = @'
    const region_settings_river &settings_river = settings->get_settings_river();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_RIVER_BRANCH_CHANCE" );
    const double river_branch_chance = ncmm_geo ? get_option<int>( "NCMM_AWS_RIVER_BRANCH_CHANCE" ) : settings_river.river_branch_chance;
    const double river_branch_remerge_chance = ncmm_geo ? get_option<int>( "NCMM_AWS_RIVER_REMERGE_CHANCE" ) : settings_river.river_branch_remerge_chance;
    const double river_branch_scale_decrease = ncmm_geo ? get_option<float>( "NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE" ) : settings_river.river_branch_scale_decrease;
    const int OMAPX_edge = OMAPX - 1;
'@
    $water = Replace-WSOnce $water $riverSettingsOld $riverSettingsNew 'river branch locals'
    $water = $water.Replace('one_in( settings_river.river_branch_chance )','one_in( river_branch_chance )')
    $water = $water.Replace('one_in( settings_river.river_branch_remerge_chance )','one_in( river_branch_remerge_chance )')
    $water = $water.Replace('river_scale - settings_river.river_branch_scale_decrease','river_scale - river_branch_scale_decrease')

    $riverMainOld = @'
    const region_settings_river &settings_river = settings->get_settings_river();
    int river_scale = settings_river.river_scale;
'@
    $riverMainNew = @'
    const region_settings_river &settings_river = settings->get_settings_river();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_RIVER_SCALE" );
    int river_scale = ncmm_geo ? get_option<int>( "NCMM_AWS_RIVER_SCALE" ) : settings_river.river_scale;
    const double river_frequency = ncmm_geo ? get_option<float>( "NCMM_AWS_RIVER_FREQUENCY" ) : settings_river.river_frequency;
'@
    $water = Replace-WSOnce $water $riverMainOld $riverMainNew 'river scale/frequency locals'
    $water = $water.Replace('std::pow( settings_river.river_frequency,','std::pow( river_frequency,')

    $lakeOld = @'
    const region_settings_lake &settings_lake = settings->get_settings_lake();
    double noise_threshold = settings_lake.noise_threshold_lake;
    const int lake_depth = settings_lake.lake_depth;
'@
    $lakeNew = @'
    const region_settings_lake &settings_lake = settings->get_settings_lake();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_LAKE_THRESHOLD" );
    double noise_threshold = ncmm_geo ? get_option<float>( "NCMM_AWS_LAKE_THRESHOLD" ) : settings_lake.noise_threshold_lake;
    const int lake_size_min = ncmm_geo ? get_option<int>( "NCMM_AWS_LAKE_MIN_SIZE" ) : settings_lake.lake_size_min;
    const int lake_depth = settings_lake.lake_depth;
'@
    $water = Replace-WSOnce $water $lakeOld $lakeNew 'lake controls'
    $water = $water.Replace('( settings_lake.lake_size_min )','( lake_size_min )')

    $oceanOld = @'
    const region_settings_ocean &settings_ocean = settings->get_settings_ocean();
    const int ocean_depth = settings_ocean.ocean_depth;
'@
    $oceanNew = @'
    const region_settings_ocean &settings_ocean = settings->get_settings_ocean();
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_OCEAN_THRESHOLD" );
    const double ocean_threshold = ncmm_geo ? get_option<float>( "NCMM_AWS_OCEAN_THRESHOLD" ) : settings_ocean.noise_threshold_ocean;
    const int ocean_size_min = ncmm_geo ? get_option<int>( "NCMM_AWS_OCEAN_MIN_SIZE" ) : settings_ocean.ocean_size_min;
    const int ocean_depth = settings_ocean.ocean_depth;
'@
    $water = Replace-WSOnce $water $oceanOld $oceanNew 'ocean controls'
    $water = $water.Replace('f.noise_at( p ) + ocean_adjust > settings_ocean.noise_threshold_ocean','f.noise_at( p ) + ocean_adjust > ocean_threshold')
    $water = $water.Replace('( settings_ocean.ocean_size_min )','( ocean_size_min )')
    Write-WS $waterPath $water

    $overmapH = Read-WS $overmapHPath
    $highwayDeclOld = '        void set_options();'
    $highwayDeclNew = @'
        void set_options( int row_override = -1, int column_override = -1,
                          int variance_override = -1 );
'@
    $overmapH = Replace-WSOnce $overmapH $highwayDeclOld $highwayDeclNew 'highway grid scoped options declaration'
    Write-WS $overmapHPath $overmapH

    $highway = Read-WS $highwayPath
    $setOptionsOld = @'
void highway_intersection_grid::set_options()
{
    row_separation = get_option<int>( "HIGHWAY_GRID_ROW_SEPARATION" );
    column_separation = get_option<int>( "HIGHWAY_GRID_COLUMN_SEPARATION" );
    max_offset_variance = get_option<int>( "HIGHWAY_GRID_VARIANCE" );
}
'@
    $setOptionsNew = @'
void highway_intersection_grid::set_options( int row_override, int column_override,
        int variance_override )
{
    row_separation = row_override >= 0 ? row_override : get_option<int>( "HIGHWAY_GRID_ROW_SEPARATION" );
    column_separation = column_override >= 0 ? column_override :
                        get_option<int>( "HIGHWAY_GRID_COLUMN_SEPARATION" );
    max_offset_variance = variance_override >= 0 ? variance_override :
                          get_option<int>( "HIGHWAY_GRID_VARIANCE" );
    const int safe_limit = std::max( 0, std::min( row_separation, column_separation ) / 2 - 1 );
    max_offset_variance = std::min( max_offset_variance, safe_limit );
}
'@
    $highway = Replace-WSOnce $highway $setOptionsOld $setOptionsNew 'highway grid'
    $gridCallOld = @'
    highway_grid.set_options();
'@
    $gridCallNew = @'
    const bool ncmm_custom_grid = settings->id.str() == "default" &&
                                  get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                  get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                  get_options().has_option( "NCMM_AWS_HIGHWAY_GRID_ROW" );
    if( ncmm_custom_grid ) {
        highway_grid.set_options( get_option<int>( "NCMM_AWS_HIGHWAY_GRID_ROW" ),
                                  get_option<int>( "NCMM_AWS_HIGHWAY_GRID_COLUMN" ),
                                  get_option<int>( "NCMM_AWS_HIGHWAY_GRID_VARIANCE" ) );
    } else {
        highway_grid.set_options();
    }
'@
    $highway = Replace-WSOnce $highway $gridCallOld $gridCallNew 'default-region highway grid selection'

    $straightOld = @'
    const region_settings_highway &highway_settings = settings->get_settings_highway();
    const int HIGHWAY_MAX_DEVIANCE = highway_settings.HIGHWAY_MAX_DEVIANCE;
    //used until intersections can be safely placed at corners of the overmap with two-bend pathing
'@
    $straightNew = @'
    const region_settings_highway &highway_settings = settings->get_settings_highway();
    const int HIGHWAY_MAX_DEVIANCE = highway_settings.HIGHWAY_MAX_DEVIANCE;
    const bool ncmm_geo = settings->id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_HIGHWAY_STRAIGHTNESS" );
    const double ncmm_highway_straightness = ncmm_geo ? get_option<float>( "NCMM_AWS_HIGHWAY_STRAIGHTNESS" ) :
                                             highway_settings.straightness_chance;
    //used until intersections can be safely placed at corners of the overmap with two-bend pathing
'@
    $highway = Replace-WSOnce $highway $straightOld $straightNew 'highway straightness local'
    $highway = $highway.Replace('x_in_y( highway_settings.straightness_chance, 1.0 )','x_in_y( ncmm_highway_straightness, 1.0 )')

    # Lake-aware highway intersection avoidance follows the same world lake controls.
    $lakeAvoidOld = @'
            const region_settings_lake &lake_settings = settings.get_settings_lake();
            val_emplaced.first->second =
                !overmap::guess_has_lake( pt, lake_settings.noise_threshold_lake,
                                          lake_settings.lake_size_min );
'@
    $lakeAvoidNew = @'
            const region_settings_lake &lake_settings = settings.get_settings_lake();
            const bool ncmm_geo = settings.id.str() == "default" && get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) && get_options().has_option( "NCMM_AWS_LAKE_THRESHOLD" );
            const bool ncmm_lakes_enabled = !ncmm_geo || !get_options().has_option( "NCMM_AWS_ENABLE_LAKES" ) || get_option<bool>( "NCMM_AWS_ENABLE_LAKES" );
            const double lake_threshold = ncmm_geo ? get_option<float>( "NCMM_AWS_LAKE_THRESHOLD" ) : lake_settings.noise_threshold_lake;
            const int lake_min_size = ncmm_geo ? get_option<int>( "NCMM_AWS_LAKE_MIN_SIZE" ) : lake_settings.lake_size_min;
            val_emplaced.first->second = !ncmm_lakes_enabled ||
                                         !overmap::guess_has_lake( pt, lake_threshold, lake_min_size );
'@
    $highway = Replace-WSOnce $highway $lakeAvoidOld $lakeAvoidNew 'highway lake avoidance'
    $oceanHandleOld = @'
overmap::highway_handle_oceans()
{
    std::bitset<HIGHWAY_MAX_CONNECTIONS> ocean_adjacent;
'@
    $oceanHandleNew = @'
overmap::highway_handle_oceans()
{
    std::bitset<HIGHWAY_MAX_CONNECTIONS> ocean_adjacent;
    const bool ncmm_oceans_disabled = settings->id.str() == "default" &&
                                      get_options().has_option( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                      get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" ) &&
                                      get_options().has_option( "NCMM_AWS_ENABLE_OCEANS" ) &&
                                      !get_option<bool>( "NCMM_AWS_ENABLE_OCEANS" );
    if( ncmm_oceans_disabled ) {
        return { false, ocean_adjacent };
    }
'@
    $highway = Replace-WSOnce $highway $oceanHandleOld $oceanHandleNew 'disabled-ocean highway continuity'
    Write-WS $highwayPath $highway

    Write-Utf8NoBom $marker ("NCMM World Settings API v2 geography hooks for CDDA 0546`n")
    Write-Host "World Settings API v2 geography hooks: READY" -ForegroundColor Green
}

function Read-GameSourceCommit([string]$Root) {
    $version = Join-Path $Root "VERSION.txt"
    if (-not (Test-Path $version -PathType Leaf)) {
        return ""
    }
    foreach ($line in Get-Content $version -ErrorAction SilentlyContinue) {
        if ($line -match '^\s*commit sha:\s*(\S+)\s*$') {
            return ([string]$matches[1]).Trim().ToLowerInvariant()
        }
    }
    return ""
}

function Read-GameBindingCommit([string]$Root) {
    $binding = Join-Path $Root "ncmm\host.binding.json"
    if (-not (Test-Path $binding -PathType Leaf)) {
        return ""
    }
    try {
        $parsed = Get-Content $binding -Raw | ConvertFrom-Json
        return ([string]$parsed.source_commit).Trim().ToLowerInvariant()
    } catch {
        return ""
    }
}

function Resolve-GameRoot([string]$RequestedRoot,[string]$ExpectedCommit,[string]$ExpectedFolder) {
    if (-not [string]::IsNullOrWhiteSpace($RequestedRoot)) {
        $resolved = [IO.Path]::GetFullPath($RequestedRoot)
        if (-not (Test-Path $resolved -PathType Container)) {
            throw "Game root missing: $resolved"
        }
        return $resolved
    }

    $expectedFolder = $ExpectedFolder
    $candidates = New-Object System.Collections.Generic.List[string]
    $standard = Join-Path $env:LOCALAPPDATA "com.munetmo.cat-launcher\Assets\DarkDaysAhead\$expectedFolder"
    $candidates.Add($standard)
    if (-not [string]::IsNullOrWhiteSpace($env:CDDA_ROOT)) {
        $candidates.Add($env:CDDA_ROOT)
    }
    $candidates.Add((Get-Location).Path)

    $assets = Join-Path $env:LOCALAPPDATA "com.munetmo.cat-launcher\Assets\DarkDaysAhead"
    if (Test-Path $assets -PathType Container) {
        foreach ($dir in @(Get-ChildItem $assets -Directory -ErrorAction SilentlyContinue)) {
            $candidates.Add($dir.FullName)
        }
    }

    $ranked = @()
    foreach ($candidateRaw in @($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($candidateRaw)) { continue }
        $candidate = [IO.Path]::GetFullPath($candidateRaw)
        if (-not (Test-Path $candidate -PathType Container)) { continue }
        if (-not (Test-Path (Join-Path $candidate "cataclysm-tiles.exe") -PathType Leaf) -and
            -not (Test-Path (Join-Path $candidate "cataclysm-tiles.vanilla.exe") -PathType Leaf)) {
            continue
        }

        $sourceCommit = Read-GameSourceCommit $candidate
        $bindingCommit = Read-GameBindingCommit $candidate
        if (-not [string]::IsNullOrWhiteSpace($sourceCommit) -and $sourceCommit -ne $ExpectedCommit) {
            continue
        }
        if (-not [string]::IsNullOrWhiteSpace($bindingCommit) -and $bindingCommit -ne $ExpectedCommit) {
            continue
        }

        $score = 0
        if ($sourceCommit -eq $ExpectedCommit) { $score += 100 }
        if ($bindingCommit -eq $ExpectedCommit) { $score += 80 }
        if ([IO.Path]::GetFileName($candidate) -eq $expectedFolder) { $score += 40 }
        if ((Normalize-Path $candidate) -eq (Normalize-Path $standard)) { $score += 20 }
        if ((Normalize-Path $candidate) -eq (Normalize-Path (Get-Location).Path)) { $score += 10 }
        if ($score -gt 0) {
            $ranked += [pscustomobject]@{ Root = $candidate; Score = $score }
        }
    }

    $best = $ranked | Sort-Object Score -Descending | Select-Object -First 1
    if ($best) {
        Write-Host "Auto-detected CDDA:" -ForegroundColor Green
        Write-Host "  Root:   $($best.Root)"
        Write-Host "  Target: $ExpectedCommit"
        return [string]$best.Root
    }

    throw "Could not auto-detect the requested CDDA build ($ExpectedCommit). Pass -GameRoot explicitly if it is installed in a custom location."
}

function Assert-NcmmPatchedSourceIntegrity([string]$Root) {
    Write-Host "Auditing patched CDDA source integrity before MSBuild..." -ForegroundColor Cyan
    $src = Join-Path $Root "src"
    $checks = @(
        @{ Path = (Join-Path $src 'magic.cpp'); Needle = 'static const char *ncmm_spell_hook_id('; Expected = 1; Name = 'generic spell runtime-hook mapper' },
        @{ Path = (Join-Path $src 'magic.cpp'); Needle = 'class ncmm_spell_source_scope'; Expected = 1; Name = 'generic spell source context' },
        @{ Path = (Join-Path $src 'character_knowledge.cpp'); Needle = 'static float ncmm_runtime_skill_bonus( const skill_id &ident )'; Expected = 1; Name = 'generic skill runtime hook' },
        @{ Path = (Join-Path $src 'creature.cpp'); Needle = 'combat.damage_to_species_pct'; Expected = 1; Name = 'generic creature species damage hook' },
        @{ Path = (Join-Path $src 'character_health.cpp'); Needle = 'combat.damage_avoid_pct'; Expected = 1; Name = 'generic full-damage avoidance hook' },
        @{ Path = (Join-Path $src 'creature.cpp'); Needle = 'combat.damage_taken_pct'; Expected = 1; Name = 'generic incoming damage reduction hook' },
        @{ Path = (Join-Path $src 'character.cpp'); Needle = 'combat.dodge_attempts_bonus'; Expected = 1; Name = 'generic extra dodge-attempt hook' },
        @{ Path = (Join-Path $src 'character.cpp'); Needle = 'combat.free_dodge_attempts_bonus'; Expected = 1; Name = 'generic free dodge-attempt hook' },
        @{ Path = (Join-Path $src 'character.cpp'); Needle = 'combat.block_attempts_bonus'; Expected = 1; Name = 'generic block-attempt hook' },
        @{ Path = (Join-Path $src 'melee.cpp'); Needle = 'combat.melee_crit_chance_pct'; Expected = 1; Name = 'generic melee critical-chance hook' },
        @{ Path = (Join-Path $src 'melee.cpp'); Needle = 'combat.melee_crit_damage_pct'; Expected = 1; Name = 'generic melee critical-damage hook' },
        @{ Path = (Join-Path $src 'ranged.cpp'); Needle = 'combat.ranged_crit_damage_pct'; Expected = 1; Name = 'generic ranged critical-damage hook' },
        @{ Path = (Join-Path $src 'character.cpp'); Needle = 'combat.riposte_chance_pct'; Expected = 1; Name = 'reactive riposte hook' },
        @{ Path = (Join-Path $src 'character.cpp'); Needle = 'combat.on_dodge_moves'; Expected = 1; Name = 'after-dodge move hook' },
        @{ Path = (Join-Path $src 'melee.cpp'); Needle = 'combat.on_crit_moves'; Expected = 1; Name = 'after-crit move hook' },
        @{ Path = (Join-Path $src 'creature.cpp'); Needle = 'combat.execute_threshold_pct'; Expected = 1; Name = 'execute-window hook' },
        @{ Path = (Join-Path $src 'creature.cpp'); Needle = 'combat.damage_dealt_pct'; Expected = 1; Name = 'generic outgoing damage hook' },
        @{ Path = (Join-Path $src 'monster.cpp'); Needle = 'runtime_player_kill_notify()'; Expected = 1; Name = 'named player-kill event dispatch' },
        @{ Path = (Join-Path $src 'crafting.cpp'); Needle = 'const double ncmm_failure_save = std::max( 0.0, std::min( 35.0,'; Expected = 1; Name = 'craft failure-save runtime hook' },
        @{ Path = (Join-Path $src 'crafting.cpp'); Needle = 'const double ncmm_catastrophic_save = std::max( 0.0, std::min( 35.0,'; Expected = 1; Name = 'craft failure-save UI hook' },
        @{ Path = (Join-Path $src 'crafting.cpp'); Needle = 'Keep the displayed estimate aligned with the actual NCMM-adjusted success roll.'; Expected = 1; Name = 'craft success-estimate parity hook' },
        @{ Path = (Join-Path $src 'crafting.cpp'); Needle = 'crafting.component_loss_reduction_pct'; Expected = 1; Name = 'craft component-protection hook' },
        @{ Path = (Join-Path $src 'activity_actor.cpp'); Needle = 'scavenging.lockpick_roll_flat'; Expected = 1; Name = 'lockpick roll hook' },
        @{ Path = (Join-Path $src 'activity_actor.cpp'); Needle = 'const int ncmm_lock_floor = ncmm_perfect_lockpick ?'; Expected = 1; Name = 'lockpick vanilla-floor preservation' },
        @{ Path = (Join-Path $src 'activity_actor.cpp'); Needle = 'to_moves<int>( 5_seconds ) : to_moves<int>( 30_seconds );'; Expected = 1; Name = 'lockpick vanilla-floor values' },
        @{ Path = (Join-Path $src 'activity_actor.cpp'); Needle = 'ncmm_lock_moves = std::max( ncmm_lock_floor,'; Expected = 1; Name = 'lockpick reduction floor clamp' },
        @{ Path = (Join-Path $src 'activity_actor.cpp'); Needle = 'scavenging.lockpick_alarm_avoid_pct'; Expected = 1; Name = 'lockpick alarm hook' },
        @{ Path = (Join-Path $src 'trap.cpp'); Needle = 'scavenging.trap_detection_flat'; Expected = 1; Name = 'trap detection hook' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'bool active_module_matches( const char *module_id );'; Expected = 1; Name = 'active_module_matches forward declaration' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = "bool active_module_matches( const char *module_id )`n{"; Expected = 1; Name = 'active_module_matches definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = "bool claim_world_setting( const char *module_id, const char *setting_id, uint32_t scope )`n{"; Expected = 1; Name = 'World Settings claim helper' },
        @{ Path = (Join-Path $src 'options.h'); Needle = 'bool ncmm_register_world_bool( const std::string &name'; Expected = 1; Name = 'options world-bool declaration' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'bool options_manager::ncmm_register_world_bool('; Expected = 1; Name = 'options world-bool implementation' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'static std::unordered_map<std::string, std::string> ncmm_deferred_option_values;'; Expected = 1; Name = 'deferred NCMM global option persistence' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'options.find( name ) == options.end() && name.rfind( "NCMM_", 0 ) == 0'; Expected = 1; Name = 'deferred NCMM deserialize gate' },
        @{ Path = (Join-Path $src 'worldfactory.cpp'); Needle = 'opts.get_option( name ).getPage() == "ncmm_experimental"'; Expected = 1; Name = 'experimental world-save restore' },
        @{ Path = (Join-Path $src 'options.h'); Needle = 'bool ncmm_get_option_bool_or( const std::string &name, bool fallback );'; Expected = 1; Name = 'safe option-bool declaration' },
        @{ Path = (Join-Path $src 'options.h'); Needle = 'int ncmm_get_option_int_or( const std::string &name, int fallback );'; Expected = 1; Name = 'safe option-int declaration' },
        @{ Path = (Join-Path $src 'options.h'); Needle = 'float ncmm_get_option_float_or( const std::string &name, float fallback );'; Expected = 1; Name = 'safe option-float declaration' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'NCMM v8.7.6.6 fail-safe accessor definitions'; Expected = 1; Name = 'safe option accessor definition marker' },
        @{ Path = (Join-Path $src 'options.h'); Needle = 'bool ncmm_begin_experimental_group( const std::string &group_id'; Expected = 1; Name = 'experimental-world page declaration' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'void options_manager::ncmm_ensure_experimental_page()'; Expected = 1; Name = 'experimental-world page implementation' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'ncmm::on_language_changed();'; Expected = 1; Name = 'NCMM locale-change dispatch from options' },
        @{ Path = (Join-Path $src 'options.cpp'); Needle = 'pages_.emplace( debug_page, "ncmm_experimental"'; Expected = 1; Name = 'experimental-world page insertion' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'int worldgen_experimental_group_begin('; Expected = 1; Name = 'experimental-world host bridge' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'int ui_card_choose_themed('; Expected = 1; Name = 'themed-card host entry' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'int ui_tree_choose_themed('; Expected = 1; Name = 'themed-tree host entry' },
        @{ Path = (Join-Path $src 'ncmm_api.h'); Needle = 'typedef struct ncmm_ui_theme_v1 {'; Expected = 1; Name = 'UI theme API type' },
        @{ Path = (Join-Path $src 'ncmm_api.h'); Needle = '#define NCMM_API_VERSION_MINOR 9u'; Expected = 1; Name = 'NCMM legacy bridge API 1.9 version' },
        @{ Path = (Join-Path $src 'ncmm_api.h'); Needle = '#define NCMM_HOST_API_V2_CORE_MAJOR 2u'; Expected = 1; Name = 'Host API 2.0 Core major' },
        @{ Path = (Join-Path $src 'ncmm_api.h'); Needle = 'typedef struct ncmm_host_api_v2_core {'; Expected = 1; Name = 'Host API 2.0 Core ABI table' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'const ncmm_host_api_v2_core api_v2_core = {'; Expected = 1; Name = 'Host API 2.0 Core table instance' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor );'; Expected = 1; Name = 'Host API 2.0 query interface declaration' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = "const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )`n{"; Expected = 1; Name = 'Host API 2.0 query interface definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = '    &query_interface_v2'; Expected = 1; Name = 'Host API 2.0 legacy v1 query-interface bridge' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'double runtime_hook_modifier( const char *hook_id, const char *subject_id,'; Expected = 1; Name = 'Host API 2.0 public runtime hook definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'void runtime_event_notify( uint32_t event_id )'; Expected = 1; Name = 'Host API 2.0 public runtime event notifier' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'void runtime_player_kill_notify()'; Expected = 1; Name = 'Host API 2.0 named player-kill notifier' },
        @{ Path = (Join-Path $src 'ncmm_loader.h'); Needle = 'void runtime_player_kill_notify();'; Expected = 1; Name = 'Host API 2.0 named player-kill declaration' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'std::string runtime_source_mod_swap( const std::string &source_mod_id )'; Expected = 1; Name = 'Host API 2.0 public source-mod swap definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'const std::string &runtime_source_mod()'; Expected = 1; Name = 'Host API 2.0 public source-mod getter definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'double runtime_hook_modifier_for_creatures( const char *hook_id,'; Expected = 1; Name = 'Host API 2.0 public creature runtime hook definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'bool worldgen_hook_bound( const char *hook_id )'; Expected = 1; Name = 'Host API 2.0 public worldgen bound definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'int worldgen_hook_bool( const char *hook_id, int fallback )'; Expected = 1; Name = 'Host API 2.0 public worldgen bool definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback )'; Expected = 1; Name = 'Host API 2.0 public worldgen int definition' },
        @{ Path = (Join-Path $src 'ncmm_loader.cpp'); Needle = 'double worldgen_hook_f64( const char *hook_id, double fallback )'; Expected = 1; Name = 'Host API 2.0 public worldgen float definition' }
    )
    foreach ($check in $checks) {
        if (-not (Test-Path $check.Path -PathType Leaf)) {
            throw "v8.7.3 patched-source audit missing file: $($check.Path)"
        }
        $body = Normalize-Lf ([IO.File]::ReadAllText($check.Path))
        $count = ([regex]::Matches($body,[regex]::Escape([string]$check.Needle))).Count
        if ($count -ne [int]$check.Expected) {
            throw "v8.7.3 patched-source audit failed: $($check.Name) expected $($check.Expected), found $count"
        }
    }

    $characterReactiveAudit = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'character.cpp')))
    foreach($needle0112 in @(
        'ncmm_vanilla_counter_fired','get_stamina() >= get_stamina_max() / 3','!source->is_dead_state()',
        '!source->is_hallucination()','source->attitude_to( *this ) == Creature::Attitude::HOSTILE'
    )) {
        if(-not $characterReactiveAudit.Contains($needle0112)) { throw "0.11.2 riposte edge audit missing: $needle0112" }
    }
    $meleeReactiveAudit = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'melee.cpp')))
    if(-not $meleeReactiveAudit.Contains('is_avatar() && ncmm_hostile_target_before && dam > 0 && !t.is_hallucination()')) { throw '0.11.3 hostile damaging-critical reward guard missing.' }
    $creatureReactiveAudit = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'creature.cpp')))
    if(-not $creatureReactiveAudit.Contains('source->is_avatar() && source != this')) { throw '0.11.2 self-damage guard missing.' }
    $monsterReactiveAudit = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'monster.cpp')))
    foreach($needleKill0112 in @('ch->is_avatar() && kill_xp > 0','ncmm_kill_attitude == MATT_ATTACK || ncmm_kill_attitude == MATT_FLEE')) {
        if(-not $monsterReactiveAudit.Contains($needleKill0112)) { throw "0.11.2 hostile/zero-XP kill gate missing: $needleKill0112" }
    }
    $activityReactiveAudit = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'activity_actor.cpp')))
    foreach($lockpickFloorNeedle0112 in @(
        'const bool ncmm_perfect_lockpick = lockpick != nullptr && lockpick->has_flag( flag_PERFECT_LOCKPICK );',
        'const int ncmm_lock_floor = ncmm_perfect_lockpick ?',
        'to_moves<int>( 5_seconds ) : to_moves<int>( 30_seconds );',
        'ncmm_lock_moves = std::max( ncmm_lock_floor,'
    )) {
        if(-not $activityReactiveAudit.Contains($lockpickFloorNeedle0112)) {
            throw "0.11.2 lockpick floor/null-safety audit missing: $lockpickFloorNeedle0112"
        }
    }

    $loaderBody = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'ncmm_loader.cpp')))
    $queryDeclNeedle = 'const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor );'
    $queryDefNeedle = "const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )`n{"
    $queryDeclPos = $loaderBody.IndexOf($queryDeclNeedle)
    $legacyApiPos = $loaderBody.IndexOf('const ncmm_host_api_v1 api = {')
    $queryDefPos = $loaderBody.IndexOf($queryDefNeedle)
    if ($queryDeclPos -lt 0 -or $legacyApiPos -lt 0 -or $queryDefPos -lt 0 -or
        $queryDeclPos -gt $legacyApiPos -or $legacyApiPos -gt $queryDefPos) {
        throw 'Host API 2.0 query-interface declaration/legacy-table/definition order is invalid.'
    }
    $anonNamespaceClosePos = $loaderBody.IndexOf('} // namespace')
    $publicRuntimeHookPos = $loaderBody.IndexOf('double runtime_hook_modifier( const char *hook_id, const char *subject_id,')
    $gameplayModifierPos = $loaderBody.IndexOf('double gameplay_modifier( const char *modifier_id )')
    if ($anonNamespaceClosePos -lt 0 -or $publicRuntimeHookPos -lt 0 -or $gameplayModifierPos -lt 0 -or
        $anonNamespaceClosePos -gt $publicRuntimeHookPos -or $publicRuntimeHookPos -gt $gameplayModifierPos) {
        throw 'Host API 2.0 public runtime/worldgen hooks are not in the external ncmm namespace after the anonymous namespace closes.'
    }
    $optionsHeaderBody = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'options.h')))
    $optionsCppBody = Normalize-Lf ([IO.File]::ReadAllText((Join-Path $src 'options.cpp')))
    if ($optionsHeaderBody.Contains('inline bool ncmm_get_option_bool_or') -or
        $optionsHeaderBody.Contains('inline int ncmm_get_option_int_or') -or
        $optionsHeaderBody.Contains('inline float ncmm_get_option_float_or')) {
        throw 'NCMM safe option getters must not instantiate cOpt::value_as<T> from options.h.'
    }
    $valueAsIntPos = $optionsCppBody.IndexOf('int options_manager::cOpt::value_as<int>( bool convert ) const')
    $safeGetterDefPos = $optionsCppBody.IndexOf('NCMM v8.7.6.6 fail-safe accessor definitions')
    if ($valueAsIntPos -lt 0 -or $safeGetterDefPos -lt 0 -or $safeGetterDefPos -lt $valueAsIntPos) {
        throw 'NCMM safe option getter definitions must follow cOpt::value_as<T> explicit specializations.'
    }

    $forwardPos = $loaderBody.IndexOf('bool active_module_matches( const char *module_id );')
    $claimPos = $loaderBody.IndexOf('bool claim_world_setting( const char *module_id, const char *setting_id, uint32_t scope )')
    if ($forwardPos -lt 0 -or $claimPos -lt 0 -or $forwardPos -gt $claimPos) {
        throw 'v8.7.3 World Settings active_module_matches declaration order is invalid.'
    }
    foreach ($capability in @('"active_mods.v1"','"active_mods.registry.v2"','"world_settings.v2"','"world_options.experimental.v1"','"ui.theme.v1"','"host_api.v2.core"','"events.core.v2"','"settings.typed.v2"','"character.modifiers.v2"','"runtime_hooks.registry.v2"','"worldgen.bindings.v2"','"module.lifecycle.query.v2"')) {
        $count = ([regex]::Matches($loaderBody,[regex]::Escape($capability))).Count
        if ($count -ne 1) {
            throw "v8.7.3 capability audit expected exactly one $capability in loader, found $count"
        }
    }
    Write-Host "Patched CDDA source integrity: PASS" -ForegroundColor Green
}

$GameRoot = Resolve-GameRoot $GameRoot $CddaCommit $CddaFolder
$earlyVanillaPath = Join-Path $GameRoot "cataclysm-tiles.vanilla.exe"
$earlyBindingPath = Join-Path $GameRoot "ncmm\host.binding.json"
$earlySourceCommit = Read-GameSourceCommit $GameRoot
$earlyBindingCommit = Read-GameBindingCommit $GameRoot
if (-not [string]::IsNullOrWhiteSpace($earlySourceCommit) -and $earlySourceCommit -ne $CddaCommit) {
    throw "Target game source commit mismatch before build. Expected $CddaCommit, got $earlySourceCommit"
}
if ($HostSourceProbeOnly) {
    if ($earlySourceCommit -ne $CddaCommit) {
        throw "Deep source probe requires VERSION.txt to verify exact target commit $CddaCommit."
    }
    Write-Host "Target game root: $GameRoot"
    Write-Host "Target game exact source commit: VERIFIED (deep source probe; runtime binding not required)" -ForegroundColor Green
} else {
    $earlyHasVanilla = Test-Path $earlyVanillaPath -PathType Leaf
    $earlyHasBinding = Test-Path $earlyBindingPath -PathType Leaf
    $earlyFreshVanilla = (-not $earlyHasVanilla) -and (-not $earlyHasBinding)
    if ($earlyHasVanilla -xor $earlyHasBinding) {
        throw "Target NCMM runtime is partially installed (vanilla backup/binding mismatch). Repair or restore the previous NCMM installation before continuing: $GameRoot"
    }
    if ($earlyHasBinding) {
        try {
            $null = Get-Content $earlyBindingPath -Raw | ConvertFrom-Json
        } catch {
            throw "Existing ncmm\host.binding.json is malformed: $($_.Exception.Message)"
        }
        if (-not [string]::IsNullOrWhiteSpace($earlyBindingCommit) -and $earlyBindingCommit -ne $CddaCommit) {
            throw "Existing NCMM binding targets a different CDDA source commit before build: $earlyBindingCommit"
        }
    } elseif (-not (Test-Path (Join-Path $GameRoot "cataclysm-tiles.exe") -PathType Leaf)) {
        throw "Fresh NCMM install requires the original cataclysm-tiles.exe: $GameRoot"
    }
    Write-Host "Target game root: $GameRoot"
    if ($earlySourceCommit -eq $CddaCommit) {
        Write-Host "Target game exact source commit: VERIFIED" -ForegroundColor Green
    } elseif ($earlyBindingCommit -eq $CddaCommit) {
        Write-Host "Target game exact NCMM binding commit: VERIFIED" -ForegroundColor Green
    } else {
        Write-Host "Target game commit metadata unavailable; exact source validation remains mandatory before install." -ForegroundColor Yellow
    }
    if ($earlyFreshVanilla) {
        Write-Host "NCMM runtime state: FRESH VANILLA TARGET (bootstrap/host will be installed transactionally)" -ForegroundColor Cyan
    }
}

# v4 portability fix: this can be a completely fresh PC.  The build root must
# exist before Find-Vs() writes its diagnostic report or before source bootstrap.
New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null

$SeedBranch = "survivor-099-mouse-graphical-tree-validation"
$SeedCommit = "c15fbff6dffbded6382a638288f3984228873fa4"
$Seed099 = Join-Path $BuildRoot "ncmm_seed_099_immutable"
$SeedMarker = Join-Path $Seed099 ".ncmm_seed_commit"
$Work0910 = Join-Path $BuildRoot "ncmm_0910_work"
$NcmmRoot = $Work0910
$CddaRoot = Join-Path $BuildRoot ($CddaCacheKey + "_ncmm")
$VcpkgRoot = Join-Path $BuildRoot "vcpkg"
$Source0910 = Join-Path $BuildRoot "survivor_0910_src"
$Snap0910 = Join-Path $BuildRoot "ncmm_0910_snapshot"
$Snap0911 = Join-Path $BuildRoot "ncmm_0911_snapshot"
$Snap0912 = Join-Path $BuildRoot "ncmm_0912_snapshot"
$Snap0913 = Join-Path $BuildRoot "ncmm_0913_snapshot"
$Snap0914 = Join-Path $BuildRoot "ncmm_0914_snapshot"
$Snap0915 = Join-Path $BuildRoot "ncmm_0915_snapshot"
$ReleaseRoot = Join-Path $BuildRoot "SurvivorProgression_Releases"

Write-Host "=== Survivor Progression 0.9.11-0.9.15 - Prime Specialization / Registry Installer v8.7.6.8 ===" -ForegroundColor Cyan
Write-Host "NCMM Infrastructure adapter target: $CddaTag / $CddaCommit" -ForegroundColor DarkCyan
Write-Host "Adapter cache key: $CddaCacheKey" -ForegroundColor DarkCyan
Write-Host "Portable source bootstrap: exact pinned GitHub 0.9.9 commit."
Write-Host "Seed commit: $SeedCommit"
Write-Host ""

Write-Host "Source transform/audit preflight runs before Visual Studio, CDDA and vcpkg work."
Write-Host "Project source Git: NOT REQUIRED; vcpkg bootstrap Git is auto-detected/installed."

function Test-ExactSeed([string]$Destination,[string]$Commit,[string]$MarkerPath) {
    if (-not (Test-Path $MarkerPath -PathType Leaf)) {
        return $false
    }
    $cached = (Get-Content $MarkerPath -Raw).Trim()
    if ($cached -ne $Commit) {
        return $false
    }

    $module = Join-Path $Destination "mods\SurvivorProgression\src\survivor_progression.cpp"
    $manifest = Join-Path $Destination "mods\SurvivorProgression\mod.json"
    $loader = Join-Path $Destination "host_patch\ncmm_loader.cpp"
    $sdk = Join-Path $Destination "sdk\ncmm_api.h"
    $apply = Join-Path $Destination "host_patch\Apply-NCMMHostPatch.ps1"
    $revision = Join-Path $Destination "ci\Get-PatchRevision.ps1"
    $runtime = Join-Path $Destination "runtime\NCMMBootstrap.cs"
    $smoke = Join-Path $Destination "tests\smoke_host.cpp"
    $contracts = Join-Path $Destination "compat\contracts.json"
    $sourceContracts = Join-Path $Destination "ci\Test-SourceContracts.ps1"
    $hostPackage = Join-Path $Destination "ci\Build-HostPackage.ps1"

    foreach ($p in @($module,$manifest,$loader,$sdk,$apply,$revision,$runtime,$smoke,$contracts,$sourceContracts,$hostPackage)) {
        if (-not (Test-Path $p -PathType Leaf)) {
            return $false
        }
    }

    try {
        $meta = Get-Content $manifest -Raw | ConvertFrom-Json
        if ([string]$meta.version -ne "0.9.9") {
            return $false
        }

        $loaderText = [IO.File]::ReadAllText($loader)
        $sdkText = [IO.File]::ReadAllText($sdk)
        $moduleText = [IO.File]::ReadAllText($module)

        # Validate REAL 0.9.9 features, not one fragile comment string.
        foreach ($needle in @(
            "int ui_tree_choose(",
            "LINE_XXXX",
            'ctxt.register_action( "MOUSE_MOVE" )',
            "NCMM_UI_TREE_SHOW_CARDS"
        )) {
            if (-not $loaderText.Contains($needle)) {
                return $false
            }
        }

        if (-not $sdkText.Contains("NCMM_UI_CARD_SHOW_TREE")) {
            return $false
        }
        if (-not $moduleText.Contains("Survivor Progression v0.9.9")) {
            return $false
        }
    } catch {
        return $false
    }

    return $true
}

function Download-ExactSeed([string]$Commit,[string]$Destination,[string]$MarkerPath) {
    if (Test-ExactSeed $Destination $Commit $MarkerPath) {
        Write-Host "Exact immutable 0.9.9 seed: OK" -ForegroundColor Green
        return
    }

    if (Test-Path $Destination) {
        Write-Host "Cached 0.9.9 seed is stale/modified; refreshing it..." -ForegroundColor Yellow
        Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Downloading exact immutable 0.9.9 source ZIP from GitHub..." -ForegroundColor Cyan
    $tmpRoot = Join-Path $env:TEMP ("NCMM_SEED099_" + [Guid]::NewGuid().ToString("N"))
    $zipPath = Join-Path $tmpRoot "seed.zip"
    $extract = Join-Path $tmpRoot "extract"
    New-Item -ItemType Directory -Force $extract | Out-Null

    $url = "https://github.com/Neversalimus/NCMM/archive/$Commit.zip"
    try {
        try {
            Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zipPath -ErrorAction Stop
        } catch {
            Write-Host "Invoke-WebRequest failed, trying WebClient..." -ForegroundColor Yellow
            Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue
            $client = New-Object Net.WebClient
            $client.Headers.Add("User-Agent","NCMM-Survivor-0910")
            $client.DownloadFile($url,$zipPath)
        }

        if (-not [IO.File]::Exists($zipPath)) {
            throw "Downloaded seed ZIP is missing."
        }
        $seedZipLength = (Get-Item -LiteralPath $zipPath).Length
        if ($seedZipLength -lt 10000) {
            throw "Downloaded seed ZIP is suspiciously small ($seedZipLength bytes)."
        }

        Expand-Archive -Path $zipPath -DestinationPath $extract -Force
        $root = Get-ChildItem $extract -Directory | Select-Object -First 1
        if (-not $root) {
            throw "Downloaded seed ZIP did not contain a source root."
        }

        Remove-Item $Destination -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force $Destination | Out-Null
        Copy-Item (Join-Path $root.FullName "*") $Destination -Recurse -Force
        Write-Utf8NoBom $MarkerPath ($Commit + "`n")

        if (-not (Test-ExactSeed $Destination $Commit $MarkerPath)) {
            throw "Freshly downloaded 0.9.9 seed failed integrity validation."
        }
    } finally {
        Remove-Item $tmpRoot -Recurse -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Exact immutable 0.9.9 seed: READY" -ForegroundColor Green
}

Download-ExactSeed $SeedCommit $Seed099 $SeedMarker
Write-Host "0.9.9 seed commit: $SeedCommit"

$src099Module = Join-Path $Seed099 "mods\SurvivorProgression\src\survivor_progression.cpp"
$src099Manifest = Join-Path $Seed099 "mods\SurvivorProgression\mod.json"
$src099Loader = Join-Path $Seed099 "host_patch\ncmm_loader.cpp"
$src099Sdk = Join-Path $Seed099 "sdk\ncmm_api.h"

# IMPORTANT: never patch the immutable seed itself.
# Every run starts from a clean mutable work copy.
Write-Host "Preparing clean 0.9.10 working copy..." -ForegroundColor Cyan
Remove-Item $Work0910 -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $Work0910 | Out-Null
Copy-Item (Join-Path $Seed099 "*") $Work0910 -Recurse -Force

foreach ($p in @(
    (Join-Path $NcmmRoot "mods\SurvivorProgression\src\survivor_progression.cpp"),
    (Join-Path $NcmmRoot "mods\SurvivorProgression\mod.json"),
    (Join-Path $NcmmRoot "host_patch\ncmm_loader.cpp"),
    (Join-Path $NcmmRoot "sdk\ncmm_api.h"),
    (Join-Path $NcmmRoot "host_patch\Apply-NCMMHostPatch.ps1"),
    (Join-Path $NcmmRoot "ci\Get-PatchRevision.ps1")
)) {
    if (-not (Test-Path $p -PathType Leaf)) {
        throw "0.9.10 working copy is incomplete: $p"
    }
}

# Fresh module source copied from exact 0.9.9, then advanced cumulatively through 0.9.10 -> 0.9.14.
Remove-Item $Source0910 -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $Source0910 "src") | Out-Null
Copy-Item (Join-Path $NcmmRoot "mods\SurvivorProgression\src\survivor_progression.cpp") `
          (Join-Path $Source0910 "src\survivor_progression.cpp") -Force
Copy-Item (Join-Path $NcmmRoot "mods\SurvivorProgression\mod.json") `
          (Join-Path $Source0910 "mod.json") -Force

$loaderPath = Join-Path $NcmmRoot "host_patch\ncmm_loader.cpp"
$loader = [IO.File]::ReadAllText($loaderPath)

$connectorStart = "        // Build dependency connectors as a real box-drawing graph."
$connectorEnd = "        for( size_t i = 0; i < node_count; ++i ) {"

$newConnectors = @'
        // 0.9.10: route dependency lines through the two-character gutters
        // between node columns.  Long vertical runs no longer pass through
        // unrelated perk tiles.
        constexpr int edge_n = 1;
        constexpr int edge_e = 2;
        constexpr int edge_s = 4;
        constexpr int edge_w = 8;
        const int grid_size = frame_width * frame_height;
        std::vector<int> edge_mask( static_cast<size_t>( grid_size ), 0 );
        std::vector<int> edge_style( static_cast<size_t>( grid_size ), 0 );
        std::vector<std::array<int, 4>> blocked_rects;
        blocked_rects.reserve( node_count );

        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) {
                continue;
            }
            const int bx = node_x( i );
            const int by = node_y( i );
            blocked_rects.push_back( {{ bx, by, bx + node_width - 1, by + node_height - 1 }} );
        }

        auto grid_index = [&]( int x, int y ) {
            return y * frame_width + x;
        };
        auto is_blocked = [&]( int x, int y ) {
            for( const std::array<int, 4> &r : blocked_rects ) {
                if( x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3] ) {
                    return true;
                }
            }
            return false;
        };
        auto mark_dir = [&]( int x, int y, int dir, int style ) {
            if( x <= 0 || x >= divider_x || y < header_height ||
                y >= frame_height - footer_height || is_blocked( x, y ) ) {
                return;
            }
            const int index = grid_index( x, y );
            edge_mask[index] |= dir;
            edge_style[index] = std::max( edge_style[index], style );
        };
        auto connect_cells = [&]( int ax, int ay, int bx, int by, int style ) {
            if( ax == bx && by == ay + 1 ) {
                mark_dir( ax, ay, edge_s, style );
                mark_dir( bx, by, edge_n, style );
            } else if( ax == bx && by == ay - 1 ) {
                mark_dir( ax, ay, edge_n, style );
                mark_dir( bx, by, edge_s, style );
            } else if( ay == by && bx == ax + 1 ) {
                mark_dir( ax, ay, edge_e, style );
                mark_dir( bx, by, edge_w, style );
            } else if( ay == by && bx == ax - 1 ) {
                mark_dir( ax, ay, edge_w, style );
                mark_dir( bx, by, edge_e, style );
            }
        };
        auto style_for_node = [&]( size_t i ) {
            if( nodes[i].flags & NCMM_UI_CARD_OWNED ) return 4;
            if( nodes[i].flags & NCMM_UI_CARD_MAJOR ) return 3;
            if( nodes[i].flags & NCMM_UI_CARD_EFFECT ) return 2;
            if( nodes[i].flags & NCMM_UI_CARD_LOCKED ) return 0;
            return 1;
        };
        auto color_for_style = [&]( int style ) {
            switch( style ) {
                case 4: return c_cyan;
                case 3: return c_yellow;
                case 2: return c_magenta;
                case 1: return c_light_gray;
                default: return c_dark_gray;
            }
        };
        auto glyph_for_mask = [&]( int mask ) -> int {
            const bool n = ( mask & edge_n ) != 0;
            const bool e = ( mask & edge_e ) != 0;
            const bool s = ( mask & edge_s ) != 0;
            const bool w = ( mask & edge_w ) != 0;
            if( n && e && s && w ) return LINE_XXXX;
            if( n && e && s ) return LINE_XXXO;
            if( n && e && w ) return LINE_XXOX;
            if( n && s && w ) return LINE_XOXX;
            if( e && s && w ) return LINE_OXXX;
            if( n && e ) return LINE_XXOO;
            if( e && s ) return LINE_OXXO;
            if( s && w ) return LINE_OOXX;
            if( n && w ) return LINE_XOOX;
            if( n || s ) return LINE_XOXO;
            return LINE_OXOX;
        };

        std::vector<bool> has_incoming( node_count, false );
        std::vector<bool> has_outgoing( node_count, false );

        for( size_t e = 0; e < edge_count; ++e ) {
            const size_t from = edges[e].from_index;
            const size_t to = edges[e].to_index;
            if( !visible( from ) || !visible( to ) ) {
                continue;
            }

            has_outgoing[from] = true;
            has_incoming[to] = true;

            const int from_left = node_x( from );
            const int from_right = from_left + node_width - 1;
            const int x1 = from_left + node_width / 2;
            const int y1 = node_y( from ) + node_height;
            const int x2 = node_x( to ) + node_width / 2;
            const int y2 = node_y( to ) - 1;
            if( y2 < y1 ) {
                continue;
            }

            const int style = style_for_node( to );

            auto vertical_clear = [&]( int x, int ya, int yb ) {
                if( x <= 0 || x >= divider_x ) {
                    return false;
                }
                if( yb < ya ) {
                    std::swap( ya, yb );
                }
                for( int y = ya; y <= yb; ++y ) {
                    if( is_blocked( x, y ) ) {
                        return false;
                    }
                }
                return true;
            };

            std::vector<int> candidates;
            candidates.reserve( static_cast<size_t>( divider_x ) );
            const int preferred = x2 > x1 ? from_right + 1 :
                                  x2 < x1 ? from_left - 1 :
                                  from_right + 1;
            for( int distance = 0; distance < divider_x; ++distance ) {
                const int right = preferred + distance;
                const int left = preferred - distance;
                if( right > 0 && right < divider_x ) {
                    candidates.push_back( right );
                }
                if( distance > 0 && left > 0 && left < divider_x ) {
                    candidates.push_back( left );
                }
            }

            int route_x = -1;
            for( const int candidate : candidates ) {
                if( vertical_clear( candidate, y1, y2 ) ) {
                    route_x = candidate;
                    break;
                }
            }
            if( route_x < 0 ) {
                route_x = x2 >= x1 ? divider_x - 1 : 1;
            }

            int cx = x1;
            int cy = y1;

            while( cx != route_x ) {
                const int nx = cx + ( route_x > cx ? 1 : -1 );
                connect_cells( cx, cy, nx, cy, style );
                cx = nx;
            }
            while( cy != y2 ) {
                const int ny = cy + ( y2 > cy ? 1 : -1 );
                connect_cells( cx, cy, cx, ny, style );
                cy = ny;
            }
            while( cx != x2 ) {
                const int nx = cx + ( x2 > cx ? 1 : -1 );
                connect_cells( cx, cy, nx, cy, style );
                cx = nx;
            }
        }

        for( int y = header_height; y < frame_height - footer_height; ++y ) {
            for( int x = 1; x < divider_x; ++x ) {
                const int index = grid_index( x, y );
                if( edge_mask[index] == 0 ) {
                    continue;
                }
                const nc_color color = color_for_style( edge_style[index] );
                wattron( frame, color );
                mvwaddch( frame, point( x, y ), glyph_for_mask( edge_mask[index] ) );
                wattroff( frame, color );
            }
        }
'@

$loader = Replace-CppRange $loader $connectorStart $connectorEnd $newConnectors "0.9.10 safe connector routing"
Write-Utf8NoBom $loaderPath $loader

# ---------------------------------------------------------------------------
# Module: ranked foundational perks. Existing p_<perk> state becomes rank value.
# Existing 0/1 saves remain valid: 1 simply means rank I.
# ---------------------------------------------------------------------------
$spPath = Join-Path $Source0910 "src\survivor_progression.cpp"
$sp = [IO.File]::ReadAllText($spPath)

$sp = $sp.Replace('"0.9.9"','"0.9.10"')
$sp = $sp.Replace("Survivor Progression v0.9.9","Survivor Progression v0.9.10")
$sp = $sp.Replace("Survivor Progression 0.9.9 initialized:","Survivor Progression 0.9.10 initialized:")

$rankHelpers = @'
struct ranked_perk_rule {
    const char *id;
    int max_rank;
    double extra_scale;
};

const ranked_perk_rule *ranked_perk_rule_for( const perk_def &perk )
{
    static const ranked_perk_rule rules[] = {
        { "c_conditioning", 5, 0.125 }, { "s_field", 3, 0.50 },
        { "m_light", 5, 1.0 / 6.0 }, { "f_hands", 5, 0.40 },
        { "g_route", 3, 1.0 / 3.0 }, { "a_adapt", 5, 0.20 },

        { "mg_arcane_focus", 5, 0.125 }, { "mg_spellcraft_drills", 5, 0.125 },
        { "mg_mana_sensitivity", 3, 0.25 }, { "mg_mana_regeneration", 3, 0.25 },
        { "mom_mental_focus", 5, 0.125 }, { "mom_metaphysical_method", 5, 0.125 },
        { "mom_neural_reserve", 3, 0.25 }, { "mom_channel_discipline", 3, 0.25 },
        { "xe_anomaly_method", 5, 0.125 }, { "xe_gramarye_studies", 5, 0.125 },
        { "xe_field_agent", 3, 0.25 }, { "xe_dimensional_model", 3, 0.25 },
        { "af_systems_operator", 5, 0.125 }, { "af_targeting_link", 5, 0.125 },
        { "af_metaphysical_training", 5, 0.125 }, { "af_conditioning", 3, 0.25 },

        { "afp_prime_operator", 5, 0.125 }, { "afp_smartgun_interface", 5, 0.125 },
        { "afp_systems_theory", 3, 0.25 }, { "afp_translocation_calculus", 3, 0.25 },
        { "sec_field_researcher", 5, 0.125 }, { "sec_hunter_drills", 5, 0.125 },
        { "sec_pathogen_hardening", 3, 0.25 }, { "sec_crimson_anatomy", 3, 0.25 },
        { "secx_flesh_initiate", 5, 0.125 }, { "secx_flesh_weaving", 5, 0.125 },
        { "secx_biomorph_training", 5, 0.125 }, { "secx_neural_link", 3, 0.25 }
    };
    const std::string id = perk.id ? perk.id : "";
    for( const ranked_perk_rule &rule : rules ) {
        if( id == rule.id ) return &rule;
    }
    return nullptr;
}

bool ranked_perk_id( const perk_def &perk )
{
    return ranked_perk_rule_for( perk ) != nullptr;
}

int perk_max_rank( const perk_def &perk )
{
    const ranked_perk_rule *rule = ranked_perk_rule_for( perk );
    return rule ? rule->max_rank : 1;
}

double perk_extra_rank_scale( const perk_def &perk )
{
    const ranked_perk_rule *rule = ranked_perk_rule_for( perk );
    return rule ? rule->extra_scale : 0.0;
}

int perk_rank( const perk_def &perk )
{
    return static_cast<int>( std::max<int64_t>(
        0, std::min<int64_t>( perk_max_rank( perk ), get_state( perk_key( perk ), 0 ) ) ) );
}

bool perk_maxed( const perk_def &perk )
{
    return perk_rank( perk ) >= perk_max_rank( perk );
}

double perk_rank_multiplier_for( const perk_def &perk, int rank )
{
    if( rank <= 0 ) {
        return 0.0;
    }
    rank = std::min( rank, perk_max_rank( perk ) );
    return 1.0 + static_cast<double>( rank - 1 ) * perk_extra_rank_scale( perk );
}

double perk_rank_multiplier( const perk_def &perk )
{
    return perk_rank_multiplier_for( perk, perk_rank( perk ) );
}

std::string rank_roman( int rank )
{
    switch( rank ) {
        case 1: return "I";
        case 2: return "II";
        case 3: return "III";
        case 4: return "IV";
        case 5: return "V";
        default: return std::to_string( rank );
    }
}

std::string rank_chevrons( const perk_def &perk )
{
    const int max_rank = perk_max_rank( perk );
    if( max_rank <= 1 ) return {};
    const int rank = perk_rank( perk );
    std::string result;
    for( int i = 0; i < max_rank; ++i ) {
        result += i < rank ? "▲" : "△";
    }
    return result;
}

std::string perk_display_name( const perk_def &perk )
{
    std::string result = russian() ? perk.name_ru : perk.name_en;
    const int rank = perk_rank( perk );
    if( perk_max_rank( perk ) > 1 && rank > 0 ) {
        result += " " + rank_roman( rank );
    }
    return result;
}

bool owned( const perk_def &perk )
{
    return perk_rank( perk ) > 0;
}
'@

$ownedStart = "bool owned( const perk_def &perk )"
$ownedEnd = "const perk_def *find_perk"
$sp = Replace-CppRange $sp $ownedStart $ownedEnd $rankHelpers "rank helpers"

$effectHelpers = @'
std::string ranked_effect_summary( const perk_def &perk, int rank )
{
    if( rank <= 0 ) {
        return {};
    }

    const double multiplier = perk_rank_multiplier_for( perk, rank );
    std::vector<std::string> parts;
    for( int i = 0; i < perk.effect_count; ++i ) {
        if( perk.effects[i].id == nullptr ) {
            continue;
        }
        const double value = perk.effects[i].value * multiplier;
        const std::string sign = value > 0.0 ? "+" : "";
        parts.push_back( effect_label( perk.effects[i].id ) + ": " +
                         sign + format_number( value ) );
    }
    if( perk.xp_bonus_pct != 0 ) {
        const int value = static_cast<int>(
                              std::llround( static_cast<double>( perk.xp_bonus_pct ) * multiplier ) );
        parts.push_back( tr( "Survivor XP: +", "Опыт Survivor: +" ) +
                         std::to_string( value ) + "%" );
    }

    std::string result;
    for( size_t i = 0; i < parts.size(); ++i ) {
        if( i != 0 ) {
            result += ", ";
        }
        result += parts[i];
    }
    return result;
}

std::string perk_description( const perk_def &perk )
{
    std::string result = russian() ? perk.desc_ru : perk.desc_en;
    const int max_rank = perk_max_rank( perk );
    if( max_rank <= 1 ) {
        return result;
    }

    const int rank = perk_rank( perk );
    result += "\n" + tr( "Rank ", "Ранг " ) + std::to_string( rank ) + "/" +
              std::to_string( max_rank );

    if( rank > 0 ) {
        result += "\n" + tr( "Current: ", "Сейчас: " ) +
                  ranked_effect_summary( perk, rank );
    }
    if( rank < max_rank ) {
        result += "\n" + tr( "Next: ", "Следующий: " ) +
                  ranked_effect_summary( perk, rank + 1 );
    }
    return result;
}

'@

$calcMarker = "struct calculated_effects {"
$calcPos = $sp.IndexOf($calcMarker)
if ($calcPos -lt 0) { throw "calculated_effects marker missing." }
$sp = $sp.Substring(0,$calcPos) + $effectHelpers + $sp.Substring($calcPos)

$newCalculate = @'
calculated_effects calculate_owned_effects()
{
    calculated_effects result;
    std::array<bool, 6> branch_active = {{ false, false, false, false, false, false }};

    // Pass 1: one ownership lookup per perk.  The old implementation scanned the
    // full catalog once per branch, then again for majors and amplifier effects.
    for( const perk_def &perk : perks ) {
        if( !perk_world_available( perk ) ) {
            continue;
        }
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }
        if( !integration_perk( perk ) ) {
            branch_active[branch_index( perk.branch )] = true;
        }
        if( perk.currency == currency_id::major ) {
            ++result.major_owned;
        }
        if( effective_kind( perk ) == perk_kind::effect ) {
            const double rank_scale = perk_rank_multiplier_for( perk, rank );
            result.branch_amp[branch_index( perk.branch )] +=
                perk.branch_amp_pct * rank_scale / 100.0;
            result.global_amp += perk.global_amp_pct * rank_scale / 100.0;
        }
    }
    for( bool active : branch_active ) {
        if( active ) {
            ++result.active_branches;
        }
    }

    // Pass 2: resolve scaling after active-branch and major totals are known.
    for( const perk_def &perk : perks ) {
        if( !perk_world_available( perk ) ) {
            continue;
        }
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }

        double scale = 1.0;
        if( perk.scaling == perk_scaling::per_active_branch ) {
            scale = static_cast<double>( result.active_branches );
        } else if( perk.scaling == perk_scaling::per_owned_major ) {
            scale = static_cast<double>( result.major_owned );
        }

        scale *= perk_rank_multiplier_for( perk, rank );

        if( effective_kind( perk ) == perk_kind::stat ) {
            scale *= result.global_amp * result.branch_amp[branch_index( perk.branch )];
        }

        result.xp_bonus_pct += static_cast<int>( std::llround( perk.xp_bonus_pct * scale ) );
        for( int i = 0; i < perk.effect_count; ++i ) {
            if( perk.effects[i].id != nullptr ) {
                result.modifiers[perk.effects[i].id] += perk.effects[i].value * scale;
            }
        }
    }

    result.xp_bonus_pct = std::max( 0, std::min( 5000, result.xp_bonus_pct ) );
    return result;
}
'@


$sp = Replace-CppRange $sp "calculated_effects calculate_owned_effects()" `
    "std::map<std::string, double> owned_effect_totals()" $newCalculate "ranked effect calculation"

$newStatus = @'
std::string status_prefix( const perk_def &perk, int64_t level, int64_t perk_points, int64_t major_points )
{
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );
    const std::string chevrons = rank_chevrons( perk );
    const std::string rank_prefix = chevrons.empty() ? std::string() : "[" + chevrons + "] ";
    if( rank >= max_rank ) {
        return rank_prefix + "[✓] ";
    }
    if( level < perk.required_level ) {
        return rank_prefix + std::string( russian() ? "[УР " : "[L" ) +
               std::to_string( perk.required_level ) + "] ";
    }
    if( !prerequisites_met( perk ) ) {
        return rank_prefix + ( russian() ? "[ТРЕБ.] " : "[REQ] " );
    }
    const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;
    if( !enough ) {
        return rank_prefix + ( russian() ? "[НЕТ ОЧКОВ] " : "[NO POINTS] " );
    }
    return rank_prefix + ( perk.currency == currency_id::perk ? "[1P] " : "[1M] " );
}
'@
$sp = Replace-CppRange $sp "std::string status_prefix(" "std::string cost_text(" $newStatus "ranked status prefix"

$newPurchase = @'
bool purchase_perk( const perk_def &perk )
{
    const int64_t level = branch_level( perk.branch );
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( rank >= max_rank ) {
        message( tr( "This perk is already at maximum rank.",
                     "Этот перк уже максимального ранга." ) );
        return false;
    }
    if( level < perk.required_level ) {
        message( tr( "Your branch level is too low.", "Недостаточный уровень этой ветки." ) );
        return false;
    }
    if( !prerequisites_met( perk ) ) {
        message( tr( "Prerequisites are not met.", "Не выполнены требования предыдущих перков." ) );
        return false;
    }

    if( perk.currency == currency_id::perk ) {
        if( perk_points <= 0 ) {
            message( tr( "Not enough perk points.", "Недостаточно очков перков." ) );
            return false;
        }
        set_state( "perk_points", perk_points - 1 );
    } else {
        if( major_points <= 0 ) {
            message( tr( "Not enough major points.", "Недостаточно больших очков." ) );
            return false;
        }
        set_state( "major_points", major_points - 1 );
    }

    set_state( perk_key( perk ), rank + 1 );
    effects_dirty = true;
    recalculate_effects();

    std::string text = rank == 0 ?
                       tr( "Perk purchased: ", "Куплен перк: " ) :
                       tr( "Perk upgraded: ", "Перк улучшен: " );
    text += russian() ? perk.name_ru : perk.name_en;
    if( max_rank > 1 ) {
        text += " " + std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
    }
    message( text );
    return true;
}
'@
$sp = Replace-CppRange $sp "bool purchase_perk(" "void show_perk_detail(" $newPurchase "ranked purchase"

$newDetail = @'
void show_perk_detail( const perk_def &perk )
{
    while( true ) {
        const int level = static_cast<int>( branch_level( perk.branch ) );
        const int rank = perk_rank( perk );
        const int max_rank = perk_max_rank( perk );
        const bool maxed = rank >= max_rank;
        const bool unlocked = level >= perk.required_level && prerequisites_met( perk );

        std::string title = perk_display_name( perk );
        title += "\n" + perk_description( perk );
        title += "\n" + tr( "Tier ", "Тир " ) + std::to_string( perk.tier );
        title += " | " + tr( "Requires branch level ", "Нужен уровень ветки " ) +
                 std::to_string( perk.required_level );
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );

        std::string buy;
        if( maxed ) {
            buy = tr( "[Maximum rank]", "[Максимальный ранг]" );
        } else if( !unlocked ) {
            buy = tr( "Locked", "Закрыто" );
        } else if( rank > 0 ) {
            buy = tr( "Upgrade to rank ", "Улучшить до ранга " ) +
                  std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
        } else {
            buy = tr( "Purchase", "Купить" );
        }

        std::string back = tr( "Back", "Назад" );
        const char *entries[] = { buy.c_str(), back.c_str() };
        const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
        if( choice != 0 ) {
            return;
        }
        if( maxed ) {
            return;
        }
        if( !unlocked ) {
            message( tr( "This perk is locked.", "Этот перк пока закрыт." ) );
            continue;
        }
        purchase_perk( perk );
        return;
    }
}
'@
$sp = Replace-CppRange $sp "void show_perk_detail(" "int branch_unlocked_count(" $newDetail "ranked perk detail"

$newUnlocked = @'
int branch_unlocked_count( branch_id branch, int64_t )
{
    const int64_t level = branch_level( branch );
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !perk_maxed( perk ) &&
            level >= perk.required_level && prerequisites_met( perk ) ) {
            ++result;
        }
    }
    return result;
}
'@
$sp = Replace-CppRange $sp "int branch_unlocked_count(" "struct card_text {" $newUnlocked "ranked unlocked count"

$newShowBranch = @'
void show_branch( branch_id branch )
{
    bool tree_mode = true;

    while( true ) {
        const int64_t level = branch_level( branch );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        std::vector<const perk_def *> branch_perks;
        branch_perks.reserve( 24 );
        std::vector<card_text> texts;
        texts.reserve( 24 );

        for( const perk_def &perk : perks ) {
            if( perk.branch != branch ) {
                continue;
            }
            branch_perks.push_back( &perk );

            const int rank = perk_rank( perk );
            const int max_rank = perk_max_rank( perk );
            const bool maxed = rank >= max_rank;
            const bool unlocked = level >= perk.required_level && prerequisites_met( perk );
            const bool enough = perk.currency == currency_id::perk ?
                                perk_points > 0 : major_points > 0;

            card_text card;
            card.id = perk.id;
            card.title = perk_display_name( perk );
            card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                            tr( "Lv ", "Ур " ) + std::to_string( perk.required_level ) +
                            " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
            if( max_rank > 1 ) {
                card.subtitle += " | " + tr( "R ", "Р " ) +
                                 std::to_string( rank ) + "/" + std::to_string( max_rank );
            }
            card.body = perk_description( perk );

            card.badge = perk_kind_label( perk );
            const std::string chevrons = rank_chevrons( perk );
            if( !chevrons.empty() ) card.badge += " | " + chevrons;
            card.badge += " | ";
            if( maxed ) {
                card.badge += max_rank > 1 ?
                              tr( "MAX ", "МАКС " ) + std::to_string( rank ) + "/" +
                              std::to_string( max_rank ) :
                              tr( "OWNED", "КУПЛЕНО" );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( rank > 0 ) {
                card.badge += tr( "RANK ", "РАНГ " ) + std::to_string( rank ) + "/" +
                              std::to_string( max_rank );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( !unlocked ) {
                card.badge += tr( "LOCKED", "ЗАКРЫТО" );
                card.flags |= NCMM_UI_CARD_LOCKED;
            } else if( !enough ) {
                card.badge += tr( "NO POINTS", "НЕТ ОЧКОВ" );
            } else {
                card.badge += tr( "AVAILABLE", "ДОСТУПНО" );
            }

            if( perk.currency == currency_id::major ) {
                card.flags |= NCMM_UI_CARD_MAJOR;
            }
            if( effective_kind( perk ) == perk_kind::effect ) {
                card.flags |= NCMM_UI_CARD_EFFECT;
            }
            card.icon_key = std::string( "survivor/perk/" ) + perk.id;
            texts.push_back( std::move( card ) );
        }

        const int owned_now = branch_owned_count( branch );
        const int total_now = branch_total_count( branch );
        const int unlocked_now = branch_unlocked_count( branch, level );

        std::string title = "Survivor Progression > " + branch_name( branch );
        std::string summary =
            tr( "Purchased ", "Куплено " ) + std::to_string( owned_now ) + "/" +
            std::to_string( total_now ) +
            tr( " | available ", " | доступно " ) + std::to_string( unlocked_now ) +
            " | P " + std::to_string( perk_points ) +
            " | M " + std::to_string( major_points );

        const int64_t blevel = branch_level( branch );
        const int64_t bxp = branch_xp( branch );
        const int64_t bnext = branch_xp_to_next( blevel );
        summary += tr( " | Lv ", " | ур. " ) + std::to_string( blevel ) +
                   " XP " + std::to_string( bxp ) + "/" + std::to_string( bnext ) +
                   " | " + branch_efficiency_text( branch );

        std::string progress_label =
            branch_name( branch ) + tr( " XP -> L", " XP -> ур." ) +
            std::to_string( blevel + 1 );
        ncmm_ui_progress_v1 progress{
            progress_label.c_str(),
            bxp,
            bnext
        };

        if( tree_mode && host->ui_tree_choose ) {
            std::vector<tree_node_text> tree_texts;
            tree_texts.reserve( branch_perks.size() );
            std::map<std::string, size_t> index_by_id;

            for( size_t i = 0; i < branch_perks.size(); ++i ) {
                const perk_def &perk = *branch_perks[i];
                tree_node_text node;
                node.card = texts[i];
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( perk );
                const std::pair<int, int> position = branch_tree_position( branch, i );
                node.row = position.first;
                node.column = position.second;
                index_by_id[perk.id] = i;
                tree_texts.push_back( std::move( node ) );
            }

            std::vector<ncmm_ui_tree_edge_v1> edges;
            auto add_edge = [&]( const char *prereq, size_t to ) {
                if( prereq == nullptr || prereq[0] == '\0' ) {
                    return;
                }
                const auto it = index_by_id.find( prereq );
                if( it != index_by_id.end() ) {
                    edges.push_back( { it->second, to } );
                }
            };
            for( size_t i = 0; i < branch_perks.size(); ++i ) {
                add_edge( branch_perks[i]->prereq1, i );
                add_edge( branch_perks[i]->prereq2, i );
            }

            std::vector<ncmm_ui_tree_node_v1> nodes = bind_tree_nodes( tree_texts );
            const std::string tree_summary =
                summary + tr( " | Tree view | Tab: cards",
                              " | Дерево | Tab: карточки" );
            const int choice = host->ui_tree_choose(
                                   title.c_str(), tree_summary.c_str(), &progress,
                                   nodes.data(), nodes.size(), edges.data(), edges.size() );
            if( choice == NCMM_UI_TREE_SHOW_CARDS ) {
                tree_mode = false;
                continue;
            }
            if( choice < 0 || static_cast<size_t>( choice ) >= branch_perks.size() ) {
                return;
            }
            show_perk_detail( *branch_perks[choice] );
            continue;
        }

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const std::string card_summary =
            summary + tr( " | Cards | Tab: tree",
                          " | Карточки | Tab: дерево" );
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), card_summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 2 ) :
                           -1;
        if( choice == NCMM_UI_CARD_SHOW_TREE ) {
            tree_mode = true;
            continue;
        }
        if( choice < 0 || static_cast<size_t>( choice ) >= branch_perks.size() ) {
            return;
        }
        show_perk_detail( *branch_perks[choice] );
    }
}
'@
$sp = Replace-CppRange $sp "void show_branch( branch_id branch )" "void show_overview()" $newShowBranch "0.9.10 branch UI"

$newRespec = @'
void respec()
{
    int64_t refund_perk = 0;
    int64_t refund_major = 0;
    for( const perk_def &perk : perks ) {
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }
        if( perk.currency == currency_id::perk ) {
            refund_perk += rank;
        } else {
            refund_major += rank;
        }
    }

    if( refund_perk == 0 && refund_major == 0 ) {
        message( tr( "No Survivor perks to reset.", "Нет перков Survivor для сброса." ) );
        return;
    }

    std::string title = tr(
        "Respec all Survivor perks?\nRefund: ",
        "Сбросить все перки Survivor?\nВозврат: " );
    title += std::to_string( refund_perk ) + "P / " + std::to_string( refund_major ) + "M";
    title += tr( "\nRanked perks refund every purchased rank.",
                 "\nМногоуровневые перки возвращают очко за каждый купленный ранг." );

    std::string yes = tr( "Respec", "Сбросить" );
    std::string no = tr( "Cancel", "Отмена" );
    const char *entries[] = { yes.c_str(), no.c_str() };
    const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
    if( choice != 0 ) {
        return;
    }

    for( const perk_def &perk : perks ) {
        if( perk_rank( perk ) > 0 ) {
            set_state( perk_key( perk ), 0 );
        }
    }

    set_state( "perk_points", get_state( "perk_points", 0 ) + refund_perk );
    set_state( "major_points", get_state( "major_points", 0 ) + refund_major );
    set_state( "fast_learner", 0 );
    effects_dirty = true;
    recalculate_effects();

    message( tr( "Survivor perks reset. Refunded: ", "Перки Survivor сброшены. Возвращено: " ) +
             std::to_string( refund_perk ) + "P / " + std::to_string( refund_major ) + "M" );
}
'@
$sp = Replace-CppRange $sp "void respec()" "void open_progression()" $newRespec "ranked respec"

# Keep the log/descriptor version synchronized even if snapshot had extra text.
$sp = $sp.Replace(
    "anti-farm branch XP / 120 perks / 6 integrated RPG trees.",
    "branch progression / perk ranks / anti-farm XP."
)

Write-Utf8NoBom $spPath $sp

$manifestPath = Join-Path $Source0910 "mod.json"
$manifest = [IO.File]::ReadAllText($manifestPath)
$manifest = $manifest.Replace('"version": "0.9.9"','"version": "0.9.10"')
Write-Utf8NoBom $manifestPath $manifest

# ===========================================================================
# Survivor Progression 0.9.11 -> 0.9.14 cumulative feature train
# ===========================================================================
function Save-SurvivorSourceSnapshot([string]$SnapshotRoot,[string]$VersionLabel) {
    $sdkSource = Join-Path $NcmmRoot "sdk\ncmm_api.h"
    $inputs = @($loaderPath,$sdkSource,$spPath,$manifestPath)
    foreach ($p in $inputs) {
        if (-not (Test-Path $p -PathType Leaf)) { throw "Snapshot source missing: $p" }
    }
    $fingerprint = Hash-Text ((@(
        "v8.2",
        $VersionLabel,
        (Hash-File $loaderPath),
        (Hash-File $sdkSource),
        (Hash-File $spPath),
        (Hash-File $manifestPath)
    )) -join "|")
    $stamp = Join-Path $SnapshotRoot ".snapshot.sha256"
    if ((Test-Path $stamp -PathType Leaf) -and
        (Test-Path (Join-Path $SnapshotRoot "host_patch\ncmm_loader.cpp") -PathType Leaf) -and
        (Test-Path (Join-Path $SnapshotRoot "mods\SurvivorProgression\src\survivor_progression.cpp") -PathType Leaf) -and
        ((Get-Content $stamp -Raw).Trim() -eq $fingerprint)) {
        Write-Host "${VersionLabel} source snapshot: unchanged (cache hit)" -ForegroundColor DarkGray
        return
    }

    Remove-Item $SnapshotRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force (Join-Path $SnapshotRoot "host_patch") | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $SnapshotRoot "sdk") | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $SnapshotRoot "mods\SurvivorProgression\src") | Out-Null
    Copy-Item $loaderPath (Join-Path $SnapshotRoot "host_patch\ncmm_loader.cpp") -Force
    Copy-Item $sdkSource (Join-Path $SnapshotRoot "sdk\ncmm_api.h") -Force
    Copy-Item $spPath (Join-Path $SnapshotRoot "mods\SurvivorProgression\src\survivor_progression.cpp") -Force
    Copy-Item $manifestPath (Join-Path $SnapshotRoot "mods\SurvivorProgression\mod.json") -Force
    Write-Utf8NoBom $stamp ($fingerprint + "`n")
    Write-Host "${VersionLabel} source snapshot: $SnapshotRoot" -ForegroundColor DarkGray
}

# ---------------------------------------------------------------------------
# 0.9.11 — Tree Clarity + real branch progress bar
# ---------------------------------------------------------------------------
Write-Host "Applying Survivor 0.9.11: Tree Clarity + Branch Bar..." -ForegroundColor Cyan
$loader = [IO.File]::ReadAllText($loaderPath)

$connector0911 = @'
        // 0.9.11: obstacle-safe dependency graph.
        // Connectors no longer encode perk type with milestone/effect colors.
        // Nodes carry semantic color; edges stay neutral, with only the edge
        // touching the current selection highlighted.  Every committed edge is
        // a complete BFS path, so a blocked horizontal leg can never appear as
        // a visually broken half-connector.
        constexpr int edge_n = 1;
        constexpr int edge_e = 2;
        constexpr int edge_s = 4;
        constexpr int edge_w = 8;
        const int grid_size = frame_width * frame_height;
        std::vector<int> edge_mask( static_cast<size_t>( grid_size ), 0 );
        std::vector<int> edge_style( static_cast<size_t>( grid_size ), 0 );
        std::vector<std::array<int, 4>> blocked_rects;
        blocked_rects.reserve( node_count );

        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) {
                continue;
            }
            const int bx = node_x( i );
            const int by = node_y( i );
            blocked_rects.push_back( {{ bx, by, bx + node_width - 1, by + node_height - 1 }} );
        }

        auto grid_index = [&]( int x, int y ) {
            return y * frame_width + x;
        };
        auto is_blocked = [&]( int x, int y ) {
            for( const std::array<int, 4> &r : blocked_rects ) {
                if( x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3] ) {
                    return true;
                }
            }
            return false;
        };
        auto mark_dir = [&]( int x, int y, int dir, int style ) {
            if( x <= 0 || x >= divider_x || y < header_height ||
                y >= frame_height - footer_height || is_blocked( x, y ) ) {
                return;
            }
            const int index = grid_index( x, y );
            edge_mask[index] |= dir;
            edge_style[index] = std::max( edge_style[index], style );
        };
        auto connect_cells = [&]( int ax, int ay, int bx, int by, int style ) {
            if( ax == bx && by == ay + 1 ) {
                mark_dir( ax, ay, edge_s, style );
                mark_dir( bx, by, edge_n, style );
            } else if( ax == bx && by == ay - 1 ) {
                mark_dir( ax, ay, edge_n, style );
                mark_dir( bx, by, edge_s, style );
            } else if( ay == by && bx == ax + 1 ) {
                mark_dir( ax, ay, edge_e, style );
                mark_dir( bx, by, edge_w, style );
            } else if( ay == by && bx == ax - 1 ) {
                mark_dir( ax, ay, edge_w, style );
                mark_dir( bx, by, edge_e, style );
            }
        };
        auto color_for_style = [&]( int style ) {
            if( style >= 2 ) return c_light_green;
            if( style == 1 ) return c_light_gray;
            return c_dark_gray;
        };
        auto glyph_for_mask = [&]( int mask ) -> int {
            const bool n = ( mask & edge_n ) != 0;
            const bool e = ( mask & edge_e ) != 0;
            const bool s = ( mask & edge_s ) != 0;
            const bool w = ( mask & edge_w ) != 0;
            if( n && e && s && w ) return LINE_XXXX;
            if( n && e && s ) return LINE_XXXO;
            if( n && e && w ) return LINE_XXOX;
            if( n && s && w ) return LINE_XOXX;
            if( e && s && w ) return LINE_OXXX;
            if( n && e ) return LINE_XXOO;
            if( e && s ) return LINE_OXXO;
            if( s && w ) return LINE_OOXX;
            if( n && w ) return LINE_XOOX;
            if( n || s ) return LINE_XOXO;
            return LINE_OXOX;
        };

        std::vector<bool> has_incoming( node_count, false );
        std::vector<bool> has_outgoing( node_count, false );

        for( size_t e = 0; e < edge_count; ++e ) {
            const size_t from = edges[e].from_index;
            const size_t to = edges[e].to_index;
            if( !visible( from ) || !visible( to ) ) {
                continue;
            }

            const int x1 = node_x( from ) + node_width / 2;
            const int y1 = node_y( from ) + node_height;
            const int x2 = node_x( to ) + node_width / 2;
            const int y2 = node_y( to ) - 1;
            if( y2 < y1 || x1 <= 0 || x1 >= divider_x ||
                x2 <= 0 || x2 >= divider_x ) {
                continue;
            }

            const bool related_to_selection =
                from == static_cast<size_t>( selected ) || to == static_cast<size_t>( selected );
            const bool locked_path = ( nodes[to].flags & NCMM_UI_CARD_LOCKED ) != 0;
            const int style = related_to_selection ? 2 : ( locked_path ? 0 : 1 );

            const int start_index = grid_index( x1, y1 );
            const int goal_index = grid_index( x2, y2 );
            std::vector<int> previous( static_cast<size_t>( grid_size ), -1 );
            std::vector<int> frontier;
            frontier.reserve( static_cast<size_t>( grid_size ) );
            previous[start_index] = start_index;
            frontier.push_back( start_index );

            size_t head = 0;
            while( head < frontier.size() && previous[goal_index] < 0 ) {
                const int current = frontier[head++];
                const int cx = current % frame_width;
                const int cy = current / frame_width;

                // Prefer downward movement first: dependency trees remain easy to read,
                // while BFS still guarantees a complete route around every tile.
                const std::array<std::pair<int, int>, 4> directions = {{
                    { 0, 1 }, { x2 >= cx ? 1 : -1, 0 },
                    { x2 >= cx ? -1 : 1, 0 }, { 0, -1 }
                }};
                for( const auto &dir : directions ) {
                    const int nx = cx + dir.first;
                    const int ny = cy + dir.second;
                    if( nx <= 0 || nx >= divider_x || ny < header_height ||
                        ny >= frame_height - footer_height ) {
                        continue;
                    }
                    if( is_blocked( nx, ny ) && !( nx == x2 && ny == y2 ) ) {
                        continue;
                    }
                    const int next = grid_index( nx, ny );
                    if( previous[next] >= 0 ) {
                        continue;
                    }
                    previous[next] = current;
                    frontier.push_back( next );
                }
            }

            // Never draw a partial connector.  If no complete path exists in the
            // visible canvas, omit this edge for the current scroll window.
            if( previous[goal_index] < 0 ) {
                continue;
            }

            // Only expose the border tees after a COMPLETE connector exists.
            // This prevents the apparent one-cell "broken line" stubs that
            // v1 could leave when routing failed in the current scroll window.
            has_outgoing[from] = true;
            has_incoming[to] = true;

            std::vector<int> path;
            for( int at = goal_index; ; at = previous[at] ) {
                path.push_back( at );
                if( at == start_index ) {
                    break;
                }
            }
            std::reverse( path.begin(), path.end() );
            for( size_t p = 1; p < path.size(); ++p ) {
                const int a = path[p - 1];
                const int b = path[p];
                connect_cells( a % frame_width, a / frame_width,
                               b % frame_width, b / frame_width, style );
            }
        }

        for( int y = header_height; y < frame_height - footer_height; ++y ) {
            for( int x = 1; x < divider_x; ++x ) {
                const int index = grid_index( x, y );
                if( edge_mask[index] == 0 ) {
                    continue;
                }
                const nc_color color = color_for_style( edge_style[index] );
                wattron( frame, color );
                mvwaddch( frame, point( x, y ), glyph_for_mask( edge_mask[index] ) );
                wattroff( frame, color );
            }
        }
'@
$connectorEnd0911 = @'
        for( size_t i = 0; i < node_count; ++i ) {
            if( !visible( i ) ) continue;
            const int x = node_x( i );
'@
$loader = Replace-CppRange $loader `
    "        // 0.9.10: route dependency lines" `
    $connectorEnd0911.TrimEnd() `
    $connector0911 `
    "0.9.11 complete obstacle-safe connector routing"

$oldProgressLine = 'line += "[" + std::to_string( percent ) + "%]";'
$newProgressLine = @'
const int bar_width = 18;
            const int filled = std::max( 0, std::min( bar_width,
                               static_cast<int>( std::llround( percent * bar_width / 100.0 ) ) ) );
            line += "[" + std::string( static_cast<size_t>( filled ), '#' ) +
                    std::string( static_cast<size_t>( bar_width - filled ), '-' ) + "] " +
                    std::to_string( percent ) + "%";
'@
$progressCount = ([regex]::Matches($loader,[regex]::Escape($oldProgressLine))).Count
if ($progressCount -ne 2) {
    throw "0.9.11 progress-bar patch expected 2 renderer sites, found $progressCount"
}
$loader = $loader.Replace($oldProgressLine,$newProgressLine.TrimEnd())

# Structural guard: none of the 0.9.10 semantic routing tail may survive.
foreach ($obsolete in @(
    "style_for_node",
    "const int style = style_for_node( to );",
    "auto vertical_clear =",
    "std::vector<int> candidates;",
    "int route_x = -1;"
)) {
    if ($loader.Contains($obsolete)) {
        throw "0.9.11 connector replacement incomplete; obsolete fragment survived: $obsolete"
    }
}

Write-Utf8NoBom $loaderPath $loader

$sp = [IO.File]::ReadAllText($spPath)
$sp = $sp.Replace('"0.9.10"','"0.9.11"')
$sp = $sp.Replace('Survivor Progression v0.9.10','Survivor Progression v0.9.11')
$sp = $sp.Replace('Survivor Progression 0.9.10 initialized:','Survivor Progression 0.9.11 initialized:')
Write-Utf8NoBom $spPath $sp
$manifest = [IO.File]::ReadAllText($manifestPath).Replace('"version": "0.9.10"','"version": "0.9.11"')
Write-Utf8NoBom $manifestPath $manifest
Save-SurvivorSourceSnapshot $Snap0911 "0.9.11"

# ---------------------------------------------------------------------------
# 0.9.12 — Branch progression expansion: diversity + branch level notices
# ---------------------------------------------------------------------------
Write-Host "Applying Survivor 0.9.12: Branch Progression Expansion..." -ForegroundColor Cyan
$sp = [IO.File]::ReadAllText($spPath)

$awardBranch0912 = @'
int64_t award_branch_xp( branch_id branch, int64_t raw_gained )
{
    const int64_t gained = anti_farm_adjust( branch, raw_gained );
    if( gained <= 0 ) {
        return 0;
    }

    const int64_t old_level = branch_level( branch );
    int64_t level = old_level;
    int64_t xp = branch_xp( branch ) + gained;
    while( xp >= branch_xp_to_next( level ) &&
           level < std::numeric_limits<int64_t>::max() ) {
        xp -= branch_xp_to_next( level );
        ++level;
    }
    set_state( branch_state_key( branch, "level" ), level );
    set_state( branch_state_key( branch, "xp" ), xp );

    // Only post-anti-farm branch XP reaches the global Survivor level.
    award_global_xp( gained );

    if( level > old_level ) {
        message( branch_name( branch ) + tr( " branch level up: ", " — новый уровень ветки: " ) +
                 std::to_string( level ) );
    }
    return gained;
}
'@
$sp = Replace-CppRange $sp "int64_t award_branch_xp( branch_id branch, int64_t raw_gained )" "int64_t metric_now( const char *metric )" $awardBranch0912 "0.9.12 branch level notification"

$poll0912 = @'
int activity_diversity_bonus_pct( int active_branches )
{
    if( active_branches >= 4 ) return 15;
    if( active_branches == 3 ) return 10;
    if( active_branches == 2 ) return 5;
    return 0;
}

int64_t apply_activity_diversity_bonus( branch_id branch, int64_t raw, int bonus_pct )
{
    if( raw <= 0 || bonus_pct <= 0 ) {
        return raw;
    }
    const std::string key = branch_state_key( branch, "diversity_fraction" );
    int64_t scaled = raw * ( 100 + bonus_pct ) +
                     std::max<int64_t>( 0, get_state( key, 0 ) );
    const int64_t result = scaled / 100;
    set_state( key, scaled % 100 );
    return result;
}

void poll_branch_xp()
{
    decay_branch_fatigue();

    const int64_t kills = metric_delta( "combat.kills", "metric_combat_kills" );
    const int64_t kill_xp = metric_delta( "combat.kill_xp", "metric_combat_kill_xp" );
    int64_t combat_gain = std::min<int64_t>( std::max<int64_t>( kills, ( kill_xp + 49 ) / 50 ), 25 );

    int64_t healing = metric_delta( "survival.healing", "metric_survival_healing" );
    healing += get_state( "survival_heal_remainder", 0 );
    int64_t survival_gain = std::min<int64_t>( healing / 10, 8 );
    set_state( "survival_heal_remainder", healing % 10 );

    int64_t steps = metric_delta( "mobility.steps", "metric_mobility_steps" );
    steps += get_state( "mobility_step_remainder", 0 );
    int64_t mobility_gain = std::min<int64_t>( steps / 150, 3 );
    set_state( "mobility_step_remainder", steps % 150 );

    const int64_t crafts = metric_delta( "crafting.completed", "metric_crafting_completed" );
    int64_t crafting_gain = std::min<int64_t>( crafts * 3, 12 );

    const int64_t omts = metric_delta( "scavenging.omt", "metric_scavenging_omt" );
    int64_t scavenging_gain = std::min<int64_t>( omts * 4, 8 );

    const int64_t skill_levels = metric_delta( "mastery.skill_levels", "metric_mastery_skill_levels" );
    int64_t mastery_gain = std::min<int64_t>( skill_levels * 6, 18 );

    int active = 0;
    active += combat_gain > 0 ? 1 : 0;
    active += survival_gain > 0 ? 1 : 0;
    active += mobility_gain > 0 ? 1 : 0;
    active += crafting_gain > 0 ? 1 : 0;
    active += scavenging_gain > 0 ? 1 : 0;
    const int diversity_bonus = activity_diversity_bonus_pct( active );

    combat_gain = apply_activity_diversity_bonus( branch_id::combat, combat_gain, diversity_bonus );
    survival_gain = apply_activity_diversity_bonus( branch_id::survival, survival_gain, diversity_bonus );
    mobility_gain = apply_activity_diversity_bonus( branch_id::mobility, mobility_gain, diversity_bonus );
    crafting_gain = apply_activity_diversity_bonus( branch_id::crafting, crafting_gain, diversity_bonus );
    scavenging_gain = apply_activity_diversity_bonus( branch_id::scavenging, scavenging_gain, diversity_bonus );

    int64_t activity_total = 0;
    activity_total += award_branch_xp( branch_id::combat, combat_gain );
    activity_total += award_branch_xp( branch_id::survival, survival_gain );
    activity_total += award_branch_xp( branch_id::mobility, mobility_gain );
    activity_total += award_branch_xp( branch_id::crafting, crafting_gain );
    activity_total += award_branch_xp( branch_id::scavenging, scavenging_gain );

    int64_t mastery_fraction = get_state( "mastery_share_fraction", 0 );
    mastery_fraction += activity_total * 10;
    mastery_gain += mastery_fraction / 100;
    mastery_fraction %= 100;
    set_state( "mastery_share_fraction", mastery_fraction );
    award_branch_xp( branch_id::mastery, mastery_gain );
}
'@
$sp = Replace-CppRange $sp "void poll_branch_xp()" "void tick()" $poll0912 "0.9.12 activity diversity"
$sp = $sp.Replace('"0.9.11"','"0.9.12"')
$sp = $sp.Replace('Survivor Progression v0.9.11','Survivor Progression v0.9.12')
$sp = $sp.Replace('Survivor Progression 0.9.11 initialized:','Survivor Progression 0.9.12 initialized:')
Write-Utf8NoBom $spPath $sp
$manifest = [IO.File]::ReadAllText($manifestPath).Replace('"version": "0.9.11"','"version": "0.9.12"')
Write-Utf8NoBom $manifestPath $manifest
Save-SurvivorSourceSnapshot $Snap0912 "0.9.12"

# ---------------------------------------------------------------------------
# 0.9.13 — Active-mod integration API v1
# ---------------------------------------------------------------------------
Write-Host "Applying Survivor 0.9.13: Active-Mod Integration Framework..." -ForegroundColor Cyan
$sdkPath = Join-Path $NcmmRoot "sdk\ncmm_api.h"
$sdk = [IO.File]::ReadAllText($sdkPath)
if (-not $sdk.Contains('#define NCMM_API_VERSION_MINOR 4u')) {
    throw "0.9.13 expected NCMM API minor 4 before active-mod extension."
}
$sdk = $sdk.Replace('#define NCMM_API_VERSION_MINOR 4u','#define NCMM_API_VERSION_MINOR 5u')
$metricPointer = '    int64_t ( *gameplay_metric_get_i64 )( const char *metric_id );'
if (-not $sdk.Contains($metricPointer)) {
    throw "0.9.13 SDK gameplay metric tail not found."
}
$sdk = $sdk.Replace($metricPointer, $metricPointer + "`n" + '    int ( *world_mod_active )( const char *mod_id );')
Write-Utf8NoBom $sdkPath $sdk

$loader = [IO.File]::ReadAllText($loaderPath)
$loader = $loader.Replace('    "gameplay.metrics.v1",', '    "gameplay.metrics.v1",' + "`n" + '    "active_mods.v1",')
$activeModHost = @'
int world_mod_active( const char *requested_mod_id )
{
    if( requested_mod_id == nullptr || requested_mod_id[0] == '\0' ||
        world_generator == nullptr || world_generator->active_world == nullptr ) {
        return 0;
    }

    // Compare mod_id values directly. type_id.h only forward-declares
    // MOD_INFORMATION; dereferencing mod_id here is unnecessary.
    const mod_id wanted( requested_mod_id );
    for( const mod_id &mod : world_generator->active_world->active_mod_order ) {
        if( mod == wanted ) {
            return 1;
        }
    }
    return 0;
}

'@
$loader = $loader.Replace('int can_expose_worldgen_option( const char *option_id )', $activeModHost + 'int can_expose_worldgen_option( const char *option_id )')
$loader = $loader.Replace('    &ui_tree_choose,' + "`n" + '    &gameplay_metric_get_i64' + "`n" + '};',
                          '    &ui_tree_choose,' + "`n" + '    &gameplay_metric_get_i64,' + "`n" + '    &world_mod_active' + "`n" + '};')
Write-Utf8NoBom $loaderPath $loader

# Keep the smoke host source-compatible with the additive API tail.
$smokePath = Join-Path $NcmmRoot "tests\smoke_host.cpp"
if (Test-Path $smokePath -PathType Leaf) {
    $smoke = [IO.File]::ReadAllText($smokePath)
    if (-not $smoke.Contains('"gameplay.metrics.v1"')) {
        $smoke = $smoke.Replace('           std::strcmp( cap, "ui.tree.v1" ) == 0 ||',
            '           std::strcmp( cap, "ui.tree.v1" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "gameplay.metrics.v1" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "active_mods.v1" ) == 0 ||')
        $smoke = $smoke.Replace('    "ui.cards.v1", "ui.tree.v1", "module_hotkeys.context.v1",',
            '    "ui.cards.v1", "ui.tree.v1", "gameplay.metrics.v1", "active_mods.v1", "module_hotkeys.context.v1",')
    } elseif (-not $smoke.Contains('"active_mods.v1"')) {
        $smoke = $smoke.Replace('           std::strcmp( cap, "gameplay.metrics.v1" ) == 0 ||',
            '           std::strcmp( cap, "gameplay.metrics.v1" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "active_mods.v1" ) == 0 ||')
        $smoke = $smoke.Replace('"gameplay.metrics.v1",', '"gameplay.metrics.v1", "active_mods.v1",')
    }
    if (-not $smoke.Contains('world_mod_active_fn')) {
        $stubs = @'
int64_t gameplay_metric_get_i64_fn( const char * )
{
    return 0;
}

int world_mod_active_fn( const char * )
{
    return 0;
}

'@
        $smoke = $smoke.Replace('template<typename T>', $stubs + 'template<typename T>')
        $smoke = $smoke.Replace('        &ui_tree_choose_fn' + "`n" + '    };',
            '        &ui_tree_choose_fn,' + "`n" + '        &gameplay_metric_get_i64_fn,' + "`n" +
            '        &world_mod_active_fn' + "`n" + '    };')
    }
    $smoke = $smoke.Replace('t.find( "Survivor Progression v0.9.4" )', 't.find( "Survivor Progression" )')
    $smoke = $smoke.Replace('migrate( &api, 2, 4 )', 'migrate( &api, 2, 7 )')
    $smoke = $smoke.Replace('character_state[prefix + "schema"] != 4', 'character_state[prefix + "schema"] != 7')
    $smoke = $smoke.Replace('Survivor Progression 0.9.4 tree/migration/purchase/effects/respec slice',
                            'Survivor Progression 0.9.14 progression/integration slice')
    Write-Utf8NoBom $smokePath $smoke
}

$sp = [IO.File]::ReadAllText($spPath)
$sp = $sp.Replace('    "gameplay.metrics.v1",', '    "gameplay.metrics.v1",' + "`n" + '    "active_mods.v1",')
$activeModModule = @'
bool active_world_mod( const char *mod_id )
{
    return host != nullptr && host->world_mod_active != nullptr &&
           host->world_mod_active( mod_id ) != 0;
}

'@
$sp = $sp.Replace('int64_t get_state( const std::string &key, int64_t fallback )', $activeModModule + 'int64_t get_state( const std::string &key, int64_t fallback )')
$sp = $sp.Replace('"0.9.12"','"0.9.13"')
$sp = $sp.Replace('!api->gameplay_metric_get_i64 || !api->ui_message',
                  '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->ui_message')
$sp = $sp.Replace('Survivor Progression v0.9.12','Survivor Progression v0.9.13')
$sp = $sp.Replace('Survivor Progression 0.9.12 initialized:','Survivor Progression 0.9.13 initialized:')
Write-Utf8NoBom $spPath $sp

$manifest = [IO.File]::ReadAllText($manifestPath)
$manifest = $manifest.Replace('"version": "0.9.12"','"version": "0.9.13"')
$manifest = $manifest.Replace('"api_min_minor": 4','"api_min_minor": 5')
$manifest = $manifest.Replace('    "gameplay.metrics.v1",', '    "gameplay.metrics.v1",' + "`n" + '    "active_mods.v1",')
Write-Utf8NoBom $manifestPath $manifest
Save-SurvivorSourceSnapshot $Snap0913 "0.9.13"

# ---------------------------------------------------------------------------
# 0.9.14 — Exclusive specializations + deep conditional mod integrations
# ---------------------------------------------------------------------------
Write-Host "Applying Survivor 0.9.14: Specializations + Deep Mod Integrations..." -ForegroundColor Cyan
$sp = [IO.File]::ReadAllText($spPath)
if (-not $sp.Contains('#include <string_view>')) {
    $sp = Replace-Once $sp '#include <string>' ('#include <string>' + "`n" + '#include <string_view>') '0.9.14 string_view include'
}

# Schema 7 stores specialization choice per branch. Existing rank state is unchanged.
$sp = $sp.Replace('constexpr int state_schema = 6;','constexpr int state_schema = 7;')

$extraPerks0914 = @'
    { "spc_c_juggernaut", branch_id::combat, 4, 15, currency_id::perk, "c_conditioning", "", "Juggernaut", "Штурмовик", "Commit to armored endurance: +10% stamina and +0.5 STR.", "Ставка на силовую выносливость: +10% выносливости и +0,5 СИЛ.", {{ { "stamina_max_pct", 10 }, { "str_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_c_duelist", branch_id::combat, 4, 15, currency_id::perk, "c_tempo", "", "Duelist", "Дуэлянт", "Commit to mobility and timing: +3% speed and +0.5 dodge.", "Ставка на мобильность и темп: +3% скорости и +0,5 к уклонению.", {{ { "speed_pct", 3 }, { "dodge_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_c_tactician", branch_id::combat, 4, 15, currency_id::perk, "c_precision", "c_reflexes", "Tactician", "Тактик", "Commit to control: +0.5 PER and +0.25 melee hit.", "Ставка на контроль: +0,5 ВОС и +0,25 точности ближнего боя.", {{ { "per_flat", 0.5 }, { "melee_hit_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_c_juggernaut_cap", branch_id::combat, 5, 25, currency_id::major, "spc_c_juggernaut", "", "Iron Advance", "Железный натиск", "+1 STR, +12% stamina, +5% carry.", "+1 СИЛ, +12% выносливости, +5% грузоподъёмности.", {{ { "str_flat", 1 }, { "stamina_max_pct", 12 }, { "carry_weight_pct", 5 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_c_duelist_cap", branch_id::combat, 5, 25, currency_id::major, "spc_c_duelist", "", "Perfect Tempo", "Идеальный темп", "+5% speed, +1 dodge, -3% move cost.", "+5% скорости, +1 к уклонению, -3% стоимости движения.", {{ { "speed_pct", 5 }, { "dodge_flat", 1 }, { "move_cost_pct", -3 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_c_tactician_cap", branch_id::combat, 5, 25, currency_id::major, "spc_c_tactician", "", "Battlefield Control", "Контроль поля боя", "+1 PER, +0.75 melee hit, +3% speed.", "+1 ВОС, +0,75 точности, +3% скорости.", {{ { "per_flat", 1 }, { "melee_hit_flat", 0.75 }, { "speed_pct", 3 }, { nullptr, 0 } }}, 3, 0 },
    { "mg_battlemage", branch_id::combat, 4, 18, currency_id::perk, "c_tempo", "mg_arcane_focus", "Battlemage Practice", "Практика боевого мага", "Magiclysm: weave movement into casting; +3% speed and +0.25 melee hit.", "Magiclysm: движение вплетено в колдовство; +3% скорости и +0,25 точности.", {{ { "speed_pct", 3 }, { "melee_hit_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "mom_combat_focus", branch_id::combat, 4, 18, currency_id::perk, "c_reflexes", "mom_mental_focus", "Psionic Combat Focus", "Псионический боевой фокус", "Mind Over Matter: +0.5 melee hit and +2% speed.", "Mind Over Matter: +0,5 точности и +2% скорости.", {{ { "melee_hit_flat", 0.5 }, { "speed_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "xe_dimensional_hunter", branch_id::combat, 4, 18, currency_id::perk, "c_precision", "xe_anomaly_method", "Dimensional Hunter", "Охотник на аномалии", "Xedra Evolved: +0.5 melee hit and +0.5 dodge.", "Xedra Evolved: +0,5 точности и +0,5 к уклонению.", {{ { "melee_hit_flat", 0.5 }, { "dodge_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "af_combat_technician", branch_id::combat, 4, 18, currency_id::perk, "c_precision", "af_systems_operator", "Combat Technician", "Боевой техник", "Aftershock: +0.5 melee hit and +1 PER.", "Aftershock: +0,5 точности и +1 ВОС.", {{ { "melee_hit_flat", 0.5 }, { "per_flat", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_s_nomad", branch_id::survival, 4, 15, currency_id::perk, "s_endurance", "", "Nomad", "Кочевник", "Commit to long expeditions: +8% stamina and +8% carry.", "Ставка на дальние походы: +8% выносливости и +8% грузоподъёмности.", {{ { "stamina_max_pct", 8 }, { "carry_weight_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_s_medic", branch_id::survival, 4, 15, currency_id::perk, "s_field", "s_resilient", "Field Medic", "Полевой медик", "Commit to recovery: +15% healing.", "Ставка на восстановление: +15% лечения.", {{ { "healing_pct", 15 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0 },
    { "spc_s_quartermaster", branch_id::survival, 4, 15, currency_id::perk, "s_pack", "", "Quartermaster", "Интендант", "Commit to preparation: +15% carry and +5% crafting speed.", "Ставка на подготовку: +15% грузоподъёмности и +5% скорости крафта.", {{ { "carry_weight_pct", 15 }, { "craft_speed_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_s_nomad_cap", branch_id::survival, 5, 25, currency_id::major, "spc_s_nomad", "", "Long Road", "Долгая дорога", "+15% stamina, +15% carry, -3% move cost.", "+15% выносливости, +15% грузоподъёмности, -3% стоимости движения.", {{ { "stamina_max_pct", 15 }, { "carry_weight_pct", 15 }, { "move_cost_pct", -3 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_s_medic_cap", branch_id::survival, 5, 25, currency_id::major, "spc_s_medic", "", "Trauma Veteran", "Ветеран травм", "+30% healing and +8% stamina.", "+30% лечения и +8% выносливости.", {{ { "healing_pct", 30 }, { "stamina_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_s_quartermaster_cap", branch_id::survival, 5, 25, currency_id::major, "spc_s_quartermaster", "", "Prepared for Anything", "Готов ко всему", "+25% carry and +8% crafting speed.", "+25% грузоподъёмности и +8% скорости крафта.", {{ { "carry_weight_pct", 25 }, { "craft_speed_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "mg_wayfarer", branch_id::survival, 4, 18, currency_id::perk, "s_instinct", "mg_arcane_focus", "Arcane Wayfarer", "Магический странник", "Magiclysm: +6% stamina and -2% move cost.", "Magiclysm: +6% выносливости и -2% стоимости движения.", {{ { "stamina_max_pct", 6 }, { "move_cost_pct", -2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "mom_neural_reserve", branch_id::survival, 4, 18, currency_id::perk, "s_resilient", "mom_mental_focus", "Neural Reserve", "Нейронный резерв", "Mind Over Matter: +8% stamina and +5% healing.", "Mind Over Matter: +8% выносливости и +5% лечения.", {{ { "stamina_max_pct", 8 }, { "healing_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "af_conditioning", branch_id::survival, 4, 18, currency_id::perk, "s_hardy", "af_systems_operator", "Exoplanet Conditioning", "Экзопланетная закалка", "Aftershock: +8% stamina and +6% carry.", "Aftershock: +8% выносливости и +6% грузоподъёмности.", {{ { "stamina_max_pct", 8 }, { "carry_weight_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_sprinter", branch_id::mobility, 4, 15, currency_id::perk, "m_cardio", "", "Sprinter", "Спринтер", "Commit to burst mobility: +3% speed and +5% stamina.", "Ставка на рывок: +3% скорости и +5% выносливости.", {{ { "speed_pct", 3 }, { "stamina_max_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_ghost", branch_id::mobility, 4, 15, currency_id::perk, "m_light", "m_parkour", "Ghost", "Призрак", "Commit to evasive movement: -4% move cost and +0.5 dodge.", "Ставка на уклончивость: -4% стоимости движения и +0,5 к уклонению.", {{ { "move_cost_pct", -4 }, { "dodge_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_pathfinder", branch_id::mobility, 4, 15, currency_id::perk, "m_stride", "", "Pathfinder", "Путепроходец", "Commit to efficient travel: -3% move cost and +8% stamina.", "Ставка на эффективный путь: -3% стоимости движения и +8% выносливости.", {{ { "move_cost_pct", -3 }, { "stamina_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_sprinter_cap", branch_id::mobility, 5, 25, currency_id::major, "spc_m_sprinter", "", "Burst Engine", "Двигатель рывка", "+6% speed and +10% stamina.", "+6% скорости и +10% выносливости.", {{ { "speed_pct", 6 }, { "stamina_max_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_ghost_cap", branch_id::mobility, 5, 25, currency_id::major, "spc_m_ghost", "", "Vanishing Step", "Исчезающий шаг", "-7% move cost and +1 dodge.", "-7% стоимости движения и +1 к уклонению.", {{ { "move_cost_pct", -7 }, { "dodge_flat", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_m_pathfinder_cap", branch_id::mobility, 5, 25, currency_id::major, "spc_m_pathfinder", "", "Always a Route", "Путь всегда есть", "-5% move cost, +10% carry, +10% stamina.", "-5% стоимости движения, +10% грузоподъёмности, +10% выносливости.", {{ { "move_cost_pct", -5 }, { "carry_weight_pct", 10 }, { "stamina_max_pct", 10 }, { nullptr, 0 } }}, 3, 0 },
    { "mom_kinetic_control", branch_id::mobility, 4, 18, currency_id::perk, "m_quick", "mom_mental_focus", "Kinetic Control", "Кинетический контроль", "Mind Over Matter: -3% move cost and +0.5 dodge.", "Mind Over Matter: -3% стоимости движения и +0,5 к уклонению.", {{ { "move_cost_pct", -3 }, { "dodge_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_systems", branch_id::crafting, 4, 15, currency_id::perk, "f_engineer", "", "Systems Engineer", "Системный инженер", "Commit to engineering: +10% crafting speed and +0.5 INT.", "Ставка на инженерию: +10% скорости крафта и +0,5 ИНТ.", {{ { "craft_speed_pct", 10 }, { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_improviser", branch_id::crafting, 4, 15, currency_id::perk, "f_hands", "f_workflow", "Improviser", "Импровизатор", "Commit to practical work: +8% crafting speed and +5% carry.", "Ставка на практику: +8% скорости крафта и +5% грузоподъёмности.", {{ { "craft_speed_pct", 8 }, { "carry_weight_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_researcher", branch_id::crafting, 4, 15, currency_id::perk, "f_reader", "f_scholar", "Researcher", "Исследователь", "Commit to theory: +10% reading speed and +0.5 INT.", "Ставка на теорию: +10% скорости чтения и +0,5 ИНТ.", {{ { "read_speed_pct", 10 }, { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_systems_cap", branch_id::crafting, 5, 25, currency_id::major, "spc_f_systems", "", "Chief Engineer", "Главный инженер", "+18% crafting speed and +1 INT.", "+18% скорости крафта и +1 ИНТ.", {{ { "craft_speed_pct", 18 }, { "int_flat", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_improviser_cap", branch_id::crafting, 5, 25, currency_id::major, "spc_f_improviser", "", "Make It Work", "Заставить работать", "+15% crafting speed and +10% carry.", "+15% скорости крафта и +10% грузоподъёмности.", {{ { "craft_speed_pct", 15 }, { "carry_weight_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_f_researcher_cap", branch_id::crafting, 5, 25, currency_id::major, "spc_f_researcher", "", "Applied Theory", "Прикладная теория", "+20% reading, +1 INT, +5% crafting speed.", "+20% чтения, +1 ИНТ, +5% скорости крафта.", {{ { "read_speed_pct", 20 }, { "int_flat", 1 }, { "craft_speed_pct", 5 }, { nullptr, 0 } }}, 3, 0 },
    { "mg_ritual_craft", branch_id::crafting, 4, 18, currency_id::perk, "f_study", "mg_arcane_focus", "Ritual Craft", "Ритуальное ремесло", "Magiclysm: +8% crafting and +5% reading speed.", "Magiclysm: +8% крафта и +5% скорости чтения.", {{ { "craft_speed_pct", 8 }, { "read_speed_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "xe_occult_engineer", branch_id::crafting, 4, 18, currency_id::perk, "f_engineer", "xe_anomaly_method", "Occult Engineer", "Оккультный инженер", "Xedra Evolved: +8% crafting speed and +0.5 INT.", "Xedra Evolved: +8% крафта и +0,5 ИНТ.", {{ { "craft_speed_pct", 8 }, { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "af_systems_operator", branch_id::crafting, 4, 18, currency_id::perk, "f_engineer", "", "Systems Operator", "Оператор систем", "Aftershock: +10% crafting speed and +0.5 INT.", "Aftershock: +10% крафта и +0,5 ИНТ.", {{ { "craft_speed_pct", 10 }, { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_g_prospector", branch_id::scavenging, 4, 15, currency_id::perk, "g_observer", "", "Prospector", "Искатель", "Commit to finding value: +0.5 PER and +5% carry.", "Ставка на поиск ценного: +0,5 ВОС и +5% грузоподъёмности.", {{ { "per_flat", 0.5 }, { "carry_weight_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_g_courier", branch_id::scavenging, 4, 15, currency_id::perk, "g_pack", "g_endurance", "Courier", "Курьер", "Commit to loaded travel: +15% carry and -2% move cost.", "Ставка на движение с грузом: +15% грузоподъёмности и -2% стоимости движения.", {{ { "carry_weight_pct", 15 }, { "move_cost_pct", -2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_g_investigator", branch_id::scavenging, 4, 15, currency_id::perk, "g_awareness", "", "Investigator", "Исследователь руин", "Commit to reading the environment: +1 PER and +5% reading speed.", "Ставка на анализ окружения: +1 ВОС и +5% скорости чтения.", {{ { "per_flat", 1 }, { "read_speed_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_g_prospector_cap", branch_id::scavenging, 5, 25, currency_id::major, "spc_g_prospector", "", "Nothing Wasted", "Ничего не пропадает", "+1 PER and +10% carry.", "+1 ВОС и +10% грузоподъёмности.", {{ { "per_flat", 1 }, { "carry_weight_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_g_courier_cap", branch_id::scavenging, 5, 25, currency_id::major, "spc_g_courier", "", "Heavy Route", "Тяжёлый маршрут", "+25% carry, -4% move cost, +8% stamina.", "+25% грузоподъёмности, -4% стоимости движения, +8% выносливости.", {{ { "carry_weight_pct", 25 }, { "move_cost_pct", -4 }, { "stamina_max_pct", 8 }, { nullptr, 0 } }}, 3, 0 },
    { "spc_g_investigator_cap", branch_id::scavenging, 5, 25, currency_id::major, "spc_g_investigator", "", "Read the Ruins", "Читать руины", "+1.5 PER, +10% reading, -2% move cost.", "+1,5 ВОС, +10% чтения, -2% стоимости движения.", {{ { "per_flat", 1.5 }, { "read_speed_pct", 10 }, { "move_cost_pct", -2 }, { nullptr, 0 } }}, 3, 0 },
    { "xe_field_agent", branch_id::scavenging, 4, 18, currency_id::perk, "g_awareness", "xe_anomaly_method", "XEDRA Field Agent", "Полевой агент XEDRA", "Xedra Evolved: +1 PER and -2% move cost.", "Xedra Evolved: +1 ВОС и -2% стоимости движения.", {{ { "per_flat", 1 }, { "move_cost_pct", -2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "af_expedition_logistics", branch_id::scavenging, 4, 18, currency_id::perk, "g_pack", "af_systems_operator", "Expedition Logistics", "Экспедиционная логистика", "Aftershock: +10% carry and -2% move cost.", "Aftershock: +10% грузоподъёмности и -2% стоимости движения.", {{ { "carry_weight_pct", 10 }, { "move_cost_pct", -2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
    { "spc_a_specialist", branch_id::mastery, 4, 15, currency_id::perk, "a_focus", "a_growth", "Specialist", "Специалист", "Commit to depth: +10% Survivor XP.", "Ставка на глубину: +10% опыта Survivor.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 10 },
    { "spc_a_polymath", branch_id::mastery, 4, 15, currency_id::perk, "a_balance", "a_polymath", "Polymath Path", "Путь универсала", "Commit to breadth: +0.25 to all primary stats.", "Ставка на широту: +0,25 ко всем основным характеристикам.", {{ { "str_flat", 0.25 }, { "dex_flat", 0.25 }, { "per_flat", 0.25 }, { "int_flat", 0.25 } }}, 4, 0 },
    { "spc_a_selfteacher", branch_id::mastery, 4, 15, currency_id::perk, "a_adapt", "", "Self-Teacher", "Самоучка", "Commit to self-directed growth: +5% reading, +5% crafting, +5% Survivor XP.", "Ставка на самостоятельный рост: +5% чтения, +5% крафта, +5% опыта Survivor.", {{ { "read_speed_pct", 5 }, { "craft_speed_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 5 },
    { "spc_a_specialist_cap", branch_id::mastery, 5, 25, currency_id::major, "spc_a_specialist", "", "Dedicated Study", "Углублённое обучение", "+20% Survivor XP.", "+20% опыта Survivor.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 20 },
    { "spc_a_polymath_cap", branch_id::mastery, 5, 25, currency_id::major, "spc_a_polymath", "", "Cross Discipline", "Перекрёстная дисциплина", "+0.5 to all primary stats.", "+0,5 ко всем основным характеристикам.", {{ { "str_flat", 0.5 }, { "dex_flat", 0.5 }, { "per_flat", 0.5 }, { "int_flat", 0.5 } }}, 4, 0 },
    { "spc_a_selfteacher_cap", branch_id::mastery, 5, 25, currency_id::major, "spc_a_selfteacher", "", "Self-Taught Expertise", "Опыт самоучки", "+10% reading, +10% crafting, +10% Survivor XP.", "+10% чтения, +10% крафта, +10% опыта Survivor.", {{ { "read_speed_pct", 10 }, { "craft_speed_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 10 },
    { "mg_arcane_focus", branch_id::mastery, 3, 12, currency_id::perk, "a_adapt", "", "Arcane Focus", "Магический фокус", "Magiclysm: +8% reading speed.", "Magiclysm: +8% скорости чтения.", {{ { "read_speed_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0 },
    { "mom_mental_focus", branch_id::mastery, 3, 12, currency_id::perk, "a_focus", "", "Psionic Focus", "Псионический фокус", "Mind Over Matter: +0.5 INT and +5% Survivor XP.", "Mind Over Matter: +0,5 ИНТ и +5% опыта Survivor.", {{ { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 5 },
    { "xe_anomaly_method", branch_id::mastery, 3, 12, currency_id::perk, "a_insight", "", "Anomaly Method", "Метод аномалий", "Xedra Evolved: +1 PER and +5% reading speed.", "Xedra Evolved: +1 ВОС и +5% скорости чтения.", {{ { "per_flat", 1 }, { "read_speed_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0 },
'@
$perkArrayStart = $sp.IndexOf('const perk_def perks[] = {')
if ($perkArrayStart -lt 0) { throw "0.9.14 perk array start not found." }
$perkArrayEnd = $sp.IndexOf("`n};",$perkArrayStart)
if ($perkArrayEnd -lt 0) { throw "0.9.14 perk array end not found." }
$sp = $sp.Insert($perkArrayEnd,"`n" + $extraPerks0914.TrimEnd())

$availabilityHelpers = @'
enum class integration_id {
    none,
    magiclysm,
    mindovermatter,
    xedra_evolved,
    aftershock_exoplanet,
    aftershock_prime,
    secronom,
    secronom_plus
};

integration_id perk_integration( const perk_def &perk )
{
    const std::string_view id = perk.id ? std::string_view( perk.id ) : std::string_view();
    if( id.rfind( "mg_", 0 ) == 0 ) return integration_id::magiclysm;
    if( id.rfind( "mom_", 0 ) == 0 ) return integration_id::mindovermatter;
    if( id.rfind( "xe_", 0 ) == 0 ) return integration_id::xedra_evolved;
    if( id.rfind( "af_", 0 ) == 0 ) return integration_id::aftershock_exoplanet;
    return integration_id::none;
}

bool integration_perk( const perk_def &perk )
{
    return perk_integration( perk ) != integration_id::none;
}

const char *integration_mod_id( integration_id integration )
{
    switch( integration ) {
        case integration_id::magiclysm: return "magiclysm";
        case integration_id::mindovermatter: return "mindovermatter";
        case integration_id::xedra_evolved: return "xedra_evolved";
        case integration_id::aftershock_exoplanet: return "aftershock_exoplanet";
        case integration_id::aftershock_prime: return "aftershock_prime";
        case integration_id::secronom: return "secronom";
        case integration_id::secronom_plus: return "secronom_lore_expansion";
        case integration_id::none: break;
    }
    return "";
}

const char *integration_mod_id( const perk_def &perk )
{
    return integration_mod_id( perk_integration( perk ) );
}

std::string integration_mod_name( const perk_def &perk )
{
    switch( perk_integration( perk ) ) {
        case integration_id::magiclysm: return "Magiclysm";
        case integration_id::mindovermatter: return "Mind Over Matter";
        case integration_id::xedra_evolved: return "Xedra Evolved";
        case integration_id::aftershock_exoplanet: return "Aftershock Exoplanet";
        case integration_id::aftershock_prime: return "Aftershock Prime";
        case integration_id::secronom: return "Secronom";
        case integration_id::secronom_plus: return "Secronom+";
        case integration_id::none: break;
    }
    return {};
}

bool perk_world_available( const perk_def &perk )
{
    const integration_id integration = perk_integration( perk );
    if( integration == integration_id::none ) {
        return true;
    }
    return active_world_mod( integration_mod_id( integration ) );
}

int specialization_root_slot( const char *raw_id )
{
    const std::string_view id = raw_id ? std::string_view( raw_id ) : std::string_view();
    if( id == "spc_c_juggernaut" || id == "spc_s_nomad" || id == "spc_m_sprinter" ||
        id == "spc_f_systems" || id == "spc_g_prospector" || id == "spc_a_specialist" ) return 1;
    if( id == "spc_c_duelist" || id == "spc_s_medic" || id == "spc_m_ghost" ||
        id == "spc_f_improviser" || id == "spc_g_courier" || id == "spc_a_polymath" ) return 2;
    if( id == "spc_c_tactician" || id == "spc_s_quartermaster" || id == "spc_m_pathfinder" ||
        id == "spc_f_researcher" || id == "spc_g_investigator" || id == "spc_a_selfteacher" ) return 3;
    return 0;
}

int specialization_slot( const perk_def &perk )
{
    const int direct = specialization_root_slot( perk.id );
    if( direct > 0 ) {
        return direct;
    }
    return specialization_root_slot( perk.prereq1 );
}

bool specialization_perk( const perk_def &perk )
{
    return specialization_slot( perk ) > 0;
}

bool specialization_root( const perk_def &perk )
{
    return specialization_root_slot( perk.id ) > 0;
}

std::string specialization_state_key( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return "spec_combat";
        case branch_id::survival: return "spec_survival";
        case branch_id::mobility: return "spec_mobility";
        case branch_id::crafting: return "spec_crafting";
        case branch_id::scavenging: return "spec_scavenging";
        case branch_id::mastery: return "spec_mastery";
    }
    return "spec_unknown";
}

bool specialization_allowed( const perk_def &perk )
{
    if( !specialization_perk( perk ) ) {
        return true;
    }
    const int slot = specialization_slot( perk );
    const int64_t selected_slot = get_state( specialization_state_key( perk.branch ), 0 );
    if( specialization_root( perk ) ) {
        return selected_slot == 0 || selected_slot == slot;
    }
    return selected_slot == slot;
}

bool owned( const perk_def &perk )
{
    return perk_world_available( perk ) && perk_rank( perk ) > 0;
}
'@

$sp = Replace-CppRange $sp "bool owned( const perk_def &perk )" "const perk_def *find_perk" $availabilityHelpers "0.9.14 availability and specialization helpers"

$findPerk0914 = @'
const perk_def *find_perk( const char *id )
{
    if( id == nullptr || *id == '\0' ) {
        return nullptr;
    }
    static const std::map<std::string_view, const perk_def *> index = []() {
        std::map<std::string_view, const perk_def *> result;
        for( const perk_def &perk : perks ) {
            result.emplace( std::string_view( perk.id ), &perk );
        }
        return result;
    }();
    const auto it = index.find( std::string_view( id ) );
    if( it == index.end() || !perk_world_available( *it->second ) ) {
        return nullptr;
    }
    return it->second;
}
'@

$sp = Replace-CppRange $sp "const perk_def *find_perk( const char *id )" "int64_t xp_to_next( int64_t level )" $findPerk0914 "0.9.14 conditional find_perk"

# Only visible/active-world perks contribute to UI counts and active effects.
$counts0914 = @'
int branch_owned_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && perk_world_available( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}

int branch_total_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && perk_world_available( perk ) ) {
            ++result;
        }
    }
    return result;
}

int visible_perk_count()
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk_world_available( perk ) ) {
            ++result;
        }
    }
    return result;
}

int owned_count( currency_id currency )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.currency == currency && perk_world_available( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}
'@
$sp = Replace-CppRange $sp "int branch_owned_count( branch_id branch )" "std::string format_number( double value )" $counts0914 "0.9.14 visible perk counts"

$prereq0914 = @'
bool prerequisites_met( const perk_def &perk )
{
    if( !perk_world_available( perk ) || !specialization_allowed( perk ) ) {
        return false;
    }
    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) {
            continue;
        }
        const perk_def *required = find_perk( id );
        if( required == nullptr || !owned( *required ) ) {
            return false;
        }
    }
    return true;
}
'@
$sp = Replace-CppRange $sp "bool prerequisites_met( const perk_def &perk )" "std::string prereq_text( const perk_def &perk )" $prereq0914 "0.9.14 specialization prerequisites"

$purchase0914 = @'
bool purchase_perk( const perk_def &perk )
{
    const int64_t level = branch_level( perk.branch );
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( !perk_world_available( perk ) ) {
        message( tr( "The required mod is not active.", "Требуемый мод не активен." ) );
        return false;
    }
    if( rank >= max_rank ) {
        message( tr( "This perk is already at maximum rank.",
                     "Этот перк уже максимального ранга." ) );
        return false;
    }
    if( level < perk.required_level ) {
        message( tr( "Your branch level is too low.", "Недостаточный уровень этой ветки." ) );
        return false;
    }
    if( !prerequisites_met( perk ) ) {
        if( specialization_perk( perk ) && !specialization_allowed( perk ) ) {
            message( tr( "Another specialization is already committed in this branch. Respec to change it.",
                         "В этой ветке уже выбрана другая специализация. Для смены нужен сброс." ) );
        } else {
            message( tr( "Prerequisites are not met.", "Не выполнены требования предыдущих перков." ) );
        }
        return false;
    }

    int chosen_slot = 0;
    if( specialization_root( perk ) ) {
        chosen_slot = specialization_slot( perk );
        const int64_t existing = get_state( specialization_state_key( perk.branch ), 0 );
        if( existing == 0 ) {
            std::string prompt = tr(
                "Commit to this specialization? The other two paths in this branch will lock until a full respec.\n",
                "Выбрать эту специализацию? Два других пути этой ветки закроются до полного сброса.\n" );
            prompt += perk_display_name( perk );
            std::string yes = tr( "Commit", "Выбрать" );
            std::string no = tr( "Cancel", "Отмена" );
            const char *entries[] = { yes.c_str(), no.c_str() };
            const int choice = host->ui_choose ? host->ui_choose( prompt.c_str(), entries, 2 ) : -1;
            if( choice != 0 ) {
                return false;
            }
        }
    }

    if( perk.currency == currency_id::perk ) {
        if( perk_points <= 0 ) {
            message( tr( "Not enough perk points.", "Недостаточно очков перков." ) );
            return false;
        }
        set_state( "perk_points", perk_points - 1 );
    } else {
        if( major_points <= 0 ) {
            message( tr( "Not enough major points.", "Недостаточно больших очков." ) );
            return false;
        }
        set_state( "major_points", major_points - 1 );
    }

    if( chosen_slot > 0 ) {
        set_state( specialization_state_key( perk.branch ), chosen_slot );
    }
    set_state( perk_key( perk ), rank + 1 );
    effects_dirty = true;
    recalculate_effects();

    std::string text = rank == 0 ?
                       tr( "Perk purchased: ", "Куплен перк: " ) :
                       tr( "Perk upgraded: ", "Перк улучшен: " );
    text += russian() ? perk.name_ru : perk.name_en;
    if( max_rank > 1 ) {
        text += " " + std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
    }
    message( text );
    return true;
}
'@
$sp = Replace-CppRange $sp "bool purchase_perk( const perk_def &perk )" "void show_perk_detail( const perk_def &perk )" $purchase0914 "0.9.14 specialization commit purchase"

$position0914 = @'
std::pair<int, int> branch_tree_position( branch_id branch, size_t branch_index )
{
    // Base 20-node topology is preserved exactly for save/UI familiarity.
    static const std::array<std::pair<int, int>, 20> standard = {{
        { 0, 0 }, { 0, 4 }, { 2, 0 }, { 2, 4 }, { 4, 0 }, { 4, 4 },
        { 6, 0 }, { 6, 4 }, { 8, 2 }, { 10, 2 },
        { 1, 1 }, { 1, 3 }, { 3, 1 }, { 3, 3 }, { 5, 1 }, { 5, 3 },
        { 7, 1 }, { 7, 3 }, { 9, 2 }, { 11, 2 }
    }};
    static const std::array<std::pair<int, int>, 20> mobility = {{
        { 0, 0 }, { 0, 4 }, { 2, 1 }, { 2, 3 }, { 4, 0 }, { 4, 4 },
        { 6, 1 }, { 6, 3 }, { 8, 2 }, { 10, 2 },
        { 1, 1 }, { 1, 3 }, { 3, 0 }, { 3, 4 }, { 5, 1 }, { 5, 3 },
        { 7, 0 }, { 7, 4 }, { 9, 2 }, { 11, 2 }
    }};
    static const std::array<std::pair<int, int>, 20> mastery = {{
        { 0, 1 }, { 0, 3 }, { 2, 0 }, { 2, 4 }, { 4, 1 }, { 4, 3 },
        { 6, 0 }, { 6, 4 }, { 8, 2 }, { 10, 2 },
        { 1, 2 }, { 1, 4 }, { 3, 1 }, { 3, 3 }, { 5, 2 }, { 5, 4 },
        { 7, 1 }, { 7, 3 }, { 9, 2 }, { 11, 2 }
    }};

    if( branch_index < 20 ) {
        if( branch == branch_id::mobility || branch == branch_id::scavenging ) {
            return mobility[branch_index];
        }
        if( branch == branch_id::mastery ) {
            return mastery[branch_index];
        }
        return standard[branch_index];
    }

    // 20..22 are exclusive specialization roots; 23..25 their capstones.
    if( branch_index < 23 ) {
        return { 12, static_cast<int>( ( branch_index - 20 ) * 2 ) };
    }
    if( branch_index < 26 ) {
        return { 14, static_cast<int>( ( branch_index - 23 ) * 2 ) };
    }

    // Conditional mod-integration nodes live below specializations in a compact grid.
    const size_t integration_index = branch_index - 26;
    return { 16 + static_cast<int>( integration_index / 3 ) * 2,
             static_cast<int>( integration_index % 3 ) * 2 };
}
'@
$sp = Replace-CppRange $sp "std::pair<int, int> branch_tree_position( branch_id branch, size_t branch_index )" "std::vector<ncmm_ui_tree_node_v1> bind_tree_nodes" $position0914 "0.9.14 specialization tree layout"

$perkKind0914 = @'
std::string perk_kind_label( const perk_def &perk )
{
    if( specialization_perk( perk ) ) {
        return tr( "SPECIALIZATION", "СПЕЦИАЛИЗАЦИЯ" );
    }
    if( integration_perk( perk ) ) {
        return tr( "MOD PERK", "ПЕРК МОДА" );
    }
    if( perk.currency == currency_id::major ) {
        return tr( "MAJOR PERK", "БОЛЬШОЙ ПЕРК" );
    }
    return tr( "PERK", "ПЕРК" );
}
'@
$sp = Replace-CppRange $sp "std::string perk_kind_label( const perk_def &perk )" "std::string branch_icon_key( branch_id branch )" $perkKind0914 "0.9.14 perk badges"

# Hide integration nodes when their parent world mod is not active.
$sp = $sp.Replace('        for( const perk_def &perk : perks ) {' + "`n" + '            if( perk.branch != branch ) {',
                  '        for( const perk_def &perk : perks ) {' + "`n" + '            if( perk.branch != branch || !perk_world_available( perk ) ) {')

# UI total uses only perks that can exist in the active world.
$sp = $sp.Replace('const int total_perks = static_cast<int>( sizeof( perks ) / sizeof( perks[0] ) );',
                  'const int total_perks = visible_perk_count();')

# Respec also clears exclusive branch commitments; hidden integration purchases are
# deliberately still refunded because perk_rank() reads raw saved rank state.
$respecNeedle = '    set_state( "fast_learner", 0 );' + "`n" + '    effects_dirty = true;'
$respecReplacement = @'
    set_state( "fast_learner", 0 );
    for( branch_id branch : all_branches ) {
        set_state( specialization_state_key( branch ), 0 );
    }
    effects_dirty = true;
'@
if (-not $sp.Contains($respecNeedle)) { throw "0.9.14 respec specialization anchor not found." }
$sp = Replace-TextBlock $sp $respecNeedle $respecReplacement "0.9.14 respec specialization state"

# Initialize/sanitize specialization slots during schema-7 migration.
$migrateNeedle = '        set_state( branch_state_key( branch, "xp" ),' + "`n" +
                 '                   std::max<int64_t>( 0, get_state( branch_state_key( branch, "xp" ), 0 ) ) );' + "`n" +
                 '    }'
$migrateReplacement = @'
        set_state( branch_state_key( branch, "xp" ),
                   std::max<int64_t>( 0, get_state( branch_state_key( branch, "xp" ), 0 ) ) );
        const int64_t selected_spec = get_state( specialization_state_key( branch ), 0 );
        set_state( specialization_state_key( branch ),
                   selected_spec >= 1 && selected_spec <= 3 ? selected_spec : 0 );
    }
'@
if (-not $sp.Contains($migrateNeedle)) { throw "0.9.14 migration specialization anchor not found." }
$sp = Replace-TextBlock $sp $migrateNeedle $migrateReplacement "0.9.14 migration specialization state"

# Deep integrations also affect EARNED thematic branch XP, never passive time XP.
$poll0914 = @'
int integration_branch_xp_bonus_pct( branch_id branch )
{
    int bonus = 0;
    if( active_world_mod( "magiclysm" ) &&
        ( branch == branch_id::crafting || branch == branch_id::mastery ) ) bonus += 10;
    if( active_world_mod( "mindovermatter" ) &&
        ( branch == branch_id::mobility || branch == branch_id::mastery ) ) bonus += 10;
    if( active_world_mod( "xedra_evolved" ) &&
        ( branch == branch_id::scavenging || branch == branch_id::mastery ) ) bonus += 10;
    if( active_world_mod( "aftershock_exoplanet" ) &&
        ( branch == branch_id::crafting || branch == branch_id::scavenging ) ) bonus += 10;
    return std::min( bonus, 20 );
}

int64_t scale_activity_xp( branch_id branch, int64_t raw, int diversity_bonus_pct )
{
    if( raw <= 0 ) return 0;
    const int total_bonus = diversity_bonus_pct + integration_branch_xp_bonus_pct( branch );
    if( total_bonus <= 0 ) return raw;
    const std::string key = branch_state_key( branch, "activity_bonus_fraction" );
    int64_t scaled = raw * ( 100 + total_bonus ) +
                     std::max<int64_t>( 0, get_state( key, 0 ) );
    const int64_t result = scaled / 100;
    set_state( key, scaled % 100 );
    return result;
}

void poll_branch_xp()
{
    decay_branch_fatigue();

    const int64_t kills = metric_delta( "combat.kills", "metric_combat_kills" );
    const int64_t kill_xp = metric_delta( "combat.kill_xp", "metric_combat_kill_xp" );
    int64_t combat_gain = std::min<int64_t>( std::max<int64_t>( kills, ( kill_xp + 49 ) / 50 ), 25 );

    int64_t healing = metric_delta( "survival.healing", "metric_survival_healing" );
    healing += get_state( "survival_heal_remainder", 0 );
    int64_t survival_gain = std::min<int64_t>( healing / 10, 8 );
    set_state( "survival_heal_remainder", healing % 10 );

    int64_t steps = metric_delta( "mobility.steps", "metric_mobility_steps" );
    steps += get_state( "mobility_step_remainder", 0 );
    int64_t mobility_gain = std::min<int64_t>( steps / 150, 3 );
    set_state( "mobility_step_remainder", steps % 150 );

    const int64_t crafts = metric_delta( "crafting.completed", "metric_crafting_completed" );
    int64_t crafting_gain = std::min<int64_t>( crafts * 3, 12 );
    const int64_t omts = metric_delta( "scavenging.omt", "metric_scavenging_omt" );
    int64_t scavenging_gain = std::min<int64_t>( omts * 4, 8 );
    const int64_t skill_levels = metric_delta( "mastery.skill_levels", "metric_mastery_skill_levels" );
    int64_t mastery_gain = std::min<int64_t>( skill_levels * 6, 18 );

    int active = 0;
    active += combat_gain > 0 ? 1 : 0;
    active += survival_gain > 0 ? 1 : 0;
    active += mobility_gain > 0 ? 1 : 0;
    active += crafting_gain > 0 ? 1 : 0;
    active += scavenging_gain > 0 ? 1 : 0;
    const int diversity_bonus = activity_diversity_bonus_pct( active );

    combat_gain = scale_activity_xp( branch_id::combat, combat_gain, diversity_bonus );
    survival_gain = scale_activity_xp( branch_id::survival, survival_gain, diversity_bonus );
    mobility_gain = scale_activity_xp( branch_id::mobility, mobility_gain, diversity_bonus );
    crafting_gain = scale_activity_xp( branch_id::crafting, crafting_gain, diversity_bonus );
    scavenging_gain = scale_activity_xp( branch_id::scavenging, scavenging_gain, diversity_bonus );
    mastery_gain = scale_activity_xp( branch_id::mastery, mastery_gain, 0 );

    int64_t activity_total = 0;
    activity_total += award_branch_xp( branch_id::combat, combat_gain );
    activity_total += award_branch_xp( branch_id::survival, survival_gain );
    activity_total += award_branch_xp( branch_id::mobility, mobility_gain );
    activity_total += award_branch_xp( branch_id::crafting, crafting_gain );
    activity_total += award_branch_xp( branch_id::scavenging, scavenging_gain );

    int64_t mastery_fraction = get_state( "mastery_share_fraction", 0 );
    mastery_fraction += activity_total * 10;
    mastery_gain += mastery_fraction / 100;
    mastery_fraction %= 100;
    set_state( "mastery_share_fraction", mastery_fraction );
    award_branch_xp( branch_id::mastery, mastery_gain );
}
'@
$sp = Replace-CppRange $sp "void poll_branch_xp()" "void tick()" $poll0914 "0.9.14 thematic mod XP integration"

# Overview explicitly lists detected integrations.
$overviewAnchor = '    out += "\n" + tr( "Major points: every 5 levels, with no level cap.",' + "`n" +
                  '                       "Большие очки: каждые 5 уровней, без ограничения уровня." );'
$overviewInsert = @'
    out += "\n" + tr( "Major points: every 5 levels, with no level cap.",
                       "Большие очки: каждые 5 уровней, без ограничения уровня." );
    out += "\n" + tr( "Mod perks available for: ", "Перки модов доступны для: " );
    std::vector<std::string> integration_names;
    if( active_world_mod( "magiclysm" ) ) integration_names.push_back( "Magiclysm" );
    if( active_world_mod( "mindovermatter" ) ) integration_names.push_back( "Mind Over Matter" );
    if( active_world_mod( "xedra_evolved" ) ) integration_names.push_back( "Xedra Evolved" );
    if( active_world_mod( "aftershock_exoplanet" ) ) integration_names.push_back( "Aftershock Exoplanet" );
    if( active_world_mod( "aftershock_prime" ) ) integration_names.push_back( "Aftershock Prime" );
    if( active_world_mod( "secronom" ) ) integration_names.push_back( "Secronom" );
    if( active_world_mod( "secronom_lore_expansion" ) ) integration_names.push_back( "Secronom+" );
    if( integration_names.empty() ) {
        out += tr( "none", "нет" );
    } else {
        for( size_t i = 0; i < integration_names.size(); ++i ) {
            if( i != 0 ) out += ", ";
            out += integration_names[i];
        }
    }
'@
if (-not $sp.Contains($overviewAnchor)) { throw "0.9.14 overview integration anchor not found." }
$sp = Replace-TextBlock $sp $overviewAnchor $overviewInsert "0.9.14 overview integrations"

# Detail window explains exclusive and mod-dependent nodes.
$detailAnchor = '        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );'
$detailReplacement = @'
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );
        if( specialization_root( perk ) ) {
            title += "\n" + tr( "Exclusive choice: the other two specializations stay locked until full respec.",
                                 "Эксклюзивный выбор: две другие специализации будут закрыты до полного сброса." );
        }
        if( integration_perk( perk ) ) {
            title += "\n" + tr( "Requires mod: ", "Нужен мод: " ) + integration_mod_name( perk );
        }
'@
if (-not $sp.Contains($detailAnchor)) { throw "0.9.14 detail integration anchor not found." }
$sp = Replace-TextBlock $sp $detailAnchor $detailReplacement "0.9.14 detail integrations"

# ---------------------------------------------------------------------------
# v7 — logical tree navigation + dedicated active-mod branches
# ---------------------------------------------------------------------------
Write-Host "Applying v7 UI correction: logical arrows + dedicated mod branches..." -ForegroundColor Cyan

# Tree keyboard navigation is now based on DECLARED logical rows/columns, not on
# the auto-routed connector geometry.  Vertical input moves exactly one occupied
# logical row at a time; horizontal input stays on the current row and moves to
# the nearest declared column.  This makes every node reachable without random
# multi-row jumps caused by parent-centering of layout_x2.
$loader = [IO.File]::ReadAllText($loaderPath)
$oldTreeNav = @'
    auto select_direction = [&]( int row_sign, int col_sign ) {
        int best = -1;
        int best_score = 1000000;
        const int sr = nodes[selected].row;
        const int sc = layout_x2[selected];
        for( size_t i = 0; i < node_count; ++i ) {
            if( static_cast<int>( i ) == selected ) continue;
            const int dr = nodes[i].row - sr;
            const int dc = layout_x2[i] - sc;
            if( row_sign < 0 && dr >= 0 ) continue;
            if( row_sign > 0 && dr <= 0 ) continue;
            if( col_sign < 0 && dc >= 0 ) continue;
            if( col_sign > 0 && dc <= 0 ) continue;
            const int primary = row_sign != 0 ? std::abs( dr ) : std::abs( dc );
            const int secondary = row_sign != 0 ? std::abs( dc ) : std::abs( dr );
            const int score = primary * 100 + secondary * 10;
            if( score < best_score ) {
                best_score = score;
                best = static_cast<int>( i );
            }
        }
        if( best >= 0 ) selected = best;
    };
'@
$newTreeNav = @'
    // v7: logical navigation follows the declared tree grid instead of the
    // post-routing layout_x2.  This prevents multi-row jumps and makes every
    // tile on an occupied row reachable by arrows.
    auto select_direction = [&]( int row_sign, int col_sign ) {
        const int sr = nodes[selected].row;
        const int sc = nodes[selected].column;
        int best = -1;

        if( row_sign != 0 ) {
            int nearest_row_delta = 1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( static_cast<int>( i ) == selected ) {
                    continue;
                }
                const int dr = nodes[i].row - sr;
                if( ( row_sign < 0 && dr >= 0 ) || ( row_sign > 0 && dr <= 0 ) ) {
                    continue;
                }
                nearest_row_delta = std::min( nearest_row_delta, std::abs( dr ) );
            }
            if( nearest_row_delta == 1000000 ) {
                return;
            }

            int best_col_delta = 1000000;
            int best_declared_col = 1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( static_cast<int>( i ) == selected ) {
                    continue;
                }
                const int dr = nodes[i].row - sr;
                if( ( row_sign < 0 && dr >= 0 ) || ( row_sign > 0 && dr <= 0 ) ||
                    std::abs( dr ) != nearest_row_delta ) {
                    continue;
                }
                const int col_delta = std::abs( nodes[i].column - sc );
                if( col_delta < best_col_delta ||
                    ( col_delta == best_col_delta && nodes[i].column < best_declared_col ) ) {
                    best_col_delta = col_delta;
                    best_declared_col = nodes[i].column;
                    best = static_cast<int>( i );
                }
            }
        } else if( col_sign != 0 ) {
            int nearest_col_delta = 1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( static_cast<int>( i ) == selected || nodes[i].row != sr ) {
                    continue;
                }
                const int dc = nodes[i].column - sc;
                if( ( col_sign < 0 && dc >= 0 ) || ( col_sign > 0 && dc <= 0 ) ) {
                    continue;
                }
                const int col_delta = std::abs( dc );
                if( col_delta < nearest_col_delta ) {
                    nearest_col_delta = col_delta;
                    best = static_cast<int>( i );
                }
            }
        }

        if( best >= 0 ) {
            selected = best;
        }
    };
'@
$loader = Replace-TextBlock $loader $oldTreeNav $newTreeNav "v7 logical tree navigation"
$loader = $loader.Replace(
    '            "Mouse: hover/click/wheel  Tab: cards  Enter: details  Esc: back",' + "`n" +
    '            "Мышь: наведение/клик/колесо  Tab: карточки  Enter: детали  Esc: назад" );',
    '            "Arrows: nearest node  Mouse: hover/click/wheel  Tab: cards  Enter: details  Esc: back",' + "`n" +
    '            "Стрелки: соседний узел  Мышь: наведение/клик/колесо  Tab: карточки  Enter: детали  Esc: назад" );'
)
Write-Utf8NoBom $loaderPath $loader

# Base Survivor branch counters no longer absorb active-mod integration nodes.
# Integration purchases still remain part of global purchased/visible totals.
$countsV7 = @'
int branch_owned_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !integration_perk( perk ) &&
            perk_world_available( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}

int branch_total_count( branch_id branch )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !integration_perk( perk ) &&
            perk_world_available( perk ) ) {
            ++result;
        }
    }
    return result;
}

int visible_perk_count()
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk_world_available( perk ) ) {
            ++result;
        }
    }
    return result;
}

int owned_count( currency_id currency )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.currency == currency && perk_world_available( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}
'@
$sp = Replace-CppRange $sp "int branch_owned_count( branch_id branch )" "std::string format_number( double value )" $countsV7 "v7 base-branch counts"

$unlockedV7 = @'
int branch_unlocked_count( branch_id branch, int64_t )
{
    const int64_t level = branch_level( branch );
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( perk.branch == branch && !integration_perk( perk ) &&
            perk_world_available( perk ) && !perk_maxed( perk ) &&
            level >= perk.required_level && prerequisites_met( perk ) ) {
            ++result;
        }
    }
    return result;
}
'@
$sp = Replace-CppRange $sp "int branch_unlocked_count( branch_id branch, int64_t )" "struct card_text {" $unlockedV7 "v7 base-branch unlocked count"

$oldBaseFilter = '            if( perk.branch != branch || !perk_world_available( perk ) ) {'
$newBaseFilter = '            if( perk.branch != branch || integration_perk( perk ) || !perk_world_available( perk ) ) {'
if (-not $sp.Contains($oldBaseFilter)) {
    throw "v7 base-branch filter anchor not found."
}
$sp = $sp.Replace($oldBaseFilter,$newBaseFilter)

$openProgressionV7 = @'
const char *integration_anchor_id( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return "mg_arcane_focus";
    if( mod_id == "mindovermatter" ) return "mom_mental_focus";
    if( mod_id == "xedra_evolved" ) return "xe_anomaly_method";
    if( mod_id == "aftershock_exoplanet" ) return "af_systems_operator";
    if( mod_id == "aftershock_prime" ) return "afp_prime_operator";
    if( mod_id == "secronom" ) return "sec_field_researcher";
    if( mod_id == "secronom_lore_expansion" ) return "secx_flesh_initiate";
    return "";
}

std::string integration_mod_display_name( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return "Magiclysm";
    if( mod_id == "mindovermatter" ) return "Mind Over Matter";
    if( mod_id == "xedra_evolved" ) return "Xedra Evolved";
    if( mod_id == "aftershock_exoplanet" ) return "Aftershock Exoplanet";
    if( mod_id == "aftershock_prime" ) return "Aftershock Prime";
    if( mod_id == "secronom" ) return "Secronom";
    if( mod_id == "secronom_lore_expansion" ) return "Secronom+";
    return mod_id;
}

std::string integration_mod_focus( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) {
        return tr( "Magiclysm perks for spells, survival and crafting.",
                   "Перки Magiclysm для магии, выживания и крафта." );
    }
    if( mod_id == "mindovermatter" ) {
        return tr( "Mind Over Matter perks for psionics, mobility and endurance.",
                   "Перки Mind Over Matter для псионики, мобильности и выносливости." );
    }
    if( mod_id == "xedra_evolved" ) {
        return tr( "Xedra Evolved perks for anomaly research, field work and combat.",
                   "Перки Xedra Evolved для исследования аномалий, полевой работы и боя." );
    }
    if( mod_id == "aftershock_exoplanet" ) {
        return tr( "Aftershock Exoplanet perks for combat, survival and expedition gear.",
                   "Перки Aftershock Exoplanet для боя, выживания и экспедиционного снаряжения." );
    }
    return {};
}

int integration_owned_count( const std::string &mod_id )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( integration_perk( perk ) && perk_world_available( perk ) &&
            mod_id == integration_mod_id( perk ) && owned( perk ) ) {
            ++result;
        }
    }
    return result;
}

int integration_total_count( const std::string &mod_id )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( integration_perk( perk ) && perk_world_available( perk ) &&
            mod_id == integration_mod_id( perk ) ) {
            ++result;
        }
    }
    return result;
}

void show_integration_branch( const std::string &mod_id )
{
    if( !active_world_mod( mod_id.c_str() ) ) {
        message( tr( "This mod is not active in the current world.",
                     "Этот мод не активен в текущем мире." ) );
        return;
    }

    bool tree_mode = true;
    while( true ) {
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        std::vector<const perk_def *> mod_perks;
        mod_perks.reserve( 4 );

        const char *anchor_id = integration_anchor_id( mod_id );
        const perk_def *anchor = find_perk( anchor_id );
        if( anchor != nullptr && integration_perk( *anchor ) &&
            mod_id == integration_mod_id( *anchor ) ) {
            mod_perks.push_back( anchor );
        }
        for( const perk_def &perk : perks ) {
            if( !integration_perk( perk ) || !perk_world_available( perk ) ||
                mod_id != integration_mod_id( perk ) || &perk == anchor ) {
                continue;
            }
            mod_perks.push_back( &perk );
        }

        if( mod_perks.empty() ) {
            message( tr( "No Survivor perks are available for this mod.",
                         "Для этого мода нет доступных перков Survivor." ) );
            return;
        }

        std::vector<card_text> texts;
        texts.reserve( mod_perks.size() );
        for( const perk_def *perk_ptr : mod_perks ) {
            const perk_def &perk = *perk_ptr;
            const int level = static_cast<int>( branch_level( perk.branch ) );
            const int rank = perk_rank( perk );
            const int max_rank = perk_max_rank( perk );
            const bool maxed = rank >= max_rank;
            const bool unlocked = level >= perk.required_level && prerequisites_met( perk );
            const bool enough = perk.currency == currency_id::perk ?
                                perk_points > 0 : major_points > 0;

            card_text card;
            card.id = perk.id;
            card.title = perk_display_name( perk );
            card.subtitle = branch_name( perk.branch ) + " | " +
                            tr( "Lv ", "Ур " ) + std::to_string( perk.required_level ) +
                            " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
            card.body = perk_description( perk ) + "\n" +
                        tr( "Base branch gate: ", "Требование базовой ветки: " ) +
                        branch_name( perk.branch ) + " L" +
                        std::to_string( perk.required_level );

            card.badge = perk_kind_label( perk );
            const std::string chevrons = rank_chevrons( perk );
            if( !chevrons.empty() ) card.badge += " | " + chevrons;
            card.badge += " | ";
            if( maxed ) {
                card.badge += max_rank > 1 ?
                              tr( "MAX ", "МАКС " ) + std::to_string( rank ) + "/" +
                              std::to_string( max_rank ) :
                              tr( "OWNED", "КУПЛЕНО" );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( rank > 0 ) {
                card.badge += tr( "RANK ", "РАНГ " ) + std::to_string( rank ) + "/" +
                              std::to_string( max_rank );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( !unlocked ) {
                card.badge += tr( "LOCKED", "ЗАКРЫТО" );
                card.flags |= NCMM_UI_CARD_LOCKED;
            } else if( !enough ) {
                card.badge += tr( "NO POINTS", "НЕТ ОЧКОВ" );
            } else {
                card.badge += tr( "AVAILABLE", "ДОСТУПНО" );
            }

            if( perk.currency == currency_id::major ) card.flags |= NCMM_UI_CARD_MAJOR;
            if( effective_kind( perk ) == perk_kind::effect ) card.flags |= NCMM_UI_CARD_EFFECT;
            card.icon_key = std::string( "survivor/mod/" ) + mod_id + "/" + perk.id;
            texts.push_back( std::move( card ) );
        }

        const std::string mod_name = integration_mod_display_name( mod_id );
        std::string title = "Survivor Progression > " + mod_name;
        std::string summary =
            tr( "Mod perks", "Перки мода" ) +
            tr( " | purchased ", " | куплено " ) +
            std::to_string( integration_owned_count( mod_id ) ) + "/" +
            std::to_string( integration_total_count( mod_id ) ) +
            " | P " + std::to_string( perk_points ) +
            " | M " + std::to_string( major_points ) +
            tr( " | nodes use their linked base-branch levels",
                " | узлы используют уровни связанных базовых веток" );

        if( tree_mode && host->ui_tree_choose ) {
            std::vector<tree_node_text> tree_texts;
            tree_texts.reserve( mod_perks.size() );
            std::map<std::string, size_t> index_by_id;

            for( size_t i = 0; i < mod_perks.size(); ++i ) {
                const perk_def &perk = *mod_perks[i];
                tree_node_text node;
                node.card = texts[i];
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( perk );
                if( i == 0 ) {
                    node.row = 0;
                    node.column = 2;
                } else {
                    node.row = 2;
                    node.column = static_cast<int>( i - 1 ) * 2;
                }
                index_by_id[perk.id] = i;
                tree_texts.push_back( std::move( node ) );
            }

            std::vector<ncmm_ui_tree_edge_v1> edges;
            auto add_edge = [&]( const char *prereq, size_t to ) {
                if( prereq == nullptr || prereq[0] == '\0' ) return;
                const auto it = index_by_id.find( prereq );
                if( it != index_by_id.end() ) {
                    edges.push_back( { it->second, to } );
                }
            };
            for( size_t i = 0; i < mod_perks.size(); ++i ) {
                add_edge( mod_perks[i]->prereq1, i );
                add_edge( mod_perks[i]->prereq2, i );
            }

            std::vector<ncmm_ui_tree_node_v1> nodes = bind_tree_nodes( tree_texts );
            const int choice = host->ui_tree_choose(
                                   title.c_str(), summary.c_str(), nullptr,
                                   nodes.data(), nodes.size(), edges.data(), edges.size() );
            if( choice == NCMM_UI_TREE_SHOW_CARDS ) {
                tree_mode = false;
                continue;
            }
            if( choice < 0 || static_cast<size_t>( choice ) >= mod_perks.size() ) {
                return;
            }
            show_perk_detail( *mod_perks[choice] );
            continue;
        }

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), nullptr,
                                                 cards.data(), cards.size(), 2 ) :
                           -1;
        if( choice == NCMM_UI_CARD_SHOW_TREE ) {
            tree_mode = true;
            continue;
        }
        if( choice < 0 || static_cast<size_t>( choice ) >= mod_perks.size() ) {
            return;
        }
        show_perk_detail( *mod_perks[choice] );
    }
}

void open_progression()
{
    if( !character_available() ) {
        message( tr( "Survivor Progression: load a character first.",
                     "Survivor Progression: сначала загрузите персонажа." ) );
        return;
    }

    migrate_state();
    if( effects_dirty ) {
        recalculate_effects();
    }

    while( true ) {
        const int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
        const int64_t xp = std::max<int64_t>( 0, get_state( "xp", 0 ) );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        const std::array<branch_id, 6> branches = {
            branch_id::combat, branch_id::survival, branch_id::mobility,
            branch_id::crafting, branch_id::scavenging, branch_id::mastery
        };

        std::vector<card_text> texts;
        texts.reserve( 16 );
        for( branch_id branch : branches ) {
            card_text card;
            card.id = branch_name_en( branch );
            card.title = branch_name( branch );
            const int64_t blevel = branch_level( branch );
            const int64_t bxp = branch_xp( branch );
            const int64_t bnext = branch_xp_to_next( blevel );
            card.subtitle =
                tr( "Lv ", "Ур. " ) + std::to_string( blevel ) +
                " | XP " + std::to_string( bxp ) + "/" + std::to_string( bnext ) +
                " | " + std::to_string( branch_owned_count( branch ) ) + "/" +
                std::to_string( branch_total_count( branch ) );
            card.body = branch_focus( branch ) + "\n" + branch_xp_source( branch ) +
                        "\n" + branch_efficiency_text( branch );
            card.badge = tr( "BRANCH", "ВЕТКА" );
            card.icon_key = branch_icon_key( branch );
            card.flags = NCMM_UI_CARD_ACCENT;
            texts.push_back( std::move( card ) );
        }

        std::vector<std::string> mod_branches;
        auto add_mod_branch = [&]( const char *mod_id ) {
            if( !active_world_mod( mod_id ) ) {
                return;
            }
            const std::string id = mod_id;
            mod_branches.push_back( id );

            card_text card;
            card.id = std::string( "mod_" ) + id;
            card.title = integration_mod_display_name( id );
            card.subtitle =
                tr( "Mod perks | ", "Перки мода | " ) +
                std::to_string( integration_owned_count( id ) ) + "/" +
                std::to_string( integration_total_count( id ) );
            card.body = integration_mod_focus( id ) + "\n" +
                        tr( "Available only in worlds where this mod is active.",
                            "Доступно только в мирах, где активен этот мод." );
            card.badge = tr( "MOD PERKS", "ПЕРКИ МОДА" );
            card.icon_key = std::string( "survivor/mod/" ) + id;
            card.flags = NCMM_UI_CARD_ACCENT;
            texts.push_back( std::move( card ) );
        };
        add_mod_branch( "magiclysm" );
        add_mod_branch( "mindovermatter" );
        add_mod_branch( "xedra_evolved" );
        add_mod_branch( "aftershock_exoplanet" );
        add_mod_branch( "aftershock_prime" );
        add_mod_branch( "secronom" );
        add_mod_branch( "secronom_lore_expansion" );

        const int overview_index = static_cast<int>( texts.size() );
        card_text overview;
        overview.id = "overview";
        overview.title = tr( "Overview", "Обзор" );
        overview.subtitle = tr( "Level / points / active effects", "Уровень / очки / активные эффекты" );
        overview.body = tr( "View your level, points, branch progress and active bonuses.",
                            "Уровень, очки, прогресс веток и действующие бонусы." );
        overview.badge = tr( "INFO", "ИНФО" );
        overview.icon_key = "survivor/action/overview";
        texts.push_back( std::move( overview ) );

        const int respec_index = static_cast<int>( texts.size() );
        card_text reset;
        reset.id = "respec";
        reset.title = tr( "Respec", "Сброс перков" );
        reset.subtitle = tr( "Refund every purchase", "Вернуть все покупки" );
        reset.body = tr( "Refund perk and major points and clear Survivor modifiers.",
                         "Вернуть очки и снять модификаторы Survivor." );
        reset.badge = tr( "ACTION", "ДЕЙСТВИЕ" );
        reset.icon_key = "survivor/action/respec";
        texts.push_back( std::move( reset ) );

        const int close_index = static_cast<int>( texts.size() );
        card_text close;
        close.id = "close";
        close.title = tr( "Close", "Закрыть" );
        close.subtitle = tr( "Return to game", "Вернуться в игру" );
        close.body = tr( "Keep your build and continue playing.",
                         "Сохранить билд и вернуться в игру." );
        close.badge = tr( "ACTION", "ДЕЙСТВИЕ" );
        close.icon_key = "survivor/action/close";
        texts.push_back( std::move( close ) );

        const int total_owned = owned_count( currency_id::perk ) +
                                owned_count( currency_id::major );
        const int total_perks = visible_perk_count();

        std::string title = "Survivor Progression v0.9.14";
        std::string summary =
            tr( "Level ", "Уровень " ) + std::to_string( level ) +
            " | P " + std::to_string( perk_points ) +
            " | M " + std::to_string( major_points ) +
            tr( " | purchased ", " | куплено " ) +
            std::to_string( total_owned ) + "/" + std::to_string( total_perks );
        if( !mod_branches.empty() ) {
            summary += tr( " | mods ", " | модов " ) +
                       std::to_string( mod_branches.size() );
        }

        const int64_t xp_needed = xp_to_next( level );
        std::string progress_label =
            "XP " + std::to_string( xp ) + "/" + std::to_string( xp_needed ) +
            tr( " -> Level ", " -> Уровень " ) + std::to_string( level + 1 );
        ncmm_ui_progress_v1 progress{ progress_label.c_str(), xp, xp_needed };

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 3 ) :
                           -1;

        if( choice >= 0 && choice < static_cast<int>( branches.size() ) ) {
            show_branch( branches[static_cast<size_t>( choice )] );
            continue;
        }

        const int mod_offset = static_cast<int>( branches.size() );
        const int mod_end = mod_offset + static_cast<int>( mod_branches.size() );
        if( choice >= mod_offset && choice < mod_end ) {
            show_integration_branch( mod_branches[static_cast<size_t>( choice - mod_offset )] );
            continue;
        }
        if( choice == overview_index ) {
            show_overview();
            continue;
        }
        if( choice == respec_index ) {
            respec();
            continue;
        }
        if( choice == close_index || choice < 0 ) {
            return;
        }
    }
}
'@
$sp = Replace-CppRange $sp "void open_progression()" "void award_global_xp(" $openProgressionV7 "v7 dedicated mod branches"

$sp = $sp.Replace('"0.9.13"','"0.9.14"')
$sp = $sp.Replace('Survivor Progression v0.9.13','Survivor Progression v0.9.14')
$sp = $sp.Replace('Survivor Progression 0.9.13 initialized:','Survivor Progression 0.9.14 initialized:')
$sp = $sp.Replace('anti-farm branch XP / routed trees / ranked foundational perks.',
                  'branch progress / exclusive specializations / perks for supported mods.')
Write-Utf8NoBom $spPath $sp

$manifest = [IO.File]::ReadAllText($manifestPath).Replace('"version": "0.9.13"','"version": "0.9.14"')
$manifest = $manifest.Replace('"state_schema": 6','"state_schema": 7')
Write-Utf8NoBom $manifestPath $manifest

# ---------------------------------------------------------------------------
# v8 — audited mod-native progression redesign (still Survivor 0.9.14)
# ---------------------------------------------------------------------------
Write-Host "Applying v8.7.6.6 base: Prime specializations + registry + World Settings API v2..." -ForegroundColor Cyan
$sp = [IO.File]::ReadAllText($spPath)

# Replace the old four-per-mod generic-stat integrations. IDs that existed in v7
# are deliberately reused in the new 20-node trees so purchased ranks remain valid.
$legacyIntegrationIds = @(
    'mg_arcane_focus','mg_battlemage','mg_ritual_craft','mg_wayfarer',
    'mom_mental_focus','mom_kinetic_control','mom_combat_focus','mom_neural_reserve',
    'xe_anomaly_method','xe_field_agent','xe_dimensional_hunter','xe_occult_engineer',
    'af_systems_operator','af_expedition_logistics','af_combat_technician','af_conditioning'
)
# v8.4 hotfix: scope legacy removal to actual perk_def rows.
# v8.3 used a broad line regex and therefore also matched ranked-perk metadata such as
#     { "mg_arcane_focus", 5, 0.125 }, ...
# which made one real perk definition look like two matches.  Requiring branch_id::
# keeps the rank table intact while removing only the 0.9.14 legacy perk row.
foreach ($legacyId in $legacyIntegrationIds) {
    $pattern = '(?m)^[ \t]*\{ "' + [regex]::Escape($legacyId) + '",\s*branch_id::.*\r?\n'
    $rx = New-Object System.Text.RegularExpressions.Regex($pattern)
    $count = $rx.Matches($sp).Count
    if ($count -ne 1) { throw "v8.4 legacy perk-definition replacement expected one $legacyId, found $count" }
    $sp = $rx.Replace($sp,'',1)
}

$modPerksV8 = @'
    { "mg_arcane_focus", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Arcane Focus", "Магический фокус", "Magiclysm: +0.5 Spellcraft for spell checks.", "Magiclysm: +0,5 Spellcraft для проверок заклинаний.", {{ { "mg_spellcraft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_mana_sensitivity", branch_id::mastery, 2, 5, currency_id::perk, "mg_arcane_focus", "", "Mana Sensitivity", "Чувствительность к мане", "Magiclysm: +8% maximum mana.", "Magiclysm: +8% к максимуму маны.", {{ { "mg_mana_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_battlemage", branch_id::mastery, 2, 5, currency_id::perk, "mg_arcane_focus", "", "Invocation Drills", "Тренировка заклинаний", "Magiclysm: spell casting time -5%.", "Magiclysm: время сотворения заклинаний -5%.", {{ { "mg_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_wayfarer", branch_id::mastery, 2, 5, currency_id::perk, "mg_arcane_focus", "", "Arcane Reach", "Магическая дальность", "Magiclysm: spell range +5%.", "Magiclysm: дальность заклинаний +5%.", {{ { "mg_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_mana_regeneration", branch_id::mastery, 3, 9, currency_id::perk, "mg_mana_sensitivity", "", "Mana Regeneration", "Регенерация маны", "Magiclysm: mana regeneration +10%.", "Magiclysm: восстановление маны +10%.", {{ { "mg_mana_regen_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_stable_formula", branch_id::mastery, 3, 9, currency_id::perk, "mg_battlemage", "", "Stable Formula", "Стабильная формула", "Magiclysm: spell failure chance is reduced by 7% multiplicatively.", "Magiclysm: шанс провала заклинаний снижается на 7% мультипликативно.", {{ { "mg_fail_pct", -7 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_shaped_evocation", branch_id::mastery, 3, 9, currency_id::perk, "mg_wayfarer", "", "Shaped Evocation", "Формованная эвокация", "Magiclysm: spell power +6% to damage and healing.", "Magiclysm: сила заклинаний +6% к урону и лечению.", {{ { "mg_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_efficient_channels", branch_id::mastery, 4, 14, currency_id::perk, "mg_mana_regeneration", "", "Efficient Channels", "Эффективные каналы", "Magiclysm: spell mana cost -6%.", "Magiclysm: стоимость заклинаний в мане -6%.", {{ { "mg_spell_cost_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_spellcraft_drills", branch_id::mastery, 4, 14, currency_id::perk, "mg_stable_formula", "", "Spellcraft Drills", "Практика Spellcraft", "Magiclysm: +0.5 Spellcraft for spell checks.", "Magiclysm: +0,5 Spellcraft для проверок заклинаний.", {{ { "mg_spellcraft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_sustained_weave", branch_id::mastery, 4, 14, currency_id::perk, "mg_shaped_evocation", "", "Sustained Weave", "Удержание плетения", "Magiclysm: spell duration +8%.", "Magiclysm: длительность заклинаний +8%.", {{ { "mg_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mg_deep_reservoir", branch_id::mastery, 5, 20, currency_id::perk, "mg_efficient_channels", "", "Deep Reservoir", "Глубокий резерв", "Magiclysm: +12% maximum mana and +8% mana regeneration.", "Magiclysm: +12% максимум маны и +8% восстановления маны.", {{ { "mg_mana_max_pct", 12 }, { "mg_mana_regen_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_quick_invocation", branch_id::mastery, 5, 20, currency_id::perk, "mg_spellcraft_drills", "", "Quick Invocation", "Быстрое сотворение", "Magiclysm: casting time -8% and failure chance -5%.", "Magiclysm: время сотворения -8%, шанс провала -5%.", {{ { "mg_cast_time_pct", -8 }, { "mg_fail_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_arcane_geometry", branch_id::mastery, 5, 20, currency_id::perk, "mg_sustained_weave", "", "Arcane Geometry", "Магическая геометрия", "Magiclysm: area of effect +6% and range +5%.", "Magiclysm: площадь действия +6%, дальность +5%.", {{ { "mg_aoe_pct", 6 }, { "mg_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_mana_mastery", branch_id::mastery, 6, 25, currency_id::major, "mg_deep_reservoir", "", "Mana Mastery", "Мастерство маны", "Spell cost -10%, mana regeneration +15%.", "Стоимость заклинаний -10%, регенерация маны +15%.", {{ { "mg_spell_cost_pct", -10 }, { "mg_mana_regen_pct", 15 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_ritual_craft", branch_id::mastery, 6, 25, currency_id::major, "mg_quick_invocation", "", "Ritual Mastery", "Мастерство ритуалов", "+0.75 Spellcraft and +12% spell XP.", "+0,75 Spellcraft и +12% опыта заклинаний.", {{ { "mg_spellcraft_flat", 0.75 }, { "mg_spell_xp_pct", 12 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_high_thaumaturgy", branch_id::mastery, 6, 25, currency_id::major, "mg_arcane_geometry", "", "High Thaumaturgy", "Высшая тауматургия", "Spell potency +10%, duration +10%.", "Мощность +10%, длительность +10%.", {{ { "mg_spell_power_pct", 10 }, { "mg_duration_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_efficient_theory", branch_id::mastery, 7, 30, currency_id::perk, "mg_mana_mastery", "mg_ritual_craft", "Efficient Theory", "Эффективная теория", "Spell cost -5%, spell XP +8%.", "Стоимость -5%, опыт заклинаний +8%.", {{ { "mg_spell_cost_pct", -5 }, { "mg_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_combat_weave", branch_id::mastery, 7, 30, currency_id::perk, "mg_ritual_craft", "mg_high_thaumaturgy", "Combat Weave", "Боевое плетение", "Casting time -5%, spell potency +8%.", "Время сотворения -5%, мощность +8%.", {{ { "mg_cast_time_pct", -5 }, { "mg_spell_power_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_resonant_reserve", branch_id::mastery, 7, 30, currency_id::perk, "mg_mana_mastery", "mg_high_thaumaturgy", "Resonant Reserve", "Резонансный резерв", "Maximum mana +10%, spell duration +8%.", "Максимум маны +10%, длительность +8%.", {{ { "mg_mana_max_pct", 10 }, { "mg_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mg_archmage", branch_id::mastery, 8, 40, currency_id::major, "mg_efficient_theory", "mg_combat_weave", "Archmage", "Архимаг", "+0.75 Spellcraft, -5% failure, +8% potency, +8% spell XP.", "+0,75 Spellcraft, -5% провала, +8% мощности, +8% опыта заклинаний.", {{ { "mg_spellcraft_flat", 0.75 }, { "mg_fail_pct", -5 }, { "mg_spell_power_pct", 8 }, { "mg_spell_xp_pct", 8 } }}, 4, 0, perk_kind::effect },

    { "mom_mental_focus", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Psionic Focus", "Псионический фокус", "Mind Over Matter powers: +0.5 effective Metaphysics while channeling.", "Силы Mind Over Matter: +0,5 к эффективной Metaphysics при ченнелинге.", {{ { "mom_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_still_mind", branch_id::mastery, 2, 5, currency_id::perk, "mom_mental_focus", "", "Still Mind", "Спокойный разум", "Mind Over Matter: power failure chance -6%.", "Mind Over Matter: шанс провала псионических сил -6%.", {{ { "mom_fail_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_neural_reserve", branch_id::mastery, 2, 5, currency_id::perk, "mom_mental_focus", "", "Neural Reserve", "Нейронный резерв", "Mind Over Matter: psionic stamina cost -5%.", "Mind Over Matter: затраты выносливости на псионику -5%.", {{ { "mom_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_kinetic_control", branch_id::mastery, 2, 5, currency_id::perk, "mom_mental_focus", "", "Kinetic Control", "Кинетический контроль", "Mind Over Matter: power range +5%.", "Mind Over Matter: дальность псионических сил +5%.", {{ { "mom_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_channel_discipline", branch_id::mastery, 3, 9, currency_id::perk, "mom_still_mind", "", "Channel Discipline", "Дисциплина канала", "Mind Over Matter: activation/casting time -5%.", "Mind Over Matter: время активации/применения -5%.", {{ { "mom_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_efficient_channel", branch_id::mastery, 3, 9, currency_id::perk, "mom_neural_reserve", "", "Efficient Channel", "Эффективный канал", "Mind Over Matter: psionic stamina cost -6%.", "Mind Over Matter: затраты выносливости на псионику -6%.", {{ { "mom_spell_cost_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_psionic_pressure", branch_id::mastery, 3, 9, currency_id::perk, "mom_kinetic_control", "", "Psionic Pressure", "Псионическое давление", "Mind Over Matter: psionic power potency +6%.", "Mind Over Matter: мощность псионических сил +6%.", {{ { "mom_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_metaphysical_method", branch_id::mastery, 4, 14, currency_id::perk, "mom_channel_discipline", "", "Metaphysical Method", "Метод метафизики", "Mind Over Matter powers: +0.5 effective Metaphysics while channeling.", "Силы Mind Over Matter: +0,5 к эффективной Metaphysics при ченнелинге.", {{ { "mom_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_controlled_exposure", branch_id::mastery, 4, 14, currency_id::perk, "mom_efficient_channel", "", "Controlled Exposure", "Контролируемое воздействие", "Mind Over Matter: power-use XP +8%. Nether Attunement itself is not rewritten.", "Mind Over Matter: опыт за применение сил +8%. Сам Nether Attunement не переписывается.", {{ { "mom_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_extended_pattern", branch_id::mastery, 4, 14, currency_id::perk, "mom_psionic_pressure", "", "Extended Pattern", "Продлённый паттерн", "Mind Over Matter: power duration +8%.", "Mind Over Matter: длительность псионических сил +8%.", {{ { "mom_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mom_mental_lattice", branch_id::mastery, 5, 20, currency_id::perk, "mom_metaphysical_method", "", "Mental Lattice", "Ментальная решётка", "Mind Over Matter: failure chance -7%, activation time -5%.", "Mind Over Matter: шанс провала -7%, время активации -5%.", {{ { "mom_fail_pct", -7 }, { "mom_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_recovery_cycle", branch_id::mastery, 5, 20, currency_id::perk, "mom_controlled_exposure", "", "Recovery Cycle", "Цикл восстановления", "Mind Over Matter: stamina cost -7%, power-use XP +8%.", "Mind Over Matter: стоимость по выносливости -7%, опыт сил +8%.", {{ { "mom_spell_cost_pct", -7 }, { "mom_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_field_shaping", branch_id::mastery, 5, 20, currency_id::perk, "mom_extended_pattern", "", "Field Shaping", "Формирование поля", "Mind Over Matter: area of effect +6%, range +5%.", "Mind Over Matter: площадь действия +6%, дальность +5%.", {{ { "mom_aoe_pct", 6 }, { "mom_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_combat_focus", branch_id::mastery, 6, 25, currency_id::major, "mom_mental_lattice", "", "Noetic Control", "Ноэтический контроль", "+0.75 effective Metaphysics while channeling, failure chance -8%.", "+0,75 эффективной Metaphysics при ченнелинге, шанс провала -8%.", {{ { "mom_metaphysics_flat", 0.75 }, { "mom_fail_pct", -8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_nether_discipline", branch_id::mastery, 6, 25, currency_id::major, "mom_recovery_cycle", "", "Nether Discipline", "Дисциплина Низины", "Stamina cost -10%, power-use XP +12%.", "Стоимость по выносливости -10%, опыт сил +12%.", {{ { "mom_spell_cost_pct", -10 }, { "mom_spell_xp_pct", 12 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_noetic_projection", branch_id::mastery, 6, 25, currency_id::major, "mom_field_shaping", "", "Noetic Projection", "Ноэтическая проекция", "Potency +10%, duration +10%.", "Мощность +10%, длительность +10%.", {{ { "mom_spell_power_pct", 10 }, { "mom_duration_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_stable_channel", branch_id::mastery, 7, 30, currency_id::perk, "mom_combat_focus", "mom_nether_discipline", "Stable Channel", "Стабильный канал", "Failure chance -5%, stamina cost -5%.", "Шанс провала -5%, стоимость по выносливости -5%.", {{ { "mom_fail_pct", -5 }, { "mom_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_precise_manifestation", branch_id::mastery, 7, 30, currency_id::perk, "mom_combat_focus", "mom_noetic_projection", "Precise Manifestation", "Точная манифестация", "Activation time -5%, range +6%.", "Время активации -5%, дальность +6%.", {{ { "mom_cast_time_pct", -5 }, { "mom_range_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_efficient_force", branch_id::mastery, 7, 30, currency_id::perk, "mom_nether_discipline", "mom_noetic_projection", "Efficient Force", "Эффективная сила", "Stamina cost -5%, potency +8%.", "Стоимость по выносливости -5%, мощность +8%.", {{ { "mom_spell_cost_pct", -5 }, { "mom_spell_power_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mom_transcendent_focus", branch_id::mastery, 8, 40, currency_id::major, "mom_stable_channel", "mom_efficient_force", "Transcendent Focus", "Трансцендентный фокус", "+0.75 effective Metaphysics while channeling, failure -5%, potency +8%, power XP +8%.", "+0,75 эффективной Metaphysics при ченнелинге, шанс провала -5%, мощность +8%, опыт сил +8%.", {{ { "mom_metaphysics_flat", 0.75 }, { "mom_fail_pct", -5 }, { "mom_spell_power_pct", 8 }, { "mom_spell_xp_pct", 8 } }}, 4, 0, perk_kind::effect },

    { "xe_anomaly_method", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Anomaly Method", "Метод аномалий", "Xedra Evolved: +0.5 effective Deduction.", "Xedra Evolved: +0,5 к эффективной Deduction.", {{ { "xe_deduction_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_field_agent", branch_id::mastery, 2, 5, currency_id::perk, "xe_anomaly_method", "", "XEDRA Field Analysis", "Полевой анализ XEDRA", "Xedra Evolved: +0.5 effective Deduction.", "Xedra Evolved: +0,5 к эффективной Deduction.", {{ { "xe_deduction_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dross_resonance", branch_id::mastery, 2, 5, currency_id::perk, "xe_anomaly_method", "", "Dreamdross Resonance", "Резонанс дримдросса", "Xedra Evolved: +8% maximum mana.", "Xedra Evolved: +8% к максимуму маны.", {{ { "xe_mana_max_pct", 8 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dimensional_hunter", branch_id::mastery, 2, 5, currency_id::perk, "xe_anomaly_method", "", "Liminal Reach", "Пограничная дальность", "Xedra Evolved: spell/power range +5%.", "Xedra Evolved: дальность заклинаний/сил +5%.", {{ { "xe_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_gramarye_studies", branch_id::mastery, 3, 9, currency_id::perk, "xe_field_agent", "", "Gramarye Studies", "Изучение Gramarye", "Xedra Evolved: +0.5 effective Gramarye for fae magicks.", "Xedra Evolved: +0,5 к эффективной Gramarye для магии фей.", {{ { "xe_gramarye_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dream_metabolism", branch_id::mastery, 3, 9, currency_id::perk, "xe_dross_resonance", "", "Dream Metabolism", "Метаболизм сновидений", "Xedra Evolved: mana regeneration +10%.", "Xedra Evolved: восстановление маны +10%.", {{ { "xe_mana_regen_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_fast_manifestation", branch_id::mastery, 3, 9, currency_id::perk, "xe_dimensional_hunter", "", "Fast Manifestation", "Быстрая манифестация", "Xedra Evolved: casting/activation time -5%.", "Xedra Evolved: время сотворения/активации -5%.", {{ { "xe_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_dimensional_model", branch_id::mastery, 4, 14, currency_id::perk, "xe_gramarye_studies", "", "Dimensional Model", "Модель измерений", "Xedra Evolved: spell failure chance -6%.", "Xedra Evolved: шанс провала заклинаний -6%.", {{ { "xe_fail_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_efficient_oneiromancy", branch_id::mastery, 4, 14, currency_id::perk, "xe_dream_metabolism", "", "Efficient Oneiromancy", "Эффективная онейромантия", "Xedra Evolved: mana cost -6% for Xedra spells.", "Xedra Evolved: стоимость маны заклинаний Xedra -6%.", {{ { "xe_spell_cost_pct", -6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_oneiric_force", branch_id::mastery, 4, 14, currency_id::perk, "xe_fast_manifestation", "", "Oneiric Force", "Онейрическая сила", "Xedra Evolved: spell/power potency +6%.", "Xedra Evolved: мощность заклинаний/сил +6%.", {{ { "xe_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "xe_pattern_archive", branch_id::mastery, 5, 20, currency_id::perk, "xe_dimensional_model", "", "Anomaly Archive", "Архив аномалий", "Xedra Evolved: +0.5 Deduction and +8% spell XP.", "Xedra Evolved: +0,5 Deduction и +8% опыта заклинаний.", {{ { "xe_deduction_flat", 0.5 }, { "xe_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_dream_reservoir", branch_id::mastery, 5, 20, currency_id::perk, "xe_efficient_oneiromancy", "", "Dream Reservoir", "Резерв сновидений", "Xedra Evolved: +12% maximum mana and +8% mana regeneration.", "Xedra Evolved: +12% максимум маны и +8% восстановления маны.", {{ { "xe_mana_max_pct", 12 }, { "xe_mana_regen_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_liminal_persistence", branch_id::mastery, 5, 20, currency_id::perk, "xe_oneiric_force", "", "Liminal Persistence", "Пограничная устойчивость", "Xedra Evolved: duration +8%, area of effect +6%.", "Xedra Evolved: длительность +8%, площадь действия +6%.", {{ { "xe_duration_pct", 8 }, { "xe_aoe_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_occult_engineer", branch_id::mastery, 6, 25, currency_id::major, "xe_pattern_archive", "", "Occult Engineer", "Оккультный инженер", "+0.75 Deduction and +0.75 Gramarye.", "+0,75 Deduction и +0,75 Gramarye.", {{ { "xe_deduction_flat", 0.75 }, { "xe_gramarye_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_dream_economy", branch_id::mastery, 6, 25, currency_id::major, "xe_dream_reservoir", "", "Dream Economy", "Экономия сновидений", "Mana cost -10%, mana regeneration +15%.", "Стоимость маны -10%, регенерация +15%.", {{ { "xe_spell_cost_pct", -10 }, { "xe_mana_regen_pct", 15 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_reality_shaper", branch_id::mastery, 6, 25, currency_id::major, "xe_liminal_persistence", "", "Reality Shaper", "Формирователь реальности", "Potency +10%, range +8%.", "Мощность +10%, дальность +8%.", {{ { "xe_spell_power_pct", 10 }, { "xe_range_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_dream_theorist", branch_id::mastery, 7, 30, currency_id::perk, "xe_occult_engineer", "xe_dream_economy", "Dream Theorist", "Теоретик сновидений", "Spell XP +8%, mana cost -5%.", "Опыт заклинаний +8%, стоимость маны -5%.", {{ { "xe_spell_xp_pct", 8 }, { "xe_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_liminal_engineer", branch_id::mastery, 7, 30, currency_id::perk, "xe_occult_engineer", "xe_reality_shaper", "Liminal Engineer", "Пограничный инженер", "Failure chance -5%, cast time -5%.", "Шанс провала -5%, время сотворения -5%.", {{ { "xe_fail_pct", -5 }, { "xe_cast_time_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_oneiric_architect", branch_id::mastery, 7, 30, currency_id::perk, "xe_dream_economy", "xe_reality_shaper", "Oneiric Architect", "Онейрический архитектор", "Potency +8%, duration +8%.", "Мощность +8%, длительность +8%.", {{ { "xe_spell_power_pct", 8 }, { "xe_duration_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "xe_boundary_master", branch_id::mastery, 8, 40, currency_id::major, "xe_dream_theorist", "xe_oneiric_architect", "Boundary Master", "Мастер границы", "+0.75 Deduction, +0.5 Gramarye, failure -5%, potency +8%.", "+0,75 Deduction, +0,5 Gramarye, шанс провала -5%, мощность +8%.", {{ { "xe_deduction_flat", 0.75 }, { "xe_gramarye_flat", 0.5 }, { "xe_fail_pct", -5 }, { "xe_spell_power_pct", 8 } }}, 4, 0, perk_kind::effect },

    { "af_systems_operator", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Systems Operator", "Оператор систем", "Aftershock Exoplanet: +0.25 effective Smartgun and +0.25 effective Metaphysics while channeling esper powers.", "Aftershock Exoplanet: +0,25 к Smartgun и +0,25 к эффективной Metaphysics при ченнелинге эспер-сил.", {{ { "af_smartgun_flat", 0.25 }, { "af_metaphysics_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_targeting_link", branch_id::mastery, 2, 5, currency_id::perk, "af_systems_operator", "", "Targeting Link", "Связь с прицелом", "Aftershock Exoplanet: +0.25 effective Smartgun.", "Aftershock Exoplanet: +0,25 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_conditioning", branch_id::mastery, 2, 5, currency_id::perk, "af_systems_operator", "", "Psi Endurance", "Пси-выносливость", "Aftershock Exoplanet esper powers: stamina cost -5%.", "Псионика Aftershock Exoplanet: затраты выносливости -5%.", {{ { "af_spell_cost_pct", -5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_expedition_logistics", branch_id::mastery, 2, 5, currency_id::perk, "af_systems_operator", "", "Vector Projection", "Векторная проекция", "Aftershock Exoplanet esper powers: range +5%.", "Псионика Aftershock Exoplanet: дальность +5%.", {{ { "af_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_predictive_fire", branch_id::mastery, 3, 9, currency_id::perk, "af_targeting_link", "", "Predictive Fire", "Предиктивный огонь", "Aftershock Exoplanet: +0.25 effective Smartgun.", "Aftershock Exoplanet: +0,25 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_metaphysical_training", branch_id::mastery, 3, 9, currency_id::perk, "af_conditioning", "", "Metaphysical Training", "Тренировка метафизики", "Aftershock Exoplanet esper powers: +0.5 effective Metaphysics while channeling.", "Эспер-силы Aftershock Exoplanet: +0,5 к эффективной Metaphysics при ченнелинге.", {{ { "af_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_telekinetic_geometry", branch_id::mastery, 3, 9, currency_id::perk, "af_expedition_logistics", "", "Esper Geometry", "Геометрия эспера", "Aftershock Exoplanet esper powers: area of effect +6%.", "Псионика Aftershock Exoplanet: площадь действия +6%.", {{ { "af_aoe_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_sensor_fusion", branch_id::mastery, 4, 14, currency_id::perk, "af_predictive_fire", "", "Sensor Fusion", "Слияние сенсоров", "Aftershock Exoplanet: +0.25 effective Smartgun.", "Aftershock Exoplanet: +0,25 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_stable_esper", branch_id::mastery, 4, 14, currency_id::perk, "af_metaphysical_training", "", "Stable Esper", "Стабильный эспер", "Aftershock Exoplanet esper powers: failure chance -7%.", "Псионика Aftershock Exoplanet: шанс провала -7%.", {{ { "af_fail_pct", -7 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_esper_force", branch_id::mastery, 4, 14, currency_id::perk, "af_telekinetic_geometry", "", "Esper Force", "Сила эспера", "Aftershock Exoplanet esper powers: potency +6%.", "Псионика Aftershock Exoplanet: мощность +6%.", {{ { "af_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_combat_technician", branch_id::mastery, 5, 20, currency_id::perk, "af_sensor_fusion", "", "Combat Technician", "Боевой техник", "Aftershock Exoplanet: +0.5 effective Smartgun.", "Aftershock Exoplanet: +0,5 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_efficient_esper", branch_id::mastery, 5, 20, currency_id::perk, "af_stable_esper", "", "Efficient Esper", "Эффективный эспер", "Aftershock Exoplanet esper powers: stamina cost -7%, power XP +8%.", "Псионика Aftershock Exoplanet: стоимость по выносливости -7%, опыт сил +8%.", {{ { "af_spell_cost_pct", -7 }, { "af_spell_xp_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_sustained_phenomena", branch_id::mastery, 5, 20, currency_id::perk, "af_esper_force", "", "Sustained Phenomena", "Устойчивые феномены", "Aftershock Exoplanet esper powers: duration +8%, range +5%.", "Псионика Aftershock Exoplanet: длительность +8%, дальность +5%.", {{ { "af_duration_pct", 8 }, { "af_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_smartgun_mastery", branch_id::mastery, 6, 25, currency_id::major, "af_combat_technician", "", "Smartgun Mastery", "Мастерство Smartgun", "+0.75 effective Smartgun.", "+0,75 к эффективному Smartgun.", {{ { "af_smartgun_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "af_esper_mastery", branch_id::mastery, 6, 25, currency_id::major, "af_efficient_esper", "", "Esper Mastery", "Мастерство эспера", "+0.75 effective Metaphysics while channeling, failure chance -10%.", "+0,75 эффективной Metaphysics при ченнелинге, шанс провала -10%.", {{ { "af_metaphysics_flat", 0.75 }, { "af_fail_pct", -10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_noetic_artillery", branch_id::mastery, 6, 25, currency_id::major, "af_sustained_phenomena", "", "Noetic Artillery", "Ноэтическая артиллерия", "Esper potency +10%, range +8%.", "Мощность эспера +10%, дальность +8%.", {{ { "af_spell_power_pct", 10 }, { "af_range_pct", 8 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_neural_targeting", branch_id::mastery, 7, 30, currency_id::perk, "af_smartgun_mastery", "af_esper_mastery", "Neural Targeting", "Нейронное наведение", "+0.25 Smartgun and +0.5 effective Metaphysics while channeling.", "+0,25 Smartgun и +0,5 эффективной Metaphysics при ченнелинге.", {{ { "af_smartgun_flat", 0.25 }, { "af_metaphysics_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_psionic_firecontrol", branch_id::mastery, 7, 30, currency_id::perk, "af_smartgun_mastery", "af_noetic_artillery", "Psionic Fire Control", "Псионическое управление огнём", "+0.25 Smartgun, esper potency +6%.", "+0,25 Smartgun, мощность эспера +6%.", {{ { "af_smartgun_flat", 0.25 }, { "af_spell_power_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_stable_projection", branch_id::mastery, 7, 30, currency_id::perk, "af_esper_mastery", "af_noetic_artillery", "Stable Projection", "Стабильная проекция", "Stamina cost -5%, failure chance -5%.", "Стоимость по выносливости -5%, шанс провала -5%.", {{ { "af_spell_cost_pct", -5 }, { "af_fail_pct", -5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "af_posthuman_operator", branch_id::mastery, 8, 40, currency_id::major, "af_neural_targeting", "af_stable_projection", "Posthuman Operator", "Постчеловеческий оператор", "+0.5 Smartgun, +0.75 effective Metaphysics while channeling, stamina cost -5%, esper potency +8%.", "+0,5 Smartgun, +0,75 эффективной Metaphysics при ченнелинге, стоимость выносливости -5%, мощность эспера +8%.", {{ { "af_smartgun_flat", 0.5 }, { "af_metaphysics_flat", 0.75 }, { "af_spell_cost_pct", -5 }, { "af_spell_power_pct", 8 } }}, 4, 0, perk_kind::effect },
    { "afp_prime_operator", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Prime Operator", "Оператор Prime", "Aftershock Prime: +0.25 Smartgun for related checks and +3% XP for Prime abilities.", "Aftershock Prime: +0,25 Smartgun для связанных проверок и +3% опыта способностей Prime.", {{ { "afp_smartgun_flat", 0.25 }, { "afp_spell_xp_pct", 3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_smartgun_interface", branch_id::mastery, 2, 5, currency_id::perk, "afp_prime_operator", "", "Smartgun Interface", "Интерфейс Smartgun", "Prime smart weapons: +0.25 effective Smartgun.", "Умное оружие Prime: +0,25 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_systems_theory", branch_id::mastery, 2, 5, currency_id::perk, "afp_prime_operator", "", "Systems Theory", "Теория систем", "Prime abilities: energy cost -4%, XP +4%.", "Способности Prime: стоимость энергии -4%, опыт +4%.", {{ { "afp_spell_cost_pct", -4 }, { "afp_spell_xp_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_translocation_calculus", branch_id::mastery, 2, 5, currency_id::perk, "afp_prime_operator", "", "Translocation Calculus", "Расчёт транслокации", "Prime spatial abilities: range +5%, duration +4%.", "Пространственные способности Prime: дальность +5%, длительность +4%.", {{ { "afp_range_pct", 5 }, { "afp_duration_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_predictive_targeting", branch_id::mastery, 3, 9, currency_id::perk, "afp_smartgun_interface", "", "Predictive Targeting", "Предиктивное наведение", "Prime smart weapons: +0.25 effective Smartgun.", "Умное оружие Prime: +0,25 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_power_budget", branch_id::mastery, 3, 9, currency_id::perk, "afp_systems_theory", "", "Power Budget", "Энергобюджет", "Prime abilities: energy cost -4%, activation time -3%.", "Способности Prime: стоимость энергии -4%, время активации -3%.", {{ { "afp_spell_cost_pct", -4 }, { "afp_cast_time_pct", -3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_spatial_solution", branch_id::mastery, 3, 9, currency_id::perk, "afp_translocation_calculus", "", "Spatial Solution", "Пространственное решение", "Prime spatial abilities: range +5%, area +5%.", "Пространственные способности Prime: дальность +5%, площадь +5%.", {{ { "afp_range_pct", 5 }, { "afp_aoe_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_sensor_fusion", branch_id::mastery, 4, 14, currency_id::perk, "afp_predictive_targeting", "", "Sensor Fusion", "Слияние сенсоров", "Prime smart weapons: +0.25 effective Smartgun.", "Умное оружие Prime: +0,25 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_utility_protocols", branch_id::mastery, 4, 14, currency_id::perk, "afp_power_budget", "", "Utility Suite", "Набор утилит", "Prime abilities: XP +6%, failure chance -4%.", "Способности Prime: опыт +6%, шанс провала -4%.", {{ { "afp_spell_xp_pct", 6 }, { "afp_fail_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_stable_translation", branch_id::mastery, 4, 14, currency_id::perk, "afp_spatial_solution", "", "Stable Translocation", "Стабильная транслокация", "Prime spatial abilities: duration +6%, failure chance -4%.", "Пространственные способности Prime: длительность +6%, шанс провала -4%.", {{ { "afp_duration_pct", 6 }, { "afp_fail_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_combat_technician", branch_id::mastery, 5, 20, currency_id::perk, "afp_sensor_fusion", "", "Prime Combat Technician", "Боевой техник Prime", "Prime smart weapons: +0.5 effective Smartgun.", "Умное оружие Prime: +0,5 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_systems_automation", branch_id::mastery, 5, 20, currency_id::perk, "afp_utility_protocols", "", "Systems Automation", "Автоматизация систем", "Prime abilities: activation time -5%, XP +7%.", "Способности Prime: время активации -5%, опыт +7%.", {{ { "afp_cast_time_pct", -5 }, { "afp_spell_xp_pct", 7 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_field_projection", branch_id::mastery, 5, 20, currency_id::perk, "afp_stable_translation", "", "Field Projection", "Полевая проекция", "Prime abilities: potency +6%, range +5%.", "Способности Prime: мощность +6%, дальность +5%.", {{ { "afp_spell_power_pct", 6 }, { "afp_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_smartgun_mastery", branch_id::mastery, 6, 25, currency_id::major, "afp_combat_technician", "", "Prime Smartgun Mastery", "Мастерство Smartgun Prime", "+0.75 effective Smartgun.", "+0,75 к эффективному Smartgun.", {{ { "afp_smartgun_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "afp_prime_systems_mastery", branch_id::mastery, 6, 25, currency_id::major, "afp_systems_automation", "", "Prime Systems Mastery", "Мастерство систем Prime", "Energy cost -7%, activation time -6%, XP +8%.", "Стоимость энергии -7%, время активации -6%, опыт +8%.", {{ { "afp_spell_cost_pct", -7 }, { "afp_cast_time_pct", -6 }, { "afp_spell_xp_pct", 8 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "afp_translocation_mastery", branch_id::mastery, 6, 25, currency_id::major, "afp_field_projection", "", "Translocation Mastery", "Мастерство транслокации", "Potency +8%, range +8%, duration +8%.", "Мощность +8%, дальность +8%, длительность +8%.", {{ { "afp_spell_power_pct", 8 }, { "afp_range_pct", 8 }, { "afp_duration_pct", 8 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "afp_integrated_firecontrol", branch_id::mastery, 7, 30, currency_id::perk, "afp_smartgun_mastery", "afp_prime_systems_mastery", "Integrated Fire Control", "Интегрированное управление огнём", "+0.25 Smartgun, activation time -4%.", "+0,25 Smartgun, время активации -4%.", {{ { "afp_smartgun_flat", 0.25 }, { "afp_cast_time_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_remote_geometry", branch_id::mastery, 7, 30, currency_id::perk, "afp_prime_systems_mastery", "afp_translocation_mastery", "Remote Geometry", "Удалённая геометрия", "Area +5%, duration +5%.", "Площадь +5%, длительность +5%.", {{ { "afp_aoe_pct", 5 }, { "afp_duration_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_mobile_platform", branch_id::mastery, 7, 30, currency_id::perk, "afp_smartgun_mastery", "afp_translocation_mastery", "Mobile Platform", "Мобильная платформа", "+0.25 Smartgun, range +5%.", "+0,25 Smartgun, дальность +5%.", {{ { "afp_smartgun_flat", 0.25 }, { "afp_range_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "afp_prime_integrator", branch_id::mastery, 8, 40, currency_id::major, "afp_integrated_firecontrol", "afp_remote_geometry", "Prime Integrator", "Интегратор Prime", "+0.5 Smartgun, energy cost -5%, potency +6%, duration +6%.", "+0,5 Smartgun, стоимость энергии -5%, мощность +6%, длительность +6%.", {{ { "afp_smartgun_flat", 0.5 }, { "afp_spell_cost_pct", -5 }, { "afp_spell_power_pct", 6 }, { "afp_duration_pct", 6 } }}, 4, 0, perk_kind::effect },

    { "sec_field_researcher", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Secronom Field Researcher", "Полевой исследователь Secronom", "Against Secronom creatures: damage +2%, incoming damage -2%.", "Против существ Secronom: урон +2%, входящий урон -2%.", {{ { "sec_damage_pct", 2 }, { "sec_resist_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_hunter_drills", branch_id::mastery, 2, 5, currency_id::perk, "sec_field_researcher", "", "Hunter Drills", "Тренировки охотника", "Against Secronom creatures: damage +3%.", "Против существ Secronom: урон +3%.", {{ { "sec_damage_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_pathogen_hardening", branch_id::mastery, 2, 5, currency_id::perk, "sec_field_researcher", "", "Pathogen Hardening", "Закалка против патогенов", "Against Secronom creatures: incoming damage -3%.", "Против существ Secronom: входящий урон -3%.", {{ { "sec_resist_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_crimson_anatomy", branch_id::mastery, 2, 5, currency_id::perk, "sec_field_researcher", "", "Crimson Anatomy", "Анатомия Crimson", "Against Crimson Horror species: extra damage +3%.", "Против видов Crimson Horror: дополнительный урон +3%.", {{ { "sec_crimson_damage_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_vital_targets", branch_id::mastery, 3, 9, currency_id::perk, "sec_hunter_drills", "", "Vital Targets", "Уязвимые точки", "Against Secronom creatures: damage +4%.", "Против существ Secronom: урон +4%.", {{ { "sec_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_toxicology", branch_id::mastery, 3, 9, currency_id::perk, "sec_pathogen_hardening", "", "Toxicology", "Токсикология", "Against Secronom creatures: incoming damage -4%.", "Против существ Secronom: входящий урон -4%.", {{ { "sec_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_flesh_patterning", branch_id::mastery, 3, 9, currency_id::perk, "sec_crimson_anatomy", "", "Flesh Patterning", "Структура плоти", "Against Crimson Horror species: extra damage +4%.", "Против видов Crimson Horror: дополнительный урон +4%.", {{ { "sec_crimson_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_elite_tracking", branch_id::mastery, 4, 14, currency_id::perk, "sec_vital_targets", "", "Elite Tracking", "Выслеживание элиты", "Against elite/catastrophic Secronom species: extra damage +4%.", "Против элитных/катастрофических видов Secronom: дополнительный урон +4%.", {{ { "sec_elite_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_adaptive_response", branch_id::mastery, 4, 14, currency_id::perk, "sec_toxicology", "", "Adaptive Response", "Адаптивная реакция", "Against elite/catastrophic Secronom species: incoming damage -4% extra.", "Против элитных/катастрофических видов Secronom: дополнительное снижение урона -4%.", {{ { "sec_elite_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_crimson_countermeasures", branch_id::mastery, 4, 14, currency_id::perk, "sec_flesh_patterning", "", "Crimson Countermeasures", "Контрмеры Crimson", "Against Crimson Horror species: incoming damage -4% extra.", "Против видов Crimson Horror: дополнительное снижение урона -4%.", {{ { "sec_crimson_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_execution_protocol", branch_id::mastery, 5, 20, currency_id::perk, "sec_elite_tracking", "", "Elite Hunter", "Охотник на элиту", "Against elite/catastrophic Secronom species: extra damage +5%.", "Против элитных/катастрофических видов Secronom: дополнительный урон +5%.", {{ { "sec_elite_damage_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_hardened_survivor", branch_id::mastery, 5, 20, currency_id::perk, "sec_adaptive_response", "", "Hardened Survivor", "Закалённый выживший", "Against elite/catastrophic Secronom species: incoming damage -5% extra.", "Против элитных/катастрофических видов Secronom: дополнительное снижение урона -5%.", {{ { "sec_elite_resist_pct", 5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sec_fleshbreaker", branch_id::mastery, 5, 20, currency_id::perk, "sec_crimson_countermeasures", "", "Fleshbreaker", "Разрушитель плоти", "Against Crimson Horror species: extra damage +5%, incoming damage -3% extra.", "Против видов Crimson Horror: дополнительный урон +5%, дополнительное снижение урона -3%.", {{ { "sec_crimson_damage_pct", 5 }, { "sec_crimson_resist_pct", 3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_hunter_mastery", branch_id::mastery, 6, 25, currency_id::major, "sec_execution_protocol", "", "Veteran Secronom Hunter", "Опытный охотник Secronom", "Damage +6% to all Secronom and +4% extra to elites.", "Урон +6% по всем Secronom и ещё +4% по элите.", {{ { "sec_damage_pct", 6 }, { "sec_elite_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_survival_mastery", branch_id::mastery, 6, 25, currency_id::major, "sec_hardened_survivor", "", "Secronom Survivor", "Выживший против Secronom", "Incoming damage -6% from all Secronom and -4% extra from elites.", "Входящий урон -6% от всех Secronom и ещё -4% от элиты.", {{ { "sec_resist_pct", 6 }, { "sec_elite_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_crimson_mastery", branch_id::mastery, 6, 25, currency_id::major, "sec_fleshbreaker", "", "Crimson Veteran", "Ветеран Crimson Horror", "Extra damage +7%, incoming damage -5% extra.", "Дополнительный урон +7%, дополнительное снижение урона -5%.", {{ { "sec_crimson_damage_pct", 7 }, { "sec_crimson_resist_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_apex_hunter", branch_id::mastery, 7, 30, currency_id::perk, "sec_hunter_mastery", "sec_survival_mastery", "Nightmare Hunter", "Охотник на кошмары", "Damage +4%, incoming damage -4%.", "Урон +4%, входящий урон -4%.", {{ { "sec_damage_pct", 4 }, { "sec_resist_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_ultimate_protocol", branch_id::mastery, 7, 30, currency_id::perk, "sec_survival_mastery", "sec_crimson_mastery", "Crimson Doctrine", "Багровая доктрина", "Elite resistance +5%, Crimson damage +4%.", "Защита от элиты +5%, урон по Crimson +4%.", {{ { "sec_elite_resist_pct", 5 }, { "sec_crimson_damage_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_red_harvest", branch_id::mastery, 7, 30, currency_id::perk, "sec_hunter_mastery", "sec_crimson_mastery", "Red Harvest", "Красная жатва", "Elite damage +5%, Crimson damage +5%.", "Урон по элите +5%, урон по Crimson +5%.", {{ { "sec_elite_damage_pct", 5 }, { "sec_crimson_damage_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sec_nightmare_specialist", branch_id::mastery, 8, 40, currency_id::major, "sec_apex_hunter", "sec_red_harvest", "Nightmare Specialist", "Специалист по кошмарам", "Damage +5%, resistance +5%, elite damage +5%, Crimson damage +5%.", "Урон +5%, защита +5%, урон по элите +5%, урон по Crimson +5%.", {{ { "sec_damage_pct", 5 }, { "sec_resist_pct", 5 }, { "sec_elite_damage_pct", 5 }, { "sec_crimson_damage_pct", 5 } }}, 4, 0, perk_kind::effect },

    { "secx_flesh_initiate", branch_id::mastery, 1, 2, currency_id::perk, "", "", "Flesh Initiate", "Посвящённый плоти", "Secronom+: +0.25 effective Flesh Weaving and +0.25 Bio-organic Weapons.", "Secronom+: +0,25 к Flesh Weaving и +0,25 к Bio-organic Weapons.", {{ { "secx_flesh_craft_flat", 0.25 }, { "secx_flesh_combat_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_flesh_weaving", branch_id::mastery, 2, 5, currency_id::perk, "secx_flesh_initiate", "", "Flesh Weaving Practice", "Практика Flesh Weaving", "Secronom+: +0.5 effective Flesh Weaving.", "Secronom+: +0,5 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_biomorph_training", branch_id::mastery, 2, 5, currency_id::perk, "secx_flesh_initiate", "", "Biomorph Training", "Тренировка Biomorph", "Secronom+: +0.5 effective Bio-organic Weapons.", "Secronom+: +0,5 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_neural_link", branch_id::mastery, 2, 5, currency_id::perk, "secx_flesh_initiate", "", "Flesh Vessel Neural Link", "Нейросвязь Flesh Vessel", "Secronom+ abilities: energy cost -4%, XP +4%.", "Способности Secronom+: стоимость энергии -4%, опыт +4%.", {{ { "secx_spell_cost_pct", -4 }, { "secx_spell_xp_pct", 4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_resource_shaping", branch_id::mastery, 3, 9, currency_id::perk, "secx_flesh_weaving", "", "Resource Shaping", "Формирование ресурсов", "Secronom+: +0.5 effective Flesh Weaving.", "Secronom+: +0,5 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_armament_drills", branch_id::mastery, 3, 9, currency_id::perk, "secx_biomorph_training", "", "Armament Drills", "Тренировки вооружения", "Secronom+: +0.5 effective Bio-organic Weapons.", "Secronom+: +0,5 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_morph_control", branch_id::mastery, 3, 9, currency_id::perk, "secx_neural_link", "", "Morph Control", "Контроль морфинга", "Secronom+ abilities: activation time -4%, failure chance -4%.", "Способности Secronom+: время активации -4%, шанс провала -4%.", {{ { "secx_cast_time_pct", -4 }, { "secx_fail_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_advanced_weaving", branch_id::mastery, 4, 14, currency_id::perk, "secx_resource_shaping", "", "Advanced Flesh Weaving", "Продвинутое Flesh Weaving", "Secronom+: +0.5 effective Flesh Weaving.", "Secronom+: +0,5 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_combat_instinct", branch_id::mastery, 4, 14, currency_id::perk, "secx_armament_drills", "", "Bio-organic Combat Instinct", "Биоорганический боевой инстинкт", "Secronom+: +0.5 effective Bio-organic Weapons.", "Secronom+: +0,5 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_flesh_channel", branch_id::mastery, 4, 14, currency_id::perk, "secx_morph_control", "", "Flesh Channel", "Канал плоти", "Secronom+ abilities: duration +5%, potency +5%.", "Способности Secronom+: длительность +5%, мощность +5%.", {{ { "secx_duration_pct", 5 }, { "secx_spell_power_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_material_mastery", branch_id::mastery, 5, 20, currency_id::perk, "secx_advanced_weaving", "", "Living Material Mastery", "Мастерство живого материала", "Secronom+: +0.75 effective Flesh Weaving.", "Secronom+: +0,75 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_bioorganic_mastery", branch_id::mastery, 5, 20, currency_id::perk, "secx_combat_instinct", "", "Bio-organic Armament Mastery", "Мастерство биооружия", "Secronom+: +0.75 effective Bio-organic Weapons.", "Secronom+: +0,75 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 0.75 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_vessel_resonance", branch_id::mastery, 5, 20, currency_id::perk, "secx_flesh_channel", "", "Flesh Vessel Resonance", "Резонанс Flesh Vessel", "Secronom+ abilities: energy cost -5%, duration +6%.", "Способности Secronom+: стоимость энергии -5%, длительность +6%.", {{ { "secx_spell_cost_pct", -5 }, { "secx_duration_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_fleshcraft_mastery", branch_id::mastery, 6, 25, currency_id::major, "secx_material_mastery", "", "Fleshcraft Mastery", "Мастерство Fleshcraft", "+1.0 effective Flesh Weaving.", "+1,0 к эффективному Flesh Weaving.", {{ { "secx_flesh_craft_flat", 1.0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_biomorph_mastery", branch_id::mastery, 6, 25, currency_id::major, "secx_bioorganic_mastery", "", "Biomorph Mastery", "Мастерство Biomorph", "+1.0 effective Bio-organic Weapons.", "+1,0 к эффективному Bio-organic Weapons.", {{ { "secx_flesh_combat_flat", 1.0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "secx_flesh_vessel_mastery", branch_id::mastery, 6, 25, currency_id::major, "secx_vessel_resonance", "", "Flesh Vessel Mastery", "Мастерство Flesh Vessel", "Potency +8%, energy cost -6%, duration +8%.", "Мощность +8%, стоимость энергии -6%, длительность +8%.", {{ { "secx_spell_power_pct", 8 }, { "secx_spell_cost_pct", -6 }, { "secx_duration_pct", 8 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "secx_living_arsenal", branch_id::mastery, 7, 30, currency_id::perk, "secx_fleshcraft_mastery", "secx_biomorph_mastery", "Living Arsenal", "Живой арсенал", "+0.5 Flesh Weaving and +0.5 Bio-organic Weapons.", "+0,5 Flesh Weaving и +0,5 Bio-organic Weapons.", {{ { "secx_flesh_craft_flat", 0.5 }, { "secx_flesh_combat_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_adaptive_morph", branch_id::mastery, 7, 30, currency_id::perk, "secx_biomorph_mastery", "secx_flesh_vessel_mastery", "Adaptive Morph", "Адаптивный морф", "+0.5 Bio-organic Weapons, activation time -4%.", "+0,5 Bio-organic Weapons, время активации -4%.", {{ { "secx_flesh_combat_flat", 0.5 }, { "secx_cast_time_pct", -4 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_artificial_ecology", branch_id::mastery, 7, 30, currency_id::perk, "secx_fleshcraft_mastery", "secx_flesh_vessel_mastery", "Artificial Ecology", "Искусственная экология", "+0.5 Flesh Weaving, ability XP +6%.", "+0,5 Flesh Weaving, опыт способностей +6%.", {{ { "secx_flesh_craft_flat", 0.5 }, { "secx_spell_xp_pct", 6 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "secx_flesh_architect", branch_id::mastery, 8, 40, currency_id::major, "secx_living_arsenal", "secx_adaptive_morph", "Flesh Architect", "Архитектор плоти", "+0.75 Flesh Weaving, +0.75 Bio-organic Weapons, potency +6%, energy cost -5%.", "+0,75 Flesh Weaving, +0,75 Bio-organic Weapons, мощность +6%, стоимость энергии -5%.", {{ { "secx_flesh_craft_flat", 0.75 }, { "secx_flesh_combat_flat", 0.75 }, { "secx_spell_power_pct", 6 }, { "secx_spell_cost_pct", -5 } }}, 4, 0, perk_kind::effect },
'@
$perkArrayStartV8 = $sp.IndexOf('const perk_def perks[] = {')
if ($perkArrayStartV8 -lt 0) { throw 'v8 perk array start not found.' }
$perkArrayEndV8 = $sp.IndexOf("`n};",$perkArrayStartV8)
if ($perkArrayEndV8 -lt 0) { throw 'v8 perk array end not found.' }
$sp = $sp.Insert($perkArrayEndV8,"`n" + $modPerksV8.TrimEnd())

# v8.2: integration ownership is an exact typed registry generated from the
# canonical mod-perk block.  Stable save IDs remain strings, but runtime logic
# no longer treats arbitrary mg_/mom_/xe_/af_ prefixes as executable metadata.
$integrationEntriesV82 = New-Object System.Collections.Generic.List[string]
foreach ($lineV82 in (Normalize-Lf $modPerksV8).Split("`n")) {
    if ($lineV82 -match '^\s*\{\s*"((mg|mom|xe|af|afp|sec|secx)_[^"]+)"') {
        $perkIdV82 = $Matches[1]
        $prefixV82 = $Matches[2]
        $enumV82 = switch ($prefixV82) {
            'mg'  { 'magiclysm' }
            'mom' { 'mindovermatter' }
            'xe'  { 'xedra_evolved' }
            'af'  { 'aftershock_exoplanet' }
            'afp' { 'aftershock_prime' }
            'sec' { 'secronom' }
            'secx' { 'secronom_plus' }
            default { throw "v8.2 unknown integration prefix while building exact registry: $prefixV82" }
        }
        $integrationEntriesV82.Add('        { "' + $perkIdV82 + '", integration_id::' + $enumV82 + ' },')
    }
}
if ($integrationEntriesV82.Count -ne 140) {
    throw "v8.3 exact integration registry expected 140 IDs, found $($integrationEntriesV82.Count)"
}
$integrationRegistryV82 = @"
integration_id perk_integration( const perk_def &perk )
{
    static const std::map<std::string_view, integration_id> registry = {
$($integrationEntriesV82.ToArray() -join "`n")
    };
    const auto it = registry.find( perk.id ? std::string_view( perk.id ) : std::string_view() );
    return it == registry.end() ? integration_id::none : it->second;
}
"@
$sp = Replace-CppRange $sp 'integration_id perk_integration( const perk_def &perk )' 'bool integration_perk( const perk_def &perk )' $integrationRegistryV82 'v8.3 exact integration registry'

# v8.2: cache immutable perk state keys once instead of allocating "p_<id>" on every ownership query.
$perkKeyV82 = @'
const std::string &perk_key( const perk_def &perk )
{
    static const std::map<std::string_view, std::string> keys = []() {
        std::map<std::string_view, std::string> result;
        for( const perk_def &entry : perks ) {
            result.emplace( std::string_view( entry.id ), std::string( "p_" ) + entry.id );
        }
        return result;
    }();
    const auto it = keys.find( std::string_view( perk.id ) );
    if( it != keys.end() ) {
        return it->second;
    }
    static const std::string invalid_key = "p_invalid";
    return invalid_key;
}
'@
$sp = Replace-CppRange $sp 'std::string perk_key( const perk_def &perk )' 'struct ranked_perk_rule {' $perkKeyV82 'v8.5 cached perk state keys preserving ranked rule table and helpers'

$gateHelperV8 = @'
int64_t perk_progression_level( const perk_def &perk )
{
    if( integration_perk( perk ) ) {
        return std::max<int64_t>( 1, get_state( "level", 1 ) );
    }
    return branch_level( perk.branch );
}

'@
if (-not $sp.Contains('int64_t perk_progression_level( const perk_def &perk )')) {
    $sp = $sp.Replace('bool prerequisites_met( const perk_def &perk )',$gateHelperV8 + 'bool prerequisites_met( const perk_def &perk )')
}

$purchaseV8 = @'
bool purchase_perk( const perk_def &perk )
{
    const int64_t level = perk_progression_level( perk );
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( !perk_world_available( perk ) ) {
        message( tr( "The required mod is not active.", "Требуемый мод не активен." ) );
        return false;
    }
    if( rank >= max_rank ) {
        message( tr( "This perk is already at maximum rank.",
                     "Этот перк уже максимального ранга." ) );
        return false;
    }
    if( level < perk.required_level ) {
        message( integration_perk( perk ) ?
                 tr( "Your Survivor level is too low for this perk.",
                     "Недостаточный уровень Survivor для этого перка." ) :
                 tr( "Your branch level is too low.", "Недостаточный уровень этой ветки." ) );
        return false;
    }
    if( !prerequisites_met( perk ) ) {
        if( specialization_perk( perk ) && !specialization_allowed( perk ) ) {
            message( tr( "Another specialization is already committed in this branch. Respec to change it.",
                         "В этой ветке уже выбрана другая специализация. Для смены нужен сброс." ) );
        } else {
            message( tr( "Prerequisites are not met.", "Не выполнены требования предыдущих перков." ) );
        }
        return false;
    }

    int chosen_slot = 0;
    if( specialization_root( perk ) ) {
        chosen_slot = specialization_slot( perk );
        const int64_t existing = get_state( specialization_state_key( perk.branch ), 0 );
        if( existing == 0 ) {
            std::string prompt = tr(
                "Commit to this specialization? The other two paths in this branch will lock until a full respec.\n",
                "Выбрать эту специализацию? Два других пути этой ветки закроются до полного сброса.\n" );
            prompt += perk_display_name( perk );
            std::string yes = tr( "Commit", "Выбрать" );
            std::string no = tr( "Cancel", "Отмена" );
            const char *entries[] = { yes.c_str(), no.c_str() };
            const int choice = host->ui_choose ? host->ui_choose( prompt.c_str(), entries, 2 ) : -1;
            if( choice != 0 ) return false;
        }
    }

    if( perk.currency == currency_id::perk ) {
        if( perk_points <= 0 ) {
            message( tr( "Not enough perk points.", "Недостаточно очков перков." ) );
            return false;
        }
        set_state( "perk_points", perk_points - 1 );
    } else {
        if( major_points <= 0 ) {
            message( tr( "Not enough major points.", "Недостаточно больших очков." ) );
            return false;
        }
        set_state( "major_points", major_points - 1 );
    }

    if( chosen_slot > 0 ) set_state( specialization_state_key( perk.branch ), chosen_slot );
    set_state( perk_key( perk ), rank + 1 );
    effects_dirty = true;
    recalculate_effects();

    std::string text = rank == 0 ? tr( "Perk purchased: ", "Куплен перк: " ) :
                                   tr( "Perk upgraded: ", "Перк улучшен: " );
    text += russian() ? perk.name_ru : perk.name_en;
    if( max_rank > 1 ) text += " " + std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
    message( text );
    return true;
}
'@
$sp = Replace-CppRange $sp 'bool purchase_perk( const perk_def &perk )' 'void show_perk_detail( const perk_def &perk )' $purchaseV8 'v8 global-level mod gating'

$detailV8 = @'
void show_perk_detail( const perk_def &perk )
{
    while( true ) {
        const int level = static_cast<int>( perk_progression_level( perk ) );
        const int rank = perk_rank( perk );
        const int max_rank = perk_max_rank( perk );
        const bool maxed = rank >= max_rank;
        const bool unlocked = level >= perk.required_level && prerequisites_met( perk );

        std::string title = perk_display_name( perk );
        title += "\n" + perk_description( perk );
        title += "\n" + tr( "Tier ", "Тир " ) + std::to_string( perk.tier );
        if( integration_perk( perk ) ) {
            title += " | " + tr( "Requires Survivor level ", "Нужен уровень Survivor " ) +
                     std::to_string( perk.required_level );
        } else {
            title += " | " + tr( "Requires branch level ", "Нужен уровень ветки " ) +
                     std::to_string( perk.required_level );
        }
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );
        if( specialization_root( perk ) ) {
            title += "\n" + tr( "Exclusive choice: the other two specializations stay locked until full respec.",
                                  "Эксклюзивный выбор: две другие специализации будут закрыты до полного сброса." );
        }
        if( integration_perk( perk ) ) {
            title += "\n" + tr( "Requires mod: ", "Нужен мод: " ) + integration_mod_name( perk );
            title += "\n" + tr( "This perk improves abilities or mechanics from that mod.",
                                  "Эффект мода: меняет механику этого мода, а не даёт общие бонусы Survivor." );
        }

        std::string buy;
        if( maxed ) buy = tr( "[Maximum rank]", "[Максимальный ранг]" );
        else if( !unlocked ) buy = tr( "Locked", "Закрыто" );
        else if( rank > 0 ) buy = tr( "Upgrade to rank ", "Улучшить до ранга " ) +
                                  std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
        else buy = tr( "Purchase", "Купить" );

        std::string back = tr( "Back", "Назад" );
        const char *entries[] = { buy.c_str(), back.c_str() };
        const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
        if( choice != 0 ) return;
        if( maxed ) return;
        if( !unlocked ) {
            message( tr( "This perk is locked.", "Этот перк пока закрыт." ) );
            continue;
        }
        purchase_perk( perk );
        return;
    }
}
'@
$sp = Replace-CppRange $sp 'void show_perk_detail( const perk_def &perk )' 'int branch_unlocked_count(' $detailV8 'v8 mod detail gating'

# Human-readable labels for the new native modifier surface.
$effectLabelAnchor = '    if( id == "craft_speed_pct" ) return tr( "Crafting %", "Крафт %" );' + "`n" + '    return id;'
$effectLabelV8 = @'
    if( id == "craft_speed_pct" ) return tr( "Crafting %", "Крафт %" );
    if( id == "mg_spellcraft_flat" ) return "Magiclysm Spellcraft";
    if( id == "mom_metaphysics_flat" ) return "MoM channeling Metaphysics";
    if( id == "xe_deduction_flat" ) return "Xedra Deduction";
    if( id == "xe_gramarye_flat" ) return "Xedra Gramarye";
    if( id == "af_smartgun_flat" ) return "Aftershock Smartgun";
    if( id == "af_metaphysics_flat" ) return "Aftershock Exoplanet channeling Metaphysics";
    if( id == "afp_smartgun_flat" ) return "Aftershock Prime Smartgun";
    if( id == "secx_flesh_craft_flat" ) return "Secronom+ Flesh Weaving";
    if( id == "secx_flesh_combat_flat" ) return "Secronom+ Bio-organic Weapons";
    if( id == "sec_damage_pct" ) return tr( "Damage vs Secronom %", "Урон по Secronom %" );
    if( id == "sec_resist_pct" ) return tr( "Resistance vs Secronom %", "Защита от Secronom %" );
    if( id == "sec_elite_damage_pct" ) return tr( "Damage vs Secronom elites %", "Урон по элите Secronom %" );
    if( id == "sec_elite_resist_pct" ) return tr( "Resistance vs Secronom elites %", "Защита от элиты Secronom %" );
    if( id == "sec_crimson_damage_pct" ) return tr( "Damage vs Crimson Horrors %", "Урон по Crimson Horrors %" );
    if( id == "sec_crimson_resist_pct" ) return tr( "Resistance vs Crimson Horrors %", "Защита от Crimson Horrors %" );
    if( id.find( "spell_cost_pct" ) != std::string::npos ) return tr( "Power cost %", "Стоимость силы %" );
    if( id.find( "cast_time_pct" ) != std::string::npos ) return tr( "Cast time %", "Время применения %" );
    if( id.find( "fail_pct" ) != std::string::npos ) return tr( "Failure %", "Провал %" );
    if( id.find( "spell_xp_pct" ) != std::string::npos ) return tr( "Spell/power XP %", "Опыт сил %" );
    if( id.find( "spell_power_pct" ) != std::string::npos ) return tr( "Spell/power potency %", "Мощность сил %" );
    if( id.find( "range_pct" ) != std::string::npos ) return tr( "Range %", "Дальность %" );
    if( id.find( "aoe_pct" ) != std::string::npos ) return tr( "Area %", "Площадь %" );
    if( id.find( "duration_pct" ) != std::string::npos ) return tr( "Duration %", "Длительность %" );
    return id;
'@
$sp = Replace-TextBlock $sp $effectLabelAnchor $effectLabelV8 "v8 effect-label surface"

$integrationUiV8 = @'
const char *integration_anchor_id( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return "mg_arcane_focus";
    if( mod_id == "mindovermatter" ) return "mom_mental_focus";
    if( mod_id == "xedra_evolved" ) return "xe_anomaly_method";
    if( mod_id == "aftershock_exoplanet" ) return "af_systems_operator";
    if( mod_id == "aftershock_prime" ) return "afp_prime_operator";
    if( mod_id == "secronom" ) return "sec_field_researcher";
    if( mod_id == "secronom_lore_expansion" ) return "secx_flesh_initiate";
    return "";
}

std::string integration_mod_display_name( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return "Magiclysm";
    if( mod_id == "mindovermatter" ) return "Mind Over Matter";
    if( mod_id == "xedra_evolved" ) return "Xedra Evolved";
    if( mod_id == "aftershock_exoplanet" ) return "Aftershock Exoplanet";
    if( mod_id == "aftershock_prime" ) return "Aftershock Prime";
    if( mod_id == "secronom" ) return "Secronom";
    if( mod_id == "secronom_lore_expansion" ) return "Secronom+";
    return mod_id;
}

std::string integration_mod_focus( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return tr(
        "Magiclysm: better Spellcraft, safer casting and stronger spells.",
        "Magiclysm: выше Spellcraft, надёжнее сотворение и сильнее заклинания." );
    if( mod_id == "mindovermatter" ) return tr(
        "Mind Over Matter: stronger powers, steadier channeling and lower stamina cost.",
        "Mind Over Matter: сильнее способности, стабильнее концентрация и ниже расход выносливости." );
    if( mod_id == "xedra_evolved" ) return tr(
        "Xedra Evolved: Deduction, Gramarye and stronger anomaly abilities.",
        "Xedra Evolved: Deduction, Gramarye и усиление аномальных способностей." );
    if( mod_id == "aftershock_exoplanet" ) return tr(
        "Aftershock Exoplanet: Smartgun expertise and stronger, steadier esper powers.",
        "Aftershock Exoplanet: навык Smartgun и более сильные, стабильные эспер-способности." );
    if( mod_id == "aftershock_prime" ) return tr(
        "Aftershock Prime: Smartgun expertise, translocation and more efficient abilities.",
        "Aftershock Prime: навык Smartgun, транслокация и более эффективные способности." );
    if( mod_id == "secronom" ) return tr(
        "Secronom: more damage and resistance against its creatures, elites and Crimson Horrors.",
        "Secronom: больше урона и защиты против его существ, элиты и Crimson Horrors." );
    if( mod_id == "secronom_lore_expansion" ) return tr(
        "Secronom+: Flesh Weaving, living weapons and Flesh Vessel abilities.",
        "Secronom+: Flesh Weaving, живое оружие и способности Flesh Vessel." );
    return {};
}

int integration_owned_count( const std::string &mod_id )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( integration_perk( perk ) && perk_world_available( perk ) &&
            mod_id == integration_mod_id( perk ) && owned( perk ) ) ++result;
    }
    return result;
}

int integration_total_count( const std::string &mod_id )
{
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( integration_perk( perk ) && perk_world_available( perk ) &&
            mod_id == integration_mod_id( perk ) ) ++result;
    }
    return result;
}

std::pair<int, int> integration_tree_position( size_t i )
{
    static const std::array<std::pair<int, int>, 20> layout = {{
        { 0, 2 },
        { 2, 0 }, { 2, 2 }, { 2, 4 },
        { 4, 0 }, { 4, 2 }, { 4, 4 },
        { 6, 0 }, { 6, 2 }, { 6, 4 },
        { 8, 0 }, { 8, 2 }, { 8, 4 },
        { 10, 0 }, { 10, 2 }, { 10, 4 },
        { 12, 0 }, { 12, 2 }, { 12, 4 },
        { 14, 2 }
    }};
    return i < layout.size() ? layout[i] : std::make_pair( 16, static_cast<int>( i % 3 ) * 2 );
}

void show_integration_branch( const std::string &mod_id )
{
    if( !active_world_mod( mod_id.c_str() ) ) {
        message( tr( "This mod is not active in the current world.",
                     "Этот мод не активен в текущем мире." ) );
        return;
    }

    bool tree_mode = true;
    while( true ) {
        const int64_t survivor_level = std::max<int64_t>( 1, get_state( "level", 1 ) );
        const int64_t survivor_xp = std::max<int64_t>( 0, get_state( "xp", 0 ) );
        const int64_t survivor_next = xp_to_next( survivor_level );
        const int64_t perk_points = get_state( "perk_points", 0 );
        const int64_t major_points = get_state( "major_points", 0 );

        std::vector<const perk_def *> mod_perks;
        mod_perks.reserve( 20 );
        const char *anchor_id = integration_anchor_id( mod_id );
        const perk_def *anchor = find_perk( anchor_id );
        if( anchor != nullptr && integration_perk( *anchor ) && mod_id == integration_mod_id( *anchor ) ) {
            mod_perks.push_back( anchor );
        }
        for( const perk_def &perk : perks ) {
            if( !integration_perk( perk ) || !perk_world_available( perk ) ||
                mod_id != integration_mod_id( perk ) || &perk == anchor ) continue;
            mod_perks.push_back( &perk );
        }
        if( mod_perks.empty() ) {
            message( tr( "No Survivor perks are available for this mod.",
                         "Для этого мода нет доступных перков Survivor." ) );
            return;
        }

        std::vector<card_text> texts;
        texts.reserve( mod_perks.size() );
        for( const perk_def *perk_ptr : mod_perks ) {
            const perk_def &perk = *perk_ptr;
            const int rank = perk_rank( perk );
            const int max_rank = perk_max_rank( perk );
            const bool maxed = rank >= max_rank;
            const bool unlocked = survivor_level >= perk.required_level && prerequisites_met( perk );
            const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;

            card_text card;
            card.id = perk.id;
            card.title = perk_display_name( perk );
            card.subtitle = tr( "Survivor L", "Survivor ур." ) + std::to_string( perk.required_level ) +
                            " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
            card.body = perk_description( perk );
            card.badge = perk_kind_label( perk );
            const std::string chevrons = rank_chevrons( perk );
            if( !chevrons.empty() ) card.badge += " | " + chevrons;
            card.badge += " | ";
            if( maxed ) {
                card.badge += tr( "OWNED", "КУПЛЕНО" );
                card.flags |= NCMM_UI_CARD_OWNED;
            } else if( !unlocked ) {
                card.badge += tr( "LOCKED", "ЗАКРЫТО" );
                card.flags |= NCMM_UI_CARD_LOCKED;
            } else if( !enough ) {
                card.badge += tr( "NO POINTS", "НЕТ ОЧКОВ" );
            } else {
                card.badge += tr( "AVAILABLE", "ДОСТУПНО" );
            }
            if( perk.currency == currency_id::major ) card.flags |= NCMM_UI_CARD_MAJOR;
            card.flags |= NCMM_UI_CARD_EFFECT;
            card.icon_key = std::string( "survivor/mod/" ) + mod_id + "/" + perk.id;
            texts.push_back( std::move( card ) );
        }

        const std::string mod_name = integration_mod_display_name( mod_id );
        std::string title = "Survivor Progression > " + mod_name;
        std::string summary = tr( "Mod perks", "Перки мода" ) +
                              tr( " | purchased ", " | куплено " ) +
                              std::to_string( integration_owned_count( mod_id ) ) + "/" +
                              std::to_string( integration_total_count( mod_id ) ) +
                              " | P " + std::to_string( perk_points ) + " | M " + std::to_string( major_points ) +
                              tr( " | requires Survivor level", " | нужен уровень Survivor" );
        std::string progress_label = "Survivor XP " + std::to_string( survivor_xp ) + "/" +
                                     std::to_string( survivor_next ) + tr( " -> L", " -> ур." ) +
                                     std::to_string( survivor_level + 1 );
        ncmm_ui_progress_v1 progress{ progress_label.c_str(), survivor_xp, survivor_next };

        if( tree_mode && host->ui_tree_choose ) {
            std::vector<tree_node_text> tree_texts;
            tree_texts.reserve( mod_perks.size() );
            std::map<std::string, size_t> index_by_id;
            for( size_t i = 0; i < mod_perks.size(); ++i ) {
                tree_node_text node;
                node.card = texts[i];
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( *mod_perks[i] );
                const std::pair<int, int> pos = integration_tree_position( i );
                node.row = pos.first;
                node.column = pos.second;
                index_by_id[mod_perks[i]->id] = i;
                tree_texts.push_back( std::move( node ) );
            }
            std::vector<ncmm_ui_tree_edge_v1> edges;
            auto add_edge = [&]( const char *prereq, size_t to ) {
                if( prereq == nullptr || prereq[0] == '\0' ) return;
                const auto it = index_by_id.find( prereq );
                if( it != index_by_id.end() ) edges.push_back( { it->second, to } );
            };
            for( size_t i = 0; i < mod_perks.size(); ++i ) {
                add_edge( mod_perks[i]->prereq1, i );
                add_edge( mod_perks[i]->prereq2, i );
            }
            std::vector<ncmm_ui_tree_node_v1> nodes = bind_tree_nodes( tree_texts );
            const int choice = host->ui_tree_choose( title.c_str(), summary.c_str(), &progress,
                               nodes.data(), nodes.size(), edges.data(), edges.size() );
            if( choice == NCMM_UI_TREE_SHOW_CARDS ) { tree_mode = false; continue; }
            if( choice < 0 || static_cast<size_t>( choice ) >= mod_perks.size() ) return;
            show_perk_detail( *mod_perks[choice] );
            continue;
        }

        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 2 ) : -1;
        if( choice == NCMM_UI_CARD_SHOW_TREE ) { tree_mode = true; continue; }
        if( choice < 0 || static_cast<size_t>( choice ) >= mod_perks.size() ) return;
        show_perk_detail( *mod_perks[choice] );
    }
}

'@
$sp = Replace-CppRange $sp 'const char *integration_anchor_id( const std::string &mod_id )' 'void open_progression()' $integrationUiV8 'v8 20-node mod UI'

# ---------------------------------------------------------------------------
# v8.7.3 — Survivor UI Theme API adoption + compact tree presentation
# ---------------------------------------------------------------------------
Write-Host "Applying v8.7.6.6 base: themed Survivor branches + compact tree tiles..." -ForegroundColor Cyan

# Survivor requires the additive API 1.7 theme capability.  The legacy card/tree
# entry points remain in the ABI for older modules, but Survivor 0.9.14 now opts
# into the explicit themed entry points generated later in this installer.
if (-not $sp.Contains('    "active_mods.v1",')) { throw 'v8.7.3 Survivor active_mods capability anchor missing.' }
$sp = $sp.Replace('    "active_mods.v1",', '    "active_mods.v1",' + "`n" + '    "ui.theme.v1",')

$uiThemeHelpers = @'
uint32_t branch_theme_color( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return NCMM_UI_COLOR_RED;
        case branch_id::survival: return NCMM_UI_COLOR_GREEN;
        case branch_id::mobility: return NCMM_UI_COLOR_CYAN;
        case branch_id::crafting: return NCMM_UI_COLOR_YELLOW;
        case branch_id::scavenging: return NCMM_UI_COLOR_BLUE;
        case branch_id::mastery: return NCMM_UI_COLOR_MAGENTA;
    }
    return NCMM_UI_COLOR_DEFAULT;
}

uint32_t integration_theme_color( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "mindovermatter" ) return NCMM_UI_COLOR_CYAN;
    if( mod_id == "xedra_evolved" ) return NCMM_UI_COLOR_GREEN;
    if( mod_id == "aftershock_exoplanet" ) return NCMM_UI_COLOR_BLUE;
    if( mod_id == "aftershock_prime" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "secronom" ) return NCMM_UI_COLOR_RED;
    if( mod_id == "secronom_lore_expansion" ) return NCMM_UI_COLOR_YELLOW;
    return NCMM_UI_COLOR_DEFAULT;
}

ncmm_ui_theme_v1 branch_ui_theme( branch_id branch )
{
    return { branch_theme_color( branch ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES,
             30, 46, nullptr, 0 };
}

ncmm_ui_theme_v1 integration_ui_theme( const std::string &mod_id )
{
    return { integration_theme_color( mod_id ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES,
             30, 46, nullptr, 0 };
}

std::string compact_tree_badge( const perk_def &perk, bool unlocked,
                                int64_t perk_points, int64_t major_points,
                                bool mod_branch )
{
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );
    const bool maxed = rank >= max_rank;
    const bool enough = perk.currency == currency_id::perk ? perk_points > 0 : major_points > 0;

    std::string result;
    if( mod_branch ) {
        result = "MOD";
    }
    const std::string chevrons = rank_chevrons( perk );
    if( !chevrons.empty() ) {
        if( !result.empty() ) result += " | ";
        result += chevrons;
    }
    if( !result.empty() ) result += " | ";
    if( maxed ) {
        result += max_rank > 1 ? tr( "MAX ", "МАКС " ) + std::to_string( rank ) + "/" +
                  std::to_string( max_rank ) : tr( "OWN", "КУП" );
    } else if( rank > 0 ) {
        result += "R" + std::to_string( rank ) + "/" + std::to_string( max_rank );
    } else if( !unlocked ) {
        result += tr( "LOCK", "ЗАКР" );
    } else if( !enough ) {
        result += tr( "NO PTS", "НЕТ ОЧК" );
    } else {
        result += tr( "READY", "ГОТОВ" );
    }
    return result;
}

'@
if (-not $sp.Contains('bool perk_world_available( const perk_def &perk )')) {
    throw 'v8.7.3 Survivor theme helper insertion anchor missing.'
}
$sp = $sp.Replace('bool perk_world_available( const perk_def &perk )',
                  $uiThemeHelpers + 'bool perk_world_available( const perk_def &perk )')

$coreTreeNodeOld = @'
                tree_node_text node;
                node.card = texts[i];
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( perk );
'@
$coreTreeNodeNew = @'
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( perk );
                const int tree_max_rank = perk_max_rank( perk );
                const bool tree_unlocked = level >= perk.required_level && prerequisites_met( perk );
                node.card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                                     tr( "Lv ", "ур. " ) + std::to_string( perk.required_level ) +
                                     " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                node.card.badge = compact_tree_badge( perk, tree_unlocked,
                                                       perk_points, major_points, false );
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( perk );
'@
$sp = Replace-TextBlock $sp $coreTreeNodeOld $coreTreeNodeNew 'v8.7.3 compact core tree node'

$coreTreeCallOld = @'
            const int choice = host->ui_tree_choose(
                                   title.c_str(), tree_summary.c_str(), &progress,
                                   nodes.data(), nodes.size(), edges.data(), edges.size() );
'@
$coreTreeCallNew = @'
            const ncmm_ui_theme_v1 theme = branch_ui_theme( branch );
            const int choice = host->ui_tree_choose_themed(
                                   title.c_str(), tree_summary.c_str(), &progress,
                                   nodes.data(), nodes.size(), edges.data(), edges.size(), &theme );
'@
$sp = Replace-TextBlock $sp $coreTreeCallOld $coreTreeCallNew 'v8.7.3 themed core tree call'

$coreCardCallOld = @'
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), card_summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 2 ) :
                           -1;
'@
$coreCardCallNew = @'
        const ncmm_ui_theme_v1 theme = branch_ui_theme( branch );
        const int choice = host->ui_card_choose_themed ?
                           host->ui_card_choose_themed( title.c_str(), card_summary.c_str(), &progress,
                                                        cards.data(), cards.size(), 2, &theme ) :
                           -1;
'@
$sp = Replace-TextBlock $sp $coreCardCallOld $coreCardCallNew 'v8.7.3 themed core cards'

$modTreeNodeOld = @'
                tree_node_text node;
                node.card = texts[i];
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( *mod_perks[i] );
'@
$modTreeNodeNew = @'
                const perk_def &tree_perk = *mod_perks[i];
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( tree_perk );
                const int tree_max_rank = perk_max_rank( tree_perk );
                const bool tree_unlocked = survivor_level >= tree_perk.required_level &&
                                           prerequisites_met( tree_perk );
                node.card.subtitle = tr( "Lv ", "ур. " ) + std::to_string( tree_perk.required_level ) +
                                     " | " + ( tree_perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                node.card.badge = compact_tree_badge( tree_perk, tree_unlocked,
                                                       perk_points, major_points, true );
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( tree_perk );
'@
$sp = Replace-TextBlock $sp $modTreeNodeOld $modTreeNodeNew 'v8.7.3 compact mod tree node'

$modTreeCallOld = @'
            const int choice = host->ui_tree_choose( title.c_str(), summary.c_str(), &progress,
                               nodes.data(), nodes.size(), edges.data(), edges.size() );
'@
$modTreeCallNew = @'
            const ncmm_ui_theme_v1 theme = integration_ui_theme( mod_id );
            const int choice = host->ui_tree_choose_themed( title.c_str(), summary.c_str(), &progress,
                               nodes.data(), nodes.size(), edges.data(), edges.size(), &theme );
'@
$sp = Replace-TextBlock $sp $modTreeCallOld $modTreeCallNew 'v8.7.3 themed mod tree call'

$modCardCallOld = @'
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 2 ) : -1;
'@
$modCardCallNew = @'
        const ncmm_ui_theme_v1 theme = integration_ui_theme( mod_id );
        const int choice = host->ui_card_choose_themed ?
                           host->ui_card_choose_themed( title.c_str(), summary.c_str(), &progress,
                                                        cards.data(), cards.size(), 2, &theme ) : -1;
'@
$sp = Replace-TextBlock $sp $modCardCallOld $modCardCallNew 'v8.7.3 themed mod cards'

$overviewCallOld = @'
        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        const int choice = host->ui_card_choose ?
                           host->ui_card_choose( title.c_str(), summary.c_str(), &progress,
                                                 cards.data(), cards.size(), 3 ) :
                           -1;
'@
$overviewCallNew = @'
        std::vector<ncmm_ui_card_v1> cards = bind_cards( texts );
        std::vector<uint32_t> item_accents;
        item_accents.reserve( cards.size() );
        for( branch_id branch : branches ) {
            item_accents.push_back( branch_theme_color( branch ) );
        }
        for( const std::string &mod_id : mod_branches ) {
            item_accents.push_back( integration_theme_color( mod_id ) );
        }
        while( item_accents.size() < cards.size() ) {
            item_accents.push_back( NCMM_UI_COLOR_DEFAULT );
        }
        const ncmm_ui_theme_ex_v1 overview_theme{
            NCMM_UI_COLOR_DEFAULT,
            NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_SECTIONED_DETAIL,
            0, 42, item_accents.data(), item_accents.size(), nullptr, 0
        };
        const int choice = host->ui_card_choose_rpg ?
                           host->ui_card_choose_rpg( title.c_str(), summary.c_str(), &progress,
                                                     cards.data(), cards.size(), 3, &overview_theme ) :
                           host->ui_card_choose_themed ?
                           host->ui_card_choose_themed( title.c_str(), summary.c_str(), &progress,
                                                        cards.data(), cards.size(), 3,
                                                        reinterpret_cast<const ncmm_ui_theme_v1 *>( &overview_theme ) ) :
                           -1;
'@
$sp = Replace-TextBlock $sp $overviewCallOld $overviewCallNew 'v8.7.3 themed overview cards'

$initUiOld = '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->ui_message'
$initUiNew = '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_message'
if (-not $sp.Contains($initUiOld)) { throw 'v8.7.3 Survivor themed API init-check anchor missing.' }
$sp = $sp.Replace($initUiOld,$initUiNew)

# Keep module manifest aligned with the new host dependency.
$manifest = [IO.File]::ReadAllText($manifestPath)
if (-not $manifest.Contains('"api_min_minor": 5')) { throw 'v8.7.3 Survivor manifest API 1.5 anchor missing.' }
$manifest = $manifest.Replace('"api_min_minor": 5','"api_min_minor": 7')
if (-not $manifest.Contains('    "active_mods.v1",')) { throw 'v8.7.3 Survivor manifest active_mods anchor missing.' }
$manifest = $manifest.Replace('    "active_mods.v1",', '    "active_mods.v1",' + "`n" + '    "ui.theme.v1",')
Write-Utf8NoBom $manifestPath $manifest

# ---------------------------------------------------------------------------
# 0.9.15 — Prime specialization tradeoffs (core + every supported mod branch)
# ---------------------------------------------------------------------------
Write-Host "Applying Survivor 0.9.15: Prime Specialization Tradeoffs..." -ForegroundColor Cyan

# New persisted per-mod Prime commitments require schema 8.  Existing perk IDs and
# the six core specialization-state keys are retained, so 0.9.14 saves migrate in place.
if (-not $sp.Contains('constexpr int state_schema = 7;')) { throw '0.9.15 expected state schema 7.' }
$sp = $sp.Replace('constexpr int state_schema = 7;','constexpr int state_schema = 8;')
if (-not $sp.Contains('#include <map>')) { throw '0.9.15 C++ include anchor missing.' }
if (-not $sp.Contains('#include <set>')) { $sp = $sp.Replace('#include <map>','#include <map>' + "`n" + '#include <set>') }

function Replace-PrimeCorePerk0915([string]$Id,[string]$Row) {
    $pattern = '(?m)^[ \t]*\{ "' + [regex]::Escape($Id) + '",\s*branch_id::.*$'
    $rx = New-Object System.Text.RegularExpressions.Regex($pattern)
    $count = $rx.Matches($sp).Count
    if ($count -ne 1) { throw "0.9.15 core Prime replacement expected one $Id, found $count" }
    $script:sp = $rx.Replace($script:sp,$Row,1)
}

$corePrime0915 = [ordered]@{
'spc_c_juggernaut' = '    { "spc_c_juggernaut", branch_id::combat, 4, 15, currency_id::perk, "c_conditioning", "", "Prime Juggernaut", "Прайм: Штурмовик", "+2 STR, +25% stamina, +20% carry; -12% speed.", "+2 СИЛ, +25% выносливости, +20% грузоподъёмности; -12% скорости.", {{ { "str_flat", 2 }, { "stamina_max_pct", 25 }, { "carry_weight_pct", 20 }, { "speed_pct", -12 } }}, 4, 0, perk_kind::effect },'
'spc_c_duelist' = '    { "spc_c_duelist", branch_id::combat, 4, 15, currency_id::perk, "c_tempo", "", "Prime Duelist", "Прайм: Дуэлянт", "+10% speed, +2 dodge, -10% move cost; -25% carry.", "+10% скорости, +2 к уклонению, -10% стоимости движения; -25% грузоподъёмности.", {{ { "speed_pct", 10 }, { "dodge_flat", 2 }, { "move_cost_pct", -10 }, { "carry_weight_pct", -25 } }}, 4, 0, perk_kind::effect },'
'spc_c_tactician' = '    { "spc_c_tactician", branch_id::combat, 4, 15, currency_id::perk, "c_precision", "c_reflexes", "Prime Tactician", "Прайм: Тактик", "+2 PER, +1.5 melee hit, +5% speed; -20% stamina.", "+2 ВОС, +1,5 точности ближнего боя, +5% скорости; -20% выносливости.", {{ { "per_flat", 2 }, { "melee_hit_flat", 1.5 }, { "speed_pct", 5 }, { "stamina_max_pct", -20 } }}, 4, 0, perk_kind::effect },'
'spc_s_nomad' = '    { "spc_s_nomad", branch_id::survival, 4, 15, currency_id::perk, "s_endurance", "", "Prime Nomad", "Прайм: Кочевник", "+30% stamina, +30% carry; -12% speed.", "+30% выносливости, +30% грузоподъёмности; -12% скорости.", {{ { "stamina_max_pct", 30 }, { "carry_weight_pct", 30 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_s_medic' = '    { "spc_s_medic", branch_id::survival, 4, 15, currency_id::perk, "s_field", "s_resilient", "Prime Field Medic", "Прайм: Полевой медик", "+60% healing, +15% stamina; -25% carry.", "+60% лечения, +15% выносливости; -25% грузоподъёмности.", {{ { "healing_pct", 60 }, { "stamina_max_pct", 15 }, { "carry_weight_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_s_quartermaster' = '    { "spc_s_quartermaster", branch_id::survival, 4, 15, currency_id::perk, "s_pack", "", "Prime Quartermaster", "Прайм: Интендант", "+40% carry, +25% crafting speed; -12% speed.", "+40% грузоподъёмности, +25% скорости крафта; -12% скорости.", {{ { "carry_weight_pct", 40 }, { "craft_speed_pct", 25 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_m_sprinter' = '    { "spc_m_sprinter", branch_id::mobility, 4, 15, currency_id::perk, "m_cardio", "", "Prime Sprinter", "Прайм: Спринтер", "+12% speed, -10% move cost; -35% carry.", "+12% скорости, -10% стоимости движения; -35% грузоподъёмности.", {{ { "speed_pct", 12 }, { "move_cost_pct", -10 }, { "carry_weight_pct", -35 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_m_ghost' = '    { "spc_m_ghost", branch_id::mobility, 4, 15, currency_id::perk, "m_light", "m_parkour", "Prime Ghost", "Прайм: Призрак", "-15% move cost, +2 dodge; -25% stamina.", "-15% стоимости движения, +2 к уклонению; -25% выносливости.", {{ { "move_cost_pct", -15 }, { "dodge_flat", 2 }, { "stamina_max_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_m_pathfinder' = '    { "spc_m_pathfinder", branch_id::mobility, 4, 15, currency_id::perk, "m_stride", "", "Prime Pathfinder", "Прайм: Путепроходец", "-12% move cost, +25% stamina, +20% carry; -25% healing.", "-12% стоимости движения, +25% выносливости, +20% грузоподъёмности; -25% лечения.", {{ { "move_cost_pct", -12 }, { "stamina_max_pct", 25 }, { "carry_weight_pct", 20 }, { "healing_pct", -25 } }}, 4, 0, perk_kind::effect },'
'spc_f_systems' = '    { "spc_f_systems", branch_id::crafting, 4, 15, currency_id::perk, "f_engineer", "", "Prime Systems Engineer", "Прайм: Системный инженер", "+30% crafting speed, +2 INT; -12% speed.", "+30% скорости крафта, +2 ИНТ; -12% скорости.", {{ { "craft_speed_pct", 30 }, { "int_flat", 2 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_f_improviser' = '    { "spc_f_improviser", branch_id::crafting, 4, 15, currency_id::perk, "f_hands", "f_workflow", "Prime Improviser", "Прайм: Импровизатор", "+25% crafting speed, +25% carry; -30% reading speed.", "+25% скорости крафта, +25% грузоподъёмности; -30% скорости чтения.", {{ { "craft_speed_pct", 25 }, { "carry_weight_pct", 25 }, { "read_speed_pct", -30 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_f_researcher' = '    { "spc_f_researcher", branch_id::crafting, 4, 15, currency_id::perk, "f_reader", "f_scholar", "Prime Researcher", "Прайм: Исследователь", "+35% reading speed, +2 INT; -20% crafting speed.", "+35% скорости чтения, +2 ИНТ; -20% скорости крафта.", {{ { "read_speed_pct", 35 }, { "int_flat", 2 }, { "craft_speed_pct", -20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_g_prospector' = '    { "spc_g_prospector", branch_id::scavenging, 4, 15, currency_id::perk, "g_observer", "", "Prime Prospector", "Прайм: Искатель", "+2 PER, +25% carry; -12% speed.", "+2 ВОС, +25% грузоподъёмности; -12% скорости.", {{ { "per_flat", 2 }, { "carry_weight_pct", 25 }, { "speed_pct", -12 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_g_courier' = '    { "spc_g_courier", branch_id::scavenging, 4, 15, currency_id::perk, "g_pack", "g_endurance", "Prime Courier", "Прайм: Курьер", "+40% carry, -12% move cost; -1.5 PER.", "+40% грузоподъёмности, -12% стоимости движения; -1,5 ВОС.", {{ { "carry_weight_pct", 40 }, { "move_cost_pct", -12 }, { "per_flat", -1.5 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_g_investigator' = '    { "spc_g_investigator", branch_id::scavenging, 4, 15, currency_id::perk, "g_awareness", "", "Prime Investigator", "Прайм: Исследователь руин", "+2 PER, +30% reading speed; -25% carry.", "+2 ВОС, +30% скорости чтения; -25% грузоподъёмности.", {{ { "per_flat", 2 }, { "read_speed_pct", 30 }, { "carry_weight_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },'
'spc_a_specialist' = '    { "spc_a_specialist", branch_id::mastery, 4, 15, currency_id::perk, "a_focus", "a_growth", "Prime Specialist", "Прайм: Специалист", "+30% Survivor XP and +1 INT; -1 STR, -1 DEX.", "+30% опыта Survivor и +1 ИНТ; -1 СИЛ, -1 ЛОВ.", {{ { "int_flat", 1 }, { "str_flat", -1 }, { "dex_flat", -1 }, { nullptr, 0 } }}, 3, 30, perk_kind::effect },'
'spc_a_polymath' = '    { "spc_a_polymath", branch_id::mastery, 4, 15, currency_id::perk, "a_balance", "a_polymath", "Prime Polymath", "Прайм: Универсал", "+1 STR/DEX/PER/INT; -20% Survivor XP.", "+1 СИЛ/ЛОВ/ВОС/ИНТ; -20% опыта Survivor.", {{ { "str_flat", 1 }, { "dex_flat", 1 }, { "per_flat", 1 }, { "int_flat", 1 } }}, 4, -20, perk_kind::effect },'
'spc_a_selfteacher' = '    { "spc_a_selfteacher", branch_id::mastery, 4, 15, currency_id::perk, "a_adapt", "", "Prime Self-Teacher", "Прайм: Самоучка", "+25% reading, +25% crafting, +15% Survivor XP; -15% speed.", "+25% чтения, +25% крафта, +15% опыта Survivor; -15% скорости.", {{ { "read_speed_pct", 25 }, { "craft_speed_pct", 25 }, { "speed_pct", -15 }, { nullptr, 0 } }}, 3, 15, perk_kind::effect },'
}
foreach ($primeEntry in $corePrime0915.GetEnumerator()) {
    Replace-PrimeCorePerk0915 $primeEntry.Key $primeEntry.Value
}

$modPrimePerks0915 = @'
    { "mg_prime_arcanist", branch_id::mastery, 9, 45, currency_id::major, "mg_archmage", "", "Prime Arcanist", "Прайм: Арканист", "+2 Spellcraft, +20% potency, +15% range; mana cost +30%.", "+2 Spellcraft, +20% мощности, +15% дальности; стоимость маны +30%.", {{ { "mg_spellcraft_flat", 2 }, { "mg_spell_power_pct", 20 }, { "mg_range_pct", 15 }, { "mg_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mg_prime_channeler", branch_id::mastery, 9, 45, currency_id::major, "mg_archmage", "", "Prime Channeler", "Прайм: Проводник", "+40% mana, +30% mana regen, spell cost -20%; casting time +30%.", "+40% маны, +30% восстановления маны, стоимость заклинаний -20%; время сотворения +30%.", {{ { "mg_mana_max_pct", 40 }, { "mg_mana_regen_pct", 30 }, { "mg_spell_cost_pct", -20 }, { "mg_cast_time_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mg_prime_warcaster", branch_id::mastery, 9, 45, currency_id::major, "mg_archmage", "", "Prime Warcaster", "Прайм: Боевой маг", "Casting time -25%, failure chance -20%, potency +20%; spell XP -35%.", "Время сотворения -25%, шанс провала -20%, мощность +20%; опыт заклинаний -35%.", {{ { "mg_cast_time_pct", -25 }, { "mg_fail_pct", -20 }, { "mg_spell_power_pct", 20 }, { "mg_spell_xp_pct", -35 } }}, 4, 0, perk_kind::effect },

    { "mom_prime_kinetic", branch_id::mastery, 9, 45, currency_id::major, "mom_transcendent_focus", "", "Prime Kinetic Savant", "Прайм: Кинетик", "Potency +25%, range +20%, area +20%; psionic cost +30%.", "Мощность +25%, дальность +20%, площадь +20%; стоимость псионики +30%.", {{ { "mom_spell_power_pct", 25 }, { "mom_range_pct", 20 }, { "mom_aoe_pct", 20 }, { "mom_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mom_prime_overclock", branch_id::mastery, 9, 45, currency_id::major, "mom_transcendent_focus", "", "Prime Neural Overclock", "Прайм: Нейроразгон", "+1.5 Metaphysics, activation -25%, power XP +20%; failure +30%.", "+1,5 Metaphysics, время активации -25%, опыт сил +20%; шанс провала +30%.", {{ { "mom_metaphysics_flat", 1.5 }, { "mom_cast_time_pct", -25 }, { "mom_spell_xp_pct", 20 }, { "mom_fail_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "mom_prime_ascetic", branch_id::mastery, 9, 45, currency_id::major, "mom_transcendent_focus", "", "Prime Deep Focus", "Прайм: Глубокий фокус", "Failure chance -25%, cost -20%, duration +30%; potency -20%.", "Шанс провала -25%, стоимость -20%, длительность +30%; мощность -20%.", {{ { "mom_fail_pct", -25 }, { "mom_spell_cost_pct", -20 }, { "mom_duration_pct", 30 }, { "mom_spell_power_pct", -20 } }}, 4, 0, perk_kind::effect },

    { "xe_prime_analyst", branch_id::mastery, 9, 45, currency_id::major, "xe_boundary_master", "", "Prime Anomaly Analyst", "Прайм: Аналитик аномалий", "+1.5 Deduction, +1.5 Gramarye, potency +20%; failure +30%.", "+1,5 Deduction, +1,5 Gramarye, мощность +20%; шанс провала +30%.", {{ { "xe_deduction_flat", 1.5 }, { "xe_gramarye_flat", 1.5 }, { "xe_spell_power_pct", 20 }, { "xe_fail_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "xe_prime_resonant", branch_id::mastery, 9, 45, currency_id::major, "xe_boundary_master", "", "Prime Resonance Vessel", "Прайм: Резонансный сосуд", "+40% mana, +30% mana regen, cost -20%; range -20%.", "+40% маны, +30% восстановления маны, стоимость -20%; дальность -20%.", {{ { "xe_mana_max_pct", 40 }, { "xe_mana_regen_pct", 30 }, { "xe_spell_cost_pct", -20 }, { "xe_range_pct", -20 } }}, 4, 0, perk_kind::effect },
    { "xe_prime_riftwalker", branch_id::mastery, 9, 45, currency_id::major, "xe_boundary_master", "", "Prime Riftwalker", "Прайм: Странник разломов", "Range +30%, area +25%, casting time -20%; duration -30%.", "Дальность +30%, площадь +25%, время сотворения -20%; длительность -30%.", {{ { "xe_range_pct", 30 }, { "xe_aoe_pct", 25 }, { "xe_cast_time_pct", -20 }, { "xe_duration_pct", -30 } }}, 4, 0, perk_kind::effect },

    { "af_prime_smartgun", branch_id::mastery, 9, 45, currency_id::major, "af_posthuman_operator", "", "Prime Smartgun Ace", "Прайм: Ас Smartgun", "+2 Smartgun, potency +20%, range +15%; energy cost +30%.", "+2 Smartgun, мощность +20%, дальность +15%; стоимость энергии +30%.", {{ { "af_smartgun_flat", 2 }, { "af_spell_power_pct", 20 }, { "af_range_pct", 15 }, { "af_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "af_prime_systems", branch_id::mastery, 9, 45, currency_id::major, "af_posthuman_operator", "", "Prime Systems Specialist", "Прайм: Специалист по системам", "+1.5 Metaphysics, cost -20%, activation -20%; ability XP -30%.", "+1,5 Metaphysics, стоимость -20%, время активации -20%; опыт способностей -30%.", {{ { "af_metaphysics_flat", 1.5 }, { "af_spell_cost_pct", -20 }, { "af_cast_time_pct", -20 }, { "af_spell_xp_pct", -30 } }}, 4, 0, perk_kind::effect },
    { "af_prime_phase", branch_id::mastery, 9, 45, currency_id::major, "af_posthuman_operator", "", "Prime Phase Engineer", "Прайм: Фазовый инженер", "Range +30%, area +25%, duration +25%; failure chance +30%.", "Дальность +30%, площадь +25%, длительность +25%; шанс провала +30%.", {{ { "af_range_pct", 30 }, { "af_aoe_pct", 25 }, { "af_duration_pct", 25 }, { "af_fail_pct", 30 } }}, 4, 0, perk_kind::effect },

    { "afp_prime_gunslinger", branch_id::mastery, 9, 45, currency_id::major, "afp_prime_integrator", "", "Prime Gunslinger", "Прайм: Стрелок Prime", "+2 Smartgun, potency +20%, range +15%; energy cost +30%.", "+2 Smartgun, мощность +20%, дальность +15%; стоимость энергии +30%.", {{ { "afp_smartgun_flat", 2 }, { "afp_spell_power_pct", 20 }, { "afp_range_pct", 15 }, { "afp_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "afp_prime_systems_specialist", branch_id::mastery, 9, 45, currency_id::major, "afp_prime_integrator", "", "Prime Systems Controller", "Прайм: Системный оператор", "Energy cost -25%, activation -20%, duration +25%; ability XP -30%.", "Стоимость энергии -25%, время активации -20%, длительность +25%; опыт способностей -30%.", {{ { "afp_spell_cost_pct", -25 }, { "afp_cast_time_pct", -20 }, { "afp_duration_pct", 25 }, { "afp_spell_xp_pct", -30 } }}, 4, 0, perk_kind::effect },
    { "afp_prime_translocator", branch_id::mastery, 9, 45, currency_id::major, "afp_prime_integrator", "", "Prime Translocator", "Прайм: Транслокатор", "Range +35%, area +25%, activation time -20%; failure chance +30%.", "Дальность +35%, площадь +25%, время активации -20%; шанс провала +30%.", {{ { "afp_range_pct", 35 }, { "afp_aoe_pct", 25 }, { "afp_cast_time_pct", -20 }, { "afp_fail_pct", 30 } }}, 4, 0, perk_kind::effect },

    { "sec_prime_hunter", branch_id::mastery, 9, 45, currency_id::major, "sec_nightmare_specialist", "", "Prime Hunter", "Прайм: Охотник", "+20% damage vs Secronom, +15% elite damage; -20% Secronom resistance.", "+20% урона по Secronom, +15% урона по элите; -20% защиты от Secronom.", {{ { "sec_damage_pct", 20 }, { "sec_elite_damage_pct", 15 }, { "sec_resist_pct", -20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "sec_prime_bulwark", branch_id::mastery, 9, 45, currency_id::major, "sec_nightmare_specialist", "", "Prime Bulwark", "Прайм: Бастион", "+25% Secronom resistance, +20% elite resistance; -20% damage vs Secronom.", "+25% защиты от Secronom, +20% защиты от элиты; -20% урона по Secronom.", {{ { "sec_resist_pct", 25 }, { "sec_elite_resist_pct", 20 }, { "sec_damage_pct", -20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "sec_prime_crimson", branch_id::mastery, 9, 45, currency_id::major, "sec_nightmare_specialist", "", "Prime Crimson Reaper", "Прайм: Багровый жнец", "+30% Crimson damage, +20% elite damage; -25% Crimson resistance.", "+30% урона по Crimson, +20% урона по элите; -25% защиты от Crimson.", {{ { "sec_crimson_damage_pct", 30 }, { "sec_elite_damage_pct", 20 }, { "sec_crimson_resist_pct", -25 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "secx_prime_architect", branch_id::mastery, 9, 45, currency_id::major, "secx_flesh_architect", "", "Prime Flesh Architect", "Прайм: Архитектор плоти", "+2 Flesh Weaving, potency +20%, ability XP +20%; energy cost +30%.", "+2 Flesh Weaving, мощность +20%, опыт способностей +20%; стоимость энергии +30%.", {{ { "secx_flesh_craft_flat", 2 }, { "secx_spell_power_pct", 20 }, { "secx_spell_xp_pct", 20 }, { "secx_spell_cost_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "secx_prime_predator", branch_id::mastery, 9, 45, currency_id::major, "secx_flesh_architect", "", "Prime Biomorph Predator", "Прайм: Биоморф-хищник", "+2 Bio-organic Weapons, potency +20%, range +15%; failure +30%.", "+2 Bio-organic Weapons, мощность +20%, дальность +15%; шанс провала +30%.", {{ { "secx_flesh_combat_flat", 2 }, { "secx_spell_power_pct", 20 }, { "secx_range_pct", 15 }, { "secx_fail_pct", 30 } }}, 4, 0, perk_kind::effect },
    { "secx_prime_vessel", branch_id::mastery, 9, 45, currency_id::major, "secx_flesh_architect", "", "Prime Flesh Vessel", "Прайм: Сосуд плоти", "Energy cost -25%, failure chance -20%, duration +30%; Flesh Weaving -1.5.", "Стоимость энергии -25%, шанс провала -20%, длительность +30%; Flesh Weaving -1,5.", {{ { "secx_spell_cost_pct", -25 }, { "secx_fail_pct", -20 }, { "secx_duration_pct", 30 }, { "secx_flesh_craft_flat", -1.5 } }}, 4, 0, perk_kind::effect }
'@

# Append exactly three Prime choices to each of the seven conditional mod branches.
$primeArrayStart = $sp.IndexOf('const perk_def perks[] = {')
if ($primeArrayStart -lt 0) { throw '0.9.15 perk array start missing.' }
$primeArrayEnd = $sp.IndexOf("`n};",$primeArrayStart)
if ($primeArrayEnd -lt 0) { throw '0.9.15 perk array end missing.' }
$sp = $sp.Insert($primeArrayEnd,"`n" + $modPrimePerks0915.TrimEnd())

$primeIntegrationEntries0915 = @'
        { "mg_prime_arcanist", integration_id::magiclysm },
        { "mg_prime_channeler", integration_id::magiclysm },
        { "mg_prime_warcaster", integration_id::magiclysm },
        { "mom_prime_kinetic", integration_id::mindovermatter },
        { "mom_prime_overclock", integration_id::mindovermatter },
        { "mom_prime_ascetic", integration_id::mindovermatter },
        { "xe_prime_analyst", integration_id::xedra_evolved },
        { "xe_prime_resonant", integration_id::xedra_evolved },
        { "xe_prime_riftwalker", integration_id::xedra_evolved },
        { "af_prime_smartgun", integration_id::aftershock_exoplanet },
        { "af_prime_systems", integration_id::aftershock_exoplanet },
        { "af_prime_phase", integration_id::aftershock_exoplanet },
        { "afp_prime_gunslinger", integration_id::aftershock_prime },
        { "afp_prime_systems_specialist", integration_id::aftershock_prime },
        { "afp_prime_translocator", integration_id::aftershock_prime },
        { "sec_prime_hunter", integration_id::secronom },
        { "sec_prime_bulwark", integration_id::secronom },
        { "sec_prime_crimson", integration_id::secronom },
        { "secx_prime_architect", integration_id::secronom_plus },
        { "secx_prime_predator", integration_id::secronom_plus },
        { "secx_prime_vessel", integration_id::secronom_plus },
'@
$registryAnchor0915 = '        { "secx_flesh_architect", integration_id::secronom_plus },'
if (-not $sp.Contains($registryAnchor0915)) { throw '0.9.15 integration registry tail anchor missing.' }
$sp = $sp.Replace($registryAnchor0915,$registryAnchor0915 + "`n" + $primeIntegrationEntries0915.TrimEnd())

$primeHelpers0915 = @'
int mod_prime_root_slot( const char *raw_id )
{
    const std::string_view id = raw_id ? std::string_view( raw_id ) : std::string_view();
    if( id == "mg_prime_arcanist" || id == "mom_prime_kinetic" || id == "xe_prime_analyst" ||
        id == "af_prime_smartgun" || id == "afp_prime_gunslinger" || id == "sec_prime_hunter" ||
        id == "secx_prime_architect" ) return 1;
    if( id == "mg_prime_channeler" || id == "mom_prime_overclock" || id == "xe_prime_resonant" ||
        id == "af_prime_systems" || id == "afp_prime_systems_specialist" || id == "sec_prime_bulwark" ||
        id == "secx_prime_predator" ) return 2;
    if( id == "mg_prime_warcaster" || id == "mom_prime_ascetic" || id == "xe_prime_riftwalker" ||
        id == "af_prime_phase" || id == "afp_prime_translocator" || id == "sec_prime_crimson" ||
        id == "secx_prime_vessel" ) return 3;
    return 0;
}

bool mod_prime_specialization_root( const perk_def &perk )
{
    return mod_prime_root_slot( perk.id ) > 0;
}

std::string mod_prime_state_key( const perk_def &perk )
{
    switch( perk_integration( perk ) ) {
        case integration_id::magiclysm: return "prime_magiclysm";
        case integration_id::mindovermatter: return "prime_mindovermatter";
        case integration_id::xedra_evolved: return "prime_xedra_evolved";
        case integration_id::aftershock_exoplanet: return "prime_aftershock_exoplanet";
        case integration_id::aftershock_prime: return "prime_aftershock_prime";
        case integration_id::secronom: return "prime_secronom";
        case integration_id::secronom_plus: return "prime_secronom_plus";
        default: return "prime_unknown";
    }
}

bool mod_prime_specialization_allowed( const perk_def &perk )
{
    const int slot = mod_prime_root_slot( perk.id );
    if( slot <= 0 ) return true;
    const int64_t selected = get_state( mod_prime_state_key( perk ), 0 );
    return selected == 0 || selected == slot;
}

bool exclusive_specialization_perk( const perk_def &perk )
{
    return specialization_perk( perk ) || mod_prime_specialization_root( perk );
}

bool exclusive_specialization_root( const perk_def &perk )
{
    return specialization_root( perk ) || mod_prime_specialization_root( perk );
}

int exclusive_specialization_slot( const perk_def &perk )
{
    const int mod_slot = mod_prime_root_slot( perk.id );
    return mod_slot > 0 ? mod_slot : specialization_slot( perk );
}

std::string exclusive_specialization_state_key( const perk_def &perk )
{
    return mod_prime_specialization_root( perk ) ?
           mod_prime_state_key( perk ) : specialization_state_key( perk.branch );
}

bool exclusive_specialization_allowed( const perk_def &perk )
{
    if( mod_prime_specialization_root( perk ) ) return mod_prime_specialization_allowed( perk );
    return specialization_allowed( perk );
}

'@
if (-not $sp.Contains('bool owned( const perk_def &perk )')) { throw '0.9.15 exclusive specialization insertion anchor missing.' }
$sp = $sp.Replace('bool owned( const perk_def &perk )',$primeHelpers0915 + 'bool owned( const perk_def &perk )')

$prereqPrime0915 = @'
bool prerequisites_met( const perk_def &perk )
{
    if( !perk_world_available( perk ) || !exclusive_specialization_allowed( perk ) ) {
        return false;
    }
    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) continue;
        const perk_def *required = find_perk( id );
        if( required == nullptr || !owned( *required ) ) return false;
    }
    return true;
}
'@
$sp = Replace-CppRange $sp 'bool prerequisites_met( const perk_def &perk )' 'std::string prereq_text( const perk_def &perk )' $prereqPrime0915 '0.9.15 Prime prerequisites'

$purchasePrime0915 = @'
bool purchase_perk( const perk_def &perk )
{
    const int64_t level = perk_progression_level( perk );
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( !perk_world_available( perk ) ) {
        message( tr( "The required mod is not active.", "Требуемый мод не активен." ) );
        return false;
    }
    if( rank >= max_rank ) {
        message( tr( "This perk is already at maximum rank.", "Этот перк уже максимального ранга." ) );
        return false;
    }
    if( level < perk.required_level ) {
        message( integration_perk( perk ) ?
                 tr( "Your Survivor level is too low for this perk.", "Недостаточный уровень Survivor для этого перка." ) :
                 tr( "Your branch level is too low.", "Недостаточный уровень этой ветки." ) );
        return false;
    }
    if( !prerequisites_met( perk ) ) {
        if( exclusive_specialization_perk( perk ) && !exclusive_specialization_allowed( perk ) ) {
            message( tr( "Another Prime specialization is already committed here. Full respec is required to change it.",
                         "Здесь уже выбрана другая Прайм-специализация. Для смены нужен полный сброс." ) );
        } else {
            message( tr( "Prerequisites are not met.", "Не выполнены требования предыдущих перков." ) );
        }
        return false;
    }

    int chosen_slot = 0;
    std::string chosen_state;
    if( exclusive_specialization_root( perk ) ) {
        chosen_slot = exclusive_specialization_slot( perk );
        chosen_state = exclusive_specialization_state_key( perk );
        const int64_t existing = get_state( chosen_state, 0 );
        if( existing == 0 ) {
            std::string prompt = tr(
                "Choose this Prime specialization? Its bonus and drawback remain until a full respec, and the other two choices will be locked.\n",
                "Выбрать эту Прайм-специализацию? Её бонус и штраф останутся до полного сброса, а два других варианта будут закрыты.\n" );
            prompt += perk_display_name( perk ) + "\n" + perk_description( perk );
            std::string yes = tr( "Choose Prime", "Выбрать" );
            std::string no = tr( "Cancel", "Отмена" );
            const char *entries[] = { yes.c_str(), no.c_str() };
            const int choice = host->ui_choose ? host->ui_choose( prompt.c_str(), entries, 2 ) : -1;
            if( choice != 0 ) return false;
        }
    }

    if( perk.currency == currency_id::perk ) {
        if( perk_points <= 0 ) {
            message( tr( "Not enough perk points.", "Недостаточно очков перков." ) );
            return false;
        }
        set_state( "perk_points", perk_points - 1 );
    } else {
        if( major_points <= 0 ) {
            message( tr( "Not enough major points.", "Недостаточно больших очков." ) );
            return false;
        }
        set_state( "major_points", major_points - 1 );
    }

    if( chosen_slot > 0 ) set_state( chosen_state, chosen_slot );
    set_state( perk_key( perk ), rank + 1 );
    effects_dirty = true;
    recalculate_effects();

    std::string result = rank == 0 ? tr( "Perk purchased: ", "Куплен перк: " ) :
                                     tr( "Perk upgraded: ", "Перк улучшен: " );
    result += russian() ? perk.name_ru : perk.name_en;
    if( max_rank > 1 ) result += " " + std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
    message( result );
    return true;
}
'@
$sp = Replace-CppRange $sp 'bool purchase_perk( const perk_def &perk )' 'void show_perk_detail( const perk_def &perk )' $purchasePrime0915 '0.9.15 Prime purchase/commit'

$detailPrime0915 = @'
void show_perk_detail( const perk_def &perk )
{
    while( true ) {
        const int level = static_cast<int>( perk_progression_level( perk ) );
        const int rank = perk_rank( perk );
        const int max_rank = perk_max_rank( perk );
        const bool maxed = rank >= max_rank;
        const bool unlocked = level >= perk.required_level && prerequisites_met( perk );

        std::string title = perk_display_name( perk );
        title += "\n" + perk_description( perk );
        title += "\n" + tr( "Tier ", "Тир " ) + std::to_string( perk.tier );
        if( integration_perk( perk ) ) {
            title += " | " + tr( "Requires Survivor level ", "Нужен уровень Survivor " ) + std::to_string( perk.required_level );
        } else {
            title += " | " + tr( "Requires branch level ", "Нужен уровень ветки " ) + std::to_string( perk.required_level );
        }
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );
        if( exclusive_specialization_root( perk ) ) {
            title += "\n" + tr( "Prime specialization: choosing it locks the other two choices until a full respec.",
                                  "Прайм-специализация: её выбор закрывает два других варианта до полного сброса." );
        } else if( specialization_perk( perk ) ) {
            title += "\n" + tr( "Part of the chosen Prime specialization.", "Часть выбранной Прайм-специализации." );
        }
        if( integration_perk( perk ) ) {
            title += "\n" + tr( "Requires mod: ", "Нужен мод: " ) + integration_mod_name( perk );
            title += "\n" + tr( "This perk improves abilities or mechanics from that mod.",
                                  "Этот перк усиливает способности или механику этого мода." );
        }

        std::string buy;
        if( maxed ) buy = tr( "[Maximum rank]", "[Максимальный ранг]" );
        else if( !unlocked ) buy = tr( "Locked", "Закрыто" );
        else if( rank > 0 ) buy = tr( "Upgrade to rank ", "Улучшить до ранга " ) +
                                  std::to_string( rank + 1 ) + "/" + std::to_string( max_rank );
        else buy = tr( "Purchase", "Купить" );

        std::string back = tr( "Back", "Назад" );
        const char *entries[] = { buy.c_str(), back.c_str() };
        const int choice = host->ui_choose ? host->ui_choose( title.c_str(), entries, 2 ) : -1;
        if( choice != 0 || maxed ) return;
        if( !unlocked ) {
            message( tr( "This perk is locked.", "Этот перк пока закрыт." ) );
            continue;
        }
        purchase_perk( perk );
        return;
    }
}
'@
$sp = Replace-CppRange $sp 'void show_perk_detail( const perk_def &perk )' 'int branch_unlocked_count(' $detailPrime0915 '0.9.15 Prime perk details'

$perkKindPrime0915 = @'
std::string perk_kind_label( const perk_def &perk )
{
    if( mod_prime_specialization_root( perk ) || specialization_root( perk ) ) {
        return tr( "PRIME", "ПРАЙМ" );
    }
    if( specialization_perk( perk ) ) {
        return tr( "PRIME PERK", "ПРАЙМ-ПЕРК" );
    }
    if( integration_perk( perk ) ) return tr( "MOD PERK", "ПЕРК МОДА" );
    if( perk.currency == currency_id::major ) return tr( "MAJOR PERK", "БОЛЬШОЙ ПЕРК" );
    return tr( "PERK", "ПЕРК" );
}
'@
$sp = Replace-CppRange $sp 'std::string perk_kind_label( const perk_def &perk )' 'std::string branch_icon_key( branch_id branch )' $perkKindPrime0915 '0.9.15 Prime badges'

$integrationPos0915 = @'
std::pair<int, int> integration_tree_position( size_t i )
{
    static const std::array<std::pair<int, int>, 20> layout = {{
        { 0, 2 },
        { 2, 0 }, { 2, 2 }, { 2, 4 },
        { 4, 0 }, { 4, 2 }, { 4, 4 },
        { 6, 0 }, { 6, 2 }, { 6, 4 },
        { 8, 0 }, { 8, 2 }, { 8, 4 },
        { 10, 0 }, { 10, 2 }, { 10, 4 },
        { 12, 0 }, { 12, 2 }, { 12, 4 },
        { 14, 2 }
    }};
    if( i < layout.size() ) return layout[i];
    if( i < 23 ) return { 16, static_cast<int>( i - 20 ) * 2 };
    return { 18 + static_cast<int>( ( i - 23 ) / 3 ) * 2,
             static_cast<int>( ( i - 23 ) % 3 ) * 2 };
}
'@
$sp = Replace-CppRange $sp 'std::pair<int, int> integration_tree_position( size_t i )' 'void show_integration_branch( const std::string &mod_id )' $integrationPos0915 '0.9.15 mod Prime tree row'
$sp = $sp.Replace('        mod_perks.reserve( 20 );','        mod_perks.reserve( 24 );')

# Clear and migrate the seven independent mod-Prime commitments.
$respecPrimeAnchor = '    for( branch_id branch : all_branches ) {' + "`n" + '        set_state( specialization_state_key( branch ), 0 );' + "`n" + '    }' + "`n" + '    effects_dirty = true;'
$respecPrimeNew = @'
    for( branch_id branch : all_branches ) {
        set_state( specialization_state_key( branch ), 0 );
    }
    for( const char *key : {
             "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
             "prime_aftershock_exoplanet", "prime_aftershock_prime",
             "prime_secronom", "prime_secronom_plus"
         } ) {
        set_state( key, 0 );
    }
    effects_dirty = true;
'@
if (-not $sp.Contains($respecPrimeAnchor)) { throw '0.9.15 respec Prime-state anchor missing.' }
$sp = Replace-TextBlock $sp $respecPrimeAnchor $respecPrimeNew '0.9.15 respec mod Prime states'

$migratePrimeAnchor = '    set_state( "schema", state_schema );' + "`n" + '    effects_dirty = true;'
$migratePrimeNew = @'
    for( const char *key : {
             "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
             "prime_aftershock_exoplanet", "prime_aftershock_prime",
             "prime_secronom", "prime_secronom_plus"
         } ) {
        const int64_t selected = get_state( key, 0 );
        set_state( key, selected >= 1 && selected <= 3 ? selected : 0 );
    }
    set_state( "schema", state_schema );
    effects_dirty = true;
'@
if (-not $sp.Contains($migratePrimeAnchor)) { throw '0.9.15 migration schema anchor missing.' }
$sp = Replace-TextBlock $sp $migratePrimeAnchor $migratePrimeNew '0.9.15 migrate mod Prime states'

# API 1.8 active-mod registry will be added below; Survivor consumes it to build
# mod tabs from one generic host registry instead of seven hand-coded activity checks.
if (-not $sp.Contains('    "ui.theme.v1",')) { throw '0.9.15 Survivor UI theme capability anchor missing.' }
$sp = $sp.Replace('    "ui.theme.v1",', '    "ui.theme.v1",' + "`n" + '    "active_mods.registry.v2",')

$activeRegistry0915 = @'
std::vector<std::string> active_supported_integration_mods()
{
    static const std::array<const char *, 7> supported = {{
        "magiclysm", "mindovermatter", "xedra_evolved", "aftershock_exoplanet",
        "aftershock_prime", "secronom", "secronom_lore_expansion"
    }};
    std::set<std::string> active;
    if( host != nullptr && host->world_mod_count != nullptr && host->world_mod_id != nullptr ) {
        const size_t count = std::min<size_t>( host->world_mod_count(), 1024 );
        for( size_t i = 0; i < count; ++i ) {
            const char *id = host->world_mod_id( i );
            if( id != nullptr && id[0] != '\0' ) active.emplace( id );
        }
    }
    std::vector<std::string> result;
    result.reserve( supported.size() );
    for( const char *id : supported ) {
        if( active.count( id ) != 0 || active_world_mod( id ) ) result.emplace_back( id );
    }
    return result;
}

'@
if (-not $sp.Contains('const char *integration_anchor_id( const std::string &mod_id )')) { throw '0.9.15 active integration registry anchor missing.' }
$sp = $sp.Replace('const char *integration_anchor_id( const std::string &mod_id )',$activeRegistry0915 + 'const char *integration_anchor_id( const std::string &mod_id )')

$hardcodedMods0915 = @'
        add_mod_branch( "magiclysm" );
        add_mod_branch( "mindovermatter" );
        add_mod_branch( "xedra_evolved" );
        add_mod_branch( "aftershock_exoplanet" );
        add_mod_branch( "aftershock_prime" );
        add_mod_branch( "secronom" );
        add_mod_branch( "secronom_lore_expansion" );
'@
# Earlier exact C++ transforms normalize $sp to LF. Windows PowerShell here-strings
# remain CRLF, so a raw String.Contains check can fail even though the block is present.
$hardcodedMods0915 = Normalize-Lf $hardcodedMods0915
$registryMods0915 = @'
        for( const std::string &mod_id : active_supported_integration_mods() ) {
            add_mod_branch( mod_id.c_str() );
        }
'@
if (-not $sp.Contains($hardcodedMods0915)) { throw '0.9.15 hardcoded mod-tab list anchor missing.' }
$sp = Replace-TextBlock $sp $hardcodedMods0915 $registryMods0915 '0.9.15 registry-driven mod tabs'

$initPrimeOld = '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_message'
$initPrimeNew = '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->world_mod_count || !api->world_mod_id || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_message'
if (-not $sp.Contains($initPrimeOld)) { throw '0.9.15 registry API init-check anchor missing.' }
$sp = $sp.Replace($initPrimeOld,$initPrimeNew)

# Promote module version after all 0.9.15 transforms.
$sp = $sp.Replace('"0.9.14"','"0.9.15"')
$sp = $sp.Replace('Survivor Progression v0.9.14','Survivor Progression v0.9.15')
$sp = $sp.Replace('Survivor Progression 0.9.14 initialized:','Survivor Progression 0.9.15 initialized:')

$manifest = [IO.File]::ReadAllText($manifestPath)
if (-not $manifest.Contains('"version": "0.9.14"')) { throw '0.9.15 manifest version anchor missing.' }
$manifest = $manifest.Replace('"version": "0.9.14"','"version": "0.9.15"')
if (-not $manifest.Contains('"state_schema": 7')) { throw '0.9.15 manifest state schema 7 anchor missing.' }
$manifest = $manifest.Replace('"state_schema": 7','"state_schema": 8')
if (-not $manifest.Contains('"api_min_minor": 7')) { throw '0.9.15 manifest API 1.7 anchor missing.' }
$manifest = $manifest.Replace('"api_min_minor": 7','"api_min_minor": 8')
if (-not $manifest.Contains('    "ui.theme.v1",')) { throw '0.9.15 manifest UI theme capability anchor missing.' }
$manifest = $manifest.Replace('    "ui.theme.v1",', '    "ui.theme.v1",' + "`n" + '    "active_mods.registry.v2",')
Write-Utf8NoBom $manifestPath $manifest
Write-Utf8NoBom $spPath $sp
Save-SurvivorSourceSnapshot $Snap0915 "0.9.15"

# The host accepts the new modifier IDs without changing the ABI/API tail.
$loader = [IO.File]::ReadAllText($loaderPath)
$modifierAnchor = '    { "craft_speed_pct", { -90.0, 500.0 } }' + "`n" + '};'
$modifierSurfaceV8 = @'
    { "craft_speed_pct", { -90.0, 500.0 } },
    { "mg_spellcraft_flat", { -10.0, 10.0 } },
    { "mom_metaphysics_flat", { -10.0, 10.0 } },
    { "xe_deduction_flat", { -10.0, 10.0 } },
    { "xe_gramarye_flat", { -10.0, 10.0 } },
    { "af_smartgun_flat", { -10.0, 10.0 } },
    { "af_metaphysics_flat", { -10.0, 10.0 } },
    { "mg_mana_max_pct", { -90.0, 500.0 } },
    { "mg_mana_regen_pct", { -90.0, 500.0 } },
    { "xe_mana_max_pct", { -90.0, 500.0 } },
    { "xe_mana_regen_pct", { -90.0, 500.0 } },
    { "mg_spell_cost_pct", { -90.0, 500.0 } },
    { "mom_spell_cost_pct", { -90.0, 500.0 } },
    { "xe_spell_cost_pct", { -90.0, 500.0 } },
    { "af_spell_cost_pct", { -90.0, 500.0 } },
    { "mg_cast_time_pct", { -90.0, 500.0 } },
    { "mom_cast_time_pct", { -90.0, 500.0 } },
    { "xe_cast_time_pct", { -90.0, 500.0 } },
    { "af_cast_time_pct", { -90.0, 500.0 } },
    { "mg_fail_pct", { -100.0, 500.0 } },
    { "mom_fail_pct", { -100.0, 500.0 } },
    { "xe_fail_pct", { -100.0, 500.0 } },
    { "af_fail_pct", { -100.0, 500.0 } },
    { "mg_spell_xp_pct", { -100.0, 500.0 } },
    { "mom_spell_xp_pct", { -100.0, 500.0 } },
    { "xe_spell_xp_pct", { -100.0, 500.0 } },
    { "af_spell_xp_pct", { -100.0, 500.0 } },
    { "mg_spell_power_pct", { -100.0, 500.0 } },
    { "mom_spell_power_pct", { -100.0, 500.0 } },
    { "xe_spell_power_pct", { -100.0, 500.0 } },
    { "af_spell_power_pct", { -100.0, 500.0 } },
    { "mg_range_pct", { -100.0, 500.0 } },
    { "mom_range_pct", { -100.0, 500.0 } },
    { "xe_range_pct", { -100.0, 500.0 } },
    { "af_range_pct", { -100.0, 500.0 } },
    { "mg_aoe_pct", { -100.0, 500.0 } },
    { "mom_aoe_pct", { -100.0, 500.0 } },
    { "xe_aoe_pct", { -100.0, 500.0 } },
    { "af_aoe_pct", { -100.0, 500.0 } },
    { "mg_duration_pct", { -100.0, 500.0 } },
    { "mom_duration_pct", { -100.0, 500.0 } },
    { "xe_duration_pct", { -100.0, 500.0 } },
    { "af_duration_pct", { -100.0, 500.0 } },
    { "afp_smartgun_flat", { -10.0, 10.0 } },
    { "afp_spell_cost_pct", { -90.0, 500.0 } },
    { "afp_cast_time_pct", { -90.0, 500.0 } },
    { "afp_fail_pct", { -100.0, 500.0 } },
    { "afp_spell_xp_pct", { -100.0, 500.0 } },
    { "afp_spell_power_pct", { -100.0, 500.0 } },
    { "afp_range_pct", { -100.0, 500.0 } },
    { "afp_aoe_pct", { -100.0, 500.0 } },
    { "afp_duration_pct", { -100.0, 500.0 } },
    { "sec_damage_pct", { -75.0, 75.0 } },
    { "sec_resist_pct", { -75.0, 75.0 } },
    { "sec_elite_damage_pct", { -75.0, 75.0 } },
    { "sec_elite_resist_pct", { -75.0, 75.0 } },
    { "sec_crimson_damage_pct", { -75.0, 75.0 } },
    { "sec_crimson_resist_pct", { -75.0, 75.0 } },
    { "secx_flesh_craft_flat", { -10.0, 10.0 } },
    { "secx_flesh_combat_flat", { -10.0, 10.0 } },
    { "secx_spell_cost_pct", { -90.0, 500.0 } },
    { "secx_cast_time_pct", { -90.0, 500.0 } },
    { "secx_fail_pct", { -100.0, 500.0 } },
    { "secx_spell_xp_pct", { -100.0, 500.0 } },
    { "secx_spell_power_pct", { -100.0, 500.0 } },
    { "secx_range_pct", { -100.0, 500.0 } },
    { "secx_aoe_pct", { -100.0, 500.0 } },
    { "secx_duration_pct", { -100.0, 500.0 } }
};
'@
$loader = Replace-TextBlock $loader $modifierAnchor $modifierSurfaceV8 "v8 host modifier surface"

# v8.2: modifier reads occur from hot spell/skill paths, while writes happen only
# on load/purchase/respec.  Maintain aggregate totals on write so gameplay_modifier
# is O(log M), not O(number_of_modules * map_lookup) for every spell query.
$eraseCountV82 = ([regex]::Matches($loader,[regex]::Escape('character_modifier_values.erase('))).Count
if ($eraseCountV82 -ne 7) { throw "v8.2 expected exactly seven module-modifier erase sites, found $eraseCountV82" }
$loader = $loader.Replace('character_modifier_values.erase(','erase_module_modifiers(')
$modifierGlobalsOldV82 = 'std::map<std::string, std::map<std::string, double>> character_modifier_values;'
$modifierGlobalsNewV82 = @'
std::map<std::string, std::map<std::string, double>> character_modifier_values;
std::map<std::string, double, std::less<>> character_modifier_totals;

void erase_module_modifiers( const std::string &module_id )
{
    const auto module_it = character_modifier_values.find( module_id );
    if( module_it == character_modifier_values.end() ) {
        return;
    }
    for( const auto &entry : module_it->second ) {
        const auto total_it = character_modifier_totals.find( entry.first );
        if( total_it == character_modifier_totals.end() ) {
            continue;
        }
        total_it->second -= entry.second;
        if( std::abs( total_it->second ) < 1.0e-12 ) {
            character_modifier_totals.erase( total_it );
        }
    }
    character_modifier_values.erase( module_it );
}
'@
$loader = Replace-TextBlock $loader $modifierGlobalsOldV82 $modifierGlobalsNewV82 "v8.2 aggregate modifier globals"
$loader = Replace-Once $loader 'thread_local std::string active_module_id;' `
    ('thread_local std::string active_module_id;' + "`n" + 'thread_local double contextual_metaphysics_bonus = 0.0;') `
    'v8.2 contextual Metaphysics storage'

$loader = Replace-Once $loader `
    'const std::map<std::string, std::pair<double, double>> character_modifier_limits = {' `
    'const std::map<std::string, std::pair<double, double>, std::less<>> character_modifier_limits = {' `
    'v8.2 transparent modifier policy lookup'

$modifierSetV82 = @'
int character_modifier_set( const char *module_id, const char *modifier_id, double value )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        module_modifiers_quarantined( module_id ) ||
        modifier_id == nullptr || !std::isfinite( value ) ) {
        return 0;
    }

    const auto policy = character_modifier_limits.find( modifier_id );
    if( policy == character_modifier_limits.end() ||
        value < policy->second.first || value > policy->second.second ) {
        return 0;
    }

    auto &module_values = character_modifier_values[module_id];
    double &module_value = module_values[modifier_id];
    const double previous_value = module_value;
    module_value = value;

    double &aggregate = character_modifier_totals[modifier_id];
    aggregate += value - previous_value;
    if( std::abs( aggregate ) < 1.0e-12 ) {
        character_modifier_totals.erase( modifier_id );
    }
    return 1;
}

int character_modifier_clear_module( const char *module_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ) {
        return 0;
    }
    erase_module_modifiers( module_id );
    return 1;
}
'@
$loader = Replace-CppRange $loader 'int character_modifier_set( const char *module_id, const char *modifier_id, double value )' 'const ncmm_host_api_v1 api = {' $modifierSetV82 'v8.2 aggregate modifier writes'

$gameplayModifierV82 = @'
double contextual_metaphysics_swap( double value )
{
    const double previous = contextual_metaphysics_bonus;
    contextual_metaphysics_bonus = std::isfinite( value ) ?
                                   std::max( -20.0, std::min( 20.0, value ) ) : 0.0;
    return previous;
}

double contextual_metaphysics_modifier()
{
    return contextual_metaphysics_bonus;
}

double gameplay_modifier( const char *modifier_id )
{
    if( modifier_id == nullptr || character_modifier_limits.find( modifier_id ) == character_modifier_limits.end() ) {
        return 0.0;
    }
    const auto it = character_modifier_totals.find( modifier_id );
    if( it == character_modifier_totals.end() ) {
        return 0.0;
    }
    return std::max( -500.0, std::min( 500.0, it->second ) );
}
'@
$loader = Replace-CppRange $loader 'double gameplay_modifier( const char *modifier_id )' 'std::string settings_menu_label()' $gameplayModifierV82 'v8.2 aggregate modifier reads'
$modifierClearNeedleV82 = '    character_modifier_values.clear();'
$modifierClearCountV82 = ([regex]::Matches($loader,[regex]::Escape($modifierClearNeedleV82))).Count
if ($modifierClearCountV82 -ne 2) {
    throw "v8.2 expected exactly two global modifier clear sites, found $modifierClearCountV82"
}
$loader = $loader.Replace($modifierClearNeedleV82,
    $modifierClearNeedleV82 + "`n" + '    character_modifier_totals.clear();' + "`n" + '    contextual_metaphysics_bonus = 0.0;')

$loaderHeaderPathV82 = Join-Path $NcmmRoot "host_patch\ncmm_loader.h"
$loaderHeaderV82 = [IO.File]::ReadAllText($loaderHeaderPathV82)
$headerContextAnchorV82 = '/** Aggregate runtime gameplay modifier registered by loaded NCMM modules. */'
$headerContextV82 = @'
/** Internal thread-local skill context used by source-scoped spell integrations. */
double contextual_metaphysics_swap( double value );
double contextual_metaphysics_modifier();

/** Aggregate runtime gameplay modifier registered by loaded NCMM modules. */
'@
$loaderHeaderV82 = Replace-TextBlock $loaderHeaderV82 $headerContextAnchorV82 $headerContextV82 'v8.2 contextual Metaphysics declarations'
Write-Utf8NoBom $loaderHeaderPathV82 $loaderHeaderV82
Write-Utf8NoBom $loaderPath $loader


# ---------------------------------------------------------------------------
# World Settings API v2 + Advanced World Settings 0.6.1
# ---------------------------------------------------------------------------
Write-Host "Applying NCMM World Settings API v2 + geographic settings..." -ForegroundColor Cyan
$sdk = [IO.File]::ReadAllText($sdkPath)
if (-not $sdk.Contains('#define NCMM_API_VERSION_MINOR 5u')) {
    throw 'World Settings API v2 expected NCMM API minor 5 before extension.'
}
$sdk = $sdk.Replace('#define NCMM_API_VERSION_MINOR 5u','#define NCMM_API_VERSION_MINOR 6u')
if (-not $sdk.Contains('typedef struct ncmm_host_api_v1 {')) { throw 'World Settings v2 SDK struct anchor missing.' }
if (-not $sdk.Contains('NCMM_WORLD_SETTING_NEW_MAP')) {
    $scopeEnum = @'
typedef enum ncmm_world_setting_scope_v2 {
    NCMM_WORLD_SETTING_LIVE = 0,
    NCMM_WORLD_SETTING_RELOAD = 1,
    NCMM_WORLD_SETTING_NEW_MAP = 2,
    NCMM_WORLD_SETTING_NEW_WORLD = 3
} ncmm_world_setting_scope_v2;

'@
    $sdk = $sdk.Replace('typedef struct ncmm_host_api_v1 {',$scopeEnum + 'typedef struct ncmm_host_api_v1 {')
}

if (-not $sdk.Contains('    "world_settings.v2",')) {
    $sdk = $sdk.Replace('#define NCMM_API_VERSION_MINOR 6u',
        '#define NCMM_API_VERSION_MINOR 6u' + "`n" + '/* capability: world_settings.v2 */')
}

$worldModTail = '    int ( *world_mod_active )( const char *mod_id );'
if (-not $sdk.Contains($worldModTail)) { throw 'World Settings v2 SDK world_mod_active tail missing.' }
$wsApiTail = @'
    int ( *world_mod_active )( const char *mod_id );

    /* NCMM API 1.6: World Settings API v2. Additive ABI-v1 tail. */
    int ( *world_setting_register_bool )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           int default_value, uint32_t scope );
    int ( *world_setting_register_int )( const char *module_id, const char *setting_id,
                                          const char *display_name, const char *tooltip,
                                          int min_value, int max_value, int default_value,
                                          uint32_t scope );
    int ( *world_setting_register_float )( const char *module_id, const char *setting_id,
                                            const char *display_name, const char *tooltip,
                                            double min_value, double max_value,
                                            double default_value, double step,
                                            uint32_t scope );
    int ( *world_setting_register_enum )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           const char *const *value_ids,
                                           const char *const *display_names, size_t count,
                                           const char *default_value, uint32_t scope );
    int ( *world_setting_get_bool )( const char *setting_id, int fallback );
    int64_t ( *world_setting_get_i64 )( const char *setting_id, int64_t fallback );
    double ( *world_setting_get_f64 )( const char *setting_id, double fallback );
    const char *( *world_setting_get_string )( const char *setting_id, const char *fallback );
'@
$sdk = Replace-TextBlock $sdk $worldModTail $wsApiTail 'World Settings v2 SDK API tail'
Write-Utf8NoBom $sdkPath $sdk

$loader = [IO.File]::ReadAllText($loaderPath)
if (-not $loader.Contains('#include <cctype>')) {
    if (-not $loader.Contains('#include <cmath>')) { throw 'World Settings v2 loader cctype include anchor missing.' }
    $loader = $loader.Replace('#include <cmath>','#include <cmath>' + "`n" + '#include <cctype>')
}
if (-not $loader.Contains('    "active_mods.v1",')) { throw 'World Settings v2 loader capability anchor missing.' }
$loader = $loader.Replace('    "active_mods.v1",','    "active_mods.v1",' + "`n" + '    "world_settings.v2",')
$globalsAnchor = 'std::set<std::string> hotkey_registration_logged;'
if (-not $loader.Contains($globalsAnchor)) { throw 'World Settings v2 loader globals anchor missing.' }
$globalsNew = @'
std::set<std::string> hotkey_registration_logged;
std::map<std::string, std::string> world_setting_owners;
std::map<std::string, uint32_t> world_setting_scopes;
std::string world_setting_string_cache;
'@
$loader = Replace-TextBlock $loader $globalsAnchor $globalsNew 'World Settings v2 host globals'

$wsHost = @'
// Defined later in the loader; World Settings v2 is injected before that definition.
bool active_module_matches( const char *module_id );

bool safe_world_setting_id( const char *value )
{
    if( value == nullptr ) {
        return false;
    }
    const std::string text( value );
    if( text.size() < 6 || text.size() > 80 || text.rfind( "NCMM_", 0 ) != 0 ) {
        return false;
    }
    for( unsigned char c : text ) {
        if( !( std::isupper( c ) || std::isdigit( c ) || c == '_' ) ) {
            return false;
        }
    }
    return true;
}

bool claim_world_setting( const char *module_id, const char *setting_id, uint32_t scope )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !safe_world_setting_id( setting_id ) || scope > NCMM_WORLD_SETTING_NEW_WORLD ) {
        return false;
    }
    const std::string id( setting_id );
    const auto owner = world_setting_owners.find( id );
    if( owner != world_setting_owners.end() && owner->second != module_id ) {
        return false;
    }
    world_setting_owners[id] = module_id;
    world_setting_scopes[id] = scope;
    return true;
}

int world_setting_register_bool( const char *module_id, const char *setting_id,
                                 const char *display_name, const char *tooltip,
                                 int default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    return get_options().ncmm_register_world_bool( setting_id, to_translation( display_name ),
            to_translation( tooltip ), default_value != 0 ) ? 1 : 0;
}

int world_setting_register_int( const char *module_id, const char *setting_id,
                                const char *display_name, const char *tooltip,
                                int min_value, int max_value, int default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    return get_options().ncmm_register_world_int( setting_id, to_translation( display_name ),
            to_translation( tooltip ), min_value, max_value, default_value ) ? 1 : 0;
}

int world_setting_register_float( const char *module_id, const char *setting_id,
                                  const char *display_name, const char *tooltip,
                                  double min_value, double max_value, double default_value,
                                  double step, uint32_t scope )
{
    if( !display_name || !tooltip || !std::isfinite( min_value ) || !std::isfinite( max_value ) ||
        !std::isfinite( default_value ) || !std::isfinite( step ) ||
        !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    return get_options().ncmm_register_world_float( setting_id, to_translation( display_name ),
            to_translation( tooltip ), static_cast<float>( min_value ), static_cast<float>( max_value ),
            static_cast<float>( default_value ), static_cast<float>( step ) ) ? 1 : 0;
}

int world_setting_register_enum( const char *module_id, const char *setting_id,
                                 const char *display_name, const char *tooltip,
                                 const char *const *value_ids, const char *const *display_names,
                                 size_t count, const char *default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !value_ids || !display_names || !default_value ||
        count == 0 || count > 64 || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    std::vector<options_manager::id_and_option> items;
    items.reserve( count );
    for( size_t i = 0; i < count; ++i ) {
        if( !value_ids[i] || !display_names[i] ) {
            return 0;
        }
        items.emplace_back( value_ids[i], to_translation( display_names[i] ) );
    }
    return get_options().ncmm_register_world_enum( setting_id, to_translation( display_name ),
            to_translation( tooltip ), items, default_value ) ? 1 : 0;
}

int world_setting_get_bool( const char *setting_id, int fallback )
{
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return fallback;
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "bool" ) {
        return fallback;
    }
    return opt.value_as<bool>() ? 1 : 0;
}

int64_t world_setting_get_i64( const char *setting_id, int64_t fallback )
{
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return fallback;
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "int" && opt.getType() != "int_map" ) {
        return fallback;
    }
    return static_cast<int64_t>( opt.value_as<int>() );
}

double world_setting_get_f64( const char *setting_id, double fallback )
{
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return fallback;
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "float" ) {
        return fallback;
    }
    return static_cast<double>( opt.value_as<float>() );
}

const char *world_setting_get_string( const char *setting_id, const char *fallback )
{
    world_setting_string_cache = fallback ? fallback : "";
    if( !safe_world_setting_id( setting_id ) || !get_options().has_option( setting_id ) ) {
        return world_setting_string_cache.c_str();
    }
    const options_manager::cOpt &opt = get_options().get_option( setting_id );
    if( opt.getType() != "string_select" && opt.getType() != "string_input" && opt.getType() != "string" ) {
        return world_setting_string_cache.c_str();
    }
    world_setting_string_cache = opt.value_as<std::string>();
    return world_setting_string_cache.c_str();
}

'@
$loader = $loader.Replace('int can_expose_worldgen_option( const char *option_id )',$wsHost + 'int can_expose_worldgen_option( const char *option_id )')
$apiTailOld = '    &gameplay_metric_get_i64,' + "`n" + '    &world_mod_active' + "`n" + '};'
$apiTailNew = @'
    &gameplay_metric_get_i64,
    &world_mod_active,
    &world_setting_register_bool,
    &world_setting_register_int,
    &world_setting_register_float,
    &world_setting_register_enum,
    &world_setting_get_bool,
    &world_setting_get_i64,
    &world_setting_get_f64,
    &world_setting_get_string
};
'@
if (-not $loader.Contains($apiTailOld)) { throw 'World Settings v2 host API initializer anchor missing.' }
$loader = $loader.Replace($apiTailOld,$apiTailNew.TrimEnd())
Write-Utf8NoBom $loaderPath $loader

# ---------------------------------------------------------------------------
# NCMM Host API 1.7 / NCMM 0.7.3 — themed UI + experimental world page
# ---------------------------------------------------------------------------
Write-Host "Applying NCMM 0.7.3 UI Theme API + Experimental World page..." -ForegroundColor Cyan
$sdk = [IO.File]::ReadAllText($sdkPath)
if (-not $sdk.Contains('#define NCMM_API_VERSION_MINOR 6u')) {
    throw 'NCMM 0.7.3 expected API 1.6 before UI theme extension.'
}
$sdk = $sdk.Replace('#define NCMM_API_VERSION_MINOR 6u','#define NCMM_API_VERSION_MINOR 7u')

if (-not $sdk.Contains('typedef enum ncmm_ui_color_v1')) {
    $themeTypes = @'
typedef enum ncmm_ui_color_v1 {
    NCMM_UI_COLOR_DEFAULT = 0,
    NCMM_UI_COLOR_RED = 1,
    NCMM_UI_COLOR_GREEN = 2,
    NCMM_UI_COLOR_CYAN = 3,
    NCMM_UI_COLOR_YELLOW = 4,
    NCMM_UI_COLOR_BLUE = 5,
    NCMM_UI_COLOR_MAGENTA = 6
} ncmm_ui_color_v1;

typedef enum ncmm_ui_theme_flags_v1 {
    NCMM_UI_THEME_NONE = 0u,
    NCMM_UI_THEME_STRONG_BORDER = 1u << 0,
    NCMM_UI_THEME_WIDE_NODES = 1u << 1
} ncmm_ui_theme_flags_v1;

typedef struct ncmm_ui_theme_v1 {
    uint32_t accent;
    uint32_t flags;
    int32_t preferred_node_width;
    int32_t preferred_detail_width;
    const uint32_t *item_accents;
    size_t item_accent_count;
} ncmm_ui_theme_v1;

'@
    if (-not $sdk.Contains('typedef struct ncmm_host_api_v1 {')) { throw 'NCMM UI theme SDK host struct anchor missing.' }
    $sdk = $sdk.Replace('typedef struct ncmm_host_api_v1 {',$themeTypes + 'typedef struct ncmm_host_api_v1 {')
}

$wsStringTail = '    const char *( *world_setting_get_string )( const char *setting_id, const char *fallback );'
if (-not $sdk.Contains($wsStringTail)) { throw 'NCMM UI theme SDK World Settings tail anchor missing.' }
$ui17Tail = @'
    const char *( *world_setting_get_string )( const char *setting_id, const char *fallback );

    /* NCMM API 1.7: dedicated experimental world-settings page. */
    int ( *worldgen_experimental_group_begin )( const char *group_id,
                                                 const char *display_name,
                                                 const char *tooltip );

    /* NCMM API 1.7: explicit, scoped UI themes; old card/tree APIs remain unchanged. */
    int ( *ui_card_choose_themed )( const char *title, const char *summary,
                                    const ncmm_ui_progress_v1 *progress,
                                    const ncmm_ui_card_v1 *cards, size_t count,
                                    size_t columns, const ncmm_ui_theme_v1 *theme );
    int ( *ui_tree_choose_themed )( const char *title, const char *summary,
                                    const ncmm_ui_progress_v1 *progress,
                                    const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                    const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                    const ncmm_ui_theme_v1 *theme );
'@
$sdk = Replace-TextBlock $sdk $wsStringTail $ui17Tail 'NCMM 0.7.3 SDK tail'
Write-Utf8NoBom $sdkPath $sdk

# Keep the repository smoke host aligned with API 1.7 as well.  This matters for
# future CI/publishing: aggregate initialization with a shorter tail would compile,
# but Survivor would correctly reject the null themed callbacks at runtime.
$smokePath17 = Join-Path $NcmmRoot 'tests\smoke_host.cpp'
if (Test-Path $smokePath17 -PathType Leaf) {
    $smoke17 = [IO.File]::ReadAllText($smokePath17)
    if (-not $smoke17.Contains('"ui.theme.v1"')) {
        $smoke17 = $smoke17.Replace(
            '           std::strcmp( cap, "active_mods.v1" ) == 0 ||',
            '           std::strcmp( cap, "active_mods.v1" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "world_settings.v2" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "world_options.experimental.v1" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "ui.theme.v1" ) == 0 ||')
        $smoke17 = $smoke17.Replace(
            '"gameplay.metrics.v1", "active_mods.v1",',
            '"gameplay.metrics.v1", "active_mods.v1", "world_settings.v2", "world_options.experimental.v1", "ui.theme.v1",')
    }

    if (-not $smoke17.Contains('ui_card_choose_themed_fn')) {
        $smoke17Stubs = @'
int synthetic_setting_count = 0;

int world_setting_register_bool_fn( const char *, const char *, const char *, const char *, int, uint32_t )
{
    ++synthetic_setting_count;
    return 1;
}
int world_setting_register_int_fn( const char *, const char *, const char *, const char *,
                                   int, int, int, uint32_t )
{
    ++synthetic_setting_count;
    return 1;
}
int world_setting_register_float_fn( const char *, const char *, const char *, const char *,
                                     double, double, double, double, uint32_t )
{
    ++synthetic_setting_count;
    return 1;
}
int world_setting_register_enum_fn( const char *, const char *, const char *, const char *,
                                    const char *const *, const char *const *, size_t,
                                    const char *, uint32_t )
{
    ++synthetic_setting_count;
    return 1;
}
int world_setting_get_bool_fn( const char *, int fallback ) { return fallback; }
int64_t world_setting_get_i64_fn( const char *, int64_t fallback ) { return fallback; }
double world_setting_get_f64_fn( const char *, double fallback ) { return fallback; }
const char *world_setting_get_string_fn( const char *, const char *fallback ) { return fallback; }

int experimental_group_begin_fn( const char *group_id, const char *name, const char *tooltip )
{
    return group_begin_fn( group_id, name, tooltip );
}

int ui_card_choose_themed_fn( const char *title, const char *summary,
                              const ncmm_ui_progress_v1 *progress,
                              const ncmm_ui_card_v1 *cards, size_t count, size_t columns,
                              const ncmm_ui_theme_v1 * )
{
    return ui_card_choose_fn( title, summary, progress, cards, count, columns );
}

int ui_tree_choose_themed_fn( const char *title, const char *summary,
                              const ncmm_ui_progress_v1 *progress,
                              const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                              const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                              const ncmm_ui_theme_v1 * )
{
    return ui_tree_choose_fn( title, summary, progress, nodes, node_count, edges, edge_count );
}

'@
        if (-not $smoke17.Contains('template<typename T>')) { throw 'NCMM API 1.7 smoke stub anchor missing.' }
        $smoke17 = $smoke17.Replace('template<typename T>', $smoke17Stubs + 'template<typename T>')

        $smoke17ApiOld = '        &world_mod_active_fn' + "`n" + '    };'
        $smoke17ApiNew = @'
        &world_mod_active_fn,
        &world_setting_register_bool_fn,
        &world_setting_register_int_fn,
        &world_setting_register_float_fn,
        &world_setting_register_enum_fn,
        &world_setting_get_bool_fn,
        &world_setting_get_i64_fn,
        &world_setting_get_f64_fn,
        &world_setting_get_string_fn,
        &experimental_group_begin_fn,
        &ui_card_choose_themed_fn,
        &ui_tree_choose_themed_fn
    };
'@
        if (-not $smoke17.Contains($smoke17ApiOld)) { throw 'NCMM API 1.7 smoke initializer anchor missing.' }
        $smoke17 = $smoke17.Replace($smoke17ApiOld,$smoke17ApiNew.TrimEnd())
    }

    $awsSmokeOld = @'
        if( actual != expected || groups.size() != 2 ||
            groups[0] != "aws_advanced" || groups[1] != "aws_experimental" ) {
            std::cerr << "AWS grouped registration failed\n";
            return 8;
        }
'@
    $awsSmokeNew = @'
        const std::vector<std::string> expected_groups = {
            "aws_difficulty", "aws_time", "aws_geo_city", "aws_geo_forest",
            "aws_geo_water", "aws_geo_transport"
        };
        if( actual != expected || groups != expected_groups || synthetic_setting_count != 48 ) {
            std::cerr << "AWS vanilla/experimental registration split failed\n";
            return 8;
        }
'@
    $smoke17 = Replace-TextBlock $smoke17 $awsSmokeOld $awsSmokeNew 'NCMM API 1.7 AWS smoke split'
    $smoke17 = $smoke17.Replace('return "0.7.2-smoke";','return "0.7.3-smoke";')
    $smoke17 = $smoke17.Replace('NCMM smoke test: PASS (AWS 0.5.0 grouped controls)',
                                'NCMM smoke test: PASS (AWS 0.6.1 vanilla + Experimental controls)')
    Write-Utf8NoBom $smokePath17 $smoke17
}

$loader = [IO.File]::ReadAllText($loaderPath)
if (-not $loader.Contains('    "world_settings.v2",')) { throw 'NCMM 0.7.3 world settings capability anchor missing.' }
$loader = $loader.Replace('    "world_settings.v2",',
    '    "world_settings.v2",' + "`n" + '    "world_options.experimental.v1",' + "`n" + '    "ui.theme.v1",')

# Keep runtime host metadata synchronized with the new additive API tail.
$loader = $loader.Replace('return "0.7.2";','return "0.7.3";')
$loader = $loader.Replace('"host_version": "0.7.2"','"host_version": "0.7.3"')
# modules.state.json writes a C++ escaped JSON literal, so refresh that spelling too.
$loader = $loader.Replace('\"host_version\": \"0.7.2\"','\"host_version\": \"0.7.3\"')

$themeGlobalsAnchor = 'thread_local std::string active_module_id;'
if (-not $loader.Contains($themeGlobalsAnchor)) { throw 'NCMM UI theme globals anchor missing.' }
$themeGlobals = @'
thread_local std::string active_module_id;
thread_local ncmm_ui_theme_v1 active_ui_theme = {
    NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0
};

uint32_t ncmm_ui_theme_accent_id( size_t item_index = static_cast<size_t>( -1 ) )
{
    if( item_index != static_cast<size_t>( -1 ) && active_ui_theme.item_accents != nullptr &&
        item_index < active_ui_theme.item_accent_count ) {
        const uint32_t item = active_ui_theme.item_accents[item_index];
        if( item <= NCMM_UI_COLOR_MAGENTA ) {
            return item;
        }
    }
    return active_ui_theme.accent;
}

bool ncmm_ui_theme_enabled( size_t item_index = static_cast<size_t>( -1 ) )
{
    return ncmm_ui_theme_accent_id( item_index ) != NCMM_UI_COLOR_DEFAULT;
}

nc_color ncmm_ui_theme_accent( size_t item_index = static_cast<size_t>( -1 ) )
{
    switch( ncmm_ui_theme_accent_id( item_index ) ) {
        case NCMM_UI_COLOR_RED: return c_light_red;
        case NCMM_UI_COLOR_GREEN: return c_light_green;
        case NCMM_UI_COLOR_CYAN: return c_light_cyan;
        case NCMM_UI_COLOR_YELLOW: return c_yellow;
        case NCMM_UI_COLOR_BLUE: return c_light_blue;
        case NCMM_UI_COLOR_MAGENTA: return c_pink;
        default: return c_light_gray;
    }
}

class ncmm_ui_theme_scope
{
    public:
        explicit ncmm_ui_theme_scope( const ncmm_ui_theme_v1 *theme ) : previous_( active_ui_theme )
        {
            active_ui_theme = { NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0 };
            if( theme != nullptr ) {
                active_ui_theme = *theme;
                if( active_ui_theme.accent > NCMM_UI_COLOR_MAGENTA ) {
                    active_ui_theme.accent = NCMM_UI_COLOR_DEFAULT;
                }
                active_ui_theme.flags &= NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES;
            }
        }

        ncmm_ui_theme_scope( const ncmm_ui_theme_scope & ) = delete;
        ncmm_ui_theme_scope &operator=( const ncmm_ui_theme_scope & ) = delete;

        ~ncmm_ui_theme_scope()
        {
            active_ui_theme = previous_;
        }

    private:
        ncmm_ui_theme_v1 previous_;
};
'@
$loader = Replace-TextBlock $loader $themeGlobalsAnchor $themeGlobals 'NCMM 0.7.3 UI theme globals'

# Theme card borders/titles while retaining old semantics for unthemed modules.
$cardStyleOld = @'
            const nc_color border = active ? c_light_green :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta :
                                    accent ? c_light_blue : BORDER_COLOR;
            const nc_color title_color = locked ? c_dark_gray :
                                          active ? c_white : c_light_gray;
'@
$cardStyleNew = @'
            const bool themed = ncmm_ui_theme_enabled( static_cast<size_t>( index ) );
            const nc_color theme_accent = ncmm_ui_theme_accent( static_cast<size_t>( index ) );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    accent ? c_light_blue :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color title_color = locked ? c_dark_gray :
                                          active ? c_white :
                                          themed ? theme_accent : c_light_gray;
'@
$loader = Replace-TextBlock $loader $cardStyleOld $cardStyleNew 'NCMM 0.7.3 themed cards'

$treeGeometryOld = @'
    constexpr int node_width = 24;
    constexpr int node_height = 6;
    constexpr int hgap = 2;
    constexpr int vgap = 1;
    constexpr int header_height = 6;
    constexpr int footer_height = 2;
    constexpr int detail_width = 42;
'@
$treeGeometryNew = @'
    const int requested_node_width = active_ui_theme.preferred_node_width > 0 ?
                                     active_ui_theme.preferred_node_width :
                                     ( ( active_ui_theme.flags & NCMM_UI_THEME_WIDE_NODES ) ? 30 : 24 );
    const int requested_detail_width = active_ui_theme.preferred_detail_width > 0 ?
                                       active_ui_theme.preferred_detail_width : 42;
    int node_width = std::clamp( requested_node_width, 24, 34 );
    constexpr int node_height = 6;
    constexpr int hgap = 2;
    constexpr int vgap = 1;
    constexpr int header_height = 6;
    constexpr int footer_height = 2;
    int detail_width = std::clamp( requested_detail_width, 38, 54 );

    // Preserve every logical tree column before spending horizontal space on
    // cosmetic width.  Large displays get the requested 30-char Survivor
    // nodes; narrower terminals automatically step down to the proven 24-char
    // geometry instead of silently clipping the right-hand branch.
    const auto required_tree_width = [&]( int candidate_node, int candidate_detail ) {
        const int candidate_lane = candidate_node + hgap;
        const int candidate_tree = candidate_node +
                                   ( max_layout_x2 * candidate_lane + 1 ) / 2;
        return candidate_tree + candidate_detail + 7;
    };
    while( detail_width > 38 && required_tree_width( node_width, detail_width ) > TERMX - 2 ) {
        --detail_width;
    }
    while( node_width > 24 && required_tree_width( node_width, detail_width ) > TERMX - 2 ) {
        --node_width;
    }
'@
$loader = Replace-TextBlock $loader $treeGeometryOld $treeGeometryNew 'NCMM 0.7.3 adaptive tree geometry'

$treeStyleOld = @'
            const nc_color border = active ? c_light_green :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = locked ? c_dark_gray :
                                        active ? c_white : c_light_gray;
'@
$treeStyleNew = @'
            const bool themed = ncmm_ui_theme_enabled( i );
            const nc_color theme_accent = ncmm_ui_theme_accent( i );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = locked ? c_dark_gray :
                                        active ? c_white :
                                        themed ? theme_accent : c_light_gray;
'@
$loader = Replace-TextBlock $loader $treeStyleOld $treeStyleNew 'NCMM 0.7.3 themed tree nodes'


# A persistent accent marker makes themed tiles readable even when their dark
# interior matches the game's black background.  Keep the selected marker as
# '>' and use '*' only for non-selected STRONG_BORDER themes.
$cardMarkerOld = @'
            if( active ) {
                mvwprintz( card_win, point( 1, 1 ), c_light_green, ">" );
            }
'@
$cardMarkerNew = @'
            if( active ) {
                mvwprintz( card_win, point( 1, 1 ), themed ? theme_accent : c_light_green, ">" );
            } else if( themed && ( active_ui_theme.flags & NCMM_UI_THEME_STRONG_BORDER ) ) {
                mvwprintz( card_win, point( 1, 1 ), theme_accent, "*" );
            }
'@
$loader = Replace-TextBlock $loader $cardMarkerOld $cardMarkerNew 'NCMM 0.7.3 card contrast marker'

$treeMarkerOld = '            if( active ) mvwprintz( frame, point( x + 1, y + 1 ), c_light_green, ">" );'
$treeMarkerNew = @'
            if( active ) {
                mvwprintz( frame, point( x + 1, y + 1 ), themed ? theme_accent : c_light_green, ">" );
            } else if( themed && ( active_ui_theme.flags & NCMM_UI_THEME_STRONG_BORDER ) ) {
                mvwprintz( frame, point( x + 1, y + 1 ), theme_accent, "*" );
            }
'@
$loader = Replace-TextBlock $loader $treeMarkerOld $treeMarkerNew 'NCMM 0.7.3 tree contrast marker'

# Branch identity should read at the window level as well as on each tile.
# Both card/tree renderers use this exact header call in the pinned host.
$hostTitleOld = '        ncmm_trim_and_print_literal( frame, point( 2, 1 ), frame_width - 4, c_white, title );'
$hostTitleNew = '        ncmm_trim_and_print_literal( frame, point( 2, 1 ), frame_width - 4,' + "`n" +
                '                                    ncmm_ui_theme_enabled() ? ncmm_ui_theme_accent() : c_white, title );'
$hostTitleCount = ([regex]::Matches($loader,[regex]::Escape($hostTitleOld))).Count
if ($hostTitleCount -ne 2) { throw "NCMM 0.7.3 themed window-title anchor expected twice, found $hostTitleCount" }
$loader = $loader.Replace($hostTitleOld,$hostTitleNew)

$detailTitleOld = @'
            ncmm_trim_and_print_literal( frame, point( dx, dy++ ), detail_width - 3,
                                        c_white, detail_title[line] );
'@
$detailTitleNew = @'
            ncmm_trim_and_print_literal( frame, point( dx, dy++ ), detail_width - 3,
                                        ncmm_ui_theme_enabled( static_cast<size_t>( selected ) ) ?
                                        ncmm_ui_theme_accent( static_cast<size_t>( selected ) ) : c_white,
                                        detail_title[line] );
'@
$loader = Replace-TextBlock $loader $detailTitleOld $detailTitleNew 'NCMM 0.7.3 themed detail title'

# The two progress bars (cards/tree) inherit branch accent when a theme is active.
$loader = $loader.Replace(
    'frame_width - 4,' + "`n" + '                                        c_light_green, line );',
    'frame_width - 4,' + "`n" + '                                        ncmm_ui_theme_enabled() ? ncmm_ui_theme_accent() : c_light_green, line );' )

$experimentalHost = @'
int worldgen_experimental_group_begin( const char *group_id, const char *display_name,
                                       const char *tooltip )
{
    if( !group_id || !display_name || !tooltip ) {
        return 0;
    }
    return get_options().ncmm_begin_experimental_group(
               group_id, to_translation( display_name ), to_translation( tooltip ) ) ? 1 : 0;
}

'@
if (-not $loader.Contains('int worldgen_group_begin( const char *group_id, const char *display_name, const char *tooltip )')) {
    throw 'NCMM experimental group host anchor missing.'
}
$loader = $loader.Replace('int worldgen_group_begin( const char *group_id, const char *display_name, const char *tooltip )',
                          $experimentalHost + 'int worldgen_group_begin( const char *group_id, const char *display_name, const char *tooltip )')

$themedWrappers = @'
int ui_card_choose_themed( const char *title, const char *summary,
                           const ncmm_ui_progress_v1 *progress,
                           const ncmm_ui_card_v1 *cards, size_t count, size_t columns,
                           const ncmm_ui_theme_v1 *theme )
{
    ncmm_ui_theme_v1 safe_theme = theme != nullptr ? *theme :
                                  ncmm_ui_theme_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                    0, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, count );
    }
    ncmm_ui_theme_scope scope( &safe_theme );
    return ui_card_choose( title, summary, progress, cards, count, columns );
}

int ui_tree_choose_themed( const char *title, const char *summary,
                           const ncmm_ui_progress_v1 *progress,
                           const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                           const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                           const ncmm_ui_theme_v1 *theme )
{
    ncmm_ui_theme_v1 safe_theme = theme != nullptr ? *theme :
                                  ncmm_ui_theme_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                    0, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, node_count );
    }
    ncmm_ui_theme_scope scope( &safe_theme );
    return ui_tree_choose( title, summary, progress, nodes, node_count, edges, edge_count );
}

'@
if (-not $loader.Contains('void ui_message( const char *message )')) { throw 'NCMM themed UI wrapper anchor missing.' }
$loader = $loader.Replace('void ui_message( const char *message )',$themedWrappers + 'void ui_message( const char *message )')

$api17Old = @'
    &world_setting_get_bool,
    &world_setting_get_i64,
    &world_setting_get_f64,
    &world_setting_get_string
};
'@
$api17New = @'
    &world_setting_get_bool,
    &world_setting_get_i64,
    &world_setting_get_f64,
    &world_setting_get_string,
    &worldgen_experimental_group_begin,
    &ui_card_choose_themed,
    &ui_tree_choose_themed
};
'@
$loader = Replace-TextBlock $loader $api17Old $api17New 'NCMM 0.7.3 host API initializer'
Write-Utf8NoBom $loaderPath $loader

# ---------------------------------------------------------------------------
# NCMM Host API 1.8 / NCMM 0.7.4 — active-mod registry + runtime coherence
# ---------------------------------------------------------------------------
Write-Host "Applying NCMM 0.7.4 Active-Mod Registry API..." -ForegroundColor Cyan
$sdk = [IO.File]::ReadAllText($sdkPath)
if (-not $sdk.Contains('#define NCMM_API_VERSION_MINOR 7u')) { throw 'NCMM 0.7.4 expected API 1.7 before registry extension.' }
$sdk = $sdk.Replace('#define NCMM_API_VERSION_MINOR 7u','#define NCMM_API_VERSION_MINOR 8u')
$themeTail18 = @'
    int ( *ui_tree_choose_themed )( const char *title, const char *summary,
                                    const ncmm_ui_progress_v1 *progress,
                                    const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                    const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                    const ncmm_ui_theme_v1 *theme );
'@
$registryTail18 = @'
    int ( *ui_tree_choose_themed )( const char *title, const char *summary,
                                    const ncmm_ui_progress_v1 *progress,
                                    const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                    const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                    const ncmm_ui_theme_v1 *theme );

    /* NCMM API 1.8: enumerate active world mods through a stable host registry. */
    size_t ( *world_mod_count )();
    const char *( *world_mod_id )( size_t index );
'@
$sdk = Replace-TextBlock $sdk $themeTail18 $registryTail18 'NCMM 0.7.4 SDK active-mod registry tail'
Write-Utf8NoBom $sdkPath $sdk

$loader = [IO.File]::ReadAllText($loaderPath)
if (-not $loader.Contains('    "active_mods.v1",')) { throw 'NCMM 0.7.4 active_mods.v1 capability anchor missing.' }
$loader = $loader.Replace('    "active_mods.v1",','    "active_mods.v1",' + "`n" + '    "active_mods.registry.v2",')
$loader = $loader.Replace('return "0.7.3";','return "0.7.4";')
$loader = $loader.Replace('"host_version": "0.7.3"','"host_version": "0.7.4"')
$loader = $loader.Replace('\"host_version\": \"0.7.3\"','\"host_version\": \"0.7.4\"')

$registryHost18 = @'
size_t world_mod_count()
{
    if( world_generator == nullptr || world_generator->active_world == nullptr ) return 0;
    return world_generator->active_world->active_mod_order.size();
}

const char *world_mod_id( size_t index )
{
    static thread_local std::string id_cache;
    id_cache.clear();
    if( world_generator == nullptr || world_generator->active_world == nullptr ) return nullptr;
    const auto &mods = world_generator->active_world->active_mod_order;
    if( index >= mods.size() ) return nullptr;
    id_cache = mods[index].str();
    return id_cache.c_str();
}

'@
if (-not $loader.Contains('int can_expose_worldgen_option( const char *option_id )')) { throw 'NCMM 0.7.4 registry host insertion anchor missing.' }
$loader = $loader.Replace('int can_expose_worldgen_option( const char *option_id )',$registryHost18 + 'int can_expose_worldgen_option( const char *option_id )')

$api18Old = @'
    &worldgen_experimental_group_begin,
    &ui_card_choose_themed,
    &ui_tree_choose_themed
};
'@
$api18New = @'
    &worldgen_experimental_group_begin,
    &ui_card_choose_themed,
    &ui_tree_choose_themed,
    &world_mod_count,
    &world_mod_id
};
'@
$loader = Replace-TextBlock $loader $api18Old $api18New 'NCMM 0.7.4 host API registry initializer'
Write-Utf8NoBom $loaderPath $loader

# Fix the bootstrap/runtime version drift left by the 1.7 UI pass: the seed runtime
# still says 0.7.2 unless explicitly transformed.  Keep binding/loader/bootstrap coherent.
$runtimeBootstrapPath18 = Join-Path $NcmmRoot 'runtime\NCMMBootstrap.cs'
if (-not (Test-Path $runtimeBootstrapPath18 -PathType Leaf)) { throw 'NCMM 0.7.4 bootstrap source missing.' }
$runtime18 = [IO.File]::ReadAllText($runtimeBootstrapPath18)
if ($runtime18.Contains('private const string RuntimeVersion = "0.7.2";')) {
    $runtime18 = $runtime18.Replace('private const string RuntimeVersion = "0.7.2";','private const string RuntimeVersion = "0.7.4";')
} elseif ($runtime18.Contains('private const string RuntimeVersion = "0.7.3";')) {
    $runtime18 = $runtime18.Replace('private const string RuntimeVersion = "0.7.3";','private const string RuntimeVersion = "0.7.4";')
} elseif (-not $runtime18.Contains('private const string RuntimeVersion = "0.7.4";')) {
    throw 'NCMM 0.7.4 bootstrap runtime version anchor missing.'
}
Write-Utf8NoBom $runtimeBootstrapPath18 $runtime18

# The base host-patch script still carries its historical 0.7.2 marker in the seed.
# Promote every version marker/message in that script to the final 0.7.4 host version
# so an installed source tree never claims a different NCMM version than loader/runtime.
$applyHostPath18 = Join-Path $NcmmRoot 'host_patch\Apply-NCMMHostPatch.ps1'
if (-not (Test-Path $applyHostPath18 -PathType Leaf)) { throw 'NCMM 0.7.4 host patch script missing.' }
$applyHost18 = [IO.File]::ReadAllText($applyHostPath18)
$applyVersionCount18 = ([regex]::Matches($applyHost18,[regex]::Escape('0.7.2'))).Count
if ($applyVersionCount18 -lt 5) {
    throw "NCMM 0.7.4 expected historical 0.7.2 markers in Apply-NCMMHostPatch.ps1; found $applyVersionCount18."
}
$applyHost18 = $applyHost18.Replace('0.7.2','0.7.4')
if ($applyHost18.Contains('0.7.2') -or -not $applyHost18.Contains('NCMM Host API v1 / NCMM 0.7.4 module contract')) {
    throw 'NCMM 0.7.4 host patch marker promotion failed.'
}
Write-Utf8NoBom $applyHostPath18 $applyHost18

# Runtime/bootstrap behavior is part of the installed NCMM contract, so bind it
# into patch_revision as well.  This closes the old gap where a bootstrap-only
# change could ship under an unchanged host patch revision.
$revRuntimePath18 = Join-Path $NcmmRoot 'ci\Get-PatchRevision.ps1'
$revRuntime18 = [IO.File]::ReadAllText($revRuntimePath18)
if (-not $revRuntime18.Contains("'runtime/NCMMBootstrap.cs'")) {
    $revRuntimeAnchor18 = "    'sdk/ncmm_api.h',"
    if (-not $revRuntime18.Contains($revRuntimeAnchor18)) { throw 'NCMM 0.7.4 patch-revision runtime anchor missing.' }
    $revRuntime18 = $revRuntime18.Replace($revRuntimeAnchor18,"    'runtime/NCMMBootstrap.cs'," + "`n" + $revRuntimeAnchor18)
    Write-Utf8NoBom $revRuntimePath18 $revRuntime18
}

# Keep smoke host ABI/API tail complete.  A zero-length registry is enough for the
# generic smoke path; dedicated checks below verify count/id behavior separately.
$smokePath18 = Join-Path $NcmmRoot 'tests\smoke_host.cpp'
if (Test-Path $smokePath18 -PathType Leaf) {
    $smoke18 = [IO.File]::ReadAllText($smokePath18)
    if (-not $smoke18.Contains('"active_mods.registry.v2"')) {
        $smoke18 = $smoke18.Replace(
            '           std::strcmp( cap, "active_mods.v1" ) == 0 ||',
            '           std::strcmp( cap, "active_mods.v1" ) == 0 ||' + "`n" +
            '           std::strcmp( cap, "active_mods.registry.v2" ) == 0 ||')
        $smoke18 = $smoke18.Replace('"gameplay.metrics.v1", "active_mods.v1",',
            '"gameplay.metrics.v1", "active_mods.v1", "active_mods.registry.v2",')
    }
    if (-not $smoke18.Contains('size_t world_mod_count_registry_fn()')) {
        $smokeRegistryStubs18 = @'
size_t world_mod_count_registry_fn()
{
    return 0;
}
const char *world_mod_id_registry_fn( size_t )
{
    return nullptr;
}

'@
        if (-not $smoke18.Contains('template<typename T>')) { throw 'NCMM API 1.8 smoke registry stub anchor missing.' }
        $smoke18 = $smoke18.Replace('template<typename T>',$smokeRegistryStubs18 + 'template<typename T>')
    }
    $smokeApi18Old = @'
        &experimental_group_begin_fn,
        &ui_card_choose_themed_fn,
        &ui_tree_choose_themed_fn
    };
'@
    $smokeApi18New = @'
        &experimental_group_begin_fn,
        &ui_card_choose_themed_fn,
        &ui_tree_choose_themed_fn,
        &world_mod_count_registry_fn,
        &world_mod_id_registry_fn
    };
'@
    # Replace-TextBlock normalizes LF/CRLF and trims the here-string edge.  Do not
    # preflight this block with raw .Contains(): the preceding API 1.7 pass intentionally
    # inserted its initializer with TrimEnd(), so a Windows here-string carries one extra
    # trailing CRLF and produces a false 'anchor missing' on otherwise-correct source.
    $smoke18 = Replace-TextBlock $smoke18 $smokeApi18Old $smokeApi18New 'NCMM API 1.8 smoke initializer'
    $smoke18 = $smoke18.Replace('return "0.7.3-smoke";','return "0.7.4-smoke";')
    $smoke18 = $smoke18.Replace('migrate( &api, 2, 7 )','migrate( &api, 2, 8 )')
    $smoke18 = $smoke18.Replace('character_state[prefix + "schema"] != 7','character_state[prefix + "schema"] != 8')
    $smoke18 = $smoke18.Replace('Survivor Progression 0.9.14 progression/integration slice',
                                'Survivor Progression 0.9.15 Prime/registry slice')
    Write-Utf8NoBom $smokePath18 $smoke18
}

$awsPath = Join-Path $NcmmRoot 'mods\AdvancedWorldSettings\src\aws.cpp'
$awsManifestPath = Join-Path $NcmmRoot 'mods\AdvancedWorldSettings\mod.json'
if (-not (Test-Path $awsPath -PathType Leaf) -or -not (Test-Path $awsManifestPath -PathType Leaf)) {
    throw 'AdvancedWorldSettings source missing from pinned NCMM seed.'
}
$awsSource = @'
#define NCMM_MOD_BUILD
#include "ncmm_api.h"

#include <cstddef>
#include <string>

namespace
{
constexpr const char *module_id = "advanced_world_settings";
const char *required_caps[] = {
    "core.v1", "world_options.v1", "world_options.layout.v1", "world_settings.v2",
    "world_options.experimental.v1", "locale.v1", "module_contract.v1", "api.versioning.v1"
};

struct option_desc {
    const char *id;
    const char *name;
    const char *tooltip;
};

const option_desc options_en[] = {
    { "SPAWN_DENSITY", "Monster spawn density", "Multiplier for monster spawn density. 1.00 is the default." },
    { "ITEM_SPAWNRATE", "Item spawn rate", "Multiplier for generated item quantity. 1.00 is the default." },
    { "MONSTER_SPEED", "Monster speed", "Global monster speed percentage. 100% is the default." },
    { "MONSTER_RESILIENCE", "Monster resilience", "Global monster health percentage. 100% is the default." },
    { "EVOLUTION_INVERSE_MULTIPLIER", "Monster evolution time multiplier", "Higher values slow evolution; 0 disables upgrades where supported." },
    { "SEASON_LENGTH", "Season length (days)", "Requires world reload. Number of days in each season. CDDA default is 91." },
    { "CONSTRUCTION_SCALING", "Construction time scaling", "Percentage of base construction time. 100% is normal; 50% is twice as fast." },
    { "ETERNAL_SEASON", "Eternal season", "Requires world reload. Stops normal season progression." },
    { "ETERNAL_TIME_OF_DAY", "Fixed time of day", "Requires world reload. Normal time flow, permanent day, or permanent night." }
};
const option_desc options_ru[] = {
    { "SPAWN_DENSITY", "Плотность монстров", "Множитель плотности появления монстров. 1,00 — стандарт." },
    { "ITEM_SPAWNRATE", "Количество предметов", "Множитель количества генерируемых предметов. 1,00 — стандарт." },
    { "MONSTER_SPEED", "Скорость монстров", "Глобальная скорость монстров в процентах. 100% — стандарт." },
    { "MONSTER_RESILIENCE", "Живучесть монстров", "Глобальный запас здоровья монстров в процентах. 100% — стандарт." },
    { "EVOLUTION_INVERSE_MULTIPLIER", "Время эволюции монстров", "Большие значения замедляют эволюцию; 0 отключает улучшения там, где это поддерживается." },
    { "SEASON_LENGTH", "Длина сезона (дни)", "Нужна перезагрузка мира. Количество дней в сезоне. Стандарт CDDA — 91." },
    { "CONSTRUCTION_SCALING", "Время строительства", "Процент от базового времени. 100% — стандарт; 50% — вдвое быстрее." },
    { "ETERNAL_SEASON", "Вечный сезон", "Нужна перезагрузка мира. Останавливает обычную смену сезонов." },
    { "ETERNAL_TIME_OF_DAY", "Фиксированное время суток", "Нужна перезагрузка мира. Обычный цикл, постоянный день или постоянная ночь." }
};

bool russian( const ncmm_host_api_v1 *api ) {
    const char *raw = api && api->get_locale ? api->get_locale() : "en";
    const std::string locale = raw ? raw : "en";
    return locale == "ru" || locale.rfind( "ru_", 0 ) == 0;
}
const char *tr( bool ru, const char *en, const char *ru_text ) { return ru ? ru_text : en; }

bool expose_range( const ncmm_host_api_v1 *api, const option_desc *options, std::size_t begin,
                   std::size_t end, const char *group_id, const char *group_name,
                   const char *group_tooltip ) {
    if( !api->worldgen_group_begin( group_id, group_name, group_tooltip ) ) return false;
    bool ok = true;
    for( std::size_t i = begin; i < end; ++i ) {
        if( !api->expose_worldgen_option( options[i].id, options[i].name, options[i].tooltip ) ) {
            ok = false; break;
        }
    }
    api->worldgen_group_end();
    return ok;
}

bool reg_bool( const ncmm_host_api_v1 *api, bool ru, const char *id, const char *en,
               const char *ru_name, const char *en_tip, const char *ru_tip, bool def ) {
    return api->world_setting_register_bool( module_id, id, tr( ru, en, ru_name ),
            tr( ru, en_tip, ru_tip ), def ? 1 : 0, NCMM_WORLD_SETTING_NEW_MAP ) != 0;
}
bool reg_int( const ncmm_host_api_v1 *api, bool ru, const char *id, const char *en,
              const char *ru_name, const char *en_tip, const char *ru_tip,
              int minv, int maxv, int def ) {
    return api->world_setting_register_int( module_id, id, tr( ru, en, ru_name ),
            tr( ru, en_tip, ru_tip ), minv, maxv, def, NCMM_WORLD_SETTING_NEW_MAP ) != 0;
}
bool reg_float( const ncmm_host_api_v1 *api, bool ru, const char *id, const char *en,
                const char *ru_name, const char *en_tip, const char *ru_tip,
                double minv, double maxv, double def, double step ) {
    return api->world_setting_register_float( module_id, id, tr( ru, en, ru_name ),
            tr( ru, en_tip, ru_tip ), minv, maxv, def, step, NCMM_WORLD_SETTING_NEW_MAP ) != 0;
}

bool geography( const ncmm_host_api_v1 *api, bool ru ) {
    bool ok = true;
    auto begin = [&]( const char *id, const char *en, const char *ru_name, const char *en_tip, const char *ru_tip ) {
        return api->worldgen_experimental_group_begin( id, tr( ru, en, ru_name ),
                tr( ru, en_tip, ru_tip ) ) != 0;
    };
    auto end = [&]() { api->worldgen_group_end(); };

    if( !begin( "aws_geo_city", "Cities and infrastructure", "Города и инфраструктура",
                "Affects only areas generated after this change.",
                "Влияет только на новые области, созданные после изменения." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_CUSTOM_GEOGRAPHY","Use custom geography","Использовать свою географию","Leave this off to use the world's normal geography. Turn it on to customize areas generated from now on.","Оставьте выключенным для обычной географии мира. Включите, чтобы настраивать области, которые будут созданы после изменения.",false );
    ok &= reg_int( api,ru,"NCMM_AWS_CITY_SIZE","Base city size","Базовый размер города","0 disables random cities; default 8.","0 отключает случайные города; стандарт 8.",0,32,8 );
    ok &= reg_int( api,ru,"NCMM_AWS_CITY_SPACING","City spacing","Расстояние между городами","Higher values produce fewer cities; default 4.","Чем выше значение, тем реже города; стандарт 4.",0,8,4 );
    ok &= reg_int( api,ru,"NCMM_AWS_MAX_URBANITY","Maximum city growth","Максимальный рост городов","Limits how strongly regional generation can enlarge cities; default 8.","Ограничивает, насколько сильно региональные настройки могут увеличивать города; стандарт 8.",1,16,8 );
    ok &= reg_bool( api,ru,"NCMM_AWS_MEGACITY","Megacity generation","Мегаполис","Generates new areas as a dense megacity. This can noticeably increase generation time.","Новые области генерируются как плотный мегаполис. Это может заметно увеличить время генерации.",false );
    ok &= reg_int( api,ru,"NCMM_AWS_SHOP_RADIUS","Shop radius","Радиус магазинов","Controls how far from the city center shops may appear. Larger values spread shops farther out; 0 prevents shops from being placed by this rule. CDDA 0546 default is 30.","Определяет, насколько далеко от центра города могут появляться магазины. Чем выше значение, тем дальше они распространяются; 0 запрещает размещение магазинов по этому правилу. Стандарт CDDA 0546 — 30.",0,200,30 );
    ok &= reg_int( api,ru,"NCMM_AWS_SHOP_SIGMA","Shop spread","Разброс магазинов","Controls how widely shops are scattered around the city center. CDDA 0546 default is 50.","Определяет, насколько широко магазины распределяются вокруг центра города. Стандарт CDDA 0546 — 50.",0,200,50 );
    ok &= reg_int( api,ru,"NCMM_AWS_PARK_RADIUS","Park radius","Радиус парков","Controls how far from the city center parks may appear. Larger values spread parks farther out; 0 prevents parks from being placed by this rule. CDDA 0546 default is 20.","Определяет, насколько далеко от центра города могут появляться парки. Чем выше значение, тем дальше они распространяются; 0 запрещает размещение парков по этому правилу. Стандарт CDDA 0546 — 20.",0,200,20 );
    ok &= reg_int( api,ru,"NCMM_AWS_PARK_SIGMA","Park spread","Разброс парков","Controls how widely parks are scattered around the city center. CDDA 0546 default is 80.","Определяет, насколько широко парки распределяются вокруг центра города. Стандарт CDDA 0546 — 80.",0,200,80 );
    ok &= reg_bool( api,ru,"NCMM_AWS_PLACE_ROADS","Generate roads","Генерировать дороги","Disables new inter-city roads when off.","Отключает новые межгородские дороги.",true );
    ok &= reg_bool( api,ru,"NCMM_AWS_PLACE_RAILROADS","Generate railroads","Генерировать железные дороги","Disables new railroads when off.","Отключает новые железные дороги.",true );
    ok &= reg_bool( api,ru,"NCMM_AWS_PLACE_SPECIALS","Generate special locations","Генерировать особые локации","Controls placement of new special locations.","Управляет размещением новых особых локаций.",true );
    ok &= reg_bool( api,ru,"NCMM_AWS_NEIGHBOR_CONNECTIONS","Connect neighboring map regions","Связывать соседние области карты","Keeps roads, rail lines and rivers continuous across map-region borders.","Сохраняет непрерывность дорог, железных дорог и рек между областями карты.",true );
    end(); if( !ok ) return false;

    if( !begin( "aws_geo_forest", "Forests, swamps and trails", "Леса, болота и тропы",
                "Lower threshold values generate more of the selected terrain in new areas.",
                "Чем ниже порог, тем больше соответствующего ландшафта появится в новых областях." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_FORESTS","Generate forests","Генерировать леса","Turns forest generation on or off in new areas.","Включает или отключает леса в новых областях.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_FOREST_THRESHOLD","Forest threshold","Порог леса","Lower = more forest. CDDA 0546 default is 0.20.","Ниже = больше леса. Стандарт CDDA 0546 — 0,20.",0.0,1.0,0.20,0.01 );
    ok &= reg_float( api,ru,"NCMM_AWS_FOREST_THICK_THRESHOLD","Dense forest threshold","Порог густого леса","Lower = more dense forest. CDDA 0546 default is 0.25.","Ниже = больше густого леса. Стандарт CDDA 0546 — 0,25.",0.0,1.0,0.25,0.01 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_SWAMPS","Generate swamps","Генерировать болота","Turns swamp generation on or off in new areas.","Включает или отключает болота в новых областях.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_SWAMP_ADJ_THRESHOLD","Floodplain swamp threshold","Порог пойменных болот","Lower = more river-adjacent swamp. Default 0.30.","Ниже = больше болот у рек. Стандарт 0,30.",0.0,1.0,0.30,0.01 );
    ok &= reg_float( api,ru,"NCMM_AWS_SWAMP_ISOLATED_THRESHOLD","Isolated swamp threshold","Порог изолированных болот","Lower = more isolated swamp. Default 0.60.","Ниже = больше отдельных болот. Стандарт 0,60.",0.0,1.0,0.60,0.01 );
    ok &= reg_int( api,ru,"NCMM_AWS_FLOODPLAIN_MIN","Floodplain radius minimum","Минимальный радиус поймы","Minimum river floodplain buffer. Default 3.","Минимальный буфер поймы реки. Стандарт 3.",0,30,3 );
    ok &= reg_int( api,ru,"NCMM_AWS_FLOODPLAIN_MAX","Floodplain radius maximum","Максимальный радиус поймы","Maximum river floodplain buffer. Default 15.","Максимальный буфер поймы реки. Стандарт 15.",0,60,15 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_TRAILS","Generate forest trails","Генерировать лесные тропы","Turns forest trails and trailheads on or off in new areas.","Включает или отключает лесные тропы и выходы к дорогам в новых областях.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAIL_CHANCE","Forest trail chance (1 in X)","Шанс лесной тропы (1 из X)","1 means every qualifying forest; CDDA 0546 default is 2.","1 означает каждый подходящий лес; стандарт CDDA 0546 — 2.",1,32,2 );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAIL_MIN_FOREST","Minimum forest size for trails","Минимальный лес для троп","Minimum contiguous forest tiles; CDDA 0546 default is 100.","Минимальный размер связного леса; стандарт CDDA 0546 — 100.",1,1000,100 );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAILHEAD_CHANCE","Trailhead chance (1 in X)","Шанс входа на тропу (1 из X)","1 means every eligible trail end; default 1.","1 означает каждый подходящий конец тропы; стандарт 1.",1,32,1 );
    ok &= reg_int( api,ru,"NCMM_AWS_TRAILHEAD_ROAD_DISTANCE","Trailhead road distance","Дистанция тропы до дороги","Maximum road-search radius for a trailhead; default 6.","Радиус поиска дороги для входа на тропу; стандарт 6.",1,30,6 );
    end(); if( !ok ) return false;

    if( !begin( "aws_geo_water", "Rivers, lakes and oceans", "Реки, озёра и океаны",
                "Controls rivers, lakes and oceans in newly generated areas.",
                "Настройки рек, озёр и океанов в новых областях." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_RIVERS","Generate rivers","Генерировать реки","Turns river generation on or off in new areas.","Включает или отключает реки в новых областях.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_RIVER_SCALE","River width scale","Масштаб ширины рек","0 disables rivers; default region value is 1.","0 отключает реки; стандарт региона 1.",0,5,1 );
    ok &= reg_float( api,ru,"NCMM_AWS_RIVER_FREQUENCY","River frequency","Частота рек","Higher = fewer new major rivers. Default 1.5.","Выше = меньше новых крупных рек. Стандарт 1,5.",1.0,8.0,1.5,0.1 );
    ok &= reg_int( api,ru,"NCMM_AWS_RIVER_BRANCH_CHANCE","River branch chance (1 in X)","Ветвление рек (1 из X)","Lower = more branches. Default 64.","Ниже = больше ответвлений. Стандарт 64.",1,256,64 );
    ok &= reg_int( api,ru,"NCMM_AWS_RIVER_REMERGE_CHANCE","Branch merging chance (1 in X)","Слияние рукавов (1 из X)","Lower = river branches merge back more often. Default 2.","Ниже = рукава рек чаще сливаются обратно. Стандарт 2.",1,64,2 );
    ok &= reg_float( api,ru,"NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE","Branch narrowing","Сужение рукавов","How much narrower each new river branch becomes. Default 1.0.","Насколько уже становится каждый новый рукав. Стандарт 1,0.",0.0,5.0,1.0,0.25 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_LAKES","Generate lakes","Генерировать озёра","Turns lake generation on or off in new areas.","Включает или отключает озёра в новых областях.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_LAKE_THRESHOLD","Lake threshold","Порог озёр","Lower = more lake terrain. Default 0.25.","Ниже = больше озёр. Стандарт 0,25.",0.0,1.0,0.25,0.01 );
    ok &= reg_int( api,ru,"NCMM_AWS_LAKE_MIN_SIZE","Minimum lake size","Минимальный размер озера","Lakes smaller than this are not generated. Default 20.","Озёра меньше этого размера не генерируются. Стандарт 20.",1,1000,20 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_OCEANS","Generate oceans","Генерировать океаны","Turns ocean generation on or off where the current region supports it.","Включает или отключает океаны там, где текущий регион их поддерживает.",true );
    ok &= reg_float( api,ru,"NCMM_AWS_OCEAN_THRESHOLD","Ocean threshold","Порог океана","Lower = ocean expands more easily where coastline generation is active. Default 0.25.","Ниже = океан легче расширяется там, где активна береговая генерация. Стандарт 0,25.",0.0,1.0,0.25,0.01 );
    ok &= reg_int( api,ru,"NCMM_AWS_OCEAN_MIN_SIZE","Minimum ocean body size","Минимальный размер океана","Ocean areas smaller than this are not generated. Default 100.","Океанские области меньше этого размера не генерируются. Стандарт 100.",1,5000,100 );
    end(); if( !ok ) return false;

    if( !begin( "aws_geo_transport", "Highways and ravines", "Шоссе и овраги",
                "Controls highways and ravines in newly generated areas.",
                "Настройки шоссе и оврагов в новых областях." ) ) return false;
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_HIGHWAYS","Generate highways","Генерировать шоссе","Turns highway generation on or off in new areas.","Включает или отключает шоссе в новых областях.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_HIGHWAY_GRID_ROW","Highway row separation","Расстояние между горизонтальными шоссе","Distance between highway rows, measured in map regions. Default 8.","Расстояние между рядами шоссе в областях карты. Стандарт 8.",2,32,8 );
    ok &= reg_int( api,ru,"NCMM_AWS_HIGHWAY_GRID_COLUMN","Highway column separation","Расстояние между вертикальными шоссе","Distance between highway columns, measured in map regions. Default 10.","Расстояние между колоннами шоссе в областях карты. Стандарт 10.",2,32,10 );
    ok &= reg_int( api,ru,"NCMM_AWS_HIGHWAY_GRID_VARIANCE","Highway alignment variation","Разброс линий шоссе","How far highway intersections may shift from the grid. Default 2.","Насколько перекрёстки могут смещаться относительно сетки. Стандарт 2.",0,7,2 );
    ok &= reg_float( api,ru,"NCMM_AWS_HIGHWAY_STRAIGHTNESS","Highway straightness chance","Прямолинейность шоссе","Chance for new highway endpoints to align. Default 0.60.","Шанс выравнивания новых участков шоссе. Стандарт 0,60.",0.0,1.0,0.60,0.05 );
    ok &= reg_bool( api,ru,"NCMM_AWS_ENABLE_RAVINES","Generate ravines","Генерировать овраги","Turns ravines on or off where the current region supports them.","Включает или отключает овраги там, где текущий регион их поддерживает.",true );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_COUNT","Ravines per map region","Оврагов на область карты","0 disables ravines. Default region value is 0.","0 отключает овраги. В стандартном регионе по умолчанию 0.",0,16,0 );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_RANGE","Ravine length range","Длина оврага","Path displacement range. Default 45.","Диапазон смещения пути. Стандарт 45.",1,120,45 );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_WIDTH","Ravine width","Ширина оврага","Ravine width control. CDDA 0546 default is 3.","Управление шириной оврага. Стандарт CDDA 0546 — 3.",1,10,3 );
    ok &= reg_int( api,ru,"NCMM_AWS_RAVINE_DEPTH","Ravine depth Z-level","Глубина оврага по Z","Negative Z-level for ravine floor. Default -3.","Отрицательный Z-уровень дна оврага. Стандарт -3.",-20,-1,-3 );
    end();
    return ok;
}

int expose_all( const ncmm_host_api_v1 *api, bool log_errors ) {
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || !api->has_capability ||
        !api->has_capability( "world_settings.v2" ) ||
        !api->has_capability( "world_options.experimental.v1" ) ||
        !api->can_expose_worldgen_option || !api->expose_worldgen_option ||
        !api->worldgen_group_begin || !api->worldgen_experimental_group_begin ||
        !api->worldgen_group_end || !api->worldgen_set_string_choices ||
        !api->world_setting_register_bool ||
        !api->world_setting_register_int || !api->world_setting_register_float ) return 0;
    const option_desc *options = russian( api ) ? options_ru : options_en;
    const std::size_t count = sizeof( options_en ) / sizeof( options_en[0] );
    for( std::size_t i = 0; i < count; ++i ) {
        if( !api->can_expose_worldgen_option( options[i].id ) ) {
            if( log_errors ) api->log( NCMM_LOG_ERROR, "AWS: vanilla option preflight failed." );
            return 0;
        }
    }
    const char *time_ids[] = { "normal", "day", "night" };
    const char *time_en[] = { "Normal", "Day", "Night" };
    const char *time_ru[] = { "Обычное", "День", "Ночь" };
    if( !api->worldgen_set_string_choices( "ETERNAL_TIME_OF_DAY", time_ids,
            russian( api ) ? time_ru : time_en, 3 ) ) return 0;
    const bool ru = russian( api );
    if( !expose_range( api, options, 0, 5, "aws_difficulty",
            tr(ru,"Difficulty and population","Сложность и население"),
            tr(ru,"Independent controls that replace the single coarse difficulty preset.","Независимые настройки вместо одного грубого пресета сложности.") ) ) return 0;
    if( !expose_range( api, options, 5, count, "aws_time",
            tr(ru,"Time and calendar","Время и календарь"),
            tr(ru,"Calendar controls. Some require a world reload.","Настройки календаря. Некоторые требуют перезагрузки мира.") ) ) return 0;
    if( !geography( api, ru ) ) {
        if( log_errors ) api->log( NCMM_LOG_ERROR, "AWS: geography registration failed." );
        return 0;
    }
    return 1;
}

int init( const ncmm_host_api_v1 *api ) {
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) return 0;
    for( const char *capability : required_caps ) {
        if( !api->has_capability || !api->has_capability( capability ) ) return 0;
    }
    if( api->get_api_version_major && api->get_api_version_minor ) {
        if( api->get_api_version_major() != 1 || api->get_api_version_minor() < 7 ) return 0;
    }
    if( !expose_all( api, true ) ) return 0;
    api->log( NCMM_LOG_INFO, "Advanced World Settings 0.6.1 initialized: vanilla controls + experimental geography page active." );
    return 1;
}
void shutdown() {}

const ncmm_mod_descriptor_v1 descriptor = {
    NCMM_ABI_VERSION, module_id, "Advanced World Settings", "0.6.1",
    required_caps, sizeof( required_caps ) / sizeof( required_caps[0] ), &init, &shutdown
};
}

extern "C" NCMM_EXPORT const ncmm_mod_descriptor_v1 *ncmm_get_descriptor_v1() { return &descriptor; }
extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1( const ncmm_host_api_v1 *api ) { expose_all( api, false ); }
'@
Write-Utf8NoBom $awsPath $awsSource
$awsManifest = @'
{
  "id": "advanced_world_settings",
  "name": "Advanced World Settings",
  "version": "0.6.1",
  "loader_api": 1,
  "api_major": 1,
  "api_min_minor": 7,
  "requires": [
    "core.v1",
    "world_options.v1",
    "world_options.layout.v1",
    "world_settings.v2",
    "world_options.experimental.v1",
    "locale.v1",
    "module_contract.v1",
    "api.versioning.v1"
  ],
  "failure_policy": "disable"
}
'@
Write-Utf8NoBom $awsManifestPath $awsManifest

$worldSettingsContractPath = Join-Path $NcmmRoot 'compat\world_settings_v2_geography.contract.txt'
$worldSettingsDefinition = (Get-Command Apply-WorldSettingsV2Patch -CommandType Function).Definition
Write-Utf8NoBom $worldSettingsContractPath ("NCMM World Settings API v2 + full geography hooks for CDDA 0546`n" + $worldSettingsDefinition + "`n")
$revPathWS = Join-Path $NcmmRoot 'ci\Get-PatchRevision.ps1'
$revTextWS = [IO.File]::ReadAllText($revPathWS)
if (-not $revTextWS.Contains('compat/world_settings_v2_geography.contract.txt')) {
    $revAnchorWS = "    'host_patch/Apply-NCMMHostPatch.ps1',"
    if (-not $revTextWS.Contains($revAnchorWS)) { throw 'World Settings v2 patch revision anchor missing.' }
    $revTextWS = $revTextWS.Replace($revAnchorWS,$revAnchorWS + "`n    'compat/world_settings_v2_geography.contract.txt',")
    Write-Utf8NoBom $revPathWS $revTextWS
}

# Bind the v8 engine-hook implementation into the NCMM patch revision.  The
# hook is executed by this cumulative installer, but its exact function body is
# stored in the working repository and hashed alongside the normal host patch.
$mechanicsContractPath = Join-Path $NcmmRoot "compat\survivor_mod_mechanics_v82.contract.txt"
$mechanicsDefinition = (Get-Command Apply-NcmmRuntimeGameplayHooksV2 -CommandType Function).Definition
$mechanicsDefinition += "`n" + (Get-Command Apply-NcmmReactiveMechanics0112 -CommandType Function).Definition
$mechanicsDefinition += "`n" + (Get-Command Apply-NcmmReactiveMechanics0113 -CommandType Function).Definition
Write-Utf8NoBom $mechanicsContractPath ("NCMM Host API 2.0 generic runtime gameplay hooks; Survivor bindings live in module DLL`n" + $mechanicsDefinition + "`n")
$revPathV8 = Join-Path $NcmmRoot "ci\Get-PatchRevision.ps1"
$revTextV8 = [IO.File]::ReadAllText($revPathV8)
if (-not $revTextV8.Contains('compat/survivor_mod_mechanics_v82.contract.txt')) {
    $revAnchorV8 = "    'host_patch/Apply-NCMMHostPatch.ps1',"
    if (-not $revTextV8.Contains($revAnchorV8)) { throw 'v8 patch-revision anchor not found.' }
    $revTextV8 = $revTextV8.Replace($revAnchorV8, $revAnchorV8 + "`n    'compat/survivor_mod_mechanics_v82.contract.txt',")
    Write-Utf8NoBom $revPathV8 $revTextV8
}

# Final source root mirrors what will be compiled and what GitHub should receive.
Copy-Item $spPath (Join-Path $NcmmRoot "mods\SurvivorProgression\src\survivor_progression.cpp") -Force
Copy-Item $manifestPath (Join-Path $NcmmRoot "mods\SurvivorProgression\mod.json") -Force
Save-SurvivorSourceSnapshot $Snap0914 "0.9.14"
# v8 adds an engine-hook source contract that the older snapshot helper did not
# know about.  Preserve it explicitly with the exact patch-revision inputs.
New-Item -ItemType Directory -Force (Join-Path $Snap0914 "compat") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Snap0914 "ci") | Out-Null
Copy-Item $mechanicsContractPath (Join-Path $Snap0914 "compat\survivor_mod_mechanics_v82.contract.txt") -Force
Copy-Item $revPathV8 (Join-Path $Snap0914 "ci\Get-PatchRevision.ps1") -Force
Copy-Item (Join-Path $NcmmRoot "host_patch\Apply-NCMMHostPatch.ps1") (Join-Path $Snap0914 "host_patch\Apply-NCMMHostPatch.ps1") -Force
Copy-Item (Join-Path $NcmmRoot "host_patch\ncmm_loader.h") (Join-Path $Snap0914 "host_patch\ncmm_loader.h") -Force
New-Item -ItemType Directory -Force (Join-Path $Snap0914 "mods\AdvancedWorldSettings\src") | Out-Null
Copy-Item $worldSettingsContractPath (Join-Path $Snap0914 "compat\world_settings_v2_geography.contract.txt") -Force
Copy-Item $awsPath (Join-Path $Snap0914 "mods\AdvancedWorldSettings\src\aws.cpp") -Force
Copy-Item $awsManifestPath (Join-Path $Snap0914 "mods\AdvancedWorldSettings\mod.json") -Force


# ---------------------------------------------------------------------------
# v8.7 — RPG Visual Pass / UI host layout extension (additive on top of API 1.8)
# ---------------------------------------------------------------------------
Write-Host "Applying v8.7 RPG Visual Pass + UI host layout extensions..." -ForegroundColor Cyan

$sdk = [IO.File]::ReadAllText($sdkPath)
if (-not $sdk.Contains('NCMM_UI_THEME_HORIZONTAL_VIEWPORT')) {
    $flagsOld87 = @'
typedef enum ncmm_ui_theme_flags_v1 {
    NCMM_UI_THEME_NONE = 0u,
    NCMM_UI_THEME_STRONG_BORDER = 1u << 0,
    NCMM_UI_THEME_WIDE_NODES = 1u << 1
} ncmm_ui_theme_flags_v1;
'@
    $flagsNew87 = @'
typedef enum ncmm_ui_theme_flags_v1 {
    NCMM_UI_THEME_NONE = 0u,
    NCMM_UI_THEME_STRONG_BORDER = 1u << 0,
    NCMM_UI_THEME_WIDE_NODES = 1u << 1,
    NCMM_UI_THEME_HORIZONTAL_VIEWPORT = 1u << 2,
    NCMM_UI_THEME_SECTIONED_DETAIL = 1u << 3
} ncmm_ui_theme_flags_v1;
'@
    $sdk = Replace-TextBlock $sdk $flagsOld87 $flagsNew87 'v8.7 UI theme flags'
}
if (-not $sdk.Contains('typedef enum ncmm_ui_border_style_v1')) {
    $themeStructOld87 = @'
typedef struct ncmm_ui_theme_v1 {
    uint32_t accent;
    uint32_t flags;
    int32_t preferred_node_width;
    int32_t preferred_detail_width;
    const uint32_t *item_accents;
    size_t item_accent_count;
} ncmm_ui_theme_v1;
'@
    $themeStructNew87 = @'
typedef struct ncmm_ui_theme_v1 {
    uint32_t accent;
    uint32_t flags;
    int32_t preferred_node_width;
    int32_t preferred_detail_width;
    const uint32_t *item_accents;
    size_t item_accent_count;
} ncmm_ui_theme_v1;

typedef enum ncmm_ui_border_style_v1 {
    NCMM_UI_BORDER_AUTO = 0u,
    NCMM_UI_BORDER_NORMAL = 1u,
    NCMM_UI_BORDER_MAJOR = 2u,
    NCMM_UI_BORDER_PRIME = 3u,
    NCMM_UI_BORDER_EXCLUDED = 4u
} ncmm_ui_border_style_v1;

typedef struct ncmm_ui_theme_ex_v1 {
    uint32_t accent;
    uint32_t flags;
    int32_t preferred_node_width;
    int32_t preferred_detail_width;
    const uint32_t *item_accents;
    size_t item_accent_count;
    const uint32_t *item_border_styles;
    size_t item_border_style_count;
} ncmm_ui_theme_ex_v1;
'@
    $sdk = Replace-TextBlock $sdk $themeStructOld87 $themeStructNew87 'v8.7 UI theme ex types'
}
if (-not $sdk.Contains('ui_tree_choose_rpg')) {
    $registryTailOld87 = @'
    /* NCMM API 1.8: enumerate active world mods through a stable host registry. */
    size_t ( *world_mod_count )();
    const char *( *world_mod_id )( size_t index );
'@
    $registryTailNew87 = @'
    /* NCMM API 1.8: enumerate active world mods through a stable host registry. */
    size_t ( *world_mod_count )();
    const char *( *world_mod_id )( size_t index );

    /* v8.7 additive UI host extension: per-node border styles, horizontal tree viewport,
       and structured/sectioned detail panes for RPG-like progression screens. */
    int ( *ui_card_choose_rpg )( const char *title, const char *summary,
                                 const ncmm_ui_progress_v1 *progress,
                                 const ncmm_ui_card_v1 *cards, size_t count,
                                 size_t columns, const ncmm_ui_theme_ex_v1 *theme );
    int ( *ui_tree_choose_rpg )( const char *title, const char *summary,
                                 const ncmm_ui_progress_v1 *progress,
                                 const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                 const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                 const ncmm_ui_theme_ex_v1 *theme );
'@
    $sdk = Replace-TextBlock $sdk $registryTailOld87 $registryTailNew87 'v8.7 SDK RPG UI tail'
}
Write-Utf8NoBom $sdkPath $sdk

$loader = [IO.File]::ReadAllText($loaderPath)
if (-not $loader.Contains('"ui.layout.v1"')) {
    if (-not $loader.Contains('    "ui.theme.v1",')) { throw 'v8.7 UI layout capability anchor missing.' }
    $loader = $loader.Replace('    "ui.theme.v1",','    "ui.theme.v1",' + "`n" + '    "ui.layout.v1",')
}

if ($loader.Contains('thread_local ncmm_ui_theme_v1 active_ui_theme = {') -and -not $loader.Contains('thread_local const uint32_t *active_ui_border_styles')) {
    $themeGlobalsOld87 = @'
thread_local std::string active_module_id;
thread_local ncmm_ui_theme_v1 active_ui_theme = {
    NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0
};

uint32_t ncmm_ui_theme_accent_id( size_t item_index = static_cast<size_t>( -1 ) )
{
    if( item_index != static_cast<size_t>( -1 ) && active_ui_theme.item_accents != nullptr &&
        item_index < active_ui_theme.item_accent_count ) {
        const uint32_t item = active_ui_theme.item_accents[item_index];
        if( item <= NCMM_UI_COLOR_MAGENTA ) {
            return item;
        }
    }
    return active_ui_theme.accent;
}

bool ncmm_ui_theme_enabled( size_t item_index = static_cast<size_t>( -1 ) )
{
    return ncmm_ui_theme_accent_id( item_index ) != NCMM_UI_COLOR_DEFAULT;
}

nc_color ncmm_ui_theme_accent( size_t item_index = static_cast<size_t>( -1 ) )
{
    switch( ncmm_ui_theme_accent_id( item_index ) ) {
        case NCMM_UI_COLOR_RED: return c_light_red;
        case NCMM_UI_COLOR_GREEN: return c_light_green;
        case NCMM_UI_COLOR_CYAN: return c_light_cyan;
        case NCMM_UI_COLOR_YELLOW: return c_yellow;
        case NCMM_UI_COLOR_BLUE: return c_light_blue;
        case NCMM_UI_COLOR_MAGENTA: return c_pink;
        default: return c_light_gray;
    }
}

class ncmm_ui_theme_scope
{
    public:
        explicit ncmm_ui_theme_scope( const ncmm_ui_theme_v1 *theme ) : previous_( active_ui_theme )
        {
            active_ui_theme = { NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0 };
            if( theme != nullptr ) {
                active_ui_theme = *theme;
                if( active_ui_theme.accent > NCMM_UI_COLOR_MAGENTA ) {
                    active_ui_theme.accent = NCMM_UI_COLOR_DEFAULT;
                }
                active_ui_theme.flags &= NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES;
            }
        }

        ncmm_ui_theme_scope( const ncmm_ui_theme_scope & ) = delete;
        ncmm_ui_theme_scope &operator=( const ncmm_ui_theme_scope & ) = delete;

        ~ncmm_ui_theme_scope()
        {
            active_ui_theme = previous_;
        }

    private:
        ncmm_ui_theme_v1 previous_;
};
'@
    $themeGlobalsNew87 = @'
thread_local std::string active_module_id;
thread_local ncmm_ui_theme_v1 active_ui_theme = {
    NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0
};
thread_local const uint32_t *active_ui_border_styles = nullptr;
thread_local size_t active_ui_border_style_count = 0;
thread_local uint32_t active_ui_layout_flags = NCMM_UI_THEME_NONE;

uint32_t ncmm_ui_theme_accent_id( size_t item_index = static_cast<size_t>( -1 ) )
{
    if( item_index != static_cast<size_t>( -1 ) && active_ui_theme.item_accents != nullptr &&
        item_index < active_ui_theme.item_accent_count ) {
        const uint32_t item = active_ui_theme.item_accents[item_index];
        if( item <= NCMM_UI_COLOR_MAGENTA ) {
            return item;
        }
    }
    return active_ui_theme.accent;
}

uint32_t ncmm_ui_border_style_id( size_t item_index = static_cast<size_t>( -1 ) )
{
    if( item_index != static_cast<size_t>( -1 ) && active_ui_border_styles != nullptr &&
        item_index < active_ui_border_style_count ) {
        const uint32_t item = active_ui_border_styles[item_index];
        if( item <= NCMM_UI_BORDER_EXCLUDED ) {
            return item;
        }
    }
    return NCMM_UI_BORDER_AUTO;
}

bool ncmm_ui_theme_enabled( size_t item_index = static_cast<size_t>( -1 ) )
{
    return ncmm_ui_theme_accent_id( item_index ) != NCMM_UI_COLOR_DEFAULT;
}

bool ncmm_ui_horizontal_viewport()
{
    return ( active_ui_layout_flags & NCMM_UI_THEME_HORIZONTAL_VIEWPORT ) != 0u;
}

bool ncmm_ui_sectioned_detail()
{
    return ( active_ui_layout_flags & NCMM_UI_THEME_SECTIONED_DETAIL ) != 0u;
}

nc_color ncmm_ui_theme_accent( size_t item_index = static_cast<size_t>( -1 ) )
{
    switch( ncmm_ui_theme_accent_id( item_index ) ) {
        case NCMM_UI_COLOR_RED: return c_light_red;
        case NCMM_UI_COLOR_GREEN: return c_light_green;
        case NCMM_UI_COLOR_CYAN: return c_light_cyan;
        case NCMM_UI_COLOR_YELLOW: return c_yellow;
        case NCMM_UI_COLOR_BLUE: return c_light_blue;
        case NCMM_UI_COLOR_MAGENTA: return c_pink;
        default: return c_light_gray;
    }
}

class ncmm_ui_theme_scope
{
    public:
        explicit ncmm_ui_theme_scope( const ncmm_ui_theme_v1 *theme ) : previous_( active_ui_theme ),
            previous_border_( active_ui_border_styles ), previous_border_count_( active_ui_border_style_count ),
            previous_layout_flags_( active_ui_layout_flags )
        {
            active_ui_theme = { NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0 };
            active_ui_border_styles = nullptr;
            active_ui_border_style_count = 0;
            active_ui_layout_flags = NCMM_UI_THEME_NONE;
            if( theme != nullptr ) {
                active_ui_theme = *theme;
                if( active_ui_theme.accent > NCMM_UI_COLOR_MAGENTA ) {
                    active_ui_theme.accent = NCMM_UI_COLOR_DEFAULT;
                }
                active_ui_theme.flags &= NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                                         NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL;
                active_ui_layout_flags = active_ui_theme.flags;
            }
        }

        ncmm_ui_theme_scope( const ncmm_ui_theme_scope & ) = delete;
        ncmm_ui_theme_scope &operator=( const ncmm_ui_theme_scope & ) = delete;

        ~ncmm_ui_theme_scope()
        {
            active_ui_theme = previous_;
            active_ui_border_styles = previous_border_;
            active_ui_border_style_count = previous_border_count_;
            active_ui_layout_flags = previous_layout_flags_;
        }

    private:
        ncmm_ui_theme_v1 previous_;
        const uint32_t *previous_border_;
        size_t previous_border_count_;
        uint32_t previous_layout_flags_;
};

class ncmm_ui_rpg_theme_scope
{
    public:
        explicit ncmm_ui_rpg_theme_scope( const ncmm_ui_theme_ex_v1 *theme ) : previous_( active_ui_theme ),
            previous_border_( active_ui_border_styles ), previous_border_count_( active_ui_border_style_count ),
            previous_layout_flags_( active_ui_layout_flags )
        {
            active_ui_theme = { NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE, 0, 0, nullptr, 0 };
            active_ui_border_styles = nullptr;
            active_ui_border_style_count = 0;
            active_ui_layout_flags = NCMM_UI_THEME_NONE;
            if( theme != nullptr ) {
                active_ui_theme = { theme->accent, theme->flags, theme->preferred_node_width,
                                    theme->preferred_detail_width, theme->item_accents,
                                    theme->item_accent_count };
                if( active_ui_theme.accent > NCMM_UI_COLOR_MAGENTA ) {
                    active_ui_theme.accent = NCMM_UI_COLOR_DEFAULT;
                }
                active_ui_theme.flags &= NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                                         NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL;
                active_ui_layout_flags = active_ui_theme.flags;
                active_ui_border_styles = theme->item_border_styles;
                active_ui_border_style_count = theme->item_border_style_count;
            }
        }

        ncmm_ui_rpg_theme_scope( const ncmm_ui_rpg_theme_scope & ) = delete;
        ncmm_ui_rpg_theme_scope &operator=( const ncmm_ui_rpg_theme_scope & ) = delete;

        ~ncmm_ui_rpg_theme_scope()
        {
            active_ui_theme = previous_;
            active_ui_border_styles = previous_border_;
            active_ui_border_style_count = previous_border_count_;
            active_ui_layout_flags = previous_layout_flags_;
        }

    private:
        ncmm_ui_theme_v1 previous_;
        const uint32_t *previous_border_;
        size_t previous_border_count_;
        uint32_t previous_layout_flags_;
};
'@
    $loader = Replace-TextBlock $loader $themeGlobalsOld87 $themeGlobalsNew87 'v8.7 RPG theme globals'
}

$cardStyleOld87 = @'
            const bool themed = ncmm_ui_theme_enabled( static_cast<size_t>( index ) );
            const nc_color theme_accent = ncmm_ui_theme_accent( static_cast<size_t>( index ) );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    accent ? c_light_blue :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color title_color = locked ? c_dark_gray :
                                          active ? c_white :
                                          themed ? theme_accent : c_light_gray;
'@
$cardStyleNew87 = @'
            const bool themed = ncmm_ui_theme_enabled( static_cast<size_t>( index ) );
            const uint32_t border_style = ncmm_ui_border_style_id( static_cast<size_t>( index ) );
            const nc_color theme_accent = ncmm_ui_theme_accent( static_cast<size_t>( index ) );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                    border_style == NCMM_UI_BORDER_MAJOR ? c_yellow :
                                    border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    accent ? c_light_blue :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color title_color = locked || border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                          active ? c_white :
                                          border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                          themed ? theme_accent : c_light_gray;
'@
$loader = Replace-TextBlock $loader $cardStyleOld87 $cardStyleNew87 'v8.7 card border styles'

$treeStyleOld87 = @'
            const bool themed = ncmm_ui_theme_enabled( i );
            const nc_color theme_accent = ncmm_ui_theme_accent( i );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = locked ? c_dark_gray :
                                        active ? c_white :
                                        themed ? theme_accent : c_light_gray;
'@
$treeStyleNew87 = @'
            const bool themed = ncmm_ui_theme_enabled( i );
            const uint32_t border_style = ncmm_ui_border_style_id( i );
            const nc_color theme_accent = ncmm_ui_theme_accent( i );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                    border_style == NCMM_UI_BORDER_MAJOR ? c_yellow :
                                    border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = locked || border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                        active ? c_white :
                                        border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                        themed ? theme_accent : c_light_gray;
'@
$loader = Replace-TextBlock $loader $treeStyleOld87 $treeStyleNew87 'v8.7 tree border styles'

$cardMarkerOld87 = @'
            if( active ) {
                mvwprintz( card_win, point( 1, 1 ), themed ? theme_accent : c_light_green, ">" );
            } else if( themed && ( active_ui_theme.flags & NCMM_UI_THEME_STRONG_BORDER ) ) {
                mvwprintz( card_win, point( 1, 1 ), theme_accent, "*" );
            }
'@
$cardMarkerNew87 = @'
            const char *marker = active ? ">" :
                                 border_style == NCMM_UI_BORDER_PRIME ? "*" :
                                 border_style == NCMM_UI_BORDER_MAJOR ? "+" :
                                 border_style == NCMM_UI_BORDER_EXCLUDED ? "x" :
                                 ( themed && ( active_ui_layout_flags & NCMM_UI_THEME_STRONG_BORDER ) ? "*" : nullptr );
            if( marker != nullptr ) {
                mvwprintz( card_win, point( 1, 1 ),
                           border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                           ( themed ? theme_accent : c_light_green ), marker );
            }
'@
$loader = Replace-TextBlock $loader $cardMarkerOld87 $cardMarkerNew87 'v8.7 card border markers'

$treeMarkerOld87 = @'
            if( active ) {
                mvwprintz( frame, point( x + 1, y + 1 ), themed ? theme_accent : c_light_green, ">" );
            } else if( themed && ( active_ui_theme.flags & NCMM_UI_THEME_STRONG_BORDER ) ) {
                mvwprintz( frame, point( x + 1, y + 1 ), theme_accent, "*" );
            }
'@
$treeMarkerNew87 = @'
            const char *marker = active ? ">" :
                                 border_style == NCMM_UI_BORDER_PRIME ? "*" :
                                 border_style == NCMM_UI_BORDER_MAJOR ? "+" :
                                 border_style == NCMM_UI_BORDER_EXCLUDED ? "x" :
                                 ( themed && ( active_ui_layout_flags & NCMM_UI_THEME_STRONG_BORDER ) ? "*" : nullptr );
            if( marker != nullptr ) {
                mvwprintz( frame, point( x + 1, y + 1 ),
                           border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                           ( themed ? theme_accent : c_light_green ), marker );
            }
'@
$loader = Replace-TextBlock $loader $treeMarkerOld87 $treeMarkerNew87 'v8.7 tree border markers'

if (-not $loader.Contains('int ui_tree_choose_rpg(')) {
    $rpgWrappers87 = @'
int ui_card_choose_rpg( const char *title, const char *summary,
                        const ncmm_ui_progress_v1 *progress,
                        const ncmm_ui_card_v1 *cards, size_t count, size_t columns,
                        const ncmm_ui_theme_ex_v1 *theme )
{
    ncmm_ui_theme_ex_v1 safe_theme = theme != nullptr ? *theme :
                                     ncmm_ui_theme_ex_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                          0, 0, nullptr, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, count );
    }
    if( safe_theme.item_border_styles == nullptr ) {
        safe_theme.item_border_style_count = 0;
    } else {
        safe_theme.item_border_style_count = std::min( safe_theme.item_border_style_count, count );
    }
    ncmm_ui_rpg_theme_scope scope( &safe_theme );
    return ui_card_choose( title, summary, progress, cards, count, columns );
}

int ui_tree_choose_rpg( const char *title, const char *summary,
                        const ncmm_ui_progress_v1 *progress,
                        const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                        const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                        const ncmm_ui_theme_ex_v1 *theme )
{
    ncmm_ui_theme_ex_v1 safe_theme = theme != nullptr ? *theme :
                                     ncmm_ui_theme_ex_v1{ NCMM_UI_COLOR_DEFAULT, NCMM_UI_THEME_NONE,
                                                          0, 0, nullptr, 0, nullptr, 0 };
    if( safe_theme.item_accents == nullptr ) {
        safe_theme.item_accent_count = 0;
    } else {
        safe_theme.item_accent_count = std::min( safe_theme.item_accent_count, node_count );
    }
    if( safe_theme.item_border_styles == nullptr ) {
        safe_theme.item_border_style_count = 0;
    } else {
        safe_theme.item_border_style_count = std::min( safe_theme.item_border_style_count, node_count );
    }
    ncmm_ui_rpg_theme_scope scope( &safe_theme );
    return ui_tree_choose( title, summary, progress, nodes, node_count, edges, edge_count );
}

'@
    if (-not $loader.Contains('void ui_message( const char *message )')) { throw 'v8.7 RPG wrapper anchor missing.' }
    $loader = $loader.Replace('void ui_message( const char *message )',$rpgWrappers87 + 'void ui_message( const char *message )')
}

if (-not $loader.Contains('&ui_tree_choose_rpg')) {
    $api18Old87 = @'
    &worldgen_experimental_group_begin,
    &ui_card_choose_themed,
    &ui_tree_choose_themed,
    &world_mod_count,
    &world_mod_id
};
'@
    $api18New87 = @'
    &worldgen_experimental_group_begin,
    &ui_card_choose_themed,
    &ui_tree_choose_themed,
    &world_mod_count,
    &world_mod_id,
    &ui_card_choose_rpg,
    &ui_tree_choose_rpg
};
'@
    $loader = Replace-TextBlock $loader $api18Old87 $api18New87 'v8.7 host API RPG initializer'
}
Write-Utf8NoBom $loaderPath $loader

$sp = [IO.File]::ReadAllText($spPath)
if (-not $sp.Contains('ui.layout.v1')) {
    if (-not $sp.Contains('    "active_mods.registry.v2",')) { throw 'v8.7 Survivor registry capability anchor missing.' }
    $sp = $sp.Replace('    "active_mods.registry.v2",', '    "active_mods.registry.v2",' + "`n" + '    "ui.layout.v1",')
}
if (-not $sp.Contains('rpg_border_style_for')) {
    $uiThemeHelpersOld87 = @'
uint32_t branch_theme_color( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return NCMM_UI_COLOR_RED;
        case branch_id::survival: return NCMM_UI_COLOR_GREEN;
        case branch_id::mobility: return NCMM_UI_COLOR_CYAN;
        case branch_id::crafting: return NCMM_UI_COLOR_YELLOW;
        case branch_id::scavenging: return NCMM_UI_COLOR_BLUE;
        case branch_id::mastery: return NCMM_UI_COLOR_MAGENTA;
    }
    return NCMM_UI_COLOR_DEFAULT;
}

uint32_t integration_theme_color( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "mindovermatter" ) return NCMM_UI_COLOR_CYAN;
    if( mod_id == "xedra_evolved" ) return NCMM_UI_COLOR_GREEN;
    if( mod_id == "aftershock_exoplanet" ) return NCMM_UI_COLOR_BLUE;
    if( mod_id == "aftershock_prime" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "secronom" ) return NCMM_UI_COLOR_RED;
    if( mod_id == "secronom_lore_expansion" ) return NCMM_UI_COLOR_YELLOW;
    return NCMM_UI_COLOR_DEFAULT;
}

ncmm_ui_theme_v1 branch_ui_theme( branch_id branch )
{
    return { branch_theme_color( branch ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES,
             30, 46, nullptr, 0 };
}

ncmm_ui_theme_v1 integration_ui_theme( const std::string &mod_id )
{
    return { integration_theme_color( mod_id ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES,
             30, 46, nullptr, 0 };
}
'@
    $uiThemeHelpersNew87 = @'
uint32_t branch_theme_color( branch_id branch )
{
    switch( branch ) {
        case branch_id::combat: return NCMM_UI_COLOR_RED;
        case branch_id::survival: return NCMM_UI_COLOR_GREEN;
        case branch_id::mobility: return NCMM_UI_COLOR_CYAN;
        case branch_id::crafting: return NCMM_UI_COLOR_YELLOW;
        case branch_id::scavenging: return NCMM_UI_COLOR_BLUE;
        case branch_id::mastery: return NCMM_UI_COLOR_MAGENTA;
    }
    return NCMM_UI_COLOR_DEFAULT;
}

uint32_t integration_theme_color( const std::string &mod_id )
{
    if( mod_id == "magiclysm" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "mindovermatter" ) return NCMM_UI_COLOR_CYAN;
    if( mod_id == "xedra_evolved" ) return NCMM_UI_COLOR_GREEN;
    if( mod_id == "aftershock_exoplanet" ) return NCMM_UI_COLOR_BLUE;
    if( mod_id == "aftershock_prime" ) return NCMM_UI_COLOR_MAGENTA;
    if( mod_id == "secronom" ) return NCMM_UI_COLOR_RED;
    if( mod_id == "secronom_lore_expansion" ) return NCMM_UI_COLOR_YELLOW;
    return NCMM_UI_COLOR_DEFAULT;
}

bool prime_visual_perk( const perk_def &perk )
{
    const std::string id = perk.id ? perk.id : "";
    const std::string name = perk.name_en ? perk.name_en : "";
    return id.rfind( "spc_", 0 ) == 0 || name.find( "Prime" ) != std::string::npos;
}

uint32_t rpg_border_style_for( const perk_def &perk )
{
    if( prime_visual_perk( perk ) ) {
        return NCMM_UI_BORDER_PRIME;
    }
    if( perk.currency == currency_id::major ) {
        return NCMM_UI_BORDER_MAJOR;
    }
    return NCMM_UI_BORDER_NORMAL;
}

std::string rpg_detail_body( const perk_def &perk, const std::string &body,
                             const std::string &requires_text )
{
    std::string result;
    result += tr( "BONUS:", "БОНУС:" );
    result += "\n";
    result += body;
    result += "\n\n";
    result += tr( "REQUIRES:", "ТРЕБУЕТ:" );
    result += "\n";
    result += requires_text;
    if( prime_visual_perk( perk ) ) {
        result += "\n\n";
        result += tr( "DRAWBACK:", "ШТРАФ:" );
        result += "\n";
        result += body;
    }
    return result;
}

ncmm_ui_theme_v1 branch_ui_theme( branch_id branch )
{
    return { branch_theme_color( branch ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
             NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
             26, 42, nullptr, 0 };
}

ncmm_ui_theme_v1 integration_ui_theme( const std::string &mod_id )
{
    return { integration_theme_color( mod_id ),
             NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
             NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
             26, 42, nullptr, 0 };
}
'@
    $sp = Replace-TextBlock $sp $uiThemeHelpersOld87 $uiThemeHelpersNew87 'v8.7 Survivor RPG UI helpers'
}

$coreTreeNodeOld87 = @'
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( perk );
                const int tree_max_rank = perk_max_rank( perk );
                const bool tree_unlocked = level >= perk.required_level && prerequisites_met( perk );
                node.card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                                     tr( "Lv ", "ур. " ) + std::to_string( perk.required_level ) +
                                     " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                node.card.badge = compact_tree_badge( perk, tree_unlocked,
                                                       perk_points, major_points, false );
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( perk );
'@
$coreTreeNodeNew87 = @'
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( perk );
                const int tree_max_rank = perk_max_rank( perk );
                const bool tree_unlocked = level >= perk.required_level && prerequisites_met( perk );
                node.card.subtitle = "T" + std::to_string( perk.tier ) + " | " +
                                     tr( "Lv ", "ур. " ) + std::to_string( perk.required_level ) +
                                     " | " + ( perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                if( prime_visual_perk( perk ) ) {
                    node.card.title = "★ " + node.card.title;
                }
                node.card.badge = compact_tree_badge( perk, tree_unlocked,
                                                       perk_points, major_points, false );
                node.card.body = rpg_detail_body( perk, node.card.body, prereq_text( perk ) );
'@
$sp = Replace-TextBlock $sp $coreTreeNodeOld87 $coreTreeNodeNew87 'v8.7 core tree detail sections'

$modTreeNodeOld87 = @'
                const perk_def &tree_perk = *mod_perks[i];
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( tree_perk );
                const int tree_max_rank = perk_max_rank( tree_perk );
                const bool tree_unlocked = survivor_level >= tree_perk.required_level &&
                                           prerequisites_met( tree_perk );
                node.card.subtitle = tr( "Lv ", "ур. " ) + std::to_string( tree_perk.required_level ) +
                                     " | " + ( tree_perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                node.card.badge = compact_tree_badge( tree_perk, tree_unlocked,
                                                       perk_points, major_points, true );
                node.card.body += "\n" + tr( "Prerequisites: ", "Требования: " ) +
                                  prereq_text( tree_perk );
'@
$modTreeNodeNew87 = @'
                const perk_def &tree_perk = *mod_perks[i];
                tree_node_text node;
                node.card = texts[i];
                const int tree_rank = perk_rank( tree_perk );
                const int tree_max_rank = perk_max_rank( tree_perk );
                const bool tree_unlocked = survivor_level >= tree_perk.required_level &&
                                           prerequisites_met( tree_perk );
                node.card.subtitle = tr( "Lv ", "ур. " ) + std::to_string( tree_perk.required_level ) +
                                     " | " + ( tree_perk.currency == currency_id::perk ? "1P" : "1M" );
                if( tree_max_rank > 1 && tree_rank > 0 ) {
                    node.card.subtitle += " | R" + std::to_string( tree_rank ) + "/" +
                                          std::to_string( tree_max_rank );
                }
                if( prime_visual_perk( tree_perk ) ) {
                    node.card.title = "★ " + node.card.title;
                }
                node.card.badge = compact_tree_badge( tree_perk, tree_unlocked,
                                                       perk_points, major_points, true );
                node.card.body = rpg_detail_body( tree_perk, node.card.body, prereq_text( tree_perk ) );
'@
$sp = Replace-TextBlock $sp $modTreeNodeOld87 $modTreeNodeNew87 'v8.7 mod tree detail sections'

$coreTreeCallOld87 = @'
            const ncmm_ui_theme_v1 theme = branch_ui_theme( branch );
            const int choice = host->ui_tree_choose_themed(
                                   title.c_str(), tree_summary.c_str(), &progress,
                                   nodes.data(), nodes.size(), edges.data(), edges.size(), &theme );
'@
$coreTreeCallNew87 = @'
            std::vector<uint32_t> rpg_item_accents;
            std::vector<uint32_t> rpg_item_borders;
            rpg_item_accents.reserve( branch_perks.size() );
            rpg_item_borders.reserve( branch_perks.size() );
            for( const perk_def *visual_perk : branch_perks ) {
                rpg_item_accents.push_back( branch_theme_color( branch ) );
                rpg_item_borders.push_back( visual_perk ? rpg_border_style_for( *visual_perk ) : NCMM_UI_BORDER_NORMAL );
            }
            const ncmm_ui_theme_ex_v1 theme{
                branch_theme_color( branch ),
                NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
                26, 42, rpg_item_accents.data(), rpg_item_accents.size(),
                rpg_item_borders.data(), rpg_item_borders.size()
            };
            const int choice = host->ui_tree_choose_rpg ?
                                   host->ui_tree_choose_rpg( title.c_str(), tree_summary.c_str(), &progress,
                                                             nodes.data(), nodes.size(), edges.data(), edges.size(), &theme ) :
                                   host->ui_tree_choose_themed( title.c_str(), tree_summary.c_str(), &progress,
                                                                nodes.data(), nodes.size(), edges.data(), edges.size(),
                                                                reinterpret_cast<const ncmm_ui_theme_v1 *>( &theme ) );
'@
$sp = Replace-TextBlock $sp $coreTreeCallOld87 $coreTreeCallNew87 'v8.7 core tree RPG call'
if ($sp.Contains('for( const perk_def &visual_perk : branch_perks )')) {
    throw 'v8.7.3 core RPG visual loop regressed to perk_def reference over pointer container.'
}
if (-not $sp.Contains('for( const perk_def *visual_perk : branch_perks )') -or
    -not $sp.Contains('rpg_border_style_for( *visual_perk )')) {
    throw 'v8.7.3 core RPG visual pointer-loop contract missing.'
}

$modTreeCallOld87 = @'
            const ncmm_ui_theme_v1 theme = integration_ui_theme( mod_id );
            const int choice = host->ui_tree_choose_themed( title.c_str(), summary.c_str(), &progress,
                               nodes.data(), nodes.size(), edges.data(), edges.size(), &theme );
'@
$modTreeCallNew87 = @'
            std::vector<uint32_t> rpg_item_accents;
            std::vector<uint32_t> rpg_item_borders;
            rpg_item_accents.reserve( mod_perks.size() );
            rpg_item_borders.reserve( mod_perks.size() );
            for( const perk_def *visual_perk : mod_perks ) {
                rpg_item_accents.push_back( integration_theme_color( mod_id ) );
                rpg_item_borders.push_back( visual_perk ? rpg_border_style_for( *visual_perk ) : NCMM_UI_BORDER_NORMAL );
            }
            const ncmm_ui_theme_ex_v1 theme{
                integration_theme_color( mod_id ),
                NCMM_UI_THEME_STRONG_BORDER | NCMM_UI_THEME_WIDE_NODES |
                NCMM_UI_THEME_HORIZONTAL_VIEWPORT | NCMM_UI_THEME_SECTIONED_DETAIL,
                26, 42, rpg_item_accents.data(), rpg_item_accents.size(),
                rpg_item_borders.data(), rpg_item_borders.size()
            };
            const int choice = host->ui_tree_choose_rpg ?
                               host->ui_tree_choose_rpg( title.c_str(), summary.c_str(), &progress,
                                                         nodes.data(), nodes.size(), edges.data(), edges.size(), &theme ) :
                               host->ui_tree_choose_themed( title.c_str(), summary.c_str(), &progress,
                                                            nodes.data(), nodes.size(), edges.data(), edges.size(),
                                                            reinterpret_cast<const ncmm_ui_theme_v1 *>( &theme ) );
'@
$sp = Replace-TextBlock $sp $modTreeCallOld87 $modTreeCallNew87 'v8.7 mod tree RPG call'

$initPrimeNeedle87 = '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->world_mod_count || !api->world_mod_id || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_message'
$initPrimeNew87 = '!api->gameplay_metric_get_i64 || !api->world_mod_active || !api->world_mod_count || !api->world_mod_id || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_tree_choose_rpg || !api->ui_message'
if ($sp.Contains($initPrimeNeedle87)) {
    $sp = $sp.Replace($initPrimeNeedle87,$initPrimeNew87)
}
Write-Utf8NoBom $spPath $sp

$manifest = [IO.File]::ReadAllText($manifestPath)
if (-not $manifest.Contains('"ui.layout.v1"')) {
    if (-not $manifest.Contains('    "active_mods.registry.v2",')) { throw 'v8.7 manifest registry capability anchor missing.' }
    $manifest = $manifest.Replace('    "active_mods.registry.v2",', '    "active_mods.registry.v2",' + "`n" + '    "ui.layout.v1",')
}
Write-Utf8NoBom $manifestPath $manifest


# ---------------------------------------------------------------------------
# v8.7.3 — Prime row visibility + real horizontal viewport + section rendering
# ---------------------------------------------------------------------------
Write-Host "Applying v8.7.5 overview frame + Prime layout/text + locale refresh..." -ForegroundColor Cyan

# The three Prime choices were laid out at logical columns 0/2/4.  With 30-char
# RPG nodes that expands to three nodes separated by an entire lane, so the third
# choice could sit behind the fixed detail pane while still remaining keyboard-selectable.
# Prime rows use compact adjacent columns 0/1/2; the rest of each branch keeps its
# established topology.
$sp = [IO.File]::ReadAllText($spPath)
$primeCoreLayoutOld871 = @'
    // 20..22 are exclusive specialization roots; 23..25 their capstones.
    if( branch_index < 23 ) {
        return { 12, static_cast<int>( ( branch_index - 20 ) * 2 ) };
    }
    if( branch_index < 26 ) {
        return { 14, static_cast<int>( ( branch_index - 23 ) * 2 ) };
    }
'@
$primeCoreLayoutNew871 = @'
    // 20..22 are exclusive Prime roots; 23..25 their capstones.
    // Compact 0/1/2 lanes keep all three choices visible; the host preserves Prime lanes.
    if( branch_index < 23 ) {
        return { 12, static_cast<int>( branch_index - 20 ) };
    }
    if( branch_index < 26 ) {
        return { 14, static_cast<int>( branch_index - 23 ) };
    }
'@
$sp = Replace-TextBlock $sp $primeCoreLayoutOld871 $primeCoreLayoutNew871 'v8.7.3 compact core Prime row'

$primeModLayoutOld871 = @'
    if( i < layout.size() ) return layout[i];
    if( i < 23 ) return { 16, static_cast<int>( i - 20 ) * 2 };
    return { 18 + static_cast<int>( ( i - 23 ) / 3 ) * 2,
             static_cast<int>( ( i - 23 ) % 3 ) * 2 };
'@
$primeModLayoutNew871 = @'
    if( i < layout.size() ) return layout[i];
    if( i < 23 ) return { 16, static_cast<int>( i - 20 ) };
    return { 18 + static_cast<int>( ( i - 23 ) / 3 ) * 2,
             static_cast<int>( ( i - 23 ) % 3 ) };
'@
$sp = Replace-TextBlock $sp $primeModLayoutOld871 $primeModLayoutNew871 'v8.7.3 compact mod Prime row'

# Make sectioned detail text actually separate the positive and negative halves
# of Prime tradeoffs instead of repeating the full sentence twice.
$rpgDetailOld871 = @'
std::string rpg_detail_body( const perk_def &perk, const std::string &body,
                             const std::string &requires_text )
{
    std::string result;
    result += tr( "BONUS:", "БОНУС:" );
    result += "\n";
    result += body;
    result += "\n\n";
    result += tr( "REQUIRES:", "ТРЕБУЕТ:" );
    result += "\n";
    result += requires_text;
    if( prime_visual_perk( perk ) ) {
        result += "\n\n";
        result += tr( "DRAWBACK:", "ШТРАФ:" );
        result += "\n";
        result += body;
    }
    return result;
}
'@
$rpgDetailNew871 = @'
std::string rpg_detail_body( const perk_def &perk, const std::string &body,
                             const std::string &requires_text )
{
    std::string bonus = body;
    std::string drawback;
    if( prime_visual_perk( perk ) ) {
        const size_t colon = bonus.find( ':' );
        if( colon != std::string::npos ) {
            bonus = bonus.substr( colon + 1 );
        }
        const size_t semicolon = bonus.find( ';' );
        if( semicolon != std::string::npos ) {
            drawback = bonus.substr( semicolon + 1 );
            bonus = bonus.substr( 0, semicolon );
        }
        while( !bonus.empty() && bonus.front() == ' ' ) bonus.erase( bonus.begin() );
        while( !drawback.empty() && drawback.front() == ' ' ) drawback.erase( drawback.begin() );
    }

    std::string result;
    result += tr( "BONUS:", "БОНУС:" );
    result += "\n" + bonus;
    if( prime_visual_perk( perk ) && !drawback.empty() ) {
        result += "\n\n";
        result += tr( "DRAWBACK:", "ШТРАФ:" );
        result += "\n" + drawback;
    }
    result += "\n\n";
    result += tr( "REQUIRES:", "ТРЕБУЕТ:" );
    result += "\n" + requires_text;
    return result;
}
'@
$sp = Replace-TextBlock $sp $rpgDetailOld871 $rpgDetailNew871 'v8.7.3 split Prime detail sections'
Write-Utf8NoBom $spPath $sp

$loader = [IO.File]::ReadAllText($loaderPath)

# Preserve declared Prime lanes during the host's parent-centering pass.
# Without this, different Prime choices that share parents can be recentered onto the same x-position.
$primeLaneOld875 = @'
    for( int pass = 0; pass < 3; ++pass ) {
        for( size_t i = 0; i < node_count; ++i ) {
            int parent_count = 0;
'@
$primeLaneNew875 = @'
    for( int pass = 0; pass < 3; ++pass ) {
        for( size_t i = 0; i < node_count; ++i ) {
            if( ncmm_ui_border_style_id( i ) == NCMM_UI_BORDER_PRIME ) {
                continue;
            }
            int parent_count = 0;
'@
$loader = Replace-TextBlock $loader $primeLaneOld875 $primeLaneNew875 'v8.7.5 preserve Prime lanes'

# HOTFIX13: parent-centering is useful for singleton rows but can collapse siblings onto
# the same physical x coordinate.  That makes cards overlap and node_at() returns only the
# first hit, leaving a visually present node impossible to click.  Deconflict every row that
# contains multiple nodes back onto declared logical lanes, while retaining centered singleton rows.
$treeRowDeconflictOld013 = @'
    }

    int max_layout_x2 = 0;
'@
$treeRowDeconflictNew013 = @'
    }

    // NCMM HOTFIX13: keep every multi-node row physically non-overlapping after routing.
    // Singleton rows retain parent-centering; siblings use declared lanes with a one-lane minimum gap.
    std::map<int, std::vector<size_t>> ncmm_row_nodes;
    for( size_t i = 0; i < node_count; ++i ) {
        ncmm_row_nodes[nodes[i].row].push_back( i );
    }
    for( auto &row_entry : ncmm_row_nodes ) {
        std::vector<size_t> &row_nodes = row_entry.second;
        if( row_nodes.size() < 2 ) {
            continue;
        }
        std::stable_sort( row_nodes.begin(), row_nodes.end(), [&]( size_t lhs, size_t rhs ) {
            if( nodes[lhs].column != nodes[rhs].column ) {
                return nodes[lhs].column < nodes[rhs].column;
            }
            return lhs < rhs;
        } );
        int next_x2 = nodes[row_nodes.front()].column * 2;
        for( size_t index : row_nodes ) {
            const int declared_x2 = nodes[index].column * 2;
            next_x2 = std::max( next_x2, declared_x2 );
            layout_x2[index] = next_x2;
            next_x2 += 2;
        }
    }

    int max_layout_x2 = 0;
'@
$loader = Replace-TextBlock $loader $treeRowDeconflictOld013 $treeRowDeconflictNew013 'HOTFIX13 tree row physical deconfliction'

# A real horizontal viewport: keyboard navigation now automatically pans the tree
# so the selected node cannot remain logically selected outside the visible canvas.
$treeViewportOld871 = @'
    int selected = 0;
    int first_row = 0;

    auto keep_visible = [&]() {
        const int row = nodes[selected].row;
        if( row < first_row ) first_row = row;
        else if( row >= first_row + visible_rows ) first_row = row - visible_rows + 1;
        first_row = std::max( 0, std::min( first_row,
                         std::max( 0, max_row - visible_rows + 1 ) ) );
    };

    auto node_x = [&]( size_t i ) {
        return 2 + ( layout_x2[i] * lane_step + 1 ) / 2;
    };
    auto node_y = [&]( size_t i ) {
        return header_height + ( nodes[i].row - first_row ) * ( node_height + vgap );
    };
    auto visible = [&]( size_t i ) {
        return nodes[i].row >= first_row && nodes[i].row < first_row + visible_rows &&
               node_x( i ) + node_width < canvas_width + 2;
    };
'@
$treeViewportNew871 = @'
    int selected = 0;
    int first_row = 0;
    int first_x = 0;
    const int max_first_x = std::max( 0, logical_tree_width - canvas_width + 1 );

    auto logical_node_x = [&]( size_t i ) {
        return 2 + ( layout_x2[i] * lane_step + 1 ) / 2;
    };

    auto keep_visible = [&]() {
        const int row = nodes[selected].row;
        if( row < first_row ) first_row = row;
        else if( row >= first_row + visible_rows ) first_row = row - visible_rows + 1;
        first_row = std::max( 0, std::min( first_row,
                         std::max( 0, max_row - visible_rows + 1 ) ) );

        if( ncmm_ui_horizontal_viewport() ) {
            int row_left = 1000000;
            int row_right = -1000000;
            for( size_t i = 0; i < node_count; ++i ) {
                if( nodes[i].row != row ) continue;
                const int candidate_left = logical_node_x( i );
                row_left = std::min( row_left, candidate_left );
                row_right = std::max( row_right, candidate_left + node_width - 1 );
            }

            // If the entire logical row fits, show it as a group.  This is crucial
            // for the three-way Prime choice: all alternatives remain visible at once.
            if( row_left <= row_right && row_right - row_left + 1 < canvas_width ) {
                first_x = row_left - 2;
            } else {
                const int left = logical_node_x( static_cast<size_t>( selected ) );
                const int right = left + node_width - 1;
                if( left - first_x < 2 ) {
                    first_x = left - 2;
                } else if( right - first_x >= canvas_width + 2 ) {
                    first_x = right - canvas_width;
                }
            }
            first_x = std::max( 0, std::min( first_x, max_first_x ) );
        } else {
            first_x = 0;
        }
    };

    auto node_x = [&]( size_t i ) {
        return logical_node_x( i ) - first_x;
    };
    auto node_y = [&]( size_t i ) {
        return header_height + ( nodes[i].row - first_row ) * ( node_height + vgap );
    };
    auto visible = [&]( size_t i ) {
        const int x = node_x( i );
        return nodes[i].row >= first_row && nodes[i].row < first_row + visible_rows &&
               x >= 2 && x + node_width < canvas_width + 2;
    };
'@
$loader = Replace-TextBlock $loader $treeViewportOld871 $treeViewportNew871 'v8.7.3 horizontal tree viewport'

# Actual per-node border glyph styling.  Prime nodes now look different even on
# monochrome terminals; Major nodes keep the normal box glyphs but their color/marker.
$treeBorderOld871 = @'
            wattron( frame, border );
            mvwhline( frame, point( x + 1, y ), LINE_OXOX, node_width - 2 );
            mvwhline( frame, point( x + 1, y + node_height - 1 ), LINE_OXOX, node_width - 2 );
            mvwvline( frame, point( x, y + 1 ), LINE_XOXO, node_height - 2 );
            mvwvline( frame, point( x + node_width - 1, y + 1 ), LINE_XOXO, node_height - 2 );
            mvwaddch( frame, point( x, y ), LINE_OXXO );
            mvwaddch( frame, point( x + node_width - 1, y ), LINE_OOXX );
            mvwaddch( frame, point( x, y + node_height - 1 ), LINE_XXOO );
            mvwaddch( frame, point( x + node_width - 1, y + node_height - 1 ), LINE_XOOX );

            const int center_x = x + node_width / 2;
            if( has_incoming[i] ) {
                mvwaddch( frame, point( center_x, y ), LINE_XXOX );
            }
            if( has_outgoing[i] ) {
                mvwaddch( frame, point( center_x, y + node_height - 1 ), LINE_OXXX );
            }
            wattroff( frame, border );
'@
$treeBorderNew871 = @'
            const bool prime_border = border_style == NCMM_UI_BORDER_PRIME;
            const int horizontal_glyph = prime_border ? '=' : LINE_OXOX;
            const int vertical_glyph = prime_border ? '|' : LINE_XOXO;
            const int top_left_glyph = prime_border ? '+' : LINE_OXXO;
            const int top_right_glyph = prime_border ? '+' : LINE_OOXX;
            const int bottom_left_glyph = prime_border ? '+' : LINE_XXOO;
            const int bottom_right_glyph = prime_border ? '+' : LINE_XOOX;

            wattron( frame, border );
            mvwhline( frame, point( x + 1, y ), horizontal_glyph, node_width - 2 );
            mvwhline( frame, point( x + 1, y + node_height - 1 ), horizontal_glyph, node_width - 2 );
            mvwvline( frame, point( x, y + 1 ), vertical_glyph, node_height - 2 );
            mvwvline( frame, point( x + node_width - 1, y + 1 ), vertical_glyph, node_height - 2 );
            mvwaddch( frame, point( x, y ), top_left_glyph );
            mvwaddch( frame, point( x + node_width - 1, y ), top_right_glyph );
            mvwaddch( frame, point( x, y + node_height - 1 ), bottom_left_glyph );
            mvwaddch( frame, point( x + node_width - 1, y + node_height - 1 ), bottom_right_glyph );

            const int center_x = x + node_width / 2;
            if( has_incoming[i] ) {
                mvwaddch( frame, point( center_x, y ), prime_border ? '+' : LINE_XXOX );
            }
            if( has_outgoing[i] ) {
                mvwaddch( frame, point( center_x, y + node_height - 1 ), prime_border ? '+' : LINE_OXXX );
            }
            wattroff( frame, border );
'@
$loader = Replace-TextBlock $loader $treeBorderOld871 $treeBorderNew871 'v8.7.3 Prime border glyphs'

# Section headers in the detail pane receive semantic colors rather than rendering
# as one undifferentiated gray paragraph.
$detailBodyOld871 = @'
        if( detail.body && detail.body[0] != '\0' ) {
            const std::vector<std::string> folded = foldstring( detail.body, detail_width - 3 );
            const int max_lines = std::max( 1, frame_height - footer_height - dy - 1 );
            for( int line = 0; line < std::min<int>( max_lines, folded.size() ); ++line ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy + line ), detail_width - 3,
                                            c_light_gray, folded[line] );
            }
        }
'@
$detailBodyNew871 = @'
        if( detail.body && detail.body[0] != '\0' ) {
            const std::vector<std::string> folded = foldstring( detail.body, detail_width - 3 );
            const int max_lines = std::max( 1, frame_height - footer_height - dy - 1 );
            for( int line = 0; line < std::min<int>( max_lines, folded.size() ); ++line ) {
                nc_color detail_color = c_light_gray;
                if( ncmm_ui_sectioned_detail() ) {
                    if( folded[line] == "BONUS:" || folded[line] == "БОНУС:" ) {
                        detail_color = c_light_green;
                    } else if( folded[line] == "DRAWBACK:" || folded[line] == "ШТРАФ:" ) {
                        detail_color = c_light_red;
                    } else if( folded[line] == "REQUIRES:" || folded[line] == "ТРЕБУЕТ:" ) {
                        detail_color = c_yellow;
                    }
                }
                ncmm_trim_and_print_literal( frame, point( dx, dy + line ), detail_width - 3,
                                            detail_color, folded[line] );
            }
        }
'@
$loader = Replace-TextBlock $loader $detailBodyOld871 $detailBodyNew871 'v8.7.3 sectioned detail renderer'
$borderDeclCount873 = ([regex]::Matches($loader,[regex]::Escape('const uint32_t border_style = ncmm_ui_border_style_id('))).Count
if ($borderDeclCount873 -ne 2) {
    throw "v8.7.3 host border-style local declaration contract expected 2, found $borderDeclCount873"
}
foreach($layoutNeedle013 in @(
    'std::map<int, std::vector<size_t>> ncmm_row_nodes;',
    'std::stable_sort( row_nodes.begin(), row_nodes.end()',
    'layout_x2[index] = next_x2;',
    'next_x2 += 2;'
)) {
    if(-not $loader.Contains($layoutNeedle013)){throw ('HOTFIX13 generated tree-layout audit missing: '+$layoutNeedle013)}
}
Write-Utf8NoBom $loaderPath $loader

# Final guards for the exact regression reported from the live v8.7.0 UI.
$sp871Audit = Normalize-Lf ([IO.File]::ReadAllText($spPath))
foreach ($needle in @(
    'return { 12, static_cast<int>( branch_index - 20 ) };',
    'return { 14, static_cast<int>( branch_index - 23 ) };',
    'if( i < 23 ) return { 16, static_cast<int>( i - 20 ) };'
)) {
    if (-not $sp871Audit.Contains($needle)) { throw "v8.7.3 Prime-row visibility audit missing: $needle" }
}
$loader871Audit = Normalize-Lf ([IO.File]::ReadAllText($loaderPath))
foreach ($needle in @(
    'int first_x = 0;',
    'ncmm_ui_horizontal_viewport()',
    'const bool prime_border = border_style == NCMM_UI_BORDER_PRIME;',
    'if( ncmm_ui_border_style_id( i ) == NCMM_UI_BORDER_PRIME )',
    'folded[line] == "DRAWBACK:"',
    'folded[line] == "REQUIRES:"'
)) {
    if (-not $loader871Audit.Contains($needle)) { throw "v8.7.3 UI visibility/render audit missing: $needle" }
}


# ---------------------------------------------------------------------------
# v8.7.5 — real overview detail frame + lighter overview cards
# ---------------------------------------------------------------------------
Write-Host "Applying v8.7.5 real overview detail frame..." -ForegroundColor Cyan

$loader = [IO.File]::ReadAllText($loaderPath)
$cardGeometryOld875 = @'
    int columns = static_cast<int>( std::min( requested_columns, count ) );
    constexpr int gap = 1;
    constexpr int card_height = 8;
    constexpr int header_height = 6;
    constexpr int footer_height = 2;

    while( columns > 1 ) {
        const int candidate = ( TERMX - 4 - gap * ( columns - 1 ) ) / columns;
        if( candidate >= 28 ) {
            break;
        }
        --columns;
    }

    const int card_width = std::max( 28, std::min( 46,
                           ( TERMX - 4 - gap * ( columns - 1 ) ) / columns ) );
    const int frame_width = columns * card_width + gap * ( columns - 1 ) + 2;
'@
$cardGeometryNew875 = @'
    int columns = static_cast<int>( std::min( requested_columns, count ) );
    constexpr int gap = 1;
    constexpr int card_height = 8;
    constexpr int header_height = 6;
    constexpr int footer_height = 2;
    const bool detail_panel = ncmm_ui_sectioned_detail() && TERMX >= 108;
    const int requested_card_detail_width = active_ui_theme.preferred_detail_width > 0 ?
                                            active_ui_theme.preferred_detail_width : 40;
    const int card_detail_width = detail_panel ?
                                  std::clamp( requested_card_detail_width, 34, 48 ) : 0;
    const int card_detail_reserve = detail_panel ? card_detail_width + 1 : 0;

    while( columns > 1 ) {
        const int candidate = ( TERMX - 4 - card_detail_reserve - gap * ( columns - 1 ) ) / columns;
        if( candidate >= 28 ) {
            break;
        }
        --columns;
    }

    const int card_width = std::max( 28, std::min( 46,
                           ( TERMX - 4 - card_detail_reserve - gap * ( columns - 1 ) ) / columns ) );
    const int frame_width = columns * card_width + gap * ( columns - 1 ) + 2 + card_detail_reserve;
'@
$loader = Replace-TextBlock $loader $cardGeometryOld875 $cardGeometryNew875 'v8.7.5 overview detail geometry'

$cardBodyLinesOld875 = @'
                const std::vector<std::string> folded = foldstring( card.body, card_width - 4 );
                for( size_t line = 0; line < std::min<size_t>( 3, folded.size() ); ++line ) {
'@
$cardBodyLinesNew875 = @'
                const std::vector<std::string> folded = foldstring( card.body, card_width - 4 );
                const size_t card_body_lines = detail_panel ? 1 : 3;
                for( size_t line = 0; line < std::min<size_t>( card_body_lines, folded.size() ); ++line ) {
'@
$loader = Replace-TextBlock $loader $cardBodyLinesOld875 $cardBodyLinesNew875 'v8.7.5 lighter overview card body'

$cardFooterOld875 = @'
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ),
                                    frame_width - 4, c_dark_gray, footer );

        wnoutrefresh( frame );

        const int first_index = first_row * columns;
'@
$cardFooterNew875 = @'
        ncmm_trim_and_print_literal( frame, point( 2, frame_height - 2 ),
                                    frame_width - 4, c_dark_gray, footer );

        if( detail_panel ) {
            const int divider_x = 1 + columns * card_width + gap * ( columns - 1 );
            for( int y = header_height - 1; y < frame_height - footer_height; ++y ) {
                mvwaddch( frame, point( divider_x, y ), LINE_XOXO );
            }
            ncmm_trim_and_print_literal( frame, point( divider_x + 2, header_height - 1 ),
                                        card_detail_width - 3, c_dark_gray,
                                        tr_ui( "DETAIL", "ДЕТАЛИ" ) );

            const ncmm_ui_card_v1 &detail = cards[selected];
            const int dx = divider_x + 2;
            int dy = header_height;
            const nc_color detail_accent = ncmm_ui_theme_enabled( static_cast<size_t>( selected ) ) ?
                                           ncmm_ui_theme_accent( static_cast<size_t>( selected ) ) : c_white;
            const std::vector<std::string> detail_title =
                foldstring( detail.title ? detail.title : "", card_detail_width - 3 );
            for( size_t line = 0; line < std::min<size_t>( 2, detail_title.size() ); ++line ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy++ ), card_detail_width - 3,
                                            detail_accent, detail_title[line] );
            }
            if( detail.subtitle && detail.subtitle[0] != '\0' ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy++ ), card_detail_width - 3,
                                            c_light_gray, detail.subtitle );
            }
            if( detail.badge && detail.badge[0] != '\0' ) {
                ncmm_trim_and_print_literal( frame, point( dx, dy++ ), card_detail_width - 3,
                                            detail_accent, detail.badge );
            }
            ++dy;
            if( detail.body && detail.body[0] != '\0' ) {
                const std::vector<std::string> folded = foldstring( detail.body, card_detail_width - 3 );
                const int max_lines = std::max( 1, frame_height - footer_height - dy - 1 );
                for( int line = 0; line < std::min<int>( max_lines, folded.size() ); ++line ) {
                    nc_color detail_color = c_light_gray;
                    if( folded[line] == "HOW TO GAIN XP:" || folded[line] == "КАК КАЧАТЬ:" ||
                        folded[line] == "PROGRESSION:" || folded[line] == "ПРОГРЕСС:" ) {
                        detail_color = c_light_green;
                    } else if( folded[line] == "EFFICIENCY:" || folded[line] == "ЭФФЕКТИВНОСТЬ:" ) {
                        detail_color = c_yellow;
                    }
                    ncmm_trim_and_print_literal( frame, point( dx, dy + line ), card_detail_width - 3,
                                                detail_color, folded[line] );
                }
            }
        }

        wnoutrefresh( frame );

        const int first_index = first_row * columns;
'@
$loader = Replace-TextBlock $loader $cardFooterOld875 $cardFooterNew875 'v8.7.5 overview detail renderer'
Write-Utf8NoBom $loaderPath $loader

$sp = [IO.File]::ReadAllText($spPath)
$overviewCoreBodyOld875 = @'
            card.body = branch_focus( branch ) + "\n" + branch_xp_source( branch ) +
                        "\n" + branch_efficiency_text( branch );
'@
$overviewCoreBodyNew875 = @'
            card.body = branch_focus( branch ) + "\n\n" +
                        tr( "HOW TO GAIN XP:", "КАК КАЧАТЬ:" ) + "\n" +
                        branch_xp_source( branch ) + "\n\n" +
                        tr( "EFFICIENCY:", "ЭФФЕКТИВНОСТЬ:" ) + "\n" +
                        branch_efficiency_text( branch );
'@
$sp = Replace-TextBlock $sp $overviewCoreBodyOld875 $overviewCoreBodyNew875 'v8.7.5 overview branch detail text'

$overviewModBodyOld875 = @'
            card.body = integration_mod_focus( id ) + "\n" +
                        tr( "Available only in worlds where this mod is active.",
                            "Доступно только в мирах, где активен этот мод." );
'@
$overviewModBodyNew875 = @'
            card.body = integration_mod_focus( id ) + "\n\n" +
                        tr( "PROGRESSION:", "ПРОГРЕСС:" ) + "\n" +
                        tr( "These perks use your overall Survivor level instead of separate branch XP.",
                            "Эти перки используют общий уровень Survivor вместо отдельного опыта ветки." ) + "\n\n" +
                        tr( "Available only in worlds where this mod is active.",
                            "Доступно только в мирах, где активен этот мод." );
'@
$sp = Replace-TextBlock $sp $overviewModBodyOld875 $overviewModBodyNew875 'v8.7.5 overview mod detail text'
Write-Utf8NoBom $spPath $sp

# Exact v8.7.5 visual/clarity guards.
$loader875Audit = Normalize-Lf ([IO.File]::ReadAllText($loaderPath))
foreach ($needle in @(
    'const bool detail_panel = ncmm_ui_sectioned_detail() && TERMX >= 108;',
    'const size_t card_body_lines = detail_panel ? 1 : 3;',
    'tr_ui( "DETAIL", "ДЕТАЛИ" )',
    'folded[line] == "HOW TO GAIN XP:"'
)) {
    if (-not $loader875Audit.Contains($needle)) { throw "v8.7.5 overview frame audit missing: $needle" }
}
$sp875Audit = Normalize-Lf ([IO.File]::ReadAllText($spPath))
foreach ($needle in @(
    'tr( "HOW TO GAIN XP:", "КАК КАЧАТЬ:" )',
    'tr( "PROGRESSION:", "ПРОГРЕСС:" )',
    'These perks use your overall Survivor level instead of separate branch XP.'
)) {
    if (-not $sp875Audit.Contains($needle)) { throw "v8.7.5 overview clarity audit missing: $needle" }
}

# ---------------------------------------------------------------------------
# v8.7.6.1 — Clarity Pass: reasons, next unlocks, dependency focus, viewport cues
# ---------------------------------------------------------------------------
Write-Host "Applying v8.7.6.8 Clarity Pass + cache-marker hardening..." -ForegroundColor Cyan

$sp = [IO.File]::ReadAllText($spPath)

$clarityHelpers876 = @'
std::string perk_lock_reason( const perk_def &perk )
{
    const int64_t level = perk_progression_level( perk );
    const int rank = perk_rank( perk );
    const int max_rank = perk_max_rank( perk );

    if( !perk_world_available( perk ) ) {
        const std::string mod_name = integration_mod_name( perk );
        return mod_name.empty() ?
               tr( "Required mod is not active.", "Требуемый мод не активен." ) :
               tr( "Requires mod: ", "Нужен мод: " ) + mod_name;
    }
    if( rank >= max_rank ) {
        return tr( "Maximum rank reached.", "Достигнут максимальный ранг." );
    }

    std::vector<std::string> reasons;
    if( level < perk.required_level ) {
        reasons.push_back(
            ( integration_perk( perk ) ? tr( "Survivor level ", "Уровень Survivor " ) :
                                        tr( "Branch level ", "Уровень ветки " ) ) +
            std::to_string( level ) + "/" + std::to_string( perk.required_level ) );
    }

    if( exclusive_specialization_perk( perk ) && !exclusive_specialization_allowed( perk ) ) {
        reasons.push_back( tr( "Another Prime path is already selected; full respec is required.",
                               "Уже выбран другой Прайм-путь; для смены нужен полный сброс." ) );
    }

    for( const char *id : { perk.prereq1, perk.prereq2 } ) {
        if( id == nullptr || *id == '\0' ) {
            continue;
        }
        const perk_def *required = find_perk( id );
        if( required == nullptr ) {
            reasons.push_back( tr( "Required perk is unavailable in this world.",
                                   "Требуемый перк недоступен в этом мире." ) );
        } else if( !owned( *required ) ) {
            reasons.push_back( tr( "Requires: ", "Нужен перк: " ) + perk_display_name( *required ) );
        }
    }

    if( reasons.empty() ) {
        const int64_t points = perk.currency == currency_id::perk ?
                               get_state( "perk_points", 0 ) : get_state( "major_points", 0 );
        if( points <= 0 ) {
            reasons.push_back( perk.currency == currency_id::perk ?
                               tr( "Need 1 perk point; available: 0.", "Нужно 1 очко перка; доступно: 0." ) :
                               tr( "Need 1 major point; available: 0.", "Нужно 1 большое очко; доступно: 0." ) );
        }
    }

    if( reasons.empty() ) {
        return rank > 0 ? tr( "Ready to upgrade.", "Можно улучшить." ) :
                          tr( "Ready to purchase.", "Можно купить." );
    }

    std::string result;
    for( size_t i = 0; i < reasons.size(); ++i ) {
        if( i > 0 ) result += "\n";
        result += "- " + reasons[i];
    }
    return result;
}

std::string branch_next_unlock_text( branch_id branch )
{
    const int64_t current_level = branch_level( branch );
    const perk_def *best = nullptr;
    int best_level = 1000000;
    for( const perk_def &perk : perks ) {
        if( perk.branch != branch || integration_perk( perk ) || !perk_world_available( perk ) ||
            perk_maxed( perk ) || perk.required_level <= current_level ) {
            continue;
        }
        if( perk.required_level < best_level ) {
            best_level = perk.required_level;
            best = &perk;
        }
    }
    if( best == nullptr ) {
        return tr( "No later level-gated perk.", "Нет следующего перка по уровню." );
    }
    return perk_display_name( *best ) + " — " +
           tr( "branch level ", "уровень ветки " ) + std::to_string( best_level );
}

int integration_ready_count( const std::string &mod_id )
{
    const int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    int result = 0;
    for( const perk_def &perk : perks ) {
        if( !integration_perk( perk ) || !perk_world_available( perk ) ||
            mod_id != integration_mod_id( perk ) || perk_maxed( perk ) ) {
            continue;
        }
        if( level >= perk.required_level && prerequisites_met( perk ) ) {
            ++result;
        }
    }
    return result;
}

std::string integration_next_unlock_text( const std::string &mod_id )
{
    const int64_t current_level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    const perk_def *best = nullptr;
    int best_level = 1000000;
    for( const perk_def &perk : perks ) {
        if( !integration_perk( perk ) || !perk_world_available( perk ) ||
            mod_id != integration_mod_id( perk ) || perk_maxed( perk ) ||
            perk.required_level <= current_level ) {
            continue;
        }
        if( perk.required_level < best_level ) {
            best_level = perk.required_level;
            best = &perk;
        }
    }
    if( best == nullptr ) {
        return tr( "No later Survivor-level unlock.", "Нет следующего открытия по уровню Survivor." );
    }
    return perk_display_name( *best ) + " — " +
           tr( "Survivor level ", "уровень Survivor " ) + std::to_string( best_level );
}

'@
$sp = Replace-TextBlock $sp 'std::string prereq_text( const perk_def &perk )' `
    ($clarityHelpers876 + 'std::string prereq_text( const perk_def &perk )') `
    'v8.7.6.1 clarity helper insertion'

$detailStatusOld876 = @'
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );
'@
$detailStatusNew876 = @'
        title += "\n" + tr( "Prerequisites: ", "Требования: " ) + prereq_text( perk );
        title += "\n" + tr( "Cost per rank: ", "Цена за ранг: " ) + cost_text( perk );
        title += "\n" + tr( "Status: ", "Статус: " ) + perk_lock_reason( perk );
'@
$sp = Replace-TextBlock $sp $detailStatusOld876 $detailStatusNew876 'v8.7.6.1 exact perk lock reason in detail dialog'

$coreTreeStatusOld876 = @'
                node.card.body = rpg_detail_body( perk, node.card.body, prereq_text( perk ) );
'@
$coreTreeStatusNew876 = @'
                node.card.body = rpg_detail_body( perk, node.card.body, prereq_text( perk ) ) +
                                 "\n\n" + tr( "STATUS:", "СТАТУС:" ) + "\n" + perk_lock_reason( perk );
'@
$sp = Replace-TextBlock $sp $coreTreeStatusOld876 $coreTreeStatusNew876 'v8.7.6.1 core tree lock reason'

$modTreeStatusOld876 = @'
                node.card.body = rpg_detail_body( tree_perk, node.card.body, prereq_text( tree_perk ) );
'@
$modTreeStatusNew876 = @'
                node.card.body = rpg_detail_body( tree_perk, node.card.body, prereq_text( tree_perk ) ) +
                                 "\n\n" + tr( "STATUS:", "СТАТУС:" ) + "\n" + perk_lock_reason( tree_perk );
'@
$sp = Replace-TextBlock $sp $modTreeStatusOld876 $modTreeStatusNew876 'v8.7.6.1 mod tree lock reason'

$overviewCoreOld876 = @'
            card.body = branch_focus( branch ) + "\n\n" +
                        tr( "HOW TO GAIN XP:", "КАК КАЧАТЬ:" ) + "\n" +
                        branch_xp_source( branch ) + "\n\n" +
                        tr( "EFFICIENCY:", "ЭФФЕКТИВНОСТЬ:" ) + "\n" +
                        branch_efficiency_text( branch );
'@
$overviewCoreNew876 = @'
            card.body = branch_focus( branch ) + "\n\n" +
                        tr( "HOW TO GAIN XP:", "КАК КАЧАТЬ:" ) + "\n" +
                        branch_xp_source( branch ) + "\n\n" +
                        tr( "PROGRESSION:", "ПРОГРЕСС:" ) + "\n" +
                        tr( "Branch level ", "Уровень ветки " ) + std::to_string( blevel ) +
                        " | XP " + std::to_string( bxp ) + "/" + std::to_string( bnext ) + "\n" +
                        tr( "Available now: ", "Доступно сейчас: " ) +
                        std::to_string( branch_unlocked_count( branch, blevel ) ) + "\n" +
                        tr( "Next unlock: ", "Следующее открытие: " ) + branch_next_unlock_text( branch ) + "\n\n" +
                        tr( "EFFICIENCY:", "ЭФФЕКТИВНОСТЬ:" ) + "\n" +
                        branch_efficiency_text( branch );
'@
$sp = Replace-TextBlock $sp $overviewCoreOld876 $overviewCoreNew876 'v8.7.6.1 overview next core unlock'

$overviewModOld876 = @'
            card.body = integration_mod_focus( id ) + "\n\n" +
                        tr( "PROGRESSION:", "ПРОГРЕСС:" ) + "\n" +
                        tr( "These perks use your overall Survivor level instead of separate branch XP.",
                            "Эти перки используют общий уровень Survivor вместо отдельного опыта ветки." ) + "\n\n" +
                        tr( "Available only in worlds where this mod is active.",
                            "Доступно только в мирах, где активен этот мод." );
'@
$overviewModNew876 = @'
            card.body = integration_mod_focus( id ) + "\n\n" +
                        tr( "PROGRESSION:", "ПРОГРЕСС:" ) + "\n" +
                        tr( "These perks use your overall Survivor level instead of separate branch XP.",
                            "Эти перки используют общий уровень Survivor вместо отдельного опыта ветки." ) + "\n" +
                        tr( "Survivor level: ", "Уровень Survivor: " ) + std::to_string( level ) + "\n" +
                        tr( "Available now: ", "Доступно сейчас: " ) +
                        std::to_string( integration_ready_count( id ) ) + "\n" +
                        tr( "Next unlock: ", "Следующее открытие: " ) + integration_next_unlock_text( id ) + "\n\n" +
                        tr( "Available only in worlds where this mod is active.",
                            "Доступно только в мирах, где активен этот мод." );
'@
$sp = Replace-TextBlock $sp $overviewModOld876 $overviewModNew876 'v8.7.6.1 overview next mod unlock'
Write-Utf8NoBom $spPath $sp

$loader = [IO.File]::ReadAllText($loaderPath)

$dependencySeedOld876 = @'
        std::vector<bool> has_incoming( node_count, false );
        std::vector<bool> has_outgoing( node_count, false );

        for( size_t e = 0; e < edge_count; ++e ) {
'@
$dependencySeedNew876 = @'
        std::vector<bool> has_incoming( node_count, false );
        std::vector<bool> has_outgoing( node_count, false );

        // v8.7.6.1: focus the selected node's actual dependency chain.  Ancestors
        // and descendants are propagated separately so sibling branches do not
        // become highlighted merely because they share a common parent.
        std::vector<bool> dependency_ancestor( node_count, false );
        std::vector<bool> dependency_descendant( node_count, false );
        std::vector<bool> dependency_related( node_count, false );
        dependency_ancestor[static_cast<size_t>( selected )] = true;
        dependency_descendant[static_cast<size_t>( selected )] = true;
        for( size_t pass = 0; pass < node_count; ++pass ) {
            bool changed = false;
            for( size_t e = 0; e < edge_count; ++e ) {
                const size_t from = edges[e].from_index;
                const size_t to = edges[e].to_index;
                if( dependency_ancestor[to] && !dependency_ancestor[from] ) {
                    dependency_ancestor[from] = true;
                    changed = true;
                }
                if( dependency_descendant[from] && !dependency_descendant[to] ) {
                    dependency_descendant[to] = true;
                    changed = true;
                }
            }
            if( !changed ) break;
        }
        for( size_t i = 0; i < node_count; ++i ) {
            dependency_related[i] = dependency_ancestor[i] || dependency_descendant[i];
        }

        for( size_t e = 0; e < edge_count; ++e ) {
'@
$loader = Replace-TextBlock $loader $dependencySeedOld876 $dependencySeedNew876 'v8.7.6.1 dependency focus propagation'

$edgeFocusOld876 = @'
            const bool related_to_selection =
                from == static_cast<size_t>( selected ) || to == static_cast<size_t>( selected );
            const bool locked_path = ( nodes[to].flags & NCMM_UI_CARD_LOCKED ) != 0;
            const int style = related_to_selection ? 2 : ( locked_path ? 0 : 1 );
'@
$edgeFocusNew876 = @'
            const bool related_to_selection =
                ( dependency_ancestor[from] && dependency_ancestor[to] ) ||
                ( dependency_descendant[from] && dependency_descendant[to] );
            const int style = related_to_selection ? 2 : 0;
'@
$loader = Replace-TextBlock $loader $edgeFocusOld876 $edgeFocusNew876 'v8.7.6.1 dependency edge focus'

$treeStyleOld876 = @'
            const bool themed = ncmm_ui_theme_enabled( i );
            const uint32_t border_style = ncmm_ui_border_style_id( i );
            const nc_color theme_accent = ncmm_ui_theme_accent( i );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                    border_style == NCMM_UI_BORDER_MAJOR ? c_yellow :
                                    border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = locked || border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                        active ? c_white :
                                        border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                        themed ? theme_accent : c_light_gray;
'@
$treeStyleNew876 = @'
            const bool themed = ncmm_ui_theme_enabled( i );
            const bool dependency_focus = dependency_related[i];
            const uint32_t border_style = ncmm_ui_border_style_id( i );
            const nc_color theme_accent = ncmm_ui_theme_accent( i );
            const nc_color border = active ? ( themed ? hilite( theme_accent ) : c_light_green ) :
                                    !dependency_focus ? c_dark_gray :
                                    border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                    border_style == NCMM_UI_BORDER_MAJOR ? c_yellow :
                                    border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                    themed ? theme_accent :
                                    owned ? c_cyan :
                                    major ? c_yellow :
                                    effect ? c_magenta : BORDER_COLOR;
            const nc_color text_color = active ? c_white :
                                        !dependency_focus ? c_dark_gray :
                                        locked || border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                                        border_style == NCMM_UI_BORDER_PRIME ? c_white :
                                        themed ? theme_accent : c_light_gray;
'@
$loader = Replace-TextBlock $loader $treeStyleOld876 $treeStyleNew876 'v8.7.6.1 dependency node focus'

# The marker expression itself exists in both card and tree renderers.  Anchor
# on the complete TREE rendering block, including mvwprintz(frame,...).  The card
# renderer uses card_win, so this contract is unique and independent of where the
# preceding text-color calculation sits in the function.
$treeMarkerOld876 = @'
            const char *marker = active ? ">" :
                                 border_style == NCMM_UI_BORDER_PRIME ? "*" :
                                 border_style == NCMM_UI_BORDER_MAJOR ? "+" :
                                 border_style == NCMM_UI_BORDER_EXCLUDED ? "x" :
                                 ( themed && ( active_ui_layout_flags & NCMM_UI_THEME_STRONG_BORDER ) ? "*" : nullptr );
            if( marker != nullptr ) {
                mvwprintz( frame, point( x + 1, y + 1 ),
                           border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                           ( themed ? theme_accent : c_light_green ), marker );
            }
'@
$treeMarkerNew876 = @'
            const char *marker = active ? ">" :
                                 !dependency_focus ? nullptr :
                                 border_style == NCMM_UI_BORDER_PRIME ? "*" :
                                 border_style == NCMM_UI_BORDER_MAJOR ? "+" :
                                 border_style == NCMM_UI_BORDER_EXCLUDED ? "x" :
                                 ( themed && ( active_ui_layout_flags & NCMM_UI_THEME_STRONG_BORDER ) ? "*" : nullptr );
            if( marker != nullptr ) {
                mvwprintz( frame, point( x + 1, y + 1 ),
                           border_style == NCMM_UI_BORDER_EXCLUDED ? c_dark_gray :
                           ( themed ? theme_accent : c_light_green ), marker );
            }
'@
$loader = Replace-TextBlock $loader $treeMarkerOld876 $treeMarkerNew876 'v8.7.6.6 tree-renderer dependency marker focus'

$sectionStatusOld876 = @'
                    } else if( folded[line] == "REQUIRES:" || folded[line] == "ТРЕБУЕТ:" ) {
                        detail_color = c_yellow;
                    }
'@
$sectionStatusNew876 = @'
                    } else if( folded[line] == "REQUIRES:" || folded[line] == "ТРЕБУЕТ:" ) {
                        detail_color = c_yellow;
                    } else if( folded[line] == "STATUS:" || folded[line] == "СТАТУС:" ) {
                        detail_color = c_cyan;
                    }
'@
$loader = Replace-TextBlock $loader $sectionStatusOld876 $sectionStatusNew876 'v8.7.6.1 status section color'

$treeFooterOld876 = @'
        std::string footer = tr_ui(
            "Arrows: nearest node  Mouse: hover/click/wheel  Tab: cards  Enter: details  Esc: back",
            "Стрелки: соседний узел  Мышь: наведение/клик/колесо  Tab: карточки  Enter: детали  Esc: назад" );
        footer += "  " + std::to_string( selected + 1 ) + "/" + std::to_string( node_count );
'@
$treeFooterNew876 = @'
        int hidden_left = 0;
        int hidden_right = 0;
        for( size_t i = 0; i < node_count; ++i ) {
            if( nodes[i].row < first_row || nodes[i].row >= first_row + visible_rows ) {
                continue;
            }
            const int x = node_x( i );
            if( x < 2 ) {
                ++hidden_left;
            } else if( x + node_width >= canvas_width + 2 ) {
                ++hidden_right;
            }
        }

        std::string footer = tr_ui(
            "Arrows: move  PgUp/PgDn: jump  Home/End: ends  Tab: cards  Enter: details  Esc: back",
            "Стрелки: ход  PgUp/PgDn: прыжок  Home/End: края  Tab: карточки  Enter: детали  Esc: назад" );
        if( hidden_left > 0 ) {
            footer = "< " + std::to_string( hidden_left ) + "  " + footer;
        }
        if( hidden_right > 0 ) {
            footer += "  " + std::to_string( hidden_right ) + " >";
        }
        footer += "  " + std::to_string( selected + 1 ) + "/" + std::to_string( node_count );
'@
$loader = Replace-TextBlock $loader $treeFooterOld876 $treeFooterNew876 'v8.7.6.1 offscreen indicators and context controls'
Write-Utf8NoBom $loaderPath $loader

# v8.7.6.1 structural guards: fail before MSVC if any clarity feature drifted.
$sp876Audit = Normalize-Lf ([IO.File]::ReadAllText($spPath))
foreach ($needle in @(
    'std::string perk_lock_reason( const perk_def &perk )',
    'std::string branch_next_unlock_text( branch_id branch )',
    'int integration_ready_count( const std::string &mod_id )',
    'std::string integration_next_unlock_text( const std::string &mod_id )',
    'tr( "STATUS:", "СТАТУС:" )',
    'tr( "Next unlock: ", "Следующее открытие: " )',
    'std::to_string( branch_unlocked_count( branch, blevel ) )'
)) {
    if (-not $sp876Audit.Contains($needle)) { throw "v8.7.6.1 Survivor clarity audit missing: $needle" }
}
$loader876Audit = Normalize-Lf ([IO.File]::ReadAllText($loaderPath))
foreach ($needle in @(
    'std::vector<bool> dependency_ancestor( node_count, false );',
    'std::vector<bool> dependency_descendant( node_count, false );',
    'const bool dependency_focus = dependency_related[i];',
    'int hidden_left = 0;',
    'PgUp/PgDn: jump',
    'folded[line] == "STATUS:"'
)) {
    if (-not $loader876Audit.Contains($needle)) { throw "v8.7.6.1 host clarity audit missing: $needle" }
}

# Refresh the final 0.9.15 snapshot AFTER API 1.8/runtime/contracts are complete.
# The earlier stage snapshot intentionally captured the content transform boundary;
# this second call changes its fingerprint and makes the published snapshot exact.
Save-SurvivorSourceSnapshot $Snap0915 "0.9.15"
New-Item -ItemType Directory -Force (Join-Path $Snap0915 "compat") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Snap0915 "ci") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Snap0915 "runtime") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Snap0915 "tests") | Out-Null
Copy-Item $mechanicsContractPath (Join-Path $Snap0915 "compat\survivor_mod_mechanics_v82.contract.txt") -Force
Copy-Item $worldSettingsContractPath (Join-Path $Snap0915 "compat\world_settings_v2_geography.contract.txt") -Force
Copy-Item (Join-Path $NcmmRoot "compat\contracts.json") (Join-Path $Snap0915 "compat\contracts.json") -Force
Copy-Item $revPathV8 (Join-Path $Snap0915 "ci\Get-PatchRevision.ps1") -Force
Copy-Item (Join-Path $NcmmRoot "ci\Test-SourceContracts.ps1") (Join-Path $Snap0915 "ci\Test-SourceContracts.ps1") -Force
Copy-Item (Join-Path $NcmmRoot "ci\Build-HostPackage.ps1") (Join-Path $Snap0915 "ci\Build-HostPackage.ps1") -Force
Copy-Item (Join-Path $NcmmRoot "runtime\NCMMBootstrap.cs") (Join-Path $Snap0915 "runtime\NCMMBootstrap.cs") -Force
Copy-Item (Join-Path $NcmmRoot "tests\smoke_host.cpp") (Join-Path $Snap0915 "tests\smoke_host.cpp") -Force
Copy-Item (Join-Path $NcmmRoot "host_patch\Apply-NCMMHostPatch.ps1") (Join-Path $Snap0915 "host_patch\Apply-NCMMHostPatch.ps1") -Force
Copy-Item (Join-Path $NcmmRoot "host_patch\ncmm_loader.h") (Join-Path $Snap0915 "host_patch\ncmm_loader.h") -Force
Copy-Item (Join-Path $NcmmRoot "host_patch\ncmm_fault_policy.h") (Join-Path $Snap0915 "host_patch\ncmm_fault_policy.h") -Force
Copy-Item (Join-Path $NcmmRoot "host_patch\ncmm_manifest_policy.h") (Join-Path $Snap0915 "host_patch\ncmm_manifest_policy.h") -Force
New-Item -ItemType Directory -Force (Join-Path $Snap0915 "mods\AdvancedWorldSettings\src") | Out-Null
Copy-Item $awsPath (Join-Path $Snap0915 "mods\AdvancedWorldSettings\src\aws.cpp") -Force
Copy-Item $awsManifestPath (Join-Path $Snap0915 "mods\AdvancedWorldSettings\mod.json") -Force

# Final cumulative audit.
$loaderAudit = [IO.File]::ReadAllText($loaderPath)
$spAudit = [IO.File]::ReadAllText($spPath)
$sdkAudit = [IO.File]::ReadAllText($sdkPath)
$manifestAudit = [IO.File]::ReadAllText($manifestPath)
$mechanicsContractAudit = [IO.File]::ReadAllText($mechanicsContractPath)
$revisionScriptAudit = [IO.File]::ReadAllText($revPathV8)
$worldSettingsContractAudit = [IO.File]::ReadAllText($worldSettingsContractPath)
$awsAudit = [IO.File]::ReadAllText($awsPath)
$awsManifestAudit = [IO.File]::ReadAllText($awsManifestPath)
$runtimeBootstrapAudit = [IO.File]::ReadAllText((Join-Path $NcmmRoot 'runtime\NCMMBootstrap.cs'))
if (-not $runtimeBootstrapAudit.Contains('private const string RuntimeVersion = "0.7.4";')) {
    throw 'NCMM runtime/bootstrap version drift detected: expected 0.7.4.'
}
if (-not $revisionScriptAudit.Contains('runtime/NCMMBootstrap.cs')) {
    throw 'NCMM bootstrap runtime is not bound into patch revision.'
}
$applyHostAudit = [IO.File]::ReadAllText((Join-Path $NcmmRoot 'host_patch\Apply-NCMMHostPatch.ps1'))
if ($applyHostAudit.Contains('0.7.2') -or
    -not $applyHostAudit.Contains('NCMM Host API v1 / NCMM 0.7.4 module contract')) {
    throw 'NCMM host patch marker/version drift detected: expected 0.7.4 everywhere.'
}
# Host API 2.0 migration boundary:
# the CDDA/Host mechanics contract must contain ONLY generic integration hooks.
# Survivor-specific modifier IDs stay in the generated Survivor module and are
# audited there instead of being required from the Host/CDDA source contract.
foreach ($needle in @(
    'ncmm_spell_source_modifier',
    'ncmm_shared_mana_modifier',
    'ncmm_spell_hook_id',
    'spell.cost_pct',
    'magic.mana.max_pct',
    'magic.mana.regen_pct',
    'ncmm_runtime_skill_bonus',
    'skill.',
    'combat.damage_to_species_pct',
    'combat.resist_from_species_pct',
    'combat.damage_avoid_pct',
    'combat.damage_taken_pct',
    'combat.dodge_attempts_bonus',
    'combat.free_dodge_attempts_bonus',
    'combat.block_attempts_bonus',
    'combat.melee_crit_chance_pct',
    'combat.melee_crit_damage_pct',
    'combat.ranged_crit_damage_pct'
)) {
    if (-not $mechanicsContractAudit.Contains($needle)) { throw "Host API 2.0 mechanics contract audit missing: $needle" }
}
if (-not $revisionScriptAudit.Contains('compat/survivor_mod_mechanics_v82.contract.txt')) {
    throw 'Host API 2.0 mechanics contract is not bound into patch revision.'
}
foreach ($needle in @(
    "0.9.11: obstacle-safe dependency graph",
    "std::vector<int> previous",
    "Never draw a partial connector",
    "bar_width = 18",
    "color_for_style",
    "v7: logical navigation follows the declared tree grid",
    "PgUp/PgDn: jump"
)) {
    if (-not $loaderAudit.Contains($needle)) { throw "0.9.11 host/UI audit missing: $needle" }
}
foreach ($needle in @(
    "mg_spellcraft_flat",
    "mg_mana_max_pct",
    "mg_spell_power_pct",
    "mom_metaphysics_flat",
    "mom_spell_cost_pct",
    "xe_deduction_flat",
    "xe_mana_regen_pct",
    "af_smartgun_flat",
    "afp_smartgun_flat",
    "secx_flesh_craft_flat",
    "secx_flesh_combat_flat",
    "sec_damage_pct",
    "sec_resist_pct",
    "sec_elite_damage_pct",
    "sec_elite_resist_pct",
    "sec_crimson_damage_pct",
    "sec_crimson_resist_pct"
)) {
    if (-not $spAudit.Contains($needle)) { throw "Survivor 0.9.15 mechanics audit missing: $needle" }
}
foreach ($needle in @(
    "activity_diversity_bonus_pct",
    "branch level up",
    "active_mods.v1",
    "world_mod_active",
    "specialization_allowed",
    "exclusive_specialization_root",
    "integration_branch_xp_bonus_pct",
    "Mod perks",
    "mod_perks.reserve( 24 )",
    "show_integration_branch",
    "MOD PERKS",
    "integration_tree_position",
    "perk_progression_level",
    "mg_archmage",
    "mom_transcendent_focus",
    "xe_boundary_master",
    "af_posthuman_operator",
    "integration_perk( perk ) || !perk_world_available",
    'active_world_mod( "magiclysm" )',
    'active_world_mod( "mindovermatter" )',
    'active_world_mod( "xedra_evolved" )',
    'active_world_mod( "aftershock_exoplanet" )',
    'active_world_mod( "aftershock_prime" )',
    'active_world_mod( "secronom" )',
    'active_world_mod( "secronom_lore_expansion" )',
    "Survivor Progression v0.9.15",
    '"0.9.15"'
)) {
    if (-not $spAudit.Contains($needle)) { throw "0.9.15 module audit missing: $needle" }
}
if (-not $sdkAudit.Contains('#define NCMM_API_VERSION_MINOR 8u')) {
    throw "0.9.15 expected NCMM API 1.8 after themed UI/experimental-page/registry extension."
}
if (-not $sdkAudit.Contains('int ( *world_mod_active )( const char *mod_id );')) {
    throw "0.9.13 SDK active-mod function audit failed."
}

if ($loaderAudit.Contains("mod->ident")) {
    throw "0.9.13 active-mod bridge still dereferences incomplete MOD_INFORMATION."
}
if (-not $loaderAudit.Contains("const mod_id wanted( requested_mod_id );") -or
    -not $loaderAudit.Contains("if( mod == wanted )")) {
    throw "0.9.13 active-mod direct-ID comparison audit failed."
}
if (-not $manifestAudit.Contains('"version": "0.9.15"') -or
    -not $manifestAudit.Contains('"api_min_minor": 8') -or
    -not $manifestAudit.Contains('"active_mods.v1"') -or
    -not $manifestAudit.Contains('"ui.theme.v1"') -or
    -not $manifestAudit.Contains('"active_mods.registry.v2"') -or
    -not $manifestAudit.Contains('"state_schema": 8')) {
    throw "0.9.15 manifest/API 1.8 audit failed."
}
# Count node DEFINITIONS only inside the perk array.  v8.1 counted every quoted
# mg_/mom_/xe_/af_/afp_/sec_/secx_ reference (prerequisites and modifier IDs included), so a valid
# 80-node tree was incorrectly reported as 207 integration nodes.
$perkAuditStart = $spAudit.IndexOf('const perk_def perks[] = {')
if ($perkAuditStart -lt 0) { throw "Perk-array audit start not found." }
$perkAuditEnd = $spAudit.IndexOf("`n};", $perkAuditStart)
if ($perkAuditEnd -lt 0) { throw "Perk-array audit end not found." }
$perkAudit = $spAudit.Substring($perkAuditStart, $perkAuditEnd - $perkAuditStart)

# v8.7.3: validate the separator BETWEEN consecutive perk initializers.
# A comma may legally be attached to the previous initializer OR placed alone on the
# following line (the historical core/effect boundary uses the latter form).  The
# previous v8.6.6.4 audit incorrectly rejected that valid standalone-comma layout.
$perkDefinitionMatches8665 = [regex]::Matches(
    $perkAudit,
    '(?m)^[ \t]*\{ "[^"]+",\s*branch_id::.*$'
)
if ($perkDefinitionMatches8665.Count -lt 2) {
    throw "v8.7.3 perk-array separator audit found too few rows: $($perkDefinitionMatches8665.Count)"
}
for ($rowIndex8665 = 0; $rowIndex8665 -lt $perkDefinitionMatches8665.Count - 1; ++$rowIndex8665) {
    $currentRow8665 = $perkDefinitionMatches8665[$rowIndex8665]
    $nextRow8665 = $perkDefinitionMatches8665[$rowIndex8665 + 1]
    $rowText8665 = $currentRow8665.Value.TrimEnd()
    $gapStart8665 = $currentRow8665.Index + $currentRow8665.Length
    $gapLength8665 = $nextRow8665.Index - $gapStart8665
    if ($gapLength8665 -lt 0) {
        throw "v8.7.3 perk-array separator audit produced an invalid row order."
    }
    $gapText8665 = if ($gapLength8665 -gt 0) {
        $perkAudit.Substring($gapStart8665, $gapLength8665).Trim()
    } else {
        ''
    }
    $inlineComma8665 = $rowText8665.EndsWith(',')
    if ($inlineComma8665) {
        if ($gapText8665 -ne '') {
            throw "v8.7.3 unexpected text/double separator before row $($rowIndex8665 + 2): $gapText8665"
        }
    } elseif ($gapText8665 -ne ',') {
        throw "v8.7.3 perk-array separator missing before row $($rowIndex8665 + 2): $rowText8665"
    }
}

$specMatches = [regex]::Matches($perkAudit, '(?m)^[ \t]*\{ "(spc_[^"]+)",')
$integrationMatches = [regex]::Matches(
    $perkAudit,
    '(?m)^[ \t]*\{ "((?:mg_|mom_|xe_|af_|afp_|sec_|secx_)[^"]+)",\s*branch_id::mastery,'
)
$specCount = $specMatches.Count
$integrationCount = $integrationMatches.Count
if ($specCount -ne 36) { throw "Expected 36 specialization nodes, found $specCount" }
if ($integrationCount -ne 161) { throw "Expected 161 mod integration node definitions, found $integrationCount" }

$integrationIdsAudit = @($integrationMatches | ForEach-Object { $_.Groups[1].Value })
$uniqueIntegrationIds = @($integrationIdsAudit | Sort-Object -Unique)
if ($uniqueIntegrationIds.Count -ne 161) {
    throw "Mod integration node IDs are not unique: $($uniqueIntegrationIds.Count) unique of 161 definitions."
}
foreach ($prefix in @("mg_","mom_","xe_","af_","afp_","sec_","secx_")) {
    $prefixCount = @($integrationIdsAudit | Where-Object { $_.StartsWith($prefix) }).Count
    if ($prefixCount -ne 23) { throw "Expected 23 $prefix mod integration node definitions, found $prefixCount" }
}
# v8.7.3 graph invariant: every integration is exactly one 23-node connected DAG (20 base + 3 Prime),
# prerequisites stay inside the same mod prefix, and all nodes are reachable from its root.
$integrationDefinitionLines = [regex]::Matches(
    $perkAudit,
    '(?m)^[ \t]*\{ "((?:mg_|mom_|xe_|af_|afp_|sec_|secx_)[^"]+)",\s*branch_id::mastery,\s*\d+,\s*\d+,\s*currency_id::(?:perk|major),\s*"([^"]*)",\s*"([^"]*)",'
)
$integrationPrereqs = @{}
foreach ($match in $integrationDefinitionLines) {
    $integrationPrereqs[$match.Groups[1].Value] = @($match.Groups[2].Value,$match.Groups[3].Value)
}
foreach ($prefix in @("mg_","mom_","xe_","af_","afp_","sec_","secx_")) {
    $ids = @($integrationIdsAudit | Where-Object { $_.StartsWith($prefix) })
    $idSet = @{}
    foreach ($id in $ids) { $idSet[$id] = $true }
    $roots = @()
    foreach ($id in $ids) {
        $parents = @($integrationPrereqs[$id] | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        if ($parents.Count -eq 0) { $roots += $id }
        foreach ($parent in $parents) {
            if (-not $idSet.ContainsKey($parent)) { throw "v8.3 cross/missing prerequisite in $prefix tree: $id -> $parent" }
        }
    }
    if ($roots.Count -ne 1) { throw "v8.3 expected one root in $prefix tree, found $($roots.Count)" }
    $visited = @{}
    $queue = New-Object System.Collections.Generic.Queue[string]
    $queue.Enqueue($roots[0])
    while ($queue.Count -gt 0) {
        $current = $queue.Dequeue()
        if ($visited.ContainsKey($current)) { continue }
        $visited[$current] = $true
        foreach ($candidate in $ids) {
            if ($visited.ContainsKey($candidate)) { continue }
            $parents = @($integrationPrereqs[$candidate] | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
            if ($parents -contains $current) { $queue.Enqueue($candidate) }
        }
    }
    if ($visited.Count -ne 23) { throw "v8.7.3 unreachable/cyclic nodes in $prefix tree: reached $($visited.Count)/23" }
}
$rankRulesStartV85 = $spAudit.IndexOf('static const ranked_perk_rule rules[] = {')
if ($rankRulesStartV85 -lt 0) { throw 'v8.5 ranked-skill rule table start not found.' }
$rankRulesEndV85 = $spAudit.IndexOf("`n    };", $rankRulesStartV85)
if ($rankRulesEndV85 -lt 0) { throw 'v8.5 ranked-skill rule table end not found.' }
$rankRulesBlockV85 = $spAudit.Substring($rankRulesStartV85, $rankRulesEndV85 - $rankRulesStartV85)
$rankRuleMatchesV85 = [regex]::Matches(
    $rankRulesBlockV85,
    '\{ "((?:c_|s_|m_|f_|g_|a_|mg_|mom_|xe_|af_|afp_|sec_|secx_)[^"]+)",\s*([35]),\s*([^}]+)\}'
)
$rankRuleCountV85 = $rankRuleMatchesV85.Count
if ($rankRuleCountV85 -ne 34) { throw "v8.6 expected exactly 34 ranked-skill rules, found $rankRuleCountV85" }
$rankRuleIdsV85 = @($rankRuleMatchesV85 | ForEach-Object { $_.Groups[1].Value })
$rankRuleUniqueV85 = @($rankRuleIdsV85 | Sort-Object -Unique)
if ($rankRuleUniqueV85.Count -ne 34) {
    throw "v8.6 ranked-skill rule IDs are not unique: $($rankRuleUniqueV85.Count) unique of 34."
}
foreach ($requiredRankV85 in @(
    'c_conditioning','s_field','m_light','f_hands','g_route','a_adapt',
    'mg_arcane_focus','mom_mental_focus','xe_anomaly_method','af_systems_operator',
    'afp_prime_operator','sec_field_researcher','secx_flesh_initiate'
)) {
    if ($rankRuleIdsV85 -notcontains $requiredRankV85) {
        throw "v8.6 required ranked-skill rule missing: $requiredRankV85"
    }
}
foreach ($needleV83 in @('rank_chevrons','▲','△','afp_prime_operator','sec_field_researcher','secx_flesh_initiate')) {
    if (-not $spAudit.Contains($needleV83)) { throw "v8.3 ranked/integration UI audit missing: $needleV83" }
}
$legacyIntegrationIdsAudit = @(
    'mg_arcane_focus','mg_battlemage','mg_ritual_craft','mg_wayfarer',
    'mom_mental_focus','mom_kinetic_control','mom_combat_focus','mom_neural_reserve',
    'xe_anomaly_method','xe_field_agent','xe_dimensional_hunter','xe_occult_engineer',
    'af_systems_operator','af_expedition_logistics','af_combat_technician','af_conditioning'
)
foreach ($legacyId in $legacyIntegrationIdsAudit) {
    if ($integrationIdsAudit -notcontains $legacyId) { throw "Save-compat legacy integration ID missing from v8.3 tree: $legacyId" }
}
if ($loaderAudit.Contains("style_for_node")) {
    throw "Obsolete 0.9.10 semantic connector-style helper survived 0.9.11 cleanup."
}
$connectorAuditNormalized = Normalize-Lf $loaderAudit
$connectorAuditStart = $connectorAuditNormalized.IndexOf("        // 0.9.11: obstacle-safe dependency graph.")
# Audit against the connector's OWN stable tail, not the following node renderer.
# The old range ended on a multiline CRLF here-string describing the next renderer;
# generated C++ is normalized to LF, so Windows PowerShell 5.1 could report a false
# "range not found" even though the complete BFS connector was present and valid.
$connectorAuditTailMarker = Normalize-Lf @'
        for( int y = header_height; y < frame_height - footer_height; ++y ) {
            for( int x = 1; x < divider_x; ++x ) {
                const int index = grid_index( x, y );
                if( edge_mask[index] == 0 ) {
                    continue;
                }
                const nc_color color = color_for_style( edge_style[index] );
                wattron( frame, color );
                mvwaddch( frame, point( x, y ), glyph_for_mask( edge_mask[index] ) );
                wattroff( frame, color );
            }
        }
'@
$connectorAuditTailMarker = $connectorAuditTailMarker.TrimEnd()
$connectorAuditTail = $connectorAuditNormalized.IndexOf($connectorAuditTailMarker, $connectorAuditStart)
if ($connectorAuditStart -lt 0 -or $connectorAuditTail -le $connectorAuditStart) {
    throw "0.9.11 connector audit range not found."
}
$connectorAuditEnd = $connectorAuditTail + $connectorAuditTailMarker.Length
$connectorAudit = $connectorAuditNormalized.Substring($connectorAuditStart, $connectorAuditEnd - $connectorAuditStart)
foreach ($forbidden in @("c_yellow","c_magenta","c_cyan","style_for_node")) {
    if ($connectorAudit.Contains($forbidden)) {
        throw "0.9.11 semantic connector color survived in routing block: $forbidden"
    }
}
if (-not $connectorAudit.Contains("Only expose the border tees after a COMPLETE connector exists.")) {
    throw "0.9.11 complete-route border-stub guard missing."
}
# v8.2 technical invariants.
foreach ($needle in @(
    'std::map<std::string, double, std::less<>> character_modifier_totals;',
    'void erase_module_modifiers( const std::string &module_id )',
    'const auto it = character_modifier_totals.find( modifier_id );'
)) {
    if (-not $loaderAudit.Contains($needle)) { throw "v8.2 host optimization audit missing: $needle" }
}
if ($loaderAudit.Contains('for( const auto &module : character_modifier_values )')) {
    throw 'v8.2 gameplay_modifier still scans every module.'
}
$rawEraseCountV82 = ([regex]::Matches($loaderAudit,[regex]::Escape('character_modifier_values.erase('))).Count
if ($rawEraseCountV82 -ne 1) {
    throw "v8.2 aggregate cache expected one raw map erase inside erase_module_modifiers, found $rawEraseCountV82"
}
$totalsClearCountV82 = ([regex]::Matches($loaderAudit,[regex]::Escape('character_modifier_totals.clear();'))).Count
if ($totalsClearCountV82 -ne 2) {
    throw "v8.2 aggregate cache expected two total-map reset sites, found $totalsClearCountV82"
}
foreach ($sharedMana in @('mg_mana_max_pct','mg_mana_regen_pct','xe_mana_max_pct','xe_mana_regen_pct')) {
    if (-not $spAudit.Contains('"' + $sharedMana + '"')) {
        throw "Survivor 0.9.15 shared-mana modifier definition missing: $sharedMana"
    }
}
foreach ($needle in @('std::string_view','specialization_root_slot','std::map<std::string_view, const perk_def *> index','std::map<std::string_view, std::string> keys','std::map<std::string_view, integration_id> registry','const std::string &perk_key')) {
    if (-not $spAudit.Contains($needle)) { throw "v8.2 Survivor structure audit missing: $needle" }
}
# v8.2.2 regression guard: the cached perk-key optimization must never consume
# the ranked-perk helper block that follows perk_key() in the generated source.
$rankHelperDefsV822 = @(
    'struct ranked_perk_rule {',
    'const ranked_perk_rule *ranked_perk_rule_for( const perk_def &perk )',
    'bool ranked_perk_id( const perk_def &perk )',
    'int perk_max_rank( const perk_def &perk )',
    'double perk_extra_rank_scale( const perk_def &perk )',
    'int perk_rank( const perk_def &perk )',
    'bool perk_maxed( const perk_def &perk )',
    'double perk_rank_multiplier_for( const perk_def &perk, int rank )',
    'double perk_rank_multiplier( const perk_def &perk )',
    'std::string rank_roman( int rank )',
    'std::string perk_display_name( const perk_def &perk )'
)
foreach ($signatureV822 in $rankHelperDefsV822) {
    $countV822 = ([regex]::Matches($spAudit,[regex]::Escape($signatureV822))).Count
    if ($countV822 -ne 1) {
        throw "v8.2.2 ranked-perk helper audit expected exactly one '$signatureV822', found $countV822"
    }
}
$perkKeyPosV822 = $spAudit.IndexOf('const std::string &perk_key( const perk_def &perk )')
$rankStructPosV85 = $spAudit.IndexOf('struct ranked_perk_rule {')
$rankRuleForPosV85 = $spAudit.IndexOf('const ranked_perk_rule *ranked_perk_rule_for( const perk_def &perk )')
$rankedIdPosV822 = $spAudit.IndexOf('bool ranked_perk_id( const perk_def &perk )')
$maxRankPosV822 = $spAudit.IndexOf('int perk_max_rank( const perk_def &perk )')
$rankPosV822 = $spAudit.IndexOf('int perk_rank( const perk_def &perk )')
$displayNamePosV822 = $spAudit.IndexOf('std::string perk_display_name( const perk_def &perk )')
$findPerkPosV822 = $spAudit.IndexOf('const perk_def *find_perk( const char *id )')
if ($perkKeyPosV822 -lt 0 -or $rankStructPosV85 -le $perkKeyPosV822 -or
    $rankRuleForPosV85 -le $rankStructPosV85 -or $rankedIdPosV822 -le $rankRuleForPosV85 -or
    $maxRankPosV822 -le $rankedIdPosV822 -or $rankPosV822 -le $maxRankPosV822 -or
    $displayNamePosV822 -le $rankPosV822 -or $findPerkPosV822 -le $displayNamePosV822) {
    throw 'v8.6 ranked-perk rule table/helper declaration order is invalid for C++ compilation.'
}
if ($spAudit.Contains('id.find( "juggernaut"') -or $spAudit.Contains('id.find( "selfteacher"')) {
    throw 'v8.2 specialization logic still depends on substring heuristics.'
}
if ($spAudit.Contains('id.rfind( "mg_"') -or $spAudit.Contains('id.rfind( "mom_"') -or
    $spAudit.Contains('id.rfind( "xe_"') -or $spAudit.Contains('id.rfind( "af_"') -or
    $spAudit.Contains('id.rfind( "afp_"') -or $spAudit.Contains('id.rfind( "sec_"') -or
    $spAudit.Contains('id.rfind( "secx_"')) {
    throw 'v8.2 integration classification still depends on namespace-prefix heuristics.'
}
$loaderHeaderAuditV82 = [IO.File]::ReadAllText((Join-Path $NcmmRoot 'host_patch\ncmm_loader.h'))
# Host API 2.0 replaces the old contextual-Metaphysics bridge with a generic
# source-mod context.  The concrete Survivor modifier IDs are module-owned.
foreach ($needle in @('ncmm_spell_source_scope','runtime_source_mod_swap','ncmm_spell_skill_level')) {
    if (-not $mechanicsContractAudit.Contains($needle)) {
        throw "Host API 2.0 source-context mechanics contract missing: $needle"
    }
}
foreach ($needle in @(
    'ncmm_spell_hook_id',
    'ncmm_spell_source_modifier',
    'ncmm_spell_skill_level',
    'ncmm_spell_source_scope',
    'ncmm_shared_mana_modifier',
    'ncmm_runtime_skill_bonus',
    'spell.cost_pct',
    'spell.cast_time_pct',
    'spell.failure_pct',
    'spell.experience_pct',
    'spell.power_pct',
    'spell.range_pct',
    'spell.area_pct',
    'spell.duration_pct',
    'magic.mana.max_pct',
    'magic.mana.regen_pct',
    'combat.damage_to_species_pct',
    'combat.resist_from_species_pct',
    'combat.damage_avoid_pct',
    'combat.damage_taken_pct',
    'combat.dodge_attempts_bonus',
    'combat.free_dodge_attempts_bonus',
    'combat.block_attempts_bonus',
    'combat.melee_crit_chance_pct',
    'combat.melee_crit_damage_pct',
    'combat.ranged_crit_damage_pct',
    'spell::effect_area',
    'spell::damage_dot',
    'bool spell::in_aoe('
)) {
    if (-not $mechanicsContractAudit.Contains($needle)) { throw "Host API 2.0 mechanics optimization audit missing: $needle" }
}
if ($mechanicsContractAudit.Contains('const std::string sid = ident.str()')) {
    throw 'Host API 2.0 obsolete skill-string hot-path hook remains in mechanics contract.'
}


foreach ($needle in @(
    'typedef struct ncmm_ui_theme_v1 {',
    'const uint32_t *item_accents;',
    'ui_card_choose_themed',
    'ui_tree_choose_themed',
    'worldgen_experimental_group_begin'
)) {
    if (-not $sdkAudit.Contains($needle)) { throw "NCMM API 1.8 SDK audit missing: $needle" }
}
foreach ($needle in @(
    '"ui.theme.v1"',
    '"world_options.experimental.v1"',
    'ncmm_ui_theme_scope',
    'ncmm_ui_theme_accent',
    'ui_card_choose_themed(',
    'ui_tree_choose_themed(',
    'worldgen_experimental_group_begin('
)) {
    if (-not $loaderAudit.Contains($needle)) { throw "NCMM 0.7.4 themed/experimental/registry host audit missing: $needle" }
}
foreach ($needle in @(
    'branch_theme_color',
    'integration_theme_color',
    'compact_tree_badge',
    'ui_tree_choose_themed',
    'ui_card_choose_themed',
    'NCMM_UI_THEME_WIDE_NODES'
)) {
    if (-not $spAudit.Contains($needle)) { throw "Survivor themed UI audit missing: $needle" }
}
foreach ($needle in @(
    'ncmm_ensure_experimental_page',
    'ncmm_begin_experimental_group',
    'ncmm_experimental'
)) {
    if (-not $worldSettingsContractAudit.Contains($needle)) { throw "Experimental world-page contract audit missing: $needle" }
}
if (-not $awsAudit.Contains('worldgen_experimental_group_begin') -or
    -not $awsAudit.Contains('"Time and calendar"') -or
    -not $awsAudit.Contains('worldgen_group_begin( group_id')) {
    throw 'AWS experimental split audit failed: vanilla time controls and synthetic geography are not separated.'
}

foreach ($needle in @('world_settings.v2','world_setting_register_bool','world_setting_register_int','world_setting_register_float','world_setting_get_i64','world_setting_get_f64')) {
    if ($needle -eq 'world_settings.v2') {
        if (-not ($loaderAudit.Contains($needle) -or $sdkAudit.Contains($needle) -or $sdkAudit.Contains('capability: world_settings.v2'))) {
            throw "World Settings API v2 capability audit missing: $needle"
        }
    } elseif (-not $loaderAudit.Contains($needle) -or -not $sdkAudit.Contains($needle)) {
        throw "World Settings API v2 host/SDK audit missing: $needle"
    }
}
if (-not $sdkAudit.Contains('#define NCMM_API_VERSION_MINOR 8u')) { throw 'World Settings API v2 + UI theme + mod registry expected NCMM API 1.8.' }
$awsGeoMatchesV86 = [regex]::Matches(
    $awsAudit,
    'reg_(?:bool|int|float)\(\s*api,ru,"(NCMM_AWS_[A-Z0-9_]+)"'
)
if ($awsGeoMatchesV86.Count -ne 48) {
    throw "AWS 0.6 expected exactly 48 registered geography settings, found $($awsGeoMatchesV86.Count)."
}
$awsGeoIdsV86 = @($awsGeoMatchesV86 | ForEach-Object { $_.Groups[1].Value })
$awsGeoUniqueV86 = @($awsGeoIdsV86 | Sort-Object -Unique)
if ($awsGeoUniqueV86.Count -ne 48) {
    throw "AWS 0.6 geography setting IDs are not unique: $($awsGeoUniqueV86.Count) unique of 48."
}
if ($awsGeoIdsV86 -notcontains 'NCMM_AWS_CUSTOM_GEOGRAPHY') {
    throw 'AWS 0.6 custom-geography master switch missing from registered settings.'
}
if (-not $awsAudit.Contains('"NCMM_AWS_CUSTOM_GEOGRAPHY","Use custom geography"') -or
    -not $awsAudit.Contains("Leave this off to use the world's normal geography.") -or
    -not $awsAudit.Contains('Оставьте выключенным для обычной географии мира.')) {
    throw 'AWS 0.6 custom-geography OFF-by-default preservation contract missing.'
}
foreach ($geoIdV86 in $awsGeoUniqueV86) {
    if (-not $worldSettingsContractAudit.Contains($geoIdV86)) {
        throw "World Settings v2 engine contract does not reference registered geography setting: $geoIdV86"
    }
}
foreach ($needle in @('NCMM_AWS_CUSTOM_GEOGRAPHY','NCMM_AWS_CITY_SIZE','NCMM_AWS_CITY_SPACING','NCMM_AWS_MEGACITY','NCMM_AWS_SHOP_RADIUS','NCMM_AWS_PARK_RADIUS','NCMM_AWS_FOREST_THRESHOLD','NCMM_AWS_RIVER_FREQUENCY','NCMM_AWS_LAKE_THRESHOLD','NCMM_AWS_OCEAN_THRESHOLD','NCMM_AWS_HIGHWAY_GRID_ROW','NCMM_AWS_RAVINE_COUNT')) {
    if (-not $awsAudit.Contains($needle)) { throw "AWS 0.6 geography setting missing: $needle" }
    if (-not $worldSettingsContractAudit.Contains($needle)) { throw "World Settings v2 geography contract missing: $needle" }
}
foreach ($needle in @('ncmm_disable_forests','ncmm_custom_grid','ncmm_lakes_enabled','ncmm_oceans_disabled','set_options( int row_override')) {
    if (-not $worldSettingsContractAudit.Contains($needle)) {
        throw "World Settings v2 consistency guard missing from geography contract: $needle"
    }
}
if (-not $awsManifestAudit.Contains('"version": "0.6.1"') -or
    -not $awsManifestAudit.Contains('"api_min_minor": 7') -or
    -not $awsManifestAudit.Contains('"world_settings.v2"') -or
    -not $awsManifestAudit.Contains('"world_options.experimental.v1"')) {
    throw 'AWS 0.6.1 manifest/API 1.7 contract audit failed.'
}
if (-not $revisionScriptAudit.Contains('compat/world_settings_v2_geography.contract.txt')) {
    throw 'World Settings v2 geography contract is not bound into patch revision.'
}

# v8.7.3 Prime/registry integrity gate.
if (-not $sdkAudit.Contains('"active_mods.registry.v2"') -and -not $loaderAudit.Contains('"active_mods.registry.v2"')) {
    throw 'NCMM API 1.8 active_mods.registry.v2 capability missing.'
}
foreach ($needle in @('world_mod_count','world_mod_id','active_supported_integration_mods','mod_prime_root_slot','exclusive_specialization_allowed')) {
    if (-not $spAudit.Contains($needle) -and -not $loaderAudit.Contains($needle) -and -not $sdkAudit.Contains($needle)) {
        throw "v8.7.3 registry/Prime audit missing: $needle"
    }
}
foreach ($needle in @(
    'size_t ( *world_mod_count )();',
    'const char *( *world_mod_id )( size_t index );'
)) {
    if (-not $sdkAudit.Contains($needle)) { throw "NCMM API 1.8 SDK registry tail missing: $needle" }
}
foreach ($needle in @(
    'size_t world_mod_count()',
    'const char *world_mod_id( size_t index )',
    '"active_mods.registry.v2"',
    '&world_mod_count',
    '&world_mod_id'
)) {
    if (-not $loaderAudit.Contains($needle)) { throw "NCMM 0.7.4 registry host audit missing: $needle" }
}
$primeRootIds866 = @(
    'spc_c_juggernaut','spc_c_duelist','spc_c_tactician','spc_s_nomad','spc_s_medic','spc_s_quartermaster',
    'spc_m_sprinter','spc_m_ghost','spc_m_pathfinder','spc_f_systems','spc_f_improviser','spc_f_researcher',
    'spc_g_prospector','spc_g_courier','spc_g_investigator','spc_a_specialist','spc_a_polymath','spc_a_selfteacher',
    'mg_prime_arcanist','mg_prime_channeler','mg_prime_warcaster','mom_prime_kinetic','mom_prime_overclock','mom_prime_ascetic',
    'xe_prime_analyst','xe_prime_resonant','xe_prime_riftwalker','af_prime_smartgun','af_prime_systems','af_prime_phase',
    'afp_prime_gunslinger','afp_prime_systems_specialist','afp_prime_translocator','sec_prime_hunter','sec_prime_bulwark','sec_prime_crimson',
    'secx_prime_architect','secx_prime_predator','secx_prime_vessel'
)
foreach ($primeId866 in $primeRootIds866) {
    $pattern866 = '(?m)^[ 	]*\{ "' + [regex]::Escape($primeId866) + '",\s*branch_id::'
    $count866 = ([regex]::Matches($spAudit,$pattern866)).Count
    if ($count866 -ne 1) { throw "v8.7.3 Prime perk-definition uniqueness failed: $primeId866 count=$count866" }
}
if ($primeRootIds866.Count -ne 39) { throw "v8.7.3 expected 39 Prime roots, found $($primeRootIds866.Count)" }
$integrationRows866 = [regex]::Matches($spAudit,'\{ "(?:mg|mom|xe|af|afp|sec|secx)_[^"]+",\s*branch_id::')
if ($integrationRows866.Count -ne 161) { throw "v8.7.3 expected 161 mod-native perk rows, found $($integrationRows866.Count)" }
foreach ($secNeedle866 in @(
    '{ "sec_damage_pct", { -75.0, 75.0 } }',
    '{ "sec_resist_pct", { -75.0, 75.0 } }',
    '{ "sec_crimson_resist_pct", { -75.0, 75.0 } }'
)) {
    if (-not $loaderAudit.Contains($secNeedle866)) { throw "v8.7.3 Secronom tradeoff range missing: $secNeedle866" }
}

Write-Host "0.9.11-0.9.15 cumulative source audit: PASS (v8.7.6.8 cache-marker hotfix + v8.7.6.7 connector audit + v8.7.6.6 runtime infrastructure)" -ForegroundColor Green
Set-InfrastructureTransactionPhase "legacy_generate" "passed" "0.9.11-0.9.15 deterministic generation/audit complete"
Write-Host "  Balanced build profile default + Safe/Maximum overrides + bounded MSBuild/CL/vcpkg concurrency"
Write-Host "  0.9.11 full-block BFS connector replacement + neutral lines + no orphan border stubs + branch progress bar"
Write-Host "  0.9.12 diversity-aware branch XP + level-up feedback"
Write-Host "  0.9.13 active_mods.v1 / direct mod_id comparison"
Write-Host "  0.9.15 Prime tradeoffs: 18 core Prime roots + 21 mod Prime roots; 161 mod-native nodes; schema 8"
Write-Host "  World Settings API v2 / NCMM API 1.8 + active_mods.registry.v2 + Advanced World Settings 0.6.1"
Write-Host "  Locale refresh: changing USE_LANG now redispatches NCMM locale callbacks and refreshes world-option labels"

# ---------------------------------------------------------------------------
# Optional Recipe Finalization Profiler support.
# Dormant unless code_mods\NCMM_Recipe_Finalization_Profiler\profile.enabled exists
# and that module directory is not disabled.  No gameplay data or recipe semantics
# are changed; the hook only records timing around existing finalize calls.
# ---------------------------------------------------------------------------
function Apply-RecipeFinalizeProfilerSupportPatch([string]$SourceRoot) {
    $recipeDictPath = Join-Path $SourceRoot 'src\recipe_dictionary.cpp'
    $supportMarker = Join-Path $SourceRoot '.ncmm_recipe_finalize_profiler_v1'
    if (-not (Test-Path $recipeDictPath -PathType Leaf)) {
        throw "Recipe profiler support source missing: $recipeDictPath"
    }

    $text = Normalize-Lf ([IO.File]::ReadAllText($recipeDictPath))
    $requiredNeedles = @(
        'ncmm_recipe_profiler_requested',
        'ncmm_recipe_profiler_write_report',
        'NCMM_Recipe_Finalization_Profiler',
        'profile.enabled'
    )
    if (Test-Path $supportMarker -PathType Leaf) {
        $supportComplete = $true
        foreach ($needle in $requiredNeedles) {
            if (-not $text.Contains($needle)) {
                $supportComplete = $false
                break
            }
        }
        if ($supportComplete) {
            Write-Host 'Recipe finalization profiler support already present and verified.' -ForegroundColor Green
            return
        }
        Write-Host 'Stale recipe-profiler marker detected after pristine source reset; rebuilding support patch.' -ForegroundColor Yellow
        Remove-Item -LiteralPath $supportMarker -Force -ErrorAction SilentlyContinue
    }

    if ($text.Contains('ncmm_recipe_profiler_requested')) {
        throw 'Partial/interrupted recipe profiler support detected; clean pinned source is required.'
    }

    $profilerIncludesOld = '#include <chrono>'
    $profilerIncludesNew = @'
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
'@
    $text = Replace-TextBlock $text $profilerIncludesOld $profilerIncludesNew 'recipe-profiler includes'

    $namespaceClose = '} // namespace'
    $namespaceIndex = $text.IndexOf($namespaceClose)
    if ($namespaceIndex -lt 0) { throw 'Recipe profiler namespace insertion anchor missing.' }
    $profilerHelpers = @'
struct ncmm_recipe_profile_entry {
    std::string phase;
    std::string recipe_id;
    std::string source_mod;
    double milliseconds = 0.0;
};

struct ncmm_recipe_profile_state {
    bool enabled = false;
    std::string phase;
    std::chrono::steady_clock::time_point total_start;
    double pre_ms = 0.0;
    double recipes_ms = 0.0;
    double uncraft_ms = 0.0;
    std::vector<ncmm_recipe_profile_entry> entries;
};

ncmm_recipe_profile_state &ncmm_recipe_profiler()
{
    static ncmm_recipe_profile_state state;
    return state;
}

bool ncmm_recipe_profiler_requested()
{
    const std::filesystem::path dir = std::filesystem::current_path() / "code_mods" /
                                      "NCMM_Recipe_Finalization_Profiler";
    return std::filesystem::exists( dir / "profile.enabled" ) &&
           !std::filesystem::exists( dir / "disabled" );
}

double ncmm_recipe_profile_ms( const std::chrono::steady_clock::time_point &start )
{
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start ).count();
}

void ncmm_recipe_profiler_write_report( double total_ms )
{
    ncmm_recipe_profile_state &profile = ncmm_recipe_profiler();
    if( !profile.enabled ) {
        return;
    }

    const std::filesystem::path ncmm_dir = std::filesystem::current_path() / "ncmm";
    std::error_code ec;
    std::filesystem::create_directories( ncmm_dir, ec );
    const std::filesystem::path report_path = ncmm_dir / "recipe_finalize_profile.txt";
    std::ofstream out( report_path, std::ios::trunc | std::ios::binary );
    if( !out ) {
        return;
    }

    std::map<std::string, std::pair<double, std::size_t>> by_source;
    for( const ncmm_recipe_profile_entry &entry : profile.entries ) {
        if( entry.phase != "recipes" ) {
            continue;
        }
        auto &bucket = by_source[entry.source_mod];
        bucket.first += entry.milliseconds;
        ++bucket.second;
    }

    std::vector<ncmm_recipe_profile_entry> slowest = profile.entries;
    std::sort( slowest.begin(), slowest.end(),
    []( const ncmm_recipe_profile_entry & a, const ncmm_recipe_profile_entry & b ) {
        return a.milliseconds > b.milliseconds;
    } );

    std::vector<std::pair<std::string, std::pair<double, std::size_t>>> sources(
        by_source.begin(), by_source.end() );
    std::sort( sources.begin(), sources.end(),
    []( const auto & a, const auto & b ) {
        return a.second.first > b.second.first;
    } );

    const double post_ms = std::max( 0.0, total_ms - profile.pre_ms -
                                     profile.recipes_ms - profile.uncraft_ms );
    out << "NCMM Recipe Finalization Profiler v1\n";
    out << "This report changes no recipe data; timings are diagnostic only.\n\n";
    out << std::fixed << std::setprecision( 3 );
    out << "TOTAL_MS=" << total_ms << '\n';
    out << "STAGE_PREP_MS=" << profile.pre_ms << '\n';
    out << "STAGE_RECIPES_MS=" << profile.recipes_ms << '\n';
    out << "STAGE_UNCRAFT_MS=" << profile.uncraft_ms << '\n';
    out << "STAGE_POST_MS=" << post_ms << '\n';
    out << "TIMED_FINALIZE_CALLS=" << profile.entries.size() << "\n\n";

    out << "BY_SOURCE_MOD (recipe::finalize time only)\n";
    out << "------------------------------------------\n";
    for( const auto &source : sources ) {
        out << source.first << " | " << source.second.first << " ms | "
            << source.second.second << " recipes\n";
    }

    out << "\nSLOWEST recipe::finalize CALLS\n";
    out << "------------------------------\n";
    const std::size_t limit = std::min<std::size_t>( slowest.size(), 80 );
    for( std::size_t i = 0; i < limit; ++i ) {
        const ncmm_recipe_profile_entry &entry = slowest[i];
        out << ( i + 1 ) << ". " << entry.milliseconds << " ms | "
            << entry.phase << " | " << entry.source_mod << " | "
            << entry.recipe_id << '\n';
    }

    out << "\nInterpretation:\n";
    out << "- If STAGE_RECIPES_MS dominates, inspect BY_SOURCE_MOD / SLOWEST calls.\n";
    out << "- If STAGE_POST_MS dominates, the expensive work is after individual recipe finalization\n";
    out << "  (book/reversible processing, nested validation, caches, food-loop analysis).\n";
    out.flush();
}

'@
    $text = $text.Substring(0,$namespaceIndex) + (Normalize-Lf $profilerHelpers).TrimEnd() + "`n`n" + $text.Substring($namespaceIndex)

    $finalizeInternalOld = @'
    for( auto &elem : obj ) {
        erase_if( elem.second.nested_category_data, [&]( const recipe_id & nest ) {
            return !nest.is_valid() || nest->will_be_blacklisted();
        } );
        elem.second.finalize();
        inp_mngr.pump_events();
    }
'@
    $finalizeInternalNew = @'
    for( auto &elem : obj ) {
        erase_if( elem.second.nested_category_data, [&]( const recipe_id & nest ) {
            return !nest.is_valid() || nest->will_be_blacklisted();
        } );
        ncmm_recipe_profile_state &profile = ncmm_recipe_profiler();
        if( profile.enabled ) {
            const auto started = std::chrono::steady_clock::now();
            elem.second.finalize();
            std::string source_mod = "core_or_unknown";
            if( !elem.second.src.empty() ) {
                source_mod = elem.second.src.back().second.str();
                if( source_mod.empty() ) {
                    source_mod = "core_or_unknown";
                }
            }
            profile.entries.push_back( { profile.phase, elem.first.str(), source_mod,
                                         ncmm_recipe_profile_ms( started ) } );
        } else {
            elem.second.finalize();
        }
        inp_mngr.pump_events();
    }
'@
    $text = Replace-TextBlock $text $finalizeInternalOld $finalizeInternalNew 'recipe-profiler per-recipe timing'

    $finalizeStartOld = @'
void recipe_dictionary::finalize()
{
    DynamicDataLoader::get_instance().load_deferred( deferred );

    // remove abstract recipes
    delete_if( []( const recipe & element ) {
        return element.abstract;
    } );

    finalize_internal( recipe_dict.recipes );
    finalize_internal( recipe_dict.uncraft );
'@
    $finalizeStartNew = @'
void recipe_dictionary::finalize()
{
    ncmm_recipe_profile_state &profile = ncmm_recipe_profiler();
    profile = ncmm_recipe_profile_state{};
    profile.enabled = ncmm_recipe_profiler_requested();
    if( profile.enabled ) {
        profile.total_start = std::chrono::steady_clock::now();
    }

    const auto prep_started = std::chrono::steady_clock::now();
    DynamicDataLoader::get_instance().load_deferred( deferred );

    // remove abstract recipes
    delete_if( []( const recipe & element ) {
        return element.abstract;
    } );
    if( profile.enabled ) {
        profile.pre_ms = ncmm_recipe_profile_ms( prep_started );
    }

    profile.phase = "recipes";
    const auto recipes_started = std::chrono::steady_clock::now();
    finalize_internal( recipe_dict.recipes );
    if( profile.enabled ) {
        profile.recipes_ms = ncmm_recipe_profile_ms( recipes_started );
    }

    profile.phase = "uncraft";
    const auto uncraft_started = std::chrono::steady_clock::now();
    finalize_internal( recipe_dict.uncraft );
    if( profile.enabled ) {
        profile.uncraft_ms = ncmm_recipe_profile_ms( uncraft_started );
    }
    profile.phase = "post";
'@
    $text = Replace-TextBlock $text $finalizeStartOld $finalizeStartNew 'recipe-profiler finalize entry/stages'

    $finalizeEndOld = @'
    recipe_dict.find_items_on_loops();
}

void recipe_dictionary::check_consistency()
'@
    $finalizeEndNew = @'
    recipe_dict.find_items_on_loops();

    if( profile.enabled ) {
        const double total_ms = ncmm_recipe_profile_ms( profile.total_start );
        ncmm_recipe_profiler_write_report( total_ms );
        profile.enabled = false;
    }
}

void recipe_dictionary::check_consistency()
'@
    $text = Replace-TextBlock $text $finalizeEndOld $finalizeEndNew 'recipe-profiler finalize report'

    foreach ($needle in $requiredNeedles) {
        if (-not $text.Contains($needle)) {
            throw "Recipe profiler support post-check failed: $needle"
        }
    }
    Write-Utf8NoBom $recipeDictPath $text
    Write-Utf8NoBom $supportMarker "NCMM recipe finalization profiler support v1`n"
    Write-Host 'Recipe finalization profiler support: READY (dormant until profiler module is enabled)' -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# v8.7.6.6 Runtime Infrastructure hardening.
# - type-safe NCMM world-option reads for geography switches
# - NCMM-aware debug/support routing
# - human-readable manager failure reasons
# - durable diagnostics summary
# ---------------------------------------------------------------------------
function Apply-NcmmRuntimeInfrastructureV8766([string]$Root) {
    $marker = Join-Path $Root '.ncmm_runtime_infra_v8766'
    $optionsH = Join-Path $Root 'src\options.h'
    $optionsCpp = Join-Path $Root 'src\options.cpp'
    $debugCpp = Join-Path $Root 'src\debug.cpp'
    $loaderCpp = Join-Path $Root 'src\ncmm_loader.cpp'
    foreach ($required in @($optionsH,$optionsCpp,$debugCpp,$loaderCpp)) {
        if (-not (Test-Path $required -PathType Leaf)) {
            throw "NCMM v8.7.6.6 runtime infrastructure source missing: $required"
        }
    }

    $verify = {
        $hCheck = Normalize-Lf ([IO.File]::ReadAllText($optionsH))
        $oCheck = Normalize-Lf ([IO.File]::ReadAllText($optionsCpp))
        $dCheck = Normalize-Lf ([IO.File]::ReadAllText($debugCpp))
        $lCheck = Normalize-Lf ([IO.File]::ReadAllText($loaderCpp))
        foreach ($needle in @(
            'ncmm_get_option_bool_or',
            'NCMM v8.7.6.6 fail-safe accessor definitions',
            'https://github.com/Neversalimus/NCMM/issues',
            'manager_reason_text',
            'diagnostics.txt'
        )) {
            if (-not ($hCheck.Contains($needle) -or $oCheck.Contains($needle) -or $dCheck.Contains($needle) -or $lCheck.Contains($needle))) {
                throw "NCMM v8.7.6.6 runtime infrastructure post-check missing: $needle"
            }
        }
    }

    if (Test-Path $marker -PathType Leaf) {
        try {
            & $verify
            Write-Host 'NCMM v8.7.6.6 runtime infrastructure already present and verified.' -ForegroundColor Green
            return
        } catch {
            Write-Host 'Stale runtime-infrastructure marker detected after pristine source reset; rebuilding runtime patch.' -ForegroundColor Yellow
            Remove-Item -LiteralPath $marker -Force -ErrorAction SilentlyContinue
        }
    }

    # Shared fail-safe getters. They never call value_as<T>() unless the option really has that type,
    # preventing VOID-option debug storms if an NCMM setting is missing or registration is partial.
    $h = Normalize-Lf ([IO.File]::ReadAllText($optionsH))
    if (-not $h.Contains('bool ncmm_get_option_bool_or( const std::string &name, bool fallback );')) {
        $getterAnchor = @'
template<typename T>
inline T get_option( const std::string &name, bool convert = false )
{
    return get_options().get_option( name ).value_as<T>( convert );
}
'@
        $getterReplacement = @'
template<typename T>
inline T get_option( const std::string &name, bool convert = false )
{
    return get_options().get_option( name ).value_as<T>( convert );
}

/** NCMM fail-safe accessors for optional settings injected by native modules.
 *  Definitions live in options.cpp after cOpt::value_as<T> explicit specializations.
 *  Keeping calls out of this header prevents premature template instantiation on MSVC.
 */
bool ncmm_get_option_bool_or( const std::string &name, bool fallback );
int ncmm_get_option_int_or( const std::string &name, int fallback );
float ncmm_get_option_float_or( const std::string &name, float fallback );
'@
        $h = Replace-TextBlock $h $getterAnchor $getterReplacement 'v8.7.6.6 safe NCMM option getters'
        Write-Utf8NoBom $optionsH $h
    }

    # Repair stale/partial active-world copies transactionally on re-registration.
    # This is the root fix for cOpt::value_as<bool>() being called on a VOID copy after older builds.
    $oc = Normalize-Lf ([IO.File]::ReadAllText($optionsCpp))

    # MSVC requires explicit cOpt::value_as<T> specializations to be seen before any
    # translation-unit call can instantiate those template arguments.  The old inline
    # header helpers called value_as<bool/int/float>() too early and caused C2908/C2910.
    if (-not $oc.Contains('NCMM v8.7.6.6 fail-safe accessor definitions')) {
        $valueAsIntAnchor = @'
template<>
int options_manager::cOpt::value_as<int>( bool convert ) const
{
    if( std::optional<int> ret = _convert<int>(); convert && ret ) {
        return *ret;
    }
    if( eType != CVT_INT ) {
        debugmsg( "%s tried to get integer value from option of type %s", sName, sType );
    }
    return iSet;
}
'@
        $safeGetterDefinitions = @'
// NCMM v8.7.6.6 fail-safe accessor definitions.
// These intentionally live after cOpt::value_as<T> explicit specializations.
bool ncmm_get_option_bool_or( const std::string &name, bool fallback )
{
    options_manager &opts = get_options();
    if( !opts.has_option( name ) ) {
        return fallback;
    }
    options_manager::cOpt &opt = opts.get_option( name );
    return opt.getType() == "bool" ? opt.value_as<bool>() : fallback;
}

int ncmm_get_option_int_or( const std::string &name, int fallback )
{
    options_manager &opts = get_options();
    if( !opts.has_option( name ) ) {
        return fallback;
    }
    options_manager::cOpt &opt = opts.get_option( name );
    const std::string type = opt.getType();
    return ( type == "int" || type == "int_map" ) ? opt.value_as<int>() : fallback;
}

float ncmm_get_option_float_or( const std::string &name, float fallback )
{
    options_manager &opts = get_options();
    if( !opts.has_option( name ) ) {
        return fallback;
    }
    options_manager::cOpt &opt = opts.get_option( name );
    return opt.getType() == "float" ? opt.value_as<float>() : fallback;
}
'@
        $valueAsIntBlock = (Normalize-Lf $valueAsIntAnchor).TrimEnd()
        $safeGetterBlock = (Normalize-Lf $safeGetterDefinitions).TrimEnd()
        $count = ([regex]::Matches($oc,[regex]::Escape($valueAsIntBlock))).Count
        if ($count -ne 1) {
            throw "NCMM fail-safe accessor insertion expected cOpt::value_as<int> specialization once, found $count"
        }
        $oc = $oc.Replace($valueAsIntBlock, $valueAsIntBlock + "`n`n" + $safeGetterBlock)
        Write-Utf8NoBom $optionsCpp $oc
    }

    if (-not $oc.Contains('NCMM v8.7.6.6 stale world-option repair')) {
        $registerStart = 'bool options_manager::ncmm_register_world_bool( const std::string &name,'
        $registerEnd = 'void options_manager::update_global_locale()'
        $registerReplacement = @'
// NCMM v8.7.6.6 stale world-option repair: existing world copies from older builds may
// contain a default/VOID cOpt. Re-registering a module repairs type metadata in place.
bool options_manager::ncmm_register_world_bool( const std::string &name,
        const translation &menu_text, const translation &tooltip, bool default_value )
{
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, default_value, COPT_WORLDGEN_ONLY );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" ) {
        return false;
    }
    if( opt.eType != cOpt::CVT_BOOL ) {
        opt.sType = "bool";
        opt.eType = cOpt::CVT_BOOL;
        opt.bSet = default_value;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.bDefault = default_value;
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            if( w->second.eType != cOpt::CVT_BOOL ) {
                w->second = opt;
            } else {
                w->second.sMenuText = menu_text;
                w->second.sTooltip = tooltip;
                w->second.hide = COPT_WORLDGEN_ONLY;
                w->second.sPage = "ncmm_experimental";
                w->second.bDefault = default_value;
            }
        }
    }
    return true;
}

bool options_manager::ncmm_register_world_int( const std::string &name,
        const translation &menu_text, const translation &tooltip, int min_value,
        int max_value, int default_value )
{
    if( min_value > max_value || default_value < min_value || default_value > max_value ) {
        return false;
    }
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, min_value, max_value, default_value,
             COPT_WORLDGEN_ONLY );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" ) {
        return false;
    }
    if( opt.eType != cOpt::CVT_INT ) {
        opt.sType = "int";
        opt.eType = cOpt::CVT_INT;
        opt.iSet = default_value;
        opt.format = "%i";
        opt.verbose = false;
        opt.mIntValues.clear();
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.iMin = min_value;
    opt.iMax = max_value;
    opt.iDefault = default_value;
    opt.iSet = std::clamp( opt.iSet, min_value, max_value );
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            if( w->second.eType != cOpt::CVT_INT ) {
                w->second = opt;
            } else {
                w->second.sMenuText = menu_text;
                w->second.sTooltip = tooltip;
                w->second.hide = COPT_WORLDGEN_ONLY;
                w->second.sPage = "ncmm_experimental";
                w->second.iMin = min_value;
                w->second.iMax = max_value;
                w->second.iDefault = default_value;
                w->second.iSet = std::clamp( w->second.iSet, min_value, max_value );
            }
        }
    }
    return true;
}

bool options_manager::ncmm_register_world_float( const std::string &name,
        const translation &menu_text, const translation &tooltip, float min_value,
        float max_value, float default_value, float step )
{
    if( min_value > max_value || default_value < min_value || default_value > max_value || step <= 0.0f ) {
        return false;
    }
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, min_value, max_value,
             default_value, step, COPT_WORLDGEN_ONLY );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" ) {
        return false;
    }
    if( opt.eType != cOpt::CVT_FLOAT ) {
        opt.sType = "float";
        opt.eType = cOpt::CVT_FLOAT;
        opt.fSet = default_value;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.fMin = min_value;
    opt.fMax = max_value;
    opt.fDefault = default_value;
    opt.fStep = step;
    opt.fSet = std::clamp( opt.fSet, min_value, max_value );
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            if( w->second.eType != cOpt::CVT_FLOAT ) {
                w->second = opt;
            } else {
                w->second.sMenuText = menu_text;
                w->second.sTooltip = tooltip;
                w->second.hide = COPT_WORLDGEN_ONLY;
                w->second.sPage = "ncmm_experimental";
                w->second.fMin = min_value;
                w->second.fMax = max_value;
                w->second.fDefault = default_value;
                w->second.fStep = step;
                w->second.fSet = std::clamp( w->second.fSet, min_value, max_value );
            }
        }
    }
    return true;
}

bool options_manager::ncmm_register_world_enum( const std::string &name,
        const translation &menu_text, const translation &tooltip,
        const std::vector<id_and_option> &items, const std::string &default_value )
{
    if( items.empty() ) {
        return false;
    }
    const auto contains = [&]( const std::string &value ) {
        return std::any_of( items.begin(), items.end(), [&]( const id_and_option &item ) {
            return item.first == value;
        } );
    };
    if( !contains( default_value ) ) {
        return false;
    }
    auto it = options.find( name );
    if( it == options.end() ) {
        ncmm_ensure_experimental_page();
        add( name, "ncmm_experimental", menu_text, tooltip, items, default_value, COPT_WORLDGEN_ONLY );
        return true;
    }
    cOpt &opt = it->second;
    if( opt.sPage == "world_default" ) {
        opt.sPage = "ncmm_experimental";
    }
    if( opt.sPage != "ncmm_experimental" ) {
        return false;
    }
    if( opt.eType != cOpt::CVT_STRING ) {
        opt.eType = cOpt::CVT_STRING;
        opt.sSet = default_value;
    }
    opt.sMenuText = menu_text;
    opt.sTooltip = tooltip;
    opt.hide = COPT_WORLDGEN_ONLY;
    opt.sType = "string_select";
    opt.vItems = items;
    opt.sDefault = default_value;
    opt.iMaxLength = 0;
    if( !contains( opt.sSet ) ) {
        opt.sSet = default_value;
    }
    if( world_options.has_value() ) {
        auto w = ( **world_options ).find( name );
        if( w != ( **world_options ).end() ) {
            if( w->second.eType != cOpt::CVT_STRING ) {
                w->second = opt;
            } else {
                w->second.sMenuText = menu_text;
                w->second.sTooltip = tooltip;
                w->second.hide = COPT_WORLDGEN_ONLY;
                w->second.sPage = "ncmm_experimental";
                w->second.sType = "string_select";
                w->second.vItems = items;
                w->second.sDefault = default_value;
                w->second.iMaxLength = 0;
                if( !contains( w->second.sSet ) ) {
                    w->second.sSet = default_value;
                }
            }
        }
    }
    return true;
}
'@
        $oc = Replace-CppRange $oc $registerStart $registerEnd $registerReplacement 'v8.7.6.6 world-setting stale-copy repair'
        Write-Utf8NoBom $optionsCpp $oc
    }

    # Harden every NCMM geography boolean read. Defaults are deliberately conservative:
    # custom geography defaults OFF; feature switches default ON so missing settings preserve vanilla output.
    $boolReplacements = [ordered]@{
        'get_option<bool>( "NCMM_AWS_CUSTOM_GEOGRAPHY" )' = 'ncmm_get_option_bool_or( "NCMM_AWS_CUSTOM_GEOGRAPHY", false )'
        'get_option<bool>( "NCMM_AWS_MEGACITY" )' = 'ncmm_get_option_bool_or( "NCMM_AWS_MEGACITY", false )'
        'get_option<bool>( "NCMM_AWS_ENABLE_FORESTS" )' = 'ncmm_get_option_bool_or( "NCMM_AWS_ENABLE_FORESTS", true )'
        'get_option<bool>( "NCMM_AWS_ENABLE_LAKES" )' = 'ncmm_get_option_bool_or( "NCMM_AWS_ENABLE_LAKES", true )'
        'get_option<bool>( "NCMM_AWS_ENABLE_OCEANS" )' = 'ncmm_get_option_bool_or( "NCMM_AWS_ENABLE_OCEANS", true )'
    }
    $srcDir = Join-Path $Root 'src'
    foreach ($file in Get-ChildItem $srcDir -Filter '*.cpp' -File) {
        $cpp = Normalize-Lf ([IO.File]::ReadAllText($file.FullName))
        if (-not $cpp.Contains('NCMM_AWS_')) {
            continue
        }
        $before = $cpp
        foreach ($oldText in $boolReplacements.Keys) {
            $cpp = $cpp.Replace([string]$oldText,[string]$boolReplacements[$oldText])
        }
        $geoOld = 'return !ncmm_geo || !get_options().has_option( id ) || get_option<bool>( id );'
        $geoNew = 'return !ncmm_geo || ncmm_get_option_bool_or( id, true );'
        $cpp = $cpp.Replace($geoOld,$geoNew)
        if ($cpp -ne $before) {
            Write-Utf8NoBom $file.FullName $cpp
        }
    }

    # NCMM-modified builds should never tell users only to report to upstream CDDA.
    $d = Normalize-Lf ([IO.File]::ReadAllText($debugCpp))
    if (-not $d.Contains('NCMM/code-mod support: https://github.com/Neversalimus/NCMM/issues')) {
        $repAnchor = 'static repetition_folder rep_folder;'
        $supportHelper = @'
std::string ncmm_debug_report_text( const std::string &text )
{
    return text +
           "\n\nNCMM/code-mod support: https://github.com/Neversalimus/NCMM/issues"
           "\nVanilla CDDA issues: https://github.com/CleverRaven/Cataclysm-DDA/issues";
}

static repetition_folder rep_folder;
'@
        $d = Replace-TextBlock $d $repAnchor $supportHelper 'v8.7.6.6 debug support helper'

        $bufferedOld = 'buffered_prompts().push_back( {filename, line, funcname, text, false } );'
        $bufferedNew = 'buffered_prompts().push_back( {filename, line, funcname, ncmm_debug_report_text( text ), false } );'
        $d = Replace-TextBlock $d $bufferedOld $bufferedNew 'v8.7.6.6 buffered debug support URL'

        $promptOld = '    debug_error_prompt( filename, line, funcname, text.c_str(), false );'
        $promptNew = @'
    const std::string ncmm_prompt_text = ncmm_debug_report_text( text );
    debug_error_prompt( filename, line, funcname, ncmm_prompt_text.c_str(), false );
'@
        $d = Replace-TextBlock $d $promptOld $promptNew 'v8.7.6.6 live debug support URL'

        $repNeedle = '"Excessive error repetition detected.  Please file a bug report at https://github.com/CleverRaven/Cataclysm-DDA/issues\n            "'
        $repValue = '"Excessive error repetition detected.\nNCMM/code-mod support: https://github.com/Neversalimus/NCMM/issues\nVanilla CDDA issues: https://github.com/CleverRaven/Cataclysm-DDA/issues\n            "'
        $repCount = ([regex]::Matches($d,[regex]::Escape($repNeedle))).Count
        if ($repCount -ne 2) {
            throw "v8.7.6.6 expected two repeated-error URL literals, found $repCount"
        }
        $d = $d.Replace($repNeedle,$repValue)
        Write-Utf8NoBom $debugCpp $d
    }

    # Human-readable manager diagnostics and a durable support snapshot.
    $l = Normalize-Lf ([IO.File]::ReadAllText($loaderCpp))
    if (-not $l.Contains('std::string manager_reason_text')) {
        $managerAnchor = 'std::vector<manager_entry> manager_entries()'
        $managerInfra = @'
std::string manager_reason_text( const std::string &reason )
{
    if( reason.empty() || reason == "ok" ) {
        return reason;
    }
    if( reason == "api_versioning_capability_required" ) {
        return tr_ui( "manifest declares API version but does not require api.versioning.v1",
                      "manifest объявляет версию API, но не требует api.versioning.v1" );
    }
    if( reason == "api_version_mismatch" ) {
        return tr_ui( "module requires a different NCMM API version",
                      "модулю требуется другая версия NCMM API" );
    }
    if( reason == "loader_api_mismatch" ) {
        return tr_ui( "module requires a different loader API",
                      "модулю требуется другая версия loader API" );
    }
    if( reason == "capability_contract_mismatch" ) {
        return tr_ui( "mod.json and DLL capability lists do not match",
                      "списки capabilities в mod.json и DLL не совпадают" );
    }
    if( reason == "manifest_descriptor_mismatch" ) {
        return tr_ui( "mod.json and DLL id/version do not match",
                      "id/версия в mod.json и DLL не совпадают" );
    }
    if( reason.rfind( "missing_capability:", 0 ) == 0 ) {
        return tr_ui( "missing host capability: ", "нет возможности host: " ) +
               reason.substr( std::string( "missing_capability:" ).size() );
    }
    return reason;
}

void write_diagnostics_summary()
{
    const std::filesystem::path path = game_root() / "ncmm" / "diagnostics.txt";
    std::ofstream out( path, std::ios::trunc | std::ios::binary );
    if( !out ) {
        return;
    }
    out << "NCMM diagnostics\n";
    out << "support=https://github.com/Neversalimus/NCMM/issues\n";
    out << "host_version=" << get_host_version() << '\n';
    out << "loader_api=" << get_loader_api() << '\n';
    out << "api_version=" << get_api_version_major() << '.' << get_api_version_minor() << '\n';
    out << "locale=" << current_locale() << '\n';
    out << "capabilities=";
    for( size_t i = 0; i < get_capability_count(); ++i ) {
        if( i != 0 ) {
            out << ',';
        }
        out << get_capability( i );
    }
    out << "\nmodules=" << module_states.size() << '\n';
    for( const module_state &state : module_states ) {
        out << state.id << " | " << state.version << " | " << state.state
            << " | " << state.reason << " | " << state.directory.filename().string() << '\n';
    }
    out.flush();
}

std::vector<manager_entry> manager_entries()
'@
        $l = Replace-TextBlock $l $managerAnchor $managerInfra 'v8.7.6.6 manager diagnostics infrastructure'

        $reasonOld = '                label += " - " + entry.reason;'
        $reasonNew = '                label += " - " + manager_reason_text( entry.reason );'
        if ($l.Contains($reasonOld)) {
            # Legacy manager already exposed raw reason text: preserve its layout and translate the reason.
            $l = Replace-TextBlock $l $reasonOld $reasonNew 'v8.7.6.6 manager readable failure reason'
        } else {
            # Host 0.8 manager no longer appends raw reasons at all. Restore the useful detail
            # explicitly, but only for non-OK states so normal module rows stay compact.
            $reasonBlockOld = @'
            const loaded_mod *runtime = find_loaded( entry.directory );
            if( runtime != nullptr && runtime->open_ui != nullptr ) {
                label += tr_ui( " [SETTINGS]", " [НАСТРОЙКИ]" );
            }
            menu.addentry( i, true, MENU_AUTOASSIGN, label );
'@
            $reasonBlockNew = @'
            const loaded_mod *runtime = find_loaded( entry.directory );
            if( runtime != nullptr && runtime->open_ui != nullptr ) {
                label += tr_ui( " [SETTINGS]", " [НАСТРОЙКИ]" );
            }
            if( !entry.reason.empty() && entry.reason != "ok" ) {
                label += " - " + manager_reason_text( entry.reason );
            }
            menu.addentry( i, true, MENU_AUTOASSIGN, label );
'@
            $l = Replace-TextBlock $l $reasonBlockOld $reasonBlockNew 'v8.7.6.6 manager readable failure reason current host'
        }

        $menuEnOld = 'NCMM — Mod Configuration\nPress F2 to open this menu (the key can be changed in Controls). Press Enter to open settings for supported mods.'
        $menuEnNew = 'NCMM — Mod Configuration\nPress F2 to open this menu (the key can be changed in Controls). Press Enter to open settings for supported mods.\nSupport: https://github.com/Neversalimus/NCMM/issues'
        $menuRuOld = 'NCMM — Настройка модов\nF2 открывает это меню; клавишу можно изменить в управлении. Enter открывает настройки поддерживаемого мода.'
        $menuRuNew = 'NCMM — Настройка модов\nF2 открывает это меню; клавишу можно изменить в управлении. Enter открывает настройки поддерживаемого мода.\nПоддержка: https://github.com/Neversalimus/NCMM/issues'
        if (-not $l.Contains($menuEnOld) -or -not $l.Contains($menuRuOld)) {
            throw 'v8.7.6.6 manager support-text anchor missing.'
        }
        $l = $l.Replace($menuEnOld,$menuEnNew).Replace($menuRuOld,$menuRuNew)

        $initOld = @'
    write_modules_state();
    mark_ready();
'@
        $initNew = @'
    write_modules_state();
    write_diagnostics_summary();
    mark_ready();
'@
        $l = Replace-TextBlock $l $initOld $initNew 'v8.7.6.6 initialize diagnostics snapshot'

        $showOld = @'
void show_manager()
{
    while( true ) {
'@
        $showNew = @'
void show_manager()
{
    write_diagnostics_summary();
    while( true ) {
'@
        $l = Replace-TextBlock $l $showOld $showNew 'v8.7.6.6 manager refresh diagnostics snapshot'
        Write-Utf8NoBom $loaderCpp $l
    }

    & $verify
    Write-Utf8NoBom $marker "NCMM v8.7.6.6 runtime infrastructure\n"
    Write-Host 'NCMM v8.7.6.6 runtime infrastructure: READY' -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# NCMM Host API 2.0 Core -- dual-stack bridge, generic runtime registries
# ---------------------------------------------------------------------------
function Apply-NcmmHostApi20Core {
    Write-Host "Applying NCMM Host 0.8.0 / Host API 2.0 Core..." -ForegroundColor Cyan
    $sdk20Path = Join-Path $NcmmRoot 'sdk\ncmm_api.h'
    $loader20Path = Join-Path $NcmmRoot 'host_patch\ncmm_loader.cpp'
    $loader20HeaderPath = Join-Path $NcmmRoot 'host_patch\ncmm_loader.h'
    $runtime20Path = Join-Path $NcmmRoot 'runtime\NCMMBootstrap.cs'
    $applyHost20Path = Join-Path $NcmmRoot 'host_patch\Apply-NCMMHostPatch.ps1'
    foreach($p20 in @($sdk20Path,$loader20Path,$loader20HeaderPath,$runtime20Path,$applyHost20Path)) {
        if(-not(Test-Path $p20 -PathType Leaf)){throw "Host API 2.0 Core source missing: $p20"}
    }

    $sdk20 = Normalize-Lf ([IO.File]::ReadAllText($sdk20Path))
    if($sdk20.Contains('#define NCMM_HOST_API_V2_CORE_MAJOR 2u')) {
        foreach($needle20 in @('#define NCMM_API_VERSION_MINOR 9u','typedef struct ncmm_host_api_v2_core {','query_interface','NCMM_HOST_API_V2_CORE_ID')) {
            if(-not $sdk20.Contains($needle20)){throw "Host API 2.0 Core partial SDK state: $needle20"}
        }
    } else {
        if(-not $sdk20.Contains('#define NCMM_API_VERSION_MINOR 8u')){throw 'Host API 2.0 Core expected legacy API 1.8 source.'}
        $apiVersionDefines20 = @'
#define NCMM_API_VERSION_MINOR 9u
#define NCMM_HOST_API_V2_CORE_ID "ncmm.host_api.v2.core"
#define NCMM_HOST_API_V2_CORE_ABI 2u
#define NCMM_HOST_API_V2_CORE_MAJOR 2u
#define NCMM_HOST_API_V2_CORE_MINOR 0u
'@
        $sdk20 = $sdk20.Replace('#define NCMM_API_VERSION_MINOR 8u', $apiVersionDefines20.TrimEnd())

        $v1Tail20 = @'
    int ( *ui_tree_choose_rpg )( const char *title, const char *summary,
                                 const ncmm_ui_progress_v1 *progress,
                                 const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                 const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                 const ncmm_ui_theme_ex_v1 *theme );
'@
        $v1Tail20New = @'
    int ( *ui_tree_choose_rpg )( const char *title, const char *summary,
                                 const ncmm_ui_progress_v1 *progress,
                                 const ncmm_ui_tree_node_v1 *nodes, size_t node_count,
                                 const ncmm_ui_tree_edge_v1 *edges, size_t edge_count,
                                 const ncmm_ui_theme_ex_v1 *theme );

    /* Legacy ABI-v1 bridge into stable Host API 2.x interface tables. */
    const void *( *query_interface )( const char *interface_id,
                                      uint32_t min_major, uint32_t min_minor );
'@
        $sdk20 = Replace-TextBlock $sdk20 $v1Tail20 $v1Tail20New 'Host API 2.0 legacy query-interface tail'

        $v2Types20 = @'

typedef enum ncmm_event_id_v2 {
    NCMM_EVENT_HOST_READY_V2 = 1u,
    NCMM_EVENT_WORLD_LOADED_V2 = 2u,
    NCMM_EVENT_WORLD_UNLOADED_V2 = 3u,
    NCMM_EVENT_TURN_V2 = 4u,
    NCMM_EVENT_LOCALE_CHANGED_V2 = 5u,
    NCMM_EVENT_PLAYER_KILL_V2 = 6u
} ncmm_event_id_v2;

typedef enum ncmm_rule_selector_v2 {
    NCMM_SELECTOR_ANY_V2 = 0u,
    NCMM_SELECTOR_SUBJECT_ID_V2 = 1u,
    NCMM_SELECTOR_SOURCE_MOD_V2 = 2u,
    NCMM_SELECTOR_SOURCE_SPECIES_V2 = 3u,
    NCMM_SELECTOR_TARGET_SPECIES_V2 = 4u
} ncmm_rule_selector_v2;

typedef enum ncmm_worldgen_value_type_v2 {
    NCMM_WORLDGEN_BOOL_V2 = 1u,
    NCMM_WORLDGEN_INT_V2 = 2u,
    NCMM_WORLDGEN_FLOAT_V2 = 3u
} ncmm_worldgen_value_type_v2;

typedef void ( *ncmm_event_callback_v2 )( uint32_t event_id, void *user_data );

typedef struct ncmm_host_api_v2_core {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t api_major;
    uint32_t api_minor;
    const ncmm_host_api_v1 *legacy_v1;

    void ( *log )( ncmm_log_level_v1 level, const char *message );
    int ( *has_capability )( const char *capability );
    const char *( *get_host_version )( void );
    const char *( *current_module_id )( void );

    int ( *event_available )( uint32_t event_id );
    int ( *event_subscribe )( const char *module_id, uint32_t event_id,
                              ncmm_event_callback_v2 callback, void *user_data );
    int ( *event_unsubscribe_all )( const char *module_id );

    int ( *world_setting_register_bool )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           int default_value, uint32_t scope );
    int ( *world_setting_register_int )( const char *module_id, const char *setting_id,
                                          const char *display_name, const char *tooltip,
                                          int min_value, int max_value, int default_value,
                                          uint32_t scope );
    int ( *world_setting_register_float )( const char *module_id, const char *setting_id,
                                            const char *display_name, const char *tooltip,
                                            double min_value, double max_value,
                                            double default_value, double step,
                                            uint32_t scope );
    int ( *world_setting_register_enum )( const char *module_id, const char *setting_id,
                                           const char *display_name, const char *tooltip,
                                           const char *const *value_ids,
                                           const char *const *display_names, size_t count,
                                           const char *default_value, uint32_t scope );
    int ( *world_setting_get_bool )( const char *setting_id, int fallback );
    int64_t ( *world_setting_get_i64 )( const char *setting_id, int64_t fallback );
    double ( *world_setting_get_f64 )( const char *setting_id, double fallback );
    const char *( *world_setting_get_string )( const char *setting_id, const char *fallback );

    size_t ( *world_mod_count )( void );
    const char *( *world_mod_id )( size_t index );
    int ( *world_mod_active )( const char *mod_id );

    int ( *character_state_available )( void );
    int64_t ( *character_state_get_i64 )( const char *module_id, const char *key,
                                          int64_t fallback );
    int ( *character_state_set_i64 )( const char *module_id, const char *key,
                                      int64_t value );

    int ( *module_is_loaded )( const char *module_id );
    const char *( *module_version )( const char *module_id );
    const char *( *module_state )( const char *module_id );

    int ( *modifier_define )( const char *module_id, const char *modifier_id,
                              double min_value, double max_value );
    int ( *modifier_set )( const char *module_id, const char *modifier_id, double value );
    int ( *modifier_clear_module )( const char *module_id );
    double ( *modifier_get_total )( const char *modifier_id );

    int ( *runtime_hook_bind_modifier )( const char *module_id, const char *hook_id,
                                          uint32_t selector_kind, const char *selector_value,
                                          const char *modifier_id );
    double ( *runtime_hook_value )( const char *hook_id, const char *subject_id,
                                    const char *source_mod_id,
                                    const char *source_species_id,
                                    const char *target_species_id );

    int ( *worldgen_hook_bind_setting )( const char *module_id, const char *hook_id,
                                          const char *setting_id, uint32_t value_type );
    int ( *worldgen_hook_bool )( const char *hook_id, int fallback );
    int64_t ( *worldgen_hook_i64 )( const char *hook_id, int64_t fallback );
    double ( *worldgen_hook_f64 )( const char *hook_id, double fallback );
} ncmm_host_api_v2_core;
'@
        $sdk20 = $sdk20.Replace('} ncmm_host_api_v1;' + "`n`n" + 'typedef int ( *ncmm_mod_init_v1 )',
                                '} ncmm_host_api_v1;' + $v2Types20 + "`n" + 'typedef int ( *ncmm_mod_init_v1 )')
        Write-Utf8NoBom $sdk20Path $sdk20
    }

    $loader20 = Normalize-Lf ([IO.File]::ReadAllText($loader20Path))
    if(-not $loader20.Contains('#include "creature.h"')) {
        if(-not $loader20.Contains('#include "avatar.h"')) { throw 'Host API 2.0 creature include anchor missing.' }
        $loader20 = $loader20.Replace('#include "avatar.h"','#include "avatar.h"' + "`n" + '#include "creature.h"')
    }

    # HOTFIX13: character state is not safe during new-character construction.
    # active_world exists before chargen has finished, so the old availability predicate could
    # expose get_avatar().get_values() while Magiclysm/MoM/Xedra EVENT EOCs were still building it.
    # game::do_turn clears g->new_game immediately before ncmm::on_turn(), so this becomes true
    # on the first real gameplay turn without delaying normal WORLD_LOADED delivery.
    $characterStateOld013 = @'
int character_state_available()
{
    return g != nullptr && world_generator != nullptr && world_generator->active_world != nullptr ? 1 : 0;
}
'@
    $characterStateNew013 = @'
int character_state_available()
{
    return g != nullptr && !g->new_game && world_generator != nullptr &&
           world_generator->active_world != nullptr ? 1 : 0;
}
'@
    if((Normalize-Lf $loader20).Contains((Normalize-Lf $characterStateOld013))) {
        $loader20 = Replace-TextBlock $loader20 $characterStateOld013 $characterStateNew013 'HOTFIX13 chargen-safe character state availability'
    } elseif(-not (Normalize-Lf $loader20).Contains((Normalize-Lf $characterStateNew013))) {
        throw 'HOTFIX13 character-state lifecycle anchor is neither pristine nor already hardened.'
    }

    if(-not $loader20.Contains('const ncmm_host_api_v2_core api_v2_core = {')) {
        foreach($cap20 in @('host_api.v2.core','events.core.v2','settings.typed.v2','character.modifiers.v2','runtime_hooks.registry.v2','worldgen.bindings.v2','module.lifecycle.query.v2')) {
            if(-not $loader20.Contains('    "'+$cap20+'",')) {
                if(-not $loader20.Contains('    "api.versioning.v1",')){throw 'Host API 2.0 capability insertion anchor missing.'}
                $loader20 = $loader20.Replace('    "api.versioning.v1",','    "api.versioning.v1",' + "`n" + '    "'+$cap20+'",')
            }
        }
        $loader20 = $loader20.Replace('return "0.7.4";','return "0.8.0";')
        $loader20 = $loader20.Replace('"host_version": "0.7.4"','"host_version": "0.8.0"')
        $loader20 = $loader20.Replace('\"host_version\": \"0.7.4\"','\"host_version\": \"0.8.0\"')

        $limitsOld20 = 'const std::map<std::string, std::pair<double, double>, std::less<>> character_modifier_limits = {'
        if(-not $loader20.Contains($limitsOld20)){throw 'Host API 2.0 modifier policy anchor missing.'}
        $loader20 = $loader20.Replace($limitsOld20,'std::map<std::string, std::pair<double, double>, std::less<>> character_modifier_limits = {')

        $globalsOld20 = @'
std::map<std::string, std::map<std::string, double>> character_modifier_values;
std::map<std::string, double, std::less<>> character_modifier_totals;
'@
        $globalsNew20 = @'
std::map<std::string, std::map<std::string, double>> character_modifier_values;
std::map<std::string, double, std::less<>> character_modifier_totals;
std::map<std::string, std::string, std::less<>> modifier_owners_v2;

struct ncmm_event_subscription_v2_internal {
    std::string module_id;
    uint32_t event_id = 0u;
    ncmm_event_callback_v2 callback = nullptr;
    void *user_data = nullptr;
};
struct ncmm_runtime_hook_rule_v2_internal {
    std::string module_id;
    std::string hook_id;
    uint32_t selector_kind = NCMM_SELECTOR_ANY_V2;
    std::string selector_value;
    std::string modifier_id;
};
struct ncmm_worldgen_binding_v2_internal {
    std::string module_id;
    std::string setting_id;
    uint32_t value_type = 0u;
};
std::vector<ncmm_event_subscription_v2_internal> event_subscriptions_v2;
std::vector<ncmm_runtime_hook_rule_v2_internal> runtime_hook_rules_v2;
std::map<std::string, ncmm_worldgen_binding_v2_internal, std::less<>> worldgen_bindings_v2;
thread_local std::string api_v2_string_cache;
thread_local std::string runtime_source_mod_context_v2;
bool api_v2_world_announced = false;
'@
        $loader20 = Replace-TextBlock $loader20 $globalsOld20 $globalsNew20 'Host API 2.0 generic registry globals'

        $coreImpl20 = @'
bool api_v2_token_safe( const char *value )
{
    if( value == nullptr ) return false;
    const std::string text( value );
    if( text.empty() || text.size() > 128 ) return false;
    for( unsigned char c : text ) {
        if( !( std::isalnum( c ) || c == '_' || c == '-' || c == '.' || c == ':' ) ) return false;
    }
    return true;
}

void clear_module_runtime_v2( const std::string &module_id )
{
    erase_module_modifiers( module_id );
    event_subscriptions_v2.erase( std::remove_if( event_subscriptions_v2.begin(), event_subscriptions_v2.end(),
    [&]( const ncmm_event_subscription_v2_internal &s ) { return s.module_id == module_id; } ), event_subscriptions_v2.end() );
    runtime_hook_rules_v2.erase( std::remove_if( runtime_hook_rules_v2.begin(), runtime_hook_rules_v2.end(),
    [&]( const ncmm_runtime_hook_rule_v2_internal &r ) { return r.module_id == module_id; } ), runtime_hook_rules_v2.end() );
    for( auto it = modifier_owners_v2.begin(); it != modifier_owners_v2.end(); ) {
        if( it->second == module_id ) {
            character_modifier_limits.erase( it->first );
            it = modifier_owners_v2.erase( it );
        } else {
            ++it;
        }
    }
    for( auto it = worldgen_bindings_v2.begin(); it != worldgen_bindings_v2.end(); ) {
        if( it->second.module_id == module_id ) it = worldgen_bindings_v2.erase( it ); else ++it;
    }
}

const char *current_module_id_v2()
{
    return active_module_id.empty() ? nullptr : active_module_id.c_str();
}

int event_available_v2( uint32_t event_id )
{
    switch( event_id ) {
        case NCMM_EVENT_HOST_READY_V2:
        case NCMM_EVENT_WORLD_LOADED_V2:
        case NCMM_EVENT_WORLD_UNLOADED_V2:
        case NCMM_EVENT_TURN_V2:
        case NCMM_EVENT_LOCALE_CHANGED_V2:
        case NCMM_EVENT_PLAYER_KILL_V2:
            return 1;
        default:
            return 0;
    }
}

int event_subscribe_v2( const char *module_id, uint32_t event_id,
                        ncmm_event_callback_v2 callback, void *user_data )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !event_available_v2( event_id ) || callback == nullptr ) return 0;
    for( const auto &s : event_subscriptions_v2 ) {
        if( s.module_id == module_id && s.event_id == event_id &&
            s.callback == callback && s.user_data == user_data ) return 1;
    }
    event_subscriptions_v2.push_back( { module_id, event_id, callback, user_data } );
    return 1;
}

int event_unsubscribe_all_v2( const char *module_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ) return 0;
    event_subscriptions_v2.erase( std::remove_if( event_subscriptions_v2.begin(), event_subscriptions_v2.end(),
    [&]( const ncmm_event_subscription_v2_internal &s ) { return s.module_id == module_id; } ), event_subscriptions_v2.end() );
    return 1;
}

void dispatch_event_v2( uint32_t event_id )
{
    if( !event_available_v2( event_id ) || event_subscriptions_v2.empty() ) return;
    const auto snapshot = event_subscriptions_v2;
    std::set<std::string> failed;
    for( const auto &s : snapshot ) {
        if( s.callback == nullptr || module_ids.count( s.module_id ) == 0 ) continue;
        try {
            module_call_scope scope( s.module_id.c_str() );
            s.callback( event_id, s.user_data );
        } catch( ... ) {
            failed.insert( s.module_id );
            log_line( NCMM_LOG_WARN, ( "Host API 2.0 event callback failed: " + s.module_id ).c_str() );
        }
    }
    if( !failed.empty() ) {
        event_subscriptions_v2.erase( std::remove_if( event_subscriptions_v2.begin(), event_subscriptions_v2.end(),
        [&]( const ncmm_event_subscription_v2_internal &s ) { return failed.count( s.module_id ) != 0; } ), event_subscriptions_v2.end() );
    }
}

int module_is_loaded_v2( const char *module_id )
{
    return module_id && module_ids.count( module_id ) != 0 ? 1 : 0;
}

const char *module_version_v2( const char *module_id )
{
    api_v2_string_cache.clear();
    if( module_id == nullptr ) return nullptr;
    for( const loaded_mod &mod : loaded ) {
        if( mod.descriptor && mod.descriptor->id && std::string( mod.descriptor->id ) == module_id ) {
            api_v2_string_cache = mod.descriptor->version ? mod.descriptor->version : "";
            return api_v2_string_cache.c_str();
        }
    }
    for( const module_state &state : module_states ) {
        if( state.id == module_id ) {
            api_v2_string_cache = state.version;
            return api_v2_string_cache.c_str();
        }
    }
    return nullptr;
}

const char *module_state_v2( const char *module_id )
{
    api_v2_string_cache.clear();
    if( module_id == nullptr ) return nullptr;
    for( const module_state &state : module_states ) {
        if( state.id == module_id ) {
            api_v2_string_cache = state.state;
            if( !state.lifecycle.empty() ) api_v2_string_cache += "/" + state.lifecycle;
            return api_v2_string_cache.c_str();
        }
    }
    return nullptr;
}

int modifier_define_v2( const char *module_id, const char *modifier_id,
                        double min_value, double max_value )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( modifier_id ) || !std::isfinite( min_value ) ||
        !std::isfinite( max_value ) || min_value > max_value ||
        min_value < -100000.0 || max_value > 100000.0 ) return 0;
    const auto owner = modifier_owners_v2.find( modifier_id );
    const auto policy = character_modifier_limits.find( modifier_id );
    if( policy != character_modifier_limits.end() ) {
        if( owner == modifier_owners_v2.end() || owner->second != module_id ) return 0;
        return std::abs( policy->second.first - min_value ) < 1.0e-12 &&
               std::abs( policy->second.second - max_value ) < 1.0e-12 ? 1 : 0;
    }
    character_modifier_limits.emplace( modifier_id, std::make_pair( min_value, max_value ) );
    modifier_owners_v2[modifier_id] = module_id;
    return 1;
}

int runtime_hook_bind_modifier_v2( const char *module_id, const char *hook_id,
                                   uint32_t selector_kind, const char *selector_value,
                                   const char *modifier_id )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( hook_id ) || !api_v2_token_safe( modifier_id ) ||
        selector_kind > NCMM_SELECTOR_TARGET_SPECIES_V2 ) return 0;
    if( selector_kind != NCMM_SELECTOR_ANY_V2 && !api_v2_token_safe( selector_value ) ) return 0;
    if( character_modifier_limits.find( modifier_id ) == character_modifier_limits.end() ) return 0;
    const auto owner = modifier_owners_v2.find( modifier_id );
    if( owner != modifier_owners_v2.end() && owner->second != module_id ) return 0;
    const std::string selector = selector_kind == NCMM_SELECTOR_ANY_V2 ? "" : selector_value;
    for( const auto &r : runtime_hook_rules_v2 ) {
        if( r.module_id == module_id && r.hook_id == hook_id &&
            r.selector_kind == selector_kind && r.selector_value == selector &&
            r.modifier_id == modifier_id ) return 1;
    }
    runtime_hook_rules_v2.push_back( { module_id, hook_id, selector_kind, selector, modifier_id } );
    return 1;
}

bool runtime_rule_matches_v2( const ncmm_runtime_hook_rule_v2_internal &r,
                              const char *subject_id, const char *source_mod_id,
                              const char *source_species_id, const char *target_species_id )
{
    switch( r.selector_kind ) {
        case NCMM_SELECTOR_ANY_V2: return true;
        case NCMM_SELECTOR_SUBJECT_ID_V2: return subject_id && r.selector_value == subject_id;
        case NCMM_SELECTOR_SOURCE_MOD_V2: return source_mod_id && r.selector_value == source_mod_id;
        case NCMM_SELECTOR_SOURCE_SPECIES_V2: return source_species_id && r.selector_value == source_species_id;
        case NCMM_SELECTOR_TARGET_SPECIES_V2: return target_species_id && r.selector_value == target_species_id;
        default: return false;
    }
}

double runtime_hook_value_v2( const char *hook_id, const char *subject_id,
                              const char *source_mod_id, const char *source_species_id,
                              const char *target_species_id )
{
    // HOTFIX13: mod character-creation EOCs may query spell/skill formulas before the
    // avatar is fully established. Runtime gameplay modifiers stay neutral until the
    // first real turn announces the world.
    if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;
    double total = 0.0;
    std::set<std::string> counted;
    for( const auto &r : runtime_hook_rules_v2 ) {
        if( r.hook_id != hook_id || !runtime_rule_matches_v2( r, subject_id, source_mod_id,
                source_species_id, target_species_id ) ) continue;
        const auto module_it = character_modifier_values.find( r.module_id );
        if( module_it == character_modifier_values.end() ) continue;
        const auto value_it = module_it->second.find( r.modifier_id );
        if( value_it == module_it->second.end() ) continue;
        const std::string key = r.module_id + "\n" + r.modifier_id;
        if( counted.insert( key ).second ) total += value_it->second;
    }
    return std::max( -100000.0, std::min( 100000.0, total ) );
}

int worldgen_hook_bind_setting_v2( const char *module_id, const char *hook_id,
                                   const char *setting_id, uint32_t value_type )
{
    if( !active_module_matches( module_id ) || module_ids.count( module_id ) == 0 ||
        !api_v2_token_safe( hook_id ) || !api_v2_token_safe( setting_id ) ||
        value_type < NCMM_WORLDGEN_BOOL_V2 || value_type > NCMM_WORLDGEN_FLOAT_V2 ) return 0;
    const auto existing = worldgen_bindings_v2.find( hook_id );
    if( existing != worldgen_bindings_v2.end() ) {
        return existing->second.module_id == module_id && existing->second.setting_id == setting_id &&
               existing->second.value_type == value_type ? 1 : 0;
    }
    worldgen_bindings_v2.emplace( hook_id, ncmm_worldgen_binding_v2_internal{ module_id, setting_id, value_type } );
    return 1;
}

int worldgen_hook_bool_v2( const char *hook_id, int fallback )
{
    const auto it = hook_id ? worldgen_bindings_v2.find( hook_id ) : worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() || it->second.value_type != NCMM_WORLDGEN_BOOL_V2 ) return fallback;
    return world_setting_get_bool( it->second.setting_id.c_str(), fallback );
}
int64_t worldgen_hook_i64_v2( const char *hook_id, int64_t fallback )
{
    const auto it = hook_id ? worldgen_bindings_v2.find( hook_id ) : worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() || it->second.value_type != NCMM_WORLDGEN_INT_V2 ) return fallback;
    return world_setting_get_i64( it->second.setting_id.c_str(), fallback );
}
double worldgen_hook_f64_v2( const char *hook_id, double fallback )
{
    const auto it = hook_id ? worldgen_bindings_v2.find( hook_id ) : worldgen_bindings_v2.end();
    if( it == worldgen_bindings_v2.end() || it->second.value_type != NCMM_WORLDGEN_FLOAT_V2 ) return fallback;
    return world_setting_get_f64( it->second.setting_id.c_str(), fallback );
}

double modifier_get_total_v2( const char *modifier_id )
{
    return gameplay_modifier( modifier_id );
}
'@
        if(-not $loader20.Contains('int character_modifier_set( const char *module_id')){throw 'Host API 2.0 implementation insertion anchor missing.'}
        $loader20 = $loader20.Replace('int character_modifier_set( const char *module_id',$coreImpl20 + "`n" + 'int character_modifier_set( const char *module_id')

        $ownerOld20 = @'
    const auto policy = character_modifier_limits.find( modifier_id );
    if( policy == character_modifier_limits.end() ||
        value < policy->second.first || value > policy->second.second ) {
        return 0;
    }

'@
        $ownerNew20 = @'
    const auto policy = character_modifier_limits.find( modifier_id );
    if( policy == character_modifier_limits.end() ||
        value < policy->second.first || value > policy->second.second ) {
        return 0;
    }
    const auto dynamic_owner = modifier_owners_v2.find( modifier_id );
    if( dynamic_owner != modifier_owners_v2.end() && dynamic_owner->second != module_id ) {
        return 0;
    }

'@
        $loader20 = Replace-TextBlock $loader20 $ownerOld20 $ownerNew20 'Host API 2.0 modifier ownership'

        $publicHooks20 = @'
double runtime_hook_modifier( const char *hook_id, const char *subject_id,
                              const char *source_mod_id, const char *source_species_id,
                              const char *target_species_id )
{
    return runtime_hook_value_v2( hook_id, subject_id, source_mod_id,
                                  source_species_id, target_species_id );
}

void runtime_event_notify( uint32_t event_id )
{
    dispatch_event_v2( event_id );
}

std::string runtime_source_mod_swap( const std::string &source_mod_id )
{
    std::string previous = runtime_source_mod_context_v2;
    runtime_source_mod_context_v2 = source_mod_id;
    return previous;
}

const std::string &runtime_source_mod()
{
    return runtime_source_mod_context_v2;
}

double runtime_hook_modifier_for_creatures( const char *hook_id,
        const Creature *source, const Creature *target )
{
    // HOTFIX13: never expose combat/runtime modifier state during chargen or pre-world load.
    if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;
    double total = 0.0;
    std::set<std::string> counted;
    for( const auto &r : runtime_hook_rules_v2 ) {
        if( r.hook_id != hook_id ) continue;
        bool match = false;
        switch( r.selector_kind ) {
            case NCMM_SELECTOR_ANY_V2: match = true; break;
            case NCMM_SELECTOR_SOURCE_MOD_V2:
                match = !runtime_source_mod_context_v2.empty() &&
                        r.selector_value == runtime_source_mod_context_v2;
                break;
            case NCMM_SELECTOR_SOURCE_SPECIES_V2:
                match = source != nullptr && source->in_species( species_id( r.selector_value ) );
                break;
            case NCMM_SELECTOR_TARGET_SPECIES_V2:
                match = target != nullptr && target->in_species( species_id( r.selector_value ) );
                break;
            default: break;
        }
        if( !match ) continue;
        const auto module_it = character_modifier_values.find( r.module_id );
        if( module_it == character_modifier_values.end() ) continue;
        const auto value_it = module_it->second.find( r.modifier_id );
        if( value_it == module_it->second.end() ) continue;
        const std::string key = r.module_id + "\n" + r.modifier_id;
        if( counted.insert( key ).second ) total += value_it->second;
    }
    return std::max( -100000.0, std::min( 100000.0, total ) );
}

bool worldgen_hook_bound( const char *hook_id )
{
    return hook_id != nullptr && worldgen_bindings_v2.find( hook_id ) != worldgen_bindings_v2.end();
}
int worldgen_hook_bool( const char *hook_id, int fallback )
{
    return worldgen_hook_bool_v2( hook_id, fallback );
}
int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback )
{
    return worldgen_hook_i64_v2( hook_id, fallback );
}
double worldgen_hook_f64( const char *hook_id, double fallback )
{
    return worldgen_hook_f64_v2( hook_id, fallback );
}

'@
        if(-not $loader20.Contains('double gameplay_modifier( const char *modifier_id )')){throw 'Host API 2.0 public hook anchor missing.'}
        # HOTFIX17: public API2 hook definitions must be inserted only after the legacy
        # contextual_metaphysics block has been removed.  Inserting them here would put
        # them inside the later cleanup range [contextual_metaphysics_swap, gameplay_modifier)
        # and silently delete all eight definitions before the host source is written.

        $apiOld20 = @'
    &world_mod_count,
    &world_mod_id,
    &ui_card_choose_rpg,
    &ui_tree_choose_rpg
};
'@
        $apiNew20 = @'
    &world_mod_count,
    &world_mod_id,
    &ui_card_choose_rpg,
    &ui_tree_choose_rpg,
    &query_interface_v2
};
'@

        $v2ApiImpl20 = @'
const ncmm_host_api_v2_core api_v2_core = {
    sizeof( ncmm_host_api_v2_core ),
    NCMM_HOST_API_V2_CORE_ABI,
    NCMM_HOST_API_V2_CORE_MAJOR,
    NCMM_HOST_API_V2_CORE_MINOR,
    &api,
    &log_line,
    &has_capability,
    &get_host_version,
    &current_module_id_v2,
    &event_available_v2,
    &event_subscribe_v2,
    &event_unsubscribe_all_v2,
    &world_setting_register_bool,
    &world_setting_register_int,
    &world_setting_register_float,
    &world_setting_register_enum,
    &world_setting_get_bool,
    &world_setting_get_i64,
    &world_setting_get_f64,
    &world_setting_get_string,
    &world_mod_count,
    &world_mod_id,
    &world_mod_active,
    &character_state_available,
    &character_state_get_i64,
    &character_state_set_i64,
    &module_is_loaded_v2,
    &module_version_v2,
    &module_state_v2,
    &modifier_define_v2,
    &character_modifier_set,
    &character_modifier_clear_module,
    &modifier_get_total_v2,
    &runtime_hook_bind_modifier_v2,
    &runtime_hook_value_v2,
    &worldgen_hook_bind_setting_v2,
    &worldgen_hook_bool_v2,
    &worldgen_hook_i64_v2,
    &worldgen_hook_f64_v2
};

const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor )
{
    if( interface_id == nullptr || std::string( interface_id ) != NCMM_HOST_API_V2_CORE_ID ) return nullptr;
    if( min_major > NCMM_HOST_API_V2_CORE_MAJOR ) return nullptr;
    if( min_major == NCMM_HOST_API_V2_CORE_MAJOR && min_minor > NCMM_HOST_API_V2_CORE_MINOR ) return nullptr;
    return &api_v2_core;
}

'@
        # Keep declaration order simple and standard C++: forward-declare only the query function,
        # define the legacy v1 table, then define the v2 table that points back to v1.
        if(-not $loader20.Contains('const ncmm_host_api_v1 api = {')){throw 'Host API 2.0 legacy API table anchor missing.'}
        $queryDecl20 = 'const void *query_interface_v2( const char *interface_id, uint32_t min_major, uint32_t min_minor );' + "`n`n"
        $loader20 = $loader20.Replace('const ncmm_host_api_v1 api = {',$queryDecl20 + 'const ncmm_host_api_v1 api = {')
        $loader20 = Replace-TextBlock $loader20 $apiOld20 ($apiNew20 + "`n" + $v2ApiImpl20) 'Host API 2.0 legacy bridge initializer'

        # Every failed/retried init must lose v2 contracts as well as values.
        $loader20 = $loader20.Replace('erase_module_modifiers( manifest.id );','clear_module_runtime_v2( manifest.id );')

        # Core event dispatch uses existing safe Host hook points; no module-specific CDDA patch is introduced here.
        $onTurnPatch20 = @'
void on_turn()
{
    if( !api_v2_world_announced && character_state_available() ) {
        api_v2_world_announced = true;
        dispatch_event_v2( NCMM_EVENT_WORLD_LOADED_V2 );
    }
    dispatch_event_v2( NCMM_EVENT_TURN_V2 );
'@
        $loader20 = $loader20.Replace('void on_turn()' + "`n" + '{', $onTurnPatch20.TrimEnd())
        $onLanguagePatch20 = @'
void on_language_changed()
{
    dispatch_event_v2( NCMM_EVENT_LOCALE_CHANGED_V2 );
'@
        $loader20 = $loader20.Replace('void on_language_changed()' + "`n" + '{', $onLanguagePatch20.TrimEnd())
        $loader20 = $loader20.Replace('    write_modules_state();' + "`n" + '    mark_ready();',
                                      '    dispatch_event_v2( NCMM_EVENT_HOST_READY_V2 );' + "`n" + '    write_modules_state();' + "`n" + '    mark_ready();')
        $shutdownPatch20 = @'
void shutdown()
{
    if( api_v2_world_announced ) {
        dispatch_event_v2( NCMM_EVENT_WORLD_UNLOADED_V2 );
        api_v2_world_announced = false;
    }
#ifdef _WIN32
'@
        $loader20 = $loader20.Replace('void shutdown()' + "`n" + '{' + "`n" + '#ifdef _WIN32', $shutdownPatch20.TrimEnd())
        # Clear all v2 registries on host re-entry/shutdown. Existing modifier value clearing remains intact.
        $clearSeq20 = '    character_modifier_totals.clear();' + "`n" + '    contextual_metaphysics_bonus = 0.0;'
        $clearSeq20New = $clearSeq20 + "`n" + '    for( const auto &owned : modifier_owners_v2 ) {' + "`n" + '        character_modifier_limits.erase( owned.first );' + "`n" + '    }' + "`n" + '    modifier_owners_v2.clear();' + "`n" + '    event_subscriptions_v2.clear();' + "`n" + '    runtime_hook_rules_v2.clear();' + "`n" + '    worldgen_bindings_v2.clear();' + "`n" + '    runtime_source_mod_context_v2.clear();' + "`n" + '    api_v2_world_announced = false;'
        $clearCount20 = ([regex]::Matches($loader20,[regex]::Escape($clearSeq20))).Count
        if($clearCount20 -ne 2){throw "Host API 2.0 expected two global clear sequences, found $clearCount20"}
        $loader20 = $loader20.Replace($clearSeq20,$clearSeq20New)

        $apiVersionDiag20 = 'out << "api_version=" << get_api_version_major() << ''.'' << get_api_version_minor() << ''\n'';'
        if($loader20.Contains($apiVersionDiag20) -and
           -not $loader20.Contains('host_api_v2_core=2.0')) {
            $loader20 = $loader20.Replace($apiVersionDiag20,
                $apiVersionDiag20 + "`n" + '    out << "host_api_v2_core=2.0\n";')
        }
        # Survivor 0.9.16 owns integration modifiers through Host API 2.0.
        # Remove legacy module-specific IDs from Host policy before writing the host source.
        $legacyDynamicIds20 = @(
            'mg_spellcraft_flat','mom_metaphysics_flat','xe_deduction_flat','xe_gramarye_flat','af_smartgun_flat','af_metaphysics_flat',
            'mg_mana_max_pct','mg_mana_regen_pct','xe_mana_max_pct','xe_mana_regen_pct',
            'mg_spell_cost_pct','mom_spell_cost_pct','xe_spell_cost_pct','af_spell_cost_pct','mg_cast_time_pct','mom_cast_time_pct','xe_cast_time_pct','af_cast_time_pct',
            'mg_fail_pct','mom_fail_pct','xe_fail_pct','af_fail_pct','mg_spell_xp_pct','mom_spell_xp_pct','xe_spell_xp_pct','af_spell_xp_pct',
            'mg_spell_power_pct','mom_spell_power_pct','xe_spell_power_pct','af_spell_power_pct','mg_range_pct','mom_range_pct','xe_range_pct','af_range_pct',
            'mg_aoe_pct','mom_aoe_pct','xe_aoe_pct','af_aoe_pct','mg_duration_pct','mom_duration_pct','xe_duration_pct','af_duration_pct',
            'afp_smartgun_flat','afp_spell_cost_pct','afp_cast_time_pct','afp_fail_pct','afp_spell_xp_pct','afp_spell_power_pct','afp_range_pct','afp_aoe_pct','afp_duration_pct',
            'sec_damage_pct','sec_resist_pct','sec_elite_damage_pct','sec_elite_resist_pct','sec_crimson_damage_pct','sec_crimson_resist_pct',
            'secx_flesh_craft_flat','secx_flesh_combat_flat','secx_spell_cost_pct','secx_cast_time_pct','secx_fail_pct','secx_spell_xp_pct','secx_spell_power_pct','secx_range_pct','secx_aoe_pct','secx_duration_pct'
        )
        foreach($legacyId20 in $legacyDynamicIds20) {
            $pattern20 = '(?m)^[ \t]*\{ "' + [regex]::Escape($legacyId20) + '", \{[^\r\n]+\} \}[ \t]*,?[ \t]*\r?\n?'
            $hits20 = [regex]::Matches($loader20,$pattern20).Count
            if($hits20 -gt 1){throw "Host API 2.0 duplicate legacy modifier policy: $legacyId20 count=$hits20"}
            if($hits20 -eq 1){$loader20=[regex]::Replace($loader20,$pattern20,'',1)}
        }
        $loader20 = $loader20.Replace('thread_local double contextual_metaphysics_bonus = 0.0;' + "`n",'')
        $contextStart20 = $loader20.IndexOf('double contextual_metaphysics_swap( double value )')
        if($contextStart20 -ge 0) {
            $gameplayStart20 = $loader20.IndexOf('double gameplay_modifier( const char *modifier_id )',$contextStart20)
            if($gameplayStart20 -lt 0){throw 'Host API 2.0 legacy contextual-skill cleanup end anchor missing.'}
            $loader20 = $loader20.Substring(0,$contextStart20) + $loader20.Substring($gameplayStart20)
        }
        $loader20 = $loader20.Replace('    contextual_metaphysics_bonus = 0.0;' + "`n",'')
        foreach($legacyId20 in $legacyDynamicIds20) {
            if($loader20.Contains('"' + $legacyId20 + '"')) { throw "Host API 2.0 output still embeds legacy Survivor modifier id: $legacyId20" }
        }
        if($loader20.Contains('contextual_metaphysics_')) { throw 'Host API 2.0 output still embeds legacy contextual Metaphysics state.' }

        # HOTFIX17: the legacy cleanup above deliberately removes everything between
        # contextual_metaphysics_swap() and gameplay_modifier().  Insert the new public
        # Host API 2.0 runtime/worldgen definitions only after that destructive cleanup.
        $publicAnchor20 = 'double gameplay_modifier( const char *modifier_id )'
        if(-not $loader20.Contains($publicAnchor20)){throw 'Host API 2.0 post-cleanup public hook anchor missing.'}
        $publicHookDefinitionNeedles20 = @(
            'double runtime_hook_modifier( const char *hook_id, const char *subject_id,',
            'void runtime_event_notify( uint32_t event_id )',
            'std::string runtime_source_mod_swap( const std::string &source_mod_id )',
            'const std::string &runtime_source_mod()',
            'double runtime_hook_modifier_for_creatures( const char *hook_id,',
            'bool worldgen_hook_bound( const char *hook_id )',
            'int worldgen_hook_bool( const char *hook_id, int fallback )',
            'int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback )',
            'double worldgen_hook_f64( const char *hook_id, double fallback )'
        )
        foreach($publicNeedle20 in $publicHookDefinitionNeedles20) {
            if($loader20.Contains($publicNeedle20)) { throw "Host API 2.0 public hook definition unexpectedly survived legacy cleanup before reinsertion: $publicNeedle20" }
        }
        $loader20 = $loader20.Replace($publicAnchor20,$publicHooks20 + $publicAnchor20)
        foreach($publicNeedle20 in $publicHookDefinitionNeedles20) {
            $publicCount20 = ([regex]::Matches($loader20,[regex]::Escape($publicNeedle20))).Count
            if($publicCount20 -ne 1) { throw "Host API 2.0 post-cleanup public hook definition expected 1, found ${publicCount20}: $publicNeedle20" }
        }
        $anonClose20 = $loader20.IndexOf('} // namespace')
        $firstPublicHook20 = $loader20.IndexOf($publicHookDefinitionNeedles20[0])
        $gameplayPublic20 = $loader20.IndexOf($publicAnchor20)
        if($anonClose20 -lt 0 -or $firstPublicHook20 -lt 0 -or $gameplayPublic20 -lt 0 -or
           $anonClose20 -gt $firstPublicHook20 -or $firstPublicHook20 -gt $gameplayPublic20) {
            throw 'Host API 2.0 post-cleanup public hooks are not in the external ncmm namespace before gameplay_modifier().'
        }
        Write-Utf8NoBom $loader20Path $loader20
    }

    $header20 = Normalize-Lf ([IO.File]::ReadAllText($loader20HeaderPath))
    if(-not $header20.Contains('class Creature;')) { $header20 = 'class Creature;' + "`n" + $header20 }
    if(-not $header20.Contains('runtime_hook_modifier( const char *hook_id')) {
        if(-not $header20.Contains('#include <cstdint>')){$header20=$header20.Replace('#include <string>','#include <cstdint>' + "`n" + '#include <string>')}
        $tail20 = @'

/** Host-owned generic integration points. Individual modules register rules/bindings through API 2.0. */
double runtime_hook_modifier( const char *hook_id, const char *subject_id = nullptr,
                              const char *source_mod_id = nullptr,
                              const char *source_species_id = nullptr,
                              const char *target_species_id = nullptr );
void runtime_event_notify( uint32_t event_id );
std::string runtime_source_mod_swap( const std::string &source_mod_id );
const std::string &runtime_source_mod();
double runtime_hook_modifier_for_creatures( const char *hook_id,
        const Creature *source, const Creature *target );
bool worldgen_hook_bound( const char *hook_id );
int worldgen_hook_bool( const char *hook_id, int fallback );
int64_t worldgen_hook_i64( const char *hook_id, int64_t fallback );
double worldgen_hook_f64( const char *hook_id, double fallback );
'@
        $last20=$header20.LastIndexOf('}')
        if($last20 -lt 0){throw 'Host API 2.0 loader header namespace tail missing.'}
        $header20=$header20.Substring(0,$last20)+$tail20+"`n"+$header20.Substring($last20)
        $legacyHeaderContext20 = @'
/** Internal thread-local skill context used by source-scoped spell integrations. */
double contextual_metaphysics_swap( double value );
double contextual_metaphysics_modifier();

'@
        $header20 = $header20.Replace((Normalize-Lf $legacyHeaderContext20),'')
        Write-Utf8NoBom $loader20HeaderPath $header20
    }

    $runtime20 = Normalize-Lf ([IO.File]::ReadAllText($runtime20Path))
    if($runtime20.Contains('private const string RuntimeVersion = "0.7.4";')) {
        $runtime20=$runtime20.Replace('private const string RuntimeVersion = "0.7.4";','private const string RuntimeVersion = "0.8.0";')
    } elseif(-not $runtime20.Contains('private const string RuntimeVersion = "0.8.0";')) { throw 'Host API 2.0 bootstrap version anchor missing.' }
    Write-Utf8NoBom $runtime20Path $runtime20

    $apply20 = Normalize-Lf ([IO.File]::ReadAllText($applyHost20Path))
    if($apply20.Contains('0.7.4')){$apply20=$apply20.Replace('0.7.4','0.8.0')}
    if(-not $apply20.Contains('NCMM Host API v1 / NCMM 0.8.0 module contract')){throw 'Host API 2.0 host-patch version promotion failed.'}
    Write-Utf8NoBom $applyHost20Path $apply20

    $smoke20Path=Join-Path $NcmmRoot 'tests\smoke_host.cpp'
    if(Test-Path $smoke20Path -PathType Leaf){
        $smoke20=Normalize-Lf ([IO.File]::ReadAllText($smoke20Path))
        $smoke20=$smoke20.Replace('return "0.7.4-smoke";','return "0.8.0-smoke";')
        Write-Utf8NoBom $smoke20Path $smoke20
    }

    $sdkAudit20=Normalize-Lf ([IO.File]::ReadAllText($sdk20Path))
    $loaderAudit20=Normalize-Lf ([IO.File]::ReadAllText($loader20Path))
    foreach($n20 in @('#define NCMM_API_VERSION_MINOR 9u','#define NCMM_HOST_API_V2_CORE_MAJOR 2u','typedef struct ncmm_host_api_v2_core {','query_interface')){if(-not $sdkAudit20.Contains($n20)){throw "Host API 2.0 SDK audit missing: $n20"}}
    foreach($n20 in @('const ncmm_host_api_v2_core api_v2_core = {','const void *query_interface_v2(','runtime_hook_bind_modifier_v2','worldgen_hook_bind_setting_v2','dispatch_event_v2','return "0.8.0";','"host_api.v2.core"','"events.core.v2"','"settings.typed.v2"','"character.modifiers.v2"','"runtime_hooks.registry.v2"','"worldgen.bindings.v2"','"module.lifecycle.query.v2"')){if(-not $loaderAudit20.Contains($n20)){throw "Host API 2.0 loader audit missing: $n20"}}
    if(-not $loaderAudit20.Contains('return g != nullptr && !g->new_game && world_generator != nullptr &&')){throw 'HOTFIX13 generated Host character-state lifecycle guard missing.'}
    if($loaderAudit20.Contains('return g != nullptr && world_generator != nullptr && world_generator->active_world != nullptr ? 1 : 0;')){throw 'HOTFIX13 generated Host retained chargen-unsafe character-state predicate.'}
    if(([regex]::Matches($loaderAudit20,[regex]::Escape('if( !api_v2_world_announced || !character_state_available() || !api_v2_token_safe( hook_id ) ) return 0.0;'))).Count -ne 2){throw 'HOTFIX13 generated Host runtime-hook lifecycle gates expected 2.'}
    Write-Host 'NCMM Host API 2.0 Core: READY (legacy module ABI v1 preserved)' -ForegroundColor Green
}

Set-InfrastructureTransactionPhase "api2_migrate" "running" "Host API 2.0 + module migrations"
Apply-NcmmHostApi20Core



function Apply-AwsWorldgenHostApi20([string]$Root) {
    Write-Host "Migrating AWS geography source integration to Host API 2.0 generic hooks..." -ForegroundColor Cyan
    $src = Join-Path $Root 'src'
    $paths = @(
        (Join-Path $src 'overmap_city.cpp'),
        (Join-Path $src 'overmap.cpp'),
        (Join-Path $src 'overmap_water.cpp'),
        (Join-Path $src 'overmap_highway.cpp')
    )
    $bindings = @(
        @{ Old='NCMM_AWS_CUSTOM_GEOGRAPHY'; Hook='geography.custom.enabled'; Type='bool'; Default='false' },
        @{ Old='NCMM_AWS_CITY_SIZE'; Hook='geography.city.size'; Type='int'; Default='8' },
        @{ Old='NCMM_AWS_CITY_SPACING'; Hook='geography.city.spacing'; Type='int'; Default='4' },
        @{ Old='NCMM_AWS_MAX_URBANITY'; Hook='geography.city.max_urbanity'; Type='int'; Default='8' },
        @{ Old='NCMM_AWS_MEGACITY'; Hook='geography.city.megacity'; Type='bool'; Default='false' },
        @{ Old='NCMM_AWS_SHOP_RADIUS'; Hook='geography.city.shop_radius'; Type='int'; Default='30' },
        @{ Old='NCMM_AWS_SHOP_SIGMA'; Hook='geography.city.shop_sigma'; Type='int'; Default='50' },
        @{ Old='NCMM_AWS_PARK_RADIUS'; Hook='geography.city.park_radius'; Type='int'; Default='20' },
        @{ Old='NCMM_AWS_PARK_SIGMA'; Hook='geography.city.park_sigma'; Type='int'; Default='80' },
        @{ Old='NCMM_AWS_PLACE_ROADS'; Hook='geography.roads.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_PLACE_RAILROADS'; Hook='geography.railroads.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_PLACE_SPECIALS'; Hook='geography.specials.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_NEIGHBOR_CONNECTIONS'; Hook='geography.neighbor_connections.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_ENABLE_FORESTS'; Hook='geography.forests.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_FOREST_THRESHOLD'; Hook='geography.forests.threshold'; Type='float'; Default='0.20' },
        @{ Old='NCMM_AWS_FOREST_THICK_THRESHOLD'; Hook='geography.forests.thick_threshold'; Type='float'; Default='0.25' },
        @{ Old='NCMM_AWS_ENABLE_SWAMPS'; Hook='geography.swamps.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_SWAMP_ADJ_THRESHOLD'; Hook='geography.swamps.adjacent_threshold'; Type='float'; Default='0.30' },
        @{ Old='NCMM_AWS_SWAMP_ISOLATED_THRESHOLD'; Hook='geography.swamps.isolated_threshold'; Type='float'; Default='0.60' },
        @{ Old='NCMM_AWS_FLOODPLAIN_MIN'; Hook='geography.swamps.floodplain_min'; Type='int'; Default='3' },
        @{ Old='NCMM_AWS_FLOODPLAIN_MAX'; Hook='geography.swamps.floodplain_max'; Type='int'; Default='15' },
        @{ Old='NCMM_AWS_ENABLE_TRAILS'; Hook='geography.trails.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_TRAIL_CHANCE'; Hook='geography.trails.chance'; Type='int'; Default='2' },
        @{ Old='NCMM_AWS_TRAIL_MIN_FOREST'; Hook='geography.trails.min_forest'; Type='int'; Default='100' },
        @{ Old='NCMM_AWS_TRAILHEAD_CHANCE'; Hook='geography.trails.trailhead_chance'; Type='int'; Default='1' },
        @{ Old='NCMM_AWS_TRAILHEAD_ROAD_DISTANCE'; Hook='geography.trails.road_distance'; Type='int'; Default='6' },
        @{ Old='NCMM_AWS_ENABLE_RIVERS'; Hook='geography.rivers.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_RIVER_SCALE'; Hook='geography.rivers.scale'; Type='int'; Default='1' },
        @{ Old='NCMM_AWS_RIVER_FREQUENCY'; Hook='geography.rivers.frequency'; Type='float'; Default='1.5' },
        @{ Old='NCMM_AWS_RIVER_BRANCH_CHANCE'; Hook='geography.rivers.branch_chance'; Type='int'; Default='64' },
        @{ Old='NCMM_AWS_RIVER_REMERGE_CHANCE'; Hook='geography.rivers.remerge_chance'; Type='int'; Default='2' },
        @{ Old='NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE'; Hook='geography.rivers.branch_scale_decrease'; Type='float'; Default='1.0' },
        @{ Old='NCMM_AWS_ENABLE_LAKES'; Hook='geography.lakes.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_LAKE_THRESHOLD'; Hook='geography.lakes.threshold'; Type='float'; Default='0.25' },
        @{ Old='NCMM_AWS_LAKE_MIN_SIZE'; Hook='geography.lakes.min_size'; Type='int'; Default='20' },
        @{ Old='NCMM_AWS_ENABLE_OCEANS'; Hook='geography.oceans.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_OCEAN_THRESHOLD'; Hook='geography.oceans.threshold'; Type='float'; Default='0.25' },
        @{ Old='NCMM_AWS_OCEAN_MIN_SIZE'; Hook='geography.oceans.min_size'; Type='int'; Default='100' },
        @{ Old='NCMM_AWS_ENABLE_HIGHWAYS'; Hook='geography.highways.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_HIGHWAY_GRID_ROW'; Hook='geography.highways.grid_row'; Type='int'; Default='8' },
        @{ Old='NCMM_AWS_HIGHWAY_GRID_COLUMN'; Hook='geography.highways.grid_column'; Type='int'; Default='10' },
        @{ Old='NCMM_AWS_HIGHWAY_GRID_VARIANCE'; Hook='geography.highways.grid_variance'; Type='int'; Default='2' },
        @{ Old='NCMM_AWS_HIGHWAY_STRAIGHTNESS'; Hook='geography.highways.straightness'; Type='float'; Default='0.60' },
        @{ Old='NCMM_AWS_ENABLE_RAVINES'; Hook='geography.ravines.enabled'; Type='bool'; Default='true' },
        @{ Old='NCMM_AWS_RAVINE_COUNT'; Hook='geography.ravines.count'; Type='int'; Default='0' },
        @{ Old='NCMM_AWS_RAVINE_RANGE'; Hook='geography.ravines.range'; Type='int'; Default='45' },
        @{ Old='NCMM_AWS_RAVINE_WIDTH'; Hook='geography.ravines.width'; Type='int'; Default='3' },
        @{ Old='NCMM_AWS_RAVINE_DEPTH'; Hook='geography.ravines.depth'; Type='int'; Default='-3' }
    )
    foreach($path in $paths) {
        if(-not(Test-Path $path -PathType Leaf)) { throw "AWS API2 migration source missing: $path" }
        $body = Normalize-Lf ([IO.File]::ReadAllText($path))
        foreach($b in $bindings) {
            $old = [string]$b.Old; $hook = [string]$b.Hook; $type = [string]$b.Type; $def = [string]$b.Default
            $body = $body.Replace('get_options().has_option( "' + $old + '" )','ncmm::worldgen_hook_bound( "' + $hook + '" )')
            if($type -eq 'bool') {
                $body = $body.Replace('get_option<bool>( "' + $old + '" )','ncmm::worldgen_hook_bool( "' + $hook + '", ' + $def + ' )')
            } elseif($type -eq 'int') {
                $body = $body.Replace('get_option<int>( "' + $old + '" )','static_cast<int>( ncmm::worldgen_hook_i64( "' + $hook + '", ' + $def + ' ) )')
            } else {
                $body = $body.Replace('get_option<float>( "' + $old + '" )','static_cast<float>( ncmm::worldgen_hook_f64( "' + $hook + '", ' + $def + ' ) )')
            }
            $body = $body.Replace('"' + $old + '"','"' + $hook + '"')
        }
        $body = $body.Replace('return !ncmm_geo || !get_options().has_option( id ) || get_option<bool>( id );',
                              'return !ncmm_geo || !ncmm::worldgen_hook_bound( id ) || ncmm::worldgen_hook_bool( id, true );')
        if($body.Contains('ncmm::worldgen_hook_') -and -not $body.Contains('#include "ncmm_loader.h"')) {
            if($body.Contains('#include "options.h"')) {
                $body = $body.Replace('#include "options.h"','#include "options.h"' + "`n" + '#include "ncmm_loader.h"')
            } else {
                throw "AWS API2 migration could not add ncmm_loader.h include: $path"
            }
        }
        if($body.Contains('NCMM_AWS_')) { throw "AWS-specific setting id leaked into final CDDA source: $path" }
        Write-Utf8NoBom $path $body
    }
    foreach($path in $paths) {
        $check = [IO.File]::ReadAllText($path)
        if($check.Contains('NCMM_AWS_')) { throw "AWS API2 post-check failed: $path" }
    }
    Write-Host "AWS geography CDDA integration: generic Host API 2.0 hooks READY" -ForegroundColor Green
}

function Apply-SurvivorHostApi20Migration {
    Write-Host "Migrating Survivor Progression 0.9.15 -> 0.9.16 Host API 2.0..." -ForegroundColor Cyan
    $sp = [IO.File]::ReadAllText($spPath)
    $manifest = [IO.File]::ReadAllText($manifestPath)
    if($sp.Contains('Survivor Progression v0.9.16') -and $manifest.Contains('"version": "0.9.16"')) {
        Write-Host "Survivor 0.9.16 Host API 2.0 migration already present." -ForegroundColor Green
        return
    }
    if(-not $sp.Contains('const ncmm_host_api_v1 *host = nullptr;')) { throw 'Survivor 0.9.16 host pointer anchor missing.' }
    $sp = $sp.Replace('const ncmm_host_api_v1 *host = nullptr;', 'const ncmm_host_api_v1 *host = nullptr;' + "`n" + 'const ncmm_host_api_v2_core *host2 = nullptr;')
    if(-not $sp.Contains('    "active_mods.registry.v2",')) { throw 'Survivor 0.9.16 capability insertion anchor missing.' }
    $sp = $sp.Replace('    "active_mods.registry.v2",','    "active_mods.registry.v2",' + "`n" + '    "host_api.v2.core",' + "`n" + '    "character.modifiers.v2",' + "`n" + '    "runtime_hooks.registry.v2",')

    $api2Setup = @'
bool configure_host_api2_runtime_hooks()
{
    if( host2 == nullptr || !host2->modifier_define || !host2->runtime_hook_bind_modifier ) return false;
    const char *dynamic_modifiers[] = {
        "mg_spell_cost_pct","mg_cast_time_pct","mg_fail_pct","mg_spell_xp_pct","mg_spell_power_pct","mg_range_pct","mg_aoe_pct","mg_duration_pct","mg_mana_max_pct","mg_mana_regen_pct","mg_spellcraft_flat",
        "mom_spell_cost_pct","mom_cast_time_pct","mom_fail_pct","mom_spell_xp_pct","mom_spell_power_pct","mom_range_pct","mom_aoe_pct","mom_duration_pct","mom_metaphysics_flat",
        "xe_spell_cost_pct","xe_cast_time_pct","xe_fail_pct","xe_spell_xp_pct","xe_spell_power_pct","xe_range_pct","xe_aoe_pct","xe_duration_pct","xe_mana_max_pct","xe_mana_regen_pct","xe_deduction_flat","xe_gramarye_flat",
        "af_spell_cost_pct","af_cast_time_pct","af_fail_pct","af_spell_xp_pct","af_spell_power_pct","af_range_pct","af_aoe_pct","af_duration_pct","af_metaphysics_flat","af_smartgun_flat",
        "afp_spell_cost_pct","afp_cast_time_pct","afp_fail_pct","afp_spell_xp_pct","afp_spell_power_pct","afp_range_pct","afp_aoe_pct","afp_duration_pct","afp_smartgun_flat",
        "secx_spell_cost_pct","secx_cast_time_pct","secx_fail_pct","secx_spell_xp_pct","secx_spell_power_pct","secx_range_pct","secx_duration_pct","secx_flesh_craft_flat","secx_flesh_combat_flat",
        "sec_damage_pct","sec_resist_pct","sec_elite_damage_pct","sec_elite_resist_pct","sec_crimson_damage_pct","sec_crimson_resist_pct"
    };
    for( const char *id : dynamic_modifiers ) if( !host2->modifier_define( module_id, id, -1000.0, 1000.0 ) ) return false;
    auto bind = [&]( const char *hook, uint32_t kind, const char *selector, const char *modifier ) {
        return host2->runtime_hook_bind_modifier( module_id, hook, kind, selector, modifier ) != 0;
    };
    struct spell_source { const char *mod; const char *prefix; bool area; };
    const spell_source spell_sources[] = {
        {"magiclysm","mg",true},{"mindovermatter","mom",true},{"xedra_evolved","xe",true},
        {"aftershock_exoplanet","af",true},{"aftershock_prime","afp",true},{"secronom_lore_expansion","secx",false}
    };
    for( const spell_source &src : spell_sources ) {
        const std::string p = src.prefix;
        if( !bind("spell.cost_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_spell_cost_pct").c_str()) ||
            !bind("spell.cast_time_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_cast_time_pct").c_str()) ||
            !bind("spell.failure_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_fail_pct").c_str()) ||
            !bind("spell.experience_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_spell_xp_pct").c_str()) ||
            !bind("spell.power_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_spell_power_pct").c_str()) ||
            !bind("spell.range_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_range_pct").c_str()) ||
            !bind("spell.duration_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_duration_pct").c_str()) ) return false;
        if( src.area && !bind("spell.area_pct",NCMM_SELECTOR_SOURCE_MOD_V2,src.mod,(p+"_aoe_pct").c_str()) ) return false;
    }
    if( !bind("magic.mana.max_pct",NCMM_SELECTOR_ANY_V2,nullptr,"mg_mana_max_pct") ||
        !bind("magic.mana.max_pct",NCMM_SELECTOR_ANY_V2,nullptr,"xe_mana_max_pct") ||
        !bind("magic.mana.regen_pct",NCMM_SELECTOR_ANY_V2,nullptr,"mg_mana_regen_pct") ||
        !bind("magic.mana.regen_pct",NCMM_SELECTOR_ANY_V2,nullptr,"xe_mana_regen_pct") ||
        !bind("skill.spellcraft.flat",NCMM_SELECTOR_ANY_V2,nullptr,"mg_spellcraft_flat") ||
        !bind("skill.deduction.flat",NCMM_SELECTOR_ANY_V2,nullptr,"xe_deduction_flat") ||
        !bind("skill.gramarye.flat",NCMM_SELECTOR_ANY_V2,nullptr,"xe_gramarye_flat") ||
        !bind("skill.smartgun.flat",NCMM_SELECTOR_ANY_V2,nullptr,"af_smartgun_flat") ||
        !bind("skill.smartgun.flat",NCMM_SELECTOR_ANY_V2,nullptr,"afp_smartgun_flat") ||
        !bind("skill.secro_flesh_craft.flat",NCMM_SELECTOR_ANY_V2,nullptr,"secx_flesh_craft_flat") ||
        !bind("skill.secro_flesh_combat.flat",NCMM_SELECTOR_ANY_V2,nullptr,"secx_flesh_combat_flat") ||
        !bind("skill.metaphysics.flat",NCMM_SELECTOR_SOURCE_MOD_V2,"mindovermatter","mom_metaphysics_flat") ||
        !bind("skill.metaphysics.flat",NCMM_SELECTOR_SOURCE_MOD_V2,"aftershock_exoplanet","af_metaphysics_flat") ) return false;
    const char *base_species[] = {"SECROZED_1","SECROZED_2","SECROZED_3","SECROZED_ULTIMATE","SECROSPEC","SECROWORM","SECRODRAG","SECROSWARMER","SECROSWARMER_ALPHA","SFLESH","SFLESH_FLESHLING","SFLESH_FLESHLING_EX","SSADDLER"};
    const char *elite_species[] = {"SECROZED_2","SECROZED_3","SECROZED_ULTIMATE","SECRODRAG","SECROSWARMER_ALPHA"};
    const char *crimson_species[] = {"SFLESH","SFLESH_FLESHLING","SFLESH_FLESHLING_EX"};
    for( const char *sp : base_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_resist_pct") ) return false;
    for( const char *sp : elite_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_elite_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_elite_resist_pct") ) return false;
    for( const char *sp : crimson_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_crimson_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_crimson_resist_pct") ) return false;
    return true;
}

'@
    $initAnchor = 'int init( const ncmm_host_api_v1 *api )'
    if(-not $sp.Contains($initAnchor)) { throw 'Survivor 0.9.16 init anchor missing.' }
    $sp = $sp.Replace($initAnchor,$api2Setup + $initAnchor)
    $initOpen = @'
int init( const ncmm_host_api_v1 *api )
{
'@
    $initOpenNew = @'
int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || api->query_interface == nullptr ) return 0;
    host2 = static_cast<const ncmm_host_api_v2_core *>(
                api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 0u ) );
    if( host2 == nullptr || host2->api_major != 2u || !configure_host_api2_runtime_hooks() ) return 0;
'@
    $sp = Replace-TextBlock $sp $initOpen $initOpenNew 'Survivor 0.9.16 API2 init bridge'
    $sp = $sp.Replace('"0.9.15"','"0.9.16"').Replace('Survivor Progression v0.9.15','Survivor Progression v0.9.16').Replace('Survivor Progression 0.9.15 initialized:','Survivor Progression 0.9.16 initialized:')
    $shutdownBridge = '    host = nullptr;' + "`n" + '    turn_accumulator = 0;'
    if(-not $sp.Contains($shutdownBridge)) { throw 'Survivor 0.9.16 shutdown bridge anchor missing.' }
    $sp = $sp.Replace($shutdownBridge, '    host = nullptr;' + "`n" + '    host2 = nullptr;' + "`n" + '    turn_accumulator = 0;')
    if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.9.16 must retain state schema 8.' }
    Write-Utf8NoBom $spPath $sp

    if(-not $manifest.Contains('"version": "0.9.15"')) { throw 'Survivor 0.9.16 manifest version anchor missing.' }
    $manifest = $manifest.Replace('"version": "0.9.15"','"version": "0.9.16"').Replace('"api_min_minor": 8','"api_min_minor": 9')
    if(-not $manifest.Contains('    "active_mods.registry.v2",')) { throw 'Survivor 0.9.16 manifest capability anchor missing.' }
    $manifest = $manifest.Replace('    "active_mods.registry.v2",','    "active_mods.registry.v2",' + "`n" + '    "host_api.v2.core",' + "`n" + '    "character.modifiers.v2",' + "`n" + '    "runtime_hooks.registry.v2",')
    Write-Utf8NoBom $manifestPath $manifest
    Write-Host "Survivor Progression 0.9.16 Host API 2.0 migration: READY" -ForegroundColor Green
}

Apply-SurvivorHostApi20Migration

$spMigrationAudit = [IO.File]::ReadAllText($spPath)
$manifestMigrationAudit = [IO.File]::ReadAllText($manifestPath)
foreach($needle in @('const ncmm_host_api_v2_core *host2 = nullptr;','configure_host_api2_runtime_hooks','NCMM_HOST_API_V2_CORE_ID','runtime_hook_bind_modifier','host2 = nullptr;','Survivor Progression v0.9.16')) { if(-not $spMigrationAudit.Contains($needle)){ throw "Survivor 0.9.16 API2 audit missing: $needle" } }
foreach($needle in @('"version": "0.9.16"','"api_min_minor": 9','"host_api.v2.core"','"runtime_hooks.registry.v2"')) { if(-not $manifestMigrationAudit.Contains($needle)){ throw "Survivor 0.9.16 manifest API2 audit missing: $needle" } }
foreach($needle in @(
    'mg_mana_max_pct','mg_spell_power_pct','mom_metaphysics_flat','mom_spell_cost_pct',
    'xe_deduction_flat','xe_mana_regen_pct','af_smartgun_flat','afp_smartgun_flat',
    'secx_flesh_craft_flat','secx_flesh_combat_flat','sec_damage_pct','sec_crimson_damage_pct',
    'spell.cost_pct','spell.power_pct','magic.mana.max_pct','skill.metaphysics.flat',
    'combat.damage_to_species_pct','combat.resist_from_species_pct'
)) { if(-not $spMigrationAudit.Contains($needle)){ throw "Survivor 0.9.16 module-owned binding audit missing: $needle" } }
if(-not $spMigrationAudit.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.9.16 state schema changed unexpectedly.' }
Write-Host "Survivor 0.9.16 Host API 2.0 module audit: PASS" -ForegroundColor Green


function Apply-SurvivorMechanicalPerks0100 {
    Write-Host "Applying Survivor Progression 0.10.0: Mechanical Perks Pass..." -ForegroundColor Cyan
    $sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))
    $manifest = Normalize-Lf ([IO.File]::ReadAllText($manifestPath))
    if($sp.Contains('Survivor Progression v0.10.0') -and $manifest.Contains('"version": "0.10.0"')) {
        Write-Host "Survivor 0.10.0 Mechanical Perks Pass already present." -ForegroundColor Green
        return
    }
    if(-not $sp.Contains('Survivor Progression v0.9.16') -or -not $manifest.Contains('"version": "0.9.16"')) {
        throw 'Survivor 0.10.0 requires the completed 0.9.16 Host API 2.0 migration.'
    }

    $perkArrayStart0100 = $sp.IndexOf('const perk_def perks[] = {')
    $perkArrayEnd0100 = $sp.IndexOf("`n};",$perkArrayStart0100)
    if($perkArrayStart0100 -lt 0 -or $perkArrayEnd0100 -lt 0) { throw 'Survivor 0.10.0 perk array boundary missing.' }
    $perkArrayBefore0100 = $sp.Substring($perkArrayStart0100,$perkArrayEnd0100-$perkArrayStart0100)
    $perkCountBefore0100 = [regex]::Matches($perkArrayBefore0100,'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountBefore0100 -lt 271) { throw "Survivor 0.10.0 baseline perk count unexpectedly low: $perkCountBefore0100" }

    $mechanicalPerks0100 = @'
    { "cm_critical_eye", branch_id::combat, 3, 12, currency_id::perk, "c_precision", "ce_lessons", "Critical Eye", "Критический глаз", "+2 percentage points to melee critical-hit chance.", "+2 процентных пункта к шансу критического удара в ближнем бою.", {{ { "sp_melee_crit_chance_pct", 2 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_vital_strike", branch_id::combat, 4, 17, currency_id::perk, "cm_critical_eye", "c_bruiser", "Vital Strike", "Смертельный удар", "Melee critical damage +10%.", "Урон критических ударов в ближнем бою +10%.", {{ { "sp_melee_crit_damage_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_execution_window", branch_id::combat, 5, 23, currency_id::major, "cm_vital_strike", "c_veteran", "Execution Window", "Окно для добивания", "Melee critical chance +3 points and critical damage +15%.", "+3 пункта к шансу критического удара и +15% к критическому урону в ближнем бою.", {{ { "sp_melee_crit_chance_pct", 3 }, { "sp_melee_crit_damage_pct", 15 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cm_guard_reserve", branch_id::combat, 3, 14, currency_id::perk, "c_conditioning", "ce_reserve", "Guard Reserve", "Резерв защиты", "+1 block attempt whenever defensive attempts refresh.", "+1 попытка блока при каждом обновлении защитных попыток.", {{ { "sp_block_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_second_reaction", branch_id::combat, 4, 18, currency_id::perk, "c_reflexes", "ce_drills", "Second Reaction", "Вторая реакция", "Gain one extra dodge before dodge attempts refresh.", "Одно дополнительное уклонение до следующего восстановления попыток.", {{ { "sp_dodge_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_ballistic_weakpoints", branch_id::combat, 4, 18, currency_id::perk, "c_precision", "ce_tactics", "Ballistic Weakpoints", "Баллистические уязвимости", "Projectile critical multiplier +12%.", "Множитель критического урона снарядов +12%.", {{ { "sp_ranged_crit_damage_pct", 12 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cm_lethal_mastery", branch_id::combat, 6, 32, currency_id::major, "cm_execution_window", "cm_ballistic_weakpoints", "Killing Edge", "Смертельная грань", "Melee crit chance +2 points; melee and ranged critical damage +15%.", "+2 пункта к шансу крита в ближнем бою; критический урон ближнего и дальнего боя +15%.", {{ { "sp_melee_crit_chance_pct", 2 }, { "sp_melee_crit_damage_pct", 15 }, { "sp_ranged_crit_damage_pct", 15 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "sm_damage_control", branch_id::survival, 3, 12, currency_id::perk, "s_hardy", "se_lessons", "Damage Control", "Контроль повреждений", "Incoming damage -3%.", "Входящий урон -3%.", {{ { "sp_damage_taken_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sm_hard_to_kill", branch_id::survival, 4, 18, currency_id::perk, "sm_damage_control", "s_resilient", "Hard to Kill", "Живучий", "Incoming damage -4%.", "Входящий урон -4%.", {{ { "sp_damage_taken_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sm_defy_fate", branch_id::survival, 4, 16, currency_id::perk, "s_instinct", "se_adaptive", "Defy Fate", "Обмануть судьбу", "Each rank adds a 1% chance to completely avoid incoming damage, up to 5% at rank V.", "Каждый ранг даёт 1% шанс полностью избежать входящего урона, до 5% на V ранге.", {{ { "sp_damage_avoid_pct", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sm_brace_for_impact", branch_id::survival, 5, 23, currency_id::perk, "sm_hard_to_kill", "s_survivor", "Brace for Impact", "Принять удар", "+1 block attempt and +5% maximum stamina.", "+1 попытка блока и +5% максимума выносливости.", {{ { "sp_block_attempts_bonus", 1 }, { "stamina_max_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "sm_indomitable_body", branch_id::survival, 6, 34, currency_id::major, "sm_brace_for_impact", "sm_defy_fate", "Indomitable Body", "Несокрушимое тело", "Another 5% incoming damage reduction and +10% healing.", "Ещё -5% входящего урона и +10% лечения.", {{ { "sp_damage_taken_pct", 5 }, { "healing_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "mm_second_dodge", branch_id::mobility, 3, 12, currency_id::perk, "m_parkour", "me_breath", "Second Dodge", "Второе уклонение", "Gain one extra dodge before dodge attempts refresh.", "Одно дополнительное уклонение до следующего восстановления попыток.", {{ { "sp_dodge_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mm_efficient_evasion", branch_id::mobility, 4, 17, currency_id::perk, "mm_second_dodge", "m_quick", "Efficient Evasion", "Экономное уклонение", "One dodge attempt per refresh costs no stamina.", "Одно уклонение до следующего восстановления попыток не тратит выносливость.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mm_afterimage", branch_id::mobility, 4, 19, currency_id::perk, "m_runner", "me_stride", "Afterimage", "Послеобраз", "1% chance to completely avoid incoming damage.", "1% шанс полностью избежать входящего урона.", {{ { "sp_damage_avoid_pct", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mm_perfect_step", branch_id::mobility, 5, 25, currency_id::major, "mm_efficient_evasion", "mm_afterimage", "Perfect Step", "Идеальный шаг", "+2% chance to completely avoid incoming damage and -3% move cost.", "+2% шанс полностью избежать входящего урона и -3% стоимости движения.", {{ { "sp_damage_avoid_pct", 2 }, { "move_cost_pct", -3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mm_combat_flow", branch_id::mobility, 6, 34, currency_id::major, "mm_perfect_step", "m_untouchable", "Combat Flow", "Боевой поток", "One dodge per refresh costs no stamina; +2% speed and +1 point melee critical chance.", "Одно уклонение до восстановления попыток не тратит выносливость; +2% скорости и +1 пункт шанса крита в ближнем бою.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { "speed_pct", 2 }, { "sp_melee_crit_chance_pct", 1 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "fm_precision_assembly", branch_id::crafting, 4, 18, currency_id::perk, "f_engineer", "fe_standard", "Precision Assembly", "Точная сборка", "+8% crafting speed and +0.5 INT.", "+8% скорости крафта и +0,5 ИНТ.", {{ { "craft_speed_pct", 8 }, { "int_flat", 0.5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "fm_field_maintenance", branch_id::crafting, 5, 24, currency_id::perk, "fm_precision_assembly", "f_master", "Field Maintenance", "Полевая эксплуатация", "+8% crafting speed and +10% carrying capacity.", "+8% скорости крафта и +10% грузоподъёмности.", {{ { "craft_speed_pct", 8 }, { "carry_weight_pct", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "fm_masterwork_discipline", branch_id::crafting, 6, 34, currency_id::major, "fm_field_maintenance", "f_genius", "Masterful Worksmanship", "Высшее мастерство", "+12% crafting, +10% reading and +1 INT.", "+12% крафта, +10% чтения и +1 ИНТ.", {{ { "craft_speed_pct", 12 }, { "read_speed_pct", 10 }, { "int_flat", 1 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "gm_weakpoint_eye", branch_id::scavenging, 4, 17, currency_id::perk, "g_awareness", "ge_cache", "Weakpoint Eye", "Глаз на уязвимости", "+1 point melee critical chance and +5% projectile critical multiplier.", "+1 пункт шанса крита в ближнем бою и +5% множителя критического урона снарядов.", {{ { "sp_melee_crit_chance_pct", 1 }, { "sp_ranged_crit_damage_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "gm_scrap_armor_instinct", branch_id::scavenging, 5, 23, currency_id::perk, "g_mule", "ge_instinct", "Scrap Armor Instinct", "Инстинкт бронесборщика", "Incoming damage reduction +2% and carrying capacity +5%.", "-2% входящего урона и +5% грузоподъёмности.", {{ { "sp_damage_taken_pct", 2 }, { "carry_weight_pct", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "gm_escape_route", branch_id::scavenging, 6, 33, currency_id::major, "gm_weakpoint_eye", "gm_scrap_armor_instinct", "Escape Route", "Маршрут отхода", "+1 free dodge, +3% speed and -2% move cost.", "+1 бесплатное уклонение, +3% скорости и -2% стоимости движения.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { "speed_pct", 3 }, { "move_cost_pct", -2 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "am_combat_synthesis", branch_id::mastery, 4, 19, currency_id::perk, "a_insight", "ae_integrate", "Battle Sense", "Боевое чутьё", "+1 point melee critical chance; melee and ranged critical damage +5%.", "+1 пункт шанса крита; критический урон ближнего и дальнего боя +5%.", {{ { "sp_melee_crit_chance_pct", 1 }, { "sp_melee_crit_damage_pct", 5 }, { "sp_ranged_crit_damage_pct", 5 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "am_survival_synthesis", branch_id::mastery, 5, 25, currency_id::major, "a_paragon", "am_combat_synthesis", "Hardened Reflexes", "Закалённые рефлексы", "Incoming damage -2% and +1% chance to completely avoid incoming damage.", "-2% входящего урона и +1% шанс полностью избежать входящего урона.", {{ { "sp_damage_taken_pct", 2 }, { "sp_damage_avoid_pct", 1 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "am_reflex_memory", branch_id::mastery, 5, 27, currency_id::perk, "a_polymath", "ae_longgame", "Reflex Memory", "Память рефлексов", "One dodge per refresh costs no stamina; +2% speed.", "Одно уклонение до восстановления попыток не тратит выносливость; +2% скорости.", {{ { "sp_free_dodge_attempts_bonus", 1 }, { "speed_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "am_apex_adaptation", branch_id::mastery, 6, 40, currency_id::major, "a_transcendent", "am_survival_synthesis", "Total Adaptation", "Полная адаптация", "Incoming damage -2%, +1 point melee crit chance, +5% melee crit damage and +5% ranged crit damage.", "-2% входящего урона, +1 пункт шанса крита, +5% критического урона ближнего и дальнего боя.", {{ { "sp_damage_taken_pct", 2 }, { "sp_melee_crit_chance_pct", 1 }, { "sp_melee_crit_damage_pct", 5 }, { "sp_ranged_crit_damage_pct", 5 } }}, 4, 0, perk_kind::effect }
'@
    # 0.9.15's final mod-Prime perk is intentionally comma-less while it is the array tail.
    # 0.10.0 appends 27 mechanical perks, so turn the old tail into a non-final record first.
    $perkPrefix0100 = "`n"
    $perkArrayPrefix0100 = $sp.Substring(0,$perkArrayEnd0100).TrimEnd()
    if(-not $perkArrayPrefix0100.EndsWith(',')) { $perkPrefix0100 = ",`n" }
    $sp = $sp.Insert($perkArrayEnd0100,$perkPrefix0100 + (Normalize-Lf $mechanicalPerks0100).TrimEnd())

    # Compile-shape preflight: 0.9.15 Prime -> 0.10.0 Mechanical must be a valid initializer-list boundary.
    $firstMechanicalPos0100 = $sp.IndexOf('{ "cm_critical_eye"',$perkArrayStart0100)
    if($firstMechanicalPos0100 -lt 0) { throw 'Survivor 0.10.0 first mechanical perk missing after insertion.' }
    $separatorProbe0100 = $sp.Substring(0,$firstMechanicalPos0100).TrimEnd()
    if(-not $separatorProbe0100.EndsWith(',')) {
        throw 'Survivor 0.10.0 perk-array separator missing before cm_critical_eye.'
    }

    # Defy Fate is one node with five purchasable ranks: 1/2/3/4/5% full avoidance.
    $rankAnchor0100 = '        { "c_conditioning", 5, 0.125 }, { "s_field", 3, 0.50 },'
    if(-not $sp.Contains($rankAnchor0100)) { throw 'Survivor 0.10.0 ranked-perk anchor missing.' }
    $sp = $sp.Replace($rankAnchor0100,
        '        { "c_conditioning", 5, 0.125 }, { "s_field", 3, 0.50 },' + "`n" +
        '        { "sm_defy_fate", 5, 1.0 },')

    # Human-readable labels for mechanical Host API 2.0 modifiers.
    $effectFnStart0100 = $sp.IndexOf('std::string effect_label( const char *raw )')
    if($effectFnStart0100 -lt 0) { $effectFnStart0100 = $sp.IndexOf('std::string effect_label(') }
    if($effectFnStart0100 -lt 0) { throw 'Survivor 0.10.0 effect-label function missing.' }
    $effectReturn0100 = $sp.IndexOf('    return id;',$effectFnStart0100)
    if($effectReturn0100 -lt 0) { throw 'Survivor 0.10.0 effect-label return anchor missing.' }
    $mechanicalLabels0100 = @'
    if( id == "sp_melee_crit_chance_pct" ) return tr( "Melee critical chance points", "Пункты шанса крита в ближнем бою" );
    if( id == "sp_melee_crit_damage_pct" ) return tr( "Melee critical damage %", "Критический урон ближнего боя %" );
    if( id == "sp_ranged_crit_damage_pct" ) return tr( "Projectile critical damage %", "Критический урон снарядов %" );
    if( id == "sp_damage_avoid_pct" ) return tr( "Full damage avoidance %", "Полное избегание урона %" );
    if( id == "sp_damage_taken_pct" ) return tr( "Incoming damage reduction %", "Снижение входящего урона %" );
    if( id == "sp_dodge_attempts_bonus" ) return tr( "Dodge attempts", "Попытки уклонения" );
    if( id == "sp_free_dodge_attempts_bonus" ) return tr( "Free dodge attempts", "Бесплатные уклонения" );
    if( id == "sp_block_attempts_bonus" ) return tr( "Block attempts", "Попытки блока" );
'@
    $sp = $sp.Insert($effectReturn0100,$mechanicalLabels0100)

    # The module owns all concrete modifier IDs; the Host/CDDA side sees only generic hook names.
    $dynamicLoop0100 = '    for( const char *id : dynamic_modifiers ) if( !host2->modifier_define( module_id, id, -1000.0, 1000.0 ) ) return false;'
    if(-not $sp.Contains($dynamicLoop0100)) { throw 'Survivor 0.10.0 dynamic modifier loop anchor missing.' }
    $mechanicalModifierSetup0100 = @'
    for( const char *id : dynamic_modifiers ) if( !host2->modifier_define( module_id, id, -1000.0, 1000.0 ) ) return false;
    const char *mechanical_modifiers[] = {
        "sp_melee_crit_chance_pct", "sp_melee_crit_damage_pct", "sp_ranged_crit_damage_pct",
        "sp_damage_avoid_pct", "sp_damage_taken_pct", "sp_dodge_attempts_bonus",
        "sp_free_dodge_attempts_bonus", "sp_block_attempts_bonus"
    };
    for( const char *id : mechanical_modifiers ) if( !host2->modifier_define( module_id, id, -100.0, 100.0 ) ) return false;
'@
    $sp = $sp.Replace($dynamicLoop0100,$mechanicalModifierSetup0100.TrimEnd())

    $bindAnchor0100 = @'
    struct spell_source { const char *mod; const char *prefix; bool area; };
'@
    if(-not $sp.Contains($bindAnchor0100)) { throw 'Survivor 0.10.0 Host API2 bind anchor missing.' }
    $mechanicalBindings0100 = @'
    if( !bind("combat.melee_crit_chance_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_melee_crit_chance_pct") ||
        !bind("combat.melee_crit_damage_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_melee_crit_damage_pct") ||
        !bind("combat.ranged_crit_damage_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_ranged_crit_damage_pct") ||
        !bind("combat.damage_avoid_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_damage_avoid_pct") ||
        !bind("combat.damage_taken_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_damage_taken_pct") ||
        !bind("combat.dodge_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_dodge_attempts_bonus") ||
        !bind("combat.free_dodge_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_free_dodge_attempts_bonus") ||
        !bind("combat.block_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_block_attempts_bonus") ) return false;

'@
    $sp = $sp.Replace($bindAnchor0100,$mechanicalBindings0100 + $bindAnchor0100)

    $sp = $sp.Replace('Survivor Progression v0.9.16','Survivor Progression v0.10.0')
    $sp = $sp.Replace('Survivor Progression 0.9.16 initialized:','Survivor Progression 0.10.0 initialized:')
    $sp = $sp.Replace('"0.9.16"','"0.10.0"')
    if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.10.0 must retain state schema 8.' }

    $perkArrayEndAfter0100 = $sp.IndexOf("`n};",$perkArrayStart0100)
    $perkArrayAfter0100 = $sp.Substring($perkArrayStart0100,$perkArrayEndAfter0100-$perkArrayStart0100)
    $perkCountAfter0100 = [regex]::Matches($perkArrayAfter0100,'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountAfter0100 -ne $perkCountBefore0100 + 27) {
        throw "Survivor 0.10.0 node preservation failed: before=$perkCountBefore0100 after=$perkCountAfter0100 expected=$($perkCountBefore0100 + 27)"
    }
    Write-Utf8NoBom $spPath $sp

    if(-not $manifest.Contains('"version": "0.9.16"')) { throw 'Survivor 0.10.0 manifest version anchor missing.' }
    $manifest = $manifest.Replace('"version": "0.9.16"','"version": "0.10.0"')
    Write-Utf8NoBom $manifestPath $manifest
    Write-Host "Survivor Progression 0.10.0 Mechanical Perks Pass: READY ($perkCountBefore0100 -> $perkCountAfter0100 nodes; no removals)" -ForegroundColor Green
}

Apply-SurvivorMechanicalPerks0100

$spMechanicalAudit0100 = [IO.File]::ReadAllText($spPath)
$manifestMechanicalAudit0100 = [IO.File]::ReadAllText($manifestPath)
foreach($needle0100 in @(
    'Survivor Progression v0.10.0','sm_defy_fate','cm_second_reaction','mm_second_dodge','am_apex_adaptation',
    'sp_melee_crit_chance_pct','sp_melee_crit_damage_pct','sp_ranged_crit_damage_pct','sp_damage_avoid_pct',
    'sp_damage_taken_pct','sp_dodge_attempts_bonus','sp_free_dodge_attempts_bonus','sp_block_attempts_bonus',
    'combat.melee_crit_chance_pct','combat.melee_crit_damage_pct','combat.ranged_crit_damage_pct','combat.damage_avoid_pct',
    'combat.damage_taken_pct','combat.dodge_attempts_bonus','combat.free_dodge_attempts_bonus','combat.block_attempts_bonus',
    '{ "sm_defy_fate", 5, 1.0 }'
)) { if(-not $spMechanicalAudit0100.Contains($needle0100)){ throw "Survivor 0.10.0 mechanical audit missing: $needle0100" } }
foreach($id0100 in @(
    'cm_critical_eye','cm_vital_strike','cm_execution_window','cm_guard_reserve','cm_second_reaction','cm_ballistic_weakpoints','cm_lethal_mastery',
    'sm_damage_control','sm_hard_to_kill','sm_defy_fate','sm_brace_for_impact','sm_indomitable_body',
    'mm_second_dodge','mm_efficient_evasion','mm_afterimage','mm_perfect_step','mm_combat_flow',
    'fm_precision_assembly','fm_field_maintenance','fm_masterwork_discipline',
    'gm_weakpoint_eye','gm_scrap_armor_instinct','gm_escape_route',
    'am_combat_synthesis','am_survival_synthesis','am_reflex_memory','am_apex_adaptation'
)) {
    $idCount0100 = ([regex]::Matches($spMechanicalAudit0100,[regex]::Escape('"' + $id0100 + '"'))).Count
    if($idCount0100 -lt 1) { throw "Survivor 0.10.0 mechanical perk missing: $id0100" }
}
if(-not $manifestMechanicalAudit0100.Contains('"version": "0.10.0"')) { throw 'Survivor 0.10.0 manifest audit failed.' }
if(-not $spMechanicalAudit0100.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.10.0 state schema changed unexpectedly.' }
Write-Host "Survivor 0.10.0 Mechanical Perks module audit: PASS (27 appended nodes; baseline nodes preserved)" -ForegroundColor Green


function Apply-SurvivorReactiveMechanics0110 {
    Write-Host "Applying Survivor Progression 0.11.0: Reactive Mechanics + Technical Mastery..." -ForegroundColor Cyan
    $sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))
    $manifest = Normalize-Lf ([IO.File]::ReadAllText($manifestPath))
    if($sp.Contains('Survivor Progression v0.11.0') -and $manifest.Contains('"version": "0.11.0"')) {
        Write-Host "Survivor 0.11.0 Reactive Mechanics already present." -ForegroundColor Green
        return
    }
    if(-not $sp.Contains('Survivor Progression v0.10.0') -or -not $manifest.Contains('"version": "0.10.0"')) {
        throw 'Survivor 0.11.0 requires the completed 0.10.0 Mechanical Perks Pass.'
    }

    $perkArrayStart0110 = $sp.IndexOf('const perk_def perks[] = {')
    $perkArrayEnd0110 = $sp.IndexOf("`n};",$perkArrayStart0110)
    if($perkArrayStart0110 -lt 0 -or $perkArrayEnd0110 -lt 0) { throw 'Survivor 0.11.0 perk array boundary missing.' }
    $perkArrayBefore0110 = $sp.Substring($perkArrayStart0110,$perkArrayEnd0110-$perkArrayStart0110)
    $perkCountBefore0110 = [regex]::Matches($perkArrayBefore0110,'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountBefore0110 -lt 298) { throw "Survivor 0.11.0 baseline perk count unexpectedly low: $perkCountBefore0110" }

    # Deliberately unequal branch expansion: mechanics follow branch identity, not artificial node symmetry.
    $reactivePerks0110 = @'
    { "cr_riposte", branch_id::combat, 4, 20, currency_id::perk, "cm_second_reaction", "ce_tactics", "Riposte", "Рипост", "Reactive: every successful dodge has a 20% chance to launch one guarded automatic melee counterattack.", "Реакция: каждое успешное уклонение даёт 20% шанс на одну защищённую автоматическую контратаку в ближнем бою.", {{ { "sp_riposte_chance_pct", 20 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "cr_counterflow", branch_id::combat, 5, 25, currency_id::perk, "cr_riposte", "c_veteran", "Counterflow", "Поток контратаки", "Ripostes refund 50% of the moves they spend; successful dodges also return 5 moves.", "Рипост возвращает 50% потраченных ходов; успешное уклонение также возвращает 5 ходов.", {{ { "sp_riposte_refund_pct", 50 }, { "sp_on_dodge_moves", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cr_critical_surge", branch_id::combat, 4, 21, currency_id::perk, "cm_critical_eye", "cm_vital_strike", "Critical Surge", "Критический импульс", "After every melee critical: +10 moves and restore 2% maximum stamina.", "После каждого критического удара в ближнем бою: +10 ходов и восстановление 2% максимальной выносливости.", {{ { "sp_on_crit_moves", 10 }, { "sp_on_crit_stamina_pct", 2 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cr_execution_protocol", branch_id::combat, 5, 27, currency_id::major, "cm_execution_window", "cm_vital_strike", "Finisher", "Добивание", "Execute window: against targets at 18% HP or lower, outgoing normal damage is increased by 60%.", "Окно добивания: по целям с 18% здоровья или меньше обычный исходящий урон увеличивается на 60%.", {{ { "sp_execute_threshold_pct", 18 }, { "sp_execute_damage_pct", 60 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "cr_predator_momentum", branch_id::combat, 5, 28, currency_id::perk, "c_veteran", "ce_tactics", "Predator Momentum", "Импульс хищника", "Kills build Momentum: up to 3 stacks for 12 turns; each stack grants +3% outgoing damage and +1% speed.", "Убийства накапливают Импульс: до 3 зарядов на 12 ходов; каждый заряд даёт +3% исходящего урона и +1% скорости.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 0, perk_kind::effect },
    { "cr_relentless_momentum", branch_id::combat, 6, 38, currency_id::major, "cr_predator_momentum", "cm_lethal_mastery", "Relentless Momentum", "Неудержимый импульс", "Momentum cap becomes 5 and lasts 20 turns; every kill also returns 15 moves and 3% maximum stamina.", "Лимит Импульса становится 5, длительность — 20 ходов; каждое убийство также возвращает 15 ходов и 3% максимальной выносливости.", {{ { "sp_on_kill_moves", 15 }, { "sp_on_kill_stamina_pct", 3 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "sr_adrenal_recovery", branch_id::survival, 4, 20, currency_id::perk, "sm_hard_to_kill", "s_recovery", "Adrenal Recovery", "Адреналиновое восстановление", "Every player-attributed kill restores 4% maximum stamina.", "Каждое убийство, засчитанное игроку, восстанавливает 4% максимальной выносливости.", {{ { "sp_on_kill_stamina_pct", 4 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "sr_battle_breath", branch_id::survival, 5, 26, currency_id::perk, "sr_adrenal_recovery", "s_survivor", "Battle Breath", "Боевое дыхание", "Melee criticals restore 2% max stamina; kills return 5 moves.", "Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства возвращают 5 ходов.", {{ { "sp_on_crit_stamina_pct", 2 }, { "sp_on_kill_moves", 5 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "mr_slipstream", branch_id::mobility, 4, 19, currency_id::perk, "mm_second_dodge", "me_stride", "Slipstream", "Скольжение", "Every successful dodge immediately returns 12 moves.", "Каждое успешное уклонение немедленно возвращает 12 ходов.", {{ { "sp_on_dodge_moves", 12 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mr_breath_return", branch_id::mobility, 4, 21, currency_id::perk, "mm_efficient_evasion", "m_marathon", "Breath Return", "Возврат дыхания", "Every successful dodge restores 3% maximum stamina.", "Каждое успешное уклонение восстанавливает 3% максимальной выносливости.", {{ { "sp_on_dodge_stamina_pct", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "mr_reactive_step", branch_id::mobility, 5, 27, currency_id::perk, "mr_slipstream", "mm_perfect_step", "Reactive Step", "Ответный шаг", "Successful dodges gain +10 percentage points of riposte chance; ripostes refund 25% of their move cost.", "Успешные уклонения получают +10 процентных пунктов шанса рипоста; рипосты возвращают 25% стоимости хода.", {{ { "sp_riposte_chance_pct", 10 }, { "sp_riposte_refund_pct", 25 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },
    { "mr_kinetic_chain", branch_id::mobility, 6, 36, currency_id::major, "mr_breath_return", "mm_combat_flow", "Kinetic Chain", "Кинетическая цепь", "Criticals return 5 moves; kills return 10 moves. Movement keeps feeding combat tempo.", "Криты возвращают 5 ходов, убийства — 10 ходов. Движение продолжает подпитывать темп боя.", {{ { "sp_on_crit_moves", 5 }, { "sp_on_kill_moves", 10 }, { nullptr, 0 }, { nullptr, 0 } }}, 2, 0, perk_kind::effect },

    { "fr_quality_control", branch_id::crafting, 4, 19, currency_id::perk, "fm_precision_assembly", "fe_theory", "Quality Control", "Контроль качества", "+0.25 to crafting success checks; the displayed success chance uses the same bonus.", "+0,25 к проверкам успеха крафта; отображаемый шанс успеха учитывает тот же бонус.", {{ { "sp_craft_success_roll_flat", 0.25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_second_measure", branch_id::crafting, 5, 24, currency_id::perk, "fr_quality_control", "f_master", "Measure Twice", "Семь раз отмерь", "10% chance to prevent a crafting failure before it causes defects, destroys components or removes progress. The next failure check still advances normally.", "10% шанс предотвратить ошибку крафта до появления дефекта, потери компонентов или прогресса. Следующая проверка ошибки всё равно сдвигается вперёд.", {{ { "sp_craft_failure_save_pct", 10 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_material_discipline", branch_id::crafting, 5, 26, currency_id::perk, "fm_field_maintenance", "fr_quality_control", "Careful Handling", "Бережная работа", "Each component threatened by a crafting failure has a 25% chance to survive.", "Каждый компонент, которому грозит уничтожение при ошибке крафта, имеет 25% шанс сохраниться.", {{ { "sp_craft_component_loss_reduction_pct", 25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_failure_analysis", branch_id::crafting, 5, 28, currency_id::perk, "fr_second_measure", "fr_material_discipline", "Failure Analysis", "Анализ ошибок", "Lose 35% less progress when crafting fails.", "При ошибке крафта теряется на 35% меньше прогресса.", {{ { "sp_craft_progress_loss_reduction_pct", 35 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "fr_zero_defect", branch_id::crafting, 6, 38, currency_id::major, "fr_failure_analysis", "fm_masterwork_discipline", "Flawless Work", "Безупречная работа", "+10% chance to prevent a crafting failure, +15% component protection and 20% less progress loss.", "+10% шанс предотвратить ошибку крафта, +15% защиты компонентов и на 20% меньше потери прогресса.", {{ { "sp_craft_failure_save_pct", 10 }, { "sp_craft_component_loss_reduction_pct", 15 }, { "sp_craft_progress_loss_reduction_pct", 20 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },

    { "gr_trap_reader", branch_id::scavenging, 3, 14, currency_id::perk, "g_awareness", "ge_field", "Trap Reader", "Чтение ловушек", "+2 to trap detection checks.", "+2 к проверкам обнаружения ловушек.", {{ { "sp_trap_detection_flat", 2 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_lock_whisperer", branch_id::scavenging, 4, 19, currency_id::perk, "gr_trap_reader", "g_pathfinder", "Lock Whisperer", "Шёпот замков", "+2 to lockpicking checks.", "+2 к проверкам взлома.", {{ { "sp_lockpick_roll_flat", 2 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_quick_entry", branch_id::scavenging, 4, 21, currency_id::perk, "gr_lock_whisperer", "ge_network", "Quick Entry", "Быстрый вход", "Lockpicking is 20% faster, but cannot go below 30 seconds with normal picks or 5 seconds with perfect picks.", "Взлом на 20% быстрее, но не может занять меньше 30 секунд обычной отмычкой или 5 секунд идеальной.", {{ { "sp_lockpick_time_reduction_pct", 20 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_gentle_tools", branch_id::scavenging, 5, 27, currency_id::perk, "gr_lock_whisperer", "gm_scrap_armor_instinct", "Gentle Tools", "Бережный инструмент", "50% chance to keep your lockpick from being damaged or destroyed after a severe failure.", "50% шанс сохранить отмычку от повреждения или уничтожения после тяжёлой неудачи.", {{ { "sp_lockpick_tool_protection_pct", 50 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },
    { "gr_alarm_bypass", branch_id::scavenging, 6, 35, currency_id::major, "gr_quick_entry", "gr_gentle_tools", "Alarm Bypass", "Обход сигнализации", "25% chance to prevent an alarm from triggering after attempting an alarmed lock.", "25% шанс подавить проверку тревоги после попытки взлома сигнализированного замка.", {{ { "sp_lockpick_alarm_avoid_pct", 25 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 1, 0, perk_kind::effect },

    { "ar_reactive_synthesis", branch_id::mastery, 5, 28, currency_id::perk, "am_combat_synthesis", "ae_integrate", "Reflex Chain", "Цепная реакция", "dodges, melee criticals and kills each return 5 moves.", "уклонения, критические удары в ближнем бою и убийства возвращают по 5 ходов.", {{ { "sp_on_dodge_moves", 5 }, { "sp_on_crit_moves", 5 }, { "sp_on_kill_moves", 5 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect },
    { "ar_momentum_engine", branch_id::mastery, 6, 38, currency_id::major, "ar_reactive_synthesis", "am_apex_adaptation", "Unbroken Momentum", "Непрерывный импульс", "With Predator Momentum, each stack gains another +1% damage and +1% speed, and the maximum increases by 2 stacks.", "С Импульсом хищника каждый заряд даёт ещё +1% урона и +1% скорости, а максимум увеличивается на 2 заряда.", {{ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } }}, 0, 0, perk_kind::effect },
    { "ar_perfect_process", branch_id::mastery, 6, 40, currency_id::major, "ar_reactive_synthesis", "ae_ascendant", "Masterful Work", "Работа мастера", "+5% chance to prevent a crafting failure, +10% component protection and 15% less progress loss.", "+5% шанс предотвратить ошибку крафта, +10% защиты компонентов и на 15% меньше потери прогресса.", {{ { "sp_craft_failure_save_pct", 5 }, { "sp_craft_component_loss_reduction_pct", 10 }, { "sp_craft_progress_loss_reduction_pct", 15 }, { nullptr, 0 } }}, 3, 0, perk_kind::effect }
'@
    # Append safely even when the previous layer's final perk intentionally has no trailing comma.
    # 0.10.0 ends with am_apex_adaptation without a comma because it was the final array element;
    # 0.11.0 turns it into a non-final element, so supply exactly one separator when needed.
    $perkPrefix0110 = "`n"
    $perkArrayPrefix0110 = $sp.Substring(0,$perkArrayEnd0110).TrimEnd()
    if(-not $perkArrayPrefix0110.EndsWith(',')) { $perkPrefix0110 = ",`n" }
    $sp = $sp.Insert($perkArrayEnd0110,$perkPrefix0110 + (Normalize-Lf $reactivePerks0110).TrimEnd())

    # Compile-shape preflight: node counting alone cannot detect a missing comma between two records.
    $firstReactivePos0110 = $sp.IndexOf('{ "cr_riposte"',$perkArrayStart0110)
    if($firstReactivePos0110 -lt 0) { throw 'Survivor 0.11.0 first reactive perk missing after insertion.' }
    $separatorProbe0110 = $sp.Substring(0,$firstReactivePos0110).TrimEnd()
    if(-not $separatorProbe0110.EndsWith(',')) {
        throw 'Survivor 0.11.0 perk-array separator missing before cr_riposte.'
    }

    # Labels are visible in Overview active-effects output.
    $effectFnStart0110 = $sp.IndexOf('std::string effect_label( const std::string &id )')
    if($effectFnStart0110 -lt 0) { $effectFnStart0110 = $sp.IndexOf('std::string effect_label(') }
    if($effectFnStart0110 -lt 0) { throw 'Survivor 0.11.0 effect-label function missing.' }
    $effectReturn0110 = $sp.IndexOf('    return id;',$effectFnStart0110)
    if($effectReturn0110 -lt 0) { throw 'Survivor 0.11.0 effect-label return anchor missing.' }
    $labels0110 = @'
    if( id == "sp_riposte_chance_pct" ) return tr( "Riposte chance %", "Шанс рипоста %" );
    if( id == "sp_riposte_refund_pct" ) return tr( "Riposte move refund %", "Возврат ходов рипоста %" );
    if( id == "sp_on_dodge_moves" ) return tr( "Moves restored after dodge", "Ходы после уклонения" );
    if( id == "sp_on_dodge_stamina_pct" ) return tr( "Stamina restored after dodge %", "Восстановление выносливости после уклонения %" );
    if( id == "sp_on_crit_moves" ) return tr( "Moves restored after melee critical", "Ходы после критического удара" );
    if( id == "sp_on_crit_stamina_pct" ) return tr( "Stamina restored after melee critical %", "Восстановление выносливости после критического удара %" );
    if( id == "sp_execute_threshold_pct" ) return tr( "Finisher health threshold %", "Порог здоровья для добивания %" );
    if( id == "sp_execute_damage_pct" ) return tr( "Finisher damage %", "Урон при добивании %" );
    if( id == "sp_damage_dealt_pct" ) return tr( "Outgoing damage %", "Исходящий урон %" );
    if( id == "sp_on_kill_moves" ) return tr( "Moves restored after kill", "Ходы после убийства" );
    if( id == "sp_on_kill_stamina_pct" ) return tr( "Stamina restored after kill %", "Восстановление выносливости после убийства %" );
    if( id == "sp_craft_success_roll_flat" ) return tr( "Crafting success bonus", "Бонус к успеху крафта" );
    if( id == "sp_craft_failure_save_pct" ) return tr( "Craft failure prevention %", "Предотвращение ошибки крафта %" );
    if( id == "sp_craft_component_loss_reduction_pct" ) return tr( "Craft component protection %", "Защита компонентов крафта %" );
    if( id == "sp_craft_progress_loss_reduction_pct" ) return tr( "Reduced crafting progress loss %", "Снижение потери прогресса при крафте %" );
    if( id == "sp_lockpick_roll_flat" ) return tr( "Lockpicking bonus", "Бонус к взлому" );
    if( id == "sp_lockpick_time_reduction_pct" ) return tr( "Lockpicking time reduction %", "Сокращение времени взлома %" );
    if( id == "sp_lockpick_tool_protection_pct" ) return tr( "Lockpick protection %", "Сохранность отмычки %" );
    if( id == "sp_lockpick_alarm_avoid_pct" ) return tr( "Alarm bypass %", "Обход сигнализации %" );
    if( id == "sp_trap_detection_flat" ) return tr( "Trap detection bonus", "Бонус к обнаружению ловушек" );
'@
    $sp = $sp.Insert($effectReturn0110,(Normalize-Lf $labels0110))

    # Expand module-owned mechanical modifiers while keeping all concrete perk IDs out of Host/CDDA.
    $mechArrayOld0110 = @'
    const char *mechanical_modifiers[] = {
        "sp_melee_crit_chance_pct", "sp_melee_crit_damage_pct", "sp_ranged_crit_damage_pct",
        "sp_damage_avoid_pct", "sp_damage_taken_pct", "sp_dodge_attempts_bonus",
        "sp_free_dodge_attempts_bonus", "sp_block_attempts_bonus"
    };
'@
    $mechArrayNew0110 = @'
    const char *mechanical_modifiers[] = {
        "sp_melee_crit_chance_pct", "sp_melee_crit_damage_pct", "sp_ranged_crit_damage_pct",
        "sp_damage_avoid_pct", "sp_damage_taken_pct", "sp_dodge_attempts_bonus",
        "sp_free_dodge_attempts_bonus", "sp_block_attempts_bonus",
        "sp_riposte_chance_pct", "sp_riposte_refund_pct", "sp_on_dodge_moves", "sp_on_dodge_stamina_pct",
        "sp_on_crit_moves", "sp_on_crit_stamina_pct", "sp_execute_threshold_pct", "sp_execute_damage_pct",
        "sp_damage_dealt_pct", "sp_on_kill_moves", "sp_on_kill_stamina_pct",
        "sp_craft_success_roll_flat", "sp_craft_failure_save_pct", "sp_craft_component_loss_reduction_pct",
        "sp_craft_progress_loss_reduction_pct", "sp_lockpick_roll_flat", "sp_lockpick_time_reduction_pct",
        "sp_lockpick_tool_protection_pct", "sp_lockpick_alarm_avoid_pct", "sp_trap_detection_flat"
    };
'@
    $sp = Replace-TextBlock $sp $mechArrayOld0110 $mechArrayNew0110 'Survivor 0.11.0 mechanical modifier array'

    $bindEnd0110 = @'
        !bind("combat.free_dodge_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_free_dodge_attempts_bonus") ||
        !bind("combat.block_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_block_attempts_bonus") ) return false;
'@
    $bindEndNew0110 = @'
        !bind("combat.free_dodge_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_free_dodge_attempts_bonus") ||
        !bind("combat.block_attempts_bonus",NCMM_SELECTOR_ANY_V2,nullptr,"sp_block_attempts_bonus") ||
        !bind("combat.riposte_chance_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_riposte_chance_pct") ||
        !bind("combat.riposte_refund_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_riposte_refund_pct") ||
        !bind("combat.on_dodge_moves",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_dodge_moves") ||
        !bind("combat.on_dodge_stamina_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_dodge_stamina_pct") ||
        !bind("combat.on_crit_moves",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_crit_moves") ||
        !bind("combat.on_crit_stamina_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_crit_stamina_pct") ||
        !bind("combat.execute_threshold_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_execute_threshold_pct") ||
        !bind("combat.execute_damage_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_execute_damage_pct") ||
        !bind("combat.damage_dealt_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_damage_dealt_pct") ||
        !bind("combat.on_kill_moves",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_kill_moves") ||
        !bind("combat.on_kill_stamina_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_on_kill_stamina_pct") ||
        !bind("crafting.success_roll_flat",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_success_roll_flat") ||
        !bind("crafting.failure_save_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_failure_save_pct") ||
        !bind("crafting.component_loss_reduction_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_component_loss_reduction_pct") ||
        !bind("crafting.progress_loss_reduction_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_craft_progress_loss_reduction_pct") ||
        !bind("scavenging.lockpick_roll_flat",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_roll_flat") ||
        !bind("scavenging.lockpick_time_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_time_reduction_pct") ||
        !bind("scavenging.lockpick_tool_protection_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_tool_protection_pct") ||
        !bind("scavenging.lockpick_alarm_avoid_pct",NCMM_SELECTOR_ANY_V2,nullptr,"sp_lockpick_alarm_avoid_pct") ||
        !bind("scavenging.trap_detection_flat",NCMM_SELECTOR_ANY_V2,nullptr,"sp_trap_detection_flat") ) return false;
'@
    $sp = Replace-TextBlock $sp $bindEnd0110 $bindEndNew0110 'Survivor 0.11.0 mechanical binding tail'

    # Module-owned Momentum state. Host only exposes a generic kill event and modifier registry.
    $configAnchor0110 = 'bool configure_host_api2_runtime_hooks()'
    $configPos0110 = $sp.IndexOf($configAnchor0110)
    if($configPos0110 -lt 0) { throw 'Survivor 0.11.0 API2 configure function missing.' }
    $eventCode0110 = @'
bool survivor_has_perk_id( const char *id )
{
    const perk_def *perk = find_perk( id );
    return perk != nullptr && owned( *perk );
}

void survivor_reactive_event_v2( uint32_t event_id, void * )
{
    if( !character_available() ) return;
    if( event_id == NCMM_EVENT_WORLD_LOADED_V2 ) {
        set_state( "momentum_stacks", 0 );
        set_state( "momentum_turns", 0 );
        effects_dirty = true;
        recalculate_effects();
        return;
    }
    if( event_id == NCMM_EVENT_PLAYER_KILL_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) return;
        int max_stacks = survivor_has_perk_id( "cr_relentless_momentum" ) ? 5 : 3;
        if( survivor_has_perk_id( "ar_momentum_engine" ) ) max_stacks += 2;
        const int64_t stacks = std::min<int64_t>( max_stacks,
                               std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) ) + 1 );
        set_state( "momentum_stacks", stacks );
        set_state( "momentum_turns", survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12 );
        effects_dirty = true;
        recalculate_effects();
        return;
    }
    if( event_id == NCMM_EVENT_TURN_V2 ) {
        int64_t remaining = std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) );
        if( remaining <= 0 ) return;
        --remaining;
        set_state( "momentum_turns", remaining );
        if( remaining == 0 && get_state( "momentum_stacks", 0 ) != 0 ) {
            set_state( "momentum_stacks", 0 );
            effects_dirty = true;
            recalculate_effects();
        }
    }
}

'@
    $sp = $sp.Insert($configPos0110,(Normalize-Lf $eventCode0110))

    # Anchor inside the API2 configure function structurally instead of matching its entire tail.
    # The generated 0.10.0 source can carry mixed newline styles after source bootstrap/transforms,
    # so an exact multi-line here-string is intentionally avoided here.
    $configCrimsonAnchor0110 = '    for( const char *sp : crimson_species ) if( !bind("combat.damage_to_species_pct",NCMM_SELECTOR_TARGET_SPECIES_V2,sp,"sec_crimson_damage_pct") || !bind("combat.resist_from_species_pct",NCMM_SELECTOR_SOURCE_SPECIES_V2,sp,"sec_crimson_resist_pct") ) return false;'
    $configCrimsonPos0110 = $sp.IndexOf($configCrimsonAnchor0110,$configPos0110)
    if($configCrimsonPos0110 -lt 0) { throw 'Survivor 0.11.0 configure crimson binding anchor missing.' }
    $configReturnPos0110 = $sp.IndexOf('    return true;',$configCrimsonPos0110 + $configCrimsonAnchor0110.Length)
    if($configReturnPos0110 -lt 0) { throw 'Survivor 0.11.0 configure return anchor missing.' }
    $configSubscribe0110 = @'
    if( !host2->event_available || !host2->event_subscribe ) return false;
    if( !host2->event_available( NCMM_EVENT_TURN_V2 ) ||
        !host2->event_available( NCMM_EVENT_WORLD_LOADED_V2 ) ||
        !host2->event_available( NCMM_EVENT_PLAYER_KILL_V2 ) ) return false;
    if( !host2->event_subscribe( module_id, NCMM_EVENT_TURN_V2, &survivor_reactive_event_v2, nullptr ) ||
        !host2->event_subscribe( module_id, NCMM_EVENT_WORLD_LOADED_V2, &survivor_reactive_event_v2, nullptr ) ||
        !host2->event_subscribe( module_id, NCMM_EVENT_PLAYER_KILL_V2, &survivor_reactive_event_v2, nullptr ) ) return false;
'@
    $sp = $sp.Insert($configReturnPos0110,(Normalize-Lf $configSubscribe0110))

    # Momentum contributes runtime values only while stacks are alive.
    # Insert structurally inside calculate_owned_effects(); do not depend on CRLF/LF or its whole tail.
    $calcFnAnchor0110 = 'calculated_effects calculate_owned_effects()'
    $calcFnPos0110 = $sp.IndexOf($calcFnAnchor0110)
    if($calcFnPos0110 -lt 0) { throw 'Survivor 0.11.0 calculated-effects function missing.' }
    $calcXpAnchor0110 = '    result.xp_bonus_pct = std::max( 0, std::min( 5000, result.xp_bonus_pct ) );'
    $calcXpPos0110 = $sp.IndexOf($calcXpAnchor0110,$calcFnPos0110)
    if($calcXpPos0110 -lt 0) { throw 'Survivor 0.11.0 calculated-effects XP clamp anchor missing.' }
    $calcMomentum0110 = @'
    auto owns_id = []( const char *id ) {
        const perk_def *perk = find_perk( id );
        return perk != nullptr && owned( *perk );
    };
    const int64_t momentum_stacks = std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) );
    const int64_t momentum_turns = std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) );
    if( momentum_stacks > 0 && momentum_turns > 0 && owns_id( "cr_predator_momentum" ) ) {
        double damage_per_stack = 3.0;
        double speed_per_stack = 1.0;
        if( owns_id( "ar_momentum_engine" ) ) {
            damage_per_stack += 1.0;
            speed_per_stack += 1.0;
        }
        result.modifiers["sp_damage_dealt_pct"] += momentum_stacks * damage_per_stack;
        result.modifiers["speed_pct"] += momentum_stacks * speed_per_stack;
    }
'@
    $sp = $sp.Insert($calcXpPos0110,(Normalize-Lf $calcMomentum0110))

    # Clean event subscription on module shutdown. Scope the insertion to shutdown() so another host reset cannot match.
    $shutdownFnAnchor0110 = 'void shutdown()'
    $shutdownFnPos0110 = $sp.IndexOf($shutdownFnAnchor0110)
    if($shutdownFnPos0110 -lt 0) { throw 'Survivor 0.11.0 shutdown function missing.' }
    $shutdownHostPos0110 = $sp.IndexOf('    host = nullptr;',$shutdownFnPos0110)
    if($shutdownHostPos0110 -lt 0) { throw 'Survivor 0.11.0 shutdown host-reset anchor missing.' }
    $shutdownHost2Pos0110 = $sp.IndexOf('    host2 = nullptr;',$shutdownHostPos0110)
    if($shutdownHost2Pos0110 -lt 0) { throw 'Survivor 0.11.0 shutdown Host API2 reset anchor missing.' }
    $sp = $sp.Insert($shutdownHostPos0110,
        '    if( host2 != nullptr && host2->event_unsubscribe_all ) host2->event_unsubscribe_all( module_id );' + "`n")

    # Events are now an explicit module requirement.
    if(-not $sp.Contains('    "host_api.v2.core",')) { throw 'Survivor 0.11.0 required-capability anchor missing.' }
    if(-not $sp.Contains('    "events.core.v2",')) {
        $sp = $sp.Replace('    "host_api.v2.core",','    "host_api.v2.core",' + "`n" + '    "events.core.v2",')
    }

    $sp = $sp.Replace('Survivor Progression v0.10.0','Survivor Progression v0.11.0')
    $sp = $sp.Replace('Survivor Progression 0.10.0 initialized:','Survivor Progression 0.11.0 initialized:')
    $sp = $sp.Replace('"0.10.0"','"0.11.0"')
    if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.0 must retain state schema 8.' }

    $perkArrayEndAfter0110 = $sp.IndexOf("`n};",$perkArrayStart0110)
    $perkArrayAfter0110 = $sp.Substring($perkArrayStart0110,$perkArrayEndAfter0110-$perkArrayStart0110)
    $perkCountAfter0110 = [regex]::Matches($perkArrayAfter0110,'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountAfter0110 -ne $perkCountBefore0110 + 25) {
        throw "Survivor 0.11.0 node preservation failed: before=$perkCountBefore0110 after=$perkCountAfter0110 expected=$($perkCountBefore0110 + 25)"
    }
    Write-Utf8NoBom $spPath $sp

    if(-not $manifest.Contains('"version": "0.10.0"')) { throw 'Survivor 0.11.0 manifest version anchor missing.' }
    $manifest = $manifest.Replace('"version": "0.10.0"','"version": "0.11.0"')
    if(-not $manifest.Contains('"events.core.v2"')) {
        $manifest = $manifest.Replace('"host_api.v2.core",','"host_api.v2.core",' + "`n" + '    "events.core.v2",')
    }
    Write-Utf8NoBom $manifestPath $manifest
    Write-Host "Survivor Progression 0.11.0 Reactive Mechanics: READY ($perkCountBefore0110 -> $perkCountAfter0110 nodes; 25 appended, zero removed)" -ForegroundColor Green
}

Apply-SurvivorReactiveMechanics0110

$spReactiveAudit0110 = [IO.File]::ReadAllText($spPath)
$manifestReactiveAudit0110 = [IO.File]::ReadAllText($manifestPath)
foreach($needle0110 in @(
    'Survivor Progression v0.11.0','cr_riposte','cr_execution_protocol','cr_predator_momentum','cr_relentless_momentum',
    'fr_second_measure','fr_material_discipline','gr_lock_whisperer','gr_alarm_bypass','ar_momentum_engine',
    'sp_riposte_chance_pct','sp_on_dodge_moves','sp_on_crit_moves','sp_execute_threshold_pct','sp_damage_dealt_pct',
    'sp_craft_failure_save_pct','sp_lockpick_roll_flat','sp_trap_detection_flat','survivor_reactive_event_v2',
    'NCMM_EVENT_PLAYER_KILL_V2','combat.riposte_chance_pct','crafting.failure_save_pct','scavenging.lockpick_roll_flat'
)) { if(-not $spReactiveAudit0110.Contains($needle0110)){ throw "Survivor 0.11.0 reactive audit missing: $needle0110" } }
foreach($id0110 in @(
    'cr_riposte','cr_counterflow','cr_critical_surge','cr_execution_protocol','cr_predator_momentum','cr_relentless_momentum',
    'sr_adrenal_recovery','sr_battle_breath',
    'mr_slipstream','mr_breath_return','mr_reactive_step','mr_kinetic_chain',
    'fr_quality_control','fr_second_measure','fr_material_discipline','fr_failure_analysis','fr_zero_defect',
    'gr_trap_reader','gr_lock_whisperer','gr_quick_entry','gr_gentle_tools','gr_alarm_bypass',
    'ar_reactive_synthesis','ar_momentum_engine','ar_perfect_process'
)) { if(-not $spReactiveAudit0110.Contains('"' + $id0110 + '"')) { throw "Survivor 0.11.0 perk missing: $id0110" } }
if(-not $manifestReactiveAudit0110.Contains('"version": "0.11.0"')) { throw 'Survivor 0.11.0 manifest audit failed.' }
if(-not $manifestReactiveAudit0110.Contains('"events.core.v2"')) { throw 'Survivor 0.11.0 event capability manifest audit failed.' }
if(-not $spReactiveAudit0110.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.0 state schema changed unexpectedly.' }
Write-Host "Survivor 0.11.0 Reactive Mechanics module audit: PASS (25 appended nodes; 0.10.0 nodes preserved)" -ForegroundColor Green

function Apply-SurvivorReactivePolish0111 {
    Write-Host "Applying Survivor Progression 0.11.1: Reactive Mechanics Polish..." -ForegroundColor Cyan
    $sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))
    $manifest = Normalize-Lf ([IO.File]::ReadAllText($manifestPath))
    if($sp.Contains('Survivor Progression v0.11.1') -and $manifest.Contains('"version": "0.11.1"')) {
        Write-Host "Survivor 0.11.1 polish already present." -ForegroundColor Green
        return
    }
    if(-not $sp.Contains('Survivor Progression v0.11.0') -or -not $manifest.Contains('"version": "0.11.0"')) {
        throw 'Survivor 0.11.1 polish requires the completed 0.11.0 Reactive Mechanics layer.'
    }
    $perkStartBeforePolish0111 = $sp.IndexOf('const perk_def perks[] = {')
    $perkEndBeforePolish0111 = $sp.IndexOf("`n};",$perkStartBeforePolish0111)
    if($perkStartBeforePolish0111 -lt 0 -or $perkEndBeforePolish0111 -lt 0) { throw 'Survivor 0.11.1 pre-polish perk-array boundary missing.' }
    $perkCountBeforePolish0111 = [regex]::Matches($sp.Substring($perkStartBeforePolish0111,$perkEndBeforePolish0111-$perkStartBeforePolish0111),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count

    # Text polish mirrors hardened runtime semantics without changing topology or state schema.
    $sp = $sp.Replace(
        'Reactive: every successful dodge has a 20% chance to launch one guarded automatic melee counterattack.',
        'Reactive: after a successful dodge, if no martial-arts counter fires and you have combat stamina, gain a 20% chance for one guarded automatic melee counterattack.')
    $sp = $sp.Replace(
        'Реакция: каждое успешное уклонение даёт 20% шанс на одну защищённую автоматическую контратаку в ближнем бою.',
        'Реакция: после успешного уклонения, если не сработала контратака боевого искусства и хватает выносливости, есть 20% шанс на одну защищённую автоматическую контратаку.')
    $sp = $sp.Replace(
        'Kills build Momentum: up to 3 stacks for 12 turns; each stack grants +3% outgoing damage and +1% speed.',
        'Hostile monster kills that grant XP build Momentum, up to 3 stacks for 12 turns. Each stack grants +3% damage and +1% speed. Revived enemies that grant no XP do not add stacks.')
    $sp = $sp.Replace(
        'Убийства накапливают Импульс: до 3 зарядов на 12 ходов; каждый заряд даёт +3% исходящего урона и +1% скорости.',
        'Убийства монстров, дающие опыт, накапливают Импульс: до 3 зарядов на 12 ходов; каждый заряд даёт +3% исходящего урона и +1% скорости. Убийства возрождённых целей без опыта не срабатывают.')
    $sp = $sp.Replace(
        'Lockpicking is 20% faster, but cannot go below 30 seconds with normal picks or 5 seconds with perfect picks.',
        'Lockpicking is 20% faster, but cannot go below 30 seconds with normal picks or 5 seconds with perfect picks.')
    $sp = $sp.Replace(
        'Взлом на 20% быстрее, но не может занять меньше 30 секунд обычной отмычкой или 5 секунд идеальной.',
        'Взлом на 20% быстрее, но не может занять меньше 30 секунд обычной отмычкой или 5 секунд идеальной.')
    $sp = $sp.Replace(
        '+0.25 to crafting success checks; the displayed success chance uses the same bonus.',
        '+0.25 to crafting success checks; the displayed success chance uses the same bonus.')
    $sp = $sp.Replace(
        '+0,25 к проверкам успеха крафта; отображаемый шанс успеха учитывает тот же бонус.',
        '+0,25 к проверкам успеха крафта; отображаемый шанс успеха учитывает тот же бонус.')
    $sp = $sp.Replace(
        'Momentum cap becomes 5 and lasts 20 turns; every kill also returns 15 moves and 3% maximum stamina.',
        'Momentum cap becomes 5 and lasts 20 turns; every hostile monster kill that grants XP also returns 15 moves and 3% maximum stamina.')
    $sp = $sp.Replace(
        'Лимит Импульса становится 5, длительность — 20 ходов; каждое убийство также возвращает 15 ходов и 3% максимальной выносливости.',
        'Лимит Импульса становится 5, длительность — 20 ходов; каждое убийство монстра, дающее опыт, также возвращает 15 ходов и 3% максимальной выносливости.')
    $sp = $sp.Replace(
        'Every player-attributed kill restores 4% maximum stamina.',
        'A hostile monster kill credited to you that grants XP restores 4% maximum stamina.')
    $sp = $sp.Replace(
        'Каждое убийство, засчитанное игроку, восстанавливает 4% максимальной выносливости.',
        'Каждое убийство монстра, дающее опыт и засчитанное игроку, восстанавливает 4% максимальной выносливости.')
    $sp = $sp.Replace(
        'Melee criticals restore 2% max stamina; kills return 5 moves.',
        'Melee criticals restore 2% max stamina; hostile monster kills that grant XP return 5 moves.')
    $sp = $sp.Replace(
        'Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства возвращают 5 ходов.',
        'Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства монстров, дающие опыт, возвращают 5 ходов.')
    $sp = $sp.Replace(
        'Criticals return 5 moves; kills return 10 moves. Movement keeps feeding combat tempo.',
        'Criticals return 5 moves; hostile monster kills that grant XP return 10 moves. Movement keeps feeding combat tempo.')
    $sp = $sp.Replace(
        'Криты возвращают 5 ходов, убийства — 10 ходов. Движение продолжает подпитывать темп боя.',
        'Криты возвращают 5 ходов, убийства монстров, дающие опыт, — 10 ходов. Движение продолжает подпитывать темп боя.')
    $sp = $sp.Replace(
        'dodges, melee criticals and kills each return 5 moves.',
        'dodges and melee criticals return 5 moves; hostile monster kills that grant XP also return 5 moves.')
    $sp = $sp.Replace(
        'уклонения, критические удары в ближнем бою и убийства возвращают по 5 ходов.',
        'уклонения и критические удары в ближнем бою возвращают 5 ед. хода; убийства монстров, дающие опыт, также возвращают 5 ед. хода.')

    # Russian UX: CDDA "moves" are action points, not whole turns. Avoid implying +5/+10/+15 full turns.
    $sp = $sp.Replace('Рипост возвращает 50% потраченных ходов; успешное уклонение также возвращает 5 ходов.',
                      'Рипост возвращает 50% потраченных единиц хода; успешное уклонение также возвращает 5 ед. хода.')
    $sp = $sp.Replace('После каждого критического удара в ближнем бою: +10 ходов и восстановление 2% максимальной выносливости.',
                      'После каждого критического удара в ближнем бою: +10 ед. хода и восстановление 2% максимальной выносливости.')
    $sp = $sp.Replace('Лимит Импульса становится 5, длительность — 20 ходов; каждое убийство монстра, дающее опыт, также возвращает 15 ходов и 3% максимальной выносливости.',
                      'Лимит Импульса становится 5, длительность — 20 ходов; каждое убийство монстра, дающее опыт, также возвращает 15 ед. хода и 3% максимальной выносливости.')
    $sp = $sp.Replace('Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства монстров, дающие опыт, возвращают 5 ходов.',
                      'Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства монстров, дающие опыт, возвращают 5 ед. хода.')
    $sp = $sp.Replace('Каждое успешное уклонение немедленно возвращает 12 ходов.',
                      'Каждое успешное уклонение немедленно возвращает 12 ед. хода.')
    $sp = $sp.Replace('Криты возвращают 5 ходов, убийства монстров, дающие опыт, — 10 ходов. Движение продолжает подпитывать темп боя.',
                      'Криты возвращают 5 ед. хода, убийства монстров, дающие опыт, — 10 ед. хода. Движение продолжает подпитывать темп боя.')
    $sp = $sp.Replace('Возврат ходов рипоста %','Возврат стоимости рипоста %')
    $sp = $sp.Replace('Ходы после уклонения','Возврат ед. хода после уклонения')
    $sp = $sp.Replace('Ходы после критического удара','Возврат ед. хода после критического удара')
    $sp = $sp.Replace('Ходы после убийства','Возврат ед. хода после убийства')

    $sp = $sp.Replace('Survivor Progression v0.11.0','Survivor Progression v0.11.1')
    $sp = $sp.Replace('Survivor Progression 0.11.0 initialized:','Survivor Progression 0.11.1 initialized:')
    $sp = $sp.Replace('"0.11.0"','"0.11.1"')
    if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.1 must retain state schema 8.' }

    $perkStart0111 = $sp.IndexOf('const perk_def perks[] = {')
    $perkEnd0111 = $sp.IndexOf("`n};",$perkStart0111)
    if($perkStart0111 -lt 0 -or $perkEnd0111 -lt 0) { throw 'Survivor 0.11.1 perk-array boundary missing.' }
    $perkCount0111 = [regex]::Matches($sp.Substring($perkStart0111,$perkEnd0111-$perkStart0111),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountBeforePolish0111 -lt 323) { throw "Survivor 0.11.1 pre-polish node count unexpectedly low: $perkCountBeforePolish0111" }
    if($perkCount0111 -ne $perkCountBeforePolish0111) { throw "Survivor 0.11.1 node preservation failed: before=$perkCountBeforePolish0111 after=$perkCount0111" }
    Write-Utf8NoBom $spPath $sp

    $manifest = $manifest.Replace('"version": "0.11.0"','"version": "0.11.1"')
    Write-Utf8NoBom $manifestPath $manifest
    Write-Host "Survivor Progression 0.11.1 Reactive Mechanics Polish: READY ($perkCount0111 nodes; zero removed)" -ForegroundColor Green
}

Apply-SurvivorReactivePolish0111
$spPolishAudit0111 = [IO.File]::ReadAllText($spPath)
$manifestPolishAudit0111 = [IO.File]::ReadAllText($manifestPath)
foreach($needle0111 in @(
    'Survivor Progression v0.11.1','cr_riposte','cr_predator_momentum','fr_quality_control','gr_quick_entry',
    'Hostile monster kills that grant XP build Momentum','cannot go below 30 seconds with normal picks or 5 seconds with perfect picks.',
    'displayed success chance uses the same bonus.','Возврат ед. хода после уклонения','Возврат стоимости рипоста %'
)) { if(-not $spPolishAudit0111.Contains($needle0111)){ throw "Survivor 0.11.1 polish audit missing: $needle0111" } }
if(-not $manifestPolishAudit0111.Contains('"version": "0.11.1"')) { throw 'Survivor 0.11.1 manifest audit failed.' }
if(-not $spPolishAudit0111.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.1 state schema changed unexpectedly.' }
Write-Host "Survivor 0.11.1 Reactive Mechanics Polish module audit: PASS (323+ nodes preserved)" -ForegroundColor Green

function Apply-SurvivorReactiveEdgePolish0112 {
    Write-Host "Applying Survivor Progression 0.11.2: Reactive Edge-Case Polish..." -ForegroundColor Cyan
    $sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))
    $manifest = Normalize-Lf ([IO.File]::ReadAllText($manifestPath))
    if($sp.Contains('Survivor Progression v0.11.2') -and $manifest.Contains('"version": "0.11.2"')) {
        $existingStart0112 = $sp.IndexOf('const perk_def perks[] = {')
        $existingEnd0112 = $sp.IndexOf("`n};",$existingStart0112)
        if($existingStart0112 -lt 0 -or $existingEnd0112 -lt 0) { throw 'Survivor 0.11.2 existing-source perk-array boundary missing.' }
        $existingCount0112 = [regex]::Matches($sp.Substring($existingStart0112,$existingEnd0112-$existingStart0112),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
        if($existingCount0112 -lt 323) { throw "Survivor 0.11.2 existing-source node count unexpectedly low: $existingCount0112" }
        foreach($existingId0112 in @('cr_riposte','cr_predator_momentum','fr_quality_control','gr_alarm_bypass','ar_momentum_engine')) {
            if(-not $sp.Contains('{ "'+$existingId0112+'", branch_id::')) { throw "Survivor 0.11.2 existing-source required perk missing: $existingId0112" }
        }
        if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.2 existing-source state schema mismatch.' }
        Write-Host "Survivor 0.11.2 edge-case polish already present ($existingCount0112 nodes audited)." -ForegroundColor Green
        return
    }
    if(-not $sp.Contains('Survivor Progression v0.11.1') -or -not $manifest.Contains('"version": "0.11.1"')) {
        throw 'Survivor 0.11.2 edge-case polish requires the completed 0.11.1 Reactive Mechanics Polish layer.'
    }

    $perkStartBefore0112 = $sp.IndexOf('const perk_def perks[] = {')
    $perkEndBefore0112 = $sp.IndexOf("`n};",$perkStartBefore0112)
    if($perkStartBefore0112 -lt 0 -or $perkEndBefore0112 -lt 0) { throw 'Survivor 0.11.2 pre-polish perk-array boundary missing.' }
    $perkCountBefore0112 = [regex]::Matches($sp.Substring($perkStartBefore0112,$perkEndBefore0112-$perkStartBefore0112),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountBefore0112 -lt 323) { throw "Survivor 0.11.2 pre-polish node count unexpectedly low: $perkCountBefore0112" }

    # Momentum is a transient combat state. Clamp corrupted/stale timers and clear it as soon as the owning perk is absent.
    $turnOld0112 = @'
    if( event_id == NCMM_EVENT_TURN_V2 ) {
        int64_t remaining = std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) );
        if( remaining <= 0 ) return;
        --remaining;
        set_state( "momentum_turns", remaining );
        if( remaining == 0 && get_state( "momentum_stacks", 0 ) != 0 ) {
            set_state( "momentum_stacks", 0 );
            effects_dirty = true;
            recalculate_effects();
        }
    }
'@
    $turnNew0112 = @'
    if( event_id == NCMM_EVENT_TURN_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) {
            if( get_state( "momentum_stacks", 0 ) != 0 || get_state( "momentum_turns", 0 ) != 0 ) {
                set_state( "momentum_stacks", 0 );
                set_state( "momentum_turns", 0 );
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        const int64_t duration_cap = survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12;
        int64_t remaining = std::min<int64_t>( duration_cap,
                            std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) ) );
        if( remaining <= 0 ) {
            if( get_state( "momentum_stacks", 0 ) != 0 ) {
                set_state( "momentum_stacks", 0 );
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        --remaining;
        set_state( "momentum_turns", remaining );
        if( remaining == 0 && get_state( "momentum_stacks", 0 ) != 0 ) {
            set_state( "momentum_stacks", 0 );
            effects_dirty = true;
            recalculate_effects();
        }
    }
'@
    $sp = Replace-TextBlock $sp $turnOld0112 $turnNew0112 'Survivor 0.11.2 Momentum turn-handler'

    # Clamp transient stack/timer reads to the currently-owned perk caps so corrupt/stale state can never amplify modifiers.
    $calcOld0112 = @'
    const int64_t momentum_stacks = std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) );
    const int64_t momentum_turns = std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) );
    if( momentum_stacks > 0 && momentum_turns > 0 && owns_id( "cr_predator_momentum" ) ) {
        double damage_per_stack = 3.0;
        double speed_per_stack = 1.0;
        if( owns_id( "ar_momentum_engine" ) ) {
            damage_per_stack += 1.0;
            speed_per_stack += 1.0;
        }
        result.modifiers["sp_damage_dealt_pct"] += momentum_stacks * damage_per_stack;
        result.modifiers["speed_pct"] += momentum_stacks * speed_per_stack;
    }
'@
    $calcNew0112 = @'
    int64_t momentum_stack_cap = owns_id( "cr_relentless_momentum" ) ? 5 : 3;
    if( owns_id( "ar_momentum_engine" ) ) momentum_stack_cap += 2;
    const int64_t momentum_turn_cap = owns_id( "cr_relentless_momentum" ) ? 20 : 12;
    const int64_t momentum_stacks = std::min<int64_t>( momentum_stack_cap,
                                      std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) ) );
    const int64_t momentum_turns = std::min<int64_t>( momentum_turn_cap,
                                     std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) ) );
    if( momentum_stacks > 0 && momentum_turns > 0 && owns_id( "cr_predator_momentum" ) ) {
        double damage_per_stack = 3.0;
        double speed_per_stack = 1.0;
        if( owns_id( "ar_momentum_engine" ) ) {
            damage_per_stack += 1.0;
            speed_per_stack += 1.0;
        }
        result.modifiers["sp_damage_dealt_pct"] += momentum_stacks * damage_per_stack;
        result.modifiers["speed_pct"] += momentum_stacks * speed_per_stack;
    }
'@
    $sp = Replace-TextBlock $sp $calcOld0112 $calcNew0112 'Survivor 0.11.2 Momentum modifier'

    # A full respec must immediately destroy transient Momentum, preventing a buy -> stack -> respec -> rebuy carryover.
    $respecOld0112 = @'
    for( const char *key : {
             "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
             "prime_aftershock_exoplanet", "prime_aftershock_prime",
             "prime_secronom", "prime_secronom_plus"
         } ) {
        set_state( key, 0 );
    }
    effects_dirty = true;
'@
    $respecNew0112 = @'
    for( const char *key : {
             "prime_magiclysm", "prime_mindovermatter", "prime_xedra_evolved",
             "prime_aftershock_exoplanet", "prime_aftershock_prime",
             "prime_secronom", "prime_secronom_plus"
         } ) {
        set_state( key, 0 );
    }
    set_state( "momentum_stacks", 0 );
    set_state( "momentum_turns", 0 );
    effects_dirty = true;
'@
    $sp = Replace-TextBlock $sp $respecOld0112 $respecNew0112 'Survivor 0.11.2 respec Momentum reset'

    # Text now mirrors the edge-hardened runtime exactly.
    $sp = $sp.Replace(
        'Reactive: after a successful dodge, if no martial-arts counter fires and you have combat stamina, gain a 20% chance for one guarded automatic melee counterattack.',
        'Reactive: after a successful dodge, if no martial-arts counter fires and you have combat stamina, gain a 20% chance to counter the adjacent hostile attacker. Friendly, neutral and hallucination sources are never auto-targeted.')
    $sp = $sp.Replace(
        'Реакция: после успешного уклонения, если не сработала контратака боевого искусства и хватает выносливости, есть 20% шанс на одну защищённую автоматическую контратаку.',
        'Реакция: после успешного уклонения, если не сработала контратака боевого искусства и хватает выносливости, есть 20% шанс контратаковать соседнего враждебного атакующего. Дружественные, нейтральные и иллюзорные цели автоматически не атакуются.')
    $sp = $sp.Replace(
        'After every melee critical: +10 moves and restore 2% maximum stamina.',
        'After every damaging melee critical against a real target: +10 moves and restore 2% maximum stamina.')
    $sp = $sp.Replace(
        'После каждого критического удара в ближнем бою: +10 ед. хода и восстановление 2% максимальной выносливости.',
        'После каждого критического удара в ближнем бою, наносящего урон реальной цели: +10 ед. хода и восстановление 2% максимальной выносливости.')
    $sp = $sp.Replace(
        'Hostile monster kills that grant XP build Momentum, up to 3 stacks for 12 turns. Each stack grants +3% damage and +1% speed. Revived enemies that grant no XP do not add stacks.',
        'Killing a hostile monster that grants XP builds Momentum, up to 3 stacks for 12 turns. Each stack grants +3% damage and +1% speed. Allies, neutral creatures and revived enemies that grant no XP do not add stacks.')
    $sp = $sp.Replace(
        'Убийства монстров, дающие опыт, накапливают Импульс: до 3 зарядов на 12 ходов; каждый заряд даёт +3% исходящего урона и +1% скорости. Убийства возрождённых целей без опыта не срабатывают.',
        'Убийство враждебного монстра, за которое начисляется опыт, даёт заряд Импульса: до 3 зарядов на 12 ходов. Каждый заряд даёт +3% урона и +1% скорости. Союзники, нейтральные существа и возрождённые враги без опыта зарядов не дают.')
    $sp = $sp.Replace('every hostile monster kill that grants XP also returns 15 moves and 3% maximum stamina.',
                      'every hostile monster kill that grants XP also returns 15 moves and 3% maximum stamina.')
    $sp = $sp.Replace('каждое убийство монстра, дающее опыт, также возвращает 15 ед. хода и 3% максимальной выносливости.',
                      'каждое убийство враждебного монстра, дающее опыт, также возвращает 15 ед. хода и 3% максимальной выносливости.')
    $sp = $sp.Replace('A hostile monster kill credited to you that grants XP restores 4% maximum stamina.',
                      'A hostile monster kill credited to you that grants XP restores 4% maximum stamina.')
    $sp = $sp.Replace('Каждое убийство монстра, дающее опыт и засчитанное игроку, восстанавливает 4% максимальной выносливости.',
                      'Убийство враждебного монстра, засчитанное вам и дающее опыт, восстанавливает 4% максимальной выносливости.')
    $sp = $sp.Replace('hostile monster kills that grant XP return 5 moves.', 'hostile monster kills that grant XP return 5 moves.')
    $sp = $sp.Replace('убийства монстров, дающие опыт, возвращают 5 ед. хода.', 'убийства враждебных монстров, дающие опыт, возвращают 5 ед. хода.')
    $sp = $sp.Replace('hostile monster kills that grant XP return 10 moves.', 'hostile monster kills that grant XP return 10 moves.')
    $sp = $sp.Replace('убийства монстров, дающие опыт, — 10 ед. хода.', 'убийства враждебных монстров, дающие опыт, — 10 ед. хода.')
    $sp = $sp.Replace('hostile monster kills that grant XP also return 5 moves.', 'hostile monster kills that grant XP also return 5 moves.')
    $sp = $sp.Replace('убийства монстров, дающие опыт, также возвращают 5 ед. хода.', 'убийства враждебных монстров, дающие опыт, также возвращают 5 ед. хода.')
    $sp = $sp.Replace(
        '25% chance to prevent an alarm from triggering after attempting an alarmed lock.',
        '25% chance to prevent an alarm from triggering after attempting an alarmed lock.')
    $sp = $sp.Replace(
        '25% шанс подавить проверку тревоги после попытки взлома сигнализированного замка.',
        '25% шанс не дать сигнализации сработать после попытки взлома защищённого замка.')

    $sp = $sp.Replace('Survivor Progression v0.11.1','Survivor Progression v0.11.2')
    $sp = $sp.Replace('Survivor Progression 0.11.1 initialized:','Survivor Progression 0.11.2 initialized:')
    $sp = $sp.Replace('"0.11.1"','"0.11.2"')
    if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.2 must retain state schema 8.' }

    $perkStart0112 = $sp.IndexOf('const perk_def perks[] = {')
    $perkEnd0112 = $sp.IndexOf("`n};",$perkStart0112)
    if($perkStart0112 -lt 0 -or $perkEnd0112 -lt 0) { throw 'Survivor 0.11.2 perk-array boundary missing.' }
    $perkCount0112 = [regex]::Matches($sp.Substring($perkStart0112,$perkEnd0112-$perkStart0112),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCount0112 -ne $perkCountBefore0112) { throw "Survivor 0.11.2 node preservation failed: before=$perkCountBefore0112 after=$perkCount0112" }
    Write-Utf8NoBom $spPath $sp

    $manifest = $manifest.Replace('"version": "0.11.1"','"version": "0.11.2"')
    Write-Utf8NoBom $manifestPath $manifest
    Write-Host "Survivor Progression 0.11.2 Reactive Edge-Case Polish: READY ($perkCount0112 nodes; zero removed)" -ForegroundColor Green
}

Apply-SurvivorReactiveEdgePolish0112
$spEdgeAudit0112 = [IO.File]::ReadAllText($spPath)
$manifestEdgeAudit0112 = [IO.File]::ReadAllText($manifestPath)
foreach($needle0112 in @(
    'Survivor Progression v0.11.2','cr_riposte','cr_predator_momentum','fr_quality_control','gr_quick_entry',
    'Friendly, neutral and hallucination sources are never auto-targeted.','damaging melee critical against a real target',
    'Killing a hostile monster that grants XP builds Momentum','hostile monster kill that grants XP',
    'A hostile monster kill credited to you that grants XP restores 4% maximum stamina.',
    'hostile monster kills that grant XP return 5 moves.','hostile monster kills that grant XP return 10 moves.',
    'set_state( "momentum_stacks", 0 );','set_state( "momentum_turns", 0 );','momentum_stack_cap',
    'alarmed lock.'
)) { if(-not $spEdgeAudit0112.Contains($needle0112)){ throw "Survivor 0.11.2 edge audit missing: $needle0112" } }
foreach($obsolete0112 in @(
    'XP-awarding monster kills build Momentum: up to 3 stacks for 12 turns;',
    'suppress the alarm check after an attempt on an alarmed door lock.'
)) { if($spEdgeAudit0112.Contains($obsolete0112)){ throw "Survivor 0.11.2 stale pre-polish text survived: $obsolete0112" } }
if(-not $manifestEdgeAudit0112.Contains('"version": "0.11.2"')) { throw 'Survivor 0.11.2 manifest audit failed.' }
if(-not $spEdgeAudit0112.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.2 state schema changed unexpectedly.' }
Write-Host "Survivor 0.11.2 Reactive Edge-Case Polish module audit: PASS (323+ nodes preserved)" -ForegroundColor Green

function Apply-SurvivorCombinatorialEdgePolish0113 {
    Write-Host "Applying Survivor Progression 0.11.3: Combinatorial Edge Polish..." -ForegroundColor Cyan
    $sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))
    $manifest = Normalize-Lf ([IO.File]::ReadAllText($manifestPath))
    if($sp.Contains('Survivor Progression v0.11.3') -and $manifest.Contains('"version": "0.11.3"')) {
        $existingStart0113 = $sp.IndexOf('const perk_def perks[] = {')
        $existingEnd0113 = $sp.IndexOf("`n};",$existingStart0113)
        if($existingStart0113 -lt 0 -or $existingEnd0113 -lt 0) { throw 'Survivor 0.11.3 existing-source perk-array boundary missing.' }
        $existingCount0113 = [regex]::Matches($sp.Substring($existingStart0113,$existingEnd0113-$existingStart0113),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
        if($existingCount0113 -lt 323) { throw "Survivor 0.11.3 existing-source node count unexpectedly low: $existingCount0113" }
        foreach($existingId0113 in @('cr_riposte','cr_counterflow','cr_critical_surge','cr_execution_protocol','cr_predator_momentum','fr_second_measure')) {
            if(-not $sp.Contains('{ "'+$existingId0113+'", branch_id::')) { throw "Survivor 0.11.3 existing-source required perk missing: $existingId0113" }
        }
        if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.3 existing-source state schema mismatch.' }
        Write-Host "Survivor 0.11.3 combinatorial polish already present ($existingCount0113 nodes audited)." -ForegroundColor Green
        return
    }
    if(-not $sp.Contains('Survivor Progression v0.11.2') -or -not $manifest.Contains('"version": "0.11.2"')) {
        throw 'Survivor 0.11.3 requires the completed 0.11.2 Reactive Edge-Case Polish layer.'
    }

    $perkStartBefore0113 = $sp.IndexOf('const perk_def perks[] = {')
    $perkEndBefore0113 = $sp.IndexOf("`n};",$perkStartBefore0113)
    if($perkStartBefore0113 -lt 0 -or $perkEndBefore0113 -lt 0) { throw 'Survivor 0.11.3 pre-polish perk-array boundary missing.' }
    $perkCountBefore0113 = [regex]::Matches($sp.Substring($perkStartBefore0113,$perkEndBefore0113-$perkStartBefore0113),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCountBefore0113 -lt 323) { throw "Survivor 0.11.3 pre-polish node count unexpectedly low: $perkCountBefore0113" }

    # Overflow-safe kill increments: clamp before +1 so hostile kill events cannot overflow tampered state.
    $killOld0113 = @'
    if( event_id == NCMM_EVENT_PLAYER_KILL_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) return;
        int max_stacks = survivor_has_perk_id( "cr_relentless_momentum" ) ? 5 : 3;
        if( survivor_has_perk_id( "ar_momentum_engine" ) ) max_stacks += 2;
        const int64_t stacks = std::min<int64_t>( max_stacks,
                               std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) ) + 1 );
        set_state( "momentum_stacks", stacks );
        set_state( "momentum_turns", survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12 );
        effects_dirty = true;
        recalculate_effects();
        return;
    }
'@
    $killNew0113 = @'
    if( event_id == NCMM_EVENT_PLAYER_KILL_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) return;
        int max_stacks = survivor_has_perk_id( "cr_relentless_momentum" ) ? 5 : 3;
        if( survivor_has_perk_id( "ar_momentum_engine" ) ) max_stacks += 2;
        const int64_t raw_stacks = get_state( "momentum_stacks", 0 );
        const int64_t current_stacks = std::min<int64_t>( max_stacks,
                                       std::max<int64_t>( 0, raw_stacks ) );
        const int64_t stacks = std::min<int64_t>( max_stacks, current_stacks + 1 );
        set_state( "momentum_stacks", stacks );
        set_state( "momentum_turns", survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12 );
        if( stacks != raw_stacks ) {
            effects_dirty = true;
            recalculate_effects();
        }
        return;
    }
'@
    $sp = Replace-TextBlock $sp $killOld0113 $killNew0113 'Survivor 0.11.3 Momentum kill-handler'

    # Self-heal persisted/tampered transient state, while avoiding a full effect recalculation for timer-only changes.
    $turnOld0113 = @'
    if( event_id == NCMM_EVENT_TURN_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) {
            if( get_state( "momentum_stacks", 0 ) != 0 || get_state( "momentum_turns", 0 ) != 0 ) {
                set_state( "momentum_stacks", 0 );
                set_state( "momentum_turns", 0 );
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        const int64_t duration_cap = survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12;
        int64_t remaining = std::min<int64_t>( duration_cap,
                            std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) ) );
        if( remaining <= 0 ) {
            if( get_state( "momentum_stacks", 0 ) != 0 ) {
                set_state( "momentum_stacks", 0 );
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        --remaining;
        set_state( "momentum_turns", remaining );
        if( remaining == 0 && get_state( "momentum_stacks", 0 ) != 0 ) {
            set_state( "momentum_stacks", 0 );
            effects_dirty = true;
            recalculate_effects();
        }
    }
'@
    $turnNew0113 = @'
    if( event_id == NCMM_EVENT_TURN_V2 ) {
        if( !survivor_has_perk_id( "cr_predator_momentum" ) ) {
            if( get_state( "momentum_stacks", 0 ) != 0 || get_state( "momentum_turns", 0 ) != 0 ) {
                set_state( "momentum_stacks", 0 );
                set_state( "momentum_turns", 0 );
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        int64_t stack_cap = survivor_has_perk_id( "cr_relentless_momentum" ) ? 5 : 3;
        if( survivor_has_perk_id( "ar_momentum_engine" ) ) stack_cap += 2;
        const int64_t duration_cap = survivor_has_perk_id( "cr_relentless_momentum" ) ? 20 : 12;
        const int64_t raw_stacks = get_state( "momentum_stacks", 0 );
        const int64_t raw_turns = get_state( "momentum_turns", 0 );
        int64_t stacks = std::min<int64_t>( stack_cap, std::max<int64_t>( 0, raw_stacks ) );
        int64_t remaining = std::min<int64_t>( duration_cap, std::max<int64_t>( 0, raw_turns ) );
        bool modifier_changed = stacks != raw_stacks;
        if( remaining <= 0 ) {
            modifier_changed = modifier_changed || stacks != 0;
            stacks = 0;
        }
        if( stacks != raw_stacks ) set_state( "momentum_stacks", stacks );
        if( remaining != raw_turns ) set_state( "momentum_turns", remaining );
        if( remaining <= 0 ) {
            if( modifier_changed ) {
                effects_dirty = true;
                recalculate_effects();
            }
            return;
        }
        --remaining;
        set_state( "momentum_turns", remaining );
        if( remaining == 0 && stacks != 0 ) {
            set_state( "momentum_stacks", 0 );
            effects_dirty = true;
            recalculate_effects();
        } else if( modifier_changed ) {
            effects_dirty = true;
            recalculate_effects();
        }
    }
'@
    $sp = Replace-TextBlock $sp $turnOld0113 $turnNew0113 'Survivor 0.11.3 Momentum turn self-heal'

    # Text follows the refined runtime semantics.
    $sp = $sp.Replace(
        'Reactive: after a successful dodge, if no martial-arts counter fires and you have combat stamina, gain a 20% chance to counter the adjacent hostile attacker. Friendly, neutral and hallucination sources are never auto-targeted.',
        'Reactive: after a successful dodge on foot, if no martial-arts counter fires and you have combat stamina, gain a 20% chance to counter the adjacent hostile attacker. Friendly, neutral, hallucination and mounted cases are never auto-targeted.')
    $sp = $sp.Replace(
        'Реакция: после успешного уклонения, если не сработала контратака боевого искусства и хватает выносливости, есть 20% шанс контратаковать соседнего враждебного атакующего. Дружественные, нейтральные и иллюзорные цели автоматически не атакуются.',
        'Реакция: после успешного уклонения пешком, если не сработала контратака боевого искусства и хватает выносливости, есть 20% шанс контратаковать соседнего враждебного атакующего. Дружественные, нейтральные, иллюзорные и ситуации верхом исключены.')
    $sp = $sp.Replace(
        'Ripostes refund 50% of the moves they spend; successful dodges also return 5 moves.',
        'Ripostes refund 50% of their base move cost; successful dodges also return 5 moves.')
    $sp = $sp.Replace(
        'Рипост возвращает 50% потраченных единиц хода; успешное уклонение также возвращает 5 ед. хода.',
        'Рипост возвращает 50% базовой стоимости атаки; успешное уклонение также возвращает 5 ед. хода.')
    $sp = $sp.Replace(
        'After every damaging melee critical against a real target: +10 moves and restore 2% maximum stamina.',
        'A damaging melee critical against a hostile target returns 10 moves and restores 2% maximum stamina. Fleeing enemies still count.')
    $sp = $sp.Replace(
        'После каждого критического удара в ближнем бою, наносящего урон реальной цели: +10 ед. хода и восстановление 2% максимальной выносливости.',
        'Критический удар в ближнем бою по враждебной цели возвращает 10 ед. хода и восстанавливает 2% максимальной выносливости. Убегающие враги тоже учитываются.')
    $sp = $sp.Replace(
        'Execute window: against targets at 18% HP or lower, outgoing normal damage is increased by 60%.',
        'Deal +60% damage to hostile targets at 18% health or less. Friendly and neutral targets are unaffected.')
    $sp = $sp.Replace(
        'Окно добивания: по целям с 18% здоровья или меньше обычный исходящий урон увеличивается на 60%.',
        'По враждебным целям с 18% здоровья или меньше урон увеличивается на 60%. Дружественные и нейтральные цели не затрагиваются.')
    $sp = $sp.Replace(
        '10% chance to prevent a crafting failure before it causes defects, destroys components or removes progress. The next failure check still advances normally.',
        '10% chance to prevent a crafting failure before it causes defects, destroys components or removes progress. The next failure check still advances normally.')
    $sp = $sp.Replace(
        '10% шанс предотвратить ошибку крафта до появления дефекта, потери компонентов или прогресса. Следующая проверка ошибки всё равно сдвигается вперёд.',
        '10% шанс предотвратить ошибку крафта до появления дефекта, потери компонентов или прогресса. Следующая проверка ошибки всё равно сдвигается вперёд.')

    $sp = $sp.Replace('Survivor Progression v0.11.2','Survivor Progression v0.11.3')
    $sp = $sp.Replace('Survivor Progression 0.11.2 initialized:','Survivor Progression 0.11.3 initialized:')
    $sp = $sp.Replace('"0.11.2"','"0.11.3"')
    if(-not $sp.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.3 must retain state schema 8.' }

    $perkStart0113 = $sp.IndexOf('const perk_def perks[] = {')
    $perkEnd0113 = $sp.IndexOf("`n};",$perkStart0113)
    if($perkStart0113 -lt 0 -or $perkEnd0113 -lt 0) { throw 'Survivor 0.11.3 perk-array boundary missing.' }
    $perkCount0113 = [regex]::Matches($sp.Substring($perkStart0113,$perkEnd0113-$perkStart0113),'(?m)^\s*\{\s*"[^"]+"\s*,\s*branch_id::').Count
    if($perkCount0113 -ne $perkCountBefore0113) { throw "Survivor 0.11.3 node preservation failed: before=$perkCountBefore0113 after=$perkCount0113" }
    Write-Utf8NoBom $spPath $sp

    $manifest = $manifest.Replace('"version": "0.11.2"','"version": "0.11.3"')
    Write-Utf8NoBom $manifestPath $manifest
    Write-Host "Survivor Progression 0.11.3 Combinatorial Edge Polish: READY ($perkCount0113 nodes; zero removed)" -ForegroundColor Green
}

Apply-SurvivorCombinatorialEdgePolish0113
$spEdgeAudit0113 = [IO.File]::ReadAllText($spPath)
$manifestEdgeAudit0113 = [IO.File]::ReadAllText($manifestPath)
# Module-only audit: engine-hook invariants are verified after Apply-NcmmReactiveMechanics0113.
foreach($needle0113 in @(
    'Survivor Progression v0.11.3','cr_riposte','cr_counterflow','cr_critical_surge','cr_execution_protocol','cr_predator_momentum','fr_second_measure',
    'base move cost','damaging melee critical against a hostile target','Deal +60% damage to hostile targets at 18% health or less',
    'The next failure check still advances normally.','current_stacks + 1','raw_stacks','raw_turns'
)) { if(-not $spEdgeAudit0113.Contains($needle0113)){ throw "Survivor 0.11.3 combinatorial module audit missing: $needle0113" } }
if(-not $manifestEdgeAudit0113.Contains('"version": "0.11.3"')) { throw 'Survivor 0.11.3 manifest audit failed.' }
if(-not $spEdgeAudit0113.Contains('constexpr int state_schema = 8;')) { throw 'Survivor 0.11.3 state schema changed unexpectedly.' }
Write-Host "Survivor 0.11.3 Combinatorial Edge Polish module audit: PASS (323+ nodes preserved)" -ForegroundColor Green


function Apply-NcmmManagerUiV1Source {
    Write-Host "Applying NCMM 0.8.1 two-pane manager + Survivor 0.12.0 live balance settings..." -ForegroundColor Cyan
    $loaderUiPath = Join-Path $NcmmRoot 'host_patch\ncmm_loader.cpp'
    $spUiPath = $spPath
    $spUiManifestPath = $manifestPath
    foreach($requiredUi in @($loaderUiPath,$spUiPath,$spUiManifestPath)){
        if(-not(Test-Path $requiredUi -PathType Leaf)){throw "NCMM manager/settings source missing: $requiredUi"}
    }

    $loaderUi = Normalize-Lf ([IO.File]::ReadAllText($loaderUiPath))
    $loaderUi = Replace-CppRange $loaderUi 'std::map<std::string, std::string> world_setting_owners;' 'std::map<std::string, size_t> manifest_id_counts;' @'
std::map<std::string, std::string> world_setting_owners;
std::map<std::string, uint32_t> world_setting_scopes;
std::string world_setting_string_cache;

struct module_setting_meta {
    std::string module_id;
    std::string setting_id;
    std::string name;
    std::string tooltip;
    std::string type;
    uint32_t scope = NCMM_WORLD_SETTING_LIVE;
    double min_value = 0.0;
    double max_value = 0.0;
    double step = 1.0;
    std::vector<std::pair<std::string, std::string>> choices;
};
std::vector<module_setting_meta> module_settings;
'@ 'manager typed-setting metadata globals'
    $loaderUi = Replace-CppRange $loaderUi 'int world_setting_register_bool(' 'int world_setting_get_bool(' @'
void remember_module_setting( const module_setting_meta &meta )
{
    auto existing = std::find_if( module_settings.begin(), module_settings.end(),
    [&]( const module_setting_meta &entry ) {
        return entry.module_id == meta.module_id && entry.setting_id == meta.setting_id;
    } );
    if( existing != module_settings.end() ) {
        *existing = meta;
    } else {
        module_settings.push_back( meta );
    }
}

bool manager_visible_setting_scope( uint32_t scope )
{
    return scope == NCMM_WORLD_SETTING_LIVE || scope == NCMM_WORLD_SETTING_RELOAD;
}

int world_setting_register_bool( const char *module_id, const char *setting_id,
                                 const char *display_name, const char *tooltip,
                                 int default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    const int registered = get_options().ncmm_register_world_bool(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), default_value != 0 ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "bool";
        meta.scope = scope;
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_register_int( const char *module_id, const char *setting_id,
                                const char *display_name, const char *tooltip,
                                int min_value, int max_value, int default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    const int registered = get_options().ncmm_register_world_int(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), min_value, max_value, default_value ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "int";
        meta.scope = scope;
        meta.min_value = min_value;
        meta.max_value = max_value;
        meta.step = 1.0;
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_register_float( const char *module_id, const char *setting_id,
                                  const char *display_name, const char *tooltip,
                                  double min_value, double max_value, double default_value,
                                  double step, uint32_t scope )
{
    if( !display_name || !tooltip || !std::isfinite( min_value ) || !std::isfinite( max_value ) ||
        !std::isfinite( default_value ) || !std::isfinite( step ) ||
        !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    const int registered = get_options().ncmm_register_world_float(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), static_cast<float>( min_value ),
                               static_cast<float>( max_value ), static_cast<float>( default_value ),
                               static_cast<float>( step ) ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "float";
        meta.scope = scope;
        meta.min_value = min_value;
        meta.max_value = max_value;
        meta.step = step;
        remember_module_setting( meta );
    }
    return registered;
}

int world_setting_register_enum( const char *module_id, const char *setting_id,
                                 const char *display_name, const char *tooltip,
                                 const char *const *value_ids, const char *const *display_names,
                                 size_t count, const char *default_value, uint32_t scope )
{
    if( !display_name || !tooltip || !value_ids || !display_names || !default_value ||
        count == 0 || count > 64 || !claim_world_setting( module_id, setting_id, scope ) ) {
        return 0;
    }
    std::vector<options_manager::id_and_option> items;
    items.reserve( count );
    for( size_t i = 0; i < count; ++i ) {
        if( !value_ids[i] || !display_names[i] ) {
            return 0;
        }
        items.emplace_back( value_ids[i], to_translation( display_names[i] ) );
    }
    const int registered = get_options().ncmm_register_world_enum(
                               setting_id, to_translation( display_name ),
                               to_translation( tooltip ), items, default_value ) ? 1 : 0;
    if( registered && manager_visible_setting_scope( scope ) ) {
        module_setting_meta meta;
        meta.module_id = module_id;
        meta.setting_id = setting_id;
        meta.name = display_name;
        meta.tooltip = tooltip;
        meta.type = "enum";
        meta.scope = scope;
        for( size_t i = 0; i < count; ++i ) {
            meta.choices.emplace_back( value_ids[i], display_names[i] );
        }
        remember_module_setting( meta );
    }
    return registered;
}
'@ 'manager typed-setting registration metadata'
    $loaderUi = Replace-CppRange $loaderUi 'struct manager_entry {' '#ifdef _WIN32' @'
struct manager_entry {
    std::filesystem::path directory;
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string default_hotkey;
    std::string runtime_state;
    std::string reason;
    bool disabled = false;
    bool loaded_now = false;
};

std::string manager_description( const std::filesystem::path &directory )
{
    const std::filesystem::path localized = directory /
        ( russian_ui() ? "about.ru.txt" : "about.en.txt" );
    std::string result = read_text_file( localized );
    if( result.empty() && russian_ui() ) {
        result = read_text_file( directory / "about.en.txt" );
    }
    while( !result.empty() && ( result.back() == '\n' || result.back() == '\r' ) ) {
        result.pop_back();
    }
    return result;
}

std::vector<manager_entry> manager_entries()
{
    std::vector<manager_entry> result;
    const std::filesystem::path mods_root = game_root() / "code_mods";
    if( !std::filesystem::exists( mods_root ) ) {
        return result;
    }

    std::vector<std::filesystem::path> dirs;
    for( const auto &entry : std::filesystem::directory_iterator( mods_root ) ) {
        if( entry.is_directory() && std::filesystem::exists( entry.path() / "ncmm_mod.dll" ) ) {
            dirs.push_back( entry.path() );
        }
    }
    std::sort( dirs.begin(), dirs.end() );

    for( const std::filesystem::path &dir : dirs ) {
        manager_entry entry;
        entry.directory = dir;
        entry.disabled = std::filesystem::exists( dir / "disabled" );
        const loaded_mod *runtime = find_loaded( dir );
        const module_state *state = find_module_state( dir );
        entry.loaded_now = runtime != nullptr;
        if( state ) {
            entry.runtime_state = state->state;
            entry.reason = state->reason;
        }

        if( runtime && runtime->descriptor ) {
            entry.id = runtime->descriptor->id ? runtime->descriptor->id : "";
            entry.name = runtime->descriptor->name ? runtime->descriptor->name : dir.filename().string();
            entry.version = runtime->descriptor->version ? runtime->descriptor->version : "";
            entry.default_hotkey = runtime->default_hotkey;
        } else if( state ) {
            entry.id = state->id;
            entry.name = state->name;
            entry.version = state->version;
            entry.default_hotkey = state->default_hotkey;
        } else {
            const manifest_contract manifest = read_manifest( dir );
            entry.id = manifest.id;
            entry.name = manifest.name;
            entry.version = manifest.version;
            entry.default_hotkey = manifest.ui_hotkey;
            if( entry.name.empty() ) {
                entry.name = dir.filename().string();
            }
        }
        entry.description = manager_description( dir );
        result.push_back( entry );
    }
    return result;
}
'@ 'manager entry metadata and descriptions'
    $loaderUi = Replace-CppRange $loaderUi 'void show_manager()' 'void on_turn()' @'
std::string manager_state_label( const manager_entry &entry )
{
    if( entry.disabled ) return tr_ui( "OFF", "ВЫКЛ" );
    if( entry.runtime_state == "runtime_fault" || entry.runtime_state == "failed" ) {
        return tr_ui( "ON / error", "ВКЛ / ошибка" );
    }
    if( entry.runtime_state == "suspended" ) {
        return tr_ui( "ON / needs attention", "ВКЛ / требует внимания" );
    }
    if( entry.loaded_now ) return tr_ui( "ON / loaded", "ВКЛ / загружен" );
    if( entry.runtime_state == "rejected" ) {
        return tr_ui( "ON / incompatible", "ВКЛ / несовместим" );
    }
    return tr_ui( "ON / restart required", "ВКЛ / нужен перезапуск" );
}

std::vector<const module_setting_meta *> manager_settings_for( const std::string &module_id )
{
    std::vector<const module_setting_meta *> result;
    for( const module_setting_meta &setting : module_settings ) {
        if( setting.module_id == module_id ) {
            result.push_back( &setting );
        }
    }
    return result;
}

std::string manager_setting_value( const module_setting_meta &setting )
{
    if( !get_options().has_option( setting.setting_id ) ) {
        return tr_ui( "unavailable", "недоступно" );
    }
    if( setting.type == "bool" ) {
        return world_setting_get_bool( setting.setting_id.c_str(), 0 ) ?
               tr_ui( "On", "Вкл" ) : tr_ui( "Off", "Выкл" );
    }
    if( setting.type == "int" ) {
        return std::to_string( world_setting_get_i64( setting.setting_id.c_str(), 0 ) );
    }
    if( setting.type == "float" ) {
        std::ostringstream out;
        out << world_setting_get_f64( setting.setting_id.c_str(), 0.0 );
        return out.str();
    }
    const std::string current = world_setting_get_string( setting.setting_id.c_str(), "" );
    for( const auto &choice : setting.choices ) {
        if( choice.first == current ) {
            return choice.second;
        }
    }
    return current;
}

bool manager_adjust_setting( const module_setting_meta &setting, int direction )
{
    if( direction == 0 || !get_options().has_option( setting.setting_id ) ) {
        return false;
    }
    options_manager::cOpt &opt = get_options().get_option( setting.setting_id );
    if( setting.type == "bool" ) {
        const bool current = world_setting_get_bool( setting.setting_id.c_str(), 0 ) != 0;
        opt.setValue( current ? "false" : "true" );
        return true;
    }
    if( setting.type == "int" ) {
        const int64_t current = world_setting_get_i64( setting.setting_id.c_str(), 0 );
        const int64_t next = std::max<int64_t>( static_cast<int64_t>( setting.min_value ),
                             std::min<int64_t>( static_cast<int64_t>( setting.max_value ),
                                               current + direction * static_cast<int64_t>( setting.step ) ) );
        opt.setValue( std::to_string( next ) );
        return next != current;
    }
    if( setting.type == "float" ) {
        const double current = world_setting_get_f64( setting.setting_id.c_str(), 0.0 );
        const double next = std::max( setting.min_value,
                                     std::min( setting.max_value,
                                               current + direction * setting.step ) );
        std::ostringstream value;
        value << next;
        opt.setValue( value.str() );
        return std::abs( next - current ) > 0.000001;
    }
    if( setting.type == "enum" && !setting.choices.empty() ) {
        const std::string current = world_setting_get_string( setting.setting_id.c_str(),
                                    setting.choices.front().first.c_str() );
        size_t index = 0;
        for( size_t i = 0; i < setting.choices.size(); ++i ) {
            if( setting.choices[i].first == current ) {
                index = i;
                break;
            }
        }
        if( direction < 0 && index > 0 ) --index;
        if( direction > 0 && index + 1 < setting.choices.size() ) ++index;
        opt.setValue( setting.choices[index].first );
        return setting.choices[index].first != current;
    }
    return false;
}

bool manager_open_module_ui( const manager_entry &entry )
{
    loaded_mod *runtime = find_loaded_mutable( entry.directory );
    if( runtime == nullptr || runtime->open_ui == nullptr ) {
        return false;
    }
    if( !ensure_state_migrated( *runtime ) ) {
        popup( tr_ui( "This mod could not load its saved data safely. Open NCMM diagnostics for details.",
                      "Не удалось безопасно загрузить сохранённые данные этого мода. Подробности — в диагностике NCMM." ) );
        return true;
    }
    try {
        module_call_scope scope( runtime->descriptor && runtime->descriptor->id ?
                                 runtime->descriptor->id : nullptr );
        runtime->open_ui( &api );
    } catch( ... ) {
        quarantine_runtime_callback( *runtime, runtime_callback_kind::ui, "ui_exception" );
        log_line( NCMM_LOG_WARN, ( "Module UI callback failed: " + entry.name ).c_str() );
        popup( tr_ui( "This mod's interface failed to open and has been disabled for this session.",
                      "Интерфейс мода не открылся и отключён до перезапуска игры." ) );
    }
    return true;
}

void manager_toggle_module( const manager_entry &entry )
{
    const std::filesystem::path marker = entry.directory / "disabled";
    std::error_code ec;
    if( entry.disabled ) {
        std::filesystem::remove( marker, ec );
        if( ec ) {
            popup( tr_ui( "Could not enable the mod.", "Не удалось включить мод." ) );
        } else {
            popup( tr_ui( "Mod enabled. Restart CDDA to apply.",
                          "Мод включён. Перезапустите CDDA для применения." ) );
        }
        return;
    }

    std::ofstream out( marker, std::ios::trunc );
    if( !out ) {
        popup( tr_ui( "Could not disable the mod.", "Не удалось выключить мод." ) );
        return;
    }
    out << "Disabled by NCMM Mod Configuration. Restart required.\n";
    out.close();
    popup( tr_ui( "Mod disabled. Restart CDDA to apply.",
                  "Мод выключен. Перезапустите CDDA для применения." ) );
}

void show_manager()
{
    write_diagnostics_summary();
    while( true ) {
        const std::vector<manager_entry> entries = manager_entries();
        if( entries.empty() ) {
            popup( tr_ui( "No NCMM mods are installed.", "Моды NCMM не установлены." ) );
            return;
        }

        if( TERMX < 78 || TERMY < 20 ) {
            uilist menu;
            menu.text = tr_ui( "NCMM — Mod Configuration", "NCMM — Настройка модов" );
            for( int i = 0; i < static_cast<int>( entries.size() ); ++i ) {
                menu.addentry( i, true, MENU_AUTOASSIGN,
                               "[" + manager_state_label( entries[i] ) + "] " +
                               entries[i].name + "  " + entries[i].version );
            }
            menu.query();
            if( menu.ret < 0 || menu.ret >= static_cast<int>( entries.size() ) ) return;
            const manager_entry &entry = entries[menu.ret];
            loaded_mod *runtime = find_loaded_mutable( entry.directory );
            uilist action;
            action.text = entry.name;
            int open_index = -1;
            if( runtime != nullptr && runtime->open_ui != nullptr ) {
                open_index = 0;
                action.addentry( 0, true, MENU_AUTOASSIGN,
                                 tr_ui( "Open mod interface", "Открыть интерфейс мода" ) );
            }
            const int toggle_index = open_index == 0 ? 1 : 0;
            action.addentry( toggle_index, true, MENU_AUTOASSIGN,
                             entry.disabled ? tr_ui( "Enable mod", "Включить мод" ) :
                             tr_ui( "Disable mod", "Выключить мод" ) );
            action.query();
            if( open_index >= 0 && action.ret == open_index ) manager_open_module_ui( entry );
            else if( action.ret == toggle_index ) manager_toggle_module( entry );
            continue;
        }

        const int frame_width = std::min( TERMX - 2, 118 );
        const int frame_height = std::min( TERMY - 2, 32 );
        const int left_width = std::max( 26, std::min( 36, frame_width / 3 ) );
        const int divider_x = left_width + 1;
        const int right_x = divider_x + 2;
        const int right_width = frame_width - right_x - 2;
        const int list_top = 3;
        const int list_bottom = frame_height - 3;
        const int visible_modules = std::max( 1, list_bottom - list_top + 1 );
        const point origin( ( TERMX - frame_width ) / 2, ( TERMY - frame_height ) / 2 );
        catacurses::window frame = catacurses::newwin( frame_height, frame_width, origin );

        input_context ctxt( "NCMM_MANAGER", keyboard_mode::keychar );
        ctxt.register_cardinal();
        ctxt.register_action( "NEXT_TAB" );
        ctxt.register_action( "CONFIRM" );
        ctxt.register_action( "QUIT" );
        ctxt.register_action( "HELP_KEYBINDINGS" );

        int selected_module = 0;
        int first_module = 0;
        int focus = 0;
        int selected_detail = 0;

        auto keep_module_visible = [&]() {
            if( selected_module < first_module ) first_module = selected_module;
            if( selected_module >= first_module + visible_modules ) {
                first_module = selected_module - visible_modules + 1;
            }
            first_module = std::max( 0, std::min( first_module,
                            std::max( 0, static_cast<int>( entries.size() ) - visible_modules ) ) );
        };

        while( true ) {
            const manager_entry &entry = entries[static_cast<size_t>( selected_module )];
            const std::vector<const module_setting_meta *> settings = manager_settings_for( entry.id );
            loaded_mod *runtime = find_loaded_mutable( entry.directory );
            const bool has_open = runtime != nullptr && runtime->open_ui != nullptr;
            const int detail_count = static_cast<int>( settings.size() ) + ( has_open ? 1 : 0 ) + 1;
            selected_detail = std::max( 0, std::min( selected_detail, detail_count - 1 ) );

            ui_adaptor ui;
            ui.position_from_window( frame );
            ui.on_redraw( [&]( const ui_adaptor & ) {
                werase( frame );
                draw_border( frame, BORDER_COLOR );
                ncmm_trim_and_print_literal( frame, point( 2, 1 ), left_width - 2,
                                            focus == 0 ? c_light_green : c_white,
                                            tr_ui( "NCMM MODS", "МОДЫ NCMM" ) );
                ncmm_trim_and_print_literal( frame, point( right_x, 1 ), right_width,
                                            focus == 1 ? c_light_green : c_white,
                                            tr_ui( "MODULE DETAILS", "СВЕДЕНИЯ О МОДЕ" ) );

                for( int y = 1; y < frame_height - 1; ++y ) {
                    mvwprintz( frame, point( divider_x, y ), BORDER_COLOR, "|" );
                }

                for( int row = 0; row < visible_modules; ++row ) {
                    const int index = first_module + row;
                    if( index >= static_cast<int>( entries.size() ) ) break;
                    const manager_entry &candidate = entries[static_cast<size_t>( index )];
                    const bool active = index == selected_module;
                    std::string label = ( active ? "> " : "  " ) + candidate.name;
                    ncmm_trim_and_print_literal( frame, point( 2, list_top + row ),
                                                left_width - 3,
                                                active ? ( focus == 0 ? c_light_green : c_cyan ) :
                                                c_light_gray, label );
                }

                int y = 3;
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            c_white, entry.name );
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            c_light_gray,
                                            tr_ui( "Version: ", "Версия: " ) + entry.version );
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            entry.loaded_now ? c_light_green : c_yellow,
                                            tr_ui( "Status: ", "Статус: " ) +
                                            manager_state_label( entry ) );
                if( !entry.id.empty() ) {
                    ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                c_dark_gray, "ID: " + entry.id );
                }
                if( !entry.default_hotkey.empty() ) {
                    ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                c_dark_gray,
                                                tr_ui( "Hotkey: ", "Горячая клавиша: " ) +
                                                entry.default_hotkey );
                }
                if( !entry.reason.empty() && entry.reason != "ok" ) {
                    ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                c_light_red, manager_reason_text( entry.reason ) );
                }

                if( !entry.description.empty() ) {
                    const std::vector<std::string> desc = foldstring( entry.description, right_width );
                    for( size_t i = 0; i < std::min<size_t>( 3, desc.size() ) &&
                         y < frame_height - 8; ++i ) {
                        ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                    c_light_gray, desc[i] );
                    }
                }
                ++y;
                ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                            c_white,
                                            tr_ui( "SETTINGS", "НАСТРОЙКИ" ) );

                int detail_index = 0;
                for( const module_setting_meta *setting : settings ) {
                    if( y >= frame_height - 4 ) break;
                    const bool active = focus == 1 && detail_index == selected_detail;
                    const std::string row = ( active ? "> " : "  " ) + setting->name +
                                            "  < " + manager_setting_value( *setting ) + " >";
                    ncmm_trim_and_print_literal( frame, point( right_x, y++ ), right_width,
                                                active ? c_light_green : c_light_gray, row );
                    ++detail_index;
                }

                if( has_open && y < frame_height - 3 ) {
                    const bool active = focus == 1 && detail_index == selected_detail;
                    ncmm_trim_and_print_literal(
                        frame, point( right_x, y++ ), right_width,
                        active ? c_light_green : c_cyan,
                        ( active ? "> " : "  " ) +
                        tr_ui( "[Open mod interface]", "[Открыть интерфейс мода]" ) );
                    ++detail_index;
                }
                if( y < frame_height - 3 ) {
                    const bool active = focus == 1 && detail_index == selected_detail;
                    ncmm_trim_and_print_literal(
                        frame, point( right_x, y++ ), right_width,
                        active ? c_light_green : c_yellow,
                        ( active ? "> " : "  " ) +
                        ( entry.disabled ? tr_ui( "[Enable mod]", "[Включить мод]" ) :
                          tr_ui( "[Disable mod]", "[Выключить мод]" ) ) );
                }

                ncmm_trim_and_print_literal(
                    frame, point( 2, frame_height - 2 ), frame_width - 4, c_dark_gray,
                    tr_ui( "Up/Down: select  Tab: panel  Left/Right: change  Enter: action  Esc: close",
                           "Вверх/вниз: выбор  Tab: панель  Влево/вправо: изменить  Enter: действие  Esc: выход" ) );
                wnoutrefresh( frame );
            } );

            ui_manager::redraw();
            const std::string action = ctxt.handle_input();

            if( action == "QUIT" ) return;
            if( action == "NEXT_TAB" || ( focus == 0 && action == "RIGHT" ) ||
                ( focus == 1 && action == "LEFT" && settings.empty() ) ) {
                focus = 1 - focus;
                continue;
            }
            if( focus == 0 ) {
                if( action == "UP" && selected_module > 0 ) {
                    --selected_module;
                    selected_detail = 0;
                    keep_module_visible();
                } else if( action == "DOWN" &&
                           selected_module + 1 < static_cast<int>( entries.size() ) ) {
                    ++selected_module;
                    selected_detail = 0;
                    keep_module_visible();
                } else if( action == "CONFIRM" ) {
                    focus = 1;
                }
                continue;
            }

            if( action == "UP" && selected_detail > 0 ) {
                --selected_detail;
                continue;
            }
            if( action == "DOWN" && selected_detail + 1 < detail_count ) {
                ++selected_detail;
                continue;
            }

            if( selected_detail < static_cast<int>( settings.size() ) ) {
                if( action == "LEFT" ) {
                    manager_adjust_setting( *settings[static_cast<size_t>( selected_detail )], -1 );
                } else if( action == "RIGHT" || action == "CONFIRM" ) {
                    manager_adjust_setting( *settings[static_cast<size_t>( selected_detail )], 1 );
                }
                continue;
            }

            int action_index = static_cast<int>( settings.size() );
            if( has_open ) {
                if( selected_detail == action_index && action == "CONFIRM" ) {
                    manager_open_module_ui( entry );
                    continue;
                }
                ++action_index;
            }
            if( selected_detail == action_index && action == "CONFIRM" ) {
                manager_toggle_module( entry );
                break;
            }
            if( action == "LEFT" ) {
                focus = 0;
            }
        }
    }
}
'@ 'two-pane NCMM manager UI'
    $loaderUi = $loaderUi.Replace('0.8.0','0.8.1')
    foreach($managerNeedle in @('module_setting_meta','manager_description','manager_setting_value','manager_adjust_setting','NCMM_MANAGER','MODULE DETAILS','СВЕДЕНИЯ О МОДЕ','return "0.8.1";')){
        if(-not $loaderUi.Contains($managerNeedle)){throw "NCMM manager generated source missing: $managerNeedle"}
    }
    Write-Utf8NoBom $loaderUiPath $loaderUi

    $spUi = Normalize-Lf ([IO.File]::ReadAllText($spUiPath))
    if(-not $spUi.Contains('#include <cstdlib>')){
        $spUi = Replace-TextBlock $spUi '#include <cstdint>' ("#include <cstdint>" + "`n" + "#include <cstdlib>") 'Survivor settings strtol include'
    }
    if(-not $spUi.Contains('    "settings.typed.v2",')){
        $spUi = Replace-TextBlock $spUi '    "host_api.v2.core",' ('    "host_api.v2.core",' + "`n" + '    "settings.typed.v2",') 'Survivor typed settings capability'
    }
    if(-not $spUi.Contains('int last_stat_power_pct = -1;')){
        $spUi = Replace-TextBlock $spUi 'int current_xp_bonus_pct = 0;' ('int current_xp_bonus_pct = 0;' + "`n" + 'int last_stat_power_pct = -1;') 'Survivor settings runtime cache'
    }
    $spUi = Replace-CppRange $spUi 'std::string tr( const char *en, const char *ru )' 'bool active_world_mod(' @'
std::string tr( const char *en, const char *ru )
{
    return russian() ? ru : en;
}

constexpr const char *xp_rate_setting = "NCMM_SP_XP_RATE";
constexpr const char *stat_power_setting = "NCMM_SP_STAT_POWER";

int progression_percent_setting( const char *setting_id, int fallback )
{
    if( host2 == nullptr || host2->world_setting_get_string == nullptr ) {
        return fallback;
    }
    const char *raw = host2->world_setting_get_string( setting_id, "" );
    if( raw == nullptr || raw[0] == '\0' ) {
        return fallback;
    }
    char *end = nullptr;
    const long value = std::strtol( raw, &end, 10 );
    if( end == raw || ( end != nullptr && *end != '\0' ) ) {
        return fallback;
    }
    return static_cast<int>( std::max<long>( 25, std::min<long>( 300, value ) ) );
}

int progression_xp_rate_pct()
{
    return progression_percent_setting( xp_rate_setting, 100 );
}

int progression_stat_power_pct()
{
    return progression_percent_setting( stat_power_setting, 100 );
}

bool configure_progression_settings()
{
    if( host2 == nullptr || host2->world_setting_register_enum == nullptr ||
        host2->world_setting_get_string == nullptr ) {
        return false;
    }
    static const char *values[] = {
        "25", "50", "75", "100", "125", "150", "175", "200", "225", "250", "275", "300"
    };
    static const char *labels[] = {
        "25%", "50%", "75%", "100%", "125%", "150%", "175%", "200%", "225%", "250%", "275%", "300%"
    };
    const size_t count = sizeof( values ) / sizeof( values[0] );
    if( !host2->world_setting_register_enum(
            module_id, xp_rate_setting,
            russian() ? "Получение опыта" : "Experience gain",
            russian() ? "Множитель опыта веток и общего уровня Survivor после антифарма. 100% сохраняет стандартный баланс." :
                        "Multiplier for branch XP and the global Survivor level after anti-farm adjustments. 100% keeps the default balance.",
            values, labels, count, "100", NCMM_WORLD_SETTING_LIVE ) ) {
        return false;
    }
    if( !host2->world_setting_register_enum(
            module_id, stat_power_setting,
            russian() ? "Сила стат-перков" : "Stat perk strength",
            russian() ? "Масштабирует прямые бонусы обычных стат-перков. Механические перки не затрагиваются." :
                        "Scales direct bonuses from regular stat perks. Mechanical perks are not affected.",
            values, labels, count, "100", NCMM_WORLD_SETTING_LIVE ) ) {
        return false;
    }
    return true;
}
'@ 'Survivor configurable balance helpers'
    $spUi = Replace-CppRange $spUi 'int64_t anti_farm_adjust( branch_id branch, int64_t raw )' 'int branch_owned_count(' @'
int64_t scale_configured_xp( branch_id branch, int64_t adjusted )
{
    if( adjusted <= 0 ) {
        return 0;
    }
    const int64_t rate = progression_xp_rate_pct();
    const std::string key = branch_state_key( branch, "rate_fraction" );
    int64_t fraction = std::max<int64_t>( 0, get_state( key, 0 ) ) % 100;
    if( adjusted > ( std::numeric_limits<int64_t>::max() - fraction ) /
        std::max<int64_t>( 1, rate ) ) {
        adjusted = ( std::numeric_limits<int64_t>::max() - fraction ) /
                   std::max<int64_t>( 1, rate );
    }
    const int64_t scaled = adjusted * rate + fraction;
    set_state( key, scaled % 100 );
    return scaled / 100;
}

int64_t anti_farm_adjust( branch_id branch, int64_t raw )
{
    if( raw <= 0 ) {
        set_state( branch_state_key( branch, "streak" ), 0 );
        return 0;
    }

    int64_t streak = std::max<int64_t>( 0,
        get_state( branch_state_key( branch, "streak" ), 0 ) );
    streak = std::min<int64_t>( 12, streak + 1 );
    set_state( branch_state_key( branch, "streak" ), streak );

    const int efficiency = branch_xp_efficiency_pct( branch );
    int64_t adjusted = raw * efficiency / 100;
    if( adjusted == 0 && raw >= 5 && efficiency >= 20 ) {
        adjusted = 1;
    }

    int64_t fatigue_gain = branch == branch_id::mastery ?
                           raw * 3 : raw * 8;
    fatigue_gain += std::max<int64_t>( 0, streak - 2 ) * 6;
    fatigue_gain = std::min<int64_t>( 180, fatigue_gain );

    set_state( branch_state_key( branch, "fatigue" ),
               std::min<int64_t>( 1000, branch_fatigue( branch ) + fatigue_gain ) );
    return scale_configured_xp( branch, adjusted );
}
'@ 'Survivor post-anti-farm XP rate'
    $spUi = Replace-CppRange $spUi 'std::string ranked_effect_summary( const perk_def &perk, int rank )' 'std::string perk_description(' @'
std::string ranked_effect_summary( const perk_def &perk, int rank )
{
    if( rank <= 0 ) {
        return {};
    }

    double multiplier = perk_rank_multiplier_for( perk, rank );
    if( effective_kind( perk ) == perk_kind::stat ) {
        multiplier *= static_cast<double>( progression_stat_power_pct() ) / 100.0;
    }
    std::vector<std::string> parts;
    for( int i = 0; i < perk.effect_count; ++i ) {
        if( perk.effects[i].id == nullptr ) {
            continue;
        }
        const double value = perk.effects[i].value * multiplier;
        const std::string sign = value > 0.0 ? "+" : "";
        parts.push_back( effect_label( perk.effects[i].id ) + ": " +
                         sign + format_number( value ) );
    }
    if( perk.xp_bonus_pct != 0 ) {
        const int value = static_cast<int>(
                              std::llround( static_cast<double>( perk.xp_bonus_pct ) * multiplier ) );
        parts.push_back( tr( "Survivor XP: +", "Опыт Survivor: +" ) +
                         std::to_string( value ) + "%" );
    }

    std::string result;
    for( size_t i = 0; i < parts.size(); ++i ) {
        if( i != 0 ) {
            result += ", ";
        }
        result += parts[i];
    }
    return result;
}
'@ 'Survivor configured perk summary'
    $spUi = Replace-CppRange $spUi 'calculated_effects calculate_owned_effects()' 'std::map<std::string, double> owned_effect_totals()' @'
calculated_effects calculate_owned_effects()
{
    calculated_effects result;
    std::array<bool, 6> branch_active = {{ false, false, false, false, false, false }};

    // Pass 1: one ownership lookup per perk.  The old implementation scanned the
    // full catalog once per branch, then again for majors and amplifier effects.
    for( const perk_def &perk : perks ) {
        if( !perk_world_available( perk ) ) {
            continue;
        }
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }
        if( !integration_perk( perk ) ) {
            branch_active[branch_index( perk.branch )] = true;
        }
        if( perk.currency == currency_id::major ) {
            ++result.major_owned;
        }
        if( effective_kind( perk ) == perk_kind::effect ) {
            const double rank_scale = perk_rank_multiplier_for( perk, rank );
            result.branch_amp[branch_index( perk.branch )] +=
                perk.branch_amp_pct * rank_scale / 100.0;
            result.global_amp += perk.global_amp_pct * rank_scale / 100.0;
        }
    }
    for( bool active : branch_active ) {
        if( active ) {
            ++result.active_branches;
        }
    }

    // Pass 2: resolve scaling after active-branch and major totals are known.
    for( const perk_def &perk : perks ) {
        if( !perk_world_available( perk ) ) {
            continue;
        }
        const int rank = perk_rank( perk );
        if( rank <= 0 ) {
            continue;
        }

        double scale = 1.0;
        if( perk.scaling == perk_scaling::per_active_branch ) {
            scale = static_cast<double>( result.active_branches );
        } else if( perk.scaling == perk_scaling::per_owned_major ) {
            scale = static_cast<double>( result.major_owned );
        }

        scale *= perk_rank_multiplier_for( perk, rank );

        if( effective_kind( perk ) == perk_kind::stat ) {
            scale *= result.global_amp * result.branch_amp[branch_index( perk.branch )];
            scale *= static_cast<double>( progression_stat_power_pct() ) / 100.0;
        }

        result.xp_bonus_pct += static_cast<int>( std::llround( perk.xp_bonus_pct * scale ) );
        for( int i = 0; i < perk.effect_count; ++i ) {
            if( perk.effects[i].id != nullptr ) {
                result.modifiers[perk.effects[i].id] += perk.effects[i].value * scale;
            }
        }
    }

    auto owns_id = []( const char *id ) {
        const perk_def *perk = find_perk( id );
        return perk != nullptr && owned( *perk );
    };
    int64_t momentum_stack_cap = owns_id( "cr_relentless_momentum" ) ? 5 : 3;
    if( owns_id( "ar_momentum_engine" ) ) momentum_stack_cap += 2;
    const int64_t momentum_turn_cap = owns_id( "cr_relentless_momentum" ) ? 20 : 12;
    const int64_t momentum_stacks = std::min<int64_t>( momentum_stack_cap,
                                      std::max<int64_t>( 0, get_state( "momentum_stacks", 0 ) ) );
    const int64_t momentum_turns = std::min<int64_t>( momentum_turn_cap,
                                     std::max<int64_t>( 0, get_state( "momentum_turns", 0 ) ) );
    if( momentum_stacks > 0 && momentum_turns > 0 && owns_id( "cr_predator_momentum" ) ) {
        double damage_per_stack = 3.0;
        double speed_per_stack = 1.0;
        if( owns_id( "ar_momentum_engine" ) ) {
            damage_per_stack += 1.0;
            speed_per_stack += 1.0;
        }
        result.modifiers["sp_damage_dealt_pct"] += momentum_stacks * damage_per_stack;
        result.modifiers["speed_pct"] += momentum_stacks * speed_per_stack;
    }    result.xp_bonus_pct = std::max( 0, std::min( 5000, result.xp_bonus_pct ) );
    return result;
}
'@ 'Survivor configured stat effects'
    $spUi = Replace-CppRange $spUi 'void award_global_xp( int64_t raw_gained )' 'int64_t award_branch_xp(' @'
void award_global_xp( int64_t raw_gained )
{
    if( raw_gained <= 0 || !character_available() ) {
        return;
    }

    int64_t fraction = get_state( "xp_fraction", 0 );
    const int64_t multiplier = std::max<int64_t>( 0, 100 + current_xp_bonus_pct );
    if( raw_gained > ( std::numeric_limits<int64_t>::max() - fraction ) /
        std::max<int64_t>( 1, multiplier ) ) {
        raw_gained = ( std::numeric_limits<int64_t>::max() - fraction ) /
                     std::max<int64_t>( 1, multiplier );
    }
    fraction += raw_gained * multiplier;
    int64_t gained = fraction / 100;
    fraction %= 100;
    set_state( "xp_fraction", fraction );
    if( gained <= 0 ) {
        return;
    }

    int64_t level = std::max<int64_t>( 1, get_state( "level", 1 ) );
    int64_t xp = std::max<int64_t>( 0, get_state( "xp", 0 ) ) + gained;
    int64_t perk_points = get_state( "perk_points", 0 );
    int64_t major_points = get_state( "major_points", 0 );
    int64_t major_awarded = get_state( "major_awarded", 0 );
    int64_t levels_gained = 0;
    int64_t majors_gained = 0;

    while( xp >= xp_to_next( level ) && level < std::numeric_limits<int64_t>::max() ) {
        xp -= xp_to_next( level );
        ++level;
        ++perk_points;
        ++levels_gained;
        if( level % 5 == 0 ) {
            ++major_points;
            ++major_awarded;
            ++majors_gained;
        }
    }

    set_state( "level", level );
    set_state( "xp", xp );
    set_state( "perk_points", perk_points );
    set_state( "major_points", major_points );
    set_state( "major_awarded", major_awarded );

    if( levels_gained > 0 ) {
        std::string text = tr( "Survivor level up! +", "Новый уровень Survivor! +" ) +
                           std::to_string( levels_gained ) +
                           tr( " perk point(s).", " очк. перков." );
        if( majors_gained > 0 ) {
            text += tr( " +", " +" ) + std::to_string( majors_gained ) +
                    tr( " major point(s).", " больших очк." );
        }
        message( text );
    }
}
'@ 'Survivor configured global XP'
    $spUi = Replace-CppRange $spUi 'void tick()' 'bool survivor_has_perk_id(' @'
void tick()
{
    const bool available = character_available();
    if( !available ) {
        if( last_character_available ) {
            clear_runtime_modifiers();
            effects_dirty = true;
        }
        last_character_available = false;
        turn_accumulator = 0;
        return;
    }

    if( !last_character_available ) {
        last_character_available = true;
        migrate_state();
        prime_metric_baselines();
        effects_dirty = true;
    }
    const int stat_power = progression_stat_power_pct();
    if( stat_power != last_stat_power_pct ) {
        last_stat_power_pct = stat_power;
        effects_dirty = true;
    }
    if( effects_dirty ) {
        recalculate_effects();
    }

    ++turn_accumulator;
    if( turn_accumulator < 60 ) {
        return;
    }
    turn_accumulator -= 60;
    poll_branch_xp();
}
'@ 'Survivor live settings refresh'
    $spUi = Replace-CppRange $spUi 'int init( const ncmm_host_api_v1 *api )' 'const ncmm_mod_descriptor_v1 descriptor = {' @'
int init( const ncmm_host_api_v1 *api )
{
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || api->query_interface == nullptr ) return 0;
    host2 = static_cast<const ncmm_host_api_v2_core *>(
                api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 0u ) );
    if( host2 == nullptr || host2->api_major != 2u || !configure_host_api2_runtime_hooks() ) return 0;
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) {
        return 0;
    }
    if( !api->get_api_version_major || !api->get_api_version_minor ||
        api->get_api_version_major() != NCMM_API_VERSION_MAJOR ||
        api->get_api_version_minor() < NCMM_API_VERSION_MINOR ) {
        return 0;
    }
    for( const char *capability : required_caps ) {
        if( !api->has_capability || !api->has_capability( capability ) ) {
            return 0;
        }
    }
    if( !api->character_state_available || !api->character_state_get_i64 ||
        !api->character_state_set_i64 || !api->character_modifier_set ||
        !api->character_modifier_clear_module || !api->ui_choose || !api->ui_tile_choose ||
        !api->ui_card_choose || !api->ui_tree_choose ||
        !api->gameplay_metric_get_i64 || !api->world_mod_active || !api->world_mod_count || !api->world_mod_id || !api->ui_card_choose_themed || !api->ui_tree_choose_themed || !api->ui_tree_choose_rpg || !api->ui_message ) {
        return 0;
    }

    host = api;
    if( !configure_progression_settings() ) {
        return 0;
    }
    last_stat_power_pct = progression_stat_power_pct();
    api->log( NCMM_LOG_INFO,
              "Survivor Progression 0.12.0 initialized: branch bars / exclusive specializations / conditional deep mod integrations." );
    return 1;
}

void shutdown()
{
    clear_runtime_modifiers();
    if( host2 != nullptr && host2->event_unsubscribe_all ) host2->event_unsubscribe_all( module_id );
    host = nullptr;
    host2 = nullptr;
    turn_accumulator = 0;
    last_character_available = false;
    effects_dirty = true;
    current_xp_bonus_pct = 0;
    last_stat_power_pct = -1;
}
'@ 'Survivor settings registration lifecycle'
    if(-not $spUi.Contains('extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1')){
        $localeAnchor = 'extern "C" NCMM_EXPORT void ncmm_on_turn_v1( const ncmm_host_api_v1 *api )'
        $localeHandler = @'
extern "C" NCMM_EXPORT void ncmm_on_locale_changed_v1( const ncmm_host_api_v1 *api )
{
    if( api != nullptr ) {
        host = api;
    }
    configure_progression_settings();
}

'@
        if(-not $spUi.Contains($localeAnchor)){throw 'Survivor locale settings refresh anchor missing.'}
        $spUi = $spUi.Replace($localeAnchor,$localeHandler + $localeAnchor)
    }
    $spUi = $spUi.Replace('0.11.3','0.12.0')
    foreach($settingNeedle in @('NCMM_SP_XP_RATE','NCMM_SP_STAT_POWER','scale_configured_xp','configure_progression_settings','progression_xp_rate_pct','progression_stat_power_pct','settings.typed.v2','Survivor Progression v0.12.0')){
        if(-not $spUi.Contains($settingNeedle)){throw "Survivor settings generated source missing: $settingNeedle"}
    }
    Write-Utf8NoBom $spUiPath $spUi

    $spUiManifest = Normalize-Lf ([IO.File]::ReadAllText($spUiManifestPath))
    if(-not $spUiManifest.Contains('"settings.typed.v2"')){
        $spUiManifest = $spUiManifest.Replace('    "host_api.v2.core",',
            '    "host_api.v2.core",' + "`n" + '    "settings.typed.v2",')
    }
    $spUiManifest = $spUiManifest.Replace('"version": "0.11.3"','"version": "0.12.0"')
    Write-Utf8NoBom $spUiManifestPath $spUiManifest

    $runtimeUiPath = Join-Path $NcmmRoot 'runtime\NCMMBootstrap.cs'
    if(Test-Path $runtimeUiPath -PathType Leaf){
        $runtimeUi = Normalize-Lf ([IO.File]::ReadAllText($runtimeUiPath))
        $runtimeUi = $runtimeUi.Replace('private const string RuntimeVersion = "0.8.0";',
                                        'private const string RuntimeVersion = "0.8.1";')
        if(-not $runtimeUi.Contains('private const string RuntimeVersion = "0.8.1";')){
            throw 'NCMM 0.8.1 bootstrap source promotion failed.'
        }
        Write-Utf8NoBom $runtimeUiPath $runtimeUi
    }

    $setupCoreUiPath = Join-Path $NcmmRoot 'runtime\NCMMSetupCore.cs'
    if(Test-Path $setupCoreUiPath -PathType Leaf){
        $setupCoreUi = Normalize-Lf ([IO.File]::ReadAllText($setupCoreUiPath))
        $setupCoreUi = $setupCoreUi.Replace('internal const string RuntimeVersion = "0.8.0";',
                                            'internal const string RuntimeVersion = "0.8.1";')
        Write-Utf8NoBom $setupCoreUiPath $setupCoreUi
    }

    $applyHostUiPath = Join-Path $NcmmRoot 'host_patch\Apply-NCMMHostPatch.ps1'
    if(Test-Path $applyHostUiPath -PathType Leaf){
        $applyHostUi = Normalize-Lf ([IO.File]::ReadAllText($applyHostUiPath)).Replace('0.8.0','0.8.1')
        if(-not $applyHostUi.Contains('NCMM Host API v1 / NCMM 0.8.1 module contract')){
            throw 'NCMM 0.8.1 host patch marker promotion failed.'
        }
        Write-Utf8NoBom $applyHostUiPath $applyHostUi
    }

    $smokeUiPath = Join-Path $NcmmRoot 'tests\smoke_host.cpp'
    if(Test-Path $smokeUiPath -PathType Leaf){
        $smokeUi = Normalize-Lf ([IO.File]::ReadAllText($smokeUiPath))
        $smokeUi = $smokeUi.Replace('0.8.0-smoke','0.8.1-smoke').Replace('0.11.3','0.12.0')
        Write-Utf8NoBom $smokeUiPath $smokeUi
    }

    $survivorAboutEn = "Character progression system with independent activity XP branches, perks, specializations and optional integrations with supported content mods.`n"
    $survivorAboutRu = "Система развития персонажа с отдельными ветками опыта за действия, перками, специализациями и интеграциями с поддерживаемыми контентными модами.`n"
    Write-Utf8NoBom (Join-Path $NcmmRoot 'mods\SurvivorProgression\about.en.txt') $survivorAboutEn
    Write-Utf8NoBom (Join-Path $NcmmRoot 'mods\SurvivorProgression\about.ru.txt') $survivorAboutRu
    Write-Utf8NoBom (Join-Path $Source0910 'about.en.txt') $survivorAboutEn
    Write-Utf8NoBom (Join-Path $Source0910 'about.ru.txt') $survivorAboutRu
    Write-Utf8NoBom (Join-Path $NcmmRoot 'mods\AdvancedWorldSettings\about.en.txt') "Expanded world-generation and calendar controls, including cities, terrain, water, roads and time settings.`n"
    Write-Utf8NoBom (Join-Path $NcmmRoot 'mods\AdvancedWorldSettings\about.ru.txt') "Расширенные настройки генерации мира и календаря: города, ландшафт, вода, дороги и параметры времени.`n"
    Write-Host "NCMM 0.8.1 manager + Survivor 0.12.0 settings: READY" -ForegroundColor Green
}

Apply-NcmmManagerUiV1Source

function Apply-AwsHostApi20Migration {
    Write-Host "Migrating Advanced World Settings 0.6.1 -> 0.6.2 Host API 2.0..." -ForegroundColor Cyan
    $aws = [IO.File]::ReadAllText($awsPath)
    $manifest = [IO.File]::ReadAllText($awsManifestPath)
    if($aws.Contains('Advanced World Settings 0.6.2 initialized') -and $manifest.Contains('"version": "0.6.2"')) {
        Write-Host "Advanced World Settings 0.6.2 Host API 2.0 migration already present." -ForegroundColor Green
        return
    }
    if(-not $aws.Contains('constexpr const char *module_id = "advanced_world_settings";')) { throw 'AWS 0.6.2 module anchor missing.' }
    $aws = $aws.Replace('constexpr const char *module_id = "advanced_world_settings";',
                        'constexpr const char *module_id = "advanced_world_settings";' + "`n" + 'const ncmm_host_api_v2_core *host2 = nullptr;')
    if(-not $aws.Contains('"world_options.experimental.v1", "locale.v1", "module_contract.v1", "api.versioning.v1"')) { throw 'AWS 0.6.2 capability anchor missing.' }
    $aws = $aws.Replace('"world_options.experimental.v1", "locale.v1", "module_contract.v1", "api.versioning.v1"',
                        '"world_options.experimental.v1", "locale.v1", "module_contract.v1", "api.versioning.v1",' + "`n" +
                        '    "host_api.v2.core", "settings.typed.v2", "worldgen.bindings.v2"')

    $aws = $aws.Replace('return api->world_setting_register_bool( module_id, id, tr( ru, en, ru_name ),',
                        'return host2->world_setting_register_bool( module_id, id, tr( ru, en, ru_name ),')
    $aws = $aws.Replace('return api->world_setting_register_int( module_id, id, tr( ru, en, ru_name ),',
                        'return host2->world_setting_register_int( module_id, id, tr( ru, en, ru_name ),')
    $aws = $aws.Replace('return api->world_setting_register_float( module_id, id, tr( ru, en, ru_name ),',
                        'return host2->world_setting_register_float( module_id, id, tr( ru, en, ru_name ),')

    $bindingCode = @'
struct geography_binding_v2 {
    const char *hook;
    const char *setting;
    uint32_t type;
};

bool bind_geography_hooks_v2()
{
    if( host2 == nullptr || !host2->worldgen_hook_bind_setting ) return false;
    const geography_binding_v2 bindings[] = {
        { "geography.custom.enabled", "NCMM_AWS_CUSTOM_GEOGRAPHY", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.city.size", "NCMM_AWS_CITY_SIZE", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.spacing", "NCMM_AWS_CITY_SPACING", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.max_urbanity", "NCMM_AWS_MAX_URBANITY", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.megacity", "NCMM_AWS_MEGACITY", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.city.shop_radius", "NCMM_AWS_SHOP_RADIUS", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.shop_sigma", "NCMM_AWS_SHOP_SIGMA", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.park_radius", "NCMM_AWS_PARK_RADIUS", NCMM_WORLDGEN_INT_V2 },
        { "geography.city.park_sigma", "NCMM_AWS_PARK_SIGMA", NCMM_WORLDGEN_INT_V2 },
        { "geography.roads.enabled", "NCMM_AWS_PLACE_ROADS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.railroads.enabled", "NCMM_AWS_PLACE_RAILROADS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.specials.enabled", "NCMM_AWS_PLACE_SPECIALS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.neighbor_connections.enabled", "NCMM_AWS_NEIGHBOR_CONNECTIONS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.forests.enabled", "NCMM_AWS_ENABLE_FORESTS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.forests.threshold", "NCMM_AWS_FOREST_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.forests.thick_threshold", "NCMM_AWS_FOREST_THICK_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.swamps.enabled", "NCMM_AWS_ENABLE_SWAMPS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.swamps.adjacent_threshold", "NCMM_AWS_SWAMP_ADJ_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.swamps.isolated_threshold", "NCMM_AWS_SWAMP_ISOLATED_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.swamps.floodplain_min", "NCMM_AWS_FLOODPLAIN_MIN", NCMM_WORLDGEN_INT_V2 },
        { "geography.swamps.floodplain_max", "NCMM_AWS_FLOODPLAIN_MAX", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.enabled", "NCMM_AWS_ENABLE_TRAILS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.trails.chance", "NCMM_AWS_TRAIL_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.min_forest", "NCMM_AWS_TRAIL_MIN_FOREST", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.trailhead_chance", "NCMM_AWS_TRAILHEAD_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.trails.road_distance", "NCMM_AWS_TRAILHEAD_ROAD_DISTANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.enabled", "NCMM_AWS_ENABLE_RIVERS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.rivers.scale", "NCMM_AWS_RIVER_SCALE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.frequency", "NCMM_AWS_RIVER_FREQUENCY", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.rivers.branch_chance", "NCMM_AWS_RIVER_BRANCH_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.remerge_chance", "NCMM_AWS_RIVER_REMERGE_CHANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.rivers.branch_scale_decrease", "NCMM_AWS_RIVER_BRANCH_SCALE_DECREASE", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.lakes.enabled", "NCMM_AWS_ENABLE_LAKES", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.lakes.threshold", "NCMM_AWS_LAKE_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.lakes.min_size", "NCMM_AWS_LAKE_MIN_SIZE", NCMM_WORLDGEN_INT_V2 },
        { "geography.oceans.enabled", "NCMM_AWS_ENABLE_OCEANS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.oceans.threshold", "NCMM_AWS_OCEAN_THRESHOLD", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.oceans.min_size", "NCMM_AWS_OCEAN_MIN_SIZE", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.enabled", "NCMM_AWS_ENABLE_HIGHWAYS", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.highways.grid_row", "NCMM_AWS_HIGHWAY_GRID_ROW", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.grid_column", "NCMM_AWS_HIGHWAY_GRID_COLUMN", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.grid_variance", "NCMM_AWS_HIGHWAY_GRID_VARIANCE", NCMM_WORLDGEN_INT_V2 },
        { "geography.highways.straightness", "NCMM_AWS_HIGHWAY_STRAIGHTNESS", NCMM_WORLDGEN_FLOAT_V2 },
        { "geography.ravines.enabled", "NCMM_AWS_ENABLE_RAVINES", NCMM_WORLDGEN_BOOL_V2 },
        { "geography.ravines.count", "NCMM_AWS_RAVINE_COUNT", NCMM_WORLDGEN_INT_V2 },
        { "geography.ravines.range", "NCMM_AWS_RAVINE_RANGE", NCMM_WORLDGEN_INT_V2 },
        { "geography.ravines.width", "NCMM_AWS_RAVINE_WIDTH", NCMM_WORLDGEN_INT_V2 },
        { "geography.ravines.depth", "NCMM_AWS_RAVINE_DEPTH", NCMM_WORLDGEN_INT_V2 },
    };
    for( const geography_binding_v2 &binding : bindings ) {
        if( !host2->worldgen_hook_bind_setting( module_id, binding.hook, binding.setting, binding.type ) ) return false;
    }
    return true;
}

'@
    $exposeAnchor = 'int expose_all( const ncmm_host_api_v1 *api, bool log_errors )'
    if(-not $aws.Contains($exposeAnchor)) { throw 'AWS 0.6.2 geography binding insertion anchor missing.' }
    $aws = $aws.Replace($exposeAnchor,$bindingCode + $exposeAnchor)

    $initOld = @'
int init( const ncmm_host_api_v1 *api ) {
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION ) return 0;
'@
    $initNew = @'
int init( const ncmm_host_api_v1 *api ) {
    if( api == nullptr || api->abi_version != NCMM_ABI_VERSION || api->query_interface == nullptr ) return 0;
    host2 = static_cast<const ncmm_host_api_v2_core *>(
                api->query_interface( NCMM_HOST_API_V2_CORE_ID, 2u, 0u ) );
    if( host2 == nullptr || host2->api_major != 2u || !host2->worldgen_hook_bind_setting ) return 0;
'@
    $aws = Replace-TextBlock $aws $initOld $initNew 'AWS 0.6.2 API2 init bridge'
    $aws = $aws.Replace('    if( !expose_all( api, true ) ) return 0;' + "`n" +
                        '    api->log( NCMM_LOG_INFO, "Advanced World Settings 0.6.1 initialized: vanilla controls + experimental geography page active." );',
                        '    if( !expose_all( api, true ) || !bind_geography_hooks_v2() ) return 0;' + "`n" +
                        '    api->log( NCMM_LOG_INFO, "Advanced World Settings 0.6.2 initialized: Host API 2.0 typed settings + generic geography hooks active." );')
    $aws = $aws.Replace('void shutdown() {}','void shutdown() { host2 = nullptr; }')
    $aws = $aws.Replace('NCMM_ABI_VERSION, module_id, "Advanced World Settings", "0.6.1",',
                        'NCMM_ABI_VERSION, module_id, "Advanced World Settings", "0.6.2",')
    Write-Utf8NoBom $awsPath $aws

    try {
        $manifestObj = $manifest | ConvertFrom-Json
    } catch {
        throw ('AWS 0.6.2 manifest JSON parse failed before migration: ' + $_.Exception.Message)
    }
    if([string]$manifestObj.id -ne 'advanced_world_settings') { throw 'AWS 0.6.2 manifest module id mismatch.' }
    if([string]$manifestObj.version -ne '0.6.1') { throw ('AWS 0.6.2 manifest source version mismatch: ' + [string]$manifestObj.version) }
    if([int]$manifestObj.api_min_minor -ne 7) { throw ('AWS 0.6.2 manifest source API mismatch: expected 1.7, found 1.' + [string]$manifestObj.api_min_minor) }
    $manifestObj.version = '0.6.2'
    $manifestObj.api_min_minor = 9
    $manifestRequires = @($manifestObj.requires)
    foreach($requiredCapability in @('host_api.v2.core','settings.typed.v2','worldgen.bindings.v2')) {
        if($manifestRequires -notcontains $requiredCapability) { $manifestRequires += $requiredCapability }
    }
    $manifestObj.requires = @($manifestRequires)
    $manifest = ($manifestObj | ConvertTo-Json -Depth 8) + "`n"
    Write-Utf8NoBom $awsManifestPath $manifest
    Write-Host "Advanced World Settings 0.6.2 Host API 2.0 migration: READY" -ForegroundColor Green
}

Apply-AwsHostApi20Migration
Set-InfrastructureTransactionPhase "api2_migrate" "passed" "Host 0.8.1 + Survivor 0.12.0 + AWS 0.6.2 migrations complete"

$awsMigrationAudit = [IO.File]::ReadAllText($awsPath)
$awsManifestMigrationAudit = [IO.File]::ReadAllText($awsManifestPath)
foreach($needle in @('const ncmm_host_api_v2_core *host2 = nullptr;','bind_geography_hooks_v2','NCMM_HOST_API_V2_CORE_ID','worldgen_hook_bind_setting','Advanced World Settings 0.6.2 initialized','host2 = nullptr;')) { if(-not $awsMigrationAudit.Contains($needle)){ throw "AWS 0.6.2 API2 audit missing: $needle" } }
try { $awsManifestMigrationObj = $awsManifestMigrationAudit | ConvertFrom-Json } catch { throw ('AWS 0.6.2 migrated manifest JSON invalid: ' + $_.Exception.Message) }
if([string]$awsManifestMigrationObj.id -ne 'advanced_world_settings' -or [string]$awsManifestMigrationObj.version -ne '0.6.2' -or [int]$awsManifestMigrationObj.api_major -ne 1 -or [int]$awsManifestMigrationObj.api_min_minor -ne 9) {
    throw 'AWS 0.6.2 migrated manifest identity/API audit failed.'
}
$awsManifestRequiresAudit = @($awsManifestMigrationObj.requires)
foreach($needle in @('host_api.v2.core','settings.typed.v2','worldgen.bindings.v2')) {
    if(@($awsManifestRequiresAudit | Where-Object { $_ -eq $needle }).Count -ne 1) { throw "AWS 0.6.2 manifest capability count invalid: $needle" }
    if(-not $awsMigrationAudit.Contains('"' + $needle + '"')) { throw "AWS 0.6.2 C++ required capability missing: $needle" }
}
$awsBindingMatches20 = [regex]::Matches($awsMigrationAudit,'\{ "(geography\.[^"]+)", "(NCMM_AWS_[A-Z0-9_]+)", NCMM_WORLDGEN_(?:BOOL|INT|FLOAT)_V2 \}')
if($awsBindingMatches20.Count -ne 48) { throw "AWS 0.6.2 expected exactly 48 geography bindings, found $($awsBindingMatches20.Count)." }
$awsBindingHooks20 = @($awsBindingMatches20 | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
$awsBindingSettings20 = @($awsBindingMatches20 | ForEach-Object { $_.Groups[2].Value } | Sort-Object -Unique)
if($awsBindingHooks20.Count -ne 48 -or $awsBindingSettings20.Count -ne 48) { throw 'AWS 0.6.2 geography binding IDs are not unique.' }
if(([regex]::Matches($awsMigrationAudit,[regex]::Escape('worldgen_hook_bind_setting'))).Count -lt 2) { throw 'AWS 0.6.2 worldgen binding API not wired.' }
Write-Host "Advanced World Settings 0.6.2 Host API 2.0 module audit: PASS (48/48 unique geography bindings)" -ForegroundColor Green



function Apply-PlayerFacingCopyPolishFinal {
    Write-Host "Applying final player-facing copy polish..." -ForegroundColor Cyan

    $sp = Normalize-Lf ([IO.File]::ReadAllText($spPath))
    $detailFunction = @'
std::string rpg_detail_body( const perk_def &perk, const std::string &body,
                             const std::string &requires_text )
{
    std::string bonus = body;
    std::string drawback;
    if( prime_visual_perk( perk ) ) {
        const size_t colon = bonus.find( ':' );
        if( colon != std::string::npos ) {
            bonus = bonus.substr( colon + 1 );
        }
        const size_t semicolon = bonus.find( ';' );
        if( semicolon != std::string::npos ) {
            drawback = bonus.substr( semicolon + 1 );
            bonus = bonus.substr( 0, semicolon );
        }
        while( !bonus.empty() && bonus.front() == ' ' ) bonus.erase( bonus.begin() );
        while( !drawback.empty() && drawback.front() == ' ' ) drawback.erase( drawback.begin() );
    }

    std::string result;
    result += tr( "BONUS:", "БОНУС:" );
    result += "\n" + bonus;
    if( prime_visual_perk( perk ) && !drawback.empty() ) {
        result += "\n\n";
        result += tr( "DRAWBACK:", "ШТРАФ:" );
        result += "\n" + drawback;
    }
    result += "\n\n";
    result += tr( "REQUIRES:", "ТРЕБУЕТ:" );
    result += "\n" + requires_text;
    return result;
}
'@
    $detailFunctionStart = $sp.IndexOf('std::string rpg_detail_body( const perk_def &perk, const std::string &body,')
    $detailFunctionEnd = $sp.IndexOf('ncmm_ui_theme_v1 branch_ui_theme',$detailFunctionStart)
    if( $detailFunctionStart -lt 0 -or $detailFunctionEnd -le $detailFunctionStart ) {
        throw 'Final copy polish: rpg_detail_body function boundaries missing.'
    }
    $sp = $sp.Substring(0,$detailFunctionStart) + $detailFunction + "`n`n" + $sp.Substring($detailFunctionEnd)
    foreach($pair in @(
        @('Combat stat perks are 5% stronger.','Direct bonuses from Combat perks are 5% stronger.'),
        @('Статовые боевые перки на 5% сильнее.','Прямые бонусы боевых перков на 5% сильнее.'),
        @('Combat stat perks are another 10% stronger.','Direct bonuses from Combat perks are another 10% stronger.'),
        @('Статовые боевые перки ещё на 10% сильнее.','Прямые бонусы боевых перков ещё на 10% сильнее.'),
        @('Survival stat perks are 5% stronger.','Direct bonuses from Survival perks are 5% stronger.'),
        @('Статовые перки выживания на 5% сильнее.','Прямые бонусы перков выживания на 5% сильнее.'),
        @('Survival stat perks are another 10% stronger.','Direct bonuses from Survival perks are another 10% stronger.'),
        @('Статовые перки выживания ещё на 10% сильнее.','Прямые бонусы перков выживания ещё на 10% сильнее.'),
        @('Mobility stat perks are 5% stronger.','Direct bonuses from Mobility perks are 5% stronger.'),
        @('Статовые перки мобильности на 5% сильнее.','Прямые бонусы перков мобильности на 5% сильнее.'),
        @('Mobility stat perks are another 10% stronger.','Direct bonuses from Mobility perks are another 10% stronger.'),
        @('Статовые перки мобильности ещё на 10% сильнее.','Прямые бонусы перков мобильности ещё на 10% сильнее.'),
        @('Crafting stat perks are 5% stronger.','Direct bonuses from Crafting perks are 5% stronger.'),
        @('Статовые перки крафта на 5% сильнее.','Прямые бонусы перков крафта на 5% сильнее.'),
        @('Crafting stat perks are another 10% stronger.','Direct bonuses from Crafting perks are another 10% stronger.'),
        @('Статовые перки крафта ещё на 10% сильнее.','Прямые бонусы перков крафта ещё на 10% сильнее.'),
        @('Scavenging stat perks are 5% stronger.','Direct bonuses from Scavenging perks are 5% stronger.'),
        @('Статовые перки добычи на 5% сильнее.','Прямые бонусы перков добычи на 5% сильнее.'),
        @('Scavenging stat perks are another 10% stronger.','Direct bonuses from Scavenging perks are another 10% stronger.'),
        @('Статовые перки добычи ещё на 10% сильнее.','Прямые бонусы перков добычи ещё на 10% сильнее.'),
        @('All stat perks are 2% stronger.','Direct bonuses from all Survivor perks are 2% stronger.'),
        @('Все статовые перки на 2% сильнее.','Прямые бонусы всех перков Survivor на 2% сильнее.'),
        @('All stat perks are 5% stronger.','Direct bonuses from all Survivor perks are 5% stronger.'),
        @('Все статовые перки на 5% сильнее.','Прямые бонусы всех перков Survivor на 5% сильнее.'),
        @('All stat perks are another 5% stronger.','Direct bonuses from all Survivor perks are another 5% stronger.'),
        @('Все статовые перки ещё на 5% сильнее.','Прямые бонусы всех перков Survivor ещё на 5% сильнее.'),
        @('All stat perks are 10% stronger and Survivor XP +50%.','Direct bonuses from all Survivor perks are 10% stronger; Survivor XP +50%.'),
        @('Все статовые перки на 10% сильнее, опыт Survivor +50%.','Прямые бонусы всех перков Survivor на 10% сильнее; опыт Survivor +50%.'),
        @(' per active Survivor branch.',' for each Survivor branch you''ve invested in.'),
        @(' per active branch.',' for each Survivor branch you''ve invested in.'),
        @(' per owned major perk.',' for each major perk you own.'),
        @(' за каждую активную ветку Survivor.',' за каждую ветку Survivor с купленными перками.'),
        @(' за каждую активную ветку.',' за каждую ветку Survivor с купленными перками.'),
        @(' за каждый большой перк.',' за каждый купленный большой перк.'),
        @('"Integration", "Интеграция"','"Cross-Training", "Разносторонняя подготовка"'),
        @('Momentum cap becomes 5 and lasts 20 turns; every hostile monster kill that grants XP also returns 15 moves and 3% maximum stamina.','Momentum can build to 5 stacks and lasts 20 turns. Killing a hostile monster that grants XP also returns 15 moves and 3% maximum stamina.'),
        @('Лимит Импульса становится 5, длительность — 20 ходов; каждое убийство враждебного монстра, дающее опыт, также возвращает 15 ед. хода и 3% максимальной выносливости.','Импульс накапливается до 5 зарядов и длится 20 ходов. Убийство враждебного монстра, за которое начисляется опыт, также возвращает 15 ед. хода и 3% максимальной выносливости.'),
        @('Melee criticals restore 2% max stamina; hostile monster kills that grant XP return 5 moves.','Melee criticals restore 2% maximum stamina; hostile monster kills that grant XP return 5 moves.'),
        @('Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства враждебных монстров, дающие опыт, возвращают 5 ед. хода.','Критические удары в ближнем бою восстанавливают 2% максимальной выносливости; убийства враждебных монстров, за которые начисляется опыт, возвращают 5 ед. хода.'),
        @('Criticals return 5 moves; hostile monster kills that grant XP return 10 moves. Movement keeps feeding combat tempo.','Criticals return 5 moves; hostile monster kills that grant XP return 10 moves.'),
        @('Криты возвращают 5 ед. хода, убийства враждебных монстров, дающие опыт, — 10 ед. хода. Движение продолжает подпитывать темп боя.','Криты возвращают 5 ед. хода; убийства враждебных монстров, за которые начисляется опыт, — 10 ед. хода.'),
        @('dodges and melee criticals return 5 moves; hostile monster kills that grant XP also return 5 moves.','Dodges, melee criticals and hostile monster kills that grant XP return 5 moves.'),
        @('уклонения и критические удары в ближнем бою возвращают 5 ед. хода; убийства враждебных монстров, дающие опыт, также возвращают 5 ед. хода.','Уклонения, критические удары в ближнем бою и убийства враждебных монстров, за которые начисляется опыт, возвращают 5 ед. хода.'),
        @('XP / synergy / global growth','XP / attributes / cross-training'),
        @('Опыт / синергия / общий рост','Опыт / характеристики / разносторонность'),
        @('XP efficiency ','Experience gain '),
        @('Эффективность XP ','Получение опыта '),
        @('Survivor Progression 0.11.3 initialized: branch progression / perk ranks / anti-farm XP.','Survivor Progression 0.11.3 initialized: branch bars / exclusive specializations / conditional deep mod integrations.'),
        @('Tactical Integration','Combat Doctrine'),
        @('Тактическая интеграция','Боевая доктрина'),
        @('Flow Control','Motion Discipline'),
        @('Контроль потока','Дисциплина движения'),
        @('Standardized Process','Workshop Standards'),
        @('Стандартизация','Стандарты мастерской'),
        @('Systems Thinking','Technical Insight'),
        @('Системное мышление','Техническое чутьё'),
        @('Zero-Defect Process','Flawless Work'),
        @('Бездефектный процесс','Безупречная работа'),
        @('Perfect Process','Masterful Work'),
        @('Совершенный процесс','Работа мастера'),
        @('Apex Combatant','Elite Combatant'),
        @('Вершина боя','Элитный боец'),
        @('Pack Discipline','Efficient Packing'),
        @('Efficient Workflow','Workshop Routine'),
        @('Эффективный процесс','Отлаженная работа'),
        @('Learning Loop','Practice Pays Off'),
        @('Цикл обучения','Учёба на практике'),
        @('Kinetic Mastery','Kinetic Rhythm'),
        @('Мастерство движения','Ритм движения'),
        @('Route Discipline','Efficient Routes'),
        @('Дисциплина маршрута','Экономные маршруты'),
        @('Milestone Discipline','Milestone Focus'),
        @('Дисциплина рубежей','Ориентир на результат'),
        @('Compounding Practice','Lessons Learned'),
        @('Накопительная практика','Усвоенные уроки'),
        @('Deep Practice','Dedicated Study'),
        @('Глубокая практика','Углублённое обучение'),
        @('Compounding Insight','Self-Taught Expertise'),
        @('Накопительное понимание','Опыт самоучки'),
        @('Systems Architect','Chief Engineer'),
        @('Архитектор систем','Главный инженер'),
        @('Prime Systems Savant','Prime Systems Specialist'),
        @('Прайм: Системный савант','Прайм: Специалист по системам'),
        @('Prime Systems Integrator','Prime Systems Controller'),
        @('Прайм: Интегратор систем','Прайм: Системный оператор'),
        @('Prime Rift Operator','Prime Riftwalker'),
        @('Прайм: Оператор разлома','Прайм: Странник разломов'),
        @('Pattern Archive','Anomaly Archive'),
        @('Архив паттернов','Архив аномалий'),
        @('Material Discipline','Careful Handling'),
        @('Дисциплина материалов','Бережная работа'),
        @('Momentum Engine','Unbroken Momentum'),
        @('Двигатель импульса','Непрерывный импульс'),
        @('Master Craft','Masterful Work'),
        @('Мастерская работа','Работа мастера'),
        @('+1 free dodge and +2% speed.','One dodge per refresh costs no stamina; +2% speed.'),
        @('+1 бесплатное уклонение и +2% скорости.','Одно уклонение до восстановления попыток не тратит выносливость; +2% скорости.')
    )) {
        $sp = $sp.Replace([string]$pair[0],[string]$pair[1])
    }

    # Fix the remaining terse generated Russian stat phrases without changing values.
    $grammar = @(
        @('([+-]\d+(?:[.,]\d+)?%) максимум выносливости(?=[,;." ])','$1 максимальной выносливости'),
        @('([+-]\d+(?:[.,]\d+)?%) скорость(?=[,;." ])','$1 скорости'),
        @('([+-]\d+(?:[.,]\d+)?%) выносливость(?=[,;." ])','$1 выносливости'),
        @('([+-]\d+(?:[.,]\d+)?%) грузоподъёмность(?=[,;." ])','$1 грузоподъёмности'),
        @('([+-]\d+(?:[.,]\d+)?%) груза(?=[,;." ])','$1 грузоподъёмности'),
        @('([+-]\d+(?:[.,]\d+)?%) крафт(?=[,;." ])','$1 скорости крафта'),
        @('([+-]\d+(?:[.,]\d+)?%) чтение(?=[,;." ])','$1 скорости чтения'),
        @('([+-]\d+(?:[.,]\d+)?%) лечение(?=[,;." ])','$1 лечения'),
        @('([+-]\d+(?:[.,]\d+)?%) мощность(?=[,;." ])','$1 мощности'),
        @('([+-]\d+(?:[.,]\d+)?%) дальность(?=[,;." ])','$1 дальности'),
        @('([+-]\d+(?:[.,]\d+)?%) площадь(?=[,;." ])','$1 площади'),
        @('([+-]\d+(?:[.,]\d+)?%) длительность(?=[,;." ])','$1 длительности'),
        @('([+-]\d+(?:[.,]\d+)?%) восстановление маны(?=[,;." ])','$1 восстановления маны'),
        @('([+-]\d+(?:[.,]\d+)?%) регена маны(?=[,;." ])','$1 восстановления маны'),
        @('([+-]\d+(?:[.,]\d+)?%) опыт(?=\s|[,;." ])','$1 опыта'),
        @('([+-]\d+(?:[.,]\d+)?%) стоимость движения(?=[,;." ])','$1 стоимости движения'),
        @('([+-]\d+(?:[.,]\d+)?%) движение(?=[,;." ])','$1 стоимости движения'),
        @('([+-]\d+(?:[.,]\d+)?) сила(?=[,;." ])','$1 к силе'),
        @('([+-]\d+(?:[.,]\d+)?) ловкость(?=[,;." ])','$1 к ловкости'),
        @('([+-]\d+(?:[.,]\d+)?) восприятие(?=[,;." ])','$1 к восприятию'),
        @('([+-]\d+(?:[.,]\d+)?) интеллект(?=[,;." ])','$1 к интеллекту'),
        @('([+-]\d+(?:[.,]\d+)?) точность(?=[,;." ])','$1 к точности'),
        @('([+-]\d+(?:[.,]\d+)?) уклонени(?:е|я)(?=[,;." ])','$1 к уклонению')
    )
    foreach($rule in $grammar) {
        $sp = [regex]::Replace($sp,[string]$rule[0],[string]$rule[1])
    }
    Write-Utf8NoBom $spPath $sp
    Copy-Item $spPath (Join-Path $NcmmRoot "mods\SurvivorProgression\src\survivor_progression.cpp") -Force
    Copy-Item $manifestPath (Join-Path $NcmmRoot "mods\SurvivorProgression\mod.json") -Force

    $loader = Normalize-Lf ([IO.File]::ReadAllText($loaderPath))
    foreach($pair in @(
        @('No NCMM code mods are installed.','No NCMM mods are installed.'),
        @('NCMM code-моды не установлены.','Моды NCMM не установлены.'),
        @('NCMM — Mod Configuration\nIn game the manager is a normal remappable keybinding (F2 by default). Modules marked [UI] can be opened with Enter.',
          'NCMM — Mod Configuration\nPress F2 to open this menu (the key can be changed in Controls). Press Enter to open settings for supported mods.'),
        @('NCMM — Настройка модов\nВ игре менеджер — обычное переназначаемое действие (по умолчанию F2). Модули с [UI] открываются через Enter.',
          'NCMM — Настройка модов\nF2 открывает это меню; клавишу можно изменить в управлении. Enter открывает настройки поддерживаемого мода.'),
        @('ON / quarantined','ON / error'),
        @('ВКЛ / карантин','ВКЛ / ошибка'),
        @('ON / suspended','ON / needs attention'),
        @('ВКЛ / приостановлен','ВКЛ / требует внимания'),
        @('ON / loaded','ON'),
        @('ВКЛ / загружен','ВКЛ'),
        @('ON / rejected','ON / incompatible'),
        @('ВКЛ / отклонён','ВКЛ / несовместим'),
        @('ON / failed','ON / error'),
        @('ON / not loaded','ON / restart required'),
        @('ВКЛ / не загружен','ВКЛ / нужен перезапуск'),
        @('label += " [UI]";','label += tr_ui( " [SETTINGS]", " [НАСТРОЙКИ]" );'),
        @('Open module UI','Open settings'),
        @('Открыть интерфейс мода','Открыть настройки'),
        @('Disable module','Disable mod'),
        @('Выключить модуль','Выключить мод'),
        @('Module state migration is suspended for this character. See NCMM diagnostics.',
          'This mod could not load its saved data safely. Open NCMM diagnostics for details.'),
        @('Миграция состояния модуля приостановлена для этого персонажа. См. диагностику NCMM.',
          'Не удалось безопасно загрузить сохранённые данные этого мода. Подробности — в диагностике NCMM.'),
        @('Module UI callback failed and was quarantined for this session.',
          'This mod''s interface failed to open and has been disabled for this session.'),
        @('Ошибка callback интерфейса мода; callback помещён в карантин до перезапуска.',
          'Интерфейс мода не открылся и отключён до перезапуска игры.'),
        @('Could not enable the module.','Could not enable the mod.'),
        @('Не удалось включить модуль.','Не удалось включить мод.'),
        @('Module enabled. Restart CDDA to apply.','Mod enabled. Restart CDDA to apply.'),
        @('Модуль включён. Перезапустите CDDA для применения.','Мод включён. Перезапустите CDDA для применения.'),
        @('Could not disable the module.','Could not disable the mod.'),
        @('Не удалось выключить модуль.','Не удалось выключить мод.'),
        @('Module disabled. Restart CDDA to apply.','Mod disabled. Restart CDDA to apply.'),
        @('Модуль выключен. Перезапустите CDDA для применения.','Мод выключен. Перезапустите CDDA для применения.')
    )) {
        $loader = $loader.Replace([string]$pair[0],[string]$pair[1])
    }

    $loader = [regex]::Replace(
        $loader,
        '(?ms)^[ \t]*if\( !entry\.reason\.empty\(\) && !entry\.disabled &&\s*\( !entry\.loaded_now \|\| entry\.runtime_state == "runtime_fault" \) \) \{\s*label \+= " - " \+ entry\.reason;\s*\}[ \t]*(?:\r?\n)?',
        ''
    )
    Write-Utf8NoBom $loaderPath $loader

    Write-Host "Final player-facing copy polish: READY" -ForegroundColor Green
}

Apply-PlayerFacingCopyPolishFinal

# NCMM Infrastructure 0.8.3.1 deep probe: execute the exact host/source transform stack
# without resolving Visual Studio, compiling binaries, touching the target runtime, or installing files.
if ($HostSourceProbeOnly) {
    Write-Host ""
    Write-Host "=== NCMM Infrastructure 0.8.3.1 DEEP SOURCE PROBE ===" -ForegroundColor Cyan
    $probeRevScript = Join-Path $NcmmRoot "ci\Get-PatchRevision.ps1"
    $probePatchRevision = (& $probeRevScript -RepositoryRoot $NcmmRoot).Trim()
    if ($probePatchRevision -notmatch '^[0-9a-f]{64}$') {
        throw "Deep probe patch revision invalid: $probePatchRevision"
    }
    Ensure-BuildFreeSpace $BuildRoot 3
    Ensure-CddaBuildCache $CddaRoot $BuildRoot $CddaCommit $VcpkgCommit $CddaCacheKey
    $probePatchScript = Join-Path $NcmmRoot "host_patch\Apply-NCMMHostPatch.ps1"
    & $probePatchScript -SourceRoot $CddaRoot
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_host_v1_patched") -PathType Leaf)) { throw "Deep probe: NCMM host marker missing." }
    Apply-WorldSettingsV2Patch $CddaRoot
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_world_settings_v2_patched") -PathType Leaf)) { throw "Deep probe: World Settings marker missing." }
    Apply-AwsWorldgenHostApi20 $CddaRoot
    Apply-NcmmRuntimeGameplayHooksV2 $CddaRoot
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_runtime_gameplay_hooks_v2") -PathType Leaf)) { throw "Deep probe: Host runtime gameplay-hooks marker missing." }
    Apply-NcmmReactiveMechanics0112 $CddaRoot
    Apply-NcmmReactiveMechanics0113 $CddaRoot
    Assert-NcmmReactiveMechanics0113Source $CddaRoot
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_reactive_mechanics_0112") -PathType Leaf)) { throw "Deep probe: Survivor 0.11.2 reactive edge marker missing." }
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_reactive_mechanics_0113") -PathType Leaf)) { throw "Deep probe: Survivor 0.11.3 combinatorial edge marker missing." }
    Apply-RecipeFinalizeProfilerSupportPatch $CddaRoot
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_recipe_finalize_profiler_v1") -PathType Leaf)) { throw "Deep probe: profiler support marker missing." }
    Apply-NcmmRuntimeInfrastructureV8766 $CddaRoot
    if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_runtime_infra_v8766") -PathType Leaf)) { throw "Deep probe: runtime infrastructure marker missing." }
    Assert-NcmmPatchedSourceIntegrity $CddaRoot

    $probeReport = [ordered]@{
        schema = 1
        infrastructure = "0.8.3.1"
        status = "DEEP_SOURCE_PASS"
        source_commit = $CddaCommit
        source_tag = $CddaTag
        adapter_cache_key = $CddaCacheKey
        adapter_support = $TargetSupportMode
        transform_generation = "v8.7.6.8"
        patch_revision = $probePatchRevision
        validation = "full host + World Settings + generic runtime hooks + Survivor API2 migration + profiler + runtime infrastructure source transforms passed"
        checked_utc = [DateTime]::UtcNow.ToString("o")
    }
    $probeDir = Join-Path $BuildRoot "probe_reports"
    New-Item -ItemType Directory -Force $probeDir | Out-Null
    $deepReportPath = Join-Path $probeDir ("NCMM_DEEP_PROBE_" + $CddaCommit.Substring(0,12) + ".json")
    Write-Utf8NoBom $deepReportPath (($probeReport | ConvertTo-Json -Depth 6) + "`n")
    Write-Host "Deep source probe: PASS" -ForegroundColor Green
    Write-Host "Report: $deepReportPath"
    exit 0
}

# Build Survivor 0.12.0 after the preserved 0.11.3 gameplay stack plus NCMM-managed live balance settings.
Write-Host ""
Set-InfrastructureTransactionPhase "source_preflight" "passed" "legacy generation and API2 module migrations passed"
Set-InfrastructureTransactionPhase "compile" "running" "resolving toolchain and compiling modules/host"
Write-Host "Source preflight complete; resolving C++ toolchain..." -ForegroundColor Cyan
$vs = Find-Vs
Write-Host "Visual Studio: $($vs.Root)"
$release0110 = Compile-Survivor $Source0910 (Join-Path $NcmmRoot "sdk") $vs $ReleaseRoot
$releaseAWS = Compile-AdvancedWorldSettings $NcmmRoot (Join-Path $NcmmRoot "sdk") $vs $ReleaseRoot

# 0.9.11-0.9.15 snapshots were emitted at their stage boundaries; 0.9.15 was refreshed after API 1.8/contracts.
$revScript = Join-Path $NcmmRoot "ci\Get-PatchRevision.ps1"
$newPatchRevision = (& $revScript -RepositoryRoot $NcmmRoot).Trim()
if ($newPatchRevision -notmatch '^[0-9a-f]{64}$') {
    throw "Invalid patch revision: $newPatchRevision"
}
Write-Host "Patch revision: $newPatchRevision"

# Refresh cached patched CDDA source and link local host without PDB.
Ensure-BuildFreeSpace $BuildRoot 12
Ensure-CddaBuildCache $CddaRoot $BuildRoot $CddaCommit $VcpkgCommit $CddaCacheKey
$bootstrapGit = Find-BootstrapGit $vs.Root
Write-Host "Bootstrap Git: $bootstrapGit"
Ensure-VcpkgCache $VcpkgRoot $BuildRoot $VcpkgCommit $bootstrapGit

$patchScript = Join-Path $NcmmRoot "host_patch\Apply-NCMMHostPatch.ps1"
& $patchScript -SourceRoot $CddaRoot
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_host_v1_patched") -PathType Leaf)) {
    throw "Apply-NCMMHostPatch marker missing."
}
Apply-WorldSettingsV2Patch $CddaRoot
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_world_settings_v2_patched") -PathType Leaf)) {
    throw "World Settings API v2 geography hook marker missing."
}
Apply-AwsWorldgenHostApi20 $CddaRoot
Apply-NcmmRuntimeGameplayHooksV2 $CddaRoot
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_runtime_gameplay_hooks_v2") -PathType Leaf)) {
    throw "v8.3 mod-native gameplay hook marker missing."
}
Apply-NcmmReactiveMechanics0112 $CddaRoot
Apply-NcmmReactiveMechanics0113 $CddaRoot
Assert-NcmmReactiveMechanics0113Source $CddaRoot
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_reactive_mechanics_0112") -PathType Leaf)) {
    throw "Survivor 0.11.2 reactive edge marker missing."
}
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_reactive_mechanics_0113") -PathType Leaf)) {
    throw "Survivor 0.11.3 combinatorial edge marker missing."
}
Apply-RecipeFinalizeProfilerSupportPatch $CddaRoot
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_recipe_finalize_profiler_v1") -PathType Leaf)) {
    throw "Recipe finalization profiler support marker missing."
}
Apply-NcmmRuntimeInfrastructureV8766 $CddaRoot
if (-not (Test-Path (Join-Path $CddaRoot ".ncmm_runtime_infra_v8766") -PathType Leaf)) {
    throw "v8.7.6.6 runtime infrastructure marker missing."
}
Assert-NcmmPatchedSourceIntegrity $CddaRoot
Set-InfrastructureTransactionPhase "cdda_patch" "passed" "generic Host API2 CDDA integration (including mechanical perk hooks) and patched-source audit passed"

# v8.2: vcpkg dependency verification/install is invoked inside Build-Host-NoPdb only
# when the exact host fingerprint/binary cache cannot be reused.

function Compile-NCMMBootstrap([string]$RepoRoot,[string]$OutputRoot) {
    $source = Join-Path $RepoRoot "runtime\NCMMBootstrap.cs"
    if (-not (Test-Path $source -PathType Leaf)) {
        throw "NCMM bootstrap source missing: $source"
    }

    $csc = Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\csc.exe"
    if (-not (Test-Path $csc -PathType Leaf)) {
        throw "Framework x64 csc.exe missing: $csc"
    }

    New-Item -ItemType Directory -Force $OutputRoot | Out-Null
    $out = Join-Path $OutputRoot "cataclysm-tiles.ncmm-bootstrap.exe"
    Remove-Item $out -Force -ErrorAction SilentlyContinue

    Write-Host "Compiling matching NCMM 0.8.1 bootstrap runtime..." -ForegroundColor Cyan
    & $csc /nologo /target:winexe /optimize+ /platform:x64 `
        /reference:System.Web.Extensions.dll `
        /out:$out `
        $source
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $out -PathType Leaf)) {
        throw "NCMM bootstrap runtime compilation failed."
    }

    Write-Host "NCMM bootstrap runtime: READY" -ForegroundColor Green
    return $out
}

function Show-BootstrapDiagnosis([string]$Root,[object]$State,[string]$BindingPath) {
    Write-Host ""
    Write-Host "=== NCMM BOOTSTRAP STATE ===" -ForegroundColor Yellow
    if ($null -ne $State) {
        foreach ($name in @(
            "runtime_version","loader_api","source_commit","vanilla_sha256",
            "host_sha256","binding_host_sha256","host_valid","host_status",
            "selected_mode","reason","manual_disabled","auto_disabled",
            "boot_pending","offline","diagnostics_only"
        )) {
            Write-Host ("  {0}: {1}" -f $name,$State.$name)
        }
    } else {
        Write-Host "  runtime.state.json could not be parsed."
    }

    if (Test-Path $BindingPath -PathType Leaf) {
        Write-Host ""
        Write-Host "=== HOST BINDING ===" -ForegroundColor Yellow
        Get-Content $BindingPath -Raw | Write-Host
    }

    $bootstrapLog = Join-Path $Root "ncmm\bootstrap.log"
    if (Test-Path $bootstrapLog -PathType Leaf) {
        Write-Host ""
        Write-Host "=== BOOTSTRAP LOG TAIL ===" -ForegroundColor Yellow
        Get-Content $bootstrapLog -Tail 80 | ForEach-Object { Write-Host $_ }
    }
}


$builtHost = Build-Host-NoPdb $CddaRoot $VcpkgRoot $BuildRoot $vs $newPatchRevision
$builtBootstrap = Compile-NCMMBootstrap $NcmmRoot $ReleaseRoot


# Transactional install.
Set-InfrastructureTransactionPhase "compile" "passed" "module and host compilation completed"
Set-InfrastructureTransactionPhase "install" "running" "entering transactional runtime install"
$bindingPath = Join-Path $GameRoot "ncmm\host.binding.json"
$installedBootstrap = Join-Path $GameRoot "cataclysm-tiles.exe"
$installedHost = Join-Path $GameRoot "cataclysm-tiles.ncmm.exe"
$vanillaExe = Join-Path $GameRoot "cataclysm-tiles.vanilla.exe"
$spDir = Join-Path $GameRoot "code_mods\SurvivorProgression"
$awsDir = Join-Path $GameRoot "code_mods\AdvancedWorldSettings"
$hadBinding = Test-Path $bindingPath -PathType Leaf
$hadVanilla = Test-Path $vanillaExe -PathType Leaf
$freshVanillaInstall = (-not $hadBinding) -and (-not $hadVanilla)
if ($hadBinding -xor $hadVanilla) {
    throw "NCMM runtime is partially installed (host.binding.json/cataclysm-tiles.vanilla.exe mismatch). Refusing to guess."
}
if (-not (Test-Path $installedBootstrap -PathType Leaf)) {
    throw "cataclysm-tiles.exe missing before transactional install."
}
$runtimeSourceCommit = Read-GameSourceCommit $GameRoot
$expectedSourceCommit = $CddaCommit.ToLowerInvariant()
if ($freshVanillaInstall) {
    $actualVanillaSha = (Hash-File $installedBootstrap).ToLowerInvariant()
    $oldBinding = [pscustomobject]@{
        source_commit = $expectedSourceCommit
        ncmm_version = "none"
        patch_revision = "none"
    }
    $bindingSourceCommit = $expectedSourceCommit
} else {
    try {
        $oldBinding = Get-Content $bindingPath -Raw | ConvertFrom-Json
    } catch {
        throw "Existing host.binding.json is malformed: $($_.Exception.Message)"
    }
    $actualVanillaSha = (Hash-File $vanillaExe).ToLowerInvariant()
    $bindingSourceCommit = ([string]$oldBinding.source_commit).Trim().ToLowerInvariant()
}
if (-not [string]::IsNullOrWhiteSpace($runtimeSourceCommit) -and
    $runtimeSourceCommit -ne $expectedSourceCommit) {
    throw "Target game source commit mismatch. Expected $expectedSourceCommit, got $runtimeSourceCommit"
}
if (-not [string]::IsNullOrWhiteSpace($bindingSourceCommit) -and
    $bindingSourceCommit -ne $expectedSourceCommit) {
    throw "Existing NCMM binding targets a different CDDA source commit: $bindingSourceCommit"
}
if ([string]::IsNullOrWhiteSpace($runtimeSourceCommit)) {
    $runtimeSourceCommit = $expectedSourceCommit
}

$hadBootstrap = Test-Path $installedBootstrap -PathType Leaf
$hadHost = Test-Path $installedHost -PathType Leaf
$hadNcmmDir = Test-Path (Join-Path $GameRoot "ncmm") -PathType Container
$hadSpDir = Test-Path $spDir -PathType Container
$hadAwsDir = Test-Path $awsDir -PathType Container

Stop-TargetGameProcesses $GameRoot
Wait-Unlocked $installedBootstrap 20
Wait-Unlocked $installedHost 20

$backupRoot = Join-Path $env:USERPROFILE "Downloads\NCMM_Preview_Backups"
New-Item -ItemType Directory -Force $backupRoot | Out-Null
$stage = Join-Path $env:TEMP ("NCMM_SP0915_" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force $stage | Out-Null

try {
    if (Test-Path $installedBootstrap) {
        Copy-Item $installedBootstrap (Join-Path $stage "cataclysm-tiles.exe") -Force
    }
    if (Test-Path $installedHost) {
        Copy-Item $installedHost (Join-Path $stage "cataclysm-tiles.ncmm.exe") -Force
    }
    if ($hadVanilla) {
        Copy-Item $vanillaExe (Join-Path $stage "cataclysm-tiles.vanilla.exe") -Force
    }
    if ($hadBinding) {
        Copy-Item $bindingPath (Join-Path $stage "host.binding.json") -Force
    }
    if ($hadSpDir) {
        Copy-Item $spDir (Join-Path $stage "SurvivorProgression") -Recurse -Force
    }
    if ($hadAwsDir) {
        Copy-Item $awsDir (Join-Path $stage "AdvancedWorldSettings") -Recurse -Force
    }
    $stateStage = Join-Path $stage "ncmm_state"
    New-Item -ItemType Directory -Force $stateStage | Out-Null
    foreach ($stateName in @(
        "bootstrap.sha256","vanilla.sha256","runtime.state.json","runtime.state.json.tmp",
        "modules.state.json","modules.state.json.tmp",
        "migration.latest.json","compatibility.latest.json",
        "ncmm.auto_disabled","ncmm.auto_disabled.tmp"
    )) {
        $stateSource = Join-Path $GameRoot ("ncmm\" + $stateName)
        if (Test-Path $stateSource -PathType Leaf) {
            Copy-Item $stateSource (Join-Path $stateStage $stateName) -Force
        }
    }

    $rollbackManifest = [ordered]@{
        schema = 1
        installer = "v8.7.6.8"
        created_utc = [DateTime]::UtcNow.ToString("o")
        target_game_root = $GameRoot
        target_source_commit = $runtimeSourceCommit
        previous_ncmm_version = [string]$oldBinding.ncmm_version
        previous_patch_revision = [string]$oldBinding.patch_revision
        fresh_vanilla_install = $freshVanillaInstall
        had_bootstrap = $hadBootstrap
        had_host = $hadHost
        had_vanilla_backup = $hadVanilla
        had_binding = $hadBinding
        had_ncmm_directory = $hadNcmmDir
        had_survivor = $hadSpDir
        had_aws = $hadAwsDir
        bootstrap_sha256 = $(if ($hadBootstrap) { Hash-File $installedBootstrap } else { $null })
        host_sha256 = $(if ($hadHost) { Hash-File $installedHost } else { $null })
        vanilla_sha256 = $(if ($hadVanilla) { Hash-File $vanillaExe } else { $null })
        binding_sha256 = $(if ($hadBinding) { Hash-File $bindingPath } else { $null })
    }
    Write-Utf8NoBom (Join-Path $stage "rollback_manifest.json") (($rollbackManifest | ConvertTo-Json -Depth 6) + "`n")

    $backup = Join-Path $backupRoot ("ncmm_before_survivor_0112_" +
                                    (Get-Date -Format "yyyyMMdd_HHmmss") + ".zip")
    Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $backup -Force

    New-Item -ItemType Directory -Force (Join-Path $GameRoot "ncmm") | Out-Null
    New-Item -ItemType Directory -Force (Join-Path $GameRoot "code_mods") | Out-Null
    if ($freshVanillaInstall) {
        # On a new CatLauncher build the launch executable is still vanilla. Preserve it
        # before installing the NCMM bootstrap, then certify the copy byte-for-byte.
        Copy-Item $installedBootstrap $vanillaExe -Force
        if ((Hash-File $vanillaExe).ToLowerInvariant() -ne $actualVanillaSha) {
            throw "Fresh vanilla backup hash mismatch before bootstrap install."
        }
        Write-Host "Fresh vanilla backup created transactionally: $vanillaExe" -ForegroundColor Green
    }

    Copy-Item $builtBootstrap $installedBootstrap -Force
    Copy-Item $builtHost $installedHost -Force
    Remove-Item $spDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $spDir | Out-Null
    $releaseModuleDll = Join-Path $release0110 "ncmm_mod.dll"
    $releaseModuleManifest = Join-Path $release0110 "mod.json"
    $installedModuleDll = Join-Path $spDir "ncmm_mod.dll"
    $installedModuleManifest = Join-Path $spDir "mod.json"
    Copy-Item $releaseModuleDll $installedModuleDll -Force
    Copy-Item $releaseModuleManifest $installedModuleManifest -Force
    foreach($about in Get-ChildItem $release0110 -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
        Copy-Item $about.FullName (Join-Path $spDir $about.Name) -Force
    }
    $releaseAwsDll = Join-Path $releaseAWS "ncmm_mod.dll"
    $releaseAwsManifest = Join-Path $releaseAWS "mod.json"
    Remove-Item $awsDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force $awsDir | Out-Null
    $installedAwsDll = Join-Path $awsDir "ncmm_mod.dll"
    $installedAwsManifest = Join-Path $awsDir "mod.json"
    Copy-Item $releaseAwsDll $installedAwsDll -Force
    Copy-Item $releaseAwsManifest $installedAwsManifest -Force
    foreach($about in Get-ChildItem $releaseAWS -Filter 'about.*.txt' -File -ErrorAction SilentlyContinue){
        Copy-Item $about.FullName (Join-Path $awsDir $about.Name) -Force
    }

    if ((Hash-File $installedBootstrap) -ne (Hash-File $builtBootstrap)) {
        throw "Installed bootstrap hash mismatch after copy."
    }
    if ((Hash-File $installedHost) -ne (Hash-File $builtHost)) {
        throw "Installed host hash mismatch after copy."
    }
    if ((Hash-File $installedModuleDll) -ne (Hash-File $releaseModuleDll)) {
        throw "Installed Survivor DLL hash mismatch after copy."
    }
    if ((Hash-File $installedModuleManifest) -ne (Hash-File $releaseModuleManifest)) {
        throw "Installed Survivor manifest hash mismatch after copy."
    }
    if ((Hash-File $installedAwsDll) -ne (Hash-File $releaseAwsDll)) {
        throw "Installed Advanced World Settings DLL hash mismatch after copy."
    }
    if ((Hash-File $installedAwsManifest) -ne (Hash-File $releaseAwsManifest)) {
        throw "Installed Advanced World Settings manifest hash mismatch after copy."
    }

    $binding = [ordered]@{
        vanilla_sha256 = $actualVanillaSha
        host_sha256 = (Hash-File $installedHost).ToLowerInvariant()
        source_commit = $runtimeSourceCommit
        upstream_tag = $CddaTag
        patch_revision = $newPatchRevision
        ncmm_version = "0.8.1"
        loader_api = 1
        installed_utc = [DateTime]::UtcNow.ToString("o")
    }
    Write-Utf8NoBom $bindingPath (($binding | ConvertTo-Json -Depth 5) + "`n")
    $bootstrapSha = (Hash-File $installedBootstrap).ToLowerInvariant()
    $hostShaNow = (Hash-File $installedHost).ToLowerInvariant()
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\bootstrap.sha256") ($bootstrapSha + "`n")
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\vanilla.sha256") ($actualVanillaSha + "`n")
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\recipe_profiler_support.v1") ("recipe_dictionary.finalize timing support v1`n")
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\runtime_infrastructure.v8766") ("safe-options + diagnostics + NCMM support routing`n")
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\host_api_v2.core") ("NCMM Host 0.8.1 | legacy API 1.9 | Host API 2.0 Core`n")

    Write-Host "Binding inputs:"
    Write-Host "  bootstrap SHA: $bootstrapSha"
    Write-Host "  vanilla SHA:   $actualVanillaSha"
    Write-Host "  host SHA:      $hostShaNow"
    Write-Host "  source commit: $runtimeSourceCommit"

    foreach ($m in @(
        "boot.pending","boot.ready","boot.ready.tmp",
        "ncmm.auto_disabled","ncmm.auto_disabled.tmp"
    )) {
        Remove-Item (Join-Path $GameRoot "ncmm\$m") -Force -ErrorAction SilentlyContinue
    }

    Write-Host "Running NCMM diagnostics..." -ForegroundColor Cyan
    $diag = Start-Process (Join-Path $GameRoot "cataclysm-tiles.exe") `
        -ArgumentList @("--ncmm-offline","--ncmm-diagnose") `
        -WorkingDirectory $GameRoot -PassThru -Wait
    if ($diag.ExitCode -ne 0) {
        throw "Diagnostics failed. Backup: $backup"
    }

    $runtimeStatePath = Join-Path $GameRoot "ncmm\runtime.state.json"
    $state = $null
    try {
        $state = Get-Content $runtimeStatePath -Raw | ConvertFrom-Json
    } catch {
        Show-BootstrapDiagnosis $GameRoot $null $bindingPath
        throw "NCMM diagnostics returned 0 but runtime.state.json could not be parsed. Backup: $backup"
    }

    if (-not [bool]$state.host_valid) {
        Show-BootstrapDiagnosis $GameRoot $state $bindingPath
        throw "Bootstrap rejected the rebuilt host as invalid. Backup: $backup"
    }

    if ([string]$state.selected_mode -ne "NCMM_HOST") {
        Show-BootstrapDiagnosis $GameRoot $state $bindingPath
        if ([bool]$state.manual_disabled) {
            Write-Host ""
            Write-Host "Host is VALID; manual ncmm.disabled state was preserved." -ForegroundColor Yellow
        } else {
            throw "Host is valid but bootstrap selected $($state.selected_mode). Backup: $backup"
        }
    }

    $migrationReport = [ordered]@{
        schema = 1
        installer = "v8.7.6.8"
        infrastructure = "0.8.3.1"
        adapter_cache_key = $CddaCacheKey
        source_commit = $runtimeSourceCommit
        source_changed = (([string]$oldBinding.source_commit).Trim().ToLowerInvariant() -ne $runtimeSourceCommit.Trim().ToLowerInvariant())
        previous_ncmm_version = [string]$oldBinding.ncmm_version
        current_ncmm_version = "0.8.1"
        previous_patch_revision = [string]$oldBinding.patch_revision
        current_patch_revision = $newPatchRevision
        expected_previous_survivor_schema_max = 7
        current_survivor_schema = 8
        active_mod_registry = "active_mods.registry.v2"
        prime_root_count = 39
        mod_native_perk_count = 161
        rollback_backup = $backup
        result = "validated"
        completed_utc = [DateTime]::UtcNow.ToString("o")
    }
    $migrationReportPath = Join-Path $BuildRoot "NCMM_v8.7.6.8_MIGRATION_REPORT.json"
    Write-Utf8NoBom $migrationReportPath (($migrationReport | ConvertTo-Json -Depth 6) + "`n")
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\migration.latest.json") (($migrationReport | ConvertTo-Json -Depth 6) + "`n")

    $compatibilityReport = [ordered]@{
        schema = 2
        infrastructure = "0.8.3.1"
        adapter_cache_key = $CddaCacheKey
        status = $(if ($TargetSupportMode -eq "exact") { "verified_target_exact" } else { "verified_target_structural_reuse" })
        adapter_support = $TargetSupportMode
        source_commit = $runtimeSourceCommit
        source_tag = $CddaTag
        ncmm_version = "0.8.1"
        ncmm_api = "1.9"
        host_api_v2 = "2.0"
        loader_api = 1
        patch_revision = $newPatchRevision
        world_settings_api = "v2 / Host API 2.0 bindings"
        ui_theme_api = "ui.theme.v1"
        ui_layout_api = "ui.layout.v1"
        active_mod_registry = "active_mods.registry.v2"
        survivor = "0.12.0"
        survivor_schema = 8
        aws = "0.6.2"
        validation = "host/bootstrap diagnostics passed"
        generated_utc = [DateTime]::UtcNow.ToString("o")
    }
    $compatibilityReportPath = Join-Path $BuildRoot "NCMM_v8.7.6.8_COMPATIBILITY.json"
    Write-Utf8NoBom $compatibilityReportPath (($compatibilityReport | ConvertTo-Json -Depth 6) + "`n")
    Write-Utf8NoBom (Join-Path $GameRoot "ncmm\compatibility.latest.json") (($compatibilityReport | ConvertTo-Json -Depth 6) + "`n")

    $installReport = [ordered]@{
        installer = "v8.7.6.8"
        infrastructure = "0.8.3.1"
        adapter_cache_key = $CddaCacheKey
        adapter_support = $TargetSupportMode
        install_mode = $(if ($freshVanillaInstall) { "fresh_vanilla" } else { "upgrade_existing_ncmm" })
        build_profile = $script:BuildTuning.Profile
        logical_threads = $script:BuildTuning.LogicalThreads
        target_threads = $script:BuildTuning.TargetThreads
        msbuild_nodes = $script:BuildTuning.MsBuildNodes
        cl_mp_count = $script:BuildTuning.ClMpCount
        effective_compile_slots = $script:BuildTuning.EffectiveCompileSlots
        vcpkg_jobs = $script:BuildTuning.VcpkgJobs
        source_cache_mode = "immutable_pristine_plus_incremental_worktree"
        cdda_commit = $runtimeSourceCommit
        cdda_tag = $CddaTag
        ncmm_version = "0.8.1"
        ncmm_api = "1.9"
        survivor_version = "0.12.0"
        aws_version = "0.6.2"
        patch_revision = $newPatchRevision
        bootstrap_sha256 = $bootstrapSha
        vanilla_sha256 = $actualVanillaSha
        host_sha256 = $hostShaNow
        backup = $backup
        migration_report = $migrationReportPath
        compatibility_report = $compatibilityReportPath
        rollback_manifest = "embedded in backup"
        completed_utc = [DateTime]::UtcNow.ToString("o")
    }
    $installReportPath = Join-Path $BuildRoot "NCMM_v8.7.6.8_INSTALL_REPORT.json"
    Write-Utf8NoBom $installReportPath (($installReport | ConvertTo-Json -Depth 5) + "`n")

    Write-Host ""
    Write-Host "=== SURVIVOR 0.12.0 LIVE SETTINGS / NCMM HOST 0.8.1 INSTALLED ===" -ForegroundColor Green
    Write-Host "0.9.11: complete obstacle-safe neutral connectors + real branch bars"
    Write-Host "0.9.12: activity-diversity branch XP + branch level-up feedback"
    Write-Host "0.9.13: active_mods.v1; Host API 2.0 Core foundation"
    Write-Host "0.9.15: 39 exclusive Prime roots; compact 3-choice Prime rows; horizontal tree viewport; sectioned RPG details"
    Write-Host "0.9.16: Survivor mechanics migrated to Host API 2.0 generic runtime hooks; state schema remains 8"
    Write-Host "0.10.0: 27 mechanical nodes appended; crit, defense, dodge/block attempts and ranked 1-5% full-damage avoidance"
    Write-Host "0.11.0: +25 reactive/technical nodes; ripostes, post-dodge/crit/kill effects, execute window, Momentum, crafting failure control and fieldcraft"
    Write-Host "0.11.1: semantic polish; counter precedence, zero-XP anti-farm, self-damage guard, accurate craft UI estimate and lockpick floors"
    Write-Host "0.11.2: edge polish; hostile-only auto-riposte/kill rewards, damaging-crit gate, Momentum lifecycle clamps/respec reset and lockpick null safety"
    Write-Host "0.11.3: combinatorial edge polish; isolated riposte refund, executed-counter fallback, hostile NPC kill parity, hostile-only reactive damage/crit rewards, overflow-safe self-healing Momentum and monotonic craft-failure saves"
    Write-Host "0.12.0: NCMM-managed live XP rate and stat-perk strength settings; gameplay mechanics remain on the 0.11.3 baseline"
    Write-Host "Integrated worlds: Magiclysm / Mind Over Matter / Xedra Evolved / Aftershock Exoplanet / Aftershock Prime / Secronom / Secronom+"
    Write-Host "Advanced World Settings 0.6.2: Host API 2.0 typed settings + generic geography hooks; custom geography stays under Experimental"
    Write-Host "Snapshots:"
    Write-Host "  $Snap0911"
    Write-Host "  $Snap0912"
    Write-Host "  $Snap0913"
    Write-Host "  $Snap0914"
    Write-Host "  $Snap0915"
    Write-Host "Build profile: $($script:BuildTuning.Profile) ($($script:BuildTuning.TargetThreads) target threads)"
    Write-Host "Install report: $installReportPath"
    Write-Host "Migration report: $migrationReportPath"
    Write-Host "Compatibility report: $compatibilityReportPath"
    Write-Host "Backup: $backup"
    Write-Host ""
    Write-Host "Installed. Start Cataclysm normally. Open NCMM with F2; custom geography is under Experimental and Survivor uses themed branch UI."
} catch {
    Write-Host ""
    Write-Host "Install validation failed. Restoring pre-0.12.0 runtime files..." -ForegroundColor Red

    try {
        Stop-TargetGameProcesses $GameRoot

        $savedBootstrap = Join-Path $stage "cataclysm-tiles.exe"
        if ($hadBootstrap -and (Test-Path $savedBootstrap -PathType Leaf)) {
            Copy-Item $savedBootstrap $installedBootstrap -Force
        } elseif (-not $hadBootstrap) {
            Remove-Item $installedBootstrap -Force -ErrorAction SilentlyContinue
        }

        $savedHost = Join-Path $stage "cataclysm-tiles.ncmm.exe"
        if ($hadHost -and (Test-Path $savedHost -PathType Leaf)) {
            Copy-Item $savedHost $installedHost -Force
        } elseif (-not $hadHost) {
            Remove-Item $installedHost -Force -ErrorAction SilentlyContinue
        }

        $savedVanilla = Join-Path $stage "cataclysm-tiles.vanilla.exe"
        if ($hadVanilla -and (Test-Path $savedVanilla -PathType Leaf)) {
            Copy-Item $savedVanilla $vanillaExe -Force
        } elseif (-not $hadVanilla) {
            Remove-Item $vanillaExe -Force -ErrorAction SilentlyContinue
        }

        $savedBinding = Join-Path $stage "host.binding.json"
        if ($hadBinding -and (Test-Path $savedBinding -PathType Leaf)) {
            Copy-Item $savedBinding $bindingPath -Force
        } elseif (-not $hadBinding) {
            Remove-Item $bindingPath -Force -ErrorAction SilentlyContinue
        }

        $savedSp = Join-Path $stage "SurvivorProgression"
        Remove-Item $spDir -Recurse -Force -ErrorAction SilentlyContinue
        if ($hadSpDir -and (Test-Path $savedSp -PathType Container)) {
            Copy-Item $savedSp $spDir -Recurse -Force
        }
        $savedAws = Join-Path $stage "AdvancedWorldSettings"
        Remove-Item $awsDir -Recurse -Force -ErrorAction SilentlyContinue
        if ($hadAwsDir -and (Test-Path $savedAws -PathType Container)) {
            Copy-Item $savedAws $awsDir -Recurse -Force
        }

        foreach ($m in @("boot.pending","boot.ready","boot.ready.tmp")) {
            Remove-Item (Join-Path $GameRoot "ncmm\$m") -Force -ErrorAction SilentlyContinue
        }
        $savedStateRoot = Join-Path $stage "ncmm_state"
        foreach ($stateName in @(
            "bootstrap.sha256","vanilla.sha256","runtime.state.json","runtime.state.json.tmp",
            "modules.state.json","modules.state.json.tmp",
            "migration.latest.json","compatibility.latest.json",
            "ncmm.auto_disabled","ncmm.auto_disabled.tmp"
        )) {
            $stateTarget = Join-Path $GameRoot ("ncmm\" + $stateName)
            Remove-Item $stateTarget -Force -ErrorAction SilentlyContinue
            $savedState = Join-Path $savedStateRoot $stateName
            if (Test-Path $savedState -PathType Leaf) {
                Copy-Item $savedState $stateTarget -Force
            }
        }

        Write-Host "Rollback: COMPLETE (runtime files + NCMM state restored)" -ForegroundColor Green
    } catch {
        Write-Host "Rollback encountered an error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Manual backup is available at: $backup" -ForegroundColor Yellow
    }

    throw
} finally {
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
}
