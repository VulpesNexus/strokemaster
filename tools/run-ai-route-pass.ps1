# Route pass test (ai-route-pass.jsx) in one fresh Illustrator. Launch on the isolated hidden
# desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [Parameter(Mandatory=$true)][string]$Font,
    [string]$Effect = '',
    [switch]$NoPass,
    [ValidateSet('append','whole','insert')][string]$EditMode = 'append'
)
$ErrorActionPreference = 'Stop'
$json = @{font=$Font; effect=$Effect; noPass=[bool]$NoPass; editMode=$EditMode} | ConvertTo-Json -Compress
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-route-pass.jsx -ArgJson $json
if ($LASTEXITCODE) { exit $LASTEXITCODE }
