# Trim glyph tips on InDesign's live text (tools\jsx\id-text-tips.jsx) in a fresh, quietly
# launched InDesign 2026 with the tests' own settings file (STROKEMASTER_SETTINGS, never the
# user's), then quits it. Refuses when InDesign is already running: that session is the owner's.
# Launch on the isolated hidden desktop. Pure ASCII.
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string[]]$Fonts = @('eaglelake', 'gorefont', 'facon', 'kerberos', 'germanbeauty', 'chalk'),
    [ValidatePattern('^[A-Za-z]{0,24}$')][string]$Text = '',
    [ValidateRange(0, 40)][double]$Weight = 0,
    [switch]$Outline,
    [string]$Exe = 'C:\Program Files\Adobe\Adobe InDesign 2026\InDesign.exe'
)
$ErrorActionPreference = 'Stop'
if (Get-Process InDesign -ErrorAction SilentlyContinue) { throw 'InDesign is already running.' }
$repo = Split-Path -Parent $PSScriptRoot
$guard = Join-Path (Split-Path -Parent $repo) '.workspace\tools\adobe-workspace-guard.ps1'
New-Item -ItemType Directory -Force $OutDir | Out-Null
$out = (Resolve-Path -LiteralPath $OutDir).ProviderPath
$settings = Join-Path $out 'settings.txt'
Set-Content -LiteralPath $settings -Value 'glyph_tips=1' -Encoding ascii
$env:STROKEMASTER_SETTINGS = $settings
& $guard Save -App InDesign -QuietLaunch
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class QuietLaunchTips {
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct SI { public int cb; public string r, d, t; public int x,y,w,h,xc,yc,fa,flags; public short show, r2; public IntPtr r3, i, o, e; }
  [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr hp, ht; public int pid, tid; }
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateProcessW(string app, string cmd, IntPtr pa, IntPtr ta, bool inh, int flags, IntPtr env, string dir, ref SI si, out PI pi);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  public static int Start(string exe, string dir) {
    var si = new SI(); si.cb = Marshal.SizeOf(typeof(SI)); si.flags = 1; si.show = 4;
    PI pi; if (!CreateProcessW(exe, "\"" + exe + "\"", IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, dir, ref si, out pi)) throw new System.ComponentModel.Win32Exception();
    CloseHandle(pi.hp); CloseHandle(pi.ht); return pi.pid;
  }
}
'@
$launched = 0
try {
    $launched = [QuietLaunchTips]::Start($Exe, (Split-Path -Parent $Exe))
    $deadline = (Get-Date).AddMinutes(6)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 5
        $p = Get-Process -Id $launched -ErrorAction SilentlyContinue
        if (-not $p) { throw 'InDesign exited during launch' }
        if ($p.MainWindowTitle -like 'Adobe InDesign*') { Start-Sleep -Seconds 10; break }
    }
    $m = (Get-Process -Id $launched).Modules | Where-Object { $_.ModuleName -eq 'InDesignStrokeEngine.pln' }
    'engine sha256: ' + (Get-FileHash $m.FileName -Algorithm SHA256).Hash.Substring(0, 16)
    $argJson = @{ outDir = ($out -replace '\\', '/'); settings = ($settings -replace '\\', '/'); fonts = $Fonts; text = $Text; weight = $Weight; outline = [bool]$Outline } | ConvertTo-Json -Compress
    $r = & (Join-Path $PSScriptRoot 'run-jsx.ps1') 'id-text-tips.jsx' -ArgJson $argJson
    $r | Set-Content -LiteralPath (Join-Path $out 'host.txt') -Encoding utf8
    $r
} finally {
    if ($launched) {
        $job = Start-Job { $a = New-Object -ComObject InDesign.Application.2026; $n = $a.Documents.Count; if ($n -eq 0) { $a.Quit(1852776480) }; "docs at quit=$n" }
        if (Wait-Job $job -Timeout 120) { Receive-Job $job } else { 'quit call not answered'; Stop-Job $job }
        $deadline = (Get-Date).AddSeconds(150)
        while ((Get-Date) -lt $deadline -and (Get-Process -Id $launched -ErrorAction SilentlyContinue)) { Start-Sleep -Seconds 3 }
        if (Get-Process -Id $launched -ErrorAction SilentlyContinue) { 'HANG at quit: closing'; Stop-Process -Id $launched -Force -Confirm:$false }
    }
    & $guard Restore -App InDesign
}
