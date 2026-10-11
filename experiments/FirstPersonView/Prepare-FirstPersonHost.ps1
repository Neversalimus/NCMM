param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [string]$RepositoryRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
)
$ErrorActionPreference='Stop'
$SourceRoot=(Resolve-Path $SourceRoot).Path
$RepositoryRoot=(Resolve-Path $RepositoryRoot).Path
$commit=(& git -C $SourceRoot rev-parse HEAD).Trim()
if($commit -notin @('3f7fb352bf492ba521bd9408a0c9f6ce239e8d83','074aa98bd5be3de4c35f154082db32a0e63bb0f1')){
    throw "Unqualified experimental source: $commit"
}
if(Test-Path (Join-Path $SourceRoot 'first-person-source.json')){
    throw 'Use a fresh disposable engine worktree; experimental patches are not reapplied.'
}
# Reuse the canonical importer and patch stack, without executing Build-HostPackage
# or producing certification metadata/feed/release assets for this experiment.
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $RepositoryRoot 'ci/Build-HostPackage.ps1'),[ref]$tokens,[ref]$errors)
if(@($errors).Count){throw 'Canonical Host build script has parse errors.'}
$definition=@($ast.FindAll({param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Import-NcmmPayloadFunctions'
},$true))
if($definition.Count -ne 1){throw 'Canonical payload importer is missing or ambiguous.'}
Invoke-Expression $definition[0].Extent.Text
& (Join-Path $RepositoryRoot 'host_patch/Apply-NCMMHostPatch.ps1') -SourceRoot $SourceRoot
$stack=Get-Content (Join-Path $RepositoryRoot 'ci/host-patch-stack.json') -Raw | ConvertFrom-Json
$script:NcmmRoot=$RepositoryRoot
Import-NcmmPayloadFunctions -PayloadPath (Join-Path $RepositoryRoot 'payload/SURVIVOR_0911_0915_v8.7.6.8.ps1') -Names (@($stack.helpers)+@($stack.layers))
foreach($layer in $stack.layers){ & $layer $SourceRoot }
& python (Join-Path $RepositoryRoot 'experiments/graphics_bridge/apply_graphics_bridge.py') $SourceRoot
if($LASTEXITCODE -ne 0){throw 'Experimental graphics bridge preflight/apply failed.'}
Write-Host 'Experimental source prepared. No certified Host or player installation was published.'
