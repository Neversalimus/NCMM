param([string]$PackageRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
Write-Host 'Compatibility shim: forwarding Infrastructure 0.8.2.1 test entrypoint to 0.8.3.1 staged contract.' -ForegroundColor Yellow
& (Join-Path $PSScriptRoot 'Test-Infrastructure083.ps1') -PackageRoot $PackageRoot
exit $LASTEXITCODE
