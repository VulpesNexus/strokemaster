# The owner's retests of region-prep-29 (ai-owner-retests.jsx, then ai-owner-retests-align.jsx and
# ai-owner-retests-final.jsx, each after idle time) in one fresh Illustrator. Launch on the
# isolated hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [ValidateRange(2, 60)][int]$IdleSeconds = 5,
    [string]$ExpectedBuild = ''
)
$ErrorActionPreference = 'Stop'
# Never the user's switches: Illustrator inherits STROKEMASTER_SETTINGS (IllustratorSettings.h).
$env:STROKEMASTER_SETTINGS = Join-Path (Resolve-Path -LiteralPath $OutDir).ProviderPath ($RunId + '-settings.txt')
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-owner-retests.jsx `
    -ArgJson (@{expectedBuild = $ExpectedBuild} | ConvertTo-Json -Compress) `
    -Script2 @('ai-owner-retests-align.jsx', 'ai-owner-retests-final.jsx') -IdleSeconds $IdleSeconds
if ($LASTEXITCODE) { exit $LASTEXITCODE }
