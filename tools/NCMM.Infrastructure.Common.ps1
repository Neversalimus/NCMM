$ErrorActionPreference = 'Stop'

foreach($module in @(
    'NCMM.Infrastructure.Core.ps1',
    'NCMM.Infrastructure.Package.ps1',
    'NCMM.Infrastructure.Transaction.ps1'
)){
    $path=Join-Path $PSScriptRoot $module
    if(-not(Test-Path $path -PathType Leaf)){throw "NCMM Infrastructure module missing: $path"}
    . $path
}
