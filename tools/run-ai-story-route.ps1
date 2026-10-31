# Story route color and paint-model sweep (ai-story-route.jsx) in one fresh Illustrator.
# Launch on the isolated hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [Parameter(Mandatory=$true)][string]$Font,
    [string[]]$Cases = @('rgb','cmyk','spot','mixfill','uniform','mixboth','perframe'),
    [switch]$ViaCorrect,
    [string]$ExpectedBuild = 'region-prep-22',
    [switch]$Fix
)
$ErrorActionPreference = 'Stop'
$Cases = @($Cases | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$json = @{font=$Font; cases=@($Cases); expectedBuild=$ExpectedBuild; viaCorrect=[bool]$ViaCorrect; fix=[bool]$Fix} | ConvertTo-Json -Compress
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-story-route.jsx -ArgJson $json
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$result = Get-Content -LiteralPath (Join-Path $OutDir ($RunId + '-host.txt')) -Raw
if ($result -notmatch '\|?DONE\|') { throw ('Story route sweep failed: ' + $result) }
