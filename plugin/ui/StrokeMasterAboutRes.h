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

// The About window's resource IDs and fixed strings, shared by the panel plugin's resource
// script, StrokeMasterAbout.cpp, and tools/AboutHarness. No InDesign types, so the harness
// builds them too.

#ifndef __STROKEMASTERABOUTRES_H__
#define __STROKEMASTERABOUTRES_H__

#define IDD_STROKEMASTER_ABOUT          101

#define IDC_ABOUT_TITLE                 1001
#define IDC_ABOUT_BODY                  1002
#define IDC_ABOUT_ATTRIB                1003
#define IDC_ABOUT_LEGAL                 1004

// Where the upper band ends, in dialog units.
#define kAboutSplitDlgY                 158

#ifndef RC_INVOKED

// The release, shown in the window's title line. The plugin's file version follows it.
#define SM_VERSION_MAJOR    1
#define SM_VERSION_MINOR    0
#define SM_VERSION_PATCH    0
#define SM_WVERSION         L"1.0.0"

#define SM_COPY             L"\x00A9"   /* U+00A9 copyright sign */
#define SM_REPO_URL         L"https://github.com/VulpesNexus/strokemaster"
#define SM_AUTHOR_URL       L"https://github.com/VulpesNexus"

#endif // RC_INVOKED

#endif // __STROKEMASTERABOUTRES_H__
