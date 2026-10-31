# Route pass reopen phases (ai-route-pass-reopen.jsx x3) in one fresh Illustrator: open
# <RunId>-pass.ai, export, edit, export after idle time. Launch on the isolated hidden desktop.
# Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [ValidateRange(2, 60)][int]$IdleSeconds = 5
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-route-pass-reopen.jsx `
    -Script2 @('ai-route-pass-reopen.jsx', 'ai-route-pass-reopen.jsx') -IdleSeconds $IdleSeconds
if ($LASTEXITCODE) { exit $LASTEXITCODE }
