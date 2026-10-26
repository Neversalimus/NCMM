param([string]$Builder=(Join-Path $PSScriptRoot '../Build-FirstPersonInstaller.ps1'),
    [string]$OutputRoot=(Join-Path ([IO.Path]::GetTempPath()) ('ncmm-world-package-'+[Guid]::NewGuid().ToString('N'))))
$ErrorActionPreference='Stop'
$builderText=Get-Content -LiteralPath $Builder -Raw -Encoding UTF8
$begin=$builderText.IndexOf('$worldRoot=')
$end=$builderText.IndexOf('# End offline world resource packaging.',$begin)
if($begin -lt 0 -or $end -le $begin){throw 'Actual world packaging statements are missing.'}
$fp=[IO.Path]::GetFullPath($OutputRoot)
if(Test-Path -LiteralPath $fp){throw 'Use a fresh packaging directory.'}
New-Item -ItemType Directory -Path $fp | Out-Null
$builderDirectory=Split-Path -Parent (Resolve-Path -LiteralPath $Builder).Path
$statements=$builderText.Substring($begin,$end-$begin).Replace('$PSScriptRoot','$builderDirectory')
$source=[pscustomobject]@{source_commit='3f7fb352bf492ba521bd9408a0c9f6ce239e8d83'}
$nativeReceipt=$source
. ([scriptblock]::Create($statements))
if(-not [object]::ReferenceEquals($source,$nativeReceipt)){throw 'World packaging changed the Native receipt.'}
$runtime=Get-Content -LiteralPath (Join-Path $fp 'world-assets.manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($runtime.runtime_importer -ne $false -or $runtime.imported.models.Count -ne 43 -or $runtime.selection.models.Count -ne 43 -or
    $runtime.source.license -ne 'CC0-1.0' -or @(Get-ChildItem -LiteralPath $fp -File).Count -ne 3){throw 'Packaged world source inventory differs.'}
if((Get-Content -LiteralPath (Join-Path $fp 'world-assets-license.txt') -Raw) -notmatch 'Creative Commons Zero'){throw 'Author license missing.'}
$effects=Get-Content -LiteralPath (Join-Path $fp 'weapon-effects.references.json') -Raw | ConvertFrom-Json
if($effects.schema -ne 1 -or $effects.references.Count -ne 2){throw 'Effect reference inventory missing.'}
[ordered]@{success=$true;powershell=$PSVersionTable.PSVersion.ToString();world_models=43;native_receipt_preserved=$true;output=$fp} | ConvertTo-Json -Compress
