param(
    [Parameter(Mandatory=$true)][string]$Prefix,
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$true)][string]$SourceSha,
    [Parameter(Mandatory=$true)][string[]]$Asset
)
$ErrorActionPreference='Stop'
if($Prefix -cnotmatch '^[a-z][a-z0-9-]*$'){throw "Invalid immutable release prefix: $Prefix"}
if($Version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$'){throw "Invalid immutable release version: $Version"}
if($SourceSha -notmatch '^[0-9a-fA-F]{40}$'){throw 'Immutable release identity requires a full 40-character source SHA.'}
if($Asset.Count -eq 0){throw 'Immutable release identity requires at least one asset.'}

$records=@()
$names=@{}
foreach($path in $Asset){
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing immutable release asset: $path"}
    $name=[IO.Path]::GetFileName($path)
    if($name -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.-]*\.zip$'){
        throw "Unsafe immutable release asset filename: $name"
    }
    $key=$name.ToLowerInvariant()
    if($names.ContainsKey($key)){throw "Duplicate immutable release asset filename: $name"}
    $names[$key]=$true
    $digest=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $records+=($name+"`t"+$digest)
}
# Hash every published asset's name and exact bytes, not just the source commit:
# NCMM_Full includes workflow_run in its release manifest, so repeated builds vary.
$identity=(@($records | Sort-Object -CaseSensitive) -join "`n")+"`n"
$hash=[Security.Cryptography.SHA256]::Create()
try {
    $digestBytes=$hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($identity))
    $identityHash=(-join @($digestBytes | ForEach-Object {$_.ToString('x2')}))
} finally {
    $hash.Dispose()
}
# Identical asset sets are idempotent. Changed bytes create a new immutable tag.
Write-Output ($Prefix+'-v'+$Version+'-build-'+$SourceSha.Substring(0,12).ToLowerInvariant()+'-'+$identityHash.Substring(0,16))
