# A probe (default ai-owner-file-inspect.jsx) on a copy of a document the owner saved, in one
# fresh Illustrator, with further phases after idle time. Pass a copy, never the owner's own file:
# the probe may correct it. Without -File, any probe with -Extra's arguments: a JSON object, or
# @<path> of a file holding one (quotes do not survive the hidden desktop's command line).
# Launch on the isolated hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory=$true)][string]$OutDir,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId,
    [string]$File = '',
    [ValidatePattern('^ai-[A-Za-z0-9_-]+\.jsx$')][string]$Script = 'ai-owner-file-inspect.jsx',
    [ValidatePattern('^ai-[A-Za-z0-9_-]+\.jsx$')][string[]]$Script2 = @(),
    [ValidateRange(2, 60)][int]$IdleSeconds = 5,
    [string]$Extra = '{}',
    [string]$ExpectedBuild = ''
)
$ErrorActionPreference = 'Stop'
# Never the user's switches: Illustrator inherits STROKEMASTER_SETTINGS (IllustratorSettings.h).
$env:STROKEMASTER_SETTINGS = Join-Path (Resolve-Path -LiteralPath $OutDir).ProviderPath ($RunId + '-settings.txt')
if ($Extra.StartsWith('@')) { $Extra = Get-Content -Raw -LiteralPath $Extra.Substring(1) }
$args2 = ConvertFrom-Json $Extra
$table = @{}
if ($File) { $table.file = ((Resolve-Path -LiteralPath $File).ProviderPath -replace '\\', '/') }
foreach ($p in $args2.PSObject.Properties) { $table[$p.Name] = $p.Value }
if ($ExpectedBuild) { $table.expectedBuild = $ExpectedBuild }
& (Join-Path $PSScriptRoot 'run-ai-fresh.ps1') -OutDir $OutDir -RunId $RunId -Script $Script `
    -ArgJson ($table | ConvertTo-Json -Compress) -Script2 $Script2 -IdleSeconds $IdleSeconds
if ($LASTEXITCODE) { exit $LASTEXITCODE }
