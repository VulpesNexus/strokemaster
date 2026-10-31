# Three separate COM calls with idle periods, so the real plugin timer runs.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId
)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-threaded-refix-start.jsx -Script2 @('ai-threaded-refix-edit.jsx','ai-threaded-refix-final.jsx') -IdleSeconds 4
if($LASTEXITCODE){exit $LASTEXITCODE}
$raw=Get-Content -LiteralPath (Join-Path $OutDir ($RunId+'-host.txt')) -Raw
if($raw -match 'FAIL|WRAPPER ERROR'){throw 'Threaded timer phase failed.'}
$record=Get-Content -LiteralPath (Join-Path $OutDir ($RunId+'-results.json')) -Raw | ConvertFrom-Json
if(-not $record.passed){throw 'Threaded timer result failed.'}
