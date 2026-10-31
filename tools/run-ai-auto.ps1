# Automatic correction (ai-auto-start.jsx, then ai-auto-edit.jsx, ai-auto-read.jsx,
# ai-auto-move.jsx, and ai-auto-final.jsx, each after idle time) in one fresh Illustrator.
# Launch on the isolated hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [ValidateRange(2, 60)][int]$IdleSeconds = 5,
    [string]$ExpectedBuild = ''
)
$ErrorActionPreference = 'Stop'
# Never the user's switches: Illustrator inherits STROKEMASTER_SETTINGS (IllustratorSettings.h).
$env:STROKEMASTER_SETTINGS = Join-Path (Resolve-Path -LiteralPath $OutDir).ProviderPath ($RunId + '-settings.txt')
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-auto-start.jsx `
    -ArgJson (@{expectedBuild = $ExpectedBuild} | ConvertTo-Json -Compress) `
    -Script2 @('ai-auto-edit.jsx', 'ai-auto-read.jsx', 'ai-auto-move.jsx', 'ai-auto-final.jsx') -IdleSeconds $IdleSeconds
if ($LASTEXITCODE) { exit $LASTEXITCODE }
