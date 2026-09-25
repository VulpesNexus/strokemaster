# build-plugin.ps1 - build the InDesign Stroke Engine plugin against the InDesign 2026 SDK.
#
# Usage:
#   powershell -NoProfile -File plugin\build-plugin.ps1 -SdkPath "<path to Adobe InDesign 2026 SDK>"
#   powershell -NoProfile -File plugin\build-plugin.ps1 -SdkPath "..." -Configuration Debug
#
# What this handles, all of it learned the hard way and recorded in
# docs/INDESIGN_STROKE_SDK_MAP.md under "Building against this SDK":
#   - The SDK's precompiled-header shim accepts only MSVC 14.39 (17.9) or 14.42 (17.12).
#     Visual Studio 17.14's default 14.44 stops the build, so a supported toolset is selected.
#   - The SDK's project files hardcode a relative ID_SDK_DIR, which only resolves when the
#     linker's working directory is the project folder, so it is passed as an absolute path.
#   - Non-ASCII characters in the SDK path break the SDK's build tools, so the build runs
#     through a junction with an ASCII path, created in the temporary folder and removed after.
#   - Build output stays in build\ inside this repository; nothing is written into the SDK.
# Pure ASCII: this file is read in the system codepage.
param(
    [Parameter(Mandatory = $true)][string]$SdkPath,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [string[]]$PreferredToolsets = @('14.42.', '14.39.'),
    # Adds the experimental route W stroke-behind adornments (IseStrokeBehindAdornment.cpp).
    # Built into build\<Configuration>-RouteW, never over the default refusal build.
    [switch]$RouteW
)
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $SdkPath)) { throw "SDK not found: $SdkPath" }
$sdkFull = (Resolve-Path -LiteralPath $SdkPath).ProviderPath
if (-not (Test-Path -LiteralPath (Join-Path $sdkFull 'source\public\interfaces\text\IWaxRun.h'))) {
    throw "That does not look like the InDesign SDK (source\public\interfaces\text\IWaxRun.h is missing): $sdkFull"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw "vswhere.exe not found; is Visual Studio installed?" }
$candidates = @(& $vswhere -all -products * -requires Microsoft.Component.MSBuild -property installationPath)
if (-not $candidates) { throw "No Visual Studio installation with MSBuild was found." }

# Pick the first installation that actually carries a toolset this SDK supports. The newest
# installation is not necessarily the right one: build tools can be installed without any
# MSVC toolset at all.
$vsPath = $null
$toolset = $null
$seen = @()
foreach ($candidate in $candidates) {
    $toolsetRoot = Join-Path $candidate 'VC\Tools\MSVC'
    if (-not (Test-Path -LiteralPath $toolsetRoot)) { continue }
    $installed = Get-ChildItem -LiteralPath $toolsetRoot -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
    $seen += $installed
    foreach ($wanted in $PreferredToolsets) {
        $match = $installed | Where-Object { $_.StartsWith($wanted) } | Sort-Object | Select-Object -Last 1
        if ($match) { $vsPath = $candidate; $toolset = $match; break }
    }
    if ($toolset) { break }
}
if (-not $toolset) {
    throw ("None of the toolsets this SDK supports is installed. Found: " + (($seen | Sort-Object -Unique) -join ', ') +
           ". Add 'MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.42-17.12)' in the Visual Studio Installer.")
}
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found at $msbuild" }

$repo = Split-Path -Parent $PSScriptRoot
$variant = if ($RouteW) { $Configuration + '-RouteW' } else { $Configuration }
New-Item -ItemType Directory -Force -Path (Join-Path $repo ('build\' + $variant)), (Join-Path $repo ('build\obj\' + $variant)) | Out-Null

# Creates a junction whose name is derived from its target, so the same target always gets the
# same path. Random names would do for reaching the files, but odfrc records the absolute path
# of the .fr file it compiled into the intermediate folder and refuses to compile what it takes
# to be a second resource file into the same project, so that path has to be stable between
# runs. Stable names also keep the precompiled header and the object files valid.
function New-StableJunction {
    param([Parameter(Mandatory = $true)][string]$Prefix, [Parameter(Mandatory = $true)][string]$Target)
    $sha = [System.Security.Cryptography.SHA1]::Create()
    try { $digest = $sha.ComputeHash([System.Text.Encoding]::Unicode.GetBytes($Target.ToLowerInvariant())) }
    finally { $sha.Dispose() }
    $link = Join-Path $env:TEMP ($Prefix + ((($digest[0..3]) | ForEach-Object { $_.ToString('x2') }) -join ''))
    if ([System.IO.Directory]::Exists($link)) {
        # Left behind by an interrupted run. Reuse it only if it still points where it should;
        # anything else is removed, and a real directory in the way throws rather than be lost.
        $existing = @((Get-Item -LiteralPath $link -Force).Target) | Select-Object -First 1
        if ($existing -and $existing.TrimEnd('\') -ieq $Target.TrimEnd('\')) { return $link }
        [System.IO.Directory]::Delete($link)
    }
    New-Item -ItemType Junction -Path $link -Target $Target | Out-Null
    return $link
}

$junction = New-StableJunction -Prefix 'ise-sdk-' -Target $sdkFull
# MSBuild splits its command line on spaces and treats '&' as a switch separator, and this
# repository's path contains both. Reach the project, and write the output, through a second
# junction whose path has neither, so every argument survives intact.
$repoLink = New-StableJunction -Prefix 'ise-repo-' -Target $repo
$project = Join-Path $repoLink 'plugin\prj\InDesignStrokeEngine.vcxproj'
$outDir = Join-Path $repoLink ('build\' + $variant + '\')
$intDir = Join-Path $repoLink ('build\obj\' + $variant + '\')
try {
    Write-Host "SDK        : $sdkFull"
    Write-Host "via        : $junction"
    Write-Host "repository : $repo"
    Write-Host "via        : $repoLink"
    Write-Host "toolset    : $toolset"
    Write-Host "output     : $outDir"
    # The SDK's response files list include paths relative to the SDK's own project folder,
    # so they only work for projects that live there. Write copies with absolute paths.
    $generated = Join-Path $repoLink 'build\rsp'
    New-Item -ItemType Directory -Force -Path $generated | Out-Null
    $cppRsp = Join-Path $generated 'SDKCPPOptions.rsp'
    $odfrcRsp = Join-Path $generated 'SDKODFRCOptions.rsp'
    foreach ($pair in @(@('SDKCPPOptions.rsp', $cppRsp), @('SDKODFRCOptions.rsp', $odfrcRsp))) {
        $source = Join-Path $junction ('build\win\prj\' + $pair[0])
        (Get-Content -LiteralPath $source) -replace '\.\.\\\.\.\\\.\.', $junction |
            Set-Content -LiteralPath $pair[1] -Encoding ASCII
    }

    # odfrc remembers the .fr file it last compiled here and stops if a different path turns up.
    # A repository that has moved, or output from a build that predates stable junction names,
    # leaves a path that cannot match, so clear its state instead of failing.
    $frRecord = Join-Path $intDir 'frFilePath'
    $frWanted = Join-Path $repoLink 'plugin\source\Ise.fr'
    if (Test-Path -LiteralPath $frRecord) {
        $frSeen = (Get-Content -LiteralPath $frRecord -Raw).Trim()
        if ($frSeen -and $frSeen -ine $frWanted) {
            Write-Host "stale resources: odfrc last compiled $frSeen; clearing"
            Remove-Item -LiteralPath $frRecord -Force
            Get-ChildItem -LiteralPath $intDir -Filter '*.fres' -File -ErrorAction SilentlyContinue | Remove-Item -Force
            Get-ChildItem -LiteralPath $intDir -Filter 'idrc_*' -Directory -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
        }
    }

    $buildLog = Join-Path $repoLink 'build\msbuild.log'
    $arguments = @(
        $project,
        "/p:Configuration=$Configuration",
        '/p:Platform=x64',
        "/p:VCToolsVersion=$toolset",
        '/p:WindowsTargetPlatformVersion=10.0.26100.0',
        "/p:ID_SDK_DIR=$junction",
        "/p:IseSdkDir=$junction",
        "/p:IseCppOptions=$cppRsp",
        "/p:IseOdfrcOptions=$odfrcRsp",
        "/p:OutDir=$outDir",
        "/p:IntDir=$intDir",
        "/p:IseRouteW=$(if ($RouteW) { 'true' } else { 'false' })",
        '/m', '/v:minimal', '/nologo',
        "/flp:logfile=$buildLog;verbosity=normal"
    )
    Write-Host "log        : $buildLog"
    & $msbuild @arguments
    $buildExit = $LASTEXITCODE
    if ($buildExit -ne 0) {
        # Surface the diagnostics rather than only the exit code: the log outlives this run.
        Write-Host "--- diagnostics from $buildLog"
        Select-String -LiteralPath $buildLog -Pattern 'error|warning' -CaseSensitive:$false |
            Select-Object -First 40 | ForEach-Object { Write-Host ("    " + $_.Line.Trim()) }
        throw "MSBuild failed with exit code $buildExit; full log: $buildLog"
    }
    Get-ChildItem -LiteralPath $outDir -Filter *.pln | Select-Object Name, Length, LastWriteTime
} finally {
    # Remove the junctions themselves, never their contents.
    foreach ($link in @($junction, $repoLink)) {
        if ([System.IO.Directory]::Exists($link)) { [System.IO.Directory]::Delete($link) }
    }
}
