# Compile only. Never loads a plugin or writes to the external SDK.
#Requires -Version 7.0
param(
    [Parameter(Mandatory = $true)][string]$SdkPath,
    [string]$Python = 'python',
    [string]$Toolset = '14.42.34433',
    [string]$WindowsSdk = '10.0.26100.0'
)
$ErrorActionPreference = 'Stop'
# Resolve against the caller's directory before PiPL generation changes location.
if (Test-Path -LiteralPath $Python -PathType Leaf) {
    $Python = (Resolve-Path -LiteralPath $Python).ProviderPath
} else {
    $Python = (Get-Command -Name $Python -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Path
}
$sdk = (Resolve-Path -LiteralPath $SdkPath).Path
$output = Join-Path $PSScriptRoot 'build'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere is unavailable.' }
$installations = @(& $vswhere -all -products * -requires Microsoft.Component.MSBuild -property installationPath)
$installation = $installations | Where-Object {
    Test-Path -LiteralPath (Join-Path $_ "VC\Tools\MSVC\$Toolset\bin\Hostx64\x64\cl.exe")
} | Select-Object -First 1
if (-not $installation) { throw "Requested MSVC $Toolset is unavailable." }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
$pipl = Join-Path $sdk 'tools\pipl\create_pipl.py'
if (-not (Test-Path -LiteralPath $pipl)) { throw 'The SDK PiPL generator is missing.' }
New-Item -ItemType Directory -Force -Path $output | Out-Null
$escapedSdk = [System.Security.SecurityElement]::Escape($sdk)
$escapedToolset = [System.Security.SecurityElement]::Escape($Toolset)
$escapedWindowsSdk = [System.Security.SecurityElement]::Escape($WindowsSdk)
$props = @"
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <ProbeSdkRoot>$escapedSdk</ProbeSdkRoot>
    <VCToolsVersion>$escapedToolset</VCToolsVersion>
    <WindowsTargetPlatformVersion>$escapedWindowsSdk</WindowsTargetPlatformVersion>
  </PropertyGroup>
</Project>
"@
[System.IO.File]::WriteAllText((Join-Path $output 'inputs.props'), $props)
Push-Location -LiteralPath $output
try {
    & $Python -B $pipl -input '[{"name":"StrokeMasterProposedProbe","entry_point":"PluginMain"}]'
    if ($LASTEXITCODE -ne 0) { throw 'PiPL generation failed.' }
} finally { Pop-Location }
if (-not (Test-Path -LiteralPath (Join-Path $output 'plugin.pipl'))) { throw 'PiPL output is missing.' }
$project = Join-Path $PSScriptRoot 'Probe.vcxproj'
function Invoke-ProbeBuild {
    param([string]$LogName, [switch]$Tests)
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $msbuild
    $start.WorkingDirectory = $PSScriptRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    # Normalize case only in the child's environment. This avoids MSBuild's
    # duplicate PATH/Path failure without changing machine or user settings.
    $start.Environment.Clear()
    foreach ($pair in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        $start.Environment[$pair.Key.ToUpperInvariant()] = $pair.Value
    }
    foreach ($argument in @($project, '/p:Configuration=Release', '/p:Platform=x64', '/m:1', '/v:minimal', '/nologo')) {
        $start.ArgumentList.Add($argument)
    }
    if ($Tests) { $start.ArgumentList.Add('/p:ProbeTests=true') }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw 'MSBuild did not start.' }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(60000)) {
            $process.Kill($true)
            throw 'Bounded build exceeded 60 seconds.'
        }
        $transcript = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        [System.IO.File]::WriteAllText((Join-Path $output $LogName), $transcript)
        Write-Output $transcript
        if ($process.ExitCode -ne 0) { throw "PROPOSED build failed; see build/$LogName." }
    } finally { $process.Dispose() }
}
Invoke-ProbeBuild -LogName 'compile.log'
Invoke-ProbeBuild -LogName 'tests-compile.log' -Tests
& (Join-Path $output 'ProbeTests.exe') 2>&1 | Tee-Object -FilePath (Join-Path $output 'tests.log')
if ($LASTEXITCODE -ne 0) { throw 'Mock ownership tests failed.' }
& $Python -B (Join-Path $PSScriptRoot 'verify_build.py')
if ($LASTEXITCODE -ne 0) { throw 'PiPL or binary verification failed.' }
Write-Output 'PROPOSED: compile/link and mock tests only; no Illustrator load or host verification.'
