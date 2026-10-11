// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vixen420
//
// StrokeMaster is free software: you may redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. It comes with ABSOLUTELY NO WARRANTY. See the file LICENSE, or
// <https://www.gnu.org/licenses/>, for the full text.
//
// Additional permission under GPL-3.0 section 7: this file may be combined with
// the Adobe InDesign SDK, whose sample framework sources are compiled into
// every plugin built from it. See LICENSE-EXCEPTION.

//  HostLook.h -- Illustrator's own look for this plugin's plain Win32 windows:
//  its interface typeface, its icons, and the shape of its buttons.
//
//  Nothing of Adobe's is copied into the plugin. The typeface and the icons
//  are read at run time from modules Illustrator always loads, and the
//  buttons are drawn to measurements taken from Adobe's own dialogs. Like the
//  dialogs that use it, this file takes no Illustrator types, so a standalone
//  harness builds it too; outside Illustrator it reads the installed copy,
//  given with UseSupportFiles.

//  Copied from Subgroup (see .workspace/ADOBE_PLUGIN_ABOUT.md in the workspace) for the
//  About window. InDesign loads the same dvaui.dll, so inside InDesign the typeface comes
//  from there. Outside it, UseSupportFiles takes InDesign's install folder, where the
//  file sits at the root; the one change to the copy is that fallback (HostModule).

#ifndef __HOSTLOOK_H__
#define __HOSTLOOK_H__

#ifdef _WIN32

#include <windows.h>
#include <string>

namespace hostlook
{
    /** Where to read Illustrator's typeface and icons when not running inside
        Illustrator: the install's Support Files folder. For test harnesses. */
    void UseSupportFiles(const std::wstring& folder);

    /** The face name of Illustrator's interface typeface. */
    const wchar_t* FontFace();

    /** Illustrator's interface typeface, Adobe Clean UX, `pixels` high at 96
        dots per inch and scaled to `dpi`; null when it cannot be read, and the
        caller keeps its own font. The caller deletes it. */
    HFONT MakeFont(int pixels, int dpi);

    /** Takes the typeface back out of the process; for plugin shutdown. */
    void ReleaseFont();

    /** One of Illustrator's icons, as the SVG text in its UserInterface.aip;
        empty when it cannot be read. */
    std::string Icon(const wchar_t* name);

    /** `percent` of the way from `under` to `over`. */
    COLORREF Mix(COLORREF over, COLORREF under, int percent);

    /** Draws an icon from its SVG text into `box` in one color on `back`.
        False when it could not. */
    bool DrawIcon(HDC dc, const RECT& box, const std::string& svg, COLORREF ink, COLORREF back);

    /** How Illustrator's dialogs color a push button in a given state. */
    struct ButtonLook
    {
        COLORREF fill;
        COLORREF edge;
        COLORREF ink;
        int edgeWidth;       /**< in hairlines */
        bool innerRing;      /**< a white ring inside a focused default button */
    };
    ButtonLook LookOf(bool primary, bool pressed, bool hot, bool focused,
                      COLORREF background, COLORREF text, COLORREF accent);

    /** Draws a push button's shape as Illustrator's dialogs do: fully rounded,
        on `background`. The label is the caller's to draw, in `look.ink`.
        False when Direct2D could not draw, and the caller draws its own. */
    bool DrawButton(HDC dc, const RECT& rc, COLORREF background, const ButtonLook& look, int hairline);
}

#endif // _WIN32

#endif // __HOSTLOOK_H__
