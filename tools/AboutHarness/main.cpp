// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vixen420
//
// StrokeMaster is free software: you may redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. It comes with ABSOLUTELY NO WARRANTY. See the file LICENSE, or
// <https://www.gnu.org/licenses/>, for the full text.

/* Shows the StrokeMaster panel's About dialog on its own, outside InDesign.
 *
 * It builds the very same plugin/ui/StrokeMasterAbout.cpp and the very same dialog template
 * as the panel plugin, so the window can be looked at, and photographed, after every edit,
 * in either theme (the house template's rule, .workspace/ADOBE_PLUGIN_ABOUT.md).
 *
 *   AboutHarness.exe              the light theme
 *   AboutHarness.exe /dark        InDesign's medium-dark interface colors
 *   AboutHarness.exe /exit3000    close by itself after three seconds
 *   AboutHarness.exe /shot:x.bmp  photograph the dialog into x.bmp, then close
 *
 * Run on a desktop of its own (tools/AboutHarness/shoot.ps1), it takes no one's focus.
 *
 * InDesign's typeface is read from the installed InDesign (App Paths\InDesign.exe). Build it
 * with tools/AboutHarness/build.cmd. A development tool; not part of the plugin.
 */

#include <windows.h>
#include <string>
#include "StrokeMasterAbout.h"
#include "HostLook.h"

#pragma comment(lib, "advapi32.lib")

namespace {
const wchar_t* const kDialogCaption = L"About StrokeMaster";
wchar_t gShot[MAX_PATH] = { 0 };

DWORD WINAPI CloseAfter(LPVOID ms)
{
    Sleep(static_cast<DWORD>(reinterpret_cast<UINT_PTR>(ms)));
    HWND dlg = FindWindowW(nullptr, kDialogCaption);
    if (dlg != nullptr) PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    return 0;
}

/* Writes the dialog, as PrintWindow draws it, to a 24-bit BMP, then closes it: a picture
   taken by the harness itself, which needs no screen, no focus, and no person. */
DWORD WINAPI ShootAndClose(LPVOID)
{
    HWND dlg = nullptr;
    for (int waited = 0; waited < 10000 && dlg == nullptr; waited += 25) {
        dlg = FindWindowW(nullptr, kDialogCaption);
        if (dlg == nullptr) Sleep(25);
    }
    Sleep(600);
    if (dlg != nullptr) {
        RECT r;
        GetWindowRect(dlg, &r);
        const int w = r.right - r.left, h = r.bottom - r.top;
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        BITMAPINFO bi;
        ZeroMemory(&bi, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 24;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old = SelectObject(mem, bmp);
        // Flag 0: each control paints itself (WM_PRINT). PW_RENDERFULLCONTENT reads the
        // compositor, which a desktop of its own does not have, and came back black.
        PrintWindow(dlg, mem, 0);
        SelectObject(mem, old);
        const DWORD stride = ((w * 3 + 3) / 4) * 4, size = stride * h;
        BITMAPFILEHEADER fh;
        ZeroMemory(&fh, sizeof(fh));
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(fh) + sizeof(bi.bmiHeader);
        fh.bfSize = fh.bfOffBits + size;
        HANDLE f = CreateFileW(gShot, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD n = 0;
            WriteFile(f, &fh, sizeof(fh), &n, nullptr);
            WriteFile(f, &bi.bmiHeader, sizeof(bi.bmiHeader), &n, nullptr);
            WriteFile(f, bits, size, &n, nullptr);
            CloseHandle(f);
        }
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    }
    return 0;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR cmdLine, int)
{
    wchar_t exe[MAX_PATH] = { 0 };
    DWORD exeSize = sizeof(exe);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\InDesign.exe",
                     nullptr, RRF_RT_REG_SZ, nullptr, exe, &exeSize) == ERROR_SUCCESS) {
        std::wstring path = exe;
        const size_t slash = path.rfind(L'\\');
        if (slash != std::wstring::npos) hostlook::UseSupportFiles(path.substr(0, slash));
    }

    StrokeMasterAboutTheme theme;
    if (cmdLine != nullptr && wcsstr(cmdLine, L"/dark") != nullptr) {
        // InDesign 21.6's medium-dark interface, as the panel reads it (IseUIPanel.cpp).
        theme.panel     = RGB(0x53, 0x53, 0x53);
        theme.panelText = RGB(0xE0, 0xE0, 0xE0);
        theme.band      = RGB(0x53, 0x53, 0x53);
        theme.bandText  = RGB(0xE0, 0xE0, 0xE0);
        theme.rule      = RGB(0x3C, 0x3C, 0x3C);
        theme.link      = RGB(0x5A, 0xA9, 0xE6);
        theme.ownerDrawButton = true;
        theme.button       = RGB(0x6A, 0x6A, 0x6A);
        theme.buttonText   = RGB(0xE0, 0xE0, 0xE0);
        theme.buttonBorder = RGB(0x3C, 0x3C, 0x3C);
        theme.darkTitleBar = true;
    }

    // /shot:<file.bmp> photographs the dialog and closes it.
    const wchar_t* shot = cmdLine != nullptr ? wcsstr(cmdLine, L"/shot:") : nullptr;
    if (shot != nullptr) {
        shot += 6;
        size_t i = 0;
        const wchar_t end = *shot == L'"' ? L'"' : L' ';
        if (*shot == L'"') ++shot;
        while (*shot != L'\0' && *shot != end && i + 1 < MAX_PATH) gShot[i++] = *shot++;
        gShot[i] = L'\0';
        CloseHandle(CreateThread(nullptr, 0, ShootAndClose, nullptr, 0, nullptr));
    } else if (cmdLine != nullptr && wcsstr(cmdLine, L"/exit") != nullptr) {
        const int ms = _wtoi(wcsstr(cmdLine, L"/exit") + 5);
        CloseHandle(CreateThread(nullptr, 0, CloseAfter, reinterpret_cast<LPVOID>(static_cast<UINT_PTR>(ms > 0 ? ms : 3000)), 0, nullptr));
    }

    if (!StrokeMasterShowAboutDialog(instance, nullptr, theme)) {
        MessageBoxW(nullptr, L"StrokeMasterShowAboutDialog returned false: the dialog could not be created.",
                    L"About harness", MB_ICONERROR | MB_OK);
        return 1;
    }
    return 0;
}
