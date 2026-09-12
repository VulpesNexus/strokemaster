# Existing-host InDesign 2026 probe runner for Windows PowerShell 5.1.
param([Parameter(Mandatory=$true)][string]$Script, [string]$ArgJson='{}', [string]$Output='')
$ErrorActionPreference='Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop') { throw 'Use Windows PowerShell 5.1 (powershell.exe).' }
$null=$ArgJson | ConvertFrom-Json
$target=Join-Path (Join-Path $PSScriptRoot 'jsx') $Script
if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw 'Probe does not exist.' }
$literal=ConvertTo-Json -InputObject ($target -replace '\\','/') -Compress
# InDesign may not publish its application in the Running Object Table. Its COM class
# activation then provides the scripting connection; require a preexisting host process.
$beforeProcesses=@(Get-Process InDesign -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
if (-not $beforeProcesses) { throw 'Open InDesign before running its host probes.' }
$matchingProcesses=@(Get-Process InDesign -ErrorAction Stop | Where-Object {
    $_.Path -and ([System.Diagnostics.FileVersionInfo]::GetVersionInfo($_.Path).FileMajorPart -eq 21)
})
if ($matchingProcesses.Count -ne 1) { throw 'Require exactly one running InDesign 2026 (file major version 21); refusing COM activation.' }
try { $hostApp=[Runtime.InteropServices.Marshal]::GetActiveObject('InDesign.Application.2026') }
catch [System.Runtime.InteropServices.COMException] {
    if ($_.Exception.HResult -ne -2147221021) { throw }
    $hostApp=New-Object -ComObject InDesign.Application.2026
    $afterProcesses=@(Get-Process InDesign -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    if (@($afterProcesses | Where-Object { $_ -notin $beforeProcesses }).Count) {
        throw 'COM activation started another InDesign process; no probe was dispatched.'
    }
}
if ([string]$hostApp.Version -notmatch '^21\.') { throw 'Connected host is not InDesign 2026; no probe dispatched.' }
$js='var __result; $.global.ISE_PROBE_ARGS='+$ArgJson+'; try { __result=$.evalFile(new File('+$literal+')); } finally { delete $.global.ISE_PROBE_ARGS; } __result;'
$result=[string]$hostApp.DoScript($js,1246973031)
if (-not $result) { throw 'No result returned; host status is unknown.' }
if ($Output) { [System.IO.File]::WriteAllText($Output,$result,(New-Object System.Text.UTF8Encoding($false))) }
Write-Output $result
$record=$result | ConvertFrom-Json
if ($record.passed -ne $true) { throw 'Host checks failed; inspect the result.' }
