# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Vixen420
#
# Photographs the About dialog and the notice in both themes without touching the screen the user works on:
# the harness runs on a desktop of its own, so its dialog takes no focus, and draws itself
# into a BMP (/shot). Windows PowerShell 5.1.
#
#   powershell -NoProfile -File tools\AboutHarness\shoot.ps1 -Harness <AboutHarness.exe> -Out <folder>
param([Parameter(Mandatory = $true)][string]$Harness, [Parameter(Mandatory = $true)][string]$Out)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class OwnDesktop {
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct SI { public int cb; public string r, d, t; public int x,y,w,h,xc,yc,fa,flags; public short show, r2; public IntPtr r3, i, o, e; }
  [StructLayout(LayoutKind.Sequential)] struct PI { public IntPtr hp, ht; public int pid, tid; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr CreateDesktopW(string name, IntPtr dev, IntPtr mode, int flags, uint access, IntPtr sa);
  [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr d);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateProcessW(string app, string cmd, IntPtr pa, IntPtr ta, bool inh, int flags, IntPtr env, string dir, ref SI si, out PI pi);
  [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  public static bool Run(string exe, string args, string dir) {
    IntPtr desk = CreateDesktopW("StrokeMasterAboutShot", IntPtr.Zero, IntPtr.Zero, 0, 0x10000000u, IntPtr.Zero);
    if (desk == IntPtr.Zero) return false;
    var si = new SI(); si.cb = Marshal.SizeOf(typeof(SI)); si.d = "StrokeMasterAboutShot";
    PI pi;
    bool ok = CreateProcessW(exe, "\"" + exe + "\" " + args, IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, dir, ref si, out pi);
    if (ok) { WaitForSingleObject(pi.hp, 30000); CloseHandle(pi.hp); CloseHandle(pi.ht); }
    CloseDesktop(desk);
    return ok;
  }
}
'@
New-Item -ItemType Directory -Force $Out | Out-Null
$dir = Split-Path $Harness
foreach ($kind in @(@('about', ''), @('notice', ' /notice'))) {
  foreach ($theme in @(@('light', ''), @('dark', '/dark'))) {
    $bmp = Join-Path $Out ($kind[0] + "-" + $theme[0] + ".bmp")
    $ok = [OwnDesktop]::Run($Harness, ($theme[1] + $kind[1] + ' /shot:"' + $bmp + '"').Trim(), $dir)
    "{0} {1}: started={2} written={3}" -f $kind[0], $theme[0], $ok, (Test-Path -LiteralPath $bmp)
  }
}
