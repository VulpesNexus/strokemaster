# Separate open/read phases for an owned saved AI fixture.
param([string]$OutDir,[string]$RunId,[ValidatePattern('^[A-Za-z0-9_-]+\.ai$')][string]$File)
$ErrorActionPreference='Stop'
$json=@{inspectFile=$File} | ConvertTo-Json -Compress
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-text-parity-open.jsx -Script2 ai-story-readback.jsx -IdleSeconds 4 -ArgJson $json
if($LASTEXITCODE){exit $LASTEXITCODE}
$hostResult=Get-Content -LiteralPath (Join-Path $OutDir ($RunId+'-host.txt')) -Raw
if($hostResult -match '\|FAIL\|'){throw 'Owned AI inspection failed.'}
