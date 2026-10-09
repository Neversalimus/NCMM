param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepositoryRoot).Path
$sync=Join-Path $root 'ci\Sync-CanonicalPayload.ps1'
$relativePayload='payload\SURVIVOR_0911_0915_v8.7.6.8.ps1'
$payload=Join-Path $root $relativePayload
$temp=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-canonical-fixture-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -Force | Out-Null
try {
    $fixturePayload=Join-Path $temp $relativePayload
    New-Item -ItemType Directory -Path (Split-Path $fixturePayload -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $payload -Destination $fixturePayload -Force
    $text=[IO.File]::ReadAllText($payload,[Text.Encoding]::UTF8)
    $embedded=@(
        [regex]::Matches($text,"Write-NcmmCanonicalPayloadFile '([^']+)' '") |
          ForEach-Object { $_.Groups[1].Value }
    )
    if($embedded.Count -ne 25) { throw "Unexpected fixture inventory: $($embedded.Count) snapshots" }
    foreach($rel in $embedded) {
        if([IO.Path]::IsPathRooted($rel) -or $rel.Contains('..')) {
            throw "Unsafe fixture path: $rel"
        }
        $source=Join-Path $root $rel
        $destination=Join-Path $temp $rel
        New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }

    # The pristine fixture must agree with every checked-in source.
    & $sync -RepositoryRoot $temp -Check | Out-Null

    # Line ending differences are not payload drift.
    $target=Join-Path $temp 'runtime\NCMMBootstrap.cs'
    $original=[IO.File]::ReadAllText($target,[Text.Encoding]::UTF8)
    $crlf=$original.Replace("`r`n","`n").Replace("`r","`n").Replace("`n","`r`n")
    [IO.File]::WriteAllText($target,$crlf,(New-Object Text.UTF8Encoding($false)))
    & $sync -RepositoryRoot $temp -Check | Out-Null

    # Real source edits must fail -Check and be repaired by explicit sync.
    [IO.File]::AppendAllText($target,"`r`n// Canonical regression fixture`r`n")
    $rejected=$false
    try { & $sync -RepositoryRoot $temp -Check | Out-Null } catch { $rejected=$true }
    if(-not $rejected) { throw 'Canonical snapshot drift was accepted by -Check.' }
    $before=[IO.File]::ReadAllText($fixturePayload,[Text.Encoding]::UTF8)
    & $sync -RepositoryRoot $temp | Out-Null
    $after=[IO.File]::ReadAllText($fixturePayload,[Text.Encoding]::UTF8)
    if($before -ceq $after) { throw 'Canonical synchronization failed to change stale payload.' }
    & $sync -RepositoryRoot $temp -Check | Out-Null

    # Losing the BOM breaks ParseFile on Windows PowerShell 5.1 with Russian text.
    [IO.File]::WriteAllText($fixturePayload,$after,(New-Object Text.UTF8Encoding($false)))
    $rejected=$false
    try { & $sync -RepositoryRoot $temp -Check | Out-Null } catch { $rejected=$true }
    if(-not $rejected) { throw 'Missing payload UTF-8 BOM was accepted.' }
    & $sync -RepositoryRoot $temp | Out-Null
    & $sync -RepositoryRoot $temp -Check | Out-Null
    $tokens=$null; $errors=$null
    [void][System.Management.Automation.Language.Parser]::ParseFile($fixturePayload,[ref]$tokens,[ref]$errors)
    if($errors.Count) { throw 'Synchronized payload is not valid Windows PowerShell.' }

    # Unlisted entries and duplicate entries must both fail closed.
    foreach($extra in @(
        "Write-NcmmCanonicalPayloadFile 'unexpected\\fixture.txt' 'YQ=='",
        "Write-NcmmCanonicalPayloadFile 'runtime\NCMMBootstrap.cs' 'YQ=='"
    )) {
        [IO.File]::WriteAllText($fixturePayload,$after+"`n"+$extra+"`n",
                              (New-Object Text.UTF8Encoding($true)))
        $rejected=$false
        try { & $sync -RepositoryRoot $temp -Check | Out-Null } catch { $rejected=$true }
        if(-not $rejected) { throw "Unexpected/duplicate canonical entry was accepted: $extra" }
    }

    Write-Host 'Canonical snapshot synchronization regression: PASS (25 sources, CRLF, drift, repair, unknown, duplicate).' -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
