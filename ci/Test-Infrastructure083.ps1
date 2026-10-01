param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
. (Join-Path $PackageRoot 'tools\NCMM.Infrastructure.Common.ps1')
. (Join-Path $PackageRoot 'tools\NCMM.Update.Common.ps1')
$required=@('NCMM.cmd','NCMM.ps1','internal\NCMM.Install.ps1','internal\stages\10-Preflight.ps1','internal\stages\20-Snapshot.ps1','internal\stages\30-PayloadEngine.ps1','internal\stages\40-OfflineVerify.ps1','internal\stages\50-Commit.ps1','internal\stages\60-RuntimeVerify.ps1','pipeline\pipeline.manifest.json','compat\compatibility.manifest.json','compat\contracts.json','compat\package.integrity.json','compat\package-files.txt','compat\feed\index.json','compat\update\index.json','components\index.json','components\package-format.json','migrations\migrations.json','tools\NCMM.Infrastructure.Common.ps1','tools\NCMM.Infrastructure.Core.ps1','tools\NCMM.Infrastructure.Package.ps1','tools\NCMM.Infrastructure.Transaction.ps1','tools\NCMM.Update.Common.ps1','tools\Invoke-NCMMUpdate.ps1','tools\Collect-NCMMDiagnostics.ps1','ci\Invoke-NCMMCertification.ps1','ci\Watch-CDDAExperimental.ps1','ci\Test-GoldenRegression.ps1','ci\Test-ComponentCatalog.ps1','ci\Test-HostPayloadContracts.ps1','ci\Test-InstallerTransaction.ps1','ci\Test-SurvivorPayloadContracts.ps1','ci\Regenerate-PackageIntegrity.ps1','ci\Build-UpdateRelease.ps1','ci\Promote-UpdateRelease.ps1','adapters\base\cdda_2026_series.ps1','adapters\cdda_2026_09_23_0546.ps1','payload\SURVIVOR_0911_0915_v8.7.6.8.ps1','.github\workflows\ncmm-certify.yml','.github\workflows\ncmm-experimental-watch.yml')
foreach($r in $required){if(-not(Test-Path (Join-Path $PackageRoot $r) -PathType Leaf)){throw "Missing Infrastructure 0.8.3.1 file: $r"}}
if(-not(Assert-NcmmPackageIntegrity $PackageRoot)){throw 'Production package integrity verification did not return success.'}
$m=Get-Content (Join-Path $PackageRoot 'compat\compatibility.manifest.json') -Raw|ConvertFrom-Json
$pipeline083=Get-Content (Join-Path $PackageRoot 'pipeline\pipeline.manifest.json') -Raw|ConvertFrom-Json
if([string]$pipeline083.infrastructure -ne '0.8.3.1' -or [string]$pipeline083.transaction_gate -ne 'offline_install_verification'){throw 'Staged pipeline manifest invalid.'}
if([int]$m.schema -ne 4 -or [string]$m.infrastructure_version -ne '0.8.3.1'){throw 'Compatibility manifest schema/version mismatch.'}
# Certification must accept the same infrastructure version declared by the package.
# Extract the actual guard pattern so an accidental PowerShell over-escape is caught here.
$certificationSource=[IO.File]::ReadAllText((Join-Path $PackageRoot 'ci\Invoke-NCMMCertification.ps1'))
$guardPrefix='$infrastructureVersion -notmatch '''
$guardStart=$certificationSource.IndexOf($guardPrefix,[StringComparison]::Ordinal)
if($guardStart -lt 0){throw 'Certification infrastructure-version guard missing.'}
$patternStart=$guardStart+$guardPrefix.Length
$patternEnd=$certificationSource.IndexOf("'",$patternStart,[StringComparison]::Ordinal)
if($patternEnd -le $patternStart){throw 'Certification infrastructure-version pattern missing.'}
$certificationVersionPattern=$certificationSource.Substring($patternStart,$patternEnd-$patternStart)
if(([string]$m.infrastructure_version) -notmatch $certificationVersionPattern){
    throw ('Certification infrastructure-version guard rejects current version: '+[string]$m.infrastructure_version)
}
$exact=& (Join-Path $PackageRoot 'adapters\cdda_2026_09_23_0546.ps1') -Mode Describe
if([string]$exact.commit -ne 'e262adb299a7613b4aedc5f12c08fe0413c56a84' -or [string]$exact.support -ne 'exact'){throw 'Exact 0546 adapter identity drift.'}
$base=Get-NcmmBaseAdapter $PackageRoot;if(-not $base -or [string]$base.id -ne 'base-cdda-2026-series-v1'){throw 'Inherited base adapter resolution failed.'}
& (Join-Path $PackageRoot 'ci\Test-ComponentCatalog.ps1') -PackageRoot $PackageRoot
$r=Get-Content (Join-Path $PackageRoot 'migrations\migrations.json') -Raw|ConvertFrom-Json;if([int]$r.update_state_schema -ne 2){throw 'Migration registry invalid.'}
# Parse every PowerShell file with the actual Windows PowerShell parser.
$parseFailures=New-Object System.Collections.Generic.List[string]
foreach($ps in Get-ChildItem $PackageRoot -Recurse -File -Filter '*.ps1'){
    $tokens=$null;$errors=$null
    [void][System.Management.Automation.Language.Parser]::ParseFile($ps.FullName,[ref]$tokens,[ref]$errors)
    if($errors.Count){
        $details=@($errors|ForEach-Object{
            ('line {0}, col {1}: {2} | {3}' -f $_.Extent.StartLineNumber,$_.Extent.StartColumnNumber,$_.Message,$_.Extent.Text)
        })
        $parseFailures.Add($ps.FullName+': '+($details -join ' || '))
    }
}
if($parseFailures.Count){throw ('PowerShell parse failures: '+($parseFailures -join '; '))}
# PS5.1 binder regression: do not wrap generic List/HashSet variables directly in @().
# That pattern caused "Argument types do not match" in Resolve-NcmmDependencyPlan.
foreach($ps in Get-ChildItem $PackageRoot -Recurse -File -Filter '*.ps1'){
    if($ps.FullName -like '*\payload\*'){continue}
    $source=[IO.File]::ReadAllText($ps.FullName)
    $genericVars=@()
    foreach($sourceLine in ($source -split "`r?`n")){
        if($sourceLine -match '^\s*\$(?<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*New-Object .*System\.Collections\.Generic\.(?:List|HashSet)'){
            $genericVars += [string]$Matches['name']
        }
    }
    $genericVars=@($genericVars|Select-Object -Unique)
    foreach($genericVar in $genericVars){
        if($source.Contains('@($'+$genericVar+')')){
            throw ('PS5.1 unsafe direct generic collection array conversion: '+$ps.FullName+' -> @($'+$genericVar+')')
        }
    }
}

& (Join-Path $PackageRoot 'ci\Test-HostPayloadContracts.ps1') -PackageRoot $PackageRoot
& (Join-Path $PackageRoot 'ci\Test-SurvivorPayloadContracts.ps1') -PackageRoot $PackageRoot

Write-Host 'NCMM Infrastructure 0.8.3.1 static contract: PASS' -ForegroundColor Green
exit 0
