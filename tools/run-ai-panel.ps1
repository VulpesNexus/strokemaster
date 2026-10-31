# Panel test (ai-panel-start.jsx, then ai-panel-final.jsx after idle time) in one fresh Illustrator.
# Launch on the isolated hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [ValidateRange(2, 60)][int]$IdleSeconds = 5
)
$ErrorActionPreference = 'Stop'
# Never the user's switches: Illustrator inherits STROKEMASTER_SETTINGS (IllustratorSettings.h).
$settings = Join-Path (Resolve-Path -LiteralPath $OutDir).ProviderPath ($RunId + '-settings.txt')
$env:STROKEMASTER_SETTINGS = $settings
$json = @{settingsPath = ($settings -replace '\\', '/')} | ConvertTo-Json -Compress
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script ai-panel-start.jsx -ArgJson $json `
    -Script2 @('ai-panel-final.jsx') -IdleSeconds $IdleSeconds
if ($LASTEXITCODE) { exit $LASTEXITCODE }
