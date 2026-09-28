function Invoke-NcmmInstallStageRuntimeVerify {
    param([string]$PackageRoot,[string]$GameRoot,[switch]$CreateBundle)
    Invoke-NcmmRuntimeVerification $PackageRoot $GameRoot -CreateBundle:$CreateBundle
}
