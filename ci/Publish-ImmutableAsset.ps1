param([Parameter(Mandatory=$true)][string]$Tag,[Parameter(Mandatory=$true)][string[]]$Asset,
      [string]$Repository=$env:GITHUB_REPOSITORY)
$ErrorActionPreference='Stop'
$raw=& gh api "repos/$Repository/releases/tags/$Tag"
if($LASTEXITCODE -ne 0){throw "Cannot inspect release $Tag before uploading."}
$existing=@(($raw|ConvertFrom-Json).assets)
foreach($path in $Asset){
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing release asset: $path"}
    $name=[IO.Path]::GetFileName($path)
    $digest='sha256:'+(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $same=@($existing|Where-Object{$_.name -eq $name})
    if($same.Count -gt 1){throw "Duplicate release asset name: $name"}
    if($same.Count -eq 1){
        if([string]$same[0].digest -ne $digest){throw "Immutable asset conflict: $Tag/$name. Existing bytes are preserved; use a new source identity."}
        Write-Host "Immutable asset already matches: $Tag/$name";continue
    }
    & gh release upload $Tag $path --repo $Repository
    if($LASTEXITCODE -ne 0){throw "Upload failed (no overwrite attempted): $Tag/$name"}
    $raw=& gh api "repos/$Repository/releases/tags/$Tag"
    if($LASTEXITCODE -ne 0){throw "Cannot verify uploaded asset: $Tag/$name"}
    $remote=@(($raw|ConvertFrom-Json).assets|Where-Object{$_.name -eq $name})
    if($remote.Count -ne 1 -or [string]$remote[0].digest -ne $digest){throw "Published asset digest mismatch: $Tag/$name"}
}
