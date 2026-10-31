# Route re-fix test (ai-route-refix-*.jsx): build and route, recolor, then read the timer's
# result after idle time, in one fresh Illustrator. Launch on the isolated hidden desktop.
# Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [Parameter(Mandatory=$true)][string]$Font,
    [ValidateRange(2, 60)][int]$IdleSeconds = 5,
    [string]$ExpectedBuild = 'region-prep-22'
)
$ErrorActionPreference = 'Stop'
$json = @{font=$Font; expectedBuild=$ExpectedBuild} | ConvertTo-Json -Compress
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-route-refix-start.jsx `
    -Script2 @('ai-route-refix-edit.jsx', 'ai-route-refix-final.jsx') -IdleSeconds $IdleSeconds -ArgJson $json
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$result = Get-Content -LiteralPath (Join-Path $OutDir ($RunId + '-host.txt')) -Raw
if ($result -match 'FAIL' -or $result -notmatch 'DONE\|phase3') { throw ('Route re-fix test failed: ' + $result) }
