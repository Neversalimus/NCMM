param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$root=(Resolve-Path $PackageRoot).Path
$entries=New-Object System.Collections.Generic.List[object]
foreach($f in @(Get-ChildItem $root -File -Recurse|Sort-Object FullName)){
    $rel=$f.FullName.Substring($root.Length).TrimStart('\').Replace('/','\')
    if($rel -eq 'compat\package.integrity.json'){continue}
    if($rel.StartsWith('adapters\generated\',[StringComparison]::OrdinalIgnoreCase)){continue}
    if($rel.StartsWith('certification-output\',[StringComparison]::OrdinalIgnoreCase)){continue}
    $entries.Add([pscustomobject]@{path=$rel;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant();bytes=[int64]$f.Length})
}
$obj=[ordered]@{schema=1;infrastructure='0.8.3.1';algorithm='sha256';policy='all immutable package files except this integrity manifest; generated adapters and certification-output excluded';files=@($entries | ForEach-Object { $_ })}
$path=Join-Path $root 'compat\package.integrity.json'
[IO.File]::WriteAllText($path,(($obj|ConvertTo-Json -Depth 8)+"`n"),(New-Object Text.UTF8Encoding($false)))
Write-Host ('Regenerated package integrity: '+$path+' ('+$entries.Count+' files)') -ForegroundColor Green