# Installs one built variant of the StrokeMaster InDesign plugin into InDesign 2026's
# Plug-Ins folder, or restores the previous installation from a backup this script made.
#
#   powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-RouteW
#   powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Restore <backup folder>
#   powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-RouteW -ValidateOnly
#   powershell -NoProfile -File tools\install-indesign-plugin.ps1 -Variant Release-UI -Name StrokeMasterUI
#
# -Name picks the plugin: InDesignStrokeEngine (the default), or StrokeMasterUI, the
# StrokeMaster panel; each lives in a folder of its own under Plug-Ins.
#
# The Plug-Ins folder is writable by administrators only, so installing needs an elevated
# PowerShell; -ValidateOnly needs no elevation and writes nothing. InDesign must not be
# running: a loaded plugin cannot be replaced, and a restart is what loads the new one.
# Before anything is replaced, the installed plugin and its resources are copied to
# build\indesign-install\<UTC time>-backup\, and a record of hashes is written beside it.
# Pure ASCII: this file is read in the system codepage.
param(
    [string]$Variant = '',
    [string]$Restore = '',
    [string]$InDesign = 'C:\Program Files\Adobe\Adobe InDesign 2026',
    [ValidateSet('InDesignStrokeEngine', 'StrokeMasterUI')][string]$Name = 'InDesignStrokeEngine',
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
if ([bool]$Variant -eq [bool]$Restore) { throw 'Give exactly one of -Variant or -Restore.' }
$repo = Split-Path -Parent $PSScriptRoot
$name = $Name
# A backup restored without -Name: the plugin it holds names itself (a panel backup asked for
# -Name StrokeMasterUI, and failed without it: the review of 2026-11-01).
if ($Restore -and -not $PSBoundParameters.ContainsKey('Name')) {
    $held = @(Get-ChildItem -LiteralPath $Restore -Filter '*.pln' -File -ErrorAction SilentlyContinue)
    if ($held.Count -eq 1) { $name = $held[0].BaseName }
    elseif ($held.Count -eq 0) { throw "No plugin in $Restore. A first install leaves an empty backup: to remove that plugin, delete its folder under Plug-Ins by hand, with InDesign closed." }
    else { throw 'Backup contains several plugins; specify -Name explicitly.' }
}
$target = Join-Path $InDesign ('Plug-Ins\' + $name)
$resourcesName = '(' + $name + ' Resources)'
if (-not (Test-Path -LiteralPath (Join-Path $InDesign 'InDesign.exe'))) { throw "InDesign.exe not found under $InDesign" }

if ($Variant) {
    $source = Join-Path $repo ('build\' + $Variant)
} else {
    $source = (Resolve-Path -LiteralPath $Restore).ProviderPath
}
$sourcePlugin = Join-Path $source ($name + '.pln')
$sourceResources = Join-Path $source $resourcesName
if (-not (Test-Path -LiteralPath $sourcePlugin -PathType Leaf)) { throw "No plugin at $sourcePlugin" }
if (-not (Test-Path -LiteralPath $sourceResources -PathType Container)) { throw "No resources folder at $sourceResources" }

function Get-Tree([string]$Root) {
    $records = @()
    if (-not (Test-Path -LiteralPath $Root)) { return $records }
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -Recurse -File | Sort-Object FullName)) {
        $records += [ordered]@{ path = $file.FullName.Substring($Root.Length).TrimStart('\'); bytes = $file.Length;
                                sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    }
    return $records
}

$running = @(Get-Process -Name InDesign -ErrorAction SilentlyContinue)
$installed = Get-Tree $target
$incoming = @(Get-Tree $source | Where-Object { $_.path -eq ($name + '.pln') -or $_.path.StartsWith($resourcesName + '\') })
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$plan = [ordered]@{
    schema = 1; action = $(if ($Variant) { 'install ' + $Variant } else { 'restore' })
    inDesignRunning = ($running.Count -gt 0); elevated = $elevated
    installed = $installed; incoming = $incoming
}
if ($ValidateOnly) {
    $plan | ConvertTo-Json -Depth 5
    Write-Output 'VALIDATED: nothing was written.'
    return
}
if ($running.Count) { throw 'InDesign is running. Quit it first; this script never closes it.' }
if (-not $elevated) { throw 'The Plug-Ins folder needs an elevated PowerShell (Run as administrator).' }

$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$suffix = if ($name -ne 'InDesignStrokeEngine') { '-' + $name } else { '' }
$backup = Join-Path $repo ('build\indesign-install\' + $stamp + $suffix + '-backup')
New-Item -ItemType Directory -Force -Path $backup | Out-Null
if (Test-Path -LiteralPath $target) {
    Copy-Item -LiteralPath (Join-Path $target ($name + '.pln')) -Destination $backup -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath (Join-Path $target $resourcesName)) {
        Copy-Item -LiteralPath (Join-Path $target $resourcesName) -Destination $backup -Recurse
    }
    $backedUp = Get-Tree $backup
    if (@($backedUp).Count -ne @($installed).Count) { throw "Backup incomplete in $backup; nothing was replaced." }
} else {
    New-Item -ItemType Directory -Force -Path $target | Out-Null
}

# Replace: the plugin, and the resources folder as a whole, so no stale resource survives.
$targetResources = Join-Path $target $resourcesName
if (Test-Path -LiteralPath $targetResources) { [IO.Directory]::Delete($targetResources, $true) }
Copy-Item -LiteralPath $sourcePlugin -Destination (Join-Path $target ($name + '.pln')) -Force
Copy-Item -LiteralPath $sourceResources -Destination $target -Recurse -Force

$after = Get-Tree $target
$mismatch = @()
foreach ($file in $incoming) {
    $match = @($after | Where-Object { $_.path -eq $file.path -and $_.sha256 -eq $file.sha256 })
    if ($match.Count -ne 1) { $mismatch += $file.path }
}
$record = [ordered]@{ schema = 1; utc = $stamp; action = $plan.action; backup = $backup; before = $installed
                      after = $after; verified = ($mismatch.Count -eq 0 -and @($after).Count -eq @($incoming).Count) }
$record | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $backup 'install-record.json') -Encoding ASCII
if (-not $record.verified) { throw ('Installed files do not match the source: ' + ($mismatch -join ', ') + ". Restore with -Restore $backup") }
Write-Output ('Installed and verified. Backup: ' + $backup)
Write-Output 'Start InDesign to load it.'
