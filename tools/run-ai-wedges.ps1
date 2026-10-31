# Wedged corners on a spiky font (ai-wedges.jsx) in one fresh Illustrator. Launch on the isolated
# hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [string]$Font = 'KerberosFang'
)
$ErrorActionPreference = 'Stop'
# Never the user's switches: Illustrator inherits STROKEMASTER_SETTINGS (IllustratorSettings.h).
$env:STROKEMASTER_SETTINGS = Join-Path (Resolve-Path -LiteralPath $OutDir).ProviderPath ($RunId + '-settings.txt')
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-wedges.jsx `
    -ArgJson (@{font = $Font} | ConvertTo-Json -Compress)
if ($LASTEXITCODE) { exit $LASTEXITCODE }
