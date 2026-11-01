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
// the Adobe InDesign SDK or the Adobe Illustrator SDK, whose sources are
// compiled into every plugin built with them. See LICENSE-EXCEPTION.

#ifndef __STROKEMASTERABOUT_H__
#define __STROKEMASTERABOUT_H__

#ifdef _WIN32

#include <windows.h>

/** The colors to draw the dialog in.

    Passed in rather than read here, which is the whole point: InDesign knows
    what it is painting its own panels with (IInterfaceColors), and this file must not
    know what InDesign is. A plain struct of COLORREFs crosses that line without
    dragging the SDK across it, so tools/AboutHarness can still build this same
    file and show it -- in either theme, by filling this in itself.

    Defaults are the system colors, which is what the dialog looked like before
    it could be told otherwise, and what it still falls back to on a host that
    declines to say. */
struct StrokeMasterAboutTheme
{
    COLORREF panel;      /**< The upper band, behind the title and the prose. */
    COLORREF panelText;
    COLORREF band;       /**< The lower band, behind the button and the notices. */
    COLORREF bandText;
    COLORREF rule;       /**< The line dividing them. */
    COLORREF link;       /**< Link text, which a SysLink otherwise picks itself. */

    /** The OK button. A stock button is painted by the visual styles and no
        message recolours it, so on a dark panel it stays a white slab. When
        `ownerDrawButton` is true the dialog paints it in these colors instead. */
    bool     ownerDrawButton;
    COLORREF button;
    COLORREF buttonText;
    COLORREF buttonBorder;

    /** Ask the window manager for a dark caption. Without it a dark dialog
        wears a white title bar, which looks worse than not theming it. */
    bool     darkTitleBar;

    StrokeMasterAboutTheme()
        : panel(GetSysColor(COLOR_WINDOW)),
          panelText(GetSysColor(COLOR_WINDOWTEXT)),
          band(GetSysColor(COLOR_BTNFACE)),
          bandText(GetSysColor(COLOR_BTNTEXT)),
          rule(GetSysColor(COLOR_3DLIGHT)),
          link(GetSysColor(COLOR_HOTLIGHT)),
          ownerDrawButton(false),
          button(GetSysColor(COLOR_BTNFACE)),
          buttonText(GetSysColor(COLOR_BTNTEXT)),
          buttonBorder(GetSysColor(COLOR_3DSHADOW)),
          darkTitleBar(false) {}
};

/** Shows the About dialog.

    Deliberately free of every InDesign dependency: it takes a module handle,
    a parent window, and the colors to use, and touches nothing else. That is
    what lets the dialog be built into a small harness and looked at directly,
    instead of being inspected only through the host - which for a modal dialog
    means not at all.

    @param  instance  the module holding the dialog resource.
    @param  parent    owner window, may be null.
    @param  theme     colors to draw in; omit for the system's own.
    @return false if the dialog could not be shown, so the caller can fall back
            to something plainer. */
bool StrokeMasterShowAboutDialog(HINSTANCE instance, HWND parent,
                             const StrokeMasterAboutTheme& theme = StrokeMasterAboutTheme());

/** Whether the notice is due: not yet shown for this version, for this user, and not in a run
    that names its own switches' file (STROKEMASTER_SETTINGS, as every test does). */
bool StrokeMasterNoticeDue();

/** Records the notice as shown for this version, for this user. Called before showing it, so a
    notice that cannot be shown, or that the host closes with itself, is not shown again. */
void StrokeMasterNoticeShown();

/** Shows the notice: the caveats, in the About window's frame, with a link to the repository's
    full list (the owner, 2026-11-01). Modeless, so a host driven by scripts never waits on it.

    @return false if it could not be shown. */
bool StrokeMasterShowNotice(HINSTANCE instance, HWND parent,
                            const StrokeMasterAboutTheme& theme = StrokeMasterAboutTheme());

#endif /* _WIN32 */

#endif /* __STROKEMASTERABOUT_H__ */
