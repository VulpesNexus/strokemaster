# run-ai-fresh.ps1 - run tools\jsx\ai-*.jsx probes in a fresh, quietly launched Illustrator.
#
# Usage (Windows PowerShell 5.1):
#   powershell -NoProfile -File tools\run-ai-fresh.ps1 -OutDir <dir> -RunId sb -Script ai-stroke-behind-proof.jsx
#
# Each call starts its own Illustrator, so every probe gets a fresh process, and quits it again.
# It refuses when Illustrator is already running: that session is the owner's or another
# session's, and a probe must never close its documents or quit it.
#
# - Launch: CreateProcess with SW_SHOWNOACTIVATE, never Start-Process, which takes the focus.
# - Workspace: adobe-workspace-guard.ps1 Save before the launch and Restore after the quit, since
#   Illustrator saves the quietly launched window as the owner's layout when it quits.
# - Crash prompt: after an earlier crash Illustrator starts on a "Launch Illustrator" prompt and
#   runs no script until it is answered; a posted BM_CLICK answers it without input or focus.
# - Arguments: the probe reads $.global.ISE_REGION_ARGS = {outDir, runId} plus -ArgJson's fields
#   (forward-slash paths); it is deleted after.
# - Result: the probe's raw return string goes to <OutDir>\<RunId>-host.txt, and is printed.
# - Quit: every document left open belongs to this launch and is closed without saving, then
#   app.executeMenuCommand('quit'), which quits where Application.Quit sometimes does nothing.
#
# Pure ASCII: this file is read in the system codepage.
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$')][string]$RunId,
    [Parameter(Mandatory = $true)][ValidatePattern('^ai-[A-Za-z0-9_-]+\.jsx$')][string]$Script,
    # Further probe arguments, merged into ISE_REGION_ARGS beside outDir and runId.
    [string]$ArgJson = '{}',
    [string]$Exe = (Join-Path $env:ProgramFiles 'Adobe\Adobe Illustrator 2026\Support Files\Contents\Windows\Illustrator.exe'),
    # The versioned ProgID: since Illustrator 2025 was installed beside 2026, the plain
    # Illustrator.Application names 2025, which starts a second Illustrator instead of attaching.
    [string]$ProgId = 'Illustrator.Application.30',
    [int]$ReadySeconds = 240,
    [int]$QuitSeconds = 120
)
$ErrorActionPreference = 'Stop'
if ($PSVersionTable.PSEdition -ne 'Desktop') { throw 'Run with Windows PowerShell 5.1 (powershell.exe).' }

$target = Join-Path (Join-Path $PSScriptRoot 'jsx') $Script
if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { throw "probe not found: $target" }
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "Illustrator not found: $Exe" }
$out = (Resolve-Path -LiteralPath $OutDir).ProviderPath
$resultFile = Join-Path $out ($RunId + '-host.txt')
if (Test-Path -LiteralPath $resultFile) { throw "refusing to overwrite $resultFile" }
# Parsed before any launch. Windows PowerShell strips the double quotes from a JSON argument
# passed through powershell -File, so pass -ArgJson from an in-process call (& script.ps1).
try { $extraArgs = $ArgJson | ConvertFrom-Json } catch { throw ('-ArgJson is not valid JSON: ' + $_.Exception.Message) }
if (Get-Process Illustrator -ErrorAction SilentlyContinue) {
    throw 'Illustrator is already running; this runner only drives an instance it launched itself.'
}
$guard = Join-Path $PSScriptRoot '..\..\.workspace\tools\adobe-workspace-guard.ps1'
$hasGuard = Test-Path -LiteralPath $guard

if (-not ('SmAiLaunch' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class SmAiLaunch {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct STARTUPINFO { public int cb; public string lpReserved, lpDesktop, lpTitle; public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags; public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
    [StructLayout(LayoutKind.Sequential)]
    struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
    delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CreateProcessW(string app, string cmd, IntPtr pa, IntPtr ta, bool inherit, int flags, IntPtr env, string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] static extern int GetWindowThreadProcessId(IntPtr hwnd, out int pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int size);
    [DllImport("user32.dll")] static extern bool PostMessageW(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);

    // STARTF_USESHOWWINDOW with SW_SHOWNOACTIVATE. Returns the process id, or the negated Win32 error.
    public static int Start(string exe, string dir) {
        var si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(si);
        si.dwFlags = 1;
        si.wShowWindow = 4;
        PROCESS_INFORMATION pi;
        if (!CreateProcessW(exe, "\"" + exe + "\"", IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, dir, ref si, out pi))
            return -Marshal.GetLastWin32Error();
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return pi.dwProcessId;
    }

    // Posts BM_CLICK to the crash prompt's "Launch Illustrator" button.
    public static bool AnswerCrashPrompt(int pid) {
        bool clicked = false;
        EnumWindows((top, unused) => {
            int owner;
            GetWindowThreadProcessId(top, out owner);
            if (owner != pid) return true;
            EnumChildWindows(top, (child, unused2) => {
                var text = new StringBuilder(128);
                GetWindowTextW(child, text, text.Capacity);
                if (text.ToString() != "&Launch Illustrator") return true;
                PostMessageW(child, 0x00F5, IntPtr.Zero, IntPtr.Zero);
                clicked = true;
                return false;
            }, IntPtr.Zero);
            return !clicked;
        }, IntPtr.Zero);
        return clicked;
    }
}
"@
}

function Get-Host-Ai { [Runtime.InteropServices.Marshal]::GetActiveObject($ProgId) }

$launched = 0
$failure = $null
try {
    if ($hasGuard) { & $guard Save -App Illustrator | Out-Null }
    $launched = [SmAiLaunch]::Start($Exe, (Split-Path -Parent $Exe))
    if ($launched -le 0) { throw ('Illustrator could not be started: Win32 error {0}.' -f (-$launched)) }

    # GetActiveObject answers before Illustrator will run a script, so wait for a script to run.
    $ai = $null
    $deadline = (Get-Date).AddSeconds($ReadySeconds)
    while (-not $ai -and (Get-Date) -lt $deadline) {
        try {
            $candidate = Get-Host-Ai
            $null = $candidate.DoJavaScript('app.documents.length + "";')
            $ai = $candidate
        } catch {
            [SmAiLaunch]::AnswerCrashPrompt($launched) | Out-Null
            Start-Sleep -Seconds 3
        }
    }
    if (-not $ai) { throw 'Illustrator started but never became ready to run a script.' }
    $version = [string]$ai.DoJavaScript('app.version + "";')

    $probeTable = [ordered]@{}
    $extraArgs.PSObject.Properties | ForEach-Object { $probeTable[$_.Name] = $_.Value }
    $probeTable['outDir'] = ($out -replace '\\', '/')
    $probeTable['runId'] = $RunId
    $probeArgs = $probeTable | ConvertTo-Json -Compress
    $literal = ConvertTo-Json -InputObject ($target -replace '\\', '/') -Compress
    $js = '(function(){var had=Object.prototype.hasOwnProperty.call($.global,"ISE_REGION_ARGS"),previous=$.global.ISE_REGION_ARGS;' +
          'try{$.global.ISE_REGION_ARGS=' + $probeArgs + ';return String($.evalFile(new File(' + $literal + ')));}' +
          'catch(e){return "WRAPPER ERROR "+e.message+" line "+e.line;}' +
          'finally{if(had){$.global.ISE_REGION_ARGS=previous;}else{delete $.global.ISE_REGION_ARGS;}}})();'
    $raw = [string]$ai.DoJavaScript($js)
    [IO.File]::WriteAllText($resultFile, ('host=' + $version + '|' + $raw), (New-Object Text.UTF8Encoding($false)))
    Write-Output ('host=' + $version + '|' + $raw)
} catch {
    $failure = $_
} finally {
    if ($launched -gt 0) {
        $process = Get-Process -Id $launched -ErrorAction SilentlyContinue
        if ($process) {
            try {
                $ai = Get-Host-Ai
                $null = $ai.DoJavaScript('while (app.documents.length) app.documents[0].close(SaveOptions.DONOTSAVECHANGES); app.executeMenuCommand("quit"); "";')
            } catch {}
            if (-not $process.WaitForExit($QuitSeconds * 1000)) {
                $failure = [string]$failure + ' Illustrator did not quit; it is left running and the workspace snapshot pending.'
            }
        }
        if ($hasGuard -and -not (Get-Process -Id $launched -ErrorAction SilentlyContinue)) {
            & $guard Restore -App Illustrator | Out-Null
        }
    }
}
if ($failure) { Write-Error ('run-ai-probe failed: ' + [string]$failure); exit 1 }
