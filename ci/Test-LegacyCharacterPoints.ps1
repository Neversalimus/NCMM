param(
    [string]$RepositoryRoot=(Split-Path $PSScriptRoot -Parent),
    [string]$FixtureRoot=''
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $RepositoryRoot).Path
$apply=Join-Path $root 'host_patch\Apply-LegacyCharacterPoints.ps1'
$utf8=New-Object Text.UTF8Encoding($false,$true)
$spec=ConvertFrom-Json ([IO.File]::ReadAllText((Join-Path $root 'host_patch\legacy-character-points.patch.json'),$utf8))
if(@($spec.operations).Count -ne 26){throw 'Legacy points must retain all 26 engine contracts.'}
$names=@($spec.operations | ForEach-Object {[string]$_.name})
if(@($names|Select-Object -Unique).Count -ne $names.Count){throw 'Duplicate legacy point operation name.'}
foreach($required in @('transfer safe selection','authoritative final gate','saved template mode',
    'budget confirmation replaces ratings','always visible budget','randomization actor boundary',
    'real engine accounting test entrypoint')) {
    if($names -notcontains $required){throw "Missing legacy points boundary: $required"}
}
$temp=Join-Path ([IO.Path]::GetTempPath()) ('ncmm-lcp-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $temp | Out-Null
$refs=@{
    '0546'='e262adb299a7613b4aedc5f12c08fe0413c56a84'
    '1040'='3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'
}
$files=@('src/newcharacter.cpp','src/player_difficulty.h')
function Fingerprint([string]$directory) {
    return (@(Get-ChildItem -LiteralPath $directory -Recurse -File | Sort-Object FullName |
        ForEach-Object {$_.FullName.Substring($directory.Length)+':'+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}) -join "`n")
}
function New-Fixture([string]$directory,$sources,[bool]$crlf) {
    New-Item -ItemType Directory -Force (Join-Path $directory 'src') | Out-Null
    foreach($file in $files) {
        $text=[string]$sources[$file]
        if($crlf){$text=$text.Replace("`n","`r`n")}
        [IO.File]::WriteAllText((Join-Path $directory $file),$text,$utf8)
    }
}
$cases=0
try {
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    foreach($tag in @('0546','1040')) {
        $sources=@{}
        foreach($file in $files) {
            if($FixtureRoot) {
                $sources[$file]=[IO.File]::ReadAllText((Join-Path $FixtureRoot ($tag+'/'+$file)),$utf8)
            } else {
                $url='https://raw.githubusercontent.com/CleverRaven/Cataclysm-DDA/'+$refs[$tag]+'/'+$file
                $sources[$file]=[string](Invoke-WebRequest -UseBasicParsing -Uri $url).Content
            }
            $sources[$file]=$sources[$file].Replace("`r`n","`n").Replace("`r","`n")
        }
        foreach($crlf in @($false,$true)) {
            $case=Join-Path $temp ($tag+'-'+$crlf)
            New-Fixture $case $sources $crlf
            & $apply -SourceRoot $case
            $first=Fingerprint $case
            & $apply -SourceRoot $case
            if((Fingerprint $case) -cne $first){throw "Repeated $tag transform changed bytes."}
            $transformed=[IO.File]::ReadAllText((Join-Path $case 'src/newcharacter.cpp'),$utf8)
            if($transformed.IndexOf('if( pool == pool_type::TRANSFER )') -ge $transformed.IndexOf('ncmm::legacy_chargen::begin( pool')){throw 'Transfer bypass was moved after the budget selector.'}
            if($transformed.IndexOf('ncmm::legacy_chargen::valid( *this, true )') -ge $transformed.IndexOf('save_template( _( "Last Character" )')){throw 'Final gate was moved after initialization/persistence.'}
            $cases++
        }
        # Missing anchor must leave every input byte untouched, including the other source file.
        $case=Join-Path $temp ($tag+'-bad-anchor')
        $bad=@{};foreach($file in $files){$bad[$file]=$sources[$file]}
        $bad['src/newcharacter.cpp']=$bad['src/newcharacter.cpp'].Replace('pool_type pool = pool_type::FREEFORM;', 'pool_type pool = (pool_type::FREEFORM);')
        New-Fixture $case $bad $false
        $before=Fingerprint $case;$rejected=$false
        try { & $apply -SourceRoot $case | Out-Null } catch {$rejected=$true}
        if(-not $rejected -or (Fingerprint $case) -cne $before){throw 'Missing-anchor preflight mutated the engine or did not reject.'}
        $cases++
        # An include marker alone is not evidence that the remaining checks exist.
        $case=Join-Path $temp ($tag+'-partial')
        $bad['src/newcharacter.cpp']=$sources['src/newcharacter.cpp']+"`n#include `"ncmm_legacy_chargen.h`"`n"
        New-Fixture $case $bad $false
        $before=Fingerprint $case;$rejected=$false
        try { & $apply -SourceRoot $case | Out-Null } catch {$rejected=$true}
        if(-not $rejected -or (Fingerprint $case) -cne $before){throw 'Partially applied point patch was accepted or mutated.'}
        $cases++
        # Each altered postcondition is rejected, not silently replaced on the second pass.
        foreach($op in $spec.operations) {
            $case=Join-Path $temp ($tag+'-postcondition-'+$cases)
            New-Fixture $case $sources $false
            & $apply -SourceRoot $case | Out-Null
            $path=Join-Path $case ([string]$op.file)
            $text=[IO.File]::ReadAllText($path,$utf8)
            $at=$text.IndexOf([string]$op.after,[StringComparison]::Ordinal)
            if($at -lt 0){throw 'Postcondition fixture setup failed.'}
            $text=$text.Substring(0,$at)+'/* altered */'+$text.Substring($at+1)
            [IO.File]::WriteAllText($path,$text,$utf8)
            $before=Fingerprint $case;$rejected=$false
            try { & $apply -SourceRoot $case | Out-Null } catch {$rejected=$true}
            if(-not $rejected -or (Fingerprint $case) -cne $before){throw "Corrupted postcondition accepted/mutated: $($op.name)"}
            $cases++
        }
    }
    Write-Host "Legacy Character Points transforms: PASS ($cases cases, 0546/1040, LF/CRLF/idempotence, every postcondition)." -ForegroundColor Green
} finally {
    Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
}
