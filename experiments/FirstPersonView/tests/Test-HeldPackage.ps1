param([string]$Builder=(Join-Path $PSScriptRoot '../Build-FirstPersonInstaller.ps1'),
    [string]$OutputRoot=(Join-Path ([IO.Path]::GetTempPath()) ('ncmm-held-package-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$builderText=Get-Content -LiteralPath $Builder -Raw -Encoding UTF8
$begin=$builderText.IndexOf('$heldSelectionPath=')
$end=$builderText.IndexOf('$hostDest=',$begin)
if($begin -lt 0 -or $end -le $begin){throw 'Actual resource packaging statements are missing.'}
$heldRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../assets/held_items')).Path
$fp=[IO.Path]::GetFullPath($OutputRoot)
if(Test-Path -LiteralPath $fp){throw 'Use a new owned packaging test directory.'}
New-Item -ItemType Directory -Path $fp | Out-Null
$source=[pscustomobject]@{source_commit='3f7fb352bf492ba521bd9408a0c9f6ce239e8d83';tag='cdda-experimental-2026-10-01-1040';status='experimental-not-certified'}
$nativeReceipt=$source
# Run in the builder's actual scope. A child scriptblock conceals the original
# foreach($source ...) collision and cannot exercise this regression.
. ([scriptblock]::Create($builderText.Substring($begin,$end-$begin)))
if(-not [object]::ReferenceEquals($source,$nativeReceipt) -or $source.source_commit -ne '3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'){
    throw 'Resource packaging overwrote the native engine receipt.'
}
$runtimePath=Join-Path $fp 'held-assets.manifest.json'
$runtimeText=Get-Content -LiteralPath $runtimePath -Raw -Encoding UTF8
$suffix=',"selection":'+$heldSelectionText.Trim()+',"imported":'+$heldImportText.Trim()+'}'
if(-not $runtimeText.EndsWith($suffix,[StringComparison]::Ordinal)){throw 'Author inventory or unsigned mesh identity tokens changed.'}
$runtime=$runtimeText | ConvertFrom-Json
if($runtime.schema -ne 1 -or $runtime.runtime_importer -ne $false -or
    $runtime.selection_sha256 -ne (Get-FileHash -LiteralPath $heldSelectionPath -Algorithm SHA256).Hash.ToLowerInvariant() -or
    $runtime.generated_manifest_sha256 -ne (Get-FileHash -LiteralPath $heldImportPath -Algorithm SHA256).Hash.ToLowerInvariant()){
    throw 'Packaged held source identity differs.'
}
$licenses=Get-Content -LiteralPath (Join-Path $fp 'held-assets-licenses.txt') -Raw -Encoding UTF8
if($licenses -cne ($heldLicenseParts -join "`n") -or $heldLicenseParts.Count -ne 9 -or @(Get-ChildItem -LiteralPath $fp -File).Count -ne 2){
    throw 'Packaged source license inventory differs.'
}
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();native_receipt_preserved=$true;unsigned_identity_tokens_preserved=$true;source_licenses=$heldLicenseParts.Count;output=$fp} | ConvertTo-Json -Compress
