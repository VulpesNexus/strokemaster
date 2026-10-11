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

/* Where a dialog opens.
 *
 * The About box opens in the middle of Illustrator's window, which is where
 * the host puts its own. Centering on the owner is the whole of the idea; the
 * rest is making sure the result is somewhere a person can reach.
 *
 * Centering alone is not enough, which is what this file is for. The box used
 * to center on the owner and stop there, so an Illustrator sitting against a
 * screen edge -- or spanning two monitors -- could put it half off the desktop
 * or under the taskbar, and the prose in it does not scroll. It is now pushed
 * back inside the work area of whatever monitor it lands on.
 *
 * The same file, with the same reasoning, is in LiveShear. Both plugins show
 * the same About window, and it is placed the same way in both.
 *
 * There are no Illustrator types here, for the same reason there are none in
 * SubgroupAbout.cpp: the About harness builds that file on its own, and this
 * header goes with it. There is no WIN_ENV guard either, because this harness
 * does not define the macro and this plugin is Windows-only regardless.
 *
 * LiveShear's copy does have that guard, and its harness passes /DWIN_ENV to
 * match. Those two lines are the whole of the difference between the two files:
 * the code between them is identical, and should stay that way.
 */

#ifndef DIALOGPLACEMENT_H
#define DIALOGPLACEMENT_H

#include <windows.h>

namespace DialogPlacement
{
    /** The top-left corner to open a dialog of this size at, given its owner.
        Pass the window's full outer size, the one GetWindowRect reports. */
    inline POINT Origin(HWND owner, int width, int height)
    {
        RECT over = { 0, 0, 0, 0 };
        bool haveOwner = false;
        // A minimized window's rectangle is off in the far negative corner, so
        // it would center the dialog somewhere nobody can see.
        if (owner != nullptr && IsWindow(owner) && !IsIconic(owner))
        {
            haveOwner = GetWindowRect(owner, &over) != FALSE &&
                        over.right > over.left && over.bottom > over.top;
        }

        MONITORINFO mi;
        ZeroMemory(&mi, sizeof(mi));
        mi.cbSize = sizeof(mi);
        // Spelled out rather than nested in a conditional, because the three
        // cases really are different and MonitorFromWindow must not be handed
        // a null window: with no owner at all the primary monitor is the only
        // answer there is.
        HMONITOR mon = nullptr;
        if (haveOwner)
        {
            mon = MonitorFromRect(&over, MONITOR_DEFAULTTONEAREST);
        }
        else if (owner != nullptr)
        {
            mon = MonitorFromWindow(owner, MONITOR_DEFAULTTOPRIMARY);
        }
        else
        {
            const POINT origin = { 0, 0 };
            mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        }
        if (mon == nullptr || GetMonitorInfoW(mon, &mi) == FALSE)
        {
            mi.rcWork.left = 0;
            mi.rcWork.top = 0;
            mi.rcWork.right = GetSystemMetrics(SM_CXSCREEN);
            mi.rcWork.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
        if (!haveOwner) over = mi.rcWork;

        POINT p;
        p.x = over.left + ((over.right - over.left) - width) / 2;
        p.y = over.top + ((over.bottom - over.top) - height) / 2;

        // The far edges are clamped first and the near ones second, so that a
        // dialog larger than the work area keeps its title bar and its first
        // controls rather than its bottom corner.
        if (p.x + width > mi.rcWork.right) p.x = mi.rcWork.right - width;
        if (p.y + height > mi.rcWork.bottom) p.y = mi.rcWork.bottom - height;
        if (p.x < mi.rcWork.left) p.x = mi.rcWork.left;
        if (p.y < mi.rcWork.top) p.y = mi.rcWork.top;
        return p;
    }

    /** Moves an already-created dialog there. The About box is positioned from
        WM_INITDIALOG, by which point the window exists and can be asked how
        big it is. */
    inline void CenterOnOwner(HWND dlg)
    {
        RECT dr;
        if (GetWindowRect(dlg, &dr) == FALSE) return;
        const POINT p = Origin(GetWindow(dlg, GW_OWNER),
                               dr.right - dr.left, dr.bottom - dr.top);
        SetWindowPos(dlg, nullptr, p.x, p.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

#endif /* DIALOGPLACEMENT_H */
