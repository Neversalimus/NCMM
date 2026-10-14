[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$GameRoot)
$ErrorActionPreference='Stop'

# Native Play Now asks for confirmation when a same-seed avatar already has a
# save. Every measured process needs a fresh fixture world, while its ncmm
# receipts and captured images must survive for the paired verification.
if([string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)){throw 'The smoke reset requires a CI temporary directory.'}
$temporary=[IO.Path]::GetFullPath($env:RUNNER_TEMP).TrimEnd([char[]]'\/')+[IO.Path]::DirectorySeparatorChar
$game=[IO.Path]::GetFullPath($GameRoot).TrimEnd([char[]]'\/')
if(-not $game.StartsWith($temporary,[StringComparison]::OrdinalIgnoreCase)){throw 'The smoke reset accepts only a game below RUNNER_TEMP.'}
$save=Join-Path $game 'save'
$world=Join-Path $save 'NCMM Gameplay Smoke'
foreach($path in @($game,$save,$world)) {
    if(Test-Path -LiteralPath $path) {
        $entry=Get-Item -LiteralPath $path -Force
        if(-not $entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'The fixture save path must be an ordinary directory.'}
    }
}
if(Test-Path -LiteralPath $world){Remove-Item -LiteralPath $world -Recurse -Force}
if(Test-Path -LiteralPath $world){throw 'The fixture world could not be reset.'}
Write-Host 'Fresh native fixture world: NCMM Gameplay Smoke'
