param([ValidateSet('Describe','BeforePayload','AfterPayload')][string]$Mode='Describe',[string]$ContextJson='')
$ErrorActionPreference='Stop'
$packageRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
switch($Mode){
'Describe'{
    [pscustomobject]@{
        schema=2
        id='base-cdda-2026-series-v1'
        source_family='cdda-2026'
        payload=(Join-Path $packageRoot 'payload\SURVIVOR_0911_0915_v8.7.6.8.ps1')
        contract_registry=(Join-Path $packageRoot 'compat\contracts.json')
        transform_generation='v8.7.6.8'
        structural_reuse=$true
        inherited_hooks='no-op'
    }
}
'BeforePayload'{return}
'AfterPayload'{return}
}
