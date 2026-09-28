param(
    [string]$PackageRoot=(Split-Path $PSScriptRoot -Parent),
    [switch]$Check
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $PackageRoot).Path
$membershipPath=Join-Path $root 'compat\package-files.txt'
$manifestPath=Join-Path $root 'compat\package.integrity.json'

if(-not(Test-Path $membershipPath -PathType Leaf)){
    throw 'Package membership file is missing: compat\package-files.txt'
}

$members=@(
    Get-Content $membershipPath |
    ForEach-Object { ([string]$_).Trim() } |
    Where-Object { $_ -and -not $_.StartsWith('#') }
)
if($members.Count -eq 0){throw 'Package membership is empty.'}
if(@($members|Select-Object -Unique).Count -ne $members.Count){
    throw 'Package membership contains duplicate paths.'
}

$entries=New-Object System.Collections.Generic.List[object]
foreach($rel in @($members|Sort-Object)){
    if($rel -eq 'compat\package.integrity.json'){
        throw 'Package integrity manifest must not include itself.'
    }
    if([IO.Path]::IsPathRooted($rel) -or $rel.Contains('..')){
        throw "Unsafe package membership path: $rel"
    }
    $file=Join-Path $root ($rel.Replace('\',[IO.Path]::DirectorySeparatorChar))
    if(-not(Test-Path -LiteralPath $file -PathType Leaf)){
        throw "Package membership file is missing: $rel"
    }
    $entries.Add([pscustomobject]@{
        path=$rel
        sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes=[int64](Get-Item -LiteralPath $file).Length
    })
}

$obj=[ordered]@{
    schema=1
    infrastructure='0.8.3.1'
    algorithm='sha256'
    policy='explicit immutable package membership from compat/package-files.txt; integrity manifest excludes itself'
    files=@($entries|ForEach-Object{$_})
}
$expected=(($obj|ConvertTo-Json -Depth 8)+"`n")

if($Check){
    if(-not(Test-Path $manifestPath -PathType Leaf)){throw 'Package integrity manifest is missing.'}
    $actual=[IO.File]::ReadAllText($manifestPath).Replace("`r`n","`n")
    if($actual -ne $expected){
        throw 'Package integrity manifest is stale. Run ci\Regenerate-PackageIntegrity.ps1 and commit the result with the source change.'
    }
    Write-Host ('Package integrity is current: '+$entries.Count+' files') -ForegroundColor Green
    return
}

[IO.File]::WriteAllText($manifestPath,$expected,(New-Object Text.UTF8Encoding($false)))
Write-Host ('Regenerated package integrity: '+$manifestPath+' ('+$entries.Count+' files)') -ForegroundColor Green
