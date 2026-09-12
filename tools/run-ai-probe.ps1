# Attach-only Illustrator probe runner. Never launch a second Illustrator instance.
# Invoke with Windows PowerShell 5.1: powershell -NoProfile -File tools\run-ai-probe.ps1.
param([string]$Script = 'ai-probe-outline-dom.jsx', [string]$ArgJson = '{}', [string]$Output = '')
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop') { throw 'Run this file using Windows PowerShell 5.1 (powershell.exe).' }
$null = $ArgJson | ConvertFrom-Json
$target = Join-Path (Join-Path $PSScriptRoot 'jsx') $Script
if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw 'Probe script does not exist.' }
$literal = ConvertTo-Json -InputObject ($target -replace '\\', '/') -Compress
$ai = [Runtime.InteropServices.Marshal]::GetActiveObject('Illustrator.Application')
$js = 'var __iseResult; $.global.ISE_AI_ARGS=' + $ArgJson + '; try { __iseResult=$.evalFile(new File(' + $literal + ')); } finally { delete $.global.ISE_AI_ARGS; } __iseResult;'
$result = [string]$ai.DoJavaScript($js)
if (-not $result) { throw 'Illustrator returned no result; probe status is unknown.' }
if ($Output) {
    [System.IO.File]::WriteAllText($Output, $result, (New-Object System.Text.UTF8Encoding($false)))
}
Write-Output $result
$record = $result | ConvertFrom-Json
if ($record.passed -ne $true) { throw 'Probe failed or did not certify its checks; inspect the result.' }
