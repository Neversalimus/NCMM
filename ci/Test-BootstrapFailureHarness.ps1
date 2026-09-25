param(
    [Parameter(Mandatory=$true)][string]$RepositoryRoot,
    [Parameter(Mandatory=$true)][string]$BootstrapExe
)
$ErrorActionPreference = 'Stop'

$RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
$BootstrapExe = (Resolve-Path $BootstrapExe).Path
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
if (-not (Test-Path $csc)) { throw "Framework csc.exe not found: $csc" }

$work = Join-Path $env:TEMP ('ncmm-bootstrap-failure-harness-' + [guid]::NewGuid().ToString('N'))
$childOut = Join-Path $work 'BootstrapFailureChild.exe'
$childSource = Join-Path $RepositoryRoot 'tests\BootstrapFailureChild.cs'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$passed = 0
$total = 14

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Assert-Equal($Actual, $Expected, [string]$Message) {
    if ($Actual -ne $Expected) {
        throw "$Message (actual='$Actual', expected='$Expected')"
    }
}

function Read-State([string]$Root) {
    $path = Join-Path $Root 'ncmm\runtime.state.json'
    if (-not (Test-Path $path)) { throw "runtime.state.json missing: $Root" }
    return Get-Content $path -Raw | ConvertFrom-Json
}

function Last-Child([string]$Root) {
    $path = Join-Path $Root 'child.log'
    if (-not (Test-Path $path)) { return $null }
    $lines = @(Get-Content $path | Where-Object { $_ })
    if ($lines.Count -eq 0) { return $null }
    return ($lines[-1] -split '\|', 2)[0]
}

function Clear-ReadOnlyTree([string]$Root) {
    if (-not (Test-Path $Root)) { return }
    Get-ChildItem $Root -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            if (-not $_.PSIsContainer) {
                [IO.File]::SetAttributes($_.FullName, [IO.FileAttributes]::Normal)
            }
        } catch { }
    }
}

function Write-ValidBinding([string]$Root) {
    $vanilla = Join-Path $Root 'cataclysm-tiles.vanilla.exe'
    $hostExe = Join-Path $Root 'cataclysm-tiles.ncmm.exe'
    $binding = [ordered]@{
        vanilla_sha256 = (Get-FileHash $vanilla -Algorithm SHA256).Hash.ToLowerInvariant()
        host_sha256 = (Get-FileHash $hostExe -Algorithm SHA256).Hash.ToLowerInvariant()
        source_commit = ''
        upstream_tag = 'failure-harness'
        patch_revision = ('a' * 64)
        ncmm_version = '0.6.4'
        loader_api = 1
        installed_utc = [DateTime]::UtcNow.ToString('o')
    }
    $binding | ConvertTo-Json | Set-Content (Join-Path $Root 'ncmm\host.binding.json') -Encoding UTF8
}

function New-Scenario([string]$Name) {
    $root = Join-Path $work $Name
    New-Item -ItemType Directory -Force -Path (Join-Path $root 'ncmm') | Out-Null
    Copy-Item $BootstrapExe (Join-Path $root 'cataclysm-tiles.exe') -Force
    Copy-Item $childOut (Join-Path $root 'cataclysm-tiles.vanilla.exe') -Force
    Copy-Item $childOut (Join-Path $root 'cataclysm-tiles.ncmm.exe') -Force
    Write-ValidBinding $root
    return $root
}

function Invoke-Bootstrap([string]$Root, [string[]]$Arguments, [int]$ExpectedExit) {
    $exe = Join-Path $Root 'cataclysm-tiles.exe'
    $process = Start-Process -FilePath $exe -ArgumentList $Arguments -WorkingDirectory $Root `
        -PassThru -Wait -WindowStyle Hidden
    Assert-Equal $process.ExitCode $ExpectedExit "Bootstrap exit code mismatch in $Root"
    return $process.ExitCode
}

function Pass([string]$Name) {
    $script:passed++
    Write-Host ("  PASS " + $Name) -ForegroundColor Green
}

function Run-Scenario([string]$Name, [scriptblock]$Body) {
    try {
        & $Body
        Pass $Name
    } catch {
        throw "Bootstrap failure harness scenario '$Name' failed: $($_.Exception.Message)"
    }
}

New-Item -ItemType Directory -Force -Path $work | Out-Null
try {
    & $csc /nologo /target:exe /optimize+ /platform:x64 /out:$childOut $childSource
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $childOut)) {
        throw 'BootstrapFailureChild.cs compilation failed.'
    }

    Run-Scenario 'missing vanilla -> exit 112 / no child' {
        $root = New-Scenario 'missing-vanilla'
        Remove-Item (Join-Path $root 'cataclysm-tiles.vanilla.exe') -Force
        [void](Invoke-Bootstrap $root @('--ncmm-offline') 112)
        Assert-True (-not (Test-Path (Join-Path $root 'child.log'))) 'Child unexpectedly launched.'
        Assert-Equal (Read-State $root).selected_mode 'ERROR' 'Missing vanilla did not enter ERROR mode.'
    }

    Run-Scenario 'valid offline binding -> certified host' {
        $root = New-Scenario 'valid-host'
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.ncmm.exe' 'Certified host was not launched.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\boot.pending'))) 'Host left boot.pending.'
        Assert-True (Test-Path (Join-Path $root 'ncmm\boot.ready')) 'Host did not publish boot.ready.'
        Assert-Equal (Read-State $root).selected_mode 'NCMM_HOST' 'State did not record NCMM_HOST.'
    }

    Run-Scenario 'manual disable -> vanilla' {
        $root = New-Scenario 'manual-disabled'
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\ncmm.disabled'), "disabled`n", $Utf8NoBom)
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Manual disable did not force vanilla.'
        Assert-Equal (Read-State $root).selected_mode 'VANILLA' 'Manual disable state mismatch.'
    }

    Run-Scenario '--ncmm-vanilla -> vanilla' {
        $root = New-Scenario 'forced-vanilla'
        [void](Invoke-Bootstrap $root @('--ncmm-vanilla','--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' '--ncmm-vanilla did not force vanilla.'
    }

    Run-Scenario 'stale runtime binding -> vanilla' {
        $root = New-Scenario 'stale-version'
        $path = Join-Path $root 'ncmm\host.binding.json'
        $binding = Get-Content $path -Raw | ConvertFrom-Json
        $binding.ncmm_version = '0.6.3'
        $binding | ConvertTo-Json | Set-Content $path -Encoding UTF8
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Stale runtime binding was accepted.'
    }

    Run-Scenario 'empty patch revision -> vanilla' {
        $root = New-Scenario 'empty-patch-revision'
        $path = Join-Path $root 'ncmm\host.binding.json'
        $binding = Get-Content $path -Raw | ConvertFrom-Json
        $binding.patch_revision = ''
        $binding | ConvertTo-Json | Set-Content $path -Encoding UTF8
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Empty patch revision was accepted.'
    }

    Run-Scenario 'host SHA mismatch -> vanilla' {
        $root = New-Scenario 'bad-host-sha'
        $path = Join-Path $root 'ncmm\host.binding.json'
        $binding = Get-Content $path -Raw | ConvertFrom-Json
        $binding.host_sha256 = ('0' * 64)
        $binding | ConvertTo-Json | Set-Content $path -Encoding UTF8
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Host SHA mismatch did not fail closed.'
    }

    Run-Scenario 'corrupt binding -> vanilla' {
        $root = New-Scenario 'corrupt-binding'
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\host.binding.json'), '{broken-json', $Utf8NoBom)
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Corrupt binding did not fail closed.'
    }

    Run-Scenario 'host crash -> next launch auto-disables and uses vanilla' {
        $root = New-Scenario 'host-crash'
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-crash') 77)
        Assert-True (Test-Path (Join-Path $root 'ncmm\boot.pending')) 'Crash did not leave boot.pending.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\boot.ready'))) 'Crash unexpectedly left boot.ready.'
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Crash-loop recovery did not use vanilla.'
        Assert-True (Test-Path (Join-Path $root 'ncmm\ncmm.auto_disabled')) 'Auto-disable marker was not persisted.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\boot.pending'))) 'Recovered crash left boot.pending.'
    }

    Run-Scenario '--ncmm-reset clears auto-disable and recovers host' {
        $root = New-Scenario 'reset-recovers'
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\ncmm.auto_disabled'), "disabled`n", $Utf8NoBom)
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\boot.pending'), "stale`n", $Utf8NoBom)
        [void](Invoke-Bootstrap $root @('--ncmm-reset','--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.ncmm.exe' '--ncmm-reset did not restore host launch.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\ncmm.auto_disabled'))) 'Reset left auto-disable marker.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\boot.pending'))) 'Reset/host launch left pending.'
    }

    Run-Scenario 'pending + ready -> cleanup recovery, not crash' {
        $root = New-Scenario 'pending-ready'
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\boot.pending'), "pending`n", $Utf8NoBom)
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\boot.ready'), "ready`n", $Utf8NoBom)
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.ncmm.exe' 'pending+ready was misclassified as a crash.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\ncmm.auto_disabled'))) 'pending+ready created auto-disable.'
    }

    Run-Scenario 'auto-disable persistence failure -> vanilla and pending preserved' {
        $root = New-Scenario 'autodisable-write-failure'
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\boot.pending'), "pending`n", $Utf8NoBom)
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'ncmm\ncmm.auto_disabled.tmp') | Out-Null
        [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
        Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Marker persistence failure did not fail closed.'
        Assert-True (Test-Path (Join-Path $root 'ncmm\boot.pending')) 'Marker persistence failure lost boot.pending.'
        Assert-True (-not (Test-Path (Join-Path $root 'ncmm\ncmm.auto_disabled'))) 'Unexpected auto-disable marker exists.'
    }

    Run-Scenario 'stale ready delete failure -> vanilla' {
        $root = New-Scenario 'ready-delete-failure'
        $ready = Join-Path $root 'ncmm\boot.ready'
        [IO.File]::WriteAllText($ready, "ready`n", $Utf8NoBom)
        [IO.File]::SetAttributes($ready, [IO.FileAttributes]::ReadOnly)
        try {
            [void](Invoke-Bootstrap $root @('--ncmm-offline','--test-host-ready') 0)
            Assert-Equal (Last-Child $root) 'cataclysm-tiles.vanilla.exe' 'Ambiguous stale ready state did not fail closed.'
            Assert-True (-not (Test-Path (Join-Path $root 'ncmm\boot.pending'))) 'Failed ready cleanup still created pending.'
        } finally {
            if (Test-Path $ready) { [IO.File]::SetAttributes($ready, [IO.FileAttributes]::Normal) }
        }
    }

    Run-Scenario 'diagnostics-only preserves pending and launches no child' {
        $root = New-Scenario 'diagnostics-pending'
        [IO.File]::WriteAllText((Join-Path $root 'ncmm\boot.pending'), "pending`n", $Utf8NoBom)
        [void](Invoke-Bootstrap $root @('--ncmm-diagnose','--ncmm-offline') 0)
        Assert-True (Test-Path (Join-Path $root 'ncmm\boot.pending')) 'Diagnostics mutated boot.pending.'
        Assert-True (-not (Test-Path (Join-Path $root 'child.log'))) 'Diagnostics unexpectedly launched a child.'
        Assert-True ([bool](Read-State $root).diagnostics_only) 'Diagnostics state flag was not persisted.'
    }

    Assert-Equal $passed $total 'Harness scenario count mismatch.'
    Write-Host "NCMM Bootstrap Automated Failure Harness: PASS ($passed/$total scenarios)." -ForegroundColor Green
} finally {
    Clear-ReadOnlyTree $work
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}
