# Route exports (ai-route-exports.jsx) in one fresh Illustrator. Launch on the isolated hidden
# desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [Parameter(Mandatory=$true)][string]$Font,
    [string]$ExpectedBuild = 'region-prep-23'
)
$ErrorActionPreference = 'Stop'
$json = @{font=$Font; expectedBuild=$ExpectedBuild} | ConvertTo-Json -Compress
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-route-exports.jsx -ArgJson $json
if ($LASTEXITCODE) { exit $LASTEXITCODE }
