param([string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$helper=Join-Path $RepositoryRoot 'ci\Get-ImmutableReleaseTag.ps1'
if(-not(Test-Path -LiteralPath $helper -PathType Leaf)){throw 'Immutable release tag helper missing.'}
$dir=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-tag-test-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir -Force|Out-Null
try {
    $a=Join-Path $dir 'NCMM_Full_v0.8.2.zip'
    $b=Join-Path $dir 'NCMM_Runtime_v0.8.2.zip'
    $source='0123456789abcdef0123456789abcdef01234567'
    [IO.File]::WriteAllBytes($a,[byte[]](1,2,3,4))
    [IO.File]::WriteAllBytes($b,[byte[]](7,8,9))
    $t1=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha $source -Asset @($a,$b)
    $t2=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha $source -Asset @($b,$a)
    if($t1 -cne $t2 -or $t1 -cnotmatch '^ncmm-runtime-v0\.8\.2-build-0123456789ab-[0-9a-f]{16}$'){
        throw 'Immutable release identity is not stable across identical asset sets.'
    }
    [IO.File]::WriteAllBytes($b,[byte[]](7,8,10))
    $t3=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha $source -Asset @($a,$b)
    if($t3 -ceq $t1){throw 'Immutable release identity did not change after asset bytes changed.'}
    $source2='fedcba9876543210fedcba9876543210fedcba98'
    $t4=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha $source2 -Asset @($a,$b)
    if($t4 -ceq $t3){throw 'Immutable release identity did not bind the source commit.'}
    $rejected=$false
    try{$null=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha $source -Asset @($a,$a)}catch{$rejected=$true}
    if(-not $rejected){throw 'Duplicate immutable asset names were accepted.'}
    $rejected=$false
    try{$null=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha $source -Asset @($a,(Join-Path $dir 'missing.zip'))}catch{$rejected=$true}
    if(-not $rejected){throw 'Missing immutable asset was accepted.'}
    $rejected=$false
    try{$null=& $helper -Prefix 'ncmm-runtime' -Version '0.8.2' -SourceSha 'not-a-sha' -Asset @($a)}catch{$rejected=$true}
    if(-not $rejected){throw 'Unqualified source identity was accepted.'}
    Write-Host 'Immutable release tag identity regression: PASS (order, bytes, SHA, collisions, missing assets).' -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
}
